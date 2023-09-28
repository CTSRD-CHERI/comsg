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
#include "procdeath_tbl.h"

#include "procdeath.h"
#include <cheri/cheric.h>
#include <assert.h>
#include <err.h>
#include <comsg/coevent.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/mman.h>

pid_t pid_max;
static coevent_t *proc_table = NULL;

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
	madvise(NULL, -1, MADV_PROTECT);
	proc_table = calloc(pid_max, sizeof(coevent_t));
	assert(proc_table != NULL);
}

coevent_t *
allocate_procdeath_event(pid_t pid)
{
	coevent_t *proc;
	int in_progress, error;

	/* pid should be validated before calling this */
	proc = cheri_setboundsexact(&proc_table[pid], sizeof(coevent_t));

	in_progress = 0;
	if (!atomic_compare_exchange_strong_explicit(&proc->in_progress, &in_progress, 1, memory_order_acq_rel, memory_order_acquire)) {
		in_progress = -1; 
		if (!atomic_compare_exchange_strong_explicit(&proc->in_progress, &in_progress, 1, memory_order_acq_rel, memory_order_acquire))
			return (NULL);
	}

	if (proc->ce_pid != pid || in_progress == -1) {
		proc->event = PROCESS_DEATH;
		proc->ce_pid = pid;
		proc->ncallbacks = 0;
		STAILQ_INIT(&proc->callbacks);
		error = monitor_proc(proc); /* starting monitoring here makes it easier to avoid races */
	}

	atomic_store_explicit(&proc->in_progress, 0, memory_order_release);
	return (proc);
}

bool
is_procdeath_table_member(void *ptr)
{
	return (cheri_is_address_inbounds(proc_table, (vaddr_t)ptr));
}

bool
event_inited(pid_t pid)
{
	coevent_t *proc;
	/* pid should be validated before calling this */
	proc = &proc_table[pid];
	return (proc->ce_pid == pid);
}