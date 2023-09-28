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
#include <stdatomic.h>
#include <sysexits.h>
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

static _Atomic(coservice_t *) ukernel_services[] = {
	NULL,
#pragma push_macro("UKERN_ENDPOINT")
#define UKERN_ENDPOINT(name) NULL,
#include <comsg/ukern_calls.inc>
#pragma pop_macro("UKERN_ENDPOINT")
};

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

static bool
is_core_call(cocall_num_t func)
{
	if (func == COCALL_CODISCOVER || func == COCALL_COSELECT || func == COCALL_CODISCOVER2)
		return (true);
	return (false);
}

coservice_t *
get_ukernel_service(cocall_num_t func)
{
	nsobject_t *service_obj;
	coservice_t *s, *tmp;
	void *scb = NULL;

	if (((s = atomic_load(&ukernel_services[func])) == NULL)) {
		service_obj = coselect(ukern_func_names[func], COSERVICE, root_ns);
		if (service_obj == NULL) {
			errno = ENOSYS;
			err(EX_SOFTWARE, "%s: function %s is not present in the root namespace", __func__, ukern_func_names[func]);
		}
		atomic_compare_exchange_strong(&ukernel_services[func], &s, service_obj->coservice);
		tmp = codiscover(service_obj, &scb);
		if (tmp != NULL && scb != NULL)
			set_ukern_target(func, scb);
	} else {
		if (get_cocall_target(ukern_call_set, COCALL_CODISCOVER2) == NULL)  {
			errno = EDOOFUS;
			err(EX_SOFTWARE, "%s: called before %s was initialized", __func__, ukern_func_names[COCALL_CODISCOVER2]);
		}
		scb = codiscover2(s);
		if (scb != NULL)
			set_ukern_target(func, scb);
	}
	return (s);
}

void
set_ukernel_service(cocall_num_t func, coservice_t *s)
{
	if (is_ukernel)
		atomic_store(&ukernel_services[func], s);
	else
		return;
}

static bool
should_init_thread(void)
{
	bool res;
	res = (get_cocall_target(ukern_call_set, COCALL_CODISCOVER) == NULL);
	res |= (get_cocall_target(ukern_call_set, COCALL_COSELECT) == NULL);
	res |= (get_cocall_target(ukern_call_set, COCALL_CODISCOVER2) == NULL);
	res |= (atomic_load(&ukernel_services[COCALL_CODISCOVER]) == NULL);
	res |= (atomic_load(&ukernel_services[COCALL_COSELECT]) == NULL);
	res |= (atomic_load(&ukernel_services[COCALL_CODISCOVER2]) == NULL);
	res &= (get_global_target(COCALL_CODISCOVER) != NULL && get_global_target(COCALL_COSELECT) != NULL);
	return (res);
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
	if (get_global_target(COCALL_CODISCOVER) == NULL || get_global_target(COCALL_COSELECT) == NULL) {
		errno = ENOTCONN;
		err(EX_UNAVAILABLE, "%s: coproc_init not called; unable to perform cocalls", __func__);
	}
	if (get_cocall_target(ukern_call_set, COCALL_CODISCOVER) == NULL)
		set_ukern_target(COCALL_CODISCOVER,  get_global_target(COCALL_CODISCOVER));
	if (get_cocall_target(ukern_call_set, COCALL_CODISCOVER) == NULL)
		set_ukern_target(COCALL_CODISCOVER, get_global_target(COCALL_CODISCOVER));
	
	if (get_cocall_target(ukern_call_set, COCALL_CODISCOVER2) == NULL) {
		if (get_global_target(COCALL_CODISCOVER2) == NULL) {
			if (atomic_load(&ukernel_services[COCALL_CODISCOVER2]) != NULL) {
				errno = EDOOFUS;
				err(EX_SOFTWARE, "%s: codiscover2 service present but no codiscover2 scb can be found", __func__);
			}
			get_ukernel_service(COCALL_CODISCOVER2);
		} else {
			set_ukern_target(COCALL_CODISCOVER2,  get_global_target(COCALL_CODISCOVER2));
			get_ukernel_service(COCALL_CODISCOVER2);
		}
	}
	get_ukernel_service(COCALL_CODISCOVER);
	get_ukernel_service(COCALL_COSELECT);
}

