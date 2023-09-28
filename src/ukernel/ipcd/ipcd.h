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
#ifndef _IPCD_H
#define _IPCD_H

#ifndef COPROC_UKERN
#define COPROC_UKERN 1
#endif

#include <comsg/comsg_args.h>
#include <comsg/coservice_provision.h>
#include <sys/types.h>

#pragma push_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma push_macro("COACCEPT_ENDPOINT")
#define DECLARE_COACCEPT_ENDPOINT(name, validate_f, operation_f) COACCEPT_ENDPOINT(name,  COCALL_##name, validate_f, operation_f)
#define COACCEPT_ENDPOINT(name, op, validate, func) \
extern coservice_provision_t name##_serv;
#include "coaccept_endpoints.inc"
#pragma pop_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma pop_macro("COACCEPT_ENDPOINT")

#pragma push_macro("DECLARE_SLOACCEPT_ENDPOINT")
#pragma push_macro("SLOACCEPT_ENDPOINT")
#define DECLARE_SLOACCEPT_ENDPOINT(name, validate_f, operation_f) SLOACCEPT_ENDPOINT(name, COCALL_##name, validate_f, operation_f)
#define SLOACCEPT_ENDPOINT(name, op, validate, func) \
extern coservice_provision_t name##_serv;
#include "sloaccept_endpoints.inc"
#pragma pop_macro("SLOACCEPT_ENDPOINT")
#pragma pop_macro("DECLARE_SLOACCEPT_ENDPOINT")

//TODO-PBB: Revisit
#define IPCD_NWORKERS 12

/* Must match the capv coprocd provides exactly */
struct ipcd_capvec {
	void *coproc_init_done;
	namespace_t *root_ns;
	void *codiscover;
	void *coinsert;
	void *coselect;
};

struct cocarrier_message {
    void *buf;
    comsg_attachment_t *attachments;
    size_t nattachments;
    _Atomic bool freed;
	_Atomic bool recvd;
	_Atomic bool sent;
	char _pad[5];
};

#endif //!defined(_IPCD_H)