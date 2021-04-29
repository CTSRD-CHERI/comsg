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
#ifndef _UKERN_DAEMON_H
#define _UKERN_DAEMON_H

#include <cocall/capvec.h>
#include <cocall/cocall_args.h>
#include <cocall/worker.h>

#include <stdatomic.h>
#include <sys/types.h>
#include <sys/syslimits.h>

struct ukernel_daemon;
struct ukernel_module;

//chosen for alignment reasons
//if you change this, change the alignment of modules[] 
//and contents of module_from_daemon() to match.
#define MAX_MODULE_DAEMONS (12) 

/* 
 * Cocall provided bootstrapper. May vanish soon. 
 * Each daemon may have two: "setup", and "done", which
 * commence startup and signal its completion respectively.
 * Setup usage is expected to be uncommon, with the recommended
 * pattern being capvec + done, with prior dependencies marked
 * synchronous.
 */
typedef void (*daemon_init_func)(cocall_args_t *, void *);
/* Puts module/daemon specific caps into supplied array */
typedef void (*daemon_capv_func)(struct coexecve_capvec *);
/* 
 * Performs actions required for bootstrappers to work.
 * Called on module start, clean_restart, or on_demand start
 * after clean exit.
 * Argument is pointer to module's entry in module table.
 */
typedef void (*module_init_func)(struct ukernel_module *);
/*
 * Called to teardown module specific data
 * Required for ON_DEMAND modules, or if any daemon uses CLEAN_RESTART
 * Should clean up state from a module exiting, leaving a state that 
 * is valid for a subsequent call to the modules init function.
 */
typedef void (*module_fini_func)(void);

/* Defines behaviour if startup fails. */
typedef enum {CORE, PERIPHERAL} module_type;
typedef enum {DEFERRED} module_flags;
/*
 * daemon_failure_mode specifies preferred action if a daemon dies
 *
 * HCF - kill entire microkernel; only available if module has type CORE
 * KILL - kill entire module and emit error message
 * DIRTY_RESTART - daemon can be restarted without bother
 * CLEAN_RESTART - entire module must be restarted
 * CONTINUE - do nothing
 */
typedef enum {HCF, KILL, DIRTY_RESTART, CLEAN_RESTART, CONTINUE,} daemon_failure_mode;
/*
 * daemon_flags contains information about the daemon coprocd should know
 *
 * SETPGRP      will not be in coprocd's process group
 * LEADER       Leader of the process group for a module of SETPGRP daemons.
 *              Only one daemon per module should set this flag, which should 
 *              be at index 0 in the array of daemons. If this is set, SETPGRP
 *              must also be set for all daemons in the module.
 * SETUID       is a setuid executable
 * ON_DEMAND    daemon should not be started at module init
 * SYNCHRONOUS  should wait until daemon state is CONTINUING or RUNNING before 
 *              starting next daemon in the module
 */
typedef enum {SETPGRP, LEADER, SETUID, ON_DEMAND, SYNCHRONOUS} daemon_flags;
typedef enum {PENDING, STARTING, CONTINUING, RUNNING, KILLED, DIED} daemon_status;
typedef daemon_status module_status;


/* Contains static information about this dameon's coprocd setup routines */
typedef struct {
    daemon_init_func setup;
    daemon_init_func done;
    daemon_capv_func get_capv;
    ssize_t n_caps; /* not including SCBs to setup and done */
} daemon_setup_info;

/* Contains dynamic information about this daemon's instantiated setup routines */
typedef struct {
    worker_args_t *setup_worker;
    worker_args_t *done_worker;
} daemon_setup_state;

typedef struct {
    module_init_func start;
    module_fini_func complete;
} module_setup_info;

typedef struct {
    module_fini_func start;
    module_fini_func complete;
} module_teardown_info;

struct ukernel_daemon {
    char *name;
    char *exec_path;
    daemon_setup_info *setup_info;
    daemon_setup_state *setup_state;
    _Atomic pid_t pid;
    daemon_failure_mode fail_act;
    daemon_flags flags;
    daemon_status status;
};

struct ukernel_module {
    char *module_name;
    module_type type;
    int ndaemons;
    module_status status;
    module_flags flags; 
    module_setup_info *init;
    module_teardown_info *fini;
    struct ukernel_daemon daemons[MAX_MODULE_DAEMONS];
};

#define DAEMON(NAME, PATH, INIT_INFO, FAIL, FLAGS) \
    { #NAME, PATH, INIT_INFO, NULL, 0, FAIL, FLAGS, PENDING }

#define MODULE(NAME, TYPE, N_DAEMONS, FLAGS, INIT, FINI, DAEMON_ENUM, DAEMON_GEN) \
    { #NAME, TYPE, N_DAEMONS, PENDING, FLAGS, INIT, FINI, { DAEMON_ENUM(DAEMON_GEN) } }

void cleanup_daemon(struct ukernel_daemon *);
int init_daemon(struct ukernel_module *, struct ukernel_daemon *);
void kill_and_reap_daemon(struct ukernel_daemon *);
int restart_daemon(struct ukernel_daemon *d);

#endif //!defined(_UKERN_DAEMON_H)