#ifndef _COPROC_H
#define _COPROC_H

#include "comutex.h"
#include "coport.h"
#include "sys_comsg.h"

typedef struct _cocall_lookup_t
{
	char target[LOOKUP_STRING_LEN];
	void * __capability cap;
} cocall_lookup_t;

typedef struct _coopen_args_t
{
	char name[COPORT_NAME_LEN];
	coport_type_t type;
} coopen_args_t;

typedef struct _cocall_coopen_t
{
	coopen_args_t args;
	coport_t * __capability port; 
} cocall_coopen_t;

typedef struct _comutex_init_args_t
{
	char name[COMUTEX_NAME_LEN];
} cocall_comutex_init_args_t;

typedef struct _cocall_comutex_init_t
{
	cocall_comutex_init_args_t args;
	comutex_t * mutex; 
} cocall_comutex_init_t;

typedef struct _colock_args_t
{
	comutex_t * mutex;
	int result;
} colock_args_t;
typedef struct _colock_args_t counlock_args_t;


int ukern_lookup(void *  *code, 
	void *  *data, const char * target_name, 
	void *  *target_cap);

#endif