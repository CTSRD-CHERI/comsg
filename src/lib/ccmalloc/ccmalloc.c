/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#define BUCKET_FLAGS ( MAP_ANON | MAP_ALIGNED_CHERI )
#define BUCKET_PROT ( PROT_READ | PROT_WRITE )

typedef enum {0 = FREED, 1 = MAPPED, 2 = INUSE} bucket_status_t;

struct batch {
	LIST_ENTRY(batch) batches;
	_Atomic(void *) mem;
	_Atomic bucket_status_t status;
	_Atomic size_t freed;
	_Atomic size_t allocated;
};

struct bucket {
	LIST_HEAD(, batch) batch_list;
	size_t size;
	size_t cherisize;
	size_t alignment;
	size_t alloc_size;
	struct batch *spare;
};

static struct {
	size_t *sizes;
	struct *buckets;
	size_t nbuckets;
} bucket_table;

static const size_t alloc_size = (1024 * 1024);

static void init_bucket(struct bucket *new_bucket, size_t len);
static int bucketsize_compare(const void* arg1, const void* arg2);
static void new_batch(struct bucket *bucket);
static struct batch *get_spare(struct bucket *bucket)
static struct bucket *get_bucket(size_t size)

static 
int bucketsize_compare(const void* arg1, const void* arg2) 
{
	size_t b1, b2;
	b1 = (*(size_t *)arg1);
	b2 = (*(size_t *)arg2);
	if (b1 > b2) 
		return (1);
	else if (b1 < b2)
		return (-1);
	else
		return (0);
}

inline bool
needs_new_batch(struct bucket *bucket)
{
	if (bucket->spare == NULL)
		return (true);
	else
		return (false);
}

inline size_t
mem_remaining(struct batch *batch)
{
	void *cap;

	cap = atomic_load_explicit(&batch->mem, memory_order_acquire);
	return (cheri_getlen(cap) - cheri_getoffset(cap));
}

static void
init_bucket(struct bucket *new_bucket, size_t len)
{
	LIST_INIT(&new_bucket->batch_list);
	new_bucket->alloc_size = alloc_size;
	new_bucket->size = len;
	new_bucket->cherisize = CHERI_REPRESENTABLE_LENGTH(len);
	new_bucket->alignment = CHERI_REPRESENTABLE_ALIGNMENT(len);
	new_bucket->spare = NULL;
	
	new_batch(new_bucket);
	get_spare(new_bucket);
	new_batch(new_bucket);
	assert(new_bucket->spare != NULL);
}

void 
ccmalloc_init(size_t *bucket_sizes, size_t nbuckets)
{
	size_t i, last_bucket;

	bucket_table.buckets = calloc(nbuckets, sizeof(struct bucket));
	qsort(bucket_sizes, nbuckets, sizeof(size_t), bucketsize_compare);
	
	last_bucket = 0;
	bucket_table.nbuckets = 1;
	bucket_table.sizes[0] = bucket_sizes[0];
	for (i = 1; i < nbuckets; i++) {
		if (bucket_table.sizes[last_bucket] == bucket_sizes[i]) {
			bucket_table.buckets[last_bucket].alloc_size += alloc_size;
			continue;
		}
		last_bucket = bucket_table.nbuckets++;
		bucket_table.sizes[last_bucket] = bucket_sizes[i];
		init_bucket(&bucket_table.buckets[last_bucket], bucket_table.sizes[last_bucket]);
	}
	bucket_table.sizes[bucket_table.nbuckets] = 0;
	bucket_table.buckets = realloc(bucket_table.nbuckets * sizeof(struct bucket))
	bucket_table.flexible_bucket = &bucket_table.buckets[i-1];
}

struct batch *
get_spare(struct bucket *bucket)
{
	struct batch *spare_batch;
	spare_batch = atomic_load_explicit(&bucket->spare, memory_order_acquire);
	
	if (spare_batch == NULL)
		return (NULL);
	else if (atomic_compare_exchange_strong_explicit(&bucket->spare, &spare_batch, NULL, memory_order_acq_rel, memory_order_acquire))
		LIST_INSERT_HEAD(&bucket->batch_list, spare_batch, batches);

	return (spare_batch);
}

struct bucket *
get_bucket(size_t size)
{
	size_t i;
	
	for (i = 0; i < bucket_table.nbuckets; i++) {
		if (size == bucket_table.buckets[i].size)
			return (bucket_table.buckets[i]);
		else if (size == bucket_table.buckets[i].cherisize)
			return (bucket_table.bucket[i]);
	}
	return (bucket_table.flexible_bucket);
}

