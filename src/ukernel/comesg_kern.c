#include "comesg_kern.h"

#include <err.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "coport.h"
#include "coport_utils.h"
#include "coproc.h"
#include "sys_comsg.h"


#define DEBUG

worker_map_entry_t worker_map[U_FUNCTIONS];
int next_worker_i = 0;

unsigned int next_port_index = 0;
comutex_tbl_t comutex_table;
coport_tbl_t coport_table;

int generate_id(void)
{
	static int id_counter = 0;
	// TODO: Replace this with something smarter.
	return ++id_counter;
}

int rand_string(char * buf,unsigned int len)
{
	char c;
	char * s;
	int rand_no;
	s = (char *) malloc(sizeof(char)*len);
	srandomdev();
	for (unsigned int i = 0; i < len-1; i++)
	{
		rand_no=random() % KEYSPACE;
		c=(char)rand_no+0x21;
		s[i]=c;
	}
	s[len-1]='\0';
	strcpy(buf,s);
	free(s);
	return 0;
}

int add_port(coport_tbl_entry_t * entry)
{
	int entry_index;
	pthread_mutex_lock(&coport_table.lock);
	if(coport_table.index==MAX_COPORTS)
	{
		pthread_mutex_unlock(&coport_table.lock);
		return 1;
	}
	memcpy(&coport_table.table[coport_table.index],entry,sizeof(coport_tbl_entry_t));
	entry_index=++coport_table.index;
	pthread_mutex_unlock(&coport_table.lock);
	return entry_index;
}

int add_mutex(comutex_tbl_entry_t * entry)
{
	int entry_index;
	
	pthread_mutex_lock(&comutex_table.lock);
	if(comutex_table.index>=MAX_COPORTS)
	{
		pthread_mutex_unlock(&comutex_table.lock);
		return 1;
	}
	memcpy(&comutex_table.table[comutex_table.index],entry,sizeof(comutex_tbl_entry_t));
	entry_index=++comutex_table.index;
	pthread_mutex_unlock(&comutex_table.lock);
	return entry_index;
}

int lookup_port(char * port_name,coport_t ** port_buf)
{
	if (strlen(port_name)>COPORT_NAME_LEN)
	{
		err(1,"port name length too long");
	}
	for(int i = 0; i<coport_table.index;i++)
	{
		if(strcmp(port_name,coport_table.table[i].name)==0)
		{
			*port_buf=&coport_table.table[i].port;
			return 0;
		}
	}
	*port_buf=NULL;
	return 1;
}

int lookup_mutex(char * mtx_name,sys_comutex_t ** mtx_buf)
{
	if (strlen(mtx_name)>COPORT_NAME_LEN)
	{
		err(1,"mtx name length too long");
	}
	for(int i = 0; i<comutex_table.index;i++)
	{
		if (comutex_table.table[i].mtx.user_mtx==NULL)
		{
			*mtx_buf=NULL;
			return 1;
		}
		if(strcmp(mtx_name,comutex_table.table[i].mtx.name)==0)
		{
			*mtx_buf=&comutex_table.table[i].mtx;
			return 0;
		}
	}
	*mtx_buf=NULL;
	return 1;
}

void update_worker_args(worker_args_t * args, const char * function_name)
{
	int i,j;
	for(i = 0; i<=U_FUNCTIONS; i++)
	{
		if (strcmp(worker_map[i].func_name,function_name)==0)
		{
			break;
		}
		else if (i==U_FUNCTIONS)
		{
			err(1,"function name %s not found in map",function_name);
		}
	}
	for(j = 0; j<=WORKER_COUNT;j++)
	{
		if (strcmp(worker_map[i].workers[j].name,args->name)==0)
		{
			worker_map[i].workers[j].cap=args->cap;
			return;
		}
		else if (j==WORKER_COUNT)
		{
			err(1,"worker %s not found in map",args->name);
		}
	}
}

