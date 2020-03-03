#include <sys/mman.h>
#include <sys/param.h>
#include <cheri/cheric.h>

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <err.h>

#include "ukern_mman.h"

static buffer_table_t buffer_table;
static region_table_t region_table;
static work_queue_t jobs_queue;



int reserve_region(void)
{
	void * __capability page_start; 
	void * __capability prev_page;
	void * __capability new_page;

	unsigned int region_index;

	region_table_entry_t entry;
	region_table_entry_t * table;

	pthread_mutex_lock(&region_table.lock);

	table=region_table.table;
	/* 
	 * we prefer to have the mappings be contiguous though do not depend on this
	 */
	prev_page=table[region_table.last].mem;
	page_start=cheri_fromint(cheri_gettop(prev_page));
	new_page=RESERVE_UKERN(page_start,UKERN_MAP_LEN);

	entry.mem=new_page;
	entry.type=REGION_RESERVED;
	entry.size=UKERN_MAP_LEN;
	pthread_mutex_init(&entry.lock,NULL);
	memcpy(&table[region_table.next],&entry,sizeof(region_table_entry_t));

	region_index=++region_table.next;
	if (region_table.next!=(region_table.last+1))
	{
		region_table.last=region_index;
	}
	pthread_mutex_unlock(&region_table.lock);
	return region_index;
}

int map_reservation(int index)
{
	void * mapped;
	region_table_entry_t * region;

	pthread_mutex_lock(&region_table.lock);
	region = &region_table.table[index];
	mapped=EXTEND_UKERN(region->mem,region->size);
	/* explicitly set offset to make it clear that we use it */
	region->mem=cheri_setoffset(mapped,0);
	region->type=REGION_MAPPED;
	pthread_mutex_unlock(&region_table.lock);
	return 0;
}

int check_region(int entry_index, size_t len)
{
	int error;
	region_table_entry_t * entry = &region_table.table[entry_index];
	pthread_mutex_lock(&entry->lock);
	if(entry->type==REGION_RESERVED)
	{
		error=map_reservation(entry_index);
		if(error!=0)
		{
			err(error,"could not map region at index %d\n",entry_index);
		}
	}
	if (cheri_getlen(entry->mem)-cheri_getoffset(entry->mem)>len)
	{
		pthread_mutex_unlock(&entry->lock);
		return true;
	}
	pthread_mutex_unlock(&entry->lock);
	return false;
}

region_table_entry_t * ukern_find_memory(int hint,void ** dest_cap, size_t len)
{
	int i;
	void * __capability memory;
	region_table_entry_t * entry;

	if (len>MAX_BUFFER_SIZE)
	{
		err(1,"currently no support for buffers greater than 1 superpage");
	}
	/* walk the region table for the next region with space*/
	/* if this gets multithreaded this should be write-blocking locks */
	/* this is effectively just a placeholder for now */
	pthread_mutex_lock(&region_table.lock);
	for (i = hint; i < region_table.length; ++i)
	{
		entry=&region_table.table[i];
		pthread_mutex_lock(&entry->lock);
		if (check_region(i,len))
		{
			memory=cheri_csetbounds(entry->mem,len);
			memory=cheri_andperm(memory,BUFFER_PERMS);
			entry->mem=cheri_setoffset(entry->mem,len);

			pthread_mutex_unlock(&entry->lock);
			pthread_mutex_unlock(&region_table.lock);
			*dest_cap=memory;
			return entry;
		}
		pthread_mutex_unlock(&entry->lock);
	}
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
	void * buffer;
	region_table_entry_t * region;

	region=ukern_find_memory(0,&buffer,len);
	if (buffer==NULL)
	{
		region_idx=map_new_region();
		region=ukern_find_memory(region_idx,&buffer,len);
		if (buffer==NULL)
		{
			err(1,"unknown memory allocation error\n");
		}
	}
	if (region_dst!=NULL)
	{
		*region_dst=region;
	}
	return buffer;
}

void * buffer_malloc(size_t len)
{
	int buffer_idx;
	void * __capability buf;
	buffer_table_entry_t buf_entry;
	region_table_entry_t * region;

	buf=get_buffer_memory(&region,len);
	buf_entry.mem=buf;
	buf_entry.region=region;
	buf_entry.type=BUFFER_ACTIVE;

	pthread_mutex_lock(&buffer_table.lock);
	buffer_table.table[buffer_table.next]=buf_entry;
	buffer_idx=++buffer_table.next;
	if (buffer_table.next!=(buffer_table.last+1))
	{
		buffer_table.last=buffer_idx;
	}
	pthread_mutex_unlock(&buffer_table.lock);
	return buf_entry.mem;
}

