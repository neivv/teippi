#ifndef LOG_H
#define LOG_H

#include <stdint.h>
#include <stdio.h>
#include <atomic>
#include <string>

class DebugLog_Actual
{
    public:
        DebugLog_Actual(const char *);
        ~DebugLog_Actual();
        void Log(const char *format, ...);
        void Indent(int diff);
        void AutoFlush(bool new_state) { auto_flush = new_state; }
        void Flush() { fflush(log_file); }

    private:
        FILE *log_file;
        int indent;
        uint32_t prev_frame;
        std::atomic_flag lock;
        std::string filename;

        bool auto_flush;

        friend class Unlock;
        class Unlock
        {
            public:
                Unlock(DebugLog_Actual *parent_) : parent(parent_) {}
                Unlock(Unlock &&other) { parent = other.parent; other.parent = 0; }
                ~Unlock() { if (parent) parent->lock.clear(std::memory_order_release); }
                DebugLog_Actual *parent;
        };
        Unlock Lock();
};


class DebugLog_Empty
{
    public:
        DebugLog_Empty(const char *) {}
        ~DebugLog_Empty() {}
        void Log(const char *format, ...) {}
        void Indent(int diff) {}
        void AutoFlush(bool new_state) {}
        void Flush() {}
};

void InitLogs();

#ifdef DEBUG
typedef DebugLog_Actual DebugLog;
#else
typedef DebugLog_Empty DebugLog;
#endif
#ifdef PERFORMANCE_DEBUG
typedef DebugLog_Actual PerfLog;
#else
typedef DebugLog_Empty PerfLog;
#endif
#ifdef SYNC
typedef DebugLog_Actual SyncLog;
#else
typedef DebugLog_Empty SyncLog;
#endif

extern DebugLog *debug_log;
extern PerfLog *perf_log;
extern DebugLog_Actual *error_log;

extern SyncLog *sync_log;
extern DebugLog_Actual *unit_dump;
extern char log_path[260];


#endif // LOG_H

