/*
 * Copyright (c) 2021 Peter S. Blandford-Baker
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

#include <cocall/worker_map.h>
#include <comsg/ukern_calls.h>
#include <comsg/coport_ipc.h>
#include <comsg/coport_ipc_cinvoke.h>
#include <comsg/namespace.h>
#include <comsg/namespace_object.h>
#include <comsg/coport.h>

#include <err.h>
#include <machine/sysarch.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <statcounters.h>
#include <string.h>
#include <sysexits.h>
#include <sys/errno.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

static char *coprocd_args[] = {"/usr/bin/coprocd", NULL};
static char *child_path = "/usr/bin/comsg_benchmark";

extern char **environ;
static void *sealroot;
static pid_t monitored_child;

static const char default_name[] = "coproctestA";
static char test_str[] = "Testing...";

static coport_t *copipe = NULL;
static coport_t *cocarrier = NULL;
static coport_t *cochannel = NULL;

static char *buffer = NULL;
static char *cocarrier_buf = NULL;
static const ssize_t maxlen = 128 * 1024 * 1024; //128MiB because not sure how much ram we have, and must account  multiple copies
static ssize_t message_len = maxlen;
static ssize_t iterations = 1;


typedef enum {OP_INVALID = 0, OP_COSEND = 1, OP_CORECV = 2} coport_op;

struct benchmark_results {
	coport_type_t coport_type;
	coport_op op;
	statcounters_bank_t statcounter_diff;
	struct rusage rusage_diff;
	struct timespec timespec_diff;
	SLIST_ENTRY(benchmark_result) entries;
};

SLIST_HEAD(, benchmark_result) results_list;

static void
timevalsub(struct timeval *result, struct timeval *end, const struct timeval *start)
{

	result->tv_sec = end->tv_sec - start->tv_sec;
	result->tv_usec = end->tv_usec - start->tv_usec;
	if (result->tv_usec < 0) {
		result->tv_sec--;
		result->tv_usec += 1000000;
	}
	if (result->tv_usec >= 1000000) {
		result->tv_sec++;
		result->tv_usec -= 1000000;
	}
}

static void
clock_diff(struct timespec *result, struct timespec *end, struct timespec *start)
{
	result->tv_sec = end->tv_sec - start->tv_sec;
	result->tv_nsec = end->tv_nsec - start->tv_nsec;
	if (result->tv_nsec < 0) {
		result->tv_sec--;
		result->tv_nsec += 1000000000L;
	}
}

static void
rusage_diff(struct rusage *result, struct rusage *end, struct rusage *start)
{
	result->ru_utime = end->ru_utime;
	timevalsub(&result->ru_utime, &start->ru_utime);	/* user time used */
	result->ru_stime = end->ru_stime;
	timevalsub(&result->ru_stime,&start->ru_stime);	/* system time used */

	result->ru_maxrss = end->ru_maxrss - start->ru_maxrss;		/* max resident set size */
	result->ru_ixrss = end->ru_ixrss - start->ru_ixrss;		/* integral shared memory size */
	result->ru_idrss = end->ru_idrss - start->ru_idrss;		/* integral unshared data " */
	result->ru_isrss = end->ru_isrss - start->ru_isrss;		/* integral unshared stack " */
	result->ru_minflt = end->ru_minflt - start->ru_minflt;		/* page reclaims */
	result->ru_majflt = end->ru_majflt - start->ru_majflt;		/* page faults */
	result->ru_nswap = end->ru_nswap - start->ru_nswap;		/* swaps */
	result->ru_inblock = end->ru_inblock - start->ru_inblock;		/* block input operations */
	result->ru_oublock = end->ru_oublock - start->ru_oublock;		/* block output operations */
	result->ru_msgsnd = end->ru_msgsnd - start->ru_msgsnd;		/* messages sent */
	result->ru_msgrcv = end->ru_msgrcv - start->ru_msgrcv;		/* messages received */
	result->ru_nsignals = end->ru_nsignals - start->ru_nsignals;		/* signals received */
	result->ru_nvcsw = end->ru_nvcsw - start->ru_nvcsw;		/* voluntary context switches */
	result->ru_nivcsw = end->ru_nivcsw - start->ru_nivcsw;		/* involuntary " */
}

