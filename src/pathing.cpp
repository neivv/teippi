#include "pathing.h"

#include "offsets.h"
#include "console/assert.h"
#include "resolution.h"
#include "dialog.h"
#include "yms.h"
#include "unit.h"
#include "sprite.h"

#include <unordered_set>

#ifdef CONSOLE
#include "console/console.h"
#include "console/surface.h"
#endif

bool draw_region_borders = false;
bool draw_region_data = false;
bool draw_paths = false;

Pathing::PathingSystem *GetPathingSystem()
{
    return *bw::pathing;
}

int GetRegion(const Point &pos)
{
    using namespace Pathing;
    int region = (*bw::pathing)->map_tile_regions[pos.x / 32 + (pos.y / 32) * 256];
    if (region < 0x2000)
        return region;
    else
    {
        SplitRegion *split = &((*bw::pathing)->split_regions[region - 0x2000]);
        int minitile_x = (pos.x / 8) & 0x3;
        int minitile_y = (pos.y / 8) & 0x3;
        if (split->minitile_flags & 1 << (minitile_x + minitile_y * 4))
            return split->region_true;
        else
            return split->region_false;
    }
}

#ifdef SYNC
void *Path::operator new(size_t size)
{
    auto ret = new uint8_t[size];
    if (SyncTest)
        ScrambleStruct(ret, size);
    return ret;
}
#endif

Path::Path()
{
    dodge_unit = nullptr;
}

Path::~Path()
{
}

Path *AllocatePath(uint16_t *region_count, uint16_t *position_count)
{
    int used_space = ((*position_count * 2 + *region_count) * 2 + 0x23) & 0xfffc;
    if (used_space > 0x80)
    {
        if (*position_count >= 0x18)
        {
            *position_count = 0x17;
            *region_count = 1;
        }
        else
        {
            *region_count = (0x18 - *position_count) * 2;
        }
        used_space = ((*position_count * 2 + *region_count) * 2 + 0x23) & 0xfffc;
        if (used_space > 0x80)
            return 0;
    }
    return new Path;
}

void CreateSimplePath(Unit *unit, const Point &next_pos, const Point &end)
{
    if (unit->path)
    {
        // Delete path, allocate new...?
    }
    else
    {
        unit->path = new Path;
    }
    unit->path->total_region_count = 1;
    unit->path->unk1c = 1;
    unit->path->unk1d = 0;
    unit->path->position_count = 1;
    unit->path->position_index = 0;
    unit->path->start = unit->sprite->position;
    unit->path->end = end;
    unit->path->next_pos = next_pos;
    unit->path->values[0] = next_pos.x;
    unit->path->values[1] = next_pos.y;
    unit->path->flags = 0;
    unit->path->unk_count = 0;
    // Bw doesn't actually do this, causing weird pathing bugs
    // Dunno if this region is supposed to be end next_pos or what though
    unit->path->values[2] = GetRegion(next_pos);
}

#ifdef CONSOLE

void DrawRegionBorders(uint8_t *framebuf, int w, int h)
{
    Point pos(0, 0), screen_pos(*bw::screen_x, *bw::screen_y);
    Point add(7 - pos.x % 8, 7 - pos.y % 8);
    Point sub(7 - add.x, 7 - add.y);
    for (; pos.y < resolution::game_height; pos.y += 8)
    {
        for (; pos.x < resolution::game_width; pos.x += 8)
        {
            int reg = GetRegion(pos + screen_pos);
            int below_reg = GetRegion(pos + screen_pos + Point(0, 8));
            int right_reg = GetRegion(pos + screen_pos + Point(8, 0));
            if (reg != below_reg)
            {
                for (int i = 0; i < 8; i++)
                {
                    int x = pos.x - sub.x + i, y = pos.y + add.y;
                    if (x >= 0 && !IsOutsideGameScreen(x, y))
                        framebuf[y * w + x] = 0x6f;
                }
            }
            if (reg != right_reg)
            {
                for (int i = 0; i < 8; i++)
                {
                    int x = pos.x + add.x, y = pos.y - sub.y + i;
                    if (y >= 0 && !IsOutsideGameScreen(x, y))
                        framebuf[y * w + x] = 0x6f;
                }
            }
        }
        pos.x = 0;
    }
}

void DrawRegionData(uint8_t *framebuf, int w, int h)
{
    Point32 pos(0, 0), screen_pos(*bw::screen_x, *bw::screen_y);
    std::unordered_set<uint16_t> used_regions;
    Common::Surface surface(framebuf, w, h);

    for (; pos.y < (y32)resolution::game_height && pos.y + screen_pos.y < *bw::map_height; pos.y += 8)
    {
        for (; pos.x < (x32)resolution::game_width && pos.x + screen_pos.x < *bw::map_width; pos.x += 8)
        {
            Point32 region_pos = pos + screen_pos;
            uint16_t reg_id = GetRegion(region_pos.ToPoint16());
            if (used_regions.find(reg_id) == used_regions.end())
            {
                used_regions.insert(reg_id);
                Pathing::Region *region = (*bw::pathing)->regions + reg_id;
                Point32 center = Point32(region->x / 0x100, region->y / 0x100) - screen_pos;
                char buf[32];
                sprintf(buf, "%02x (%d)", reg_id, reg_id);
                surface.DrawText(Common::console->GetFont(), buf, center, 0x6f, [](int x, int y){ return !IsOutsideGameScreen(x, y); });
            }
        }
        pos.x = 0;
    }
}

static void DrawPaths(uint8_t *framebuf, int w, int h)
{
    Common::Surface surface(framebuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (Unit *unit : *bw::first_active_unit)
    {
        if (unit->path)
        {
            surface.DrawLine(Point32(unit->path->start) - screen_pos, Point32(unit->path->values[0], unit->path->values[1]) - screen_pos, 0x70,
                [](int x, int y){ return !IsOutsideGameScreen(x, y); });
            for (int i = 1; i < unit->path->position_count; i++)
            {
                surface.DrawLine(Point32(unit->path->values[(i - 1) * 2], unit->path->values[(i - 1) * 2 + 1]) - screen_pos,
                    Point32(unit->path->values[(i) * 2], unit->path->values[(i) * 2 + 1]) - screen_pos, 0x70,
                    [](int x, int y){ return !IsOutsideGameScreen(x, y); });
            }
            if (unit->path->dodge_unit)
            {
                surface.DrawLine(Point32(unit->path->dodge_unit->sprite->position) - screen_pos,
                        Point32(unit->sprite->position) - screen_pos, 0x6f,
                    [](int x, int y){ return !IsOutsideGameScreen(x, y); });
            }
        }
    }
    for (Unit *unit : *bw::first_active_unit)
    {
        if (unit->path)
        {
            surface.DrawLine(Point32(unit->sprite->position) - screen_pos, Point32(unit->path->next_pos) - screen_pos, 0x75, [](int x, int y){ return !IsOutsideGameScreen(x, y); });
        }
    }
}

void DrawPathingInfo(uint8_t *framebuf, xuint w, yuint h)
{
    if (!IsInGame())
        return;

    Assert(w >= resolution::game_width && h >= resolution::game_height);

    if (draw_region_borders)
    {
        DrawRegionBorders(framebuf, w, h);
        if (draw_region_data)
        {
            DrawRegionData(framebuf, w, h);
        }
    }
    if (draw_paths)
    {
        DrawPaths(framebuf, w, h);
    }
}

#endif
