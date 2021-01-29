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
#include <cocall/worker_map.h>
#include <comsg/ukern_calls.h>
#include <coproc/namespace_object.h>

#include <err.h>
#include <unistd.h>

#define COEVENTD_WORKERS (12)

static void
init_service(coservice_provision_t *serv, void *func, void *valid, const char *name)
{

	serv->function_map = spawn_workers(func, valid, COEVENTD_WORKERS);
	serv->service = coprovide(get_worker_scbs(serv->function_map), COEVENTD_WORKERS);
	serv->nsobj = coinsert(name, COSERVICE, serv->service, global_ns);
	if (serv->nsobj == NULL)
		err(errno, "init_service: error inserting %s into global namespace", name);
}

static void
init_slow_service(coservice_provision_t *serv, void *func, void *valid, const char *name)
{
	serv->function_map = spawn_slow_workers(func, valid, COEVENTD_WORKERS);
	serv->service = coprovide(get_worker_scbs(serv->function_map), COEVENTD_WORKERS);
	serv->nsobj = coinsert(name, COSERVICE, serv->service, global_ns);
	if (serv->nsobj == NULL)
		err(errno, "init_service: error inserting %s into global namespace", name);
}

void
init_callback_tables(void)
{
	setup_procdeath_table();
}

void
coeventd_startup(void)
{
	init_callback_tables();

	global_ns = coproc_init(NULL, NULL, NULL, NULL);
	if (global_ns == NULL)
		err(errno, "coeventd_startup: cocall failed");

	do {
		coprovide_nsobj = coselect(U_COPROVIDE, COSERVICE, global_ns);
		if(cheri_gettag(coprovide_nsobj) == 0)
			continue;
		else if (cheri_gettag(coprovide_nsobj->obj) == 0)
			continue;
	} while (0);

	codiscover(coprovide_nsobj, &coprovide_scb);
	set_ukern_target(COCALL_COPROVIDE, coprovide_scb);

	init_service(); /* register monitoring for process */
}