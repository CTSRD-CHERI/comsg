#include <cheri.h>
#include <unistd.h>

#include "coport.h"
#include "comsg.h"

int coopen(char * name, int flags, coport * prt)
{
	/* request new coport from microkernel */
	void * __capability switcher_code;
	void * __capability switcher_data;
	void * __capability ukern_coopen;

	uint error;

	error=cosetup(COSETUP_COCALL, &switcher_code, &switcher_data);
	if (error!=0)
	{
		err(1,"cosetup failed");
	}

	error=colookup(U_COOPEN,&ukern_coopen);
	if (error!=0)
	{
		err(1,"colookup of microkernel ipc open failed");
	}

	error=cocall(switcher_code,switcher_data,ukern_coopen,,);

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