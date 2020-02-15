#include <err.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "coport.h"
#include "sys_comsg.h"
#include "comesg_kern.h"

static coopen_data_t worker_strings[WORKER_COUNT];

extern coport_tbl_t coport_table;
extern unsigned int next_port_index = 0;

int rand_string(char * buf,unsigned int len)
{
	char c;
	char * s;
	s = (char *) malloc(sizeof(char)*len);
	srandomdev();
	for (int i = 0; i < len; i++)
	{
		c=(srandom() % KEYSPACE)+"!";
		s[i]=c;
	}
	strcpy(buf,s);
	free(s);
	return 0;
}

// I was too tired to be trusted when I wrote this
// int generate_random_strings(char * buf)
// {
// 	char new_string[LOOKUP_STRING_LEN];
// 	for (int i = 0; i < WORKER_COUNT; i++)
// 	{
// 		err=rand_string(buf,LOOKUP_STRING_LEN);
// 		err=strcpy(buf,new_string);
// 	}
// 	return 0;
// }

void *coport_open(void *args)
{
	coopen_data_t *data = (coopen_data_t *)args;
	coopen_args_t coport_args;
	coport_t port;

	void * __capability switcher_code;
	void * __capability switcher_data;
	void * __capability caller_cookie;

	error=coaccept_init(switcher_code,switcher_data,data->name);
	
	for (;;)
	{
		error=coaccept(switcher_code,switcher_data,&caller_cookie,&coport_args,sizeof(coport_args));
		/* check args are acceptable */

		/* check if port exists */
		/* set up coport */
		error=init_port(coport_args.args.name,coport_args.args.type,port);
		coport_args.port=port;
	}
}

int coaccept_init(void * __capability switcher_code,void * __capability switcher_data, char * target_name)
{
	error=cosetup(COSETUP_COREGISTER,&switcher_code,&switcher_data);
	if (error!=0)
	{
		err(1,"could not cosetup");
	}
	error=coregister(target_name,NULL);
	if (error!=0)
	{
		err(1,"could not coregister");
	}
	return 0;
}

void *coport_connect(void *args)
{
	coopen_data_t * data = (coopen_data_t *)args;
}

int coport_tbl_setup()
{
	err=pthread_mutex_init(coport_table.lock,NULL);
	coport_table.index=0;
	/* reserve a superpage or two for this, entries should be small */
	/* reserve a few superpages for ports */
	return 0;
}

int spawn_workers(void * func, pthread_t * worker_array)
{
	/* split into threads */
	pthread_t threads[WORKER_COUNT];
	pthread_t thread;
	pthread_attr_t thread_attrs;
	char thread_name[THREAD_STRING_LEN];
	for (int i = 1; i <= WORKER_COUNT; i++)
	{
		err=rand_string(thread_name,strlen(thread_name));
		err=pthread_create(&thread,&thread_attrs,func,&worker_strings[i-1]);
		if (err=0)
		{
			coopen_threads[i-1]=coopener;
		}
	}
}

int main(int argc, const char *argv[])
{
	int err;
	pthread_t coopen_threads[WORKER_COUNT];

	/* check we're in a colocated address space */

	/* we can only have one handler per cocall string */
	/*  */
	/* set up table */
	err=coport_tbl_setup();
	

	err=spawn_workers(coport_open,coopen_threads);

	

	/* perform setup */

	return 0;
}