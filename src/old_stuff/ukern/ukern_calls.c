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
#include <cocall/cocall_args.h>
#include <cocall/cocalls.h>

#include <comsg/coport.h>
#include <coproc/coservice.h>
#include <coproc/namespace.h>
#include <coproc/namespace_object.h>

#include <err.h>
#include <errno.h>

union coinsert_subject {
	void *object;
	coserivce_t *coservice;
	coport_t *coport;
};

__attribute__((constructor)) static
void init_tss(void)
{
	ukern_call_set = allocate_target_set();
}

nsobject_t *coinsert(const char *name, nsobject_type_t type, union coinsert_subject subject, namespace_t *ns)
{
	int error;
	coinsert_args_t cocall_args;
	
	strcncpy(cocall_args.name, name, NS_NAME_LEN);
	cocall_args.namespace = ns;
	cocall_args.type = type;
	switch(type)
	{
		case RESERVATION:
		case COMMAP:
			cocall_args.obj = subject;
			break;
		case COPORT:
			cocall_args.coport = subject;
			break;
		case COSERVICE:
			cocall_args.coservice = subject;
			break;
		case INVALID:
		default:
			err(EINVAL, "coinsert: invalid object type %d for coinsert");
			break;
	}
	error = targeted_cocall(ukern_call_set, COCALL_COINSERT, cocall_args, sizeof(coinsert_args_t));
	if (error != 0) {
		//TODO-PBB: handle errors better
		err(errno, "coinsert: error performing cocall");
	}
	else if (cocall_args.status == -1) {
		//TODO-PBB: handle errors better? or leave it to consumers?
		errno = cocall_args.error;
		return (NULL);
	}
	return (cocall_args.nsobj);
}

nsobject_t *coselect(const char *name, nsobject_type_t type, namespace_t *ns)
{
	coselect_args_t cocall_args;

	errno = ;
	return (NULL);
}

coservice_t *codiscover(const char *name, namespace_t *ns, void **scb)
{
	int error;
	codiscover_args_t cocall_args;

	strncpy(cocall_args.name, name, LOOKUP_STRING_LEN);
	cocall_args.namespace = ns;

	error = targeted_cocall(ukern_call_set, COCALL_CODISCOVER, &cocall_args, sizeof(codiscover_args_t));
	if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	}
	else {
		*scb = cocall_args.scb_cap;
		return (cocall_args.service);
	}
}

coservice_t *coprovide(const char *name, void **worker_scbs, int nworkers, nsobject_t *nsobj, namespace_t *ns)
{
	int error;
	coprovide_args_t cocall_args;

	size_t min_scb_array_len = nworkers * sizeof(void *);
	
	if(nsobj == NULL) 
		strncpy(cocall_args.name, name, LOOKUP_STRING_LEN);
	else 
		memset(cocall_args.name, '\0', LOOKUP_STRING_LEN);
	cocall_args.nsobj = nsobj;

	if (cheri_getlen(worker_scbs) < min_scb_array_len)
		err(EINVAL, "coprovide: length of worker_scbs %lu too short for %d workers", cheri_getlen(worker_scbs), nworkers);
	else {
		cocall_args.worker_scbs = calloc(nworkers, sizeof(void *));
		//TODO-PBB: check scb validity here.
		for(int i = 0; i < nworkers; i++)
			cocall_args.worker_scbs[i] = worker_scbs[i];
		cocall_args.nworkers = nworkers;
	}

	error = targeted_cocall(ukern_call_set, COCALL_COPROVIDE, &cocall_args, sizeof(coprovide_args_t));
	if(error)
		err(errno, "coprovide: error in cocall");
	else {
		free(cocall_args.worker_scbs);
		if (cocall_args.status == -1) {
			errno = cocall_args.error;
			return (NULL);
		}
		else 
			return (cocall_args.nsobj);
	}
	
}

namespace_t *coproc_init(namespace_t *global_ns, void *coinsert_scb, void *codiscover_scb)
{
	int error;
	
	coproc_init_args_t *cocall_args = calloc(1, sizeof(coproc_init_args_t));

	cocall_args->namespace =  global_ns;
	cocall_args->coinsert = coinsert_scb;
	cocall_args->coinsert = codiscover_scb;

	error = targeted_cocall(ukern_call_set, COCALL_COPROC_INIT, cocall_args, sizeof(coproc_init_args_t));

	if(error)
		err(error, "coproc_init: cocall failed");
	else if (cocall_args->status == -1) {
		errno = cocall_args->error;
		return (NULL);
	}
	
	set_cocall_target(ukern_call_set, COCALL_COINSERT, cocall_args->coinsert);
	set_cocall_target(ukern_call_set, COCALL_CODISCOVER, cocall_args->codiscover);
	
	return (cocall_args->namespace);
}