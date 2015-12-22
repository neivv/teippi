#include "unitsearch.h"
#include "possearch.hpp"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <algorithm>
#include <utility>

#include "unit.h"
#include "order.h"
#include "sprite.h"
#include "offsets.h"
#include "pathing.h"
#include "yms.h"
#include "memory.h"
#include "perfclock.h"
#include "console/assert.h"
#include "limits.h"
#include "log.h"
#include "unit_cache.h"
#include "warn.h"

using std::rotate;
using std::max;
using std::min;
using std::get;

MainUnitSearch *unit_search = nullptr;

void MainUnitSearch::Validate()
{
    Assert(!valid_region_cache);
    Assert(!area_cache_enabled);
    Assert(std::is_sorted(left_positions.begin(), left_positions.end()));
    for (int i = 0; i < (int)Size(); i++)
    {
        Assert(left_to_value[i]->search_left == i);
    }
}

MainUnitSearch::MainUnitSearch()
{
    capacity = 0x400;
    // This has huge problem as it may be reallocated if used with recursive calls
    // Should use some kind of deque maybe
    result_units_beg = (Unit **)malloc((capacity + 1) * 4 * sizeof(Unit **)); // capacity + 1 for the empty terminator

    left_to_value.reserve(capacity);
    left_to_right.reserve(capacity);
    left_to_top.reserve(capacity);
    left_to_bottom.reserve(capacity);
    left_positions.reserve(capacity);
}

MainUnitSearch::~MainUnitSearch()
{
    free(result_units_beg);
}

// This gets called once pathing has been inited - units may have been already added
void MainUnitSearch::Init()
{
    UnitSearch::Init();
    region_cache.SetSize((*bw::pathing)->region_count);
    area_cache.SetSize(*bw::map_width, *bw::map_height);
    enemy_unit_cache->SetSize(*bw::map_width, *bw::map_height);
}

void UnitSearch::Init()
{
    max_width = *bw::unit_max_width;
}

void MainUnitSearch::Clear()
{
    PosSearch::Clear();
    left_low_invalid = INT_MAX;
    left_high_invalid = -1;
    valid_region_cache = false;
    area_cache_enabled = false;
}

void MainUnitSearch::Add(Unit *unit)
{
    STATIC_PERF_CLOCK(UnitSearch_Add);
    Validate();
    Rect16 box = unit->GetCollisionRect();
    Assert(box.left <= box.right && box.top <= box.bottom);
    // Bw assumes that subunits do not get added to the unit search, and will
    // not update unit search when subunits are moved around. This causes
    // issues with Teippi's unit search implementation and most likely
    // has no practical uses.
    if (units_dat_flags[unit->unit_id] & UnitFlags::Subunit)
    {
        auto str = unit->DebugStr();
        Warning("Unit %s is added to unit search, but is also marked as subunit (Building flag set?)",
                str.c_str());
        return;
    }

    unsigned int old_size = Size();
    if (capacity == old_size)
    {
        capacity *= 2;
        result_units_beg = (Unit **)malloc((capacity + 1) * 4 * sizeof(Unit **));
    }

    int pos = NewFind(box.left);
    for (unsigned i = pos; i < old_size; i++)
    {
        Unit *unit = left_to_value[i];
        unit->search_left = i + 1;
    }
    PosSearch::Add(pos, move(unit), box);

    unit->search_left = pos;

    area_cache_enabled = false;
    Validate();
}

void MainUnitSearch::PopResult()
{
    *bw::position_search_units_count = bw::position_search_results_offsets[--(*bw::position_search_results_count)];
}

Unit **MainUnitSearch::NewEntry()
{
    bw::position_search_results_offsets[*bw::position_search_results_count] = *bw::position_search_units_count;
    (*bw::position_search_results_count)++;
    return result_units_beg + *bw::position_search_units_count;
}

// Legacy function, returns units inside collision box extended by radius arguments,
// ignoring units which are completely inside collision box
Unit **MainUnitSearch::FindCollidingUnits(Unit *unit, int x_radius, int y_radius)
{
    Rect16 cbox = unit->GetCollisionRect();
    Rect16 area(max(0, cbox.left - x_radius), max(0, cbox.top - y_radius),
            min((int)*bw::map_width, cbox.right + x_radius), min((int)*bw::map_height, cbox.bottom + y_radius));

    int amt;
    Unit **units = FindUnitsRect(area, &amt);
    PopResult();
    Unit **ret = units;
    Unit **out_pos = units, **end = units + amt;
    while (units != end)
    {
        Unit *other = *units++;
        Rect16 other_rect = other->GetCollisionRect();
        // Not actually sure if should use <= >=, though I *think* bw just
        // possibly misses some equals while including ones later in internal order
        // (Who knows, it might cause some rarely seen pathing bug)
        if (cbox.top < other_rect.top && cbox.bottom > other_rect.bottom)
        {
            if (cbox.left < other_rect.left && cbox.right > other_rect.right)
                continue;
        }
        *out_pos++ = other;
    }

    *out_pos++ = nullptr;
    bw::position_search_results_offsets[*bw::position_search_results_count] = *bw::position_search_units_count;
    (*bw::position_search_results_count)++;
    *bw::position_search_units_count = out_pos - ret;
    return ret;
}