void *coport_open(void *args)
{
	int error;
	int index;
	int lookup;

	worker_args_t * data = args;
	cocall_coopen_t * coport_args;
	coport_tbl_entry_t table_entry;
	coport_t port,*prt;


	char port_name[COPORT_NAME_LEN];
	coport_type_t type;

	void * __capability sw_code;
	void * __capability sw_data;
	void * __capability caller_cookie;
	void * __capability target;

	coport_args=malloc(sizeof(cocall_coopen_t));
	memset(coport_args,0,sizeof(cocall_coopen_t));

	error=coaccept_init(&sw_code,&sw_data,data->name,&target);
	data->cap=target;
	update_worker_args(data,U_COOPEN);
	for (;;)
	{
		error=coaccept(sw_code,sw_data,&caller_cookie,coport_args,sizeof(coport_args));
		/* check args are acceptable */
		strcpy(port_name,coport_args->args.name);
		/* check if port exists */
		lookup=lookup_port(port_name,&prt);
		if(lookup==1)
		{
			/* if it doesn't, set up coport */
			type=coport_args->args.type;
			error=init_port(type,&port);
			table_entry.port=port;
			table_entry.id=generate_id();
			strcpy(table_entry.name,port_name);
			index=add_port(&table_entry);
			if(error!=0)
			{
				err(1,"unable to init_port");
			}
			prt=&coport_table.table[index].port;
		}
		coport_args->port=prt;
	}
	free(coport_args);
	return 0;
}

void *comutex_setup(void *args)
{
	worker_args_t * data=args;
	cocall_comutex_init_t comutex_args;
	comutex_tbl_entry_t table_entry;
	sys_comutex_t * mtx;

	int error;
	int index;
	int lookup;

	void * __capability sw_code;
	void * __capability sw_data;
	void * __capability caller_cookie;
	void * __capability target;
	

	error=coaccept_init(&sw_code,&sw_data,data->name,&target);
	data->cap=target;
	update_worker_args(data,U_COMUTEX_INIT);
	for (;;)
	{
		error=coaccept(sw_code,sw_data,&caller_cookie,&comutex_args,sizeof(comutex_args));
		/* check args are acceptable */

		/* check if mutex exists */
		lookup=lookup_mutex(comutex_args.args.name,&mtx);
		if(lookup==1)
		{
			/* if it doesn't, set up mutex */
			error=sys_comutex_init(comutex_args.args.name,mtx);
			table_entry.mtx=*mtx;
			table_entry.id=generate_id();
			index=add_mutex(&table_entry);
			if(error!=0)
			{
				err(1,"unable to init_port");
			}
		}
		strcpy(comutex_args.mutex->name,mtx->name);
		comutex_args.mutex->mtx=mtx->user_mtx;
		comutex_args.mutex->key=NULL;
		
	}
	return 0;
}

void *comutex_lock(void *args)
{
	worker_args_t * data=args;
	colock_args_t colock_args;
	sys_comutex_t * mtx;
	comutex_t * user_mutex;

	int error;
	int lookup;

	void * __capability sw_code;
	void * __capability sw_data;
	void * __capability caller_cookie;
	void * __capability target;

	error=coaccept_init(&sw_code,&sw_data,data->name,&target);
	data->cap=target;
	update_worker_args(data,U_COLOCK);
	for (;;)
	{
		error=coaccept(sw_code,sw_data,&caller_cookie,&colock_args,sizeof(colock_args));
		/* check args are acceptable */
		// validation
		/* check if mutex exists */
		lookup=lookup_mutex(colock_args.mutex->name,&mtx);
		if(lookup==0)
		{
			user_mutex=colock_args.mutex;
			error=sys_colock(mtx,user_mutex->key);
			mtx->key=user_mutex->key;
		}
		//report errors
	}
	return 0;
}

void *comutex_unlock(void *args)
{
	void * __capability sw_code;
	void * __capability sw_data;
	void * __capability  caller_cookie;
	void * __capability  target;

	worker_args_t * data=args;
	counlock_args_t colock_args;
	sys_comutex_t * mtx;
	comutex_t * user_mutex;

	int error;
	int lookup;

	error=coaccept_init(&sw_code,&sw_data,data->name,&target);
	data->cap=target;
	update_worker_args(data,U_COUNLOCK);
	for (;;)
	{
		error=coaccept(sw_code,sw_data,&caller_cookie,&colock_args,sizeof(colock_args));
		/* check args are acceptable */
		// validation
		/* check if mutex exists */
		lookup=lookup_mutex(colock_args.mutex->name,&mtx);
		if(lookup==0)
		{
			user_mutex=colock_args.mutex;
			error=sys_counlock(mtx,user_mutex->key);
			mtx->key=user_mutex->key;
		}
		//report errors
	}
	return 0;
}

