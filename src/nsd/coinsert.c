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
#include "nsd_cap.h"
#include "nsd_crud.h"
#include "nsd_lookup.h"

#include <cocall/cocall_args.h>
#include <coproc/namespace_object.h>
#include <coproc/utils.h>



int validate_coinsert_args(coinsert_args_t *cocall_args)
{
	namespace_t *ns = unseal_ns(cocall_args->ns_cap);
	if (!valid_namespace_cap(cocall_args->ns_cap))
		return (0);
	else
		return (1);
}

void insert_namespace_object(coinsert_args_t *cocall_args, void *token)
{
	nsobject_t *obj;
	namespace_t *ns;

	ns = unseal_ns(cocall_args->ns_cap);
	if (!NS_PERMITS_WRITE(cocall_args->ns_cap)) 
		COCALL_ERR(cocall_args, EACCES);
	else if(in_namespace(cocall_args->name, ns)) 
		COCALL_ERR(cocall_args, EEXIST);

	/* create object */
	obj = create_nsobject(cocall_args->name, cocall_args->type, ns);
	switch(cocall_args->type) {
	case COMMAP:
		obj->obj = cocall_args->obj;
		obj = CLEAR_NSOBJ_STORE_PERM(obj);
		break;
	case COSERVICE:
		obj->coservice = cocall_args->coservice;
		obj = CLEAR_NSOBJ_STORE_PERM(obj);
		break;
	case COPORT:
		obj->coport = cocall_args->coport;
		obj = CLEAR_NSOBJ_STORE_PERM(obj);
		break;
	case RESERVATION:
		obj->obj = NULL;
		break;
	default:
		break;
	}
	/* TODO-PBB: I am flip-flopping on whether this is the right thing to do
	 * Sealing all nsobjects requires tighter integration of microkernel compartments
	 * and/or cross-compartment calls. Not sealing them exposes the handles stored
	 * in nsobjects more than my gut is comfortable with.
	 * One solution that might bring balance to the force would be to use local/global 
	 * or similar colours to manage flow. Handles could be local capabilities, while
	 * nsobjects could be set up as global capabilities with PERMIT_STORE_LOCAL_CAPABILITY
	 * which is stripped once the handle is stored. Using handles after this point becomes 
	 * tricky however. Microkernel calls might need to be special to allow this.
	 * It also requires the cocall_args to be allocated with PERMIT_STORE_LOCAL_CAPABILITY
	 * which should be fine so long as we make it a local variable everywhere.
	 */
	cocall_args->nsobj = seal_nsobj(obj);

	COCALL_RETURN(cocall_args);
}
