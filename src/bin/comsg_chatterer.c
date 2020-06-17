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
#define COMSG_BENCHMARK
#include "comsg.h"
#include "coport.h"

#include <sys/thr.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/rtprio.h>
#include <inttypes.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <pthread.h>
#include <pthread_np.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "statcounters.h"
#include <stdatomic.h>
#include <sys/cpuset.h>
#include <sys/param.h>
#include <machine/sysarch.h>
#include <machine/cheri.h>
#include <sys/sysctl.h>

extern char **environ;

static char * message_str = NULL;
static const char * one_k_str = "SIqfubhhJOVdGGMvw09vqHs7S8miUyi1JBaFNGbtKYy4vUs6QeB1JdrMAlWOcC5llzZ5XogADMOvIyNP9R0deF6Coi8RDsf1HUQFVZXYskgmUODJb0uB88DkY2h2qS1dMEX06tTaUWCTDOwRWt9qtgtD8OfBH0uKp6ABwt5vbCjchd7npp12jXBDdAzzkC81DOTdYGMsuuZ6iqMQt0CwkesUj4HSJ7exSaD5hQn47hpr2cinaATAlmfd8G1oKlYWCcXszmGAPkZm4qpE6lA51dTMNmR9kXvnONnMFWesrjI8XmA7qss71oSUgIu10WCnJ7YJA97lg40fCQ647lZCZKdsqGF7XZAgJEkAwSmZ7apdVK4zmlK8JXkdKCuecHxEJk3NDLdN83qvonYiJE7aoZHmibjwHMiJDAtmPKlaJBnKS5yLNXRExHLH5GXvjrvRIdDzCtQZStt4ZW8PickMMcDczSyP7Kr0OwjPaX3dSgU6PFRX3hKbYGyXx28VdzeAZ2ynvv1b5i13Hg9xW6oeidFVFw0SsuTg9gVmbYRr9F20LdDxGaJBMofsX6SbHx66JmtsgztP0DWAQxxviSRlUBi8fYvgxHqRfyYyGEFi5V1GMbPtpIKB6EZ2ixOt63VcXTK2egU4dzDcOlgognDz8253LFn0e02hNRX0nRRkamJ0xMkS9tzBW8NhdxG2iQ0zWbAyHzPWmFYQOvrXPm0u5yS2drByXmz5y9S8LvnBAZ1vlaLAylyMIxy8z6clpCIZqTovP3X0Eg6xPuLg4xQpXvxwx3U2WryMcDWRmAJWW7XL6JfzipyZTI9GGPiNp73Hs9CwSlQMpJDh9ByvzsWKmDKm3YUHLDwqe7XdmBbQfOkEKyjrQPr10TvNA0euXw0TTu6dmziZLSGrLv1DFLcuxzQ9CZkg1bkFX8RUzREjG8NmdGa0YLRdru2FWLqC1rCG6c3aD2qp2v0SuVUlJe9qj5aUsFjOlq5s9XGJJepCb6TTeHKP8jsHL7jnJQXIR9O0";
static size_t message_len = 0;
static size_t runs = 1;
static size_t total_size = 0;
static const char * port_names[] = { "benchmark_portA", "benchmark_portB", "benchmark_portC" };
static char * port_name;

static int may_start=0;
static int waiting_threads = 0;
static int acked_threads = 0;
static int trace = 0;
static int switching_types = 0;

static pthread_mutex_t start_lock;
static pthread_mutex_t async_lock; //to ensure async_lockhronous operations don't get mixed up
static pthread_mutex_t output_lock; //to prevent output_lock interleaving	
static pthread_cond_t may_continue, waiting;
static int quiet = 0;
//static coport_op_t chatter_operation = COSEND;
static coport_type_t coport_type = COPIPE;
//static const char * ts_port_name = "timestamp_port";
#undef timespecsub
#define	timespecsub(vvp, uvp)						\
	do {								\
		(vvp)->tv_sec -= (uvp)->tv_sec;				\
		(vvp)->tv_nsec -= (uvp)->tv_nsec;			\
		if ((vvp)->tv_nsec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_nsec += 1000000000;			\
		}							\
	} while (0)
void send_data(void);
void receive_data(void);
int main(int argc, char * const argv[]);

static long receiver_id, sender_id;
static char arch_name[6] = "BERI";

static statcounters_bank_t send_start, recv_end;
static int multicore;
static pthread_t sender, receiver;
/*
static void send_timestamp(struct timespec * timestamp)
{
	int status;

	status=coopen(ts_port_name,COCHANNEL,&port);
	cosend(port,timestamp,sizeof(timestamp));
	
}
*/

