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

#include "ukern_mman.h"
#include "ukern_requests.h"
#include "coproc.h"
#include "ukern_utils.h"

#include <err.h>
#include <errno.h>

#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

static pthread_mutex_t worker_map_lock;
static pthread_once_t once = PTHREAD_ONCE_INIT;

_Atomic int next_worker_i = 0;
_Atomic int next_priv_worker_i = 0;

worker_map_entry_t worker_map[U_FUNCTIONS];
worker_map_entry_t private_worker_map[UKERN_PRIV];

int coaccept_init(
    void * __capability * __capability  code_cap,
    void * __capability * __capability  data_cap, 
    const char * target_name,
    void * __capability * __capability target_cap)
{
    int error;
    error=cosetup(COSETUP_COACCEPT,code_cap,data_cap);
    if (error!=0)
    {
        err(errno,"ERROR: Could not cosetup.\n");
    }

    error=coregister(target_name,target_cap);
    if (error!=0)
    {
        err(errno,"ERROR: Could not coregister with name %s.\n",target_name);
    }
    return (error);
}


void update_worker_args(worker_args_t * args, const char * function_name)
{
    int i,j;
    char lookup_string[LOOKUP_STRING_LEN];
    strncpy(lookup_string,args->name,LOOKUP_STRING_LEN);
    for(i = 0; i<=U_FUNCTIONS; i++)
    {
        if (strcmp(worker_map[i].func_name,function_name)==0)
        {
            break;
        }
        else if (i==U_FUNCTIONS)
        {
            err(1,"function name %s not found in map",function_name);
        }
    }
    for(j = 0; j<=WORKER_COUNT;j++)
    {
        if (strncmp(worker_map[i].workers[j].name,args->name,LOOKUP_STRING_LEN)==0)
        {
            worker_map[i].workers[j].cap=args->cap;
            return;
        }
        else if (j==WORKER_COUNT)
        {
            err(1,"worker %s not found in map",args->name);
        }
    }
}

void *manage_requests(void *args)
{
    int error;
    int workers_index;

    request_handler_args_t * data = args;

    void * __capability sw_code;
    void * __capability sw_data;
    void * __capability cookie = 0;
    void * __capability target;

    worker_args_t * workers;

    cocall_lookup_t * lookup;

    error=coaccept_init(&sw_code,&sw_data,data->func_name,&target);
    workers_index=-1;
    for(int i = 0; i < U_FUNCTIONS; i++)
    {
        if(strcmp(worker_map[i].func_name,data->func_name)==0)
        {
            workers=worker_map[i].workers;
            workers_index=i;
            break;
        }
    }
    if(workers_index==-1)
    {
        err(1,"Function workers not registered");
    }
    lookup=ukern_malloc(sizeof(cocall_lookup_t));
    for(;;)
    {
        for(int j = 0; j < WORKER_COUNT; j++)
        {
            //printf("coaccepting for %s\n",data->func_name);
            error=coaccept(sw_code,sw_data,&cookie,lookup,sizeof(cocall_lookup_t));
            //printf("Lookup of %s is size %lu",workers[j].name,sizeof(cocall_lookup_t));
            lookup->cap=workers[j].cap;
        }
    }
    ukern_free(lookup);
}

static void init_worker_map_lock(void)
{
    pthread_mutex_init(&worker_map_lock,NULL);
    return;
}

int spawn_workers(void * func, pthread_t * threads, const char * name)
{
    pthread_once(&once,init_worker_map_lock);
    pthread_attr_t thread_attrs;
    worker_args_t * args;
    int e;
    int w_i;
    char thread_name[LOOKUP_STRING_LEN];
    bool private;

    if (name[0]=='_')
    	private=true;
    else
    	private=false;
    /* split into threads */
    pthread_mutex_lock(&worker_map_lock);
    if (!private)
    {
        w_i=atomic_fetch_add(&next_worker_i,1);
    	strcpy(worker_map[w_i].func_name,name);
    }
    else
    {
    	w_i=atomic_fetch_add(&next_priv_worker_i,1);
    	strcpy(private_worker_map[w_i].func_name,name);
    }
    //  printf("workers for %s\n",name);
    pthread_attr_init(&thread_attrs);
    for (int i = 0; i < WORKER_COUNT; i++)
    {
        args=ukern_malloc(sizeof(worker_args_t));
        rand_string(&thread_name[0],THREAD_STRING_LEN);
        strcpy(args->name,thread_name);
        args->cap=NULL;
        //printf("%s",thread_name);
        
        //printf("thr_name %s\n",args->name);
        e=pthread_create(&threads[i],&thread_attrs,func,args);
        if (e==0)
        {
        	if (!private)
            {
            	memcpy(&worker_map[w_i].workers[i],args,sizeof(worker_args_t));
            }
            else
            {
            	memcpy(&private_worker_map[w_i].workers[i],args,sizeof(worker_args_t));
            }
        }
    }
    pthread_mutex_unlock(&worker_map_lock);
    return 0;
}