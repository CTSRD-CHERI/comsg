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
#include "cosend.h"
#include "ipcd.h"
#include "ipcd_cap.h"
#include "copoll_utils.h"

#include <comsg/comsg_args.h>
#include <comsg/coport.h>
#include <comsg/utils.h>

#include <cheri/cheric.h>
#include <cheri/cherireg.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <err.h>

extern void begin_cocall(void);
extern void end_cocall(void);

int validate_cosend_args(coopen_args_t *cocall_args)
{
	if (cocall_args->length > COCARRIER_MAX_MSG_LEN)
		return (0);
	else if (cheri_gettag(cocall_args->message) == 0)
		return (0);
	else if (cheri_getlen(cocall_args->message) > COCARRIER_MAX_MSG_LEN)
		return (0);
	else if (!valid_cocarrier(cocall_args->cocarrier))
		return (0);
	else if (cocall_args->oob_data.len != 0) {
		if (cheri_gettag(cocall_args->oob_data.attachments) == 0)
			return (0);
		else if (cheri_getlen(cocall_args->oob_data.attachments) < cocall_args->oob_data.len * sizeof(comsg_attachment_t))
			return (0);
	}
	return (1);
}

static inline void
clear_attachment(comsg_attachment_t *ptr)
{
	ptr->type = ATTACHMENT_INVALID;
	memset(&ptr->item, '\0', sizeof(comsg_attachment_handle_t));
}

comsg_attachment_t *
handle_attachments(comsg_attachment_t *attachments, size_t len)
{
	comsg_attachment_t *attachment;
	comsg_attachment_t *attachment_buf;

	if (len == 0)
		return (NULL);

	//copy into ukernel-owned buffer 
	attachment_buf = calloc(len, sizeof(comsg_attachment_t));
	memcpy(attachment_buf, attachments, len * sizeof(comsg_attachment_t));

	//perform some basic checks and remove any items that fail
	for (size_t i = 0; i < len; i++) {
		attachment = &attachment_buf[i];
		switch (attachment->type) {
		case ATTACHMENT_COPORT:
			if (!valid_cocarrier(attachment->item.coport)) 
				clear_attachment(attachment);
			break;
		case ATTACHMENT_RESERVATION:
			//expand checks
			/*
			if (!__builtin_cheri_tag_get(attachment->item.reservation))
				clear_attachment(attachment);
			else if (!__builtin_cheri_sealed_get(attachment->item.reservation))
				clear_attachment(attachment);
			else if (__builtin_cheri_length_get(attachment->item.reservation) != sizeof(nsobject_t))
				clear_attachment(attachment);*/
			break;
		case ATTACHMENT_COEVENT:
			if (!__builtin_cheri_tag_get(attachment->item.coevent))
				clear_attachment(attachment);
			else if (__builtin_cheri_length_get(attachment->item.coevent) < sizeof(coevent_t))
				clear_attachment(attachment);
			break;
		default:
			clear_attachment(attachment);
			break;
		}
	}
	return (attachment_buf);
}