static void
spawn_ukernel(pid_t my_pid)
{
	int error;
	int retries;
	pid_t coprocd_pid;
	void *coproc_init_scb;

	error = colookup(U_COPROC_INIT, &coproc_init_scb);
    if (error != 0) {
	    coprocd_pid = vfork();
	    if (coprocd_pid == 0)
	    	coexecve(my_pid, coprocd_args[0], coprocd_args, environ);
	    do {
	    	sleep(1);
	    	error = colookup(U_COPROC_INIT, &coproc_init_scb);
	    	sleep(1);
	    } while(error != 0);
	}
    set_ukern_target(COCALL_COPROC_INIT, coproc_init_scb);

	retries = 0;
coproc_init_lbl:
	root_ns = coproc_init(NULL, NULL, NULL, NULL);
	if (root_ns == NULL) {
		if (errno == EAGAIN) {
			sleep(1);
			retries++;
			goto coproc_init_lbl;
		}
		err(errno, "do_setup: coproc_init failed");
	}

}

static void
do_recv_setup(void)
{
	int retries;
	int status;
	int error;
	pid_t child_pid;
	pid_t my_pid;
	void *capv[4];
	char *child_args[5];
	char *fmt_arg;
	
	my_pid = getpid();
	spawn_ukernel(my_pid);

	//open coports and pass to child process (sender)
	copipe = coopen(COPIPE);
	cocarrier = coopen(COCARRIER);
	cochannel = coopen(COCHANNEL);

	if (copipe == NULL 
		|| cocarrier == NULL 
		|| cochannel == NULL) {
		err(errno, "do_setup: coopen failed");
	} else {
		capv[0] = copipe;
		capv[1] = cocarrier;
		capv[2] = cochannel;
		capv[3] = NULL;
	}
	
	child_args[0] = strdup(child_path);
	child_args[1] = strdup("-s");

	fmt_arg = calloc(32, sizeof(char));
	fmt_arg = sprintf(fmt_age, "-b %lu", message_len);
	child_args[2] = strdup(fmt_arg);
	memset(fmt_arg, '\0', __builtin_cheri_length_get(fmt_arg));
	fmt_arg = sprintf(fmt_age, "-i %lu", iterations);
	child_args[3] = strdup(fmt_arg);
	free(fmt_arg);

	child_args[4] = NULL;


	//spawn sender process
	child_pid = rfork(RFSPAWN);
	if (child_pid == 0) {
        error = coexecvec(my_pid, child_args[0], child_args, environ, capv);
        _exit(EX_UNAVAILABLE); /* Should not reach unless error */
    } else if (daemon_pid == -1) {
        warn("%s: rfork failed", __func__);
        return (-1);
    }

    /* check that the sender actually started */
    pid = waitpid(child_pid, &status, (WNOHANG | WEXITED));
    if (pid > 0) {
        assert(WIFEXITED(status));
        errno = ECHILD;
        return (-1);
    } else if (pid == -1)
        return (-1);

    //allocate message buffer
    buffer = calloc(1, maxlen);

    //initialize results list
    SLIST_INIT(&results_list);
}

static void
do_warmup_recv(void)
{
	int error;
	//warmup run to synchronise sender/recver and warm up caches, touch memory, etc
	error = corecv(copipe, buffer, maxlen);
	if (error < 0)
		err(EX_SOFTWARE, "do_warmup_recv: error occurred in copipe corecv");
	//warmup run to synchronise sender/recver and warm up caches, touch memory, etc
	error = corecv(cocarrier, cocarrier_buf, maxlen);
	if (error < 0)
		err(EX_SOFTWARE, "do_warmup_recv: error occurred in copipe corecv");
	//warmup run to synchronise sender/recver and warm up caches, touch memory, etc
	error = corecv(cochannel, buffer, maxlen);
	if (error < 0)
		err(EX_SOFTWARE, "do_warmup_recv: error occurred in cochannel corecv");
	memset(buffer, '\0', maxlen);
}

static void
do_warmup_send(void)
{
	int error;

	fill_buffer();
	//warmup run to synchronise sender/recver and warm up caches, touch memory, etc
	error = cosend(copipe, buffer, maxlen);
	if (error < 0)
		err(EX_SOFTWARE, "do_warmup_send: error occurred in copipe corecv");
	//warmup run to synchronise sender/recver and warm up caches, touch memory, etc
	error = cosend(cocarrier, cocarrier_buf, maxlen);
	if (error < 0)
		err(EX_SOFTWARE, "do_warmup_send: error occurred in copipe corecv");
	//warmup run to synchronise sender/recver and warm up caches, touch memory, etc
	error = cosend(cochannel, buffer, maxlen);
	if (error < 0)
		err(EX_SOFTWARE, "do_warmup_send: error occurred in cochannel corecv");
}

