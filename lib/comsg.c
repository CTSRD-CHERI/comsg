#include <cheri.h>
#include <unistd.h>

#include "coport.h"
#include "comsg.h"

int coopen(char * coport_name, coport_type_t type, coport_t * prt)
{
	/* request new coport from microkernel */
	void * __capability switcher_code;
	void * __capability switcher_data;
	void * __capability ukern_coopen;

	struct coopen_cocall call;
	
	uint error;

	/* cocall setup */
	error=cosetup(COSETUP_COCALL, &switcher_code, &switcher_data);
	if (error!=0)
	{
		err(1,"cosetup failed");
	}

	error=colookup(,&ukern_coopen);
	if (error!=0)
	{
		err(1,"colookup of microkernel ipc open failed");
	}

	/* prepare args */
	if (strlen(name)>COPORT_NAME_LEN)
	{
		err(1,"port name length too long");
	}
	error=strcpy(call.args.name,name);

	call.args.type=type;

	error=cocall(switcher_code,switcher_data,ukern_coopen,&call,sizeof(call));
	prt=call.port;

	return 0;
}

int cosend(void * __capability port, const void * __capability mesg_buf, size_t len)
{
	return 0;
}

int coreceive(void * __capability port, void * __capability buf, size_t len)
{
	return 0;
}

int coclose(void * __capability port)
{
	return 0;
}