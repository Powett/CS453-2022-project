#include <pthread.h>
#include "lockStamp.h"
#include "macros.h"

bool init_lockstamp(lockStamp* ls, int version){
    ls->versionStamp=version;
    atomic_init(&(ls->locked), false);
    return true;
}