static void
timevalfix(struct timeval *t1)
{

	if (t1->tv_usec < 0) {
		t1->tv_sec--;
		t1->tv_usec += 1000000;
	}
	if (t1->tv_usec >= 1000000) {
		t1->tv_sec++;
		t1->tv_usec -= 1000000;
	}
}

static 
void
timevalsub(struct timeval *t1, const struct timeval *t2)
{

	t1->tv_sec -= t2->tv_sec;
	t1->tv_usec -= t2->tv_usec;
	timevalfix(t1);
}
static _Atomic int sched_set = 0;
static
void set_sched(void)
{
	if(atomic_fetch_add(&sched_set,1)!=0)
	{
		return;
	}
	struct rtprio * rt_params=malloc(sizeof(struct rtprio));
	rt_params->type=RTP_PRIO_FIFO;
	rt_params->prio=RTP_PRIO_MIN;
	if(rtprio_thread(RTP_SET,0,rt_params))
		perror("could not set rtprio for self");
	if(rtprio_thread(RTP_SET,receiver_id,rt_params))
		perror("could not set rtprio for self");
	if(rtprio_thread(RTP_SET,sender_id,rt_params))
		perror("could not set rtprio for self");
	rt_params=cheri_setoffset(rt_params,0);
	free(rt_params);
	return;
	/*
	struct sched_param sched_params;
	sched_params.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0,SCHED_FIFO,&sched_params);
	return;*/
	/*if(pthread_setschedparam(pthread_self(),SCHED_FIFO,&sched_params))
		err(errno,"could not set pthread_self prio SCHED_FIFO");
	if(pthread_setschedparam(receiver,SCHED_FIFO,&sched_params))
		err(errno,"could not set receiver prio SCHED_FIFO");
	if(pthread_setschedparam(sender,SCHED_FIFO,&sched_params))
		err(errno,"could not set sender prio SCHED_FIFO");*/

}

static
void set_sched_async(void)
{
	struct rtprio * rt_params=malloc(sizeof(struct rtprio));
	rt_params->type=RTP_PRIO_FIFO;
	rt_params->prio=RTP_PRIO_MIN;
	if(rtprio_thread(RTP_SET,0,rt_params))
		perror("could not set rtprio for self");
	rt_params=cheri_setoffset(rt_params,0);
	free(rt_params);
	return;
	/*
	struct sched_param sched_params;
	sched_params.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0,SCHED_FIFO,&sched_params);

	return;	*/
}

static
void unset_sched_async(void)
{
	struct rtprio * rt_params=malloc(sizeof(struct rtprio));
	rt_params->type=RTP_PRIO_NORMAL;
	rt_params->prio=RTP_PRIO_MIN;
	rtprio_thread(RTP_SET,0,rt_params);
	rt_params=cheri_setoffset(rt_params,0);
	free(rt_params);
	return;
/*
	struct sched_param sched_params;
	sched_params.sched_priority = sched_get_priority_max(SCHED_OTHER);
	sched_setscheduler(0,SCHED_OTHER,&sched_params);
	return;*/
}

static
void unset_sched(void)
{
	if(atomic_fetch_add(&sched_set,1)==0)
	{
		sched_set=0;
		return;
	}
	struct rtprio * rt_params=malloc(sizeof(struct rtprio));
	rt_params->type=RTP_PRIO_NORMAL;
	rt_params->prio=RTP_PRIO_MIN;
	rtprio_thread(RTP_SET,0,rt_params);
	rtprio_thread(RTP_SET,receiver_id,rt_params);
	rtprio_thread(RTP_SET,sender_id,rt_params);
	sched_set=0;
	rt_params=cheri_setoffset(rt_params,0);
	free(rt_params);
	return;
/*
	sched_yield();
	struct sched_param sched_params;
	sched_params.sched_priority = sched_get_priority_max(SCHED_OTHER);
	sched_setscheduler(0,SCHED_OTHER,&sched_params);*/
}

