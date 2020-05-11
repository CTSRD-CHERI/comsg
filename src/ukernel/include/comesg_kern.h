#ifndef _COMESG_KERN
#define _COMESG_KERN

#include <pthread.h>
#include <stdatomic.h>
#include <cheri/cherireg.h>
#include <stdbool.h>
#include <sys/queue.h>

#include "coport.h"
#include "sys_comsg.h"
#include "sys_comutex.h"
#include "ukern_params.h"


#define TBL_FLAGS (\
	MAP_ANON | MAP_SHARED | MAP_ALIGNED_CHERI \
	| MAP_ALIGNED_SUPER | MAP_PREFAULT_READ )
#define TBL_PERMS ( PROT_READ | PROT_WRITE )
/*#define TBL_PERMS ( CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP | \
	CHERI_PERM_STORE | CHERI_PERM_STORE_CAP | CHERI_PERM_GLOBAL |\
	CHERI_PERM_STORE_LOCAL_CAP )*/
#define WORKER_FUNCTIONS ( U_FUNCTIONS + UKERN_PRIV )


typedef struct _worker_args_t 
{
	char name[LOOKUP_STRING_LEN];
	_Atomic(void * __capability) cap;
} worker_args_t;

typedef struct _worker_map_entry_t
{
	char func_name[LOOKUP_STRING_LEN];
	worker_args_t workers[WORKER_COUNT];
} worker_map_entry_t;

typedef struct _request_handler_args_t
{
	char func_name[LOOKUP_STRING_LEN];
} request_handler_args_t;

typedef struct _coport_tbl_entry_t
{
	unsigned int id;
	sys_coport_t port;
	char name[COPORT_NAME_LEN];
} coport_tbl_entry_t;

typedef struct _coport_tbl_t
{
	_Atomic int index;
	coport_tbl_entry_t * table;
} coport_tbl_t;

typedef struct _comutex_tbl_entry_t
{
	unsigned int id;
	sys_comutex_t mtx;
} comutex_tbl_entry_t;

typedef struct _comutex_tbl_t
{
	int index;
	pthread_mutex_t lock;
	comutex_tbl_entry_t * table;
} comutex_tbl_t;


int generate_id(void);
int rand_string(char * buf,unsigned int len);
int add_port(coport_tbl_entry_t entry);
int add_mutex(comutex_tbl_entry_t entry);
int lookup_port(char * port_name,sys_coport_t ** port_buf);
int lookup_mutex(char * mtx_name,sys_comutex_t ** mtx_buf);
void update_worker_args(worker_args_t * args, const char * function_name);
void create_comutex(comutex_t * cmtx,char * name);
bool valid_coport(sys_coport_t * addr);
bool valid_cocarrier(sys_coport_t * addr);
bool event_match(sys_coport_t * cocarrier,coport_eventmask_t e);
void *copoll_deliver(void *args);
void *cocarrier_poll(void *args);
void *cocarrier_register(void *args);
void *cocarrier_recv(void *args);
void *cocarrier_send(void *args);
void *coport_open(void *args);
void *comutex_setup(void *args);
void *comutex_lock(void *args);
void *comutex_unlock(void *args);
int comutex_deinit(comutex_tbl_entry_t * m);
void *manage_requests(void *args);
int coaccept_init(
	void * __capability * __capability  code_cap,
	void * __capability * __capability  data_cap, 
	char * target_name,
	void * __capability * __capability target_cap);
int coport_tbl_setup(void);
int comutex_tbl_setup(void);
int spawn_workers(void * func, pthread_t * threads, const char * name);
void run_tests(void);
int main(int argc, const char *argv[]);


extern coport_tbl_t coport_table;
extern comutex_tbl_t comutex_table;

#endif