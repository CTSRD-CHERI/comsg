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
#include <cocall/cocalls.h>
#include <cocall/tls_cocall.h>

#include <err.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <unistd.h>

typedef struct call_set {
	void **target_caps;
	int ncalls;
} call_set_t;

static int default_ncalls = 32;
static pthread_key_t last_key = -1;
static int last_ncalls = 0;

static call_set_t global_set;

__attribute__ ((constructor)) static 
void init_global_set(void)
{
	global_set.target_caps = calloc(default_ncalls, sizeof(call_set_t));
	global_set.ncalls = default_ncalls;
}

pthread_key_t allocate_target_set(void)
{
	pthread_key_t tss_key;
	int error = pthread_key_create(&tss_key, free);
	if (error)
		err(error, "allocate_target_set: failed to allocate thread-specific storage");
	return (tss_key);
}

void init_target_set(pthread_key_t set_key, int ncalls)
{
	int error;
	call_set_t *set;

	if (ncalls <= 0){
		if (set_key == last_key)
			ncalls = last_ncalls;
		else
			ncalls = default_ncalls;
	}
	else if(default_ncalls < ncalls)
		default_ncalls = ncalls;

	if (pthread_getspecific(set_key) != NULL)
		err(EINVAL, "init_cocall_targets: cocall targets already initialised for current thread");

	last_ncalls = ncalls;
	last_key = set_key;

	set = calloc(1, sizeof(call_set_t));
	set->ncalls = ncalls;
	set->target_caps = calloc(ncalls, sizeof(void *));

	error = pthread_setspecific(set_key, set);
	if (error)
		err(error, "init_cocall_targets: failed to assign value to TSS");
	return;
}

void set_cocall_target(pthread_key_t set_key, int target_func, void *target_cap)
{
	call_set_t *set = pthread_getspecific(set_key);
	if (set == NULL) {
		init_target_set(set_key, 0); //defaults to size of largest set
		set = pthread_getspecific(set_key);
	}
	set->target_caps[target_func] = target_cap;
	if (global_set.target_caps[target_func] == NULL)
		global_set.target_caps[target_func] = target_cap;
	return;
}

void *get_global_target(int target_func)
{
	return (global_set.target_caps[target_func]);
}

void *get_cocall_target(pthread_key_t set_key, int target_func)
{
	call_set_t *set = pthread_getspecific(set_key);
	if (set == NULL) {
		init_target_set(set_key, 0); //defaults to size of largest set
		set = pthread_getspecific(set_key);
	}

	if (set->target_caps[target_func] != NULL)
		return (set->target_caps[target_func]);
	else
		return (NULL);
}

int targeted_slocall(pthread_key_t set_key, int target, void *buf, size_t len)
{
	void *target_cap = get_cocall_target(set_key, target);
	if (target_cap == NULL)
		return (-1);
	return (slocall_tls(target_cap, buf, len));
}

int targeted_cocall(pthread_key_t set_key, int target, void *buf, size_t len)
{
	void *target_cap = get_cocall_target(set_key, target);
	if (target_cap == NULL)
		return (-1);
	return (cocall_tls(target_cap, buf, len));
}