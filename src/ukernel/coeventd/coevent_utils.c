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

#include <ccmalloc.h>
#include <coproc/coevent.h>
#include <stdatomic.h>
#include <sys/queue.h>

int
add_cocallback(struct coevent *event, struct cocallback_func *func, struct cocallback_args *args)
{
	struct cocallback *ccb;
	int in_progress;

	ccb = cocall_malloc(sizeof(struct cocallback));
	ccb->func = func;
	memcpy(&ccb->args, args, sizeof(struct cocallback_args));

	in_progress = 0;
	if (!atomic_compare_exchange_strong_explicit(&event->in_progress, &in_progress, 1, memory_order_acq_rel, memory_order_acquire)) {
		cocall_free(ccb);
		return (-1);
	}

	SLIST_INSERT_HEAD(&event->cocallbacks, ccb, next);
	atomic_fetch_add_explicit(&func->consumers, 1, memory_order_acq_rel);
	atomic_compare_exchange_strong_explicit(&event->in_progress, &in_progress, 0, memory_order_acq_rel, memory_order_acquire);

	return (0);

}

struct cocallback *
get_next_cocallback(struct coevent *event)
{
	struct cocallback *ccb;
	int in_progress;

	in_progress = 0;
	if (!atomic_compare_exchange_strong_explicit(&event->in_progress, &in_progress, -1, memory_order_acq_rel, memory_order_acquire)) {
		assert(in_progress == 1); 
	 	assert(atomic_compare_exchange_strong_explicit(&event->in_progress, &in_progress, -1, memory_order_acq_rel, memory_order_acquire));
	}
	ccb = SLIST_FIRST(&event->cocallbacks);
	if (ccb != NULL)
		SLIST_REMOVE_HEAD(&event->cocallbacks, next);
	event->ncallbacks--;
	atomic_store_explicit(&event->in_progress, 0, memory_order_release);
	return (ccb);
}

void
free_cocallback(struct cocallback *ccb)
{
	atomic_fetch_sub_explicit(&ccb->func->consumers, 1, memory_order_acq_rel);
	cocall_free(ccb);
}