#ifndef _UKERN_MMAN_H
#define _UKERN_MMAN_H

#define DEFAULT_BUFFER_SIZE 4096
#define UKERN_MAP_LEN 
#define UKERN_MMAP_FLAGS (\
	MAP_ANON | MAP_SHARED | MAP_ALIGNED_CHERI \
	| MAP_ALIGNED_SUPER )
#define UKERN_MMAP_PROT (PROT_READ | PROT_WRITE)


#define MAP_UKERN(a,b) mmap(a,b,UKERN_MMAP_PROT,UKERN_MMAP_FLAGS,-1,0)



struct buffer_table_entry
{
	int size;
}

struct buffer_table
{
	pthread_mutex_t lock;
	int next;
	int last;

}
#endif