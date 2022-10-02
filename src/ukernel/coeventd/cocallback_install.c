/*
 * Copyright (c) 2021 Peter S. Blandford-Baker
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
#include "cocallback_install.h"
#include "coevent_utils.h"


#include <comsg/coevent.h>
#include <comsg/comsg_args.h>
#include <sys/errno.h>

int
validate_cocallback_install(comsg_args_t *cocall_args)
{
	if (!validate_coevent(cocall_args->coevent))
		return (0);
	else
		return (1);
}

static coevent_t *
get_coevent_from_handle(coevent_t *coevent)
{
	//placeholder
	return (coevent);
}

/*
 * ccb_func - cocallback function
 * ccb_args - arguments to be passed to cocallback
 * coevent - event that cocallback should be assigned to
 */
void 
install_cocallback(comsg_args_t *cocall_args, void *token)
{
	cocallback_t *cocallback;
	coevent_t *coevent;
	pid_t target_pid;

	coevent = get_coevent_from_handle(cocall_args->coevent);
	switch (coevent->event) {
	case PROCESS_DEATH:
		lock_coevent(coevent); /* can block */
		cocallback = add_cocallback(coevent, cocall_args->ccb_func, &cocall_args->ccb_args);
		unlock_coevent(coevent);
		break;
	default: 
		COCALL_ERR(cocall_args, EINVAL);
		break; //NOTREACHED
	}

	COCALL_RETURN(cocall_args, 0);
}