Unit **MainUnitSearch::FindUnitsRect(const Rect16 &rect, int *amount, Unit **out)
{
    Assert(rect.left <= rect.right && rect.top <= rect.bottom);
    STATIC_PERF_CLOCK(UnitSearch_FindUnitsRect);
    int tmp;
    if (!amount)
        amount = &tmp;
    Unit **ret = out;
    bool own_out = out;
    if (!out)
    {
        out = ret = NewEntry();
    }

    if (area_cache_enabled)
    {
        Rect16 clipped = rect.Clipped(MapBounds());
        Unit **end;
        AreaCacheBuf *bufs_end;
        AreaCacheBuf *bufs = reasonable_area_cache_buf;
        vector<AreaCacheBuf> free_dynamic;
        if (area_cache.AreaAmount(clipped) > sizeof reasonable_area_cache_buf / sizeof(AreaCacheBuf))
        {
            free_dynamic.resize(area_cache.AreaAmount(clipped));
            bufs = free_dynamic.data();
        }

        AreaCacheFind(clipped, ret, &end, bufs, &bufs_end);
        out = end;
        for (AreaCacheBuf *cache_buf_pos = bufs; cache_buf_pos != bufs_end; cache_buf_pos++)
        {
            cache_buf_pos->Iterate([&](Unit *unit, bool *stop)
            {
                *out++ = unit;
            });
        }
    }
    else
    {
        ret = out;
        UnitSearch::Find(rect, out, &out);
    }

    *amount = out - ret;
    *out++ = nullptr;
    if (!own_out)
        *bw::position_search_units_count = out - unit_search->result_units_beg;
    return ret;
}

void MainUnitSearch::AreaCacheFind(const Rect16 &rect, Unit **out, Unit ***out_end, UnitSearchAreaCache::AreaBuffer<Unit *> *out_bufs, UnitSearchAreaCache::AreaBuffer<Unit *> **out_bufs_end)
{
    STATIC_PERF_CLOCK(UnitSearch_AreaCacheFind);
    Rect16 area = area_cache.GetNonCachedArea(rect);
    if (area.IsValid())
    {
        Unit **end;
        UnitSearch::Find(area, out, &end);
        area_cache.FillCache(area, Array<Unit *>(out, end));
    }
    area_cache.Find(rect, out, out_end, out_bufs, out_bufs_end);
}

void MainUnitSearch::CacheArea(const Rect16 &rect)
{
    Rect16 area = area_cache.GetNonCachedArea(rect);
    if (area.IsValid())
    {
        Unit **out = NewEntry();
        Unit **end;
        UnitSearch::Find(area, out, &end);
        area_cache.FillCache(area, Array<Unit *>(out, end));
        PopResult();
    }
}

Unit **MainUnitSearch::FindUnitBordersRect(const Rect16 *rect)
{
    STATIC_PERF_CLOCK(UnitSearch_FindUnitBordersRect);
    Unit **out, **result_beg;
    out = result_beg = result_units_beg + *bw::position_search_units_count;
    bw::position_search_results_offsets[*bw::position_search_results_count] = *bw::position_search_units_count;
    (*bw::position_search_results_count)++;

    int beg = NewFind(rect->left - *bw::unit_max_width);
    int end = NewFind(rect->right);
    for (int it = beg; it < end; it++)
    {
        if (left_to_right[it] > rect->left)
        {
            if (rect->top < left_to_bottom[it] && rect->bottom > left_to_top[it])
            {
                // Stay true for the "borders" part just in case
                if (rect->top > left_to_top[it] && rect->bottom < left_to_bottom[it])
                    continue;
                *out++ = left_to_value[it];
            }
        }
    }

    *out++ = nullptr;
    *bw::position_search_units_count = out - result_units_beg;
    return result_beg;
}

int MainUnitSearch::DoUnitsCollide(const Unit *first, const Unit *second)
{
    // Can be happen at least in 004C7CF0, which hides and then moves unit
    // 004C7CF0 is used by TrigAction_MoveUnit
    if (first->search_left == -1 || second->search_left == -1)
        return 0;
    if (left_to_right[first->search_left] <= left_positions[second->search_left] ||
        left_positions[first->search_left] >= left_to_right[second->search_left] ||
        left_to_bottom[first->search_left] <= left_to_top[second->search_left] ||
        left_to_top[first->search_left] >= left_to_bottom[second->search_left])
        return 0;
    return 1;
}

