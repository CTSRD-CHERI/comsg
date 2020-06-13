#ifndef _COPORT_FUNCTIONS_H
#define _COPORT_FUNCTIONS_H

#include "coport.h"
#include "comesg_kern.h"
#include "ukern_tables.h"

int init_port(coport_type_t type, sys_coport_t * p);

/*
extern inline bool valid_coport(sys_coport_t * addr);
extern inline bool valid_cocarrier(sys_coport_t * addr);
extern inline bool event_match(sys_coport_t * cocarrier,coport_eventmask_t e);
*/

static inline
bool valid_coport(sys_coport_t * addr)
{
    if(cheri_getlen(addr)<sizeof(sys_coport_t))
    {
        //printf("too small to represent coport\n");
        return false;
    }
    else if(!in_coport_table(addr))
    {
        return false;
    }
    return true;
    
}

static inline
bool valid_cocarrier(sys_coport_t * addr)
{
    if(cheri_gettype(addr)!=sealed_otype)
    {
        //printf("wrong type\n");
        return false;
    }
    if(!valid_coport(addr))
    {
        return false;
    }

    return true;
}

static inline
bool event_match(sys_coport_t * cocarrier,coport_eventmask_t e)
{
	return ((bool) cocarrier->event & e);
}


#endif