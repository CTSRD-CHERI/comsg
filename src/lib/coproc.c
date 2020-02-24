#include <unistd.h>
#include <err.h>
#include <string.h>
#include <cheri/cheric.h>
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
	printf("lookup struct is: %lu bytes\n",sizeof(cocall_lookup_t));
	error=cocall(sw_code,sw_data,lookup_cap,lookup_data,sizeof(cocall_lookup_t));
	if(error!=0)
	{
		err(1,"cocall failed\n");
	}
	else
	{
		printf("success");
	}
	error=colookup(lookup_data->target,target_cap);
	*code=sw_code;
	*data=sw_data;
	return 0;
}

