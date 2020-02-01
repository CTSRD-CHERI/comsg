#include "comesg_kern.h"

#include <err.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>


#include "coport.h"
#include "sys_comsg.h"


coopen_data_t worker_strings[WORKER_COUNT];

unsigned int next_port_index = 0;
coport_tbl_t coport_table;

int rand_string(char * buf,unsigned int len)
{
	char c;
	char * s;
	int rand_no;
	s = (char *) malloc(sizeof(char)*len);
	for (int i = 0; i < len; i++)
	{
		rand_no=random() % KEYSPACE;
		c=(char)rand_no+0x21;
		s[i]=c;
	}
	strcpy(buf,s);
	free(s);
	return 0;
}

int add_port(coport_tbl_entry_t * entry)
{
	if(coport_table.index==MAX_COPORTS)
	{
		return 1;
	}

	pthread_mutex_lock(&coport_table.lock);
	memcpy(&coport_table.table[coport_table.index],entry,sizeof(coport_tbl_entry_t));
	coport_table.index++;
	pthread_mutex_unlock(&coport_table.lock);

	return 0;
}

int lookup_port(char * port_name)
{
	if (strlen(port_name)>COPORT_NAME_LEN)
	{
		err(1,"port name length too long");
	}
	return 0;
}


void *coport_open(void *args)
{
	coopen_data_t *data = (coopen_data_t *)args;
	cocall_coopen_t coport_args;
	coport_tbl_entry_t table_entry;
	coport_t port;

	int error;

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
		error=init_port(coport_args.args.name,coport_args.args.type,&table_entry);

		

		error=add_port(&table_entry);
		if(error!=0)
		{
			err(1,"unable to init_port");
		}
		port.buffer=table_entry.buffer;
		coport_args.port=port;
	}
	return 0;
}

int coaccept_init(void * __capability switcher_code,void * __capability switcher_data, char * target_name)
{
	int error;
	error=cosetup(COSETUP_COACCEPT,&switcher_code,&switcher_data);
	if (error!=0)
	{
		err(1,"could not cosetup");
	}
	error=coregister(target_name,0);
	if (error!=0)
	{
		err(1,"could not coregister");
	}
	return 0;
}

void *coport_connect(void *args)
{
	coopen_data_t * data = (coopen_data_t *)args;
	return 0;
}

int coport_tbl_setup()
{
	int error=pthread_mutex_init(&coport_table.lock,0);
	coport_table.index=0;
	/* reserve a superpage or two for this, entries should be small */
	/* reserve a few superpages for ports */
	return 0;
}

int spawn_workers(int func, pthread_t * worker_array)
{
	int error;
	/* split into threads */
	pthread_t threads[WORKER_COUNT];
	pthread_t thread;
	pthread_attr_t thread_attrs;
	char thread_name[THREAD_STRING_LEN];
	printf("Starting.");
	for (int i = 0; i < WORKER_COUNT; i++)
	{
		error=rand_string(thread_name,THREAD_STRING_LEN);
		strcpy(worker_strings[i].name,thread_name);
		if(func==1)
		{
			error=pthread_create(&thread,&thread_attrs,&coport_open,&worker_strings[i]);

		}
		if (error==0)
		{
			threads[i]=thread;
		}
	}
	return 0;
}

int main(int argc, const char *argv[])
{
	int error;
	pthread_t coopen_threads[WORKER_COUNT];

	/* check we're in a colocated address space */

	/* we can only have one handler per cocall string */
	/*  */
	/* set up table */
	printf("Starting comesg microkernel...\n");
	error=coport_tbl_setup();
	printf("Table setup complete.\n");
	
	printf("Spawning co-open listeners...\n");
	error=spawn_workers(1,coopen_threads);

	

	/* perform setup */

	return 0;
}