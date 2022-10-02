/*
 * Copyright (c) 2021 Peter S. Blandford-Baker
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
#ifndef _COMSG_CORE_DAEMONS_H
#define _COMSG_CORE_DAEMONS_H

#include "../daemon.h"

#include <comsg/comsg_args.h>

void ipcd_setup_complete(comsg_args_t *, void *);
void get_ipcd_capv(struct coexecve_capvec *);

void nsd_setup(comsg_args_t *, void *);
void nsd_setup_complete(comsg_args_t *, void *);

void coserviced_setup(comsg_args_t *, void *);
void get_coserviced_capv(struct coexecve_capvec *);

void coproc_user_init(comsg_args_t *, void *);

void coeventd_setup_complete(comsg_args_t *, void *);
void get_coeventd_capv(struct coexecve_capvec *);

void core_init_start(struct ukernel_module *);
void core_init_complete(void);

extern daemon_setup_info coprocd_init;
extern daemon_setup_info nsd_init;
extern daemon_setup_info coserviced_init;
extern daemon_setup_info coeventd_init;
extern daemon_setup_info ipcd_init;

extern module_setup_info core_init;

#define CORE_DAEMON(NAME) \
    DAEMON(NAME, "/usr/bin/" #NAME,  &NAME##_init, HCF, SYNCHRONOUS)

#define FOR_EACH_CORE_DAEMON(F) \
	F(coprocd), \
	F(nsd), \
	F(coserviced), \
	F(coeventd), \
	F(ipcd), 

#define CORE_FLAGS (0)
#define CORE_INIT (&core_init)
#define CORE_FINI (NULL)
#define N_CORE_DAEMONS (5)


#endif //!defined(_COMSG_CORE_DAEMONS_H)