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
#ifndef _NSD_TABLE_H
#define _NSD_TABLE_H

#include "ukern/namespace.h"
#include "ukern/namespace_object.h"

#include <stdatomic.h>
#include <stddef.h>
#include <sys/queue.h>

/* This is in a separate struct to make revoking namespace handles much easier */
/* It also makes symlinking, copying, and moving namespaces possible */

//TODO-PBB: rework into linked lists now we have ccmalloc

struct _member {
	LIST_ENTRY(_member) entries;
	union {
		namespace_t *ns;
		nsobject_t *nsobj;
	};
}

struct _ns_members {
	LIST_HEAD(, _member) objects;
	_Atomic size_t nobjects;
	size_t max_objects;

	LIST_HEAD(, _member) namespaces;
	_Atomic size_t nspaces;
	size_t max_namespaces;
};

namespace_t *allocate_namespace(namespace_t *parent, nstype_t type);
nsobject_t *allocate_nsobject(namespace_t *parent);

void set_global_namespace(namespace_t *ns_cap);
int is_global_namespace(namespace_t *ns_cap);
void *get_global_namespace(void);


#endif //!defined (_NSD_TABLE_H)