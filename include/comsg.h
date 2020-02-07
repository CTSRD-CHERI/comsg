#ifndef COMSG_H
#define COMSG_H

#include <cheri.h>

#include "sys/sys_comsg.h"
#include "coport.h"

struct coport
{
	char[COPORT_NAME_LEN] name;
	void * __capability port_cap;
}

int coopen(char * name, int flags);
int cosend(void * __capability port, const void * __capability mesg_buf, size_t len);
int coreceive(void * __capability port, void * __capability buf, size_t len);
int coclose(void * __capability port);


#endif