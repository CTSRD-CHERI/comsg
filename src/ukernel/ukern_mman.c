#include <sys/mman.h>
#include <sys/param.h>

#include <stddef.h>

#include "ukern_mman.h"

int map_superpage(void ** map)
{
	char * s_page;

	unsigned int buffer_size;

	s_page=MAP_UKERN(0,UKERN_MAP_LEN,UKERN_MMAP_PROT,UKERN_MMAP_FLAGS);
	map=s_page;
	return 0;
}

int allocate_buffers(void ** buffer_array)
{
	void * raw_mem_page, * buf_cap, * raw_mem_cursor;
	int error = map_superpage(&raw_mem);
	int buf_count = UKERN_MAP_LEN/DEFAULT_BUFFER_SIZE;
	raw_mem_cursor=raw_mem_page;
	for (int i = 0; i < buf_count; i++)
	{
		buf_cap=cheri_csetbounds(raw_mem_cursor,);
		raw_mem_cursor=cheri_setoffset(raw_mem_page,DEFAULT_BUFFER_SIZE);
		
}

void * buffer_malloc(size_t len)
{

}