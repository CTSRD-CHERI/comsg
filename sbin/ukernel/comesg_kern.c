#include <err.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "coport.h"
#include "sys_comsg.h"
#include "comesg_kern.h"

#define WORKER_COUNT 1
#define LOOKUP_STRING_LEN 16
#define KEYSPACE 93

static coopen_data_t worker_strings[WORKER_COUNT];

int rand_string(char * buf)
{
	char c;
	char s[LOOKUP_STRING_LEN];
	srandomdev();
	for (int i = 0; i < LOOKUP_STRING_LEN; i++)
	{
		c=(srandom() % KEYSPACE)+"!";
		s[i]=c;
	}
	strcpy(buf,s);
	return 0;
}

int generate_random_strings(char * buf)
{
	char new_string[LOOKUP_STRING_LEN];
	for (int i = 0; i < WORKER_COUNT; i++)
	{
		err=rand_string(new_string);
		worker_strings[i].name=new_string;
	}
	return 0;
}

void *coopen_worker(void *args)
{
	coopen_data_t *data = (coopen_data_t *)args;
	char coport_name[COPORT_NAME_LEN];
	void * __capability switcher_code;
	void * __capability switcher_data;
	void * __capability caller_cookie;

	error=cosetup(COSETUP_COREGISTER,&switcher_code,&switcher_data);
	if (error!=0)
	{
		err(1,"could not cosetup");
	}
	error=coregister(data->name,NULL);
	if (error!=0)
	{
		err(1,"could not coregister");
	}

	for (;;)
	{
		error=coaccept(switcher_code,switcher_data,&caller_cookie,&coport_name,sizeof(coport_name));
		/* do stuff to set up coport */
	}
}

int main(int argc, char const *argv[])
{

	/* check we're in a colocated address space */

	/* we can only have one handler per cocall string */
	/*  */

	/* split into threads */
	pthread_t worker_threads[WORKER_COUNT];
	pthread_t worker;
	pthread_attr_t worker_attrs;
	for (int i = 1; i <= WORKER_COUNT; i++)
	{
		err=pthread_create(&worker,&worker_attrs,coopen_worker,&worker_strings[i-1]);
		if (err=0)
		{
			worker_threads[i-1]=worker;
		}
	}

	/* perform setup */

	return 0;
}