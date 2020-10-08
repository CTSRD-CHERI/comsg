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
#ifndef _NAMESPACE_H
#define _NAMESPACE_H

#include <cheri/cheric.h>
#include <cheri/cherireg.h>
#include <stdatomic.h>
#include <sys/cdefs.h>


/*
 * We only have two 'user-defined' permission bits available.
 * We could use HW-defined permissions or otypes if really pressed, but these would be 
 * decidedly worse as deriving narrowed perms would require calling into the ns daemon.
 */

/* 
 * NS_PERM_OBJ:
 * 	+ authorises certain actions on objects inside its namespace that have some implications beyond the object
 * 	+ used where larger permissions might be inappropriate; e.g. querying parentage
 * 	+ Implicit in holding a valid namespace capability, which I'd prefer it wasn't, but we have only 2 bits.
 * 	+ The only way to lack this is to lack a capability to the namespace.
 * NS_PERM_RO:
 * 	+ allows querying and listing objects in a namespace
 * 	+ allows grabbing a capability to a named object inside the namespace
 * NS_PERM_WR:
 * 	+ Writing implies reading, because attempting to create an existing namespace object
 * 	  fails if the object already exists; to check this, reading is required.
 * 	+ Allows adding objects (including namespaces) to the namespace
 * 	+ Allows renaming objects if it owns them
 * NS_PERM_OWN:
 * 	+ allows all lesser operations
 * 	+ allows arbitrary deletion of objects without their ns object capabilities
 * 	+ allows deletion of the namespace if the namespace has type EXPLICIT
 */

#define NS_HWPERMS_MASK ( CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | \
	CHERI_PERM_LOAD_CAP | CHERI_PERM_SW2 | CHERI_PERM_SW3 )

#define NS_PERMS_OBJ ( 0 )
#define NS_PERM_RO   ( CHERI_PERM_SW2 )
#define NS_PERM_WR   ( CHERI_PERM_SW3 )
#define NS_PERMS_OWN ( CHERI_PERM_SW2 | CHERI_PERM_SW3 ) 

#define NS_PERMS_RO_MASK ( NS_HWPERMS_MASK | NS_PERM_RO )
#define NS_PERMS_WR_MASK ( NS_HWPERMS_MASK | NS_PERM_WR )
#define NS_PERMS_OWN_MASK ( NS_HWPERMS_MASK | NS_PERMS_OWN )
#define NS_PERMS_OBJ_MASK ( NS_HWPERMS_MASK | NS_PERMS_OBJ )

#define NS_PERMITS_WRITE(c) ( cheri_getperm(c) & NS_PERM_WR )
#define NS_PERMITS_READ(c) ( ( cheri_getperm(c) & ( NS_PERM_WR | NS_PERM_RO ) ) != 0)

/* 
 * Holders of a handle to any of these with appropriate permissions may create a sub-namespace.
 * Namespaces may contain coports or coservices.
 * Library namespaces are not yet fully thought out.
 */

typedef enum {INVALID_NS = -1, GLOBAL = 1, PROCESS = 2, THREAD = 4, EXPLICIT = 8, LIBRARY = 16} nstype_t;

#ifndef NS_NAME_LEN
#define NS_NAME_LEN ( (CHERICAP_SIZE * 2) + ( CHERICAP_SIZE - sizeof(nstype_t) ) )
#endif

struct _ns_members;
typedef struct _namespace namespace_t;

struct _namespace {
	namespace_t	*parent;
	char 		name[NS_NAME_LEN];
	nstype_t	 type;
	struct _ns_members *members;
};

#define VALID_NS_TYPE(type) ( type == GLOBAL || type == GLOBAL || type == PROCESS || type == THREAD || type == EXPLICIT || type == LIBRARY )

__BEGIN_DECLS

int valid_ns_name(const char * name);

__END_DECLS

#endif //_NAMESPACE_H