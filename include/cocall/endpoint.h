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
#ifndef _COCALL_ENDPOINT_H
#define _COCALL_ENDPOINT_H

#include <cocall/cocall_args.h>
#include <cocall/endpoint_args.h>

#include <sys/errno.h>
#include <sys/types.h>
#include <stdbool.h>

void init_endpoints(void);
void join_endpoint_thread(void);
void **get_fast_endpoints(void);
void **get_slow_endpoints(void);
size_t get_fast_endpoint_count(void);
size_t get_slow_endpoint_count(void);

#if !defined(_LIBCOCALL) && defined(COCALL_ENDPOINT_IMPL)

size_t 
get_n_fast_workers()
{
    size_t n_fast_workers = 0;
#pragma push_macro("COACCEPT_ENDPOINT")
#define COACCEPT_ENDPOINT(name, op, validate, func) n_fast_workers++;
#include "coaccept_endpoints.inc"
#pragma pop_macro("COACCEPT_ENDPOINT")
    return n_fast_workers;
}

size_t 
get_n_slow_workers()
{
    size_t n_slow_workers = 0;
#pragma push_macro("SLOACCEPT_ENDPOINT")
#define SLOACCEPT_ENDPOINT(name, op, validate, func) n_slow_workers++;
#include "sloaccept_endpoints.inc"
#pragma pop_macro("SLOACCEPT_ENDPOINT")
    return n_slow_workers;
}

void 
coaccept_handler(void *cookie, cocall_args_t *cocall_args_ptr)
{
    cocall_args_t *args = cocall_args_ptr;
    switch (args->op) {
#pragma push_macro("COACCEPT_ENDPOINT")
#define COACCEPT_ENDPOINT(name, op, validate, func) \
    case op:                                        \
        if (validate != NULL) {                     \
            if (!validate(cocall_args_ptr)) {       \
                args->status = -1;                  \
                args->error = EINVAL;               \
                return;                             \
            }                                       \
        }                                           \
        func(args, cookie);                         \
        break;
#include "coaccept_endpoints.inc"
#pragma pop_macro("COACCEPT_ENDPOINT")
    default:
        args->status = -1;
        args->error = ENOSYS;
        break;
    }
}

void 
sloaccept_handler(void *cookie, cocall_args_t *args)
{
    switch (args->op) {
#pragma push_macro("SLOACCEPT_ENDPOINT")
#define SLOACCEPT_ENDPOINT(name, op, validate, func)    \
    case op:                                            \
        if (validate != NULL) {                         \
            if (!validate(args)) {                      \
                args->status = -1;                      \
                args->error = EINVAL;                   \
                return;                                 \
            }                                           \
        }                                               \
        func(args, cookie);                             \
        break;
#include "sloaccept_endpoints.inc"
#pragma pop_macro("SLOACCEPT_ENDPOINT")
    default:
        args->status = -1;
        args->error = ENOSYS;
        break;
    }
}

#else

void sloaccept_handler(void *, cocall_args_t *);
void coaccept_handler(void *, cocall_args_t *);
size_t get_n_fast_workers();
size_t get_n_slow_workers();

#endif //!defined(_LIBCOCALL)

#endif //!defined(_COCALL_ENDPOINT_H)