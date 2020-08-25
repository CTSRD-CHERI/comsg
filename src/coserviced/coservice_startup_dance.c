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

#include "ukern/coservice.h"
#include "ukern/namespace.h"
#include "ukern/ukern_calls.h"

#include <err.h>

static namespace_t *global;


static 
coservice_t *init_service(void *func, void *valid, const char *name)
{
	worker_args_t args;
	coservice_t *service = allocate_coservice();
	function_map_t *service_map = spawn_workers(func, valid, nworkers-1);
	
	service->worker_scbs = get_worker_scbs(service_map);
	return (service);
}

static
void coproc_init_coserviced(void *switchercb)
{
	int error;
	coproc_init_args_t *cocall_args = calloc(1, sizeof(coproc_init_args_t));

	cocall_args->codiscover = switchercb;
	error = ukern_cocall(COCALL_COPROC_INIT, cocall_args, sizeof(coproc_init_args_t));
	if(error)
		err(error, "coproc_init_coserviced: cocall failed")
	else if (cocall_args->status == -1) 
		err(cocall_args->error, "coproc_init_coserviced: error in cocall");
	global = cocall_args->namespace;
	set_cocall_target(COCALL_COINSERT, cocall_args->coinsert);
	return;
}

void safety_dance(void)
{
	//init own services
	codiscover = init_service(discover_coservice, validate_codiscover_args, CODISCOVER);
	coprovide = init_service(provide_coservice, valid_coprovide_args, COPROVIDE);
	
	codiscover_scb = get_coservice_scb(codiscover);
	//connect to process daemon and do the startup dance (we can dance if we want to)
	coproc_init_coserviced(codiscover_scb);

	codiscover_obj = coinsert(CODISCOVER, COSERVICE, codiscover, global_ns);
}