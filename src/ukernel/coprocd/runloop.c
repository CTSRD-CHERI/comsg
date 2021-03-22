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
#include "runloop.h"

#include "daemon.h"
#include "module.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <sysexits.h>
#include <unistd.h>

static struct ukernel_daemon *daemon_restart = NULL;
static struct ukernel_module *module_restart = NULL;

static void runloop(void);

void
mark_module_for_restart(struct ukernel_module *m, struct ukernel_daemon *d)
{
    if (daemon_restart != NULL || module_restart != NULL)
        _exit(EX_SOFTWARE);
    module_restart = m;
    daemon_restart = d;
}

void
mark_daemon_for_restart(struct ukernel_daemon *d)
{
    if (daemon_restart != NULL || module_restart != NULL)
        _exit(EX_SOFTWARE);
    module_restart = NULL; //redundant
    daemon_restart = d;
}

void
enter_runloop(void)
{
    runloop();
}

static void
runloop(void)
{
    int error;
    sigset_t mask, oldmask;

    error = sigemptyset(&mask);
    error = sigaddset(&mask, SIGCHLD);

    pthread_sigmask(SIG_BLOCK, &mask, &oldmask);
    for (;;) {
        if (module_restart != NULL) {
            assert(daemon_restart != NULL);
            restart_module(module_restart, daemon_restart);
            module_restart = NULL;
            daemon_restart = NULL;
        } else if (daemon_restart != NULL) {
            assert(module_restart == NULL);
            restart_daemon(daemon_restart);
            daemon_restart = NULL;
        }
        sigsuspend(&oldmask);
    }
}