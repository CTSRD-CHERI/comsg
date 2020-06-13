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

#include "comesg_kern.h"

#include "ukern_mman.h"
#include "ukern_msg_malloc.h"
#include "ukern_params.h"
#include "ukern_commap.h"
#include "ukern_utils.h"
#include "ukern_requests.h"
#include "ukern_tables.h"
#include "coport_utils.h"
#include "sys_comsg.h"

#include "coport.h"
#include "coproc.h"

#include <cheri/cheric.h>
#include <machine/sysarch.h>

#include <err.h>
#include <errno.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define DEBUG

static void * __capability root_seal_cap;
otype_t seal_cap;
long sealed_otype;

pthread_mutex_t global_copoll_lock;
pthread_cond_t global_cosend_cond;



_Atomic unsigned int next_port_index = 0;

void *copoll_deliver(void *args)
{
	sys_coport_t *cocarrier;
	coport_listener_t *l,*l_temp;
	pthread_mutex_lock(&global_copoll_lock);
	for(;;)
	{
		pthread_cond_wait(&global_cosend_cond,&global_copoll_lock);
		for (int i = 0; i < coport_table.index; ++i)
		{
			cocarrier=&coport_table.table[i].port;

			if(cocarrier->type!=COCARRIER || (LIST_EMPTY(&cocarrier->listeners)))
			{
				cocarrier=NULL;
				continue;
			}
			else
			{
                atomic_thread_fence(memory_order_acquire);
				LIST_FOREACH_SAFE(l,&cocarrier->listeners,entries,l_temp)
				{
					if(event_match(cocarrier,l->eventmask))
					{
						pthread_cond_signal(&l->wakeup);
						l->revent=cocarrier->event;
					}
				}
				atomic_thread_fence(memory_order_release);
			}
		}
	}
	pthread_mutex_unlock(&global_copoll_lock);
    return args;
}


void *cocarrier_poll(void *args)
{
	int i;
    int error;

    int ncoports;
    struct timespec timeout_spec;
    struct timespec curtime_spec;

    worker_args_t * data = args;
    copoll_args_t * copoll_args = calloc(1,sizeof(copoll_args));
    sys_coport_t * cocarrier;
    pollcoport_t * targets;

    void * __capability sw_code;
    void * __capability sw_data;
    void * __capability caller_cookie;
    void * __capability target;

    pthread_cond_t wake;
    pthread_condattr_t c_attr;
    coport_listener_t ** listen_entries;

    pthread_condattr_init(&c_attr);
    pthread_cond_init(&wake,&c_attr);
    
    error=coaccept_init(&sw_code,&sw_data,data->name,&target);
    data->cap=target;
    update_worker_args(data,U_COPOLL);
    for (;;)
    {
    	//This could likely be organized much better.
    	//When the refactor comes, this will need an appointment with Dr Guillotin
        error=coaccept(sw_code,sw_data,&caller_cookie,copoll_args,sizeof(copoll_args_t));
        targets=ukern_malloc(sizeof(pollcoport_t)*copoll_args->ncoports);
        memcpy(targets,copoll_args->coports,sizeof(pollcoport_t)*copoll_args->ncoports);
        ncoports=copoll_args->ncoports;
        for(i = 0; i<ncoports;i++)
        {
            cocarrier=targets[i].coport;
            if(cocarrier==NULL)
            {
            	//there is a cap on how many coports can exist
            	//and in future i expect to impose one on how many can be polled
                break;
            }
            if(!(valid_cocarrier(cocarrier)))
            {
                copoll_args->status=-1;
                copoll_args->error=EINVAL;
                continue;
            }
            targets[i].coport=cheri_unseal(cocarrier,root_seal_cap);
        }

        if(copoll_args->status==-1)
        {
            //we need to clean up
            ukern_fast_free(targets);
            copoll_args->error=EINVAL;
            continue;
        }
        if(copoll_args->timeout!=0)
        {
        	listen_entries=ukern_fast_malloc(CHERICAP_SIZE*copoll_args->ncoports);
        	for(i = 0; i<ncoports;i++)
        	{
        		listen_entries[i]=ukern_fast_malloc(sizeof(coport_listener_t));
	            listen_entries[i]->wakeup=wake;
	            listen_entries[i]->revent=NOEVENT;
	            listen_entries[i]->eventmask=targets[i].events;
        	}
        }
        
        if (copoll_args->timeout==0)
        {
        		atomic_thread_fence(memory_order_acquire);
        		for(i=0;i<ncoports; ++i)
        		{
        			cocarrier=targets[i].coport;
        			copoll_args->coports[i].revents=(copoll_args->coports[i].revents & cocarrier->event);
        		}
        		atomic_thread_fence(memory_order_release);
        		ukern_fast_free(targets);
        		continue;
        }

        pthread_mutex_lock(&global_copoll_lock);
        for (i = 0; i < ncoports; ++i)
        {
        	cocarrier=targets[i].coport;
        	LIST_INSERT_HEAD(&cocarrier->listeners,listen_entries[i],entries);
        }

        if(copoll_args->timeout!=-1)
        {
        	timespec_get(&curtime_spec,TIME_UTC);
	        timeout_spec.tv_nsec=copoll_args->timeout;
	        timespecadd(&curtime_spec,&timeout_spec,&timeout_spec);
	        pthread_cond_timedwait(&wake,&global_copoll_lock,&timeout_spec);
        }
        else
        {
        	pthread_cond_wait(&wake,&global_copoll_lock);
        }
        /*do stuff*/
        for(i = 0; i<ncoports;i++)
        {
        	copoll_args->coports[i].revents=listen_entries[i]->revent;
        	LIST_REMOVE(listen_entries[i],entries);
        }
        pthread_mutex_unlock(&global_copoll_lock);
        ukern_fast_free(targets);
        for(i = 0; i<ncoports;i++)
        {
        	ukern_fast_free(listen_entries[i]);
        }
        ukern_fast_free(listen_entries);
    }
    return 0;
}

