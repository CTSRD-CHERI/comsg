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
#ifndef _COSERVICE_H
#define _COSERVICE_H

#include <cheri/cherireg.h>
#include <stdatomic.h>
#include <sys/cdefs.h>

#if !defined(__riscv)
#define COSERVICE_PERMS_PERMS_ARCH_SPECIFIC ( CHERI_PERM_MUTABLE_LOAD )
#else
#define COSERVICE_PERMS_PERMS_ARCH_SPECIFIC ( 0 )
#endif
#define COSERVICE_HANDLE_PERMS ( CHERI_PERM_LOAD | \
	CHERI_PERM_LOAD_CAP | COSERVICE_PERMS_PERMS_ARCH_SPECIFIC)
/* Mutable load not required or desired for endpoint handles */
#define COSERVICE_ENDPOINT_HANDLE_PERMS ( CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | \
	CHERI_PERM_LOAD_CAP | CHERI_PERM_STORE )
#define COSERVICE_MAX_WORKERS (128)

typedef enum {NONE = 0, SLOWPATH = 1} coservice_flags_t;

struct _coservice_endpoint {
	void **worker_scbs;
	int nworkers;
	_Atomic int next_worker;
};

typedef struct _coservice {
	struct _coservice_endpoint *impl;
	int op;
	coservice_flags_t flags;
} coservice_t;


/* Wrapper functions should marshall arguments */
/* Wrapper can return a value directly, or a status code */
/* */

__BEGIN_DECLS

void *
get_coservice_scb(struct _coservice_endpoint *s);

__END_DECLS

#endif //!defined(_COSERVICE_H)