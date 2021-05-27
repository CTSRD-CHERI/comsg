/*
 * Copyright (c) 2020, 2021 Peter S. Blandford-Baker
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
#include "core.h"

#include "../daemon.h"

#include <cocall/cocall_args.h>
#include <coproc/utils.h>

#include <assert.h>
#include <err.h>
#include <sys/errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>

daemon_setup_info coprocd_init = { 
	coproc_user_init, NULL, NULL, 0
};

daemon_setup_info nsd_init = {
	nsd_setup, nsd_setup_complete, NULL, 0
};

daemon_setup_info coserviced_init = {
	coserviced_setup, NULL, get_coserviced_capv, 3
};

daemon_setup_info ipcd_init = {
	NULL, ipcd_setup_complete, get_ipcd_capv, 4
};

daemon_setup_info coeventd_init = {
	NULL, coeventd_setup_complete, get_coeventd_capv, 4
};

module_setup_info core_init = {
	core_init_start, core_init_complete
};

/* Provided by coservice daemon*/
static _Atomic(void *) codiscover_scb = NULL;

/* Provided by namespace daemon */
static _Atomic(namespace_t *) global_namespace = NULL;
static void *coinsert_scb = NULL;
static void *coselect_scb = NULL;

static _Atomic bool coproc_inited = false;
static _Atomic bool ukern_inited = false;

struct ukernel_daemon *nsd, *coserviced, *ipcd, *coeventd;

void
core_init_start(struct ukernel_module *m)
{
	nsd = &m->daemons[1];
	coserviced = &m->daemons[2];
	coeventd = &m->daemons[3];
	ipcd = &m->daemons[4];
}

void
core_init_complete(void)
{
	return;
}

/* unused */
void
core_fini(void)
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

void
nsd_setup(cocall_args_t *cocall_args, void *token)
{
	UNUSED(token);
	/* clear old codiscover; whether valid or not it refers to the old universe */
	codiscover_scb = NULL;
	/* I give you: a global namespace capability, a scb capability for create nsobj (coinsert) and lookup nsobj (coselect) */
	coinsert_scb = cocall_args->coinsert;
	coselect_scb = cocall_args->coselect;
	atomic_store_explicit(&global_namespace, cocall_args->ns_cap, memory_order_release);

	nsd->status = CONTINUING;
	
	COCALL_RETURN(cocall_args, 0);
}

void
coserviced_setup(cocall_args_t *cocall_args, void *token)
{
	UNUSED(token);
	while((cocall_args->ns_cap = atomic_load_explicit(&global_namespace, memory_order_acquire)) == NULL) {
		/* TODO-PBB: should consider returning and having two calls here instead */
		// this implementation may not do what we want (priority inversion?)
		sched_yield();
	}
	/* 
	 * I give you: scb capability for codiscover 
	 */
	atomic_store_explicit(&codiscover_scb, cocall_args->codiscover, memory_order_release);

	COCALL_RETURN(cocall_args, 0);
}

void
nsd_setup_complete(cocall_args_t *cocall_args, void *token)
{
	UNUSED(token);
	if (atomic_load_explicit(&coproc_inited, memory_order_acquire) == true)
		COCALL_ERR(cocall_args, EAGAIN);

	atomic_store_explicit(&coproc_inited, true, memory_order_release);
	
	nsd->status = RUNNING;
	coserviced->status = RUNNING;

	COCALL_RETURN(cocall_args, 0);
}

void
coeventd_setup_complete(cocall_args_t *cocall_args, void *token)
{
	UNUSED(token);
	if (coeventd->status == RUNNING)
		COCALL_ERR(cocall_args, EAGAIN);

	coeventd->status = RUNNING;

	COCALL_RETURN(cocall_args, 0);
}

void
ipcd_setup_complete(cocall_args_t *cocall_args, void *token)
{
	UNUSED(token);
	if (atomic_load_explicit(&ukern_inited, memory_order_acquire) == true)
		COCALL_ERR(cocall_args, EAGAIN);

	atomic_store_explicit(&ukern_inited, true, memory_order_release);

	ipcd->status = RUNNING;

	COCALL_RETURN(cocall_args, 0);
}

void
coproc_user_init(cocall_args_t *cocall_args, void *token)
{
	UNUSED(token);
	if (!atomic_load_explicit(&ukern_inited, memory_order_acquire)) {
		/* nsd and coserviced have not yet completed their startup routines */
		COCALL_ERR(cocall_args, EAGAIN);
	}

	cocall_args->ns_cap = global_namespace;
	cocall_args->codiscover = codiscover_scb;
	cocall_args->coinsert = coinsert_scb;
	cocall_args->coselect = coselect_scb;
	
	COCALL_RETURN(cocall_args, 0);
}

void
get_coserviced_capv(struct coexecve_capvec *capvec)
{
	assert(global_namespace != NULL);
	assert(coinsert_scb != NULL);
	assert(coselect_scb != NULL);

	capvec_append(capvec, global_namespace);
	capvec_append(capvec, coinsert_scb);
	capvec_append(capvec, coselect_scb);
}

void
get_coeventd_capv(struct coexecve_capvec *capvec)
{
	assert(coproc_inited);
	assert(global_namespace != NULL);
	assert(codiscover_scb != NULL);
	assert(coinsert_scb != NULL);
	assert(coselect_scb != NULL);

	capvec_append(capvec, global_namespace);
	capvec_append(capvec, codiscover_scb);
	capvec_append(capvec, coinsert_scb);
	capvec_append(capvec, coselect_scb);
}

void
get_ipcd_capv(struct coexecve_capvec *capvec)
{
	assert(coproc_inited);
	assert(global_namespace != NULL);
	assert(codiscover_scb != NULL);
	assert(coinsert_scb != NULL);
	assert(coselect_scb != NULL);

	capvec_append(capvec, global_namespace);
	capvec_append(capvec, codiscover_scb);
	capvec_append(capvec, coinsert_scb);
	capvec_append(capvec, coselect_scb);
}