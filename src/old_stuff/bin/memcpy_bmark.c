#include <strings.h>
#include <stdio.h>
#include <statcounters.h>
#include <sys/resource.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>

static const char * one_k_str = "SIqfubhhJOVdGGMvw09vqHs7S8miUyi1JBaFNGbtKYy4vUs6QeB1JdrMAlWOcC5llzZ5XogADMOvIyNP9R0deF6Coi8RDsf1HUQFVZXYskgmUODJb0uB88DkY2h2qS1dMEX06tTaUWCTDOwRWt9qtgtD8OfBH0uKp6ABwt5vbCjchd7npp12jXBDdAzzkC81DOTdYGMsuuZ6iqMQt0CwkesUj4HSJ7exSaD5hQn47hpr2cinaATAlmfd8G1oKlYWCcXszmGAPkZm4qpE6lA51dTMNmR9kXvnONnMFWesrjI8XmA7qss71oSUgIu10WCnJ7YJA97lg40fCQ647lZCZKdsqGF7XZAgJEkAwSmZ7apdVK4zmlK8JXkdKCuecHxEJk3NDLdN83qvonYiJE7aoZHmibjwHMiJDAtmPKlaJBnKS5yLNXRExHLH5GXvjrvRIdDzCtQZStt4ZW8PickMMcDczSyP7Kr0OwjPaX3dSgU6PFRX3hKbYGyXx28VdzeAZ2ynvv1b5i13Hg9xW6oeidFVFw0SsuTg9gVmbYRr9F20LdDxGaJBMofsX6SbHx66JmtsgztP0DWAQxxviSRlUBi8fYvgxHqRfyYyGEFi5V1GMbPtpIKB6EZ2ixOt63VcXTK2egU4dzDcOlgognDz8253LFn0e02hNRX0nRRkamJ0xMkS9tzBW8NhdxG2iQ0zWbAyHzPWmFYQOvrXPm0u5yS2drByXmz5y9S8LvnBAZ1vlaLAylyMIxy8z6clpCIZqTovP3X0Eg6xPuLg4xQpXvxwx3U2WryMcDWRmAJWW7XL6JfzipyZTI9GGPiNp73Hs9CwSlQMpJDh9ByvzsWKmDKm3YUHLDwqe7XdmBbQfOkEKyjrQPr10TvNA0euXw0TTu6dmziZLSGrLv1DFLcuxzQ9CZkg1bkFX8RUzREjG8NmdGa0YLRdru2FWLqC1rCG6c3aD2qp2v0SuVUlJe9qj5aUsFjOlq5s9XGJJepCb6TTeHKP8jsHL7jnJQXIR9O0";
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
            fprintf(fp, "involuntary_context_switches,");
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
    free(pname);
    //if (!use_stdout)
    //    fclose(fp);
    return 0;
}

static
void prepare_message(void)
{
	unsigned int i, message_remaining, data_copied, to_copy;

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
	mlock(message_str,message_len);
}

int main(int argc, char *argv[])
{
	char * buffer;
	char * filename = malloc(255);
	FILE *statfp, *rusagefp;
	while((opt=getopt(argc,argv,"t:r:b:"))!=-1)
	{
		switch(opt)
		{
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
			default:
				break;
		}
	}	
	if(runs==0)
		runs=1;
	if(message_len==0)
		message_len=512;
				
	sprintf(filename,"/tmp/memcpy_b%lu.dat",message_len);
	statfp=fopen(filename,"a+");
	sprintf(filename,"/tmp/memcpy_b%lu_rusage.dat",message_len);
	rusagefp=fopen(filename,"a+");

	char * bw_str = malloc(40);
	float ipc_time;
	struct timespec start_timestamp,end_timestamp;
	struct rusage start_rusage, end_rusage;
	statcounters_bank_t end_bank,start_bank;
	buffer=calloc(message_len,sizeof(char));
	for(size_t i = 0; i<runs; i++)
	{
		memcpy(buffer,message_str,message_len);
		getrusage(&start_rusage);
		clock_gettime(&start_timestamp);
		statcounters_sample(&start_bank);
		memcpy(buffer,message_str,message_len);
		statcounters_sample(&end_bank);
		clock_gettime(&end_timestamp);
		getrusage(&end_rusage);

		timespecsub(&end_timestamp,&start_timestamp);
		statcounters_diff(&end_bank,&end_bank,&start_bank);
		rusage_diff(&end_rusage,&end_rusage,&start_rusage);
		ipc_time=(float)end_timestamp.tv_sec + (float)end_timestamp.tv_nsec / 1000000000;
		sprintf(bw_str,"%.2FKB/s",(((message_len)/ipc_time)/1024.0));

		if(i==0){
			statcounters_dump_with_args(&end_bank,"COSEND","beri",bw_str,statfp,CSV_HEADER);
			rusage_dump_with_args(&end_rusage,"COSEND","beri",bw_str,rusagefp,CSV_HEADER);
		}
		else{
			statcounters_dump_with_args(&end_bank,"COSEND","beri",bw_str,statfp,CSV_NOHEADER);		
			rusage_dump_with_args(&end_rusage,"COSEND","beri",bw_str,rusagefp,CSV_NOHEADER);

		}
		printf("Memcpy: %s\n",bw_str);
		statcounters_dump(&end_bank);
		rusage_dump_with_args(&end_rusage,"COSEND","beri",bw_str,NULL,HUMAN_READABLE);
	}
}