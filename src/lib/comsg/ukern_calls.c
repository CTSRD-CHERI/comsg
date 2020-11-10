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
#include <signal.h>
#include <unistd.h>
#include <sys/errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char *ukern_func_names[] = {"NOTUSED", U_CODISCOVER, U_COPROVIDE, U_COINSERT, U_COSELECT, U_COUPDATE, U_CODELETE, U_COOPEN, U_COCLOSE, U_COSEND, U_CORECV, U_COPOLL, U_COPROC_INIT, U_COCREATE, U_CODROP, U_COPROC_INIT_DONE, U_SLOPOLL};

static int slocall_funcs[] = {COCALL_SLOPOLL};
static int n_slocalls = 1;

namespace_t *global_ns = NULL;
bool is_ukernel = false;

static pthread_key_t ukern_call_set;

#if 1
#define err(n, s, ...) do { printf("%s: Errno=%d\n", s, n); kill(getpid(), SIGSEGV); } while(0)
#endif 

__attribute__((constructor)) static void 
init_ukern_calls(void)
{
	ukern_call_set = allocate_target_set();
	init_target_set(ukern_call_set, n_ukern_calls);
	global_ns = NULL;
}

static void
init_new_thread_calls(void)
{
	nsobject_t *service_obj;

	set_cocall_target(ukern_call_set, COCALL_CODISCOVER,  get_global_target(COCALL_CODISCOVER));
	set_cocall_target(ukern_call_set, COCALL_COSELECT, get_global_target(COCALL_COSELECT));
	
	service_obj = coselect(U_COSELECT, COSERVICE, global_ns);
	discover_ukern_func(service_obj, COCALL_COSELECT);

	service_obj = coselect(U_CODISCOVER, COSERVICE, global_ns);
	discover_ukern_func(service_obj, COCALL_CODISCOVER);
}

static inline bool
is_slocall(int func)
{
	for (int i = 0; i < n_slocalls; i++) {
		if (slocall_funcs[i] == func)
			return (true);
	}
	return (false);
}

static int 
ukern_call(int func, cocall_args_t *args)
{
	nsobject_t *func_obj;
	void *global_coselect_scb, *coselect_scb;

	if (get_cocall_target(ukern_call_set, func) == NULL) {
		if ((get_global_target(COCALL_CODISCOVER) == NULL) || (get_global_target(COCALL_COSELECT) == NULL))
			err(ESRCH, "ukern_call: coproc_init either failed or has not been called");
		else if ((get_cocall_target(ukern_call_set, COCALL_CODISCOVER) == NULL) || (get_cocall_target(ukern_call_set, COCALL_COSELECT) == NULL))
			init_new_thread_calls();	

		func_obj = coselect(ukern_func_names[func], COSERVICE, global_ns);
		if (func_obj == NULL)
			err(ENOSYS, "ukern_call: function %s is not present in the global namespace", ukern_func_names[func]);
		discover_ukern_func(func_obj, func);
	}
	if (is_slocall(func))
		return (targeted_slocall(ukern_call_set, func, args, sizeof(cocall_args_t)));
	else 
		return (targeted_cocall(ukern_call_set, func, args, sizeof(cocall_args_t)));
}

void 
discover_ukern_func(nsobject_t *service_obj, int function)
{
	coservice_t *service;
	void *scb;

	if (get_cocall_target(ukern_call_set, function) != NULL)
		return;

    service = codiscover(service_obj, &scb);
    if (service == NULL)
    	err(errno, "discover_ukern_func: codiscover failed");
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

	memset(&cocall_args, '\0', sizeof(cocall_args));
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

	error = ukern_call(COCALL_COINSERT, &cocall_args);
	if (error != 0) 
		err(errno, "coinsert: error performing cocall");
	else if (cocall_args.status == -1) {
		//TODO-PBB: handle errors better? or leave it to consumers?
		errno = cocall_args.error;
		return (NULL);
	}
	
	return (cocall_args.nsobj);
}