void cocarrier_send(coopen_args_t *cocall_args, void *token)
{
	UNUSED(token);
	coport_status_t status;
	size_t port_len, index, new_len, msg_len;
	size_t nattachments;
	coport_eventmask_t event;
	coport_t *cocarrier;
	struct cocarrier_message **cocarrier_buf;
	comsg_attachment_t *attachments;
	struct cocarrier_message *msg;
	void *msg_in, *msg_out, *msg_alloc;
	int error;
	bool locked;

	begin_cocall();

	msg_in = cheri_andperm(cocall_args->message, COPORT_INBUF_PERMS);
	msg_len = MIN(cocall_args->length, cheri_getlen(msg_in));
	msg_alloc = malloc(msg_len);
	msg_out = cheri_andperm(msg_alloc, COPORT_OUTBUF_PERMS); //ensure no tags get through here
	memcpy(msg_out, msg_in, msg_len);

	nattachments = cocall_args->oob_data.len;
	attachments = handle_attachments(cocall_args->oob_data.attachments, nattachments);
	/*if ((cheri_getlen(msg->buf) >= PAGE_SIZE) && ((cheri_getlen(msg->buf) % PAGE_SIZE) == 0)) {
		locked = true;
		error = mlock(msg->buf, cheri_getlen(msg->buf));
		if (error != 0)
			err(EX_SOFTWARE, "%s:mlock failed! args were %p, %lu", __func__, msg->buf, cheri_getlen(msg->buf));
	}*/
	locked = false;
	cocarrier = unseal_coport(cocall_args->cocarrier);
	
	/* Set the status to busy so we don't interleave.*/
	/* We are not expecting high contention, and we can't sched_yield inside cocalls without slowdown */
	status = COPORT_OPEN;
	while(!atomic_compare_exchange_weak_explicit(&cocarrier->info->status, &status, COPORT_BUSY, memory_order_acq_rel, memory_order_relaxed)) {
		switch (status) {
		case COPORT_CLOSED:
		case COPORT_CLOSING:
			/*if (locked) {
				error = munlock(msg->buf, cheri_getlen(msg->buf));
				if (error != 0)
					err(EX_SOFTWARE, "%s:munlock failed! args were %p, %lu", __func__, msg->buf, cheri_getlen(msg->buf));
			}*/
			free(msg_out);
			if (attachments != NULL)
				free(attachments);
			end_cocall();
			COCALL_ERR(cocall_args, EPIPE);
			break; /* NOTREACHED */
		default:
			status = COPORT_OPEN;
			break;
		}
	}

	cocarrier_buf = cocarrier->buffer->buf;
	event = cocarrier->info->event;
	port_len = cocarrier->info->length;

	if ((port_len >= COCARRIER_SIZE) || ((event & COPOLL_OUT) == 0)) {
		/*if (locked) {
			error = munlock(msg->buf, cheri_getlen(msg->buf));
			if (error != 0)
				err(EX_SOFTWARE, "%s:munlock failed! args were %p, %lu", __func__, msg->buf, cheri_getlen(msg->buf));
		}*/
		free(msg_out);
		if (attachments != NULL)
			free(attachments);
		event = (event | COPOLL_WERR);
        atomic_store_explicit(&cocarrier->info->event, event, memory_order_release);
        atomic_store_explicit(&cocarrier->info->status, COPORT_OPEN, memory_order_release);
		end_cocall();
        COCALL_ERR(cocall_args, EAGAIN);
    }

    index = cocarrier->info->end;
    new_len = port_len + 1;
    cocarrier->info->end = (index + 1) % COCARRIER_SIZE;
    cocarrier->info->length = new_len;

	msg = cocarrier_buf[index];
	msg->buf = msg_alloc;
	msg->attachments = attachments;
	msg->nattachments = nattachments;
	msg->freed = false;
	msg->recvd = false;

    if(new_len == COCARRIER_SIZE)
    	event = (COPOLL_IN | event) & ~(COPOLL_WERR | COPOLL_OUT);
    else
    	event = (COPOLL_IN | event) & ~COPOLL_WERR;
    cocarrier->info->event = event;
	atomic_thread_fence(memory_order_seq_cst);
    atomic_store_explicit(&cocarrier->info->status, COPORT_DONE, memory_order_release);

    copoll_notify(cocarrier, COPOLL_IN);

	/*if (locked) {
		error = munlock(msg->buf, cheri_getlen(msg->buf));
		if (error != 0)
			err(EX_SOFTWARE, "%s:munlock failed! args were %p, %lu", __func__, msg->buf, cheri_getlen(msg->buf));
	}*/
	end_cocall();
    COCALL_RETURN(cocall_args, msg_len);
}

void coport_send(coopen_args_t *cocall_args, void *token)
{
	switch (coport_gettype(cocall_args->cocarrier)) {
	case COCARRIER:
		cocarrier_send(cocall_args, token);
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