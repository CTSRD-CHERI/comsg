#include <cheri/cheri.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>

#include <unistd.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "coproc.h"
#include "coport.h"
#include "comsg.h"

int coopen(const char * coport_name, coport_type_t type, coport_t* *prt)
{
	/* request new coport from microkernel */
	void * __capability switcher_code;
	void * __capability switcher_data;
	void * __capability func;

	cocall_coopen_t * __capability call;

	uint error;

	/* cocall setup */
	//TODO-PBB: Only do this once.
	call=calloc(1,sizeof(cocall_coopen_t));
	strcpy(call->args.name,coport_name);
	call->args.type=type;
	//call->args=args;
	//printf("cocall_open_t has size %lu\n",sizeof(cocall_coopen_t));

	if (call!=NULL)
	{
		//possible deferred call to malloc ends up in cocall, causing exceptions
		error=ukern_lookup(&switcher_code,&switcher_data,U_COOPEN,&func);
		error=cocall(switcher_code,switcher_data,func,call,sizeof(cocall_coopen_t));
		*prt=call->port;
	}
	return 0;
}

int cosend(coport_t * port, const void * buf, size_t len)
{
	unsigned int old_end;
	_Atomic(void *) msg_cap;
	void ** dest_buf;
	unsigned int used_space = port->end-port->start;

	//we need some atomicity on changes toe end and start
	switch(port->type)
	{
		case COCHANNEL:
			//doesn't measure circular properly?
			if((port->length-used_space)<len)
			{
				err(1,"message too big/buffer is full");
			}
			old_end=port->end;
			port->end=port->end+len;
			memcpy((char *)port->buffer+old_end, buf, len);
			break;
		case COCARRIER:
			
			//map buffer of size len
			//POSSIBLE MEMORY LEAK HERE THAT I WOULD LIKE TO ADDRESS
			//ALSO VM ISSUES IF SENDER EXITS EARLY
			msg_cap=calloc(len,1);
			//copy data from buf
			memcpy(msg_cap,buf,len);
			//reduce capability to buffer to read only
			msg_cap=cheri_andperm(msg_cap,COCARRIER_PERMS);
			//append capability to buffer
			old_end=port->end;
			port->end=port->end+CHERICAP_SIZE;
			dest_buf=(void **)port->buffer;
			dest_buf[old_end]=msg_cap;
			break;
		default:
			err(1,"unimplemented coport type");
	}
	return 0;
}

int corecv(coport_t * port, void ** buf, size_t len)
{
	//we need more atomicity on changes to end
	int old_start;
	unsigned char ** msg_buf;
	switch(port->type)
	{
		case COCHANNEL:
			if (port->start==port->length)
			{
				//check for synchronicity
				err(1,"no message in buffer");
			}
			if(port->length<len)
			{
				err(1,"message too big");
			}
			if(port->buffer==NULL)
			{
				err(1,"coport is closed");
			}
			old_start=port->start;
			port->start=port->start+len;
			memcpy(*buf,(char *)port->buffer+old_start, len);
			port->start=port->start+len;
			break;
		case COCARRIER:
			msg_buf = port->buffer;
			//POSSIBLE MEMORY LEAK HERE THAT I WOULD LIKE TO ADDRESS
			//len is advisory
			//perhaps we should allocate buf in COCHANNEL rather than expect 
			//an allocated buffer
			old_start=port->start/CHERICAP_SIZE;
			port->start=port->start+CHERICAP_SIZE;
			//pop next cap from buffer into buf (index indicated by start)
			*buf=msg_buf[old_start];
			msg_buf[old_start]=cheri_cleartag(msg_buf[old_start]);
			//perhaps inspect length and perms for safety
			if(cheri_getlen(buf)!=len)
			{
				warn("message length (%lu) does not match len (%lu)",cheri_getlen(buf),len);
			}
			if((cheri_getperm(buf)&(CHERI_PERM_LOAD|CHERI_PERM_LOAD_CAP))==0)
			{
				err(1,"received capability does not grant read permissions");
			}
			break;
		default:
			break;
	}
	return 0;
	
}

int coclose(coport_t * port)
{
	free(port);
	return 0;
}