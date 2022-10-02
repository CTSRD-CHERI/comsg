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
#include "coclose.h"
#include "ipcd_cap.h"
#include "coport_table.h"

#include <comsg/comsg_args.h>
#include <comsg/coport.h>
#include <comsg/utils.h>

#include <sys/errno.h>

int 
validate_coclose_args(coclose_args_t *cocall_args)
{
	coport_t *port = cocall_args->port;
	if (!valid_coport(port))
		return (0);
	return (1);
}

void
coport_close(coclose_args_t *cocall_args, void *token) 
{
	UNUSED(token);
	coport_t *coport;
	coport_status_t status;
	coport_eventmask_t events;

	/* TODO-PBB: check permissions */
	coport = unseal_coport(cocall_args->port);
	
	status = coport->info->status;
	if (status == COPORT_CLOSED || status == COPORT_CLOSING)
		COCALL_ERR(cocall_args, EPIPE);

	status = COPORT_OPEN;
	while(!atomic_compare_exchange_weak_explicit(&coport->info->status, &status, COPORT_BUSY, memory_order_acq_rel, memory_order_relaxed)) {
		switch (status) {
		case COPORT_CLOSED:
		case COPORT_CLOSING:
			COCALL_ERR(cocall_args, EPIPE);
			break; /* NOTREACHED */
		default:
			status = COPORT_OPEN;
			break;
		}
	}
	events = coport->info->event;
	events &= ~(COPOLL_OUT);
	events |= COPOLL_CLOSED;
	coport->info->event = events;

	atomic_store_explicit(&coport->info->status, COPORT_CLOSING, memory_order_release);

	COCALL_RETURN(cocall_args, 0);
}
