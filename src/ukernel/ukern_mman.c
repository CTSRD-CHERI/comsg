#include <sys/mman.h>
#include <sys/param.h>
#include <cheri/cheric.h>

#include <stddef.h>
#include <pthread.h>

#include "ukern_mman.h"

static buffer_table_t buffer_table;
static region_table_t region_table;


int reserve_buffer_space()
{
	void * s_page, *next_page;

	unsigned int buffer_size;
	unsigned int old_size;

	region_table_entry_t entry;

	pthread_mutex_lock(region_table.lock);
	old_size=region_table.length*UKERN_MAP_LEN;
	region_table.table=realloc(region_table.table,old_size+ALLOCATION_SIZE);

	s_page=MAP_UKERN(0,UKERN_MAP_LEN);
	for (int i = 1; i <= RESERVE_PAGES; i++)
	{
		next_page=RESERVE_UKERN(s_page+(i*UKERN_MAP_LEN),UKERN_MAP_LEN);

		entry.mem=next_page;
		entry.type=REGION_RESERVED;
		entry.start_free=0;
		entry.end_free=0;
		entry.size=UKERN_MAP_LEN;
		pthread_mutex_init(&entry.lock,NULL);

		region_table.table[region_table.next]=entry;
		region_table.next++;
	}
	pthread_mutex_unlock(region_table.lock);
	map=s_page;
	return 0;
}

void * map_reservation(int index)
{
	void * mapped;
	region_table_entry_t * region = &region_table.table[index];

	pthread_mutex_lock(region_table.lock);
	mapped=EXTEND_UKERN(region->mem,region->size);
	region->mem=mapped;
	pthread_mutex_unlock(region_table.unlock);

}

int allocate_buffers(void ** buffer_array)
{
	void * raw_mem_page, * buf_cap, * raw_mem_cursor;
	int error = reserve_buffer_space(&raw_mem);
	int buf_count = UKERN_MAP_LEN/DEFAULT_BUFFER_SIZE;
	raw_mem_cursor=raw_mem_page;
	for (int i = 0; i < buf_count; i++)
	{
		buf_cap=cheri_csetbounds(raw_mem_cursor,DEFAULT_BUFFER_SIZE);
		buf_cap=cheri_andperms(buf_cap,BUFFER_PERMS);
		raw_mem_cursor=cheri_setoffset(raw_mem_page,DEFAULT_BUFFER_SIZE);
		buffer_array[i]=buf_cap;
	}
}

void * find_available_memory(size_t len)
{
	for (int i = 0; i < region_table.length; ++i)
	{
		if(region_table.type==REGION_RESERVED)
		{

		}
		if region_table.table[i].sta
	}
}

void * buffer_malloc(size_t len)
{
	// get 
	pthread_mutex_lock(buffer_table.lock);
	if (buffer_table.next==buffer_table.last)
	{
		MAP_UKERN(buffer_table.table[length],UKERN_EXTEND_FLAGS);
	}
	pthread_mutex_unlock(buffer_table.lock);
	return 0;
}

int buffer_table_setup(void)
{
	pthread_mutexattr_t mtx_attr;
	pthread_mutexattr_init(&mtx_attr);
	pthread_mutexattr_settype(&mtx_attr,RECURSIVE);
	
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
	pthread_mutexattr_settype(&mtx_attr,RECURSIVE);
	
	region_table.length=0;
	region_table.next=0;
	region_table.last=0;
	region_table.table=NULL;

	return pthread_mutex_init(&buffer_table.lock,&mtx_attr);
}