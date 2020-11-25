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
#include "otyped.h"

#include <cocall/worker_map.h>
#include <comsg/ukern_calls.h>
#include <coproc/otype.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <unistd.h>

coservice_provision_t ukernel_alloc_serv, user_alloc_serv;
long reserved_ukernel_types;

static void
usage(void)
{
	exit(1);
}

int main(int argc, char *const argv[])
{
	int opt, error;
	void *init_cap;
	
	opterr = 0;
	reserved_ukernel_types = -1;
	while((opt = getopt(argc, argv, "n:")) != -1) {
		switch (opt) {
		case 'n':
			reserved_ukernel_types = strtol(optarg, NULL, 10);
			if (reserved_ukernel_types <= 0 || reserved_ukernel_types > COPROC_OTYPE_SPACE_LEN)
				err(EINVAL, "cannot reserve %s ukernel types", optarg);
			break;
		case '?':
		default: 
			usage();
			break;
		}
	}
	
	if(argc >= 2) {
		error = colookup(argv[argc-1], &init_cap);
		if(error)
			err(error, "main: colookup of init %s failed", argv[argc-1]);
		set_ukern_target(COCALL_COPROC_INIT, init_cap);
	} else {
		printf("Missing lookup string for init\n");
		usage();
	}

	otyped_startup();

	for (int i = 0; i < ukernel_alloc_serv.function_map->nworkers; i++)
		pthread_join(ukernel_alloc_serv.function_map->workers[i].worker, NULL);
	for (int i = 0; i < user_alloc_serv.function_map->nworkers; i++)
		pthread_join(user_alloc_serv.function_map->workers[i].worker, NULL);

	return (0);
}