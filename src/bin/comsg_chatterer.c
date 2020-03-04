#include "comsg.h"
#include "coport.h"
#include <inttypes.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <stdio.h>

static const char * message_str = "come here!";
static const char * port_name = "benchmark_port";
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

static void send_data()
{
	struct timespec start_timestamp,end_timestamp;

	coport_t * port;
	int status;

	status=coopen(port_name,COCARRIER,&port);

	clock_gettime(CLOCK_REALTIME,&start_timestamp);
	cosend(&port,message_str,strlen(message_str));
	clock_gettime(CLOCK_REALTIME,&end_timestamp);
	timespecsub(&end_timestamp,&start_timestamp);
	printf("transferred %lu bytes in %jd.%09jds\n", strlen(message_str), (intmax_t)end_timestamp.tv_sec, (intmax_t)end_timestamp.tv_nsec);
}
/*
static void send_timestamp(struct timespec * timestamp)
{
	coport_t port;
	int status;

	status=coopen(ts_port_name,COCHANNEL,&port);
	cosend(&port,timestamp,sizeof(timestamp));
	
}
	*/
	static void receive_data()
{
	char * buf;
	struct timespec start,end;

	coport_t * port;
	int status;

	status=coopen(port_name,COCARRIER,&port);

	clock_gettime(CLOCK_REALTIME,&start);
	corecv(&port,(void **)&buf,16);
	clock_gettime(CLOCK_REALTIME,&end);
	printf("message received:%s",buf);
}

int main(int argc, char const *argv[])
{
	if(argc==0)
	{
		err(1,"must supply either -s or -r");
	}

	if(strcmp(argv[1],"-r")==0)
	{
		receive_data();

	}
	else if (strcmp(argv[1],"-s")==0)
	{
		send_data();
	}
	else
	{
		err(1,"invalid options");
	}

	return 0;
}