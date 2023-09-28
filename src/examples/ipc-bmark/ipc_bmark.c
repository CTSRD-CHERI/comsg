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

#include <assert.h>
#include <err.h>
#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <machine/sysarch.h>
#define CPU_QEMU_RISCV
#include <machine/cpufunc.h>
#include <stdbool.h>
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
#include <sys/resource.h>
#include <sys/rtprio.h>
#include <sys/thr.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <openssl/sha.h>
#include <sys/mman.h>
#include <poll.h>
#include <sys/sysctl.h>

typedef enum {OP_INVALID = 0, OP_SEND = 1, OP_RECV = 2} ipc_op;
typedef enum {IPC_INVALID, IPC_SOCKET, IPC_PIPE} ipc_type;


struct msg_checksum {
	bool is_sha;
	union {
		size_t sum;
		unsigned char sha_sum[SHA256_DIGEST_LENGTH + 1];
	};
	char *buf;
};
struct benchmark_result {
	ipc_type ipc_type;
	ipc_op op;
	ssize_t buf_len;
	statcounters_bank_t statcounter_diff;
	struct rusage rusage_diff;
	struct timespec timespec_diff;
	SLIST_ENTRY(benchmark_result) entries;
	struct msg_checksum sum;
};

static pthread_t rcvr_thr, sndr_thr;
static pthread_mutex_t results_mtx;
static pthread_cond_t results_cnd;
static SLIST_HEAD(, benchmark_result) results_list = SLIST_HEAD_INITIALIZER(benchmark_result);
static bool pipe_enabled = false;
static bool socket_enabled = false;
struct thr_arg {
	ipc_op thr_mode;
};

extern char **environ;
extern char *optarg;

static pid_t sender_pid = -1;
static pid_t recver_pid = -1;
static long sender_lwpid = -1;
static long recver_lwpid = -1;
static bool enable_sha256_workload = true;
static bool enable_dummy_workload = false;

static int aggregate_mode = 0;
static _Thread_local ipc_op mode = OP_RECV;

static int recv_pipe = -1;
static int send_pipe = -1;
static int recv_sock = -1;
static int send_sock = -1;

static _Thread_local char *buffer = NULL;
static const ssize_t maxlen = 1024 * 1024 * 2; //128MiB because not sure how much ram we have, and must account  multiple copies
static ssize_t message_len = maxlen;
static ssize_t iterations = 1;

static uint64_t instruction_count_id = 0;
static statcounters_fmt_flag_t format = CSV_HEADER; 
static _Thread_local SHA256_CTX *sha_ctx;
static size_t sock_op_len_send = 2048;
static size_t sock_op_len_recv = 2048;


#ifdef BMARK_PROC
#undef BMARK_PROC
#endif

