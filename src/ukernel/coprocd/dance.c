/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "daemons.h"
#include "dance.h"

#include <cocall/cocall_args.h>
#include <coproc/utils.h>

#include <err.h>
#include <sys/errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>

/* Provided by coservice daemon*/
static _Atomic(void *)codiscover_scb = NULL;

/* Provided by namespace daemon */
static _Atomic(namespace_t *) global_namespace = NULL;
static void *coinsert_scb = NULL;
static void *coselect_scb = NULL;

static _Atomic bool coproc_inited = false;
static _Atomic bool ukern_inited = false;

#define NSD_SELECTOR 1
#define COSERVICED_SELECTOR 2
#define IPCD_SELECTOR 3

void
invalidate_startup_info(void)
{
	coinsert_scb = NULL;
	coselect_scb = NULL;
	if (atomic_load_explicit(&coproc_inited, memory_order_acquire) == false)
		return;
	codiscover_scb = NULL;
	global_namespace = NULL;
	atomic_store_explicit(&ukern_inited, false, memory_order_release);
	atomic_store_explicit(&coproc_inited, false, memory_order_release);
}

//TODO-PBB: apply memory ordering that gives something sensible
static
int authenticate_dance(cocall_args_t *cocall_args, int selector)
{
	pid_t caller_pid, daemon_pid;
	int error;

	error = cogetpid(&caller_pid);
	switch (selector) {
		case NSD_SELECTOR:
			daemon_pid = get_nsd_pid();
			break;
		case COSERVICED_SELECTOR:
			daemon_pid = get_coserviced_pid();
			break;
		case IPCD_SELECTOR:
			daemon_pid = get_ipcd_pid();
			break;
	}
	
	if (daemon_pid == 0 || caller_pid != daemon_pid) 
		return (0);
	else
		return (1);
}

void nsd_init(cocall_args_t *cocall_args, void *token)
{
	if(!authenticate_dance(cocall_args, NSD_SELECTOR))
		COCALL_ERR(cocall_args, EPERM);

	
	/* clear old codiscover; whether valid or not it refers to the old universe */
	codiscover_scb = NULL;
	/* I give you: a global namespace capability, a scb capability for create nsobj (coinsert) and lookup nsobj (coselect) */
	coinsert_scb = cocall_args->coinsert;
	coselect_scb = cocall_args->coselect;
	atomic_store_explicit(&global_namespace, cocall_args->ns_cap, memory_order_release);
	
	/* Wait until coserviced is (re) ininitalized */
	/* You give me: scb capabilities for codiscover and coprovide */
	while((cocall_args->codiscover = atomic_load_explicit(&codiscover_scb, memory_order_acquire)) == NULL) {
		/* TODO-PBB: should consider returning and having two calls here instead */
		// this implementation may not do what we want (priority inversion?)
		sched_yield();
	}
	cocall_args->done_scb = done_worker_scb;
	COCALL_RETURN(cocall_args, 0);
}


void coserviced_init(cocall_args_t *cocall_args, void *token)
{
	if(!authenticate_dance(cocall_args, COSERVICED_SELECTOR))
		COCALL_ERR(cocall_args, EPERM);

	while((cocall_args->ns_cap = atomic_load_explicit(&global_namespace, memory_order_acquire)) == NULL) {
		/* TODO-PBB: should consider returning and having two calls here instead */
		// this implementation may not do what we want (priority inversion?)
		sched_yield();
	}
	/* 
	 * You give me: a capability for the global namespace, and scbs for coinsert and coselect 
	 * I give you: scb capability for codiscover 
	 */
	cocall_args->coinsert = coinsert_scb;
	cocall_args->coselect = coselect_scb;
	atomic_store_explicit(&codiscover_scb, cocall_args->codiscover, memory_order_release);

	COCALL_RETURN(cocall_args, 0);
}

void ipcd_init(cocall_args_t *cocall_args, void *token)
{
	if(!authenticate_dance(cocall_args, IPCD_SELECTOR))
		COCALL_ERR(cocall_args, EPERM);

	while((atomic_load_explicit(&coproc_inited, memory_order_acquire) == false)) {
		/* TODO-PBB: should consider returning and having two calls here instead */
		// this implementation may not do what we want (priority inversion?)
		sched_yield();
	}
	/* 
	 * You give me: a capability for the global namespace, scb capabilities for codiscover, coinsert and coselect
	 */ 
	cocall_args->ns_cap = global_namespace;
	cocall_args->codiscover = codiscover_scb;
	cocall_args->coinsert = coinsert_scb;
	cocall_args->coselect = coselect_scb;

	cocall_args->done_scb = done2_worker_scb;
	
	COCALL_RETURN(cocall_args, 0);
}

void coproc_init_complete(cocall_args_t *cocall_args, void *token)
{
	if(!authenticate_dance(cocall_args, NSD_SELECTOR))
		COCALL_ERR(cocall_args, EPERM);
	else if (atomic_load_explicit(&coproc_inited, memory_order_acquire) == true)
		COCALL_ERR(cocall_args, EAGAIN);

	atomic_store_explicit(&coproc_inited, true, memory_order_release);

	COCALL_RETURN(cocall_args, 0);
}

void ukern_init_complete(cocall_args_t *cocall_args, void *token)
{
	if(!authenticate_dance(cocall_args, IPCD_SELECTOR))
		COCALL_ERR(cocall_args, EPERM);
	else if (atomic_load_explicit(&ukern_inited, memory_order_acquire) == true)
		COCALL_ERR(cocall_args, EAGAIN);

	atomic_store_explicit(&ukern_inited, true, memory_order_release);

	COCALL_RETURN(cocall_args, 0);
}

void coproc_user_init(cocall_args_t *cocall_args, void *token)
{
	UNUSED(token);
	if (atomic_load_explicit(&ukern_inited, memory_order_acquire) == false) {
		/* nsd and coserviced have not yet completed their startup routines */
		COCALL_ERR(cocall_args, EAGAIN);
	}

	cocall_args->ns_cap = global_namespace;
	cocall_args->codiscover = codiscover_scb;
	cocall_args->coinsert = coinsert_scb;
	cocall_args->coselect = coselect_scb;
	
	COCALL_RETURN(cocall_args, 0);
}