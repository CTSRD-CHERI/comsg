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
#include <sys/errno.h>
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

#include <comsg/ukern_calls.h>

#define COPIPE_BUF_PERMS (CHERI_PERM_STORE | CHERI_PERM_STORE_CAP | CHERI_PERM_GLOBAL )
#define COPORT_IPC_OTYPE 1
//Sealing root
static void * __capability libcomsg_sealroot;
//Sealing cap for coport_t
static otype_t libcomsg_coport_seal;
static long libcomsg_otype;
static long cocarrier_otype = 0;
static long copipe_otype = 0;
static long cochannel_otype = 0;
static long multicore = 0;

typedef struct _port_tbl_entry {
    coport_t port;
    int wired;
} pentry_t;

struct _ports_tbl {
    int index;
    pentry_t entries[10];
};

static namespace_t *global_ns = NULL;
static nsobject_t *cosend_obj = NULL;
static nsobject_t *corecv_obj = NULL;

static void
cocarrier_preload(void)
{
    if (cosend_obj != NULL || corecv_obj != NULL)
        return;
    else if (global_ns == NULL)
        global_ns = coproc_init(NULL, NULL, NULL, NULL);

    cosend_obj = coselect(U_COSEND, COSERVICE, global_ns);
    corecv_obj = coselect(U_CORECV, COSERVICE, global_ns);
}

static void
process_coport_handle(coport_t *port, coport_type_t type)
{
    switch (type) {
    case COPIPE:
        copipe_otype = cheri_gettype(port);
        break;
    case COCHANNEL:
        cochannel_otype = cheri_gettype(port);
        break;
    case COCARRIER:
        cocarrier_otype = cheri_gettype(port);
        cocarrier_preload();
        break;
    default:
        err(ENOSYS, "coopen: ipcd returned unknown coport type");
        break; /* NOTREACHED */
    }
} 

nsobject_t *open_named_coport(const char *coport_name, coport_type_t type)
{
    coport_t *port;
    ns_object_t *port_obj, *cosend_obj, *corecv_obj;

    port = coopen(type);
    port_obj = coinsert(coport_name, COPORT, port, ns);

    port = port_obj->coport;
    process_coport_handle(port, type);

    return (port_obj);
}

coport_t *open_coport(coport_type_t type)
{
    coport_t *port;

    port = coopen(type);
    process_coport_handle(port, type);
    return (port);
}

static coport_status_t 
acquire_coport_status(coport_t *port, coport_status_t expected, coport_status_t desired)
{
    coport_status_t status_val;

    status_val = expected;
    for(;;) {
        if(atomic_compare_exchange_strong_explicit(&port->info->status, &status_val, desired, memory_order_acq_rel, memory_order_relaxed))
            break;
        switch (status_val) {
        case COPORT_CLOSING:
        case COPORT_CLOSED:
            return (status_val);
        default:
            status_val = expected;
            break;
        }
        if(multicore == 0 || (i % 30) == 0) 
            sched_yield(); 
        i++;
    }
    return (status_val);
}

static void
release_coport_status(coport_t *port, coport_status_t desired)
{
    atomic_store_explicit(&port->info->status, desired, memory_order_release);
}

static 
int copipe_send(const coport_t *port, const void *buf, size_t len)
{
    void *out_buffer;
    acquire_coport_status(port, COPORT_READY, COPORT_BUSY);
    
    out_buffer = port->buffer->buf;
    if(!cheri_gettag(out_buffer)) {
        release_coport_status(port, COPORT_DONE);
        errno = EPROT;
        return (-1);
    }
    if (cheri_getlen(out_buffer) < len) {
        release_coport_status(port, COPORT_READY);
        errno = EMSGSIZE;
        return (-1);
    }
    memcpy(out_buffer, buf, len);
    port->info->length = len;
    release_coport_status(port, COPORT_DONE);
    return (len);
}

