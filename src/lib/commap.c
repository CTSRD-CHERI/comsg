/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#define COMMAP_C

#include "commap.h"
#include "sys_comsg.h"
#include "coproc.h"

#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <poll.h>
#include <cheri/cherireg.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static lmap_tbl_t map_tbl;
static pthread_key_t replyfd;
static pthread_once_t init_replyfd_once = PTHREAD_ONCE_INIT;
static struct sockaddr_un *ukernel_sock_addr = NULL;

inline
int perms_to_prot(int perms)
{
	int prot = 0;

	if (perms & (CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP))
		prot |=  PROT_READ;
	if (perms & (CHERI_PERM_STORE | CHERI_PERM_STORE_CAP | CHERI_PERM_STORE_LOCAL_CAP))
		prot |= PROT_WRITE;
	if (perms & CHERI_PERM_EXECUTE )
		prot |= PROT_EXEC;
			   

	return prot;
}

inline
int prot_to_perms(int prot)
{
	int perms = 0;

	if (prot & PROT_READ)
		perms |= CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP;
	if (prot & PROT_WRITE)
		perms |= CHERI_PERM_STORE | CHERI_PERM_STORE_CAP |
		CHERI_PERM_STORE_LOCAL_CAP;
	if (prot & PROT_EXEC)
		perms |= CHERI_PERM_EXECUTE;

	return perms;
}

static 
void set_cloexec(token_t token)
{
	lmap_t *map, *map_temp;
	LIST_FOREACH_SAFE(map, &map_tbl.maps, entries, map_temp) {
		if (map->token==token) 
		{
			map->cloexec=1;
		}
	}
}

static
void * __capability map_from_token(token_t token, off_t offset, int prot)
{
	lmap_t *map, *map_temp;
	LIST_FOREACH_SAFE(map, &map_tbl.maps, entries, map_temp) {
		if (map->token==token && map->offset==offset && map->cap != NULL) 
		{
			//check against what we actually have
			if(HAS_PROT_PERMS(map->cap,prot))
				return (SET_PROT(map->cap,prot));
			//keep looking, we might've mapped it again with higher permissions.
		}
	}
	return NULL;
}

/*
static
token_t token_from_fd(int fd, off_t offset, int prot)
{
	lmap_t *map, *map_temp;
	int matchness = 0;
	int temp_matchness = 0;
	token_t found = NULL;
	LIST_FOREACH_SAFE(map, &map_tbl.maps, entries, map_temp) {
		if (map->offset==offset) 
			temp_matchness++;
		if (HAS_PROT(map->prot,prot))
			temp_matchness++;
		if (map->fd!=fd)
			temp_matchness=-1;
		if (temp_matchness == 2)
			return map->token;
		else if (temp_matchness > matchness)
		{
			matchness=temp_matchness;
			found=map->token;
			//keep looking, we might've mapped it again with higher permissions.
		}
		temp_matchness=0;
	}
	return found;
}
*/
static
void add_token_to_table(token_t token, int fd, off_t offset,int prot)
{
	lmap_t * entry = calloc(1,sizeof(lmap_t));
	entry->token=token;
	entry->fd=fd;
	entry->offset=offset;
	entry->prot=prot;
	LIST_INSERT_HEAD(&map_tbl.maps,entry,entries);
	map_tbl.count++;
}

static
void add_cap_to_table(token_t token, void * __capability cap)
{
	int added = 0;
	lmap_t *map, *map_temp;
	LIST_FOREACH_SAFE(map, &map_tbl.maps, entries, map_temp) {
		if (map->token==token) 
		{
			if (HAS_PROT_PERMS(cap,map->prot))
			{
				added=1;
				map->cap=cap;
			}
			//keep looking, we might've mapped it more times
		}
	}
	if(!added)
	{
		add_token_to_table(token,-1,-1,GET_PROT(cap));
		add_cap_to_table(token,cap);
	}
	return;
}


static inline
commap_info_t make_commap_info(void * __capability base, size_t size, int prot, int flags, int fd, off_t offset)
{
	commap_info_t i;
	i.fd=fd;
	i.offset=offset;
	i.flags=flags;
	i.prot=prot;
	i.size=size;
	base=NULL; //unused
	return i;
}

static inline 
commap_info_t make_reply_info(int fd)
{
	commap_info_t i = make_commap_info(0,0,0,0,fd,0);
	i.type=REPLY_ADDR;
	i.cloexec=0;
	return i;
}

extern inline 
commap_info_t make_fd_info(void * __capability base, size_t size, int prot, int flags, int fd, off_t offset)
{
	commap_info_t i = make_commap_info(base,size,prot,flags,fd,offset);
	i.type=MMAP_FILE;
	i.cloexec=0;
	return i;
}

static void 
clear_table_after_fork(void)
{
	lmap_t *map, *map_temp;
	map=LIST_FIRST(&map_tbl.maps);
	while (map!=NULL) {
		map_temp = LIST_NEXT(map,entries);
		free(map);
		map=map_temp;
	}
	close_replyfd();
}

