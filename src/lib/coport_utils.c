#include "coport_utils.h"

#include <cheri/cheric.h>
#include <sys/mman.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ukern_mman.h"
#include "comesg_kern.h"
#include "coport.h"
#include "sys_comutex.h"


int init_port(coport_type_t type, coport_t * p)
{

	p->length=COPORT_BUF_LEN;
	p->buffer=ukern_malloc(p->length);
	memset(p->buffer,0,p->length);
	//printf("got memory from ukern_mman subsystem\n");
	p->status=COPORT_OPEN;
	p->start=0;
	p->end=0;
	p->type=type;
	memset(&p->lock,0,sizeof(comutex_t));



	return 0;
}