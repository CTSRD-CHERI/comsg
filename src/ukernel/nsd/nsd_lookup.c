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
#include "nsd_lookup.h"
#include "nsd_cap.h"
#include "namespace_table.h"

#include <coproc/namespace.h>
#include <coproc/namespace_object.h>

#include <cheri/cheric.h>
#include <string.h>
#include <sys/queue.h>

coservice_t *lookup_coservice(char *name, namespace_t *ns_cap)
{
	nsobject_t *ns_obj = lookup_nsobject(name, COSERVICE, ns_cap);
	if(ns_obj != NULL)
		return (ns_obj->coservice);
	else
		return (NULL);
}

coport_t *lookup_coport(char *name, namespace_t *ns_cap)
{
	nsobject_t *ns_obj = lookup_nsobject(name, COPORT, ns_cap);
	if(ns_obj != NULL)
		return (ns_obj->coport);
	else
		return (NULL);
}

void *lookup_commap(char *name, namespace_t *ns_cap)
{
	nsobject_t *ns_obj = lookup_nsobject(name, COMMAP, ns_cap);
	if(ns_obj != NULL)
		return (ns_obj->obj);
	else
		return (NULL);
}

nsobject_t *lookup_nsobject(const char *name, nsobject_type_t nsobject_type, namespace_t *ns_cap)
{
	nsobject_t *result;
	struct _ns_member *member, *member_temp;
	ns_cap = unseal_ns(ns_cap);

	LIST_FOREACH_SAFE (member, &ns_cap->members->objects, entries, member_temp) {
		result = member->nsobj;
		if((strncmp(result->name, name, NS_NAME_LEN) == 0) && (result->type == nsobject_type)) {
			result = cheri_setboundsexact(result, sizeof(nsobject_t));
			return (result);
		}
	}
	
	return (NULL);
}

int in_namespace(const char *name, namespace_t *ns_cap)
{
	nsobject_t *obj;
	namespace_t *ns;
	struct _ns_member *member, *member_temp;
	ns_cap = unseal_ns(ns_cap);

	LIST_FOREACH_SAFE(member, &ns_cap->members->objects, entries, member_temp) {
		obj = member->nsobj;
		if(strncmp(obj->name, name, NS_NAME_LEN) == 0) 
			return (1);
	}

	LIST_FOREACH_SAFE(member, &ns_cap->members->namespaces, entries, member_temp) {
		ns = member->ns;
		if(strncmp(ns->name, name, NS_NAME_LEN) == 0) 
			return (1);
		
	}
	return (0);
}

int is_child_namespace(const char *name, namespace_t *ns_cap)
{
	namespace_t *ns;
	struct _ns_member *member, *member_temp;
	ns_cap = unseal_ns(ns_cap);

	LIST_FOREACH_SAFE(member, &ns_cap->members->namespaces, entries, member_temp) {
		ns = member->ns;
		if(strncmp(ns->name, name, NS_NAME_LEN) == 0)
			return (1);
	}
	return (0);
}
