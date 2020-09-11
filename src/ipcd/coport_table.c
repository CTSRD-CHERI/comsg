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

#include "ukern/coport.h"

#include <stdatomic.h>
#include <sys/mman.h>
#include <sys/queue.h>

typedef struct {
	coport_t port;
	size_t ref_count;
} coport_tbl_entry_t;

static struct _coport_table {
	_Atomic size_t next_coport;
	_Atomic size_t ncoports;
	size_t first_coport;
	coport_tbl_entry_t *coports;
} cocarrier_table, copipe_table, cochannel_table;

static size_t max_coports = 256;
static size_t coport_table_len = max_coports * sizeof(coport_tbl_entry_t);
static int coport_table_prot = (PROT_READ | PROT_WRITE);
static int coport_table_flags = (MAP_ANON | MAP_SHARED | MAP_STACK | MAP_ALIGNED_CHERI);

__attribute__((constructor)) static
void setup_table(void) 
{
	size_t top_of_table;

	cocarrier_table.coports = mmap(NULL, coport_table_len, coport_table_prot, coport_table_flags, -1, 0);
	top_of_table = (cheri_getlen(cocarrier_table.coports) / sizeof(struct coport_t)) - 1;
	cocarrier_table.next_coport = top_of_table;
	cocarrier_table.first_coport = top_of_table;
	cocarrier_table.ncoports = 0;

	copipe_table.coports = mmap(NULL, coport_table_len, coport_table_prot, coport_table_flags, -1, 0);
	top_of_table = (cheri_getlen(copipe_table.coports) / sizeof(struct coport_t)) - 1;
	copipe_table.next_coport = top_of_table;
	copipe_table.first_coport = top_of_table;
	copipe_table.ncoports = 0;

	cochannel_table.coports = mmap(NULL, coport_table_len, coport_table_prot, coport_table_flags, -1, 0);
	top_of_table = (cheri_getlen(cochannel_table.coports) / sizeof(struct coport_t)) - 1;
	cochannel_table.next_coport = top_of_table;
	cochannel_table.first_coport = top_of_table;
	cochannel_table.ncoports = 0;
}

static
struct _coport_table *get_coport_table(coport_type_t type)
{
	struct _coport_table *table;
	switch (type)
	{
		case COCARRIER:
			table = &cocarrier_table;
			break;
		case COPIPE:
			table = &copipe_table;
			break;
		case COCHANNEL:
			table = &cochannel_table;
			break;
	}
	return (table);
}

coport_t *allocate_coport(coport_type_t type)
{
	coport_t *ptr;

	struct _coport_table *coport_table = get_coport_table(type);
	size_t index = atomic_fetch_sub(&coport_table->next_coport, 1);

	ptr = &coport_table->coports[index];
	ptr = cheri_setbounds_exact(ptr, sizeof(coport_t));
	memset(ptr, 0, sizeof(coport_t));

	atomic_fetch_add(&coport_table->ncoports, 1);
	return (ptr);
}

int in_coport_table(coport_t *ptr, coport_type_t type)
{
	struct _coport_table *coport_table = get_coport_table(type);
	vaddr_t addr = cheri_getaddress(ptr);
	return (cheri_is_address_inbounds(coport_table->coports, addr));
}

int can_allocate_coport(coport_type_t type)
{
	struct _coport_table *coport_table = get_coport_table(type);
	if(coport_table->next_coport == 0)
		return (0);
	else
		return (1);
}

/*
 * Used for copoll delivery. Walks a vertical slice of the cocarrier table
 * and returns a NULL-terminated array of those cocarriers with threads 
 * listening for events.
 */
coport_t **
get_cocarrier_events(size_t mod, size_t r)
{
	coport_t **cocarriers, *cocarrier;
	size_t idx, end, start, expected_max_len, i;
	
	idx = 0;
	end = atomic_load(cocarrier_table.next_coport)
	start = cocarrier_table.first_coport;
	expected_max_len = ((start - end) / mod) + 1;

	cocarriers = calloc(expected_max_len, CHERICAP_SIZE);
	for (i = (start - r); i > end; i-=mod) {
		assert((start - i) % mod == r); /*TODO-PBB: remove this sanity check */
		cocarrier = &cocarrier_table.coports[i].port;
		if (cocarrier->info->status == COPORT_CLOSED)
			continue;
		else if (LIST_EMPTY(&cocarrier->cd->listeners))
			continue;
		cocarriers[++idx] = cocarrier;
	}
	if (idx != 0) {
		cocarriers[idx] = NULL;
		return (cocarriers);
	} else {
		free(cocarriers);
		return (NULL);
	}
}