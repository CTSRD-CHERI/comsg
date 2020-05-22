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

#include <math.h>
#include <err.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdlib.h>
#include <cheri/cheric.h>
#include <time.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <stdbool.h>


#include <machine/sysarch.h>

#include "coport.h"
#include "coport_utils.h"
#include "coproc.h"
#include "sys_comsg.h"
#include "ukern_mman.h"
#include "ukern_params.h"
#include "ukern_commap.h"


#define DEBUG

static void * __capability root_seal_cap;
static otype_t seal_cap;
static long sealed_otype;

pthread_mutex_t global_copoll_lock;
pthread_cond_t global_cosend_cond;

worker_map_entry_t worker_map[U_FUNCTIONS];
worker_map_entry_t private_worker_map[UKERN_PRIV];

_Atomic int next_worker_i = 0;
_Atomic int next_priv_worker_i = 0;

_Atomic unsigned int next_port_index = 0;
comutex_tbl_t comutex_table;
coport_tbl_t coport_table;

const int COPORT_TBL_LEN = (MAX_COPORTS*sizeof(coport_tbl_entry_t));
const int COMTX_TBL_LEN = (MAX_COMUTEXES*sizeof(comutex_tbl_entry_t));

int generate_id(void)
{
    // TODO: Replace this with something smarter.
    return random();
}

int rand_string(char * buf, long int len)
{
    char c;
    char * s;
    int rand_no;
    long int i;
    s = (char *) malloc(sizeof(char)*len);
    srandomdev();
    for (i = 0; i < len-1; i++)
    {
        rand_no=random() % KEYSPACE;
        c=(char)rand_no+0x21;
        while(c=='"')
        {
            rand_no=random() % KEYSPACE;
            c=(char)rand_no+0x21;
        }
        s[i]=c;
    }
    s[len-1]='\0';
    strcpy(buf,s);
    free(s);
    return i;
}

int add_port(coport_tbl_entry_t entry)
{
    int entry_index;
    
    if(coport_table.index==MAX_COPORTS)
    {
        return 1;
    }
    entry_index=atomic_fetch_add(&coport_table.index,1);
    memcpy(&coport_table.table[entry_index],&entry,sizeof(coport_tbl_entry_t));    
    return entry_index;
}

int add_mutex(comutex_tbl_entry_t entry)
{
    int entry_index;
    
    pthread_mutex_lock(&comutex_table.lock);
    if(comutex_table.index>=MAX_COPORTS)
    {
        pthread_mutex_unlock(&comutex_table.lock);
        return 1;
    }
    comutex_table.table[comutex_table.index]=entry;
    entry_index=comutex_table.index++;
    pthread_mutex_unlock(&comutex_table.lock);
    return entry_index;
}

int lookup_port(char * port_name,sys_coport_t ** port_buf)
{
    int j;
    for(j = 0; j<COPORT_NAME_LEN; j++)
    {
        if (port_name[j]=='\0')
        {
            break;
        }
    }
    if(j==COPORT_NAME_LEN)
    {
        err(1,"port name length too long");
    }
    for(int i = 0; i<coport_table.index;i++)
    {
        if(strncmp(port_name,coport_table.table[i].name,COPORT_NAME_LEN)==0)
        {
            *port_buf=cheri_csetbounds(&coport_table.table[i].port,sizeof(sys_coport_t));
            return 0;
        }   
    }
    //printf("port %s not found",port_name);
    *port_buf=NULL;
    return 1;
}

