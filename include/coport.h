#ifndef _COPORT_H
#define _COPORT_H

#include <cheri/cheric.h>
#include <sys/mman.h>
#include <sys/param.h>

#include "comutex.h"
#include "sys_comsg.h"
	
#define COPORT_OPEN 0
#define COPORT_READY 1
#define COPORT_BUSY 2
#define COPORT_CLOSED -1


#define COPORT_MMAP_FLAGS (MAP_ANONYMOUS | MAP_SHARED | MAP_ALIGNED_CHERI)
#define COPORT_MMAP_PROT (PROT_READ | PROT_WRITE)

/* 
	COCHANNEL -	simple data buffer with pipe-like semantics
	COCARRIER -	each message is copied to its own buffer to which the recipient 
				can obtain a read-only capability. sender loses write capability
				to the sent message
	COPIPE	  -	synchronous message passing ipc. as yet unimplemented.
*/

typedef enum {COCHANNEL, COCARRIER, COPIPE} coport_type_t;

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
	void * buffer;
	u_int length;
	u_int start;
	u_int end;
	u_int status;
	coport_type_t type;
	comutex_t lock;
} coport_t;

#endif