// Find units that are in unit's collision rect modified by the following offsets, but ignore units that are completely inside the original
// collision rect or clip from the other side. Also, the first pixel of the collision rect is in the "include region"
// (Bw feature for some reason)
// However, the other unit must have its collision border in the search area
// In other, possibly better words, find units these two regions:
// Rect(collision_rect.ltrb + Point(x_off, y_off)) - Rect(collision_rect.l + 1, 0, collision_rect.r - 1, inf)
// (Where units have either their left or right borders), and
// Rect(collision_rect.ltrb + Point(x_off, y_off)) - Rect(0, collision_rect.t + 1, inf, collision_rect.b - 1)
// (Where units have either their top or bottom borders)
// If an offset is zero, not even the collision rect border pixels are included (Which makes me wonder if including them is unintended)
// But then again bw does extra work to make sure it works that way
// Duplicates are allowed. The unit itself must not be in results.
// Slow implementation, as this is just a compability burden
Unit **MainUnitSearch::CheckMovementCollision(Unit *unit, int x_off, int y_off)
{
    Unit **out, **result_beg;
    out = result_beg = result_units_beg + *bw::position_search_units_count;
    bw::position_search_results_offsets[*bw::position_search_results_count] = *bw::position_search_units_count;
    (*bw::position_search_results_count)++;
    if (unit->sprite->IsHidden() || (x_off == 0 && y_off == 0))
    {
        *out++ = nullptr;
        *bw::position_search_units_count = out - result_units_beg;
        return result_beg;
    }

    Rect32 rect;
    rect.left = max(0, left_positions[unit->search_left] + x_off);
    rect.top = max(0, left_to_top[unit->search_left] + y_off);
    rect.right = left_to_right[unit->search_left] + x_off;
    rect.bottom = left_to_bottom[unit->search_left] + y_off;

    Rect32 eka = rect, toka = rect;
    if (x_off > 0)
        eka.left = max((int)rect.left, left_to_right[unit->search_left] - 1);
    else if (x_off < 0)
        eka.right = min((int)rect.right, left_positions[unit->search_left] + 1);
    else
        eka.left = eka.right;
    if (y_off > 0)
        toka.top = max((int)rect.top, left_to_bottom[unit->search_left] - 1);
    else if (y_off < 0)
        toka.bottom = min((int)rect.bottom, left_to_top[unit->search_left] + 1);
    else
        toka.left = toka.right;

    eka.left = max((int)eka.left, 0);
    eka.top = max((int)eka.top, 0);
    toka.left = max((int)toka.left, 0);
    toka.top = max((int)toka.top, 0);

    if (eka.IsValid())
    {
        Unit **pos = out;
        int len = 0;
        FindUnitsRect(eka, &len, pos);
        Unit **end = pos + len;
        while (pos != end)
        {
            Unit *other = *pos++;
            if (unit != other)
            {
                Rect16 cbox = other->GetCollisionRect();
                if (cbox.left >= rect.left && cbox.left < rect.right)
                    *out++ = other;
                else if (cbox.right > rect.left && cbox.right <= rect.right)
                    *out++ = other;
            }
        }
    }
    if (toka.IsValid())
    {
        Unit **pos = out;
        int len = 0;
        FindUnitsRect(toka, &len, pos);
        Unit **end = pos + len;
        while (pos != end)
        {
            Unit *other = *pos++;
            if (unit != other)
            {
                Rect16 cbox = other->GetCollisionRect();
                if (cbox.top >= rect.top && cbox.top < rect.bottom)
                    *out++ = other;
                else if (cbox.bottom > rect.top && cbox.bottom <= rect.bottom)
                    *out++ = other;
            }
        }
    }
    *out++ = nullptr;
    *bw::position_search_units_count = out - result_units_beg;
    return result_beg;
}

void MainUnitSearch::ChangeUnitPosition(Unit *unit, int x_diff, int y_diff)
{
    if (unit->search_left == -1)
        return;

    Validate();
    left_positions[unit->search_left] += x_diff;
    left_to_right[unit->search_left] += x_diff;
    left_to_top[unit->search_left] += y_diff;
    left_to_bottom[unit->search_left] += y_diff;
    if (x_diff > 0)
    {
        int changed = left_positions[unit->search_left];
        unsigned int it;
        for (it = unit->search_left + 1; it < Size() && left_positions[it] < changed; it++)
        {
            Unit *unit = left_to_value[it];
            unit->search_left = it - 1;
            std::swap(left_positions[it - 1], left_positions[it]);
        }
        int pos = unit->search_left;
        rotate(left_to_value.begin() + pos, left_to_value.begin() + pos + 1, left_to_value.begin() + it);
        rotate(left_to_top.begin() + pos, left_to_top.begin() + pos + 1, left_to_top.begin() + it);
        rotate(left_to_bottom.begin() + pos, left_to_bottom.begin() + pos + 1, left_to_bottom.begin() + it);
        rotate(left_to_right.begin() + pos, left_to_right.begin() + pos + 1, left_to_right.begin() + it);
        unit->search_left = it - 1;
    }
    else if (x_diff < 0)
    {
        int changed = left_positions[unit->search_left];
        int it;
        for (it = unit->search_left - 1; it >= 0 && left_positions[it] > changed; it--)
        {
            Unit *unit = left_to_value[it];
            unit->search_left = it + 1;
            std::swap(left_positions[it + 1], left_positions[it]);
        }
        int pos = unit->search_left;
        rotate(left_to_value.begin() + it + 1, left_to_value.begin() + pos, left_to_value.begin() + pos + 1);
        rotate(left_to_top.begin() + it + 1, left_to_top.begin() + pos, left_to_top.begin() + pos + 1);
        rotate(left_to_bottom.begin() + it + 1, left_to_bottom.begin() + pos, left_to_bottom.begin() + pos + 1);
        rotate(left_to_right.begin() + it + 1, left_to_right.begin() + pos, left_to_right.begin() + pos + 1);
        unit->search_left = it + 1;
    }
    area_cache_enabled = false;
    Validate();
}

