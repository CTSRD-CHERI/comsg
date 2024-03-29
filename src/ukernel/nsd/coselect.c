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
#include "nsd.h"

#include "nsd_cap.h"
#include "nsd_lookup.h"

#include <cheri/cherireg.h>
#include <comsg/comsg_args.h>
#include <comsg/namespace_object.h>

#include <sys/errno.h>

int validate_coselect_args(coselect_args_t* cocall_args)
{
	/*
	 *
	 */
	if (!valid_namespace_cap(cocall_args->ns_cap))
		return (0);
	else if (!valid_nsobj_name(cocall_args->nsobj_name))
		return (0);
	else if (!VALID_NSOBJ_TYPE(cocall_args->nsobj_type))
		return (0);
	return (1);
}

void namespace_object_select(coselect_args_t *cocall_args, void *token)
{
	nsobject_t *obj;
	

	obj = lookup_nsobject(cocall_args->nsobj_name, cocall_args->nsobj_type, cocall_args->ns_cap);
	if(obj == NULL) 
		COCALL_ERR(cocall_args, ENOENT);
	else if (!cheri_gettag(obj->obj) && cocall_args->nsobj_type != RESERVATION)
		COCALL_ERR(cocall_args, ENOENT); /* Is currently being inserted/updated */

	if (obj->type == RESERVATION)
		obj = seal_nsobj(obj);
	else 
		obj = CLEAR_NSOBJ_STORE_PERM(obj);
	
	cocall_args->nsobj = obj;
	
	COCALL_RETURN(cocall_args, 0);
}
