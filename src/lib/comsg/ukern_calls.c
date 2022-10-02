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

#include <comsg/comsg_args.h>
#include <cocall/cocalls.h>

#include <comsg/coevent.h>
#include <comsg/coport.h>
#include <comsg/coservice.h>
#include <comsg/namespace.h>
#include <comsg/namespace_object.h>

#include <err.h>
#include <signal.h>
#include <unistd.h>
#include <sys/errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char *ukern_func_names[] = {
	"NOTUSED",
#pragma push_macro("UKERN_ENDPOINT")
#define UKERN_ENDPOINT(name) #name,
#include <comsg/ukern_calls.inc>
#pragma pop_macro("UKERN_ENDPOINT")
};

#pragma push_macro("UKERN_ENDPOINT")
#define UKERN_ENDPOINT(name) static coservice_t * name##_coservice = NULL;
#include <comsg/ukern_calls.inc>
#pragma pop_macro("UKERN_ENDPOINT")

static int slocall_funcs[] = {COCALL_SLOPOLL};
static int n_slocalls = 1;

namespace_t *root_ns = NULL;
bool is_ukernel = false;

static pthread_key_t ukern_call_set;

#if 0
#define err(n, s, ...) do { printf("%s: Errno=%d\n", s, n); kill(getpid(), SIGSEGV); } while(0)
#endif 

__attribute__((constructor)) static void 
init_ukern_calls(void)
{
	ukern_call_set = allocate_target_set();
	init_target_set(ukern_call_set, N_UKERN_CALLS);
	root_ns = NULL;
}

static void
init_new_thread_calls(void)
{
	nsobject_t *service_obj;

	/*
	 * We make cocall targets thread-local to reduce contention between threads in the same process. 
	 * It is the responsibility of the callee/service manager to manage contention between threads
	 * from different processes.
	 */
	set_cocall_target(ukern_call_set, COCALL_CODISCOVER,  get_global_target(COCALL_CODISCOVER));
	set_cocall_target(ukern_call_set, COCALL_COSELECT, get_global_target(COCALL_COSELECT));
	
	service_obj = coselect(U_COSELECT, COSERVICE, root_ns);
	discover_ukern_func(service_obj, COCALL_COSELECT);

	service_obj = coselect(U_CODISCOVER, COSERVICE, root_ns);
	discover_ukern_func(service_obj, COCALL_CODISCOVER);
}

static inline bool
is_slocall(cocall_num_t func)
{
	for (int i = 0; i < n_slocalls; i++) {
		if (slocall_funcs[i] == func)
			return (true);
	}
	return (false);
}

static int
call_ukern_target(cocall_num_t func, comsg_args_t *args)
{
	args->op = func;
	if (is_slocall(func))
		return (targeted_slocall(ukern_call_set, (int)func, args, sizeof(comsg_args_t)));
	else 
		return (targeted_cocall(ukern_call_set, (int)func, args, sizeof(comsg_args_t)));
}

static int 
ukern_call(cocall_num_t func, comsg_args_t *args)
{
	nsobject_t *func_obj;
	void *global_coselect_scb, *coselect_scb;

	args->op = func;
	errno = 0;
	if (get_cocall_target(ukern_call_set, (int)func) == NULL) {
		if ((get_global_target(COCALL_CODISCOVER) == NULL) || (get_global_target(COCALL_COSELECT) == NULL))
			err(ESRCH, "ukern_call: coproc_init either failed or has not been called");
		else if ((get_cocall_target(ukern_call_set, COCALL_CODISCOVER) == NULL) || (get_cocall_target(ukern_call_set, COCALL_COSELECT) == NULL))
			init_new_thread_calls();

		func_obj = coselect(ukern_func_names[func], COSERVICE, root_ns);
		if (func_obj == NULL)
			err(ENOSYS, "ukern_call: function %s is not present in the root namespace", ukern_func_names[func]);
		discover_ukern_func(func_obj, func);
	}
	return (call_ukern_target(func, args));
}

