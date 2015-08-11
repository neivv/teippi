#if defined PERFORMANCE_DEBUG
#include "perfclock.h"
#include <algorithm>
#include "string.h"
#include "console/windows_wrap.h"

#include "log.h"

std::vector<StaticPerfClock *> StaticPerfClock::clocks;
size_t StaticPerfClock::clocks_it = 0;

static uint64_t frequency;

void InitPerfClockFrequency()
{
    QueryPerformanceFrequency((LARGE_INTEGER *)&frequency);
    frequency /= 1000;
}

PerfClock::PerfClock(bool auto_start)
{
    if (auto_start)
        Start();
}

void PerfClock::Start()
{
    running = true;
    QueryPerformanceCounter((LARGE_INTEGER *)&start_time);
}

double PerfClock::GetTime()
{
    if (running)
    {
        uint64_t current_tick;
        QueryPerformanceCounter((LARGE_INTEGER *)&current_tick);
        time = (double)(current_tick - start_time) / frequency;
    }

    return time;
}

double PerfClock::Stop()
{
    double tmp = GetTime();
    running = false;
    return tmp;
}

StaticPerfClock::StaticPerfClock(const char *name_) : name(name_)
{
    time = 0;
    count = 0;
    running = false;
    is_in_clocks = false;
}

StaticPerfClock *StaticPerfClock::PopNext()
{
    if (clocks_it >= clocks.size())
    {
        clocks_it = 0;
        clocks.clear();
        return 0;
    }
    StaticPerfClock *clock = clocks[clocks_it++];
    clock->Stop();
    clock->old_time = clock->time;
    clock->time = 0;
    clock->old_count = clock->count;
    clock->count = 0;
    clock->is_in_clocks = false;
    return clock;
}

void StaticPerfClock::Clear()
{
    for (auto &clock : clocks)
    {
        clock->Stop();
        clock->time = 0;
        clock->count = 0;
        clock->is_in_clocks = false;
    }
    clocks.clear();
    clocks_it = 0;
}

void StaticPerfClock::ClearWithLog(const char *calling_function)
{
    if (StaticPerfClock::IsEmpty())
        return;

    perf_log->Log("Misc logged calls before %s:\n", calling_function);
    perf_log->Indent(2);
    StaticPerfClock::LogCalls();
    perf_log->Indent(-2);
}

void StaticPerfClock::LogCalls()
{
    while (auto clock = StaticPerfClock::PopNext())
    {
        perf_log->Log("%s: %d times, %f ms\n", clock->GetName(), clock->GetOldCount(), clock->GetOldTime());
    }
}

double StaticPerfClock::GetTime()
{
    if (running)
    {
        uint64_t current_tick;
        QueryPerformanceCounter((LARGE_INTEGER *)&current_tick);
        time += current_tick - start_time;
        start_time = current_tick;
    }

    return (double)time / frequency;
}

double StaticPerfClock::GetOldTime()
{
    return (double)old_time / frequency;
}

void StaticPerfClock::Start()
{
    if (!running)
    {
        running = true;
        QueryPerformanceCounter((LARGE_INTEGER *)&start_time);
        if (!is_in_clocks)
        {
            auto pos = std::lower_bound
                (clocks.begin(), clocks.end(), this,
                    [](const StaticPerfClock *a, const StaticPerfClock *b) { return strcmp(a->name, b->name) < 0; }
                );
            clocks.insert(pos, this);
            is_in_clocks = true;
        }
    }
    count++;
}

void StaticPerfClock::Stop()
{
    if (running)
    {
        uint64_t current_tick;
        QueryPerformanceCounter((LARGE_INTEGER *)&current_tick);
        time += current_tick - start_time;
        start_time = current_tick;
        running = false;
    }
}

StaticPerfClock_AutoStop StaticPerfClock::Start_AutoStop()
{
    if (!running)
    {
        Start();
        return StaticPerfClock_AutoStop(this);
    }
    else
    {
        count++;
        return StaticPerfClock_AutoStop(0);
    }
}

StaticPerfClock_AutoStop::~StaticPerfClock_AutoStop()
{
    if (clock)
        clock->Stop();
}

#endif
