/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#include <comsg/coport.h>
#include <comsg/coservice.h>
#include <comsg/namespace.h>

#include <sys/cdefs.h>

/*
 * Types
 * 	+ RESERVATION:	For objects that are not yet ready to be returned via coselect
 * 					e.g. services that will be registered later, or whose workers are 
 * 					not yet ready to process requests.
 */

/* 
 * NSOBJ_PERM_R:
 * 	+ Authorises retrieval of the object handle associated with the namespace object
 * NSOBJ_PERM_W:
 *	+ If the namespace object is a reservation, authorises setting a type and object handle
 *	+ Checks for this should be accompanied by appropriate checks for object type
 * NSOBJ_PERM_D:
 *	+ If the namespace object is not a reservation, authorises its deletion when used together with 
 *	  an appropriate namespace capability.
 * Note: Hardware store permissions will only be present on reservation objects, which are sealed
 *       and not dereferenced outside of cocalls to the namespace daemon.
 */

#define NSOBJ_PERM_R ( CHERI_PERM_LOAD_CAP )
#define NSOBJ_PERM_W ( CHERI_PERM_STORE | CHERI_PERM_STORE_CAP | CHERI_PERM_STORE_LOCAL_CAP )
#define NSOBJ_PERM_D ( CHERI_PERM_SW2 )
#define NSOBJ_PERM_OWN ( NSOBJ_PERM_W | NSOBJ_PERM_D | NSOBJ_PERM_R )

#define NSOBJ_PERMITS_WRITE(c) ( cheri_getperm(c) & NSOBJ_PERM_W ) 
#define NSOBJ_PERMITS_READ(c) ( cheri_getperm(c) & NSOBJ_PERM_R )
#define NSOBJ_PERMITS_DELETE(c) ( cheri_getperm(c) & NSOBJ_PERM_D )

#define NSOBJ_PERMS_MASK ( CHERI_PERM_GLOBAL | CHERI_PERM_LOAD ) //may read name and type only

#define NSOBJ_PERMS_R_MASK ( NSOBJ_PERMS_MASK | NSOBJ_PERM_R ) 
#define NSOBJ_PERMS_W_MASK ( NSOBJ_PERMS_MASK | NSOBJ_PERM_W ) 
#define NSOBJ_PERMS_D_MASK ( NSOBJ_PERMS_MASK | NSOBJ_PERM_D ) 
#define NSOBJ_PERMS_OWN_MASK ( NSOBJ_PERMS_MASK | NSOBJ_PERM_OWN ) 

#define CLEAR_NSOBJ_STORE_PERM(c) ( cheri_andperm(c, ~NSOBJ_PERM_W) )

typedef enum {INVALID_NSOBJ=-1, RESERVATION=0, COMMAP=1, COPORT=2, COSERVICE=4} nsobject_type_t;
static const nsobject_type_t last_nsobj_type = COSERVICE;

typedef struct _nsobject
{
	char 			name[NS_NAME_LEN];
	nsobject_type_t	type;
	union
	{
		_Atomic(void *)			obj;
		_Atomic(coservice_t	*)	coservice;
		_Atomic(coport_t	*)	coport;
	};
} nsobject_t;

#define VALID_NSOBJ_TYPE(type) ( type == RESERVATION || type == COMMAP || type == COPORT || type == COSERVICE )

int valid_nsobj_name(const char *name);

#endif