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
    uint64_t source=1;
    uint64_t dest=0;
    tx_t tx, tx1;

    tx=tm_begin(tm_region, false);
    printf("Write OK?: %d \n", tm_write(tm_region, tx,  (void*) &source, sizeof(source), start));
    
    tx1=tm_begin(tm_region, false);
    source=2;
    printf("Read OK?: %d\n", tm_read(tm_region, tx1, start, sizeof(dest), (void*) &dest));
    printf("Write OK?: %d \n", tm_write(tm_region, tx1,  (void*) &source, sizeof(source), start));
    
    tm_end(tm_region, tx);
    tm_end(tm_region, tx1);


    printf("Value of dest: %d\n", dest);   
    tm_destroy(tm_region);
}