#include <stdlib.h>
#include <stdatomic.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>

#include <string.h>
#include <err.h>

#include "comutex.h"
#include "sys_comutex.h"


#define LOCK_PERMS (CHERI_PERM_LOAD|CHERI_PERM_STORE)
#define CHECK_LOCK_PERMS (CHERI_PERM_LOAD)
#define KEY_PERMS (CHERI_PERM_SEAL|CHERI_PERM_UNSEAL)
#define MTX_PERMS (CHERI_PERM_LOAD|CHERI_PERM_LOAD_CAP)



//XXX-PBB: Thinking about changing these, hence the defines.
#define LOCK_SUCC memory_order_seq_cst
#define LOCK_FAIL memory_order_seq_cst
#define UNLOCK_SUCC memory_order_er

#define ATOMIC_CAS(a,b,c) \
	atomic_compare_exchange_weak_explicit(a,b,c,LOCK_SUCC,LOCK_FAIL)
#define ATOMIC_CAS_LOCK(a,b,c) \
	atomic_compare_exchange_weak_explicit(a,b,c,LOCK_SUCC,LOCK_FAIL)
#define ATOMIC_CAS_UNLOCK(a,b,c) \
	atomic_compare_exchange_weak_explicit(a,b,c,LOCK_SUCC,LOCK_FAIL)

__inline int cmtx_cmp(comutex_t * a,comutex_t * b)
{
	int result=0;
	if(cheri_getaddress(a->mtx->lock)!=cheri_getaddress(b->mtx->lock))
	{
		result++;
	}
	if(cheri_getaddress(a->mtx->check_lock)!=cheri_getaddress(b->mtx->check_lock))
	{
		result++;
	}
	return result;
}

__inline int cmtx_validate(comutex_t * a)
{
	// return 1 (true) if a the capabilities in a match
	int result=1;
	if(cheri_getaddress(a->mtx->lock)!=cheri_getaddress(a->mtx->check_lock))
	{
		result=0;
	}
	if((cheri_getperm(a->mtx->lock)&LOCK_PERMS)==0)
	{
		result=0;
	}
	if((cheri_getperm(a->mtx->check_lock)&CHECK_LOCK_PERMS)==0)
	{
		result=0;
	}
	if((cheri_getperm(a->key)&KEY_PERMS)==0)
	{
		result=0;
	}
	return result;
}

int sys_cotrylock(sys_comutex_t * mutex, void * key)
{
	comtx_t * mtx;

	mtx=mutex->kern_mtx;
	if(atomic_load(mtx->check_lock)==COMUTEX_LOCKED)
	{
		return 1;
	}
	else if (ATOMIC_CAS(mtx->lock,COMUTEX_UNLOCKED,COMUTEX_LOCKED))
	{
		if(*mtx->check_lock!=COMUTEX_LOCKED)
		{
			err(4,"atomic cas on mtx->lock did not propagate");
		}
		mutex->key=key;
		mtx->lock=cheri_seal(mtx->lock,key);
		return 0;
	}
	return 2;
}

int sys_colock(sys_comutex_t * mutex,void * key)
{
	//TODO-PBB: convert to sleep with signal?
	while(sys_cotrylock(mutex,key)==0)
	{
		//spin
	}
	return 0;
}

int sys_counlock(sys_comutex_t * mutex,void * key)
{
	comtx_t * mtx;
	atomic_int * unlocked;
	atomic_int * locked;

	mtx=mutex->kern_mtx;
	if(atomic_load(mtx->check_lock)==COMUTEX_UNLOCKED)
	{
		return 1;
	}
	else
	{
		locked=mtx->lock;
		unlocked=cheri_unseal(mtx->lock,key);

		mtx->lock=unlocked;
		atomic_store(mtx->lock,COMUTEX_UNLOCKED);
		if(*mtx->check_lock!=COMUTEX_UNLOCKED)
		{
			err(3,"cas to mtx->lock via unlocked did not propagate");
		}
		return 0;
		
	}
}

int sys_comutex_init(char * name, sys_comutex_t * m)
{
	comtx_t * mtx;
	atomic_int * val;

	val=(_Atomic int *)malloc(sizeof(int));
	*val=COMUTEX_UNLOCKED;
	mtx=(comtx_t *)malloc(sizeof(comtx_t));

	mtx->lock=cheri_andperm(val,LOCK_PERMS);
	mtx->check_lock=cheri_andperm(val,CHECK_LOCK_PERMS);
	m->user_mtx=cheri_andperm(mtx,MTX_PERMS);
	m->kern_mtx=mtx;

	m->key=NULL;
	strcpy(m->name,name);
	return 0;
}


