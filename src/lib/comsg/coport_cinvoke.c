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

#include "coport_cinvoke.h"

#include "coport_ipc_utils.h"
#include <comsg/ukern_calls.h>
#include <comsg/coport_ipc.h>
#include <comsg/coport_ipc_cinvoke.h>
#include <coproc/coport.h>

#include <assert.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>
#include <err.h>
#include <stddef.h>
#include <sys/errno.h>
#include <machine/sysarch.h>

#ifndef CINVOKE_IMPLEMENTED
#define COPORT_CINVOKE(func, port, retval, buffer, length) \
	__asm__ volatile inline ( \
    "move $a0, %2\n" \
    "cmove $c4, %1\n" \
    "cgetpcc $c18\n" \
    "cincoffset $c18, $c18, 16\n" \
	"ccall %3, %4, 1\n" \
	"nop\n" \
    "move %0, $v0\n"\
	: "=r" (retval) \
	: "C" (buffer), "r" (length), "C" (func), "C" (port));
#else
#define COPORT_CINVOKE(func, port, retval, buffer, length) \
    __asm__ volatile inline ( \
    "move $a0, %2\n" \
    "cmove $c4, %1\n" \
    "cgetpcc $c18\n" \
    "cincoffset $c18, $c18, 16\n" \
    "cinvoke %3, %4\n" \
    "nop\n" \
    "move %0, $v0\n"\
    : "=r" (retval) \
    : "C" (buffer), "r" (length), "C" (func), "C" (port));
#endif

static coport_func_ptr _cosend_codecap = NULL;
static coport_func_ptr _corecv_codecap = NULL;
static void *_coport_invoke_return = NULL;
static void *_stack_sealcap = NULL;

extern void *cinvoke_return;

#if 0
__attribute__((cheri_ccall))
int cosend_cinvoke(coport_t *port, void *buf, size_t len)
{
	int result;
	if (coport_gettype(port) == COCARRIER)
        result = cocarrier_send(port, buf, len);
    else
       COPORT_CINVOKE(corecv_codecap, port, result, buf, len);
   if (result == -1) {
        if (errno == EINVAL)
            err(EINVAL, "cosend_cinvoke: invalid coport type");
    }
	return (result);
}

__attribute__((cheri_ccall))
int corecv_cinvoke(coport_t *port, void **buf, size_t len)
{
    void *msg;
	int result;
    if (coport_gettype(port) == COCARRIER) {
        msg = cocarrier_recv(port, len);
        if(msg == NULL)
            result = -1;
        else {
            *buf = msg;
            result = cheri_getlen(buf);
        }
    }
    else
	   COPORT_CINVOKE(corecv_codecap, port, result, buf, len);
    if (result == -1) {
        if (errno == EINVAL)
            err(EINVAL, "corecv_cinvoke: invalid coport type");
    }
	return (result);
}

#elif 0

inline int cosend_cinvoke(coport_t *port, void *buf, size_t len)
{
    return (coport_cinvoke(cosend_codecap, port, buf, len));
}

inline int corecv_cinvoke(coport_t *port, void **buf, size_t len)
{
    return (coport_cinvoke(corecv_codecap, port, buf, len));
}

#else

const coport_func_ptr *cosend_codecap = &_cosend_codecap;
const coport_func_ptr *corecv_codecap = &_corecv_codecap;
const void **return_stack_sealcap = &_stack_sealcap;
const void **cinvoke_return_cap = &_coport_invoke_return;

#endif

static __attribute__((cheri_ccallee))
int cosend_impl(coport_t *port, void *buf, size_t len)
{
	coport_type_t type;
    int retval;
    __asm__("cmove %0, $idc" : "=C" (port)); 
  
    if(len == 0)
        return (0);

    type = port->type;
    switch(type) {
    case COCHANNEL:
        retval =  (cochannel_send(port, buf, len));
        break;
    case COPIPE:
        retval = (copipe_send(port, buf, len));
        break;
    default:
        errno = EINVAL;
        retval = (-1);
        break;
    }
    /* XXX-PBB: absent a working calling convention that uses my method, this will do */
    /* TODO-PBB: clear registers etc */
    __asm__ ( 
        "move $v0, %[result]\n"
        "ccall $c21, $c22, 1\n"
        "nop"
        : : [result] "r" (retval));
    return (retval);
}

static __attribute__((cheri_ccallee))
int corecv_impl(coport_t *port, void **buf, size_t len)
{
	void *msg;
    coport_type_t type;
    int retval;
    __asm__("cmove %0, $idc" : "=C" (port));

    if(len == 0)
        return (0);
    type = port->type;
    switch(type) {
    case COCHANNEL:
        retval = cochannel_corecv(port, *buf, len);
        break;
    case COCARRIER:
        msg = cocarrier_recv(port, len);
        if(msg == NULL)
            retval = -1;
        else {
            *buf = msg;
            retval = cheri_getlen(buf);
        }
        break;
    case COPIPE:
        retval = copipe_corecv(port, buf, len);
        break;
    default:
        err(1, "corecv: invalid coport type");
        break;
    }
    __asm__ ( 
        "move $v0, %[result]\n"
        "ccall $c21, $c22, 1\n"
        "nop"
        : : [result] "r" (retval));
    return (retval);
}

void
setup_cinvoke_targets(void *coport_sealcap)
{
    /* TODO-PBB: get sealing capability via type manager and/or rtld */
    _cosend_codecap = cheri_getpcc();
    _cosend_codecap = cheri_setaddress(_cosend_codecap, (vaddr_t)&cosend_impl);
    _cosend_codecap = cheri_seal(_cosend_codecap, coport_sealcap);

    _corecv_codecap = cheri_getpcc();
    _corecv_codecap = cheri_setaddress(_corecv_codecap, (vaddr_t)&corecv_impl);
    _corecv_codecap = cheri_seal(_corecv_codecap, coport_sealcap);

    assert(cheri_getperm(_cosend_codecap) & CHERI_PERM_CCALL);
    assert(cheri_getperm(_corecv_codecap) & CHERI_PERM_CCALL);
}

__attribute__ ((constructor)) static void
coport_cinvoke_init(void)
{
    int mib[4];
    size_t len;
    int cores;
    void *r_sealroot;

    assert(sysarch(CHERI_GET_SEALCAP, &r_sealroot) != -1);

    assert((cheri_gettag(r_sealroot) != 0));    
    assert(cheri_getlen(r_sealroot) != 0);
    assert((cheri_getperm(r_sealroot) & CHERI_PERM_SEAL) != 0);
    /* TODO-PBB: simulate a divided otype space, pending type manager */
    r_sealroot = cheri_incoffset(r_sealroot, cheri_getlen(r_sealroot)-1);

    _stack_sealcap = cheri_setbounds(r_sealroot, 1);
    _coport_invoke_return = cheri_getpcc();
    _coport_invoke_return = cheri_setaddress(_coport_invoke_return, (vaddr_t)&cinvoke_return);
    _coport_invoke_return = cheri_seal(_coport_invoke_return, _stack_sealcap);
}


