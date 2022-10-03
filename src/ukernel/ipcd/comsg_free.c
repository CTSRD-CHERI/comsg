/*
 * Copyright (c) 2022 Peter S. Blandford-Baker
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
#include "comsg_free.h"
#include "ipcd.h"
#include "ipcd_cap.h"
#include "coport_table.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <ccmalloc.h>
#include <comsg/comsg_args.h>
#include <comsg/coport.h>
#include <comsg/utils.h>

#include <sys/errno.h>

int 
validate_comsg_free_args(cosend_args_t *cocall_args)
{
	coport_t *port = cocall_args->cocarrier;
	if (!valid_coport(port))
		return (0);
	else if (!__builtin_cheri_tag_get(cocall_args->message))
		return (0);
	return (1);
}

void
free_comsg(cosend_args_t *cocall_args, void *token) 
{
	UNUSED(token);
	coport_t *coport;
	void *msg, *buf;
	struct cocarrier_message **cocarrier_buf;

	/* TODO-PBB: check permissions */
	coport = unseal_coport(cocall_args->cocarrier);
	msg = cocall_args->message;
	cocarrier_buf = coport->buffer->buf;
	for (size_t i = 0; i < COCARRIER_SIZE; i++) {
		if (!atomic_load(&cocarrier_buf[i]->recvd))
			continue;
		else if (atomic_load(&cocarrier_buf[i]->freed))
			continue;
		buf = cocarrier_buf[i]->buf;
		if ((buf == msg) && (__builtin_cheri_length_get(buf) == __builtin_cheri_length_get(msg))) {
			cocall_free(msg);
			atomic_store(&cocarrier_buf[i]->freed, true);
			COCALL_RETURN(cocall_args, 0);
		}
	}
	COCALL_ERR(cocall_args, EINVAL);
}
