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
#include "codiscover.h"
#include "coservice_cap.h"
#include "coservice_table.h"

#include <cocall/cocall_args.h>
#include <coproc/coservice.h>
#include <coproc/utils.h>

#include <sys/errno.h>

int validate_codiscover_args(codiscover_args_t *cocall_args)
{
	nsobject_t *obj = cocall_args->nsobj;
	coservice_t *service_handle = obj->coservice;
	if(obj == NULL)
		return (0);
	else if (cheri_getsealed(obj))
		return (0);
	else if (obj->type != COSERVICE)
		return (0);
	else if ((cheri_getperm(service_handle) & (COSERVICE_CODISCOVER_PERMS)) == 0)
		return (0);
	else if (cheri_getlen(obj) < sizeof(nsobject_t))
		return (0);
	else if (cheri_getlen(service_handle) < sizeof(coservice_t))
		return (0);
	else if (!in_table(service_handle))
		return (0);
	else
		return (1);
}

void discover_coservice(codiscover_args_t *cocall_args, void *token)
{
	UNUSED(token);

	coservice_t *service;

	service = cocall_args->nsobj->coservice;
	cocall_args->coservice = service;
	service = unseal_coservice(service);

	cocall_args->scb_cap = get_coservice_scb(service);
	
	COCALL_RETURN(cocall_args, 0);
}
