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
    printf("Alignment: %ld, Clock %d\n", tm_region->align, atomic_load(&(tm_region->clock)));
    printf("First segment: %p\n", tm_region->segment_start);
    printf("==== Segments\n");
    segment* seg=tm_region->allocs;
    while (seg){
        display_segment(seg, tm_region->align);
        seg=seg->next;
    }
    printf("\n");
    pthread_mutex_unlock(&(tm_region->debug_lock));
}
void display_segment(segment* sg, size_t align){
    printf ("- Segment %p: ", sg);
    printf("Len: %ld, Next:%p\n", sg->len, sg->next);
    printf("Data:\n");
    for (size_t i=0;i<sg->len;i++){
        printf("(@%p):", sg->raw_data+(i*align));
        for (size_t j=0; j<align; j++){
            printf("%x|", *(unsigned char *)(sg->raw_data+(i*align)+j));
        }
    }
    printf("\nLocks:\n");
    for (size_t i=0;i<sg->len;i++){
        display_lock(&(sg->locks[i]));
    }
    printf("\n");
}
void display_rSet(rSet* s){
    if (!s){return;}
    printf("- rSet cell %p: ", s);
    printf("Old stamp: %d, Next: %p", s->old_version, s->next);
    printf("Lock: ");
    display_lock(s->ls);
    printf("\n=\n");
}
void display_wSet(wSet* s){
    while (s){
        printf("- wSet cell %p: ", s);
        printf("Dest: %p, Src: %p, IsFreed: %d,  next: %p\n", s->dest, s->src, s->isFreed, s->next);
        if (!s->isFreed){
            printf("Lock: ");
            display_lock(s->ls);
        }
        s=s->next;        
    }
    printf("\n=\n");
}
void display_transac(transac* tr){
    printf("- Transaction %p: ", tr);
    printf("rv: %d, wv: %d, is_ro: %d\n", tr->rv, tr->wv, tr->is_ro);
    printf("== rSet\n");
    display_rSet(tr->rSet);
    printf("== wSet\n");
    display_wSet(tr->wSet);
    printf("\n");
}
void display_lock(lockStamp* ls){
    printf("(@%p) [%04d|%d] ",ls, ls->versionStamp, ls->locked);
}

void init_display(region* tm_region){
    pthread_mutex_init(&(tm_region->debug_lock), NULL);
}