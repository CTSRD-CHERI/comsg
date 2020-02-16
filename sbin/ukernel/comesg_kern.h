#ifndef _COMESG_KERN
#define _COMESG_KERN

#include <pthread.h>

#include "sys_comsg.h"

#define WORKER_COUNT 1
#define LOOKUP_STRING_LEN 16
#define THREAD_STRING_LEN 16
#define KEYSPACE 93
#define MAX_COPORTS 10

typedef struct _coport_tbl_entry_t
{
	unsigned int id;
	char name[COPORT_NAME_LEN];
	unsigned int status;
	void * __capability buffer;
} coport_tbl_entry_t;

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


int rand_string(char * buf,unsigned int len);
void *coport_open(void *args);
int coaccept_init(void * __capability switcher_code,void * __capability switcher_data, char * target_name);
void *coport_connect(void *args);
int coport_tbl_setup();
int spawn_workers(void * func, pthread_t * worker_array);
void manage_requests(pthread_t * coport_open_threads);
int main(int argc, const char *argv[]);

extern coport_tbl_t coport_table;


#endif