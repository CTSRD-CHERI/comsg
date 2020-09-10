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

typedef struct {
	coport_t port;
	size_t ref_count;
} coport_tbl_entry_t;

static struct {
	_Atomic size_t next_coport;
	_Atomic size_t ncoports;
	coport_tbl_entry_t *coports;
} coport_table;

static size_t max_coports = 255;
static size_t coport_table_len = max_coports * sizeof(coport_tbl_entry_t);
static int coport_table_prot = ( PROT_READ | PROT_WRITE );
static int coport_table_flags = ( MAP_ANON | MAP_SHARED | MAP_STACK | MAP_ALIGNED_CHERI );

__attribute__((constructor)) static
void setup_table(void) 
{
	coport_table.coports = mmap(NULL, coport_table_len, coport_table_prot, coport_table_flags, -1, 0);
	max_coports = cheri_getlen(coport_table.coports) / sizeof(struct coport_t);
	coport_table.next_coport = max_coports - 1;
	coport_table.ncoports = 0;
}

coport_t *allocate_coport(void)
{
	coport_t *ptr;
	size_t index = atomic_fetch_sub(&coport_table.next_coport, 1);

	ptr = &coport_table.coports[index];
	ptr = cheri_setbounds_exact(ptr, sizeof(coport_t));
	memset(ptr, 0, sizeof(coport_t));

	atomic_fetch_add(&coport_table.ncoports, 1);
	return (ptr);
}

int in_coport_table(coport_t *ptr)
{
	vaddr_t addr = cheri_getaddress(ptr);
	return (cheri_is_address_inbounds(coport_table.coports, addr));
}

int can_allocate_coport(void)
{
	if(coport_table.next_coport == 0)
		return (0);
	else
		return (1);
}

