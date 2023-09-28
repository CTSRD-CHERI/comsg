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

#include "coport_ipc_utils.h"
#include "coport_cinvoke.h"

#include <comsg/coport.h>
#include <comsg/utils.h>
#include <comsg/ukern_calls.h>

#include <assert.h>
#include <cheri/cheric.h>
#include <err.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <unistd.h>

static bool multicore = 0;

static nsobject_t *cosend_obj = NULL;
static nsobject_t *corecv_obj = NULL;

static struct object_type copipe_otype, cochannel_otype, cocarrier_otype;
static struct object_type *allocated_otypes[] = {&copipe_otype, &cochannel_otype};
#define IPCD_OTYPE_RANGE_START (32)
#define IPCD_OTYPE_RANGE_END (63)

coport_type_t 
coport_gettype(const coport_t *port)
{
    long port_otype;
    
    if (cheri_getsealed(port) == 0)
        return (INVALID_COPORT);

    port_otype = cheri_gettype(port);
    if (port_otype == cocarrier_otype.otype)
        return (COCARRIER);
     else if (port_otype == cochannel_otype.otype)
        return (COCHANNEL);
    else if (port_otype == copipe_otype.otype)
        return (COPIPE);
    else if (cocarrier_otype.otype == 0) {
        if (port_otype >= IPCD_OTYPE_RANGE_START && port_otype <= IPCD_OTYPE_RANGE_END) {
            cocarrier_otype.otype = port_otype;
            return (COCARRIER);
        }
    }
    return (INVALID_COPORT);  
}

static void
cocarrier_preload(void)
{
    if (root_ns == NULL)
        root_ns = coproc_init(NULL, NULL, NULL, NULL);
    get_ukernel_service(COCALL_COSEND);
    get_ukernel_service(COCALL_CORECV);
}

coport_t *
process_coport_handle(coport_t *port, coport_type_t type)
{
    switch (type) {
    case COPIPE:
        if (cheri_getsealed(port) == 1) {
            break;
        }
        port = cheri_clearperm(port, CHERI_PERM_GLOBAL);
        port = cheri_seal(port, copipe_otype.sc);
        break;
    case COCHANNEL:
        if (cheri_getsealed(port) == 1)
            break;
        port = cheri_clearperm(port, CHERI_PERM_GLOBAL);
        port = cheri_seal(port, cochannel_otype.sc);
        break;
    case COCARRIER:
        if (cocarrier_otype.otype == 0)
            cocarrier_otype.otype = cheri_gettype(port);
        else if (cocarrier_otype.otype != cheri_gettype(port))
            warn("process_coport_handle: cocarrier otype has changed!!");
        cocarrier_preload();
        break;
    default:
        err(ENOSYS, "coopen: ipcd returned unknown coport type");
        break; /* NOTREACHED */
    }
    return (port);
} 

/*
 * Calculates a wait time based on a rough estimate of the memcpy bandwidth
 */
static struct timespec
calculate_wait_time(size_t message_len)
{
    struct timespec result;

    if (est_mem_bw == 0.0) {
        result.tv_sec = 0;
        result.tv_nsec = 0;
        return result;
    }
    double length = (double)(message_len);
    double time = length / est_mem_bw;
    time = time / 50;
    if (time > 0.5)
        time = 0.5;
    
    result.tv_sec = time;
    result.tv_nsec = (time - (long)(time)) * nanoseconds;
    //printf("Estimated memory bandwidth: %.2FB/s; Wait time calculated:%.2Fs\n", est_mem_bw, time);
    
    return (result);
}

coport_status_t 
acquire_coport_status(const coport_t *port, coport_status_t expected, coport_status_t desired, size_t len)
{
    coport_status_t status_val;
    _Atomic(coport_status_t) *status_ptr;
    int i;
    
    i = 0;
    status_ptr = &port->info->status;
    status_val = expected;
    for(;;i++) {
        if (atomic_compare_exchange_strong_explicit(status_ptr, &status_val, desired, memory_order_acq_rel, memory_order_relaxed)) {
            return (status_val);
        } else {
            switch (status_val) {
            case COPORT_CLOSING:
            case COPORT_CLOSED:
                return (status_val);
                break; /* NOTREACHED */
            default:
                status_val = expected;
                break;
            }
            // If single-core, we need to yield the CPU
            // If no progress is happening, we should yield
            // If we've been yielding for a long time, we should try a short sleep
            if (!multicore || (i % 10) == 0 && (i / 10) != 0) {
                if (len > 0 && ((i % 1000) == 0) && est_mem_bw != 0.0) {
                    struct timespec wait = calculate_wait_time(len);
                    clock_nanosleep(CLOCK_MONOTONIC_PRECISE, 0, &wait, NULL);
                } else {
                    sched_yield();
                }
            }
        }
    }
}

