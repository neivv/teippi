#include "mainpatch.h"

#include "console/windows_wrap.h"
#include <time.h>

#include "offsets_hooks.h"
#include "offsets.h"
#include "patchmanager.h"
#include "memory.h"
#include "commands.h"
#include "unit.h"
#include "text.h"
#include "selection.h"
#include "order.h"
#include "limits.h"
#include "targeting.h"
#include "draw.h"
#include "scthread.h"
#include "log.h"
#include "unitlist.h"
#include "bullet.h"
#include "ai.h"
#include "triggers.h"
#include "scconsole.h"
#include "bullet.h"
#include "sprite.h"
#include "replay.h"
#include "yms.h"
#include "unit_cache.h"
#include "perfclock.h"
#include "common/log_freeze.h"

namespace bw
{
    namespace storm
    {
        intptr_t base_diff;
    }
}

void WindowCreatedPatch()
{
    #ifdef CONSOLE
    Common::console->HookWndProc(*bw::main_window_hwnd);
    #endif
}

void WinMainPatch()
{
    Common::PatchContext patch = patch_mgr->BeginPatch(0, bw::base::starcraft);
    PatchDraw(&patch);
    #ifdef CONSOLE
    PatchConsole();
    Common::console->HookTranslateAccelerator(&patch, bw::base::starcraft);
    #endif
}

uint32_t GetRngSeed()
{
    uint32_t seed;
    if (StaticSeed)
        seed = StaticSeed;
    else if (SyncTest)
        seed = 0;
    else
        seed = time(0);

    debug_log->Log("Rng seed %08x\n", seed);
    return seed;
}

uint32_t __stdcall VersionCheckGuard(uint8_t *a, void *b, void *c, void *d, void *e)
{
    patch_mgr->Unpatch();
    uint32_t ret = SNetInitializeProvider(a, b, c, d, e);
    patch_mgr->Repatch();
    return ret;
}

static void SavePanickedReplay()
{
    if (!IsInGame())
        return;
    // Add some frames (1 should be enough) to make crash surely reproductable
    bw::replay_header->replay_end_frame = *bw::frame_count + 50;
    SaveReplay("crash", 1);
}

static LPTOP_LEVEL_EXCEPTION_FILTER previous_exception_filter = nullptr;
static LONG WINAPI ExceptionFilter(EXCEPTION_POINTERS *info)
{
    LONG result = EXCEPTION_CONTINUE_SEARCH;
    if (previous_exception_filter)
        result = previous_exception_filter(info);

    SavePanickedReplay();
    // Terminate on debug because why not - release crashes properly so people won't get confused
    if (Debug && result != EXCEPTION_CONTINUE_EXECUTION)
        TerminateProcess(GetCurrentProcess(), info->ExceptionRecord->ExceptionCode);

    return result;
}

struct DetectFreeze
{
    DetectFreeze() : last_value(draw_counter.load(std::memory_order_acquire)) {}
    bool operator()()
    {
        if (!IsInGame())
            return false;
        uintptr_t new_value = draw_counter.load(std::memory_order_acquire);
        bool frozen = new_value == last_value;
        last_value = new_value;
        return frozen;
    }
    uintptr_t last_value;
};

static std::string GetExePath()
{
    int bufsize = 260;
    char static_buf[260];
    ptr<char[]> large_buf;
    char *buffer = static_buf;
    while (true)
    {
        auto result = GetModuleFileName(NULL, buffer, bufsize);
        if (result == 0)
            return "";
        else if (result < bufsize)
            return std::string(buffer);
        else if (bufsize > 2500)
            return "";

        bufsize *= 2;
        large_buf.reset(new char[bufsize]);
        buffer = large_buf.get();
    }
}

static std::string GetExeDirectory()
{
    auto path = GetExePath();
    return path.substr(0, path.find_last_of("\\/"));
}

static void InitFreezeLogging()
{
    HANDLE thread_handle;
    auto success = DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
            &thread_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
    if (success == 0)
    {
        char buf[64];
        auto error = GetLastError();
        snprintf(buf, sizeof buf, "DuplicateHandle: %x (%d)", error, error);
        // Show a noisy error instead of easily missed log line
        MessageBoxA(0, buf, "InitFreezeLogging failed", 0);
    }
    else
    {
        // Firegraft does something to the current working directory,
        // so use an absolute path
        std::string exe_dir = GetExeDirectory();
        if (exe_dir.empty())
        {
            char buf[64];
            // GetLastError might be wrong here as std::string does memory allocation.
            // Oh well.
            snprintf(buf, sizeof buf, "Could not get exe directory: %d", GetLastError());
            MessageBoxA(0, buf, "How is this possible?", 0);
        }
        std::string logfile = exe_dir + "/Errors/freeze.log";
        new FreezeLogger(thread_handle, logfile, DetectFreeze(), [](const std::string &msg) {
            MessageBoxA(0, "InitFreezeLogging failed", msg.c_str(), 0);
        });
    }
}

static void CreateIdentifyingEvent()
{
    char buf[32];
    auto pid = GetCurrentProcessId();
    snprintf(buf, sizeof buf, "Teippi #%d", pid);
    CreateEventA(NULL, FALSE, FALSE, buf);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        // Maybe should silently abort patching?
        MessageBoxA(0, "Second copy of Teippi has been loaded to same process.\n\
Crashing is very likely.", "Very important warning", 0);
    }
}

void InitialPatch()
{
    CreateIdentifyingEvent();
    InitLogs();
    InitSystemInfo();
    InitPerfClockFrequency();
    InitFreezeLogging();

    threads = new ThreadPool<ScThreadVars>;
    threads->Init(sysinfo.dwNumberOfProcessors * 2);
    int thread_count = threads->GetThreadCount();
    perf_log->Log("Thread amount: %d\n", thread_count);

    patch_mgr = new Common::PatchManager;
    Common::PatchContext patch = patch_mgr->BeginPatch(nullptr, bw::base::starcraft);

    patch.CallHook(bw::WinMain, WinMainPatch);
    patch.CallHook(bw::WindowCreated, WindowCreatedPatch);

    RemoveLimits(&patch);
    patch.Patch(bw::RngSeedPatch, 0, 9, PATCH_NOP);
    patch.Patch(bw::RngSeedPatch, (void *)&GetRngSeed, 0, PATCH_CALLHOOK);

    Common::PatchContext storm_patch = patch_mgr->BeginPatch("storm", bw::base::storm);
    bw::storm::base_diff = storm_patch.GetDiff();
    // #117 is SNetInitializeProvider
    storm_patch.Patch((void *)GetProcAddress(GetModuleHandle("storm"), (char *)117), (void *)&VersionCheckGuard, 0, PATCH_JMPHOOK);

    bullet_system = new BulletSystem;
    lone_sprites = new LoneSpriteSystem;
    enemy_unit_cache = new EnemyUnitCache;

    previous_exception_filter = SetUnhandledExceptionFilter(&ExceptionFilter);
}
