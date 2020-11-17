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
#ifndef _WORKER_MAP_H
#define _WORKER_MAP_H

#include <coproc/namespace_object.h>
#include <cocall/worker.h>

#include <cheri/cherireg.h>

#define FUNC_MAP_PERMS ( CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP )

typedef struct _worker_map_entry
{
	nsobject_t *func_name;
	_Atomic int nworkers;
	worker_args_t *workers;
} function_map_t;

#ifdef COPROC_UKERN

typedef struct coservice_prov {
	coservice_t *service;
	function_map_t *function_map;
	nsobject_t *nsobj;
} coservice_provision_t;

#endif 

function_map_t *spawn_slow_worker(const char *worker_name, void *func, void *valid);
void spawn_slow_worker_thread(worker_args_t *worker, function_map_t *func_map);
function_map_t *new_function_map(void);
function_map_t *spawn_worker(const char *worker_name, void *func, void *valid);
function_map_t *spawn_workers(void *func, void *arg_func, int nworkers);
function_map_t *spawn_slow_workers(void *func, void *arg_func, int nworkers);
void spawn_worker_thread(worker_args_t *worker, function_map_t *func_map);
void **get_worker_scbs(function_map_t *func);

#endif //!defined(_WORKER_MAP_H)