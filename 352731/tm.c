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
#include "sets.h"

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
    memset(start_segment->raw_data, 0, size*align);
    memset(start_segment->locks, 0, size*sizeof(lockStamp));
    start_segment->next=NULL;
    tm_region->segment_start=start_segment;
    tm_region->allocs      = start_segment;
    tm_region->align       = align;
    tm_region->pending     = NULL;
    atomic_init(&(tm_region->clock), 0);
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
    while (tm_region->pending){
        transac* tail=tm_region->pending->next;
        tr_free(tm_region->pending);
        tm_region->pending=tail;
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
tx_t tm_begin(shared_t shared, bool is_ro) {
    region* tm_region = (region*) shared;
    transac* tr = (transac*)malloc(sizeof(transac));
    if (unlikely(!tr)){
        return invalid_tx;
    }
    tr->rSet=NULL;
    tr->wSet=NULL;
    tr->is_ro=is_ro;
    tr->next=tm_region->pending;
    tm_region->pending=tr;
    int rv = atomic_load(&(tm_region->clock));
    return (tx_t)tr;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t shared, tx_t tx) {
    region* tm_region = (region*) shared;
    transac* tr=(transac*)tx;
    
    if (!tr->is_ro){
        bool lock_free=true;
        // Acquire locks on wSet
        acquire_wSet_locks(tr->wSet);
        // Sample secondary (write-version) clock
        tr->wv=atomic_fetch_add(&(tm_region->clock), 1);
        // Check rSet state
        if (!rSet_check(tr->rSet, tr->wv,tr->rv)){
            wSet_release_locks(tr->wSet,NULL, -1);
            return false;
        }
        // Commit rSet
        rSet_commit(tm_region, tr->rSet);
        // Commit wSet
        wSet_commit(tm_region, tr->wSet);
        // Release locks and update clocks
        wSet_release_locks(tr->wSet, NULL, tr->wv);
    }
    // Terminate the transaction
    tr_free(tx);
    return true;
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t shared, tx_t tx, void const* source, size_t size, void* target) {
    region* tm_region = (region*) shared;
    transac* tr=(transac*)tx;
    for(int i=0;i<size*tm_region->align;i+=tm_region->align){
        wSet* found_wSet=wSet_contains((word*) (source+i), tr->wSet);
        if (found_wSet){
            if (found_wSet->isFreed){
                return false;
            }
            memcpy((target+i),found_wSet->src, tm_region->align);
        }else{
            rSet* newRCell= (rSet*) malloc(sizeof(rSet*));
            newRCell->dest=target+i;
            newRCell->src=source+i;
            newRCell->next=tr->rSet;
            tr->rSet=newRCell;
        }
    }
    return true;
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
bool tm_write(shared_t shared, tx_t tx, void const* source, size_t size, void* target) {
    region* tm_region = (region*) shared;
    transac* tr=(transac*)tx;
    for(int i=0;i<size*tm_region->align;i+=tm_region->align){
        wSet* found_wSet=wSet_contains((word*) (target+i), tr->wSet);
        if (found_wSet){
            if (found_wSet->isFreed){
                return false;
            }
            found_wSet->src=source+i;
        }else{
            wSet* newWCell= (wSet*) malloc(sizeof(wSet*));
            newWCell->dest=target+i;
            newWCell->src=source+i;
            newWCell->isFreed=false;
            newWCell->next=tr->wSet;
            tr->wSet=newWCell;
        }
    }
    return true;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not (abort_alloc)
**/
alloc_t tm_alloc(shared_t shared, tx_t tx, size_t size, void** target) {
    region* tm_region = (region*) shared;
    transac* tr=(transac*)tx;
    segment* newSeg = (segment*) malloc(sizeof(segment));
    if (unlikely(!newSeg)){
        return nomem_alloc;
    }
    newSeg->size=size;
    if (unlikely(posix_memalign(&(newSeg->raw_data),tm_region->align,size) !=0)){
        free(newSeg);
        return nomem_alloc;
    }
    if (unlikely(posix_memalign(&(newSeg->locks),sizeof(lockStamp),size) !=0)){
        free(newSeg->raw_data);
        free(newSeg);
        return nomem_alloc;
    }
    memset(newSeg->raw_data, 0, size*tm_region->align);
    memset(newSeg->locks, (lockStamp) tr->rv<<1, size*sizeof(lockStamp));
    newSeg->next=tm_region->allocs;
    tm_region->allocs=newSeg;
    return success_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t shared, tx_t tx, void* target) {
    region* tm_region = (region*) shared;
    transac* tr=(transac*)tx;
    segment* seg = find_segment(shared, target);
    if (unlikely(!seg)){
        return false;
    }
    for(int i=0;i<seg->size*tm_region->align;i+=tm_region->align){
        wSet* found_wSet=wSet_contains( seg->raw_data+i, tr->wSet);
        if (found_wSet){
            if (found_wSet->isFreed){
                return false;
            }
            found_wSet->isFreed=true;
        }else{
            wSet* newWCell= (wSet*) malloc(sizeof(wSet*));
            newWCell->dest=seg->raw_data+i;// pas sûr de l'aligment ?? TODO
            newWCell->src=NULL;
            newWCell->isFreed=true;
            newWCell->next=tr->wSet;
            tr->wSet=newWCell;
        }
    }
    free(seg->locks);
    free(seg->raw_data);
    free(seg);
    return true;
}