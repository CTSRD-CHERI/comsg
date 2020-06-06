#include <sys/mman.h>
#include <sys/param.h>
#include <cheri/cheric.h>

#include <stdatomic.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <assert.h>
#include "ukern_mman.h"
#include "ukern_params.h"
#include "sys_comsg.h"
#include "ukern_utils.h"

static buffer_table_t buffer_table;
static region_table_t region_table;
work_queue_t jobs_queue;


// TODO-PBB
// Improve free implementation so it actually works
// Perhaps by tracking how free a region is

int reserve_region(void)
{
	void * __capability new_page;

	unsigned int region_index;

	region_table_entry_t entry;
	region_table_entry_t * table;

	pthread_mutex_lock(&region_table.lock);

	table=region_table.table;

	new_page=RESERVE_UKERN(NULL,UKERN_MAP_LEN);
	if(errno!=0)
	{
		err(errno,"mapping region failed\n");
	}
	entry.map_cap=new_page;
	entry.mem=cheri_setoffset(new_page,cheri_getlen(new_page)-1);
	entry.type=REGION_RESERVED;
	entry.size=cheri_getlen(new_page);;
	entry.free=entry.size;
	pthread_mutex_init(&entry.lock,NULL);
	memcpy(&table[region_table.next],&entry,sizeof(region_table_entry_t));

	region_index=region_table.next++;
	region_table.length++;

	pthread_mutex_unlock(&region_table.lock);
	return region_index;
}

int map_region(void)
{
	void * __capability new_page;

	unsigned int region_index;

	region_table_entry_t entry;

	pthread_mutex_lock(&region_table.lock);

	new_page=MAP_UKERN(NULL,UKERN_MAP_LEN);
	memset(new_page,0,cheri_getlen(new_page));
	mlock(new_page,cheri_getlen(new_page));
	if(errno!=0)
	{
		err(errno,"mapping region failed\n");
	}
	entry.map_cap=new_page;
	entry.mem=cheri_setoffset(new_page,cheri_getlen(new_page)-1);;
	entry.type=REGION_MAPPED;
	entry.size=cheri_getlen(new_page);
	entry.free=entry.size;
	pthread_mutex_init(&entry.lock,NULL);
	memcpy(&region_table.table[region_table.next],&entry,sizeof(region_table_entry_t));

	region_index=region_table.next++;
	region_table.length++;

	pthread_mutex_unlock(&region_table.lock);
	return region_index;
}


int map_reservation(int index)
{
	void * mapped;
	region_table_entry_t * region;

	pthread_mutex_lock(&region_table.lock);
	region = &region_table.table[index];
	printf("region is at %lx\n", cheri_getaddress(region));
	mapped=EXTEND_UKERN(region->mem,region->size);
	// explicitly set offset to make it clear that we use it 
	region->mem=cheri_setoffset(mapped,cheri_getlen(mapped)-1);
	region->map_cap=mapped;
	region->size=cheri_getlen(region->mem);
	region->free=region->size;
	region->type=REGION_MAPPED;
	pthread_mutex_unlock(&region_table.lock);
	return 0;
}

int check_region(int entry_index, size_t len)
{
	int error;
	region_table_entry_t * entry = &region_table.table[entry_index];
	if (entry->type==REGION_NONE)
		return 0;
	pthread_mutex_lock(&entry->lock);
	if(entry->type==REGION_RESERVED)
	{
		error=map_reservation(entry_index);
		if(error!=0)
		{
			err(error,"could not map region at index %d\n",entry_index);
		}
	}
	else if (entry->type==REGION_NONE)
	{
		//not reached
		error=map_region();
		if(error==-1)
		{
			err(error,"could not map region at index %d\n",entry_index);
		}
	}
	if (entry->free>CHERI_REPRESENTABLE_LENGTH(len))
	{
		pthread_mutex_unlock(&entry->lock);
		return 1;
	}
	pthread_mutex_unlock(&entry->lock);
	return 0;
}

