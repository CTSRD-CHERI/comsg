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
#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>

#include <assert.h>
#include <unistd.h>
#include <stdatomic.h>
#include <err.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <sys/cdefs.h>
#include <sys/types.h>
#include <machine/sysarch.h>

#include "coproc.h"
#include "coport.h"
#include "comsg.h"

#define LIBCOMSG_OTYPE 1
//Sealing root
static void * __capability libcomsg_sealroot;
//Sealing cap for coport_t
static otype_t libcomsg_coport_seal;
static long libcomsg_otype;
static long cocarrier_otype = -1;

int coopen(const char * coport_name, coport_type_t type, coport_t *prt)
{
    /* request new coport from microkernel */
    void * __capability switcher_code;
    void * __capability switcher_data;
    void * __capability func;

    cocall_coopen_t * __capability call;

    uint error;

    /* cocall setup */
    //TODO-PBB: Only do this once.
    call=calloc(1,sizeof(cocall_coopen_t));
    strcpy(call->args.name,coport_name);
    call->args.type=type;
    //call->args=args;
    //printf("cocall_open_t has size %lu\n",sizeof(cocall_coopen_t));

    if (call!=NULL)
    {
        //possible deferred call to malloc ends up in cocall, causing exceptions
        error=ukern_lookup(&switcher_code,&switcher_data,U_COOPEN,&func);
        error=cocall(switcher_code,switcher_data,func,call,sizeof(cocall_coopen_t));
        if (!cheri_getsealed(call->port))
        {
            *prt=cheri_seal(call->port,libcomsg_coport_seal);
        }
        else
        {
            cocarrier_otype=cheri_gettype(call->port);
            *prt=call->port;
        }
    }
    free(call);
    return 1;
}

inline
coport_t coport_clearperm(coport_t p,int perms)
{
    perms&=CHERI_PERMS_SWALL; //prevent trashing the rest of the perms word, which is unrelated to coport access control
    return cheri_andperm(p,perms);
}

