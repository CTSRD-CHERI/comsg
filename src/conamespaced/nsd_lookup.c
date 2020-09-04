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

#include "nsd_cap.h"
#include "ukern/namespace.h"

#include <cheri/cheric.h>

coservice_t *lookup_coservice(char *name, namespace_t *ns_cap)
{
	nsobject_t *ns_obj = lookup_nsobject(name, ns_cap, COSERVICE);
	if(ns_obj != NULL)
		return (ns_obj->coservice);
	else
		return (NULL);
}

coport_t *lookup_coport(char *name, namespace_t *ns_cap)
{
	nsobject_t *ns_obj = lookup_nsobject(name, ns_cap, COPORT);
	if(ns_obj != NULL)
		return (ns_obj->coport);
	else
		return (NULL);
}

void *lookup_commap(char *name, namespace_t *ns_cap)
{
	nsobject_t *ns_obj = lookup_nsobject(name, ns_cap, COMMAP);
	if(ns_obj != NULL)
		return (ns_obj->obj);
	else
		return (NULL);
}

nsobject_t *lookup_nsobject(const char *name, nsotype_t nsobject_type, namespace_t *ns_cap)
{
	nsobject_t *result;
	struct _member *member;
	ns_cap = unseal_ns(ns_cap);
	member = LIST_FIRST(&ns_cap->members->objects);
	while (member != NULL) {
		result = member->nsobj;
		if(result->type != nsobject_type)
			member = LIST_NEXT(member, entries);
		else if(strncmp(result->name, name, NS_NAME_LEN) == 0) {
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
	struct _member *member;
	ns_cap = unseal_ns(ns_cap);
	member = LIST_FIRST(&ns_cap->members->objects);
	while (member != NULL) {
		obj = member->nsobj;
		if(strncmp(obj->name, name, NS_NAME_LEN) == 0) {
			return (1);
		}
		member = LIST_NEXT(member, entries);
	}
	member = LIST_FIRST(&ns_cap->members->namespaces);
	while (member != NULL) {
		ns = member->ns;
		if(strncmp(ns->name, name, NS_NAME_LEN) == 0) {
			return (1);
		}
		member = LIST_NEXT(member, entries);
	}
	return (0);
}


