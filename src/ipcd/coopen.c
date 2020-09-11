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
#include "coport_table.h"


#include "ukern/ccmalloc.h"
#include "ukern/cocall_args.h"
#include "ukern/coport.h"

#include <cheri/cheric.h>
#include <sys/queue.h>

int validate_coopen_args(coopen_args_t *cocall_args)
{
	if (cocall_args->type != COPIPE && cocall_args->type != COCARRIER && cocall_args->type != COCHANNEL)
		return (0);
	else
		return (1);
}

static
void init_coport(coport_type_t type, coport_t *port) 
{
	port->type = type;
	
	port->info = cocall_malloc(sizeof(coport_bounds_t));
	port->info = cheri_andperm(port.info, COPORT_INFO_PERMS);
	port->info->start = 0;
	port->info->end = 0;
	port->info->status = COPORT_OPEN;

	port->buffer = cocall_malloc(sizeof(coport_buf_t));
	port->cd = cocall_malloc(sizeof(coport_typedep_t));
	switch (port->type)
	{
		case COPIPE:
			port->info->length = CHERICAP_SIZE;
			port->info->event = NOEVENT;
			port->buffer->buf = NULL;
			port->buffer = cheri_andperm(port->buffer, COPIPE_BUFFER_PERMS);
			break;
		case COCARRIER:
			LIST_INIT(&port->cd->listeners);
		case COCHANNEL: 
			port->info->length = 0;
			port->info->event = COPOLL_INIT_EVENTS;
			port->buffer->buf = cocall_malloc(COPORT_BUF_LEN);
			port->buffer = cheri_andperm(port->buffer, DEFAULT_BUFFER_PERMS);
			break;
		default:
			//should not be reached
			COCALL_ERR(cocall_args, ENOSYS);
			break;
	}
	atomic_thread_fence(memory_order_release);
	/* To synchronise with acquires in send/recv operations */
}

static 
coport_t *seal_coport(coport_t *ptr)
{
	if (ptr->type != COCARRIER)
		return (ptr);
	else //TODO-PBB: seal
		return (ptr);
}

void open_coport(coopen_args_t *cocall_args, void *token)
{
	UNUSED(token);
	//Brain-sludge is making me forget whether static is redundant here
	static _Thread_local coport_t *port_handle = NULL;

	if (port_handle == NULL) {
		if(!can_allocate_coport() && port_handle == NULL)
			COCALL_ERR(cocall_args, ENOMEM);
		port_handle = allocate_coport(cocall_args->type);
	}
	init_coport(cocall_args->type, port_handle);

	port_handle = cheri_andperm(port_handle, COPORT_PERMS);
	port_handle = seal_coport(port_handle);
	cocall_args->port = port_handle;
	port_handle = NULL;

	COCALL_RETURN(cocall_args);
}