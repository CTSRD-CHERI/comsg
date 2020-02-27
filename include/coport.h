#ifndef _COPORT_H
#define _COPORT_H

#include <cheri/cheric.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <stdatomic.h>

#include "comutex.h"
#include "sys_comsg.h"


#define COPORT_MMAP_FLAGS (MAP_ANONYMOUS | MAP_SHARED | MAP_ALIGNED_CHERI)
#define COPORT_MMAP_PROT (PROT_READ | PROT_WRITE)

/* 
	COCHANNEL -	simple data buffer with pipe-like semantics
	COCARRIER -	each message is copied to its own buffer to which the recipient 
				can obtain a read-only capability. sender loses write capability
				to the sent message
	COPIPE	  -	synchronous message passing ipc. as yet unimplemented.
				might be a blocking one with no buffer using call into ukern
				on both sides to copy straight from dest to source	
*/
typedef enum {COSEND, CORECV} coport_op_t;
typedef enum {COCHANNEL, COCARRIER, COPIPE} coport_type_t;
typedef enum {COPORT_CLOSED=-1,COPORT_OPEN=0,COPORT_BUSY=1,COPORT_READY=2,COPORT_DONE=3} coport_status_t;

/* 
 * TODO-PBB: Rework so we can have fine-grained, least-privilege protection of 
 * struct members, specifically (referring just to the members and not their 
 * contents):
 *  - BUFFER: load cap but not store;
 *  - TYPE,LENGTH: load data only;
 *  - START,END,STATUS: store/load, but not caps;
 *  - LOCK: undetermined. likely load cap.
 */
typedef struct _coport_t
{
	void * __capability buffer;
	u_int length;
	u_int start;
	u_int end;
	_Atomic coport_status_t status;
	coport_type_t type;
	comutex_t read_lock;
	comutex_t write_lock;
} coport_t;




#endif