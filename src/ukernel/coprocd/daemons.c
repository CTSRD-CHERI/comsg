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

#include "daemons.h"
#include "dance.h"

#include <cocall/worker.h>
#include <cocall/worker_map.h>
#include <coproc/utils.h>

#include <err.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <unistd.h>


//TODO-PBB: get path better than this
static const char *coserviced_path = "/usr/bin/coserviced";
static const char *ipcd_path = "/usr/bin/ipcd";
static const char *nsd_path = "/usr/bin/nsd";

static _Atomic pid_t coserviced = 0;
static _Atomic pid_t ipcd = 0;
static _Atomic pid_t nsd = 0;
static pid_t coprocd = 0;

void *done_worker_scb = NULL;
static function_map_t *done_func_map;
void *done2_worker_scb = NULL;
static function_map_t *done2_func_map;

extern char **environ;

struct respawn_args {
	_Atomic(pid_t) *child_pid;
	char *exec_path;
	char *init_str;
};

static 
void monitor_child(pid_t child_pid)
{
	int status, opt;
	status = 0;
	opt = WEXITED; //implicit

	do {
		child_pid = waitpid(child_pid, &status, opt);
		if (child_pid == -1 ) {
			if (errno == ECHILD)
				return;
			err(errno, "monitor_child: monitoring child process failed");
		} 
	} while(status != WEXITED);

	return;
}

static void 
coredump_daemons(void)
{
	invalidate_startup_info();
	if (coserviced != 0)
		kill(coserviced, SIGSEGV);
	if (ipcd != 0)
		kill(ipcd, SIGSEGV);
	if (nsd != 0)
		kill(nsd, SIGSEGV);
	kill(coprocd, SIGSEGV);
}

static void *
respawn_daemon(void *argp)
{
	_Atomic(pid_t) *pidp;
	pid_t child_pid; 
	struct respawn_args *args = argp;
	char *exec_path, *init_str;
	
	pidp = args->child_pid;
	exec_path = args->exec_path;
	init_str = args->init_str;

	monitor_child(atomic_load(pidp));
	for(;;) {
		#if 0
		if (atomic_load(pidp) == atomic_load(&nsd))
			kill_daemons();
		
		child_pid = fork();
		if(!child_pid)
			coexec_daemon(exec_path, init_str);
		else {
			atomic_store(pidp, child_pid);
			monitor_child(child_pid);
		}
		#else 
		coredump_daemons();
		exit(0);
		#endif
	}
	return (NULL);
}

static 
pid_t spawn_daemon(_Atomic(pid_t) *pidp, const char *exec_path, void *init_func)
{
	worker_args_t wargs;
	char *init_name;
	char *coexecve_args[3];
	function_map_t *init_func_map;
	struct respawn_args *monitor_args;
	pthread_t monitor_thread;
	pid_t child_pid;
	int error;
	
	init_name = malloc(NS_NAME_LEN);
	monitor_args = calloc(1, sizeof(struct respawn_args));
	rand_string(init_name, NS_NAME_LEN);
	//TODO-PBB: Revisit lack of validation if needed
	init_func_map = spawn_slow_worker(init_name, init_func, NULL);

	coexecve_args[0] = strdup(exec_path);
	coexecve_args[1] = strdup(init_name);
	coexecve_args[2] = NULL;
	child_pid = vfork();
	if(child_pid == 0) {
		error = coexecve(coprocd, coexecve_args[0], coexecve_args, environ);
		_exit(errno); /* only reached if error */
	} else {
		atomic_store_explicit(pidp, child_pid, memory_order_release);
		monitor_args->child_pid = pidp;
		monitor_args->exec_path = exec_path;
		monitor_args->init_str = init_name;
		pthread_create(&monitor_thread, NULL, respawn_daemon, monitor_args);
	}
	
	return (child_pid);
}

void kill_daemons(void)
{
	invalidate_startup_info();
	if (coserviced != 0)
		kill(coserviced, SIGTERM);
	if (ipcd != 0)
		kill(ipcd, SIGTERM);
	if (nsd != 0)
		kill(nsd, SIGTERM);
}

void spawn_daemons(void)
{
	coprocd = getpid();
	if (done_worker_scb == NULL) {
		done_func_map = spawn_slow_worker("coproc_init_done1", coproc_init_complete, NULL);
		done_worker_scb = done_func_map->workers[0].scb_cap;
	}
	if (done2_worker_scb == NULL) {
		done2_func_map = spawn_slow_worker("coproc_init_done2", ukern_init_complete, NULL);
		done2_worker_scb = done2_func_map->workers[0].scb_cap;
	}
	spawn_daemon(&nsd, nsd_path, nsd_init);
	spawn_daemon(&coserviced, coserviced_path, coserviced_init);
	spawn_daemon(&ipcd, ipcd_path, ipcd_init);
	
	return;
}

pid_t get_coserviced_pid(void)
{
	return (atomic_load(&coserviced));
}

pid_t get_ipcd_pid(void)
{
	return (atomic_load(&ipcd));
}

pid_t get_nsd_pid(void)
{
	return (atomic_load(&nsd));
}