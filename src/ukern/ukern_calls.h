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

#include "ukern/cocalls.h"

#define COCALL_INVALID (0) //not used
#define COCALL_CODISCOVER (1)
#define COCALL_COPROVIDE (2)
#define COCALL_COINSERT (3)
#define COCALL_COSELECT (4)
#define COCALL_COUPDATE (5)
#define COCALL_CODELETE (6)
#define COCALL_COOPEN (7)
#define COCALL_COCLOSE (8)
#define COCALL_COSEND (9) 
#define COCALL_CORECV (10)
#define COCALL_COPOLL (11)
#define COCALL_COPROC_INIT (12)

const int n_ukern_calls = 13;

extern pthread_key_t ukern_call_set;

namespace_t *coproc_init(namespace_t *global_ns, void *coinsert_scb, void *codiscover_scb);

coservice_t *coprovide(const char *name, void **worker_scbs, int nworkers, nsobject_t *nsobj, namespace_t *ns);


#endif //!defined(_UKERN_CALLS_H)