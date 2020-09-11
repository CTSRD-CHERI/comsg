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

//TODO-PBB: Implement.
#include "ccmalloc.h"

#include "ukern/utils.h"

#include <assert.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_BUCKETS 12
#define BUCKET_MAX_BATCHES 1024
#define BUCKET_FLAGS ( MAP_ANON | MAP_ALIGNED_CHERI | MAP_SHARED )
#define BUCKET_PROT ( PROT_READ | PROT_WRITE )

#define WAITMAX 1000000000L
static const int bucket_batch = 64;
static pthread_t bucket_refiller, batch_freer;

typedef enum {UNINITIALIZED=0, READY=1, ALLOCATED=2, FREED=3} bucket_entry_status_t;

struct _bucket_entry {
	TAILQ_ENTRY(_bucket_entry) next;
	void *mem;
	_Atomic bucket_entry_status_t status;
}

struct _batch {
	void *map_cap;
	struct _bucket_entry *starting_entry;
	int freed;
}

struct bucket {
	TAILQ_HEAD(, _bucket_entry) entries;
	_Atomic struct _bucket_entry *next_free_entry;
	size_t size;
	size_t cherisize;
	size_t alignment;
	struct _batch[BUCKET_MAX_BATCHES] batches;
	_Atomic int nbatches;
	int batch_size;
	_Atomic int used_entries;
};

struct bucket_table
{
	struct bucket buckets[MAX_BUCKETS];
	size_t sizes[MAX_BUCKETS]; 
	size_t nbuckets;
};

static 
int bucket_compare(const void* arg1, const void* arg2) 
{
	struct bucket b1, b2;
	b1 = (*(struct bucket *)arg1);
	b2 = (*(struct bucket *)arg2);
	if (b1.size > b2.size) 
		return 1;
	else if (b1.size < b2.size)
		return -1;
	else
		return 0;
}

static
void new_bucket_batch(struct bucket *bucket)
{
	struct _bucket_entry *new_entries, *entry_ptr;
	struct _batch new_batch;
	size_t alloc_size;
	void *mem;
	int batch_idx;

	if (bucket->batches >= BUCKET_MAX_BATCHES)
		err(ENOMEM,"new_bucket_batch: cannot allocate a new bucket batch");
	else
		atomic_fetch_add(bucket->batches, 1);

	alloc_size = CHERI_REPRESENTABLE_LENGTH(bucket->cherisize * bucket_batch);
	mem = mmap(NULL, alloc_size, BUCKET_FLAGS, BUCKET_PROT, -1, 0);
	new_entries = calloc(bucket_batch, sizeof(struct _bucket_entry));

	batch_idx = atomic_fetch_add(bucket->batches, 1);
	bucket->batches[batch_idx].map_cap = mem;
	bucket->batches[batch_idx].starting_entry = cheri_setbounds(new_entries[i], sizeof(struct _bucket_entry));

	for(int i = 0; i < bucket->batch_size; i++)
	{
		entry_ptr = new_entries[i];
		if ((mem + bucket->cherisize >= cheri_gettop(mem))) {
			bucket->batch_size = i + 1;
			break;
		}
		entry_ptr->mem = cheri_setboundsexact(mem, bucket->cherisize);
		mem = cheri_setaddress(mem, cheri_gettop(entry_ptr->mem));
		mem = __builtin_align_up(mem, bucket->alignment)

		entry_ptr->status = READY;
		entry_ptr = cheri_setbounds(entry_ptr, sizeof(struct _bucket_entry));
		TAILQ_INSERT_TAIL(&bucket->entries, entry_ptr, entries);
	}

}

static
struct bucket init_bucket(size_t len)
{
	struct bucket new_bucket;
	void *mem;

	new_bucket.size = len;
	new_bucket.cherisize = CHERI_REPRESENTABLE_LENGTH(len);
	new_bucket.alignment = CHERI_REPRESENTABLE_ALIGNMENT(len);
	new_bucket.used_entries = 0;
	TAILQ_INIT(&new_bucket.entries);
	new_bucket.batch_size = bucket_batch;
	new_bucket_batch(&new_bucket);

	return new_bucket;
}

