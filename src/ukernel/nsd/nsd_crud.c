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

#include "nsd_crud.h"
#include "nsd_cap.h"
#include "nsd_lookup.h"
#include "namespace_table.h"

#include <ccmalloc.h>
#include <comsg/ukern_calls.h>
#include <comsg/namespace.h>
#include <comsg/namespace_object.h>
#include <comsg/utils.h>

#include <assert.h>
#include <cheri/cheric.h>
#include <err.h>
#include <sys/errno.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <unistd.h>

static __inline void 
validate_nscreate_params(namespace_t *parent, nstype_t type, const char *name)
{
	assert(valid_ns_name(name));
	assert(VALID_NS_TYPE(type));
	if(type != ROOT) {
		valid_namespace_cap(parent);
		assert(NS_PERMITS_WRITE(parent));
		parent = unseal_ns(parent);
	}

	switch(type)
	{
		case ROOT:
			assert(parent == NULL);
			break;
		case APPLICATION:
		case LIBRARY:
			assert(parent->type == ROOT);
			assert(is_root_namespace(parent));
			break;
		case PRIVATE:
		case PUBLIC:
			break;
		default:
			err(EINVAL, "new_namespace: invalid type supplied");
			break;
	}
}

static __inline void 
validate_nsobjcreate_params(const char *name, nsobject_type_t type, namespace_t *parent)
{
	assert(valid_namespace_cap(parent));

	assert(valid_nsobj_name(name));
	assert(VALID_NSOBJ_TYPE(type));
	assert(NS_PERMITS_WRITE(parent));
	
	assert(lookup_nsobject(name, type, parent) == NULL);
}

static __inline void
init_ns_members(namespace_t *ns_cap)
{
	struct _ns_members *target = ns_cap->members;
	
	LIST_INIT(&target->objects);
	target->nobjects = 0;
	target->max_objects = -1;

	LIST_INIT(&target->namespaces);
	target->nspaces = 0;
	target->max_namespaces = -1;
}

namespace_t *new_namespace(const char *name, nstype_t type, namespace_t *parent)
{
	namespace_t *ns_ptr;
	namespace_t *parent_cap;

	/* Absolutely should not ever get to this point with invalid parameters */
	validate_nscreate_params(parent, type, name);

	ns_ptr = allocate_namespace(parent, type);
	if (ns_ptr == NULL)
		return (NULL);

	ns_ptr->type = type;
	strncpy(ns_ptr->name, name, NS_NAME_LEN);

	if (type == ROOT) {
		ns_ptr->parent = NULL;
		root_ns = ns_ptr;
	} else {
		parent_cap = unseal_ns(parent);	
		parent_cap = cheri_andperm(parent_cap, NS_PERMS_OBJ_MASK);
		parent_cap = seal_ns(parent_cap);

		ns_ptr->parent = parent_cap;
	}

	init_ns_members(ns_ptr);
	ns_ptr = cheri_andperm(ns_ptr, NS_PERMS_OWN_MASK);
	ns_ptr = seal_ns(ns_ptr);
	
	return (ns_ptr);
}

nsobject_t *new_nsobject(const char *name, nsobject_type_t type, namespace_t *parent)
{
	validate_nsobjcreate_params(name, type, parent);
	
	nsobject_t *obj_ptr = allocate_nsobject(parent);
	if (obj_ptr == NULL)
		return (NULL);

	obj_ptr->type = type;
	strncpy(obj_ptr->name, name, NS_NAME_LEN);
	
	obj_ptr = cheri_andperm(obj_ptr, NSOBJ_PERMS_OWN_MASK);

	return (obj_ptr);
}

int 
update_nsobject(nsobject_t *nsobj, void *handle, nsobject_type_t new_type)
{
	void *expected;
	nsobj = unseal_nsobj(nsobj);
	if (nsobj->type != RESERVATION)
		return (-1);
	expected = NULL;
	if (atomic_compare_exchange_strong_explicit(&nsobj->obj, &expected, handle, memory_order_acq_rel, memory_order_acquire)) {
		nsobj->type = new_type;
		return (0);
	}
	else 
		return (-1);

}

int delete_nsobject(nsobject_t *ns_obj, namespace_t *ns_cap)
{
	nsobject_t *result;
	struct _ns_member *member, *member_temp;
	ns_cap = unseal_ns(ns_cap);
	ns_obj = unseal_nsobj(ns_obj);

	LIST_FOREACH_SAFE (member, &ns_cap->members->objects, entries, member_temp) {
		result = member->nsobj;
		if(result == ns_obj) 
			LIST_REMOVE(member, entries);
			memset(result->name, '\0', NS_NAME_LEN);
			result->obj = NULL;
			result->type = INVALID_NSOBJ;
			nsobject_deleted();
			cocall_free(member);
			return (1);
	}
	return (0);
}

int delete_namespace(namespace_t *ns_cap)
{
	struct _ns_member *member, *member_temp;
	namespace_t *parent_ns;

	ns_cap = unseal_ns(ns_cap);
	parent_ns = unseal_ns(ns_cap->parent);

	LIST_FOREACH_SAFE(member, &ns_cap->members->namespaces, entries, member_temp) {
		parent_ns = member->ns;
		if (ns_cap == parent_ns) {
			LIST_REMOVE(member, entries);
			//TODO-PBB: oh boy we gotta do big work now
			//Are all child namespaces now invalid?
			//Are all nsobjects below this invalid?
			//In the case of the root namespace, the answer to both of these is yes.
			//Should these simply "merge up?" - this could be problematic, I think
			//This could represent a substantial amount of work; should we make the cocalling thread do it for us?
			//There are large concurrency issues around this whole thing.
			return (1);
		}
	}
	return (0);

}