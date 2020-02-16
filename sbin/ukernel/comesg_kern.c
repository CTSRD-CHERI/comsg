#include "comesg_kern.h"

#include <err.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "coport.h"
#include "comsg.h"
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
	for (int i = 0; i < len-1; i++)
	{
		rand_no=random() % KEYSPACE;
		c=(char)rand_no+0x21;
		s[i]=c;
	}
	s[len-1]=0;
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

int lookup_port(char * port_name,coport_t * port_buf)
{
	if (strlen(port_name)>COPORT_NAME_LEN)
	{
		err(1,"port name length too long");
	}
	for(int i = 0; i<coport_table.index;i++)
	{
		if(strcmp(port_name,coport_table.table[i].name)==0)
		{
			port_buf->buffer=coport_table.table[i].buffer;
			return 0;
		}
	}
	return 1;
}


void *coport_open(void *args)
{
	coopen_data_t *data = (coopen_data_t *)args;
	cocall_coopen_t coport_args;
	coport_tbl_entry_t table_entry;
	coport_t port;

	int error;
	int lookup;

	void * __capability switcher_code;
	void * __capability switcher_data;
	void * __capability caller_cookie;

	error=coaccept_init(switcher_code,switcher_data,data->name);
	
	for (;;)
	{
		error=coaccept(switcher_code,switcher_data,&caller_cookie,&coport_args,sizeof(coport_args));
		/* check args are acceptable */

		/* check if port exists */
		lookup=lookup_port(coport_args.args.name,&port);
		if(lookup==1)
		{
			/* if it doesn't, set up coport */
			error=init_port(coport_args.args.name,coport_args.args.type,&table_entry);
			error=add_port(&table_entry);
			if(error!=0)
			{
				err(1,"unable to init_port");
			}
			port.buffer=table_entry.buffer;
		}	
		coport_args.port=port;
	}
	return 0;
}

void manage_coopen_requests()
{
	int error;

	void * __capability sw_code;
	void * __capability sw_data;
	void * __capability cookie;

	cocall_lookup_t lookup;

	error=coaccept_init(sw_code,sw_data,U_COOPEN);
	for(;;)
	{
		for(int i = 0; i < WORKER_COUNT; i++)
		{
			error=coaccept(sw_code,sw_data,&cookie,&lookup,sizeof(lookup));
			error=colookup(worker_strings[i].name,lookup.target);
		}
	}

}

int coaccept_init(void * __capability code_cap,void * __capability data_cap, char * target_name)
{
	int error;
	error=cosetup(COSETUP_COACCEPT,&code_cap,&data_cap);
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

int spawn_workers(void * __capability func, pthread_t * threads)
{
	int e;
	/* split into threads */
	threads=(pthread_t *) malloc(WORKER_COUNT*sizeof(pthread_t));
	
	char thread_name[THREAD_STRING_LEN];
	for (int i = 0; i < WORKER_COUNT; i++)
	{
		pthread_t thread;
		pthread_attr_t thread_attrs;

		rand_string(thread_name,THREAD_STRING_LEN);
		strcpy(worker_strings[i].name,thread_name);
		printf("%s",thread_name);
		e=pthread_attr_init(&thread_attrs);
		e=pthread_create(&thread,&thread_attrs,func,thread_name);
		if (e==0)
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
	
	/* perform setup */
	printf("Spawning co-open listeners...\n");
	error=spawn_workers(&coport_open,coopen_threads);


	/* listen for coopen requests */
	manage_coopen_requests();

	return 0;
}