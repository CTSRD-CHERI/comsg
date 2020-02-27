#include <sys/mman.h>
#include <sys/param.h>

#include <stddef.h>

#include "ukern_mman.h"

int map_superpage(void ** map)
{
	char * s_page;

	unsigned int buffer_size;

	s_page=MAP_UKERN(0,COPORT_BUF_LEN,UKERN_MMAP_PROT,UKERN_MMAP_FLAGS);
	map=s_page;
	return 0;
}

int allocate_buffers()
{

}

void * buffer_malloc(size_t len)
{

}