void
comsg_thread_force_init(void)
{
	init_new_thread_calls();
}

static inline bool
is_slocall(cocall_num_t func)
{
	coservice_t *s;

	if ((s = atomic_load(&ukernel_services[func])) != NULL) {
		if ((s->flags & SLOWPATH) != 0)
			return (true);
	}
	return (false);
}

static void *
refresh_target_scb(cocall_num_t func)
{
	coservice_t *s;
	void *scb;

	s = atomic_load(&ukernel_services[func]);
	if (s == NULL) {
		s = get_ukernel_service(func);
		return (get_cocall_target(ukern_call_set, func));
	} 
	scb = codiscover2(s);
	if (scb != NULL)
		set_ukern_target(func, scb);
	return (scb);
}

static int
call_ukern_target(cocall_num_t func, comsg_args_t *args)
{
	coservice_t *s;

	if ((s = atomic_load(&ukernel_services[func])) != NULL)
		args->op = s->op;
	else
		args->op = func;
	errno = 0;
	if (!is_slocall(func))
		return (targeted_cocall(ukern_call_set, (int)func, args, sizeof(comsg_args_t)));
	else 
		return (targeted_slocall(ukern_call_set, (int)func, args, sizeof(comsg_args_t)));
}

static int
call_ukern_service(cocall_num_t func, comsg_args_t *args)
{
	int error = 0;
	void *scb = NULL;
	for(;;) {
		error = call_ukern_target(func, args);
		if (error != 0) { /* try the alternative endpoints if we failed because it's busy, else fail */
			if (errno == EBUSY) {
				if (scb == NULL) {
					scb = refresh_target_scb(func);
					continue;
				} else if (scb == refresh_target_scb(func)) {
					errno = EBUSY;
					return (-1);
				}
			} else
				return (error);
		}
		break;
	} 
	return (error);
}

