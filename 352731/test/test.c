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
#include <sys/types.h>
#include <unistd.h>
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
    void* seg0;
    void* seg1;
    void* seg2;
    seg0=tm_start(tm_region);
    uint64_t source=1;
    uint64_t dest=0;
    tx_t tx, tx1, tx_2;

    tx=tm_begin(tm_region, false);
    printf("Write OK?: %d \n", tm_write(tm_region, tx,  (void*) &source, sizeof(source), start));
    printf("Alloc OK?: %d\n", tm_alloc(tm_region, tx,16,&seg1)==success_alloc);
    tx1=tm_begin(tm_region, false);
    source=2;
    printf("Read OK?: %d\n", tm_read(tm_region, tx1, start, sizeof(dest), (void*) &dest));
    printf("Write OK?: %d \n", tm_write(tm_region, tx1,  (void*) &source, sizeof(source), start));
    printf("Alloc OK?: %d\n", tm_alloc(tm_region, tx1,16,&seg2)==success_alloc);
    source=5;
    printf("Write OK?: %d\n", tm_write(tm_region, tx, (void*) &source, sizeof(source), seg2));
    printf("Free OK?: %d\n", tm_free(tm_region,tx, seg2));
    printf("End OK?: %d\n", tm_end(tm_region, tx));
    printf("End OK?: %d\n", tm_end(tm_region, tx1));
    // display_region((region*)tm_region);
    tm_destroy(tm_region);
}