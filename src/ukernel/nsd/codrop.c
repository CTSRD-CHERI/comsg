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
#include <coproc/utils.h>

#include <cheri/cheric.h>
#include <sys/errno.h>

int validate_codrop_args(codrop_args_t *cocall_args)
{
	//XXX-PBB: this does not leak information, as the type of a namespace capability
	//is denoted also by its otype.
	namespace_t *ns = unseal_ns(cocall_args->ns_cap);
	if(!valid_namespace_cap(cocall_args->ns_cap))
		return (0);
	else if (cocall_args->ns_type == GLOBAL || cocall_args->ns_type == INVALID_NS)
		return (0);
	else
		return (1);
}

void namespace_drop(codrop_args_t *cocall_args, void *token)
{
	UNUSED(token);
	namespace_t *ns, *parent_ns;

	if((cheri_getperm(cocall_args->ns_cap) & NS_PERMS_OWN) == 0)
		COCALL_ERR(cocall_args, EACCES);
	ns = unseal_ns(cocall_args->ns_cap);
	if (!is_child_namespace(ns->name, ns->parent))
		COCALL_ERR(cocall_args, ENOENT);
	//TODO-PBB: namespace deletion is a big job, and is not fully implemented yet
	if(!delete_namespace(cocall_args->ns_cap))
		COCALL_ERR(cocall_args, ENOENT);
	
	COCALL_RETURN(cocall_args, 0);
}