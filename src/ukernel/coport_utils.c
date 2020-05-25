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

#include "coport_utils.h"

#include <cheri/cheric.h>
#include <sys/mman.h>
#include <stdatomic.h>
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "sys_comsg.h"
#include "ukern_mman.h"
#include "comesg_kern.h"
#include "coport.h"

bool valid_coport(sys_coport_t * addr)
{
    ptrdiff_t table_offset;
    vaddr_t port_addr = (vaddr_t) addr;
    int index;

    if(cheri_getlen(addr)<sizeof(sys_coport_t))
    {
        printf("too small to represent coport\n");
        return false;
    }
    else if(!cheri_is_address_inbounds(coport_table.table,port_addr))
    {
        printf("address not in bounds\n");
        return false;
    }
    else
    {
        table_offset=port_addr-cheri_getbase(coport_table.table);
        index=table_offset/sizeof(coport_tbl_entry_t);
        if(&coport_table.table[index].port!=addr)
        {
            printf("offset looks wrong\n");
            return false;
        }
    }
    return true;
}

bool valid_cocarrier(sys_coport_t * addr)
{
    if(cheri_gettype(addr)!=sealed_otype)
    {
        printf("wrong type\n");
        return false;
    }
    else
    {
        addr=cheri_unseal(addr,root_seal_cap);
    }
    if(!valid_coport(addr))
    {
        return false;
    }

    return true;
}

int init_port(coport_type_t type, sys_coport_t* p)
{

	if(type==COPIPE)
	{
		p->length=CHERICAP_SIZE;
		p->buffer=NULL;
		p->end=0;
	}
	else if(type==COCARRIER)
	{
		p->length=0;
		p->end=-1;
		p->buffer=ukern_malloc(COCARRIER_SIZE);
		LIST_INIT(&port.listeners);
	}
	else
	{
		p->length=COPORT_BUF_LEN;
		p->buffer=ukern_malloc(COPORT_BUF_LEN);
	}
	
	//memset(p->buffer,0,p->length);
	//printf("got memory from ukern_mman subsystem\n");
	p->status=COPORT_OPEN;
	p->start=0;
	p->type=type;
	port.event=COPOLL_INIT_EVENTS;
	//memset(&p->read_lock,0,sizeof(comutex_t));
	//memset(&p->write_lock,0,sizeof(comutex_t));

	return 0;
}

void init_coport_table_entry(coport_tbl_entry_t * entry, const sys_coport * port, const char * name)
{
	coport_tbl_entry_t e;

	e.port=port;
	strncpy(entry.name,name,COPORT_NAME_LEN);
	e.id=generate_id();
}

bool event_match(sys_coport_t * cocarrier,coport_eventmask_t e)
{
	return ((bool) cocarrier->event & e);
}