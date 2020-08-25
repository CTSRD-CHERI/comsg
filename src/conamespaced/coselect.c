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
#include "nsd_lookup.h"
#include "ukern/cocall_args.h"
#include "ukern/namespace_object.h"

#include <errno.h>

int validate_coselect_args(coselect_args_t*)
{
	/*
	 *
	 */
	return (1);
}

void select_namespace_object(coselect_args_t * cocall_args, void *token)
{
	nsobject_t *obj;

	obj = lookup_nsobject(cocall_args->name, cocall_args->ns_cap, cocall_args->type);
	if(obj == NULL) {
		cocall_args->status = -1;
		cocall_args->error = ENOENT;
		cocall_args->nsobj = NULL;
		return;
	}

	obj = cheri_andperm(obj, NS_HW_PERMS_MASK);
	if(!is_privileged(token))
		cocall_args->nsobj = seal_nsobj(obj);
	else 
		cocall_args->nsobj = obj;
	
	cocall_args->status = 0;
	cocall_args->error = 0;

	return;
}
