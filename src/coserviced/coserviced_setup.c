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
#include "coservice_table.h"

#include <coproc/coservice.h>
#include <coproc/namespace.h>
#include <cocall/worker_map.h>
#include <cocall/cocall_args.h>
#include <comsg/ukern_calls.h>
#include <err.h>

static namespace_t *global;

static 
void init_service(coservice_provision_t *serv, void *func, void *valid)
{
	coservice_t *service = allocate_coservice();
	function_map_t *service_map = spawn_workers(func, valid, nworkers);
	
	service->worker_scbs = get_worker_scbs(service_map);
	
	serv->service = service;
	serv->function_map = service_map;
	return;
}

void coserviced_startup(void)
{
	coservice_t *coservice_cap;
	//init own services
	init_service(&codiscover_serv, discover_coservice, validate_codiscover_args);
	init_service(&coprovide_serv, provide_coservice, valid_coprovide_args);
	
	codiscover_scb = get_coservice_scb(codiscover_serv.service);
	//connect to process daemon and do the startup dance (we can dance if we want to)
	global = coproc_init(NULL, NULL, NULL, codiscover_scb);
	if (global == NULL)
		err(error, "coproc_init: cocall failed");

	codiscover_serv.nsobj = coinsert(U_CODISCOVER, COSERVICE, create_coservice_handle(codiscover_serv.service), global);
	if (codiscover_serv.nsobj == NULL)
		err(errno, "coserviced_startup: error coinserting codiscover into global namespace");
	coprovide_serv.nsobj = coinsert(U_COPROVIDE, COSERVICE, create_coservice_handle(coprovide_serv.service), global);
	if (coprovide_serv.nsobj == NULL)
		err(errno, "coserviced_startup: error coinserting coprovide into global namespace");
}