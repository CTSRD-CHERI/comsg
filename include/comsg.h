#ifndef COMSG_H
#define COMSG_H

#include "sys_comsg.h"
#include "coport.h"


typedef struct _cocall_lookup_t
{
	char func_name[PATH_MAX];
	void * __capability target;
} cocall_lookup_t;

int ukern_lookup(void * __capability __capability code, void * __capability __capability data, const char * target_name, void * __capability __capability target_cap);
int coopen(const char * coport_name, coport_type_t type, coport_t * prt);
int cosend(coport_t * port, const void * __capability buf, size_t len);
int coreceive(coport_t * port, void * __capability buf, size_t len);
int coclose(coport_t * port);


#endif