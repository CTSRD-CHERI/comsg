#ifndef _COMESG_KERN
#define _COMESG_KERN

#define WORKER_COUNT 1
#define LOOKUP_STRING_LEN 16
#define THREAD_STRING_LEN 16
#define KEYSPACE 93
#define MAX_COPORTS 10

typedef struct _coopen_data_t 
{
	char name[LOOKUP_STRING_LEN];
} coopen_data_t;

typedef struct _coport_tbl_t
{
	coport_tbl_entry_t table[MAX_COPORTS];
	pthread_mutex_t lock;
	int index;
} coport_tbl_t;

#endif