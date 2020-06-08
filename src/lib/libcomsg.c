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
#include <sched.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <machine/sysarch.h>
#include <sys/sysctl.h>

#include "coproc.h"
#include "coport.h"
#include "comsg.h"
#include <statcounters.h>

#define LIBCOMSG_OTYPE 1
//Sealing root
static void * __capability libcomsg_sealroot;
//Sealing cap for coport_t
static otype_t libcomsg_coport_seal;
static long libcomsg_otype;
static long cocarrier_otype = -1;
static long copipe_otype = -1;
static long cochannel_otype = -1;
static long multicore = 0;

typedef struct _port_tbl_entry
{
    coport_t port;
    int wired;
} pentry_t;

struct _ports_tbl
{
    int index;
    pentry_t entries[10];
};

//static struct _ports_tbl port_locked_table;

int coopen(const char * coport_name, coport_type_t type, coport_t *prt)
{
    /* request new coport from microkernel */
    void * __capability switcher_code;
    void * __capability switcher_data;
    void * __capability func;

    cocall_coopen_t call;

    uint error;

    /* cocall setup */
    //TODO-PBB: Only do this once.
    strcpy(call.args.name,coport_name);
    call.args.type=type;
    //call->args=args;
    //printf("cocall_open_t has size %lu\n",sizeof(cocall_coopen_t));

    
    //possible deferred call to malloc ends up in cocall, causing exceptions
    error=ukern_lookup(&switcher_code,&switcher_data,U_COOPEN,&func);
    error=cocall(switcher_code,switcher_data,func,&call,sizeof(cocall_coopen_t));
    if (!cheri_getsealed(call.port))
    {
        *prt=cheri_seal(call.port,libcomsg_coport_seal);
        mlock(call.port,sizeof(sys_coport_t));
        if(call.port->type==COPIPE)
        {
            copipe_otype=cheri_gettype(*prt);
        }
        else if(call.port->type==COCHANNEL)
        {
            cochannel_otype=cheri_gettype(*prt);   
        }
    }
    else
    {
        cocarrier_otype=cheri_gettype(call.port);
        *prt=call.port;
        //pre-fetch cocarrier operations to speed up later
        ukern_lookup(&switcher_code,&switcher_data,U_COCARRIER_SEND,&func);
        ukern_lookup(&switcher_code,&switcher_data,U_COCARRIER_RECV,&func);

    }
    
    return 1;
}

inline
coport_t coport_clearperm(coport_t p, int perms)
{
    perms&=CHERI_PERMS_SWALL; //prevent trashing the rest of the perms word, which is unrelated to coport access control
    return cheri_andperm(p,perms);
}

