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
#include <comsg/coport_ipc.h>

#include "coport_ipc_utils.h"

#include <comsg/ukern_calls.h>
#include <coproc/coport.h>

#include <assert.h>
#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>
#include <err.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <unistd.h>

nsobject_t *
open_named_coport(const char *coport_name, coport_type_t type, namespace_t *ns)
{
    coport_t *port;
    nsobject_t *port_obj, *cosend_obj, *corecv_obj;

    port = coopen(type);
    port = process_coport_handle(port, type);

    port_obj = coinsert(coport_name, COPORT, port, ns);

    return (port_obj);
}

coport_t *
open_coport(coport_type_t type)
{
    coport_t *port;

    port = coopen(type);
    process_coport_handle(port, type);
    return (port);
}

int
cosend(const coport_t *port, const void *buf, size_t len)
{
    coport_type_t type;
    int retval;
  
    if(len == 0)
        return (0);

    type = coport_gettype(port);
    switch(type) {
    case COCHANNEL:
        retval = cochannel_send(port, buf, len);
        break;
    case COCARRIER:
        retval = cocarrier_send(port, buf, len);
        break;
    case COPIPE:
        retval = copipe_send(port, buf, len);
        break;
    default:
        errno = EINVAL;
        break;
    }
    return (retval);
}

int 
corecv(const coport_t *port, void **buf, size_t len)
{
    void *msg;
    coport_type_t type;
    int retval;

    if(len == 0)
        return (0);
    type = coport_gettype(port);
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

pollcoport_t 
make_pollcoport(coport_t *port, coport_eventmask_t events)
{
    pollcoport_t pcpt;

    pcpt.coport = port;
    pcpt.events = events;
    pcpt.revents = NOEVENT;

    return (pcpt);
}