nsobject_t *
coselect(const char *name, nsobject_type_t type, namespace_t *ns)
{
	int error;
	coselect_args_t cocall_args;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	if (strlen(name) > NS_NAME_LEN)
		err(ENAMETOOLONG, "coselect: name exceeds maximum supported length of %lu", NS_NAME_LEN);
	strncpy(cocall_args.nsobj_name, name, NS_NAME_LEN);

	cocall_args.nsobj_type = type;
	cocall_args.nsobj = NULL;
	cocall_args.ns_cap = ns;

	error = targeted_cocall(ukern_call_set, COCALL_COSELECT, &cocall_args, sizeof(cocall_args));
	if (error == -1)
		err(errno, "coselect: error performing cocall to coselect");
	else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	} 
	return (cocall_args.nsobj);
}

coservice_t *
codiscover(nsobject_t *nsobj, void **scb)
{
	int error;
	codiscover_args_t cocall_args;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.nsobj = nsobj;
	cocall_args.scb_cap = NULL;

	error = targeted_cocall(ukern_call_set, COCALL_CODISCOVER, &cocall_args, sizeof(cocall_args));
	if (error == -1)
		err(errno, "codiscover: error performing cocall to codiscover");
	else if (cocall_args.status == -1) {
		err(errno, "codiscover: error performing cocall to codiscover");
		errno = cocall_args.error;
		return (NULL);
	}
	
	*scb = cocall_args.scb_cap;
	return (cocall_args.coservice);
	
}

coservice_t *
coprovide(void **worker_scbs, int nworkers)
{
	int error;
	coprovide_args_t cocall_args;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	size_t min_scb_array_len = nworkers * sizeof(void *);

	if (cheri_getlen(worker_scbs) < min_scb_array_len)
		err(EINVAL, "coprovide: length of worker_scbs %lu too short for %d workers", cheri_getlen(worker_scbs), nworkers);
	
	//TODO-PBB: check scb validity here.
	cocall_args.worker_scbs = calloc(nworkers, sizeof(void *));
	for(int i = 0; i < nworkers; i++)
		cocall_args.worker_scbs[i] = worker_scbs[i];
	cocall_args.nworkers = nworkers;

	error = ukern_call(COCALL_COPROVIDE, &cocall_args);
	if(error) {
		err(errno, "coprovide: error in cocall");
	}
	
	free(cocall_args.worker_scbs);
	if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	}
	else 
		return (cocall_args.service);
	
	
}

namespace_t *
cocreate(const char *name, nstype_t type, namespace_t *parent)
{
	int error;

	cocreate_args_t cocall_args;

	memset(&cocall_args, '\0', sizeof(cocreate_args_t));
	cocall_args.ns_cap = parent;
	cocall_args.ns_type = type;
	strncpy(cocall_args.ns_name, name, NS_NAME_LEN);

	error = ukern_call(COCALL_COCREATE, &cocall_args);
	if (error != 0)
		err(errno, "cocreate: cocall failed");
	else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	}

	return (cocall_args.child_ns_cap);
}

namespace_t *
coproc_init(namespace_t *global_ns_cap, void *coinsert_scb, void *coselect_scb, void *codiscover_scb)
{
	int error;
	
	coproc_init_args_t cocall_args;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.ns_cap = global_ns_cap;
	cocall_args.coinsert = coinsert_scb;
	cocall_args.codiscover = codiscover_scb;
	cocall_args.coselect = coselect_scb;

	//the target of this varies based on whether caller is a microkernel compartment & which compartment
	if (is_ukernel)
		error = targeted_slocall(ukern_call_set, COCALL_COPROC_INIT, &cocall_args, sizeof(coproc_init_args_t));
	else 
		error = targeted_cocall(ukern_call_set, COCALL_COPROC_INIT, &cocall_args, sizeof(coproc_init_args_t));
	if(error){
		err(errno, "coproc_init: cocall failed");
	}
	else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	}
	global_ns = cocall_args.ns_cap;
	set_cocall_target(ukern_call_set, COCALL_COINSERT, cocall_args.coinsert);
	set_cocall_target(ukern_call_set, COCALL_CODISCOVER, cocall_args.codiscover);
	set_cocall_target(ukern_call_set, COCALL_COSELECT, cocall_args.coselect);
	
	if (cheri_gettag(cocall_args.done_scb) && is_ukernel)
		set_cocall_target(ukern_call_set, COCALL_COPROC_INIT_DONE, cocall_args.done_scb);
	return (cocall_args.ns_cap);
}