static void 
clear_table_at_exit(void)
{
	lmap_t *map, *map_temp;
	map=LIST_FIRST(&map_tbl.maps);
	while (map!=NULL) {
		map_temp = LIST_NEXT(map,entries);
		_comunmap(map->token);
		free(map);
		map=map_temp;
	}
	close_replyfd();

}

static void
close_replyfd(void)
{
	int * fd;
	fd=pthread_getspecific(replyfd);
	if(fd==NULL)
		return;
	close(fd[0]);
	close(fd[1]);
	free(fd);
	pthread_setspecific(replyfd,NULL);
	return;
}

static void 
init_replyfd(void)
{
	pthread_key_create(&replyfd,free);
}


static 
int * get_replyfd(void)
{
	int * fd;
	int error;
	
	if (pthread_getspecific(replyfd))
		return ((int *)pthread_getspecific(replyfd));
	else
	{
		fd=calloc(2,sizeof(int));
		error=socketpair(PF_LOCAL, SOCK_STREAM, 0, fd);
		if(error)
			perror("Error: socketpair(2) FAILED for commap");
		pthread_setspecific(replyfd,fd);
		return fd;
	}

}

static
void * __capability map_token(token_t token, int prot)
{
	void * __capability switcher_code;
    void * __capability switcher_data;
    void * __capability func;

    void * __capability cap = NULL;
	commap_args_t * call;
	
	int error;
	int status;

	call=calloc(1,sizeof(commap_args_t));
	call->token=token;
	call->prot=prot;
    
    error=ukern_lookup(&switcher_code,&switcher_data,U_COMMAP,&func);
    error+=cocall(switcher_code,switcher_data,func,call,sizeof(commap_args_t));
    
    cap=call->cap;
    error=call->error;
    status=call->status;
    free(call);

    if(status==-1)
    {
    	errno=error;
    	perror("Error: commap failed");
    	return NULL;
    }
  	add_cap_to_table(token,cap);
    return (cap);

}

static
void get_ukerneladdr(void)
{
	void * __capability switcher_code;
    void * __capability switcher_data;
    void * __capability func;
    char path[MAX_ADDR_SIZE];

    ukernel_sock_addr=calloc(1,sizeof(struct sockaddr_un));
    ukernel_sock_addr->sun_family=AF_UNIX;

	cosetup(COSETUP_COCALL,&switcher_code,&switcher_data);
	colookup(U_SOCKADDR,&func);

	cocall(switcher_code,switcher_data,func,path,sizeof(path));
	strncpy(ukernel_sock_addr->sun_path,path,MAX_ADDR_SIZE);

	return;
}

static
int get_ukernelfd(void)
{
	int fd;
	if (ukernel_sock_addr==NULL)
		get_ukerneladdr();
	fd=socket(PF_LOCAL,(SOCK_STREAM | SOCK_NONBLOCK), 0);
	
	if(connect(fd,(struct sockaddr *)ukernel_sock_addr,SUN_LEN(ukernel_sock_addr))==-1)
		err(errno,"Error: connect(2) to ukernel socket at %s failed",ukernel_sock_addr->sun_path);
	return fd;

}

extern
struct msghdr * msghdr_alloc(size_t fds)
{
	struct msghdr *hdr;
	struct iovec *iov;
	size_t body_len = COMMAP_MSG_LEN(fds);

	hdr=calloc(1,sizeof(struct msghdr));
	
	iov=calloc(1,sizeof(struct iovec));
	iov[0].iov_base=malloc(body_len);
	iov[0].iov_len=body_len;
	
	hdr->msg_iov=iov;
	hdr->msg_iovlen=1;

	hdr->msg_control=malloc(CMSG_BUFFER_SIZE(fds));
	hdr->msg_controllen=CMSG_BUFFER_SIZE(fds);
	memset(hdr->msg_control,0,CMSG_BUFFER_SIZE(fds));

	return hdr;
}

extern
void msghdr_free(struct msghdr * hdr)
{
    for (int i = 0; i < hdr->msg_iovlen; ++i)
    {
        free(hdr->msg_iov[i].iov_base);
    }
    free(hdr->msg_iov);
    free(hdr->msg_control);
    free(hdr);
    return;
}

static 
void make_message(commap_info_t * r, size_t len, struct msghdr * hdr)
{
	unsigned char * dest;
	commap_msghdr_t h;
	int * fd_dest;


	//make message body
	h.fd_count = len;
	dest = hdr->msg_iov[0].iov_base;
	memcpy(dest,&h,sizeof(commap_msghdr_t));
	dest+=sizeof(commap_msghdr_t);
	memcpy(dest,r,len*sizeof(commap_info_t));

	//make ancillary data
	struct cmsghdr* cmsg = CMSG_FIRSTHDR(hdr);
	cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_BUFFER_SIZE(len);
    fd_dest = CMSG_INT_DATA(cmsg);
    for(size_t i = 0; i < len; ++i) {
    	fd_dest[i]=r[i].fd;
    }
    return;
}


