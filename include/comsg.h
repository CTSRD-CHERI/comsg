#ifndef COMSG_H
#define COMSG_H

#include "sys_comsg.h"
#include "coport.h"

int coopen(const char * coport_name, coport_type_t type, coport_t * prt);
int cosend(coport_t * port, const void * buf, size_t len);
int corecv(coport_t * port, void ** buf, size_t len);
int coclose(coport_t * port);


#endif