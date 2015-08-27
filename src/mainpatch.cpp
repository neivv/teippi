#include "mainpatch.h"

#include "console/windows_wrap.h"
#include <time.h>

#include "patchmanager.h"
#include "memory.h"
#include "offsets.h"
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
#include "building.h"
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
    bw::replay_header[0].replay_end_frame = *bw::frame_count + 50;
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
        new FreezeLogger(thread_handle, "Errors/freeze.log", DetectFreeze(), [](const std::string &msg) {
            MessageBoxA(0, "InitFreezeLogging failed", msg.c_str(), 0);
        });
    }
}

void InitialPatch()
{
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

    patch.Patch(bw::WinMain, (void *)&WinMainPatch, 0, PATCH_HOOKBEFORE | PATCH_CALLHOOK);
    patch.Patch(bw::WindowCreated, (void *)&WindowCreatedPatch, 0, PATCH_HOOKBEFORE | PATCH_SAFECALLHOOK);

    PatchProcessCommands(&patch);
    PatchSelection(&patch);
    PatchTargeting(&patch);
    RemoveLimits(&patch);
    patch.Patch(bw::RngSeedPatch, 0, 9, PATCH_NOP);
    patch.Patch(bw::RngSeedPatch, (void *)&GetRngSeed, 0, PATCH_CALLHOOK);
    patch.JumpHook(bw::UpdateBuildingPlacementState, UpdateBuildingPlacementState);

    Common::PatchContext storm_patch = patch_mgr->BeginPatch("storm", bw::base::storm);
    bw::storm::base_diff = storm_patch.GetDiff();
    // #117 is SNetInitializeProvider
    storm_patch.Patch((void *)GetProcAddress(GetModuleHandle("storm"), (char *)117), (void *)&VersionCheckGuard, 0, PATCH_JMPHOOK);

    if (UseConsole)
        patch.JumpHook(bw::GenerateFog, GenerateFog);

    bullet_system = new BulletSystem;
    lone_sprites = new LoneSpriteSystem;
    enemy_unit_cache = new EnemyUnitCache;

    previous_exception_filter = SetUnhandledExceptionFilter(&ExceptionFilter);
}
