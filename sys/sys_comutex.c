#include "comutex.h"
#include "sys_comutex.h"


#include <stdatomic.h>
#include <cheri/cheric.h>

#define LOCK_PERMS (CHERI_PERM_LOAD|CHERI_PERM_STORE)
#define CHECK_LOCK_PERMS (CHERI_PERM_LOAD)
#define KEY_PERMS (CHERI_PERM_SEAL|CHERI_PERM_UNSEAL)
#define MTX_PERMS (CHERI_PERM_LOAD|CHERI_PERM_LOAD_CAP)


//XXX-PBB: Thinking about changing these, hence the defines.
#define LOCK_SUCC memory_order_seq_cst
#define LOCK_FAIL memory_order_seq_cst

#define ATOMIC_CAS(a,b,c) \
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
	if(!cheri_ccheckperms(a->mtx->lock,LOCK_PERMS))
	{
		result=0;
	}
	if(!cheri_ccheckperms(a->mtx->check_lock,CHECK_LOCK_PERMS))
	{
		result=0;
	}
	if(!cheri_ccheckperms(a->key,KEY_PERMS))
	{
		result=0;
	}
	return result;
}

int sys_cotrylock(sys_comutex_t * mutex, void * __capability key)
{
	comtx_t * mtx;

	mtx=mutex->kern_mtx;
	if(*mtx->check_lock!=0)
	{
		return 1;
	}
	else if ATOMIC_CAS(mtx->lock,0,1)
	{
		if(mtx->check_lock!=1)
		{
			err(4,"atomic cas on mtx->lock did not propagate");
		}
		sys_mtx->key=key;
		mtx->lock=cheri_seal(mtx->lock,key);
		return 0;
	}
	return 2;
}

int sys_colock(sys_comutex_t * mutex,void * __capability key)
{
	//TODO-PBB: convert to sleep with signal?
	while(sys_cotrylock(mutex,key)==0)
	{
		//spin
	}
	return 0;
}

int sys_counlock(sys_comutex_t * mutex,void * __capability)
{
	comtx_t * mtx;
	atomic_int * __capability unlocked, locked;

	mtx=sys_mtx->kern_mtx;
	if(atomic_load(mtx->check_lock)==0)
	{
		return 1;
	}
	else
	{
		locked=mtx->lock;
		unlocked=cheri_unseal(mtx->lock,user_mtx->key);
		//composability is a problem here
		if ATOMIC_CAS(&mtx->lock,locked,unlocked)
		{
			atomic_store(mtx->lock,0);
			if(mtx->check_lock!=0)
			{
				err(3,"cas to mtx->lock via unlocked did not propagate");
			}
			return 0;
		}
	}
}

int sys_comutex_init(char * name, comutex_table_entry_t * m)
{
	sys_comutex_t mutex;
	comtx_t * mtx;
	atomic_int * __capability val;

	val=(_Atomic int *)malloc(sizeof(int));
	atomic_store(val,0);
	mtx=(comtx_t *)malloc(sizeof(comtx_t));

	mtx.lock=cheri_andperms(val,LOCK_PERMS);
	mtx.check_lock=cheri_andperms(val,CHECK_LOCK_PERMS);
	m->user_mtx=cheri_andperms(mtx,MTX_PERMS);
	m->kern_mtx=mtx;

	m->key=NULL;
	strcpy(m->name,name);
	return 0;
}