void *cocarrier_recv(void *args)
{
	int error;
    size_t index=0;
    size_t new_len=0;
    coport_status_t status;
    //uint len;
    size_t port_len = 0;

    worker_args_t * data = args;
    cocall_cocarrier_send_t * cocarrier_send_args;

    void * __capability sw_code;
    void * __capability sw_data;
    void * __capability caller_cookie;
    void * __capability target;

    sys_coport_t * cocarrier;
    coport_eventmask_t event;
    void ** __capability cocarrier_buf;

    cocarrier_send_args=ukern_malloc(sizeof(cocall_cocarrier_send_t));

    error=coaccept_init(&sw_code,&sw_data,data->name,&target);
    data->cap=target;
    update_worker_args(data,U_COCARRIER_RECV);
    for(;;)
    {
    	error=coaccept(sw_code,sw_data,&caller_cookie,cocarrier_send_args,sizeof(cocall_cocarrier_send_t));
        //perform checks
        cocarrier=cocarrier_send_args->cocarrier;
        if(!(valid_cocarrier(cocarrier)))
        {
            cocarrier_send_args->status=-1;
            cocarrier_send_args->error=EINVAL;
            continue;
        }
        cocarrier=cheri_unseal(cocarrier,root_seal_cap);
        cocarrier_buf=cocarrier->buffer;
        status=COPORT_OPEN;
        while(!atomic_compare_exchange_strong_explicit(&cocarrier->status,&status,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
        {
            status=COPORT_OPEN;
        }
        atomic_thread_fence(memory_order_acquire);
        event=cocarrier->event;
        port_len=cocarrier->length;
        //atomic_store_explicit(&cocarrier->status,COPORT_BUSY,memory_order_release);
        if(port_len==0 || !(event & COPOLL_IN))
        {
            //printf("cocarrier_length=%lu\n",cocarrier->length);
            //printf("cocarrier_events=%x\n",cocarrier->event);
            cocarrier->event=(event|COPOLL_RERR);
            cocarrier->status=COPORT_OPEN;

            //buffer is empty - return error
            cocarrier_send_args->status=-1;
            cocarrier_send_args->error=EAGAIN;
            continue;
        }
        index=cocarrier->start++;
        new_len=++cocarrier->length;
        if (new_len==0)
        {
        	cocarrier->event=((COPOLL_OUT | event) & ~(COPOLL_RERR | COPOLL_IN));
        }
        else
        {
            cocarrier->event=((COPOLL_OUT | event) & ~COPOLL_RERR);
        }
        atomic_thread_fence(memory_order_release);
        cocarrier_send_args->message=cocarrier_buf[index];
        atomic_store_explicit(&cocarrier->status,COPORT_OPEN,memory_order_release);

        if(!LIST_EMPTY(&cocarrier->listeners))
        {
            pthread_cond_signal(&global_cosend_cond);
        }

        cocarrier_send_args->status=cheri_getlen(cocarrier_send_args->message);
        cocarrier_send_args->error=0;
    }
    return args;
}

void *cocarrier_send(void *args)
{
    //todo implement
    int error;
    size_t index = 0;
    size_t new_len = 0;
    size_t port_len = 0;
    coport_status_t status;

    worker_args_t * data = args;
    cocall_cocarrier_send_t * cocarrier_send_args;

    
    void * __capability sw_code;
    void * __capability sw_data;
    void * __capability caller_cookie;
    void * __capability target;

    sys_coport_t * cocarrier;
    void * __capability msg_buf;
    void * __capability call_buf;
    void ** __capability cocarrier_buf;
    coport_eventmask_t event;

    cocarrier_send_args=ukern_malloc(sizeof(cocall_cocarrier_send_t));

    error=coaccept_init(&sw_code,&sw_data,data->name,&target);
    data->cap=target;
    update_worker_args(data,U_COCARRIER_SEND);
    for(;;)
    {
        error=coaccept(sw_code,sw_data,&caller_cookie,cocarrier_send_args,sizeof(cocall_cocarrier_send_t));
        //perform checks
        cocarrier=cocarrier_send_args->cocarrier;
        if(!(valid_cocarrier(cocarrier)))
        {
            cocarrier_send_args->status=-1;
            cocarrier_send_args->error=EINVAL;
            ukern_free(msg_buf);
            printf("cocarrier_send: invalid cocarrier\n");

            continue;
        }
        call_buf=cocarrier_send_args->message;
        //allocate ukernel owned buffer
        msg_buf=get_mem(cheri_getlen(cocarrier_send_args->message));
        //copy data into buffer
        memcpy(msg_buf,call_buf,cheri_getlen(cocarrier_send_args->message));
        
        //reduce cap permissions on buffer
        msg_buf=cheri_andperm(msg_buf,COCARRIER_PERMS);
        //toggle state to busy
        cocarrier=cheri_unseal(cocarrier,root_seal_cap);
        cocarrier_buf=cocarrier->buffer;
        //
        status=COPORT_OPEN;
        while(!atomic_compare_exchange_strong_explicit(&cocarrier->status,&status,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
        {
            status=COPORT_OPEN;
            //pthread_yield();
        }
        atomic_thread_fence(memory_order_acquire);
        //add buffer to cocarrrier
        event=cocarrier->event;
        port_len=cocarrier->length;
        if(port_len>=COCARRIER_SIZE || !(event & COPOLL_OUT))
        {
            cocarrier->event=(event | COPOLL_WERR);
            cocarrier->status=COPORT_OPEN;
            atomic_thread_fence(memory_order_release);
            //warn("cocarrier_send: buffer full\n");
            //buffer is full - return error
            cocarrier_send_args->status=-1;
            cocarrier_send_args->error=EAGAIN;
            continue;
        }
        index=++cocarrier->end;
        new_len=++cocarrier->length;
        if(cheri_gettag(cocarrier_buf[index]))
        {
        	//auto overwrite old messages once we've wrapped around
        	//ukern_free(cocarrier_buf[index]);
            cocarrier_buf[index]=NULL;
            //this check is here so we can free these in future
        }
        cocarrier_buf[index]=msg_buf;

        if(new_len==COCARRIER_SIZE)
        	cocarrier->event=((COPOLL_IN | event) & ~(COPOLL_WERR | COPOLL_OUT));
        else
            cocarrier->event=(COPOLL_IN | event) & ~COPOLL_WERR;
        atomic_thread_fence(memory_order_release);

        atomic_store_explicit(&cocarrier->status,COPORT_OPEN,memory_order_release);

        
        //check if anyone is waiting on messages to arrive
        if(!LIST_EMPTY(&cocarrier->listeners))
        {
            pthread_cond_signal(&global_cosend_cond);
        }
        //release
        cocarrier_send_args->status=cheri_getlen(msg_buf);
        cocarrier_send_args->error=0;
        msg_buf=NULL;
    }
    free(cocarrier_send_args);
    return 0;
}

void *coport_open(void *args)
{
    int error;
    int index;
    int lookup;

    worker_args_t * data = args;
    cocall_coopen_t * coport_args;
    coport_tbl_entry_t table_entry;
    sys_coport_t port,*prt;

    char port_name[COPORT_NAME_LEN];

    void * __capability sw_code;
    void * __capability sw_data;
    void * __capability caller_cookie;
    void * __capability target;

    coport_args=ukern_malloc(sizeof(cocall_coopen_t));
    //port=malloc(sizeof(sys_coport_t));
    memset(&port,0,sizeof(sys_coport_t));
    memset(port_name,'\0',sizeof(port_name));

    error=coaccept_init(&sw_code,&sw_data,data->name,&target);
    data->cap=target;
    update_worker_args(data,U_COOPEN);
    for (;;)
    {
        error=coaccept(sw_code,sw_data,&caller_cookie,coport_args,sizeof(cocall_coopen_t));
        strncpy(port_name,coport_args->args.name,COPORT_NAME_LEN);
        //printf("coopening...\n");
        /* check args are acceptable */
        
        //printf("coport name:%s\n", coport_args->args.name);
        /* check if port exists */
        /* if it doesn't, we need to be sure we don't get into a race to create it */

        lookup=lookup_port(port_name,&prt,coport_args->args.type);
        if(lookup==-1)
        {
            
            /* if it doesn't, set up coport */
            //printf("type read:%u\n",coport_args->args.type);
            //printf("initing port...\n");
            error=init_port(coport_args->args.type,&port);
            if(error!=0)
            {
                err(1,"unable to init_port");
            }
            //printf("inited port.\n");
            init_coport_table_entry(&table_entry,port,port_name);
            table_entry.port=port;
            table_entry.id=generate_id();
           // strcpy(table_entry.name,coport_args->args.name);
            index=add_port(table_entry);
            //printf("coport %s added to table\n",coport_args->args.name);
            prt=cheri_setbounds(&coport_table.table[index].port,sizeof(sys_coport_t));
            if(prt->type==COCARRIER)
                prt=cheri_seal(prt,seal_cap);
            coport_table.table[index].port_cap=prt; //ensure consistency
        }
        coport_args->port=prt;
        //printf("coport_perms: %lu\n",cheri_getperm(prt));

    }
    ukern_free(coport_args);
    return 0;
}

int main(int argc, const char *argv[])
{
    int verbose;
    int error = 0;
    request_handler_args_t * handler_args;

    pthread_t memory_manager, commap_manager, msg_mman;
    pthread_t coopen_threads[WORKER_COUNT];
    //pthread_t counlock_threads[WORKER_COUNT];
    //pthread_t comutex_init_threads[WORKER_COUNT];
    //pthread_t colock_threads[WORKER_COUNT];
    pthread_t cocarrier_send_threads[WORKER_COUNT];
    pthread_t cocarrier_recv_threads[WORKER_COUNT];
    pthread_t copoll_threads[WORKER_COUNT];
    pthread_t copoll_deliver_threads[WORKER_COUNT];
    //pthread_t coclose_threads[WORKER_COUNT];

    pthread_t coopen_handler;
    //pthread_t counlock_handler;
   // pthread_t comutex_init_handler;
   // pthread_t colock_handler;
    pthread_t cocarrier_send_handler;
    pthread_t cocarrier_recv_handler;
    pthread_t copoll_handler;
    //pthread_t coclose_handler;

    pthread_attr_t thread_attrs;
    /*
     * TODO-PBB: Options. 
     * - verbose
     * - address space
     */
    #ifdef DEBUG
    verbose=1;
    #endif


    /* check we're in a colocated address space ?*/
    if (argc>100)
    {
        printf("%s",argv[0]);
    }

    /* we can only have one handler per cocall string */
    /*  */
    /* set up tables */
    printf("Starting comesg microkernel...\n");
    printf("Starting memory manager...\n");

    pthread_attr_init(&thread_attrs);
    /*pthread_attr_setschedpolicy(&thread_attrs,SCHED_RR);
    struct sched_param sched_params;
    sched_params.sched_priority = sched_get_priority_max(SCHED_RR);
    pthread_attr_setschedparam(&thread_attrs,&sched_params);
    pthread_attr_setinheritsched(&thread_attrs,PTHREAD_EXPLICIT_SCHED);*/
    pthread_create(&memory_manager,&thread_attrs,ukern_mman,NULL);
    pthread_create(&msg_mman,&thread_attrs,map_new_mem,NULL);

    error+=sysarch(CHERI_GET_SEALCAP,&root_seal_cap);
    seal_cap=cheri_maketype(root_seal_cap,UKERN_OTYPE);
    sealed_otype=cheri_gettype(cheri_seal(&argc,seal_cap));
    root_seal_cap=cheri_setoffset(root_seal_cap,UKERN_OTYPE);
    memset(&worker_map,0,sizeof(worker_map_entry_t)*U_FUNCTIONS);

    while(jobs_queue.max_len!=(WORKER_COUNT*U_FUNCTIONS)+2)
    {
        //this really shouldn't take long.
        //nevertheless this is a horrible way to wait
        //i apologise
        __asm("nop");
    }
    pthread_mutexattr_t lock_attr;
    pthread_condattr_t cond_attr;

    pthread_mutexattr_init(&lock_attr);
    pthread_mutexattr_setpshared(&lock_attr,PTHREAD_PROCESS_PRIVATE);
    pthread_mutex_init(&global_copoll_lock,&lock_attr);
    pthread_condattr_init(&cond_attr);
    pthread_cond_init(&global_cosend_cond,&cond_attr);

    error+=coport_tbl_setup();
    //error+=comutex_tbl_setup();
    if(error!=0)
    {
        err(1,"Initial setup failed!!");
    }
    printf("Initial setup complete.\n");

    /* perform setup */
    //printf("Press enter to proceed\n");
    //while( getchar() != '\n');
    pthread_create(&commap_manager,&thread_attrs,ukern_mmap,NULL);
    printf("Spawning co-open listeners...\n");
    error+=spawn_workers(&coport_open,coopen_threads,U_COOPEN);
    /*
    printf("Spawning comutex_init listeners...\n");
    error+=spawn_workers(&comutex_setup,comutex_init_threads,U_COMUTEX_INIT);
    printf("Spawning colock listeners...\n");
    error+=spawn_workers(&comutex_lock,colock_threads,U_COLOCK);
    printf("Spawning counlock listeners...\n");
    error+=spawn_workers(&comutex_unlock,counlock_threads,U_COUNLOCK);
    */
    printf("Spawning cocarrier send listeners...\n");
    error+=spawn_workers(&cocarrier_send,cocarrier_send_threads,U_COCARRIER_SEND);
    printf("Spawning cocarrier recv listeners...\n");
    error+=spawn_workers(&cocarrier_recv,cocarrier_recv_threads,U_COCARRIER_RECV);
    printf("Spawning copoll listeners...\n");
    error+=spawn_workers(&cocarrier_poll,copoll_threads,U_COPOLL);
    error+=spawn_workers(&copoll_deliver,copoll_deliver_threads,"_copoll_deliver");
    // XXX-PBB: Not implemented yet
    /*
    printf("Spawning co-close listeners...\n");
    error=spawn_workers(&coport_open,coclose_threads,U_COCLOSE);
    */
    printf("Worker threads spawned.\n");

    /* listen for coopen requests */
    printf("Spawning request handlers...\n");
    handler_args=ukern_malloc(sizeof(request_handler_args_t));
    strcpy(handler_args->func_name,U_COOPEN);
    pthread_attr_init(&thread_attrs);
    pthread_create(&coopen_handler,&thread_attrs,manage_requests,handler_args);
    /*
    handler_args=ukern_malloc(sizeof(request_handler_args_t));
    strcpy(handler_args->func_name,U_COUNLOCK);
    pthread_attr_init(&thread_attrs);
    pthread_create(&counlock_handler,&thread_attrs,manage_requests,handler_args);
    
    handler_args=ukern_malloc(sizeof(request_handler_args_t));
    strcpy(handler_args->func_name,U_COMUTEX_INIT);
    pthread_attr_init(&thread_attrs);
    pthread_create(&comutex_init_handler,&thread_attrs,manage_requests,handler_args);
    
    handler_args=ukern_malloc(sizeof(request_handler_args_t));
    strcpy(handler_args->func_name,U_COLOCK);
    pthread_attr_init(&thread_attrs);
    pthread_create(&colock_handler,&thread_attrs,manage_requests,handler_args);
    */
    handler_args=ukern_malloc(sizeof(request_handler_args_t));
    strcpy(handler_args->func_name,U_COCARRIER_SEND);
    pthread_attr_init(&thread_attrs);
    pthread_create(&cocarrier_send_handler,&thread_attrs,manage_requests,handler_args);

    handler_args=ukern_malloc(sizeof(request_handler_args_t));
    strcpy(handler_args->func_name,U_COCARRIER_RECV);
    pthread_attr_init(&thread_attrs);
    pthread_create(&cocarrier_recv_handler,&thread_attrs,manage_requests,handler_args);

    handler_args=ukern_malloc(sizeof(request_handler_args_t));
    strcpy(handler_args->func_name,U_COPOLL);
    pthread_attr_init(&thread_attrs);
    pthread_create(&copoll_handler,&thread_attrs,manage_requests,handler_args);

    /*
    handler_args=malloc(sizeof(request_handler_args_t));
    strcpy(handler_args->func_name,COCLOSE);
    pthread_attr_init(&thread_attrs);
    pthread_create(&coclose_handler,&thread_attrs,manage_requests,handler_args);
    */
    printf("Done. Listening for calls.\n");
    pthread_join(coopen_threads[0],NULL);
    pthread_join(coopen_handler,NULL);

    return 0;
}