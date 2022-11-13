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
            if (DEBUG>2){
                printf("Old set lock:%p\n", set->ls);
            }
            set=set->next;
            if (DEBUG>2){
                printf("New set lock:%p\n", set->ls);
            }
        }
    return true;
}

void wSet_release_locks(wSet* start, wSet* end, int wv){
    wSet* set=start;
    while (set && set!=end){
        take_lockstamp(set->ls);
        set->ls->locked=false;
        if (DEBUG>2){
            printf("Unlocked lock %p\n", set->ls);
        }
        if (wv!=-1){
            set->ls->versionStamp=wv;
        }
        release_lockstamp(set->ls);
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
            if (set->ls->locked || set->ls->versionStamp > rv){
                if (DEBUG){
                    printf("Failed rSet check on lock %p, locked: %d, vStamp/rv : %d/%d\n", set->ls, set->ls->locked, set->ls->versionStamp, rv);
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

bool rSet_commit(region* tm_region, rSet* set){
    while (set){
        memcpy(set->dest, set->src, tm_region->align);
        set=set->next;
    }
    return true;
}

bool wSet_commit(region* tm_region, wSet* set){
    while (set){
        if (!set->isFreed){
            memcpy(set->dest, set->src, tm_region->align);
            set=set->next;
        }else{
            printf("Impossible: In commit, trying to write after free!");
            return false;
        }
    }
    return true;
}

void tr_free(unused(region* tm_region), transac* tr){
    if (unlikely(!tr)){
        return;
    }
    while (tr->rSet){
        rSet* tail=tr->rSet->next;
        free(tr->rSet);
        tr->rSet=tail;
    }
    while (tr->wSet){
        wSet* tail=tr->wSet->next;
        free(tr->wSet);
        tr->wSet=tail;
    }
    if(tr->next){
        tr->next->prev=tr->prev;
    }
    if(tr->prev){
        tr->prev->next=tr->next;
    }
    free(tr);
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