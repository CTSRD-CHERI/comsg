
#include "ukern_msg_malloc.h"
#include "ukern_mman.h"

#include <stddef.h>
#include <pthread.h>
#include <sys/mman.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>
#include <sys/sched.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <stdatomic.h>

const size_t max_len = 1024*1024;

struct {
	_Atomic(void * __capability) current_message_pool;
	pthread_cond_t need_new_mem;
} mem_tbl;
#define MSG_POOL_TOP (cheri_getbase(mem_tbl.current_message_pool)+cheri_getlen(mem_tbl.current_message_pool))

static
void map_msg_region(void)
{
	void * __capability new_page;
	new_page=MAP_UKERN(NULL,UKERN_MAP_LEN);
	memset(new_page,0,UKERN_MAP_LEN);
	mlock(new_page,UKERN_MAP_LEN);
	if(errno!=0)
	{
		err(errno,"mapping region failed\n");
	}
	mem_tbl.current_message_pool=new_page;
	return;
}


static
void get_new_mem(void)
{
	int i = 1;
	pthread_cond_signal(&mem_tbl.need_new_mem);
	while(cheri_getlen(atomic_load(&mem_tbl.current_message_pool))<max_len)
	{
		if(i%10==0)
			sched_yield();
		i++;
	}
}


void *map_new_mem(void*args)
{
	pthread_mutex_t memlock;
	pthread_mutex_lock(&memlock);
	for(;;)
	{
		map_msg_region();
		pthread_cond_wait(&mem_tbl.need_new_mem,&memlock);
	}
	return args;
}

void * __capability
get_mem(size_t len)
{
	void * __capability result;
	void * __capability reduced_pool;
	void * __capability orig_pool;
	size_t base, new_offset;
	size_t j, new_top;
	if (len>max_len)
		err(EINVAL,"may not allocate more than %lu", max_len);
	
	orig_pool = mem_tbl.current_message_pool;
	len=CHERI_REPRESENTABLE_LENGTH(len);
	do {
		if (len>cheri_getlen(orig_pool))
			get_new_mem();
		base=CHERI_REPRESENTABLE_BASE(MSG_POOL_TOP-len,len);
		result=cheri_setaddress(orig_pool,base);
		result=cheri_setboundsexact(result,len);
		new_offset=CHERI_REPRESENTABLE_LENGTH(cheri_getlen(orig_pool)-cheri_getlen(result));
		reduced_pool=cheri_setboundsexact(orig_pool,new_offset);
		j = 1;
		while(cheri_getbase(result)<=cheri_gettop(reduced_pool))
		{
			j*=2;
			new_top=CHERI_REPRESENTABLE_LENGTH(new_offset-j);
			reduced_pool=cheri_setboundsexact(reduced_pool,new_top);
		}	
	} while(!atomic_compare_exchange_weak_explicit(&mem_tbl.current_message_pool,&orig_pool,reduced_pool,memory_order_acq_rel,memory_order_acquire));

	return result;
}

