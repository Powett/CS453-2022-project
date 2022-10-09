#include "lockStamp.h"

// bool lock_init(struct lock_t* lock) {
//     return pthread_mutex_init(&(lock->mutex), NULL) == 0
//         && pthread_cond_init(&(lock->cv), NULL) == 0;
// }

// void lock_cleanup(struct lock_t* lock) {
//     pthread_mutex_destroy(&(lock->mutex));
//     pthread_cond_destroy(&(lock->cv));
// }

// bool lock_acquire(struct lock_t* lock) {
//     return pthread_mutex_lock(&(lock->mutex)) == 0;
// }

// void lock_release(struct lock_t* lock) {
//     pthread_mutex_unlock(&(lock->mutex));
// }

// void lock_wait(struct lock_t* lock) {
//     pthread_cond_wait(&(lock->cv), &(lock->mutex));
// }

// void lock_wake_up(struct lock_t* lock) {
//     pthread_cond_broadcast(&(lock->cv));
// }

lockStamp find_lock(shared_t shared, void* target){
    struct region* region=(struct region* ) shared;
    struct segment* segment=region->allocs;
    while (segment && segment->next->raw_data<target){
        segment=segment->next;
    }
    if (!segment){
        return -1;
    }
    int index = (int) (target-segment->raw_data)/region->align;
    if (index<0 || index >= segment->size){
        return -1;
    }
    return (lockStamp)(segment->locks[index]);
}