int lookup_mutex(char * mtx_name,sys_comutex_t ** mtx_buf)
{
    if (strlen(mtx_name)>COPORT_NAME_LEN)
    {
        err(1,"mtx name length too long");
    }
    for(int i = 0; i<comutex_table.index;i++)
    {
        if (comutex_table.table[i].mtx.user_mtx==NULL)
        {
            *mtx_buf=NULL;
            return 1;
        }
        if(strcmp(mtx_name,comutex_table.table[i].mtx.name)==0)
        {
            *mtx_buf=&comutex_table.table[i].mtx;
            return 0;
        }
    }
    *mtx_buf=NULL;
    return 1;
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

bool valid_coport(sys_coport_t * addr)
{
    ptrdiff_t table_offset;
    vaddr_t port_addr = (vaddr_t) addr;
    int index;

    if(cheri_getlen(addr)<sizeof(sys_coport_t))
    {
        printf("too small to represent coport\n");
        return false;
    }
    else if(!cheri_is_address_inbounds(coport_table.table,port_addr))
    {
        printf("address not in bounds\n");
        return false;
    }
    else
    {
        table_offset=port_addr-cheri_getbase(coport_table.table);
        index=table_offset/sizeof(coport_tbl_entry_t);
        if(&coport_table.table[index].port!=addr)
        {
            printf("offset looks wrong\n");
            return false;
        }
    }
    return true;
}

bool valid_cocarrier(sys_coport_t * addr)
{
    if(cheri_gettype(addr)!=sealed_otype)
    {
        printf("wrong type\n");
        return false;
    }
    else
    {
        addr=cheri_unseal(addr,root_seal_cap);
    }
    if(!valid_coport(addr))
    {
        return false;
    }

    return true;
}

bool event_match(sys_coport_t * cocarrier,coport_eventmask_t e)
{
	return ((bool) cocarrier->event & e);
}

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
        targets=ukern_fast_malloc(sizeof(pollcoport_t)*copoll_args->ncoports);
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
    uint index;
    //coport_status_t status;
    uint len;

    worker_args_t * data = args;
    cocall_cocarrier_send_t * cocarrier_send_args;

    void * __capability sw_code;
    void * __capability sw_data;
    void * __capability caller_cookie;
    void * __capability target;

    sys_coport_t * cocarrier;
    void ** __capability cocarrier_buf;

    cocarrier_send_args=malloc(sizeof(cocall_cocarrier_send_t));

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
        atomic_thread_fence(memory_order_acquire);
        atomic_store_explicit(&cocarrier->status,COPORT_BUSY,memory_order_release);
        index=cocarrier->start;
        if(cocarrier->length==0)
        {
                cocarrier->event&=COPOLL_RERR;
                atomic_thread_fence(memory_order_release);

                //buffer is empty - return error
                cocarrier_send_args->status=-1;
                cocarrier_send_args->error=EAGAIN;
                continue;
        }
        len=MIN(cheri_getlen(cocarrier_buf[index]),cheri_getlen(cocarrier_send_args->message));
        memcpy(cocarrier_send_args->message,cocarrier_buf[index],len);
        cocarrier->start++;
        cocarrier->length--;
        if (cocarrier->length==0)
        	cocarrier->event=((COPOLL_OUT | cocarrier->event) & ~COPOLL_RERR) & ~COPOLL_IN;
        else
            cocarrier->event=((COPOLL_OUT | cocarrier->event) & ~COPOLL_RERR);
        
        atomic_store_explicit(&cocarrier->status,COPORT_OPEN,memory_order_release);
        atomic_thread_fence(memory_order_release);
        if(!LIST_EMPTY(&cocarrier->listeners))
        {
            pthread_cond_signal(&global_cosend_cond);
        }

        cocarrier_send_args->status=len;
        cocarrier_send_args->error=0;
    }
    return args;
}

