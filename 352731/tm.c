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
 * Transaction manager implementation, following TLII specification
**/

// Requested features
#define _GNU_SOURCE
#define _POSIX_C_SOURCE   200809L
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

// External headers
#include <stdatomic.h>
// Internal headers
#include <tm.h>

#include "macros.h"
#include "lockStamp.h"
#include "sets.h"

static const tx_t RO_tx  = UINTPTR_MAX - 10;
static const tx_t RW_tx = UINTPTR_MAX - 11;


/**
 * @brief List of dynamically allocated segments.
 */
typedef struct segment {
    size_t size;
    lockStamp* locks;
    word* raw_data;
    //segment* prev;
    segment* next;
} segment;
typedef segment* segment_list;

/**
 * @brief Transactional Memory Region
 */
typedef struct region {
    segment* segment_start; // First segment (non-deallocatable)
    segment_list allocs;    // Shared memory segments dynamically allocated via tm_alloc within transactions, ordered !
    size_t align;           // Size of a word in the shared memory region (in bytes)
    atomic_int* clock;       // Global clock used for time-stamping, perfectible ?
    rwSet* wSet;         // rwSet to track write operations
    rwSet* rSet;          // rwSet to track read operations
 } region;


/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) {
    region* tm_region = (region*) malloc(sizeof(region));
    if (unlikely(!tm_region)) {
        return invalid_shared;
    }
    // We create a segment entry for the non-deallocatable region
    segment* start_segment = (segment*) malloc(sizeof(segment));
    start_segment->size=size;
    // We allocate the shared memory buffer such that
    // its words are correctly aligned.
    if (unlikely(posix_memalign(&(start_segment->raw_data),align,size) !=0)){
        free(start_segment);
        free(tm_region);
        return invalid_shared;
    }
    // We allocate the shared memory buffer locks such that
    // they are correctly aligned.
    if (unlikely(posix_memalign(&(start_segment->locks),sizeof(lockStamp),size) !=0)){
        free(start_segment->raw_data);
        free(start_segment);
        free(tm_region);
        return invalid_shared;
    }
    memset(start_segment->raw_data, 0, size);
    memset(start_segment->locks, 0, size);
    start_segment->next=NULL;
    tm_region->segment_start=start_segment;
    tm_region->allocs      = start_segment;
    tm_region->align       = align;
    tm_region->wSet        = NULL;
    tm_region->rSet        = NULL;
    atomic_init(tm_region->clock, 0);
    return tm_region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t unused(shared)) {
    region* tm_region = (region*) shared;
    while (tm_region->allocs) { // Free allocated segments
        segment_list tail = tm_region->allocs->next;
        free(tm_region->allocs->locks);
        free(tm_region->allocs->raw_data);
        free(tm_region->allocs);
        tm_region->allocs = tail;
    }
    free(tm_region);
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
    region* tm_region = (region*) shared;
    // TODO: tm_begin(shared_t
    // if !is_ro
    // read clock into rv
    // reset R & W sets
    int rv = atomic_load(tm_region->clock);
    
    clearSet(tm_region->wSet);
    clearSet(tm_region->rSet);
    tm_region->wSet = NULL;
    tm_region->rSet = NULL;

    return invalid_tx;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t unused(shared), tx_t unused(tx)) {
    // TODO: tm_end(shared_t, tx_t)
    // ??? à vérifier
    // take W & F locks
    // increment clock
    // Recheck RSet
    // Commit writes
    // update stamps & locks (ajoute 1 au word !)
    // Commit frees
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
    // add to readset
    // si dans le wset, applique direct
    // si dans le freeset, ab
    // check récup lock & version, check lock & version <= rv
    // applique
    // check lock & version non changé
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
    // if in fSet, abort
    // add to Wset

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
    // add segment directly ?
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
    if ((region*) target == ((region*) shared)->segment_start){
        return false;
    }
    // add to freeSet
    return false;
}