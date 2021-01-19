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
#include <comsg/coport_ipc.h>
#include <coproc/namespace.h>
#include <coproc/namespace_object.h>
#include <coproc/coport.h>
#include <comsg/ukern_calls.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <unistd.h>

static int verbose_flag = 0;

const static char *ns_name = "coport_example_namespace";
const static char *port_name = "example_copipe";

#define DEBUG(...) do { if (verbose_flag) printf(__VA_ARGS__); } while(0)

static void
init_microkernel_access(void)
{
	int error;
	pid_t recv_pid, coprocd_pid;
	void *coproc_init;

	error = colookup(U_COPROC_INIT, &coproc_init);
	if (error == 0) {
		set_ukern_target(COCALL_COPROC_INIT, coproc_init);
	} else 
		err(EINVAL, "init_microkernel: coprocd is not running.");
}

static namespace_t *ipc_ns;
static nsobject_t *port_obj;

static void
setup_coport_ipc(void)
{
	/* create/lookup ipc namespace */
	ipc_ns = cocreate(ns_name, APPLICATION, global_ns);
	if (ipc_ns == NULL)
		err(errno, "setup_coport_ipc: could not create namespace %s", ns_name);
	/* create coport + add a named handle to it in our ipc namespace */
	port_obj = coselect(port_name, COPORT, ipc_ns);
	if (port_obj == NULL)
		err(errno, "setup_coport_ipc: could not create coport %s", port_name);
}

static void 
do_send(void)
{
	char *msg_buf;
	ssize_t sent_len;
	size_t msg_len;

	msg_len = 1024;

	msg_buf = malloc(msg_len);
	memset(msg_buf, '1', msg_len);
	sent_len = cosend(port_obj->coport, msg_buf, cheri_getlen(msg_buf));
	if (sent_len == -1)
		err(errno, "do_send: error in cosend");
	
	printf("%s: do_send: sent %ld bytes\n", getprogname(),  sent_len);
	free(msg_buf);
}

static void 
usage(void)
{
	exit(-1);
}

int main(int argc, char *const argv[])
{
	int error, opt;
	pid_t sender, recver;

	while((opt = getopt(argc, argv, "")) != -1) {
		switch (opt) {
		case 'v':
			verbose_flag = 1;
			break;
		case '?':
		default: 
			usage();
			break;
		}
	}
	init_microkernel_access();
	global_ns = coproc_init(NULL, NULL, NULL, NULL);

	setup_coport_ipc();
	do_send();
	
	return (0);
}