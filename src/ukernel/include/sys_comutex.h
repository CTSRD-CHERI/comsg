#ifndef _SYS_COMUTEX_H
#define _SYS_COMUTEX_H

#include "comutex.h"
#include "sys_comsg.h"

typedef struct _sys_comutex_t
{
	comtx_t * user_mtx;
	comtx_t * kern_mtx;
	char name[COMUTEX_NAME_LEN];
	void * key;
} sys_comutex_t;

__inline int cmtx_cmp(comutex_t * a,comutex_t * b);
__inline int cmtx_validate(comutex_t * a);

int sys_cotrylock(sys_comutex_t * mutex, void * key);
int sys_colock(sys_comutex_t * mutex,void * key);
int sys_counlock(sys_comutex_t * mutex,void * key);
int sys_comutex_init(char * name, sys_comutex_t * m);

#endif