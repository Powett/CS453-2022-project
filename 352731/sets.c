#include "sets.h"

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
    while (segment && (segment->raw_data+(segment->size)*tm_region->align<target)){
        segment=segment->next;
    }
    if (!segment){
        return NULL;
    }
    // printf("Found segment: %p\n");
    return segment;
}

lockStamp* find_lock(shared_t shared, segment* segment, word* target){
    region* tm_region=(region* ) shared;
    int index = (int) (target-segment->raw_data)/tm_region->align;
    // printf("In find_lock: index of %p is %d", target, index);
    if (index<0 || index >= segment->size){
        return NULL;
    }
    return &(segment->locks[index]);
}

lockStamp* find_lock_from_target(shared_t shared, word* target){
    // printf("Trying to find lock for %p\n", target);
    segment* sg=find_segment(shared, target);
    if (sg){
        return find_lock(shared,sg,target);
    }
    return NULL;
}

bool wSet_acquire_locks(wSet* set){
    wSet* start=set;
    while (set){
            take_lockstamp(set->ls);
            if (set->ls->locked){
                release_lockstamp(set->ls);
                wSet_release_locks(start, set, -1);
                return false;
            }
            set->ls->locked=true;
            release_lockstamp(set->ls);
            set=set->next;
        }
    return true;
}

void wSet_release_locks(wSet* start, wSet* end, int wv){
    wSet* set=start;
    while (set && set!=end){
        take_lockstamp(set->ls);
        set->ls->locked=false;
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

void tr_free(region* tm_region, transac* tr){
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