static
region_table_entry_t * ukern_find_memory(void ** dest_cap, size_t len)
{
	int i,index,error;
	void * __capability memory = NULL;
	void * __capability new_entry = NULL;
	int new_offset = 0;
	int new_top = 0;
	int j;
	region_table_entry_t * entry;
	//unsigned long int mask;
	unsigned long int base;

	if (len>MAX_BUFFER_SIZE)
	{
		err(1,"currently no support for buffers greater than 1 superpage");
	}
	/* walk the region table for the next region with space*/
	/* if this gets multithreaded this should be write-blocking locks */
	/* this is effectively just a placeholder for now */
	len=CHERI_REPRESENTABLE_LENGTH(len);
	//mask=CHERI_REPRESENTABLE_ALIGNMENT(len);
	pthread_mutex_lock(&region_table.lock);
	if(region_table.length<=0)
	{
		index=map_region();
	}
	
	assert(region_table.length!=0);
	for (i = 0; i < region_table.length; ++i)
	{
		index=i;
		entry=&region_table.table[index];
		if(entry->type==REGION_NONE)
		{
			error=map_region();
			if(error!=index)
			{
				err(1,"region mapping error");
			}
		}
		pthread_mutex_lock(&entry->lock);
		if (check_region(index,len))
		{
			base=CHERI_REPRESENTABLE_BASE(cheri_gettop(entry->mem)-len,len);
			new_entry=cheri_setaddress(entry->mem,base);
			memory=cheri_setboundsexact(new_entry,len);
			//printf("memory1 perms: %lx\n",cheri_getperm(memory));
			memory=cheri_andperm(memory,BUFFER_PERMS);
			//printf("memory2 perms: %lx\n",cheri_getperm(memory));
			new_entry=cheri_setoffset(new_entry,0);
			new_offset=CHERI_REPRESENTABLE_LENGTH(cheri_getlen(entry->mem)-cheri_getlen(memory));
			new_entry=cheri_setboundsexact(new_entry,new_offset);
			j=1;
			while(cheri_getbase(memory)<=cheri_gettop(new_entry))
			{
				j*=2;
				new_top=CHERI_REPRESENTABLE_LENGTH(new_offset-j);
				new_entry=cheri_setboundsexact(new_entry,new_top);
			}						
			entry->mem=cheri_setaddress(new_entry,cheri_gettop(new_entry)-1);
			entry->free=entry->free-cheri_getlen(memory);

			pthread_mutex_unlock(&entry->lock);
			pthread_mutex_unlock(&region_table.lock);
			*dest_cap=memory;
			return entry;
		}
		pthread_mutex_unlock(&entry->lock);
	}
	pthread_mutex_unlock(&region_table.lock);
	return NULL;
}


int map_new_region(void)
{
	int index, error;
	pthread_mutex_lock(&region_table.lock);
	index=reserve_region();
	error=map_reservation(index);
	pthread_mutex_unlock(&region_table.lock);
	return index;
}

void * get_buffer_memory(region_table_entry_t ** region_dst,size_t len)
{
	int region_idx;
	void * __capability buffer = NULL;
	region_table_entry_t * region;

	region_idx=region_table.next;
	region=ukern_find_memory(&buffer,len);
	if (region==NULL || buffer==NULL)
	{
		err(1,"unknown memory allocation error\n");
	}

	*region_dst=region;
	return buffer;
}

void * buffer_malloc(size_t len)
{
	int buffer_idx;
	buffer_table_entry_t buf_entry;
	region_table_entry_t * region;

	buf_entry.mem=get_buffer_memory(&region,len);
	buf_entry.region=region;
	buf_entry.type=BUFFER_ACTIVE;

	pthread_mutex_lock(&buffer_table.lock);
	buffer_table.table[buffer_table.next]=buf_entry;
	buffer_idx=buffer_table.next++;
	buffer_table.length++;
	pthread_mutex_unlock(&buffer_table.lock);
	memset(buf_entry.mem,0,len);
	return buf_entry.mem;
}

void buffer_free(void * __capability buf)
{
	/* just a placeholder really */
	/* later i want to add facility to reuse/clean up old buffers */
	int can_free = 1;
	buffer_table_entry_t buf_entry;
	pthread_mutex_lock(&buffer_table.lock);
	for (int i = 0; i < buffer_table.length; ++i)
	{
		if (buf==buffer_table.table[i].mem)
		{
			buf_entry=buffer_table.table[i];
			if(buf_entry.type==BUFFER_ACTIVE)
			{
				buf_entry.type=BUFFER_FREED;
				memset(buf_entry.mem,0,cheri_getlen(buf_entry.mem));
				if(buf_entry.region!=&region_table.table[region_table.length-1])
				{
					for (int j = 0; i < buffer_table.length; ++j)
					{
						if(buffer_table.table[j].region==buf_entry.region)
						{
							if(buffer_table.table[j].type!=BUFFER_FREED)
							{
								can_free=0;
								break;
							}
						}
					}
					if(can_free)
						munmap(buf_entry.region->map_cap,UKERN_MAP_LEN);
				}
				break;
			}
		}
	}
	pthread_mutex_unlock(&buffer_table.lock);
	return;
}

