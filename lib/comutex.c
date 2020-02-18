#include "comutex.h"

int comutex_init(comutex_t * mtx)
{
	*mtx->lock=0;
	mtx->key=&mtx->lock;
}

int colock(comutex_t * mtx)
{
	/* whole section must be atomic */
}