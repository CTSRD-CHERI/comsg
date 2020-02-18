#include "coport_utils.h"

#include <cheric.h>
#include <sys/mman.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>

#include "comesg_kern.h"
#include "coport.h"


int generate_id()
{
	static int id_counter = 0;
	// TODO: Replace this with something smarter.
	return ++id_counter;
}

int init_port(const char * name, coport_type_t type, coport_tbl_entry_t * p)
{
	coport_tbl_entry_t new_port;
	struct coport_mutex_t mtx_lock;
	int error;

	if (strlen(name)>COPORT_NAME_LEN)
	{
		err(1,"port name length too long");
	}
	strcpy(p->name,name);

	p->port.length=COPORT_BUF_LEN;
	p->port.buffer=mmap(0,COPORT_BUF_LEN,COPORT_MMAP_PROT,COPORT_MMAP_FLAGS,-1,0);
	p->status=COPORT_OPEN;
	p->id=generate_id();

	p->port.start=0;
	p->port.end=0;
	p->port.status=COPORT_OPEN;
	p->lock=mtx_lock;

	return 0;
}