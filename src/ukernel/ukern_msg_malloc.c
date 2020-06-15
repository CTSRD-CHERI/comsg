
#include "ukern_msg_malloc.h"
#include "ukern_mman.h"

#include <assert.h>
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
#include <time.h>
#include <stdio.h>

#define MMAP_FLAGS (\
	MAP_ANON | MAP_SHARED | MAP_ALIGNED(mmap_align) | MAP_PREFAULT_READ )

const size_t max_len = 1024*1024;
static size_t align_min;
static size_t mmap_align;
const static size_t map_len = 1024*1024*16;
static size_t mmap_len;

struct {
	_Atomic(void * __capability) current_message_pool;
	_Atomic(void * __capability) vestigial_message_pool;
	_Atomic(void * __capability) next_message_pool;
	pthread_cond_t need_new_mem;
} mem_tbl;
#define MSG_POOL_TOP ( cheri_getbase(mem_tbl.current_message_pool)+cheri_getlen(mem_tbl.current_message_pool) )

//NOTE-PBB: This is currently incredibly wasteful for small amounts of memory, and will need to be reworked.

static
void map_msg_region(void)
{
	

	void * __capability new_page;
	new_page=mmap(NULL,map_len,UKERN_MMAP_PROT,MMAP_FLAGS,-1,0);
	if(errno!=0)
	{
		err(errno,"mapping region failed\n");
	}
	//mlock(new_page,cheri_getlen(new_page));
	memset(new_page,0,cheri_getlen(new_page));
	
	atomic_store_explicit(&mem_tbl.next_message_pool,new_page,memory_order_release);
	return;
}


static
void get_new_mem(void)
{
	
	int i = 1;
	while(atomic_load(&mem_tbl.next_message_pool)==NULL)
	{
		printf("spinning\n");
		if(i%100==0)
			sched_yield();
		i++;
	}
	atomic_store(&mem_tbl.current_message_pool,mem_tbl.next_message_pool);
	atomic_store(&mem_tbl.next_message_pool,NULL);
}

void *map_new_mem(void*args)
{
	mmap_align = CHERI_REPRESENTABLE_ALIGNMENT(16*1024*1024); 
	mmap_len = CHERI_REPRESENTABLE_LENGTH(map_len);
	struct timespec sleepytime;
	align_min = CHERI_REPRESENTABLE_ALIGNMENT(1);

	map_msg_region();
	atomic_store_explicit(&mem_tbl.current_message_pool,atomic_load_explicit(&mem_tbl.next_message_pool,memory_order_acquire),memory_order_release);
	atomic_store_explicit(&mem_tbl.next_message_pool,NULL,memory_order_release);
	map_msg_region();
	for(;;)
	{
		if(atomic_load_explicit(&mem_tbl.next_message_pool,memory_order_acquire)==NULL)
			map_msg_region();
		sleepytime.tv_sec=5;
		sleepytime.tv_nsec=0;
		nanosleep(&sleepytime,&sleepytime);
	}
	return args;
}

void * __capability get_mem
(size_t len)
{
	void * __capability result;
	void * __capability reduced_pool;
	void * __capability orig_pool;
	size_t raw_new_len;
	vaddr_t new_base;
	if (len>max_len)
		err(EINVAL,"may not allocate more than %lu", max_len);
	
	orig_pool = atomic_load_explicit(&mem_tbl.current_message_pool,memory_order_acquire);
	len=CHERI_REPRESENTABLE_LENGTH(len);

	do {
		if (len>(cheri_getlen(orig_pool)-cheri_getoffset(orig_pool)) || !(cheri_gettag(orig_pool)))
		{
			get_new_mem();
			orig_pool = atomic_load_explicit(&mem_tbl.current_message_pool,memory_order_acquire);
		}
		if(len>=4096 && (((len & (len-1))==0)))
			result=__builtin_align_up(orig_pool,len);
		else
			result=__builtin_align_up(orig_pool,CHERI_REPRESENTABLE_ALIGNMENT(len));
		//align on len
		

		result=cheri_setboundsexact(result,len);
		
		new_base=cheri_gettop(result);
		raw_new_len=cheri_gettop(orig_pool)-new_base-align_min;

		reduced_pool=cheri_setaddress(orig_pool,new_base);
		reduced_pool=cheri_setbounds(reduced_pool,raw_new_len); //not exact, but address should be fine
		assert(cheri_getaddress(reduced_pool)>=new_base);
		
	} while(!atomic_compare_exchange_strong_explicit(&mem_tbl.current_message_pool,&orig_pool,reduced_pool,memory_order_acq_rel,memory_order_acquire));
	if((cheri_getlen(reduced_pool)-cheri_getoffset(reduced_pool))<len)
	{
		if(atomic_compare_exchange_strong_explicit(&mem_tbl.current_message_pool,&reduced_pool,atomic_load(&mem_tbl.next_message_pool),memory_order_acq_rel,memory_order_acquire))
			atomic_store_explicit(&mem_tbl.next_message_pool,NULL,memory_order_release);

	}

	return result;
}

