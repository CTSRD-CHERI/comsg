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

#include "coport_cinvoke_macros.h"
#include "coport_ipc_utils.h"

#include <comsg/ukern_calls.h>
#include <comsg/coport_ipc.h>
#include <comsg/coport_ipc_cinvoke.h>
#include <comsg/coport.h>

#include <assert.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>
#include <err.h>
#include <stddef.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sys/types.h>

static coport_func_ptr _cosend_codecap = NULL;
static coport_func_ptr _corecv_codecap = NULL;

static coport_func_ptr _cosend_codecap_copipe = NULL;
static coport_func_ptr _corecv_codecap_copipe = NULL;

static coport_func_ptr _cosend_codecap_cochannel = NULL;
static coport_func_ptr _corecv_codecap_cochannel  = NULL;

static void *_stack_sealcap = NULL;

const coport_func_ptr *cosend_codecap = &_cosend_codecap;
const coport_func_ptr *corecv_codecap = &_corecv_codecap;

const coport_func_ptr *cosend_codecap_copipe = &_cosend_codecap_copipe;
const coport_func_ptr *corecv_codecap_copipe = &_corecv_codecap_copipe;

const coport_func_ptr *cosend_codecap_cochannel = &_cosend_codecap_cochannel;
const coport_func_ptr *corecv_codecap_cochannel = &_corecv_codecap_cochannel;

const void **return_stack_sealcap = (const void **)&_stack_sealcap;


static inline __always_inline int
validate_coport_op_args(coport_t *port, void *buf, size_t len)
{
    if (cheri_getlen(buf) < len) 
        return (ENOBUFS);
    else if (len == 0)
        return (EINVAL);
    else 
        return (0);
}

static ssize_t
cosend_impl(coport_t *port, void *buf, size_t len)
{
	coport_type_t type;
    ssize_t retval;
    GET_IDC(port);

    retval = validate_coport_op_args(port, buf, len);
    if (retval != 0) {
        errno = retval;
        retval = -1;
        CCALL_RETURN(retval);
    }
    type = port->type;
    switch(type) {
    case COCHANNEL:
        retval =  cochannel_send(port, buf, len);
        break;
    case COPIPE:
        retval = copipe_send(port, buf, len);
        break;
    default: 
        errno = EINVAL;
        retval = -1;
        break;
    }
    /* XXX-PBB: absent a working calling convention that uses a ccall-based method, this will do */
    /* TODO-PBB: clear registers etc */
    CCALL_RETURN(retval); 
    return (retval);  /* NOTREACHED */
}

static ssize_t
corecv_impl(coport_t *port, void *buf, size_t len)
{
	void *msg;
    coport_type_t type;
    ssize_t retval;
    GET_IDC(port);

    retval = validate_coport_op_args(port, buf, len);
    if (retval != 0) {
        errno = retval;
        retval = -1;
        CCALL_RETURN(retval);
    }
    type = port->type;
    switch(type) {
    case COCHANNEL:
        retval = cochannel_recv(port, buf, len);
        break;
    case COPIPE:
        retval = copipe_recv(port, buf, len);
        break;
    default: 
        errno = EINVAL; 
        retval = -1;
        break;
    }
    CCALL_RETURN(retval);
    return (retval);  /* NOTREACHED */
}

static ssize_t
corecv_impl_cochannel(coport_t *port, void *buf, size_t len)
{
    void *msg;
    coport_type_t type;
    ssize_t retval;
    GET_IDC(port);

    retval = validate_coport_op_args(port, buf, len);
    if (retval != 0) {
        errno = retval;
        retval = -1;
        CCALL_RETURN(retval);
    }
    retval = cochannel_recv(port, buf, len);
    CCALL_RETURN(retval);
    return (retval);  /* NOTREACHED */
}

static ssize_t
corecv_impl_copipe(coport_t *port, void *buf, size_t len)
{
    ssize_t retval;
    GET_IDC(port);

    retval = validate_coport_op_args(port, buf, len);
    if (retval != 0) {
        errno = retval;
        retval = -1;
        CCALL_RETURN(retval);
    }
    retval = copipe_recv(port, buf, len);
    CCALL_RETURN(retval);
    return (retval);  /* NOTREACHED */
}

static ssize_t
cosend_impl_copipe(coport_t *port, void *buf, size_t len)
{
    ssize_t retval;
    GET_IDC(port);

    //comsg_benchmark_log("cinvoke");
    retval = validate_coport_op_args(port, buf, len);
    //comsg_benchmark_log("validate args");
    if (retval != 0) {
        errno = retval;
        retval = -1;
        CCALL_RETURN(retval);
    }
    retval = copipe_send(port, buf, len);
    CCALL_RETURN(retval);
    return (retval);  /* NOTREACHED */
}

static ssize_t
cosend_impl_cochannel(coport_t *port, void *buf, size_t len)
{
    ssize_t retval;
    GET_IDC(port);

    retval = validate_coport_op_args(port, buf, len);
    if (retval != 0) {
        errno = retval;
        retval = -1;
        CCALL_RETURN(retval);
    }
    retval = cochannel_send(port, buf, len);
    CCALL_RETURN(retval);
    return (retval);  /* NOTREACHED */
}

void
setup_cinvoke_targets(void *copipe_sealcap, void *cochannel_sealcap)
{
    void *cosend_func, *corecv_func;
    void *coport_sealcap = copipe_sealcap;

    coport_sealcap = copipe_sealcap;
    /* TODO-PBB: get sealing capability via type manager and/or rtld */
    cosend_func = cheri_getpcc();
    
    cosend_func = cheri_setaddress(cosend_func, (vaddr_t)&cosend_impl);
    _cosend_codecap = cheri_seal(cosend_func, coport_sealcap);

    cosend_func = cheri_setaddress(cosend_func, (vaddr_t)&cosend_impl_copipe);
    _cosend_codecap_copipe = cheri_seal(cosend_func, copipe_sealcap);
   
    cosend_func = cheri_setaddress(cosend_func, (vaddr_t)&cosend_impl_cochannel);
    _cosend_codecap_cochannel = cheri_seal(cosend_func, cochannel_sealcap);


    corecv_func = cheri_getpcc();
    corecv_func = cheri_setaddress(corecv_func, (vaddr_t)&corecv_impl);
    
    _corecv_codecap = cheri_seal(corecv_func, coport_sealcap);

    corecv_func = cheri_setaddress(corecv_func, (vaddr_t)&corecv_impl_copipe);
    _corecv_codecap_copipe = cheri_seal(corecv_func, copipe_sealcap);
    
    corecv_func = cheri_setaddress(corecv_func, (vaddr_t)&corecv_impl_cochannel);
    _corecv_codecap_cochannel = cheri_seal(corecv_func, cochannel_sealcap);


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

    len = sizeof(r_sealroot);
    assert(sysctlbyname("security.cheri.sealcap", &r_sealroot, &len,
        NULL, 0) >= 0);

    assert((cheri_gettag(r_sealroot) != 0));    
    assert(cheri_getlen(r_sealroot) != 0);
    assert((cheri_getperm(r_sealroot) & CHERI_PERM_SEAL) != 0);
    /* TODO-PBB: simulate a divided otype space, pending type manager */
    r_sealroot = cheri_incoffset(r_sealroot, cheri_getlen(r_sealroot)-1);

    _stack_sealcap = cheri_setbounds(r_sealroot, 1);
    _stack_sealcap = cheri_andperm(r_sealroot, (CHERI_PERM_SEAL | CHERI_PERM_GLOBAL));
}