static int 
ukern_call(cocall_num_t func, comsg_args_t *args)
{
	void *func_scb, *orig_func_scb;
	void *global_coselect_scb, *coselect_scb;
	int error;
	
	if ((func_scb = get_cocall_target(ukern_call_set, (int)func)) == NULL) {
		if ((get_global_target(COCALL_CODISCOVER) == NULL) || (get_global_target(COCALL_COSELECT) == NULL)) {
			errno = ENOTCONN;
			abort();
			err(EX_SOFTWARE, "%s: coproc_init either failed or has not been called; attempted call was %s", __func__, ukern_func_names[func]);
		} else if (should_init_thread())
			init_new_thread_calls();
		get_ukernel_service(func);
	}
	return (call_ukern_service(func, args));
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
	if (strnlen(name, NS_NAME_LEN) == NS_NAME_LEN) {
		errno = ENAMETOOLONG;
		err(EX_SOFTWARE, "%s: name exceeds maximum supported length of %lu", __func__, NS_NAME_LEN);
	}

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
		errno = EINVAL;
		err(EX_SOFTWARE, "%s: invalid object type %d for coinsert", __func__, type);
		break;
	}
	cocall_args.nsobj_type = type;

	error = ukern_call(COCALL_COINSERT, &cocall_args);
	if (error != 0) 
		err(EX_SOFTWARE, "%s: error performing cocall", __func__);
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
	if (strlen(name) > NS_NAME_LEN) {
		errno = ENAMETOOLONG;
		err(EX_SOFTWARE, "%s: name exceeds maximum supported length of %lu", __func__, NS_NAME_LEN);
	}
	strncpy(cocall_args.nsobj_name, name, NS_NAME_LEN);

	cocall_args.nsobj_type = type;
	cocall_args.nsobj = NULL;
	cocall_args.ns_cap = ns;
	
	error = ukern_call(COCALL_COSELECT, &cocall_args);
	if (error == -1)
		err(EX_SOFTWARE, "%s: error performing cocall to coselect", __func__);
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
	
	error = ukern_call(COCALL_CODISCOVER, &cocall_args);
	if (error == -1) {
		if (errno == EAGAIN)
			return (NULL);
		err(EX_SOFTWARE, "%s: error performing cocall to codiscover", __func__);
	} else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		err(EX_SOFTWARE, "%s: error during cocall to codiscover", __func__);
		return (NULL);
	}
	if (cheri_gettag(scb))
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

	if (cheri_getlen(worker_scbs) < min_scb_array_len) {
		errno = EINVAL;
		err(EX_SOFTWARE, "%s: length of worker_scbs %lu too short for %d workers", __func__, cheri_getlen(worker_scbs), nworkers);
	}
	
	//TODO-PBB: check scb validity here.
	scbs = calloc(nworkers, sizeof(void *));
	for(int i = 0; i < nworkers; i++)
		scbs[i] = worker_scbs[i];
	cocall_args.worker_scbs = scbs;
	cocall_args.nworkers = nworkers;
	cocall_args.service_flags = flags;
	cocall_args.target_op = op;

	error = ukern_call(COCALL_COPROVIDE, &cocall_args);
	if(error)
		err(EX_SOFTWARE, "%s: error in cocall", __func__);
	free(scbs);
	if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	}
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
		err(EX_SOFTWARE, "%s: error in cocall", __func__);
	} else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	}
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
		err(EX_SOFTWARE, "%s: cocall failed", __func__);
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
	if (is_ukernel) {
		cocall_args.ns_cap = root_ns_cap;
		cocall_args.coinsert = coinsert_scb;
		cocall_args.codiscover = codiscover_scb;
		cocall_args.coselect = coselect_scb;
	}

	//the target of this varies based on whether caller is a microkernel compartment & which compartment
	if (is_ukernel)
		error = targeted_slocall(ukern_call_set, COCALL_COPROC_INIT, &cocall_args, sizeof(coproc_init_args_t));
	else 
		error = call_ukern_target(COCALL_COPROC_INIT, &cocall_args);
	if (error != 0){
		err(EX_SOFTWARE, "%s: cocall failed", __func__);
	} else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		return (NULL);
	} else if (cocall_args.ns_cap == NULL) {
		errno = EDOOFUS;
		err(EX_SOFTWARE, "%s: coproc_init failed but did not report an error", __func__);
	}
	root_ns = cocall_args.ns_cap;
	if (cocall_args.coinsert != NULL)
		set_ukern_target(COCALL_COINSERT, cocall_args.coinsert);
	if (cocall_args.codiscover != NULL)
		set_ukern_target(COCALL_CODISCOVER, cocall_args.codiscover);
	if (cocall_args.coselect != NULL)
		set_ukern_target(COCALL_COSELECT, cocall_args.coselect);
	
	return (cocall_args.ns_cap);
}

int
coproc_init_done(void)
{
	int error;
	coproc_init_args_t cocall_args;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	if (!is_ukernel) {
		errno = EPERM;
		err(EX_SOFTWARE, "%s: microkernel only calls should not be made by user programs", __func__);
	}
	error = targeted_slocall(ukern_call_set, COCALL_COPROC_INIT_DONE, &cocall_args, sizeof(coproc_init_args_t));
	if(error){
		err(EX_SOFTWARE, "%s: cocall failed", __func__);
	} else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		err(EX_SOFTWARE, "%s: coproc_init_done failed", __func__);
		return (-1);
	}

	return (0);
}

int 
cocarrier_send(const coport_t *port, const void *buf, size_t len)
{
	/* Currently only for cocarriers, likely to change soon */
	cosend_args_t cocall_args;
	int error;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	
	cocall_args.cocarrier = (coport_t *)port;
    buf = cheri_setbounds(buf, len);
    cocall_args.message = (void *)cheri_andperm(buf, COCARRIER_MSG_PERMS);
    cocall_args.length = len;
	cocall_args.oob_data.len = 0;
	cocall_args.oob_data.attachments = NULL;
    
    error = ukern_call(COCALL_COSEND, &cocall_args);
    if (error != 0)
        err(EX_SOFTWARE, "%s: cocall failed", __func__);

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
        err(EX_SOFTWARE, "%s: cocall failed", __func__);

    if (cocall_args.status == -1) {
        errno = cocall_args.error;
        return (NULL);
    }

	return (cocall_args.port);
}

