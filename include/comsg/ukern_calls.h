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
#include <cocall/cocall_args.h>
#include <coproc/coport.h>
#include <coproc/namespace.h>
#include <coproc/namespace_object.h>

#include <stdbool.h>
#include <stddef.h>

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
#define COCALL_COCREATE (13)
#define COCALL_CODROP (14)
#define COCALL_COPROC_INIT_DONE (15)
#define COCALL_SLOPOLL (16)
#define COCALL_CODEFINE (17)
#define COCALL_SLODEFINE (18)

/* ipcd provided */
#define U_COOPEN "coopen"
#define U_COCLOSE "coclose"
#define U_COSEND "cosend"
#define U_CORECV "corecv"
#define U_COPOLL "copoll"
#define U_SLOPOLL "copoll_slow"

/* coserviced provided */
#define U_COPROVIDE "coprovide"
#define U_CODISCOVER "codiscover"

/* nsd provided */
#define U_COCREATE "cocreate"
#define U_CODROP "codrop"
#define U_COINSERT "coinsert"
#define U_COSELECT "coselect"
#define U_COUPDATE "coupdate" 
#define U_CODELETE "codelete"

/* coprocd provided */
#define U_COPROC_INIT "coproc_init"
#define U_COPROC_INIT_DONE "coproc_init_done"

/* otyped provided */
#define U_CODEFINE "codefine"
#define U_SLODEFINE "codefine_slow"

#define n_ukern_calls (19)

extern namespace_t *global_ns;
extern bool is_ukernel;

namespace_t *coproc_init(namespace_t *global_ns, void *coinsert_scb, void *coselect_scb, void *codiscover_scb);
int coproc_init_done(void);
nsobject_t *coinsert(const char *name, nsobject_type_t type, void *subject, namespace_t *ns);
nsobject_t *coselect(const char *name, nsobject_type_t type, namespace_t *ns);
coservice_t *codiscover(nsobject_t *nsobj, void **scb);
coservice_t *coprovide(void **worker_scbs, int nworkers);
namespace_t *cocreate(const char *name, nstype_t type, namespace_t *parent);
int codelete(nsobject_t *nsobj, namespace_t *parent);
nsobject_t *coupdate(nsobject_t *nsobj, nsobject_type_t type, void *subject);
coport_t *coopen(coport_type_t type);
void *cocarrier_recv(coport_t *port, size_t len);
int cocarrier_send(coport_t *port, void *buf, size_t len);
int copoll(pollcoport_t *coports, int ncoports, int timeout);
int coclose(coport_t *coport);

void discover_ukern_func(nsobject_t *service_obj, int function);
void set_ukern_target(int function, void *target);
void set_ukern_func(nsobject_t *service_obj, int function);

#endif //!defined(_UKERN_CALLS_H)