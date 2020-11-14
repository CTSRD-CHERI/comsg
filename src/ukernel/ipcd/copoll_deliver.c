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
#include "copoll_deliver.h"
#include "copoll_utils.h"
#include "coport_table.h"

#include <coproc/coport.h>

#include <assert.h>
#include <pthread.h>

#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>


const size_t n_copoll_notifiers = 4;
static copoll_notifier_t notifiers[n_copoll_notifiers];

static void 
process_coport_event(coport_t *coport)
{
	coport_listener_t *listener, *listener_temp;
	coport_eventmask_t coport_event, listener_mask, revents;
	coport_status_t status;

	if (!cheri_gettag(coport))
		return;
	else if (LIST_EMPTY(&coport->cd->listeners))
		return; //NOTREACHED
	
	status = COPORT_DONE;
	while(!atomic_compare_exchange_strong_explicit(&coport->info->status, &status, COPORT_POLLING, memory_order_acq_rel, memory_order_acquire)) {
		switch(status) {
		case COPORT_CLOSING:
		case COPORT_OPEN:
			break;
		default:
			status = COPORT_DONE;
			break;
		}
	}
	if (coport->info->length == 0 && status == COPORT_CLOSING)
		status = COPORT_CLOSED;
	else 
		status = COPORT_OPEN;

	coport_event = coport->info->event;
	assert((coport_event & coport->cd->levent) != NOEVENT);

	LIST_FOREACH_SAFE(listener, &coport->cd->listeners, entries, listener_temp) {
		listener_mask = listener->events;
		revents = (coport_event & listener_mask);
		if(revents == NOEVENT) 
			continue;
		listener->revent = revents;
		LIST_REMOVE(listener, entries); /* ensure we don't needlessly check this again */
		atomic_store_explicit(&listener->removed, true, memory_order_release);
		//TODO-PBB: replace with copoll_utils variant (?)
		pthread_cond_signal(listener->wakeup);
	}
	coport->cd->levent &= ~coport_event;
	atomic_store_explicit(&coport->info->status, status, memory_order_release);

}

static void *
copoll_deliver(void *argp)
{
	copoll_notifier_t *notifier;
	coport_t *coport;
	size_t i, n_events;
	size_t index;

	notifier = argp;
	acquire_copoll_mutex();
	for (;;) {
		await_copoll_events(&notifier->notifier_wakeup);
		n_events = atomic_load_explicit(&notifier->next_event, memory_order_acquire);
		for (i = 0; i < n_events; i++) {
			coport = notifier->event_queue[i];
			process_coport_event(coport);
			notifier->event_queue[i] = NULL;
		}
	}
	release_copoll_mutex();
}

void 
put_coport_event(coport_t *coport)
{
	copoll_notifier_t *notifier;
	size_t event_idx, notifier_idx;

	notifier_idx = get_coport_notifier_index(coport);
	notifier = &notifiers[notifier_idx];
	event_idx = atomic_fetch_add_explicit(&notifier->next_event, 1, memory_order_acq_rel);
	assert(event_idx < COPORT_EVENTQUEUE_LEN);
	notifier->event_queue[event_idx] = coport;
	pthread_cond_signal(&notifier->notifier_wakeup);
}


void
setup_copoll_notifiers(void)
{
	size_t i;
	pthread_condattr_t notifier_cond_attrs;
	copoll_notifier_t *notifier_args;
	
	memset(notifiers, '\0', cheri_getlen(notifiers));
	pthread_condattr_init(&notifier_cond_attrs);
	for (i = 0; i < n_copoll_notifiers; i++) {
		notifier_args = cheri_setboundsexact(&notifiers[i], sizeof(copoll_notifier_t));
		pthread_cond_init(&notifier_args->notifier_wakeup, &notifier_cond_attrs);
		pthread_create(&notifiers[i].notifier_thread, NULL, copoll_deliver, notifier_args);
	}
}
