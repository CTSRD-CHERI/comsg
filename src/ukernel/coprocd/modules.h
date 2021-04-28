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
#ifndef _MODULE_TABLE_H
#define _MODULE_TABLE_H

/* Module table */

#include "daemon.h"

/*
 * To add a module, include a header that defines the necessary macros here,
 */
#include "modules/core.h"

/* 
 * then add it to the DECLARE_MODULES macro here,
 */
#define DECLARE_MODULES \
    MODULE(core, CORE, N_CORE_DAEMONS, CORE_FLAGS, CORE_INIT, CORE_FINI, FOR_EACH_CORE_DAEMON, CORE_DAEMON)

/*
 and increment the value of N_MODULES accordingly.
 */
#define N_MODULES (1)

extern struct ukernel_module modules[] __attribute__((aligned (0x400)));

/* Module table utility functions */

#include <stdint.h>
#include <sys/types.h>

/* 
 * This is used to get module struct address from address of daemon struct.
 * Only works when the base of the modules table is aligned to the size of 
 * struct ukernel module, and while the size of the module matches the mask.
 */
#define MODULE_MASK (~1024)

static inline struct ukernel_daemon *
find_daemon(pid_t dpid)
{
    int i, j;
    struct ukernel_module *m;
    for (i = 0; i < N_MODULES; i++) {
        m = &modules[i];
        for (j = 0; j < m->ndaemons; j++) {
            if (m->daemons[j].pid == dpid)
                return (&m->daemons[j]);
        }
    }
    return (NULL);
}

static inline struct ukernel_module *
module_from_daemon(struct ukernel_daemon *dptr)
{
    return (__builtin_cheri_address_set(&modules, ((uintptr_t)dptr & MODULE_MASK)));
}


#endif //!defined(_MODULE_TABLE_H)