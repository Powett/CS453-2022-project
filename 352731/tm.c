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
 * Transaction manager implementation, following TL2 specification
**/

// Requested features
#define _GNU_SOURCE
#define _POSIX_C_SOURCE   200809L
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif


// External headers
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
// Internal headers
#include <tm.h>
#include<unistd.h>

#include "macros.h"
#include "sets.h"
#include "lockStamp.h"


/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) {
    if (size%align){
        printf("Size not multiple of alignment");
        return invalid_shared;
    }
    size_t len = size/align;
   if (DEBUG>1){
       printf("== New Create: size %ld, align %ld\n", size, align);
   }
    region* tm_region = (region*) malloc(sizeof(region));
    if (unlikely(!tm_region)) {
        printf("Could not allocate region\n");
        return invalid_shared;
    }
    // We create a segment entry for the non-deallocatable region
    segment* start_segment = (segment*) malloc(sizeof(segment));
    start_segment->len=len;
    // We allocate the shared memory buffer such that
    // its words are correctly aligned.
    if (unlikely(posix_memalign((void*)&(start_segment->raw_data),align,size) !=0)){
        free(start_segment);
        free(tm_region);
        printf("Could not allocate region raw data\n");
        return invalid_shared;
    }
    // We allocate the shared memory buffer locks
    start_segment->locks=(lockStamp*) malloc(sizeof(lockStamp)*len);
    if (unlikely(!start_segment->locks)){
        free(start_segment->raw_data);
        free(start_segment);
        free(tm_region);
        printf("Could not allocate region locks\n");
        return invalid_shared;
    }
    memset(start_segment->raw_data, 0, size);
    for (size_t i=0;i<len;i++){
        if (unlikely(!init_lockstamp(&(start_segment->locks[i]), 0))){
            printf("Could not init locks\n");
            free(start_segment->raw_data);
            free(start_segment->locks);
            free(start_segment);
            free(tm_region);
            return invalid_shared;
        }
    }
    start_segment->next=NULL;
    tm_region->segment_start=start_segment;
    tm_region->allocs      = start_segment;
    tm_region->align       = align;
    atomic_init(&(tm_region->clock), 0);
    if(DEBUG){
    	printf("Region: %p, Region raw data start: %p\n", tm_region, tm_region->segment_start->raw_data);
    }
    return tm_region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) {
    if(DEBUG){
    	printf("== New destroy: %p\n", shared);
    }
    region* tm_region = (region*) shared;
    while (tm_region->allocs) { // Free allocated segments
        segment_list tail = (tm_region->allocs)->next;
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
    void* start=((region*) shared)->segment_start->raw_data;
   if (DEBUG){
       printf("Start segment: [%p,%p]\n", start, start+tm_size(shared));
   }
    return start;
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
**/
size_t tm_size(shared_t shared) {
    region* tm_region = (region*) shared;
    size_t align,len;
    align = tm_region->align;
    len = tm_region->segment_start->len;
   if (DEBUG>1){
       printf("Region starts with a len %ld, size %ld seg @%p\n", len,len*align, tm_region->segment_start);
   }
    return (len*align);
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
**/
size_t tm_align(shared_t shared) {
    size_t align = ((region*) shared)->align;
   if (DEBUG>1){
       printf("Region is %ld-bytes aligned\n", align);
   }
    return align;
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
        printf("Could not create a transaction");
        return invalid_tx;
    }
    tr->rSet=NULL;
    tr->wSet=NULL;
    tr->is_ro=is_ro;
    tr->rv= atomic_load(&(tm_region->clock));
    tr->wv=-1;
    if(DEBUG>1){
        printf("= New TX: %03lx, RO: %d\n", (tx_t)tr, is_ro);
    }
    return (tx_t)tr;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t shared, tx_t tx) {
    // if(DEBUG){
    // 	printf("= End TX: %03lx\n", tx);
    // }
    region* tm_region = (region*) shared;
    transac* tr=(transac*)tx;

    if (!tr->is_ro){
        // Acquire locks on wSet
        if(!wSet_acquire_locks(tr->wSet)){
            tr->wSet=NULL;
            if(DEBUG){
            	printf("Failed transaction, cannot acquire wSet\n");
            }
            abort_tr(tr);
            return false;
        }
        // Sample secondary (write-version) clock
        tr->wv=atomic_fetch_add(&(tm_region->clock), 1)+1;

        // Check rSet state
        if(!rSet_check(tr->rSet, tr->wv,tr->rv)){
            tr->rSet=NULL;
            wSet_release_locks(tr->wSet,NULL, -1);
            tr->wSet=NULL;
            if(DEBUG){
            	printf("Failed transaction, wrong rSet state\n");
            }
            abort_tr(tr);
            return false;
        }
        tr->rSet=NULL;
        // Commit wSet, release locks and write clocks
        wSet_commit_release(tm_region, tr->wSet, tr->wv);
        tr->wSet=NULL;
       if (DEBUG>1){
           printf("Commit succeeded, releasing locks, writing wv:%d\n", tr->wv);
       }
    }
    free(tr);
    if(DEBUG>1){
    	printf("[OK]= End TX: %03lx\n", tx);
    }
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
    if(DEBUG>1){
        printf("TX: %03lx, Read: %p to %p, size %ld\n", tx, source, target, size);
    }
    region* tm_region = (region*) shared;
    transac* tr=(transac*)tx;
    lockStamp* ls;
    segment* seg=find_segment(shared, (word*) source);

    if (unlikely(size%tm_region->align)){
        printf("Size not multiple of alignment");
        abort_tr(tr);
        return false;
    }
    if (unlikely(!seg)){
        if (DEBUG){
            printf("Could not find segment for source %p (call: Read (sh)%p to (priv)%p, %ld bytes)\n", source, source, target, size);
        }
        abort_tr(tr);
        return false;
    }
    size_t offset = (source-seg->raw_data)/tm_region->align;
    if (DEBUG>2){
        printf("Found segment for source %p @%p, offset: %ld\n", source, seg, offset);
    }
    size_t len=size/tm_region->align;
    int prev_versionStamp, post_versionStamp;
    bool prev_locked, post_locked;
    wSet* found_wSet=NULL;
    for(int i=len-1;i>=0;i--){
        if (!tr->is_ro){
            if (found_wSet && found_wSet->next && found_wSet->next->dest==(word*) (source+i*tm_region->align)){
                found_wSet=found_wSet->next;
            }else{
                found_wSet=wSet_contains((word*) (source+i*tm_region->align), tr->wSet);
            }
            if(DEBUG>2){
            	printf("Direct find in read: %d\n", found_wSet!=NULL);
            }
            if (found_wSet){
                if (unlikely(found_wSet->isFreed)){
                    if(DEBUG){
                    	printf("Failed transaction, read after free\n");
                    }
                    abort_tr(tr);
                    return false;
                }
                memcpy((target+i*tm_region->align),found_wSet->src, tm_region->align);
                continue;
            }
        }

        ls=&(seg->locks[i+offset]);
        prev_locked=atomic_load(&(ls->locked));
        // if (prev_locked){
        //     abort_tr(tr);
        //     return false;
        // }
        prev_versionStamp=ls->versionStamp;
        if (prev_versionStamp>tr->rv){
            abort_tr(tr);
            return false;
        }
        memcpy((target+i*tm_region->align),source+i*tm_region->align, tm_region->align);
        post_locked=atomic_load(&(ls->locked));
        post_versionStamp=ls->versionStamp;
        
        if (post_locked || post_versionStamp!=prev_versionStamp){
            if(DEBUG){
                printf("Read pre-validation failed transaction, post-locked:%d, TS: (post)%d/(pre)%d/(tr)%d\n", post_locked, post_versionStamp, prev_versionStamp,tr->rv);
            }
            abort_tr(tr);
            return false;
        }
        if (!tr->is_ro && !found_wSet){
            rSet* newRCell= (rSet*) malloc(sizeof(rSet));
            newRCell->ls=ls;
            newRCell->next=tr->rSet;
            tr->rSet=newRCell;
        }
    }
    if(DEBUG>1){
        printf("[OK] TX: %03lx, Read: %p to %p, size %ld\n", tx, source, target, size);
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
    if(DEBUG>2){
    	printf("TX: %03lx Write: %p to %p, size %ld\n", tx, source, target, size);
    }
    region* tm_region = (region*) shared;
    transac* tr=(transac*)tx;

    if (unlikely(size%tm_region->align)){
        printf("Size not multiple of alignment\n");
        abort_tr(tr);
        return false;
    }
    size_t len = size/tm_region->align;
    segment* seg=find_segment(shared, (word*) target);
    if (unlikely(!seg)){
        printf("Could not find segment for target %p\n", target);
        abort_tr(tr);
        return false;
    }
    size_t offset = (target-seg->raw_data)/tm_region->align;

   if (DEBUG>2){
       printf("Found segment for target %p @%p, offset: %ld\n", source, seg, offset);
   }
    if (unlikely(tr->is_ro)){
        printf("WO transaction trying to write !\n");
        abort_tr(tr);
        return false;
    }
    wSet* found_wSet=NULL;
    for(size_t i=0;i<len;i++){
        found_wSet=wSet_contains((word*) (target+i*tm_region->align), tr->wSet);
        if (found_wSet){
            if (unlikely(found_wSet->isFreed)){
                if(DEBUG){
                	printf("Failed transaction, write after free\n");
                }
                abort_tr(tr);
                return false;
            }
            memcpy(found_wSet->src,source+i*tm_region->align,tm_region->align);
        }else{
            wSet* newWCell= (wSet*) malloc(sizeof(wSet));
            newWCell->dest=(word*)(target+i*tm_region->align);
            newWCell->src=malloc(tm_region->align);
            memcpy(newWCell->src,source+i*tm_region->align,tm_region->align);
            newWCell->ls=&(seg->locks[i+offset]);
            newWCell->isFreed=false;
            newWCell->segToFree=NULL;
            newWCell->next=tr->wSet;
            tr->wSet=newWCell;
        }
    }
    if(DEBUG>2){
    	printf("[OK] TX: %03lx Write: %p to %p, size %ld\n", tx, source, target, size);
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
    if(DEBUG>1){
    	printf("TX: %03lx, Alloc: size %ld\n", tx, size);
    }
    region* tm_region = (region*) shared;
    transac* tr=(transac*)tx;
    if (unlikely(size%tm_region->align)){
        printf("Size not multiple of alignment\n");
        abort_tr(tr);
        return abort_alloc;
    }
    size_t len = size/tm_region->align;

    segment* newSeg = (segment*) malloc(sizeof(segment));
    if (unlikely(!newSeg)){
        printf("Could not allocate segment\n");
        return nomem_alloc;
    }
    newSeg->len=len;
    if (unlikely(posix_memalign((void*)&(newSeg->raw_data),tm_region->align,size) !=0)){
        free(newSeg->raw_data);
        free(newSeg);
        printf("Could not allocate segments raw data\n");
        return nomem_alloc;
    }
    newSeg->locks=(lockStamp*) malloc(sizeof(lockStamp)*len);
    if (unlikely(!newSeg->locks)){
        free(newSeg->raw_data);
        free(newSeg);
        printf("Could not allocate segments locks\n");
        return nomem_alloc;
    }
    memset(newSeg->raw_data, 0, size);
    for (size_t i=0;i<len;i++){
        if (unlikely(!init_lockstamp(&(newSeg->locks[i]), tr->rv))){
            printf("Could not init locks\n");
            abort_tr(tr);
            return abort_alloc;
        }
    }
    *target=add_segment(shared, newSeg);
    if(DEBUG>1){
    	printf("[OK] TX: %03lx, Alloc: size %ld, @%p, raw data: [%p,%p]\n", tx, size, *target, newSeg->raw_data, newSeg->raw_data+size);
    }
    return success_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t shared, tx_t tx, void* target) {
    if(DEBUG>1){
    	printf("TX: %03lx, Free: %p\n", tx, target);
    }
    region* tm_region = (region*) shared;
    transac* tr=(transac*)tx;
    segment* seg=tm_region->allocs;
    if (unlikely(tr->is_ro || tm_region->segment_start==target)){
        printf("Failed transaction, forbidden operation (free in RO or free start seg)\n");
        abort_tr(tr);
        return false;
    }
    seg=find_segment(shared, target);
    if (unlikely(!seg)){
        if(DEBUG){
        	printf("Failed transaction, cannot find segment to free\n");
        }
        abort_tr(tr);
        return false;
    }
    wSet* found_wSet=tr->wSet;
    for(size_t i=0;i<seg->len;i++){
        found_wSet=wSet_contains((word*)(target+i*tm_region->align), found_wSet);
        if (found_wSet){
            if (found_wSet->isFreed){
                if(DEBUG){
                	printf("Failed transaction, freed already\n");
                }
                abort_tr(tr);
                return false;
            }
            found_wSet->isFreed=true;
        }else{
            wSet* newWCell= (wSet*) malloc(sizeof(wSet));
            newWCell->dest=(word*)(target+i*tm_region->align);
            newWCell->src=NULL;
            newWCell->ls=&(seg->locks[i]);
            newWCell->isFreed=true;
            newWCell->segToFree=NULL;
            newWCell->next=tr->wSet;
            tr->wSet=newWCell;
            found_wSet=tr->wSet;
        }
        if (i==0){
            found_wSet->segToFree=seg;
        }
    }
    if(DEBUG>1){
    	printf("[OK] TX: %03lx, Free: %p\n", tx, target);
    }
    return true;
}