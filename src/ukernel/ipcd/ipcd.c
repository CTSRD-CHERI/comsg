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
#include "ipcd.h"
#include "ipcd_startup.h"
#include <cocall/endpoint.h>

#include <cocall/cocalls.h>
#include <comsg/ukern_calls.h>

#include <comsg/coport.h>

#include <assert.h>
#include <err.h>
#include <sys/errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#pragma push_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma push_macro("COACCEPT_ENDPOINT")
#define DECLARE_COACCEPT_ENDPOINT(name, validate_f, operation_f) COACCEPT_ENDPOINT(name, COCALL_##name, validate_f, operation_f)
#define COACCEPT_ENDPOINT(name, op, validate, func) \
coservice_provision_t name##_serv;
#include "coaccept_endpoints.inc"
#pragma pop_macro("DECLARE_COACCEPT_ENDPOINT")
#pragma pop_macro("COACCEPT_ENDPOINT")

#pragma push_macro("DECLARE_SLOACCEPT_ENDPOINT")
#pragma push_macro("DECLARE_SLOACCEPT_ENDPOINT")
#define DECLARE_SLOACCEPT_ENDPOINT(name, validate_f, operation_f) SLOACCEPT_ENDPOINT(name, COCALL_##name, validate_f, operation_f)
#define SLOACCEPT_ENDPOINT(name, op, validate, func) \
coservice_provision_t name##_serv;
#include "sloaccept_endpoints.inc"
#pragma pop_macro("DECLARE_SLOACCEPT_ENDPOINT")
#pragma pop_macro("DECLARE_SLOACCEPT_ENDPOINT")

static 
void usage(void)
{
	//todo
	exit(0);
}

static size_t buckets[] = {COPORT_BUF_LEN, sizeof(coport_info_t), sizeof(coport_buf_t), sizeof(struct cocarrier_message), sizeof(coport_typedep_t), sizeof(comsg_attachment_t), COCARRIER_MAX_MSG_LEN};
static size_t nbuckets = 7;

/* TODO-PBB: Add a cocall allowing a coport "owner" to create send/recv only capabilities */

int main(int argc, char *const argv[])
{
	int opt, error;
	void *init_cap;
	
	is_ukernel = true;

	while((opt = getopt(argc, argv, "")) != -1) {
		switch (opt) {
		case '?':
		default: 
			usage();
			break;
		}
	}
	
	ipcd_startup();

	join_endpoint_thread();

	return (0);
}