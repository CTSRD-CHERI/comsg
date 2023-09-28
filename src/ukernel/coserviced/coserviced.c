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
#include "coserviced.h"

#include "coserviced_setup.h"
#include "coservice_table.h"

#include <comsg/comsg_args.h>
#include <comsg/coservice.h>
#include <cocall/endpoint.h>
#include <comsg/ukern_calls.h>

#include <err.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <unistd.h>

#pragma push_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma push_macro("COACCEPT_ENDPOINT")
#define DECLARE_COACCEPT_ENDPOINT(name, validate_f, operation_f) COACCEPT_ENDPOINT(name, COCALL_##name, validate_f, operation_f)
#define COACCEPT_ENDPOINT(name, op, validate, func) \
coservice_provision_t name##_serv;
#include "coaccept_endpoints.inc"
#pragma pop_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma pop_macro("COACCEPT_ENDPOINT")

size_t buckets[] = {CHERICAP_SIZE * COSERVICE_MAX_WORKERS};
size_t nbuckets = 1;

static const int nworkers = 16;

static void
usage(void)
{
	//todo
	//should be called with lookup string
	//e.g "coserviced lookup_string"
	exit(0);
}

int main(int argc, char *const argv[])
{
	int opt, error;
	
	is_ukernel = true;

	while((opt = getopt(argc, argv, "")) != -1) {
		switch (opt) {
		case '?':
		default: 
			usage();
			break;
		}
	}
	coserviced_startup();
	
	join_endpoint_thread();

	return (0);
}