void 
discover_ukern_func(nsobject_t *service_obj, cocall_num_t function)
{
	coservice_t *service;
	void *scb;

	if (get_cocall_target(ukern_call_set, (int)function) != NULL)
		return;

    service = codiscover(service_obj, &scb);
    if (service == NULL)
    	err(errno, "discover_ukern_func: invalid service_obj");
    set_cocall_target(ukern_call_set, (int)function, scb);
}

void
set_ukern_target(cocall_num_t function, void *target)
{
	set_cocall_target(ukern_call_set, (int)function, target);
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
	
	error = call_ukern_target(COCALL_COSELECT, &cocall_args);
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
	
	error = call_ukern_target(COCALL_CODISCOVER, &cocall_args);;
	if (error == -1)
		err(errno, "codiscover: error performing cocall to codiscover");
	else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		err(errno, "codiscover: error during cocall to codiscover");
		return (NULL);
	}
	
	*scb = cocall_args.scb_cap;
	return (cocall_args.coservice);
	
}

coservice_t *
coprovide(void **worker_scbs, int nworkers, coservice_flags_t flags, int op)
{
	int error;
	coprovide_args_t cocall_args;
	void **scbs;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	size_t min_scb_array_len = nworkers * sizeof(void *);

	if (cheri_getlen(worker_scbs) < min_scb_array_len)
		err(EINVAL, "coprovide: length of worker_scbs %lu too short for %d workers", cheri_getlen(worker_scbs), nworkers);
	
	//TODO-PBB: check scb validity here.
	scbs = calloc(nworkers, sizeof(void *));
	for(int i = 0; i < nworkers; i++)
		scbs[i] = worker_scbs[i];
	cocall_args.worker_scbs = scbs;
	cocall_args.nworkers = nworkers;
	cocall_args.service_flags = flags;
	cocall_args.target_op = op;

	error = ukern_call(COCALL_COPROVIDE, &cocall_args);
	if(error) {
		err(errno, "coprovide: error in cocall");
	}
	
	free(scbs);
	if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	}
	else 
		return (cocall_args.service);
}

coservice_t *
coprovide2(struct _coservice_endpoint *ep, coservice_flags_t flags, int op)
{
	int error;
	coprovide_args_t cocall_args;
	void **scbs;

	memset(&cocall_args, '\0', sizeof(cocall_args));

	cocall_args.endpoint = ep;
	cocall_args.service_flags = flags;
	cocall_args.target_op = op;

	error = ukern_call(COCALL_COPROVIDE2, &cocall_args);
	if(error) {
		err(errno, "coprovide: error in cocall");
	}

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
coproc_init(namespace_t *root_ns_cap, void *coinsert_scb, void *coselect_scb, void *codiscover_scb)
{
	/* badly in need of some rework/wrapper */
	int error;
	
	coproc_init_args_t cocall_args;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.ns_cap = root_ns_cap;
	cocall_args.coinsert = coinsert_scb;
	cocall_args.codiscover = codiscover_scb;
	cocall_args.coselect = coselect_scb;

	//the target of this varies based on whether caller is a microkernel compartment & which compartment
	if (is_ukernel)
		error = targeted_slocall(ukern_call_set, COCALL_COPROC_INIT, &cocall_args, sizeof(coproc_init_args_t));
	else 
		error = call_ukern_target(COCALL_COPROC_INIT, &cocall_args);
	if (error != 0){
		err(errno, "coproc_init: cocall failed");
	} else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	}
	if (cocall_args.ns_cap == NULL && cocall_args.codiscover != NULL && root_ns != NULL) {
		set_cocall_target(ukern_call_set, COCALL_CODISCOVER, cocall_args.codiscover);
		return (root_ns);
	}
	root_ns = cocall_args.ns_cap;
	set_cocall_target(ukern_call_set, COCALL_COINSERT, cocall_args.coinsert);
	set_cocall_target(ukern_call_set, COCALL_CODISCOVER, cocall_args.codiscover);
	set_cocall_target(ukern_call_set, COCALL_COSELECT, cocall_args.coselect);
	
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
	} else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (-1);
	}

	return (0);
}

