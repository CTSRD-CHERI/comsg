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
#include "ipcd_cap.h"
#include "coport_table.h"

#include <comsg/coport.h>
#include <comsg/utils.h>

#include <assert.h>
#include <err.h>
#include <cheri/cheric.h>
#include <sys/sysctl.h>
#include <sysexits.h>
#include <unistd.h>

static struct object_type cocarrier_otype, copipe_otype, cochannel_otype;
static void *root_cap;

static __attribute__((constructor)) void
setup_ipcd_otypes(void)
{
    size_t len;
    struct object_type *otypes[] = {&copipe_otype, &cochannel_otype, &cocarrier_otype};
    
    len = sizeof(root_cap);
    assert(sysctlbyname("security.cheri.sealcap", &root_cap, &len,
        NULL, 0) >= 0);

    assert(cheri_gettag(root_cap) != 0);    
    assert(cheri_getlen(root_cap) != 0);
    assert((cheri_getperm(root_cap) & CHERI_PERM_SEAL) != 0);
    /* TODO-PBB: implement type manager and a divided otype space  */
    /* XXX-PBB: we currently simulate the eventual role of the type manager here and in libcomsg */
    root_cap = cheri_incoffset(root_cap, 32);

    root_cap = make_otypes(root_cap, 3, otypes);
}

coport_type_t
coport_gettype(coport_t *ptr)
{
    ptr = unseal_coport(ptr);
    return (ptr->type);
}

int 
valid_coport(coport_t *addr)
{
    if (!cheri_gettag(addr))
        return (0);
    else if(cheri_getlen(addr) < sizeof(coport_t))
        return (0);
    else if(!in_coport_table(addr, coport_gettype(addr)))
        return (0);
    else 
        return (1);
}

int 
valid_cocarrier(coport_t *addr)
{
    if(!valid_coport(addr))
        return (0);
    else if(cheri_gettype(addr) != cocarrier_otype.otype)
        return (0);
    else 
        return (1);
}

coport_t *
seal_coport(coport_t *ptr)
{
    if (cheri_getsealed(ptr))
        return (ptr);
    ptr = cheri_clearperm(ptr, CHERI_PERM_GLOBAL);
    if (ptr->type == COCHANNEL)
        return (cheri_seal(ptr, cochannel_otype.sc));
    else if (ptr->type == COPIPE)
        return (cheri_seal(ptr, copipe_otype.sc));
    else if (ptr->type == COCARRIER)
        return (cheri_seal(ptr, cocarrier_otype.sc));
    else
        err(EX_SOFTWARE, "%s: invalid coport type %d", __func__, ptr->type); //should not be reached
}

coport_t *
unseal_coport(coport_t *ptr)
{
    if (!cheri_getsealed(ptr))
        return (ptr);
    else if (cheri_gettype(ptr) == cocarrier_otype.otype)
        return (cheri_unseal(ptr, cocarrier_otype.usc));
    else if (cheri_gettype(ptr) == copipe_otype.otype)
        return (cheri_unseal(ptr, copipe_otype.usc));
    else if (cheri_gettype(ptr) == cochannel_otype.otype)
        return (cheri_unseal(ptr, cochannel_otype.usc));
    else 
        err(EX_SOFTWARE, "%s: invalid coport type %d", __func__, ptr->type); //should not be reached
}