int cosend(coport_t port, const void * buf, size_t len)
{
    void * __capability switcher_code;
    void * __capability switcher_data;
    void * __capability func;
    cocall_cocarrier_send_t * call;

    unsigned int old_end;
    int retval = len;
    coport_status_t status_val;
    coport_type_t type;
    
    assert(cheri_getperm(port) & COPORT_PERM_SEND);
    assert(cheri_getsealed(port)!=0);

    if(cheri_gettype(port)==libcomsg_otype)
    {
        port=cheri_unseal(port,libcomsg_sealroot);
        type=port->type;
    }
    else
    {
        type=COCARRIER;
    }
    switch(type)
    {
        case COCHANNEL:
            for(;;)
            {
                status_val=COPORT_OPEN;
                if(atomic_compare_exchange_weak_explicit(&port->status,&status_val,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
                {
                    break;
                }
            }
            atomic_thread_fence(memory_order_acquire);
            if((port->length-((port->end-port->start)%port->length))<len)
            {
                err(1,"message too big/buffer is full");
            }
            old_end=port->end;
            port->end=(port->end+len)%port->length;
            if(old_end+len>port->length)
            {
                memcpy((char *)port->buffer+old_end, buf, port->length-old_end);
                memcpy((char *)port->buffer+old_end, (const char *)buf+port->length-old_end, (old_end+len)%port->length);

            }
            else
            {
                memcpy((char *)port->buffer+old_end, buf, len);
            }
            atomic_store_explicit(&port->status,COPORT_OPEN,memory_order_relaxed);
            atomic_thread_fence(memory_order_release);
            break;
        case COCARRIER:
            call=calloc(1,sizeof(cocall_cocarrier_send_t));
            call->cocarrier=port;
            call->message=cheri_csetbounds(buf,len);
            call->message=cheri_andperm(call->message,COCARRIER_PERMS);
            
            ukern_lookup(&switcher_code,&switcher_data,U_COCARRIER_SEND,&func);
            cocall(switcher_code,switcher_data,func,call,sizeof(cocall_cocarrier_send_t));
            if(call->status<0)
            {
                warn("error occurred during cocarrier send");
                errno=call->error;
                return -1;
            }
            else
                retval=call->status;
            break;
        case COPIPE:
            for(;;)
            {
                status_val=COPORT_READY;
                if(atomic_compare_exchange_weak_explicit(&port->status,&status_val,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
                {
                    break;
                }
            }
            atomic_thread_fence(memory_order_acquire);
            if(cheri_getlen(port->buffer)<len)
            {
                err(1,"recipient buffer len %lu too small for message of length %lu",cheri_getlen(port->buffer),len);
                errno=EINVAL;
                return -1;
            }
            memcpy(port->buffer,buf,len);
            atomic_store_explicit(&port->status,COPORT_DONE,memory_order_relaxed);
            atomic_thread_fence(memory_order_release);
            break;
        default:
            errno=EINVAL;
            return -1;
    }
    
    return retval;
}

int corecv(coport_t port, void ** buf, size_t len)
{
    //we need more atomicity on changes to end
    int old_start;
    void * __capability switcher_code;
    void * __capability switcher_data;
    void * __capability func;
    cocall_cocarrier_send_t * call;
    coport_status_t status_val;
    coport_type_t type;
    
    assert(cheri_getperm(port)&COPORT_PERM_RECV);
    assert(cheri_getsealed(port)!=0);
    if(cheri_gettype(port)!=libcomsg_otype)
    {
        type=COCARRIER;
    }
    else
    {
        port=cheri_unseal(port,libcomsg_sealroot);
        type=port->type;
        for(;;)
        {
            status_val=COPORT_OPEN;
            if(atomic_compare_exchange_weak_explicit(&port->status,&status_val,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
            {
                break;
            }
        }
    }
    
    atomic_thread_fence(memory_order_acquire);
    switch(type)
    {
        case COCHANNEL:
            if (port->start==port->length)
            {
                err(EAGAIN,"no message to receive");
            }
            if(port->length<len)
            {
                err(EMSGSIZE,"message too big");
            }
            if(port->buffer==NULL)
            {
                err(EPIPE,"coport is closed");
            }
            
            for(;;)
            {
                status_val=COPORT_OPEN;
                if(atomic_compare_exchange_weak_explicit(&port->status,&status_val,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
                {
                    break;
                }
            }
            old_start=port->start;
            port->start=port->start+len;
            memcpy(*buf,(char *)port->buffer+old_start, len);
            port->start=port->start+len;
            port->status=COPORT_OPEN;
            break;
        case COCARRIER:
            call=calloc(1,sizeof(cocall_cocarrier_send_t));
            call->cocarrier=port;
            call->message=cheri_csetbounds(buf,len);
            ukern_lookup(&switcher_code,&switcher_data,U_COCARRIER_RECV,&func);
            cocall(switcher_code,switcher_data,func,call,sizeof(cocall_cocarrier_send_t));
            if(call->status<0)
            {
                err(call->error,"error occurred during cocarrier recv");
            }
            
             if(cheri_getlen(call->message)!=len)
            {
                warn("message length (%lu) does not match len (%lu)",cheri_getlen(buf),len);
            }
            if((cheri_getperm(call->message)&(CHERI_PERM_LOAD|CHERI_PERM_LOAD_CAP))==0)
            {
                err(1,"received capability does not grant read permissions");
            }
            free(call);
            break;
        case COPIPE:
            port->buffer=*buf;
            atomic_store_explicit(&port->status,COPORT_READY,memory_order_relaxed);
            atomic_thread_fence(memory_order_release);
            for(;;)
            {
                status_val=COPORT_DONE;
                if(atomic_compare_exchange_weak(&port->status,&status_val,COPORT_OPEN))
                {
                    break;
                }
            }
            port->buffer=NULL;
            break;
        default:
            err(1,"invalid coport type");
            break;
    }
    atomic_thread_fence(memory_order_release);
    return len;
    
}

coport_type_t coport_gettype(coport_t port)
{

    if(!cheri_getsealed(port))
        return INVALID;
    if(cheri_gettype(port)!=libcomsg_otype)
    {
        //XXX-PBB:this is bad. 
        //if both are true we don't reach this. 
        //we give sealed stuff a pass if we don't know the cocarrier_otype yet.
        if(cocarrier_otype==-1 || cheri_gettype(port) == cocarrier_otype)
            return (COCARRIER);
    }
    else 
    {
        port=cheri_unseal(port,libcomsg_sealroot);
        return (port->type);
    }
    return INVALID;
}

pollcoport_t make_pollcoport(coport_t port, coport_eventmask_t events)
{
    pollcoport_t pcpt;

    assert(coport_gettype(port)==COCARRIER);
    pcpt.coport=port;
    pcpt.events=events;
    pcpt.revents=0;

    return pcpt;
}

int coclose(coport_t port)
{
    //TODO-PBB: IMPLEMENT
    //if we possess the CLOSE permission, we can unilaterally close the port for all users.
    //otherwise, this operation decrements the ukernel refcount for the port
    port=NULL;
    return 0;
}

int copoll(pollcoport_t * coports, int ncoports, int timeout)
{
    void * __capability switcher_code;
    void * __capability switcher_data;
    void * __capability func;

    copoll_args_t * call;
    pollcoport_t * call_coports;
    uint error;
    int status;

    for(int i = 0; i<ncoports; i++)
    assert(cheri_getperm(coports[i].coport)&COPORT_PERM_POLL);

    /* cocall setup */
    //TODO-PBB: Only do this once.
    call=calloc(1,sizeof(copoll_args_t));
    //allocate our own for safety purposes
    call_coports=calloc(ncoports,sizeof(pollcoport_t));
    memcpy(call_coports,coports,sizeof(pollcoport_t)*ncoports);
    
    call->coports=call_coports;
    call->ncoports=ncoports;
    call->timeout=timeout;


    //possible deferred call to malloc ends up in cocall, causing exceptions?
    error=ukern_lookup(&switcher_code,&switcher_data,U_COPOLL,&func);
    if(error)
    {
        free(call_coports);
        free(call);
        return -1;
    }
    error=cocall(switcher_code,switcher_data,func,call,sizeof(cocall_coopen_t));

    errno=call->error;
    status=call->status;
    if(status!=0)
    {
        for (int i = 0; i<ncoports; ++i)
        {
            coports[i].revents=call->coports[i].revents;
        }
    }
    free(call_coports);
    free(call);

    return (status);
}

__attribute__ ((constructor)) static 
void libcomsg_init(void)
{
    if (sysarch(CHERI_GET_SEALCAP,&libcomsg_sealroot) < 0)
    {
        libcomsg_sealroot=NULL;
    }
    assert((cheri_gettag(libcomsg_sealroot) != 0));    
    assert(cheri_getlen(libcomsg_sealroot) != 0);
    assert((cheri_getperm(libcomsg_sealroot) & CHERI_PERM_SEAL) != 0);
    //XXX-PBB: Is 1 an okay value?
    libcomsg_coport_seal=cheri_maketype(libcomsg_sealroot,LIBCOMSG_OTYPE);
    libcomsg_otype=cheri_gettype(cheri_seal(libcomsg_coport_seal,libcomsg_coport_seal));
    libcomsg_sealroot=cheri_setoffset(libcomsg_sealroot,LIBCOMSG_OTYPE);
}
