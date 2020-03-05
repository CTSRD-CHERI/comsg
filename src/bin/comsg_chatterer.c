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

static const char * message_str = "come here!";
static const char * port_name = "benchmark_port";
static coport_t * port;

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

	int status,msg_len;
	unsigned int message_start;

	status=coopen(port_name,coport_type,&port);
	msg_len=strlen(message_str)+1;
	if(port->status==COPORT_CLOSED)
	{
		err(1,"port closed before sending");
	}
	clock_gettime(CLOCK_REALTIME,&start_timestamp);
	cosend(port,message_str,msg_len);
	clock_gettime(CLOCK_REALTIME,&end_timestamp);
	message_start=port->start;
	timespecsub(&end_timestamp,&start_timestamp);
	printf("transferred %lu bytes in %jd.%09jds\n", strlen(message_str), (intmax_t)end_timestamp.tv_sec, (intmax_t)end_timestamp.tv_nsec);
	while (port->start==message_start)
	{
		sleep(1);
	}
}

void receive_data(void)
{
	static char * buffer = NULL;
	struct timespec start, end;

	int status,msg_len;

	status=coopen(port_name,coport_type,&port);
	switch(port->type)
	{
		case COCHANNEL:
			buffer=calloc(strlen(message_str)+1,sizeof(char));
			//must include null character
			msg_len=strlen(message_str)+1;
			break;
		case COCARRIER:
			buffer=calloc(1,CHERICAP_SIZE);
			msg_len=CHERICAP_SIZE;
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
	corecv(port,(void **)&buffer,msg_len);
	clock_gettime(CLOCK_REALTIME,&end);
	if(buffer==NULL)
	{
		err(1,"buffer not written to");
	}
	printf("message received:%s\n",(char *)buffer);
	timespecsub(&end,&start);
	printf("transferred %lu bytes in %jd.%09jds\n", strlen(buffer), (intmax_t)end.tv_sec, (intmax_t)end.tv_nsec);
	if(port->type!=COCARRIER) free(buffer);
}	

int main(int argc, char * const argv[])
{
	int opt;
	while((opt=getopt(argc,argv,"srt:"))!=-1)
	{
		switch(opt)
		{
			case 's':
				chatter_operation=COSEND;
				break;
			case 'r':
				chatter_operation=CORECV;
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