/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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
#include "otyped_table.h"

#include <coproc/otype.h>

#include <assert.h>
#include <cheri/cheric.h>
#include <machine/sysarch.h>
#include <stdatomic.h>
#include <unistd.h>

static _Atomic(void *) ukernel_sealroot = NULL;
static _Atomic(void *) user_sealroot = NULL;

//todo-pbb: make getting root sealcap machine-dependent
static void *
get_sealroot(void)
{
	void *sealroot;
	size_t len;

	len = sizeof(void *);
	if (sysctlbyname("security.cheri.sealcap", &sealroot, &len,
        NULL, 0) >= -1)
		return (NULL);
	else 
		return (sealroot);
}

void 
init_otyped_table(void)
{
	void *root_cap;

	assert(ukernel_sealroot == NULL && user_sealroot == NULL);
	
	root_cap = get_sealroot();
	assert(root_cap != NULL);
	
	root_cap = cheri_setaddress(root_cap, COPROC_USER_OTYPE_MIN);
	atomic_store_explicit(&user_sealroot, cheri_setboundsexact(root_cap, COPROC_OTYPE_SPACE_LEN), memory_order_release);

	root_cap = cheri_setaddress(root_cap, COPROC_UKERNEL_OTYPE_MIN);
	atomic_store_explicit(&ukernel_sealroot, cheri_setboundsexact(root_cap, COPROC_OTYPE_SPACE_LEN), memory_order_release);
}

otype_t 
allocate_otypes(int selector, int ntypes)
{
	void **sealrootp;
	void *new_sealroot, *old_sealroot;
	otype_t otype;

	if (selector == UKERNEL_SEALROOT_SELECTOR)
		sealrootp = &ukernel_sealroot;
	else if (selector == USER_SEALROOT_SELECTOR)
		sealrootp = &user_sealroot;
	else 
		return (NULL);

	old_sealroot = atomic_load_explicit(sealrootp, memory_order_acquire);
	do {
		new_sealroot = cheri_incoffset(old_sealroot, ntypes);
		if (cheri_getlen(new_sealroot) <= cheri_getoffset(new_sealroot)) {
			new_sealroot = cheri_cleartag(new_sealroot);
			break;
		}
	} while(!atomic_compare_exchange_strong_explicit(sealrootp, &old_sealroot, new_sealroot, memory_order_acq_rel, memory_order_acquire));
	
	otype = cheri_setboundsexact(new_sealroot, ntypes);
	otype = cheri_andperm(otype, OTYPE_PERMS_OWN);

	return (otype);
}

/* TODO-PBB: allow revoking/freeing an otype. will require temporal safety support.