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
#ifndef _COPORT_H
#define _COPORT_H

#include <sys/queue.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <stdatomic.h>
#include <pthread.h>

#include "sys_comsg.h"

#ifndef COPORT_NAME_LEN
#define COPORT_NAME_LEN 255
#endif

#define COPORT_PERM_RECV CHERI_PERM_SW2
#define COPORT_PERM_SEND CHERI_PERM_SW3

#define COPORT_MMAP_FLAGS (MAP_ANONYMOUS | MAP_SHARED | MAP_ALIGNED_CHERI)
#define COPORT_MMAP_PROT (PROT_READ | PROT_WRITE)

/* 
    COCHANNEL - simple data buffer with pipe-like semantics
    COCARRIER - each message is copied to its own buffer to which the recipient 
                can obtain a read-only capability. sender loses write capability
                to the sent message
    COPIPE    - direct copy ipc.whereby recipients publish a capability for the
                sender to copy data into
*/
typedef enum {COSEND, CORECV} coport_op_t;
typedef enum {INVALID=0,COPIPE=1, COCARRIER=2, COCHANNEL=3} coport_type_t;
typedef enum {COPORT_CLOSED=0,COPORT_OPEN=1,COPORT_BUSY=2,COPORT_READY=4,COPORT_DONE=8} coport_status_t;
typedef enum {NOEVENT=0,COPOLL_CLOSED=1,COPOLL_IN=2,COPOLL_OUT=4,COPOLL_RERR=8,COPOLL_WERR=16} coport_eventmask_t;
typedef enum {RECV=1,SEND=2,CREAT=4,EXCL=8,ONEWAY=16,} coport_flags_t; //currently unimplemented

#define COCARRIER_PERMS ( CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP )
#define COPOLL_INIT_EVENTS ( COPOLL_OUT )

/* 
 * TODO-PBB: Rework so we can have fine-grained, least-privilege protection of 
 * struct members, specifically (referring just to the members and not their 
 * contents):
 *  - BUFFER: load cap but not store;
 *  - TYPE,LENGTH: load data only;
 *  - START,END,STATUS: store/load, but not caps;
 */
/*
typedef struct _coport_t
{
    void * __capability buffer;
    
    struct {
        u_int start;
        u_int end;
        _Atomic coport_status_t status;
        u_int length;
    } *;
    coport_type_t type;
} sys_coport_t;
*/


typedef struct __no_subobject_bounds _coport_listener 
{
    LIST_ENTRY(_coport_listener) entries;
    pthread_cond_t wakeup;
    coport_eventmask_t eventmask; 
    int revent;
} coport_listener_t;


struct _coport
{
    void * __capability buffer;
    _Atomic size_t length;
    _Atomic size_t start;
    _Atomic size_t end;
    _Atomic coport_status_t status;
    coport_type_t type;
    union //used to allow other coport_type dependent stuff later if we want it.
    {
        struct 
        {
            _Atomic coport_eventmask_t event;
            LIST_HEAD(,_coport_listener) listeners;
        };
    };
};

typedef struct _coport sys_coport_t;
typedef struct _coport *coport_t;

typedef struct _named_coport {
    char name[COPORT_NAME_LEN];
    coport_t port;
} named_coport_t;

#endif