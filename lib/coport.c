#include "coport.h"

#include <cheric.h>
#include <sys/mman.h>
#include <err.h>
#include <string.h>

#include "comesg_kern.h"

int generate_id()
{
	static int id_counter = 0;
	// TODO: Replace this with something smarter.
	return ++id_counter;
}

int init_port(const char * name, coport_type_t type, coport_tbl_entry_t * p)
{
	coport_tbl_entry_t new_port;
	int error;

	if (strlen(name)>COPORT_NAME_LEN)
	{
		err(1,"port name length too long");
	}
	strcpy(p->name,name);

	p->buffer=mmap(0,COPORT_BUF_LEN,COPORT_MMAP_PROT,COPORT_MMAP_FLAGS,-1,0);
	p->status=COPORT_OPEN;
	p->id=generate_id();

	return 0;
}