void * buffer_free(void * __capability buf)
{
	/* just a placeholder really */
	/* later i want to add facility to reuse/clean up old buffers */
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
			}
		}
	}
	pthread_mutex_unlock(&buffer_table.lock);
	return cheri_cleartag(buf);
}

int buffer_table_setup(void)
{
	pthread_mutexattr_t mtx_attr;
	pthread_mutexattr_init(&mtx_attr);
	pthread_mutexattr_settype(&mtx_attr,PTHREAD_MUTEX_RECURSIVE);
	
	buffer_table.length=0;
	buffer_table.next=0;
	buffer_table.last=0;
	buffer_table.table=NULL;

	return pthread_mutex_init(&buffer_table.lock,&mtx_attr);
}

int region_table_setup(void)
{
	pthread_mutexattr_t mtx_attr;
	pthread_mutexattr_init(&mtx_attr);
	pthread_mutexattr_settype(&mtx_attr,PTHREAD_MUTEX_RECURSIVE);
	
	region_table.length=0;
	region_table.next=0;
	region_table.last=0;
	region_table.table=NULL;

	return pthread_mutex_init(&region_table.lock,&mtx_attr);
}

int work_queue_setup(int len)
{
	int error;
	pthread_mutexattr_t mtx_attr;
	pthread_condattr_t cond_attr;

	pthread_mutexattr_init(&mtx_attr);
	pthread_condattr_init(&cond_attr);

	jobs_queue.end=0;
	jobs_queue.start=0;
	jobs_queue.count=0;
	jobs_queue.items=malloc(sizeof(CHERICAP_SIZE)*len);
	jobs_queue.max_len=len;

	error=pthread_mutex_init(&jobs_queue.lock,&mtx_attr);
	error+=pthread_cond_init(&jobs_queue.not_empty,&cond_attr);
	error+=pthread_cond_init(&jobs_queue.not_full,&cond_attr);
	return error;
}

work_queue_item_t * queue_put_job(work_queue_t * queue,work_queue_item_t * job)
{
	work_queue_item_t * q_job;
	pthread_mutex_lock(&queue->lock);
	while(queue->count==queue->max_len)
	{
		pthread_cond_wait(&queue->not_full,&queue->lock);
	}
	queue->end=(queue->end+1)%queue->max_len;
	queue->items[queue->end]=job;
	q_job=queue->items[queue->end];
	queue->count++;
	pthread_cond_signal(&queue->not_empty);
	pthread_mutex_unlock(&queue->lock);
	return q_job;
}

work_queue_item_t * queue_get_job(work_queue_t * queue)
{
	work_queue_item_t * job;
	pthread_mutex_lock(&queue->lock);
	while(queue->end==queue->start)
	{
		pthread_cond_wait(&queue->not_empty,&queue->lock);
	}
	job=queue->items[queue->start];
	queue->items[queue->start]=NULL;
	queue->start=(queue->start+1)%queue->max_len;
	queue->count--;
	pthread_cond_signal(&queue->not_full);
	pthread_mutex_unlock(&queue->lock);
	return job;
}

void * ukern_malloc(size_t len)
{
	work_queue_item_t malloc_job, *queued_job;
	pthread_mutexattr_t mtx_attr;
	pthread_condattr_t cond_attr;

	pthread_mutexattr_init(&mtx_attr);
	pthread_condattr_init(&cond_attr);

	malloc_job.action=BUFFER_ALLOCATE;
	malloc_job.len=len;
	malloc_job.subject=NULL;

	pthread_mutex_init(&malloc_job.lock,&mtx_attr);
	pthread_cond_init(&malloc_job.processed,&cond_attr);

	queued_job=queue_put_job(&jobs_queue,&malloc_job);
	pthread_mutex_lock(&queued_job->lock);
	while(queued_job->subject==NULL)
	{
		pthread_cond_wait(&queued_job->processed,&queued_job->lock);
	}
	return queued_job->subject;
}

void * ukern_mman(void *args)
{
	int error;
	work_queue_item_t * job;

	if(args!=NULL)
	{
		warn("ukern_mman takes no args");
	}
	error=buffer_table_setup();
	error+=region_table_setup();
	error+=work_queue_setup(WORKER_COUNT);
	if (error!=0)
	{
		err(1,"queue/table setup failed\n");
	}
	for (;;)
	{
		job=queue_get_job(&jobs_queue);
		pthread_mutex_lock(&job->lock);
		switch (job->action)
		{
			case BUFFER_ALLOCATE:
				job->subject=buffer_malloc(job->len);
				pthread_cond_signal(&job->processed);
				break;
			case BUFFER_FREE:
				job->subject=buffer_free(job->subject);
				pthread_cond_signal(&job->processed);
				break;
			default:
				err(1,"invalid operation specified");
				break;
		}
		pthread_mutex_unlock(&job->lock);
	}
}