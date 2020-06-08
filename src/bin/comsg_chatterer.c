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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
#include <statcounters.h>
#include <stdatomic.h>
#include <sys/cpuset.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/sysarch.h>
#include <machine/cheri.h>

extern char **environ;

static char * message_str;
static const char * one_k_str = "SIqfubhhJOVdGGMvw09vqHs7S8miUyi1JBaFNGbtKYy4vUs6QeB1JdrMAlWOcC5llzZ5XogADMOvIyNP9R0deF6Coi8RDsf1HUQFVZXYskgmUODJb0uB88DkY2h2qS1dMEX06tTaUWCTDOwRWt9qtgtD8OfBH0uKp6ABwt5vbCjchd7npp12jXBDdAzzkC81DOTdYGMsuuZ6iqMQt0CwkesUj4HSJ7exSaD5hQn47hpr2cinaATAlmfd8G1oKlYWCcXszmGAPkZm4qpE6lA51dTMNmR9kXvnONnMFWesrjI8XmA7qss71oSUgIu10WCnJ7YJA97lg40fCQ647lZCZKdsqGF7XZAgJEkAwSmZ7apdVK4zmlK8JXkdKCuecHxEJk3NDLdN83qvonYiJE7aoZHmibjwHMiJDAtmPKlaJBnKS5yLNXRExHLH5GXvjrvRIdDzCtQZStt4ZW8PickMMcDczSyP7Kr0OwjPaX3dSgU6PFRX3hKbYGyXx28VdzeAZ2ynvv1b5i13Hg9xW6oeidFVFw0SsuTg9gVmbYRr9F20LdDxGaJBMofsX6SbHx66JmtsgztP0DWAQxxviSRlUBi8fYvgxHqRfyYyGEFi5V1GMbPtpIKB6EZ2ixOt63VcXTK2egU4dzDcOlgognDz8253LFn0e02hNRX0nRRkamJ0xMkS9tzBW8NhdxG2iQ0zWbAyHzPWmFYQOvrXPm0u5yS2drByXmz5y9S8LvnBAZ1vlaLAylyMIxy8z6clpCIZqTovP3X0Eg6xPuLg4xQpXvxwx3U2WryMcDWRmAJWW7XL6JfzipyZTI9GGPiNp73Hs9CwSlQMpJDh9ByvzsWKmDKm3YUHLDwqe7XdmBbQfOkEKyjrQPr10TvNA0euXw0TTu6dmziZLSGrLv1DFLcuxzQ9CZkg1bkFX8RUzREjG8NmdGa0YLRdru2FWLqC1rCG6c3aD2qp2v0SuVUlJe9qj5aUsFjOlq5s9XGJJepCb6TTeHKP8jsHL7jnJQXIR9O0";
static unsigned long int message_len = 0;
static unsigned long int runs = 1;
static unsigned long int total_size = 0;
static const char * port_names[] = { "benchmark_portA", "benchmark_portB", "benchmark_portC" };
static char * port_name;

static uint may_start=0;
static uint may_start2=0;
//static int waiting_threads = 0;
//static int acked_threads = 0;
static int trace = 0;
static int switching_types = 0;
static int multicore = 0;
static pthread_mutex_t start_lock;
static pthread_mutex_t async_lock; //to ensure async_lockhronous operations don't get mixed up
static pthread_mutex_t output_lock; //to prevent output_lock interleaving	
static pthread_cond_t may_continue, waiting;
static pthread_attr_t send_attr,recv_attr;
static pthread_t sender, receiver;
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

static struct sched_param sched_params;

static statcounters_bank_t send_start, recv_end;

/*
static void send_timestamp(struct timespec * timestamp)
{
	int status;

	status=coopen(ts_port_name,COCHANNEL,&port);
	cosend(port,timestamp,sizeof(timestamp));
	
}
*/
static
void set_sched(void)
{
	pthread_setschedparam(pthread_self(),SCHED_FIFO,&sched_params);
}

static
void unset_sched(void)
{
	pthread_setschedparam(pthread_self(),SCHED_OTHER,&sched_params);
}

