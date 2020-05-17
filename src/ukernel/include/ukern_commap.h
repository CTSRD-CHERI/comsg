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
#ifndef UKERN_COMMAP_H
#define UKERN_COMMAP_H

#define RANDOM_LEN 3
#define U_SOCKADDR "getukernsockaddr"
#define U_COMMAP "commap"
#define RECV_FLAGS 0
#define MAX_FDS 255
#define MAX_MAP_INFO_SIZE ( MAX_FDS * sizeof(commap_info_t)  )
#define MAX_MSG_SIZE ( MAX_MAP_INFO_SIZE + sizeof(message_header) )
#define CMSG_BUFFER_SIZE ( CMSG_SPACE(sizeof(int) * MAX_FDS) )
#define TOKEN_PERMS ( CHERI_PERM_GLOBAL | CHERI_PERM_LOAD )
#define MMAP_FLAGS(f) ( ( f & ~(MAP_ANON | MAP_32BIT | MAP_GUARD MAP_STACK) ) | MAP_SHARED )
#define MAX_ADDR_SIZE 104

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


void ukern_mmap(void *args);

#endif