#ifndef UKERN_MSG_MALLOC_H
#define UKERN_MSG_MALLOC_H

void * __capability
get_memory(size_t len);

void *map_new_mem(void*args);

#endif