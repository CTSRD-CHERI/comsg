#include "comutex.h"

#include <machine/atomic.h>

//TODO-PBB: find out how to do this for read
#define atomic_cap_cmpset(ptr,old,new)

int co_trylock(comutex_t * mtx)
{
	if(cheri_getsealed(mtx->lock))
	{
		//already locked :(
		return 1;
	}
	else
	{
		void * __capability key;
		if atomic_cap_cmpset(&mtx->key,NULL,key)
		{
			//we did it!
			mtx->lock=cheri_seal(mtx->lock,mtx->key);
			return 0;
		}
		else
		{
			//beaten to it
			return 2;
		}
	}
}

int comutex_init(comutex_t * mtx)
{
	mtx->key=NULL;
	/* call into ukernel to create shared place mtx->lock can live */
}

int colock(comutex_t * mtx)
{
	if(co_trylock(mtx)==0)
	{
		return 0;
	}
	else
	{
		return colock(mtx);
	}
}