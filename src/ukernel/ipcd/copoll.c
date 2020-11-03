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
#include "copoll.h"

#include "ipcd_cap.h"
#include "copoll_utils.h"

#include <ccmalloc.h>
#include <cocall/cocall_args.h>
#include <coproc/utils.h>

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/queue.h>

int 
validate_copoll_args(copoll_args_t *cocall_args)
{
	size_t i;
	for (i = 0; i < cocall_args->ncoports; i++) {
		if (!valid_cocarrier(cocall_args->coports[i].coport))
			return (0);
	}
	return (1);
}

static int 
inspect_events(pollcoport_t *coports, size_t ncoports)
{
	size_t i;
	int matched_events;
	coport_t *cocarrier;

	matched_events = 0;
	for (i = 0; i < ncoports; i++) {
		coports[i].coport = unseal_coport(coports[i].coport);
		coports[i].revents = (coports[i].events & coports[i].coport->info->event);
		if (coports[i].revents != NOEVENT)
			matched_events++;
	}
	return (matched_events);
}

static void 
populate_revents(pollcoport_t *user, pollcoport_t *ukern, uint ncoports)
{
	uint i;
	for (i = 0; i < ncoports; i++)
		user[i].revents = ukern[i].revents;
}


/* 
 * Cocarriers call into the microkernel. Others do not. There are several reasons, among which:
 * 1. We control cocarrier use more tightly to enable event delivery
 * 2. We need 1 microkernel thread per user thread to service cocalls, and have to economise somewhere
 */
static coport_listener_t **
init_listeners(pollcoport_t *coports, uint ncoports, pthread_cond_t *cond)
{
	coport_listener_t **listen_entries;
	uint i;
	
	listen_entries = cocall_calloc(ncoports, CHERICAP_SIZE);
	for (i = 0; i < ncoports; i++) {
		listen_entries[i] = malloc(sizeof(coport_listener_t));
		listen_entries[i]->wakeup = cond;
		listen_entries[i]->revent = NOEVENT;
		listen_entries[i]->events = coports[i].events;
	}
	return (listen_entries);
}


static int 
wait_for_events(pollcoport_t *coports, uint ncoports, long timeout)
{
	coport_t *coport;
	coport_listener_t **listen_entries;
	pthread_cond_t wake;
	pthread_condattr_t wake_attr;
	coport_status_t status;
	int matched;
	uint i;

	pthread_condattr_init(&wake_attr);
	pthread_condattr_setclock(&wake_attr, CLOCK_MONOTONIC);
	pthread_cond_init(&wake, &wake_attr);

	listen_entries = init_listeners(coports, ncoports, &wake);

	acquire_copoll_mutex();
	for (i = 0; i < ncoports; i++) {
		coport = unseal_coport(coports[i].coport);
		status = COPORT_OPEN;
		//TODO-PBB: An area where better waiting could possibly be used
		while(!atomic_compare_exchange_weak_explicit(&coport->info->status, &status, COPORT_BUSY, memory_order_acq_rel, memory_order_relaxed))
			status = COPORT_OPEN;
		LIST_INSERT_HEAD(&coports[i].coport->cd->listeners, listen_entries[i], entries);
		atomic_store_explicit(&coport->info->status, COPORT_OPEN, memory_order_release);
	}

	copoll_wait(&wake, timeout);

	for (i = 0; i < ncoports; i++) {
		coport = unseal_coport(coports[i].coport);
		status = COPORT_OPEN;
		//TODO-PBB: An area where better waiting could possibly be used
		while(!atomic_compare_exchange_weak_explicit(&coport->info->status, &status, COPORT_BUSY, memory_order_acq_rel, memory_order_relaxed))
			status = COPORT_OPEN;
		LIST_REMOVE(listen_entries[i], entries);
		atomic_store_explicit(&coport->info->status, COPORT_OPEN, memory_order_release);
	}
	pthread_cond_destroy(&wake);
	release_copoll_mutex();

	matched = 0;
	for (i = 0; i < ncoports; i++) {
		coports[i].revents = listen_entries[i]->revent;
		if (coports[i].revents != NOEVENT)
			matched++;
		free(listen_entries[i]);
	}
	free(listen_entries);

	assert(matched != 0 || timeout > 0);

	return (matched);
}


void 
cocarrier_poll_slow(cocall_args_t *cocall_args, void *token)
{
	UNUSED(token);
	uint ncoports; 
	int matched;
	size_t target_len;

	ncoports = cocall_args->ncoports;
	target_len = ncoports * sizeof(pollcoport_t);

	pollcoport_t *targets = cocall_malloc(target_len);
	memcpy(targets, cocall_args->coports, target_len); //copyin

	matched = inspect_events(targets, ncoports);
	/* 
	 * If the caller is willing to wait and we have no matches on the events polled for,
	 * then wait for the specified amount of time.
	 */
	if (cocall_args->timeout != 0 && matched == 0) 
		matched = wait_for_events(targets, ncoports, cocall_args->timeout);
	
	populate_revents(cocall_args->coports, targets, ncoports);
	cocall_free(targets);

	COCALL_RETURN(cocall_args, matched);
}

void 
cocarrier_poll(copoll_args_t *cocall_args, void *token)
{
	UNUSED(token);
	uint ncoports; 
	int matched;
	size_t target_len;

	ncoports = cocall_args->ncoports;
	target_len = ncoports * sizeof(pollcoport_t);

	pollcoport_t *targets = cocall_malloc(target_len);
	memcpy(targets, cocall_args->coports, target_len); //copyin

	matched = inspect_events(targets, ncoports);
	if (cocall_args->timeout != 0 && matched == 0) {
		cocall_free(targets);
		COCALL_ERR(cocall_args, EWOULDBLOCK);
	}

	populate_revents(cocall_args->coports, targets, ncoports);
	cocall_free(targets);

	COCALL_RETURN(cocall_args, matched);
}