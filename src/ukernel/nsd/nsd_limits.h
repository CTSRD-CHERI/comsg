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
#ifndef _NSD_LIMITS_H
#define _NSD_LIMITS_H
//XXX: policy choices, should be controlled via sysctl
//TODO-PBB: define initial reservations/resource limits for each namespace type

/* Per-namespace caps for number of nsobjects */
/*
 * XXX: One option is to have the size defined by combining the parent and the type.
 * An explicit namespace in the global namespace could therefore be larger than one in a thread namespace.
 * We should also allow creators to tell us if they definitely won't use more than N objects.
 */ 
static const size_t global_max_objects;
static const size_t thread_max_objects;
static const size_t explicit_max_objects;
static const size_t library_max_objects;
static const size_t process_max_objects;

static const size_t process_max_objlen;
static const size_t thread_max_objlen;
static const size_t explicit_max_objlen;
static const size_t library_max_objlen;
static const size_t global_max_objlen;

/* Set by namespace_table.c:setup_table */
/* Determines how many child namespaces may be assigned per type */
/*
static size_t global_max_namespaces;
static size_t thread_max_namespaces;
static size_t explicit_max_namespaces;
static size_t library_max_namespaces;
static size_t process_max_namespaces;

static size_t process_max_nslen;
static size_t thread_max_nslen;
static size_t explicit_max_nslen;
static size_t library_max_nslen;
static size_t global_max_nslen = (global_max_namespaces * sizeof(namespace_t));
*/
#endif