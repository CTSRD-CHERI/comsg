#include <cheric.h>
#include <sys/mman.h>

#include "coport.h"
#include "comesg_kern.h"

int generate_id()
{
	// TODO: Replace this with something smarter.
	return next_port_index;
}

int add_port(coport_table_entry_t * entry)
{
	if(coport_table.index==MAX_COPORTS)
	{
		return 1;
	}

	pthread_mutex_lock(coport_table.lock);
	memcpy(&coport_table.table[coport_table.index],entry,sizeof(entry));
	coport_table.index++;
	pthread_mutex_unlock(coport_table.lock);

	return 0;
}

int lookup_port(char * port_name)
{
	if (strlen(port_name)>COPORT_NAME_LEN)
	{
		err(1,"port name length too long");
	}
}

int init_port(const char * name, coport_type_t type, coport_t * p)
{
	coport_tbl_entry_t new_port;
	int error;

	if (strlen(name)>COPORT_NAME_LEN)
	{
		err(1,"port name length too long");
	}
	error=strcpy(new_port.name,name);

	new_port.buffer=mmap(NULL,COPORT_BUF_LEN,COPORT_MMAP_PROT,COPORT_MMAP_FLAGS,-1,0);
	new_port.status=COPORT_OPEN;
	new_port.id=generate_id();

	err=add_port(&new_port);
	if(err!=0)
	{
		err(1,"unable to init_port");
	}

	p->buffer=new_port.buffer;
	return 0;
}