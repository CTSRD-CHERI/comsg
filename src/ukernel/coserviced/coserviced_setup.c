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
#include "coserviced_setup.h"
#include "coserviced_endpoints.h"

#include "codiscover.h"
#include "codiscover2.h"
#include "coprovide.h"
#include "coprovide2.h"
#include "coserviced.h"
#include "coservice_table.h"
#include "coservice_cap.h"

#include <comsg/coservice.h>
#include <comsg/namespace.h>
#include <comsg/comsg_args.h>
#include <comsg/ukern_calls.h>


#include <err.h>
#include <sysexits.h>
#include <sys/auxv.h>
#include <sys/errno.h>

static struct _coservice_endpoint *fast_endpoint = NULL;

static struct _coservice_endpoint *
get_fast_coservice_endpoint(void)
{
	return (fast_endpoint);
}

static void 
init_service(coservice_provision_t *serv, char *name, int op)
{
	coservice_t *service = allocate_coservice();

	service->impl = get_fast_coservice_endpoint();
	if (service->impl == NULL) {
		service->impl = allocate_endpoint();
		service->impl->worker_scbs = get_fast_endpoints();
		service->impl->nworkers = get_fast_endpoint_count();
		service->impl->next_worker = 1;
		fast_endpoint = create_coservice_handle(service)->impl;
	}
	set_ukern_target(op, get_coservice_scb(unseal_endpoint(service->impl)));
	service->op = op;
	service->flags = NONE;
	serv->service = create_coservice_handle(service);

	serv->nsobj = coinsert(name, COSERVICE, serv->service, root_ns);
	if (serv->nsobj == NULL)
		err(EX_UNAVAILABLE, "%s: error coinserting %s into root namespace", __func__, name);
	set_ukernel_service(op, serv->service);
}

static void
process_capvec(void)
{
	int error;
	struct coserviced_capvec *capvec;
	void **capv;

	//todo-pbb: add checks to ensure these are valid
	error = elf_aux_info(AT_CAPV, &capv, sizeof(capv));
	capvec = (struct coserviced_capvec *)capv;
	set_ukern_target(COCALL_COPROC_INIT, capvec->coproc_init);
	set_ukern_target(COCALL_COINSERT, capvec->coinsert);
	set_ukern_target(COCALL_COSELECT, capvec->coselect);
	root_ns = capvec->root_ns;
	if (root_ns == NULL)
		err(EX_SOFTWARE, "%s: invalid root namespace in capvec!", __func__);
}

void 
coserviced_startup(void)
{
	void *codiscover_scb;
	coservice_t *coservice_cap;

	//install scb and root ns capabilities
	process_capvec();
	if (root_ns == NULL)
		err(EX_UNAVAILABLE, "%s: incorrect capvec", __func__);
	//init own services
	init_endpoints();
	
#pragma push_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma push_macro("COACCEPT_ENDPOINT")
#define DECLARE_COACCEPT_ENDPOINT(name, validate_f, operation_f) COACCEPT_ENDPOINT(name, COCALL_##name, validate_f, operation_f)
#define COACCEPT_ENDPOINT(name, op, validate, func) \
	init_service(&name##_serv, #name, op);
#include "coaccept_endpoints.inc"
#pragma pop_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma pop_macro("COACCEPT_ENDPOINT")

	codiscover_scb = get_coservice_scb(unseal_endpoint(fast_endpoint));
	coproc_init(NULL, NULL, NULL, codiscover_scb);
	//coproc_init_done(); /* not needed; folded into coproc_init (see coprocd/modules/core.d) */
}