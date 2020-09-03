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

#include "ukern/cocall_args.h"
#include "ukern/cocalls.h"
#include "ukern/coservice.h"
#include "ukern/namespace.h"
#include "ukern/namespace_object.h"
#include "ukern/ukern_calls.h"
#include "ukern/worker.h"
#include "ukern/worker_map.h"

#include <sys/queue.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * There is a race condition inherent in use of pids to identify processes.
 * Handling this is a question of correctness. 
 */

static
void safety_dance(void)
{
	//init own services
	void **coinsert_scbs, *coprovide_scb;
	coservice_t *coprovide_service;
	nsobject_t *coinsert_obj;
	namespace_t *global_ns = get_global_namespace();
	
	coinsert_scbs = get_worker_scbs(coinsert_serv.function_map);
	
	//connect to process daemon and do the startup dance (we can dance if we want to)
	if (coproc_init(global_ns, coinsert_scbs[0], NULL) == NULL)
		err(error, "coproc_init: cocall failed");

	/* Once coprovide has been inserted into the global namespace, we can make progress*/
	while(lookup_coservice(COPROVIDE, global_ns) == NULL)
		sched_yield();

	coprovide_service = codiscover(COPROVIDE, global_ns, &coprovide_scb);
	set_cocall_target(ukern_call_set, COPROVIDE, coprovide_scb);
	
	coinsert_serv.service = coprovide(COINSERT, coinsert_scbs, nworkers, NULL, global_ns);
}

static
void init_service(const char * name, coservice_provision_t *service_prov)
{
	void **scbs;
	scbs = get_worker_scbs(service_prov->function_map);
	service_prov->service = coprovide(name, scbs, nworkers, NULL, global_ns);
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
	init_service(COINSERT, &coinsert_serv);
	init_service(COUPDATE, &coupdate_serv);
	init_service(COPROVIDE, &coprovide_serv);
	init_service(CODELETE, &codelete_serv);
	init_service(COCREATE, &cocreate_serv);
	init_service(CODROP, &codrop_serv);

	return;
}