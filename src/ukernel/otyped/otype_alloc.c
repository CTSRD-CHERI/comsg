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
#include "otype_alloc.h"

#include <cocall/cocall_args.h>
#include <coproc/otype.h>
#include <coproc/utils.h>
#include <sys/errno.h>

void
allocate_otype_user(cocall_args_t *cocall_args, void *token)
{
	UNUSED(token);
	otype_t otype;

	otype = allocate_user_otype();
	if (otype == NULL)
		COCALL_ERR(cocall_args, EINVAL); /* wrong errno, not sure what's right */
	else if (!cheri_gettag(otype))
		COCALL_ERR(cocall_args, ENOMEM);

	cocall_args->otype = otype;
	COCALL_RETURN(cocall_args, 0);
}

void
allocate_otype_ukernel(cocall_args_t *cocall_args, void *token)
{
	UNUSED(token);
	otype_t otype;

	otype = allocate_ukernel_otype();
	if (otype == NULL)
		COCALL_ERR(cocall_args, EINVAL);
	else if (!cheri_gettag(otype))
		COCALL_ERR(cocall_args, ENOMEM);

	cocall_args->otype = otype;
	COCALL_RETURN(cocall_args, 0);
}