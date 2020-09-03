#ifndef UKERN_MSG_MALLOC_H
#define UKERN_MSG_MALLOC_H

#include <stddef.h>

void * __capability get_mem(size_t len);

void *map_new_mem(void*args);

#endif