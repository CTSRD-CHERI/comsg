#ifndef _SYS_COMUTEX_H
#define _SYS_COMUTEX_H

#include "comutex.h"
#include "sys_comsg.h"

typedef struct _sys_comutex_t
{
	comtx_t * __capability user_mtx;
	comtx_t * __capability kern_mtx;
	char name[COMUTEX_NAME_LEN];
	void * __capability key;
} sys_comutex_t;


#endif