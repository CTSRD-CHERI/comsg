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
#include "module.h"

#include "modules.h"
#include "daemon.h"

#include <assert.h>
#include <err.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <unistd.h>

int
init_module(struct ukernel_module *module)
{
    struct ukernel_daemon *d;
    bool expect_setpgrp;
    int i;
    int error;

    expect_setpgrp = false;
    if (module->init->start != NULL)
        module->init->start(module);

    for (i = 0; i < module->ndaemons; i++) {
        d = &module->daemons[i];

        /* check flags */
        if ((d->flags & LEADER) != 0) {
            if (i == 0) {
                expect_setpgrp = true;
                if ((d->flags & SETPGRP) == 0)
                    err(EINVAL, "%s: %s: daemon flag LEADER set without SETPGRP for daemon %s in module %s", getprogname(), __func__, d->name, module->module_name);
            } else
                err(EINVAL, "%s: %s: daemon flag LEADER set on daemon %s at non-zero index in module %s", getprogname(), __func__, d->name, module->module_name);
        } else if (((d->flags & SETPGRP) != 0) && !expect_setpgrp)
            err(EINVAL, "%s: %s: daemon flag SETPGRP set on daemon %s without specifying a pgrp leader in module %s", getprogname(), __func__, d->name, module->module_name);
        else if (((d->flags & SETPGRP) == 0) && expect_setpgrp)
            err(EINVAL, "%s: %s: daemon flag SETPGRP not set on daemon %s but pgrp leader set in module %s", getprogname(), __func__, d->name, module->module_name);
        else if ((d->flags & ON_DEMAND) != 0)
            continue; 

        error = init_daemon(module, d);
        if (error == -1)
            return (-1);
        if ((d->flags & SYNCHRONOUS) != 0) {
            for (;;) {
                daemon_status status = atomic_load(&d->status);
                if (status == CONTINUING || status == RUNNING)
                    break;
                else if (status != STARTING) {
                    warn("%s status is not STARTING, CONTINUING, or RUNNING", d->name);
                    return (-1);
                }
                sched_yield();
            }
        }
    }

    if (module->init->complete != NULL)
        module->init->complete();

    return (i);
}

int 
kill_module(struct ukernel_module *m, struct ukernel_daemon *reason)
{
    struct ukernel_daemon *d;
    int i;

    if ((m->flags & (ON_DEMAND)) != 0) {
        if (m->status != STARTING && m->status != RUNNING)
            return 0;
    } else if (m->status != RUNNING)
        return 0;

    d = &m->daemons[0];
    if ((d->flags & (LEADER | SETPGRP)) != 0) {
        kill(-d->pid, SIGKILL); 
        for (i = 0; i < m->ndaemons; i++)
            m->daemons[i].status = KILLED;
        return (m->ndaemons - 1);
    } else {
        if (d != reason) {
            atomic_store(&d->status, KILLED);
            kill(d->pid, SIGKILL);
        }

        for (i = 1; i < m->ndaemons; i++) {
            d = &modules[i].daemons[i];
            if (d != reason) {
                atomic_store(&d->status, KILLED);
                kill(d->pid, SIGKILL);
            }
        }

        return (m->ndaemons);
    }
}

void 
kill_and_reap_module(struct ukernel_module *m, struct ukernel_daemon *d)
{
    struct ukernel_daemon *kd;
    int i;
    int status;
    int nkilled;
    pid_t killed;
    
    nkilled = kill_module(m, d);
    if (nkilled == m->ndaemons)
        i = 0;
    else if (nkilled == (m->ndaemons - 1))
        i = 1;
    else if (nkilled == 0)
        return;
    for (; i < m->ndaemons; i++) {
        kd = &m->daemons[i];
        killed = waitpid(kd->pid, &status, (WNOHANG | WEXITED));
        if (killed > 0) {
            kd->pid = 0;
            continue;
        } else if (killed == 0) {
            /* daemons crashed; not out of the question */
            /* set the correct status value, however */
            kd->pid = 0;
            if (kd != d)
                atomic_store(&kd->status, DIED);
            continue; 
        } else if (killed == -1) {
            if (errno != EINVAL) //already reaped
                err(errno, "%s: error in waitpid", __func__);
        }
    }
}

void
restart_module(struct ukernel_module *m, struct ukernel_daemon *d)
{
    assert(m->fini->start != NULL);
    assert(d->fail_act == CLEAN_RESTART);

    m->fini->start();
    kill_and_reap_module(m, d);
    init_module(m);
    m->fini->complete();
}