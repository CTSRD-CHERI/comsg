#ifndef _COPORT_FUNCTIONS_H
#define _COPORT_FUNCTIONS_H

#include "coport.h"
#include "comesg_kern.h"

int init_port(coport_type_t type, sys_coport_t * p);
bool valid_coport(sys_coport_t * addr);
bool valid_cocarrier(sys_coport_t * addr);
bool event_match(sys_coport_t * cocarrier,coport_eventmask_t e);

#endif