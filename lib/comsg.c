#include <cheri.h>
#include <cheri/cherireg.h>
#include <unistd.h>

#include "coproc.h"
#include "coport.h"
#include "comsg.h"

int coopen(const char * coport_name, coport_type_t type, coport_t * prt)
{
	/* request new coport from microkernel */
	void * __capability switcher_code;
	void * __capability switcher_data;
	void * __capability ukern_coopen;


	cocall_coopen_t call;
	cocall_lookup_t lookup;

	uint error;

	/* cocall setup */
	//TODO-PBB: Only do this once.
	error=ukern_lookup(switcher_code,switcher_data,U_COOPEN,ukern_coopen);

	strcpy(call.args.name,coport_name);
	call.args.type=type;

	error=cocall(switcher_code,switcher_data,ukern_coopen,&call,sizeof(call));
	prt=call.port;

	return 0;
}

int cosend(coport_t * port, const void * __capability buf, size_t len)
{
	unsigned int old_end;
	//we need some atomicity on changes toe end and start
	if(port->type==COCHANNEL)
	{
		if((port->length-(end-start))<len)
		{
			err(1,"message too big/buffer is full");
		}
		old_end=port->end;
		port->end=port->end+len;
		memcpy(port->buffer+old_end, buf, len);
	}
	else if(port->type=COCARRIER)
	{
		void * __capability __capability msg_cap;
		void * __capability msg_buf;
		//map buffer of size len
		//POSSIBLE MEMORY LEAK HERE THAT I WOULD LIKE TO ADDRESS
		msg_buf=(void *) malloc(len);
		//copy data from buf
		memcpy(msg_cap,buf,len)
		//reduce capability to buffer to read only
		msg_cap=cheri_andperms(message_buffer,CHERI_PERM_LOAD|CHERI_PERM_LOAD_CAP);
		//append capability to buffer
		old_end=port->end;
		port->end=port->end+CHERICAP_SIZE;
		port->buffer+old_end=msg_cap;

	}
	return 0;
}

int coreceive(coport_t * port, void * __capability buf, size_t len)
{
	//we need more atomicity on changes to end
	int old_start;
	if(port->type==COCHANNEL)
	{
		if(port->length<len)
		{
			err(1,"message too big");
		}
		if(port->buffer==NULL)
		{
			err(1,"coport %s is closed",port->name);
		}
		old_start=port->start;
		port->start=port->start+len;
		memcpy(buf,port->buffer+old_start, len);
		port->start=port->start+len;
	}
	else if(port->type==COCARRIER)
	{
		//POSSIBLE MEMORY LEAK HERE THAT I WOULD LIKE TO ADDRESS
		//len is advisory
		//perhaps we should allocate buf in COCHANNEL rather than expect 
		//an allocated buffer
		old_start=port->start;
		port->start=port->start+CHERICAP_SIZE;
		//pop next cap from buffer into buf (index indicated by start)
		buf=port->buffer+old_start;
		port->buffer+old_start=NULL;
		//perhaps inspect length and perms for safety
		if(cheri_getlen(buf)!=len)
		{
			warn("message length (%lu) does not match len (%lu)",cheri_getlen(buf),len);
		}
		if(cheri_getperms(buf)&(CHERI_PERM_LOAD|CHERI_PERM_LOAD_CAP)==0)
		{
			err(1,"received capability does not grant read permissions");
		}
	}
	return 0;
	
}

int coclose(coport_t * port)
{
	port->buffer=NULL;
	return 0;
}