void MainUnitSearch::ChangeUnitPosition_Fast(Unit *unit, int x_diff, int y_diff)
{
    if (x_diff < 0)
    {
        left_low_invalid = std::min((int)left_low_invalid, (int)left_positions[unit->search_left] + x_diff);
        left_high_invalid = std::max((int)left_high_invalid, (int)left_positions[unit->search_left]);
    }
    else if (x_diff > 0)
    {
        left_low_invalid = std::min((int)left_low_invalid, (int)left_positions[unit->search_left]);
        left_high_invalid = std::max((int)left_high_invalid, (int)left_positions[unit->search_left] + x_diff);
    }
    if (x_diff != 0)
        Assert(left_low_invalid >= 0 && left_high_invalid >= 0);
    left_positions[unit->search_left] += x_diff;
    left_to_right[unit->search_left] += x_diff;
    left_to_top[unit->search_left] += y_diff;
    left_to_bottom[unit->search_left] += y_diff;
}

class SortHelperBase
{
    public:
        typedef std::ptrdiff_t diff;
        template <class Parent>
        class ValueRef
        {
            public:
                ValueRef(Parent *parent_, int pos_) : parent(parent_), pos(pos_) {}
                bool operator<(const ValueRef &other) const { return parent->beg[pos] < parent->beg[other.pos]; }
                bool operator<(const typename Parent::Value &other) const { return parent->beg[pos] < other.val; }
                //operator typename Parent::Value() const { return typename Parent::Value(parent, pos); }
                ValueRef &operator=(const typename Parent::Value &val) { parent->SetVal(pos, val); return *this; }
                ValueRef &operator=(const ValueRef &other) { parent->SetVal(pos, other); return *this; }
                Parent * const parent;
                const int pos;
        };

        SortHelperBase(x32 *pos_, x32 * const beg_) : pos(pos_), beg(beg_) {}

        bool operator==(const SortHelperBase &other) const { return pos == other.pos; }
        bool operator!=(const SortHelperBase &other) const { return !(*this == other); }
        bool operator<(const SortHelperBase &other) const { return pos < other.pos; }
        bool operator>=(const SortHelperBase &other) const { return !(*this < other); }
        bool operator>(const SortHelperBase &other) const { return pos > other.pos; }
        bool operator<=(const SortHelperBase &other) const { return !(*this > other); }

        x32 *pos;
        x32 * const beg;
};

class LeftSortHelper : public SortHelperBase
{
    public:
        struct Value
        {
            Value(ValueRef<LeftSortHelper> &&other)
            {
                *this = std::move(other);
            }
            Value &operator=(ValueRef<LeftSortHelper> &&other)
            {
                auto *parent = other.parent;
                val = parent->beg[other.pos];
                unit = parent->unit[other.pos];
                top = parent->top[other.pos];
                bottom = parent->bottom[other.pos];
                right = parent->right[other.pos];
                return *this;
            }
            bool operator<(const ValueRef<LeftSortHelper> &other) const { return val < other.parent->beg[other.pos]; }
            x32 val;
            Unit *unit;
            y32 top;
            y32 bottom;
            x32 right;
        };
        void SetVal(int pos, const Value &val)
        {
            beg[pos] = val.val;
            unit[pos] = val.unit;
            top[pos] = val.top;
            bottom[pos] = val.bottom;
            right[pos] = val.right;
        }
        void SetVal(int pos, const ValueRef<LeftSortHelper> &val)
        {
            beg[pos] = beg[val.pos];
            unit[pos] = unit[val.pos];
            top[pos] = top[val.pos];
            bottom[pos] = bottom[val.pos];
            right[pos] = right[val.pos];
        }
        LeftSortHelper(MainUnitSearch *parent, x32 *pos_) : SortHelperBase(pos_, parent->left_positions.data()),
            unit(parent->left_to_value.data()), top(parent->left_to_top.data()), bottom(parent->left_to_bottom.data()),
            right(parent->left_to_right.data())
            { }
        LeftSortHelper &operator=(const LeftSortHelper &other) { pos = other.pos; return *this; }

