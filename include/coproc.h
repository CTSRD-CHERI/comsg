#ifndef _COPROC_H
#define _COPROC_H

typedef struct _cocall_lookup_t
{
	char func_name[PATH_MAX];
	void * __capability target;
} cocall_lookup_t;

int ukern_lookup(void * __capability __capability code, 
	void * __capability __capability data, const char * target_name, 
	void * __capability __capability target_cap);

#endif