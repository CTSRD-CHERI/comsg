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

#include <string.h>
#include <sys/types.h>
#include <unistd.h>

extern char **environ;

static int verbose_flag = 0;

static char *coprocd_args[] = {"/usr/bin/coprocd", NULL};
static char *sender_path = "/usr/bin/cosender";

const static char ns_name = "coport_example_namespace";
const static char port_name = "example_copipe";

#define DEBUG(...) do { if (verbose_flag) printf(__VA_ARGS__); } while(0)

static pid_t 
spawn_sender(void)
{
	pid_t recv_pid, send_pid;
	char *send_args[2];

	send_args[0] = strdup(sender_path);
	send_args[1] = NULL;
	recv_pid = getpid();

	sender_pid = vfork();
	if (sender_pid == 0)
		coexecve(recv_pid, send_args[0], send_args, environ);
	else
		return (sender_pid);
}

static void
start_microkernel(void)
{
	int error;
	pid_t recv_pid, coprocd_pid;
	void *coproc_init;

	error = colookup(U_COPROC_INIT, &coproc_init);
	if (error == 0) {
		set_ukern_target(COCALL_COPROC_INIT, coproc_init);
		return;
	}

	DEBUG("coprocd does not appear to be running in this address space. starting it now...\n");
	recv_pid = getpid();
	coprocd_pid = vfork();
	if (coprocd_pid == 0)
		coexecve(recv_pid, coprocd_args[0], coprocd_args, environ);
	do {
		sleep(1);
		error = colookup(U_COPROC_INIT, &coproc_init);
	} while(error != 0);
	set_ukern_target(COCALL_COPROC_INIT, coproc_init);
	DEBUG("coprocd started.\n");
}

static namespace_t *ipc_ns;
static nsobject_t *port_obj;
static coport_t *port;

static void
setup_coport_ipc(void)
{
	/* create/lookup ipc namespace */
	ipc_ns = cocreate(ns_name, APPLICATION, global_ns);
	if (ipc_ns == NULL)
		err(errno, "setup_coport_ipc: could not create namespace %s", ns_name);
	/* create coport + add a named handle to it in our ipc namespace */
	port_obj = open_named_coport(port_name, COPIPE, ipc_ns);
	if (port_obj == NULL)
		err(errno, "setup_coport_ipc: could not create coport %s", port_name);
}

static void 
do_recv(void)
{
	char *msg_buf;
	ssize_t recv_len;
	size_t msg_len;

	msg_len = 1024;

	msg_buf = malloc(msg_len);
	memset(msg_buf, '\0', msg_len);
	recv_len = corecv(port, msg_buf, msg_len);
	if (recv_len == -1)
		err(errno, "do_recv: error in corecv");
	
	printf("do_recv: received %ld bytes", recv_len);
	free(msg_buf);
}

static void 
teardown_coport_ipc(void)
{
	int error;

	error = coclose(port);
	if (error != 0)
		err(errno, "teardown_coport_ipc: could not close coport");
	
	error = codelete(port_obj, ipc_ns);
	if (error != 0)
		err(errno, "teardown_coport_ipc: could not delete coport nsobj");

	error = codrop(ipc_ns, global_ns);
	if (error != 0)
		err(errno, "teardown_coport_ipc: could not delete ipc namespace");
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
	start_microkernel();
	global_ns = coproc_init(NULL, NULL, NULL, NULL);
	setup_coport_ipc();

	sender_pid = spawn_sender();

	do_recv();
	teardown_coport_ipc();
	return (0);
}