int cosend(const coport_t prt, const void * buf, size_t len)
{
    coport_t port=prt;

    void * __capability switcher_code;
    void * __capability switcher_data;
    void * __capability func;
    void * __capability out_buffer;
    cocall_cocarrier_send_t call;

    unsigned int old_end;
    int retval = len;
    unsigned long unread;
    coport_status_t status_val;
    coport_type_t type;

    struct timespec wait;
    wait.tv_sec=0;
    wait.tv_nsec=100;
    int i = 1;
    
    //assert(cheri_getperm(port) & COPORT_PERM_SEND); //doesn't work properly
    //assert(cheri_getsealed(port)!=0);
    if(cheri_gettype(port)==libcomsg_otype)
    {
        port=cheri_unseal(port,libcomsg_sealroot);
        type=port->type;
    }
    else
    {
        type=COCARRIER;
    }
    //assert(cheri_gettag(port));
    switch(type)
    {
        case COCHANNEL:
            for(;;)
            {
                status_val=COPORT_OPEN;
                if(atomic_compare_exchange_weak_explicit(&port->status,&status_val,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
                    break;
            }
            atomic_thread_fence(memory_order_acquire);
            unread=(port->end-port->start)%port->length;
            if((port->length-unread)<len)
            {
                err(EAGAIN,"cosend: message (%luB) too big/buffer (%luB) is full (%luB)",len,port->length,unread);
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
            port->event|=COPOLL_IN;
            if ((port->end-port->start)%port->length==port->length)
            {
                port->event&=~COPOLL_OUT;
            }
            port->status=COPORT_OPEN;
            atomic_thread_fence(memory_order_release);
            break;
        case COCARRIER:
            call.cocarrier=port;
            call.message=cheri_setbounds(buf,len);
            call.status=0;
            call.error=0;
            call.message=cheri_andperm(call.message,COCARRIER_PERMS);
            
            if(ukern_lookup(&switcher_code,&switcher_data,U_COCARRIER_SEND,&func)!=0)
                perror("cosend: error in ukern_lookup");

            if(cocall(switcher_code,switcher_data,func,&call,sizeof(cocall_cocarrier_send_t))!=0)
                perror("cosend: error in cocall");
            if(call.status==-1)
            {
                warn("cosend: error occurred during cocarrier send\n");
                errno=call.error;
                retval=-call.status;
            }
            else
            {
                retval=call.status;
            }

            break;
        case COPIPE:
            for(;;)
            {
                
                status_val=COPORT_READY;
                if(atomic_compare_exchange_weak_explicit(&port->status,&status_val,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
                {
                    out_buffer=atomic_load_explicit(&port->buffer,memory_order_acquire);
                    if(out_buffer == NULL)
                        continue;
                    else
                        break;
                }
                else
                {
                    if(i%10 == 0)
                    {
                        //at this point, as long as we are the scheduled thread it won't become ready, so yield the cpu
                        //this context switch will eventually happen, so we skip some needless spinning this way
                        sched_yield(); 
                    }
                    i++;
                }
            }
            if(cheri_getlen(out_buffer)<len)
            {
                err(EMSGSIZE,"cosend: recipient buffer len %lu too small for message of length %lu",cheri_getlen(out_buffer),len);
                errno=EINVAL;
                return -1;
            }
            memcpy(out_buffer,buf,len);
            atomic_store_explicit(&port->status,COPORT_DONE,memory_order_release);
            break;
        default:
            errno=EINVAL;
            return -1;
    }
    i=0;
    if(retval==-1)
        return retval;
    return len;
}

int corecv(const coport_t prt, void ** buf, size_t len)
{
    //we need more atomicity on changes to end
    coport_t port = prt;
    int old_start;
    void * __capability switcher_code;
    void * __capability switcher_data;
    void * __capability func;
    cocall_cocarrier_send_t call;
    coport_status_t status_val;
    coport_type_t type;
    uint i = 1;
    int retval = len;
    
    //assert(cheri_getperm(port)&COPORT_PERM_RECV); //doesn't work
    //assert(cheri_getsealed(port)!=0);
    if(cheri_gettype(port)!=libcomsg_otype)
    {
        type=COCARRIER;
    }
    else
    {
        port=cheri_unseal(port,libcomsg_sealroot);
        type=port->type;
        
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
            if (port->start==port->length)
            {
                err(EAGAIN,"corecv: no message to receive");
            }
            if(port->length<len)
            {
                err(EMSGSIZE,"corecv: expected message length %lu larger than buffer", len);
            }
            if(!cheri_gettag(port->buffer))
            {
                err(EPIPE,"corecv: coport is closed");
            }
            old_start=port->start;
            port->start=port->start+len;
            memcpy(*buf,(char *)port->buffer+old_start, len);
            port->start=port->start+len;
            port->event|=COPOLL_OUT;
            if (port->end==port->start)
            {
                port->event&=~COPOLL_IN;
            }
            port->status=COPORT_OPEN;
            atomic_thread_fence(memory_order_release);
            break;
        case COCARRIER:
            call.cocarrier=port;
            call.error=0;
            call.status=0;
            ukern_lookup(&switcher_code,&switcher_data,U_COCARRIER_RECV,&func);
            cocall(switcher_code,switcher_data,func,&call,sizeof(cocall_cocarrier_send_t));
            if(call.status==-1)
            {
                errno=call.error;
                warn("corecv: error occurred during cocarrier recv");
                retval=call.status;
            }
            else 
            {
                if(cheri_getlen(call.message)!=len)
                {
                    warn("corecv: message length (%lu) does not match len (%lu)",cheri_getlen(call.message),len);
                }
                if((cheri_getperm(call.message)&(CHERI_PERM_LOAD|CHERI_PERM_LOAD_CAP))==0)
                {
                    err(1,"corecv: received capability does not grant read permissions");
                }
                memcpy(*buf,call.message,MIN(cheri_getlen(buf),MIN(cheri_getlen(call.message),len)));
            
            }
            break;
        case COPIPE:
            for(;;)
            {
                status_val=COPORT_OPEN;
                if(atomic_compare_exchange_weak_explicit(&port->status,&status_val,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
                {
                    if(atomic_load_explicit(&port->buffer,memory_order_acquire) != NULL)
                        continue;
                    else
                        break;
                }
                else
                {
                    if(i % 10 == 0)
                    {
                        //at this point, as long as we are the scheduled thread it won't become ready, so yield the cpu
                        //this context switch will eventually happen, so we skip some needless spinning this way
                        sched_yield(); 
                    }
                    i++;
                }
            }
            atomic_store_explicit(&port->buffer, *buf, memory_order_release);
            atomic_store_explicit(&port->status, COPORT_READY, memory_order_release);
            for(;;)
            {
                status_val=COPORT_DONE;
                if(atomic_compare_exchange_weak_explicit(&port->status,&status_val,COPORT_OPEN,memory_order_acq_rel,memory_order_acquire))
                {
                    atomic_store_explicit(&port->buffer, NULL, memory_order_release);
                    break;
                }
                else
                {
                    if(i%10 == 0)
                    {
                        //at this point, as long as we are the scheduled thread it won't become ready, so yield the cpu
                        //this context switch will eventually happen, so we skip some needless spinning this way
                        sched_yield(); 
                    }
                    i++;
                }
            }
            retval=0;
            break;
        default:
            err(1,"corecv: invalid coport type");
            break;
    }
    i=0;
    if(retval==-1)
        return retval;
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

pollcoport_t make_pollcoport(coport_t port, int events)
{
    pollcoport_t pcpt;

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

static
int cocarrier_poll(pollcoport_t * coports, int ncoports, int timeout)
{
    void * __capability switcher_code;
    void * __capability switcher_data;
    void * __capability func;
    copoll_args_t * call;
    pollcoport_t * call_coports;


    uint error;
    int status;
    call=calloc(1,sizeof(copoll_args_t));
    //allocate our own for safety purposes
    call_coports=calloc(ncoports,sizeof(pollcoport_t));
    memcpy(call_coports,coports,sizeof(pollcoport_t)*ncoports);
    
    call->coports=call_coports;
    call->ncoports=ncoports;
    call->timeout=timeout;
    /* cocall setup */
    //TODO-PBB: Only do this once.

    //possible deferred call to malloc ends up in cocall, causing exceptions?
    error=ukern_lookup(&switcher_code,&switcher_data,U_COPOLL,&func);
    if(error)
    {
        free(call_coports);
        free(call);
        return -1;
    }
    error=cocall(switcher_code,switcher_data,func,call,sizeof(cocall_coopen_t));
    assert(error==0);
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

int copoll(pollcoport_t * coports, int ncoports, int timeout)
{
    

    assert(ncoports>0);
    assert(cheri_gettag(coports));

    for(int i = 1; i<ncoports; i++)
        assert((coport_gettype(coports[i].coport)==coport_gettype(coports[i-1].coport))||(coport_gettype(coports[i].coport)!=COCARRIER && (coport_gettype(coports[i-1].coport)!=COCARRIER)));

    
    if(coport_gettype(coports[0].coport)==COCARRIER)
        return(cocarrier_poll(coports,ncoports,timeout));

    err(ENOSYS,"copoll: copoll on non-cocarrier coports not yet implemented");
    for(;;sched_yield())
    {
        for(int i = 0;i<ncoports;i++)
        {
            switch(coport_gettype(coports[i].coport))
            {
                case COPIPE:
                    break;
                case COCHANNEL:
                    break;
                default:
                    break;
            }
        }
    }
    
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

    int mib[4];
    int cores;
    size_t len = sizeof(cores); 

    /* set the mib for hw.ncpu */
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;

    /* get the number of CPUs from the system */
    sysctl(mib, 2, &cores, &len, NULL, 0);
    if (cores>1)
        multicore=1;
    #if 1
    multicore=0;
    #endif
}
