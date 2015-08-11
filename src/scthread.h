#ifndef SCTHREAD_H
#define SCTHREAD_H

#include "thread.h"
#include "memory.h"

struct ScThreadVars
{
    TempMemoryPool unit_search_pool;
};

extern ThreadPool<ScThreadVars> *threads;

#endif // SCTHREAD_H

