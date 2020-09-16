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
#include "nsd.h"
#include "nsd_crud.h"
#include "nsd_lookup.h"

#include <cocall/cocall_args.h>
#include <coproc/utils.h>

#include <sys/errno.h>

int validate_cocreate_args(cocreate_args_t *cocall_args)
{
	//XXX-PBB: extensive type based checking is neglected here, as types are likely to get a rework
	if(!valid_namespace_cap(cocall_args->ns_cap))
		return (0);
	else if (cocall_args->type == GLOBAL || cocall_args->type == INVALID)
		return (0);
	else if (!valid_ns_name(cocall_args->name))
		return (0);
	else
		return (1);
}

void create_namespace(cocreate_args_t *cocall_args, void *token)
{
	UNUSED(token);

	if(!NS_PERMITS_WRITE(cocall_args->ns_cap))
		COCALL_ERR(cocall_args, EACCES);
	else if (in_namespace(cocall_args->name, cocall_args->ns_cap)) 
		COCALL_ERR(cocall_args, EEXIST);

	cocall_args->child_ns_cap = create_namespace(cocall_args->name, cocall_args->type, cocall_args->ns_cap);
	
	COCALL_RETURN(cocall_args);
}