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
#include "copoll_utils.h"

#include "ukern/cocall_args.h"
#include "ukern/coport.h"
#include "ukern/utils.h"

#include <stdatomic.h>
#include <stddef.h>

int 
validate_corecv_args(corecv_args_t *cocall_args)
{
	if (!valid_cocarrier(cocall_args->cocarrier))
		return (0);
	else 
		return (1);
}

void 
cocarrier_recv(corecv_args_t *cocall_args, void *token) 
{
	UNUSED(token);
	coport_t *cocarrier;
	void **cocarrier_buf;
	coport_eventmask_t event;
	coport_status_t status;
	size_t port_len, index, new_len;

	cocarrier = unseal_coport(cocall_args->cocarrier);
	cocarrier_buf = cocarrier->buffer->buf;

	status = COPORT_OPEN;
	while(!atomic_compare_exchange_weak_explicit(&cocarrier->info->status, &status, COPORT_BUSY, memory_order_acq_rel, memory_order_relaxed)) {
		switch (status) {
		case COPORT_CLOSED:
			COCALL_ERR(cocall_args, EPIPE);
			break; /* NOTREACHED */
		case COPORT_CLOSING:
			break;
		default:
			status = COPORT_OPEN;
			break;
		}
	}
	event = cocarrier->info->event;
	port_len = cocarrier->info->length;

	if(port_len == 0 || ((event & COPOLL_IN) == 0)) {
		cocarrier->info->event = (event | COPOLL_RERR);
		atomic_store_explicit(&cocarrier->status, COPORT_OPEN, memory_order_release);
		COCALL_ERR(cocall_args, EAGAIN);
	}

	new_len = port_len - 1;
	index = cocarrier->info->start;
	cocarrier->info->start = (index + 1) % COCARRIER_SIZE;
	cocarrier->info->length = new_len;

	cocall_args->message = cocarrier_buf[index];

	if (new_len == 0)
		event = ((COPOLL_OUT | event) & ~(COPOLL_RERR | COPOLL_IN));
	else 
		event = ((COPOLL_OUT | event) & ~COPOLL_RERR);
	cocarrier->info->event = event;
	/* Restore status value (might be COPORT_CLOSING or COPORT_OPEN) */
	atomic_store_explicit(&cocarrier->info->status, status, memory_order_release);

	copoll_notify(cocarrier);
	COCALL_RETURN(cocall_args, cheri_getlen(cocall_args->message));
}

