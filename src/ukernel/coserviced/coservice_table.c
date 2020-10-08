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

#include <coproc/coservice.h>

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <unistd.h>

static const int coservice_table_prot = ( PROT_READ | PROT_WRTE );
static const int coservice_table_flags = ( MAP_ANON | MAP_SHARED | MAP_STACK | MAP_ALIGNED_CHERI );
static const int coservice_table_len = 1024 * 1024;

static size_t max_services;

static struct {
	coservice_t *services;
	_Atomic size_t active_services;
	_Atomic size_t next_service;
} coservice_table;

__attribute__ ((constructor)) static 
void setup_table(void)
{
	coservice_table.services = mmap(NULL, coservice_table_len, coservice_table_prot, coservice_table_flags, -1, 0);
	max_services = cheri_getlen(coservice_table.services) / sizeof(struct coservice_t);
	coservice_table.next_service = max_services - 1;
	coservice_table.active_services = 0UL;
}

coservice_t *allocate_coservice(void)
{
	coservice_t *ptr;
	size_t index = atomic_fetch_sub(&coservice_table.next_service, 1);
	
	ptr = &coservice_table.services[index];
	ptr = cheri_setboundsexact(ptr, sizeof(coservice_t));
	memset(ptr, 0, sizeof(coservice_t));

	atomic_fetch_add(&coservice_table.active_services, 1);

	return (ptr);
}

void *get_coservice_scb(coservice_t *service)
{
	int new_index;
	int index = atomic_load(&service->next_worker);
	for(;;)
	{
		new_index = (index + 1) % service->nworkers;
		atomic_compare_exchange_weak(&service->next_worker, &index, new_index);
	}
	return (service->worker_scbs[new_index]);
}

int in_table(coservice_t *ptr)
{
	vaddr_t addr = cheri_getaddress(ptr);
	return (cheri_is_address_inbounds(coservice_table.services, addr));
}
