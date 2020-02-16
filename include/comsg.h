#ifndef COMSG_H
#define COMSG_H

#include "sys_comsg.h"
#include "coport.h"


typedef struct _cocall_lookup_t
{
	char * function_name;
	void * __capability target;
} cocall_lookup_t;

int coopen(char * coport_name, coport_type_t type, coport_t * prt);
int cosend(void * __capability port, const void * __capability mesg_buf, size_t len);
int coreceive(void * __capability port, void * __capability buf, size_t len);
int coclose(void * __capability port);


#endif