static 
int cochannel_send(const coport_t *port, const void *buf, size_t len)
{
    size_t port_size, port_start, old_end, new_end, new_len;
    char *port_buffer, *msg_buffer;
    coport_eventmask_t event;

    port_size = port->info->length;
    new_len = len + port_size;
    if(new_len > COPORT_BUF_LEN) {
        errno = EWOULDBLOCK;
        return (-1);
    }

    if(acquire_coport_status(port, COPORT_OPEN, COPORT_BUSY) & (COPORT_CLOSING | COPORT_CLOSED) != 0)
        return (-1);

    event = port->info->event;
    port_start = port->info->start;
    old_end = port->info->end;

    if ((event & COPOLL_OUT) == 0) {
        release_coport_status(port, COPORT_OPEN);
        errno = EAGAIN;
        return (-1);
    }
    port_buffer = port->buffer->buf;
    msg_buffer = buf;
    
    new_end = (old_end + len) % COPORT_BUF_LEN;
    if (old_end + len > COPORT_BUF_LEN) {
        memcpy(&port_buffer[old_end], msg_buffer, COPORT_BUF_LEN - old_end);
        memcpy(port_buffer, &msg_buffer[COPORT_BUF_LEN-old_end], new_end);
    }
    else
        memcpy(&port_buffer[old_end], msg_buffer, len);
    port->info->end = new_end;
    port->info->length = new_len;
    
    event |= COPOLL_IN;
    if (new_len == COPORT_BUF_LEN) 
        event &= ~COPOLL_OUT;
    port->info->event = event;

    release_coport_status(port, COPORT_OPEN);

    return (len);
}


int coport_send(const coport_t *prt, const void *buf, size_t len)
{
    int retval;
    coport_type_t type;
  
    if(len == 0)
        return (0);

    type = coport_gettype(port);
    switch(type) {
    case COCHANNEL:
        retval = cochannel_send(prt, buf, len);
        break;
    case COCARRIER:
        retval = cocarrier_send(prt, buf, len);
        break;
    case COPIPE:
        retavl = copipe_send(prt, buf, len);
        break;
    default:
        errno = EINVAL;
        break;
    }
    return (retval);
}

static int
cochannel_corecv(const coport_t *port, void *buf, size_t len)
{
    char *out_buffer, *port_buffer;
    size_t port_size, old_start, port_end, new_start, len_to_end, new_len;
    coport_eventmask_t event;

    port_size = port->info->length;
    if (port_size < len) {
        err(EMSGSIZE, "corecv: expected message length %lu larger than contents of buffer", len);
        return (-1);
    }
    new_len = port_size - len;
    if(acquire_coport_status(port, COPORT_OPEN, COPORT_BUSY) & (COPORT_CLOSING | COPORT_CLOSED) != 0)
        return (-1);
    
    event = port->info->event;
    if ((event & COPOLL_IN) == 0) {
        /* UNREACHED */
        release_coport_status(port, COPORT_OPEN);
        errno = EAGAIN;
        return (-1);
    }
    out_buffer = buf;
    port_buffer = port->buffer->buf;
    old_start = port->info->start;
    new_start = (old_start + len) % COPORT_BUF_LEN;
    
    if (old_start + len > port_size) {
        len_to_end = COPORT_BUF_LEN - old_start;
        memcpy(out_buffer, &port_buffer[old_start], len_to_end);
        memcpy(out_buffer[len_to_end], port_buffer, new_start);
    }
    else
        memcpy(buf, &port_buffer[old_start], len);
    port->info->length = new_len;
    port->info->start = new_start;

    event |= COPOLL_OUT;
    if (new_len == 0)
        event &= ~COPOLL_IN;
    port->info->event = event;

    release_coport_status(port, COPORT_OPEN);
    return (len);
}

static int
copipe_corecv(const coport_t *port, void *buf, size_t len)
{
    size_t received_len;

    if (cheri_getlen(buf) < len)
        buf = cheri_setbounds(buf, len);
    buf = cheri_andperm(buf, COPIPE_BUF_PERMS);

    acquire_coport_status(port, COPORT_OPEN, COPORT_BUSY);

    port->buffer->buf = buf;

    release_coport_status(port, COPORT_READY);
    acquire_coport_status(port, COPORT_DONE, COPORT_BUSY);

    port->buffer->buf = NULL;
    received_len = port->info->length;

    release_coport_status(port, COPORT_OPEN);

    return (cheri_getlen(buf));
}

