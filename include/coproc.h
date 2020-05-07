#ifndef _COPROC_H
#define _COPROC_H

#include <cheri/cheri.h>
#include <time.h>

#include "comutex.h"
#include "coport.h"
#include "sys_comsg.h"

//NOTE: I think the _Atomic annotations are unneccessary,
//but as things work I have yet to remove them.
//TODO - Rework so all arg structs follow a standard template including:
// 		 status, error, args, and return


typedef struct _cocall_lookup_t
{
	char target[LOOKUP_STRING_LEN];
	void * __capability cap;
} __attribute__((__aligned__(16))) cocall_lookup_t;

typedef struct _coopen_args_t
{
	coport_type_t type;
	char name[COPORT_NAME_LEN];
} coopen_args_t;

typedef struct _cocall_coopen_t
{
	coopen_args_t args;
	_Atomic(coport_t) port; 
	int status;
	int error;
} __attribute__((__aligned__(16))) cocall_coopen_t;

typedef struct _comutex_init_args_t
{
	char name[COMUTEX_NAME_LEN];
} cocall_comutex_init_args_t;

typedef struct _cocall_comutex_init_t
{
	cocall_comutex_init_args_t args;
	_Atomic(comutex_t * __capability) mutex; 
} __attribute__((__aligned__(16))) cocall_comutex_init_t;

typedef struct _colock_args_t
{
	_Atomic(comutex_t * __capability) mutex;
	int result;
} __attribute__((__aligned__(16))) colock_args_t;
typedef struct _colock_args_t counlock_args_t;

typedef struct _cocarrier_send_args_t
{
	_Atomic(coport_t) cocarrier;
	void * __capability message;
	int status;
	int error;
} __attribute__((__aligned__(16))) cocall_cocarrier_send_t;


typedef struct _pollcoport_t
{
	coport_t coport;
	int events;
	int revents;
} pollcoport_t;

typedef struct _copoll_args_t
{
	pollcoport_t * coports;
	int ncoports;
	int timeout; 
	int status;
	int error;
} __attribute__((__aligned__(16))) copoll_args_t;


int ukern_lookup(void *  __capability * __capability code, 
	void * __capability  * __capability data, const char * target_name, 
	void * __capability * __capability target_cap);

#endif