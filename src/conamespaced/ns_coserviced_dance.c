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

#include "namespace_table.h"
#include "nsd_crud.h"
#include "nsd_lookup.h"

#include "ukern/cocall_args.h"

#include <sys/types.h>
#include <unistd.h>

static void *coprovide_scb, *codiscover_scb;

static
void set_global_service(const char *name, coservice_t *cap)
{
	namespace_t *global_ns = get_global_namespace();
	nsobject_t *obj = lookup_nsobject(name, COSERVICE, global_ns);
	if (obj == NULL)
		obj = create_nsobject(name, COSERVICE, global_ns);
	obj->coservice = cap;
	return;
}

/* 
 * This composes:
 * 	+ registration of a privileged caller token via known child pid,
 * 	+ 'discovery' of codiscover and coprovide,
 * 	+ registration of global ns objects for same, and
 * 	+ provision of global_ns cap to coserviced
 * to avoid messy circular dependencies.
 */

void coserviced_init(cocall_args_t * cocall_args, void *token)
{
	namespace_t *global_ns;
	pid_t expected_pid;
	pid_t copid;
	nsobject_t *obj;
	
	
	/* XXX: Risk of race condition inherent in use of pid here.
	 * We (will) resolve it by only providing this function when 
	 * racey values are expected not to change 
	 */
	cocall_args->error = cogetpid(&copid);
	expected_pid = get_coserviced_pid();
	if(cocall_args->error) {
		cocall_args->status = -1;
		return;
	}
	else if (copid != expected_pid) {
		cocall_args->status = -1;
		cocall_args->error = EPERM;
		return;
	}
	/* Privileged section */
	register_coserviced_token(token);

	set_global_service(COPROVIDE, cocall_args->coprovide.cap);
	coprovide_scb = cocall_args->coprovide.scb;
	set_global_service(CODISCOVER, cocall_args->codiscover.cap);
	codiscover_scb = cocall_args->codiscover.scb;
	
	cocall_args->global = get_global_namespace();
	cocall_args->status = 0;
	cocall_args->error = 0;
	return;
	
}