static
int batch_free(struct _batch *batch,)
{
	struct _batch_entry *cur_entry;
	cur_entry = batch->starting_entry;
	do {
		if (atomic_load(cur_entry->status) != FREED)
			return (0);
		cur_entry = TAILQ_NEXT(cur_entry, next);
	} while(cheri_is_address_inbounds(batch->map_cap, cheri_getaddress(cur_entry->mem)));
	return (1);
}

static 
void *free_batches(void *args)
{
	UNUSED(args);
	struct bucket *bucket;
	struct _batch *batch;
	void *map_cap;
	size_t map_len;
	struct timespec wait;
	size_t nbuckets = bucket_table.nbuckets;
	int didfree;
	
	wait.tv_sec = 60 / nbuckets;
	wait.tv_nsec = 0;
	for(;;) {
		didfree = 0;
		for (size_t i = 0; i < nbuckets; i++) {
			bucket = &bucket_table.buckets[i];
			for(size_t j = 0; j < bucket->nbatches; j++) {
				batch = bucket->batches[j];
				if (batch->freed)
					continue;
				else if (batch_free(batch)) {
					map_cap = batch->map_cap;
					map_len = cheri_getlen(map_cap);
					munmap(map_cap, map_len);
					batch->freed = 1;
					batch->map_cap = cheri_cleartag(map_cap);
				}
			}
			nanosleep(&wait, NULL);
		}
		if (!didfree) {
			new_wait = wait.tv_sec * 2;
			wait.tv_sec = (new_wait < 60) ? new_wait : wait.tv_sec;
		}
		else
			wait.tv_sec = 60 / nbuckets;
	}
	return (NULL);
}

static
void *refill_buckets(void *args)
{
	UNUSED(args);
	struct timespec wait_period;
	struct bucket *bucket;
	long new_wait;
	int started_freer = 0;
	int batch_remaining, int didwork;
	size_t nbuckets = bucket_table.nbuckets;

	wait_period.tv_sec = 0;
	wait_period.tv_nsec = 1000;
	for(;;) {
		didwork = 0;
		for(size_t i = 0; i < nbuckets; i++) {
			bucket = &bucket_table.buckets[i];
			batch_remaining = bucket->used_entries % bucket->batch_size;
			if (batch_remaining < (bucket->batch_size / 2)) {
				didwork += 1;
				new_bucket_batch(bucket);
			}
		}
		if (didwork)
		{
			if (!started_freer) {
				//a batch is only going to be freed after the next one has been allocated
				pthread_create(&batch_freer, NULL, free_batches, NULL);
				started_freer = 1;
			}
			wait_period.tv_nsec = 1000;
		}
		else {
			new_wait = wait_period.tv_nsec * 2;
			wait_period.tv_nsec = (new_wait < WAITMAX) ? new_wait : wait_period.tv_nsec;
		}
		clock_nanosleep(CLOCK_REALTIME, 0, &wait_period, NULL);
	}
	return (NULL);
}

__attribute__ ((constructor)) static 
void set_nbuckets(void)
{
	bucket_table.nbuckets = 0;
}

void
ccmalloc_init(size_t *bucket_sizes, size_t nbuckets)
{
	if (nbuckets != 0)
		err(EINVAL, "ccmalloc_init: called ccmalloc twice");
	else if (nbuckets > MAX_BUCKETS)
		err(EINVAL, "ccmalloc_init: number of requested buckets (%d) exceeds MAX_BUCKETS", nbuckets);
	else if (nbuckets == 0)
		err(EINVAL, "ccmalloc_init: number of requested buckets (%d) invalid", nbuckets);
	else if (cheri_getlen(bucket_sizes) < (nbuckets * sizeof(size_t)))
		err(EINVAL, "ccmalloc_init: invalid bounds for number of requested buckets (%d) ", nbuckets);

	for(size_t i = 0; i < nbuckets; i++) {
		bucket_table.buckets[i] = init_bucket(bucket_sizes[i]);
		bucket_table.nbuckets++;
	}
	qsort(bucket_table.buckets, bucket_table.nbuckets, sizeof(struct bucket), bucket_compare);
	bucket_table.sizes[0] = bucket_table.buckets[0].size;
	for(size_t i = 1; i < bucket_table.nbuckets; i++) {
		bucket_table.sizes[i] = bucket_table.buckets[i].size;
		if (bucket_table.sizes[i-1] == bucket_table.sizes[i]) 
			err(EINVAL, "ccmalloc_init: duplicate bucket sizes submitted");
	}
	int error = pthread_create(&bucket_refiller, NULL, refill_buckets, NULL);
	if (error)
		err(error, "ccmalloc_init: failed to start bucket refiller thread");
}