static void
get_instruction_count_id(void)
{
	instruction_count_id = statcounters_id_from_name("instructions");
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
            fprintf(fp, "%lu,",b->ru_majflt);
            fprintf(fp, "%lu,",b->ru_nswap);
            fprintf(fp, "%lu,",b->ru_inblock);
            fprintf(fp, "%lu,",b->ru_oublock);
            fprintf(fp, "%lu,",b->ru_msgsnd);
            fprintf(fp, "%lu,",b->ru_msgrcv);
            fprintf(fp, "%lu,",b->ru_nsignals);
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
    pname = __builtin_cheri_offset_set(pname,0);
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
wait_for_fd(int fd, bool read)
{
	int status;
	struct pollfd fds[1];

	fds[0].fd = fd;
	if (read)
		fds[0].events = POLLIN;
	else
		fds[0].events = POLLOUT;
	status = ppoll(fds, 1, NULL, NULL);
}


static void
do_recv_setup(void)
{
	int pipes[2];
	int socks[2];
	int error;
	int sockoptval;

    //allocate message buffer
    buffer = calloc(1, message_len);
	buffer = cheri_andperm(buffer, CHERI_PERM_STORE | CHERI_PERM_LOAD | CHERI_PERM_GLOBAL);
	buffer = cheri_setbounds(buffer, message_len);
	mlock(buffer, __builtin_cheri_length_get(buffer));
    //initialize results list
    SLIST_INIT(&results_list);

    //init ipc channels
	error = pipe2(pipes, O_CLOEXEC);
	if (error)
		err(errno, "%s: failed to open pipes", __func__);
	recv_pipe = pipes[0];
	send_pipe = pipes[1];

	error = socketpair(PF_LOCAL, SOCK_SEQPACKET | SOCK_NONBLOCK, 0, socks);
	if (error)
		err(errno, "%s: failed to open sockets", __func__);

	sockoptval = (message_len + 32) * 4;
	if (setsockopt(socks[0], SOL_SOCKET, SO_RCVBUF,
	    &sockoptval, sizeof(sockoptval)) < 0)
		err(EX_OSERR, "%s: setsockopt SO_RCVBUF failed", __func__);
	sockoptval = (message_len + 32) * 4;
	if (setsockopt(socks[1], SOL_SOCKET, SO_SNDBUF,
	    &sockoptval, sizeof(sockoptval)) < 0)
		err(EX_OSERR, "%s: setsockopt SO_SNDBUF failed", __func__);

	fflush(stdout);
	fflush(stderr);

	(void)sync();
	(void)sync();
	(void)sync();

	recv_sock = socks[0];
	send_sock = socks[1];
}

static void
do_warmup_recv(void)
{
	int error;
	long bytes_read;
	int retries, max_retries;
	long lwpid;

	//warmup run to synchronise sender/recver and warm up caches, touch memory, etc
	bytes_read = 0;
	while (bytes_read < __builtin_cheri_length_get(buffer)) {
		const size_t offset = bytes_read % __builtin_cheri_length_get(buffer);
		const size_t read_op_len = __builtin_cheri_length_get(buffer) - bytes_read;
		error = read(recv_pipe, buffer + offset, read_op_len);
		if (error < 0)
			err(EX_SOFTWARE, "%s: error occurred in pipe read", __func__);
		bytes_read += error;
	}
	wait_for_fd(recv_sock, true);
	bytes_read = 0;
	while (bytes_read < __builtin_cheri_length_get(buffer)) {
		const size_t offset = bytes_read % __builtin_cheri_length_get(buffer);
		const size_t read_op_len = 
			__builtin_cheri_length_get(buffer) - bytes_read < sock_op_len_recv ? 
			__builtin_cheri_length_get(buffer) - bytes_read 
			: sock_op_len_recv;
		error = recv(recv_sock, buffer + offset, read_op_len, MSG_DONTWAIT);
		if (error < 0) {
			if (errno != EAGAIN)
				err(EX_SOFTWARE, "%s: error occurred in socket read", __func__);
			else
				continue;
		}
		bytes_read += error;
	}

	pthread_cond_wait(&results_cnd, &results_mtx); //mutex held from do_benchmark
	memset(buffer, '\0', __builtin_cheri_length_get(buffer));
	error = read(recv_pipe, &lwpid, sizeof(lwpid));
	if (error < 0)
		err(EX_SOFTWARE, "%s: error getting lwpid of sender via pipe", __func__);
	if (lwpid != sender_lwpid) {
		errno = EDOOFUS;
		err(EX_SOFTWARE, "%s: recv and send threads out of sync on pipe writes", __func__);
	}
}

static void
get_message_text(char *buf, size_t len)
{
	int fd;
	ssize_t status;

	fd = open("/dev/random", O_RDONLY);
	status = read(fd, buf, len);
	close(fd);
}

static void
do_send_setup(void)
{
	size_t required;
	ssize_t status;
	int fd, error;
	void *coproc_init_scb;

	buffer = calloc(1, message_len);
	buffer = cheri_andperm(buffer, CHERI_PERM_STORE | CHERI_PERM_LOAD | CHERI_PERM_GLOBAL);
	buffer = cheri_setbounds(buffer, message_len);

	required = __builtin_cheri_length_get(buffer);
	get_message_text(buffer, required);
	mlock(buffer, required);

	while (send_sock == -1)
		sleep(1);
}

static void
do_warmup_send(void)
{
	int error;
	long lwpid;
	long bytes_written;

	bytes_written = 0;
	while (bytes_written < __builtin_cheri_length_get(buffer)) {
		const size_t offset = bytes_written % __builtin_cheri_length_get(buffer);
		const size_t write_op_len = __builtin_cheri_length_get(buffer) - bytes_written;
		error = write(send_pipe, buffer + offset, write_op_len);
		if (error < 0)
			err(EX_SOFTWARE, "%s: error occurred in pipe write", __func__);
		bytes_written += error;
	}
	
	bytes_written = 0;
	while (bytes_written < __builtin_cheri_length_get(buffer)) {
		const size_t offset = bytes_written % __builtin_cheri_length_get(buffer);
		const size_t write_op_len = 
			(__builtin_cheri_length_get(buffer) - bytes_written) < sock_op_len_send 
			? (__builtin_cheri_length_get(buffer) - bytes_written) 
			: sock_op_len_send;
		error = send(send_sock, buffer + offset, write_op_len, MSG_DONTWAIT);
		if (error < 0) {
			if (errno != EAGAIN)
				err(EX_SOFTWARE, "%s: error occurred in socket write", __func__);
			else
				continue;
		}
		bytes_written += error;
	}

	pthread_mutex_lock(&results_mtx);
	thr_self(&lwpid);
	error = write(send_pipe, &lwpid, sizeof(lwpid));
	if (error < 0)
		err(EX_SOFTWARE, "%s: error writing lwpid of sender to pipe", __func__);
	assert(error == sizeof(lwpid));
	pthread_cond_signal(&results_cnd);
	pthread_mutex_unlock(&results_mtx);
}

static inline struct msg_checksum
do_dummy_workload(char *buf, size_t length)
{
	struct msg_checksum result;

	if ((result.is_sha = enable_sha256_workload)) {
		SHA256_Update(sha_ctx, buf, length);
		SHA256_Final(result.sha_sum, sha_ctx);
		result.sha_sum[SHA256_DIGEST_LENGTH] = '\0';
	} else {
		size_t total = 0;
		size_t i = 0;
		for(;length - i > sizeof(size_t); i += sizeof(size_t)) {
			size_t *long_buf = (size_t *)&buf[i];
			total += *long_buf;
		}
		for(;length > i; i++) {
			total += (size_t)buf[i];
		}
		
		
		result.sum = total;
	}
	return result;
}

static inline struct msg_checksum
dummy_workload(char *buf, ssize_t length)
{
    if (!enable_dummy_workload || length <= 0) {
		struct msg_checksum result;
		result.is_sha = false;
		result.sum = 0;
        return (result);
	} else
		return (do_dummy_workload(buf, (size_t)length));
}

static struct benchmark_result *shared_result = NULL;

static int
do_benchmark_send(void *msg, size_t buffer_length, ipc_type type)
{
	statcounters_bank_t start_bank, end_bank;
	struct rusage start_rusage, end_rusage;
	struct timespec start_ts, end_ts;
	size_t status;
	int intval;
	int fd;
	long bytes_written;
	struct msg_checksum b;
	struct benchmark_result *result;
	int error;

	get_message_text(msg, buffer_length);
	if (type == IPC_PIPE) {
		fd = send_pipe;
		wait_for_fd(fd, false);
	} else if (type == IPC_SOCKET)
		fd = send_sock;
	else
		err(EX_SOFTWARE, "%s: invalid ipc type %d", __func__, type);

	if (!aggregate_mode) {
		error = pthread_mutex_lock(&results_mtx);
		if (error != 0 && error != EDEADLK)
			err(EX_SOFTWARE, "%s: failed to lock results mutex", __func__);
		result = calloc(1, sizeof(struct benchmark_result));
		result->ipc_type = type;
		//statcounters
		statcounters_zero(&start_bank);
		statcounters_zero(&end_bank);
		//get initial values for counters, for later comparison
		getrusage(RUSAGE_SELF, &start_rusage);
		clock_gettime(CLOCK_REALTIME_PRECISE, &start_ts);
		statcounters_sample(&start_bank);
	}

	b = dummy_workload(msg, buffer_length);
	//perform operation
	status = 0;
	bytes_written = 0;
	while (bytes_written < buffer_length) {
		const size_t offset = bytes_written % buffer_length;
		const size_t write_op_len = 
			(buffer_length - bytes_written) < sock_op_len_send || type == IPC_PIPE 
			? (buffer_length - bytes_written) 
			: sock_op_len_send;
		if (type == IPC_PIPE)
			status = write(fd, msg + offset, write_op_len);
		else if (type == IPC_SOCKET)
			status = send(fd, msg + offset, write_op_len, MSG_DONTWAIT);
		if (status < 0) {
			if (errno != EAGAIN)
				err(EX_SOFTWARE, "%s: error occurred in ipc write", __func__);
			else {
				return (status);
			}
		}
		bytes_written += status;
	}

	if (status < 0) {
		err(EX_SOFTWARE, "%s: error occurred in send for ipc type %d", __func__, type);
	} 

	if (!aggregate_mode && type == IPC_SOCKET) {
		statcounters_sample(&result->statcounter_diff);
		clock_gettime(CLOCK_REALTIME_PRECISE, &result->timespec_diff);
		getrusage(RUSAGE_SELF, &result->rusage_diff);
	} else if (aggregate_mode)
		return (0);

	if (!enable_dummy_workload && type != IPC_PIPE) {
		result->sum = do_dummy_workload(msg, bytes_written);
	} else {
		result->sum = b;
	}

	if (type == IPC_SOCKET) {
		statcounters_diff(&result->statcounter_diff, &result->statcounter_diff, &start_bank);
		rusage_diff(&result->rusage_diff, &result->rusage_diff, &start_rusage);
		clock_diff(&result->timespec_diff, &result->timespec_diff, &start_ts);
		result->op = OP_SEND;
		result->buf_len = bytes_written;
		SLIST_INSERT_HEAD(&results_list, result, entries);
	} else if (type == IPC_PIPE) {
		//calculate differences in counters
		result->statcounter_diff = start_bank;
		result->timespec_diff = start_ts;
		result->rusage_diff = start_rusage;
		shared_result = result;
	}

	pthread_cond_wait(&results_cnd, &results_mtx);
	pthread_mutex_unlock(&results_mtx);

	//output
	return (0);
}

static int 
do_benchmark_recv(void *msg, size_t msg_size, ipc_type type)
{
	statcounters_bank_t end_bank;
	struct rusage end_rusage;
	struct timespec end_ts;
	ssize_t status;
	int fd;
	long bytes_read;
	struct msg_checksum a;
	struct benchmark_result *result;

	if (type == IPC_PIPE) {
		fd = recv_pipe;
	} else if (type == IPC_SOCKET) {
		fd = recv_sock; 
		wait_for_fd(fd, true);
	} else
		err(EX_SOFTWARE, "%s: invalid ipc type %d", __func__, type);
	//statcounters
	statcounters_zero(&end_bank);
	if (!aggregate_mode && type == IPC_SOCKET) {
		result = calloc(1, sizeof(struct benchmark_result));
		getrusage(RUSAGE_SELF, &result->rusage_diff);
		clock_gettime(CLOCK_REALTIME_PRECISE, &result->timespec_diff);
		statcounters_sample(&result->statcounter_diff);
	}

	status = 0;
	bytes_read = 0;
	//perform operation
	while (bytes_read < msg_size){
		const size_t offset = bytes_read % __builtin_cheri_length_get(buffer);
		const size_t read_op_len = 
			(msg_size - bytes_read) < sock_op_len_recv || type == IPC_PIPE 
			? msg_size - bytes_read 
			: sock_op_len_recv;
		if (type == IPC_PIPE)
			status = read(fd, msg + offset, read_op_len);
		else if (type == IPC_SOCKET)
			status = recv(fd, msg + offset, read_op_len, MSG_DONTWAIT);
		if (status < 0) {
			if (errno != EAGAIN)
				err(EX_SOFTWARE, "%s: error reading from socket/pipe", __func__);
			else {
				if (bytes_read == 0)
					return (status);
			}
		}
		bytes_read += status;
	}
	a = dummy_workload((char *)msg, bytes_read);

	//get metrics
	if (!aggregate_mode) {
		statcounters_sample(&end_bank);
		clock_gettime(CLOCK_REALTIME_PRECISE, &end_ts);
		getrusage(RUSAGE_SELF, &end_rusage);
	}

	if (aggregate_mode)
		return (0);
	//calculate delta in metrics and populate fields
	pthread_mutex_lock(&results_mtx);
	if (type == IPC_PIPE) {
		result = shared_result;
		shared_result = NULL;
	} else {
		result->ipc_type = IPC_SOCKET;
		result->op = OP_RECV;
	}
	if (!enable_dummy_workload && type != IPC_PIPE) {
		result->sum = do_dummy_workload(msg, bytes_read);
	} else {
		result->sum = a;
	}
	clock_diff(&result->timespec_diff, &end_ts, &result->timespec_diff);
	rusage_diff(&result->rusage_diff, &end_rusage, &result->rusage_diff);
	statcounters_diff(&result->statcounter_diff, &end_bank, &result->statcounter_diff);
	result->buf_len = bytes_read;
	
	//insert result
	SLIST_INSERT_HEAD(&results_list, result, entries);
	
	//wakeup writer thread
	pthread_cond_signal(&results_cnd);
	pthread_mutex_unlock(&results_mtx);

	return (0);
}

static void
do_benchmark_run(ipc_op op, ipc_type type)
{
	int error;

	for (;;) {
		if (op == OP_SEND) {
			error = do_benchmark_send(buffer, message_len, type);
		} else {
			error = do_benchmark_recv(buffer, message_len, type);
		}
		if (error < 0) {
			sleep(1);
		} else
			break;
	} 
}

static inline void
aggregate_sample_start(struct benchmark_result *result, ipc_op op_mode, ipc_type type)
{
	statcounters_bank_t dummy;

	result->op = op_mode;
	result->buf_len = message_len * iterations;
	result->ipc_type = type;
	
	statcounters_sample(&dummy); //warmup sampling

	if (op_mode == OP_SEND)
		pthread_mutex_lock(&results_mtx);
	else
		sleep(1);

	statcounters_sample(&result->statcounter_diff);
}

static inline void
aggregate_sample_end(struct benchmark_result *result, ipc_op op_mode, ipc_type type)
{
	statcounters_bank_t end_bank;

	statcounters_sample(&end_bank);

	statcounters_diff(&result->statcounter_diff, &end_bank, &result->statcounter_diff);
	if (op_mode != OP_SEND) //already held by sender
		pthread_mutex_lock(&results_mtx);
	SLIST_INSERT_HEAD(&results_list, result, entries);
	pthread_mutex_unlock(&results_mtx);
	
}

static void
clear_affinity(void)
{
	int error;
	cpuset_t cpu_setA = CPUSET_T_INITIALIZER(CPUSET_FSET);
	cpuset_t cpu_setB = CPUSET_T_INITIALIZER(CPUSET_FSET);

	long tid;
	thr_self(&tid);

	error = cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, -1, sizeof(cpuset_t), &cpu_setA);
	if (error != 0)
		err(EX_SOFTWARE, "%s: unable to get cpu affinity", __func__);
	if (CPU_COUNT(&cpu_setA) == 1)
		return;
	CPU_COPY(&cpu_setB, &cpu_setA);
	//clear all except 1
	while (CPU_COUNT(&cpu_setA) > 1) {
		CPU_CLR(CPU_FFS(&cpu_setA)-1, &cpu_setA);
	}
	//clear all except 1, but not the same 1 as the other
	CPU_CLR(CPU_FFS(&cpu_setA)-1, &cpu_setB);
	while (CPU_COUNT(&cpu_setB) > 1) {
		CPU_CLR(CPU_FFS(&cpu_setB)-1, &cpu_setB);
	}
	if (tid == recver_lwpid) {
		error = pthread_setaffinity_np(rcvr_thr, sizeof(cpuset_t), &cpu_setB);
		if (error != 0)
			err(EX_SOFTWARE, "%s: unable to reset cpu affinity", __func__);
	} else {
		error = pthread_setaffinity_np(sndr_thr, sizeof(cpuset_t), &cpu_setA);
		if (error != 0)
			err(EX_SOFTWARE, "%s: unable to reset cpu affinity", __func__);
	}
}

