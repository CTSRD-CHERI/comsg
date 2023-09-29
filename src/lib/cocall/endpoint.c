/*
 * Copyright (c) 2022 Peter S. Blandford-Baker
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
#include <cocall/endpoint.h>
#include <cocall/cocall_args.h>
#include <cocall/tls_cocall.h>

#include <cheri/cheric.h>
#include <cheri/cherireg.h>

#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <sysexits.h>
#include <pthread.h>
#include <pthread_np.h>
#include <unistd.h>


static pthread_mutex_t registration_mutex;
static pthread_mutex_t worker_creation_mutex;
static pthread_cond_t registration_cond;

extern void begin_cocall(void);
extern void end_cocall(void);

__attribute__ ((constructor)) static 
void init_worker_mutexes(void)
{
    pthread_mutexattr_t mtxattr;

    pthread_mutexattr_init(&mtxattr);
    pthread_mutexattr_settype(&mtxattr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutexattr_setrobust(&mtxattr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(&registration_mutex, &mtxattr);
    pthread_mutex_init(&worker_creation_mutex, &mtxattr);
    pthread_cond_init(&registration_cond, NULL);
}

static void 
coaccept_endpoint_init(endpoint_args_t *worker_args)
{
    int error;
    pthread_mutex_lock(&registration_mutex);
#ifdef COSETUP_COGETPID
    error = cosetup(COSETUP_COGETPID);
    if (error != 0)
        err(EX_OSERR, "%s: failed to cosetup for COGETPID", __func__);
#endif
#ifdef COSETUP_COGETTID
    error = cosetup(COSETUP_COGETTID);
    if (error != 0)
        err(EX_OSERR, "%s: failed to cosetup for COSETUP_COGETTID", __func__);
#endif

    if(coregister(NULL, &worker_args->scb_cap) != 0) {
        pthread_cond_signal(&registration_cond);
        pthread_exit(NULL); /* should release robust mutex */
    }

    pthread_cond_signal(&registration_cond);
    pthread_mutex_unlock(&registration_mutex);
}

static int 
get_ncpu(void)
{
    int mib[4] = {CTL_HW, HW_NCPU, 0, 0};
    int cores;
    size_t len = sizeof(cores);
    sysctl(mib, 2, &cores, &len, NULL, 0);
    return (cores);
}

static void 
coaccept_wrapper(bool slow)
{
    void *cookie;
    char *args;

    args = malloc(MAX_COCALL_ARGS_SIZE);
    memset(args, '\0', cheri_getlen(args));
    for (;;) {
        int result;
        if (!slow) {
            result = coaccept_tls(&cookie, args, cheri_getlen(args));
            if (result >= 0)
                coaccept_handler(cookie, (cocall_args_t *)args);
        } else {
            result = sloaccept_tls(&cookie, args, cheri_getlen(args));
            if (result >= 0)
                sloaccept_handler(cookie, (cocall_args_t *)args);
        }
        if (result < 0) 
            err(EX_UNAVAILABLE, "%s: coaccept failed", __func__);
    }
}

static void *
coaccept_endpoint(void *argp)
{
    endpoint_args_t *worker_args;

    worker_args = argp;
    begin_cocall();
    void *tmp = malloc(1);
    free(tmp);
    end_cocall();

    coaccept_endpoint_init(worker_args);
    coaccept_wrapper(worker_args->slow);

    return (argp);
}

static bool 
start_endpoint_thread(endpoint_args_t *thread_args)
{
    pthread_attr_t thr_attr;
    int error;

    pthread_attr_init(&thr_attr);

    pthread_mutex_lock(&worker_creation_mutex);
    pthread_mutex_lock(&registration_mutex);

    error = pthread_create(&thread_args->worker, &thr_attr, coaccept_endpoint, thread_args);
    if (error != 0) {
        pthread_mutex_unlock(&registration_mutex);
        return (false);
    }

    error = pthread_cond_wait(&registration_cond, &registration_mutex);
    if (thread_args->scb_cap == NULL || error == EOWNERDEAD) {
        if (pthread_peekjoin_np(thread_args->worker, NULL) != 0) {
            abort();
        }
        pthread_mutex_unlock(&registration_mutex);
        pthread_mutex_unlock(&worker_creation_mutex);
        printf("failed to start endpoint thread\n");
        return (false);
    }
    pthread_mutex_unlock(&registration_mutex);
    pthread_mutex_unlock(&worker_creation_mutex);

    return (true);
}

static endpoint_args_t *fast_endpoints = NULL;
static endpoint_args_t *slow_endpoints = NULL;

static void
spawn_endpoints(bool slow, size_t n)
{
    endpoint_args_t *args, *argp;

    argp = calloc(n, sizeof(endpoint_args_t));
    if (slow) {
        if (slow_endpoints != NULL)
            err(EX_SOFTWARE, "%s: slow endpoint array was not empty", __func__);
        slow_endpoints = argp;
    } else {
        if (fast_endpoints != NULL)
            err(EX_SOFTWARE, "%s: fast endpoint array was not empty", __func__);
        fast_endpoints = argp;
    }

    for (size_t i = 0; i < n; i++) {
        args = &argp[i];
        args->slow = slow;
        start_endpoint_thread(args);
    }
}

static void **
get_endpoints(endpoint_args_t *array)
{
    size_t n = cheri_getlen(array) / sizeof(endpoint_args_t);
    void **scbs = calloc(n, sizeof(void *));
    for (size_t i = 0; i < n; i++) {
        scbs[i] = array[i].scb_cap;
    }
    return scbs;
}

void **
get_slow_endpoints(void)
{
    return get_endpoints(slow_endpoints);
}

void **
get_fast_endpoints(void)
{
    return get_endpoints(fast_endpoints);
}

size_t get_fast_endpoint_count()
{
    return cheri_getlen(fast_endpoints) / sizeof(endpoint_args_t);
}

size_t get_slow_endpoint_count()
{
    return cheri_getlen(slow_endpoints) / sizeof(endpoint_args_t);
}


void
init_endpoints()
{
    size_t n_fast_workers, n_slow_workers;
    int n_cpu;

    n_slow_workers = get_n_slow_workers();
    n_fast_workers = get_n_fast_workers();

    n_cpu = get_ncpu();
    if (n_slow_workers != 0) {
        size_t n_workers = n_slow_workers < n_cpu ? n_cpu : n_slow_workers;
        spawn_endpoints(true, n_workers);
    }
    if (n_fast_workers != 0) {
        size_t n_workers = n_fast_workers < n_cpu ? n_cpu : n_fast_workers;
        spawn_endpoints(false, n_workers);
    }    
}

void
join_endpoint_thread(void)
{
    size_t n_fast_workers = cheri_getlen(fast_endpoints) / sizeof(endpoint_args_t);
    size_t n_slow_workers = cheri_getlen(slow_endpoints) / sizeof(endpoint_args_t);

    for (size_t i = 0; i < n_fast_workers; i++) {
        pthread_join(fast_endpoints[i].worker, NULL);
    }
    for (size_t i = 0; i < n_slow_workers; i++) {
        pthread_join(slow_endpoints[i].worker, NULL);
    }
}