int 
cocarrier_send(coport_t *port, const void *buf, size_t len)
{
	/* Currently only for cocarriers, likely to change soon */
	cosend_args_t cocall_args;
	int error;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	
	cocall_args.cocarrier = port;
    buf = cheri_setbounds(buf, len);
    cocall_args.message = cheri_andperm(buf, COCARRIER_MSG_PERMS);
    cocall_args.length = len;
	cocall_args.oob_data.len = 0;
	cocall_args.oob_data.attachments = NULL;
    
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

int
cocarrier_recv(coport_t *port, void ** const buf, size_t len)
{
	/* Currently only for cocarriers, likely to change soon */
	corecv_args_t cocall_args;
	int error;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.cocarrier = port;
	cocall_args.length = len;
	cocall_args.oob_data.len = 0;
	cocall_args.oob_data.attachments = NULL;

	error = ukern_call(COCALL_CORECV, &cocall_args);
    if(error != 0)
        err(error, "cocarrier_recv: cocall failed");

    if (cocall_args.status == -1) {
        errno = cocall_args.error;
        return (NULL);
    } else if (cocall_args.oob_data.len != 0) {
        errno = EBADMSG;
        err(EX_SOFTWARE, "%s: out-of-band data present; use cocarrier_recv_oob instead", __func__);
    } else if (cocall_args.length != 0)
        *buf = cocall_args.message;

	return (cocall_args.status);
}

int
copoll(pollcoport_t *coports, int ncoports, int timeout)
{
	copoll_args_t cocall_args;
	int error;
	int function_variant;

	function_variant = COCALL_COPOLL; /* Use fastpath by default */
	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.ncoports = ncoports;
	cocall_args.timeout = timeout;

	/* We don't want this getting modified during the cocall */
	cocall_args.coports = malloc(ncoports * sizeof(pollcoport_t));
	memcpy(cocall_args.coports, coports, sizeof(pollcoport_t) * ncoports);
do_copoll:
	error = ukern_call(function_variant, &cocall_args);
	if(error != 0)
		err(errno, "copoll: cocall failed");
	if (cocall_args.status == -1) {
		if (cocall_args.error == EWOULDBLOCK && timeout != 0 && function_variant != COCALL_SLOPOLL) {
			function_variant = COCALL_SLOPOLL;
			goto do_copoll;
		} else {
			errno = cocall_args.error;
			free(cocall_args.coports);
			return (-1);
		}
	}
	memcpy(coports, cocall_args.coports, sizeof(pollcoport_t) * ncoports);
	free(cocall_args.coports);

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
	} else 
		return (0);

}

nsobject_t *
coupdate(nsobject_t *nsobj, nsobject_type_t type, void *subject)
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
		err(errno, "coupdate: error performing cocall");
	else if (cocall_args.status == -1) {
		//TODO-PBB: handle errors better? or leave it to consumers?
		errno = cocall_args.error;
		return (NULL);
	}
	
	return (cocall_args.nsobj);	
}

int
codelete(nsobject_t *nsobj, namespace_t *parent)
{
	codelete_args_t cocall_args;
	int error;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.nsobj = nsobj;
	cocall_args.ns_cap = parent;

	error = ukern_call(COCALL_CODELETE, &cocall_args);
	if (error != 0) 
		err(errno, "codelete: error performing cocall");
	else if (cocall_args.status == -1)
		errno = cocall_args.error;

	return (cocall_args.status);
}

int 
codrop(namespace_t *ns, namespace_t *parent)
{
	codrop_args_t cocall_args;
	int error;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.ns_cap = parent;
	cocall_args.child_ns_cap = ns;

	error = ukern_call(COCALL_CODROP, &cocall_args);
	if (error != 0)
		err(errno, "codrop: error performing cocall");
	else if (cocall_args.status == -1)
		errno = cocall_args.error;

	return (cocall_args.status);
}

