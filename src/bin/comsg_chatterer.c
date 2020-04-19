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

static char * message_str;
static const char * one_k_str = "SIqfubhhJOVdGGMvw09vqHs7S8miUyi1JBaFNGbtKYy4vUs6QeB1JdrMAlWOcC5llzZ5XogADMOvIyNP9R0deF6Coi8RDsf1HUQFVZXYskgmUODJb0uB88DkY2h2qS1dMEX06tTaUWCTDOwRWt9qtgtD8OfBH0uKp6ABwt5vbCjchd7npp12jXBDdAzzkC81DOTdYGMsuuZ6iqMQt0CwkesUj4HSJ7exSaD5hQn47hpr2cinaATAlmfd8G1oKlYWCcXszmGAPkZm4qpE6lA51dTMNmR9kXvnONnMFWesrjI8XmA7qss71oSUgIu10WCnJ7YJA97lg40fCQ647lZCZKdsqGF7XZAgJEkAwSmZ7apdVK4zmlK8JXkdKCuecHxEJk3NDLdN83qvonYiJE7aoZHmibjwHMiJDAtmPKlaJBnKS5yLNXRExHLH5GXvjrvRIdDzCtQZStt4ZW8PickMMcDczSyP7Kr0OwjPaX3dSgU6PFRX3hKbYGyXx28VdzeAZ2ynvv1b5i13Hg9xW6oeidFVFw0SsuTg9gVmbYRr9F20LdDxGaJBMofsX6SbHx66JmtsgztP0DWAQxxviSRlUBi8fYvgxHqRfyYyGEFi5V1GMbPtpIKB6EZ2ixOt63VcXTK2egU4dzDcOlgognDz8253LFn0e02hNRX0nRRkamJ0xMkS9tzBW8NhdxG2iQ0zWbAyHzPWmFYQOvrXPm0u5yS2drByXmz5y9S8LvnBAZ1vlaLAylyMIxy8z6clpCIZqTovP3X0Eg6xPuLg4xQpXvxwx3U2WryMcDWRmAJWW7XL6JfzipyZTI9GGPiNp73Hs9CwSlQMpJDh9ByvzsWKmDKm3YUHLDwqe7XdmBbQfOkEKyjrQPr10TvNA0euXw0TTu6dmziZLSGrLv1DFLcuxzQ9CZkg1bkFX8RUzREjG8NmdGa0YLRdru2FWLqC1rCG6c3aD2qp2v0SuVUlJe9qj5aUsFjOlq5s9XGJJepCb6TTeHKP8jsHL7jnJQXIR9O0";
static unsigned long int message_len = 10;
static unsigned long int runs = 1;
static unsigned long int total_size = 10;
static const char * port_name = "benchmark_port";
static coport_t port = (void * __capability)-1;

//static const char * message_str = "";

static coport_op_t chatter_operation = COSEND;
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
	struct timespec start_timestamp,end_timestamp;
	double ipc_time;
	int status;
	unsigned int message_start;

	status=coopen(port_name,coport_type,&port);
	message_len=strlen(message_str)+1;
	if(port->status==COPORT_CLOSED)
	{
		err(1,"port closed before sending");
	}
	clock_gettime(CLOCK_REALTIME,&start_timestamp);
	for(unsigned int j = 0; j<runs; j++)
	{
		cosend(port,message_str,message_len);
	}
	clock_gettime(CLOCK_REALTIME,&end_timestamp);
	message_start=port->start;
	timespecsub(&end_timestamp,&start_timestamp);
	ipc_time=(float)end_timestamp.tv_sec + (float)end_timestamp.tv_nsec / 1000000000;
	printf("transferred %lu bytes in %lf\n", total_size, ipc_time);
	printf("%.2FKB/s\n",(((total_size)/ipc_time)/1024.0));
	if(coport_type==COCARRIER)
	{
		while (port->start==message_start)
		{
			sleep(1);
		}
	}
}

void receive_data(void)
{
	static char * buffer = NULL;
	struct timespec start, end;
	float ipc_time;
	int status;

	status=coopen(port_name,coport_type,&port);
	switch(port->type)
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
	if(port->status==COPORT_CLOSED)
	{
		err(1,"port closed before receiving");
	}
	clock_gettime(CLOCK_REALTIME,&start);
	for(unsigned int j = 0; j<runs; j++)
	{
		corecv(port,(void **)&buffer,message_len);
	}
	clock_gettime(CLOCK_REALTIME,&end);
	if(buffer==NULL)
	{
		err(1,"buffer not written to");
	}
	//printf("message received:%s\n",(char *)buffer);
	timespecsub(&end,&start);
	ipc_time=(float)end.tv_sec + (float)end.tv_nsec / 1000000000;
	printf("transferred %lu bytes in %lf\n", total_size, ipc_time);
	printf("%.2FKB/s\n",(((total_size)/ipc_time)/1024.0));

	//if(port->type!=COCARRIER) free(buffer);
}	

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
}