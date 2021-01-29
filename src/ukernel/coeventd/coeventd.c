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
#include <comsg/ukern_calls.h>

#include <ccmalloc.h>
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

static void
usage(void)
{
	//todo
	//should be called with lookup string
	//e.g "coserviced lookup_string"
	exit(0);
}

static size_t buckets[] = {sizeof(struct cocallback), sizeof(struct cocallback_func)};
static size_t nbuckets = 2;

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
	if(argc >= 2) {
		error = colookup(argv[argc-1], &init_cap);
		if(error)
			err(errno, "main: colookup of init %s failed", argv[argc-1]);
		set_ukern_target(COCALL_COPROC_INIT, init_cap);
	} else {
		printf("%s: missing lookup string for init\n", getprogname());
		usage();
	}
	ccmalloc_init(buckets, nbuckets);
	coeventd_startup();

	return (0);
}