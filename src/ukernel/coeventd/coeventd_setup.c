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
#include "coeventd_setup.h"
#include "coeventd_endpoints.h"
#include "coeventd.h"

#include "cocallback_func_utils.h"
#include "procdeath_tbl.h"

#include <comsg/ukern_calls.h>
#include <comsg/namespace_object.h>

#include <err.h>
#include <sysexits.h>
#include <sys/auxv.h>
#include <sys/errno.h>
#include <unistd.h>

static coservice_t *fast_endpoints = NULL;

static struct _coservice_endpoint *
get_fast_coservice_endpoint(void)
{
	if (fast_endpoints == NULL)
		return (NULL);
	return (fast_endpoints->impl);
}

static void 
init_service(coservice_provision_t *serv, char *name, int op)
{
	struct _coservice_endpoint *ep = get_fast_coservice_endpoint();
	if (ep == NULL) {
		fast_endpoints = coprovide(get_fast_endpoints(), (int)get_fast_endpoint_count(), (coservice_flags_t)NONE, op);
		serv->service = fast_endpoints;
	} else
		serv->service = coprovide2(ep, NONE, op);

	if (serv->service == NULL)
		err(EX_SOFTWARE, "%s: error creating/getting endpoint coservice when initing %s", __func__, name);
	serv->nsobj = coinsert(name, COSERVICE, serv->service, root_ns);
	if (serv->nsobj == NULL)
		err(EX_SOFTWARE, "%s: error inserting %s into root namespace", __func__, name);
}

static void
init_callback_tables(void)
{
	init_cocallback_func_utils();
	setup_procdeath_table();
}

static void
process_capvec(void)
{
	int error;
	struct coeventd_capvec *capvec;
	void **capv;

	//todo-pbb: add checks to ensure these are valid
	error = elf_aux_info(AT_CAPV, &capv, sizeof(capv));
	capvec = (struct coeventd_capvec *)capv;
	set_ukern_target(COCALL_COPROC_INIT_DONE, capvec->coproc_init_done);
	set_ukern_target(COCALL_CODISCOVER, capvec->codiscover);
	set_ukern_target(COCALL_COINSERT, capvec->coinsert);
	set_ukern_target(COCALL_COSELECT, capvec->coselect);
	root_ns = capvec->root_ns;
}

void
coeventd_startup(void)
{
	nsobject_t *coprovide_nsobj;
	void *coprovide_scb;
	init_callback_tables();

	process_capvec();
	if (root_ns == NULL)
		err(errno, "coeventd_startup: cocall failed");

	do {
		coprovide_nsobj = coselect(U_COPROVIDE, COSERVICE, root_ns);
		if(cheri_gettag(coprovide_nsobj) == 0)
			continue;
		else if (cheri_gettag(coprovide_nsobj->obj) == 0)
			continue;
	} while (0);

	codiscover(coprovide_nsobj, &coprovide_scb);
	set_ukern_target(COCALL_COPROVIDE, coprovide_scb);
	init_endpoints();

#pragma push_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma push_macro("COACCEPT_ENDPOINT")
#define DECLARE_COACCEPT_ENDPOINT(name, validate_f, operation_f) COACCEPT_ENDPOINT(name, COCALL_##name, validate_f, operation_f)
#define COACCEPT_ENDPOINT(name, op, validate, func) \
	init_service(&name##_serv, #name, op);
#include "coaccept_endpoints.inc"
#pragma pop_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma pop_macro("COACCEPT_ENDPOINT")

	coproc_init_done();
}