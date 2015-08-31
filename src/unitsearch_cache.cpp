#include "unitsearch_cache.h"

#include "console/windows_wrap.h"
#include "memory.h"
#include "offsets.h"
#include "unit.h"
#include "sprite.h"
#include "perfclock.h"
#include "log.h"
#include "limits.h"

#include <algorithm>

#include "unitsearch_cache.hpp"

using std::min;
using std::max;

// Global area that can be shared by all empty areas with MarkAreaEmpty
// Using nullptrs set in constructor should be fine? Count() == nullptr - nullptr == 0
UnitSearchAreaCache::Area<Unit *> UnitSearchAreaCache::empty_area;

UnitSearchRegionCache::UnitSearchRegionCache()
{
    memory.resize(1);
}

UnitSearchRegionCache::~UnitSearchRegionCache()
{
}

void UnitSearchRegionCache::SetSize(uint32_t size_)
{
    size = size_ * 2; // Sorted by both air and ground strength
    cache.resize(size);
    cache.shrink_to_fit();
}

uintptr_t UnitSearchRegionCache::MakeEntry(uintptr_t region, bool ground)
{
    if (ground)
        return region;
    return size - region - 1;
}

Optional<UnitSearchRegionCache::Entry> UnitSearchRegionCache::Find(uint32_t region, bool ground)
{
    uint8_t *ptr = cache[MakeEntry(region, ground)];
    if (!ptr)
        return Optional<Entry>();
    return move(Entry(ptr, Limits::Players));
}

void MemAllocator::Reset()
{
    for (MemArea &area : memory_areas)
    {
        area.pos = area.beg;
    }
}

void UnitSearchRegionCache::Clear()
{
    memory[0].Reset();
    fill(cache.begin(), cache.end(), nullptr);
}

uint8_t *MemAllocator::MemAlloc(unsigned int max_size)
{
    for (MemArea &area : memory_areas)
    {
        if ((uintptr_t)(area.end - area.pos) >= max_size)
        {
            current_area = &area;
            return area.pos;
        }
    }
    /*for (MemArea &area : memory_areas)
    {
        int size = ((max_size - (area.end - area.pos)) - 1) / sysinfo.dwPageSize + 1;
        size *= sysinfo.dwPageSize;
        void *new_mem = VirtualAlloc(area.end, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (new_mem)
        {
            current_area = &area;
            area.end = area.end + size;
            return area.pos;
        }
    }*/
    MemArea *area = NewMemArea(max_size);
    current_area = area;
    return area->pos;
}

Unit **UnitSearchRegionCache::NewEntry(uintptr_t max_size)
{
    // We reserve 12 words for first player units
    // and 13th word for end pointer
    auto ptr = (Unit **)memory[0].MemAlloc(((max_size + 1 + Limits::Players) * sizeof(Unit **)));
    return ptr + Limits::Players + 1;
}

MemArea *MemAllocator::NewMemArea(uintptr_t min_size)
{
    // So ideally areas would be at least 7/8 used before allocating a new one
    if (sysinfo.dwAllocationGranularity < min_size * 8)
        min_size *= 8;
    int size = ((min_size - 1) / sysinfo.dwAllocationGranularity + 1);
    size *= sysinfo.dwAllocationGranularity;

    uint8_t *memory = (uint8_t *)VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    Assert(memory != nullptr);
    memory_areas.emplace_back(memory, memory + size);
    return &*(memory_areas.end() - 1);
}

UnitSearchRegionCache::Entry UnitSearchRegionCache::FinishEntry(Unit **arr, uint32_t entry, bool ground, uint32_t count)
{
    uint8_t *raw = ((uint8_t *)arr) - Limits::Players * sizeof(Unit *) - sizeof(Unit *);
    Unit ***player_units = (Unit ***)raw;
    for (int i = 0; i < Limits::Players; i++)
    {
        player_units[i] = std::lower_bound(arr, arr + count, i, [](const Unit *a, int b) { return a->player < b; });
    }
    player_units[Limits::Players] = arr + count; // End pointer
    cache[MakeEntry(entry, ground)] = raw;
    memory[0].EndAlloc((uint8_t *)(arr + count));
    return Entry(raw, Limits::Players);
}

template <typename T>
unsigned Log2(T value)
{
    unsigned result = 0;
    while (value >>= 1) ++result;
    return result;
}

void UnitSearchAreaCache::SetSize(xuint x, yuint y)
{
    x = (x - 1) / AreaSize + 1;
    y = (y - 1) / AreaSize + 1;
    width_shift = Log2(x - 1) + 1;
    x = 1 << width_shift;
    cache_size = x * y;
    cache.resize(cache_size);
    memory.resize(y);
    Clear();
}

