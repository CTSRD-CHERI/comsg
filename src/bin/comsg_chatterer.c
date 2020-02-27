#include "comsg.h"
#include "coport.h"
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <stdio.h>

#define MESSAGE_STR "come here!"
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
	char * buf;
	struct timespec start_timestamp,end_timestamp;

	coport_t port;
	int status;

	status=coopen(port_name,COCARRIER,&port);

	buf=malloc(sizeof(char)*strlen(MESSAGE_STR));
	strcpy(buf,MESSAGE_STR);
	clock_gettime(CLOCK_REALTIME,&start_timestamp);
	cosend(&port,"come here!",strlen("come here!"));
	clock_gettime(CLOCK_REALTIME,&end_timestamp);

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

	coport_t port;
	int status;

	status=coopen(port_name,COCARRIER,&port);

	clock_gettime(CLOCK_REALTIME,&start);
	corecv(&port,(void **)&buf,30);
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