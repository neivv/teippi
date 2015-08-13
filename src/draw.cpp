#include "draw.h"

#include "offsets.h"
#include "patchmanager.h"
#include "memory.h"
#include <vector>
#include "game.h"
#include "yms.h"
#include "log.h"
#include "perfclock.h"

std::atomic<uintptr_t> draw_counter;

class drawhook
{
    public:
        drawhook(void (*f)(uint8_t *, xuint, yuint), int p) { func = f; priority = p; }
        bool operator<(const drawhook &other) const { return priority < other.priority; }

        void (*func)(uint8_t *, xuint, yuint);
        int priority;
};
std::vector<drawhook> draw_hooks;

#include "console/windows_wrap.h"

typedef int (__stdcall SDrawLockSurface_Type)(int surface_id, Rect32 *a2, uint8_t **surface, int *width, int unused);
typedef int (__stdcall SDrawUnlockSurface_Type)(int surface_id, uint8_t *surface, int a3, int a4);
static SDrawLockSurface_Type *SDrawLockSurface_Orig;
static SDrawUnlockSurface_Type *SDrawUnlockSurface_Orig;

uint8_t fake_screenbuf[resolution::screen_width * resolution::screen_height];

// Koska sc ei redraw muualta kuin mistä on pakko, tähän copy sc:n screenbuf, mihin piirretään joka kerta custom muutokset
// (Jos piirtäs suoraan sc:n screenbuf, pitäs mark dirty jne)
uint8_t fake_screenbuf_2[resolution::screen_width * resolution::screen_height];


void DrawScreen()
{
    const auto relaxed = std::memory_order_relaxed;
    // Overflowing is fine
    draw_counter.store(draw_counter.load(relaxed) + 1, relaxed);

    // The load screen code likes to draw screen after every grp loaded,
    // and this function is a lot slower than the original one
    if (*bw::load_screen != nullptr)
    {
        static bool drawn_load_screen_once;
        if (drawn_load_screen_once)
            return;
        drawn_load_screen_once = true;
    }
    PerfClock clock;
    StaticPerfClock::Clear();
    Surface *game_screen = &*bw::game_screen;
    if (!game_screen->image)
        return;
    *bw::current_canvas = game_screen;
    if (*bw::no_draw)
    {
        memset(game_screen->image, 0, game_screen->w * game_screen->h);
        Rect32 area(0, 0, resolution::screen_width, resolution::screen_height);
        CopyToFrameBuffer(&area);
    }
    else
    {
        DrawParam param;
        for (int i = 7; i >= 0; i--)
        {
            DrawLayer *layer = &(bw::draw_layers[i]);
            if (!layer->draw)
                continue;
            if (!(layer->flags & 0x21))
            {
                if (ContainsDirtyArea(layer->area.left, layer->area.top, layer->area.left + layer->area.right, layer->area.top + layer->area.bottom))
                    layer->flags |= 0x4;
                else if (~layer->flags & 0x2)
                    continue;
            }
            param.area = Rect16(0 - layer->area.left, 0 - layer->area.top, -1 - layer->area.left + resolution::screen_width, -1 - layer->area.top + resolution::screen_height);
            param.w = resolution::screen_width;
            param.h = resolution::screen_height;
            (*layer->Draw)(0, 0, layer->func_param, &param);
            layer->flags &= ~0x7;
        }
    }

    if (*bw::trans_list && *bw::game_screen_redraw_trans)
    {
        // Äh..
        STransBind(*bw::game_screen_redraw_trans);
        STrans437(*bw::trans_list, &*bw::screen_redraw_tiles, 3, &*bw::game_screen_redraw_trans);
        CopyGameScreenToFramebuf();
        memset(bw::screen_redraw_tiles, 0, resolution::screen_width * resolution::screen_height / 16 / 16);
    }

    memcpy(fake_screenbuf_2, fake_screenbuf, resolution::screen_width * resolution::screen_height);
    for (drawhook &hook : draw_hooks)
    {
        (*hook.func)(fake_screenbuf_2, resolution::screen_width, resolution::screen_height);
    }
    uint8_t *surface;
    int width;
    if ((*SDrawLockSurface_Orig)(0, 0, &surface, &width, 0))
    {
        for (unsigned int  i = 0; i < resolution::screen_height; i++)
            memcpy(surface + i * width, fake_screenbuf_2 + i * resolution::screen_width, resolution::screen_width);
        (*SDrawUnlockSurface_Orig)(0, surface, 0, 0);
    }
//    if (!*bw::no_draw && *bw::draw_layers[0].draw)
//    {
//
//    }
    *bw::current_canvas = nullptr;
    auto time = clock.GetTime();
    if (!*bw::is_paused && time > 12.0)
    {
        perf_log->Log("DrawScreen %f ms\n", time);
        perf_log->Indent(2);
        while (auto clock = StaticPerfClock::PopNext())
        {
            perf_log->Log("%s: %d times, %f ms\n", clock->GetName(), clock->GetOldCount(), clock->GetOldTime());
        }
        perf_log->Indent(-2);
    }
}