void *cocarrier_send(void *args)
{
    //todo implement
    int error;
    size_t index;
    //coport_status_t status;

    worker_args_t * data = args;
    cocall_cocarrier_send_t * cocarrier_send_args;

    void * __capability sw_code;
    void * __capability sw_data;
    void * __capability caller_cookie;
    void * __capability target;

    sys_coport_t * cocarrier;
    void * __capability msg_buf;
    void ** __capability cocarrier_buf;

    cocarrier_send_args=malloc(sizeof(cocall_cocarrier_send_t));

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

            continue;
        }
        cocarrier=cheri_unseal(cocarrier,root_seal_cap);
        //allocate ukernel owned buffer
        msg_buf=ukern_fast_malloc(cheri_getlen(cocarrier_send_args->message));
        //copy data into buffer
        memcpy(msg_buf,cocarrier_send_args->message,cheri_getlen(cocarrier_send_args->message));
        //reduce cap permissions on buffer
        msg_buf=cheri_andperm(msg_buf,COCARRIER_PERMS);
        //toggle state to busy
        cocarrier_buf=cocarrier->buffer;
        //add buffer to cocarrrier
        cocarrier_buf=cocarrier->buffer;
        atomic_thread_fence(memory_order_acquire);
        index=(cocarrier->end+1)%COCARRIER_SIZE;
        if(cocarrier->length>0)
        {
            if(index==cocarrier->start)
            {
                cocarrier->event&=COPOLL_WERR;
                //cocarrier->status=COPORT_OPEN;
                atomic_thread_fence(memory_order_release);
                ukern_fast_free(msg_buf);

                //buffer is full - return error
                cocarrier_send_args->status=-1;
                cocarrier_send_args->error=EAGAIN;
                continue;
            }
        }
        if(cheri_gettag(cocarrier_buf[index]))
        {
        	//auto overwrite old messages once we've wrapped around
        	ukern_fast_free(cocarrier_buf[index]);
        }
        cocarrier_buf[index]=msg_buf;
        cocarrier->end=index;
        cocarrier->length++;
        
        if(cocarrier->length==COCARRIER_SIZE)
        	cocarrier->event=(((COPOLL_IN | cocarrier->event) & ~COPOLL_WERR) & ~COPOLL_OUT);
        else
            cocarrier->event=(COPOLL_IN | cocarrier->event) & ~COPOLL_WERR;
        //check if anyone is waiting on messages to arrive
        atomic_store_explicit(&cocarrier->status,COPORT_OPEN,memory_order_release);
        atomic_thread_fence(memory_order_release);
        if(!LIST_EMPTY(&cocarrier->listeners))
        {
            pthread_cond_signal(&global_cosend_cond);
        }
        //release
        cocarrier_send_args->status=cheri_getlen(cocarrier_send_args->message);
        cocarrier_send_args->error=0;
        
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

        lookup=lookup_port(coport_args->args.name,&prt);
        if(lookup==1)
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
            if(port.type==COCARRIER)
            {
                LIST_INIT(&port.listeners);
                port.event=COPOLL_INIT_EVENTS;
            }
            table_entry.port=port;
            table_entry.id=generate_id();
            strcpy(table_entry.name,coport_args->args.name);
            index=add_port(table_entry);
            //printf("coport %s added to table\n",coport_args->args.name);
            prt=cheri_csetbounds(&coport_table.table[index].port,sizeof(sys_coport_t));
        }
        if(prt->type==COCARRIER)
        {
            prt=cheri_seal(prt,seal_cap);
        }
        coport_args->port=prt;
        printf("coport_perms: %lu\n",cheri_getperm(prt));

    }
    free(coport_args);
    return 0;
}

void create_comutex(comutex_t * cmtx,char * name)
{
    int error;
    sys_comutex_t * sys_mtx;
    comutex_tbl_entry_t table_entry;

    sys_mtx=ukern_malloc(sizeof(sys_comutex_t));
    error=sys_comutex_init(name,sys_mtx);
    
    table_entry.id=generate_id();
    table_entry.mtx=*sys_mtx;

    cmtx->mtx=sys_mtx->user_mtx;
    strcpy(cmtx->name,name);

    error=add_mutex(table_entry);

    return;
}

