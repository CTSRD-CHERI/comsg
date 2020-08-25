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
#include "ukern/worker.h"

#define INIT_MAP_LEN 2

struct 
{
    pthread_mutex_t lock;
    function_map_t *function_map;
    _Atomic int nfunctions;
} worker_map;

__attribute__ ((constructor)) static 
void setup_worker_map(void)
{
    pthread_mutexattr_t lock_attr;

    pthread_mutexattr_init(&lock_attr);
    pthread_mutex_init(&worker_map.lock, &lock_attr);
}

static
void spawn_worker_threads(void *func, void* arg_func, int nworkers, function_map_t * func_worker_map)
{
    pthread_attr_t thread_attrs;
    worker_args_t * worker_arr;
    void * thread_args;

    pthread_attr_init(&thread_attrs);

    func_worker_map->nworkers += nworkers;
    if(func_worker_map->workers == NULL) 
        worker_arr = calloc(nworkers, sizeof(worker_args_t));
    else
        worker_arr = realloc(func_worker_map->workers, (func_worker_map->nworkers * sizeof(worker_args_t)));
    func_worker_map->workers = cheri_andperms(worker_arr, FUNC_MAP_PERMS);
    
    for(int i = 0; i < nworkers; i++)
    {
        thread_args = cheri_setbounds(&worker_arr[i], sizeof(worker_args_t));
#if 0
        rand_string(&thread_args.name, THREAD_STRING_LEN);
#else
        strnset(thread_args.name[0], '\0', THREAD_STRING_LEN);
#endif
        thread_args.worker_function = func;
        
        if(pthread_create(&thread_args.worker, &thread_attrs, coaccept_worker, thread_args))
            err(errno,"spawn_workers: could not spawn thread %d with name %s", i, worker_arr[i].name);
    }
}

static
function_map_t *lookup_function(const char * name)
{
    int nfunctions = atomic_load(&worker_map.nfunctions);
    for(int i = 0; i < nfunctions; i++)
    {
        if(strcmp(name, worker_map.function_map[i].func_name)==0)
            return (&worker_map.function_map[i]);
    }
    return (NULL);
}

function_map_t *new_function_map(void)
{
    void *map_ptr;

    int idx = atomic_fetch_add(&worker_map.nfunctions, 1);
    int nfunctions = idx + 1;
    size_t required_len = (nfunctions * sizeof(function_map_t));

    if (idx == 0)
        map_ptr = calloc(INIT_MAP_LEN, sizeof(function_map_t));
    else if(cheri_getlen(worker_map.function_map) < required_len)
        map_ptr = realloc(worker_map.function_map, required_len);
    else 
        map_ptr = worker_map.function_map;

    map_ptr[idx].workers = NULL; 
    map_ptr[idx].nworkers = 0;

    return (&map_ptr[idx]);
}

void
spawn_worker_thread(worker_args_t *worker, function_map_t *func_map)
{
    pthread_attr_t thread_attrs;
    worker_args_t *worker_arr;
    void *thread_args;

    pthread_attr_init(&thread_attrs);

    int idx = atomic_fetch_add(&func_map->nworkers, 1);
    if(func_map->workers == NULL) 
        worker_arr = calloc(nworkers, sizeof(worker_args_t));
    else
        worker_arr = realloc(func_map->workers, (func_map->nworkers * sizeof(worker_args_t)));
    worker_arr[idx] = *worker;
    func_map->workers = cheri_andperms(worker_arr, FUNC_MAP_PERMS);

    thread_args = cheri_setbounds(&worker_arr[idx], sizeof(worker_args_t));
    if(pthread_create(&thread_args.worker, &thread_attrs, coaccept_worker, thread_args))
        err(errno,"spawn_worker_thread: could not spawn thread %d with name %s", i, worker_arr[i].name);

    return;
}

function_map_t *spawn_worker(const char *worker_name, void *func, void *valid)
{
    worker_args_t wargs;
    function_map_t *map = new_function_map();
    
    strncpy(wargs.name, worker_name, LOOKUP_STRING_LEN);
    wargs.worker_function = func;
    wargs.validation_function = valid; 
    
    spawn_worker_thread(&wargs, map);
    return (map);
}

function_map_t *spawn_workers(void *func, void *arg_func, int nworkers)
{
    function_map_t *func_map;
   
    pthread_mutex_lock(&worker_map.lock);
    
    func_map = lookup_function(name);
    if(func_map == NULL)
        func_map = new_function_map();
 
    func_map = cheri_setboundsexact(func_map, sizeof(function_map_t));
    
    spawn_worker_threads(func, arg_func, nworkers, func_map);

    pthread_mutex_unlock(&worker_map.lock);

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
