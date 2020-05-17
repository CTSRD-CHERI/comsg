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
#include <cheri/cheric.h>
#include <cheri/cherireg.h>

#include <errno.h>
#include <err.h>
#include <pthread.h>
#include <queue.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/param.h>
#include <sys/params.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "comesg_kern.h"
#include "ukern_params.h"

#define RANDOM_LEN 3
#define U_SOCKADDR "getukernsockaddr"
#define U_COMMAP "commap"
#define RECV_FLAGS 0
#define MAX_FDS 255
#define MAX_MAP_INFO_SIZE ( MAX_FDS * sizeof(struct map_info)  )
#define MAX_MSG_SIZE ( MAX_MAP_INFO_SIZE + sizeof(message_header) )
#define CMSG_BUFFER_SIZE ( CMSG_SPACE(sizeof(int) * MAX_FDS) )
#define TOKEN_PERMS ( CHERI_PERM_GLOBAL | CHERI_PERM_LOAD )
#define MMAP_FLAGS(f) ( ( f & ~(MAP_ANON | MAP_32BIT | MAP_GUARD MAP_STACK) ) | MAP_SHARED )
#define MAX_ADDR_SIZE 104

static int rsock_fd;
static const char path_prefix = "/tmp/ukern.";
static char bind_path[MAX_ADDR_SIZE] = "/tmp/ukern.";
static sockaddr_un rsock_addr;

typedef long int token_t; //TODO-PBB: REPLACE

struct mapping {
    LIST_ENTRY(mapping) entries;
    token_t token;
    int fd;
    void * __capability map_cap;
    _Atomic int refs;
};

struct mapping_table {
    LIST_HEAD(,struct mapping) mappings;
    _Atomic uint count;
};

static struct mapping_table mmap_tbl;

typedef enum {REPLY_ADDR,MMAP_FILE} fd_type;
struct map_info {
    int sender_fd; //used by the sender to reassociate tokens with its own fds
    fd_type type;
    size_t size;
    int prot;
    int offset;
    int flags;
};

struct map_reply {
    int sender_fd;
    token_t mmap_token;
};

struct message_header {
    size_t fd_count;
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

    char path[MAX_ADDR_SIZE];

    int error;
    coaccept_init(&code,&data,U_SOCKADDR,&target);
    for (;;)
    {
        coaccept(code,data,&cookie,path,sizeof(path));
        strncpy(path,bind_path,MAX_ADDR_SIZE);
    }

    return args;
}

static
token_t generate_token(struct mapping * m)
{
    token_t t;
    t=cheri_setbounds(m,sizeof(struct mapping));
    t=cheri_andperm(t,TOKEN_PERMS);
    t=cheri_seal(t,seal_cap);

    return t;
}

static
int map_fds(struct map_info * params, int * fds, struct mapping ***fd_mappings, int * reply_fd, int len)
{
    struct mapping *new_m, **mapped_fds;
    int mapped=0;
    *fd_mappings=calloc(len,sizeof(*struct mapping));
    for(int i = 0; i<len; ++i)
    {
        if (fds[i]<0)
            continue;
        else if (params[i].type==REPLY_ADDR)
        {
            *reply_fd=fds;
            continue;
        }

        new_m=malloc(sizeof(struct mapping));
        new_m.fd=fds[i];
        new_m.token=generate_token(new_m);
        new_m.refs=0;
        new_m.map_cap=mmap(0,params[i].size,params[i].prot,fds[i],params[i].offset);
        if (new_m.map_cap == MAP_FAILED){
            perror("Error: mapping fd failed");
            free(new_m);
            continue;
        }
        LIST_INSERT_HEAD(&mmap_tbl.mappings,new_m,entries);

        fd_mappings[mapped]=new_m;
        mapped++;
    }
    mmap_tbl.count+=mapped;
    return mapped;
}

static 
int process_cmsgs(struct msghdr m, int * fda, int expected_fds)
{
    struct cmsghdr* c_msg;
    int reply = -1;
    int fd_bytes,fd_count;

    if (expected_fds>MAX_FDS)
    {
        err(1,"Error: Supplied too many fds\n");
        return;
    }

    fda=calloc(MAX_FDS,sizeof(int));
    fd_count=0;

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
        fd_bytes=(c_msg->cmsg_len - CMSG_LEN(0));
        memcpy(fda+fd_count,CMSG_DATA(c_msg),fd_bytes);
        fd_count+=fd_bytes/sizeof(int);
    }
    if(reply==-1)
    {
        err(1,"No socket supplied for reply");
        return;
    }
    else
        fd_count++;
    if(fd_count!=fds)
    {
        warn("Warning: fds in ancillary data (%d) did not match expected (%d)",fd_count,expected_fds);
    }
    return fd_count;
}

static
void do_reply(int fd, struct mapping **mapped,struct map_info *params,int len)
{
    int reply_idx = 0;
    struct map_reply * reply = calloc(len,sizeof(struct map_reply));
    for(int i = 0; i < len; ++i)
    {
        if(params[i].fd_type==REPLY_ADDR)
            continue;
        reply[reply_idx].sender_fd=params[i].sender_fd;
        reply[reply_idx].token=mapped[reply_idx];
        reply_idx++;
    }
    write(fd,reply,len*sizeof(struct map_reply));
    close(fd);
}

