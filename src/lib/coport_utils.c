#include "coport_utils.h"

#include <cheri/cheric.h>
#include <sys/mman.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>

#include "comesg_kern.h"
#include "coport.h"


int init_port(coport_type_t type, coport_t * p)
{
	int error;

	/*
	 * TODO-PBB: We will replace this mmap call so that its job is performed by
	 * a worker thread ahead of time, avoiding the context switch from the 
	 * syscall.
	 */
	p->buffer=mmap(0,COPORT_BUF_LEN,COPORT_MMAP_PROT,COPORT_MMAP_FLAGS,-1,0);
	p->length=COPORT_BUF_LEN;
	p->status=COPORT_OPEN;
	p->start=0;
	p->end=0;
	p->status=COPORT_OPEN;

	return 0;
}