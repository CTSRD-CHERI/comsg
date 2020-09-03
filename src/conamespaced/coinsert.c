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
#include "nsd_cap.h"
#include "nsd_crud.h"
#include "nsd_lookup.h"

#include "ukern/cocall_args.h"
#include "ukern/namespace_object.h"
#include "ukern/utils.h"

#define CLEAR_STORE_PERM(c) cheri_andperm(c, ~(CHERI_PERM_STORE | CHERI_PERM_STORE_CAP))

int validate_coinsert_args(coinsert_args_t *cocall_args)
{
	UNUSED(cocall_args);
	return (1);
}

void insert_namespace_object(coinsert_args_t *cocall_args)
{
	nsobject_t *obj;

	obj = in_namespace(cocall_args->name, cocall_args->ns_cap);
	if(obj != NULL) {
		cocall_args->status = -1;
		cocall_args->error = EEXIST;
		cocall_args->nsobj = NULL;
		return;
	}
	//TODO-PBB: more validation needed here

	/* create object */
	obj = create_nsobject(cocall_args->name, cocall_args->type, cocall_args->ns_cap);
	switch(cocall_args->type) {
		case COMMAP:
			obj->obj = cocall_args->obj;
			obj = CLEAR_STORE_PERM(obj);
			break;
		case COSERVICE:
			obj->coservice = cocall_args->coservice;
			obj = CLEAR_STORE_PERM(obj);
			break;
		case COPORT:
			obj->coport = cocall_args->coport;
			obj = CLEAR_STORE_PERM(obj);
			break;
		case RESERVATION:
			obj->obj = NULL;
			break;
		default:
			break;
	}
	
	if(!is_privileged(token))
		cocall_args->nsobj = seal_nsobj(obj);
	else 
		cocall_args->nsobj = obj;
	
	cocall_args->status = 0;
	cocall_args->error = 0;

	return;
}
