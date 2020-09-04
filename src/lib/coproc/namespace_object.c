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
#include "ukern/namespace_object.h"

//static long coport_nsobj_otype, coservice_nsobj_otype, commap_nsobj_otype, reservation_nsobj_otype;

int valid_nsobj_name(const char *name)
{
	return (valid_ns_name(name));
}

#if 0
int valid_nsobj_otype(long type)
{
	return (type == reservation_nsobj_otype || type == commap_nsobj_otype || type == coport_nsobj_otype || type == coservice_nsobj_otype);
}

nsobjtype_t get_nsobject_type(nsobject_t *nsobj)
{
	long otype = cheri_gettype(nsobj);
	return nsobj_otype_to_type(otype);
}

nsobjtype_t nsobj_otype_to_type(long otype)
{
	switch(otype)
	{
		case coport_nsobj_otype:
			return COPORT;
		case coservice_nsobj_otype:
			return COSERVICE;
		case commap_nsobj_otype:
			return COMMAP;
		case reservation_nsobj_otype:
			return RESERVATION;
		default:
			return INVALID;
	}
}

long nsobject_type_to_otype(nsobjtype_t type)
{
	switch(type)
	{
		case COPORT:
			return coport_nsobj_otype;
		case COSERVICE:
			return coservice_nsobj_otype;
		case COMMAP:
			return commap_nsobj_otype;
		case RESERVATION:
			return reservation_nsobj_otype;
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
#endif