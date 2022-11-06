#include <pthread.h>
#include "lockStamp.h"

bool take_lockstamp(lockStamp* ls){
    return pthread_mutex_lock(&(ls->mutex))==0;
}
bool release_lockstamp(lockStamp* ls){
    return pthread_mutex_unlock(&(ls->mutex))==0;
}

bool init_lockstamp(lockStamp* ls, int version){
    ls->locked=false;
    ls->versionStamp=version;
    return pthread_mutex_init(&(ls->mutex), NULL)==0;
}

bool destroy_lockstamp(lockStamp* ls){
    return pthread_mutex_destroy(&(ls->mutex))==0;
}