int comutex_deinit(comutex_tbl_entry_t * m)
{
	sys_comutex_t mtx = m->mtx;
	free(mtx.kern_mtx->lock);
	free(mtx.kern_mtx);
	mtx.user_mtx=NULL;

	// remove from table?
	return 0;
}

void *manage_requests(void *args)
{
	int error;
	int workers_index;

	request_handler_args_t * data = args;

	void * __capability sw_code;
	void * __capability sw_data;
	void * __capability cookie;
	void * __capability target;

	worker_args_t * workers;

	cocall_lookup_t * lookup;

	error=coaccept_init(&sw_code,&sw_data,data->func_name,&target);
	workers_index=-1;
	for(int i = 0; i < U_FUNCTIONS; i++)
	{
		if(strcmp(worker_map[i].func_name,data->func_name)==0)
		{
			workers=worker_map[i].workers;
			workers_index=i;
			break;
		}
	}
	if(workers_index==-1)
	{
		err(1,"Function workers not registered");
	}
	lookup=malloc(sizeof(cocall_lookup_t));
	for(;;)
	{
		for(int j = 0; j < WORKER_COUNT; j++)
		{
			//printf("coaccepting for %s\n",data->func_name);
			error=coaccept(sw_code,sw_data,&cookie,lookup,sizeof(cocall_lookup_t));
			//printf("Lookup of %s is size %lu",workers[j].name,sizeof(cocall_lookup_t));
			printf("a:validity: %u\n",cheri_gettag(workers[j].cap));
			lookup->cap=workers[j].cap;
			printf("b:validity: %u\n",cheri_gettag(lookup->cap));

		}
	}
}

int coaccept_init(
	void * __capability * __capability  code_cap,
	void * __capability * __capability  data_cap, 
	char * target_name,
	void * __capability * __capability target_cap)
{
	int error;
	error=cosetup(COSETUP_COACCEPT,code_cap,data_cap);
	if (error!=0)
	{
		err(1,"ERROR: Could not cosetup.\n");
	}
	printf("Attempting to coregister with name %s\n",target_name);

	error=coregister(target_name,target_cap);
	if (error!=0)
	{
		err(1,"ERROR: Could not coregister with name %s.\n",target_name);
	}
	printf("Successfully coregistered with name %s\n",target_name);
	printf("validity: %u\n",cheri_gettag(*target_cap));

	return 0;
}

int coport_tbl_setup(void)
{
	int error=pthread_mutex_init(&coport_table.lock,0);
	coport_table.index=0;
	/* reserve a superpage or two for this, entries should be small */
	/* reserve a few superpages for ports */
	return error;
}
int comutex_tbl_setup(void)
{
	int error=pthread_mutex_init(&comutex_table.lock,0);
	comutex_table.index=0;
	/* reserve a superpage or two for this, entries should be small */
	/* reserve a few superpages for ports */
	return error;
}

int spawn_workers(void * func, pthread_t * threads, const char * name)
{
	pthread_t * thread;
	pthread_attr_t thread_attrs;
	worker_args_t * args;
	int e;
	int w_i;
	char * thread_name;

	/* split into threads */
	thread_name=malloc(THREAD_STRING_LEN*sizeof(char));
	threads=(pthread_t *) malloc(WORKER_COUNT*sizeof(pthread_t));
	w_i=++next_worker_i;
	strcpy(worker_map[w_i].func_name,name);
	//	printf("workers for %s\n",name);

	for (int i = 0; i < WORKER_COUNT; i++)
	{
		thread=malloc(sizeof(pthread_t));
		args=malloc(sizeof(worker_args_t));
		rand_string(thread_name,THREAD_STRING_LEN);
		strcpy(args->name,thread_name);
		args->cap=NULL;
		//printf("%s",thread_name);
		e=pthread_attr_init(&thread_attrs);
		//printf("thr_name %s\n",args->name);
		e=pthread_create(thread,&thread_attrs,func,args);
		if (e==0)
		{
			worker_map[w_i].workers[i]=*args;
			threads[i]=*thread;
		}
		pthread_attr_destroy(&thread_attrs);
	}
	return 0;
}

