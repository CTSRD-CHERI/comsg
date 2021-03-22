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
#include "monitor.h"

#include "coproc.h"
#include "daemon.h"
#include "module.h"
#include "modules.h"
#include "runloop.h"

#include <signal.h>
#include <sysexits.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void halt_and_catch_fire(struct ukernel_daemon *cause);
static void just_reap(int sig, siginfo_t *info, ucontext_t *uap);

void
daemon_died(int sig, siginfo_t *info, ucontext_t *uap) 
{
    int errno_temp;
    int status;
    pid_t child, who_died;
    struct ukernel_daemon *d;
    struct ukernel_module *m;

    errno_temp = errno;
    child = info->si_pid;
    /* reap */
    who_died = waitpid(child, &status, (WNOHANG | WEXITED));
    if (who_died == 0 ) {
        /*
         * We reap elsewhere under certain circmstances,
         * such as when we're restarting a module. In those
         * cases, we don't want to do anything extra. In 
         * those cases, we wouldn't expect to see a SIGCHLD 
         * at all *I think*.
         */
        errno = errno_temp;
        return;
    } else if (who_died == -1) {
        /* Want to know about these cases. */
        _exit(EX_OSERR);
    }

    d = find_daemon(child);
    m = module_from_daemon(d);

    d->status = DIED;
    if (m->type == CORE && d->fail_act == HCF)
        halt_and_catch_fire(d);
    else if (d->fail_act == KILL)
        kill_module(m, d);
    else if (d->fail_act == DIRTY_RESTART)
        mark_daemon_for_restart(d);
    else if (d->fail_act == CLEAN_RESTART)
        mark_module_for_restart(m, d);
    else if (d->fail_act == CONTINUE) 
        cleanup_daemon(d);
    else
        _exit(EX_SOFTWARE);


    errno = errno_temp;
    return;
}

static void
halt_and_catch_fire(struct ukernel_daemon *cause)
{
    int i, j;
    struct ukernel_module *m;

    set_sigchld_handler(just_reap);
    // kill core last
    for (i = 1; i < N_MODULES; i++) {
        m = &modules[i];
        kill_module(m, cause);
    }
    m = &modules[0]; //core
    kill_module(m, cause);
}

static void 
just_reap(int sig, siginfo_t *info, ucontext_t *uap)
{
    int errno_temp;
    int status;
    pid_t who_died, child;
    
    errno_temp = errno;
    child = info->si_pid;
    who_died = waitpid(child, &status, (WNOHANG | WEXITED));
    //Do we care about failure here?
    errno = errno_temp;
    return;
}

void
set_sigchld_handler(void *func)
{
    struct sigaction act;
    int error;

    if(!is_main_thread()) /* only main thread should be involved */
        _exit(EX_SOFTWARE);

    act.sa_flags = SA_NOCLDSTOP | SA_SIGINFO;
    act.sa_sigaction = func;
    error = sigemptyset(&act.sa_mask);
    error = sigaddset(&act.sa_mask, SIGCHLD);
    
    error = sigaction(SIGCHLD, &act, NULL);
    if (error != 0)
        _exit(EX_OSERR);
}