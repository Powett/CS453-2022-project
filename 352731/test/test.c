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

#include <errno.h>

#include "macros.h"
#include "sets.h"
#include "lockStamp.h"
#include "displayers.h"

int main(){
    shared_t* tm_region = tm_create(8,8);
    void* start=tm_start(tm_region);
    tx_t tx=tm_begin(tm_region, false);
    uint64_t source=5;
    uint64_t dest=0;
    printf("S: %ld\n", sizeof(source));
    tm_write(tm_region, tx,  (void*) &source, sizeof(source), start);
    tm_read(tm_region, tx, start, sizeof(dest), (void*) &dest);
    source=7;
    tm_write(tm_region, tx,  (void*) &source, sizeof(source), start);
    display_region((region*) tm_region);
    printf("Ended ok ?: %d\n", tm_end(tm_region, tx));
    display_region((region*) tm_region);
    printf("Value of dest: %d\n", dest);
    // tm_destroy(tm_region);
}