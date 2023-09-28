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
#include "coopen.h"
#include "coport_table.h"

#include "ipcd.h"
#include "ipcd_cap.h"

#include <assert.h>
#include <comsg/comsg_args.h>
#include <comsg/coport.h>
#include <comsg/utils.h>
#include <stdlib.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>
#include <sys/errno.h>
#include <sys/queue.h>

extern void begin_cocall(void);
extern void end_cocall(void);

#ifdef COPIPE_PTHREAD

static pthread_mutexattr_t copipe_mtx_attr;
static pthread_condattr_t copipe_cnd_attr;

__attribute__((constructor)) static void
init_copipe_sync_attributes(void)
{
	pthread_mutexattr_init(&copipe_mtx_attr);
	pthread_mutexattr_setpshared(&copipe_mtx_attr, PTHREAD_PROCESS_SHARED);
	pthread_mutexattr_setprotocol(&copipe_mtx_attr, PTHREAD_PRIO_INHERIT);
	pthread_mutexattr_setrobust(&copipe_mtx_attr, PTHREAD_MUTEX_ROBUST);
	pthread_mutexattr_settype(&copipe_mtx_attr, PTHREAD_MUTEX_ERRORCHECK);

	pthread_condattr_init(&copipe_cnd_attr);
	pthread_condattr_setpshared(&copipe_cnd_attr, PTHREAD_PROCESS_SHARED);
	pthread_condattr_setclock(&copipe_cnd_attr, CLOCK_MONOTONIC);
}
#endif

int validate_coopen_args(coopen_args_t *cocall_args)
{
	if (cocall_args->coport_type != COPIPE && cocall_args->coport_type != COCARRIER && cocall_args->coport_type != COCHANNEL)
		return (0);
	else
		return (1);
}

static void
init_coport(coport_t *port, coport_type_t type) 
{
	size_t buf_perms;
	port->type = type;
	
	port->info = malloc(sizeof(coport_info_t));
	port->info = cheri_andperm(port->info, COPORT_INFO_PERMS);
	port->info->start = 0;
	port->info->end = 0;
	port->info->status = COPORT_OPEN;

	port->buffer = malloc(sizeof(coport_buf_t));
	port->cd = malloc(sizeof(coport_typedep_t));
	buf_perms = COCHANNEL_BUF_PERMS;
	switch (port->type)
	{
		case COPIPE:
			port->info->length = CHERICAP_SIZE;
			port->info->event = NOEVENT;
			port->buffer->buf = NULL;
			port->buffer = cheri_andperm(port->buffer, COPIPE_BUFFER_PERMS);
#ifdef COPIPE_PTHREAD
			pthread_mutex_init(&port->cd->pipe_lock, &copipe_mtx_attr);
			pthread_cond_init(&port->cd->pipe_wakeup, &copipe_cnd_attr);
			port->cd->waiting = false;
#endif
			break;
		case COCARRIER:
			LIST_INIT(&port->cd->listeners);
			port->cd->levent = NOEVENT;
			buf_perms = COCARRIER_BUF_PERMS;
		case COCHANNEL: 
			port->info->length = 0;
			port->info->event = COPOLL_INIT_EVENTS;
			port->buffer->buf = malloc(COPORT_BUF_LEN);
			port->buffer->buf = cheri_andperm(port->buffer->buf, buf_perms);
			port->buffer = cheri_andperm(port->buffer, DEFAULT_BUFFER_PERMS);
			break;
		default:
			//should not be reached
			break;
	}
	//pre-allocate cocarrier structs
	if (port->type == COCARRIER) {
		void **cocarrier_msg_buf = (void **)port->buffer->buf;
		for (size_t i = 0; i < COCARRIER_SIZE; i++) {
			cocarrier_msg_buf[i] = calloc(1, sizeof(struct cocarrier_message));
		}
	}

	atomic_thread_fence(memory_order_release);
	/* To synchronise with acquires in send/recv operations */
}

void coport_open(coopen_args_t *cocall_args, void *token)
{
	UNUSED(token);
	coport_t *port_handle;

	begin_cocall();
	if(!can_allocate_coport(cocall_args->coport_type)) {
		end_cocall();
		COCALL_ERR(cocall_args, ENOMEM);
	}
	port_handle = allocate_coport(cocall_args->coport_type);
	if (port_handle == NULL) {
		end_cocall();
		COCALL_ERR(cocall_args, ENOMEM);
	}

	init_coport(port_handle, cocall_args->coport_type);

	port_handle = cheri_andperm(port_handle, COPORT_PERMS);
	port_handle = seal_coport(port_handle);
	cocall_args->port = port_handle;

	end_cocall();
	COCALL_RETURN(cocall_args, 0);
}