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
#ifndef _NSOBJ_H
#define _NSOBJ_H

#include "ukern/namespace.h"

/*
 * Types
 * 	+ RESERVATION:	For objects that are not yet ready to be returned via coselect
 * 					e.g. services that will be registered later, or whose workers are 
 * 					not yet ready to process requests.
 */

#define NSOBJ_PERMS_OWN_MASK ( NS_PERMS_OWN_MASK | CHERI_PERM_STORE | CHERI_PERM_STORE_CAP )

typedef enum {INVALID=-1, RESERVATION=0, COMMAP=1, COPORT=2, COSERVICE=4} nsobjtype_t;

typedef struct _nsobject
{
	char name[NS_NAME_LEN];
	nsobjtype_t type;
	union
	{
		void *obj;
		coservice_t *coservice;
		coport_t *coport;
	}
} nsobject_t;

#define VALID_NSOBJ_TYPE(type) ( type == RESERVATION || type == COMMAP || type == COPORT || type == COSERVICE )

nobjtype_t get_nsobject_type(nsobject_t *nsobj);
nsobjtype_t nsobject_otype_to_type(long otype);
long nsobject_type_to_otype(nsobjtype_t type);

int valid_nsobj_name(const char *name);
int valid_nsobj_otype(long type);


#endif