void send_data(void)
{
	int intval = 1;
	int error = 0;

	coport_t port = (void * __capability)-1;
	statcounters_bank_t bank2,result;
	//struct timespec start_timestamp,end_timestamp;
	struct timespec sleepytime;
	sleepytime.tv_sec=0;
	sleepytime.tv_nsec=100;
	//double ipc_time;
	int status;
	static uint init_val;
	
	char * name = malloc(sizeof(char)*255);
	
	init_val=may_start++;
	if(init_val % 2 == 1)
		init_val=init_val-1;
	do{
		sched_yield();
	}  while(may_start-init_val!=2);
	init_val++;
	init_val++;
	switch(coport_type)
	{
		case COCARRIER:
			sprintf(name,"/root/COCARRIER_cosend_b%lu_t%lu.dat",message_len,total_size);
			break;
		case COPIPE:
			sprintf(name,"/root/COPIPE_cosend_b%lu_t%lu.dat",message_len,total_size);
			break;
		case COCHANNEL:
			sprintf(name,"/root/COCHANNEL_cosend_b%lu_t%lu.dat",message_len,total_size);
			break;
		default:
			break;
	}
	
	FILE * f;
	status=coopen(port_name,coport_type,&port);

	if(coport_type!=COCARRIER)
	{
		set_sched();
		if(pthread_mutex_trylock(&start_lock))
		{
			pthread_cond_signal(&may_continue);
		}
		else
		{
			if(may_start2!=1)
			{
				may_start2=1;
				pthread_cond_wait(&may_continue,&start_lock);
				may_start2=0;
			}
			else
			{
				pthread_cond_signal(&may_continue);		
			}
		}
		pthread_mutex_unlock(&start_lock);
	}
	for(unsigned long i = 0; i<runs; i++){
		
		if(coport_type!=COCARRIER){
			sched_yield();
		}
		if(coport_type==COCARRIER)
		{
			while(pthread_mutex_trylock(&async_lock))
			{
				sched_yield();
			}
			error=pthread_mutex_lock(&output_lock); //keep it quiet during
			if(error)
				err(error,"send_data: lock output_lock failed");
		}
		if (trace)
		{
			intval=1;
			sysarch(QEMU_SET_QTRACE, &intval);
			CHERI_START_TRACE;
		}
		
		statcounters_zero(&send_start);
		statcounters_zero(&bank2);
		statcounters_zero(&result);
		
		if (coport_type!=COCHANNEL)
		{
			for(unsigned int j = 0; j<(total_size/message_len); j++)
			{
				status=cosend(port,message_str,message_len);
			}
		}
		else
		{
			for(unsigned int j = 0; j<(total_size/4096); j++)
			{
				status=cosend(port,message_str,4096);
			}
		}
		statcounters_sample(&send_start);
		if (coport_type!=COCHANNEL)
		{
			for(unsigned int j = 0; j<(total_size/message_len); j++)
			{
				status=cosend(port,message_str,message_len);
			}
		}
		else
		{
			for(unsigned int j = 0; j<(total_size/4096); j++)
			{
				status=cosend(port,message_str,4096);
			}
		}
		statcounters_sample(&bank2);
		if (trace)
		{
			intval=0;
			sysarch(QEMU_SET_QTRACE, &intval);
			CHERI_START_TRACE;
		}
		//clock_gettime(CLOCK_REALTIME,&end_timestamp);

		if(coport_type==COCARRIER)
		{
			pthread_mutex_unlock(&async_lock);
		}
		if(!multicore)
			sched_yield();
		error=pthread_mutex_lock(&output_lock);
		if(error && coport_type!=COCARRIER)
			err(error,"send_data: lock failed");
		if(status==-1)
		{
			if(errno==EBUSY)
			{
				perror("cosend: err in cosend");
			}
			else
			{
				perror("cosend: err in cosend");
			}
			err(errno,"error!");
		}
		//timespecsub(&end_timestamp,&start_timestamp);
		statcounters_diff(&result,&bank2,&send_start);

		
		//ipc_time=(float)end_timestamp.tv_sec + (float)end_timestamp.tv_nsec / 1000000000;
		
		//printf("Sent %lu bytes in %lf\n", total_size, ipc_time);
		//printf("%.2FKB/s\n",(((total_size)/ipc_time)/1024.0));
		printf("Sent %lu bytes\n", total_size);

			
		f = fopen(name,"a+");
		//statcounters_dump_with_args calls fclose
		if(i==0)
			statcounters_dump_with_args(&result,"COSEND","","malta",f,CSV_HEADER);
		else
			statcounters_dump_with_args(&result,"COSEND","","malta",f,CSV_NOHEADER);		
		statcounters_dump(&result);
		
		while(pthread_mutex_unlock(&output_lock));
	}
	unset_sched();
	intval=0;
	if (trace)
		sysarch(QEMU_SET_QTRACE, &intval);
	coclose(port);
	port=NULL;
	//free(name);
	
}