int buffer_table_setup(void)
{
	pthread_mutexattr_t mtx_attr;
	pthread_mutexattr_init(&mtx_attr);
	pthread_mutexattr_settype(&mtx_attr,PTHREAD_MUTEX_RECURSIVE);
	
	buffer_table.length=0;
	buffer_table.next=0;
	buffer_table.table=calloc(MAX_COPORTS*U_FUNCTIONS*WORKER_COUNT*MAX_COMUTEXES,sizeof(buffer_table_entry_t));

	return pthread_mutex_init(&buffer_table.lock,&mtx_attr);
}

int region_table_setup(void)
{
	pthread_mutexattr_t mtx_attr;
	pthread_mutexattr_init(&mtx_attr);

	errno=pthread_mutexattr_settype(&mtx_attr,PTHREAD_MUTEX_RECURSIVE);
	if(errno)
		perror("could not set mutex type\n");
	
	
	region_table.length=0;
	region_table.next=0;
	region_table.table=calloc(MAX_COPORTS*WORKER_COUNT,sizeof(region_table_entry_t));
	pthread_mutex_init(&region_table.lock,&mtx_attr);

	return 0;
}

int work_queue_setup(int len)
{
	int error;
	pthread_mutexattr_t q_mtx_attr;
	pthread_condattr_t q_cond_attr;
	pthread_mutexattr_t i_mtx_attr;
	pthread_condattr_t i_cond_attr;

	pthread_mutexattr_init(&q_mtx_attr);
	pthread_mutexattr_setpshared(&q_mtx_attr,PTHREAD_PROCESS_PRIVATE);
	pthread_condattr_init(&q_cond_attr);
	pthread_condattr_setpshared(&q_cond_attr,PTHREAD_PROCESS_PRIVATE);


	pthread_mutexattr_init(&i_mtx_attr);
	pthread_mutexattr_setpshared(&i_mtx_attr,PTHREAD_PROCESS_PRIVATE);
	pthread_condattr_init(&i_cond_attr);
	pthread_condattr_setpshared(&i_cond_attr,PTHREAD_PROCESS_PRIVATE);


	jobs_queue.end=-1; //first item will move to position 0 
	jobs_queue.start=0;
	jobs_queue.count=0;
	jobs_queue.items=calloc(len,sizeof(work_queue_item_t));
	jobs_queue.max_len=len;

	error=pthread_mutex_init(&jobs_queue.lock,&q_mtx_attr);
	error+=pthread_cond_init(&jobs_queue.not_empty,&q_cond_attr);
	error+=pthread_cond_init(&jobs_queue.not_full,&q_cond_attr);

	for(int i=0;i<jobs_queue.max_len;i++)
	{
		pthread_mutex_init(&jobs_queue.items[i].lock,&i_mtx_attr);
		pthread_cond_init(&jobs_queue.items[i].processed,&i_cond_attr);
	}
	return error;
}

work_queue_item_t * queue_put_job(work_queue_t * queue,work_queue_item_t job)
{
	work_queue_item_t * q_job;
	pthread_mutex_lock(&queue->lock);
	while(queue->count==queue->max_len)
	{
		pthread_cond_wait(&queue->not_full,&queue->lock);
	}
	queue->end=(queue->end+1)%queue->max_len;
	queue->items[queue->end]=job;
	q_job=&queue->items[queue->end];
	queue->count+=1;
	pthread_cond_signal(&queue->not_empty);
	pthread_mutex_unlock(&queue->lock);
	return q_job;
}

work_queue_item_t * queue_get_job(work_queue_t * queue)
{
	work_queue_item_t * job;
	pthread_mutex_lock(&queue->lock);
	while(queue->count==0)
	{
		pthread_cond_wait(&queue->not_empty,&queue->lock);
	}
	job=&queue->items[queue->start];
	pthread_mutex_lock(&job->lock);
	queue->start=(queue->start+1)%queue->max_len;
	pthread_mutex_unlock(&queue->lock);
	return job;
}