void *comutex_setup(void *args)
{
    worker_args_t * data=args;
    cocall_comutex_init_t comutex_args;
    comutex_tbl_entry_t table_entry;
    sys_comutex_t * mtx;

    int error;
    int index;
    int lookup;

    void * __capability sw_code;
    void * __capability sw_data;
    void * __capability caller_cookie;
    void * __capability target;
    

    error=coaccept_init(&sw_code,&sw_data,data->name,&target);
    data->cap=target;
    update_worker_args(data,U_COMUTEX_INIT);
    for (;;)
    {
        error=coaccept(sw_code,sw_data,&caller_cookie,&comutex_args,sizeof(comutex_args));
        /* check args are acceptable */

        /* check if mutex exists */
        pthread_mutex_lock(&comutex_table.lock);
        lookup=lookup_mutex(comutex_args.args.name,&mtx);
        if(lookup==1)
        {
            /* if it doesn't, set up mutex */
            mtx=ukern_malloc(sizeof(sys_comutex_t));
            error=sys_comutex_init(comutex_args.args.name,mtx);
            table_entry.mtx=*mtx;
            table_entry.id=generate_id();
            index=add_mutex(table_entry);
            if(error!=0)
            {
                err(1,"unable to init_port");
            }
        }
        pthread_mutex_unlock(&comutex_table.lock);
        strcpy(comutex_args.mutex->name,mtx->name);
        comutex_args.mutex->mtx=mtx->user_mtx;
        comutex_args.mutex->key=NULL;
        
    }
    return 0;
}

void *comutex_lock(void *args)
{
    worker_args_t * data=args;
    colock_args_t colock_args;
    sys_comutex_t * mtx;
    comutex_t * user_mutex;

    int error;
    int lookup;

    void * __capability sw_code;
    void * __capability sw_data;
    void * __capability caller_cookie;
    void * __capability target;

    error=coaccept_init(&sw_code,&sw_data,data->name,&target);
    data->cap=target;
    update_worker_args(data,U_COLOCK);
    for (;;)
    {
        error=coaccept(sw_code,sw_data,&caller_cookie,&colock_args,sizeof(colock_args));
        /* check args are acceptable */
        // validation
        /* check if mutex exists */
        lookup=lookup_mutex(colock_args.mutex->name,&mtx);
        if(lookup==0)
        {
            user_mutex=colock_args.mutex;
            error=sys_colock(mtx,user_mutex->key);
            mtx->key=user_mutex->key;
        }
        //report errors
    }
    return 0;
}

void *comutex_unlock(void *args)
{
    void * __capability sw_code;
    void * __capability sw_data;
    void * __capability  caller_cookie;
    void * __capability  target;

    worker_args_t * data=args;
    counlock_args_t colock_args;
    sys_comutex_t * mtx;
    comutex_t * user_mutex;

    int error;
    int lookup;

    error=coaccept_init(&sw_code,&sw_data,data->name,&target);
    data->cap=target;
    update_worker_args(data,U_COUNLOCK);
    for (;;)
    {
        error=coaccept(sw_code,sw_data,&caller_cookie,&colock_args,sizeof(colock_args));
        /* check args are acceptable */
        // validation
        /* check if mutex exists */
        lookup=lookup_mutex(colock_args.mutex->name,&mtx);
        if(lookup==0)
        {
            user_mutex=colock_args.mutex;
            error=sys_counlock(mtx,user_mutex->key);
            mtx->key=user_mutex->key;
        }
        //report errors
    }
    return 0;
}

int comutex_deinit(comutex_tbl_entry_t * m)
{
    sys_comutex_t mtx = m->mtx;
    free(mtx.kern_mtx->lock);
    free(mtx.kern_mtx);
    mtx.user_mtx=NULL;

    // remove from table?
    return 0;
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
        err(1,"ERROR: Could not cosetup.\n");
    }
    //printf("Attempting to coregister with name %s\n",target_name);

    error=coregister(target_name,target_cap);
    if (error!=0)
    {
        err(errno,"ERROR: Could not coregister with name %s.\n",target_name);
    }
    //printf("Successfully coregistered with name %s\n",target_name);
    //printf("validity: %u\n",cheri_gettag(*target_cap));

    return (error);
}

