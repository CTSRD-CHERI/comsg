#include "coport_utils.h"

#include <cheri/cheric.h>
#include <sys/mman.h>
#include <stdatomic.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ukern_mman.h"
#include "comesg_kern.h"
#include "coport.h"
#include "sys_comutex.h"


int init_port(coport_type_t type, sys_coport_t* p)
{

	if(type==COPIPE)
	{
		p->length=CHERICAP_SIZE;
		p->buffer=NULL;
		p->end=0;
	}
	else if(type==COCARRIER)
	{
		p->length=0;
		p->end=-1;
		p->buffer=ukern_malloc(COCARRIER_SIZE);
	}
	else
	{
		p->length=COPORT_BUF_LEN;
		p->buffer=ukern_malloc(COPORT_BUF_LEN);
	}
	
	//memset(p->buffer,0,p->length);
	//printf("got memory from ukern_mman subsystem\n");
	p->status=COPORT_OPEN;
	p->start=0;
	p->type=type;
	//memset(&p->read_lock,0,sizeof(comutex_t));
	//memset(&p->write_lock,0,sizeof(comutex_t));

	return 0;
}