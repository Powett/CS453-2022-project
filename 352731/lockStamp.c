#include <pthread.h>
#include "lockStamp.h"
#include "macros.h"

bool take_lockstamp(lockStamp* ls){
    int val = pthread_mutex_lock(&(ls->mutex));
    if (DEBUG && val){
        printf("Taking lockstamp error: %d\n", val);
    }
    return val==0;
}
bool release_lockstamp(lockStamp* ls){
    int val = pthread_mutex_unlock(&(ls->mutex));
    if (DEBUG && val){
        printf("Taking lockstamp error: %d\n", val);
    }
    return val==0;
}

bool init_lockstamp(lockStamp* ls, int version){
    ls->locked=false;
    ls->versionStamp=version;
    return pthread_mutex_init(&(ls->mutex), NULL)==0;
}

bool destroy_lockstamp(lockStamp* ls){
    return pthread_mutex_destroy(&(ls->mutex))==0;
}