        ValueRef<LeftSortHelper> operator*() { return ValueRef<LeftSortHelper>(this, pos - beg); }
        LeftSortHelper operator-(diff val) const { LeftSortHelper ret(*this); ret -= val; return ret; }
        LeftSortHelper operator+(diff val) const { LeftSortHelper ret(*this); ret += val; return ret; }
        LeftSortHelper &operator+=(diff val) { pos += val; return *this; }
        LeftSortHelper &operator-=(diff val) { pos -= val; return *this; }
        LeftSortHelper &operator++() { return *this += 1; }
        LeftSortHelper operator++(int) { LeftSortHelper copy(*this); *this += 1; return copy; }
        LeftSortHelper &operator--() { return *this -= 1; }
        LeftSortHelper operator--(int) { LeftSortHelper copy(*this); *this -= 1; return copy; }
        diff operator-(const LeftSortHelper &other) const { return pos - other.pos; }
        Unit ** const unit;
        y32 * const top;
        y32 * const bottom;
        x32 * const right;
};

void swap(SortHelperBase::ValueRef<LeftSortHelper> a, SortHelperBase::ValueRef<LeftSortHelper> b)
{
    using std::swap;
    LeftSortHelper *parent = a.parent;
    swap(parent->unit[a.pos], parent->unit[b.pos]);
    swap(parent->top[a.pos], parent->top[b.pos]);
    swap(parent->bottom[a.pos], parent->bottom[b.pos]);
    swap(parent->right[a.pos], parent->right[b.pos]);
    swap(parent->beg[a.pos], parent->beg[b.pos]);
}

namespace std {
template <>
struct iterator_traits<LeftSortHelper>
{
    typedef std::ptrdiff_t difference_type;
    typedef SortHelperBase::ValueRef<LeftSortHelper> reference;
    typedef LeftSortHelper::Value value_type;
    typedef std::random_access_iterator_tag iterator_category;
};
}

void MainUnitSearch::ChangeUnitPosition_Finish()
{
    if (left_high_invalid != -1)
    {
        int low = NewFind(left_low_invalid), high = NewFind(left_high_invalid + 1);
        std::sort(LeftSortHelper(this, left_positions.data() + low), LeftSortHelper(this, left_positions.data() + high));
        for (int i = low; i < high; i++)
        {
            Unit *unit = left_to_value[i];
            unit->search_left = i;
        }

        left_low_invalid = INT_MAX;
        left_high_invalid = -1;
    }
    area_cache_enabled = false;
    Validate();
}

Unit *MainUnitSearch::FindNearestUnit(Unit *self, const Point &pos, int (__fastcall *IsValid)(const Unit *, void *), void *func_param, const Rect16 &area)
{
    return UnitSearch::FindNearest(pos, area, [self, IsValid, func_param](const Unit *unit) -> bool {
        return self != unit && IsValid(unit, func_param);
    });
}

// Searches Rect(pd->area.left - pd->unit_size.top - 1, pd->area.top
void MainUnitSearch::GetNearbyBlockingUnits(PathingData *pd)
{
    Contour *wall_it = pd->top_walls + pd->wall_counts[Pathing::top];
    // Recheck would be nice, area.top might be actually one less here (same with area.left at the last loop)
    Rect16 area(max(0, pd->area.left - pd->unit_size[Pathing::left]), max(0, pd->area.top - pd->unit_size[Pathing::top] - 1),
            pd->area.right - pd->unit_size[Pathing::right] + 1, pd->unit_pos[1] - pd->unit_size[Pathing::top] + 1);
    ForEachUnitInArea(area, [&](Unit *other)
    {
        Rect16 cbox = other->GetCollisionRect();
        if (cbox.bottom <= area.bottom)
        {
            if (other == pd->always_dodge || HasToDodge(pd->self, other))
            {
                Contour *wall_begin = pd->top_walls;
                Contour wall = { { uint16_t(cbox.bottom - 1), cbox.left, uint16_t(cbox.right - 1)}, Pathing::top, 0x3d };
                InsertContour(&wall, pd, &wall_begin, &wall_it);
            }
        }
        return false;
    });
    pd->wall_counts[Pathing::top] = wall_it - pd->top_walls;

    wall_it = pd->right_walls + pd->wall_counts[Pathing::right];
    area = Rect16(max(0, pd->unit_pos[0] - pd->unit_size[Pathing::right]), max(0, pd->area.top - pd->unit_size[Pathing::top]),
            pd->area.right - pd->unit_size[Pathing::right] + 2, pd->area.bottom - pd->unit_size[Pathing::bottom] + 1);
    ForEachUnitInArea(area, [&](Unit *other)
    {
        Rect16 cbox = other->GetCollisionRect();
        if (cbox.left >= area.left)
        {
            if (other == pd->always_dodge || HasToDodge(pd->self, other))
            {
                Contour *wall_begin = pd->right_walls;
                Contour wall = { { cbox.left, cbox.top, uint16_t(cbox.bottom - 1)}, Pathing::right, 0x32 };
                InsertContour(&wall, pd, &wall_begin, &wall_it);
            }
        }
        return false;
    });
    pd->wall_counts[Pathing::right] = wall_it - pd->right_walls;

    area = Rect16(max(0, pd->area.left - pd->unit_size[Pathing::left]), max(0, pd->unit_pos[1] - pd->unit_size[Pathing::bottom]),
            pd->area.right - pd->unit_size[Pathing::right] + 1, pd->area.bottom - pd->unit_size[Pathing::bottom] + 2);
    wall_it = pd->bottom_walls + pd->wall_counts[Pathing::bottom];
    ForEachUnitInArea(area, [&](Unit *other)
    {
        Rect16 cbox = other->GetCollisionRect();
        if (cbox.top >= area.top)
        {
            if (other == pd->always_dodge || HasToDodge(pd->self, other))
            {
                Contour *wall_begin = pd->bottom_walls;
                Contour wall = { { cbox.top, cbox.left, uint16_t(cbox.right - 1)}, Pathing::bottom, 0x3d };
                InsertContour(&wall, pd, &wall_begin, &wall_it);
            }
        }
        return false;
    });
    pd->wall_counts[Pathing::bottom] = wall_it - pd->bottom_walls;

    wall_it = pd->left_walls + pd->wall_counts[Pathing::left];
    area = Rect16(max(0, pd->area.left - pd->unit_size[Pathing::left] - 1), max(0, pd->area.top - pd->unit_size[Pathing::top]),
            pd->unit_pos[0] - pd->unit_size[Pathing::left] + 1, pd->area.bottom - pd->unit_size[Pathing::bottom] + 1);
    ForEachUnitInArea(area, [&](Unit *other)
    {
        Rect16 cbox = other->GetCollisionRect();
        if (cbox.right <= area.right)
        {
            if (other == pd->always_dodge || HasToDodge(pd->self, other))
            {
                Contour *wall_begin = pd->left_walls;
                Contour wall = { { uint16_t(cbox.right - 1), cbox.top, uint16_t(cbox.bottom - 1)}, Pathing::left, 0x32 };
                InsertContour(&wall, pd, &wall_begin, &wall_it);
            }
        }
        return false;
    });
    pd->wall_counts[Pathing::left] = wall_it - pd->left_walls;
}