static char * buffer = NULL;
void receive_data(void)
{
	int intval = 1;
	int error = 0;
	coport_t port = (void * __capability)-1;
	
	statcounters_bank_t bank1,result;
	
	//struct timespec start, end;
	//float ipc_time;
	int status;
	static uint init_val;

	char * name = malloc(sizeof(char)*255);
	init_val=may_start++;
		if(init_val % 2 == 1)
			init_val=init_val-1;
	do{
		sched_yield();
	}  while(may_start-init_val!=2);
	init_val++;
	init_val++;
	switch(coport_type)
	{
		case COCHANNEL:
			sprintf(name,"/root/COCHANNEL_corecv_b%lu_t%lu.dat",message_len,total_size);
			
			break;
		case COPIPE:
			sprintf(name,"/root/COPIPE_corecv_b%lu_t%lu.dat",message_len,total_size);
			break;
		case COCARRIER:
			sprintf(name,"/root/COCARRIER_corecv_b%lu_t%lu.dat",message_len,total_size);
			break;
		default:
			break;
	}
	FILE * f;

	
	/*if(port->status==COPORT_CLOSED)
	{
		err(1,"port closed before receiving");
	}*/
	status=coopen(port_name,coport_type,&port);
	
	if(coport_type!=COCARRIER)
	{
		set_sched();
		if(pthread_mutex_trylock(&start_lock))
		{
			pthread_cond_signal(&may_continue);
			sched_yield();
		}
		else
		{
			if(may_start2!=1)
			{
				may_start2=1;
				pthread_cond_wait(&may_continue,&start_lock);
				may_start2=0;
			}
			else
			{
				pthread_cond_signal(&may_continue);	
			}
		}
		pthread_mutex_unlock(&start_lock);
	}

	for(unsigned long i = 0; i<runs; i++)
	{
		if(coport_type==COCARRIER && trace)
		{
			intval=1;
			sysarch(QEMU_SET_QTRACE, &intval);
		}
		statcounters_zero(&bank1);
		statcounters_zero(&recv_end);
		statcounters_zero(&result);

		if(coport_type==COCARRIER)
		{
			while(pthread_mutex_trylock(&async_lock))
			{		
				sched_yield();
			}
			//if(error)
			//	err(error,"recv_data: lock async_lock failed");
			error=pthread_mutex_lock(&output_lock); //keep it quiet during
			if(error)
				err(error,"recv_data: lock output_lock failed");
		}

		if (coport_type!=COCHANNEL)
		{
			//clock_gettime(CLOCK_REALTIME,&start);
			
			for(unsigned int j = 0; j<(total_size/message_len); j++)
			{
				status=corecv(port,(void **)&buffer,message_len);
			}
			
			statcounters_sample(&bank1);
			for(unsigned int j = 0; j<(total_size/message_len); j++)
			{
				status=corecv(port,(void **)&buffer,message_len);
			}
			statcounters_sample(&recv_end);
			
			//clock_gettime(CLOCK_REALTIME,&end);
			
			/*
			for(unsigned int j = 0; j<(total_size/message_len); j++)
			{
				status=corecv(port,(void **)&buffer,message_len);
			}
			*/
		}
		else
		{
			//clock_gettime(CLOCK_REALTIME,&start);
			
			for(unsigned int j = 0; j<(total_size/4096); j++)
			{
				status=corecv(port,(void **)&buffer,4096);
			}
			statcounters_sample(&bank1);
			for(unsigned int j = 0; j<(total_size/4096); j++)
			{
				status=corecv(port,(void **)&buffer,4096);
			}
			statcounters_sample(&recv_end);
			//clock_gettime(CLOCK_REALTIME,&end);
			
			/*
			for(unsigned int j = 0; j<(total_size/4096); j++)
			{
				status=corecv(port,(void **)&buffer,4096);
			}*/
			
		}
		if (trace)
		{	
			intval=0;
			sysarch(QEMU_SET_QTRACE, &intval);
			CHERI_STOP_TRACE;
		}
		if(coport_type==COCARRIER)
		{
			while(pthread_mutex_unlock(&async_lock));
		}
		if(!multicore)
			sched_yield();
		error=pthread_mutex_lock(&output_lock);
		if(error && coport_type!=COCARRIER)
			err(error,"recv_data: lock output_lock failed");

		if(status==-1)
		{
			perror("corecv: err in corecv");
			err(errno,"corecv: error!");
		}
		
		if(buffer==NULL)
		{
			err(1,"buffer not written to");
		}
		//printf("message received:%s\n",(char *)buffer);
		//timespecsub(&end,&start);
		statcounters_diff(&result,&recv_end,&bank1);
		
		//ipc_time=(float)end.tv_sec + (float)end.tv_nsec / 1000000000;
		//printf("Received %lu bytes in %lf\n", total_size, ipc_time);
		printf("Received %lu bytes\n", total_size);


		//printf("%.2FKB/s\n",(((total_size)/ipc_time)/1024.0));

		f= fopen(name,"a+");
		if(i==0)
			statcounters_dump_with_args(&result,"CORECV","","malta",f,CSV_HEADER);
		else
			statcounters_dump_with_args(&result,"CORECV","","malta",f,CSV_NOHEADER);
		statcounters_dump(&result);

		switch(coport_type)
		{
			case COCARRIER:
				pthread_mutex_unlock(&output_lock);
				continue;
				break;
			case COCHANNEL:
				sprintf(name,"/root/COCHANNEL_wholeop_b%lu_t%lu.dat",message_len,total_size);
				break;
			case COPIPE:
				sprintf(name,"/root/COPIPE_wholeop_b%lu_t%lu.dat",message_len,total_size);
				break;
			
			default:
				break;
		}
		f = fopen(name,"a+");
		statcounters_diff(&result,&recv_end,&send_start);
		if(i==0)
			statcounters_dump_with_args(&result,"CORECV","","malta",f,CSV_HEADER);
		else
			statcounters_dump_with_args(&result,"CORECV","","malta",f,CSV_NOHEADER);
	
		while(pthread_mutex_unlock(&output_lock));
		coclose(port);
	}
	
	if(coport_type!=COCARRIER)
	{
		unset_sched();
	}
	may_start=0;
	port=NULL;
	//free(buffer);
	//free(name);
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
		while(message_remaining)
		{
			to_copy=MIN(message_remaining,1024);
			memcpy(message_str+data_copied,one_k_str,to_copy);
			data_copied+=to_copy;
			message_remaining-=to_copy;
		}
		
	}
	message_str[message_len-1]='\0';
	mlock(message_str,message_len);
	buffer=calloc(message_len,sizeof(char));
	mlock(buffer,message_len);

}

