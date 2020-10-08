/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
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

#include "namespace_table.h"
#include "nsd.h"
#include "nsd_cap.h"
#include "nsd_crud.h"
#include "nsd_lookup.h"

#include <cocall/cocall_args.h>
#include <cocall/cocalls.h>
#include <cocall/cocall_args.h>
#include <cocall/worker.h>
#include <cocall/worker_map.h>
#include <comsg/ukern_calls.h>
#include <coproc/coservice.h>
#include <coproc/namespace.h>
#include <coproc/namespace_object.h>

#include <err.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * There is a race condition inherent in use of pids to identify processes.
 * Handling this is a question of correctness. 
 */

/* TODO-PBB: make this implementation more uniform with ipcd and coserviced */

static
void startup_dance(void)
{
	//init own services
	void **coinsert_scbs, **coselect_scbs, *coprovide_scb;
	coservice_t *coprovide_service;
	nsobject_t *obj;
	namespace_t *unsealed_global_ns;
	
	unsealed_global_ns  = get_global_namespace();
	
	global_ns = seal_ns(global_ns);
	
	coinsert_scbs = get_worker_scbs(coinsert_serv.function_map);
	coselect_scbs = get_worker_scbs(coselect_serv.function_map);
	//connect to process daemon and do the startup dance (we can dance if we want to)
	if (coproc_init(global_ns, coinsert_scbs[0], coselect_scbs[0], NULL) == NULL)
		err(errno, "coproc_init: cocall failed");

	/* Once coprovide has been inserted into the global namespace, we can make progress */
	while(lookup_coservice(U_COPROVIDE, global_ns) == NULL)
		sched_yield();

	obj = lookup_nsobject(U_COPROVIDE, COSERVICE, global_ns);
	discover_ukern_func(obj, COCALL_COPROVIDE);
	
	coinsert_serv.service = coprovide(coinsert_scbs, NSD_NWORKERS);
	coinsert_serv.nsobj = create_nsobject(U_COINSERT, COSERVICE, global_ns);
	coinsert_serv.nsobj->coservice = coinsert_serv.service;

	coselect_serv.service = coprovide(coselect_scbs, NSD_NWORKERS);
	coselect_serv.nsobj = create_nsobject(U_COSELECT, COSERVICE, global_ns);
	coselect_serv.nsobj->coservice = coselect_serv.service;
}

static
void init_global_service(const char * name, coservice_provision_t *service_prov)
{
	void **scbs;
	
	scbs = get_worker_scbs(service_prov->function_map);
	service_prov->service = coprovide(scbs, NSD_NWORKERS);

	nsobject_t *obj = create_nsobject(name, COSERVICE, global_ns);
	obj->coservice = service_prov->service;
	service_prov->nsobj = obj;
	return;
}

void init_services(void)
{
	coinsert_serv.function_map = spawn_workers(namespace_object_insert, validate_coinsert_args, NSD_NWORKERS);
	coupdate_serv.function_map = spawn_workers(namespace_object_update, validate_coupdate_args, NSD_NWORKERS);
	coselect_serv.function_map = spawn_workers(namespace_object_select, validate_coselect_args, NSD_NWORKERS);
	codelete_serv.function_map = spawn_workers(namespace_object_delete, validate_codelete_args, NSD_NWORKERS);
	cocreate_serv.function_map = spawn_workers(namespace_create, validate_cocreate_args, NSD_NWORKERS);
	codrop_serv.function_map = spawn_workers(namespace_drop, validate_codrop_args, NSD_NWORKERS);

	startup_dance();
	/* COINSERT and COSELECT inited by safety_dance */
	init_global_service(U_COUPDATE, &coupdate_serv);
	init_global_service(U_CODELETE, &codelete_serv);
	init_global_service(U_COCREATE, &cocreate_serv);
	init_global_service(U_CODROP, &codrop_serv);

	return;
}