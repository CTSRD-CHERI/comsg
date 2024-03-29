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
#include "procdeath.h"
#include "coevent_utils.h"
#include "cocallback_func_utils.h"

#include <comsg/coevent.h>
#include <cocall/tls_cocall.h>

#include <err.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sysexits.h>
#include <sys/errno.h>
#include <sys/event.h>
#include <sys/types.h>
#include <unistd.h>

#define UNUSED(x) (void)(x)

#define PROCDEATH_FLAGS (EV_ADD | EV_ENABLE | EV_ONESHOT)
#define PROCDEATH_EVENT_INIT(event, pid, data) do {	\
		EV_SET(event, pid, EVFILT_PROC, PROCDEATH_FLAGS, NOTE_EXIT, 0, data); \
	} while(0)

static int procdeath_kq = -1;

int
monitor_proc(coevent_t *proc)
{
	int error;
	struct kevent proc_event;
	/* 
	 * Cocalls have a property that we leverage here to avoid the 
	 * classic race conditions inherent to using PIDs. The subject
	 * of the monitoring performs everything up to kevent(2) when
	 * the unborrowing happens. If the subject terminates before 
	 * unborrowing, the system call is never made. If we get far
	 * enough to unborrow and run the call, it seems like a matter
	 * for the kernel to do the right thing. If we install the
	 * the event before the process is reaped, it'll be handled as
	 * normal.
	 * 
	 * Installation of the event should be done before its handler
	 * would need to do anything, in case the subject process dies
	 * before reaching kevent(2).
	 */
	PROCDEATH_EVENT_INIT(&proc_event, proc->ce_pid, proc);
	error = kevent(procdeath_kq, &proc_event, 1, NULL, 0, NULL);
	/* we are now unborrowed */
	if (error == -1) {
		switch (errno) {
		case ESRCH:
			/*unborrow happened, but process has died and been reaped*/
			/* coaccepting now will cause us to die, so we should spawn a new coaccept thread and exit */
			break;
		case EINTR:
			//changes were applied, we're fine
			break;
		default:
			err(EX_SOFTWARE, "%s: kevent(2) failed", __func__);
			break;
		}
	}
	return (error);
}

static void
init_monitoring(void)
{
	int error;
	pid_t parent;
	struct kevent coprocd_dies;

	procdeath_kq = kqueue();
	if (procdeath_kq == -1)
		err(EX_SOFTWARE, "%s: kqueue(2) failed", __func__);

	parent = getppid();
	PROCDEATH_EVENT_INIT(&coprocd_dies, parent, NULL);

	error = kevent(procdeath_kq, &coprocd_dies, 1, NULL, 0, NULL);
	if (error == -1)
		err(EX_SOFTWARE, "%s: failed to add coprocd to kqueue", __func__);
}

static void
cocallback_monitoring(void)
{
	close(procdeath_kq);
}

static int
execute_cocallback(struct cocallback *cocallback)
{
	struct cocallback_func *func;
	int error;

	func = cocallback->func;
	if ((func->flags & FLAG_PROVIDER) != 0) {
		error = flag_dead_provider(cocallback);
	} else if ((func->flags & FLAG_DEAD) != 0) {
		error = 1;
	} else if ((func->flags & FLAG_SLOCALL) != 0) {
		error = slocall_tls(func->scb, cocallback->args.cocall_data, cocallback->args.len);
	} else {
		error = cocall_tls(func->scb, cocallback->args.cocall_data, cocallback->args.len);
	}
	return (error);
}

static void
handle_coprocd_death(void)
{
	_exit(0);
}

static int
trigger_cocallbacks(struct coevent *proc)
{
	int error, cocallbacks;
	struct cocallback *notify;
	
	if (proc == NULL)
		handle_coprocd_death();
	cocallbacks = 0;
	atomic_store_explicit(&proc->in_progress, 1, memory_order_release);
	while ((notify = get_next_cocallback(proc)) != NULL) {
		error = execute_cocallback(notify);
		if (error == -1)
			err(EX_SOFTWARE, "%s: cocallback failed", __func__);
		else { 
			if (error == 1)
				warn("%s: dead cocallback provider", __func__);
			cocallbacks++;
		}
		free_cocallback(notify);
	};
	atomic_store_explicit(&proc->in_progress, -1, memory_order_release);

	return (cocallbacks);
}

void *
handle_proc_events(void *argp)
{
	int error;
	int nevents, idx;
	int max_events;
	struct kevent *events;
	UNUSED(argp);

	init_monitoring();
	max_events = 1;
	events = calloc(max_events, sizeof(struct kevent));
	for (;;) {
		nevents = kevent(procdeath_kq, NULL, 0, events, max_events, NULL);
		if (nevents == -1) {
			switch (errno) {
			case EINTR:
				break;
			default:
				err(EX_SOFTWARE, "%s: kevent failed", __func__);
				break; /*NOTREACHED*/
			}
		}
		//get list of those who should be notified
		//do notification (i.e. execute cocallbacks)
		for (idx = 0; idx < nevents; idx++) {
			error = trigger_cocallbacks(events[idx].udata);
		}
		while (nevents >= max_events) {
			max_events = max_events * 2;
			events = realloc(events, (max_events * sizeof(struct kevent)));
		};
	}
	
	cocallback_monitoring();
	return (NULL);
}