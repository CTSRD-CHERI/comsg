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
#include <comsg/coport_ipc.h>

#include "coport_ipc_utils.h"

#include <comsg/coport_ipc_cinvoke.h>
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
    int error, save_errno;
    coport_t *port;
    nsobject_t *port_obj, *cosend_obj, *corecv_obj;

    port = coopen(type);
    if (port == NULL)
        return (NULL);
    
    port = process_coport_handle(port, type);
    port_obj = coinsert(coport_name, COPORT, port, ns);
    if (port_obj == NULL) {
        save_errno = errno;
        error = coclose(port);
        if (error != 0)
            err(errno, "open_named_coport: coinsert failed and coclose failed");
        else {
            errno = save_errno;
            return (NULL);
        }
    }
    

    return (port_obj);
}

coport_t *
open_coport(coport_type_t type)
{
    coport_t *port;

    port = coopen(type);
    if (port == NULL)
        return (NULL);
    process_coport_handle(port, type);
    return (port);
}

ssize_t
cosend(const coport_t *port, const void *buf, size_t len)
{
    coport_type_t type;
    ssize_t retval;

    type = coport_gettype(port);
    switch(type) {
    case COCHANNEL:
    case COPIPE:
        if (len == 0) {
            errno = EINVAL;
            retval = -1;
        } else
            retval = cosend_cinvoke(port, buf, len);
        break;
    case COCARRIER:
        /* len is an optional hint here, so we're not so worried */
        retval = cocarrier_send(port, buf, len);
        break;
    default:
        errno = EINVAL;
        retval = -1;
        break;
    }
    return (retval);
}

ssize_t 
corecv(const coport_t *port, void **buf, size_t len)
{
    void *msg;
    coport_type_t type;
    ssize_t retval;

    type = coport_gettype(port);
    switch(type) {
    case COCHANNEL:
    case COPIPE:
        if (len == 0) {
            errno = EINVAL;
            retval = -1;
        } else
            retval = corecv_cinvoke(port, buf, len);
        break;
    case COCARRIER:
        /* len is an optional hint here, so we're not so worried */
        msg = cocarrier_recv(port, len);
        if (msg == NULL)
            retval = -1;
        else {
            *buf = msg;
            retval = cheri_getlen(buf);
        }
        break;
    default:
        errno = EINVAL;
        retval = -1;
        break;
    }
    return (retval);
}

void
make_pollcoport(pollcoport_t *pcpt, coport_t *port, coport_eventmask_t events)
{
    pcpt->coport = port;
    pcpt->events = events;
    pcpt->revents = NOEVENT;
}

void 
set_coport_handle_type(coport_t *port, coport_type_t type)
{
    process_coport_handle(port, type);
}

