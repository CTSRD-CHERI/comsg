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
#include "namespace_table.h"
#include "nsd_limits.h"
#include "nsd_cap.h"

#include <comsg/namespace.h>
#include <comsg/namespace_object.h>
#include <comsg/utils.h>

#include <assert.h>
#include <cheri/cheric.h>
#include <err.h>
#include <sys/errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <unistd.h>

extern void begin_cocall();
extern void end_cocall();

static size_t max_namespaces = 0;
static size_t max_nsobjects = 0;

static namespace_t *root_namespace = NULL;

static struct {
	namespace_t *namespaces;
	_Atomic size_t namespace_count;
	_Atomic size_t next_namespace;
} namespace_table;

static struct {
	nsobject_t *nsobjects;
	_Atomic size_t nsobject_count;
	_Atomic size_t next_nsobject;
} nsobject_table;

__attribute__ ((constructor)) static 
void setup_namespace_table(void)
{
	madvise(NULL, -1, MADV_PROTECT);
	/* Add space to ensure we can map a process namespace for every process in the system */
	int maxprocs = get_maxprocs() < 1024 ? 1024 : get_maxprocs();
	/* Allocate namespace table */
	namespace_table.namespaces = calloc(maxprocs, sizeof(namespace_t));
	if (namespace_table.namespaces == NULL)
		err(EX_SOFTWARE, "%s: calloc for namespace table failed", __func__);
	max_namespaces = cheri_getlen(namespace_table.namespaces) / sizeof(namespace_t);
	namespace_table.next_namespace = 0lu;
	namespace_table.namespace_count = 0lu;
	/* Allocate namespace object table */
	nsobject_table.nsobjects = calloc(maxprocs * 2, sizeof(nsobject_t));
	if (nsobject_table.nsobjects == NULL)
		err(EX_SOFTWARE, "%s: calloc for nsobject table failed", __func__);
	max_nsobjects = cheri_getlen(nsobject_table.nsobjects) / sizeof(nsobject_t);
	nsobject_table.next_nsobject = 0lu;
	nsobject_table.nsobject_count = 0lu;

	/* Determine limits - not fully used yet.*/
	if(max_namespaces < maxprocs)
		warn("setup_namespace_table: table allocation too small! (kern.maxproc=%d, max_namespaces=%lu)", maxprocs, max_namespaces);
}

static __inline namespace_t*
new_namespace_entry(void)
{
	static int full = 0;
	namespace_t *ptr;
	size_t index;

	if (full) {
		warn("new_namespace_entry: namespace table exhausted");
		return (NULL);
	} else {
		index = atomic_fetch_add(&namespace_table.next_namespace, 1);
		if(index >= max_namespaces) {
			warn("new_namespace_entry: namespace table exhausted. further attempts to create namespaces will fail.");
			full = 1;
		}
	}
	
	ptr = &namespace_table.namespaces[index];
	ptr = cheri_setboundsexact(ptr, sizeof(namespace_t));
	memset(ptr, 0, sizeof(namespace_t));

	begin_cocall();
	ptr->members = calloc(1, sizeof(struct _ns_members));
	end_cocall();

	atomic_fetch_add(&namespace_table.namespace_count, 1);

	return (ptr);
}

static __inline nsobject_t *
new_nsobject_entry(void)
{
	static int full = 0;
	nsobject_t *ptr;
	size_t index;

	if (full) {
		warn("new_nsobject_entry: nsobject table exhausted");
		return (NULL);
	}
	else {
		index = atomic_fetch_add(&nsobject_table.next_nsobject, 1);
		if(index >= max_nsobjects) {
			warn("new_nsobject_entry: nsobject table exhausted. further attempts to create namespaces will fail.");
			full = 1;
		}
	}
	
	ptr = &nsobject_table.nsobjects[index];
	ptr = cheri_setboundsexact(ptr, sizeof(nsobject_t));
	memset(ptr, 0, sizeof(nsobject_t));

	atomic_fetch_add(&nsobject_table.nsobject_count, 1);

	return (ptr);
}

nsobject_t *
allocate_nsobject(namespace_t *parent)
{
	struct _ns_member *obj_cap;
	assert(NS_PERMITS_WRITE(parent));

	begin_cocall();
	obj_cap = malloc(sizeof(struct _ns_member));
	obj_cap->nsobj = new_nsobject_entry();
	end_cocall();
	parent = unseal_ns(parent);
	LIST_INSERT_HEAD(&parent->members->objects, obj_cap, entries);
	parent->members->nobjects++;

	return (obj_cap->nsobj);
}

namespace_t *
allocate_namespace(namespace_t *parent, nstype_t type)
{
	struct _ns_member *obj_cap;
	if (type == ROOT) {
		assert(parent == NULL);
		assert(root_namespace == NULL);
		root_namespace = new_namespace_entry();
		return (root_namespace);
	} else {
		assert(NS_PERMITS_WRITE(parent));
	}
	begin_cocall();
	obj_cap = malloc(sizeof(struct _ns_member));
	obj_cap->ns = new_namespace_entry();
	end_cocall();
	parent = unseal_ns(parent);
	LIST_INSERT_HEAD(&parent->members->namespaces, obj_cap, entries);
	parent->members->nspaces++;

	return (obj_cap->ns);
}

int
is_root_namespace(namespace_t *ns_cap)
{
	assert(root_namespace != NULL);
	return (root_namespace == ns_cap);
}

void *get_root_namespace(void)
{
	assert(root_namespace != NULL);
	return (root_namespace);
}

int in_ns_table(namespace_t *ptr)
{
	vaddr_t addr = cheri_getaddress(ptr);
	return (cheri_is_address_inbounds(namespace_table.namespaces, addr));
}

int in_nsobject_table(nsobject_t *ptr)
{
	vaddr_t addr = cheri_getaddress(ptr);
	return (cheri_is_address_inbounds(nsobject_table.nsobjects, addr));
}

void nsobject_deleted(void)
{
	atomic_fetch_sub(&nsobject_table.nsobject_count, 1);
	return;
}

void namespace_deleted(void)
{
	atomic_fetch_sub(&namespace_table.namespace_count, 1);
	return;
}