#include <pthread.h>
#include <stdatomic.h>
#include "lockStamp.h"
#include "macros.h"

bool init_lockstamp(lockStamp* ls, int version){
    ls->versionStamp=version;
    atomic_init(&(ls->locked), false);
    return true;
}

bool take_lockstamp(lockStamp* ls){
    bool expected=false;
    return atomic_compare_exchange_strong(&(ls->locked), &expected, true);
}
bool release_lockstamp(lockStamp* ls){
    bool expected=true;
    if (!atomic_compare_exchange_strong(&(ls->locked), &expected, false)){
        printf("Error: trying to release an unlocked lock\n");
        return false;
    }
    return true;
}
bool test_lockstamp(lockStamp* ls){
    return atomic_load(&(ls->locked));
}