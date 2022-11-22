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
    if (set){
        clear_wSet(set->left);
        clear_wSet(set->right);
        free(set->src);
        free(set);
    }
}

segment* find_segment(shared_t shared, word* target){
    region* tm_region=(region* ) shared;
    segment* segment=tm_region->allocs;
    while (segment && segment->raw_data<=target){
        if ((segment->raw_data+(segment->len)*tm_region->align)>=target){
       if (DEBUG>2){
           printf("Checking bounds to find seg(%p): [%p - %p]\n", target, segment->raw_data, segment->raw_data+(segment->len)*tm_region->align);
       }
            return segment;
        }
        segment=segment->next;
    }
    return NULL;
}

void* add_segment(shared_t shared, segment* seg){
    region* tm_region=(region*) shared;
    segment* cursor=tm_region->allocs;
    void* raw_data_start = seg->raw_data;
    if (raw_data_start<=tm_region->allocs->raw_data){
        seg->next=tm_region->allocs;
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


bool rSet_check(rSet* set, int wv, int rv){
    if (wv!=rv+1){
        while (set){
            rSet* tail=set->next;
            if (test_lockstamp(set->ls) || set->ls->versionStamp > rv){
               if (DEBUG>1){
                   printf("Failed rSet check on lock %p, locked: %d, vStamp/rv : %d/%d\n", set->ls, set->ls->locked, set->ls->versionStamp,rv);
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

void abort_tr(transac* tr){
    if (unlikely(!tr)){
        return;
    }
    clear_wSet(tr->wSet);
    clear_rSet(tr->rSet);
    free(tr);
}

wSet* wSet_contains(word* addr, wSet* set){
    wSet* side=NULL;
    if (!set){
        return NULL;
    }
    if (addr == set->dest){
        return set;
    }
    if (addr > set->dest){
        side=set->right;
    }else{
        side=set->left;
    }
    return wSet_contains(addr, side);
}

wSet* wSet_insert(wSet* node, word* addr, wSet* parent){
    if (unlikely(!parent)){
        return node;
    }
    wSet* side=NULL;
    if (addr > parent->dest){
        if (!parent->right){
            parent->right=node;
            return parent;
        }else{
            side=parent->right;
        }
    }else{
        if (!parent->left){
            parent->left=node;
            return parent;
        }else{
            side=parent->left;
        }
    }
    wSet_insert(node,addr,side);
    return parent;
}

bool wSet_acquire_locks(wSet* set){
    if (!set){
        return true;
    }
    if (!take_lockstamp(set->ls)){
        return false;
    }
    if (!wSet_acquire_locks(set->left)){
        release_lockstamp(set->ls);
        return false;
    }
    if (!wSet_acquire_locks(set->right)){
        wSet_release_locks(set->left, -1);
        release_lockstamp(set->ls);
        return false;
    }
    return true;
}

void wSet_release_locks(wSet* root, int wv_to_write){
    if (!root){
        return;
    }
    if (wv_to_write!=-1){
        root->ls->versionStamp=wv_to_write;
    }
    release_lockstamp(root->ls);
    wSet_release_locks(root->left,wv_to_write);
    wSet_release_locks(root->right,wv_to_write);
}

void wSet_commit_release(region* tm_region, wSet* set, int wv){
    if (!set){
        return;
    }
    if (likely(!set->isFreed)){
        memcpy(set->dest, set->src, tm_region->align);
        if (wv!=-1){
            set->ls->versionStamp=wv;
        }
        release_lockstamp(set->ls);
    }else if (set->segToFree){
        segment* seg = set->segToFree;
        segment* prev=tm_region->allocs;
        if (prev==seg){
            tm_region->allocs=seg->next;
        }else{
            while (prev && prev->next!=seg){
                prev=prev->next;
            }
            if (!prev){
                printf("Fatal error: could not free cell: possible double free?\n");
                return;
            }
            prev->next=seg->next;
        }
        free(seg->locks);
        free(seg->raw_data);
        free(seg);
    }
    wSet_commit_release(tm_region,set->left,wv);
    wSet_commit_release(tm_region,set->right,wv);
    free(set->src);
    free(set);
}
