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

#include <coproc/coservice.h>
#include <coproc/otype.h>
#include <coproc/utils.h>

#include <cheri/cheric.h>
#include <err.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <unistd.h>

static struct object_type coservice_obj;

static const struct object_type *global_object_types[] = {&coservice_obj};
static const int required_otypes = 1;

__attribute__ ((constructor)) static 
void setup_otypes(void)
{
    void *sealroot;
    size_t len;

    len = sizeof(sealroot);
    if (sysctlbyname("security.cheri.sealcap", &sealroot, &len,
        NULL, 0) < 0) {
        err(errno, "setup_otypes: error in sysarch - could not get sealroot");
    }
    sealroot = cheri_incoffset(sealroot, 64);
    sealroot = make_otypes(sealroot, required_otypes, global_object_types);
}

coservice_t *seal_coservice(coservice_t *service_handle)
{
	if (cheri_getsealed(service_handle))
		return (service_handle);
	return (cheri_seal(service_handle, coservice_obj.sc));
}

coservice_t *unseal_coservice(coservice_t *service_handle)
{
	if (!cheri_getsealed(service_handle))
		return (service_handle);
	return (cheri_unseal(service_handle, coservice_obj.usc));
}

coservice_t *create_coservice_handle(coservice_t *service)
{
	if (cheri_getsealed(service))
		return (service);
	service = cheri_clearperm(service, CHERI_PERM_GLOBAL);
	return (seal_coservice(service));
}