void UnitSearchAreaCache::Clear()
{
    for (auto &allocator : memory)
    {
        allocator.Reset();
    }
    fill(cache.begin(), cache.end(), nullptr);
    for_each_buf.clear();
}

Rect16 UnitSearchAreaCache::GetNonCachedArea(const Rect16 &in) const
{
    Rect16 rect = in;
    rect.left &= ~(AreaSize - 1);
    rect.top &= ~(AreaSize - 1);
    rect.right = ((rect.right - 1) | (AreaSize - 1)) + 1;
    rect.bottom = ((rect.bottom - 1) | (AreaSize - 1)) + 1;
    int topleft = ToCacheEntry(rect.left, rect.top), topright = ToCacheEntry(rect.right - 1, rect.top);
    int bottomleft = ToCacheEntry(rect.left, rect.bottom - 1), bottomright = ToCacheEntry(rect.right - 1, rect.bottom - 1);
    int beg, pos;
    beg = pos = topleft;
    int end = topright;
    int stride = 1 << width_shift;
    while (cache[pos] != nullptr)
    {
        pos++;
        if (pos > end)
        {
            rect.top += AreaSize;
            if (rect.top == rect.bottom)
                return rect;
            beg += stride;
            end += stride;
            pos = beg;
        }
    }
    topleft = beg, topright = end;
    beg = pos = bottomleft, end = bottomright;
    while (cache[pos] != nullptr)
    {
        pos++;
        if (pos > end)
        {
            rect.bottom -= AreaSize;
            beg -= stride;
            end -= stride;
            pos = beg;
        }
    }
    bottomleft = beg, bottomright = end;
    beg = pos = topleft, end = bottomleft;
    while (cache[pos] != nullptr)
    {
        pos += stride;
        if (pos > end)
        {
            rect.left += AreaSize;
            if (rect.left == rect.right)
                return rect;
            beg += 1;
            end += 1;
            pos = beg;
        }
    }
    beg = pos = topright, end = bottomright;
    while (cache[pos] != nullptr)
    {
        pos += stride;
        if (pos > end)
        {
            rect.right -= AreaSize;
            beg -= 1;
            end -= 1;
            pos = beg;
        }
    }
    return rect;
}

// Optimization as mingw causes memmove to be a msvcrt call
static void FillEntry_MemMove(void *out_, void *in_, size_t length)
{
    uint32_t *out = (uint32_t *)((uint8_t *)out_ + length);
    uint32_t *in = (uint32_t *)((uint8_t *)in_ + length);
    while (out != out_)
    {
        out--;
        in--;
        *out = *in;
    }
}

// MUST use FinishAreas() afterwards or everything goes wrong
template <class C>
vector<UnitSearchAreaCache::AreaInfo<C>> UnitSearchAreaCache::AllocateAreaInfos(unsigned int first, unsigned int count, unsigned int max_size)
{
    int stride = 1 << width_shift;
    Assert(count <= memory.size());
    vector<AreaInfo<C>> ret;
    ret.reserve(count);
    auto pos = first;
    for (int i = 0; i < count; i++, pos += stride)
    {
        uint8_t *pos = memory[i].MemAlloc(max_size * sizeof(C) + sizeof(Area<C>));
        Area<C> *area = (Area<C> *)pos;
        ret.emplace_back(area, (C *)(pos + sizeof(Area<C>)));
    }
    return ret;
}

template <class C>
void UnitSearchAreaCache::FinishAreas(unsigned int first, vector<AreaInfo<C>> &&areas)
{
    int stride = 1 << width_shift;
    auto pos = first;
    int index = 0;
    for (const auto &info : areas)
    {
        // Once again, this magic depends on the ordering of arrays
        info.area->rest.begin = (C *)((uint8_t *)info.area + sizeof(Area<C>));
        info.area->rest.end = info.out_rest;
        info.area->topleft.begin = info.out_rest;
        info.area->topleft.end = info.out_topleft;
        info.area->top.begin = info.out_topleft;
        info.area->top.end = info.out_top;
        info.area->left.begin = info.out_top;
        info.area->left.end = info.out_left;
        cache[pos] = info.area;
        pos += stride;
        memory[index].EndAlloc((uint8_t *)(info.out_left));
        index++;
    }
}