static int done = 0;


static
void *do_recv(void* args)
{
	//int error = 0;
	statcounters_reset();
	receive_data();

	receive_data();
	done++;
	exit(0);
	return args;
}



static
void *do_send(void* args)
{	
	//int error = 0;

	statcounters_reset();
	coport_type=COPIPE;
	port_name=malloc(strlen(port_names[1])+1);
	strcpy(port_name,port_names[0]);

	pthread_mutex_lock(&async_lock);
	
	//if(!multicore)
	sched_yield();
	

	//prepare_message();
	
	send_data();
	switching_types++;

	strcpy(port_name,port_names[1]);
	coport_type=COCARRIER;

	

	send_data();
	done++;
	for(;;)
		sched_yield();
	return args;
}

/*
static
void *manage_locks(void *args)
{
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
	return args;	
}
*/
static cpuset_t send_cpu_set = CPUSET_T_INITIALIZER(CPUSET_FSET);
static cpuset_t recv_cpu_set = CPUSET_T_INITIALIZER(CPUSET_FSET);;

int main(int argc, char * const argv[])
{
	int opt;
	char * strptr;
	
	int receive = 0;
	
	
	while((opt=getopt(argc,argv,"ot:r:b:pqc:"))!=-1)
	{
		switch(opt)
		{
			case 'o':
				break;
			case 'q':
				trace=1;
				break;
			case 'b':
				message_len = strtol(optarg, &strptr, 10);
				if (*optarg == '\0' || *strptr != '\0' || message_len <= 0)
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
	if(message_len == 0)
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
	
	pthread_attr_init(&send_attr);
	pthread_attr_init(&recv_attr);

	struct timespec sleepytime;
	sleepytime.tv_sec=0;
	sleepytime.tv_nsec=100;

	
    int mib[4];
    int cores;
    size_t len = sizeof(cores); 
    pthread_mutexattr_t type;
    /* set the mib for hw.ncpu */
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;

    /* get the number of CPUs from the system */
    sysctl(mib, 2, &cores, &len, NULL, 0);
    if (cores>1)
        multicore=1;

    if (multicore)
    {
		if(pthread_getaffinity_np(pthread_self(),sizeof(cpuset_t),&send_cpu_set))
			err(errno,"couldn't get affinity");
		if(pthread_getaffinity_np(pthread_self(),sizeof(cpuset_t),&recv_cpu_set))
			err(errno,"couldn't get affinity");
		CPU_CLR(CPU_FFS(&recv_cpu_set)-1,&recv_cpu_set);
		CPU_CLR(CPU_FFS(&recv_cpu_set)-1,&send_cpu_set);
		if(pthread_attr_setaffinity_np(&send_attr, sizeof(cpuset_t), &send_cpu_set))
			err(errno,"error setting affinity for send thread");
		if(pthread_attr_setaffinity_np(&recv_attr, sizeof(cpuset_t), &recv_cpu_set))
			err(errno,"error setting affinity for recv thread");
	}
	
	sched_params.sched_priority = sched_get_priority_max(SCHED_FIFO);
	if(sched_params.sched_priority == -1)
		perror("could not get sched priority max SCHED_FIFO");
	/*
	if(pthread_attr_setschedpolicy(&send_attr,SCHED_RR))
		err(errno,"error setting sched policy for send");
	if(pthread_attr_setschedpolicy(&recv_attr,SCHED_RR))
		err(errno,"error setting sched policy for recv");

	if(pthread_attr_setschedparam(&send_attr,&sched_params))
		err(errno,"error setting sched params for send");
	if(pthread_attr_setschedparam(&recv_attr,&sched_params))
		err(errno,"error setting sched params for recv");
	if(pthread_attr_setinheritsched(&send_attr,PTHREAD_EXPLICIT_SCHED))
		err(errno,"error setting sched inherit explicit");
	if(pthread_attr_setinheritsched(&recv_attr,PTHREAD_EXPLICIT_SCHED))
		err(errno,"error setting sched inherit explicit");
	*/
	//sched_params.sched_priority = sched_get_priority_max(SCHED_FIFO);
	//pthread_setschedparam(pthread_self(),SCHED_FIFO,&sched_params);
	if(pthread_mutexattr_init(&type))
		err(errno,"could not init type");

	if(pthread_mutexattr_settype(&type,PTHREAD_MUTEX_ERRORCHECK))
		err(errno,"could not set type PTHREAD_MUTEX_ERRORCHECK");

	pthread_mutex_init(&output_lock,&type);
	pthread_mutex_init(&start_lock,&type);

	pthread_mutexattr_settype(&type,PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&async_lock,&type);
	pthread_cond_init(&may_continue,NULL);
	if(pthread_cond_init(&waiting,NULL))
		err(errno,"error initing waiting");
	
	
	//pthread_setschedparam(pthread_self(),SCHED_FIFO,&sched_params);
	//if(pthread_mutex_lock(&start_lock))
	//	err(errno,"error locking startlock");
	//if(pthread_create(&lock_mgr,&send_attr,manage_locks,NULL))
	//	err(errno,"error starting lock manager");
	//pthread_mutex_unlock(&start_lock);
	pthread_mutex_lock(&start_lock);
	prepare_message();
	if(pthread_create(&sender,&send_attr,do_send,NULL))
		err(errno,"error starting send");
	else
		perror("started send");
	if(pthread_create(&receiver,&recv_attr,do_recv,NULL))
		err(errno,"error starting recv");
	else
		perror("started recv");

	pthread_cond_wait(&waiting,&start_lock);
	pthread_join(receiver,NULL);
	return 0;
}