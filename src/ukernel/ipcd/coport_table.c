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
#include "coport_table.h"
#include "copoll_deliver.h"
#include <comsg/coport.h>

#include <assert.h>
#include <cheri/cheric.h>
#include <err.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <unistd.h>

typedef struct {
	coport_t port;
	_Atomic size_t ref_count;
} coport_tbl_entry_t;

static struct _coport_table {
	_Atomic size_t next_coport;
	_Atomic size_t ncoports;
	size_t first_coport;
	coport_tbl_entry_t *coports;
} cocarrier_table, copipe_table, cochannel_table;

/* TODO-PBB: convert to sysctl or similar configurable value */
static const size_t max_coports = 256;
static size_t coport_table_len = max_coports * sizeof(coport_tbl_entry_t);

__attribute__((constructor)) static void 
setup_table(void) 
{
	madvise(NULL, -1, MADV_PROTECT);

	cocarrier_table.coports = calloc(max_coports, sizeof(coport_tbl_entry_t));
	cocarrier_table.next_coport = 0;
	cocarrier_table.first_coport = 0;
	cocarrier_table.ncoports = 0;

	copipe_table.coports = calloc(max_coports, sizeof(coport_tbl_entry_t));
	copipe_table.next_coport = 0;
	copipe_table.first_coport = 0;
	copipe_table.ncoports = 0;

	cochannel_table.coports = calloc(max_coports, sizeof(coport_tbl_entry_t));
	cochannel_table.next_coport = 0;
	cochannel_table.first_coport = 0;
	cochannel_table.ncoports = 0;
}

static struct _coport_table *
get_coport_table(coport_type_t type)
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
		default:
			table = NULL;
			break;
	}
	return (table);
}

coport_t *
allocate_coport(coport_type_t type)
{
	struct _coport_table *coport_table;
	coport_t *ptr;
	size_t index;

	coport_table = get_coport_table(type);
	index = atomic_fetch_add(&coport_table->next_coport, 1);

	ptr = &coport_table->coports[index].port;
	ptr = cheri_setboundsexact(ptr, sizeof(coport_t));
	memset(ptr, 0, sizeof(coport_t));

	atomic_fetch_add(&coport_table->ncoports, 1);
	atomic_store_explicit(&coport_table->coports[index].ref_count, 1, memory_order_release);
	return (ptr);
}

int 
in_coport_table(coport_t *ptr, coport_type_t type)
{
	struct _coport_table *coport_table = get_coport_table(type);
	vaddr_t addr = cheri_getaddress(ptr);
	return (cheri_is_address_inbounds(coport_table->coports, addr));
}

int
can_allocate_coport(coport_type_t type)
{
	struct _coport_table *coport_table = get_coport_table(type);
	if(coport_table->ncoports == max_coports)
		return (0);
	else
		return (1);
}

int 
get_coport_notifier_index(coport_t *coport)
{
	struct _coport_table *coport_table;
	size_t idx, end, start;

	coport_table = get_coport_table(coport->type);
	start = coport_table->first_coport;
	idx = (cheri_getaddress(&coport_table->coports[start]) - cheri_getaddress(coport)) / sizeof(coport_t);
	idx = idx % n_copoll_notifiers;

	return (idx);
}

