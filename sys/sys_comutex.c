#include "comutex.h"

#include <stdatomic.h>


//XXX-PBB: Thinking about changing these, hence the defines.
#define lock_succ memory_order_seq_cst
#define lock_fail memory_order_seq_cst

#define atomic_cas(a,b,c) \
	atomic_compare_exchange_weak_explicit(a,b,c,lock_succ,lock_fail)

int sys_co_trylock(comutex_t * mtx)
{
	if(cheri_getsealed(mtx->lock))
	{
		//already locked :(
		return 1;
	}
	else
	{
		void * __capability key;
		if atomic_cas(mtx->lock,NULL,new)
		{
			//we did it!
			mtx->key=key;
			mtx->lock=cheri_seal(mtx->lock,mtx->key);
			return 0;
		}
		else
		{
			//beaten to it
			//included in case we want to think about contention detection
			return 2;
		}
	}
}

int _comutex_init(comutex_t * mtx)
{
	mtx->key=NULL;
	/* call into ukernel to create shared place mtx->lock can live */
}
