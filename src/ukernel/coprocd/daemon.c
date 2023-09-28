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
#include "daemon.h"

#include "modules.h"
#include "launch.h"
#include "util.h"

#include <cocall/capvec.h>
#include "dynamic_endpoint.h"
#include "dynamic_endpoint_map.h"
#include <comsg/utils.h>

#include <assert.h>
#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static void kill_daemon_initializer(struct ukernel_daemon *);
static pid_t spawn_daemon_proc(struct ukernel_module *, struct ukernel_daemon *);
static int spawn_daemon_initializer(struct ukernel_daemon *);
static void set_daemon_args(char **, struct ukernel_module *, struct ukernel_daemon *);
static void free_daemon_args(char **);

void 
cleanup_daemon(struct ukernel_daemon *d)
{
    /* called if failure mode action is to do nothing */
    d->pid = 0;
    return;
}

int
init_daemon(struct ukernel_module *m, struct ukernel_daemon *d)
{
    int error;
    pid_t pid;

    atomic_store(&d->status, STARTING);

    error = spawn_daemon_initializer(d);
    if (error != 0)
        return (error);
    pid = spawn_daemon_proc(m, d);
    if (pid == -1)
        return (error);

    d->pid = pid;
    return (0);
}

void
kill_and_reap_daemon(struct ukernel_daemon *d)
{
    int error, status;
    pid_t pid;

    error = kill(d->pid, SIGKILL);
    if (error != 0) {
        if (errno == EINVAL)
            _exit(EX_SOFTWARE);
        else if (errno == EPERM)
            _exit(EX_NOPERM);
        else if (errno == ESRCH)
            return; /* already reaped or wrong pid*/
    }
    pid = waitpid(d->pid, &status, (WNOHANG | WEXITED));
    if (pid == d->pid) {
        d->pid = 0;
        atomic_store(&d->status, KILLED);
    } else if (pid == 0) {
        _exit(EX_SOFTWARE); /* something else reaped first */
    } else if (pid == -1) 
        _exit(EX_SOFTWARE);
}

static void 
kill_daemon_initializer(struct ukernel_daemon *d)
{
    if (d->setup_state == NULL)
        return;

    if (d->setup_state->setup_worker != NULL)
        pthread_cancel(d->setup_state->setup_worker->worker);

    if (d->setup_state->done_worker != NULL)
        pthread_cancel(d->setup_state->done_worker->worker);
}

int
restart_daemon(struct ukernel_daemon *d)
{
    struct ukernel_module *m;
    int error;
    pid_t new_pid;

    assert(d->fail_act == DIRTY_RESTART);
    m = module_from_daemon(d);
    
    kill_daemon_initializer(d);
    if (d->pid != 0)
        kill_and_reap_daemon(d);
    if (error != 0)
        return (error);
    atomic_store(&d->status, STARTING);
    new_pid = spawn_daemon_proc(m, d);
    if (new_pid == -1)
        return (-1);

    d->pid = new_pid;
    return (0);
}

static int
spawn_daemon_initializer(struct ukernel_daemon *daemon)
{
    daemon_setup_info *setup_info;
    daemon_setup_state *setup_state;
    function_map_t *init_map;
    char *init_lookup;

    setup_info = daemon->setup_info;
    if (daemon->setup_state == NULL) {
        setup_state = calloc(1, sizeof(daemon_setup_state));
        daemon->setup_state = setup_state;
    } else 
        setup_state = daemon->setup_state;

    //TODO-PBB: this, and the worker spawning functions in libcocall
    //need a major cleanup, esp. WRT failure modes.
    if (setup_info->setup != NULL) {
        init_map = spawn_slow_worker(NULL, setup_info->setup, NULL);
        setup_state->setup_worker = &init_map->workers[0];
        free(init_map);
    }
    
    if (setup_info->done != NULL) {
        init_map = spawn_slow_worker(NULL, setup_info->done, NULL);
        setup_state->done_worker = &init_map->workers[0];
        free(init_map);
    }

    return (0);
}

static void
set_daemon_args(char **argv, struct ukernel_module *m, struct ukernel_daemon *d)
{
    argv[0] = strdup(d->exec_path);
    if (((d->flags & SETPGRP) != 0) && ((d->flags & LEADER) == 0)) {
        argv[1] = calloc(12, sizeof(char));
        sprintf(argv[1], "%d", m->daemons[0].pid);  /* module pgrp leader */
        argv[2] = NULL;
    } else
        argv[1] = NULL;
}

static void **
build_daemon_capvec(struct ukernel_module *m, struct ukernel_daemon *d)
{
    void **capv;
    struct coexecve_capvec *capvec;
    ssize_t n_caps;

    n_caps = d->setup_info->n_caps + 2; /* allocate space for setup and done too */
    capvec = capvec_allocate(n_caps);

    if (d->setup_state->setup_worker != NULL)
        capvec_append(capvec, d->setup_state->setup_worker->scb_cap);
    if (d->setup_state->done_worker != NULL)
        capvec_append(capvec, d->setup_state->done_worker->scb_cap);
    if (d->setup_info->get_capv != NULL)
        d->setup_info->get_capv(capvec);
    capv = capvec_finalize(capvec);
    capvec_free(capvec);

    return (capv);
}

static void
free_daemon_args(char **argv)
{
    int i;

    for (i = 0; argv[i] != NULL; i++)
        free(argv[i]);
}

static pid_t
spawn_daemon_proc(struct ukernel_module *module, struct ukernel_daemon *daemon)
{
    char *daemon_args[3];
    void **capv;
    pid_t pid, daemon_pid;
    int status;
    int error;

    set_daemon_args(daemon_args, module, daemon);
    capv = build_daemon_capvec(module, daemon);
    daemon_pid = rfork(RFSPAWN);
    if (daemon_pid == 0) {
        error = coexecvec(process_manager->pid, daemon_args[0], daemon_args, environ, capv);
        _exit(EX_UNAVAILABLE); /* Should not reach unless error */
    } else if (daemon_pid == -1) {
        warn("%s: rfork failed", __func__);
        return (-1);
    }
    free_daemon_args(daemon_args);
    free(capv);

    /* check that the daemon actually started */
    pid = waitpid(daemon_pid, &status, (WNOHANG | WEXITED));
    if (pid > 0) {
        assert(WIFEXITED(status));
        errno = ECHILD;
        return (-1);
    } else if (pid == -1)
        return (-1);

    return (daemon_pid);
}

