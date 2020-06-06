/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <unistd.h>
#include <err.h>
#include <string.h>
#include <cheri/cheric.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/cdefs.h>

#include <machine/tls.h>
#include <machine/param.h>
#include <machine/sysarch.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

#include "coproc.h"
#include "sys_comsg.h"

#define TCB_ALIGN (CHERICAP_SIZE)

struct tcb {
	void			*tcb_dtv;
	struct pthread		*tcb_thread;
} __packed __aligned(TCB_ALIGN);

static __inline struct pthread *
_get_curthread_no_sysarch(void)
{
	uintcap_t _rv;

	__asm__ __volatile__ (
	    "creadhwr\t%0, $chwr_userlocal"
	    : "=C" (_rv));

	return (((struct tcb *)(_rv - TLS_TP_OFFSET - TLS_TCB_SIZE))->tcb_thread);
}

#define get_thread_self(x) _get_curthread_no_sysarch(x)

struct _coproc_lookup
{
	LIST_ENTRY(_coproc_lookup) lookups;
	char target_name[LOOKUP_STRING_LEN];
	void * __capability target_cap;
};
typedef struct _coproc_lookup cplk_t;

struct _coproc_thread
{
	LIST_ENTRY(_coproc_thread) entries;
	pthread_t tid;
	void * __capability codecap;
	void * __capability datacap;
	LIST_HEAD(,_coproc_lookup) lookups;
};
typedef struct _coproc_thread cpthr_t;

typedef struct lookup_table
{
	LIST_HEAD(,_coproc_thread) entries;
	int count;
} coproc_lookup_tbl_t;

static coproc_lookup_tbl_t coproc_cache;

typedef struct handler_table
{
	LIST_HEAD(,_coproc_lookup) entries;
	int count;
} handler_tbl_t;
static handler_tbl_t request_handler_cache;

static
int add_or_lookup_thread(cpthr_t **entry)
{
	//return 1 if found
	//return 0 if created
	int error;
	pthread_t tid = get_thread_self();
	cpthr_t *thrd, *thrd_temp;
	LIST_FOREACH_SAFE(thrd, &coproc_cache.entries, entries, thrd_temp) {
		if (thrd->tid==tid) {
			*entry=thrd;

			return 1;
		}
	}
	thrd = malloc(sizeof(cpthr_t));
	memset(thrd,0,sizeof(cpthr_t));
	thrd->tid = tid;
	error=cosetup(COSETUP_COCALL,&thrd->codecap,&thrd->datacap);
	if(error!=0)
	{
		err(ESRCH,"cosetup failed\n");
	}
	LIST_INIT(&thrd->lookups);
	LIST_INSERT_HEAD(&coproc_cache.entries,thrd,entries);
	*entry = thrd;
	coproc_cache.count++;
	return 0;
}

static
void * __capability fast_colookup(const char * target_name, cpthr_t *thread)
{
	int error = 0;
	cplk_t *known_worker;
	cplk_t *request_handler = NULL;
	cplk_t *possible_handler;
	cocall_lookup_t *lookup_data;

	LIST_FOREACH(known_worker, &thread->lookups, lookups) {
		if (strcmp(known_worker->target_name,target_name)==0)
		{
			return known_worker->target_cap;
		}
	}
	LIST_FOREACH(possible_handler, &request_handler_cache.entries, lookups) {
		if (strcmp(possible_handler->target_name,target_name)==0)
		{
			request_handler = possible_handler;
			break;
		}
	}
	if(!request_handler)
	{
		request_handler = malloc(sizeof(cplk_t));
		memset(request_handler,0,sizeof(cplk_t));

		error=colookup(target_name,&request_handler->target_cap);
		if(error!=0)
		{
			err(ESRCH,"colookup of %s failed",target_name);
		}
		strcpy(request_handler->target_name,target_name);
		request_handler_cache.count++;
		LIST_INSERT_HEAD(&request_handler_cache.entries,request_handler,lookups);
	}

	lookup_data=malloc(sizeof(cocall_lookup_t));
	memset(lookup_data,0,sizeof(cocall_lookup_t));
	strcpy(lookup_data->target,target_name);
	lookup_data->cap=NULL;

	known_worker = malloc(sizeof(cplk_t));
	memset(known_worker,0,sizeof(cplk_t));

	error=cocall(thread->codecap,thread->datacap,request_handler->target_cap,lookup_data,sizeof(cocall_lookup_t));
	if(error!=0)
	{
		warn("cocall failed, trying a fresh colookup\n");
	
		error=colookup(target_name,&request_handler->target_cap);
		if(error!=0)
		{
			err(ESRCH,"colookup of %s failed",target_name);
		}
		error=cocall(thread->codecap,thread->datacap,request_handler->target_cap,lookup_data,sizeof(cocall_lookup_t));
		if(error!=0)
		{
			err(ESRCH,"cocall failed\n");
		}
	}
	
	
	strcpy(known_worker->target_name, target_name);
	known_worker->target_cap = lookup_data->cap;
	
	LIST_INSERT_HEAD(&thread->lookups,known_worker,lookups);
	free(lookup_data);
	return known_worker->target_cap;
}

int ukern_lookup(void * __capability * __capability code, 
	void * __capability * __capability data, const char * target_name, 
	void * __capability * __capability target_cap)
{
	cpthr_t *this_thread = NULL;

	add_or_lookup_thread(&this_thread);
	*code=this_thread->codecap;
	*data=this_thread->datacap;

	*target_cap=fast_colookup(target_name,this_thread);
	return 0;
}


static void 
clear_table_after_fork(void)
{
	cpthr_t *thrd, *thrd_temp;
	cplk_t *lkp, *lkp_temp;
	thrd=LIST_FIRST(&coproc_cache.entries);
	while (thrd!=NULL) {
		lkp=LIST_FIRST(&thrd->lookups);
		while (lkp!=NULL)
		{
			lkp_temp = LIST_NEXT(lkp,lookups);
			free(lkp);
			lkp=lkp_temp;
		}
		thrd_temp = LIST_NEXT(thrd,entries);
		free(thrd);
		thrd=thrd_temp;
	}

}


__attribute__ ((constructor)) static 
void coproc_init(void)
{
    LIST_INIT(&coproc_cache.entries);
    LIST_INIT(&request_handler_cache.entries);
	pthread_atfork(NULL,NULL,clear_table_after_fork);

}
