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
#include "coservice_table.h"


#include <comsg/coservice.h>

#include <cheri/cheric.h>
#include <err.h>
#include <sys/errno.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <unistd.h>

extern void begin_cocall();
extern void end_cocall();

static const int coservice_table_prot = ( PROT_READ | PROT_WRITE );
static const int coservice_table_flags = ( MAP_ANON | MAP_SHARED | MAP_STACK | MAP_ALIGNED_CHERI );
static const int coservice_table_len = 1024 * 1024;

static size_t max_services = 256;

static struct {
	struct _coservice_endpoint *endpoints;
	_Atomic size_t active_endpoints;
	_Atomic size_t next_endpoint;
} endpoint_table;

static struct {
	coservice_t *services;
	_Atomic size_t active_services;
	_Atomic size_t next_service;
} coservice_table;

__attribute__ ((constructor)) static 
void setup_table(void)
{
	madvise(NULL, -1, MADV_PROTECT);
	coservice_table.services = calloc(max_services, sizeof(coservice_t));
	if (coservice_table.services == NULL)
		err(errno, "setup_table: allocating coservice table failed");
	coservice_table.next_service = 0UL;
	coservice_table.active_services = 0UL;

	endpoint_table.endpoints = calloc(max_services, sizeof(struct _coservice_endpoint));
	if (endpoint_table.endpoints == NULL)
		err(errno, "setup_table: allocating coservice table failed");
	endpoint_table.next_endpoint = 0UL;
	endpoint_table.active_endpoints = 0UL;
}

coservice_t *
allocate_coservice(void)
{
	coservice_t *ptr;
	size_t index = atomic_fetch_add(&coservice_table.next_service, 1);
	
	if (index >= max_services) {
		atomic_fetch_sub(&coservice_table.next_service, 1);
		return (NULL);
	}
	ptr = &coservice_table.services[index];
	ptr = cheri_setboundsexact(ptr, sizeof(coservice_t));
	memset(ptr, '\0', sizeof(coservice_t));

	atomic_fetch_add(&coservice_table.active_services, 1);

	return (ptr);
}

struct _coservice_endpoint *
allocate_endpoint(void)
{
	struct _coservice_endpoint *ptr;
	size_t index = atomic_fetch_add(&endpoint_table.next_endpoint, 1);
	
	if (index >= max_services)
		return (NULL);
	ptr = &endpoint_table.endpoints[index];
	ptr = cheri_setboundsexact(ptr, sizeof(struct _coservice_endpoint));
	memset(ptr, '\0', sizeof(struct _coservice_endpoint));

	atomic_fetch_add(&endpoint_table.active_endpoints, 1);

	return (ptr);
}

/*
void *
get_coservice_scb(coservice_t *service)
{
	int new_index, index, nworkers;

	nworkers = service->nworkers;
	index = atomic_load_explicit(&service->next_worker, memory_order_acquire);
	do {
		new_index = (index + 1) % nworkers;
	} while(atomic_compare_exchange_strong_explicit(&service->next_worker, &index, new_index, memory_order_acq_rel, memory_order_acquire) == 0);
	
	return (service->worker_scbs[new_index]);
}
*/

int 
in_table(coservice_t *ptr)
{
	vaddr_t addr = cheri_getaddress(ptr);
	return (cheri_is_address_inbounds(coservice_table.services, addr));
}