void
new_batch(struct bucket *bucket)
{
	struct batch *batch, *expected;
	void *mem;

	mem = mmap(NULL, alloc_size, BUCKET_PROT, BUCKET_FLAGS, -1, 0);
	memset(mem, '\0', cheri_getlen(mem)); /* fault in via zero */
	if(mem == (void *)-1)
		err(errno, "new_bucket_batch: mmap failed");

	batch = calloc(1, sizeof(struct batch));
	batch->status = MAPPED;
	batch->freed = 0;
	batch->allocated = 0;
	batch->mem = mem;
	expected = NULL;
	if(!atomic_compare_exchange_strong_explicit(bucket->spare, &expected, batch, memory_order_acq_rel, memory_order_acquire)) {
		munmap(mem);
		free(batch);
	}
}

void *
refill_buckets(void *args)
{
	struct bucket *bucket;
	size_t i;

	for (;;) {
		sleep(10);
		for (i = 0; i < bucket_table.nbuckets; i++) {
			bucket = bucket_table.buckets[i];
			if (needs_new_batch(bucket)) 
				new_batch(bucket);
		}
	}
}

void *
empty_buckets(void *args)
{
	struct batch *batch;
	struct bucket *bucket;
	size_t i, allocated, freed; 
	int status;
	int error;

	for (;;) {
		sleep(10);
		for (i = 0; i < bucket_table.nbuckets; i++) {
			bucket = bucket_table.buckets[i];
			batch = LIST_FIRST(&bucket->batch_list);
			for (;;) {
				batch = LIST_NEXT(batch);
				if (batch == NULL)
					break;
				else if (batch->status == FREED)
					continue;
				allocated = atomic_load_explicit(&batch->allocated, memory_order_acquire);
				freed = batch->freed;
				if (allocated != freed)
					continue;
				error = munmap(batch->mem, cheri_getlen(batch->mem));
				if (error != 0)
					err(errno, "unmap: failed to unmap batch");
				atomic_store_explicit(&batch->status, FREED, memory_order_release);
			}
		}
	}
}

void *
cocall_malloc(size_t len)
{
	struct batch *batch;
	struct bucket *bucket;
	void *result_cap, *cap, *new_cap;
	bucket_status_t status;

	bucket = get_bucket(len);
	batch = LIST_FIRST(&bucket->batch_list);
	
	status = atomic_load_explicit(&batch->status, memory_order_acquire);
	if (status == MAPPED)
		atomic_compare_exchange_strong_explicit(&batch->status, &status, INUSE, memory_order_acq_rel, memory_order_acquire);
	assert(status == INUSE);

	if (mem_remaining(batch) < len) {
		batch = get_spare(bucket);
		if (batch == NULL) 
			return (NULL);
	}

	cap = atomic_load_explicit(&batch->mem, memory_order_acquire);
	assert(cap != NULL);
	do {
		result_cap = cheri_setbounds(cap, len);
		new_cap = cheri_incoffset(cap, cheri_getlen(result_cap));
		new_cap = __builtin_align_up(new_cap, bucket->alignment);
	} while (!atomic_compare_exchange_strong_explicit(&batch->mem, &cap, new_cap, memory_order_acq_rel, memory_order_acquire));

	atomic_fetch_add_explicit(&batch->allocated, cheri_getlen(result_cap), memory_order_acq_rel);
	result_cap = cheri_andperm(result_cap, ~CHERI_PERM_CHERIABI_VMMAP);
	return (result_cap);
}

void *
cocall_calloc(size_t a, size_t b)
{
	void *mem;
	a = a * b;
	mem = cocall_malloc(a);
	memset(mem, '\0', cheri_getlen(mem));
	return (mem);
}

void
cocall_free(void *cap) 
{
	struct batch *batch;
	struct bucket *bucket;
	vaddr_t map_addr;

	bucket = get_bucket(cheri_getlen(cap));
	map_addr = cheri_getbase(cap);
	LIST_FOREACH(batch, &bucket->batch_list, batches) {
		if (cheri_is_address_inbounds(batch->mem, map_addr)) {
			atomic_fetch_add(&batch->freed, cheri_getlen(cap));
			return;
		}
	}
	err(EINVAL, "cocall_free: failed to locate mapping");
}