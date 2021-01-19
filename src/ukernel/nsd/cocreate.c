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
#include "nsd_crud.h"
#include "nsd_lookup.h"

#include <cocall/cocall_args.h>
#include <coproc/namespace.h>
#include <coproc/utils.h>

#include <cheri/cheric.h>
#include <sys/errno.h>

int validate_cocreate_args(cocreate_args_t *cocall_args)
{
	//XXX-PBB: extensive type based checking is neglected here, as types are likely to get a rework
	if(!valid_namespace_cap(cocall_args->ns_cap))
		return (0);
	else if (cocall_args->ns_type == GLOBAL || cocall_args->ns_type == INVALID_NS)
		return (0);
	else if (!valid_ns_name(cocall_args->ns_name))
		return (0);
	else
		return (1);
}

void namespace_create(cocreate_args_t *cocall_args, void *token)
{
	UNUSED(token);
	namespace_t *ns, *parent_ns;

	parent_ns = cocall_args->ns_cap;
	if(!NS_PERMITS_WRITE(parent_ns)) 
		COCALL_ERR(cocall_args, EACCES);
	/*
	 * XXX-PBB: The access control model implemented here is incorrect/incomplete.
	 * This is a placeholder that sort-of works. The hack here is to allow lookups of
	 * APPLICATION, LIBRARY, and PUBLIC  namespaces from the GLOBAL namespace
	 * where normally only PUBLIC should be allowed. Once we have linker assistance, 
	 * or passing capabilities after vfork+coexecve, we can do the right thing. 
     */
	
	ns = lookup_namespace(cocall_args->ns_name, parent_ns);
	if (ns != NULL) {
		if (!NS_PERMITS_READ(parent_ns))
			COCALL_ERR(cocall_args, EACCES);
		else if (ns->type != cocall_args->ns_type)
			COCALL_ERR(cocall_args, EEXIST);
		else if (ns->type == PRIVATE) 
			COCALL_ERR(cocall_args, EEXIST);
		else if (ns->type != PUBLIC && get_ns_type(parent_ns) != GLOBAL)
			COCALL_ERR(cocall_args, EEXIST);
		else {
			ns = cheri_andperm(ns, NS_PERMS_WR_MASK);
			cocall_args->child_ns_cap = seal_ns(ns);
			COCALL_RETURN(cocall_args, 0);
		}
	} else {
		cocall_args->child_ns_cap = new_namespace(cocall_args->ns_name, cocall_args->ns_type, parent_ns);
		COCALL_RETURN(cocall_args, 0);
	}
}