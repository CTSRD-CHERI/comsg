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

#include "sys_comsg.h"

struct coproc_init {
	void *codiscover;
	void *coinsert;
};


struct coinsert
{
	char name[NS_NAME_LEN];
	nsobject_type_t type;
	union {
		void *obj;
		coservice_t *coservice;
		coport_t *coport;
	};
	nsobject_t *nsobj;
};

struct coselect
{
	char name[NS_NAME_LEN];
	nsobject_type_t type;
	nsobject_t *nsobj;
};

struct codiscover
{
	char target[LOOKUP_STRING_LEN];
	//XXX-PBB: Which of these 3 do we actually want?
	coservice_t *service; /* would be sealed */
	//nsobject_t *service_obj; /*belongs in coselect*/
	void *scb_cap;
};

struct coprovide
{
	char name[LOOKUP_STRING_LEN];
	void **worker_scbs;
	int nworkers;
	nsobject_t *nsobj;
};

struct coopen
{
	coport_type_t type;
	char name[COPORT_NAME_LEN];
	coport_t *port; 
};

struct cocarrier_send
{
	coport_t *cocarrier;
	void *message;
};

struct commap 
{
	void *mapped;
	token_t token;
	int prot;
};

struct comunmap 
{
	token_t token;
};


//There is an unsatisfying situation here which arises from the fact that
//cocall/coaccept data lengths must (sort of) be symmetrical, so to pass a variable 
//length array, even though we can derive its length, we must pass a pointer
//to memory we cannot guarantee will not be touched or unmapped during the call.
typedef struct _pollcoport_t
{
	coport_t *coport;
	int events;
	int revents;
} pollcoport_t;

struct copoll
{
	pollcoport_t *coports;
	uint ncoports;
	int timeout; 
};

struct cocall_args
{
	int status;
	int error;
	namespace_t * ns_cap;
	union 
	{
		struct coselect;
		struct coinsert;
		/* copoll */
		struct copoll;
		/*cocarrier send/recv*/
		struct cocarrier_send;
		/* ukernel function/service lookup */
		struct codiscover;
		//
		struct coprovide;
		/*coopen*/
		struct coopen;
		//
		struct comunmap;
		struct commap;

		// Microkernel only
		struct coproc_init;

	};
} __attribute__((__aligned__(16)));

typedef struct cocall_args cocall_args_t;
typedef struct cocall_args copoll_args_t;
typedef struct cocall_args cocarrier_send_args_t;
typedef struct cocall_args codiscover_args_t;
typedef struct cocall_args coprovide_args_t;
typedef struct cocall_args coopen_args_t;
typedef struct cocall_args comunmap_args_t;
typedef struct cocall_args commap_args_t;
typedef struct cocall_args coselect_args_t;
typedef struct cocall_args coinsert_args_t;
typedef struct cocall_args coproc_init_args_t;