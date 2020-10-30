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
#include <comsg/coport_ipc.h>
#include <comsg/coport_ipc_cinvoke.h>
#include <coproc/namespace.h>
#include <coproc/namespace_object.h>
#include <coproc/coport.h>

#include <err.h>
#include <machine/sysarch.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <unistd.h>

static char *coprocd_args[] = {"/usr/bin/coprocd", NULL};
extern char **environ;
static void *sealroot;

static const char default_name[] = "coproctestA";
static char test_str[] = "Testing...";

static void 
do_tests(void)
{
	namespace_t *proc_ns;
	coport_t *port;
	nsobject_t *port_obj;
	int sent, recvd;
	char *recv;
	char *ns_name = calloc(32, sizeof(char));

	strcpy(ns_name, default_name);

coproc_init_lbl:
	printf("coproctest: initializing coproc...");
		global_ns = coproc_init(NULL, NULL, NULL, NULL);
	if (global_ns != NULL) 
		printf("\t success!\n");
	else {
		if (errno == EAGAIN) {
			printf("\t failed. retrying...\n");
			sched_yield();
			goto coproc_init_lbl;
		}
		printf("\n");
		err(errno, "do_tests: coproc_init failed");
	}
	printf("coproctest: creating namespace...");
cocreate_lbl:
	proc_ns = cocreate(ns_name, APPLICATION, global_ns);
	if (proc_ns != NULL) 
		printf("\tsuccess!\n");
	else {
		if (ns_name[10] < 'Z') {
			ns_name[10]++;
			goto cocreate_lbl;
		}
		err(errno, "coproctest: do_tests: cocreate failed");
	}
	printf("coproctest: creating named coport...");
	port_obj = open_named_coport("test_coport", COCHANNEL, proc_ns);
	if (port_obj != NULL)
		printf("\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: open_named_coport failed");
	printf("coproctest: sending message... \"%s\"", test_str);
	sent = cosend_cinvoke(port_obj->coport, test_str, strlen(test_str));
	if (sent > 0)
		printf("\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: cosend_cinvoke failed");
	printf("coproctest: receiving message...");
	recv = malloc(strlen(test_str)+1);
	recvd = corecv_cinvoke(port_obj->coport, (void **)&recv, strlen(test_str));
	if (recvd > 0)
		printf("\tsuccess! received \"%s\"\n", recv);
	else
		err(errno, "coproctest: do_tests: corecv_cinvoke failed");
}

int main(int argc, char *const argv[])
{
	int error;
	void *coproc_init_scb;
	pid_t test_pid, coprocd_pid;

    if (sysarch(CHERI_GET_SEALCAP, &sealroot) < 0)
    	err(errno, "setup_otypes: error in sysarch - could not get sealroot");

    error = colookup(U_COPROC_INIT, &coproc_init_scb);
    if (error != 0) {
	    test_pid = getpid();
	    coprocd_pid = fork();
	    if (coprocd_pid == 0)
	    	coexecve(test_pid, coprocd_args[0], coprocd_args, environ);
 
	    do {
	    	sched_yield();
	    	error = colookup(U_COPROC_INIT, &coproc_init_scb);
	    	sched_yield();
	    	sched_yield();
	    	sched_yield();
	    } while(error != 0);
	}
    set_ukern_target(COCALL_COPROC_INIT, coproc_init_scb);

	
    do_tests();

	return (0);
}