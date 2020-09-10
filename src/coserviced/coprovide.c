/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
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

#include "coservice_table.h"

#include "ukern/ccmalloc.h"
#include "ukern/cocall_args.h"
#include "ukern/namespace.h"
#include "ukern/utils.h"

#include <cheri/cheric.h>
#include <ctype.h>
#include <string.h>


int validate_coprovide_args(coprovide_args_t *args)
{

	/* 
	 * The point of this function is to ensure that the arguments passed from 
	 * a userspace caller will not crash the microkernel program. If they will, 
	 * we reject them here and return to the user.
	 * The only type of error return from failing these checks is EINVAL.
	 * Other checks, e.g. for permissions, should happen elsewhere.
	 */
	if(!valid_ns_name(args->name))
		return 0;
	else if (!valid_ns_cap(args->ns_cap))
		return 0;
	else if(args->nworkers <= 0)
		return 0;
	else if(!valid_scb(args->handler_scb))
		return 0;
	else if(cheri_getlen(args->worker_scbs) < (CHERICAP_SIZE * args->nworkers))
		return 0;
	else {
		for(int i = 0; i < args->nworkers; i++) 
			if(!valid_scb(args->worker_scbs[i]))
				return (0);
	}

	return 1;
}

void provide_coservice(coprovide_args_t *cocall_args, void *token)
{
	UNUSED(token);
	coservice_t *coservice_ptr = allocate_coservice();

	coservice_ptr->next_worker = 0;
	coservice_ptr->nworkers = cocall_args->nworkers;
	coservice_ptr->workers = cocall_calloc(cocall_args->nworkers, sizeof(worker_args_t));
	memcpy(coservice_ptr->workers, cocall_args->workers, cheri_getlen(cocall_args->workers));

	cocall_args->service = create_coservice_handle(coservice_ptr);
	cocall_args->status = 0;
	cocall_args->error = 0;

	coservice_ptr = NULL;
	return;
}

