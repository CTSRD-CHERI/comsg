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
#ifndef _NSD_TABLE_H
#define _NSD_TABLE_H

#include <comsg/namespace.h>
#include <comsg/namespace_object.h>

#include <stdatomic.h>
#include <stddef.h>
#include <sys/queue.h>

/* This is in a separate struct to make revoking namespace handles much easier */
/* It also makes symlinking, copying, and moving namespaces possible */

//XXX-PBB: These data structures are... IMO, not great. I'd like to do something "better" but am 
//not finding that it's coming to me right now.

struct _ns_member {
	LIST_ENTRY(_ns_member) entries;
	union {
		namespace_t *ns;
		nsobject_t *nsobj;
	};
};

struct _ns_members {
	LIST_HEAD(, _ns_member) objects;
	_Atomic size_t nobjects;
	size_t max_objects;

	LIST_HEAD(, _ns_member) namespaces;
	_Atomic size_t nspaces;
	size_t max_namespaces;
};

namespace_t *allocate_namespace(namespace_t *parent, nstype_t type);
nsobject_t *allocate_nsobject(namespace_t *parent);

void set_root_namespace(namespace_t *ns_cap);
int is_root_namespace(namespace_t *ns_cap);
void *get_root_namespace(void);

int in_ns_table(namespace_t *ptr);
int in_nsobject_table(nsobject_t *ptr);

void namespace_deleted(void);
void nsobject_deleted(void);


#endif //!defined (_NSD_TABLE_H)