static
struct bucket *find_bucket_min(size_t len)
{
	for(size_t i = 0; i < bucket_table.nbuckets; i++) {
		if (bucket_table.buckets[i].size < len)
			return &bucket_table.buckets[i];
	}
	return (NULL);
}

static
struct bucket *find_bucket_exact(size_t len)
{
	for(size_t i = 0; i < bucket_table.nbuckets; i++) {
		if (bucket_table.buckets[i].size == len)
			return &bucket_table.buckets[i];
	}
	return (NULL);
}

static
struct bucket *find_bucket_cheri(size_t len)
{
	for(size_t i = 0; i < bucket_table.nbuckets; i++) {
		if (bucket_table.buckets[i].cherisize == len)
			return &bucket_table.buckets[i];
	}
	return (NULL);
}

static
void *get_mem(size_t len)
{
	struct bucket *memory_bucket = find_bucket(len);
	struct _bucket_entry *new_next;
	void *cap;
	int resize = 0;
	bucket_entry_status_t new_status;

	if (memory_bucket == NULL) {
		resize = 1;
		memory_bucket = find_bucket_min(len);
		if (memory_bucket == NULL)
			err(EINVAL, "cocall_alloc: get_mem: bucket for size %lu not initialized");
	}

	while(!atomic_compare_exchange_weak(&memory_bucket->next_free_entry->status, &new_status, ALLOCATED))
		new_status = READY;
	atomic_fetch_add(&memory_bucket->used_entries, 1);
	
	cap = memory_bucket->next_free_entry->mem;
	cap = cheri_setbounds(cap, len);
	assert(len <= cheri_getlen(cap)); //something is wrong if this ever happens
	
	new_next = TAILQ_NEXT(memory_bucket->next_free_entry, next);
	assert(new_next->status == READY);
	atomic_store(&memory_bucket->next_free_entry, new_next);

	return (cap);
}

static
void *cocall_alloc_noresize(size_t len)
{
	return (get_mem(len));
}

void *cocall_malloc(size_t len)
{
	return (cocall_alloc(len, 0, 0));
}

void *cocall_alloc(size_t len, ccmalloc_flags_t flags, size_t resize_hint)
{
	UNUSED(resize_hint);
	UNUSED(flags);
	void *cap;

	if (flags != 0)
		err(ENOSYS, "cocall_malloc: resizable malloc not yet implemented");

	cap = cocall_alloc_noresize(len);
	return (cap);
}

void *cocall_calloc(size_t num, size_t size)
{
	void *mem;
	size_t length = num * size;
	size_t actual_len;

	mem = cocall_alloc(length, 0, 0);
	actual_len = cheri_getlen(mem);
	assert(length <= actual_len);
	memset(mem, 0, actual_len);
	
	return (mem);
}

static
int free_batch_entry(struct _batch *batch, void *cap)
{
	struct _batch_entry *cur_entry;
	cur_entry = batch->starting_entry;
	do {
		if (cur_entry->mem == cap) {
			//could possibly use sw-defined permissions?
			atomic_store(cur_entry->status, FREED);
			return (1);
		}
		cur_entry = TAILQ_NEXT(cur_entry, next);
		
	} while(cheri_is_address_inbounds(batch->map_cap, cheri_getaddress(cur_entry->mem)));
	return (0);
}

void cocall_free(void *cap)
{
	struct bucket *bucket = find_bucket_cheri(cheri_getlen(cap));
	struct _bucket_entry *entry;
	vaddr_t cap_base = cheri_getbase(cap);

	for (int i = 0; i < bucket->nbatches; ++i)
	{
		if (bucket->batches[i].freed)
			continue;
		else if (cheri_is_address_inbounds(bucket->batches[i].map_cap, cap_base)) {
			if (!free_batch_entry(&bucket->batches[i], cap))
				err(EINVAL, "cocall_free: cap does not authorise free");
			else
				return;
		}
	}
	err(EINVAL, "cocall_free: cap not in bounds for any batch in bucket for size %lu", bucket->size);
}