void
release_coport_status(const coport_t *port, coport_status_t desired)
{
    atomic_store_explicit(&port->info->status, desired, memory_order_release);
}

#define acquire_copipe_status(p, e, d, l) acquire_coport_status(p, e, d, l)
#define release_copipe_status(p, d) release_coport_status(p, d)

static bool
check_coport_status(const coport_t *port, coport_status_t desired)
{
    return (atomic_load_explicit(&port->info->status, memory_order_acquire) == desired);
}

ssize_t
copipe_send(const coport_t *port, const void *buf, size_t len)
{
    void *out_buffer;
    coport_status_t status;

    status = acquire_coport_status(port, COPORT_READY, COPORT_BUSY, 0);
    if ((status == COPORT_CLOSING || status == COPORT_CLOSED)) {
        errno = EPIPE;
        return (-1);
    }
    buf = cheri_andperm(buf, COPORT_INBUF_PERMS);
    out_buffer = port->buffer->buf;
    if(cheri_gettag(out_buffer) == 0) {
        release_coport_status(port, COPORT_DONE);
        errno = EPROT;
        return (-1);
    }
    if (cheri_getlen(out_buffer) < len) {
        release_coport_status(port, COPORT_READY);
        errno = EMSGSIZE;
        return (-1);
    }
    memcpy(out_buffer, buf, len);
    port->info->length = len;

    release_coport_status(port, COPORT_DONE);
    return ((ssize_t) len);
}

ssize_t
cochannel_send(const coport_t *port, const void *buf, size_t len)
{
    size_t port_size, port_start, old_end, new_end, new_len;
    ssize_t copied_bytes;
    char *port_buffer, *msg_buffer;
    coport_eventmask_t event;
    coport_status_t status;

    port_size = port->info->length;
    new_len = len + port_size;
    
    if(new_len > COPORT_BUF_LEN) {
        errno = EWOULDBLOCK;
        return (-1);
    }

    status = acquire_coport_status(port, COPORT_OPEN, COPORT_BUSY, len);
    if ((status == COPORT_CLOSING || status == COPORT_CLOSED)) {
        errno = EPIPE;
        return (-1);
    }

    event = port->info->event;
    port_start = port->info->start;
    old_end = port->info->end;

    if ((event & COPOLL_OUT) == 0) {
        release_coport_status(port, COPORT_OPEN);
        errno = EAGAIN;
        return (-1);
    }
    port_buffer = port->buffer->buf;
    msg_buffer = (char *)cheri_andperm(buf, COPORT_INBUF_PERMS);
    
    new_end = (old_end + len) % COPORT_BUF_LEN;
    if (old_end + len > COPORT_BUF_LEN) {
        memcpy(&port_buffer[old_end], msg_buffer, COPORT_BUF_LEN - old_end);
        memcpy(port_buffer, &msg_buffer[COPORT_BUF_LEN - old_end], new_end);
        copied_bytes = COPORT_BUF_LEN - (ssize_t) old_end;
        copied_bytes += (ssize_t) new_end;
    } else {
        memcpy(&port_buffer[old_end], msg_buffer, len);
        copied_bytes = len;
    }
    port->info->end = new_end;
    port->info->length = new_len;
    
    event |= COPOLL_IN;
    if (new_len == COPORT_BUF_LEN) 
        event &= ~COPOLL_OUT;
    port->info->event = event;

    release_coport_status(port, COPORT_OPEN);

    return (copied_bytes);
}

