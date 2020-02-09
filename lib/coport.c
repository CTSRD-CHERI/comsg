#include "coport.h"

static volatile unsigned int next_port_index = 0;

int init_port(const char * name)
{
	struct coport_tbl_entry new_port;

	if()
	{

	}

	if (strlen(name)>COPORT_NAME_LEN)
	{
		err(1,"port name length too long");
	}
	error=strcpy(new_port,name);

}