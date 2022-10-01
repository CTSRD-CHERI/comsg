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
#include "coprovide.h"
#include "coservice_table.h"
#include "coservice_cap.h"

#include <ccmalloc.h>
#include <cheri/cherireg.h>
#include <comsg/comsg_args.h>
#include <comsg/namespace.h>
#include <comsg/utils.h>

#include <cheri/cheric.h>
#include <ctype.h>
#include <string.h>


int validate_coprovide_args(coprovide_args_t *cocall_args)
{
	int i;
	/* 
	 * The point of this function is to ensure that the arguments passed from 
	 * a userspace caller will not crash the microkernel program. If they will, 
	 * we reject them here and return to the user.
	 * The only type of error return from failing these checks is EINVAL.
	 * Other checks, e.g. for permissions, should happen elsewhere.
	 */
	void **scbs;
	if(cocall_args->nworkers <= 0)
		return (0);
	else if (cocall_args->nworkers > COSERVICE_MAX_WORKERS)
		return (0);
	else if(cheri_getlen(cocall_args->worker_scbs) < (CHERICAP_SIZE * cocall_args->nworkers))
		return (0);
	else {
		scbs = cocall_calloc(cocall_args->nworkers, sizeof(void *));
		memcpy(scbs, cocall_args->worker_scbs, cheri_getlen(scbs));
		for(i = 0; i < cocall_args->nworkers; i++) {
			if(!valid_scb(cocall_args->worker_scbs[i])) {
				cocall_free(scbs);
				return (0);
			}
		}
		cocall_args->worker_scbs = scbs;
	}

	return (1);
}

void provide_coservice(coprovide_args_t *cocall_args, void *token)
{
	UNUSED(token);
	int i;
	coservice_t *coservice_ptr = allocate_coservice();
	coservice_ptr->impl = allocate_endpoint();
	
	coservice_ptr->impl->next_worker = 0;
	coservice_ptr->impl->nworkers = cocall_args->nworkers;
	coservice_ptr->impl->worker_scbs = cocall_calloc(cocall_args->nworkers, sizeof(void *));
	for (i = 0; i < coservice_ptr->impl->nworkers; i++) {
		coservice_ptr->impl->worker_scbs[i] = cocall_args->worker_scbs[i];
	}

	cocall_args->service = create_coservice_handle(coservice_ptr);
	cocall_free(cocall_args->worker_scbs);
	cocall_args->worker_scbs = NULL;

	COCALL_RETURN(cocall_args, 0);
}