void MainUnitSearch::Remove(Unit *unit)
{
    Validate();
    unsigned old_size = Size();

    for (unsigned i = unit->search_left + 1; i < old_size; i++)
    {
        Unit *unit = left_to_value[i];
        unit->search_left = i - 1;
    }

    RemoveAt(unit->search_left);
    unit->search_left = -1;

    area_cache_enabled = false;
    Validate();
}

int MainUnitSearch::GetDodgingDirection(const Unit *self, const Unit *other)
{
    if (other->sprite->IsHidden())
        return -1;

    int direction = GetFacingDirection(self->sprite->position.x, self->sprite->position.y, self->next_move_waypoint.x, self->next_move_waypoint.y);
    int quarter = direction / 0x40;
    int change = (direction - self->new_direction) & 0xff;
    if (change > 0x80)
        change = 0x100 - change;
    if (change >= 0x50)
        return -1;

    int other_location = GetOthersLocation(self, other);
    Rect16 other_cbox = other->GetCollisionRect();
    Rect32 dodge_area;
    // Right and bottom might need one higher values??
    dodge_area.left = other_cbox.left - units_dat_dimensionbox[self->unit_id].right - 1;
    dodge_area.right = other_cbox.right + units_dat_dimensionbox[self->unit_id].left;
    dodge_area.top = other_cbox.top - units_dat_dimensionbox[self->unit_id].bottom - 1;
    dodge_area.bottom = other_cbox.bottom + units_dat_dimensionbox[self->unit_id].top;

    uint32_t actual_position;
    uint16_t *position = (uint16_t *)&actual_position;
    switch (other_location)
    {
        case Pathing::top:
            position[1] = dodge_area.bottom;
            if (quarter == 0)
                direction = 0x40;
            else if (quarter == 3)
                direction = 0xc0;
            else
                return -1;
        break;
        case Pathing::right:
            position[0] = dodge_area.left;
            if (quarter == 0)
                direction = 0;
            else if (quarter == 1)
                direction = 0x80;
            else
                return -1;
        break;
        case Pathing::bottom:
            position[1] = dodge_area.top;
            if (quarter == 1)
                direction = 0x40;
            else if (quarter == 2)
                direction = 0xc0;
            else
                return -1;
        break;
        case Pathing::left:
            position[0] = dodge_area.right;
            if (quarter == 2)
                direction = 0x80;
            else if (quarter == 3)
                direction = 0;
            else
                return -1;
        break;
        default:
            return -1;
    }
    switch (direction / 0x40)
    {
        case 0:
            position[1] = dodge_area.top;
            if (DoUnitsBlock(self, actual_position) == 0)
                return 0;
            position[1] = dodge_area.bottom;
            if (DoUnitsBlock(self, actual_position) == 0)
                return 0x80;
            return -1;
        break;
        case 1:
            position[0] = dodge_area.right;
            if (DoUnitsBlock(self, actual_position) == 0)
                return 0x40;
            position[0] = dodge_area.left;
            if (DoUnitsBlock(self, actual_position) == 0)
                return 0xc0;
            return -1;
        break;
        case 2:
            position[1] = dodge_area.bottom;
            if (DoUnitsBlock(self, actual_position) == 0)
                return 0x80;
            position[1] = dodge_area.top;
            if (DoUnitsBlock(self, actual_position) == 0)
                return 0;
            return -1;
        break;
        case 3:
            position[0] = dodge_area.left;
            if (DoUnitsBlock(self, actual_position) == 0)
                return 0xc0;
            position[0] = dodge_area.right;
            if (DoUnitsBlock(self, actual_position) == 0)
                return 0x40;
            return -1;
        break;
    }
    return -1;
}

