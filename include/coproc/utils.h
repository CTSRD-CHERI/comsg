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
#ifndef UKERN_UTILS_H
#define UKERN_UTILS_H

#define KEYSPACE 62

#include <cheri/cheric.h>

#include <stddef.h>
#include <sys/mman.h>

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

struct object_type
{
	otype_t sc;
	otype_t usc;
	long otype;
};

int generate_id(void);
int rand_string(char * buf, size_t len);
int valid_scb(void * scb);
void *make_otypes(void * rootcap, int n_otypes, struct object_type **results);
int get_maxprocs(void);

//Handy for working with mmap(2) prot values vs CHERI permissions
inline
int perms_to_prot(int perms)
{
	int prot = 0;

	if (perms & (CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP))
		prot |=  PROT_READ;
	if (perms & (CHERI_PERM_STORE | CHERI_PERM_STORE_CAP | CHERI_PERM_STORE_LOCAL_CAP))
		prot |= PROT_WRITE;
	if (perms & CHERI_PERM_EXECUTE )
		prot |= PROT_EXEC;

	return (prot);
}

inline
int prot_to_perms(int prot)
{
	int perms = 0;

	if (prot & PROT_READ)
		perms |= (CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP);
	if (prot & PROT_WRITE)
		perms |= (CHERI_PERM_STORE | CHERI_PERM_STORE_CAP |
		CHERI_PERM_STORE_LOCAL_CAP);
	if (prot & PROT_EXEC)
		perms |= (CHERI_PERM_EXECUTE);

	return (perms);
}

#define GET_PROT(c) perms_to_prot(cheri_getperm(c))
#define SET_PROT(c, p) cheri_andperm(c, prot_to_perms(p))
#define HAS_PROT(a, b) ( a <= ( a & b ) )
#define HAS_PROT_PERMS(c, p) ( p <= ( GET_PROT(c) & p ) )

#endif