static void
do_benchmark(ipc_op op_mode)
{
	struct benchmark_result *aggregate_result;
	struct timespec ts;
	ssize_t i;

	if (op_mode == OP_SEND)
		do_warmup_send();
	else
		do_warmup_recv();

	if (op_mode == OP_RECV){
		set_priority();
		pthread_mutex_unlock(&results_mtx);
	} else
		sleep(1);

	if (pipe_enabled) {
		if (aggregate_mode){
			aggregate_result = calloc(1, sizeof(struct benchmark_result));
			aggregate_sample_start(aggregate_result, op_mode, IPC_PIPE);
		}
	
		for (i = 0; i < iterations; i++) {
			do_benchmark_run(op_mode, IPC_PIPE);
		}
	
		if (aggregate_mode)
			aggregate_sample_end(aggregate_result, op_mode, IPC_PIPE);
	}
	unset_priority();
	//clear_affinity();
	if (socket_enabled) {
		if (aggregate_mode){
			aggregate_result = calloc(1, sizeof(struct benchmark_result));
			aggregate_sample_start(aggregate_result, op_mode, IPC_SOCKET);
		}
		for (i = 0; i < iterations; i++) 
			do_benchmark_run(op_mode, IPC_SOCKET);
		if (aggregate_mode)
			aggregate_sample_end(aggregate_result, op_mode, IPC_SOCKET);
	}
		
}
	