int corecv(const coport_t *port, void **buf, size_t len)
{
    int retval;
    coport_type_t type;

    if(len == 0)
        return (0);
    type = coport_gettype(port);
    switch(type) {
    case COCHANNEL:
        retval = cochannel_corecv(port, *buf, len);
        break;
    case COCARRIER:
        retval = cocarrier_recv(port, buf, len);
        break;
    case COPIPE:
        retval = copipe_corecv(port, *buf, len);
        break;
    default:
        err(1, "corecv: invalid coport type");
        break;
    }
    return (retval);
}


coport_type_t coport_gettype(coport_t port)
{
    if(!cheri_getsealed(port))
        return (INVALID);
    if (cheri_gettype(port) != libcomsg_otype) {
        //XXX-PBB:this is bad. 
        //if both are true we don't reach this. 
        //we give sealed stuff a pass if we don't know the cocarrier_otype yet.
        if(cocarrier_otype == -1 || cheri_gettype(port) == cocarrier_otype)
            return (COCARRIER);
    } else {
        port = cheri_unseal(port, libcomsg_sealroot);
        return (port->type);
    }
    return (INVALID);
}

pollcoport_t make_pollcoport(coport_t port, int events)
{
    pollcoport_t pcpt;

    pcpt.coport = port;
    pcpt.events = events;
    pcpt.revents = 0;

    return (pcpt);
}

int coclose(coport_t port)
{
    //TODO-PBB: IMPLEMENT
    //if we possess the CLOSE permission, we can unilaterally close the port for all users.
    //otherwise, this operation decrements the ukernel refcount for the port
    port = NULL;
    return (0);
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
    call = calloc(1, sizeof(copoll_args_t));
    //allocate our own for safety purposes
    call_coports = calloc(ncoports, sizeof(pollcoport_t));
    memcpy(call_coports, coports, sizeof(pollcoport_t)*ncoports);
    
    call->coports = call_coports;
    call->ncoports = ncoports;
    call->timeout = timeout;
    /* cocall setup */
    //TODO-PBB: Only do this once.

    //possible deferred call to malloc ends up in cocall, causing exceptions?
    error = ukern_lookup(&switcher_code, &switcher_data, U_COPOLL, &func);
    if (error) {
        free(call_coports);
        free(call);
        return (-1);
    }
    error = cocall(switcher_code, switcher_data, func, call, sizeof(cocall_coopen_t));
    assert(error == 0);
    errno = call->error;
    status = call->status;
    if (status != 0) {
        for (int i = 0; i < ncoports; ++i)
            coports[i].revents = call->coports[i].revents;
    }
    free(call_coports);
    free(call);

    return (status);
}

int copoll(pollcoport_t * coports, int ncoports, int timeout)
{
    assert(ncoports>0);
    assert(cheri_gettag(coports));

    for(int i = 1; i < ncoports; i++)
        assert((coport_gettype(coports[i].coport) == coport_gettype(coports[i-1].coport))||(coport_gettype(coports[i].coport) != COCARRIER && (coport_gettype(coports[i-1].coport) != COCARRIER)));

    if(coport_gettype(coports[0].coport) == COCARRIER)
        return(cocarrier_poll(coports, ncoports, timeout));

    err(ENOSYS, "copoll: copoll on non-cocarrier coports not yet implemented");
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
    if (sysarch(CHERI_GET_SEALCAP, &libcomsg_sealroot) < 0) {
        libcomsg_sealroot = NULL;
    }
    assert((cheri_gettag(libcomsg_sealroot) != 0));    
    assert(cheri_getlen(libcomsg_sealroot) != 0);
    assert((cheri_getperm(libcomsg_sealroot) & CHERI_PERM_SEAL) != 0);
    //XXX-PBB: Is 1 an okay value?
    libcomsg_coport_seal = cheri_maketype(libcomsg_sealroot, LIBCOMSG_OTYPE);
    libcomsg_otype = cheri_gettype(cheri_seal(libcomsg_coport_seal, libcomsg_coport_seal));
    libcomsg_sealroot = cheri_setoffset(libcomsg_sealroot, LIBCOMSG_OTYPE);

    int mib[4];
    int cores;
    size_t len = sizeof(cores); 

    /* set the mib for hw.ncpu */
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;

    /* get the number of CPUs from the system */
    sysctl(mib, 2, &cores, &len, NULL, 0);
    if (cores>1)
        multicore = 1;

}