extern
token_t request_token(commap_info_t info)
{
	token_t token;
	commap_info_t reply_info, request_info[2];
	commap_reply_t * reply;
	struct msghdr * msg = msghdr_alloc(2);
	struct pollfd pfd;
	
	int * reply_fds = calloc(2,sizeof(int)); 
	reply = calloc(1,sizeof(commap_reply_t));
	int send_fd = get_ukernelfd();

	memcpy(reply_fds,get_replyfd(),2*sizeof(int));
	
	reply_info = make_reply_info(reply_fds[1]);
	
	request_info[0]=reply_info;
	request_info[1]=info;
	make_message(request_info,2,msg);


	while (sendmsg(send_fd,msg,0) == -1) {
		if (errno == EINTR)
            continue;
        else if (errno == EWOULDBLOCK || errno == EAGAIN) 
        {
            pfd.events = POLLOUT;
            pfd.revents = 0;
            pfd.fd = send_fd;
            poll(&pfd, 1, -1);
            continue;
        } 
        else
        	perror("Error: unable to send request to ukernel via socket");
	}

	while (read(reply_fds[0],reply,sizeof(commap_reply_t))==-1)
	{
		if (errno == EINTR)
			continue;
		else if (errno == EWOULDBLOCK || errno == EAGAIN)
		{
			pfd.events = POLLIN;
			pfd.revents = 0;
			pfd.fd = reply_fds[0];
			poll(&pfd,1,-1);
			continue;
		}
		else
			perror("Error: could not retrieve reply via fd");
	}
	if (reply[0].sender_fd != info.fd)
	{
		err(EINVAL,"Error: mismatch between sender fd from ukern and local fd");
	}
	add_token_to_table(reply[0].token,info.fd,info.offset,info.prot);
	token=reply[0].token;
	
	msghdr_free(msg);
	free(reply);
	free(reply_fds);

	return token; 
}


extern
token_t commap_reserve(void * __capability base, size_t size, int prot, int flags, int fd, off_t offset)
{

	commap_info_t request_info;
	token_t token;
	int cloexec;
	
	cloexec=fcntl(fd,F_GETFD,NULL) & FD_CLOEXEC;
	request_info=make_fd_info(base,size,prot,flags,fd,offset);
	token=request_token(request_info);
	if (cloexec)
		set_cloexec(token);
	return token;
}


extern
void * __capability commap2(token_t token,int prot)
{

	return (map_token(token,prot));
}

extern
void * __capability commap(void * __capability base, size_t size, int prot, int flags, int fd, off_t offset)
{
	void * __capability mapped_cap;
	commap_info_t request_info;
	token_t token;
	int cloexec;
	
	if (fd)
		cloexec=fcntl(fd,F_GETFD,NULL) & FD_CLOEXEC;
	else
		cloexec=0;
	request_info=make_fd_info(base,size,prot,flags,fd,offset);
	token=request_token(request_info);
	if (cloexec)
		set_cloexec(token);
	mapped_cap = map_from_token(token,offset,prot);
	//we already mapped this with this prot and offset
	if(mapped_cap)
		return mapped_cap;
	//call into microkernel and attempt use our token to retrieve the mapping
	mapped_cap=map_token(token,prot);

	return (mapped_cap);
}


static
void _comunmap(token_t token)
{
	void * __capability switcher_code;
    void * __capability switcher_data;
    void * __capability func;

	comunmap_args_t * call;
	
	int error;
	int status;

	call=calloc(1,sizeof(comunmap_args_t));
	call->token=token;
    
    error=ukern_lookup(&switcher_code,&switcher_data,U_COMUNMAP,&func);
    error+=cocall(switcher_code,switcher_data,func,call,sizeof(comunmap_args_t));
    
    error=call->error;
    status=call->status;
    free(call);

    if(status==-1)
    {
    	errno=error;
    	perror("Error: commap failed");
    	return;
    }
    return;
}

extern
void comunmap(void * __capability cap, size_t len)
{
	if(cheri_getlen(cap)<len)
	{
		err(1,"Error: comunmap: mismatch cap length and len");
	}
	else if (!cheri_gettag(cap))
	{
		err(1,"Error: comunmap: cannot unmap untagged capability");
	}
	//TODO-PBB:IMPLEMENT
	lmap_t *map, *map_temp;
	LIST_FOREACH_SAFE(map, &map_tbl.maps, entries, map_temp) {
		if (map->cap==cap) {
			_comunmap(map->token);
			LIST_REMOVE(map, entries);
			free(map);
		}
	}

}

__attribute__ ((constructor)) static 
void commap_setup(void)
{
	LIST_INIT(&map_tbl.maps);
	pthread_atfork(NULL,NULL,clear_table_after_fork);
	atexit(clear_table_at_exit);
	pthread_once(&init_replyfd_once, init_replyfd);
}

__attribute__ ((destructor)) static 
void commap_teardown(void)
{
	lmap_t *map, *map_temp;
	LIST_FOREACH_SAFE(map, &map_tbl.maps, entries, map_temp) {
		if (map->cloexec) 
			_comunmap(map->token);
	}
}
