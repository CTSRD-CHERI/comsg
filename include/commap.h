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
#ifndef COMMAP_H
#define COMMAP_H

#define MAX_ADDR_SIZE 104

#define FILE_POS struct { int fd; off_t offset; }

typedef void * __capability token_t;
typedef enum {REPLY_ADDR,MMAP_FILE} fd_type;


typedef struct _commap_message_header {
    size_t fd_count;
} commap_msghdr_t;


typedef struct _commap_info {
    FILE_POS; //used by the sender to reassociate tokens with its own fds
    fd_type type;
    size_t size;
    int prot;
    int flags;
} commap_info_t;

#define COMMAP_MSG_LEN(f) ( sizeof(commap_msghdr_t) + ( f * sizeof(commap_info_t) ) )
#define CMSG_BUFFER_SIZE(f) ( CMSG_SPACE(sizeof(int) * f) )

typedef struct _commap_reply {
    int sender_fd;
    off_t offset;
    token_t token;
} commap_reply_t;


typedef struct _local_mapping {
	LIST_ENTRY(_local_mapping) entries;
	FILE_POS;
	void * __capability cap;
	token_t token;
	int prot;

} lmap_t;

typedef struct _local_mapping_table {
	LIST_HEAD(,_local_mapping) maps;
} lmap_tbl_t;

//Handy for working with mmap(2) prot values vs CHERI permissions
int perms_to_prot(int prot);
int prot_to_perms(int perms);

#define GET_PROT(c) perms_to_prot(cheri_getperm(c))
#define SET_PROT(c,p) cheri_andperm(c,prot_to_perms(p))
#define HAS_PROT(a,b) ( a <= ( a & b ) )
#define HAS_PROT_PERMS(c,p) ( p <= ( GET_PROT(c) & p ) )

struct msghdr * msghdr_alloc(size_t fds);
void msghdr_free(struct msghdr * hdr);

#endif // COMMAP_H