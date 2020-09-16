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

#include <ccmalloc.h>
#include <coproc/namespace.h>
#include <coproc/namespace_object.h>
#include <coproc/utils.h>

#include <assert.h>
#include <cheri/cheric.h>
#include <err.h>
#include <sys/errno.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <unistd.h>

static __inline void
validate_namespace_cap(namespace_t *ns_cap)
{
	vaddr_t cap_addr = cheri_getaddress(ns_cap);
	assert(cheri_gettag(ns_cap));
	assert(sizeof(namespace_t) <= cheri_getlen(ns_cap));

	assert(cheri_is_address_inbounds(ns_cap, cap_addr));
	assert(in_namespace_table(ns_cap));
	
	assert(valid_ns_otype(ns_cap) || !cheri_getsealed(ns_cap));
}

static __inline void 
validate_nscreate_params(namespace_t *parent, nstype_t type, const char *name)
{
	assert(valid_ns_name(name));
	assert(VALID_NS_TYPE(type));
	if(type!=GLOBAL) {
		validate_namespace_cap(parent);
		assert(NS_PERMITS_WRITE(parent));
		parent = unseal_ns(parent);
	}

	switch(type)
	{
		case GLOBAL:
			assert(parent == NULL);
			break;
		case PROCESS:
		case LIBRARY:
			assert(parent->type == GLOBAL);
			assert(parent == global_namespace);
			break;
		case THREAD:
			assert(parent->type == PROCESS);
			break;
		case EXPLICIT:
			break;
		default:
			err(EINVAL, "create_namespace: invalid type supplied");
			break;
	}
}

static __inline void 
validate_nsobjcreate_params(const char *name, nsobject_type_t type, namespace_t *parent)
{
	validate_namespace_cap(parent);

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

namespace_t *create_namespace(const char *name, nstype_t type, namespace_t *parent)
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

	if(type == GLOBAL) {
		ns_ptr->parent = NULL;
		global_namespace = ns_ptr;
	}
	else {
		parent_cap = cheri_andperm(parent, NS_PERMS_OBJ);
		parent_cap = seal_ns(parent_cap);
		ns_ptr->parent = parent_cap;
	}

	init_ns_members(ns_ptr);
	ns_ptr = cheri_andperm(ns_ptr, NS_PERMS_OWN_MASK);
	
	return (ns_ptr);
}

nsobject_t *create_nsobject(const char *name, nsobject_type_t type, namespace_t *parent)
{
	validate_nsobjcreate_params(name, type, parent);
	
	nsobject_t *obj_ptr = allocate_nsobject(parent);
	if (obj == NULL)
		return (NULL);

	obj_ptr->type = type;
	strncpy(obj_ptr->name, name, NS_NAME_LEN);
	
	obj_ptr = cheri_andperm(obj_ptr, NSOBJ_PERMS_OWN_MASK);

	return (obj_ptr);
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
			strnset(result->name, '\0', NS_NAME_LEN);
			result->obj = NULL;
			result->type = INVALID;
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
		ns = member->ns;
		if (ns_cap == ns) {
			LIST_REMOVE(member, entries);
			//TODO-PBB: oh boy we gotta do big work now
			//Are all child namespaces now invalid?
			//Are all nsobjects below this invalid?
			//In the case of the global namespace, the answer to both of these is yes.
			//Should these simply "merge up?" - this could be problematic, I think
			//This could represent a substantial amount of work; should we make the cocalling thread do it for us?
			//There are large concurrency issues around this whole thing.
			return (1)
		}
	}

}