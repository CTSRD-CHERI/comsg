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
//ECATS-BSD
#include <cocall/cocall_args.h>

#include <coproc/utils.h>
#include <cocall/worker.h>

#include <cheri/cheric.h>
#include <cheri/cherireg.h>

#include <assert.h>
#include <err.h>
#include <sys/errno.h>
#include <pthread.h>
#include <unistd.h>


cocall_args_t test_struct_ccargs;

static
void coaccept_init(
    void **code_cap,
    void **data_cap, 
    const char *target_name,
    void **target_cap)
{
    if(cosetup(COSETUP_COACCEPT, code_cap, data_cap) != 0)
        err(errno, "coaccept_init: could not cosetup");

    if (target_name != NULL) {
        if (target_name[0] == '\0') {
            *target_cap = data_cap;
            return;
        }
        else if(coregister(target_name, target_cap) != 0) 
            err(errno, "coaccept_init: could not coregister with name %s", target_name);
    }
    else
        *target_cap = data_cap;
    
    return;
}

void *coaccept_worker(void *worker_argp)
{
	void *sw, *scb, *cookie;
	struct _cocall_args cocall_args, *cocall_args_ptr;
    
    cocall_args_ptr = &cocall_args;
    cocall_args_ptr = cheri_setbounds(cocall_args_ptr, sizeof(struct _cocall_args));
    assert(cheri_local(cocall_args_ptr));
    assert(cheri_getperm(cocall_args_ptr) & CHERI_PERM_STORE_LOCAL_CAP);

	worker_args_t *worker_args = worker_argp;
	coaccept_init(&sw, &scb, worker_args->name, &worker_args->scb_cap);
	for(;;) {
		if(coaccept(sw, scb, &cookie, cocall_args_ptr, sizeof(struct _cocall_args)) == 0) {
            if(worker_args->validation_function != NULL) 
                if(!worker_args->validation_function(cocall_args_ptr)) {
                    cocall_args_ptr->status = -1;
                    cocall_args_ptr->error = EINVAL;
                    continue;
                }
            worker_args->worker_function(cocall_args_ptr, cookie);
        } else
			err(errno, "coaccept_worker: worker failed to coaccept");
	}
    return (worker_args);
}

