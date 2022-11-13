#pragma once

#include "lockStamp.h"
#include "sets.h"
#include <tm.h>



void init_display(region* tm_region);
void display_region(region* tm_region);
void display_segment(segment* sg, size_t align);
void display_rSet(rSet* s);
void display_wSet(wSet* s);
void display_transac(transac* tr);
void display_lock(lockStamp* ls);