int
cocarrier_recv(const coport_t *port, void ** const buf, size_t len)
{
	/* Currently only for cocarriers, likely to change soon */
	corecv_args_t cocall_args;
	int error;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.cocarrier = (coport_t *)port;
	cocall_args.length = len;
	cocall_args.oob_data.len = 0;
	cocall_args.oob_data.attachments = NULL;

	error = ukern_call(COCALL_CORECV, &cocall_args);
    if(error != 0) {
        err(EX_SOFTWARE, "%s: cocall failed", __func__);
	}

    if (cocall_args.status == -1) {
        errno = cocall_args.error;
        return (-1);
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

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.ncoports = ncoports;
	cocall_args.timeout = timeout;

	/* We don't want this getting modified during the cocall */
	cocall_args.coports = malloc(ncoports * sizeof(pollcoport_t));
	memcpy(cocall_args.coports, coports, sizeof(pollcoport_t) * ncoports);
do_copoll:
	error = ukern_call(COCALL_COPOLL, &cocall_args);
	if(error != 0)
		err(EX_SOFTWARE, "%s: cocall failed", __func__);
	if (cocall_args.status == -1) {
		if (cocall_args.error == EWOULDBLOCK && timeout != 0) {
			error = ukern_call(COCALL_SLOPOLL, &cocall_args);
			if(error != 0)
				err(EX_SOFTWARE, "%s: cocall failed (slopoll)", __func__);
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
		err(EX_SOFTWARE, "%s: cocall failed", __func__);
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
		errno = EINVAL;
		err(EX_SOFTWARE, "%s: invalid object type %d for coupdate", __func__, type);
		break;
	}
	cocall_args.nsobj_type = type;
	cocall_args.nsobj = nsobj;

	error = ukern_call(COCALL_COUPDATE, &cocall_args);
	if (error != 0) 
		err(EX_SOFTWARE, "%s: error performing cocall", __func__);
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
		err(EX_SOFTWARE, "%s: error performing cocall", __func__);
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
		err(EX_SOFTWARE, "%s: error performing cocall", __func__);
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
		err(EX_SOFTWARE, "%s: error performing cocall", __func__);
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
        err(EX_SOFTWARE, "%s: cocall failed", __func__);

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
        err(EX_SOFTWARE, "%s: cocall failed", __func__);

    if (cocall_args.status == -1) {
        errno = cocall_args.error;
        return (NULL);
    }

	return (cocall_args.coevent);
}

int
cocarrier_send_oob(const coport_t *port, const void *buf, size_t len, comsg_attachment_t *oob, size_t oob_len)
{
    cosend_args_t cocall_args;
    int error;

    memset(&cocall_args, '\0', sizeof(cocall_args));

    cocall_args.cocarrier = (coport_t *)port;
    
    buf = cheri_setbounds(buf, len);
    cocall_args.message = (void *)cheri_andperm(buf, COCARRIER_MSG_PERMS);
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
cocarrier_recv_oob(const coport_t *port, void ** const buf, size_t len, comsg_attachment_set_t *oob)
{
    corecv_args_t cocall_args;
    int error;

    memset(&cocall_args, '\0', sizeof(cocall_args));
    cocall_args.cocarrier = (coport_t *)port;
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

void *
codiscover2(coservice_t *s)
{
	int error;
	codiscover_args_t cocall_args;

	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.scb_cap = NULL;
	cocall_args.coservice = s;
	
	error = ukern_call(COCALL_CODISCOVER2, &cocall_args);
	if (error == -1) {
		err(EX_SOFTWARE, "%s: error performing cocall to codiscover2", __func__);
	} else if (cocall_args.status == -1) {
		errno = cocall_args.error;
		err(EX_SOFTWARE, "%s: error during cocall to codiscover2", __func__);
		return (NULL);
	}
	return (cocall_args.scb_cap);
	
}

int
coport_msg_free(coport_t *port, void *ptr)
{
	cosend_args_t cocall_args;
	int error;
	
	memset(&cocall_args, '\0', sizeof(cocall_args));
	cocall_args.message = ptr;
	cocall_args.cocarrier = port;
	
	error = ukern_call(COCALL_COPORT_MSG_FREE, &cocall_args);
    if(error != 0)
        err(EX_SOFTWARE, "%s: cocall failed", __func__);

    if (cocall_args.status == -1) {
        errno = cocall_args.error;
    }
	
	return (cocall_args.status);
}

void begin_cocall(void)
{
	return;
}

void end_cocall(void)
{
	return;
}
