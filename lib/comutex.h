#ifndef _COMUTEX_H
#define _COMUTEX_H

typedef struct _comutex_t
{
	 void * __capability key;
	_Atomic void * __capability __capability lock;
} comutex_t;



int comutex_init(comutex_t * mtx);
int colock(comutex_t * mtx);
int counlock(comutex_t * mtx);

#endif