int
ccb_install(cocallback_func_t *ccb_func, struct cocallback_args *ccb_args, coevent_t *coevent)
{
	ccb_install_args_t cocall_args;
	int error;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.ccb_func = ccb_func;
	memcpy(&cocall_args.ccb_args, ccb_args, sizeof(struct cocallback_args));
	cocall_args.coevent = coevent;

	error = ukern_call(COCALL_CCB_INSTALL, &cocall_args);
	if (error != 0)
		err(errno, "ccb_install: error performing cocall");
	else if (cocall_args.status == -1)
		errno = cocall_args.error;

	return (cocall_args.status);
}

cocallback_func_t *
ccb_register(void *scb, cocallback_flags_t flags)
{
	/* Currently only for cocarriers, likely to change soon */
	ccb_register_args_t cocall_args;
	int error;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.provider_scb = scb;
	cocall_args.flags = flags;

	error = ukern_call(COCALL_CCB_REGISTER, &cocall_args);
    if(error != 0)
        err(error, "ccb_register: cocall failed");

    if (cocall_args.status == -1) {
        errno = cocall_args.error;
        return (NULL);
    }

	return (cocall_args.ccb_func);
}

coevent_t *
colisten(coevent_type_t type, coevent_subject_t subject)
{
	/* Currently only for cocarriers, likely to change soon */
	colisten_args_t cocall_args;
	int error;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.subject = subject;
	cocall_args.event = type;

	error = ukern_call(COCALL_COLISTEN, &cocall_args);
    if(error != 0)
        err(error, "colisten: cocall failed");

    if (cocall_args.status == -1) {
        errno = cocall_args.error;
        return (NULL);
    }

	return (cocall_args.coevent);
}

int
cocarrier_send_oob(coport_t *port, const void *buf, size_t len, comsg_attachment_t *oob, size_t oob_len)
{
    cosend_args_t cocall_args;
    int error;

    memset(&cocall_args, '\0', sizeof(cocall_args));

    cocall_args.cocarrier = port;
    
    buf = cheri_setbounds(buf, len);
    cocall_args.message = cheri_andperm(buf, COCARRIER_MSG_PERMS);
    cocall_args.length = len;

    
    if (oob != NULL) {
        oob = cheri_setbounds(oob, oob_len * sizeof(comsg_attachment_t));
		cocall_args.oob_data.attachments = calloc(oob_len, sizeof(comsg_attachment_t));
        memcpy(cocall_args.oob_data.attachments, oob, cheri_getlen(oob));
        cocall_args.oob_data.len = oob_len;
    } else {
        //ignore length if oob is NULL
        cocall_args.oob_data.attachments = NULL;
        cocall_args.oob_data.len = 0;
    }

    error = ukern_call(COCALL_COSEND, &cocall_args);
    if(error)
        err(EX_SOFTWARE, "%s: cocall failed", __func__);

    if (cocall_args.status == -1) 
        errno = cocall_args.error;
    return (cocall_args.status);
}

int
cocarrier_recv_oob(coport_t *port, void ** const buf, size_t len, comsg_attachment_set_t *oob)
{
    corecv_args_t cocall_args;
    int error;

    memset(&cocall_args, '\0', sizeof(cocall_args));
    cocall_args.cocarrier = port;
    cocall_args.length = len;

    error = ukern_call(COCALL_CORECV, &cocall_args);
    if(error != 0)
        err(EX_SOFTWARE, "%s: cocall failed", __func__);
    if (cocall_args.status == -1) 
        errno = cocall_args.error;
    else {
        if (cocall_args.length != 0)
            *buf = cocall_args.message;
        if (cocall_args.oob_data.len != 0) {
            *oob = cocall_args.oob_data;
        } else {
            oob->attachments = 0;
            oob->len = 0;
       }
    }

	return (cocall_args.status);
}
