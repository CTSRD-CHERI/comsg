#ifndef _COMESG_KERN
#define _COMESG_KERN

#include <pthread.h>

#include "coport.h"
#include "sys_comsg.h"
#include "sys_comutex.h"

#define WORKER_COUNT 1
#define LOOKUP_STRING_LEN 16
#define THREAD_STRING_LEN 16
#define KEYSPACE 93
#define MAX_COPORTS 10
#define MAX_COMUTEXES 20

typedef struct _worker_args_t 
{
	char name[LOOKUP_STRING_LEN];
} worker_args_t;

typedef struct _request_handler_args_t
{
	char func_name[LOOKUP_STRING_LEN];
} request_handler_args_t;


typedef struct _coport_tbl_entry_t
{
	unsigned int id;
	char name[COPORT_NAME_LEN];
	unsigned int status;
	coport_t port;
} coport_tbl_entry_t;

typedef struct _coport_tbl_t
{
	coport_tbl_entry_t table[MAX_COPORTS];
	pthread_mutex_t lock;
	int index;
} coport_tbl_t;

typedef struct _comutex_tbl_entry_t
{
	unsigned int id;
	sys_comutex_t mtx;
} comutex_tbl_entry_t;

typedef struct _comutex_tbl_t
{
	int index;
	comutex_tbl_entry_t table[MAX_COMUTEXES];
	pthread_mutex_t lock;
} comutex_tbl_t;


int rand_string(char * buf,unsigned int len);
void *coport_open(void *args);
int coaccept_init(void * __capability switcher_code,void * __capability switcher_data, char * target_name);
void *coport_connect(void *args);
int coport_tbl_setup();
void *manage_coopen_requests(void *args)
int spawn_workers(void * __capability func, pthread_t * threads, char * name)
void run_tests();
int main(int argc, const char *argv[]);


extern coport_tbl_t coport_table;


#endif