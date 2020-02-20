#ifndef _COPROC_H
#define _COPROC_H

#include "comutex.h"
#include "coport.h"

typedef struct _cocall_lookup_t
{
	char func_name[PATH_MAX];
	void * __capability target;
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
	comutex_t * __capability mutex; 
} cocall_comutex_init_t;

typedef struct _colock_args_t
{
	comutex_t * mutex;
	int result;
} colock_args_t;
typedef struct _colock_args_t counlock_args_t;


int ukern_lookup(void * __capability __capability code, 
	void * __capability __capability data, const char * target_name, 
	void * __capability __capability target_cap);

#endif