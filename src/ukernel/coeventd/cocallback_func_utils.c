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
#include "cocallback_func_utils.h"

#include <coproc/coevent.h>

#include <cheri/cheric.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>

static pid_t mypid;

static struct {
	cocallback_func_t *funcs;
	_Atomic size_t idx;
} ccb_func_tbl;

const size_t base_tbl_len = 4096; 
static size_t tbl_len = base_tbl_len; //arbitrary
static pthread_mutex_t table_alloc_lock;

static cocallback_func_t *allocate_ccb_func(void);
static void extend_ccb_table(void);

static cocallback_func_t *provider_death = NULL;

static cocallback_func_t *
allocate_ccb_func(void)
{
	cocallback_func_t *func;
	size_t func_index;
	int error;

	do {
		func_index = atomic_fetch_add_explicit(&ccb_func_tbl.idx, 1, memory_order_acq_rel);
		if (func_index >= tbl_len) {
			extend_ccb_table();
			continue;
		}
	} while (0);
	func = &ccb_func_tbl.funcs[func_index];
	func = __builtin_cheri_bounds_set(func, sizeof(cocallback_func_t));

	return (func);
}

static void
extend_ccb_table(void)
{
	size_t func_index;
	int error;
	void *new_alloc;

	error = pthread_mutex_lock(&table_alloc_lock);
	if (error != 0)
		err(errno, "%s: error acquiring ccb table lock", __func__);
	/* check that the allocation is still needed */
	func_index = atomic_load_explicit(&ccb_func_tbl.idx, memory_order_acquire);
	if (func_index < tbl_len) { 
		/* it's not needed, another thread got their first */
		pthread_mutex_unlock(&table_alloc_lock);
		return;
	}
	ccb_func_tbl.funcs = realloc(ccb_func_tbl.funcs, (tbl_len + base_tbl_len) * sizeof(cocallback_func_t));
	memset(ccb_func_tbl.funcs + tbl_len, '\0', base_tbl_len * sizeof(cocallback_func_t));
	tbl_len += base_tbl_len;
	pthread_mutex_unlock(&table_alloc_lock);
}

void
init_cocallback_func_utils(void)
{
	mypid = getpid();

	ccb_func_tbl.funcs = calloc(base_tbl_len, sizeof(cocallback_func_t));
	atomic_store_explicit(&ccb_func_tbl.idx, 0, memory_order_release);
	provider_death = register_cocallback_func(mypid, NULL, FLAG_PROVIDER);
}


int 
flag_dead_provider(cocallback_t *provider_death)
{
	cocallback_func_t *func;
	
	SLIST_FOREACH(func, &provider_death->args.provided_funcs, next) {
		func->flags |= FLAG_DEAD;
	};

	return (0);
}

cocallback_func_t *
get_provdeath_func(void)
{
	/* internal provider death function - provider death has the same behaviour for all subjects */
	return (provider_death);
}

cocallback_func_t *
register_cocallback_func(pid_t provider, void *scb, cocallback_flags_t flags)
{
	cocallback_func_t *func;
	size_t func_index;

	if ((provider != mypid) && (cheri_gettag(scb) != 1))
		return (NULL); /* external callbacks must have a valid cocall target */

	func = allocate_ccb_func();
	func->provider = provider;
	func->scb = scb;
	func->flags = flags;
	func->consumers = 0;

	return (func);
}

int
validate_cocallback_func(cocallback_func_t *func)
{
	if (!cheri_is_address_inbounds(ccb_func_tbl.funcs, (vaddr_t)func))
		return (0);
	else if (__builtin_cheri_tag_get(func) == 0)
		return (0);
	else
		return (1);
}