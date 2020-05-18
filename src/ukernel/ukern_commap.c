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
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "commap.h"
#include "ukern_commap.h"
#include "comesg_kern.h"
#include "ukern_params.h"
#include "coproc.h"

static int rsock_fd;
static const char * path_prefix = "/tmp/ukern.";
static char bind_path[MAX_ADDR_SIZE] = "/tmp/ukern.";
static struct sockaddr_un *rsock_addr;
static int page_size;

static void * __capability token_unseal_cap;
static otype_t token_seal_cap;
static long token_otype;

static struct ukern_mapping_table mmap_tbl;


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
    rand_string(rand,RANDOM_LEN);
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
    rsock_addr=malloc(sizeof(struct sockaddr_un));
    memset(rsock_addr,0,sizeof(struct sockaddr_un));
    rsock_addr->sun_family=AF_UNIX;

    for(;;)
    {
        generate_path();        
        strncpy(rsock_addr->sun_path,bind_path,sizeof(rsock_addr->sun_path)-1);       
        error=bind(rsock_fd,rsock_addr,SUN_LEN(rsock_addr));
        if(error==0)
        {
            atexit(remove_sockf);
            break;
        }
    }
    error=listen(rsock_fd,WORKER_COUNT); //queue length so that you are waiting on at most one op to complete (not strictly true, but close)
    if(error!=0)
        perror("Error: listen(2) failed on rsock_fd");
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
token_t generate_token(struct ukern_mapping * m)
{
    token_t t;
    t=cheri_csetbounds(m,sizeof(struct ukern_mapping));
    t=cheri_andperm(t,TOKEN_PERMS);
    t=cheri_seal(t,);

    return t;
}

