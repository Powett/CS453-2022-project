#include "sets.h"
#include "macros.h"


void clearrSet(rSet* set){
    while (set){
        rSet* tail = set->next;
        free(set);
        set=tail;
    }
}
void clearwSet(wSet* set){
    while (set){
        wSet* tail = set->next;
        free(set);
        set=tail;
    }
}

segment* find_segment(shared_t shared, word* target){
    region* tm_region=(region* ) shared;
    segment* segment=tm_region->allocs;
    while (segment && (segment->raw_data+(segment->len)*tm_region->align<target)){
        if (DEBUG){
            printf("Checking bounds to find seg(%p): [%p - %p]\n", target, segment->raw_data, segment->raw_data+(segment->len)*tm_region->align);
        }
        segment=segment->next;
    }
    if (!segment){
        return NULL;
    }
    return segment;
}

void* add_segment(shared_t shared, segment* seg){
    region* tm_region=(region*) shared;
    segment* cursor=tm_region->allocs;
    void* raw_data_start = seg->raw_data;
    if (raw_data_start<cursor->raw_data){
        seg->next=cursor;
        tm_region->allocs=seg;
        return raw_data_start;
    }
    while (cursor->next && cursor->next->raw_data < raw_data_start){
        cursor=cursor->next;
    }
    seg->next=cursor->next;
    cursor->next=seg;
    return raw_data_start;
}

bool wSet_acquire_locks(wSet* set){
    wSet* start=set;
    while (set){
            if (likely(!set->isFreed)){
                take_lockstamp(set->ls);
                if (set->ls->locked){
                    release_lockstamp(set->ls);
                    wSet_release_locks(start, set, -1);
                    if (DEBUG){
                        printf("Failed wSet acquire on lock %p\n", set->ls);
                    }
                    return false;
                }
                set->ls->locked=true;
                if (DEBUG>2){
                    printf("Locked lock %p\n", set->ls);
                }
                release_lockstamp(set->ls);
            }
            set=set->next;
        }
    return true;
}

void wSet_release_locks(wSet* start, wSet* end, int wv){
    wSet* set=start;
    while (set && set!=end){
        if (likely(!set->isFreed)){
            take_lockstamp(set->ls);
            if (DEBUG>2){
                printf("Unlocked lock %p\n", set->ls);
            }
            if (wv!=-1){
                set->ls->versionStamp=wv;
            }
            set->ls->locked=false;
            release_lockstamp(set->ls);
        }
        set=set->next;
    }
    if (set!=end){
        printf("Error occured while releasing locks: unexpected NULL");
    }
}

bool rSet_check(rSet* set, int wv, int rv){
    if (wv!=rv+1){
        while (set){
            take_lockstamp(set->ls);
            if (set->ls->locked || set->ls->versionStamp > rv || set->ls->versionStamp>set->old_version){
                if (DEBUG){
                    printf("Failed rSet check on lock %p, locked: %d, vStamp/oldVstamp/rv : %d/%d/%d\n", set->ls, set->ls->locked, set->ls->versionStamp, set->old_version,rv);
                }
                release_lockstamp(set->ls);
                return false;
            }
            release_lockstamp(set->ls);
            set=set->next;
        }
    }
    return true;
}

bool wSet_commit(region* tm_region, wSet* set){
    while (set){
        if (!set->isFreed){
            memcpy(set->dest, set->src, tm_region->align);
        }
        set=set->next;
    }
    return true;
}

void tr_free(unused(region* tm_region), transac* tr){
    if (unlikely(!tr)){
        return;
    }
    rSet* rCursor=tr->rSet;
    while (rCursor){
        rSet* rtail=(rCursor)->next;
        free(rCursor);
        rCursor=rtail;
    }
    tr->rSet=NULL;

    wSet* wCursor=tr->wSet;
    wSet* wtail;
    while (wCursor){
        wtail=wCursor->next;
        free(wCursor->src);
        free(wCursor);
        wCursor=wtail;
    }
    tr->wSet=NULL;
    
    if (tm_region->pending==tr){
        tm_region->pending=tr->next;
    }
    if(tr->next){
        (tr->next)->prev=tr->prev;
    }
    if(tr->prev){
        (tr->prev)->next=tr->next;
    }
}

wSet* wSet_contains(word* addr, wSet* set){
    while (set){
        if (set->dest==addr){
            return set;
        }
        set=set->next;
    }
    return NULL;
}