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
#include <assert.h>
#include <err.h>
#include <coproc/coevent.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/types.h>

static pid_t pid_max;
static struct coevent *proc_table;

void
setup_procdeath_table(void)
{
	size_t pid_size;
	int error;

	pid_size = sizeof(pid_max);
	error = sysctlbyname("kern.pid_max", &pid_max, &pid_size, NULL, 0);
	if (error == -1) {
		warn("setup_table: could not get pid_max via sysctl, defaulting to 99999");
		pid_max = 99999;
	}
	proc_table = calloc(pid_max, sizeof(coevent_t));
	assert(proc_table != NULL);
}

coevent_t *
allocate_procdeath_event(pid_t pid)
{
	coevent_t *proc;
	int in_progress;

	in_progress = 0;
	proc = &proc_table[pid];

	if (!atomic_compare_exchange_strong_explicit(&proc->in_progress, &in_progress, 1, memory_order_acq_rel, memory_order_acquire))
		return (NULL);

	if (proc->ce_pid != pid) {
		proc->event = PROCESS_DEATH;
		proc->ce_pid = pid;
		proc->ncallbacks = 0;
		SLIST_INIT(&proc->cocallbacks);
	}

	atomic_store_explicit(&proc->in_progress, 0, memory_order_release);
	return (proc);
}




