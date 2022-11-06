#include "displayers.h"
#include "lockStamp.h"
#include "sets.h"
#include <tm.h>
#include <stdio.h>

size_t align;

void display_region(region* tm_region){

    pthread_mutex_lock(&(tm_region->debug_lock));
    align=tm_region->align;
    printf("======= Memory region %p =======\n", tm_region);
    printf("Alignment: %d, Clock %d\n", tm_region->align, atomic_load(&(tm_region->clock)));
    printf("First segment: %p\n", tm_region->segment_start);
    printf("==== Segments\n");
    segment* seg=tm_region->allocs;
    while (seg){
        display_segment(seg);
        seg=seg->next;
    }
    printf("\n");
    printf("==== Transactions\n");
    transac* pending=tm_region->pending;
    while (pending){
        display_transac(pending);
        pending=pending->next;
    }
    printf("================================================\n");
    pthread_mutex_unlock(&(tm_region->debug_lock));
}
void display_segment(segment* sg){
    printf ("- Segment %p: ", sg);
    printf("Size: %d, Next:%p\n", sg->size, sg->next);
    printf("Data:\n");
    for (unsigned long int i=0;i<sg->size;i++){
        printf("(@%p): %02x|", sg->raw_data +i*align, *(uint8_t*)(sg->raw_data +i*align));
    }
    printf("\nLocks:\n");
    for (unsigned long int i=0;i<sg->size;i++){
        display_lock(&(sg->locks[i]));
    }
}
void display_rSet(rSet* s){
    if (!s){return;}
    printf("- rSet cell %p: ", s);
    printf("Dest: %p, Src: %p, next: %p", s->dest, s->src, s->next);
    printf("Lock: ");
    display_lock(s->ls);
    printf("\n=\n");
}
void display_wSet(wSet* s){
    while (s){
        printf("- wSet cell %p: ", s);
        printf("Dest: %p, Src: %p, IsFreed: %d,  next: %p\n", s->dest, s->src, s->isFreed, s->next);
        printf("Lock: ");
        display_lock(s->ls);
        s=s->next;        
    }
    printf("\n=\n");
}
void display_transac(transac* tr){
    printf("- Transaction %p: ", tr);
    printf("rv: %d, wv: %d, is_ro: %d, prev: %p, next: %p\n", tr->rv, tr->wv, tr->is_ro, tr->prev, tr->next);
    printf("== rSet\n");
    display_rSet(tr->rSet);
    printf("== wSet\n");
    display_wSet(tr->wSet);
    printf("\n");
}
void display_lock(lockStamp* ls){
    take_lockstamp(ls);
    printf("(@%p) [%04d|%d] ",ls, ls->versionStamp, ls->locked);
    release_lockstamp(ls);
}

void init_display(region* tm_region){
    pthread_mutex_init(&(tm_region->debug_lock), NULL);
    return;
}