// Assumes units to be sorted by left coord, first and last are both included
// Also assumes that units are always in valid x, and that until invalid x is found,
// y is included
void UnitSearchAreaCache::FillEntryColumn(unsigned int first, unsigned int count, Array<Unit *> units)
{
    int x = (first - (first >> width_shift << width_shift)) * AreaSize;
    int first_entry_y = (first >> width_shift);
    vector<AreaInfo<Unit *>> areas = AllocateAreaInfos<Unit *>(first, count, units.len);
    // data will be formatted in the following way:
    // area, area->rest, area->topleft, area->top, area->left
    // Since area->rest is always located at area + sizeof(Area),
    // the pointer could be completely dropped, but 16-byte alignment never hurts
    // (Though atm region cache will break the alignment)
    for (Unit *unit : units)
    {
        const Rect16 box = unit->GetCollisionRect();
        if (box.left >= x + AreaSize)
            break;
        if (unit->TempFlagCheck())
        {
            auto entry_y = max(0, box.top / AreaSize - first_entry_y);
            auto last = min((int)count - 1, (box.bottom - 1) / AreaSize - first_entry_y);
            for (; entry_y <= last; entry_y++)
            {
                AreaInfo<Unit *> &a = areas[entry_y];
                int y = (entry_y + first_entry_y) * AreaSize;
                // These xpos ifs ould be switched out of for loop
                if (box.left >= x && box.left < x + AreaSize) // Not left
                {
                    if (box.top >= y) // Not top => rest
                    {
                        FillEntry_MemMove(a.out_rest + 1, a.out_rest, (a.out_left - a.out_rest) * sizeof(Unit *));
                        *a.out_rest++ = unit;
                        a.out_topleft++, a.out_top++, a.out_left++;
                    }
                    else // Top => top
                    {
                        Assert(box.bottom > y);
                        FillEntry_MemMove(a.out_top + 1, a.out_top, (a.out_left - a.out_top) * sizeof(Unit *));
                        *a.out_top++ = unit;
                        a.out_left++;
                    }
                }
                else if (box.right > x) // Left
                {
                    Assert(box.right > x);
                    if (box.top >= y) // Not top => left
                    {
                        *a.out_left++ = unit;
                    }
                    else // Top => topleft
                    {
                        Assert(box.bottom > y);
                        FillEntry_MemMove(a.out_topleft + 1, a.out_topleft, (a.out_left - a.out_topleft) * sizeof(Unit *));
                        *a.out_topleft++ = unit;
                        a.out_top++, a.out_left++;
                    }
                }
            }
        }
    }
    Unit::ClearTempFlags();
    FinishAreas(first, move(areas));
}

void UnitSearchAreaCache::FillCache(const Rect16 &rect, Array<Unit *> units)
{
    int topleft = ToCacheEntry(rect.left, rect.top), topright = ToCacheEntry(rect.right - 1, rect.top);
    int bottomleft = ToCacheEntry(rect.left, rect.bottom - 1);
    int width = topright - topleft + 1;
    int height = ((bottomleft - topleft) >> width_shift) + 1;
    auto max_width = *bw::unit_max_width;
    Assert(std::is_sorted(units.begin(), units.end(), [](Unit *a, Unit *b) { return a->GetCollisionRect().left < b->GetCollisionRect().left; }));
    for (int i = 0; i < width; i++)
    {
        int left = max(0, rect.left + i * AreaSize - max_width);
        auto beg = std::lower_bound(units.begin(), units.end(), left,
                [](const Unit *u, const x32 &pos) { return u->GetCollisionRect().left < pos; });
        FillEntryColumn(topleft + i, height, Array<Unit *>(beg.ptr(), units.end().ptr()));
        units.SetBeg(beg);
    }
}

