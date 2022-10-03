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

#include <comsg/ukern_calls.h>
#include <comsg/coport_ipc.h>
#include <comsg/coport_ipc_cinvoke.h>
#include <comsg/namespace.h>
#include <comsg/namespace_object.h>
#include <comsg/coport.h>

#include <assert.h>
#include <err.h>
#include <cheri/cheri.h>
#include <machine/sysarch.h>
#define CPU_QEMU_RISCV
#include <machine/cpufunc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <pthread_np.h>
#include <statcounters.h>
#include <sched.h>
#include <string.h>

#include <sysexits.h>
#include <sys/auxv.h>
#include <sys/cpuset.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <sys/signal.h>
#include <sys/resource.h>
#include <sys/rtprio.h>
#include <sys/thr.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef enum {OP_INVALID = 0, OP_COSEND = 1, OP_CORECV = 2} coport_op;

struct benchmark_result {
	coport_type_t coport_type;
	coport_op op;
	ssize_t buf_len;
	statcounters_bank_t statcounter_diff;
	struct rusage rusage_diff;
	struct timespec timespec_diff;
	SLIST_ENTRY(benchmark_result) entries;
};

pthread_mutex_t results_mtx;
SLIST_HEAD(, benchmark_result) results_list;

struct thr_arg {
	coport_op thr_mode;
};

static char *coprocd_args[] = {"/usr/bin/coprocd", NULL};
static char *child_path = "/usr/bin/comsg-benchmark";

extern char **environ;

static pid_t coprocd_pid;

static pid_t sender_pid = -1;
static pid_t recver_pid = -1;
static long sender_lwpid = -1;
static long recver_lwpid = -1;

static int aggregate_mode = 0;
static _Thread_local coport_op mode = OP_CORECV;

static coport_t *copipe = NULL;
static coport_t *cocarrier = NULL;
static coport_t *cochannel = NULL;

static _Thread_local char *buffer = NULL;
static _Thread_local char *cocarrier_buf = NULL;
static const ssize_t maxlen = 1024 * 1024; //128MiB because not sure how much ram we have, and must account  multiple copies
static ssize_t message_len = maxlen;
static ssize_t iterations = 1;

static uint64_t instruction_count_id = 0;
static statcounters_fmt_flag_t format = CSV_HEADER; 

#ifdef BMARK_PROC
#undef BMARK_PROC
#endif

static void
get_instruction_count_id(void)
{
	instruction_count_id = statcounters_id_from_name("instructions");
}

static void
process_capvec(void)
{
	int error;
	void **capv;

	error = elf_aux_info(AT_CAPV, &capv, sizeof(capv));
	if (capv[3] != NULL)
		err(EINVAL, "process_capvec: invalid capvec format");
	//should validate these better
	copipe = capv[0];
	set_coport_handle_type(copipe, COPIPE);

	cocarrier = capv[1];
	set_coport_handle_type(cocarrier, COCARRIER);
	
	cochannel = capv[2];
	set_coport_handle_type(cochannel, COCHANNEL);

}


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
	timevalsub(&result->ru_utime,&end->ru_utime, &start->ru_utime);	/* user time used */
	result->ru_stime = end->ru_stime;
	timevalsub(&result->ru_stime,&end->ru_stime, &start->ru_stime);	/* system time used */

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

static const char *
getarchname(void)
{
	const char *result = "unknown_arch";
	result = STATCOUNTERS_ARCH STATCOUNTERS_ABI;
	return result;
}

