#ifndef PERF_KLOK_HOO
#define PERF_KLOK_HOO

#if defined PERFORMANCE_DEBUG
#include <vector>
#include <string>

#define STATIC_PERF_CLOCK(str) \
    static StaticPerfClock *clock_ ## str ## __;\
    if (clock_ ## str ## __ == nullptr) { clock_ ## str ## __ = new StaticPerfClock(#str); }\
    auto str ## _clock_stop = clock_ ## str ## __->Start_AutoStop();

void InitPerfClockFrequency();

class StaticPerfClock;

class StaticPerfClock_AutoStop
{
    public:
        StaticPerfClock_AutoStop(StaticPerfClock *clock_) : clock(clock_) {}
        StaticPerfClock_AutoStop(const StaticPerfClock_AutoStop &other) = delete;
        ~StaticPerfClock_AutoStop();

        StaticPerfClock_AutoStop(StaticPerfClock_AutoStop &&other) = default;
        StaticPerfClock_AutoStop& operator=(StaticPerfClock_AutoStop &&other) = default;

    private:
        StaticPerfClock *clock;
};

class PerfClock
{
    public:
        PerfClock(bool auto_start = true);
        ~PerfClock() {}
        void Start();
        double GetTime();
        double Stop();

    private:
        uint64_t start_time;
        double time;
        bool running;
};

class StaticPerfClock
{
    public:
        StaticPerfClock(const char *name);
        ~StaticPerfClock() {}

        double GetTime();
        void Start();
        StaticPerfClock_AutoStop Start_AutoStop();
        void Stop();

        const char *GetName() { return name; }
        double GetOldTime();
        int GetOldCount() { return old_count; }

        static StaticPerfClock *PopNext();
        static bool IsEmpty() { return clocks.empty(); }
        static void Clear();
        static void ClearWithLog(const char *calling_function);
        static void LogCalls();

    private:
        const char *name;
        int count;
        int old_count;
        static std::vector<StaticPerfClock *> clocks;
        static size_t clocks_it;
        bool is_in_clocks;

        uint64_t start_time;
        uint64_t time;
        uint64_t old_time;
        bool running;
};
#else

#define STATIC_PERF_CLOCK(str)

inline void InitPerfClockFrequency() {}

class PerfClock
{
    public:
        PerfClock(bool auto_start = true) {}
        ~PerfClock() {}
        void Start() {}
        double GetTime() { return 0; }
        double Stop() { return 0; }
};

class StaticPerfClock
{
    public:
        class StaticPerfClock_AutoStop
        {
        };

        static StaticPerfClock *PopNext() { return nullptr; }
        static bool IsEmpty() { return true; }
        static void Clear() {}
        static void ClearWithLog(const char *calling_function) {}
        static void LogCalls() {}

        double GetTime() { return 0; }
        void Start() {}
        StaticPerfClock_AutoStop Start_AutoStop() { return StaticPerfClock_AutoStop(); }
        void Stop() {}

        const char *GetName() { return nullptr; } // Should never be called
        double GetOldTime() { return 0; }
        int GetOldCount() { return 0; }
};
#endif

#endif // PERF_KLOK_HOO
