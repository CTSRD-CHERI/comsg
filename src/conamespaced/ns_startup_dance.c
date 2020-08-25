/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
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
#include "namespace_table.h"

#include "ukern/cocall_args.h"
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#if 0

#ifndef timevalcmp
#define	timevalcmp(tvp, uvp, cmp)					\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?				\
	    ((tvp)->tv_usec cmp (uvp)->tv_usec) :			\
	    ((tvp)->tv_sec cmp (uvp)->tv_sec))
#endif

struct _proc_namespace {
	TAILQ_ENTRY(_proc_namespace) entries;
	namespace_t *ns;
	pid_t pid;
	struct timeval start_time;
};

static struct {
	TAILQ_HEAD(, _proc_namespace) procs;
	_Atomic size_t nprocs;
} proc_table;

static struct kinfo_proc *proc_list;

struct _proc_namespace *find_proc(pid_t pid)
{
	struct _proc_namespace *entry;
	entry = TAILQ_LAST(proc_table, _proc_namespace);
	while()
		if (pid == entry.pid) 
			return (entry);
	return (NULL);
};

void add_new_proc(pid_t pid, namespace_t *ns)
{
	struct _proc_namespace *new_proc = cocall_calloc(1, sizeof(struct _proc_namespace));
	new_proc->pid = pid;
	new_proc->ns = ns;
	new_proc->start_time = get_starttime(pid);
	atomic_fetch_add(&proc_table.nprocs, 1);
	return;
}

void replace_old_proc(struct _proc_namespace *old_proc, pid_t pid, namespace_t *ns, struct timeval *starttime)
{
	struct _proc_namespace *new_proc = cocall_calloc(1, sizeof(struct _proc_namespace));
	new_proc->pid = pid;
	new_proc->ns = ns;
	new_proc->start_time = get_starttime(pid);
	atomic_fetch_add(&proc_table.nprocs, 1);
	return;
}

#endif

/*
 * There is a race condition inherent in use of pids to identify processes.
 * Handling this is a question of correctness. 
 */
void coproc_init(cocall_args_t * cocall_args, void *token)
{
	static namespace_t *global_ns = get_global_namespace();

	/*
 	 * This function is not as intended. More than one cap to the global ns should
 	 * not be given to a process. More than one process-local namespace should not 
	 * be created per process.
 	 */

	cocall_args->global = global_ns;
	cocall_args->status = 0;
	cocall_args->error = 0;
	return;
}

