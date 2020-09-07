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
#include "ukern/ukern_calls.h"
#include "ukern/cocalls.h"

#include "ukern/coport.h"
#include "ukern/coservice.h"
#include "ukern/namespace.h"
#include "ukern/namespace_object.h"

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
	
	if (strlen(name) > NS_NAME_LEN)
		err(ENAMETOOLONG, "coinsert: name exceeds maximum supported length of %d", NS_NAME_LEN);

	strncpy(cocall_args.name, name, NS_NAME_LEN);
	cocall_args.namespace = ns;
	switch(type)
	{
		case RESERVATION:
			cocall_args.obj = NULL;
			break;
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
	cocall_args.type = type;

	error = targeted_cocall(ukern_call_set, COCALL_COINSERT, cocall_args, sizeof(coinsert_args_t));
	if (error != 0) 
		err(errno, "coinsert: error performing cocall");
	else if (cocall_args.status == -1) {
		//TODO-PBB: handle errors better? or leave it to consumers?
		errno = cocall_args.error;
		return (NULL);
	}
	else
		return (cocall_args.nsobj);
}

nsobject_t *coselect(const char *name, nsobject_type_t type, namespace_t *ns)
{
	coselect_args_t cocall_args;

	if (strlen(name) > NS_NAME_LEN)
		err(ENAMETOOLONG, "coselect: name exceeds maximum supported length of %d", NS_NAME_LEN);
	strncpy(cocall_args.name, name, NS_NAME_LEN);

	cocall_args.type = type;
	cocall_args.nsobj = NULL;
	cocall_args.ns_cap = ns;

	error = targeted_cocall(ukern_call_set, COCALL_COSELECT, &cocall_args, sizeof(coselect_args_t));
	if (error == -1)
		err("coselect: error performing cocall to coselect");
	else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	}
	else 
		return (cocall_args.nsobj);
}

coservice_t *codiscover(nsobject_t *nsobj, void **scb)
{
	int error;
	codiscover_args_t cocall_args;

	cocall_args.nsobj = nsobj;
	cocall_args.scb = NULL;

	error = targeted_cocall(ukern_call_set, COCALL_CODISCOVER, &cocall_args, sizeof(codiscover_args_t));
	if (error == -1)
		err("codiscover: error performing cocall to codiscover");
	else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	}
	else {
		*scb = cocall_args.scb_cap;
		return (cocall_args.service);
	}
}

coservice_t *coprovide(void **worker_scbs, int nworkers)
{
	int error;
	coprovide_args_t cocall_args;

	size_t min_scb_array_len = nworkers * sizeof(void *);

	if (cheri_getlen(worker_scbs) < min_scb_array_len)
		err(EINVAL, "coprovide: length of worker_scbs %lu too short for %d workers", cheri_getlen(worker_scbs), nworkers);
	
	//TODO-PBB: check scb validity here.
	cocall_args.worker_scbs = calloc(nworkers, sizeof(void *));
	for(int i = 0; i < nworkers; i++)
		cocall_args.worker_scbs[i] = worker_scbs[i];
	cocall_args.nworkers = nworkers;

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
			return (cocall_args.service);
	}
	
}

namespace_t *coproc_init(namespace_t *global_ns, void *coinsert_scb, void *coselect_scb, void *codiscover_scb)
{
	int error;
	
	coproc_init_args_t *cocall_args = calloc(1, sizeof(coproc_init_args_t));

	cocall_args->namespace =  global_ns;
	cocall_args->coinsert = coinsert_scb;
	cocall_args->coinsert = codiscover_scb;
	cocall_args->coselect = coselect_scb;

	//the target of this varies based on whether caller is a microkernel compartment & which compartment
	error = targeted_cocall(ukern_call_set, COCALL_COPROC_INIT, cocall_args, sizeof(coproc_init_args_t));
	if(error)
		err(error, "coproc_init: cocall failed");
	else if (cocall_args->status == -1) {
		errno = cocall_args->error;
		return (NULL);
	}
	
	set_cocall_target(ukern_call_set, COCALL_COINSERT, cocall_args->coinsert);
	set_cocall_target(ukern_call_set, COCALL_CODISCOVER, cocall_args->codiscover);
	set_cocall_target(ukern_call_set, COCALL_COSELECT, cocall_args->coselect);
	
	return (cocall_args->namespace);
}

#if 0
int coopen(const char *coport_name, coport_type_t type, coport_t **prt)
{
    /* request new coport from microkernel */
    void * __capability func;
    cocall_coopen_t call;
    uint error;

    /* cocall setup */
    strcpy(call.name, coport_name);
    call.args.type = type;
    
    //possible deferred call to malloc ends up in cocall, causing exceptions
    if (get_cocall_target(COCALL_COOPEN) == NULL) {
    	if (codiscover(U_COOPEN, global_ns, &func) == NULL)
    		error("coopen: error performing codiscover");
    	else
    		set_cocall_target(ukern_call_set, COCALL_COOPEN, func);
	}
    error = targeted_cocall(COCALL_COOPEN, &call, sizeof(cocall_coopen_t));
    if (!cheri_getsealed(call.port)) {
        *prt = cheri_seal(call.port, libcomsg_coport_seal);
        if (call.port->type == COPIPE) 
            copipe_otype = cheri_gettype(*prt);
        else if (call.port->type == COCHANNEL) 
            cochannel_otype = cheri_gettype(*prt);
        else 
        	err("coopen: ipcd returned unknown coport type");
    }
    else {
        cocarrier_otype = cheri_gettype(call.port);
        *prt = call.port;
        //pre-fetch cocarrier operations to speed up usage
        ukern_lookup(&switcher_code, &switcher_data, U_COCARRIER_SEND, &func);
        ukern_lookup(&switcher_code, &switcher_data, U_COCARRIER_RECV, &func);
    }
    
    return (1);
}
#endif