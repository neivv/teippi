#ifndef SCTHREAD_H
#define SCTHREAD_H

#include "thread.h"
#include "patch/memory.h"

struct ScThreadVars
{
    TempMemoryPool unit_search_pool;
};

extern ThreadPool<ScThreadVars> *threads;

#endif // SCTHREAD_H