void run_tests(void)
{
	//TODO-PBB: Would be handy.
	return;
}

int main(int argc, const char *argv[])
{
	int verbose;
	int error;
	request_handler_args_t * handler_args;
	pthread_t coopen_threads[WORKER_COUNT];
	pthread_t counlock_threads[WORKER_COUNT];
	pthread_t comutex_init_threads[WORKER_COUNT];
	pthread_t colock_threads[WORKER_COUNT];
	//pthread_t coclose_threads[WORKER_COUNT];

	pthread_t coopen_handler;
	pthread_t counlock_handler;
	pthread_t comutex_init_handler;
	pthread_t colock_handler;
	//pthread_t coclose_handler;

	pthread_attr_t thread_attrs;
	/*
	 * TODO-PBB: Options. 
	 * - verbose
	 * - address space
	 */
	#ifdef DEBUG
	verbose=1;
	#endif


	/* check we're in a colocated address space */
	if (argc>100)
	{
		printf("%s",argv[0]);
	}

	/* we can only have one handler per cocall string */
	/*  */
	/* set up table */
	printf("Starting comesg microkernel...\n");
	error=coport_tbl_setup();
	error+=comutex_tbl_setup();
	if(error!=0)
	{
		err(1,"Table setup failed!!");
	}
	printf("Table setup complete.\n");

	/* perform setup */
	printf("Spawning co-open listeners...\n");
	error=spawn_workers(&coport_open,coopen_threads,U_COOPEN);
	printf("Spawning comutex_init listeners...\n");
	error=spawn_workers(&comutex_setup,comutex_init_threads,U_COMUTEX_INIT);
	printf("Spawning colock listeners...\n");
	error=spawn_workers(&comutex_lock,colock_threads,U_COLOCK);
	printf("Spawning counlock listeners...\n");
	error=spawn_workers(&comutex_unlock,counlock_threads,U_COUNLOCK);
	// XXX-PBB: Not implemented yet
	/*
	printf("Spawning co-close listeners...\n");
	error=spawn_workers(&coport_open,coclose_threads,U_COCLOSE);
	*/
	printf("Worker threads spawned.\n");

	/* listen for coopen requests */
	printf("Spawning request handlers...\n");
	handler_args=malloc(sizeof(request_handler_args_t));
	strcpy(handler_args->func_name,U_COOPEN);
	pthread_attr_init(&thread_attrs);
	pthread_create(&coopen_handler,&thread_attrs,manage_requests,handler_args);

	
	handler_args=malloc(sizeof(request_handler_args_t));
	strcpy(handler_args->func_name,U_COUNLOCK);
	pthread_attr_init(&thread_attrs);
	pthread_create(&counlock_handler,&thread_attrs,manage_requests,handler_args);
	

	handler_args=malloc(sizeof(request_handler_args_t));
	strcpy(handler_args->func_name,U_COMUTEX_INIT);
	pthread_attr_init(&thread_attrs);
	pthread_create(&comutex_init_handler,&thread_attrs,manage_requests,handler_args);

	handler_args=malloc(sizeof(request_handler_args_t));
	strcpy(handler_args->func_name,U_COLOCK);
	pthread_attr_init(&thread_attrs);
	pthread_create(&colock_handler,&thread_attrs,manage_requests,handler_args);

	/*
	handler_args=malloc(sizeof(request_handler_args_t));
	strcpy(handler_args->func_name,COCLOSE);
	pthread_attr_init(&thread_attrs);
	pthread_create(&coclose_handler,&thread_attrs,manage_requests,handler_args);
	*/

	pthread_join(coopen_threads[0],NULL);
	pthread_join(counlock_handler,NULL);

	return 0;
}