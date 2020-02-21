#include <unistd.h>
#include <err.h>
#include <string.h>


#include "coproc.h"

int ukern_lookup(void * __capability __capability code, 
	void * __capability __capability data, const char * target_name, 
	void * __capability __capability target_cap)
{
	int error;
	cocall_lookup_t lookup_data;
	void * __capability lookup_cap;
	if(code==NULL || data==NULL)
	{
		error=cosetup(COSETUP_COCALL,code,data);
		if(error!=0)
		{
			err(1,"cosetup failed");
		}
	}
	if(strlen(target_name)>COPORT_NAME_LEN)
	{
		err(1,"target name too long");
	}

	error=colookup(target_name,lookup_cap);
	if(error!=0)
	{
		err(1,"colookup failed");
	}
	strcpy(lookup_data.func_name,target_name);

	error=cocall(code,data,lookup_cap,&lookup_data,sizeof(lookup_data));
	if(error!=0)
	{
		err(1,"cocall failed");
	}
	target_cap=lookup_data.target;
	return 0;
}

