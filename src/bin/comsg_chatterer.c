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

#include "comsg.h"
#include "coport.h"



#include <inttypes.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <statcounters.h>

extern char **environ;

static char * message_str;
static const char * one_k_str = "SIqfubhhJOVdGGMvw09vqHs7S8miUyi1JBaFNGbtKYy4vUs6QeB1JdrMAlWOcC5llzZ5XogADMOvIyNP9R0deF6Coi8RDsf1HUQFVZXYskgmUODJb0uB88DkY2h2qS1dMEX06tTaUWCTDOwRWt9qtgtD8OfBH0uKp6ABwt5vbCjchd7npp12jXBDdAzzkC81DOTdYGMsuuZ6iqMQt0CwkesUj4HSJ7exSaD5hQn47hpr2cinaATAlmfd8G1oKlYWCcXszmGAPkZm4qpE6lA51dTMNmR9kXvnONnMFWesrjI8XmA7qss71oSUgIu10WCnJ7YJA97lg40fCQ647lZCZKdsqGF7XZAgJEkAwSmZ7apdVK4zmlK8JXkdKCuecHxEJk3NDLdN83qvonYiJE7aoZHmibjwHMiJDAtmPKlaJBnKS5yLNXRExHLH5GXvjrvRIdDzCtQZStt4ZW8PickMMcDczSyP7Kr0OwjPaX3dSgU6PFRX3hKbYGyXx28VdzeAZ2ynvv1b5i13Hg9xW6oeidFVFw0SsuTg9gVmbYRr9F20LdDxGaJBMofsX6SbHx66JmtsgztP0DWAQxxviSRlUBi8fYvgxHqRfyYyGEFi5V1GMbPtpIKB6EZ2ixOt63VcXTK2egU4dzDcOlgognDz8253LFn0e02hNRX0nRRkamJ0xMkS9tzBW8NhdxG2iQ0zWbAyHzPWmFYQOvrXPm0u5yS2drByXmz5y9S8LvnBAZ1vlaLAylyMIxy8z6clpCIZqTovP3X0Eg6xPuLg4xQpXvxwx3U2WryMcDWRmAJWW7XL6JfzipyZTI9GGPiNp73Hs9CwSlQMpJDh9ByvzsWKmDKm3YUHLDwqe7XdmBbQfOkEKyjrQPr10TvNA0euXw0TTu6dmziZLSGrLv1DFLcuxzQ9CZkg1bkFX8RUzREjG8NmdGa0YLRdru2FWLqC1rCG6c3aD2qp2v0SuVUlJe9qj5aUsFjOlq5s9XGJJepCb6TTeHKP8jsHL7jnJQXIR9O0";
static unsigned long int message_len = 10;
static unsigned long int runs = 1;
static unsigned long int total_size = 10;
static const char * port_names[] = { "benchmark_portA", "benchmark_portB", "benchmark_portC" };
static char * port_name;

static coport_t port = (void * __capability)-1;

//static const char * message_str = "";

//static coport_op_t chatter_operation = COSEND;
static coport_type_t coport_type = COCARRIER;
//static const char * ts_port_name = "timestamp_port";

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

/*
static void send_timestamp(struct timespec * timestamp)
{
	int status;

	status=coopen(ts_port_name,COCHANNEL,&port);
	cosend(port,timestamp,sizeof(timestamp));
	
}
*/
void send_data(void)
{
	statcounters_bank_t bank1,bank2,result;
	struct timespec start_timestamp,end_timestamp;
	double ipc_time;
	int status;

	status=coopen(port_name,coport_type,&port);
	message_len=strlen(message_str)+1;
	/*if(port->status==COPORT_CLOSED)
	{
		err(1,"port closed before sending");
	}*/
	clock_gettime(CLOCK_REALTIME,&start_timestamp);
	statcounters_sample(&bank1);
	for(unsigned int j = 0; j<(total_size/message_len); j++)
	{
		cosend(port,message_str,message_len);
	}
	statcounters_sample(&bank2);
	clock_gettime(CLOCK_REALTIME,&end_timestamp);
	timespecsub(&end_timestamp,&start_timestamp);
	statcounters_diff(&result,&bank2,&bank1);
	
	ipc_time=(float)end_timestamp.tv_sec + (float)end_timestamp.tv_nsec / 1000000000;
	
	printf("Sent %lu bytes in %lf\n", total_size, ipc_time);
	printf("%.2FKB/s\n",(((total_size)/ipc_time)/1024.0));
	statcounters_dump(&result);
	coclose(port);
	port=NULL;
}

