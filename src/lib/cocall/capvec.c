/*
 * Copyright (c) 2021 Peter S. Blandford-Baker
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
#include <cocall/capvec.h>

#include <cheri/cheric.h>

#include <assert.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>

struct coexecve_capvec *
capvec_allocate(ssize_t maxlen)
{
	struct coexecve_capvec *capvec;

	assert(maxlen > 0);

	capvec = calloc(1, sizeof(struct coexecve_capvec));
	capvec->capv = calloc(maxlen + 1, sizeof(void **));

	capvec->length = maxlen + 1;
	atomic_store_explicit(&capvec->index, 0, memory_order_release);

	return (capvec);
}

void
capvec_append(struct coexecve_capvec *capvec, void *cap)
{
	ssize_t i, l;
	
	i = atomic_load_explicit(&capvec->index, memory_order_acquire);
	l = capvec->length;
	
	assert(i != -1);
	assert(i + 1 < l);

	assert(atomic_compare_exchange_strong_explicit(&capvec->index, &i, i + 1, memory_order_acq_rel, memory_order_acquire));

	capvec->capv[i] = cap; 
}

void
capvec_free(struct coexecve_capvec *capvec)
{
	free(capvec);
}

void **
capvec_finalize(struct coexecve_capvec *capvec)
{
	void **capv;
	ssize_t i, l;

	i = atomic_load_explicit(&capvec->index, memory_order_acquire);
	l = capvec->length;

	assert(i != -1);
	assert(i + 1 <= l);

	atomic_store_explicit(&capvec->index, -1, memory_order_release);

	capv = capvec->capv;
	capv[i] = NULL;
	l = sizeof(void *) * (i+1);
	capv = cheri_setbounds(capv, l);

	return (capv);
}
