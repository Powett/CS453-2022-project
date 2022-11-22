#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include "tm.h"

typedef struct lockStamp{
    pthread_mutex_t mutex;
    int versionStamp;
    atomic_bool locked;
}lockStamp;

bool init_lockstamp(lockStamp* ls, int version);