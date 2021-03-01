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
#ifndef _UKERN_WORKER_H
#define _UKERN_WORKER_H

#include <cocall/cocall_args.h>
#include <coproc/namespace.h>

#include <cheri/cherireg.h>
#include <pthread.h>
#include <stdatomic.h>

typedef struct _worker_args
{
	/* the worker thread */
	pthread_t worker;
	/* 
	 * pointer to a function that: 
	 *  + takes a pointer to struct cocall_args, 
	 *  + modifies it in place,
	 *  + sets status and error values before returning.
	 */
	void (*worker_function)(cocall_args_t *, void *);
	/* 
	 * pointer to a function that:
	 * 	+ takes a pointer to struct cocall_args,
	 * 	+ validates the arguments passed from a cocall
	 * 	+ returns 0 if they fail, else non-zero
	 */
	int (*validation_function)(cocall_args_t *);
	/* name to coregister under */
	char *name;
	/* result of coregister */
	void *scb_cap;
} worker_args_t;

typedef struct _worker_args handler_args_t;

bool start_coaccept_worker(worker_args_t *thread_args);
void *coaccept_worker(void *worker_argp);
bool start_sloaccept_worker(worker_args_t *thread_args);
void *sloaccept_worker(void *worker_argp);

#endif