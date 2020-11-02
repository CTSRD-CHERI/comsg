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
#include "nsd_cap.h"

#include <cocall/cocall_args.h>
#include <coproc/utils.h>

#include <cheri/cheric.h>
#include <sys/errno.h>

int validate_coupdate_args(coupdate_args_t *cocall_args)
{
	nsobject_t *obj = unseal_nsobj(cocall_args->nsobj);
	if (!valid_nsobject_cap(cocall_args->nsobj))
		return (0);
	else if (!cheri_getsealed(cocall_args->nsobj))
		return (0);
	else if ((cocall_args->nsobj_type & (RESERVATION | INVALID_NSOBJ)) == 0)
		return (0);
	else if (!cheri_getsealed(cocall_args->obj))
		return (0);
	else if (cheri_getperm(cocall_args->obj) & CHERI_PERM_GLOBAL != 0)
		return (0);
	else
		return (1);
}

void namespace_object_update(coupdate_args_t *cocall_args, void *token)
{
	UNUSED(token);
	nsobject_t *nsobj;


	/* BIGTODO-PBB: I'm almost certain there are many races now. Fix this. */
	nsobj = unseal_nsobj(cocall_args->nsobj);
	if (nsobj == NULL)
		COCALL_ERR(cocall_args, ENOENT);
	else if (!NSOBJ_PERMITS_WRITE(nsobj))
		COCALL_ERR(cocall_args, EPERM);
	else if (nsobj->type != RESERVATION) /* Not reached(?). Could be in races? */
		COCALL_ERR(cocall_args, ENODEV);

	nsobj->type = cocall_args->nsobj_type;
	switch(cocall_args->nsobj_type) {
		case COSERVICE:
			nsobj->coservice = cocall_args->coservice;
			nsobj = CLEAR_NSOBJ_STORE_PERM(nsobj);
			break;
		case COPORT:
			nsobj->coport = cocall_args->coport;
			nsobj = CLEAR_NSOBJ_STORE_PERM(nsobj);
			break;
			case COMMAP:
		default:
			nsobj->obj = cocall_args->obj;
			nsobj = CLEAR_NSOBJ_STORE_PERM(nsobj);
			break;
	}
	cocall_args->nsobj = nsobj;
	COCALL_RETURN(cocall_args, 0);
}	