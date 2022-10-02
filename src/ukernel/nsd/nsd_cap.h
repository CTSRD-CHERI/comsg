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
#ifndef _NSD_CAP_H
#define _NSD_CAP_H

#include <comsg/namespace.h>
#include <comsg/namespace_object.h>

#include <cheri/cherireg.h>

#define NS_INTERNAL_HWPERMS_MASK ( CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | \
	CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE | CHERI_PERM_STORE_CAP | CHERI_PERM_SW2 | CHERI_PERM_SW3 )

namespace_t *make_ns_handle(namespace_t *);
nsobject_t *make_nsobj_handle(nsobject_t *);

namespace_t *unseal_ns(namespace_t *ns_cap);
namespace_t *seal_ns(namespace_t *ns_cap);

nsobject_t *seal_nsobj(nsobject_t *nsobj_cap);
nsobject_t *unseal_nsobj(nsobject_t *nsobj_cap);

bool valid_namespace_cap(namespace_t *ns_cap);
bool valid_nsobject_cap(nsobject_t *obj_cap);
bool valid_reservation_cap(nsobject_t *obj_cap);

nstype_t get_ns_type(namespace_t *ns);

#endif //_NSD_CAP_H