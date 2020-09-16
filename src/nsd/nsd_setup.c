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
#include "nsd.h"
#include "namespace_table.h"
#include "nsd_crud.h"

#include <cocall/cocall_args.h>
#include <cocall/cocalls.h>
#include <cocall/cocall_args.h>
#include <cocall/worker.h>
#include <cocall/worker_map.h>
#include <comsg/ukern_calls.h>
#include <coproc/coservice.h>
#include <coproc/namespace.h>
#include <coproc/namespace_object.h>

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
void safety_dance(void)
{
	//init own services
	void **coinsert_scbs, **coselect_scbs, *coprovide_scb;
	coservice_t *coprovide_service;
	nsobject_t *obj;
	namespace_t *global_ns, *sealed_global_ns;
	
	global_ns = get_global_namespace();
	sealed_global_ns = seal_ns(global_ns);
	
	coinsert_scbs = get_worker_scbs(coinsert_serv.function_map);
	coselect_scbs = get_worker_scbs(coselect_serv.function_map);
	//connect to process daemon and do the startup dance (we can dance if we want to)
	if (coproc_init(sealed_global_ns, coinsert_scbs[0], coselect_scbs[0], NULL) == NULL)
		err(error, "coproc_init: cocall failed");

	/* Once coprovide has been inserted into the global namespace, we can make progress*/
	while(lookup_coservice(U_COPROVIDE, global_ns) == NULL)
		sched_yield();

	coprovide_service = codiscover(U_COPROVIDE, global_ns, &coprovide_scb);
	set_cocall_target(ukern_call_set, COCALL_COPROVIDE, coprovide_scb);
	
	coinsert_serv.service = coprovide(coinsert_scbs, nworkers);
	coinsert_serv.nsobj = create_nsobject(U_COINSERT, COSERVICE, global_ns);
	coinsert_serv.nsobj->coservice = coinsert_serv.service;

	coselect_serv.service = coprovide(coselect_scbs, nworkers);
	coselect_serv.nsobj = create_nsobject(U_COSELECT, COSERVICE, global_ns);
	coselect_serv.nsobj->coservice = coselect_serv.service;
}

static
void init_global_service(const char * name, coservice_provision_t *service_prov)
{
	void **scbs;
	static namespace_t *global = get_global_namespace();
	
	scbs = get_worker_scbs(service_prov->function_map);
	service_prov->service = coprovide(scbs, nworkers);

	nsobject_t *obj = create_nsobject(name, COSERVICE, global);
	obj->coservice = service_prov->service;
	service_prov->nsobj = obj;
	return;
}

void init_services(void)
{
	coinsert_serv.function_map = spawn_workers(insert_namespace_object, validate_coinsert_args, nworkers);
	coupdate_serv.function_map = spawn_workers(update_namespace_object, validate_coupdate_args, nworkers);
	coselect_serv.function_map = spawn_workers(select_namespace_object, validate_coselect_args, nworkers);
	codelete_serv.function_map = spawn_workers(delete_namespace_object, validate_codelete_args, nworkers);
	cocreate_serv.function_map = spawn_workers(create_namespace, validate_cocreate_args, nworkers);
	codrop_serv.function_map = spawn_workers(drop_namespace, validate_codrop_args, nworkers);

	safety_dance();
	/* COINSERT and COSELECT inited by safety_dance */
	init_global_service(U_COUPDATE, &coupdate_serv);
	init_global_service(U_COPROVIDE, &coprovide_serv);
	init_global_service(U_CODELETE, &codelete_serv);
	init_global_service(U_COCREATE, &cocreate_serv);
	init_global_service(U_CODROP, &codrop_serv);

	return;
}