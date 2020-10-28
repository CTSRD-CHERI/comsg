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

#include <cheri/cheric.h>
#include <cheri/cherireg.h>

#define CINVOKE(code, data, retval) \
	__asm__ volatile inline ( \
	"cinvoke\t%0, %1\n\t" \
	"nop\n\t" \
	: "=r" (retval) \
	: "C" (code), "C" (data));

static void *cosend_codecap;
static void *corecv_codecap;

int cosend(coport_t *port, void *buf, size_t len)
{
	int result;
	CINVOKE(cosend_codecap, port, result);
	return (result);
}

int corecv(coport_t *port, void *buf, size_t len)
{
	int result;
	CINVOKE(corecv_codecap, port, result);
	return (result);
}

int cosend_impl(coport_t *port, void *buf, size_t len)
{
	coport_type_t type;
    int retval;
  
    if(len == 0)
        return (0);

    type = port->type;
    switch(type) {
    case COCHANNEL:
        return (cochannel_send(port, buf, len));
    case COCARRIER:
        return (cocarrier_send(port, buf, len));
    case COPIPE:
        return (copipe_send(port, buf, len));
    default:
        errno = EINVAL;
        return (-1);
    }

}

int corecv_impl(coport_t *port, void *buf, size_t len)
{
	void *msg;
    coport_type_t type;
    int retval;

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
    return (retval);
}

__attribute__ ((constructor)) static void 
init_library_cinvoke(void)
{
	void *coport_sealcap;
	/* TODO-PBB: get sealing capability via type manager and/or rtld */
	cosend_codecap = cheri_seal(_cosend, coport_sealcap);
	corecv_codecap = cheri_seal(_corecv, coport_sealcap);

	assert(cheri_getperm(cosend_codecap) & CHERI_PERM_CCALL);
	assert(cheri_getperm(corecv_codecap) & CHERI_PERM_CCALL);
}


