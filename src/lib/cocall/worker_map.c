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

#include <cheri/cheric.h>
#include <err.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>

static
void spawn_worker_threads(void *func, void* arg_func, int nworkers, function_map_t * func_worker_map)
{
    pthread_attr_t thread_attrs;
    worker_args_t *worker_arr;
    worker_args_t *thread_args;

    pthread_attr_init(&thread_attrs);

    func_worker_map->nworkers += nworkers;
    if(func_worker_map->workers == NULL) 
        worker_arr = calloc(nworkers, sizeof(worker_args_t));
    else
        worker_arr = realloc(func_worker_map->workers, (func_worker_map->nworkers * sizeof(worker_args_t)));
    func_worker_map->workers = cheri_andperm(worker_arr, FUNC_MAP_PERMS);
    
    for(int i = 0; i < nworkers; i++)
    {
        thread_args = cheri_setbounds(&worker_arr[i], sizeof(worker_args_t));
#if 0
        rand_string(&thread_args.name, NS_NAME_LEN);
#else
        memset(thread_args->name, '\0', NS_NAME_LEN);
#endif
        thread_args->worker_function = func;
        start_coaccept_worker(thread_args);
        
    }
}

function_map_t *new_function_map(void)
{
    function_map_t *map = malloc(sizeof(function_map_t));
    memset(map, '\0', sizeof(function_map_t));
    return (map);
}

/* TODO-PBB: do something nicer than two almost identical functions */
void
spawn_slow_worker_thread(worker_args_t *worker, function_map_t *func_map)
{
    int error;
    pthread_attr_t thread_attrs;
    worker_args_t *worker_arr;
    worker_args_t *thread_args;

    pthread_attr_init(&thread_attrs);

    int idx = atomic_fetch_add(&func_map->nworkers, 1);
    if(func_map->workers == NULL)
        worker_arr = calloc(func_map->nworkers, sizeof(worker_args_t));
    else
        worker_arr = realloc(func_map->workers, (func_map->nworkers * sizeof(worker_args_t)));
    worker_arr[idx] = *worker;
    func_map->workers = cheri_andperm(worker_arr, FUNC_MAP_PERMS);

    thread_args = cheri_setbounds(&worker_arr[idx], sizeof(worker_args_t));
    start_sloaccept_worker(thread_args);
    
    return;
}

void
spawn_worker_thread(worker_args_t *worker, function_map_t *func_map)
{
    int error;
    pthread_attr_t thread_attrs;
    worker_args_t *worker_arr;
    worker_args_t *thread_args;

    pthread_attr_init(&thread_attrs);

    int idx = atomic_fetch_add(&func_map->nworkers, 1);
    if(func_map->workers == NULL)
        worker_arr = calloc(func_map->nworkers, sizeof(worker_args_t));
    else
        worker_arr = realloc(func_map->workers, (func_map->nworkers * sizeof(worker_args_t)));
    worker_arr[idx] = *worker;
    func_map->workers = cheri_andperm(worker_arr, FUNC_MAP_PERMS);

    thread_args = cheri_setbounds(&worker_arr[idx], sizeof(worker_args_t));
    start_coaccept_worker(thread_args);
    
    return;
}

function_map_t *spawn_slow_worker(const char *worker_name, void *func, void *valid)
{
    worker_args_t wargs;
    function_map_t *map = new_function_map();
    
    strncpy(wargs.name, worker_name, NS_NAME_LEN);
    wargs.worker_function = func;
    wargs.validation_function = valid; 
    
    spawn_slow_worker_thread(&wargs, map);
    return (map);
}

function_map_t *spawn_worker(const char *worker_name, void *func, void *valid)
{
    worker_args_t wargs;
    function_map_t *map = new_function_map();
    
    strncpy(wargs.name, worker_name, NS_NAME_LEN);
    wargs.worker_function = func;
    wargs.validation_function = valid; 
    
    spawn_worker_thread(&wargs, map);
    return (map);
}

function_map_t *spawn_workers(void *func, void *arg_func, int nworkers)
{
    function_map_t *func_map = new_function_map();
   
    spawn_worker_threads(func, arg_func, nworkers, func_map);

    return (func_map);
}

void **get_worker_scbs(function_map_t *func)
{
    int nworkers = func->nworkers;
    void **scbs = calloc(nworkers, CHERICAP_SIZE);
    for(int i = 0; i < nworkers; i++)
        scbs[i] = func->workers[i].scb_cap;

    return (scbs);
}
