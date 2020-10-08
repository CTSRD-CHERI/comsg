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
#ifndef _COCALL_ARGS_H
#define _COCALL_ARGS_H

#include <coproc/coport.h>
#include <coproc/coservice.h>
#include <coproc/namespace.h>
#include <coproc/namespace_object.h>

#ifndef COCALL_ERR
#define COCALL_ERR(c, n) do { c->status = (-1);\
	c->error = (n);\
	return; } while(0)
#endif

#ifndef COCALL_RETURN
#define COCALL_RETURN(c, n) do { c->status = (n);\
	c->error = (0);\
	return; } while(0)
#endif

#if 0
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
#endif

//There is an unsatisfying situation here which arises from the fact that
//cocall/coaccept data lengths must (sort of) be symmetrical, so to pass a variable 
//length array, even though we can derive its length, we must pass a pointer
//to memory we cannot guarantee will not be touched or unmapped during the call.
typedef struct _pollcoport_t
{
	coport_t *coport;
	coport_eventmask_t events;
	coport_eventmask_t revents;
} pollcoport_t;

struct copoll
{
	pollcoport_t *coports;
	uint ncoports;
	long timeout; 
};

struct _cocall_args
{
	int status;
	int error;
	namespace_t *ns_cap;
	union 
	{
		struct {
			void *codiscover;
			void *coinsert;
			void *coselect;	
		}; //coproc_init
		struct {
			char ns_name[NS_NAME_LEN];
			nstype_t ns_type;
			namespace_t *child_ns_cap;
		}; //cocreate, codrop
		struct {
			char nsobj_name[NS_NAME_LEN];
			nsobject_t *nsobj;
			nsobject_type_t nsobj_type;
			union {
				void *obj;	
				coservice_t *coservice;
				coport_t *coport;
			};
			void *scb_cap;
		}; //coupdate, coinsert, codelete, coselect, codiscover
		struct {
			void **worker_scbs;
			int nworkers;
			coservice_t *service;
		}; //coprovide
		struct {
			coport_type_t coport_type;
			coport_t *port;
		}; //coopen
		struct {
			pollcoport_t *coports;
			uint ncoports;
			long timeout; 
		}; //copoll
		struct {
			coport_t *cocarrier;
			void *message;
			size_t length;
		}; //cosend/corecv
	};
} __attribute__((__aligned__(16)));

typedef struct _cocall_args cocall_args_t;
typedef struct _cocall_args copoll_args_t;
typedef struct _cocall_args cosend_args_t;
typedef struct _cocall_args corecv_args_t;
typedef struct _cocall_args codiscover_args_t;
typedef struct _cocall_args coprovide_args_t;
typedef struct _cocall_args coopen_args_t;
typedef struct _cocall_args coclose_args_t;
typedef struct _cocall_args coselect_args_t;
typedef struct _cocall_args coinsert_args_t;
typedef struct _cocall_args coproc_init_args_t;
typedef struct _cocall_args codrop_args_t;
typedef struct _cocall_args codelete_args_t;
typedef struct _cocall_args coupdate_args_t;
typedef struct _cocall_args cocreate_args_t;


#endif //!defined(_COCALL_ARGS_H)