static
void process_msg(struct msghdr msg)
{
    void * raw_data;
    struct message_header header;
    struct map_info *map_params;
    int *fds, reply, fd_count;
    struct mapping **mapped_fds;

    raw_data=msg.iov[0].iov_base;
    memcpy(&header,raw_data,sizeof(struct message_header));
    raw_data+=sizeof(struct message_header);

    map_params=calloc(message_header.fd_count,sizeof(struct map_info));
    memcpy(map_params,raw_data,sizeof(struct map_info)*message_header.fd_count);

    fd_count=process_cmsgs(msg,message_header.fd_count);
    fd_count=MIN(fd_count,message_header.fd_count);
    map_fds(map_params,fds,&mapped_fds,&reply,fd_count);

    do_reply(reply,mapped_fds,map_params,fd_count);
    free(mapped_fds);
    free(map_params);
    return;
}

static
struct msghdr * header_alloc()
{
    struct msghdr *hdr;
    struct iovec *iov;

    hdr=malloc(sizeof(struct msghdr));
    memset(hdr,0,sizeof(struct msghdr));
    
    iov=calloc(1,sizeof(struct iovec));
    iov[0].iov_base=malloc(MAX_MSG_SIZE);
    iov[0].iov_len=MAX_MSG_SIZE;
    memset(iov[0].iov_base,0,MAX_MSG_SIZE);

    hdr->msg_control = malloc(CMSG_BUFFER_SIZE);
    hdr->msg_controllen = CMSG_BUFFER_SIZE;
    memset(hdr.msg_control, 0, CMSG_BUFFER_SIZE);
    
    hdr->msg_iov=iov;
    hdr->msg_iovlen=1;

    return hdr;
}

static
void header_free(struct msghdr * hdr)
{
    for (int i = 0; i < hdr->msg_iovlen; ++i)
    {
        free(hdr->iov[i].iov_base);
    }
    free(hdr->iov);
    free(hdr->msg_control);
    free(hdr);
    return;
}

static
void *getfds(void *args)
{
    int s, fd;
    int rlen;
    struct msghdr *msg;
    
    for(;;)
    {
        msg=header_alloc();
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
            printf("Error: communicating socket did not send any ancillary data.\n");
        }
        else
        {
            process_msg(msg);
        }
        close(s);
        header_free(msg);
    }
    return args;
}

#define get_prot(c) perms_to_prot(cheri_getperm(c))
#define set_prot(c,p) cheri_andperm(c,prot_to_perms(p))

static 
int perms_to_prot(int prot);
static 
int prot_to_perms(int perms);


static
void *commap(void *args)
{
    void * __capability code;
    void * __capability data;
    void * __capability cookie = 0;
    void * __capability target;

    struct mapping *map, *map_temp;
    commap_args_t * commap_args;
    //char path[MAX_ADDR_SIZE];

    int error, prot;
    worker_args_t * arg_data = args;

    commap_args=calloc(1,sizeof(commap_args_t));

    coaccept_init(&code,&data,arg_data->name,&target);
    arg_data->cap=target;
    update_worker_args(arg_data,U_COMMAP);
    for (;;)
    {
        coaccept(code,data,&cookie,commap_args,sizeof(commap_args));
        commap_args->cap=NULL;
        LIST_FOREACH_SAFE(map, &mmap_tbl.mappings, entries, map_temp) {
            if(commap_args->token==map->token) {
                commap_args->cap=map->map_cap;
                break;
            }
        }
        if(commap_args->cap==NULL)
        {
            commap_args->status=MAP_FAILED;
            commap_args->error=EINVAL;
            continue;
        }
        prot=get_prot(commap_args->cap);
        prot&=commap_args->prot;
        if((commap_args->prot-prot)!=0)
        {
            commap_args->cap=NULL;
            commap_args->status=MAP_FAILED;
            commap_args->error=EACCES;
            continue;
        }
        commap_args->cap=set_prot(commap_args->cap,prot);
        //flags not included
    }

    return args;
}

static
void mmap_tbl_init(void)
{
    LIST_INIT(&mmap_tbl.mappings);
    mmap_tbl.count=0;
    return;
}

void ukern_mmap(void *args)
{

    //pthread_t sock_advertiser_threads[WORKER_COUNT];
    pthread_t sock_advertiser;

    pthread_t commap_threads[WORKER_COUNT];
    pthread_t commap_handler;

    pthread_attr_t thread_attrs;

    request_handler_args_t * handler_args;

    mmap_tbl_init();
    sock_init();

    //spawn_workers(&advertise_sockaddr,sock_advertiser_threads,U_SOCKADDR);
    spawn_workers(&commap,commap_threads,U_COMMAP);

    pthread_attr_init(&thread_attrs);
    pthread_create(&sock_advertiser,&thread_attrs,advertise_sockaddr,NULL);

    handler_args=malloc(sizeof(request_handler_args_t));
    strcpy(handler_args->func_name,U_COMMAP);
    pthread_attr_init(&thread_attrs);
    pthread_create(&commap_handler,&thread_attrs,manage_requests,handler_args);

    
}