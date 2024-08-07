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
#include "nsd_cap.h"
#include "namespace_table.h"

#include <comsg/namespace.h>
#include <comsg/namespace_object.h>
#include <comsg/utils.h>

#include <cheri/cheric.h>
#include <err.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>


static struct object_type root_nscap, app_nscap, private_nscap, public_nscap, library_nscap;
static struct object_type coservice_nsobj, coport_nsobj, commap_nsobj, reservation_nsobj;

static struct object_type *global_object_types[] = {&root_nscap, &app_nscap, &private_nscap, &public_nscap, &library_nscap, &reservation_nsobj, &coservice_nsobj, &coport_nsobj, &commap_nsobj};
static const int required_otypes = 9;

__attribute__ ((constructor)) static void 
setup_otypes(void)
{
    void *sealroot;
    size_t len;

    len = sizeof(sealroot);
    if (sysctlbyname("security.cheri.sealcap", &sealroot, &len,
        NULL, 0) < 0) {
        err(errno, "setup_otypes: error in sysarch - could not get sealroot");
    }
    sealroot = cheri_incoffset(sealroot, 96);
    sealroot = make_otypes(sealroot, required_otypes, global_object_types);
}

static otype_t 
get_ns_sealcap(nstype_t type)
{
	switch(type) {
	case ROOT:
		return (root_nscap.sc);
	case APPLICATION:
		return (app_nscap.sc);
	case PRIVATE:
		return (private_nscap.sc);
	case PUBLIC:
		return (public_nscap.sc);
	case LIBRARY:
		return (library_nscap.sc);
	default:
		err(EINVAL, "get_ns_sealcap: invalid nstype value %d supplied", type);
	}
	return (NULL);
}

static otype_t 
get_ns_unsealcap(nstype_t type)
{
	switch(type) {
	case ROOT:
		return (root_nscap.usc);
	case APPLICATION:
		return (app_nscap.usc);
	case PRIVATE:
		return (private_nscap.usc);
	case PUBLIC:
		return (public_nscap.usc);
	case LIBRARY:
		return (library_nscap.usc);
	default:
		err(EINVAL, "get_ns_unsealcap: invalid nstype value %d supplied", type);
	}
	return (NULL);
}

static otype_t 
get_nsobj_sealcap(nsobject_type_t type)
{
	switch(type) {
	case COMMAP:
		return (commap_nsobj.sc);
	case COPORT:
		return (coport_nsobj.sc);
	case COSERVICE:
		return (coservice_nsobj.sc);
	case RESERVATION:
		return (reservation_nsobj.sc);
	default:
		err(EINVAL, "get_nsobj_sealcap: invalid nsobject_type_t value %d supplied", type);
	}
	return (NULL);
}

static otype_t 
get_nsobj_unsealcap(nsobject_type_t type)
{
	switch(type) {
	case COMMAP:
		return (commap_nsobj.usc);
	case COPORT:
		return (coport_nsobj.usc);
	case COSERVICE:
		return (coservice_nsobj.usc);
	case RESERVATION:
		return (reservation_nsobj.usc);
	default:
		err(EINVAL, "get_nsobj_unsealcap: invalid nsobject_type_t value %d supplied", type);
	}
	return (NULL);
}

static nsobject_type_t 
nsobj_otype_to_type(long otype)
{
	if (otype == coport_nsobj.otype)
		return (COPORT);
	else if (otype == coservice_nsobj.otype)
		return (COSERVICE);
	else if (otype == commap_nsobj.otype)
		return (COMMAP);
	else if (otype == reservation_nsobj.otype)
		return (RESERVATION);
	else
		return (INVALID_NSOBJ);
}

static nstype_t 
ns_otype_to_type(long otype)
{
	if (otype == root_nscap.otype)
		return (ROOT);
	else if (otype == app_nscap.otype)
		return (APPLICATION);
	else if (otype ==  private_nscap.otype)
		return (PRIVATE);
	else if (otype ==  public_nscap.otype)
		return (PUBLIC);
	else if (otype == library_nscap.otype)
		return (LIBRARY);
	else
		return (INVALID_NS);
	
}

static nsobject_type_t 
get_nsobject_type(nsobject_t *nsobj)
{
	long otype;
	if (!cheri_getsealed(nsobj))
		return (nsobj->type);
	otype = cheri_gettype(nsobj);
	return (nsobj_otype_to_type(otype));
}

nstype_t 
get_ns_type(namespace_t *ns)
{
	long otype;
	if (!cheri_getsealed(ns))
		return (ns->type);
	otype = cheri_gettype(ns);
	return (ns_otype_to_type(otype));
}

