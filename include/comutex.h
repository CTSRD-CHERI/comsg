#ifndef _COMUTEX_H
#define _COMUTEX_H

#include <stdatomic.h>

#include "sys_comsg.h"
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
	atomic_int * __capability lock;
	atomic_int * __capability check_lock;
} comtx_t;

typedef struct _comutex_t
{
	comutex_t * __capability mtx;
	char * name;
	void * __capability key;
} comutex_t;

int comutex_init(comutex_t * mtx);
int colock(comutex_t * mtx);
int comutex_init(char * mtx_name, comutex_t * mutex);

#endif