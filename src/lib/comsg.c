#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>

#include <unistd.h>
#include <stdatomic.h>
#include <err.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "coproc.h"
#include "coport.h"
#include "comsg.h"

#ifdef BENCHMARK_MEMCPY
#define	timespecsub(vvp, uvp)						\
	do {								\
		(vvp)->tv_sec -= (uvp)->tv_sec;				\
		(vvp)->tv_nsec -= (uvp)->tv_nsec;			\
		if ((vvp)->tv_nsec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_nsec += 1000000000;			\
		}							\
	} while (0)
#endif

int coopen(const char * coport_name, coport_type_t type, coport_t *prt)
{
	/* request new coport from microkernel */
	void * __capability switcher_code;
	void * __capability switcher_data;
	void * __capability func;

	cocall_coopen_t * __capability call;

	uint error;

	/* cocall setup */
	//TODO-PBB: Only do this once.
	call=calloc(1,sizeof(cocall_coopen_t));
	strcpy(call->args.name,coport_name);
	call->args.type=type;
	//call->args=args;
	//printf("cocall_open_t has size %lu\n",sizeof(cocall_coopen_t));

	if (call!=NULL)
	{
		//possible deferred call to malloc ends up in cocall, causing exceptions
		error=ukern_lookup(&switcher_code,&switcher_data,U_COOPEN,&func);
		error=cocall(switcher_code,switcher_data,func,call,sizeof(cocall_coopen_t));
		*prt=call->port;
	}
	return 0;
}