int __stdcall SDrawLockSurface_Hook(int surface_id, Rect32 *a2, uint8_t **surface, int *width, int unused)
{
    if (surface_id == 0)
    {
        *surface = fake_screenbuf;
        *width = resolution::screen_width;
        return 1;
    }
    else
    {
        return (*SDrawLockSurface_Orig)(surface_id, a2, surface, width, unused);
    }
}

int __stdcall SDrawUnlockSurface_Hook(int surface_id, uint8_t *surface, int a3, int a4)
{
    if (surface == fake_screenbuf)
        return 1;

    return (*SDrawUnlockSurface_Orig)(surface_id, surface, a3, a4);
}

void GenerateFog()
{
    int screen_x = *bw::screen_pos_x_tiles;
    if (screen_x != 0)
        screen_x--;
    int screen_y = *bw::screen_pos_y_tiles;
    if (screen_y != 0)
        screen_y--;
    uint32_t *flags = (*bw::map_tile_flags) + screen_y * *bw::map_width_tiles + screen_x;
    uint32_t *orig_flags = flags;
    uint8_t *pos = *bw::fog_arr1;
    int shown_value = *bw::fog_variance_amount;
    int fow_value = shown_value / 2;

    int y_pos = screen_y;
    for (int i = 0; i < 0x11; i++)
    {
        int x_pos = *bw::screen_pos_x_tiles - 1;
        flags = orig_flags;
        for (int i = 0; i < 0x18; i++)
        {
            // Obviously people can just remove multiplayer check if they wish
            // Bw had nice vision-based sync but it does not work with dynamically allocated sprites
            if (all_visions && !IsMultiplayer())
            {
                if ((0xff00 & flags[0]) == 0xff00)
                    *pos = 0;
                else if ((0xff & flags[0]) == 0xff)
                    *pos = fow_value;
                else
                    *pos = shown_value;
            }
            else if (IsReplay())
            {
                if (*bw::replay_show_whole_map)
                    *pos = shown_value;
                else if (!((*bw::replay_visions << 8) & ~flags[0]))
                    *pos = 0;
                else if (!(*bw::replay_visions & ~flags[0]))
                    *pos = fow_value;
                else
                    *pos = shown_value;
            }
            else
            {
                if (*bw::player_exploration_visions & flags[0])
                    *pos = 0;
                else if (*bw::player_visions & flags[0])
                    *pos = fow_value;
                else
                    *pos = shown_value;
            }
            if (x_pos < *bw::map_width_tiles - 1 && x_pos >= 0)
                flags++;
            x_pos++;
            pos++;
        }
        if (y_pos < *bw::map_height_tiles - 1 && y_pos >= 0)
            orig_flags += *bw::map_width_tiles;
        y_pos++;
    }

    // Blend fog
    // Screen is 0x18 x 0x11 tiles
    // Well every border has 1 nonvisible tile, it is only used for blending?
    pos = *bw::fog_arr1 + 0x18 + 0x1;
    uint8_t *out = *bw::fog_arr2 + 0x18 + 0x1;
    for (int i = 0; i < 0x11 - 2; i++)
    {
        for (int i = 0; i < 0x18 - 2; i++)
        {
            int val = pos[0] * 2;
            val = (val + pos[-1] + pos[1] + pos[-0x18] + pos[0x18]) * 2;
            val = (val + pos[-0x17] + pos[0x17] + pos[-0x19] + pos[0x19]) / 16;
            *out = val;
            pos++;
            out++;
        }
        pos += 2;
        out += 2;
    }
}

void AddDrawHook(void (*func)(uint8_t *, xuint, yuint), int priority)
{
    drawhook hook(func, priority);
    auto it = lower_bound(draw_hooks.begin(), draw_hooks.end(), hook);
    draw_hooks.insert(it, hook);
}


void PatchDraw(Common::PatchContext *patch)
{
    HMODULE storm_dll = GetModuleHandle("storm.dll");
    SDrawLockSurface_Orig = (SDrawLockSurface_Type *)GetProcAddress(storm_dll, (const char *)350);
    SDrawUnlockSurface_Orig = (SDrawUnlockSurface_Type *)GetProcAddress(storm_dll, (const char *)356);

    patch->JumpHook(bw::SDrawLockSurface, SDrawLockSurface_Hook);
    patch->JumpHook(bw::SDrawUnlockSurface, SDrawUnlockSurface_Hook);

    patch->JumpHook(bw::DrawScreen, DrawScreen);
}
