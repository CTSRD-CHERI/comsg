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
#include "ipcd_cap.h"

#include "ukern/ccmalloc.h"
#include "ukern/cocall_args.h"
#include "ukern/coport.h"
#include "ukern/utils.h"

#include <cheri/cheric.h>
#include <cheri/cherireg.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/queue.h>

//TODO-PBB: better definition of these
#define COCARRIER_MAX_LEN (1024 * 1024)
#define COCARRIER_PERMS ( CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP )

int validate_cosend_args(coopen_args_t *cocall_args)
{
	if (cocall_args->length > COCARRIER_MAX_LEN)
		return (0);
	else if (cheri_getlen(cocall_args->message))
		return (0);
	else if (!valid_cocarrier(cocall_args->cocarrier))
		return (0);
	else 
		return (1);
}

void cocarrier_send(coopen_args_t *cocall_args, void *token)
{
	UNUSED(token);
	coport_t *cocarrier;
	void **cocarrier_buf;
	size_t port_len, index, new_len;
	coport_eventmask_t event;
	coport_status_t status = COPORT_OPEN;

	size_t msg_len = MIN(cocall_args->length, cheri_getlen(cocall_args->message));
	char *msg_buffer = cocall_malloc(msg_len);

	memcpy(msg_buffer, cocall_args->message, msg_len);

	cocarrier = unseal_coport(cocall_args->cocarrier);
	cocarrier_buf = cocarrier->buffer;

	/* Set the status to busy so we don't interleave.*/
	/* We are not expecting high contention, and we can't sched_yield inside cocalls without slowdown */
	while(!atomic_compare_exchange_strong_explicit(&cocarrier->status, &status, COPORT_BUSY, memory_order_acq_rel, memory_order_acquire))
		status = COPORT_OPEN;
	atomic_thread_fence(memory_order_acquire);
	event = cocarrier->event;
	port_len = cocarrier->length;

	if(port_len >= COCARRIER_SIZE || !(event & COPOLL_OUT)) {
        cocarrier->event = (event | COPOLL_WERR);
        atomic_thread_fence(memory_order_release);
        atomic_store_explicit(&cocarrier->status, COPORT_OPEN, memory_order_release);
        COCALL_ERR(cocall_args, EAGAIN);
    }

    index = (cocarrier->end);
    cocarrier->end = (index + 1) % COCARRIER_SIZE;
    new_len = port_len + 1;
    cocarrier->length = new_len;

    cocarrier_buf[index] = msg_buffer;

    if(new_len == COCARRIER_SIZE)
    	cocarrier->event = ((COPOLL_IN | event) & ~(COPOLL_WERR | COPOLL_OUT));
    else
        cocarrier->event = (COPOLL_IN | event) & ~COPOLL_WERR;
    atomic_thread_fence(memory_order_release);
    /* Should synchronise after this point */
    /* Not sure if this emits the right fence, but I'm pretty sure it does? */
    /* The fields accessed between these fences are only accessed here (excluding initialisation) */
    atomic_store_explicit(&cocarrier->status, COPORT_OPEN, memory_order_release);

    //check if anyone is waiting on messages to arrive
    //TODO-PBB: Adapt for new structure of ipc daemon
    if(!LIST_EMPTY(&cocarrier->listeners))
        pthread_cond_signal(&global_cosend_cond);

    COCALL_RETURN(cocall_args, msg_len);
}