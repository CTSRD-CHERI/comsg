 #ifndef _COPORT_H
#define _COPORT_H

#define COPORT_NAME_LEN 255
#define MAX_COPORTS_ALLOWED 10

#define COPORT_OPEN 0
#define COPORT_READY 0x1
#define COPORT_BUSY 0x2 
#define COPORT_CLOSED -1

typedef int cochannel;

struct coport_buf
{
	uint size;
	uint start;
	uint end;
	void * __capability base;
};

struct coport_tbl_entry
{
	uint id;
	char[COPORT_NAME_LEN] name;
	unsigned int status;
	struct coport_buf coport_buffer;
};

int init_port(const char * name);

#endif