static
int rusage_dump_with_args (
    const struct rusage * const b,
    const char * progname,
    const char * phase,
    const char * archname,
    FILE * const fileptr,
    const statcounters_fmt_flag_t format_flag)
{
    // preparing default values for NULL arguments
    // displayed progname
#define MAX_NAME_SIZE 512
    if (!progname) {
        progname = getenv("STATCOUNTERS_PROGNAME");
        if (!progname || progname[0] == '\0')
	    progname = getprogname();
    }
    size_t pname_s = strnlen(progname,MAX_NAME_SIZE);
    size_t phase_s = 0;
    if (phase) {
        phase_s = strnlen(phase,MAX_NAME_SIZE);
    }
    char * pname = malloc((sizeof(char) * (pname_s + phase_s)) + 1);
    strncpy(pname, progname, pname_s + 1);
    if (phase) {
        strncat(pname, phase, phase_s);
    }
    // displayed archname
    if (!archname) {
		archname = getenv("STATCOUNTERS_ARCHNAME");
		if (!archname || archname[0] == '\0')
			archname = getarchname();
	}
    // dump file pointer
    bool display_header = true;
    bool use_stdout = false;
    FILE * fp = fileptr;
    if (!fp) {
        const char * const fname = getenv("STATCOUNTERS_OUTPUT");
        if (!fname || fname[0] == '\0') {
            use_stdout = true;
        } else {
            if (access(fname, F_OK) != -1) {
                display_header = false;
            }
            fp = fopen(fname, "a");
        }
        if (!fp && !use_stdout) {
            warn("Failed to open statcounters output %s", fname);
            use_stdout = true;
        }
    } else {
        use_stdout = false;
    }
    if (use_stdout)
        fp = stdout;
    // output format
    const char * const fmt = getenv("STATCOUNTERS_FORMAT");
    statcounters_fmt_flag_t fmt_flg = format_flag;
    if (fmt && (strcmp(fmt,"csv") == 0)) {
       if (display_header)
           fmt_flg = CSV_HEADER;
       else
           fmt_flg = CSV_NOHEADER;
    }

    if (b == NULL || fp == NULL)
        return -1;
    switch (fmt_flg)
    {
        case CSV_HEADER:
            fprintf(fp, "progname,");
            fprintf(fp, "archname,");
            fprintf(fp, "user_time,");
            fprintf(fp, "sys_time,");
            fprintf(fp, "max_resident_set_size,");
            fprintf(fp, "integral_shared_memory_size,");
            fprintf(fp, "integral_unshared_data,");
            fprintf(fp, "integral_unshared_stack,");
            fprintf(fp, "page_reclaims,");
            fprintf(fp, "page_faults,");
            fprintf(fp, "swaps,");
            fprintf(fp, "block_input,");
            fprintf(fp, "block_output,");
            fprintf(fp, "messages_sent,");
            fprintf(fp, "messages_received,");
            fprintf(fp, "signals_received,");
            fprintf(fp, "voluntary_context_switches,");
            fprintf(fp, "involuntary_context_switches");
            fprintf(fp, "\n");
            // fallthrough
        case CSV_NOHEADER:
            fprintf(fp, "%s,",pname);
            fprintf(fp, "%s,",archname);
            fprintf(fp, "%ld.%ld,",b->ru_utime.tv_sec,b->ru_utime.tv_usec);
            fprintf(fp, "%ld.%ld,",b->ru_stime.tv_sec,b->ru_stime.tv_usec);
            fprintf(fp, "%lu,",b->ru_maxrss);
            fprintf(fp, "%lu,",b->ru_ixrss);
            fprintf(fp, "%lu,",b->ru_idrss);
            fprintf(fp, "%lu,",b->ru_isrss);
            fprintf(fp, "%lu,",b->ru_minflt);
            fprintf(fp, "%lu",b->ru_majflt);
            fprintf(fp, "%lu,",b->ru_nswap);
            fprintf(fp, "%lu,",b->ru_inblock);
            fprintf(fp, "%lu,",b->ru_oublock);
            fprintf(fp, "%lu,",b->ru_msgsnd);
            fprintf(fp, "%lu,",b->ru_msgrcv);
            fprintf(fp, "%lu",b->ru_nsignals);
            fprintf(fp, "%lu,",b->ru_nvcsw);
            fprintf(fp, "%lu",b->ru_nivcsw);
            fprintf(fp, "\n");
            break;
        case HUMAN_READABLE:
        default:
            fprintf(fp, "===== %s -- %s =====\n",pname, archname);
            fprintf(fp, "user_time:                       \t%ld.%ld\n",b->ru_utime.tv_sec,b->ru_utime.tv_usec);
            fprintf(fp, "sys_time:                        \t%ld.%ld\n",b->ru_stime.tv_sec,b->ru_stime.tv_usec);
            fprintf(fp, "max_resident_set_size:           \t%lu\n",b->ru_maxrss);
            fprintf(fp, "integral_shared_memory_size:     \t%lu\n",b->ru_ixrss);
            fprintf(fp, "integral_unshared_data:          \t%lu\n",b->ru_idrss);
            fprintf(fp, "integral_unshared_stack:         \t%lu\n",b->ru_isrss);
            fprintf(fp, "page_reclaims:                   \t%lu\n",b->ru_minflt);
            fprintf(fp, "page_faults:                     \t%lu\n",b->ru_majflt);
            fprintf(fp, "swaps:                           \t%lu\n",b->ru_nswap);
            fprintf(fp, "block_input:                     \t%lu\n",b->ru_inblock);
            fprintf(fp, "block_output:                    \t%lu\n",b->ru_oublock);
            fprintf(fp, "messages_sent:                   \t%lu\n",b->ru_msgsnd);
            fprintf(fp, "messages_received:               \t%lu\n",b->ru_msgrcv);
            fprintf(fp, "signals_received:                \t%lu\n",b->ru_nsignals);
            fprintf(fp, "voluntary_context_switches:      \t%lu\n",b->ru_nvcsw);
            fprintf(fp, "involuntary_context_switches:    \t%lu\n",b->ru_nivcsw);
            fprintf(fp, "\n");
            break;
    }
    pname = cheri_setoffset(pname,0);
	free(pname);
    //if (!use_stdout)
    //    fclose(fp);
    return 0;
}

