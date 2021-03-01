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
#include <sys/errno.h>
#include <threads.h>
#include <unistd.h>

static void *coaccept_cap = NULL;
static void *cocall_cap = NULL;
static thread_local void *scb_cap = NULL;


static void
cocall_init(int target)
{
	int error;

	switch (target) {
	case COSETUP_COCALL:
		error = _cosetup(COSETUP_COCALL, &cocall_cap, &scb_cap);
		break;
	case COSETUP_COACCEPT:
		error = _cosetup(COSETUP_COACCEPT, &coaccept_cap, &scb_cap);
		break;
	default:
		err(EINVAL, "cocall_init: invalid target for cosetup %d", target);
		break;
	}

	if(error != 0)
		err(errno, "cocall_init: error in cosetup(2)");
}

void *
get_scb(void)
{
	if (scb_cap == NULL)
		cocall_init(COSETUP_COACCEPT);
	return (scb_cap);
}

int
cocall_tls(void *target, void *buffer, size_t len)
{
	if ((cocall_cap == NULL) || (scb_cap == NULL))
		cocall_init(COSETUP_COCALL);
	return (_cocall(cocall_cap, scb_cap, target, buffer, len, buffer, len));
}

int
slocall_tls(void *target, void *buffer, size_t len)
{
	if ((cocall_cap == NULL) || (scb_cap == NULL))
		cocall_init(COSETUP_COCALL);
	return (cocall_slow(target, buffer, len, buffer, len));
}

int
coaccept_tls(void **tokenp, void *buffer, size_t len)
{
	if ((coaccept_cap == NULL) || (scb_cap == NULL))
		cocall_init(COSETUP_COACCEPT);
	return (_coaccept(coaccept_cap, scb_cap, tokenp, buffer, len, buffer, len));
}

int
sloaccept_tls(void **tokenp, void *buffer, size_t len)
{
	if ((coaccept_cap == NULL) || (scb_cap == NULL))
		cocall_init(COSETUP_COACCEPT);
	return (coaccept_slow(tokenp, buffer, len, buffer, len));
}