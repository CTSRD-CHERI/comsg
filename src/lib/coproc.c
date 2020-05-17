/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
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


int ukern_lookup(void * __capability * __capability code, 
	void * __capability * __capability data, const char * target_name, 
	void * __capability * __capability target_cap)
{
	int error;
	cocall_lookup_t * lookup_data;
	void * __capability lookup_cap;
	void * __capability sw_code;
	void * __capability sw_data;


	error=cosetup(COSETUP_COCALL,&sw_code,&sw_data);
	if(error!=0)
	{
		err(ESRCH,"cosetup failed\n");
	}
	*code=sw_code;
	*data=sw_data;
	if(strlen(target_name)>COPORT_NAME_LEN)
	{
		err(ESRCH,"target name too long\n");
	}

	error=colookup(target_name,&lookup_cap);
	if(error!=0)
	{
		err(ESRCH,"colookup of %s failed\n",target_name);
	}
	lookup_data=malloc(sizeof(cocall_lookup_t));
	memset(lookup_data,0,sizeof(cocall_lookup_t));
	strcpy(lookup_data->target,target_name);
	lookup_data->cap=(void * __capability)NULL;
	error=cocall(sw_code,sw_data,lookup_cap,lookup_data,sizeof(cocall_lookup_t));
	if(error!=0)
	{
		err(ESRCH,"cocall failed\n");
	}
	//error=colookup(lookup_data->target,target_cap);
	*target_cap=lookup_data->cap;
	//free(lookup_data);
	return 0;
}
