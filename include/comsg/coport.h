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
#ifndef _COPORT_H
#define _COPORT_H

#include <sys/queue.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>
#include <sys/param.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>

#include <comsg/namespace.h>

#ifndef COPORT_NAME_LEN
#ifdef NS_NAME_LEN
#define COPORT_NAME_LEN NS_NAME_LEN
#else // NS_NAME_LEN
#define COPORT_NAME_LEN 255
#endif
#endif 

#define COPORT_PERM_RECV (CHERI_PERM_SW2)
#define COPORT_PERM_SEND (CHERI_PERM_SW3)

/* 
    COCHANNEL - simple data buffer with pipe-like semantics
    COCARRIER - each message is copied to its own buffer to which the recipient 
                can obtain a read-only capability. sender loses write capability
                to the sent message
    COPIPE    - direct copy ipc.whereby recipients publish a capability for the
                sender to copy data into
*/
typedef enum {COPORT_OP_COSEND = 0, COPORT_OP_CORECV = 1, COPORT_OP_POLL = 2} coport_op_t;
typedef enum {INVALID_COPORT = 0, COPIPE = 1, COCARRIER = 2, COCHANNEL = 3} coport_type_t;
typedef enum {COPORT_CLOSED = 0, COPORT_OPEN = 1, COPORT_BUSY = 2, COPORT_READY = 4, COPORT_DONE = 8, COPORT_CLOSING = 16, COPORT_POLLING = 32} coport_status_t;
typedef enum {NOEVENT = 0, COPOLL_CLOSED = 1, COPOLL_IN = 2, COPOLL_OUT = 4, COPOLL_RERR = 8, COPOLL_WERR = 16} coport_eventmask_t;
typedef enum {RECV = 1, SEND = 2, CREAT = 4, EXCL = 8, ONEWAY = 16} coport_flags_t; //currently unimplemented


#define COPOLL_INIT_EVENTS ( COPOLL_OUT )

#define COPORT_INFO_PERMS ( CHERI_PERM_STORE | CHERI_PERM_LOAD )
#if !defined(__riscv)
#define COPORT_PERMS_ARCH_SPECIFIC ( CHERI_PERM_MUTABLE_LOAD )
#else
#define COPORT_PERMS_ARCH_SPECIFIC ( 0 )
#endif
#define COPORT_PERMS ( CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP |\
    COPORT_PERM_SEND | COPORT_PERM_RECV | CHERI_PERM_INVOKE | COPORT_PERMS_ARCH_SPECIFIC )

#define COPORT_LOAD_CAP_BUFFER_PERMS ( CHERI_PERM_LOAD_CAP | CHERI_PERM_LOAD | COPORT_PERMS_ARCH_SPECIFIC )
#define COCARRIER_MSG_PERMS (CHERI_PERM_LOAD | CHERI_PERM_GLOBAL)
#define COCARRIER_OOB_PERMS ( COPORT_LOAD_CAP_BUFFER_PERMS )
#define DEFAULT_BUFFER_PERMS ( COPORT_LOAD_CAP_BUFFER_PERMS | CHERI_PERM_GLOBAL )
#define COPIPE_BUFFER_PERMS ( COPORT_LOAD_CAP_BUFFER_PERMS |\
    CHERI_PERM_STORE | CHERI_PERM_STORE_CAP | CHERI_PERM_GLOBAL )
#define COPIPE_RECVBUF_PERMS ( CHERI_PERM_STORE | CHERI_PERM_GLOBAL )
#define COCARRIER_BUF_PERMS ( CHERI_PERM_STORE | CHERI_PERM_STORE_CAP | COPORT_LOAD_CAP_BUFFER_PERMS | CHERI_PERM_GLOBAL )
#define COCHANNEL_BUF_PERMS ( CHERI_PERM_STORE | CHERI_PERM_LOAD | CHERI_PERM_GLOBAL )

#define COPORT_INBUF_PERMS (CHERI_PERM_LOAD | CHERI_PERM_GLOBAL)
#define COPORT_OUTBUF_PERMS (CHERI_PERM_STORE | CHERI_PERM_GLOBAL)


//TODO-PBB: better definition of these
#define COCARRIER_MAX_MSG_LEN (1024 * 1024 * 2)
#define COPORT_BUF_LEN (4096)

#define COCARRIER_SIZE (COPORT_BUF_LEN / CHERICAP_SIZE)

typedef struct __no_subobject_bounds _coport_listener {
    LIST_ENTRY(_coport_listener) entries;
    _Atomic bool removed;
    pthread_cond_t *wakeup;
    coport_eventmask_t events; 
    coport_eventmask_t revent;
} coport_listener_t;

typedef union {
    struct {
        LIST_HEAD(, _coport_listener) listeners;
        coport_eventmask_t levent; /* bitwise or of listener events */
    };  /* COCARRIER */
} coport_typedep_t;

typedef struct {
    _Atomic size_t length; 
    _Atomic size_t start;
    _Atomic size_t end;
    _Atomic coport_status_t status;
    _Atomic coport_eventmask_t event;
} coport_info_t; //bad name :c and too many atomics (I think)

typedef struct {
    void *buf;
} coport_buf_t;

struct _coport {
    coport_info_t *info; //Read and Write data only
    coport_type_t type; 
    coport_buf_t *buffer;  //Permissions vary on type
    coport_typedep_t *cd; //Read and Write + R/W Caps
}; //Pointer to whole struct has Load + Load caps, but no store

typedef struct _coport coport_t; 

#endif