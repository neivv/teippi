#include "log.h"

#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <io.h>
#include <windows.h>

#include <algorithm>

#include "offsets.h"

char log_path[260];

DebugLog *debug_log;
PerfLog *perf_log;
SyncLog *sync_log;
DebugLog_Actual *unit_dump;
DebugLog_Actual *error_log;

DebugLog_Actual::DebugLog_Actual(const char *f) : lock(ATOMIC_FLAG_INIT), filename(f)
{
    log_file = nullptr;
    indent = 0;
    prev_frame = 0xffffffff;
    auto_flush = true;
}

DebugLog_Actual::~DebugLog_Actual()
{
    if (log_file != nullptr)
        fclose(log_file);
}

DebugLog_Actual::Unlock DebugLog_Actual::Lock()
{
    while (lock.test_and_set(std::memory_order_acquire))
        ; //Nothing
    return Unlock(this);
}

void DebugLog_Actual::Log(const char *format, ...)
{
    auto unlock = Lock();
    if (log_file == nullptr)
    {
        log_file = fopen(filename.c_str(), "w");
        if (log_file == nullptr)
            return;

        char timestamp[256];
        time_t time_ = time(0);
        struct tm *time = localtime(&time_);
        strftime(timestamp, 256, "Log started on %Y-%m-%d %H:%M:%S\n", time);
        fputs(timestamp, log_file);
    }

    uint32_t current_frame = *bw::frame_count;
    if (current_frame != prev_frame)
    {
        fprintf(log_file, "--- Frame %d\n", *bw::frame_count);
        prev_frame = current_frame;
    }

    if (indent)
    {
        char spaces[256];
        memset(spaces, ' ', std::min(indent, 256));
        for (int i = 0; i < indent; i+= 256)
        {
            if (indent - i > 256)
                fwrite(spaces, 1, 256, log_file);
            else
                fwrite(spaces, 1, indent - i, log_file);
        }
    }

    va_list varg;
    va_start(varg, format);
    vfprintf(log_file, format, varg);
    va_end(varg);

    if (auto_flush)
        Flush();
}

void DebugLog_Actual::Indent(int diff)
{
    auto unlock = Lock();
    indent += diff;
    if (indent < 0)
        indent = 0;
}

void InitLogs()
{
    mkdir("Logs");
    CreateEvent(NULL, FALSE, FALSE, "Teippi log multi-instance check");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        snprintf(log_path, sizeof log_path, "Logs/%lu", GetCurrentProcessId());
        mkdir(log_path);
    }
    else
        strncpy(log_path, "Logs", sizeof log_path);

    char buf[260];
    snprintf(buf, sizeof buf, "%s/teippi.txt", log_path);
    debug_log = new DebugLog(buf);
    snprintf(buf, sizeof buf, "%s/error.txt", log_path);
    error_log = new DebugLog_Actual(buf);
    if (PerfTest)
    {
        snprintf(buf, sizeof buf, "%s/performance.txt", log_path);
        perf_log = new PerfLog(buf);
    }
    if (SyncTest)
    {
        snprintf(buf, sizeof buf, "%s/sync.txt", log_path);
        sync_log = new SyncLog(buf);
        snprintf(buf, sizeof buf, "%s/dump", log_path);
        mkdir(buf);
        int i = 1;
        do {
            snprintf(buf, sizeof buf, "%s/dump/units_%d.txt", log_path, i * 1000);
            i++;
        } while (remove(buf) == 0);
        unit_dump = nullptr;
    }
}
