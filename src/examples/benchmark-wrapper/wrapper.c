/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <sysexits.h>

#include <comsg/ukern_calls.h>

extern char **environ;

static pid_t cochatter_pid, ipc_pid, memcpy_pid;
static int cochatter_status, ipc_status, memcpy_status;
static char *cochatter_args[6];
static char *ipc_args[5];
static char *memcpy_args[6];

static char *buf_size_str;
static ssize_t iterations = 24;
static bool enable_sha256_workload = false;
static bool enable_dummy_workload = false;
static bool explicit = false;
static bool do_ipc = false;
static bool do_memcpy = false;
static bool do_coproc = false;
static const size_t maxlen = 1048576UL * 2;
static size_t max_buf_size = maxlen;
static size_t initial_size = 1;


#define BENCHMARK_TIMEOUT (600)
static int
wait_for_pid(pid_t childpid)
{
	int status, error;
	pid_t exited;
	pid_t timeout_pid;

	timeout_pid = fork();
	if (timeout_pid == 0) {
		sleep(BENCHMARK_TIMEOUT);
		_exit(0);
	}
	exited = wait(&status);
	if (exited == childpid) {
		error = kill(timeout_pid, SIGKILL);
		error = waitpid(timeout_pid, NULL, WEXITED);
		return (status);
	} else if (exited == timeout_pid) {
		error = kill(childpid, SIGKILL);
		error = waitpid(childpid, NULL, WEXITED);
		printf("Timed out.\n");
		return (-1);
	} else
		err(EX_SOFTWARE, "%s: unexpected pid %d exited ", __func__, exited);

}

static void
do_benchmark_size(size_t i)
{
	memset(buf_size_str, '\0', 32);
	sprintf(buf_size_str, "-b %lu", i);
	
	cochatter_args[2] = strdup(buf_size_str);
	while (!explicit || (explicit && do_coproc)) {
		if(!(cochatter_pid = fork())) {
			//child
			if(execve(cochatter_args[0], cochatter_args, environ)){
				err(errno,"main:execve failed to execute comesg benchmark");
			}
		}	
		cochatter_status = wait_for_pid(cochatter_pid);
		if (cochatter_status == 0) 
			break;
		else
			printf("Retrying cochatter bmark %lu...\n", i);
	}
	free(cochatter_args[2]);

	ipc_args[2] = strdup(buf_size_str);
	while (!explicit || (explicit && do_ipc)) {
		if(!(ipc_pid = fork())) {
			//child
			if(execve(ipc_args[0], ipc_args, environ))
				err(errno,"main:execve failed to execute ipc benchmark");
		}	
		ipc_status = wait_for_pid(ipc_pid);
		if (ipc_status == 0)
			break;
		else
			printf("Retrying ipc bmark %lu\n", i);
	}
	free(ipc_args[2]);

	memcpy_args[2] = strdup(buf_size_str);
	while (!explicit || (explicit && do_memcpy)) {
		if(!(memcpy_pid = fork())) {
			//child
			if(execve(memcpy_args[0], memcpy_args, environ))
				err(errno,"main:execve failed to execute memcpy benchmark");
		}	
		memcpy_status = wait_for_pid(memcpy_pid);
		if (memcpy_status == 0) 
			break;
		else
			printf("Retrying memcpy bmark %lu\n", i);
	}
	free(memcpy_args[2]);

	printf("Done %lu bytes.\n", i);
}


int main(int argc, char * const argv[])
{
	int opt, error;
	char *strptr;
	char *iter_str;
	char *workload_str;
	size_t i;

	while((opt = getopt(argc, argv, "mcsi:b:t:dC")) != -1) {
		switch (opt) {
		case 'C':
			enable_sha256_workload = true;
		case 'd':
			enable_dummy_workload = true;
			break;
		case 'm':
			explicit = true;
			do_memcpy = true;
			break;
		case 'c':
			explicit = true;
			do_coproc = true;
			break;
		case 's':
			explicit = true;
			do_ipc = true;
			break;
		case 'i':
			iterations =  strtoul(optarg, &strptr, 10);
			if (*optarg == '\0' || *strptr != '\0' || iterations <= 0)
				err(EX_USAGE, "invalid number of iterations");
			break;
		case 'b':
			initial_size = strtoul(optarg, &strptr, 10);
			if (*optarg == '\0' || *strptr != '\0' || initial_size < 1 || initial_size > maxlen)
				err(EX_USAGE, "invalid buffer length");
			break;
		case 't':
			max_buf_size = strtoul(optarg, &strptr, 10);
			if (*optarg == '\0' || *strptr != '\0' || max_buf_size < 1 || max_buf_size < initial_size || max_buf_size > maxlen)
				err(EX_USAGE, "invalid buffer length");
			break;
		case '?':
		default: 
			err(EX_USAGE, "invalid flag '%c'", (char)optopt);
			break;
		}
	}

	iter_str = calloc(32, sizeof(char));
	sprintf(iter_str, "-i %ld", iterations);
	workload_str = enable_sha256_workload ? strdup("-c") : (enable_dummy_workload ? strdup("-d") : NULL);

	cochatter_args[0] = strdup("/usr/bin/comsg-benchmark");
	cochatter_args[1] = strdup(iter_str);
	cochatter_args[3] = workload_str == NULL ? NULL : strdup(workload_str);
	cochatter_args[4] = NULL;

	ipc_args[0] = strdup("/usr/bin/ipc-bmark");
	ipc_args[1] = strdup(iter_str);
	ipc_args[3] = workload_str == NULL ? NULL : strdup(workload_str);
	ipc_args[4] = NULL;

	memcpy_args[0] = strdup("/usr/bin/memcpy-bmark");
	memcpy_args[1] = strdup(iter_str);
	memcpy_args[3] = strdup("-q");
	memcpy_args[4] = workload_str == NULL ? NULL : strdup(workload_str);
	memcpy_args[5] = NULL;
	
	buf_size_str = calloc(32, sizeof(char));

	if (initial_size < 1024) {
		for (i = initial_size; i < 1024; i*=2)
			do_benchmark_size(i);	

		for(i = 1024; i <= max_buf_size; i+=1024)
			do_benchmark_size(i);
	} else {
		initial_size &= ~1023;
		for(i = initial_size; i <= max_buf_size; i+=1024)
			do_benchmark_size(i);
	}
	
	
	return (0);
}