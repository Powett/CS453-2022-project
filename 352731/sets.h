#pragma once

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <tm.h>
typedef void word;

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
    atomic_int clock;       // Global clock used for time-stamping, perfectible ?
    transac* pending;       // Currently began but not ended transactions
 } region;

// Linked list structure to keep track of active transactions
typedef struct transac{
    int rv;             // First clock counter
    int wv;             // Second clock counter
    wSet* wSet;         // wSet to track write operations
    rSet* rSet;         // rSet to track read operations
    bool is_ro;
    transac* prev;       
    transac* next;
}transac;

// Linked lists to track write operations
// A "free" is considered as a special write, preventing further r/w
typedef struct wSet{
    void* src;
    word* dest;
    bool isFreed;
    wSet* next;
    wSet* prev;
}wSet;

// Linked lists to track read operations
typedef struct rSet{
    void* dest;
    word* src;
    rSet* next;
}rSet;

typedef int lockStamp;

#define non_atomic_islock(lockStamp) lockStamp & 1
#define non_atomic_version(lockStamp) lockStamp>>1

/** Getting lockStamp for given word in memory
 * @param shared Shared memory region associated with the transaction
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
lockStamp* find_lock(shared_t shared, void* target);
segment* find_segment(shared_t shared, void* target);

/** Recursively freeing a rwSet
 * @param set Set to clear (every node is freed, at the end set is an unusable memory address)
 * @return void ?
**/
void clearrSet(rSet* set);
void clearwSet(wSet* set);

bool wSet_acquire_locks(wSet* set);
void wSet_release_locks(wSet* set, wSet* stop, int wv);
bool rSet_check(rSet* set, int wv, int rv);
void rSet_commit(region* tm_region, rSet* set);
void wSet_commit(region* tm_region, wSet* set);

void tr_free(tx_t tx);