int
coproc_init_done(void)
{
	int error;
	coproc_init_args_t cocall_args;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	if (!is_ukernel)
		err(EPERM, "coproc_init_done: microkernel only calls should not be made by user programs");

	error = targeted_slocall(ukern_call_set, COCALL_COPROC_INIT_DONE, &cocall_args, sizeof(coproc_init_args_t));
	if(error){
		err(errno, "coproc_init_done: cocall failed");
	}
	else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (-1);
	}

	return (0);
}

int 
cocarrier_send(coport_t *port, void *buf, size_t len)
{
	/* Currently only for cocarriers, likely to change soon */
	cosend_args_t cocall_args;
	int error;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	
	cocall_args.cocarrier = port;
    buf = cheri_setbounds(buf, len);
    cocall_args.message = cheri_andperm(buf, COCARRIER_MSG_PERMS);
    cocall_args.length = len;
    
    error = ukern_call(COCALL_COSEND, &cocall_args);
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

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.coport_type = type;
    
    error = ukern_call(COCALL_COOPEN, &cocall_args);
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

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.cocarrier = port;
	cocall_args.length = len;

	error = ukern_call(COCALL_CORECV, &cocall_args);
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
	int function_variant;

	function_variant = COCALL_COPOLL;
	memset(&cocall_args, '\0', sizeof(cocall_args));
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
do_copoll:
	error = ukern_call(function_variant, &cocall_args);
	if(error != 0)
		err(errno, "copoll: cocall failed");
	if (cocall_args.status == -1) {
		if (cocall_args.error == EWOULDBLOCK && timeout != 0 && function_variant != COCALL_SLOPOLL) {
			function_variant = COCALL_SLOPOLL;
			goto do_copoll;
		}
		else {
			errno = cocall_args.error;
			return (-1);
		}
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

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.port = coport;
	error = ukern_call(COCALL_COCLOSE, &cocall_args);
	if(error != 0)
		err(error, "coclose: cocall failed");
	if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (-1);
	}
	else 
		return (0);

}

nsobject_t *coupdate(nsobject_t *nsobj, nsobject_type_t type, void *subject)
{
	coupdate_args_t cocall_args;
	int error;

	memset(&cocall_args, '\0', sizeof(cocall_args));

	switch(type) {
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
	case RESERVATION:
	default:
		err(EINVAL, "coupdate: invalid object type %d for coupdate", type);
		break;
	}
	cocall_args.nsobj_type = type;
	cocall_args.nsobj = nsobj;

	error = ukern_call(COCALL_COUPDATE, &cocall_args);
	if (error != 0) 
		err(errno, "cupdate: error performing cocall");
	else if (cocall_args.status == -1) {
		//TODO-PBB: handle errors better? or leave it to consumers?
		errno = cocall_args.error;
		return (NULL);
	}
	
	return (cocall_args.nsobj);	
}

int codelete(nsobject_t *nsobj, namespace_t *parent)
{
	codelete_args_t cocall_args;
	int error;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.nsobj = nsobj;
	cocall_args.ns_cap = parent;

	error = ukern_call(COCALL_CODELETE, &cocall_args);
	if (error != 0) 
		err(errno, "cupdate: error performing cocall");
	else if (cocall_args.status == -1)
		errno = cocall_args.error;

	return (cocall_args.status);
}