int cosend(coport_t port, const void * buf, size_t len)
{
	unsigned int old_end;
	coport_status_t status_val;
	_Atomic(void *) msg_cap;
	void ** dest_buf;
	//struct timespec start, end;
	
	switch(port->type)
	{
		case COCHANNEL:
			for(;;)
			{
				status_val=COPORT_OPEN;
				if(atomic_compare_exchange_weak_explicit(&port->status,&status_val,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
				{
					break;
				}
			}
			atomic_thread_fence(memory_order_acquire);
			if((port->length-((port->end-port->start)%port->length))<len)
			{
				err(1,"message too big/buffer is full");
			}
			old_end=port->end;
			port->end=(port->end+len)%port->length;
			if(old_end+len>port->length)
			{
				memcpy((char *)port->buffer+old_end, buf, port->length-old_end);
				memcpy((char *)port->buffer+old_end, (const char *)buf+port->length-old_end, (old_end+len)%port->length);

			}
			else
			{
				memcpy((char *)port->buffer+old_end, buf, len);
			}
			atomic_store_explicit(&port->status,COPORT_OPEN,memory_order_relaxed);
			break;
		case COCARRIER:
			for(;;)
			{
				status_val=COPORT_OPEN;
				if(atomic_compare_exchange_weak_explicit(&port->status,&status_val,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
				{
					break;
				}
			}
			atomic_thread_fence(memory_order_acquire);
			//map buffer of size len
			//POSSIBLE MEMORY LEAK HERE THAT I WOULD LIKE TO ADDRESS
			//ALSO VM ISSUES IF SENDER EXITS EARLY
			//Perhaps mmap? Perhaps ukern mmap offers memory?
			msg_cap=calloc(len,1);
			//copy data from buf
			memcpy(msg_cap,buf,len);
			//reduce capability to buffer to read only
			msg_cap=cheri_andperm(msg_cap,COCARRIER_PERMS);
			//append capability to buffer
			old_end=port->end;
			port->end=port->end+CHERICAP_SIZE;
			dest_buf=(void **)port->buffer;
			dest_buf[old_end]=msg_cap;
			atomic_store_explicit(&port->status,COPORT_OPEN,memory_order_relaxed);
			break;
		case COPIPE:
			for(;;)
			{
				status_val=COPORT_READY;
				if(atomic_compare_exchange_weak_explicit(&port->status,&status_val,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
				{
					break;
				}
			}
			atomic_thread_fence(memory_order_acquire);
			if(cheri_getlen(port->buffer)<len)
			{
				err(1,"recipient buffer len %lu too small for message of length %lu",cheri_getlen(port->buffer),len);
			}
			#ifdef BENCHMARK_MEMCPY
			clock_gettime(CLOCK_REALTIME,&start);
			#endif
			memcpy(port->buffer,buf,len);
			#ifdef BENCHMARK_MEMCPY
			clock_gettime(CLOCK_REALTIME,&end);
			#endif
			atomic_store_explicit(&port->status,COPORT_DONE,memory_order_relaxed);

			#ifdef BENCHMARK_MEMCPY
			timespecsub(&end,&start);
			printf("memcpy time:%jd.%09jds\n",(intmax_t)end.tv_sec,(intmax_t)end.tv_nsec);
			#endif

			break;
		default:
			err(1,"unimplemented coport type");
	}
	atomic_thread_fence(memory_order_release);
	return 0;
}

int corecv(coport_t port, void ** buf, size_t len)
{
	//we need more atomicity on changes to end
	int old_start;
	unsigned char ** msg_buf;
	coport_status_t status_val;
	for(;;)
	{
		status_val=COPORT_OPEN;
		if(atomic_compare_exchange_weak_explicit(&port->status,&status_val,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
		{
			break;
		}
	}
	atomic_thread_fence(memory_order_acquire);
	switch(port->type)
	{
		case COCHANNEL:
			if (port->start==port->length)
			{
				//check for synchronicity
				warn("no message in buffer, blocking...");
				while(port->start==port->length)
				{
					__asm("nop");
				}
			}
			if(port->length<len)
			{
				err(1,"message too big");
			}
			if(port->buffer==NULL)
			{
				err(1,"coport is closed");
			}
			
			for(;;)
			{
				status_val=COPORT_OPEN;
				if(atomic_compare_exchange_weak_explicit(&port->status,&status_val,COPORT_BUSY,memory_order_acq_rel,memory_order_acquire))
				{
					break;
				}
			}
			old_start=port->start;
			port->start=port->start+len;
			memcpy(*buf,(char *)port->buffer+old_start, len);
			port->start=port->start+len;
			atomic_store_explicit(&port->status,COPORT_OPEN,memory_order_relaxed);
			break;
		case COCARRIER:
			msg_buf = port->buffer;
			//POSSIBLE MEMORY LEAK HERE THAT I WOULD LIKE TO ADDRESS
			//len is advisory
			//perhaps we should allocate buf in COCHANNEL rather than expect 
			//an allocated buffer
			old_start=port->start/CHERICAP_SIZE;
			port->start=port->start+CHERICAP_SIZE;
			//pop next cap from buffer into buf (index indicated by start)
			*buf=msg_buf[old_start];
			msg_buf[old_start]=cheri_cleartag(msg_buf[old_start]);
			//perhaps inspect length and perms for safety
			if(cheri_getlen(buf)!=len)
			{
				warn("message length (%lu) does not match len (%lu)",cheri_getlen(buf),len);
			}
			if((cheri_getperm(buf)&(CHERI_PERM_LOAD|CHERI_PERM_LOAD_CAP))==0)
			{
				err(1,"received capability does not grant read permissions");
			}
			break;
		case COPIPE:
			port->buffer=*buf;
			atomic_store_explicit(&port->status,COPORT_READY,memory_order_relaxed);
			atomic_thread_fence(memory_order_release);
			for(;;)
			{
				status_val=COPORT_DONE;
				if(atomic_compare_exchange_weak(&port->status,&status_val,COPORT_OPEN))
				{
					break;
				}
			}
			port->buffer=NULL;
			break;
		default:
			break;
	}
	atomic_thread_fence(memory_order_release);
	return 0;
	
}

int coclose(coport_t port)
{
	port=NULL;
	return 0;
}

int copoll(coport_t port)
{
	if (port)
	{
		return 0;
	}
	return 0;
}