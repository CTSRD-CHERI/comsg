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
#include "nsd_setup.h"
#include "nsd_endpoints.h"

#include "namespace_table.h"
#include "nsd.h"
#include "nsd_cap.h"
#include "nsd_crud.h"
#include "nsd_lookup.h"

#include <comsg/comsg_args.h>
#include <cocall/cocalls.h>
#include <comsg/comsg_args.h>
#include <comsg/ukern_calls.h>
#include <comsg/coservice.h>
#include <comsg/namespace.h>
#include <comsg/namespace_object.h>

#include <err.h>
#include <sysexits.h>
#include <sys/auxv.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


static coservice_t *fast_endpoints = NULL;

static struct _coservice_endpoint *
get_fast_coservice_endpoint(void)
{
	if (fast_endpoints == NULL)
		return (NULL);
	return (fast_endpoints->impl);
}

/*
 * There is a race condition inherent in use of pids to identify processes.
 * Handling this is a question of correctness. 
 */

/* TODO-PBB: make this implementation more uniform with ipcd and coserviced */

static void 
startup_dance(void)
{
	//init own services
	void *coprovide_scb;
	void **endpoint_scbs;
	coservice_t *codiscover_service;
	nsobject_t *coprovide_nsobj;
	coservice_t *coprovide_service;

	root_ns = make_ns_handle(get_root_namespace());
	if (!cheri_gettag(root_ns))
		err(EX_SOFTWARE, "%s: root namespace cap lacks tag!!", __func__);

	endpoint_scbs = get_fast_endpoints();
	set_ukern_target(COCALL_COSELECT, endpoint_scbs[0]);
	set_ukern_target(COCALL_COINSERT, endpoint_scbs[1]);
	
	/* codiscover no longer returned from coproc_init */
	/* this wrecks load balancing by putting all main threads on the first worker */

	//connect to process daemon and do the startup dance (we can dance if we want to)
	if (coproc_init(root_ns, endpoint_scbs[0], endpoint_scbs[1], NULL) == NULL)
		err(EX_SOFTWARE, "%s: cocall failed", __func__);

	if (!cheri_gettag(root_ns))
		err(EX_SOFTWARE, "%s: root namespace cap lacks tag!!", __func__);

	/* Once coprovide has been inserted into the root namespace, we can make progress */
	while(lookup_coservice(U_COPROVIDE, root_ns) == NULL)
		sched_yield(); /* would microsleep be better? or proper event delivery? */
	
	do {
		if (coproc_init(NULL, NULL, NULL, NULL) != root_ns) {
			if (errno == EAGAIN) {
				sched_yield();
				continue;
			} else
				err(EX_SOFTWARE, "%s: failed to get codiscover scb from coprocd", __func__);
		}
	} while (0);
	coprovide_nsobj = lookup_nsobject(U_COPROVIDE, COSERVICE, root_ns);
	coprovide_service = codiscover(coprovide_nsobj, &coprovide_scb);
	set_ukernel_service(COCALL_COPROVIDE, coprovide_service);
	set_ukern_target(COCALL_COPROVIDE, coprovide_scb);
	set_ukernel_service(COCALL_CODISCOVER, lookup_coservice(U_CODISCOVER, root_ns));
	set_ukernel_service(COCALL_CODISCOVER2, lookup_coservice(U_CODISCOVER2, root_ns));
}

static void 
start_fast_service(coservice_provision_t *serv, int op)
{
	struct _coservice_endpoint *ep = get_fast_coservice_endpoint();
	if (ep == NULL) {
		fast_endpoints = coprovide(get_fast_endpoints(), (int)get_fast_endpoint_count(), (coservice_flags_t)NONE, op);
		serv->service = fast_endpoints;
	} else
		serv->service = coprovide2(ep, NONE, op);
	if (serv->service == NULL)
		err(EX_SOFTWARE, "%s: error creating/getting endpoint coservice when initing %s", __func__, serv->nsobj->name);
	if (update_nsobject(serv->nsobj, serv->service, COSERVICE) != 0)
		err(EX_SOFTWARE, "%s: error inserting %s into root namespace", __func__, serv->nsobj->name);
	set_ukernel_service(op, serv->service);
}

static void 
init_service(coservice_provision_t *service_prov, const char * name)
{
	service_prov->nsobj = new_nsobject(name, RESERVATION, root_ns);
}

static void
process_capvec(void)
{
	int error;
	struct nsd_capvec *capvec;
	void **capv;

	//todo-pbb: add checks to ensure these are valid
	error = elf_aux_info(AT_CAPV, &capv, sizeof(capv));
	capvec = (struct nsd_capvec *)capv;
	set_ukern_target(COCALL_COPROC_INIT, capvec->coproc_init);
	set_ukern_target(COCALL_COPROC_INIT_DONE, capvec->coproc_init_done);
}

void init_services(void)
{
	process_capvec();
	init_endpoints();

#pragma push_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma push_macro("COACCEPT_ENDPOINT")
#define DECLARE_COACCEPT_ENDPOINT(name, validate_f, operation_f) COACCEPT_ENDPOINT(name, name, validate_f, operation_f)
#define COACCEPT_ENDPOINT(name, op, validate, func) \
	init_service(&name##_serv, #name);
#include "coaccept_endpoints.inc"
#pragma pop_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma pop_macro("COACCEPT_ENDPOINT")

	startup_dance();

#pragma push_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma push_macro("COACCEPT_ENDPOINT")
#define DECLARE_COACCEPT_ENDPOINT(name, validate_f, operation_f) COACCEPT_ENDPOINT(name, COCALL_##name, validate_f, operation_f)
#define COACCEPT_ENDPOINT(name, op, validate, func) \
	start_fast_service(&name##_serv, op);
#include "coaccept_endpoints.inc"
#pragma pop_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma pop_macro("COACCEPT_ENDPOINT")

	coproc_init_done();
}