static
int map_fds(commap_info_t * params, int * fds, struct ukern_mapping **fd_mappings, int * reply_fd, int len)
{
    struct ukern_mapping *new_m, **mapped_fds;
    int mapped=0;
    *fd_mappings=calloc(len,sizeof(struct ukern_mapping*));
    for(int i = 0; i<len; ++i)
    {
        if (fds[i]<0)
            continue;
        else if (params[i].type==REPLY_ADDR)
        {
            *reply_fd=fds[i];
            continue;
        }

        new_m=malloc(sizeof(struct ukern_mapping));
        new_m->fd=fds[i];
        new_m->token=generate_token(new_m);
        new_m->refs=0;
        new_m->offset=params[i].offset;
        new_m->map_cap=mmap(0,params[i].size,params[i].prot,params[i].flags,fds[i],params[i].offset);
        if (new_m->map_cap == MAP_FAILED){
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
int process_cmsgs(struct msghdr *m, int * fda, int expected_fds)
{
    struct cmsghdr* c_msg;
    int fd_bytes,fd_count;

    if (expected_fds>MAX_FDS)
    {
        err(1,"Error: Supplied too many fds\n");
        return -1;
    }

    fda=calloc(MAX_FDS,sizeof(int));
    fd_count=0;

    for(c_msg = CMSG_FIRSTHDR(m); c_msg; c_msg = CMSG_NXTHDR(m, c_msg))
    {
        if(!(c_msg->cmsg_level == SOL_SOCKET) || !(c_msg->cmsg_type == SCM_RIGHTS)) 
        {
            printf("Error: Invalid control message type %d\n",c_msg->cmsg_type);
            continue;
        }
        fd_bytes=(c_msg->cmsg_len - CMSG_LEN(0));
        memcpy(fda+fd_count,CMSG_DATA(c_msg),fd_bytes);
        fd_count+=fd_bytes/sizeof(int);
    }
    if(fd_count!=expected_fds)
    {
        warn("Warning: fds in ancillary data (%d) did not match expected (%d)",fd_count,expected_fds);
    }
    return fd_count;
}

static
void do_reply(int fd, struct ukern_mapping **mapped, commap_info_t *params, int len)
{
    int reply_idx = 0;
    commap_reply_t * reply = calloc(len,sizeof(commap_reply_t));
    for(int i = 0; i < len; ++i)
    {
        if(params[i].type==REPLY_ADDR)
            continue;
        reply[reply_idx].sender_fd=params[i].fd;
        reply[reply_idx].sender_fd=params[i].offset;
        reply[reply_idx].token=mapped[reply_idx];
        reply_idx++;
    }
    write(fd, reply, len * sizeof(commap_reply_t));
    close(fd);
}

static
void process_msg(struct msghdr *msg)
{
    void * raw_data;
    commap_msghdr_t header;
    commap_info_t *map_params;
    int *fds, reply, fd_count;
    struct ukern_mapping **mapped_fds;

    raw_data=msg->msg_iov[0].iov_base;
    memcpy(&header,raw_data,sizeof(commap_msghdr_t));
    raw_data+=sizeof(commap_msghdr_t);

    map_params=calloc(header.fd_count,sizeof(commap_info_t));
    memcpy(map_params,raw_data,sizeof(commap_info_t)*header.fd_count);

    fd_count=process_cmsgs(msg,fds,header.fd_count);
    fd_count=MIN(fd_count,header.fd_count);
    map_fds(map_params,fds,mapped_fds,&reply,fd_count);

    do_reply(reply,mapped_fds,map_params,fd_count);
    free(mapped_fds);
    free(map_params);
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
        msg=msghdr_alloc(MAX_FDS);
        s=accept(rsock_fd,NULL,NULL);
        if(s==-1)
            perror("Error: accept(2) failed on rsock_fd");
        if(recvmsg(s,msg,RECV_FLAGS)==-1)
        {
            perror("Error: recvmsg(2) failed on s");
            break;
        }
        else if(msg->msg_controllen==0)
        {
            //drop connection, invalid.
            printf("Error: communicating socket did not send any ancillary data.\n");
        }
        else
        {
            process_msg(msg);
        }
        close(s);
        msghdr_free(msg);
    }
    return args;
}

static
void *co_mmap(void *args)
{
    void * __capability code;
    void * __capability data;
    void * __capability cookie = 0;
    void * __capability target;


    struct ukern_mapping *map;
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
        if (cheri_gettype(commap_args->token)==token_otype)
        {
            map=cheri_unseal(commap_args->token,token_unseal_cap);
            if (map->token!=commap_args->token)
            {
                commap_args->status=-1;
                commap_args->error=EINVAL;
                continue;
            }
            commap_args->cap=map->map_cap;
        }
        
        if(commap_args->cap==NULL)
        {
            commap_args->status=-1;
            commap_args->error=EINVAL;
            continue;
        }
        prot=GET_PROT(commap_args->cap);
        prot&=commap_args->prot;
        if((commap_args->prot-prot)!=0)
        {
            commap_args->cap=NULL;
            commap_args->status=-1;
            commap_args->error=EACCES;
            continue;
        }
        commap_args->cap=SET_PROT(commap_args->cap,prot);
        commap_args->status=0;
        commap_args->errno=0;
        //flags not included
    }
    return args;
}

static
void mmap_tbl_init(void)
{
    void __capability root_cap;
    
    LIST_INIT(&mmap_tbl.mappings);
    mmap_tbl.count=0;
    error+=sysarch(CHERI_GET_SEALCAP,&root_cap);
    token_seal_cap=cheri_maketype(root_cap,TOKEN_OTYPE);
    token_otype=cheri_gettype(cheri_seal(token_seal_cap,token_seal_cap));
    token_unseal_cap=cheri_setoffset(root_cap,TOKEN_OTYPE);

    root_cap=cheri_unseal(cheri_seal(token_seal_cap,token_seal_cap),token_unseal_cap); //test

    return;
}

void *ukern_mmap(void *args)
{

    //pthread_t sock_advertiser_threads[WORKER_COUNT];
    pthread_t sock_advertiser;

    pthread_t commap_threads[WORKER_COUNT];
    pthread_t commap_handler;

    pthread_attr_t thread_attrs;

    request_handler_args_t * handler_args;

    mmap_tbl_init();
    sock_init();
    page_size=getpagesize();
    //spawn_workers(&advertise_sockaddr,sock_advertiser_threads,U_SOCKADDR);
    spawn_workers(&commap,commap_threads,U_COMMAP);

    pthread_attr_init(&thread_attrs);
    pthread_create(&sock_advertiser,&thread_attrs,advertise_sockaddr,NULL);

    handler_args=malloc(sizeof(request_handler_args_t));
    strcpy(handler_args->func_name,U_COMMAP);
    pthread_attr_init(&thread_attrs);
    pthread_create(&commap_handler,&thread_attrs,manage_requests,handler_args);

    pthread_join(commap_threads[0],NULL);

    return args;
}