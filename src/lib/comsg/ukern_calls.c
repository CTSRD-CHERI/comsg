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
#include <comsg/ukern_calls.h>

#include <cocall/cocall_args.h>
#include <cocall/cocalls.h>

#include <coproc/coport.h>
#include <coproc/coservice.h>
#include <coproc/namespace.h>
#include <coproc/namespace_object.h>

#include <err.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <string.h>

namespace_t *global_ns;

static pthread_key_t ukern_call_set;

__attribute__((constructor)) static void 
init_ukern_calls(void)
{
	ukern_call_set = allocate_target_set();
	global_ns = NULL;
}

void 
discover_ukern_func(nsobject_t *service_obj, int function)
{
	coservice_t *service;
	void *scb;

	if (get_cocall_target(ukern_call_set, function) == NULL)
		return;

    service = codiscover(service_obj, &scb);
    set_cocall_target(ukern_call_set, function, scb);
}

void
set_ukern_target(int function, void *target)
{
	set_cocall_target(ukern_call_set, function, target);
}

nsobject_t *
coinsert(const char *name, nsobject_type_t type, void *subject, namespace_t *ns)
{
	coinsert_args_t cocall_args;
	int error;

	if (strlen(name) > NS_NAME_LEN)
		err(ENAMETOOLONG, "coinsert: name exceeds maximum supported length of %lu", NS_NAME_LEN);

	strncpy(cocall_args.nsobj_name, name, NS_NAME_LEN);
	cocall_args.ns_cap = ns;
	switch(type) {
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
	case INVALID_NSOBJ:
	default:
		err(EINVAL, "coinsert: invalid object type %d for coinsert", type);
		break;
	}
	cocall_args.nsobj_type = type;

	error = targeted_cocall(ukern_call_set, COCALL_COINSERT, &cocall_args, sizeof(coinsert_args_t));
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

nsobject_t *
coselect(const char *name, nsobject_type_t type, namespace_t *ns)
{
	int error;
	coselect_args_t cocall_args;

	if (strlen(name) > NS_NAME_LEN)
		err(ENAMETOOLONG, "coselect: name exceeds maximum supported length of %lu", NS_NAME_LEN);
	strncpy(cocall_args.nsobj_name, name, NS_NAME_LEN);

	cocall_args.nsobj_type = type;
	cocall_args.nsobj = NULL;
	cocall_args.ns_cap = ns;

	error = targeted_cocall(ukern_call_set, COCALL_COSELECT, &cocall_args, sizeof(coselect_args_t));
	if (error == -1)
		err(errno, "coselect: error performing cocall to coselect");
	else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	}
	else 
		return (cocall_args.nsobj);
}

coservice_t *
codiscover(nsobject_t *nsobj, void **scb)
{
	int error;
	codiscover_args_t cocall_args;

	cocall_args.nsobj = nsobj;
	cocall_args.scb_cap = NULL;

	error = targeted_cocall(ukern_call_set, COCALL_CODISCOVER, &cocall_args, sizeof(codiscover_args_t));
	if (error == -1)
		err(errno, "codiscover: error performing cocall to codiscover");
	else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	}
	else {
		*scb = cocall_args.scb_cap;
		return (cocall_args.service);
	}
}

coservice_t *
coprovide(void **worker_scbs, int nworkers)
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

namespace_t *
coproc_init(namespace_t *global_ns_cap, void *coinsert_scb, void *coselect_scb, void *codiscover_scb)
{
	int error;
	
	coproc_init_args_t cocall_args;

	cocall_args.ns_cap = global_ns_cap;
	cocall_args.coinsert = coinsert_scb;
	cocall_args.coinsert = codiscover_scb;
	cocall_args.coselect = coselect_scb;

	//the target of this varies based on whether caller is a microkernel compartment & which compartment
	error = targeted_cocall(ukern_call_set, COCALL_COPROC_INIT, &cocall_args, sizeof(coproc_init_args_t));
	if(error)
		err(error, "coproc_init: cocall failed");
	else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	}
	
	set_cocall_target(ukern_call_set, COCALL_COINSERT, cocall_args.coinsert);
	set_cocall_target(ukern_call_set, COCALL_CODISCOVER, cocall_args.codiscover);
	set_cocall_target(ukern_call_set, COCALL_COSELECT, cocall_args.coselect);
	
	return (cocall_args.ns_cap);
}

int 
cocarrier_send(coport_t *port, void *buf, size_t len)
{
	/* Currently only for cocarriers, likely to change soon */
	cosend_args_t cocall_args;
	int error;
	
	cocall_args.cocarrier = port;
    buf = cheri_setbounds(buf, len);
    cocall_args.message = cheri_andperm(buf, COCARRIER_MSG_PERMS);
    
    error = targeted_cocall(ukern_call_set, COCALL_COSEND, &cocall_args, sizeof(cosend_args_t));
    if(error)
        err(error, "cocarrier_send: cocall failed");

    if (cocall_args.status == -1) {
        errno = cocall_args.error;
        return (-1);
    }

	return (cocall_args.status);
}

coport_t *
coopen(coport_type_t type)
{
	coopen_args_t cocall_args;
	int error;

	cocall_args.coport_type = type;
    
    error = targeted_cocall(ukern_call_set, COCALL_COOPEN, &cocall_args, sizeof(cosend_args_t));
    if(error)
        err(error, "coopen: cocall failed");

    if (cocall_args.status == -1) {
        errno = cocall_args.error;
        return (NULL);
    }

	return (cocall_args.port);
}

void *
cocarrier_recv(coport_t *port, size_t len)
{
	/* Currently only for cocarriers, likely to change soon */
	corecv_args_t cocall_args;
	int error;

	cocall_args.cocarrier = port;

	error = targeted_cocall(ukern_call_set, COCALL_COSEND, &cocall_args, sizeof(cosend_args_t));
    if(error != 0)
        err(error, "cocarrier_recv: cocall failed");

    if (cocall_args.status == -1) {
        errno = cocall_args.error;
        return (NULL);
    }

	return (cocall_args.message);
}

int
copoll(pollcoport_t *coports, int ncoports, int timeout)
{
	copoll_args_t cocall_args;
	int error;

	cocall_args.ncoports = ncoports;
	cocall_args.timeout = timeout;
	/* 
	 * If there is no chance we will unborrow/go to the slow path, then we can use
	 * the pointer supplied, as getting something modified in there so quickly
	 * quickly would be our fault. Otherwise, we map something ourselves to be 
	 * a bit more confident that that won't happen. Not sure I'm happy with either
	 * situation.
	 */
	if (timeout == 0)
		cocall_args.coports = coports;
	else {
		cocall_args.coports = calloc(ncoports, sizeof(pollcoport_t));
		memcpy(cocall_args.coports, coports, sizeof(pollcoport_t) * ncoports);
	}

	error = targeted_cocall(ukern_call_set, COCALL_COPOLL, &cocall_args, sizeof(copoll_args_t));
	if(error != 0)
		err(error, "copoll: cocall failed");
	if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (-1);
	}
	if (timeout != 0) {
		memcpy(coports, cocall_args.coports, sizeof(pollcoport_t) * ncoports);
		free(cocall_args.coports);
	}
	return (cocall_args.status);
}

int 
coclose(coport_t *coport)
{
	coclose_args_t cocall_args;
	int error;

	cocall_args.port = coport;
	error = targeted_cocall(ukern_call_set, COCALL_COCLOSE, &cocall_args, sizeof(coclose_args_t));
	if(error != 0)
		err(error, "coclose: cocall failed");
	if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (-1);
	}
	else 
		return (0);

}