void receive_data(void)
{
	statcounters_bank_t bank1,bank2,result;
	static char * buffer = NULL;
	struct timespec start, end;
	float ipc_time;
	int status;

	status=coopen(port_name,coport_type,&port);
	switch(coport_type)
	{
		case COCHANNEL:
		case COPIPE:
			buffer=calloc(strlen(message_str)+1,sizeof(char));
			//must include null character
			message_len=strlen(message_str)+1;
			break;
		case COCARRIER:
			buffer=calloc(1,CHERICAP_SIZE);
			message_len=CHERICAP_SIZE;
			break;
		default:
			buffer=calloc(4096,sizeof(char));
			break;
	}
	/*if(port->status==COPORT_CLOSED)
	{
		err(1,"port closed before receiving");
	}*/
	clock_gettime(CLOCK_REALTIME,&start);
	statcounters_sample(&bank1);
	for(unsigned int j = 0; j<(total_size/message_len); j++)
	{
		corecv(port,(void **)&buffer,message_len);
	}
	statcounters_sample(&bank2);
	clock_gettime(CLOCK_REALTIME,&end);
	if(buffer==NULL)
	{
		err(1,"buffer not written to");
	}
	//printf("message received:%s\n",(char *)buffer);
	timespecsub(&end,&start);
	statcounters_diff(&result,&bank2,&bank1);
	
	ipc_time=(float)end.tv_sec + (float)end.tv_nsec / 1000000000;
	printf("Received %lu bytes in %lf\n", total_size, ipc_time);
	

	printf("%.2FKB/s\n",(((total_size)/ipc_time)/1024.0));
	statcounters_dump(&result);
	printf("----------------------------------------------------");
	coclose(port);
	port=NULL;
	if(coport_type!=COCARRIER) free(buffer);
}	

static
void prepare_message(void)
{
	unsigned int i, message_remaining, data_copied, to_copy;
	message_str=calloc(message_len+1,sizeof(char));
	if (message_len%1024==0)
	{
		;
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
}


int main(int argc, char * const argv[])
{
	int opt;
	char * strptr;
	pid_t p,pp;
	int receiver = 0;
	port_name=malloc(strlen(port_names[0])+1);
	while((opt=getopt(argc,argv,"ot:r:b:p"))!=-1)
	{
		switch(opt)
		{
			case 'o':
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
			case 'p':
				receiver=1;
				break;
			default:
				break;
		}
	}
	if (total_size % message_len !=0)
	{
		if(total_size!=10 && message_len !=10)
		{
			err(1,"total size must be a multiple of buffer size");
		}
		else
		{
			if(total_size==0)
				total_size=message_len;
			else if (message_len==10)
				message_len=total_size;
		}
	}

	if (!receiver)
	{
		p=fork();
		if (!p)
		{
			pp=getppid();
			char ** new_argv = malloc(sizeof(char *)*(argc+1));
			for(int i = 0; i < argc; i++)
			{
				new_argv[i]=malloc((strlen(argv[i])+1)*sizeof(char));
				strcpy(new_argv[i],argv[i]);
			}
			new_argv[argc]=malloc((strlen("p")+1)*sizeof(char));
			strcpy(new_argv[argc],"p");
			coexecve(pp,new_argv[0],new_argv,environ);
		}
		else
		{
			prepare_message();
			coport_type=COCARRIER;
			strcpy(port_name,port_names[0]);
			for(unsigned int i = 0; i < runs; i++)
				send_data();
			strcpy(port_name,port_names[1]);
			coport_type=COPIPE;
			for(unsigned int i = 0; i < runs; i++)
				send_data();
			strcpy(port_name,port_names[2]);
			coport_type=COCHANNEL;
			for(unsigned int i = 0; i < runs; i++)
				send_data();
		}
	}
	else
	{
		prepare_message();
		coport_type=COCARRIER;
		strcpy(port_name,port_names[0]);
		for(unsigned int i = 0; i < runs; i++)
			receive_data();
		strcpy(port_name,port_names[1]);
		coport_type=COPIPE;
		for(unsigned int i = 0; i < runs; i++)
			receive_data();
		strcpy(port_name,port_names[2]);
		coport_type=COCHANNEL;
		for(unsigned int i = 0; i < runs; i++)
			receive_data();
	}
		
}
/*
int main(int argc, char * const argv[])
{
	int opt;
	char * strptr;
	while((opt=getopt(argc,argv,"srt:i:b:"))!=-1)
	{
		switch(opt)
		{
			case 's':
				chatter_operation=COSEND;
				break;
			case 'r':
				chatter_operation=CORECV;
				break;
			case 'b':
				message_len = strtol(optarg, &strptr, 10);
				if (*optarg == '\0' || *strptr != '\0' || message_len <= 0)
					err(1,"invalid length");
				break;
			case 'i':
				runs = strtol(optarg, &strptr, 10);
				if (*optarg == '\0' || *strptr != '\0' || message_len <= 0)
					err(1,"invalid length");
				break;
			case 't':
				if (strcmp(optarg,"COCARRIER")==0)
				{
					coport_type=COCARRIER;
				}
				else if (strcmp(optarg,"COCHANNEL")==0)
				{
					coport_type=COCHANNEL;
				}
				else if (strcmp(optarg,"COPIPE")==0)
				{
					coport_type=COPIPE;
				}
				else
				{
					err(EINVAL,"Invalid coport type %s",optarg);
				}
				break;
			default:
				break;
		}
	}
	total_size = message_len * runs;
	unsigned int i;
	if (message_len%1024==0)
	{
		message_str=calloc(message_len+1,sizeof(char));
		for(i = 0; i < message_len-1024; i+=1024)
		{
			memcpy(message_str+i,one_k_str,1024);
		}
		strncpy(message_str+i,one_k_str,1023);
	}
	else
	{
		message_str=calloc(message_len+1,sizeof(char));
		strncpy(message_str,one_k_str,message_len);
	}
	if(chatter_operation==CORECV)
	{
		receive_data();
	}
	else if (chatter_operation==COSEND)
	{
		send_data();
	}
	else
	{
		err(1,"invalid options");
	}

	return 0;
}*/