static void
process_results(void)
{
	FILE *outfile;
	char *filename;
	char *rusage_filename;
	char *progname;
	char *phase;
	char *bandwidth;
	struct benchmark_result *run;
	float printable_bw;
	ssize_t buf_len;
	pid_t pid;
	int status;

	bool started[1][2] = {{false, false}};
	int idx_a, idx_b;

	SLIST_FOREACH(run, &results_list, entries) {
		idx_a = 0;
		switch (run->ipc_type) {
		case IPC_PIPE:
			progname = strdup("PIPE");
			phase = strdup("PIPE");
			idx_b = 0;
			break;
		case IPC_SOCKET:
			if (run->op == OP_SEND)
				progname = strdup("SEND");
			else
				progname = strdup("RECV");
			phase = strdup("SOCKET");
			idx_b = 1;
			break;
		default:
			progname = strdup("UNKNOWN/UNSPECIFIED IPC TYPE");
			phase = strdup("ERROR");
			break;
		}
		printable_bw = ((float)run->buf_len / 1024.0) / (((float) run->timespec_diff.tv_sec + (float)run->timespec_diff.tv_nsec) / 1000000000);
		printf("%s:%s: %.2FKB/s\n", phase, progname, printable_bw);
		
		
		if (format != HUMAN_READABLE) {
			filename = calloc(sizeof(char), 255);
			rusage_filename = calloc(sizeof(char), 255);
			bandwidth = calloc(sizeof(char), 255);

			sprintf(filename, "/tmp/%s_b%lu.csv", phase, message_len);
			sprintf(rusage_filename, "/tmp/%s_b%lu_rusage.csv", phase, message_len);
			sprintf(bandwidth, "%.2FKB/s", printable_bw);

			//if (phase != NULL)
				//free(phase); //todo: fix snmalloc buf with freeing strdup'd stuff
			phase = calloc(sizeof(char), (2 * SHA256_DIGEST_LENGTH) + 2);
			phase[0] = '-';
			if (run->sum.is_sha) {
				for (size_t i = 0; i < SHA256_DIGEST_LENGTH; i++) {
					sprintf(phase + 1 + (i * 2), "%02x", run->sum.sha_sum[i]);
				}
			} else {
				sprintf(phase + 1, "%lx", run->sum.sum);
			}

			bool incl_headers;
			if (!started[idx_a][idx_b]) {
				int s = open(filename, O_RDONLY | O_CREAT | O_EXCL);
				incl_headers = (s != -1);
				if (incl_headers)
					close(s);
			} else
				incl_headers = false;

			outfile = fopen(filename, "a+"); //closed by dump with args
			if (!incl_headers)
				statcounters_dump_with_args(&run->statcounter_diff, progname, phase, bandwidth, outfile, CSV_NOHEADER);
			else
				statcounters_dump_with_args(&run->statcounter_diff, progname, phase, bandwidth, outfile, CSV_HEADER);
				
			
			outfile = fopen(rusage_filename, "a+"); //closed by dump with args
			if (!incl_headers)
				rusage_dump_with_args(&run->rusage_diff, progname, phase, bandwidth, outfile, CSV_NOHEADER);
			else
				rusage_dump_with_args(&run->rusage_diff, progname, phase, bandwidth, outfile, CSV_HEADER);
			started[idx_a][idx_b] = true;
		} else {
			statcounters_dump_with_args(&run->statcounter_diff, progname, phase, NULL, NULL, format);
			rusage_dump_with_args(&run->rusage_diff, progname, phase, NULL, NULL, format);
		}

	}
	printf("%lu byte IPC benchmark done.\n", message_len);
}

