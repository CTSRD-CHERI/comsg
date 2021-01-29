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
#ifndef COEVENT_H
#define COEVENT_H

#include <stdatomic.h>
#include <sys/queue.h>
#include <sys/types.h>

typedef enum {FLAG_PROVIDER = 1, FLAG_SLOCALL = 2, FLAG_DEAD = 4} cocallback_flags_t;

typedef enum {PROCESS_DEATH} coevent_type_t;


struct cocallback_args {
	size_t len;
	union {
		void *cocall_data;
		struct cocallback_func **funcs;
	};
};

typedef struct cocallback_func {
	pid_t provider;
	void *scb;
	int flags;
	_Atomic int consumers;
} cocallback_func_t;

typedef struct cocallback {
	struct cocallback_func *func;
	struct cocallback_args args;
	SLIST_ENTRY(cocallback) next;
} cocallback_t;

union coevent_subject {
	struct {
		pid_t pid;
		void *scb_token;
	} _procdeath;
};

typedef struct coevent {
	coevent_type_t event;
	union coevent_subject _subject;
	_Atomic int in_progress;
	int ncallbacks;
	SLIST_HEAD(, cocallback) callbacks;
} coevent_t;

#define ce_pid _subject._procdeath.pid

#endif //!defined(COEVENT_H)