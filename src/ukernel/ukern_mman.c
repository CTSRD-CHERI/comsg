#include <sys/mman.h>
#include <sys/param.h>
#include <cheri/cheric.h>

#include <stddef.h>
#include <pthread.h>

#include "ukern_mman.h"

static buffer_table_t buffer_table;
static region_table_t region_table;



inline int cheri_getobjlen(void * __capability cap, void obj)
{
	return (cheri_getlen(cap)/sizeof(obj));
}

void extend_region_table(void);

int reserve_region(void)
{
	void * __capability page_start; 
	void * __capability prev_page;
	void * __capability new_page;

	unsigned int buffer_size;
	unsigned int region_index;

	region_table_entry_t entry;
	region_table_entry_t * table;

	pthread_mutex_lock(region_table.lock);

	table=region_table.table;
	/* 
	 * we prefer to have the mappings be contiguous though do not depend on this
	 */
	prev_page=table[region_table.last].mem;
	page_start=cheri_fromint(cheri_gettop(prev_page));
	new_page=RESERVE_UKERN(page_start,UKERN_MAP_LEN);

	entry.mem=new_page;
	entry.type=REGION_RESERVED;
	entry.start_free=0;
	entry.end_free=0;
	entry.size=UKERN_MAP_LEN;
	pthread_mutex_init(&entry.lock,NULL);
	memcpy(&table[region_table.next],entry);

	region_index=++region_table.next;
	if (region_table.next!=(region_table.last+1))
	{
		region_table.last=region_index;
	}
	pthread_mutex_unlock(region_table.lock);
	return region_index;
}

int map_reservation(int index)
{
	void * mapped;
	region_table_entry_t * region;

	pthread_mutex_lock(region_table.lock);
	region = &region_table.table[index];
	mapped=EXTEND_UKERN(region->mem,region->size);
	/* explicitly set offset to make it clear that we use it */
	region->mem=cheri_setoffset(mapped,0);
	region->type=REGION_MAPPED;
	pthread_mutex_unlock(region_table.unlock);
	return 0;
}

void * allocate_memory_buffer(void ** cap,size_t length)
{
	void * buf_cap;
	buf_cap=cheri_csetbounds(cap,length);
	buf_cap=cheri_andperms(buf_cap,BUFFER_PERMS);
	cap=cheri_setoffset(cap,length);
	return buf_cap;
}

int check_region(region_table_entry_t * entry, size_t len)
{
	pthread_mutex_lock(&entry->lock);
	if(entry->type==REGION_RESERVED)
	{
		error=map_reservation(i);
	}
	if (cheri_getlen(entry->mem)-cheri_getoffset(entry->mem)>len)
	{
		pthread_mutex_unlock(&entry->unlock);
		return TRUE;
	}
	pthread_mutex_unlock(&entry->unlock);
	return FALSE;
}

region_table_entry_t * ukern_find_memory(int hint,void ** dest_cap, size_t len)
{
	int error;
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
	for (i = 0; i < region_table.length; ++i)
	{
		entry=&region_table.table[i];
		pthread_mutex_lock(&entry->lock);
		if (check_region(entry,len))
		{
			memory=cheri_csetbounds(entry->mem,len);
			memory=cheri_andperms(memory,BUFFER_PERMS);
			entry->mem=cheri_setoffset(entry->mem,len);

			pthread_mutex_unlock(&entry->lock);
			pthread_mutex_unlock(&region_table.lock);
			*dest_cap=memory;
			return entry;
		}
		pthread_mutex_unlock(&entry->unlock);
	}
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
	int region_idx
	void * buffer;
	region_table_entry_t * region;

	region=ukern_find_memory(0,&buf_mem,len);
	if (buf_mem==NULL)
	{
		region_idx=map_new_region();
		region=ukern_find_memory(region_idx,&buf_mem,len);
		if (buf_mem==NULL)
		{
			err(1,"unknown memory allocation error\n");
		}
	}
	if (region_dst!=NULL)
	{
		*region_dst=region;
	}
	return buf;
}

void * buffer_malloc(size_t len)
{
	int region_idx,buffer_idx;
	void * __capability buf;
	buffer_table_entry_t buf_entry;
	region_table_entry_t * region;

	buf=get_buffer_memory(&region,len);
	buf_entry.mem=buf;
	buf_entry.region=region;
	buf_entry.type=BUFFER_ACTIVE;

	pthread_mutex_lock(buffer_table.lock);
	buffer_table.table[buffer_table.next]=buf_entry;
	buffer_idx=++buffer_table.next;
	if (buffer_table.next!=(buffer_table.last+1))
	{
		buffer_table.last=buffer_idx;
	}
	pthread_mutex_unlock(buffer_table.lock);
	return buf_entry.mem;
}

void buffer_free(void * __capability buf)
{
	/* just a placeholder really */
	/* later i want to add facility to reuse old buffers */
	buffer_table_entry_t buf_entry;
	pthread_mutex_lock(buffer_table.lock);
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
	pthread_mutex_unlock(buffer_table.lock);
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

	return pthread_mutex_init(&region_table.lock,&mtx_attr);
}

void * ukern_mman(void *args)
{
	int error;
	error=buffer_table_setup();
	error+=region_table_setup();
}