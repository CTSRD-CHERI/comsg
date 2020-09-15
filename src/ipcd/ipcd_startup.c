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
#include "coport_table.h"
#include "ipcd.h"
#include "ipcd_startup.h"

#include <coproc/namespace.h>
#include <cocall/worker_map.h>
#include <cocall/cocall_args.h>

#include <err.h>

static namespace_t *global;

static 
void init_service(coservice_provision_t *serv, void *func, void *valid, const char *name)
{
	function_map_t *service_map = spawn_workers(func, valid, nworkers);

	serv->function_map = service_map;
	serv->service = coprovide(get_worker_scbs(service_map), nworkers);
	serv->nsobj = coinsert(name, COSERVICE, serv->service, global);
	if (serv->nsobj == NULL)
		err(errno, "init_service: error inserting %s into global namespace", name);
}

void ipcd_startup(void)
{
	global = coproc_init(NULL, NULL, NULL, NULL);
	if (global == NULL)
		err(error, "coproc_init: cocall failed");
	//init own services
	init_service(&coopen_serv, open_coport, validate_coopen_args, U_COOPEN);
	init_service(&coclose_serv, close_coport, validate_coclose_args, U_COCLOSE);
	init_service(&cosend_serv, cocarrier_send, validate_cosend_args, U_COSEND);
	init_service(&corecv_serv, cocarrier_recv, validate_corecv_args, U_CORECV);
	init_service(&copoll_serv, cocarrier_poll, validate_copoll_args, U_COPOLL);

}