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

#include "ukern/cocall_args.h"

#include <err.h>
#include <errno.h>
#include <stdatomic.h>
#include <sys/types.h>
#include <unistd.h>

static _Atomic(void *) codiscover_scb;
static _Atomic(coservice_t *) codiscover;

static _Atomic(namespace_t *) global_namespace = NULL;
static _Atomic(void *) coinsert_scb;

#define NSD_SELECTOR 1
#define COSERVICED_SELECTOR 2
#define IPCD_SELECTOR 3

//TODO-PBB: apply memory ordering that gives something sensible
static
int authenticate_dance(cocall_args_t *cocall_args, int selector)
{
	pid_t caller_pid, daemon_pid;
	int error = cogetpid(&caller_pid);
	switch (selector)
	{
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
	
	if (daemon_pid == 0) {
		cocall_args->status = -1;
		cocall_args->error = EAGAIN;
		return (0);
	}
	else if (caller_pid != daemon_pid) {
		cocall_args->status = -1;
		cocall_args->error = EPERM;
		return (0);
	}
	return (1);
}

void nsd_init(cocall_args_t *cocall_args, void *token)
{
	if(!authenticate_dance(cocall_args, NSD_SELECTOR))
		return;
	/* I give you: a global namespace capability, a scb capability for create nsobj (coinsert) */
	atomic_store(&coinsert_scb, cocall_args->coinsert);
	atomic_store(&global_namespace, cocall_args->namespace);
	/* clear old codiscover; whether valid or not it refers to the old universe */
	atomic_store(&codiscover_scb, NULL);
	/* Wait until coserviced is (re) ininitalized */
	while(atomic_load(&codiscover_scb) == NULL) {
		/* TODO-PBB: should consider returning and having two calls here instead */
		// this implementation may not do what we want
		sched_yield();
	}

	/* You give me: scb capabilities for codiscover and coprovide */
	cocall_args->codiscover = atomic_load(&codiscover_scb);

	cocall_args->status = 0;
	cocall_args->error = 0;
	return;
}


void coserviced_init(cocall_args_t * cocall_args, void *token)
{
	if(!authenticate_dance(cocall_args, COSERVICED_SELECTOR))
		return;
	/* 
	 * You give me: a capability for the global namespace, and a scb capability for create nsobj 
	 * I give you: scb capabilities for codiscover and coprovide
	 */
	if (atomic_load(&global_namespace) == NULL) {
		/* nsd has not yet started */
		cocall_args->status = -1;
		cocall_args->error = EAGAIN;
		return;
	}
	cocall_args->namespace = atomic_load(&global_namespace);
	cocall_args->coinsert = atomic_load(&coinsert_scb);
	atomic_store(&codiscover_scb, cocall_args->codiscover);

	cocall_args->status = 0;
	cocall_args->error = 0;
	return;
}

void ipcd_init(cocall_args_t * cocall_args, void *token)
{
	if(!authenticate_dance(cocall_args, IPCD_SELECTOR))
		return;
	/* 
	 * You give me: a capability for the global namespace, scb capabilities for codiscover and coprovide
	 */ 
	if (atomic_load(&global_namespace) == NULL) {
		/* nsd has not yet started */
		cocall_args->status = -1;
		cocall_args->error = EAGAIN;
		return;
	}
	cocall_args->namespace = atomic_load(&global_namespace);
	cocall_args->codiscover = atomic_load(&codiscover_scb);
	cocall_args->coprovide = atomic_load(&coprovide_scb);

	cocall_args->status = 0;
	cocall_args->error = 0;
	return;
}
