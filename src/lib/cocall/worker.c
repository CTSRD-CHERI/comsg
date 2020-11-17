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
//ECATS-BSD
#include <cocall/cocall_args.h>

#include <coproc/utils.h>
#include <cocall/worker.h>

#include <cheri/cheric.h>
#include <cheri/cherireg.h>

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <pthread.h>
#include <unistd.h>

static pthread_mutex_t registration_mutex;
static pthread_cond_t registration_cond;

__attribute__ ((constructor)) static 
void init_coregister_mutex(void)
{
    pthread_mutexattr_t mtxattr;
    pthread_mutexattr_init(&mtxattr);
    pthread_mutexattr_settype(&mtxattr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&registration_mutex, &mtxattr);
    pthread_cond_init(&registration_cond, NULL);
}

static
void coaccept_init(
    void **code_cap,
    void **data_cap, 
    const char *target_name,
    void **target_cap)
{
    pthread_mutex_lock(&registration_mutex);

    if(cosetup(COSETUP_COACCEPT, code_cap, data_cap) != 0)
        err(errno, "coaccept_init: could not cosetup");

    if (target_name == NULL) {
        //printf("coaccept_init: null target_name\n");
        *target_cap = *data_cap;
    } else if (target_name[0] == '\0') {
        //printf("coaccept_init: empty string target_name\n");
        *target_cap = *data_cap;
    } else {
        //printf("coaccept_init: coregistering as %s\n", target_name);
        if(coregister(target_name, target_cap) != 0)
            err(errno, "coaccept_init: could not coregister with name %s", target_name);
    }

    pthread_cond_signal(&registration_cond);
    pthread_mutex_unlock(&registration_mutex);
    return;
}

void 
start_coaccept_worker(worker_args_t *thread_args)
{
    pthread_attr_t thr_attr;
    int error;

    pthread_attr_init(&thr_attr);

    pthread_mutex_lock(&registration_mutex);

    error = pthread_create(&thread_args->worker, &thr_attr, coaccept_worker, thread_args);
    if (error != 0)
        err(errno, "start_coaccept_worker: could not spawn thread");

    pthread_cond_wait(&registration_cond, &registration_mutex);
    pthread_mutex_unlock(&registration_mutex);
    return;
}


void 
start_sloaccept_worker(worker_args_t *thread_args)
{
    pthread_attr_t thr_attr;
    int error;

    pthread_attr_init(&thr_attr);

    pthread_mutex_lock(&registration_mutex);

    error = pthread_create(&thread_args->worker, &thr_attr, sloaccept_worker, thread_args);
    if (error != 0)
        err(errno, "start_sloaccept_worker: could not spawn thread");

    pthread_cond_wait(&registration_cond, &registration_mutex);
    pthread_mutex_unlock(&registration_mutex);
    return;
}

void *coaccept_worker(void *worker_argp)
{
	void *sw, *scb, *cookie;
    void **cookie_cap;
	cocall_args_t cocall_args, *cocall_args_ptr;

    cookie_cap = &cookie;
    
    cocall_args_ptr = &cocall_args;
    cocall_args_ptr = cheri_setbounds(cocall_args_ptr, sizeof(cocall_args_t));
    memset(cocall_args_ptr, '\0', cheri_getlen(cocall_args_ptr));
    assert(cheri_local(cocall_args_ptr));
    assert(cheri_getperm(cocall_args_ptr) & CHERI_PERM_STORE_LOCAL_CAP);

	worker_args_t *worker_args = worker_argp;
	coaccept_init(&sw, &scb, worker_args->name, &worker_args->scb_cap);
	for(;;) {
		if(coaccept(sw, scb, &cookie, cocall_args_ptr, sizeof(cocall_args_t)) == 0) {
            if(cheri_gettag(worker_args->validation_function) != 0) {
                if((*worker_args->validation_function)(cocall_args_ptr) == 0) {
                    cocall_args_ptr->status = -1;
                    cocall_args_ptr->error = EINVAL;
                    continue;
                }
            }
            (*worker_args->worker_function)(cocall_args_ptr, cookie);
        } else
			err(errno, "coaccept_worker: worker failed to coaccept");
	}
    return (worker_args);
}

void *sloaccept_worker(void *worker_argp)
{
    void *sw, *scb, *cookie;
    struct _cocall_args cocall_args, *cocall_args_ptr;
    
    cocall_args_ptr = &cocall_args;
    cocall_args_ptr = cheri_setbounds(cocall_args_ptr, sizeof(struct _cocall_args));
    memset(cocall_args_ptr, '\0', cheri_getlen(cocall_args_ptr));
    assert(cheri_local(cocall_args_ptr));
    assert(cheri_getperm(cocall_args_ptr) & CHERI_PERM_STORE_LOCAL_CAP);

    worker_args_t *worker_args = worker_argp;
    coaccept_init(&sw, &scb, worker_args->name, &worker_args->scb_cap);
    for(;;) {
        if(coaccept_slow(sw, scb, &cookie, cocall_args_ptr, sizeof(struct _cocall_args)) == 0) {
            if(cheri_gettag(worker_args->validation_function) != 0) 
                if((*worker_args->validation_function)(cocall_args_ptr) == 0) {
                    cocall_args_ptr->status = -1;
                    cocall_args_ptr->error = EINVAL;
                    continue;
                }
            (*worker_args->worker_function)(cocall_args_ptr, cookie);
        } else
            err(errno, "coaccept_worker: worker failed to coaccept");
    }
    return (worker_args);
}