// There's code duplication in ForEach :/
void UnitSearchAreaCache::Find(const Rect16 &rect, Unit **out, Unit ***out_end, AreaBuffer<Unit *> *out_bufs, AreaBuffer<Unit *> **out_bufs_end)
{
    const int Left = 1, Top = 2, Right = 4, Bottom= 8;
    // Loops use intentionally raw_pos < end and not !=, as raw_pos can be already larger than end if the area is small
    int stride = 1 << width_shift;
    // Easier to determine last raw cache entry which is inside bound than first outside
    int raw_topleft = ToCacheEntry(rect.left, rect.top);
    int raw_topright = ToCacheEntry(rect.right - 1, rect.top);
    int raw_bottomleft = ToCacheEntry(rect.left, rect.bottom - 1);
    int raw_bottomright = ToCacheEntry(rect.right - 1, rect.bottom - 1);
    int raw_pos = raw_topleft;

    bool left_aligned = rect.left % AreaSize == 0, top_aligned = rect.top % AreaSize == 0;
    bool right_aligned = rect.right % AreaSize == 0, bottom_aligned = rect.bottom % AreaSize == 0;
    // Topleft
    if (!left_aligned || !top_aligned)
    {
        auto *area = cache[raw_pos];
        out = AddMatchingUnits<Top | Left | Bottom | Right>(rect, area->topleft, out);
        out = AddMatchingUnits<Top | Left | Bottom | Right>(rect, area->top, out);
        out = AddMatchingUnits<Top | Left | Bottom | Right>(rect, area->left, out);
        out = AddMatchingUnits<Top | Left | Bottom | Right>(rect, area->rest, out);
    }
    else
    {
        auto *area = cache[raw_pos];
        *out_bufs++ = area->topleft;
        *out_bufs++ = area->top;
        *out_bufs++ = area->left;
        *out_bufs++ = area->rest;
    }
    // Top
    {
        raw_pos = raw_topleft + 1;
        while (raw_pos < raw_topright)
        {
            auto *area = cache[raw_pos];
            if (!top_aligned)
            {
                out = AddMatchingUnits<Top | Bottom>(rect, area->top, out);
                out = AddMatchingUnits<Top | Bottom>(rect, area->rest, out);
            }
            else
            {
                *out_bufs++ = area->top;
                *out_bufs++ = area->rest;
            }
            raw_pos += 1;
        }
        if (raw_pos == raw_topright)
        {
            auto *area = cache[raw_pos];
            if (!top_aligned || !right_aligned)
            {
                out = AddMatchingUnits<Top | Right | Bottom>(rect, area->top, out);
                out = AddMatchingUnits<Top | Right | Bottom>(rect, area->rest, out);
            }
            else
            {
                *out_bufs++ = area->top;
                *out_bufs++ = area->rest;
            }
        }
    }
    // Left
    {
        raw_pos = raw_topleft + stride;
        while (raw_pos < raw_bottomleft)
        {
            auto *area = cache[raw_pos];
            if (!left_aligned)
            {
                out = AddMatchingUnits<Left | Right>(rect, area->left, out);
                out = AddMatchingUnits<Left | Right>(rect, area->rest, out);
            }
            else
            {
                *out_bufs++ = area->left;
                *out_bufs++ = area->rest;
            }
            raw_pos += stride;
        }
        if (raw_pos == raw_bottomleft)
        {
            auto *area = cache[raw_pos];
            if (!left_aligned || !bottom_aligned)
            {
                out = AddMatchingUnits<Left | Right | Bottom>(rect, area->left, out);
                out = AddMatchingUnits<Left | Right | Bottom>(rect, area->rest, out);
            }
            else
            {
                *out_bufs++ = area->left;
                *out_bufs++ = area->rest;
            }
        }
    }
    // Middle
    {
        int center_width = raw_topright - raw_topleft - 2;
        raw_pos = raw_topleft + 1 + stride;
        int end = raw_bottomright - 1 - stride;
        while (raw_pos <= end)
        {
            int line_end = raw_pos + center_width;
            for (int i = raw_pos; i <= line_end; i += 1)
            {
                auto *area = cache[i];
                *out_bufs++ = area->rest;
            }
            raw_pos += stride;
        }
    }
    // Right
    if (raw_topleft != raw_topright)
    {
        raw_pos = raw_topright + stride;
        while (raw_pos < raw_bottomright)
        {
            auto *area = cache[raw_pos];
            if (!right_aligned)
                out = AddMatchingUnits<Right>(rect, area->rest, out);
            else
                *out_bufs++ = area->rest;
            raw_pos += stride;
        }
    }
    // Bottom
    if (raw_topleft != raw_bottomleft)
    {
        raw_pos = raw_bottomleft + 1;
        while (raw_pos < raw_bottomright)
        {
            auto *area = cache[raw_pos];
            if (!bottom_aligned)
                out = AddMatchingUnits<Bottom>(rect, area->rest, out);
            else
                *out_bufs++ = area->rest;
            raw_pos += 1;
        }
        // Bottom corner
        if (raw_pos == raw_bottomright) // False if 1 cache slot width
        {
            if (!bottom_aligned || !right_aligned)
                out = AddMatchingUnits<Bottom | Right>(rect, cache[raw_pos]->rest, out);
            else
                *out_bufs++ = cache[raw_pos]->rest;
        }
    }

    *out_end = out;
    *out_bufs_end = out_bufs;
}
