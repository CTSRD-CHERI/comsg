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
#ifndef _COCALL_ARGS_H
#define _COCALL_ARGS_H

#ifndef COCALL_ERR
#define COCALL_ERR(c, n) do { c->status = (-1);\
	c->error = (n);\
	return; } while(0)
#endif //!defined(COCALL_ERR)

#ifndef COCALL_RETURN
#define COCALL_RETURN(c, n) do { c->status = (n);\
	c->error = (0);\
	return; } while(0)
#endif //!defined(COCALL_RETURN)

/* arbitrary-ish. */
#ifndef MAX_COCALL_ARGS_SIZE
#define MAX_COCALL_ARGS_SIZE (256) 
#endif

// Minimal cocall args struct
// Consumers of this library will define their own, but it must match this
struct _cocall_args
{
	int status;
	int error;
	int op;
#define PAD_LENGTH (MAX_COCALL_ARGS_SIZE - (sizeof(int) * 3))
	char pad[PAD_LENGTH];
} __attribute__((__aligned__(16)));

typedef struct _cocall_args cocall_args_t;

#endif //!defined(_COCALL_ARGS_H)