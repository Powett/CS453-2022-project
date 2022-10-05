/**
 * @file   tm.c
 * @author Nathan PELUSO <nathan.peluso@epfl.ch>
 *
 * @section LICENSE
 *
 * Copyright © 2022-2023 Nathan Peluso.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * any later version. Please see https://gnu.org/licenses/gpl.html
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * @section DESCRIPTION
 *
 * Transaction manager implementation, following XXXX
**/

// Requested features
#define _GNU_SOURCE
#define _POSIX_C_SOURCE   200809L
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

// External headers

// Internal headers
#include <tm.h>

#include "macros.h"

static const tx_t RO_tx  = UINTPTR_MAX - 10;
static const tx_t RW_tx = UINTPTR_MAX - 11;

typedef int lockStamp;

#define non_atomic_islock(lockStamp) lockStamp & 1
#define non_atomic_version(lockStamp) lockStamp>>1

/**
 * @brief List of dynamically allocated segments.
 */
struct segment {
    size_t size;
    lockStamp* locks;
    //struct segment* prev;
    void* raw_data;
    struct segment* next;
};
typedef struct segment* segment_list;

/**
 * @brief Transactional Memory Region
 */
struct region {
    struct segment* segment_start;// First segment (non-deallocatable)
    segment_list allocs; // Shared memory segments dynamically allocated via tm_alloc within transactions, ordered !
    size_t align;       // Size of a word in the shared memory region (in bytes)
};


/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) {
    struct region* region = (struct region*) malloc(sizeof(struct region));
    if (unlikely(!region)) {
        return invalid_shared;
    }
    // We create a segment entry for the non-deallocatable region
    struct segment* start_segment = (struct segment*) malloc(sizeof(struct segment));
    start_segment->size=size;
    // We allocate the shared memory buffer such that
    // its words are correctly aligned.
    if (unlikely(posix_memalign(&(start_segment->raw_data),align,size) !=0)){
        free(region);
        free(start_segment);
        return invalid_shared;
    }
    // We allocate the shared memory buffer locks such that
    // they are correctly aligned.
    if (unlikely(posix_memalign(&(start_segment->locks),sizeof(lockStamp),size) !=0)){
        free(region);
        free(start_segment->raw_data);
        free(start_segment);
        return invalid_shared;
    }
    memset(start_segment->raw_data, 0, size);
    memset(start_segment->locks, 0, size);
    start_segment->next=NULL;
    region->segment_start=start_segment;
    region->allocs      = start_segment;
    region->align       = align;
    return region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t unused(shared)) {
    struct region* region = (struct region*) shared;
    while (region->allocs) { // Free allocated segments
        segment_list tail = region->allocs->next;
        free(region->allocs->locks);
        free(region->allocs->raw_data);
        free(region->allocs);
        region->allocs = tail;
    }
    free(region);
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t shared) {
    return ((struct region*) shared)->segment_start->raw_data;
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
**/
size_t tm_size(shared_t shared) {
    return ((struct region*) shared)->segment_start->size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
**/
size_t tm_align(shared_t shared) {
    return ((struct region*) shared)->align;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t unused(shared), bool unused(is_ro)) {
    // TODO: tm_begin(shared_t)
    return invalid_tx;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t unused(shared), tx_t unused(tx)) {
    // TODO: tm_end(shared_t, tx_t)
    return false;
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t unused(shared), tx_t unused(tx), void const* unused(source), size_t unused(size), void* unused(target)) {
    // TODO: tm_read(shared_t, tx_t, void const*, size_t, void*)
    return false;
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
bool tm_write(shared_t unused(shared), tx_t unused(tx), void const* unused(source), size_t unused(size), void* unused(target)) {
    // TODO: tm_write(shared_t, tx_t, void const*, size_t, void*)
    return false;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not (abort_alloc)
**/
alloc_t tm_alloc(shared_t unused(shared), tx_t unused(tx), size_t unused(size), void** unused(target)) {
    // TODO: tm_alloc(shared_t, tx_t, size_t, void**)
    return abort_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(target)) {
    // TODO: tm_free(shared_t, tx_t, void*)
    return false;
}

/** Getting lockStamp for given word in memory
 * @param shared Shared memory region associated with the transaction
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
lockStamp find_lock(shared_t shared, void* target){
    struct region* region=(struct region* ) shared;
    struct segment* segment=region->allocs;
    while (segment && segment->next->raw_data<target){
        segment=segment->next;
    }
    if (!segment){r
        return -1;
    }
    int index = (int) (target-segment->raw_data)/region->align;
    if (index<0 || index >= segment->size){
        return -1;
    }
    return (lockStamp)(segment->locks[index]);
}