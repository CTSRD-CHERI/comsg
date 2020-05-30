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
#ifndef UKERN_TABLES_H
#define UKERN_TABLES_H

#include "coport.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>


#define TBL_FLAGS (\
	MAP_ANON | MAP_SHARED | MAP_ALIGNED_CHERI \
	| MAP_ALIGNED_SUPER | MAP_PREFAULT_READ )
#define TBL_PERMS ( PROT_READ | PROT_WRITE )

typedef struct _coport_tbl_entry_t
{
	unsigned int id;
	sys_coport_t port;
	sys_coport_t * port_cap;
	char name[COPORT_NAME_LEN];
} coport_tbl_entry_t;

typedef struct _coport_tbl_t
{
	_Atomic int index;
	coport_tbl_entry_t * table;
} coport_tbl_t;


void init_coport_table_entry(coport_tbl_entry_t * entry, sys_coport_t port, const char * name);
int coport_tbl_setup(void);
int lookup_port(char * port_name,sys_coport_t ** port_buf);
int add_port(coport_tbl_entry_t entry);
bool in_coport_table(void * __capability addr);

//extern comutex_tbl_t comutex_table;
extern coport_tbl_t coport_table;

#endif