ssize_t
cochannel_recv(const coport_t *port, void *buf, size_t len)
{
    char *out_buffer, *port_buffer;
    size_t port_size, port_buf_len;
    size_t old_start, port_end, new_start, len_to_end, new_len;
    coport_eventmask_t event;
    coport_status_t status;

    port_size = port->info->length;
    if (cheri_getlen(buf) < len) {
        errno = EMSGSIZE;
        return (-1);
    }
    new_len = port_size - len;
    status = acquire_coport_status(port, COPORT_OPEN, COPORT_BUSY, len);
    if ((status == COPORT_CLOSING || status == COPORT_CLOSED)) {
        /* port is closed */
        errno = EPIPE;
        return (-1);
    }
    
    event = port->info->event;
    if ((event & COPOLL_IN) == 0) {
        /* UNREACHED */
        release_coport_status(port, COPORT_OPEN);
        errno = EAGAIN;
        return (-1);
    }
    out_buffer = cheri_andperm(buf, COPORT_OUTBUF_PERMS);
    port_buffer = port->buffer->buf;
    port_buf_len = __builtin_cheri_length_get(port_buffer);
    old_start = port->info->start;
    new_start = (old_start + len) % COPORT_BUF_LEN;
    
    if (old_start + len > port_buf_len) {
        len_to_end = port_buf_len - old_start;
        memcpy(out_buffer, &port_buffer[old_start], len_to_end);
        memcpy(&out_buffer[len_to_end], port_buffer, new_start);
    }
    else
        memcpy(buf, &port_buffer[old_start], len);
    port->info->length = new_len;
    port->info->start = new_start;

    event |= COPOLL_OUT;
    if (new_len == 0)
        event &= ~COPOLL_IN;
    port->info->event = event;

    release_coport_status(port, COPORT_OPEN);
    return ((ssize_t) len);
}

ssize_t
copipe_recv(const coport_t *port, void *buf, size_t len)
{
    coport_status_t status;
    ssize_t received_len;

    if (cheri_getlen(buf) > len)
        buf = cheri_setbounds(buf, len);
    else if (cheri_getlen(buf) < len) {
        errno = EMSGSIZE;
        return (-1);
    }

    buf = cheri_andperm(buf, COPIPE_RECVBUF_PERMS);

    status = acquire_coport_status(port, COPORT_OPEN, COPORT_BUSY, len);
    if ((status == COPORT_CLOSING || status == COPORT_CLOSED)) {
        errno = EPIPE;
        return (-1);
    }

    port->buffer->buf = buf;

    release_coport_status(port, COPORT_READY);
    status = acquire_coport_status(port, COPORT_DONE, COPORT_BUSY, len);
    if ((status == COPORT_CLOSING || status == COPORT_CLOSED)) {
        errno = EPIPE;
        return (-1);
    }

    received_len = (ssize_t)port->info->length;
    port->buffer->buf = NULL;
    port->info->length = 0;

    release_coport_status(port, COPORT_OPEN);

    return (received_len);
}

static void
clock_diff(struct timespec *result, struct timespec *end, struct timespec *start)
{
	result->tv_sec = end->tv_sec - start->tv_sec;
	result->tv_nsec = end->tv_nsec - start->tv_nsec;
	if (result->tv_nsec < 0) {
		result->tv_sec--;
		result->tv_nsec += 1000000000L;
	}
}

static void
estimate_memory_bandwidth(void)
{
    char a[16 * 1024 * 1024];
    char b[16 * 1024 * 1024];

    struct timespec start, end, duration;
    size_t length;
    double time;

    length = __builtin_cheri_length_get(a);
    clock_gettime(CLOCK_MONOTONIC_PRECISE, &start);
    memcpy(a, b, length);
    clock_gettime(CLOCK_MONOTONIC_PRECISE, &end);

    clock_diff(&duration, &end, &start);
    time = (double)(duration.tv_sec) + ((double)(duration.tv_nsec) / nanoseconds);
    est_mem_bw = (double)(length) / time;
}

void
enable_copipe_sleep(void)
{
    estimate_memory_bandwidth();
}

__attribute__ ((constructor)) static void
coport_ipc_utils_init(void)
{
	int mib[4];
    size_t len;
    int cores;
    void *sealroot, *returncap_sealroot;

    len = sizeof(sealroot);
    
    assert(sysctlbyname("security.cheri.sealcap", &sealroot, &len,
        NULL, 0) >= 0);

    assert((cheri_gettag(sealroot) != 0));    
    assert(cheri_getlen(sealroot) != 0);
    assert((cheri_getperm(sealroot) & CHERI_PERM_SEAL) != 0);
    /* TODO-PBB: simulate a divided otype space, pending a type manager */
    sealroot = cheri_incoffset(sealroot, 32);

    sealroot = make_otypes(sealroot, 2, allocated_otypes);
    copipe_otype.usc = __builtin_cheri_tag_clear(copipe_otype.usc);
    cochannel_otype.usc = __builtin_cheri_tag_clear(cochannel_otype.usc);
    cocarrier_otype.otype = 0;
    setup_cinvoke_targets(copipe_otype.sc, cochannel_otype.sc);

    len = sizeof(cores); 
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    sysctl(mib, 2, &cores, &len, NULL, 0);
    multicore = cores > 1;

}