namespace_t *
make_ns_handle(namespace_t *ns)
{
	ns = cheri_andperm(ns, NS_PERMS_WR_MASK);
	return (seal_ns(ns));
}

nsobject_t *
make_nsobj_handle(nsobject_t *nsobj)
{
	if (nsobj->type == RESERVATION) 
		nsobj = seal_nsobj(nsobj);
	else
		nsobj = CLEAR_NSOBJ_STORE_PERM(nsobj);
	return (nsobj);
}

namespace_t *
unseal_ns(namespace_t *ns_cap)
{
	otype_t unseal_cap;
	nstype_t type;

	if (cheri_getsealed(ns_cap) == 0)
		return (ns_cap);
	type = get_ns_type(ns_cap);
	if (type == INVALID_NS)
		return (NULL);
	if (!cheri_gettag(ns_cap)) {
		errno = EINVAL;
		err(-127, "%s: attempted to unseal an invalid capability %p", __func__, ns_cap);
	}	
	unseal_cap = get_ns_unsealcap(type);
	ns_cap = cheri_unseal(ns_cap, unseal_cap);
	return (ns_cap);
}

namespace_t *
seal_ns(namespace_t *ns_cap)
{
	otype_t seal_cap;
	nstype_t type;

	if (cheri_getsealed(ns_cap) == 1)
		return (ns_cap);
	type = ns_cap->type;
	if (type == INVALID_NS)
		return (NULL);
	if (!cheri_gettag(ns_cap)) {
		errno = EINVAL;
		err(-127, "%s: attempted to seal an invalid capability %p", __func__, ns_cap);
	}
	seal_cap = get_ns_sealcap(type);
	ns_cap = cheri_seal(ns_cap, seal_cap);
	return (ns_cap);
}

nsobject_t *
unseal_nsobj(nsobject_t *nsobj_cap)
{
	otype_t unseal_cap;
	nsobject_type_t type; 

	if (cheri_getsealed(nsobj_cap) == 0)
		return (nsobj_cap);
	type = get_nsobject_type(nsobj_cap);
	if (type == INVALID_NSOBJ)
		return (NULL);
	unseal_cap = get_nsobj_unsealcap(type);
	return (cheri_unseal(nsobj_cap, unseal_cap));
}

nsobject_t *
seal_nsobj(nsobject_t *nsobj_cap)
{
	otype_t seal_cap;
	nsobject_type_t type;

	if (cheri_getsealed(nsobj_cap) == 1)
		return (nsobj_cap);
	type = nsobj_cap->type;
	if (type == INVALID_NSOBJ)
		return (NULL);
	seal_cap = get_nsobj_sealcap(type);
	return (cheri_seal(nsobj_cap, seal_cap));
}

bool
valid_namespace_cap(namespace_t *ns_cap)
{
	vaddr_t cap_addr = cheri_getaddress(ns_cap);
	if (!cheri_gettag(ns_cap)) {
		//printf("untagged ns cap\n");
		return (false);
	} else if (sizeof(namespace_t) > cheri_getlen(ns_cap)) {
		//printf("ns cap too small\n");
		return (false);
	} else if (!cheri_is_address_inbounds(ns_cap, cap_addr)) {
		//printf("cap address out of bounds\n");
		return (false);
	} else if (!in_ns_table(ns_cap)) {
		//printf("ns cap not in ns table\n");
		return (false);
	} else if (get_ns_type(ns_cap) == INVALID_NS) {
		//printf("cap has invalid type\n");
		return (false);
	}
	else
		return (true);
}

bool
valid_nsobject_cap(nsobject_t *obj_cap)
{
	vaddr_t cap_addr = cheri_getaddress(obj_cap);
	if (!cheri_gettag(obj_cap))
		return (false);
	else if (sizeof(nsobject_t) > cheri_getlen(obj_cap))
		return (false);
	else if (!cheri_is_address_inbounds(obj_cap, cap_addr))
		return (false);
	else if (!in_nsobject_table(obj_cap))
		return (false);
	else if (get_nsobject_type(obj_cap) == INVALID_NSOBJ)
		return (false);
	else
		return (true);
}

bool
valid_reservation_cap(nsobject_t *obj_cap)
{
	if (!cheri_getsealed(obj_cap))
		return (false);
	else if (get_nsobject_type(obj_cap) != RESERVATION)
		return (false);
	else
		return (valid_nsobject_cap(obj_cap));
}