static 
void rusage_diff(struct rusage *dest,struct rusage *end,struct rusage *start)
{
	dest->ru_utime=end->ru_utime;
	timevalsub(&dest->ru_utime,&start->ru_utime);	/* user time used */
	dest->ru_stime=end->ru_stime;
	timevalsub(&dest->ru_stime,&start->ru_stime);	/* system time used */

	dest->ru_maxrss=end->ru_maxrss-start->ru_maxrss;		/* max resident set size */
	dest->ru_ixrss=end->ru_ixrss-start->ru_ixrss;		/* integral shared memory size */
	dest->ru_idrss=end->ru_idrss-start->ru_idrss;		/* integral unshared data " */
	dest->ru_isrss=end->ru_isrss-start->ru_isrss;		/* integral unshared stack " */
	dest->ru_minflt=end->ru_minflt-start->ru_minflt;		/* page reclaims */
	dest->ru_majflt=end->ru_majflt-start->ru_majflt;		/* page faults */
	dest->ru_nswap=end->ru_nswap-start->ru_nswap;		/* swaps */
	dest->ru_inblock=end->ru_inblock-start->ru_inblock;		/* block input operations */
	dest->ru_oublock=end->ru_oublock-start->ru_oublock;		/* block output operations */
	dest->ru_msgsnd=end->ru_msgsnd-start->ru_msgsnd;		/* messages sent */
	dest->ru_msgrcv=end->ru_msgrcv-start->ru_msgrcv;		/* messages received */
	dest->ru_nsignals=end->ru_nsignals-start->ru_nsignals;		/* signals received */
	dest->ru_nvcsw=end->ru_nvcsw-start->ru_nvcsw;		/* voluntary context switches */
	dest->ru_nivcsw=end->ru_nivcsw-start->ru_nivcsw;		/* involuntary " */
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
    const char * aname;
    if (!archname) {
        aname = getenv("STATCOUNTERS_ARCHNAME");
        if (!aname || aname[0] == '\0')
            aname = "\0";
    } else {
        aname = archname;
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
            fprintf(fp, "%s,",aname);
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
            fprintf(fp, "===== %s -- %s =====\n",pname, aname);
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
    pname=cheri_setoffset(pname,0);
	free(pname);
    //if (!use_stdout)
    //    fclose(fp);
    return 0;
}


void send_data(void)
{
	int intval = 1;
	int error = 0;

	coport_t port = (void * __capability)-1;
	statcounters_bank_t bank2,result;
	struct timespec start_timestamp,end_timestamp;
	struct timespec sleepytime;
	sleepytime.tv_sec=0;
	sleepytime.tv_nsec=100;
	float ipc_time;
	int status;
	size_t remaining_len = total_size;
	struct rusage start_rusage, end_rusage, result_rusage;
	char * name = NULL;

	if(name == NULL)
	{
		name = malloc(sizeof(char)*255);
	}
	char * rusage_name = NULL;
	if(rusage_name == NULL)
	{
		rusage_name = malloc(sizeof(char)*255);
	}
	
	char * bw_str = malloc(100);
	
	FILE *fp, *rusage_fp;

	error=pthread_mutex_lock(&start_lock);
	if(error)
		err(error,"send_data: lock start_lock failed");
	while(waiting_threads!=acked_threads)
	{
		pthread_mutex_unlock(&start_lock);
		sched_yield();
		pthread_mutex_lock(&start_lock);
	}
	waiting_threads++;
	pthread_cond_signal(&waiting);
	pthread_cond_wait(&may_continue,&start_lock);
	pthread_mutex_unlock(&start_lock);
	
	switch(coport_type)
	{
		case COCARRIER:
			sprintf(name,"/tmp/COCARRIER_cosend_b%lu_t%lu.dat",message_len,total_size);
			sprintf(rusage_name,"/tmp/COCARRIER_cosend_b%lu_t%lu_rusage.dat",message_len,total_size);
			break;
		case COPIPE:
			sprintf(name,"/tmp/COPIPE_cosend_b%lu_t%lu.dat",message_len,total_size);
			sprintf(rusage_name,"/tmp/COPIPE_cosend_b%lu_t%lu_rusage.dat",message_len,total_size);
			break;
		case COCHANNEL:
			sprintf(name,"/tmp/COCHANNEL_cosend_b%lu_t%lu.dat",message_len,total_size);
			sprintf(rusage_name,"/tmp/COCHANNEL_cosend_b%lu_t%lu_rusage.dat",message_len,total_size);
			break;
		default:
			break;
	}
	fp = fopen(name,"a+");
	rusage_fp = fopen(rusage_name,"a+");
	
	status=coopen(port_name,coport_type,&port);
	size_t waited_at = 0;
	for(unsigned long i = 0; i<runs; i++)
	{	
		if(coport_type==COPIPE){
			sleepytime.tv_sec=0;
			sleepytime.tv_nsec=10000;
			nanosleep(&sleepytime,&sleepytime);
			sched_yield();
			sched_yield();
		}
		else if (coport_type == COCARRIER && (((i*message_len*2)-waited_at)>=(1024*1024*16)))
		{
			waited_at=i*message_len*2;
			sleepytime.tv_sec=5;
			sleepytime.tv_nsec=0;
			nanosleep(&sleepytime,&sleepytime);
		}
		
		statcounters_zero(&send_start);
		statcounters_zero(&bank2);
		statcounters_zero(&result);
		
		if(coport_type!=COPIPE)
		{
			pthread_mutex_lock(&async_lock);
			error=pthread_mutex_lock(&output_lock); //keep it quiet during
			if(error)
				err(error,"send_data: lock output_lock failed");
		}
		if(coport_type==COCARRIER)
		{
			set_sched_async();
		}
		else
		{
			set_sched();
			if(coport_type==COPIPE)
				sched_yield();
		}
		
		if (trace)
		{
			intval=1;
			sysarch(QEMU_SET_QTRACE, &intval);
			CHERI_START_TRACE;
		}
		if (coport_type!=COCHANNEL)
		{
			status=cosend(port,message_str,message_len);
		}
		if(coport_type==COPIPE)
		{
			sched_yield();
			sched_yield();
		}
		remaining_len=total_size;
		getrusage(RUSAGE_THREAD,&start_rusage);
		clock_gettime(CLOCK_REALTIME,&start_timestamp);
		statcounters_sample(&send_start);
		if (coport_type!=COCHANNEL)
		{
			status=cosend(port,message_str,message_len);
		}
		else
		{			
			status=cosend(port,message_str,MIN(remaining_len,4096));
		}
		statcounters_sample_end(&bank2);
		clock_gettime(CLOCK_REALTIME,&end_timestamp);
		getrusage(RUSAGE_THREAD,&end_rusage);
		if (trace)
		{
			intval=0;
			sysarch(QEMU_SET_QTRACE, &intval);
			//CHERI_START_TRACE;
		}
		//clock_gettime(CLOCK_REALTIME,&end_timestamp);
	
		if(coport_type==COCARRIER)
		{
			unset_sched_async();
			/*sleepytime.tv_sec=0;
			sleepytime.tv_nsec=100000;
			nanosleep(&sleepytime,&sleepytime);*/
			pthread_mutex_unlock(&async_lock);
		}
		else 
		{
			unset_sched();
			if (coport_type==COCHANNEL)
			{
				while(!pthread_mutex_unlock(&async_lock));
			}
		}
		sched_yield();
		error=pthread_mutex_lock(&output_lock);
		if(error && coport_type==COPIPE)
			err(error,"send_data: lock failed");
		if(status==-1 && errno==EAGAIN && coport_type==COCHANNEL)
		{
			if(i>0)
				i--;
			pthread_mutex_unlock(&async_lock);
			pthread_mutex_unlock(&output_lock);
			sched_yield();
			continue;
		}
		else if(status==-1)
		{
			perror("cosend: err in cosend");
			while(!pthread_mutex_unlock(&async_lock));
			while(!pthread_mutex_unlock(&output_lock));
			continue;
		}
		timespecsub(&end_timestamp,&start_timestamp);
		statcounters_diff(&result,&bank2,&send_start);

		
		ipc_time=(float)end_timestamp.tv_sec + (float)end_timestamp.tv_nsec / 1000000000;
		
		printf("Send %lu: %.2FKB/s\n",total_size,(((total_size)/ipc_time)/1024.0));
		//printf("Sent %lu bytes\n", total_size);
		
		sprintf(bw_str,"%.2FKB/s",(((total_size)/ipc_time)/1024.0));
		rusage_diff(&result_rusage,&end_rusage,&start_rusage);
		//statcounters_dump_with_args calls fclose - not any more
		if(i==0){
			statcounters_dump_with_args(&result,"COSEND",arch_name,bw_str,fp,CSV_HEADER);
			rusage_dump_with_args(&result_rusage,"COSEND",arch_name,bw_str,rusage_fp,CSV_HEADER);
		}
		else{
			statcounters_dump_with_args(&result,"COSEND",arch_name,bw_str,fp,CSV_NOHEADER);		
			rusage_dump_with_args(&result_rusage,"COSEND",arch_name,bw_str,rusage_fp,CSV_NOHEADER);
		}
		if(!quiet)
		{
			statcounters_dump(&result);
			rusage_dump_with_args(&result_rusage,"COSEND",arch_name,bw_str,NULL,HUMAN_READABLE);
		}

		while(!pthread_mutex_unlock(&output_lock));
		sched_yield();

		if(coport_type!=COCHANNEL)
		{
			while(pthread_mutex_trylock(&output_lock))
				sched_yield();
			while(!pthread_mutex_unlock(&output_lock));
		}
		if(coport_type==COPIPE)
			sched_yield();
		
	}
	if(coport_type==COPIPE && 0)
		unset_sched();

	intval=0;
	if (trace)
		sysarch(QEMU_SET_QTRACE, &intval);
	if(coport_type!=COPIPE)
		while(!pthread_mutex_unlock(&async_lock));
	fclose(fp);
	fclose(rusage_fp);
	memset(name,0,strlen(name));
	coclose(port);
	bw_str=cheri_setoffset(bw_str,0);
	free(bw_str);
	name=cheri_setoffset(name,0);
	free(name);
	rusage_name=cheri_setoffset(rusage_name,0);
	free(rusage_name);
	port=NULL;

	//name=cheri_setoffset(name,0);
	free(name);
	
}

void receive_data(void)
{
	int intval = 1;
	int error = 0;
	coport_t port = (void * __capability)-1;
	
	statcounters_bank_t bank1,result;
	static char * buffer = NULL;
	struct timespec start_timestamp,end_timestamp;
	float ipc_time;
	int status;
	size_t remaining_len = message_len;
	struct rusage start_rusage, end_rusage, result_rusage;
	char * bw_str = malloc(100);
	char * name = calloc(255,sizeof(char));
	char * name2 = calloc(255,sizeof(char));
	char * rusage_name = calloc(255,sizeof(char));
	float bw_val;
	while(may_start==0)
		sched_yield();
	may_start=-1;
	switch(coport_type)
	{
		case COCHANNEL:
			sprintf(name,"/tmp/COCHANNEL_corecv_b%lu_t%lu.dat",message_len,total_size);
			sprintf(rusage_name,"/tmp/COCHANNEL_corecv_b%lu_t%lu_rusage.dat",message_len,total_size);

			//sprintf(name2,"/tmp/COCHANNEL_wholeop_b%lu_t%lu.dat",message_len,total_size);
			if (buffer==NULL)
				buffer=calloc(4096,sizeof(char));
			else if (cheri_getlen(buffer)<4096 || (cheri_getperm(buffer)&(CHERI_PERM_STORE))==0)
				buffer=calloc(4096,sizeof(char));
			else
				break;
			mlock(buffer,cheri_getlen(buffer));
			break;
		case COPIPE:
			sprintf(name,"/tmp/COPIPE_corecv_b%lu_t%lu.dat",message_len,total_size);
			sprintf(name2,"/tmp/COPIPE_wholeop_b%lu_t%lu.dat",message_len,total_size);
			sprintf(rusage_name,"/tmp/COPIPE_wholeop_b%lu_t%lu_rusage.dat",message_len,total_size);

			if (buffer==NULL || (cheri_getperm(buffer)&(CHERI_PERM_STORE))==0)
				buffer=calloc(message_len,sizeof(char));
			else if (cheri_getlen(buffer)<message_len || (cheri_getperm(buffer)&(CHERI_PERM_STORE))==0)
				buffer=calloc(message_len,sizeof(char));
			else
				break;
			mlock(buffer,cheri_getlen(buffer));
			break;
		case COCARRIER:
			sprintf(name,"/tmp/COCARRIER_corecv_b%lu_t%lu.dat",message_len,total_size);
			sprintf(rusage_name,"/tmp/COCARRIER_corecv_b%lu_t%lu_rusage.dat",message_len,total_size);

			if (buffer==NULL)
				buffer=calloc(message_len,sizeof(char));
			else if (cheri_getlen(buffer)<message_len || (cheri_getperm(buffer)&(CHERI_PERM_STORE))==0)
				buffer=calloc(message_len,sizeof(char));
			else
				break;
			mlock(buffer,cheri_getlen(buffer));

			break;
		default:
			break;
	}
	FILE *fp, *fp2, *rusage_fp;

	
	/*if(port->status==COPORT_CLOSED)
	{
		err(1,"port closed before receiving");
	}*/
	
	
	error=pthread_mutex_lock(&start_lock);
	if(error)
		err(error,"rsend_data: lock start_lock failed");
	while(waiting_threads!=acked_threads)
	{
		pthread_mutex_unlock(&start_lock);
		sched_yield();
		pthread_mutex_lock(&start_lock);
	}
	waiting_threads++;
	pthread_cond_signal(&waiting);
	pthread_cond_wait(&may_continue,&start_lock);
	pthread_mutex_unlock(&start_lock);
	if(trace)
		sysarch(QEMU_SET_QTRACE, &intval);

	fp= fopen(name,"a+");
	if(coport_type==COPIPE)
		fp2 = fopen(name2,"a+");
	rusage_fp=fopen(rusage_name,"a+");


	if(coport_type==COCARRIER)
		pthread_mutex_lock(&async_lock);
	status=coopen(port_name,coport_type,&port);
	for(unsigned long i = 0; i<runs; i++)
	{
		statcounters_zero(&bank1);
		statcounters_zero(&recv_end);
		statcounters_zero(&result);

		if(coport_type!=COPIPE)
		{
			if(coport_type==COCARRIER ||1)
			{
				pthread_mutex_lock(&async_lock);
			}
			error=pthread_mutex_lock(&output_lock); //keep it quiet during
			if(error)
				err(error,"recv_data: lock output_lock failed");

		}
		if(coport_type==COCARRIER)
		{
			set_sched_async();
		}
		else
		{
			set_sched();
			if(coport_type==COPIPE)
				sched_yield();
		}
		if (coport_type==COCHANNEL)
		{
			//status=corecv(port,(void **)&buffer,MIN(remaining_len,4096));
		}
		else
		{
			status=corecv(port,(void **)&buffer,message_len);
			
		}
		remaining_len=total_size;
		getrusage(RUSAGE_THREAD,&start_rusage);
		clock_gettime(CLOCK_REALTIME,&start_timestamp);
		
		if (coport_type==COCHANNEL)
		{
			statcounters_sample(&bank1);
			status=corecv(port,(void **)&buffer,MIN(remaining_len,4096));
			statcounters_sample_end(&recv_end);	
		}
		else
		{
			statcounters_sample(&bank1);
			status=corecv(port,(void **)&buffer,message_len);
			statcounters_sample_end(&recv_end);	
		}
		clock_gettime(CLOCK_REALTIME,&end_timestamp);
		getrusage(RUSAGE_THREAD,&end_rusage);
		if(coport_type==COPIPE)
		{
			sched_yield();
			unset_sched();			
		}
		else if(coport_type==COCHANNEL)
		{
			unset_sched();
			while(!pthread_mutex_unlock(&async_lock));
		}
		else if(coport_type == COCARRIER)
		{
			unset_sched_async();
			pthread_mutex_unlock(&async_lock);
		}
		sched_yield();
		error=pthread_mutex_lock(&output_lock);
		if(error && coport_type==COPIPE)
			err(error,"recv_data: lock output_lock failed");

		if(status==-1 && errno==EAGAIN && coport_type==COCHANNEL)
		{
			if(i>0)
				i--;
			perror("corecv: err in corecv, retrying...");
			pthread_mutex_unlock(&async_lock);
			pthread_mutex_unlock(&output_lock);
			sched_yield();
			continue;
		}
		else if(status==-1)
		{
			perror("corecv: err in corecv");
			while(!pthread_mutex_unlock(&async_lock));
			while(!pthread_mutex_unlock(&output_lock));
			continue;
		}
		
		if(buffer==NULL)
		{
			err(1,"buffer not written to");
		}
		//printf("message received:%s\n",(char *)buffer);
		timespecsub(&end_timestamp,&start_timestamp);
		statcounters_diff(&result,&recv_end,&bank1);
		
		ipc_time=(float)end_timestamp.tv_sec + (float)end_timestamp.tv_nsec / 1000000000;
		bw_val=((total_size)/ipc_time)/1024.0;
		//printf("Received %lu bytes in %lf\n", total_size, ipc_time);
		//printf("Received %lu bytes\n", total_size);

		sprintf(bw_str,"%.2FKB/s",bw_val);
		rusage_diff(&result_rusage,&end_rusage,&start_rusage);
		if(i==0)
		{
			statcounters_dump_with_args(&result,"CORECV",arch_name,bw_str,fp,CSV_HEADER);
			rusage_dump_with_args(&result_rusage,"CORECV",arch_name,bw_str,rusage_fp,CSV_HEADER);

		}
		else
		{
			statcounters_dump_with_args(&result,"CORECV",arch_name,bw_str,fp,CSV_NOHEADER);
			rusage_dump_with_args(&result_rusage,"CORECV",arch_name,bw_str,rusage_fp,CSV_NOHEADER);
		}
		printf("%.2FKB/s\n",bw_val);
		if(!quiet)
		{
			statcounters_dump(&result);
			rusage_dump_with_args(&result_rusage,"CORECV",arch_name,bw_str,0,HUMAN_READABLE);
		}
		if (coport_type!=COPIPE)
		{
			while(!pthread_mutex_unlock(&output_lock));
			sched_yield();
		}
		else
		{
			statcounters_diff(&result,&recv_end,&send_start);
			if(i==0)
				statcounters_dump_with_args(&result,"CORECV",arch_name,bw_str,fp2,CSV_HEADER);
			else
				statcounters_dump_with_args(&result,"CORECV",arch_name,bw_str,fp2,CSV_NOHEADER);
		}
		while(!pthread_mutex_unlock(&output_lock));
		sched_yield();
		while(pthread_mutex_trylock(&output_lock))
			sched_yield();
		while(!pthread_mutex_unlock(&output_lock));
		if(coport_type==COCHANNEL)
			sched_yield();
	}
	if(coport_type==COPIPE && 0)
		unset_sched();
	intval=0;
	if(coport_type!=COPIPE)
	{
		if (trace)
		{	
			sysarch(QEMU_SET_QTRACE, &intval);
			CHERI_STOP_TRACE;
		}
	}
	else
		fclose(fp2);
	fclose(fp);
	memset(name,0,strlen(name));
	memset(name2,0,strlen(name2));
	coclose(port);
	bw_str=cheri_setoffset(bw_str,0);
	free(bw_str);
	name=cheri_setoffset(name,0);
	free(name);
	rusage_name=cheri_setoffset(rusage_name,0);
	free(rusage_name);
	if(coport_type!=COPIPE)
		while(!pthread_mutex_unlock(&async_lock));
	port=NULL;
	may_start=0;
}	

static
void prepare_message(void)
{
	unsigned int i, message_remaining, data_copied, to_copy;
	munlock(message_str,cheri_getlen(message_str));
	if(message_str!=NULL)
		message_str=cheri_setoffset(message_str,0);
	free(message_str);
	message_str=calloc(message_len,sizeof(char));
	if (message_len%1024==0)
	{
		for(i = 0; i < message_len-1024; i+=1024)
		{
			memcpy(message_str+i,one_k_str,1024);
		}
		strncpy(message_str+i,one_k_str,1023);
	}
	else
	{
		message_remaining=message_len;
		data_copied=0;
		while(1)
		{
			to_copy=MIN(message_remaining,1024);
			memcpy(message_str+data_copied,one_k_str,to_copy);
			data_copied+=to_copy;
			if (message_remaining==0)
				break;
			message_remaining-=to_copy;
		}
		
	}
	message_str[message_len-1]='\0';
	mlock(message_str,cheri_getlen(message_str));
}

static int done = 0;
static int big_run = 0;

static
void *do_send(void* args)
{	
	//int error = 0;
	thr_self(&sender_id);
	port_name=malloc(strlen(port_names[1])+1);
	
	do {
		pthread_mutex_lock(&output_lock);
		pthread_mutex_lock(&async_lock);
		prepare_message();
		coport_type=COPIPE;
		strcpy(port_name,port_names[0]);
		
		
		statcounters_reset();

		may_start=1;
		pthread_mutex_unlock(&output_lock);
		send_data();

		while(may_start!=0)
			sched_yield();
		if(message_len<=1024*1024)
		{
			switching_types++;
			strcpy(port_name,port_names[1]);
			coport_type=COCARRIER;

			may_start=1;
			send_data();
		}
		while(!pthread_mutex_unlock(&async_lock));
		if(message_len<=4096)
		{	
			switching_types++;
			sched_yield();
			while(may_start!=0)
				sched_yield();
			pthread_mutex_lock(&async_lock);
			strcpy(port_name,port_names[2]);
			coport_type=COCHANNEL;

			may_start=1;
			send_data();

		}
		while(!pthread_mutex_unlock(&async_lock));
		done++;	
		while(may_start!=0)
			sched_yield();
		sched_yield();

	} while(message_len<1048576 && big_run);
	pthread_join(receiver,NULL);

	return args;
}

static
void *do_recv(void* args)
{
	//int error = 0;
	thr_self(&receiver_id);
	do {
		statcounters_reset();
		receive_data();
		switching_types++;
		if(message_len<=1024*1024)
			receive_data();
		if(message_len<=4096)
		{
			switching_types++;
			receive_data();
		}
		pthread_mutex_lock(&output_lock);
		done++;
		if(big_run)
		{
			message_len+=1024;
			total_size=message_len;
		}
		pthread_mutex_unlock(&output_lock);
	} while(message_len<1048576 && big_run);
	
	return args;
}
/*
static cpusetid_t cochatter_setid;*/
//static cpuset_t fullset = CPUSET_T_INITIALIZER(CPUSET_FSET);
static cpuset_t recv_cpu_set = CPUSET_T_INITIALIZER(CPUSET_FSET);
static cpuset_t send_cpu_set = CPUSET_T_INITIALIZER(CPUSET_FSET);

static
void get_arch(void)
{
	int mib[4];
    int cores;
    size_t len = sizeof(cores); 

    /* set the mib for hw.ncpu */
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;

    /* get the number of CPUs from the system */
    sysctl(mib, 2, &cores, &len, NULL, 0);
    if (cores>1)
        multicore=1;
    if(!multicore)
    	strcpy(arch_name,"MALTA");
}
int main(int argc, char * const argv[])
{
	int opt;
	char * strptr;
	
	int receive = 0;
	
	int explicit;
	while((opt=getopt(argc,argv,"ot:r:b:pqc:Q"))!=-1)
	{
		switch(opt)
		{
			case 'o':
				break;
			case 'Q':
				quiet=1;
				break;
			case 'q':
				trace=1;
				break;
			case 'b':
				message_len = strtol(optarg, &strptr, 10);
				explicit=1;
				if (*optarg == '\0' || *strptr != '\0' || message_len < 0)
					err(1,"invalid buffer length");
				break;
			case 'r':
				runs = strtol(optarg, &strptr, 10);
				if (*optarg == '\0' || *strptr != '\0' || runs <= 0)
					err(1,"invalid runs");
				break;
			case 't':
				total_size = strtol(optarg, &strptr, 10);
				if (*optarg == '\0' || *strptr != '\0' || total_size <=0)
					err(1,"invalid total length");
				break;
			case 'p':
				receive=1;
				break;
			case 'c':
				receive=1;
				break;
			default:
				break;
		}
	}
	if(message_len == 0 && explicit)
	{
		big_run=1;
		message_len=1024;
		total_size=message_len;
	}
	else if(message_len == 0)
	{
		if(total_size == 0)
		{
			total_size=512;
			message_len=512;
		}
		else
			message_len=total_size;
	}
	else if (total_size==0 || total_size % message_len !=0)
	{
		total_size=message_len;
	}
	CPU_CLR(CPU_FFS(&send_cpu_set)-1,&send_cpu_set);
	CPU_CLR(CPU_FFS(&send_cpu_set)-1,&recv_cpu_set);
	//send_cpu_set = CPUSET_NAND(&fullset,&recv_cpu_set);
	get_arch();
	struct timespec sleepytime;
	sleepytime.tv_sec=0;
	sleepytime.tv_nsec=100;
	pthread_attr_t send_attr,recv_attr;
	pthread_attr_init(&send_attr);
	pthread_attr_init(&recv_attr);
	
	pthread_attr_setaffinity_np(&send_attr, sizeof(cpuset_t), &send_cpu_set);
	pthread_attr_setaffinity_np(&recv_attr, sizeof(cpuset_t), &recv_cpu_set);

	struct sched_param sched_params;
	sched_params.sched_priority = sched_get_priority_min(SCHED_FIFO);
	pthread_attr_setschedpolicy(&send_attr,SCHED_FIFO);
	pthread_attr_setschedpolicy(&recv_attr,SCHED_FIFO);
	pthread_attr_setschedparam(&send_attr,&sched_params);
	pthread_attr_setschedparam(&recv_attr,&sched_params);
	
	pthread_mutexattr_t type;
	pthread_mutexattr_init(&type);
	pthread_mutexattr_settype(&type,PTHREAD_MUTEX_ERRORCHECK);
	
	pthread_mutex_init(&output_lock,&type);
	pthread_mutex_init(&start_lock,&type);

	pthread_mutexattr_settype(&type,PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&async_lock,&type);
	pthread_cond_init(&may_continue,NULL);
	pthread_cond_init(&waiting,NULL);

	statcounters_reset();

	pthread_mutex_lock(&start_lock);
	
	setpriority(PRIO_PROCESS,0,PRIO_MIN);
	pthread_create(&sender,&send_attr,do_send,NULL);
	pthread_create(&receiver,&recv_attr,do_recv,NULL);
	while (done<2)
	{
		pthread_cond_wait(&waiting,&start_lock);
		acked_threads++;
		pthread_cond_wait(&waiting,&start_lock);
		acked_threads++;
		pthread_cond_broadcast(&may_continue);
		acked_threads=0;
		waiting_threads=0;

	}
	pthread_mutex_unlock(&start_lock);
	return 0;
}