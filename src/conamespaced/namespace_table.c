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

#include "ns_limits.h"

#include "ukern/ccmalloc.h"
#include "ukern/namespace.h"
#include "ukern/namespace_object.h"
#include "ukern/utils.h"

#include <assert.h>
#include <cheri/cheric.h>
#include <err.h>
#include <errno.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <unistd.h>

/* Constants */
static const int namespace_table_prot = ( PROT_READ | PROT_WRTE );
static const int namespace_table_flags = ( MAP_ANON | MAP_SHARED | MAP_STACK | MAP_ALIGNED_SUPER );

#if MACHINE == "mips"
#define ALLOC_UNIT PDRSIZE
#else 
#define ALLOC_UNIT ( 1024 * 1024 ) 
#endif

static size_t namespace_table_len = ALLOC_UNIT; //should get from OS and relate to process limits
static size_t max_namespaces;
static size_t max_nsobjects;

static namespace_t *global_namespace = NULL;

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
	/* Add space to ensure we can map a process namespace for every process in the system */
	int maxprocs = get_maxprocs();
	namespace_table_len += (((maxprocs * sizeof(namespace_t)) / ALLOC_UNIT) + 1) * ALLOC_UNIT;
	/* Allocate namespace table */
	namespace_table.namespaces = mmap(NULL, namespace_table_len, namespace_table_prot, namespace_table_flags, -1, 0);
	max_namespaces = cheri_getlen(namespace_table.namespaces) / sizeof(struct namespace_t);
	namespace_table.next_namespace = max_namespaces - 1;
	namespace_table.namespace_count = 0lu;
	/* Allocate namespace object table */
	nsobject_table.nsobjects = mmap(NULL, namespace_table_len, namespace_table_prot, namespace_table_flags, -1, 0);
	max_nsobjects = cheri_getlen(nsobject_table.nsobjects) / sizeof(struct nsobject_t);
	nsobject_table.next_nsobject = max_nsobjects - 1;
	nsobject_table.nsobject_count = 0lu;

	/* Determine limits - not fully used yet.*/
	if(max_namespaces < maxprocs)
		warn("setup_namespace_table: table allocation too small! (kern.maxproc=%d, max_namespaces=%d)", maxproc, max_namespaces);
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
	}
	else {
		index = atomic_fetch_sub(&namespace_table.next_namespace, 1);
		if(index == 0) {
			warn("new_namespace_entry: namespace table exhausted. further attempts to create namespaces will fail.");
			full = 1;
		}
	}
	
	ptr = &namespace_table.namespaces[index];
	ptr = cheri_setboundsexact(ptr, sizeof(namespace_t));
	memset(ptr, 0, sizeof(namespace_t));

	ptr->members = cocall_calloc(1, sizeof(struct _ns_members));

	atomic_fetch_add(&namespace_table.namespace_count, 1);

	return (ptr);
}

static __inline namespace_t*
new_nsobject_entry(void)
{
	nsobject_t *ptr;
	size_t index;

	if (full) {
		warn("new_nsobject_entry: nsobject table exhausted");
		return (NULL);
	}
	else {
		index = atomic_fetch_sub(&nsobject_table.next_nsobject, 1);
		if(index == 0) {
			warn("new_nsobject_entry: nsobject table exhausted. further attempts to create namespaces will fail.");
			full = 1;
		}
	}
	
	ptr = &nsobject_table.next_nsobject[index];
	ptr = cheri_setboundsexact(ptr, sizeof(nsobject_t));
	memset(ptr, 0, sizeof(nsobject_t));

	atomic_fetch_add(&nsobject_table.nsobject_count, 1);

	return (ptr);
}

nsobject_t *allocate_nsobject(namespace_t *parent)
{
	struct _member *obj_cap;
	assert(NS_PERMITS_WRITE(parent));

	obj_cap = cocall_malloc(sizeof(struct _member));
	obj_cap->nsobj = new_nsobject_entry();
	parent = unseal_ns(parent);
	LIST_INSERT_HEAD(&parent->members.objects, obj_cap, entries);

	return (obj_cap->nsobj);
}

namespace_t *allocate_namespace(namespace_t *parent, nstype_t type)
{
	struct _member *obj_cap;
	if(type == GLOBAL) {
		assert(parent == NULL);
		assert(global_namespace == NULL);
		global_namespace = new_namespace_entry();
		return (global_namespace);
	}
	else {
		assert(NS_PERMITS_WRITE(parent));
	}
	obj_cap = cocall_malloc(sizeof(struct _member));
	obj_cap->ns = new_namespace_entry();
	parent = unseal_ns(parent);
	LIST_INSERT_HEAD(&parent->members.namespaces, obj_cap, entries);

	return (obj_cap->ns);
}

int
is_global_namespace(namespace_t *ns_cap)
{
	assert(global_namespace != NULL);
	return (global_namespace == ns_cap);
}

void *get_global_namespace(void)
{
	assert(global_namespace != NULL);
	return (global_namespace);
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