bool UnitSearch::DoesBlockArea(const Unit *unit, const CollisionArea *area) const
{
    if (~unit->pathing_flags & 0x1 || unit->flags & UnitStatus::NoCollision)
        return false;
    if (area->self && (unit == area->self || !area->self->CanCollideWith(unit)))
        return false;
    Rect16 cbox = unit->GetCollisionRect();
    if (cbox.right <= area->area.left && cbox.left >= area->area.right)
        return false;
    if (cbox.bottom <= area->area.top && cbox.top >= area->area.bottom)
        return false;
    return true;
}

void MainUnitSearch::ClearRegionCache()
{
    region_cache.Clear();
    valid_region_cache = true;
}

void MainUnitSearch::EnableAreaCache()
{
    if (area_cache_enabled)
        return;
    area_cache.Clear();
    area_cache_enabled = true;
}

void MainUnitSearch::DisableAreaCache()
{
    // No need to clear
    area_cache_enabled = false;
}

class ChooseTargetSort
{
    public:
        struct Value;
        struct ValueRef;

        static bool Compare(Unit *a, Unit *b, uint32_t a_str, uint32_t b_str) {
            if (a->player == b->player)
            {
                if (a_str == b_str)
                    return a->lookup_id < b->lookup_id;
                else
                    return a_str > b_str;
            }
            else
                return a->player < b->player;
        }
        typedef std::ptrdiff_t diff;
        struct ValueRef
        {
            ValueRef(ChooseTargetSort *parent_, int pos_) : parent(parent_), pos(pos_) {}
            bool operator<(const ValueRef &other) const {
                return ChooseTargetSort::Compare(parent->units[pos], parent->units[other.pos],
                        parent->strength[pos], parent->strength[other.pos]);
            }
            bool operator<(const Value &other) const {
                return ChooseTargetSort::Compare(parent->units[pos], other.unit,
                        parent->strength[pos], other.strength);
            }
            ValueRef &operator=(const Value &val) { parent->SetVal(pos, val); return *this; }
            ValueRef &operator=(const ValueRef &other) { parent->SetVal(pos, other); return *this; }
            ChooseTargetSort * const parent;
            const int pos;
        };
        struct Value
        {
            Value(ValueRef &&other)
            {
                *this = std::move(other);
            }
            Value &operator=(ValueRef &&other)
            {
                auto *parent = other.parent;
                unit = parent->units[other.pos];
                strength = parent->strength[other.pos];
                return *this;
            }
            bool operator<(const ValueRef &other) const {
                return ChooseTargetSort::Compare(unit, other.parent->units[other.pos],
                        strength, other.parent->strength[other.pos]);
            }
            Unit *unit;
            uint32_t strength;
        };

        bool operator==(const ChooseTargetSort &other) const { return pos == other.pos; }
        bool operator!=(const ChooseTargetSort &other) const { return !(*this == other); }
        bool operator<(const ChooseTargetSort &other) const { return pos < other.pos; }
        bool operator>=(const ChooseTargetSort &other) const { return !(*this < other); }
        bool operator>(const ChooseTargetSort &other) const { return pos > other.pos; }
        bool operator<=(const ChooseTargetSort &other) const { return !(*this > other); }

        void SetVal(int pos, const Value &val)
        {
            units[pos] = val.unit;
            strength[pos] = val.strength;
        }
        void SetVal(int pos, const ValueRef &val)
        {
            units[pos] = units[val.pos];
            strength[pos] = strength[val.pos];
        }
        ChooseTargetSort(Unit **base, Unit **u, uint32_t *str_base) : pos(u), units(base), strength(str_base) {}
        ChooseTargetSort &operator=(const ChooseTargetSort &other) { pos = other.pos; return *this; }

        ValueRef operator*() { return ValueRef(this, pos - units); }
        ChooseTargetSort operator-(diff val) const { ChooseTargetSort ret(*this); ret -= val; return ret; }
        ChooseTargetSort operator+(diff val) const { ChooseTargetSort ret(*this); ret += val; return ret; }
        ChooseTargetSort &operator+=(diff val) { pos += val; return *this; }
        ChooseTargetSort &operator-=(diff val) { pos -= val; return *this; }
        ChooseTargetSort &operator++() { return *this += 1; }
        ChooseTargetSort operator++(int) { ChooseTargetSort copy(*this); *this += 1; return copy; }
        ChooseTargetSort &operator--() { return *this -= 1; }
        ChooseTargetSort operator--(int) { ChooseTargetSort copy(*this); *this -= 1; return copy; }
        diff operator-(const ChooseTargetSort &other) const { return pos - other.pos; }
        Unit **pos;
        Unit ** const units;
        uint32_t * const strength;
};

