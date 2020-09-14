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
#include "namespace_table.h"
#include "nsd.h"
#include "nsd_cap.h"
#include "nsd_crud.h"
#include "nsd_lookup.h"

#include "ukern/cocall_args.h"
#include "ukern/utils.h"

#include <assert.h>
#include <string.h>

int validate_codelete_args(codelete_args_t *cocall_args)
{
	nsobject_t *obj = unseal_nsobj(cocall_args->nsobj);
	namespace_t *ns = unseal_ns(cocall_args->ns_cap);
	if (!valid_namespace_cap(cocall_args->ns_cap))
		return (0);
	else if (!valid_nsobject_cap(cocall_args->nsobj)) 
		return (0);
	else
		return (1);
}

void delete_namespace_object(codelete_args_t *cocall_args, void *token)
{
	UNUSED(token);
	nsobject_t *nsobj, *parent_obj;

	if (!NSOBJ_PERMITS_DELETE(cocall_args->nsobj)) 
		COCALL_ERR(EACCES);
	
	nsobj = unseal_nsobj(cocall_args->nsobj);
	//Check that the supplied namespace authorises actions on this namespace object
	parent_obj = lookup_nsobject(nsobj->name, nsobj->type, cocall_args->ns_cap);
	if (parent_obj == NULL) 
		COCALL_ERR(cocall_args, ENOENT); //No object with that name and type.
	else if (parent_obj != nsobj) 
		COCALL_ERR(cocall_args, EPERM); //There is an object with that name and type, but it's not this object
	//XXX-PBB: The EPERM above may be incorrect and leak information about the namespace contents.
	
	if(!delete_nsobject(parent_obj, cocall_args->ns_cap)) {
		//Someone beat us to it
		//Or there's a bug
		assert(lookup_nsobject(nsobj->name, nsobj->type, cocall_args->ns_cap) != nsobj) 
		COCALL_ERR(cocall_args, ENOENT);
	}
	
	COCALL_RETURN(cocall_args);
}