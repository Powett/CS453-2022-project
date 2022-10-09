#pragma once

#include <pthread.h>
#include <stdbool.h>

typedef void word;

// Linked lists to track read and write operations
typedef struct rwSet{  //needs concurrency-safe ?
    void* ext;
    word* cell;
    rwSet* next;
}rwSet;

// Linked list to track segments to be freed
typedef struct fSet{
    segment* segment;
    fSet* next;
}fSet;

//allocatedSet to make sure no mem leaks if abort ?



/** Recursively freeing a rwSet
 * @param set Set to clear (every node is freed, at the end set is an unusable memory address)
 * @return void ?
**/
void clearSet(rwSet* set);

