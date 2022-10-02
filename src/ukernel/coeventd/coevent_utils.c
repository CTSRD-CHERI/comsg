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
#include "coevent_utils.h"
#include "procdeath_tbl.h"

#include <assert.h>
#include <ccmalloc.h>
#include <comsg/coevent.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/sched.h>

cocallback_t *
add_cocallback(coevent_t *event, cocallback_func_t *func, struct cocallback_args *args)
{
	cocallback_t *ccb;
	int in_progress;

	/* this function must only be called when we hold the in_progress lock */
	in_progress = atomic_load_explicit(&event->in_progress, memory_order_acquire);
	assert(in_progress != 0);

	ccb = cocall_malloc(sizeof(cocallback_t));
	ccb->func = func;
	memcpy(&ccb->args, args, sizeof(struct cocallback_args));

	STAILQ_INSERT_TAIL(&event->callbacks, ccb, next);
	atomic_fetch_add_explicit(&func->consumers, 1, memory_order_acq_rel);

	return (ccb);
}

void
lock_coevent(coevent_t *coevent)
{
	int in_progress;

	in_progress = 0;
	while (!atomic_compare_exchange_strong_explicit(&coevent->in_progress, &in_progress, 1, memory_order_acq_rel, memory_order_acquire)) {
		in_progress = 0;
		sched_yield();
	}
}

void
unlock_coevent(coevent_t *coevent)
{
	int in_progress;

	/* this function must only be called when we hold the in_progress lock */
	in_progress = atomic_load_explicit(&coevent->in_progress, memory_order_acquire);
	assert(in_progress != 0);
	/* assert here might help catch cases where this is called when this thread doesn't actually hold the lock */
	assert(atomic_compare_exchange_strong_explicit(&coevent->in_progress, &in_progress, 0, memory_order_acq_rel, memory_order_acquire));
}

cocallback_t *
get_next_cocallback(coevent_t *event)
{
	cocallback_t *ccb;
	int in_progress;

	/* this function must only be called when we hold the in_progress lock */
	in_progress = atomic_load_explicit(&event->in_progress, memory_order_acquire);
	assert(in_progress != 0);

	ccb = STAILQ_FIRST(&event->callbacks);
	if (ccb != NULL)
		STAILQ_REMOVE_HEAD(&event->callbacks, next);
	event->ncallbacks--;
	return (ccb);
}

void
free_cocallback(cocallback_t *ccb)
{
	atomic_fetch_sub_explicit(&ccb->func->consumers, 1, memory_order_acq_rel);
	cocall_free(ccb);
}

bool
validate_coevent(coevent_t *coevent)
{
	/* new coevent types should come before final else and return true if type-specific checks pass */
	if (__builtin_cheri_tag_get(coevent) == 0)
		return (false);
	else if (coevent->event == PROCESS_DEATH) {
		if (!is_procdeath_table_member(coevent))
			return (false);
		else
			return (true);
	} else /* invalid type */
		return (false);
}