static void *
thread_main(void *args)
{
	struct thr_arg *argp;
	argp = args;
	ipc_op op_mode = argp->thr_mode;

	sha_ctx = calloc(1, sizeof(SHA256_CTX));
	SHA256_Init(sha_ctx);
	if (op_mode == OP_RECV) {
		pthread_mutex_lock(&results_mtx);
		thr_self(&recver_lwpid);
		do_recv_setup();
	} else {
		while(&recver_lwpid < 0)
			sleep(1);
		thr_self(&sender_lwpid);
		do_send_setup();
	}

    do_benchmark(op_mode);
	free(buffer);

    if (op_mode == OP_RECV) {
    	pthread_mutex_lock(&results_mtx);
		
		close(send_pipe);
		close(recv_pipe);
		close(send_sock);
		close(recv_sock);
		
    	process_results();
    	pthread_mutex_unlock(&results_mtx);
    }

	return (args);
}

static cpuset_t pinned_cpu_set = CPUSET_T_INITIALIZER(CPUSET_FSET);

static void
run_in_thread(void)
{
	pthread_attr_t thr_attr, thr_attr2;
	
	struct sched_param sched_params;
	struct thr_arg args, args2;
	pthread_mutexattr_t mtxattr;
	pthread_condattr_t cndattr;
	int error;

	pthread_mutexattr_init(&mtxattr);
	pthread_mutexattr_settype(&mtxattr, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(&results_mtx, &mtxattr);

	pthread_condattr_init(&cndattr);
	pthread_cond_init(&results_cnd, &cndattr);

	pthread_attr_init(&thr_attr);
	pthread_attr_init(&thr_attr2);

	error = cpuset_getaffinity(CPU_LEVEL_CPUSET, CPU_WHICH_PID, -1, sizeof(cpuset_t), &pinned_cpu_set);
	while (CPU_COUNT(&pinned_cpu_set) != 1) {
		CPU_CLR(CPU_FFS(&pinned_cpu_set)-1, &pinned_cpu_set);
	}
	pthread_attr_setaffinity_np(&thr_attr, sizeof(pinned_cpu_set), &pinned_cpu_set);
	pthread_attr_setaffinity_np(&thr_attr2, sizeof(pinned_cpu_set), &pinned_cpu_set);

	sched_params.sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_attr_setschedpolicy(&thr_attr, SCHED_FIFO);
	pthread_attr_setschedparam(&thr_attr, &sched_params);
	pthread_attr_setinheritsched(&thr_attr, PTHREAD_EXPLICIT_SCHED);

	pthread_attr_setschedpolicy(&thr_attr2, SCHED_FIFO);
	pthread_attr_setschedparam(&thr_attr2, &sched_params);
	pthread_attr_setinheritsched(&thr_attr2, PTHREAD_EXPLICIT_SCHED);

	args.thr_mode = OP_RECV;
	pthread_create(&rcvr_thr, &thr_attr, thread_main, &args);
	args2.thr_mode = OP_SEND;
	pthread_create(&sndr_thr, &thr_attr2, thread_main, &args2);

	pthread_join(sndr_thr, NULL);
	pthread_join(rcvr_thr, NULL);
}

static void
get_limits(void)
{
    size_t len;
    size_t max_sock_op_size_send, max_sock_op_size_recv;
	
	len = sizeof(max_sock_op_size_send); 
	sysctlbyname("net.local.seqpacket.maxseqpacket", &max_sock_op_size_send, &len, NULL, 0);
	len = sizeof(max_sock_op_size_recv); 
	sysctlbyname("net.local.seqpacket.recvspace", &max_sock_op_size_recv, &len, NULL, 0);
	
	sock_op_len_send = max_sock_op_size_send;
	sock_op_len_recv = max_sock_op_size_recv;
}

int main(int argc, char *const argv[])
{
	int opt, error;
	char *strptr;

	while((opt = getopt(argc, argv, "sb:i:ahdcPS")) != -1) {
		switch (opt) {
		case 'h':
			format = HUMAN_READABLE;
			break;
		case 'a':
			aggregate_mode = 1;
			break;
		case 's':
			mode = OP_SEND;
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
		case 'c':
			enable_sha256_workload = true;
			enable_dummy_workload = true;
			break;
		case 'd':
			enable_sha256_workload = false;
			enable_dummy_workload = true;
			break;
		case 'P':
			pipe_enabled = true;
			break;
		case 'S':
			socket_enabled = true;
			break;
		case '?':
		default: 
			err(EX_USAGE, "invalid flag '%c'", (char)optopt);
			break;
		}
	}
	if (!pipe_enabled && !socket_enabled) {
		pipe_enabled = true;
		socket_enabled = true;
	}
	get_limits();
	run_in_thread();

	return (0);
}