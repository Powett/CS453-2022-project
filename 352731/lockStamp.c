#include <pthread.h>
#include "lockStamp.h"
#include "macros.h"

// bool take_lockstamp(lockStamp* ls){
//     int val = pthread_mutex_lock(&(ls->mutex));
// //    if (DEBUG && val){
// //        printf("Taking lockstamp error: %d\n", val);
// //    }
//     return val==0;
// }
// bool release_lockstamp(lockStamp* ls){
//     int val = pthread_mutex_unlock(&(ls->mutex));
// //    if (DEBUG && val){
// //        printf("Taking lockstamp error: %d\n", val);
// //    }
//     return val==0;
// }

bool init_lockstamp(lockStamp* ls, int version){
    ls->versionStamp=version;
    atomic_init(&(ls->locked), false);
    return true;
}

