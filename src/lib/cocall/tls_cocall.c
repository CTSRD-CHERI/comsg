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
#include <cocall/tls_cocall.h>

#include <err.h>
#include <stdbool.h>
#include <sys/errno.h>
#include <threads.h>
#include <unistd.h>

static thread_local bool did_cocall_setup = false;
static thread_local bool did_coaccept_setup = false;

static void
cocall_init(int what)
{
	int error;

	switch (what) {
	case COSETUP_COCALL:
		error = cosetup(COSETUP_COCALL);
		did_cocall_setup = true;
		break;
	case COSETUP_COACCEPT:
		error = cosetup(COSETUP_COACCEPT);
		did_coaccept_setup = true;
		break;
	default:
		err(EINVAL, "cocall_init: invalid target for cosetup %d", what);
		break;
	}

	if(error != 0)
		err(errno, "cocall_init: error in cosetup(2)");
}

int
cocall_tls(void *target, void *buffer, size_t len)
{
	if (!did_cocall_setup) 
		cocall_init(COSETUP_COCALL);
	return (cocall(target, buffer, len, buffer, len));
}

int
slocall_tls(void *target, void *buffer, size_t len)
{
	if (!did_cocall_setup) 
		cocall_init(COSETUP_COCALL);
	return (cocall_slow(target, buffer, len, buffer, len));
}

int
coaccept_tls(void **tokenp, void *buffer, size_t len)
{
	if (!did_coaccept_setup) 
		cocall_init(COSETUP_COACCEPT);
	return (coaccept(tokenp, buffer, len, buffer, len));
}

int
sloaccept_tls(void **tokenp, void *buffer, size_t len)
{
	if (!did_coaccept_setup) 
		cocall_init(COSETUP_COACCEPT);
	return (coaccept_slow(tokenp, buffer, len, buffer, len));
}