#include "sets.h"

void clearSet(rwSet* set){
    while (set){
        rwSet* tail = set->next;
        free(set);
        set=tail;
    }
}