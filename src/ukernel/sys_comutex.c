/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdatomic.h>
#include <cheri/cheric.h>

#include <cheri/cherireg.h>

#include <string.h>
#include <err.h>

#include "comutex.h"
#include "ukern_mman.h"
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
	atomic_compare_exchange_weak_explicit(a,&b,c,LOCK_SUCC,LOCK_FAIL)
#define ATOMIC_CAS_LOCK(a,b,c) \
	atomic_compare_exchange_weak_explicit(a,&b,c,LOCK_SUCC,LOCK_FAIL)
#define ATOMIC_CAS_UNLOCK(a,b,c) \
	atomic_compare_exchange_weak_explicit(a,&b,c,LOCK_SUCC,LOCK_FAIL)

int LOCKED = COMUTEX_LOCKED;
int UNLOCKED = COMUTEX_UNLOCKED;

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
	if(atomic_load(mtx->check_lock)==LOCKED)
	{
		return 1;
	}
	else if (ATOMIC_CAS(mtx->lock,UNLOCKED,LOCKED))
	{
		if(*mtx->check_lock!=LOCKED)
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
		__asm("nop");
	}
	return 0;
}

int sys_counlock(sys_comutex_t * mutex,void * key)
{
	comtx_t * mtx;
	atomic_int * unlocked;
	atomic_int * locked;

	mtx=mutex->kern_mtx;
	if(atomic_load(mtx->check_lock)==UNLOCKED)
	{
		return 1;
	}
	else
	{
		locked=mtx->lock;
		unlocked=cheri_unseal(mtx->lock,key);

		mtx->lock=unlocked;
		atomic_store(mtx->lock,UNLOCKED);
		if(*mtx->check_lock!=UNLOCKED)
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

	val=(_Atomic int *)ukern_malloc(sizeof(int));
	*val=COMUTEX_UNLOCKED;
	mtx=(comtx_t *)ukern_malloc(sizeof(comtx_t));

	mtx->lock=cheri_andperm(val,LOCK_PERMS);
	mtx->check_lock=cheri_andperm(val,CHECK_LOCK_PERMS);
	m->user_mtx=cheri_andperm(mtx,MTX_PERMS);
	m->kern_mtx=mtx;

	m->key=NULL;
	strcpy(m->name,name);
	return 0;
}


