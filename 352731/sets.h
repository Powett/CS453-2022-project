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
    struct wSet* left;
    struct wSet* right;
    
} wSet;

// Linked lists to track read operations
typedef struct rSet{
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
    struct segment* segment_start; // First allocated segment (non-deallocatable) (may not be the first in the allocs list)
    segment_list allocs;    // Shared memory segments dynamically allocated via tm_alloc within transactions, ordered by growing raw data (first) address
    size_t align;           // Size of a word in the shared memory region (in bytes)
    atomic_int clock;       // Global clock used for time-stamping, perfectible ?
 } region;


segment* find_segment(shared_t shared, word* target);
void* add_segment(shared_t shared, segment* seg);

void clear_rSet(rSet* set);
void clear_wSet(wSet* set);

wSet* wSet_contains(word* addr, wSet* set);
wSet* wSet_insert(wSet* node, word* addr, wSet* parent);

bool wSet_acquire_locks(wSet* set);
void wSet_abandon_locks_free(wSet* root);

bool rSet_check(rSet* set, int wv, int rv);
void wSet_commit_release_free(region* tm_region, wSet* set, int wv);

void abort_tr(transac* tx);