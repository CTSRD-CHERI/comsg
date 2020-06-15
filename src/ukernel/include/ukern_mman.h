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
#ifndef _UKERN_MMAN_H
#define _UKERN_MMAN_H

#include "sys_comsg.h"
#include "ukern_params.h"

#include <stdatomic.h>
#include <sys/param.h>
#include <pthread.h>
#include <cheri/cherireg.h>

#define DEFAULT_BUFFER_SIZE 4096
#define MAX_BUFFER_SIZE (1UL<<PDRSHIFT)

#define BUFFER_PERMS ( CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP | \
	CHERI_PERM_STORE | CHERI_PERM_STORE_CAP | CHERI_PERM_GLOBAL |\
	CHERI_PERM_STORE_LOCAL_CAP )
#define UKERN_MAP_LEN (1UL<<PDRSHIFT)
#define UKERN_MMAP_FLAGS (\
	MAP_ANON | MAP_SHARED | MAP_ALIGNED_CHERI \
	| MAP_PREFAULT_READ | MAP_ALIGNED_SUPER )
#define UKERN_RESERVE_FLAGS (\
	MAP_GUARD | MAP_ALIGNED_CHERI)
#define UKERN_EXTEND_FLAGS ( UKERN_MMAP_FLAGS | MAP_FIXED | MAP_EXCL )
#define UKERN_MMAP_PROT ( PROT_READ | PROT_WRITE )

#define BUFFER_TABLE_SIZE ( WORKER_COUNT * U_FUNCTIONS * MAX_COPORTS )

#define BUFFER_PREALLOC_COUNT ( UKERN_MAP_LEN / DEFAULT_BUFFER_SIZE )

#define RESERVE_PAGES 1
#define RESERVE_SIZE ( RESERVE_PAGES * UKERN_MAP_LEN )

#define MAP_UKERN(a,b) mmap(a,b,UKERN_MMAP_PROT,UKERN_MMAP_FLAGS,-1,0)
#define EXTEND_UKERN(a,b) mmap(a,b,UKERN_MMAP_PROT,UKERN_EXTEND_FLAGS,-1,0)
#define RESERVE_UKERN(a,b) mmap(a,b,UKERN_MMAP_PROT,UKERN_RESERVE_FLAGS,-1,0)


typedef enum {BUFFER_NONE=0,BUFFER_ACTIVE=1,BUFFER_FREED=-1} buffer_type_t;

typedef enum {REGION_NONE,REGION_RESERVED,REGION_MAPPED} region_type_t;
typedef enum {BUFFER_ALLOCATE, BUFFER_FREE, MESSAGE_BUFFER_ALLOCATE, MESSAGE_BUFFER_FREE} queue_action_t;

typedef struct _region_table_entry
{
	pthread_mutex_t lock;
	void * __capability map_cap;
	_Atomic(void *  __capability) mem;
	region_type_t type;
	_Atomic size_t size;
	_Atomic size_t free;
} region_table_entry_t;

typedef struct _region_table
{
	pthread_mutex_t lock;
	_Atomic int next;
	_Atomic int length;
	region_table_entry_t *  table;
} region_table_t;

typedef struct _buffer_table_entry
{
	_Atomic buffer_type_t type;
	region_table_entry_t * region;
	_Atomic(void * __capability) mem;
} buffer_table_entry_t;

typedef struct _buffer_table
{
	pthread_mutex_t lock;
	_Atomic int next;
	_Atomic int length;
	buffer_table_entry_t * table;
} buffer_table_t;

typedef struct _work_queue_item
{
	queue_action_t action;
	_Atomic size_t len;
	_Atomic(void * __capability) subject;
	pthread_mutex_t lock;
	pthread_cond_t processed;
} work_queue_item_t;

typedef struct _work_queue
{
	pthread_mutex_t lock;
	pthread_cond_t not_empty;
	pthread_cond_t not_full;
	_Atomic int max_len;
	_Atomic int end;
	_Atomic int start;
	_Atomic int count;
	work_queue_item_t * items;
} work_queue_t;

extern work_queue_t jobs_queue;
//void extend_region_table(void);
int reserve_region(void);
int map_region(void);
int map_reservation(int index);
int check_region(int entry_index, size_t len);
int map_new_region(void);
void * __capability get_buffer_memory(region_table_entry_t ** region_dst,size_t len);
void * __capability buffer_malloc(size_t len);
void buffer_free(void * __capability buf);
int buffer_table_setup(void);
int region_table_setup(void);
int work_queue_setup(int len);
work_queue_item_t queue_do_job(work_queue_t * queue,work_queue_item_t job);
work_queue_item_t * queue_put_job(work_queue_t * queue,work_queue_item_t job);
work_queue_item_t * queue_get_job(work_queue_t * queue);
void * ukern_mman(void *args);
void * __capability ukern_do_malloc(size_t length, queue_action_t type);
void * __capability ukern_fast_malloc(size_t length);
void * __capability ukern_malloc(size_t len);
void ukern_do_free(void * __capability buf_cap, queue_action_t type);
void ukern_fast_free(void * __capability buf_cap);
void ukern_free(void * __capability buf_cap);
#endif