void swap(ChooseTargetSort::ValueRef a, ChooseTargetSort::ValueRef b)
{
    using std::swap;
    ChooseTargetSort *parent = a.parent;
    swap(parent->units[a.pos], parent->units[b.pos]);
    swap(parent->strength[a.pos], parent->strength[b.pos]);
}

namespace std {
template <>
struct iterator_traits<ChooseTargetSort>
{
    typedef std::ptrdiff_t difference_type;
    typedef ChooseTargetSort::ValueRef reference;
    typedef ChooseTargetSort::Value value_type;
    typedef std::random_access_iterator_tag iterator_category;
};
}

// Result is sorted by GetCurrentStrength(ground), so ChooseTarget can stop as soon as it finds
// acceptable unit
// Returns one array for each player
UnitSearchRegionCache::Entry MainUnitSearch::FindUnits_ChooseTarget(int region_id, bool ground)
{
    if (!valid_region_cache)
    {
        ClearRegionCache();
    }
    else
    {
        auto entry = region_cache.Find(region_id, ground);
        if (entry)
            return entry.take();
        entry = region_cache.Find(region_id, !ground);
        if (entry)
        {
            auto size = entry.take().Size();
            Unit **copy = region_cache.NewEntry(size);
            memcpy(copy, entry.take().GetRaw(), size * sizeof(Unit *));
            auto str_beg = make_unique<uint32_t[]>(size);
            auto str_end = str_beg.get() + size;
            for (auto str_out = str_beg.get(); str_out != str_end; str_out++)
            {
                auto i = str_out - str_beg.get();
                *str_out = GetCurrentStrength(copy[i], ground);
            }
            std::sort(ChooseTargetSort(copy, copy, str_beg.get()), ChooseTargetSort(copy, copy + size, str_beg.get()));
            return region_cache.FinishEntry(copy, region_id, ground, size);
        }
    }
    STATIC_PERF_CLOCK(UnitSearch_FindUnits_ChooseTarget);

    // Based on FindUnitBordersRect
    Pathing::Region *region = (*bw::pathing)->regions + region_id;
    Rect16 rect(Point(region->x >> 8, region->y >> 8), 0x120);
    Unit **out, **result_beg;

    out = result_beg = region_cache.NewEntry(Size() + 1);
    // Allocate a separate array to hold GetCurrentStrength as calling it for every comparision is slow
    // (first field is entry to out[], second is the strength)
    auto str_beg = make_unique<uintptr_t[]>(Size() + 1);
    uintptr_t *str_out = str_beg.get();
    unsigned int beg, end;
    beg = NewFind(rect.left - *bw::unit_max_width);
    end = NewFind(rect.right);

    for (unsigned int it = beg; it < end; it++)
    {
        if (left_to_right[it] > rect.left)
        {
            if (rect.top < left_to_bottom[it] && rect.bottom > left_to_top[it])
            {
                Unit *unit = left_to_value[it];
                if (unit->order != Order::Die && !unit->IsInvincible())
                {
                    // FindUnitBordersRect does this to be truly "borders only",
                    // just in case some weird bw function depends on it,
                    // but I doubt it is necessary here.
                    // if (rect->top > left_to_top[it] && rect->bottom < left_to_bottom[it])
                    //     continue;
                    if (~unit->flags & UnitStatus::Hallucination || unit->GetHealth() == unit->GetMaxHealth())
                    {
                        *out++ = unit;
                        *str_out = GetCurrentStrength(unit, ground);
                        str_out++;
                    }
                }
            }
        }
    }

    STATIC_PERF_CLOCK(UnitSearch_FindUnits_CT_sort);
    std::sort(ChooseTargetSort(result_beg, result_beg, str_beg.get()), ChooseTargetSort(result_beg, out, str_beg.get()));
    return region_cache.FinishEntry(result_beg, region_id, ground, out - result_beg);
}

Unit **MainUnitSearch::FindHelpingUnits(Unit *own, const Rect16 &rect, TempMemoryPool *allocation_pool)
{
    Unit **out, **result_beg = allocation_pool->Allocate<Unit *>(Size() + 1);
    out = result_beg;

    unsigned int beg, end;
    beg = NewFind(rect.left - max_width);
    end = NewFind(rect.right);

    int count = 0;
    for (unsigned int it = beg; it < end; it++)
    {
        if (left_to_right[it] > rect.left)
        {
            if (rect.top < left_to_bottom[it] && rect.bottom > left_to_top[it])
            {
                Unit *unit = left_to_value[it];
                if (unit->player != own->player)
                    continue;
                // Non-ai danimoths are skipped as well, but ai ones aren't
                if (unit->unit_id == Unit::Arbiter)
                    continue;
                // Workers are searched later if needed, as they are likely a rare case.
                // (And that is only for ai units)
                if (unit->IsWorker())
                    continue;
                if (unit == own)
                    continue;
                *out++ = unit;
                count++;
            }
        }
    }
    *out++ = nullptr;
    allocation_pool->SetPos(out);
    return result_beg;
}