static
void queue_job_done(work_queue_t * queue,work_queue_item_t * job)
{
	pthread_mutex_lock(&queue->lock);
	
	pthread_cond_signal(&job->processed);
	while(pthread_mutex_unlock(&job->lock));

	queue->count-=1;	
	pthread_cond_signal(&queue->not_full);
	pthread_mutex_unlock(&queue->lock);
	return;
}


work_queue_item_t queue_do_job(work_queue_t * queue,work_queue_item_t job)
{
	work_queue_item_t * q_job;
	pthread_mutex_lock(&queue->lock);
	while(queue->count==queue->max_len)
	{
		pthread_cond_wait(&queue->not_full,&queue->lock);
	}
	queue->end=(queue->end+1)%queue->max_len;
	q_job=&queue->items[queue->end];
	pthread_mutex_lock(&q_job->lock);
	q_job->action=job.action;
	q_job->len=job.len;
	q_job->subject=job.subject;
	queue->count+=1;
	pthread_cond_signal(&queue->not_empty);
	pthread_mutex_unlock(&queue->lock);
	while(q_job->subject==job.subject)
	{
		//printf("waiting for job completion...\n");
		pthread_cond_wait(&q_job->processed,&q_job->lock);
	}
	job.subject=q_job->subject;
	while(pthread_mutex_unlock(&q_job->lock));
	return job;
}

void * __capability ukern_malloc(size_t length)
{
	return ukern_do_malloc(length,BUFFER_ALLOCATE);
}

void * __capability ukern_do_malloc(size_t length, queue_action_t type)
{
	work_queue_item_t malloc_job;
	void * __capability memory;

	if(length<=0)
	{
		err(1,"cannot request buffer of length less than 1");
	}
	else if (length>UKERN_MAP_LEN)
	{
		err(1,"cannot request buffer or length greater than UKERN_MAP_LEN");
	}

	malloc_job.action=type;
	malloc_job.len=length;
	malloc_job.subject=NULL;

	//same lock as q_job
	malloc_job=queue_do_job(&jobs_queue,malloc_job);	
	memory=malloc_job.subject;
	return memory;
}

void * __capability ukern_fast_malloc(size_t length)
{
	return ukern_do_malloc(length,MESSAGE_BUFFER_ALLOCATE);
}

void ukern_do_free(void * __capability buf_cap,queue_action_t type)
{
	work_queue_item_t free_job;

	if(cheri_gettag(buf_cap)==0)
	{
		err(1,"trying to free via untagged capability");
	}

	free_job.action=type;
	free_job.len=0;
	free_job.subject=buf_cap;

	free_job=queue_do_job(&jobs_queue,free_job);
	return;
}

void ukern_free(void * __capability buf_cap)
{
	ukern_do_free(buf_cap,BUFFER_FREE);
	return;
}

void ukern_fast_free(void * __capability buf_cap)
{
	ukern_do_free(buf_cap,MESSAGE_BUFFER_FREE);
	return;
}

void * ukern_mman(void *args)
{
	int error;
	work_queue_item_t * task;

	if(args!=NULL)
	{
		warn("ukern_mman takes no args");
	}
	error=work_queue_setup((WORKER_COUNT*U_FUNCTIONS)+2);
	error+=buffer_table_setup();
	error+=region_table_setup();
	if (error!=0)
	{
		err(1,"queue/table setup failed\n");
	}
	task=NULL;
	for (;;)
	{
		task=queue_get_job(&jobs_queue);
		if (task==NULL)
		{
			err(1,"no task returned");
		}
		switch (task->action)
		{
			case BUFFER_ALLOCATE:
				task->subject=buffer_malloc(task->len);
				break;
			case BUFFER_FREE:
				buffer_free(task->subject);
				task->subject=NULL;
				break;
			case MESSAGE_BUFFER_ALLOCATE:
				//message buffers have a much shorter lifespan than coport data structures
				//and are subject to fewer requirements WRT sharing as they are only ever
				//accessed by the microkernel
				task->subject=calloc(task->len,sizeof(char));
				break;
			case MESSAGE_BUFFER_FREE:
				free(task->subject);
				task->subject=NULL;
				break;
			default:
				err(1,"invalid operation %d specified",task->action);
		}
		queue_job_done(&jobs_queue,task);
		
	}
}