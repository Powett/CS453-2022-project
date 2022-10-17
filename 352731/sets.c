#include "sets.h"

void clearSet(rSet* set){
    while (set){
        rSet* tail = set->next;
        free(set);
        set=tail;
    }
}
void clearSet(wSet* set){
    while (set){
        wSet* tail = set->next;
        free(set);
        set=tail;
    }
}

lockStamp* find_lock(shared_t shared, void* target){
    struct region* region=(struct region* ) shared;
    struct segment* segment=region->allocs;
    while (segment && segment->next->raw_data<target){
        segment=segment->next;
    }
    if (!segment){
        return NULL;
    }
    int index = (int) (target-segment->raw_data)/region->align;
    if (index<0 || index >= segment->size){
        return NULL;
    }
    return (lockStamp*)&(segment->locks[index]);
}
segment* find_segment(shared_t shared, void* target){
    struct region* region=(struct region* ) shared;
    struct segment* segment=region->allocs;
    while (segment && segment->next->raw_data<target){
        segment=segment->next;
    }
    if (!segment){
        return NULL;
    }
    return segment;
}

bool wSet_acquire_locks(wSet* set){
    wSet* init=set;
    bool lock_free=true;
    while (set){
            // CAS lock -> lock_free
            if (!lock_free){
                wSet_release_locks(init, set, -1);
                return false;
            }
            set=set->next;
        }
}

void wSet_release_locks(wSet* set, wSet* stop, int wv)
{
    while (set && set!=stop){
        // unlock 
        set=set->prev;
        if (wv!=-1){
            // set clock
        }
    }
}

bool rSet_check(rSet* set, int wv, int rv){
    bool lock_free=true;
    int localVersion=0;
    if (wv>rv+1){
        while (set){
            // fetch lock -> lock_free
            if (!lock_free || localVersion>rv){
                return false;
            }
            set=set->next;
        }
    }
    return true;
}

void rSet_commit(region* tm_region, rSet* set){
    while (set){
        memcpy(set->dest, set->src, tm_region->align);
        set=set->next;
    }
}

void wSet_commit(region* tm_region, wSet* set){
    while (set){
        if (!set->isFreed){
            memcpy(set->dest, set->src, tm_region->align);
            set=set->next;
        }
    }
}

void tr_free(tx_t tx){
    transac* tr=(transac*)tx;
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
    free(tx);
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