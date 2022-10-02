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
#ifndef _COPORT_IPC_CINVOKE_H
#define _COPORT_IPC_CINVOKE_H

#include <comsg/coport.h>
#include <stddef.h>
#include <sys/cdefs.h>
#include <sys/types.h>

typedef ssize_t (*coport_func_ptr)(coport_t *, void *, size_t);

extern const coport_func_ptr *cosend_codecap;
extern const coport_func_ptr *corecv_codecap;

extern const coport_func_ptr *cosend_codecap_copipe;
extern const coport_func_ptr *corecv_codecap_copipe;

extern const coport_func_ptr *cosend_codecap_cochannel;
extern const coport_func_ptr *corecv_codecap_cochannel;

extern const void **return_stack_sealcap;

__BEGIN_DECLS

extern ssize_t coport_cinvoke(void *codecap, coport_t *coport, void *buf, const void *ret_sealcap, size_t len);

extern ssize_t cochannel_cosend_cinvoke(void *codecap, coport_t *coport, void *buf, const void *ret_sealcap, size_t len);
extern ssize_t cochannel_corecv_cinvoke(void *codecap, coport_t *coport, void *buf, const void *ret_sealcap, size_t len);
extern ssize_t copipe_cosend_cinvoke(void *codecap, coport_t *coport, void *buf, const void *ret_sealcap, size_t len);
extern ssize_t copipe_corecv_cinvoke(void *codecap, coport_t *coport, void *buf, const void *ret_sealcap, size_t len);

static inline __always_inline ssize_t 
cosend_cinvoke(coport_t *port, void *buffer, size_t length)
{
	return (coport_cinvoke(*cosend_codecap, port, buffer, (*return_stack_sealcap), length));
} 

static inline __always_inline ssize_t 
corecv_cinvoke(coport_t *port, void *buffer, size_t length)
{
	return (coport_cinvoke(*corecv_codecap, port, buffer, (*return_stack_sealcap), length));
}

static inline __always_inline ssize_t 
cosend_cinvoke_copipe(coport_t *port, void *buffer, size_t length)
{
    return (copipe_cosend_cinvoke(*cosend_codecap_copipe, port, buffer, (*return_stack_sealcap), length));
}

static inline __always_inline ssize_t 
corecv_cinvoke_copipe(coport_t *port, void *buffer, size_t length)
{
    return (copipe_corecv_cinvoke(*corecv_codecap_copipe, port, buffer, (*return_stack_sealcap), length));
}

static inline __always_inline ssize_t 
cosend_cinvoke_cochannel(coport_t *port, void *buffer, size_t length)
{
    return (cochannel_cosend_cinvoke(*cosend_codecap_cochannel, port, buffer, (*return_stack_sealcap), length));
}

static inline __always_inline ssize_t 
corecv_cinvoke_cochannel(coport_t *port, void *buffer, size_t length)
{
    return (cochannel_corecv_cinvoke(*corecv_codecap_cochannel, port, buffer, (*return_stack_sealcap), length));
}

__END_DECLS

#endif