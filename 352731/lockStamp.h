#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include "tm.h"

typedef struct lockStamp{
    int versionStamp;
    atomic_bool locked;
}lockStamp;


bool init_lockstamp(lockStamp* ls, int version);
bool take_lockstamp(lockStamp* ls);
bool release_lockstamp(lockStamp* ls);
bool test_lockstamp(lockStamp* ls);