static void
set_priority(void)
{
	struct sched_param sched_params;
	struct rtprio rt_params;
	int error;

	rt_params.type = RTP_PRIO_FIFO;
	rt_params.prio = RTP_PRIO_MIN;
	error = rtprio_thread(RTP_SET, recver_lwpid, &rt_params);
	assert(error == 0);
	error = rtprio_thread(RTP_SET, sender_lwpid, &rt_params);
	assert(error == 0);
}

static void
unset_priority(void)
{
	struct sched_param sched_params;
	struct rtprio rt_params;
	int error;

	rt_params.type = RTP_PRIO_NORMAL;
	rt_params.prio = RTP_PRIO_MIN;
	error = rtprio_thread(RTP_SET, 0, &rt_params);
	assert(error == 0);
}

static void
spawn_ukernel(pid_t my_pid)
{
	int error;
	int retries;
	void *coproc_init_scb;

	error = colookup(U_COPROC_INIT, &coproc_init_scb);
    if (error != 0) {
	    coprocd_pid = vfork();
	    if (coprocd_pid == 0){
	    	error = coexecve(my_pid, coprocd_args[0], coprocd_args, environ);
	    	_exit(EX_SOFTWARE);
	    }

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
	pid_t child_pid, pid;
	void *capv[4];
	char *child_args[5];
	char *fmt_arg;

#ifdef BMARK_PROC
	recver_pid = getpid();
	spawn_ukernel(recver_pid);
	//open coports and pass to child process (sender)
	copipe = open_coport(COPIPE);
	cocarrier = open_coport(COCARRIER);
	cochannel = open_coport(COCHANNEL);

	if (copipe == NULL 
		|| cocarrier == NULL 
		|| cochannel == NULL) {
		err(errno, "%s: coopen failed", __func__);
	} else {
		capv[0] = copipe;
		capv[1] = cocarrier;
		capv[2] = cochannel;
		capv[3] = NULL;
	}
	
	child_args[0] = strdup(child_path);
	child_args[1] = strdup("-s");

	fmt_arg = calloc(32, sizeof(char));
	sprintf(fmt_arg, "-b %lu", message_len);
	child_args[2] = strdup(fmt_arg);
	memset(fmt_arg, '\0', __builtin_cheri_length_get(fmt_arg));
	sprintf(fmt_arg, "-i %lu", iterations);
	child_args[3] = strdup(fmt_arg);
	free(fmt_arg);

	child_args[4] = NULL;


	//spawn sender process
	child_pid = rfork(RFSPAWN);
	if (child_pid == 0) {
        error = coexecvec(recver_pid, child_args[0], child_args, environ, capv);
        _exit(EX_UNAVAILABLE); /* Should not reach unless error */
    } else if (child_pid == -1) {
        err(EX_SOFTWARE, "%s: rfork failed", __func__);
    }

    /* check that the sender actually started */
    pid = waitpid(child_pid, &status, (WNOHANG | WEXITED));
    if (pid > 0) {
        assert(WIFEXITED(status));
        errno = ECHILD;
        err(EX_SOFTWARE, "sender exited earlier than expected");
    } else if (pid == -1)
        err(EX_SOFTWARE, "sender failed to start as expected");
    sender_pid = child_pid;
#endif
    //allocate message buffer
    buffer = calloc(1, message_len);

    //initialize results list
    SLIST_INIT(&results_list);
}

static void
do_warmup_recv(void)
{
	int error;
	int retries, max_retries;
	long *lwpid_ptr;


	//warmup run to synchronise sender/recver and warm up caches, touch memory, etc
	error = corecv(copipe, (void **)&buffer, message_len);
	if (error < 0)
		err(EX_SOFTWARE, "%s: error occurred in copipe corecv", __func__);
	//warmup run to synchronise sender/recver and warm up caches, touch memory, etc
	max_retries = 10;
	retries = 0;
	for (retries = 0; retries < max_retries; retries++) {
		error = corecv(cocarrier, (void **)&cocarrier_buf, message_len);
		if (error < 0) {
			if (errno == EAGAIN) {
				if (retries < max_retries){
					sleep(1);
				}
			} else
				err(EX_SOFTWARE, "%s: error occurred in cocarrier corecv", __func__);
		} else
			break;
	}
	//warmup run to synchronise sender/recver and warm up caches, touch memory, etc
	for (retries = 0; retries < max_retries; retries++) {
		error = corecv(cochannel, (void **)&buffer, MIN(message_len, 4096));
		if (error < 0) {
			if (errno == EAGAIN) {
				if (retries < max_retries){
					sleep(1);
				}
			} else
				err(EX_SOFTWARE, "%s: error occurred in cochannel corecv", __func__);
		} else
			break;
	}
	memset(buffer, '\0', message_len);
	lwpid_ptr = &sender_lwpid;
	error = corecv(copipe, (void **)&lwpid_ptr, sizeof(sender_lwpid));
	assert(error > 0);
}

static void
do_send_setup(void)
{
	size_t required;
	ssize_t status;
	int fd, error;
	void *coproc_init_scb;

	buffer = calloc(1, message_len);
	required = __builtin_cheri_length_get(buffer);
	fd = open("/dev/random", O_RDONLY);
	status = read(fd, buffer, required);

	
#ifdef BMARK_PROC
	error = colookup(U_COPROC_INIT, &coproc_init_scb);
    if (error != 0)
	    err(EX_SOFTWARE, "%s: microkernel is not running", __func__);
    set_ukern_target(COCALL_COPROC_INIT, coproc_init_scb);

	process_capvec();
#else
	while (cochannel == NULL)
		sleep(1);
#endif
}

static void
do_warmup_send(void)
{
	int error;

	//warmup run to synchronise sender/recver and warm up caches, touch memory, etc
	error = cosend(copipe, buffer, message_len);
	if (error < 0)
		err(EX_SOFTWARE, "%s: error occurred in copipe cosend", __func__);
	//warmup run to synchronise sender/recver and warm up caches, touch memory, etc
	error = cosend(cocarrier, buffer, message_len);
	if (error < 0)
		err(EX_SOFTWARE, "%s: error occurred in cocarrier cosend", __func__);
	//warmup run to synchronise sender/recver and warm up caches, touch memory, etc
	error = cosend(cochannel, buffer, MIN(message_len, 4096));
	if (error < 0)
		err(EX_SOFTWARE, "%s: error occurred in cochannel cosend", __func__);
	error = cosend(copipe, &sender_lwpid, sizeof(sender_lwpid));
	assert(error > 0);
}

static struct benchmark_result * 
do_benchmark_send(void *buffer, size_t buffer_length, coport_t *port)
{
	struct benchmark_result *result;
	statcounters_bank_t start_bank, end_bank;
	struct rusage start_rusage, end_rusage;
	struct timespec start_ts, end_ts;
	int status;
	int intval;
	coport_type_t coport_type;

	coport_type = coport_gettype(port);
	if (coport_type == COCHANNEL)
		buffer_length = MIN(4096, buffer_length);
	
	if (!aggregate_mode) {
		//statcounters
		statcounters_zero(&start_bank);
		statcounters_zero(&end_bank);
		//get initial values for counters, for later comparison
		getrusage(RUSAGE_SELF, &start_rusage);
		clock_gettime(CLOCK_REALTIME_PRECISE, &start_ts);

		/*if (coport_type == COCHANNEL) {
			intval = 1;
			//QEMU_SET_TRACE_BUFFERED_MODE;
			sysarch(QEMU_SET_QTRACE_USER, &intval);
			CHERI_START_USER_TRACE;
		}*/

		statcounters_sample(&start_bank);
	}
	//perform operation
	status = cosend(port, buffer, buffer_length);
	
	if (!aggregate_mode) {
		//get new values for counters
		//end_bank.instructions = statcounters_sample_by_id(instruction_count_id);
		statcounters_sample(&end_bank);
		/*if (coport_type == COCHANNEL) {
			//QEMU_FLUSH_TRACE_BUFFER;
			CHERI_STOP_TRACE;
			CHERI_STOP_USER_TRACE;
			intval = 0;
			sysarch(QEMU_SET_QTRACE_USER, &intval);
		}*/
		clock_gettime(CLOCK_REALTIME_PRECISE, &end_ts);
		getrusage(RUSAGE_SELF, &end_rusage);
	}

	if (status < 0) {
		if (errno == EWOULDBLOCK || errno == EMSGSIZE || errno == EOPNOTSUPP)
			return (NULL);
		else
			err(EX_SOFTWARE, "%s: error occurred in cosend for coport type %d", __func__, coport_type);
	} 

	if (aggregate_mode)
		return (void *)(-1);
	//calculate differences in counters
	result = calloc(1, sizeof(struct benchmark_result));
	clock_diff(&result->timespec_diff, &end_ts, &start_ts);
	rusage_diff(&result->rusage_diff, &end_rusage, &start_rusage);
	statcounters_diff(&result->statcounter_diff, &end_bank, &start_bank);
	result->op = OP_COSEND;
	result->coport_type = coport_type;
	result->buf_len = buffer_length;
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
	coport_type_t coport_type;

	coport_type = coport_gettype(port);
	if (coport_type == COCHANNEL)
		buffer_length = MIN(4096, buffer_length);
	//statcounters
	statcounters_zero(&start_bank);
	statcounters_zero(&end_bank);

	//get initial values for counters, for later comparison
	if (!aggregate_mode) {
		getrusage(RUSAGE_SELF, &start_rusage);
		clock_gettime(CLOCK_REALTIME_PRECISE, &start_ts);
		statcounters_sample(&start_bank);
	}

	//perform operation
	status = corecv(port, (void **)buffer, buffer_length);

	//get new values for counters
	if (!aggregate_mode) {
		statcounters_sample(&end_bank);
		clock_gettime(CLOCK_REALTIME_PRECISE, &end_ts);
		getrusage(RUSAGE_SELF, &end_rusage);
	}

	if (status < 0) {
		if (errno == EAGAIN && coport_type != COPIPE)
			return (NULL); //message not available
		err(EX_SOFTWARE, "error occurred in corecv for coport type %d", coport_type);
	}
	
	if (coport_type == COCARRIER)
		coport_msg_free(port, cocarrier_buf);

	if (aggregate_mode)
		return (void *)(-1);
	//calculate differences in counters
	result = calloc(1, sizeof(struct benchmark_result));
	clock_diff(&result->timespec_diff, &end_ts, &start_ts);
	rusage_diff(&result->rusage_diff, &end_rusage, &start_rusage);
	statcounters_diff(&result->statcounter_diff, &end_bank, &start_bank);
	result->op = OP_CORECV;
	result->buf_len = buffer_length;
	result->coport_type = coport_type;

	//output
	return (result);
}

static void
do_benchmark_run(coport_op op_mode, coport_t *port)
{
	struct benchmark_result *result;

	for (;;) {
		if (op_mode == OP_COSEND) {
			result = do_benchmark_send(buffer, message_len, port);
		} else {
			result = do_benchmark_recv((void **)&buffer, message_len, port);
		}
		if (result == NULL) {
			sleep(1);
		} else
			break;
	} 
	if (aggregate_mode)
		return;
	pthread_mutex_lock(&results_mtx);
	SLIST_INSERT_HEAD(&results_list, result, entries);
	pthread_mutex_unlock(&results_mtx);
}

static inline void
aggregate_sample_start(struct benchmark_result *result, coport_op op_mode, coport_type_t type)
{
	statcounters_bank_t dummy;

	result->op = op_mode;
	result->buf_len = message_len * iterations;
	result->coport_type = type;
	
	statcounters_sample(&dummy); //warmup sampling

	if (op_mode == OP_COSEND)
		pthread_mutex_lock(&results_mtx);
	else
		sleep(1);

	statcounters_sample(&result->statcounter_diff);
}

static inline void
aggregate_sample_end(struct benchmark_result *result, coport_op op_mode, coport_type_t type)
{
	statcounters_bank_t end_bank;

	statcounters_sample(&end_bank);

	statcounters_diff(&result->statcounter_diff, &end_bank, &result->statcounter_diff);
	if (op_mode != OP_COSEND) //already held by sender
		pthread_mutex_lock(&results_mtx);
	SLIST_INSERT_HEAD(&results_list, result, entries);
	pthread_mutex_unlock(&results_mtx);
	
}

static void
do_benchmark(coport_op op_mode)
{
	struct benchmark_result *result;
	struct timespec ts;
	coport_t *port;
	ssize_t i, x;

	if (op_mode == OP_COSEND)
		do_warmup_send();
	else
		do_warmup_recv();

	if (op_mode == OP_CORECV)
		set_priority();
	else
		sleep(1);

	if (aggregate_mode){
		result = calloc(1, sizeof(struct benchmark_result));
		aggregate_sample_start(result, op_mode, COPIPE);
	}
	for (i = 0; i < iterations; i++) {
		if (op_mode == OP_COSEND) {
			ts.tv_nsec = 1;
			ts.tv_sec = 0;
			nanosleep(&ts, NULL);
		}
		do_benchmark_run(op_mode, copipe);
	}
	if (aggregate_mode)
		aggregate_sample_end(result, op_mode, COPIPE);
	unset_priority();

	if (aggregate_mode){
		result = calloc(1, sizeof(struct benchmark_result));
		aggregate_sample_start(result, op_mode, COCARRIER);
	}
	for (i = 0; i < iterations; i++) 
		do_benchmark_run(op_mode, cocarrier);
	if (aggregate_mode)
		aggregate_sample_end(result, op_mode, COCARRIER);

	if (message_len > 4096)
		return;

	x = ((4096 + message_len - 1) / message_len);
	if (aggregate_mode){
		result = calloc(1, sizeof(struct benchmark_result));
		aggregate_sample_start(result, op_mode, COCHANNEL);
	}
	for (i = 0; i < iterations; i++) {
		do_benchmark_run(op_mode, cochannel);
		if (!aggregate_mode && op_mode == OP_COSEND){
			if (((i % x) == (x - 1)) && (i != 0))
				sleep(1);
		}

	}
	if (aggregate_mode)
		aggregate_sample_end(result, op_mode, COCHANNEL);
		
}
	

static void
process_results(void)
{
	FILE *outfile;
	char *filename;
	char *progname;
	char *phase;
	char *bandwidth;
	struct benchmark_result *run;
	float printable_bw;
	ssize_t buf_len;
	pid_t pid;
	int status;

	bool started[2][3] = {{false, false, false}, {false, false, false}};
	int idx_a, idx_b;

	SLIST_FOREACH(run, &results_list, entries) {
		switch (run->op) {
		case OP_COSEND:
			progname = strdup("COSEND");
			idx_a = 0;
			break;
		case OP_CORECV:
			progname = strdup("CORECV");
			idx_a = 1;
			break;
		default:
			progname = strdup("ERROR - invalid op");
			break;
		}
		switch (run->coport_type) {
		case COPIPE:
			phase = strdup("COPIPE");
			idx_b = 0;
			break;
		case COCARRIER:
			phase = strdup("COCARRIER");
			idx_b = 1;
			break;
		case COCHANNEL:
			phase = strdup("COCHANNEL");
			idx_b = 2;
			break;
		default:
			phase = strdup("UNKNOWN/UNSPECIFIED COPORT TYPE");
			break;
		}
		printable_bw = ((float)run->buf_len / 1024.0) / (((float) run->timespec_diff.tv_sec + (float)run->timespec_diff.tv_nsec) / 1000000000);
		printf("%s -- %s: %.2FKB/s\n", progname, phase, printable_bw);
		if (format != HUMAN_READABLE) {
			filename = calloc(sizeof(char), 255);
			bandwidth = calloc(sizeof(char), 255);
			sprintf(filename, "/tmp/%s_b%lu.csv", phase, message_len);
			sprintf(bandwidth, "%.2FKB/s", printable_bw);
			outfile = fopen(filename, "a+");
			if (started[idx_a][idx_b])
				statcounters_dump_with_args(&run->statcounter_diff, progname, phase, bandwidth, outfile, CSV_NOHEADER);
			else
				statcounters_dump_with_args(&run->statcounter_diff, progname, phase, bandwidth, outfile, CSV_HEADER);
			started[idx_a][idx_b] = true;
		} else {
			statcounters_dump_with_args(&run->statcounter_diff, progname, phase, NULL, NULL, format);
			rusage_dump_with_args(&run->rusage_diff, progname, phase, NULL, NULL, format);
		}

	}
}

static void *
thread_main(void *args)
{
	struct thr_arg *argp;
	argp = args;
	coport_op op_mode = argp->thr_mode;

	sleep(2);

	if (op_mode == OP_CORECV) {
		thr_self(&recver_lwpid);
		do_recv_setup();
	} else {
		thr_self(&sender_lwpid);
		do_send_setup();
	}
    do_benchmark(op_mode);

    if (op_mode == OP_CORECV) {
    	pthread_mutex_lock(&results_mtx);
    	process_results();
    	pthread_mutex_unlock(&results_mtx);

    }

	return (args);
}

static cpuset_t recv_cpu_set = CPUSET_T_INITIALIZER(CPUSET_FSET);
static cpuset_t send_cpu_set = CPUSET_T_INITIALIZER(CPUSET_FSET);

static void
run_in_thread(void)
{
	pthread_attr_t thr_attr, thr_attr2;
	pthread_t thr, thr2;
	struct sched_param sched_params;
	struct thr_arg args, args2;
	pthread_mutexattr_t mtxattr;
	int error;

	pthread_mutexattr_init(&mtxattr);
	pthread_mutex_init(&results_mtx, &mtxattr);
	pthread_attr_init(&thr_attr);
	pthread_attr_init(&thr_attr2);

	/*error = cpuset_getaffinity(CPU_LEVEL_CPUSET, CPU_WHICH_PID, -1, sizeof(cpuset_t), &send_cpu_set);
	recv_cpu_set = send_cpu_set;	
	CPU_CLR(CPU_FFS(&send_cpu_set)-1, &send_cpu_set);
	CPU_CLR(CPU_FFS(&send_cpu_set)-1, &recv_cpu_set);

	pthread_attr_setaffinity_np(&thr_attr, sizeof(cpuset_t), &recv_cpu_set);
	pthread_attr_setaffinity_np(&thr_attr2, sizeof(cpuset_t), &send_cpu_set);*/

	sched_params.sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_attr_setschedpolicy(&thr_attr, SCHED_FIFO);
	pthread_attr_setschedparam(&thr_attr, &sched_params);
	pthread_attr_setinheritsched(&thr_attr, PTHREAD_EXPLICIT_SCHED);

	pthread_attr_setschedpolicy(&thr_attr2, SCHED_FIFO);
	pthread_attr_setschedparam(&thr_attr2, &sched_params);
	pthread_attr_setinheritsched(&thr_attr2, PTHREAD_EXPLICIT_SCHED);

	args.thr_mode = OP_CORECV;
	pthread_create(&thr, &thr_attr, thread_main, &args);
	args2.thr_mode = OP_COSEND;
	pthread_create(&thr2, &thr_attr2, thread_main, &args2);

	pthread_join(thr2, NULL);
	pthread_join(thr, NULL);
}

int main(int argc, char *const argv[])
{
	int opt, error;
	char *strptr;

	while((opt = getopt(argc, argv, "sb:i:ah")) != -1) {
		switch (opt) {
		case 'h':
			format = HUMAN_READABLE;
			break;
		case 'a':
			aggregate_mode = 1;
			break;
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
			break;
		case '?':
		default: 
			err(EX_USAGE, "invalid flag '%c'", (char)optopt);
			break;
		}
	}
#ifndef BMARK_PROC
	recver_pid = getpid();

	spawn_ukernel(recver_pid);
	copipe = open_coport(COPIPE);
	cocarrier = open_coport(COCARRIER);
	cochannel = open_coport(COCHANNEL);
#endif
	run_in_thread();

	kill(-coprocd_pid, SIGKILL);
	return (0);
}