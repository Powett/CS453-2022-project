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
    printf("Coucou\n");
    shared_t* tm_region = tm_create(8,8);
    void* start=tm_start(tm_region);
    uint64_t source=0xffff;
    uint64_t dest=0;
    tx_t tx,tx0, tx1;

    tx=tm_begin(tm_region, false);
    if (tm_write(tm_region, tx,  (void*) &source, sizeof(source), start)){
        printf("Write OK\n");
        tm_end(tm_region, tx);
    }

    // if (fork()==0){
        // source=0xaa;
        // tx0=tm_begin(tm_region, false);
        // if (tm_write(tm_region, tx0,  (void*) &source, sizeof(source), start)){
        //     printf("Write %d OK\n", source);
        //     tm_end(tm_region, tx0);
        // }
    // }else{
        // source=7;
        // tx1=tm_begin(tm_region, false);
        // if (tm_write(tm_region, tx1,  (void*) &source, sizeof(source), start)){
        //     printf("Write %d OK\n", source);
        //     tm_end(tm_region, tx1);
        // }
        // return 0;
        // return 0;
    // }
    // tx_t tx=tm_begin(tm_region, false);

    // printf("S: %ld\n", sizeof(source));
    tx0=tm_begin(tm_region, true);
    if (tm_read(tm_region, tx0, start, sizeof(dest), (void*) &dest)){
        printf("Read OK\n");
        tm_end(tm_region, tx0);
    }
    // source=7;
    // tm_write(tm_region, tx,  (void*) &source, sizeof(source), start);
    // display_region((region*) tm_region);
    // printf("Ended ok ?: %d\n", tm_end(tm_region, tx));
    display_region((region*) tm_region);
    printf("Value of dest: %d\n", dest);
    tm_destroy(tm_region);
}