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

#include "ukern/namespace.h"

static long global_ns_otype, proc_ns_otype, thread_ns_otype, explicit_ns_otype, library_ns_otype;

int valid_ns_name(const char * name)
{
	if(name[0]=='\0')
		return 0;

	for(int i = 0; i < strnlen(name, NS_NAME_LEN))
	{
		if(!isalnum(name[i]) && name[i] != '-' && name[i] != '_')
			return 0;
	}
	return 1;
}

int valid_ns_otype(long otype)
{
	return ((otype == global_ns_otype) || (otype == proc_ns_otype) || (otype == thread_ns_otype) || (otype == explicit_ns_otype) || (otype == library_ns_otype));
}

nstype_t get_ns_type(namespace_t *ns)
{
	long otype = cheri_gettype(ns);
	return ns_otype_to_type(otype);
	
}

nstype_t ns_otype_to_type(long otype)
{
	switch(otype)
	{
		case global_ns_otype:
			return GLOBAL;
		case proc_ns_otype:
			return PROCESS;
		case thread_ns_otype:
			return THREAD;
		case explicit_ns_otype:
			return EXPLICIT;
		default:
			return INVALID;
	}
}

long ns_type_to_otype(nstype_t type)
{
	switch(type)
	{
		case GLOBAL:
			return global_ns_otype;
		case PROCESS:
			return proc_ns_otype;
		case THREAD:
			return thread_ns_otype;
		case EXPLICIT:
			return explicit_ns_otype;
		default:
			/* should perhaps error instead */
			return 0; // 0 AKA unsealed
	}
}

__attribute__ ((constructor)) static 
void setup_otypes(void)
{
	/* call into namespace daemon and get otypes */
}