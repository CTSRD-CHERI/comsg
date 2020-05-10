#ifndef _COPORT_H
#define _COPORT_H

#include <sys/queue.h>
#include <cheri/cheric.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <stdatomic.h>
#include <pthread.h>

#include "sys_comsg.h"


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
typedef enum {COPIPE, COCARRIER, COCHANNEL} coport_type_t;
typedef enum {COPORT_CLOSED=0,COPORT_OPEN=1,COPORT_BUSY=2,COPORT_READY=4,COPORT_DONE=8} coport_status_t;
typedef enum {NOEVENT=0,COPOLL_CLOSED=1,COPOLL_IN=2,COPOLL_OUT=4,COPOLL_READ_ERROR=8,COPOLL_WRITE_ERROR=16} coport_eventmask_t;
typedef enum {RECV=1,SEND=2,CREAT=4,EXCL=8,ONEWAY=16,} coport_flags_t; //currently unimplemented

#define COCARRIER_PERMS ( CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP )


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
    u_int length;
    u_int start;
    u_int end;
    _Atomic coport_status_t status;
    coport_type_t type;
    union 
    {
        struct 
        {
            coport_eventmask_t event;
            LIST_HEAD(,_coport_listener) listeners;
        };
    };
};

typedef struct _coport sys_coport_t;
typedef struct _coport *coport_t;

#endif