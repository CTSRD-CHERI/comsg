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
#include "corecv.h"
#include "ipcd.h"
#include "ipcd_cap.h"
#include "copoll_utils.h"

#include <comsg/comsg_args.h>
#include <comsg/coport.h>
#include <comsg/utils.h>

#include <sys/errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>

int 
validate_corecv_args(corecv_args_t *cocall_args)
{
	if (!valid_cocarrier(cocall_args->cocarrier))
		return (0);
	else 
		return (1);
}

static void 
cocarrier_recv(corecv_args_t *cocall_args, void *token) 
{
	UNUSED(token);
	coport_t *cocarrier;
	struct cocarrier_message **cocarrier_buf, *msg;
	coport_eventmask_t event;
	coport_status_t status;
	size_t port_len, index, new_len;
	bool closing;

	cocarrier = unseal_coport(cocall_args->cocarrier);
	cocarrier_buf = cocarrier->buffer->buf;

	status = COPORT_OPEN;
	closing = false;
	while(!atomic_compare_exchange_weak_explicit(&cocarrier->info->status, &status, COPORT_BUSY, memory_order_acq_rel, memory_order_relaxed)) {
		switch (status) {
		case COPORT_CLOSED:
			COCALL_ERR(cocall_args, EPIPE);
			break; /* NOTREACHED */
		case COPORT_CLOSING:
			closing = true;
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
		atomic_store_explicit(&cocarrier->info->status, COPORT_OPEN, memory_order_release);
		COCALL_ERR(cocall_args, EAGAIN);
	}

	new_len = port_len - 1;
	index = cocarrier->info->start;
	cocarrier->info->start = (index + 1) % COCARRIER_SIZE;
	cocarrier->info->length = new_len;

	msg = cocarrier_buf[index];
	if (!closing)
		event |= COPOLL_OUT;
	if (new_len == 0)
		event &= ~(COPOLL_RERR | COPOLL_IN);
	else 
		event &= ~COPOLL_RERR;
	cocarrier->info->event = event;
	/* Restore status value (might be COPORT_CLOSING or COPORT_OPEN) */
	atomic_store_explicit(&cocarrier->info->status, status, memory_order_release);

	cocall_args->message = __builtin_cheri_perms_and(msg->buf, COCARRIER_MSG_PERMS);
	cocall_args->length = __builtin_cheri_length_get(msg->buf);
	if (msg->attachments != NULL)
		cocall_args->oob_data.attachments = __builtin_cheri_perms_and(msg->attachments, COCARRIER_OOB_PERMS);
	else
		cocall_args->oob_data.attachments = NULL;
	cocall_args->oob_data.len = msg->nattachments;
	atomic_thread_fence(memory_order_seq_cst);
	atomic_store(&msg->recvd, true);

	copoll_notify(cocarrier, COPOLL_OUT);
	COCALL_RETURN(cocall_args, cheri_getlen(cocall_args->message));
}

void coport_recv(coopen_args_t *cocall_args, void *token)
{
	switch (coport_gettype(cocall_args->cocarrier)) {
	case COCARRIER:
		cocarrier_recv(cocall_args, token);
		break;
	case COPIPE:
		COCALL_ERR(cocall_args, ENOSYS);
		break;
	default:
		COCALL_ERR(cocall_args, ENOSYS);
		break;
	}
	//return/error values set by type-specific handler functions or by fallback case
}
