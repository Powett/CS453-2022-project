#include "sets.h"
#include "macros.h"


void clear_rSet(rSet* set){
    while (set){
        rSet* tail = set->next;
        free(set);
        set=tail;
    }
}
void clear_wSet(wSet* set){
    while (set){
        wSet* tail = set->next;
        free(set->src);
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
    bool expected_lock=false;
    while (set){
            if (likely(!set->isFreed)){
                if (!atomic_compare_exchange_strong(&(set->ls->locked), &expected_lock, true)){
                    wSet_release_locks(start, set, -1);
                    if (DEBUG){
                        printf("Failed wSet acquire on lock %p\n", set->ls);
                    }
                    return false;
                }
                if (DEBUG>2){
                    printf("Locked lock %p\n", set->ls);
                }
            }
            set=set->next;
        }
    return true;
}

void wSet_release_locks(wSet* start, wSet* end, int wv){
    wSet* set=start;
    bool expected_lock=true;
    while (set && set!=end){
        wSet* tail=set->next;
        if (likely(!set->isFreed)){
            if (DEBUG>2){
                printf("Unlocked lock %p\n", set->ls);
            }
            if (wv!=-1){
                set->ls->versionStamp=wv;
            }
            if (!atomic_compare_exchange_strong(&(set->ls->locked), &expected_lock, false)){
                printf("Error: Tried to release unlocked lock\n");
                return;
            }
        }
        set=tail;
    }
    if (set!=end){
        printf("Error occured while releasing locks: unexpected NULL");
    }
}

bool rSet_check(rSet* set, int wv, int rv){
    if (wv!=rv+1){
        while (set){
            rSet* tail=set->next;
            if (atomic_load(&(set->ls->locked)) || set->ls->versionStamp > rv || set->ls->versionStamp>set->old_version){
                if (DEBUG){
                    printf("Failed rSet check on lock %p, locked: %d, vStamp/oldVstamp/rv : %d/%d/%d\n", set->ls, set->ls->locked, set->ls->versionStamp, set->old_version,rv);
                }
                return false;
            }
            free(set);
            set=tail;
        }
    }else{
        clear_rSet(set);
    }
    return true;
}

bool wSet_commit_release(region* tm_region, wSet* set, int wv){
    bool expected_lock=true;
    while (set){
        wSet* tail=set->next;
        if (!set->isFreed){
            memcpy(set->dest, set->src, tm_region->align);
        }
        if (likely(!set->isFreed)){
            if (wv!=-1){
                set->ls->versionStamp=wv;
            }
            if (!atomic_compare_exchange_strong(&(set->ls->locked), &expected_lock, false)){
                printf("Fatal Error: Tried to release unlocked lock in wSet commit release clear\n");
            }
        }
        set=tail;
    }
    return true;
}

void abort_tr(transac* tr){
    if (unlikely(!tr)){
        return;
    }
    clear_wSet(tr->wSet);
    clear_rSet(tr->rSet);
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