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
#include "launch.h"

#include "daemon.h"
#include "module.h"
#include "modules.h"
#include "monitor.h"
#include "util.h"

#include <cocall/worker_map.h>
#include <comsg/ukern_calls.h>

#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <unistd.h>

static bool core_loaded = false;

pid_t ukernel_sid = 0;
struct ukernel_daemon *process_manager = NULL;

static void
init_core(void)
{
    pid_t daemon_pid;
    function_map_t *init_map;
    struct ukernel_module *core_module;
    struct ukernel_daemon *d;
    int i;

    //Should only be called once; microkernel can't currently
    //recover from core module death
    assert(!core_loaded);
    assert(process_manager == NULL);

    //Core module must always be the first item in the module array
    assert(modules[0].type == CORE);
    assert(strcmp(modules[0].module_name, "core") == 0);
    //coprocd (i.e. this) should always be the first daemon in the core module
    assert(strcmp(modules[0].daemons[0].name, getprogname()) == 0);

    core_module = &modules[0];
    process_manager = &core_module->daemons[0];
    process_manager->pid = getpid();
    process_manager->status = RUNNING;

    /* TODO-PBB: rework worker_map stuff */
    process_manager->setup_state = calloc(1, sizeof(daemon_setup_state));
    init_map = spawn_worker(U_COPROC_INIT, process_manager->setup_info->setup, NULL);
    if (init_map == NULL)
        err(EEXIST, "could not start microkernel in this address space. Is an old instance still running?");
    process_manager->setup_state->setup_worker = &init_map->workers[0];
    free(init_map);

    core_module->init->start(core_module);
    for (i = 1; i < core_module->ndaemons; i++) {
        d = &core_module->daemons[i];
        daemon_pid = init_daemon(core_module, d);
        if (daemon_pid == -1) 
            err(errno, "failed to start microkernel core daemon %s", d->name);
    }
    core_module->init->complete();

    core_loaded = true;
}

void 
init_microkernel(void)
{
    struct ukernel_module *m;
    int i;

    /* Create microkernel as a new session */
    ukernel_sid = setsid();
    /* Signal mask is inherited, we expect SIGCHLD, and we don't want to be interrupted */
    set_sigchld_handler(daemon_died);
    set_sigmask(SIGCHLD);
    /* Microkernel core must always be loaded first */
    if (!core_loaded)
        init_core();

    for (i = 1; i < N_MODULES; i++) {
        m = &modules[i];
        if (init_module(m) == -1) {
            if (m->type == CORE) {
                err(errno, "failed to start core microkernel module %s", m->module_name);
            } else
                warn("failed to start microkernel module %s", m->module_name);
        }
    }
    
    clear_sigmask();
}