int coport_tbl_setup(void)
{
    pthread_mutexattr_t lock_attr;
    pthread_condattr_t cond_attr;

    pthread_mutexattr_init(&lock_attr);
    pthread_mutex_init(&global_copoll_lock,&lock_attr);
    pthread_condattr_init(&cond_attr);
    pthread_cond_init(&global_cosend_cond,&cond_attr);

    coport_table.index=0;
    coport_table.table=ukern_malloc(COPORT_TBL_LEN);
    mlock(coport_table.table,COPORT_TBL_LEN);
    
    /* reserve a superpage or two for this, entries should be small */
    /* reserve a few superpages for ports */
    return 0;
}
int comutex_tbl_setup(void)
{
    pthread_mutexattr_t lock_attr;
    pthread_mutexattr_init(&lock_attr);
    pthread_mutexattr_settype(&lock_attr,PTHREAD_MUTEX_RECURSIVE);
    int error=pthread_mutex_init(&comutex_table.lock,&lock_attr);
    comutex_table.index=0;
    comutex_table.table=ukern_malloc(COMTX_TBL_LEN);
    mlock(comutex_table.table,COMTX_TBL_LEN);
    /* reserve a superpage or two for this, entries should be small */
    /* reserve a few superpages for ports */
    return error;
}

int spawn_workers(void * func, pthread_t * threads, const char * name)
{
    pthread_t * thread;
    pthread_attr_t thread_attrs;
    worker_args_t * args;
    int e;
    int w_i;
    char * thread_name;
    bool private;

    if (name[0]=='_')
    	private=true;
    else
    	private=false;
    /* split into threads */
    thread_name=ukern_fast_malloc(THREAD_STRING_LEN*sizeof(char));
    threads=(pthread_t *) ukern_malloc(WORKER_COUNT*sizeof(pthread_t));
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

    for (int i = 0; i < WORKER_COUNT; i++)
    {
        thread=ukern_fast_malloc(sizeof(pthread_t));
        args=ukern_fast_malloc(sizeof(worker_args_t));
        rand_string(thread_name,THREAD_STRING_LEN);
        strcpy(args->name,thread_name);
        args->cap=NULL;
        //printf("%s",thread_name);
        e=pthread_attr_init(&thread_attrs);
        //printf("thr_name %s\n",args->name);
        e=pthread_create(thread,&thread_attrs,func,args);
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
            threads[i]=*thread;
        }
        pthread_attr_destroy(&thread_attrs);
        ukern_fast_free(thread);
        ukern_fast_free(args);
    }
    ukern_fast_free(thread_name);
    return 0;
}

void run_tests(void)
{
    //TODO-PBB: Would be handy.
    return;
}

int main(int argc, const char *argv[])
{
    int verbose;
    int error = 0;
    request_handler_args_t * handler_args;

    pthread_t memory_manager, commap_manager;
    pthread_t coopen_threads[WORKER_COUNT];
    pthread_t counlock_threads[WORKER_COUNT];
    pthread_t comutex_init_threads[WORKER_COUNT];
    pthread_t colock_threads[WORKER_COUNT];
    pthread_t cocarrier_send_threads[WORKER_COUNT];
    pthread_t cocarrier_recv_threads[WORKER_COUNT];
    pthread_t copoll_threads[WORKER_COUNT];
    pthread_t copoll_deliver_threads[WORKER_COUNT];
    //pthread_t coclose_threads[WORKER_COUNT];

    pthread_t coopen_handler;
    pthread_t counlock_handler;
    pthread_t comutex_init_handler;
    pthread_t colock_handler;
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
    pthread_create(&memory_manager,&thread_attrs,ukern_mman,NULL);

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
    error+=coport_tbl_setup();
    error+=comutex_tbl_setup();
    if(error!=0)
    {
        err(1,"Initial setup failed!!");
    }
    printf("Initial setup complete.\n");
    pthread_create(&commap_manager,&thread_attrs,ukern_mmap,NULL);

    /* perform setup */
    printf("Spawning co-open listeners...\n");
    error+=spawn_workers(&coport_open,coopen_threads,U_COOPEN);
    printf("Spawning comutex_init listeners...\n");
    error+=spawn_workers(&comutex_setup,comutex_init_threads,U_COMUTEX_INIT);
    printf("Spawning colock listeners...\n");
    error+=spawn_workers(&comutex_lock,colock_threads,U_COLOCK);
    printf("Spawning counlock listeners...\n");
    error+=spawn_workers(&comutex_unlock,counlock_threads,U_COUNLOCK);
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

    pthread_join(coopen_threads[0],NULL);
    pthread_join(coopen_handler,NULL);

    return 0;
}