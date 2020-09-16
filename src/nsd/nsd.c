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

/*
 * Functions provided:
 * create child namespace - requires ownership of the parent namespace
 * create object within specified namespace - requires write permission to the namespace 
 * list objects within specified namespace - requires read permission to the namespace 
 * retrieve handle to named object from specified namespace - requires read permission to the namespace
 * delete object from namespace - requires ownership of the object and write permission to the namespace; or ownership of the namespace
 * delete namespace - requires ownership; certain types are deleted by the system.
 * 
 */

/* 
 * Responsibilities:
 * track lifetime of coprocess-aware processes and threads inside them
 * 	+ creates and deletes namespaces for them 
 * 	+ manages the global namespace
 * 	+ performs cleanup on thread/program exit
 * 		+ deletes coservices provided by dead threads
 */
#include "nsd.h"
#include "nsd_crud.h"
#include "nsd_setup.h"

#include <ccmalloc.h>
#include <comsg/ukern_calls.h>

#include <err.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <unistd.h>

static 
void usage(void)
{
	//todo
	//should be called with lookup string
	//e.g "coserviced lookup_string"
	exit(1);
}

const size_t nbuckets = 2;
size_t buckets[] = {sizeof(struct _ns_members), sizeof(struct _ns_member)}


int main(int argc, char const *argv[])
{
	int opt, error;
	void *init_cap;
	
	while((opt == getopt(argc, argv, "")) != -1) {
		switch (opt) {
		case '?':
		default: 
			usage();
			break;
		}
	}
	if(argc >= 2) {
		error = colookup(argv[argc], &init_cap);
		if(error)
			err(error, "main: colookup of init %s failed", optarg);
		set_cocall_target(ukern_call_set, COCALL_COPROC_INIT, init_cap);
	} else {
		printf("Missing lookup string for init\n");
		usage();
	}

	//we can dance if we want to
	global_ns = create_namespace("coproc", GLOBAL, NULL);
	ccmalloc_init(buckets, nbuckets);
	
	init_services();

	for (int i = 0; i < coinsert_serv.function_map->nworkers; i++)
		pthread_join(coinsert_serv.function_map->workers[i].worker, NULL);
	for (int i = 0; i < codelete_serv.function_map->nworkers; i++)
		pthread_join(codelete_serv.function_map->workers[i].worker, NULL);


	return (0);
}