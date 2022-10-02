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
#ifndef _UKERN_CALLS_H
#define _UKERN_CALLS_H

#include <cocall/cocalls.h>
#include <comsg/comsg_args.h>
#include <comsg/coevent.h>
#include <comsg/coport.h>
#include <comsg/coservice.h>
#include <comsg/namespace.h>
#include <comsg/namespace_object.h>

#include <stdbool.h>
#include <stddef.h>
#include <sys/cdefs.h>

#pragma push_macro("UKERN_ENDPOINT")
#define UKERN_ENDPOINT(name) static const char * U_##name = #name;
#include <comsg/ukern_calls.inc>
#pragma pop_macro("UKERN_ENDPOINT")

extern namespace_t *root_ns;
extern bool is_ukernel;

__BEGIN_DECLS

namespace_t *coproc_init(namespace_t *, void *, void *, void *);
int coproc_init_done(void);
nsobject_t *coinsert(const char *, nsobject_type_t, void *, namespace_t *);
nsobject_t *coselect(const char *, nsobject_type_t, namespace_t *);
coservice_t *codiscover(nsobject_t *, void **);
coservice_t *coprovide(void **, int, coservice_flags_t, int);
coservice_t *coprovide2(struct _coservice_endpoint *, coservice_flags_t, int);
namespace_t *cocreate(const char *, nstype_t, namespace_t *);
int codrop(namespace_t *, namespace_t *);
int codelete(nsobject_t *, namespace_t *);
nsobject_t *coupdate(nsobject_t *, nsobject_type_t, void *);
coport_t *coopen(coport_type_t);
int cocarrier_recv(coport_t *, void ** const, size_t);
int cocarrier_send(coport_t *, const void *, size_t);
int cocarrier_recv_oob(coport_t *, void ** const, size_t, comsg_attachment_set_t *);
int cocarrier_send_oob(coport_t *, const void *, size_t, comsg_attachment_t *, size_t);
int copoll(pollcoport_t *, int , int );
int coclose(coport_t *);
int ccb_install(cocallback_func_t *, struct cocallback_args *, coevent_t *);
cocallback_func_t *ccb_register(void *, cocallback_flags_t);
coevent_t *colisten(coevent_type_t , coevent_subject_t);

void discover_ukern_func(nsobject_t *, cocall_num_t);
void set_ukern_target(cocall_num_t , void *);
void set_ukern_func(nsobject_t *, cocall_num_t);

__END_DECLS

#endif //!defined(_UKERN_CALLS_H)