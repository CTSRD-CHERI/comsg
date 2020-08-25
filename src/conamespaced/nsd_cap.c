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
#include "ukern/nsobject_t.h"
#include "ukern/utils.h"

static struct object_type global_ns, proc_ns, thread_ns, explicit_ns, library_ns;
static struct object_type coservice_nsobj, coport_nsobj, commap_nsobj, reservation_nsobj;

static const struct object_type *global_object_types[] = {&global_ns, &proc_ns, &thread_ns, &explicit_ns, &library_ns, &reservation_nsobj, &coservice_nsobj, &coport_nsobj, &commap_nsobj};
static const int required_otypes = 9;

__attribute__ ((constructor)) static 
void setup_otypes(void)
{
    void *sealroot;
    if (sysarch(CHERI_GET_SEALCAP, &sealroot) < 0)
    {
        err(errno, "setup_otypes: error in sysarch - could not get sealroot");
    }
    sealroot = make_otypes(sealroot, required_otypes, global_object_types);

}

static 
otype_t get_ns_sealcap(nstype_t type)
{
	switch(type)
	{
		case GLOBAL:
			return global_ns.sc;
		case PROCESS:
			return proc_ns.sc;
		case THREAD:
			return thread_ns.sc;
		case EXPLICIT:
			return explicit_ns.sc;
		default:
			err(EINVAL, "get_ns_sealcap: invalid nstype value %d supplied", type);
	}
	return NULL;
}

static 
otype_t get_ns_unsealcap(nstype_t type)
{
	switch(type)
	{
		case GLOBAL:
			return global_ns.usc;
		case PROCESS:
			return proc_ns.usc;
		case THREAD:
			return thread_ns.usc;
		case EXPLICIT:
			return explicit_ns.usc;
		default:
			err(EINVAL, "get_ns_unsealcap: invalid nstype value %d supplied", type);
	}
	return NULL;
}

static 
otype_t get_nsobj_sealcap(nsobjtype_t type)
{
	switch(type)
	{
		case COMMAP:
			return coservice_nsobj.sc;
		case COPORT:
			return coport_nsobj.sc;
		case COSERVICE:
			return commap_nsobj.sc;
		case RESERVATION:
			return reservation_nsobj.sc;
		default:
			err(EINVAL, "get_nsobj_sealcap: invalid nsobjtype_t value %d supplied", type);
	}
	return NULL;
}

static 
otype_t get_nsobj_unsealcap(nsobjtype_t type)
{
	switch(type)
	{
		case COMMAP:
			return coservice_nsobj.usc;
		case COPORT:
			return coport_nsobj.usc;
		case COSERVICE:
			return commap_nsobj.usc;
		case RESERVATION:
			return reservation_nsobj.usc;
		default:
			err(EINVAL, "get_nsobj_unsealcap: invalid nsobjtype_t value %d supplied", type);
	}
	return NULL;
}

namespace_t *unseal_ns(namespace_t *ns_cap)
{
	if (!cheri_getsealed(ns_cap))
		return (ns_cap);
	nstype_t type = get_ns_type(ns_cap);
	otype_t unseal_cap = get_ns_unsealcap(type);
	return cheri_unseal(ns_cap, unseal_cap);
}

namespace_t *seal_ns(namespace_t *ns_cap);
{
	if (cheri_getsealed(ns_cap))
		return (ns_cap);
	nstype_t type = ns_cap->type;
	otype_t seal_cap = get_ns_sealcap(type);
	return cheri_seal(ns_cap, seal_cap);
}

nsobject_t *unseal_nsobj(nsobject_t *nsobj_cap)
{
	if (!cheri_getsealed(nsobj_cap))
		return (nsobj_cap);
	nsobjtype_t type = get_nsobject_type(nsobj_cap);
	otype_t unseal_cap = get_nsobj_unsealcap(type);
	return cheri_unseal(nsobj_cap, unseal_cap);
}

nsobject_t *seal_nsobj(nsobject_t *nsobj_cap)
{
	if (cheri_getsealed(nsobj_cap))
		return (nsobj_cap);
	nsobjtype_t type = nsobj_cap->type;
	otype_t seal_cap = get_nsobj_sealcap(type);
	return cheri_seal(nsobj_cap, seal_cap);
}

