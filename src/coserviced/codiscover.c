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
#include "coservice_table.h"

#include "ukern/cocall_args.h"
#include "ukern/coservice.h"
#include "ukern/utils.h"

#include <errno.h>

#define TOKEN_CACHE_LEN 64

static void *recent_tokens[TOKEN_CACHE_LEN];

static void 
add_token(void *token)
{
	static int next_token = 0;
	recent_tokens[next_token] = token;
	next_token++;
}

static 
int check_token(void *token)
{
	for(int i = 0; i < TOKEN_CACHE_LEN; i++)
	{
		if (token == recent_tokens[i])
			return (1)
		else if (recent_tokens[i] == 0)
			break;
	}
	return (0);
}

__attribute__ ((constructor)) static 
void init_recent_tokens(void)
{
	memset(&recent_tokens, 0, sizeof(recent_tokens)); //?
}


int validate_codiscover_args(codiscover_args_t *args)
{
	return (1);
}

void discover_coservice(codiscover_args_t *cocall_args, void *token)
{
	UNUSED(token);

	int index, new_index;
	coservice_t *service;

	nsobject_t *service_obj = coselect(cocall_args->ns_cap, cocall_args->target, COSERVICE);
	if(service == NULL)
	{
		cocall_args->status = -1;
		/* 
		 * this is not correct, and needs reworking. the true error cause might
		 * be invalid permissions, or that the ns daemon has died.
		 */
		cocall_args->error = ENOENT; 
		return;
	}
	
	service = service_obj->coservice;
	cocall_args->cap = get_coservice_scb(service);
	cocall_args->status = 0;
	cocall_args->error = 0;

	return;
}

void discover_codiscover(codiscover_args_t *cocall_args, void *token)
{
	if (strncmp(cocall_args->target, CODISCOVER, LOOKUP_STRING_LEN) != 0) {
		if(check_token(token)) {
			cocall_args->status = -1;
			cocall_args->error = EBUSY;
			return;
		}
	}
	discover_coservice(cocall_args, token);
	add_token(token);

	return;
}