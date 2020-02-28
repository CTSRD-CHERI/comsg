#ifndef _UKERN_MMAN_H
#define _UKERN_MMAN_H

#include "sys_comsg.h"
#include "comesg_ukernel.h"

#include <stdatomic.h>
#include <sys/param.h>
#include <cheri/cherireg.h>

#define DEFAULT_BUFFER_SIZE 4096
#define BUFFER_PERMS ( CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP | \
	CHERI_PERM_STORE | CHERI_PERM_STORE_CAP )
#define UKERN_MAP_LEN (1UL<<PDRSHIFT)
#define UKERN_MMAP_FLAGS (\
	MAP_ANON | MAP_SHARED | MAP_ALIGNED_CHERI \
	| MAP_ALIGNED_SUPER | MAP_PREFAULT_READ )
#define UKERN_RESERVE_FLAGS( UKERN_MMAP_FLAGS | MAP_GUARD )
#define UKERN_EXTEND_FLAGS ( UKERN_MMAP_FLAGS | MAP_FIXED )
#define UKERN_MMAP_PROT ( PROT_READ | PROT_WRITE )
#define BUFFER_TABLE_SIZE (WORKER_COUNT * U_FUNCTIONS * MAX_COPORTS)

#define RESERVE_PAGES 1
#define RESERVE_SIZE ( RESERVE_PAGES * UKERN_MAP_LEN )

#define MAP_UKERN(a,b) mmap(a,b,UKERN_MMAP_PROT,UKERN_MMAP_FLAGS,-1,0)
#define EXTEND_UKERN(a,b) mmap(a,b,UKERN_MMAP_PROT,UKERN_EXTEND_FLAGS,-1,0)
#define RESERVE_UKERN(a,b) mmap(a,b,UKERN_MMAP_PROT,UKERN_RESERVE_FLAGS,-1,0)

#define REGION_MAPPED 1
#define REGION_RESERVED 2

typedef struct _buffer_table_entry
{
	int size;
	_ATOMIC int free;
	int region_index;
	void * buffer;
} __attribute__((__aligned__(16))) buffer_table_entry_t;

typedef struct _buffer_table
{
	pthread_mutex_t lock;
	int next;
	int last;
	int length;
	buffer_table_entry * table;
} buffer_table_t;

typedef struct _region_table_entry
{
	pthread_mutex_t lock;
	void * mem;
	int type;
	int start_free;
	int end_free;
	int size;
} region_table_entry_t;

typedef struct _region_table
{
	pthread_mutex_t lock;
	int next;
	int last;
	int length;
	region_table_entry_t * table;
} region_table_t;

static buffer_table_t buffer_table; 

#endif