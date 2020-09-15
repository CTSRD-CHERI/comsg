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
#ifndef UKERN_REQUESTS_H
#define UKERN_REQUESTS_H

#include "sys_comsg.h"
#include "ukern_params.h"

#include <pthread.h>

typedef struct _worker_args_t 
{
	char name[LOOKUP_STRING_LEN];
	void * __capability cap;
} worker_args_t;

typedef struct _worker_map_entry_t
{
	char func_name[LOOKUP_STRING_LEN];
	worker_args_t workers[WORKER_COUNT];
} worker_map_entry_t;

typedef struct _request_handler_args_t
{
	char func_name[LOOKUP_STRING_LEN];
} request_handler_args_t;

void update_worker_args(worker_args_t * args, const char * function_name);
void *manage_requests(void *args);
int spawn_workers(void * func, pthread_t * threads, const char * name);
int coaccept_init(
    void * __capability * __capability  code_cap,
    void * __capability * __capability  data_cap, 
    const char * target_name,
    void * __capability * __capability target_cap);

extern worker_map_entry_t worker_map[U_FUNCTIONS];
extern worker_map_entry_t private_worker_map[UKERN_PRIV];

#endif