static struct benchmark_result * 
do_benchmark_send(void *buffer, size_t buffer_length, coport_t *port)
{
	struct benchmark_result *result;
	statcounters_bank_t start_bank, end_bank;
	struct rusage start_rusage, end_rusage;
	struct timespec start_ts, end_ts;
	int status;

	//statcounters
	statcounters_zero(&start_bank);
	statcounters_zero(&end_bank);

	//get initial values for counters, for later comparison
	getrusage(RUSAGE_THREAD, &start_rusage);
	clock_gettime(CLOCK_REALTIME_PRECISE, &start_ts);
	statcounters_sample(&start_bank);

	//perform operation
	status = cosend(port, buffer, buffer_length);

	//get new values for counters
	statcounters_sample(&end_bank);
	clock_gettime(CLOCK_REALTIME_PRECISE, &end_ts);
	getrusage(RUSAGE_THREAD, &end_rusage);

	//calculate differences in counters
	result = calloc(1, sizeof(benchmark_result));
	clock_diff(&result->timespec_diff, &end_ts, &start_ts);
	rusage_diff(&result->rusage_diff, &end_rusage, &start_rusage);
	statcounters_diff(&result->statcounter_diff, &end_bank, &start_bank);
	result->op = COSEND;
	result->coport_type = coport_gettype(port);
	//output
	return (result);
}

static struct benchmark_result * 
do_benchmark_recv(void **buffer, size_t buffer_length, coport_t *port)
{
	struct benchmark_result *result;
	statcounters_bank_t start_bank, end_bank;
	struct rusage start_rusage, end_rusage;
	struct timespec start_ts, end_ts;
	int status;

	//statcounters
	statcounters_zero(&start_bank);
	statcounters_zero(&end_bank);

	//get initial values for counters, for later comparison
	getrusage(RUSAGE_THREAD, &start_rusage);
	clock_gettime(CLOCK_REALTIME_PRECISE, &start_ts);
	statcounters_sample(&start_bank);

	//perform operation
	status = corecv(port, buffer, buffer_length);

	//get new values for counters
	statcounters_sample(&end_bank);
	clock_gettime(CLOCK_REALTIME_PRECISE, &end_ts);
	getrusage(RUSAGE_THREAD, &end_rusage);

	//calculate differences in counters
	result = calloc(1, sizeof(benchmark_result));
	clock_diff(&result->timespec_diff, &end_ts, &start_ts);
	rusage_diff(&result->rusage_diff, &end_rusage, &start_rusage);
	statcounters_diff(&result->statcounter_diff, &end_bank, &start_bank);
	result->op = CORECV;

	//output
	return (result);
}

static void
do_benchmark_run(coport_t *port)
{
	if (mode == OP_COSEND) {
		result = do_benchmark_send(buffer, message_len, port);
	} else {
		result = do_benchmark_recv(buffer, message_len, port);
	}
	SLIST_INSERT_HEAD(&results_list, result, entries);
}

static void
do_benchmark(void)
{
	coport_t *port;
	ssize_t i;
	struct benchmark_result *result;

	if (mode == OP_COSEND)
		do_warmup_send();
	else
		do_warmup_recv();

	for (i = 0; i < iterations; i++) {
		do_benchmark_run(copipe);
		do_benchmark_run(cocarrier);
		do_benchmark_run(cochannel);
	}
}

static void
process_capvec(void)
{
	int error;
	void *capv[4];

	error = elf_aux_info(AT_CAPV, &capv, sizeof(capv));
	if (capv[3] != NULL)
		err(EINVAL, "process_capvec: invalid capvec format");
	//should validate these better
	copipe = capv[0];
	cocarrier = capv[1];
	cochannel = capv[2];
}

int main(int argc, char *const argv[])
{
	int opt, error;
	char *strptr;

	while((opt = getopt(argc, argv, "sb:i:")) != -1) {
		switch (opt) {
		case 's':
			mode = OP_COSEND;
			break;
		case 'b':
			message_len = strtol(optarg, &strptr, 10);
			if (*optarg == '\0' || *strptr != '\0' || message_len < 0 || message_len > maxlen)
				err(EX_USAGE, "invalid buffer length");
			break;
		case 'i':
			iterations =  strtol(optarg, &strptr, 10);
			if (*optarg == '\0' || *strptr != '\0' || iterations <= 0)
				err(EX_USAGE, "invalid number of iterations");
		case '?':
		default: 
			err(EX_USAGE, "invalid flag '%c'", (char)optopt);
			break;
		}
	}
    do_benchmark();
    process_results();

	return (0);
}