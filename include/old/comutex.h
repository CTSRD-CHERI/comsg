#ifndef _COMUTEX_H
#define _COMUTEX_H

#include <stdatomic.h>

#include "sys_comsg.h"

#define COMUTEX_LOCKED 1
#define COMUTEX_UNLOCKED 0

/*
 * PBB: comutexes
 * mutexes for colocated processes using cheri capabilities
 * lock and check_lock:
 * 		lock and check_lock refer to the same place in memory
 * 		lock allows writing, and is used for acquiring/releasing
 * 		check_lock is read only and is used for inspecting
 * key:
 * -	this is intended to be local to the holder, but can be shared if desired
 * -	used to seal lock on acquisition to prevent mutex from being acquired by
 *  	another process
 * name:
 * -	optional field for named mutexes
*/

// The natural next step with these is to have them act as wrappers around
// more interesting things than ints, allowing shared objects to be passed 
// between processes.

typedef struct _comtx_t
{
	_Atomic int * lock;
	_Atomic int * check_lock;
} comtx_t;

typedef struct _comutex_t
{
	comtx_t * mtx;
	_Atomic(void * __capability) key;
	char * name;
} comutex_t;

int counlock(comutex_t * mtx);
int colock(comutex_t * mtx, _Atomic(void * __capability) key);
int comutex_init(char * mtx_name, comutex_t * mutex);

#endif