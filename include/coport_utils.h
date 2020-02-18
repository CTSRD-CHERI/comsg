#ifndef _COPORT_FUNCTIONS_H
#define _COPORT_FUNCTIONS_H

#include "coport.h"
#include "comesg_kern.h"

int generate_id();
int init_port(const char * name, coport_type_t type, coport_tbl_entry_t * p);

#endif