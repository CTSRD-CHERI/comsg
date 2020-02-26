#include <unistd.h>
#include <err.h>
#include <string.h>
#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/cdefs.h>

#include <machine/param.h>
#include <machine/sysarch.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>

#include "coproc.h"


int ukern_lookup(void * * code, 
	void * * data, const char * target_name, 
	void * * target_cap)
{
	int error;
	cocall_lookup_t * lookup_data;
	void * __capability lookup_cap;
	void * __capability sw_code;
	void * __capability sw_data;

	error=cosetup(COSETUP_COCALL,&sw_code,&sw_data);
	if(error!=0)
	{
		err(1,"cosetup failed\n");
	}
	if(strlen(target_name)>COPORT_NAME_LEN)
	{
		err(1,"target name too long\n");
	}

	error=colookup(target_name,&lookup_cap);
	if(error!=0)
	{
		err(1,"colookup of %s failed\n",target_name);
	}
	lookup_data=malloc(sizeof(cocall_lookup_t));
	strcpy(lookup_data->target,target_name);
	lookup_data->cap=NULL;
	error=cocall(sw_code,sw_data,lookup_cap,lookup_data,sizeof(cocall_lookup_t));
	if(error!=0)
	{
		err(1,"cocall failed\n");
	}
	*code=sw_code;
	*data=sw_data;
	*target_cap=lookup_data->cap;
	return 0;
}
