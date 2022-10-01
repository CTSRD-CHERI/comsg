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
#include "coservice_cap.h"

#include <comsg/coservice.h>
#include <comsg/utils.h>

#include <assert.h>
#include <cheri/cheric.h>
#include <err.h>
#include <stdbool.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <unistd.h>

static struct object_type coservice_otype;


static __attribute__((constructor)) void
setup_ipcd_otypes(void)
{
    size_t len;
    struct object_type *otypes[] = {&coservice_otype};
	void *root_cap;
    
    len = sizeof(root_cap);
    assert(sysctlbyname("security.cheri.sealcap", &root_cap, &len,
        NULL, 0) >= 0);

    assert(cheri_gettag(root_cap) != 0);    
    assert(cheri_getlen(root_cap) >= 96);
    assert((cheri_getperm(root_cap) & CHERI_PERM_SEAL) != 0);
    /* TODO-PBB: implement type manager and a divided otype space  */
    /* XXX-PBB: we currently simulate the eventual role of the type manager here and in libcomsg */
    root_cap = cheri_incoffset(root_cap, 64);
    root_cap = make_otypes(root_cap, 1, otypes);
}

bool
is_valid_endpoint(struct _coservice_endpoint *ep)
{
    if (!cheri_gettag(ep))
        return false;
    else if (cheri_gettype(ep) != coservice_otype.otype)
        return false;
    
    return true;
}

coservice_t *create_coservice_handle(coservice_t *service)
{
    if (!cheri_getsealed(service->impl)) {
        service->impl = cheri_setboundsexact(service->impl, sizeof(struct _coservice_endpoint));
        service->impl = cheri_andperm(service->impl, COSERVICE_ENDPOINT_HANDLE_PERMS);
	    service->impl = cheri_seal(service->impl, coservice_otype.sc);
    }
	service = cheri_andperm(service, COSERVICE_HANDLE_PERMS);
	service = cheri_setboundsexact(service, sizeof(coservice_t));
	
	return ((service));
}

struct _coservice_endpoint *unseal_endpoint(struct _coservice_endpoint *ep)
{
    if (!cheri_getsealed(ep))
        return (ep);
	if (cheri_gettype(ep) != coservice_otype.otype)
		return NULL; //malformed coservice
	return cheri_unseal(ep, coservice_otype.usc);
}

struct _coservice_endpoint *get_service_endpoint(coservice_t *service)
{
	return unseal_endpoint(service->impl);
}
