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
#ifndef _COPROC_H
#define _COPROC_H

#include <cheri/cheri.h>
#include <time.h>

#include "commap.h"
#include "comutex.h"
#include "coport.h"
#include "sys_comsg.h"

//NOTE: I think the _Atomic annotations are unneccessary,
//but as things work I have yet to remove them.
//TODO - Rework so all arg structs follow a standard template including:
// 		 status, error, args, and return


typedef struct _cocall_lookup_t
{
	char target[LOOKUP_STRING_LEN];
	void * __capability cap;
} __attribute__((__aligned__(16))) cocall_lookup_t;

typedef struct _coopen_args_t
{
	coport_type_t type;
	char name[COPORT_NAME_LEN];
} coopen_args_t;

typedef struct _cocall_coopen_t
{
	coopen_args_t args;
	coport_t port; 
	int status;
	int error;
} __attribute__((__aligned__(16))) cocall_coopen_t;

typedef struct _comutex_init_args_t
{
	char name[COMUTEX_NAME_LEN];
} cocall_comutex_init_args_t;

typedef struct _cocall_comutex_init_t
{
	cocall_comutex_init_args_t args;
	_Atomic(comutex_t * __capability) mutex; 
} __attribute__((__aligned__(16))) cocall_comutex_init_t;

typedef struct _colock_args_t
{
	_Atomic(comutex_t * __capability) mutex;
	int result;
} __attribute__((__aligned__(16))) colock_args_t;
typedef struct _colock_args_t counlock_args_t;

typedef struct _cocarrier_send_args_t
{
	coport_t cocarrier;
	void * __capability message;
	int status;
	int error;
} __attribute__((__aligned__(16))) cocall_cocarrier_send_t;

typedef struct _commap_args 
{
	int status;
	int error;
	struct {
		void * __capability cap;
		token_t token;
		int prot;
	};
} commap_args_t;

typedef struct _comunmap_args 
{
	int status;
	int error;
	struct {
		token_t token;
	};
} comunmap_args_t;

typedef struct _pollcoport_t
{
	coport_t coport;
	int events;
	int revents;
} pollcoport_t;


//There is an unsatisfying situation here which arises from the fact that
//cocall/coaccept data lengths must be symmetrical, so to pass a variable 
//length array, even though we can derive its length, we must pass a pointer
//to memory we cannot guarantee will not be touched during the call.
typedef struct _copoll_args_t
{
	pollcoport_t * coports;
	uint ncoports;
	int timeout; 
	int status;
	int error;
} __attribute__((__aligned__(16))) copoll_args_t;

#if 0

struct cocall_args
{
	int status;
	int error;
	union {} retval;
	union 
	{
		struct
		{
			pollcoport_t * coports;
			int ncoports;
			int timeout;
		};
		struct
		{
			_Atomic(coport_t) cocarrier;
			void * __capability message;
		};
		struct
		{
			_Atomic(comutex_t * __capability) mutex;
		};
		struct
		{
			char target[LOOKUP_STRING_LEN];
			void * __capability func_cap;
		};
		struct 
		{
			char name[COMUTEX_NAME_LEN];
		};
		struct
		{
			cocall_comutex_init_args_t args;
			_Atomic(comutex_t * __capability) mutex; 
		};
		struct
		{
			coopen_args_t args;
			_Atomic(coport_t) port; 
		}
	} args;
};

#endif

int ukern_lookup(void *  __capability * __capability code, 
	void * __capability  * __capability data, const char * target_name, 
	void * __capability * __capability target_cap);

#endif