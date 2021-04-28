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
#include "ipcd_startup.h"
#include "copoll_deliver.h"

#include "coclose.h"
#include "coopen.h"
#include "copoll.h"
#include "coport_table.h"
#include "cosend.h"
#include "corecv.h"
#include "ipcd.h"


#include <cocall/worker_map.h>
#include <cocall/cocall_args.h>
#include <coproc/namespace.h>
#include <coproc/namespace_object.h>
#include <comsg/ukern_calls.h>

#include <assert.h>
#include <cheri/cheric.h>
#include <err.h>
#include <sys/auxv.h>
#include <sys/errno.h>

static 
void init_service(coservice_provision_t *serv, void *func, void *valid, const char *name)
{

	serv->function_map = spawn_workers(func, valid, IPCD_NWORKERS);
	serv->service = coprovide(get_worker_scbs(serv->function_map), IPCD_NWORKERS);
	serv->nsobj = coinsert(name, COSERVICE, serv->service, global_ns);
	if (serv->nsobj == NULL)
		err(errno, "init_service: error inserting %s into global namespace", name);
}

static 
void init_slow_service(coservice_provision_t *serv, void *func, void *valid, const char *name)
{
	serv->function_map = spawn_slow_workers(func, valid, IPCD_NWORKERS);
	serv->service = coprovide(get_worker_scbs(serv->function_map), IPCD_NWORKERS);
	serv->nsobj = coinsert(name, COSERVICE, serv->service, global_ns);
	if (serv->nsobj == NULL)
		err(errno, "init_service: error inserting %s into global namespace", name);
}

static void
process_capvec(void)
{
	int error;
	struct ipcd_capvec *capvec;
	void **capv;

	//todo-pbb: add checks to ensure these are valid
	error = elf_aux_info(AT_CAPV, &capv, sizeof(capv));
	capvec = (struct ipcd_capvec *)capv;
	set_ukern_target(COCALL_COPROC_INIT_DONE, capvec->coproc_init_done);
	set_ukern_target(COCALL_CODISCOVER, capvec->codiscover);
	set_ukern_target(COCALL_COINSERT, capvec->coinsert);
	set_ukern_target(COCALL_COSELECT, capvec->coselect);
	global_ns = capvec->global_ns;
}

void ipcd_startup(void)
{
	nsobject_t *coprovide_nsobj;
	void *coprovide_scb;

	setup_copoll_notifiers();
	
	global_ns = coproc_init(NULL, NULL, NULL, NULL);
	if (global_ns == NULL)
		err(errno, "ipcd_startup: cocall failed");

	do {
		coprovide_nsobj = coselect(U_COPROVIDE, COSERVICE, global_ns);
		if(cheri_gettag(coprovide_nsobj) == 0)
			continue;
		else if (cheri_gettag(coprovide_nsobj->obj) == 0)
			continue;
	} while (0);
	
	codiscover(coprovide_nsobj, &coprovide_scb);
	set_ukern_target(COCALL_COPROVIDE, coprovide_scb);
	//init own services
	init_service(&coopen_serv, coport_open, validate_coopen_args, U_COOPEN);
	init_service(&coclose_serv, coport_close, validate_coclose_args, U_COCLOSE);
	init_service(&cosend_serv, coport_send, validate_cosend_args, U_COSEND);
	init_service(&corecv_serv, coport_recv, validate_corecv_args, U_CORECV);
	init_service(&copoll_serv, cocarrier_poll, validate_copoll_args, U_COPOLL);
	init_slow_service(&slopoll_serv, cocarrier_poll_slow, validate_copoll_args, U_SLOPOLL);

	coproc_init_done();

}