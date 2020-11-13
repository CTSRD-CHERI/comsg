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
#include "copoll_utils.h"
#include "copoll_deliver.h"
#include <coproc/coport.h>

#include <err.h>
#include <sys/errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <unistd.h>

static pthread_mutex_t global_copoll_lock;
static pthread_cond_t global_cosend_cond;

static pthread_cond_t *coport_event_conds;



__attribute__((constructor)) static void 
init_copoll_lock(void)
{
	pthread_mutexattr_t global_mtx_attr;
	pthread_condattr_t global_cond_attr;
	
	pthread_mutexattr_init(&global_mtx_attr);
	pthread_condattr_init(&global_cond_attr);

	pthread_mutexattr_settype(&global_mtx_attr, PTHREAD_MUTEX_ERRORCHECK);

	pthread_mutex_init(&global_copoll_lock, &global_mtx_attr);
	pthread_cond_init(&global_cosend_cond, &global_cond_attr);
}

/*
 * TODO-PBB: have a single thread wait on the global condition
 * that wakes up the approriate thread rather than waking them all up.
 */
	
void 
acquire_copoll_mutex(void)
{
	pthread_mutex_lock(&global_copoll_lock);
}

void 
release_copoll_mutex(void)
{
	pthread_mutex_unlock(&global_copoll_lock);
}

void 
copoll_wait(pthread_cond_t *wait_cond, long timeout)
{
	struct timespec wait_time, curtime;
	int locked;

	locked = pthread_mutex_lock(&global_copoll_lock);
	if (locked != EDEADLK)
		err(locked, "copoll_wait: lock acquisition failed");
	if (timeout > 0) {
		wait_time.tv_sec = timeout / 1000;
		wait_time.tv_nsec = (timeout % 1000) * 1000000;
		clock_gettime(CLOCK_MONOTONIC, &curtime);
		timespecadd(&wait_time, &curtime, &wait_time);
		pthread_cond_timedwait(wait_cond, &global_copoll_lock, &wait_time);
	}
	else 
		pthread_cond_wait(wait_cond, &global_copoll_lock);
	/* If we locked from within this function, */
	if (locked == 0)
		pthread_mutex_unlock(&global_copoll_lock);
}

void 
await_copoll_events(pthread_cond_t *wait_cond)
{
	pthread_cond_wait(&wait_cond, &global_copoll_lock);
}

void 
copoll_notify(coport_t *cocarrier, coport_eventmask_t event)
{

	coport_status_t status;
	if(!LIST_EMPTY(&cocarrier->cd->listeners) && ((cocarrier->cd->levent & event) != NOEVENT)) {
		acquire_copoll_mutex();
        put_coport_event(cocarrier);
    	release_copoll_mutex(); 
    } else {
    	/* 
    	 * If there are no listeners for this coport, or for this event,
    	 * then we need to perform the status change ourselves. In future, there might
    	 * be cases where the status could have changed, e.g. to COPORT_CLOSED, since
    	 * we set it to COPORT_DONE, so we use cmpxchg rather than a blind store.
    	 */
    	status = COPORT_DONE;
    	atomic_compare_exchange_strong_explicit(&cocarrier->info->status, &status, COPORT_OPEN, memory_order_release, memory_order_relaxed);
    }
	
}


