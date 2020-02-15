#ifndef _COPORT_H
#define _COPORT_H

#include <cheric.h>
#include <sys/mman.h>

#define COPORT_NAME_LEN 255


#define COPORT_OPEN 0
#define COPORT_READY 0x1
#define COPORT_BUSY 0x2 
#define COPORT_CLOSED -1

#define COPORT_MMAP_FLAGS (MAP_ANONYMOUS | MAP_SHARED | MAP_ALIGNED_CHERI)
#define COPORT_MMAP_PROT (PROT_READ | PROT_WRITE)

#define COPORT_BUF_LEN 4096

typedef enum _coport_type_t {cochannel} coport_type_t;

typedef struct _coopen_args_t
{
	char[COPORT_NAME_LEN] name;
	coport_type_t type;
} coopen_args_t;

typedef struct _coport_tbl_entry_t
{
	uint id;
	char[COPORT_NAME_LEN] name;
	unsigned int status;
	void * __capability buffer;
} coport_tbl_entry_t;

typedef struct _coport_t
{
	void * __capability buffer;
} coport_t;

typedef struct _cocall_coopen_t
{
	coopen_args_t args;
	coport_t port; 
} cocall_coopen_t;

int coopen(char * coport_name, enum coport_type type, coport_t __capability * prt);

#endif