/*
 * Copyright (c) 2023 Peter S. Blandford-Baker
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

#include <comsg/coport.h>
#include <stddef.h>
#include <sys/cdefs.h>
#include <sys/types.h>

extern ssize_t coport_cinvoke(void *codecap, const coport_t *coport, void *buf, const void *ret_sealcap, size_t len, coport_op_t op);

/*
 * bool 
 * copipe_ready_cinvoke(void *codecap, const coport_t *coport, const void *ret_sealcap)
 */
bool 
copipe_ready_cinvoke(void *codecap, const coport_t *coport, const void *ret_sealcap)
{
    return ((bool)coport_cinvoke(codecap, coport, NULL, ret_sealcap, 0, COPORT_OP_POLL));
}

/*
 * int 
 * copipe_cosend_cinvoke(void *codecap, const coport_t *coport, void *buf, const void *ret_sealcap, size_t len)
 */
ssize_t 
copipe_cosend_cinvoke(void *codecap, const coport_t *coport, void *buf, const void *ret_sealcap, size_t len)
{
    return (coport_cinvoke(codecap, coport, buf, ret_sealcap, len, COPORT_OP_COSEND));
}

/*
 * int 
 * copipe_corecv_cinvoke(void *codecap, const coport_t *coport, void *buf, const void *ret_sealcap, size_t len)
 */
ssize_t 
copipe_corecv_cinvoke(void *codecap, const coport_t *coport, void *buf, const void *ret_sealcap, size_t len)
{
    return (coport_cinvoke(codecap, coport, buf, ret_sealcap, len, COPORT_OP_CORECV));
}

ssize_t 
cochannel_cosend_cinvoke(void *codecap, const coport_t *coport, const void *buf, const void *ret_sealcap, size_t len)
{
    return (coport_cinvoke(codecap, coport, (void *)buf, ret_sealcap, len, COPORT_OP_COSEND));
}

ssize_t 
cochannel_corecv_cinvoke(void *codecap, const coport_t *coport, void *buf, const void *ret_sealcap, size_t len)
{
    return (coport_cinvoke(codecap, coport, buf, ret_sealcap, len, COPORT_OP_CORECV));
}
