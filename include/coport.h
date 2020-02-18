#ifndef _COPORT_H
#define _COPORT_H

#include <cheric.h>
#include <sys/mman.h>
#include <sys/param.h>

#include "comutex.h"
#include "sys_comsg.h"
	
#define COPORT_OPEN 0
#define COPORT_READY 0x1
#define COPORT_BUSY 0x2 
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
typedef enum _coport_type_t {COCHANNEL, COCARRIER,COPIPE} coport_type_t;

typedef struct _coopen_args_t
{
	char name[COPORT_NAME_LEN];
	coport_type_t type;
} coopen_args_t;

typedef struct _coport_mutex_t
{
	int lock;
} coport_mutex_t;


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
	u_int status;
	coport_type_t type;
	comutex_t lock;
} coport_t;

typedef struct _cocall_coopen_t
{
	coopen_args_t args;
	coport_t * __capability port; 
} cocall_coopen_t;

#endif