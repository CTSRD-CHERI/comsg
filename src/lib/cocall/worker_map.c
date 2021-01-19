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
#include <cocall/worker.h>
#include <coproc/namespace.h>
#include <coproc/utils.h>

#include <cheri/cheric.h>
#include <err.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>


static bool
spawn_worker_threads(void *func, void* arg_func, int nworkers, function_map_t * func_worker_map)
{
    worker_args_t *worker_arr;
    worker_args_t *thread_args;

    func_worker_map->nworkers += nworkers;
    if(func_worker_map->workers == NULL) 
        worker_arr = calloc(nworkers, sizeof(worker_args_t));
    else
        worker_arr = realloc(func_worker_map->workers, (func_worker_map->nworkers * sizeof(worker_args_t)));
    func_worker_map->workers = cheri_andperm(worker_arr, FUNC_MAP_PERMS);
    
    for(int i = 0; i < nworkers; i++)
    {
        thread_args = cheri_setbounds(&worker_arr[i], sizeof(worker_args_t));
        memset(thread_args->name, '\0', NS_NAME_LEN);
        thread_args->validation_function = arg_func;
        thread_args->worker_function = func;
        if (!start_coaccept_worker(thread_args))
            return (false);
    }
    return (true);
}

static bool
spawn_slow_worker_threads(void *func, void* arg_func, int nworkers, function_map_t * func_worker_map)
{
    worker_args_t *worker_arr;
    worker_args_t *thread_args;

    func_worker_map->nworkers += nworkers;
    if(func_worker_map->workers == NULL) 
        worker_arr = calloc(nworkers, sizeof(worker_args_t));
    else
        worker_arr = realloc(func_worker_map->workers, (func_worker_map->nworkers * sizeof(worker_args_t)));
    func_worker_map->workers = cheri_andperm(worker_arr, FUNC_MAP_PERMS);
    
    for(int i = 0; i < nworkers; i++) {
        thread_args = cheri_setbounds(&worker_arr[i], sizeof(worker_args_t));
        rand_string(thread_args->name, NS_NAME_LEN);
        thread_args->validation_function = arg_func;
        thread_args->worker_function = func;
        if (!start_sloaccept_worker(thread_args))
            return (false);
    }
    return (true);
}

static void
teardown_function_map(function_map_t *map)
{
   free(map->workers);
   free(map);
}

static function_map_t *
new_function_map(void)
{
    function_map_t *map = malloc(sizeof(function_map_t));
    memset(map, '\0', sizeof(function_map_t));
    return (map);
}

/* TODO-PBB: do something nicer than two almost identical functions */
bool
spawn_slow_worker_thread(worker_args_t *worker, function_map_t *func_map)
{
    int error, idx;
    worker_args_t *worker_arr;
    worker_args_t *thread_args;

    idx = atomic_fetch_add(&func_map->nworkers, 1);
    if(func_map->workers == NULL)
        worker_arr = calloc(func_map->nworkers, sizeof(worker_args_t));
    else
        worker_arr = realloc(func_map->workers, (func_map->nworkers * sizeof(worker_args_t)));
    
    func_map->workers = cheri_andperm(worker_arr, FUNC_MAP_PERMS);
    thread_args = cheri_setbounds(&worker_arr[idx], sizeof(worker_args_t));
    
    thread_args->worker_function = worker->worker_function;
    thread_args->validation_function = worker->validation_function;
    strncpy(thread_args->name, worker->name, NS_NAME_LEN);
    
    return (start_sloaccept_worker(thread_args));
}

bool
spawn_worker_thread(worker_args_t *worker, function_map_t *func_map)
{
    int error, idx;
    worker_args_t *worker_arr;
    worker_args_t *thread_args;

    idx = atomic_fetch_add(&func_map->nworkers, 1);
    if(func_map->workers == NULL)
        worker_arr = calloc(func_map->nworkers, sizeof(worker_args_t));
    else
        worker_arr = realloc(func_map->workers, (func_map->nworkers * sizeof(worker_args_t)));
   
    func_map->workers = cheri_andperm(worker_arr, FUNC_MAP_PERMS);
    thread_args = cheri_setbounds(&worker_arr[idx], sizeof(worker_args_t));
    
    thread_args->worker_function = worker->worker_function;
    thread_args->validation_function = worker->validation_function;
    strncpy(thread_args->name, worker->name, NS_NAME_LEN);
  
    return (start_coaccept_worker(thread_args));
}

function_map_t *
spawn_slow_worker(const char *worker_name, void *func, void *valid)
{
    worker_args_t wargs;
    function_map_t *map;

    map = new_function_map();
    memset(&wargs, '\0', sizeof(wargs));
    if(worker_name != NULL)
        strncpy(wargs.name, worker_name, NS_NAME_LEN);
    wargs.worker_function = func;
    wargs.validation_function = valid; 
    
    if (!spawn_slow_worker_thread(&wargs, map)) {
        teardown_function_map(map);
        return (NULL);
    }

    return (map);
}

function_map_t *
spawn_worker(const char *worker_name, void *func, void *valid)
{
    worker_args_t wargs;
    function_map_t *map;

    map = new_function_map();
    memset(&wargs, '\0', sizeof(wargs));
    if(worker_name != NULL)
        strncpy(wargs.name, worker_name, NS_NAME_LEN);

    wargs.worker_function = func;
    wargs.validation_function = valid; 
    
    if (!spawn_worker_thread(&wargs, map)) {
        teardown_function_map(map);
        return (NULL);
    }

    return (map);
}

function_map_t *
spawn_workers(void *func, void *arg_func, int nworkers)
{
    function_map_t *func_map;

    func_map = new_function_map();
    if (!spawn_worker_threads(func, arg_func, nworkers, func_map)) {
        teardown_function_map(func_map);
        return (NULL);
    }

    return (func_map);
}

function_map_t *
spawn_slow_workers(void *func, void *arg_func, int nworkers)
{
    function_map_t *func_map;

    func_map = new_function_map();
   if (!spawn_slow_worker_threads(func, arg_func, nworkers, func_map)) {
        teardown_function_map(func_map);
        return (NULL);
    }

    return (func_map);
}

void **
get_worker_scbs(function_map_t *func)
{
    void ** scbs;
    int nworkers, i;

    nworkers = func->nworkers;
    scbs = calloc(nworkers, CHERICAP_SIZE);
    for(i = 0; i < nworkers; i++) 
        scbs[i] = func->workers[i].scb_cap;
    
    return (scbs);
}
