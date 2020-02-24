#include "comsg.h"
#include "coport.h"
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>

const char * port_name = "benchmark_port";
const char * ts_port_name = "timestamp_port";

#define	timespecsub(vvp, uvp)						\
	do {								\
		(vvp)->tv_sec -= (uvp)->tv_sec;				\
		(vvp)->tv_nsec -= (uvp)->tv_nsec;			\
		if ((vvp)->tv_nsec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_nsec += 1000000000;			\
		}							\
	} while (0)

void send_data()
{
	u_int * buf;
	struct timespec start_timestamp;

	coport_t port;
	int status;

	buf=(u_int *)malloc(4096/sizeof(u_int));

	status=coopen(port_name,COCHANNEL,&port);
	buf[0]=0;

	clock_gettime(CLOCK_REALTIME,&start_timestamp);
	cosend(&port,buf,4096);
}

void send_timestamp(struct timespec * timestamp)
{
	coport_t port;
	int status;

	status=coopen(ts_port_name,COCHANNEL,&port);
	cosend(&port,timestamp,sizeof(timestamp));
	
}

void receive_data()
{
	u_int * buf;
	struct timespec start,end;

	coport_t port;
	int status;

	buf=(u_int *)malloc(4096/sizeof(u_int));

	status=coopen(port_name,COCHANNEL,&port);

	clock_gettime(CLOCK_REALTIME,&start);
	corecv(&port,buf,4096);
	clock_gettime(CLOCK_REALTIME,&end);
}

void receive_timestamp();

int main(int argc, char const *argv[])
{
	pid_t pid;

	if(strcmp(argv[1],"-r"))
	{
		receive_data();

	}
	else if (strcmp(argv[1],"-s"))
	{
		send_data();
	}
	else
	{
		err(1,"invalid options");
	}

	return 0;
}