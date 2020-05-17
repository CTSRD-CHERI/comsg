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
//TODO-PBB:REFACTOR THIS MESS
#include <stdatomic.h>

#include <errno.h>
#include <err.h>

#include <pthread.h>
#include <queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/params.h>
#include <sys/un.h>
#include <unistd.h>

#include "comesg_kern.h"
#include "ukern_params.h"

#define RANDOM_LEN 3
#define U_SOCKADDR "getukernsockaddr"
#define RECV_FLAGS 0
#define MAX_FDS 255
#define MAX_MSG_SIZE ( MAX_FDS * sizeof(struct map_info) )

static int rsock_fd;
static const char path_prefix = "/tmp/ukern.";
static char bind_path[MAXPATHLEN] = "/tmp/ukern.";
static sockaddr_un rsock_addr;

struct mapping {
	LIST_ENTRY(mapping) entries;
	long int token;
	int fd;
	void * __capability map_cap;
	_Atomic int refs;
};

struct mapping_table {
	LIST_HEAD(,struct mapping) mappings;
};

static struct mapping_table mmap_tbl;

typedef enum {REPLY_ADDR,MMAP_FILE} fd_type;
struct map_info {
	fd_type type;
	size_t size;
};

static
void remove_sockf(void)
{
	if (strcmp(path_prefix,bind_path)!=0)
		unlink(bind_path);
}

static
int generate_path(void)
{
	char * rand;
	rand_string(rand,RANDOM_LEN)
	strcpy(bind_path,path_prefix);
	strcat(bind_path,rand);
	free(rand);
	return strlen(bind_path);
}

static
void sock_init(void)
{
	int error;
	
	rsock_fd=socket(PF_UNIX,SOCK_STREAM,0);
	memset(&rsock_addr,0,sizeof(rsock_addr));
	rsock_addr.sun_family=AF_UNIX;

	for(;;)
		generate_path();		
		strncpy(rsock_addr.sun_path,bind_path,sizeof(rsock_addr.sun_path)-1);		
		error=bind(rsock_fd,rsock_addr,SUN_LEN(&rsock_addr));
		if(error==0)
			atexit(remove_sockf);
			break;
	error=listen(rsock_fd,WORKER_COUNT); //queue length so that you are waiting on at most one op to complete (not strictly true, but close)
	if(error!=0)
		perror("Error: listen(2) failed on rsock_fd");
	return error;
}

static
void *advertise_sockaddr(void *args)
{
	void * __capability code;
	void * __capability data;
	void * __capability cookie = 0;
    void * __capability target;

    char path[MAXPATHLEN];

    int error;
    coaccept_init(&code,&data,U_SOCKADDR,&target);
    for (;;)
    {
    	coaccept(code,data,target,path,sizeof(path));
    	strcpy(path,bind_path);
    }

    return args;
}

static
int map_fds(int * fd, int len)
{
	struct mapping *new;
	new_m=calloc(len,sizeof(struct mapping));
	for(int i = 0; i<len; ++i)
	{
		new_m[i].fd=fd[i];
		
	}
}

static 
void process_cmsgs(struct msghdr m)
{
	struct cmsghdr* c_msg;
	int reply = -1;
	int fdc;
	int * fda;

	for(c_msg = CMSG_FIRSTHDR(&m); c_msg; c_msg = CMSG_NXTHDR(&m, c_msg))
	{
		if(!(c_msg->cmsg_level == SOL_SOCKET) || (!c_msg->cmsg_type == SCM_RIGHTS)) 
		{
			printf("Error: Invalid control message type %d\n",c_msg->cmsg_type);
			continue;
		}
		if (cmsg->cmsg_len == CMSG_LEN(sizeof(int))&&(reply==-1))
		{
			memcpy(&reply,CMSG_DATA(c_msg),sizeof(int));
			continue;
		}
		fdc=c_msg->cmsg_len - CMSG_LEN(0);
		fda=calloc(fdc,sizeof(int));
		memcpy(fda,CMSG_DATA(c_msg));
	}
}

static
struct msghdr * header_alloc()
{
	struct msghdr *hdr;
	struct iovec *iov;

	hdr=malloc(sizeof(struct msghdr));
	memset(hdr,0,sizeof(struct msghdr));
	iov=calloc(1,sizeof(struct iovec));
	iov[0].iov_base=calloc(MAX_FDS,sizeof(struct map_info));
	iov[0].iov_len=MAX_MSG_SIZE;

	hdr.msg_controllen = CMSG_SPACE(sizeof(int) * MAX_FDS);
	hdr.msg_control = malloc(CMSG_SPACE(sizeof(int) * MAX_FDS));
	memset(hdr.msg_control,0,CMSG_SPACE(sizeof(int) * MAX_FDS) );
	hdr.msg_iov=iov;
	hdr.msg_iovlen=1;

}

static
void *getfds(void *args)
{
	int s, fd;
	int rlen;
	char buf[64];
	struct msghdr msg;
	struct iovec iov[1];

	for(;;)
	{
		iov[0].iov_base=buffer;
		iov[0].iov_len=sizeof(buffer);
		


		s=accept(rsock_fd,NULL,NULL);
		if(s==-1)
			perror("Error: accept(2) failed on rsock_fd");
		if(recvmsg(s,&msg,RECV_FLAGS)==-1)
		{
			perror("Error: recvmsg(2) failed on s");
			break;
		}
		else if(msg.msg_controllen==0)
		{
			//drop connection, invalid.
			printf("Error: communicating socket did not send a file descriptor.\n");
			close(s);
			continue;
		}

	}

    return args;
}

static
void *commap(void *args)
{
	void * __capability code;
	void * __capability data;
	void * __capability cookie = 0;
    void * __capability target;

    char path[MAXPATHLEN];

    int error;
    coaccept_init(&code,&data,U_SOCKADDR,&target);
    for (;;)
    {
    	coaccept(code,data,target,path,sizeof(path));
    	strcpy(path,bind_path);
    }

    return args;
}

void ukern_mmap(void *args)
{
	pthread_t sock_advertiser;
	pthread_attr_t thread_attrs;

	sock_init();

	pthread_attr_init(&thread_attrs);
	pthread_create(&sock_advertiser,&thread_attrs,advertise_sockaddr,NULL);

	LIST_INIT(&mmap_tbl.mappings);
}