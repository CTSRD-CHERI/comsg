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
#include "coevent_listen.h"

#include "procdeath_tbl.h"

#include <assert.h>
#include <coproc/coevent.h>
#include <cocall/cocall_args.h>
#include <sys/errno.h>
#include <stdio.h>
#include <unistd.h>

int
validate_colisten(cocall_args_t *cocall_args)
{
	return (1);
}

static coevent_t *
make_coevent_handle(coevent_t *coevent)
{
	//placeholder
	return (coevent);
}

/*
 * coevent_type_t
 * union coevent_subject
 * 
 * NOTE-PBB: requiring both a valid scb and the pid ensures:
 * 		* there is no information leakage regarding processes that might be OS invisible
 		  to the caller, as it already is coprocess aware that it exists
 		* it limits process death monitoring to processes in the caller's address space
 */
void 
add_event_listener(cocall_args_t *cocall_args, void *token)
{
	coevent_t *coevent;
	pid_t target_pid, requested_pid;
	union coevent_subject subject;

	coevent = NULL;
	subject = cocall_args->subject;
	switch (cocall_args->event) {
	case PROCESS_DEATH:
		/* event allocator must be the subject (i.e. the monitored process) */
		target_pid = cogetpid2();
		coevent = allocate_procdeath_event(target_pid);
		break;
	default:
		COCALL_ERR(cocall_args, EINVAL);
		break; /* NOTREACHED */
	}
	assert(coevent != NULL); /* should return prior to this point if we fail */
	cocall_args->coevent = make_coevent_handle(coevent);

	COCALL_RETURN(cocall_args, 0);
}