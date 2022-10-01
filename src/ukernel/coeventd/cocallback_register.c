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
#include "cocallback_register.h"

#include "cocallback_func_utils.h"
#include "procdeath_tbl.h"
#include "coevent_utils.h"

#include <assert.h>
#include <ccmalloc.h>
#include <cheri/cheric.h>
#include <comsg/comsg_args.h>
#include <comsg/coevent.h>
#include <sys/errno.h>
#include <unistd.h>

static void add_func_to_provider(coevent_t *, cocallback_func_t *);
static coevent_t *monitor_provider(pid_t);

int
validate_cocallback_register(comsg_args_t *cocall_args)
{
	void *provider_scb;

	//todo-pbb: add ability to properly verify if something's an scb
	provider_scb = cocall_args->provider_scb;
	if (cheri_gettag(provider_scb) == 0)
		return (0);
	else if (cheri_getsealed(provider_scb) == 0)
		return (0);
	else
		return (1);
}

static coevent_t *
monitor_provider(pid_t pid)
{
	cocallback_t *cocallback;
	coevent_t *provider_death;
	cocallback_func_t *handle_death;
	struct cocallback_args args;
	int error;

	/* 
	 * if this is the first cocallback function registered by this provider,
	 * install event monitoring for if this provider dies, so we can clean
	 * up our internal state.
	 */
	provider_death = allocate_procdeath_event(pid);
	lock_coevent(provider_death); /* can block */
	if (STAILQ_EMPTY(&provider_death->callbacks)) {
		args.len = 0;
		SLIST_INIT(&args.provided_funcs);
		handle_death = get_provdeath_func();
		cocallback = add_cocallback(provider_death, handle_death, &args);
	}
	unlock_coevent(provider_death);

	return (provider_death);
}

static void
add_func_to_provider(coevent_t *provider, cocallback_func_t *func)
{
	cocallback_t *ccb;
	
	lock_coevent(provider);
	ccb = STAILQ_FIRST(&provider->callbacks);
	assert(ccb->func == get_provdeath_func());
	SLIST_INSERT_HEAD(&ccb->args.provided_funcs, func, next);
	unlock_coevent(provider);
}

/*
 * provider_scb - cocallback function
 * flags - currently only FLAG_SLOCALL is supported
 */
void 
cocallback_register(comsg_args_t *cocall_args, void *token)
{
	coevent_t *provider_death;
	cocallback_func_t *ccb_func;
	pid_t pid;

	/* XXX-PBB: this functionality (cogetpid2) doesn't exist in cheribsd in non-private branches */
	pid = cogetpid2();

	assert(pid > 0);
	provider_death = monitor_provider(pid);
	ccb_func = register_cocallback_func(pid, cocall_args->provider_scb, cocall_args->flags);
	if (ccb_func == NULL)
		COCALL_ERR(cocall_args, EINVAL);
	add_func_to_provider(provider_death, ccb_func);

	cocall_args->ccb_func = ccb_func;
	COCALL_RETURN(cocall_args, 0);
}