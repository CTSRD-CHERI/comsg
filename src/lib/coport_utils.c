#include "coport_utils.h"

#include <cheri/cheric.h>
#include <sys/mman.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>

#include "ukern_mman.h"
#include "comesg_kern.h"
#include "coport.h"


int init_port(coport_type_t type, coport_t * p)
{
	/*
	 * TODO-PBB: We will replace this mmap call so that its job is performed by
	 * a worker thread ahead of time, avoiding the context switch from the 
	 * syscall.
	 */
	p->buffer=ukern_malloc(COPORT_BUF_LEN);
	p->length=COPORT_BUF_LEN;
	memset(p->buffer,0,p->length);
	p->status=COPORT_OPEN;
	p->start=0;
	p->end=0;
	p->type=type;

	return 0;
}