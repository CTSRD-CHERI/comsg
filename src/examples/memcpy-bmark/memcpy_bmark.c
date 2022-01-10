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
#include <cheri/cheric.h>
#include <err.h>
#include <errno.h>
#include <statcounters.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sysexits.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static char * message_str;
static size_t message_len = 0;
static size_t runs = 0;


int main(int argc, char * const argv[]);

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
    free(pname);
    //if (!use_stdout)
    //    fclose(fp);
    return 0;
}

static
void prepare_message(void)
{
	int fd, status;

	message_str = calloc(message_len, sizeof(char));
	fd = open("/dev/random", O_RDONLY);
	status = read(fd, message_str, message_len);
	if (status < 0)
		err(EX_SOFTWARE, "%s: error reading random data for message",__func__ );
}

static int quiet = 0;
int main(int argc, char * const argv[])
{
	int opt;
	char * strptr;
	char * buffer;
	char *stat_path, *rusage_path;
	FILE *statfp, *rusagefp;
	while((opt=getopt(argc,argv,"t:i:b:q"))!=-1)
	{
		switch(opt)
		{
			case 'b':
				message_len = strtol(optarg, &strptr, 10);
				if (*optarg == '\0' || *strptr != '\0' || message_len < 0)
					err(1,"invalid buffer length");
				break;
			case 'i':
				runs = strtol(optarg, &strptr, 10);
				if (*optarg == '\0' || *strptr != '\0' || runs <= 0)
					err(1,"invalid runs");
				break;
            case 'q':
                quiet = 1;
                break;
			default:
				break;
		}
	}	
	if(runs==0)
		runs=1;
	if(message_len==0)
		message_len=512;
				
	prepare_message();

	stat_path = calloc(255, sizeof(char));
	rusage_path = calloc(255, sizeof(char));

	sprintf(stat_path, "/tmp/memcpy_b%lu.csv", message_len);
	sprintf(rusage_path, "/tmp/memcpy_b%lu_rusage.dat", message_len);
	

	float ipc_time;
	struct timespec start_timestamp,end_timestamp;
	struct rusage start_rusage, end_rusage;
	statcounters_bank_t end_bank,start_bank;
	char *bw_str;
	
	buffer = calloc(message_len, sizeof(char));
	for(size_t i = 0; i<runs; i++)
	{
		statcounters_zero(&start_bank);
		statcounters_zero(&end_bank);

		getrusage(RUSAGE_THREAD, &start_rusage);
		clock_gettime(CLOCK_REALTIME, &start_timestamp);
		statcounters_sample(&start_bank);

		memcpy(buffer, message_str, message_len);

		statcounters_sample(&end_bank);
		clock_gettime(CLOCK_REALTIME, &end_timestamp);
		getrusage(RUSAGE_THREAD, &end_rusage);

		timespecsub(&end_timestamp,&start_timestamp);
		statcounters_diff(&end_bank,&end_bank,&start_bank);
		rusage_diff(&end_rusage,&end_rusage,&start_rusage);
		ipc_time = (float)end_timestamp.tv_sec + (float)end_timestamp.tv_nsec / 1000000000;
		bw_str = calloc(40, sizeof(char));
		sprintf(bw_str, "%.2FKB/s", ((message_len / ipc_time) / 1024.0));

		//*_dump_with_args closes the files so must open each time
		statfp = fopen(stat_path, "a+");
		rusagefp = fopen(rusage_path, "a+");
		if(i == 0){
			statcounters_dump_with_args(&end_bank, "MEMCPY", "", bw_str, statfp, CSV_HEADER);
			rusage_dump_with_args(&end_rusage, "MEMCPY", "", bw_str, rusagefp, CSV_HEADER);
		} else {
			statcounters_dump_with_args(&end_bank, "MEMCPY", "", bw_str, statfp, CSV_NOHEADER);		
			rusage_dump_with_args(&end_rusage, "MEMCPY", "", bw_str, rusagefp, CSV_NOHEADER);
		}
		printf("Memcpy: %s\n",bw_str);
        if(!quiet)
        {
		  statcounters_dump(&end_bank);
		  rusage_dump_with_args(&end_rusage,"MEMCPY","",bw_str,NULL,HUMAN_READABLE);
        }
	}
	free(buffer);

	return (0);
}