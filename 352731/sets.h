#pragma once

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <tm.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "lockStamp.h"

typedef void word;

// Linked lists to track write operations
// A "free" is considered as a special write, preventing further r/w
typedef struct wSet{
    void* src;
    word* dest;
    lockStamp* ls;
    bool isFreed;
    struct wSet* next;
    struct wSet* prev;
} wSet;

// Linked lists to track read operations
typedef struct rSet{
    void* dest;
    word* src;
    lockStamp* ls;
    struct rSet* next;
} rSet;

// Linked list structure to keep track of active transactions
typedef struct transac{
    int rv;             // First clock counter
    int wv;             // Second clock counter
    wSet* wSet;         // wSet to track write operations
    rSet* rSet;         // rSet to track read operations
    bool is_ro;
    struct transac* prev;       
    struct transac* next;
} transac;

/**
 * @brief List of dynamically allocated segments.
 */
typedef struct segment{
    size_t len;
    lockStamp* locks;
    word* raw_data;
    struct segment* next;
} segment;
typedef segment* segment_list;

/**
 * @brief Transactional Memory Region
 */
typedef struct region{
    segment* segment_start; // First allocated segment (non-deallocatable) (may not be the first in the allocs list)
    segment_list allocs;    // Shared memory segments dynamically allocated via tm_alloc within transactions, ordered by growing raw data (first) address
    size_t align;           // Size of a word in the shared memory region (in bytes)
    atomic_int clock;       // Global clock used for time-stamping, perfectible ?
    transac* pending;       // Currently began but not ended transactions

    pthread_mutex_t debug_lock; // debug coarse lock
 } region;


segment* find_segment(shared_t shared, word* target);
lockStamp* find_lock(shared_t shared, segment* segment, word* target);
lockStamp* find_lock_from_target(shared_t shared, word* target);

void clearrSet(rSet* set);
void clearwSet(wSet* set);

wSet* wSet_contains(word* addr, wSet* set);

bool wSet_acquire_locks(wSet* set);
void wSet_release_locks(wSet* start, wSet* stop, int wv_to_write);

bool rSet_check(rSet* set, int wv, int rv);
bool rSet_commit(region* tm_region, rSet* set);
bool wSet_commit(region* tm_region, wSet* set);

void tr_free(region* tm_region, transac* tx);