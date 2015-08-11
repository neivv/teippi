#ifndef UNITSEARCH_CACHE_HPP
#define UNITSEARCH_CACHE_HPP 

#include "types.h"
#include "common/assert.h"
#include <algorithm>

template <class C, int N>
void SecondaryAreaCache<C, N>::SetSize(xuint x, yuint y)
{
    for (int i = 0; i < N; i++)
        caches[i].SetSize(x, y);
}

template <int Directions>
Unit **UnitSearchAreaCache::AddMatchingUnits(const Rect16 &rect, const AreaBuffer<Unit *> &in, Unit **out)
{
    const int Left = 1, Top = 2, Right = 4, Bottom= 8;
    in.Iterate([&](Unit *unit, bool *stop)
    {
        Rect16 box = unit->GetCollisionRect();
        if (Directions & Left && box.right <= rect.left)
            return;
        if (Directions & Right && box.left >= rect.right)
            return;
        if (Directions & Top && box.bottom <= rect.top)
            return;
        if (Directions & Bottom && box.top >= rect.bottom)
            return;
        *out++ = unit;
    });
    return out;
}

// There's code duplication in Find :/
// This function makes an assumption that it is faster to check multiple units to a buffer at once,
// and do callback to all of them afterwards. That assumption might be completely incorrect.
template <class Cb>
void UnitSearchAreaCache::ForEach(const Rect16 &rect, Cb callback)
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

    int width = raw_topright - raw_topleft + 1;
    uintptr_t largest_area_size = 0;
    for (int i = 0; i < width; i++)
    {
        auto *area = cache[raw_topleft + i];
        uintptr_t area_size = area->Left().Count() + area->Top().Count() +
            area->TopLeft().Count() + area->Rest().Count();
        largest_area_size = std::max(largest_area_size, area_size);
        area = cache[raw_bottomleft + i];
        area_size = area->Left().Count() + area->Top().Count() +
            area->TopLeft().Count() + area->Rest().Count();
        largest_area_size = std::max(largest_area_size, area_size);
    }
    for (int pos = raw_topleft + stride; pos < raw_bottomleft; pos += stride)
    {
        auto *area = cache[pos];
        uintptr_t area_size = area->Left().Count() + area->Top().Count() +
            area->TopLeft().Count() + area->Rest().Count();
        largest_area_size = std::max(largest_area_size, area_size);
        area = cache[pos + width - 1];
        area_size = area->Left().Count() + area->Top().Count() +
            area->TopLeft().Count() + area->Rest().Count();
        largest_area_size = std::max(largest_area_size, area_size);
    }
    for_each_buf.resize(largest_area_size);
    Unit **buf = for_each_buf.data.get();

    bool left_aligned = rect.left % AreaSize == 0, top_aligned = rect.top % AreaSize == 0;
    bool right_aligned = rect.right % AreaSize == 0, bottom_aligned = rect.bottom % AreaSize == 0;
    bool stop = false;
    // Topleft
    if (!left_aligned || !top_aligned)
    {
        auto *area = cache[raw_pos];
        Unit **buf_pos = buf;
        buf_pos = AddMatchingUnits<Top | Left | Bottom | Right>(rect, area->topleft, buf_pos);
        buf_pos = AddMatchingUnits<Top | Left | Bottom | Right>(rect, area->top, buf_pos);
        buf_pos = AddMatchingUnits<Top | Left | Bottom | Right>(rect, area->left, buf_pos);
        buf_pos = AddMatchingUnits<Top | Left | Bottom | Right>(rect, area->rest, buf_pos);
        for (Unit **buf_iter = buf; buf_iter != buf_pos; ++buf_iter)
        {
            callback(*buf_iter, &stop);
            if (stop)
                return;
        }
    }
    else
    {
        auto *area = cache[raw_pos];
        area->TopLeft().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
        if (stop)
            return;
        area->Top().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
        if (stop)
            return;
        area->Left().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
        if (stop)
            return;
        area->Rest().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
        if (stop)
            return;
    }
    // Top
    {
        raw_pos = raw_topleft + 1;
        while (raw_pos < raw_topright)
        {
            auto *area = cache[raw_pos];
            if (!top_aligned)
            {
                Unit **buf_pos = buf;
                buf_pos = AddMatchingUnits<Top | Bottom>(rect, area->top, buf_pos);
                buf_pos = AddMatchingUnits<Top | Bottom>(rect, area->rest, buf_pos);
                Assert(for_each_buf.end() >= buf_pos);
                for (Unit **buf_iter = buf; buf_iter != buf_pos; ++buf_iter)
                {
                    callback(*buf_iter, &stop);
                    if (stop)
                        return;
                }
            }
            else
            {
                area->Top().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
                if (stop)
                    return;
                area->Rest().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
                if (stop)
                    return;
            }
            raw_pos += 1;
        }
        if (raw_pos == raw_topright)
        {
            auto *area = cache[raw_pos];
            if (!top_aligned || !right_aligned)
            {
                Unit **buf_pos = buf;
                buf_pos = AddMatchingUnits<Top | Right | Bottom>(rect, area->top, buf_pos);
                buf_pos = AddMatchingUnits<Top | Right | Bottom>(rect, area->rest, buf_pos);
                Assert(for_each_buf.end() >= buf_pos);
                for (Unit **buf_iter = buf; buf_iter != buf_pos; ++buf_iter)
                {
                    callback(*buf_iter, &stop);
                    if (stop)
                        return;
                }
            }
            else
            {
                area->Top().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
                if (stop)
                    return;
                area->Rest().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
                if (stop)
                    return;
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
                Unit **buf_pos = buf;
                buf_pos = AddMatchingUnits<Left | Right>(rect, area->left, buf_pos);
                buf_pos = AddMatchingUnits<Left | Right>(rect, area->rest, buf_pos);
                Assert(for_each_buf.end() >= buf_pos);
                for (Unit **buf_iter = buf; buf_iter != buf_pos; ++buf_iter)
                {
                    callback(*buf_iter, &stop);
                    if (stop)
                        return;
                }
            }
            else
            {
                area->Left().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
                if (stop)
                    return;
                area->Rest().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
                if (stop)
                    return;
            }
            raw_pos += stride;
        }
        if (raw_pos == raw_bottomleft)
        {
            auto *area = cache[raw_pos];
            if (!left_aligned || !bottom_aligned)
            {
                Unit **buf_pos = buf;
                buf_pos = AddMatchingUnits<Left | Right | Bottom>(rect, area->left, buf_pos);
                buf_pos = AddMatchingUnits<Left | Right | Bottom>(rect, area->rest, buf_pos);
                Assert(for_each_buf.end() >= buf_pos);
                for (Unit **buf_iter = buf; buf_iter != buf_pos; ++buf_iter)
                {
                    callback(*buf_iter, &stop);
                    if (stop)
                        return;
                }
            }
            else
            {
                area->Left().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
                if (stop)
                    return;
                area->Rest().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
                if (stop)
                    return;
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
                area->Rest().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
                if (stop)
                    return;
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
            {
                Unit **buf_pos = buf;
                buf_pos = AddMatchingUnits<Right>(rect, area->rest, buf_pos);
                Assert(for_each_buf.end() >= buf_pos);
                for (Unit **buf_iter = buf; buf_iter != buf_pos; ++buf_iter)
                {
                    callback(*buf_iter, &stop);
                    if (stop)
                        return;
                }
            }
            else
            {
                area->Rest().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
                if (stop)
                    return;
            }
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
            {
                Unit **buf_pos = buf;
                buf_pos = AddMatchingUnits<Bottom>(rect, area->rest, buf);
                Assert(for_each_buf.end() >= buf_pos);
                for (Unit **buf_iter = buf; buf_iter != buf_pos; ++buf_iter)
                {
                    callback(*buf_iter, &stop);
                    if (stop)
                        return;
                }
            }
            else
            {
                area->Rest().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
                if (stop)
                    return;
            }
            raw_pos += 1;
        }
        // Bottom corner
        if (raw_pos == raw_bottomright) // False if 1 cache slot width
        {
            auto *area = cache[raw_pos];
            if (!bottom_aligned || !right_aligned)
            {
                Unit **buf_pos = buf;
                buf_pos = AddMatchingUnits<Bottom | Right>(rect, area->rest, buf_pos);
                Assert(for_each_buf.end() >= buf_pos);
                for (Unit **buf_iter = buf; buf_iter != buf_pos; ++buf_iter)
                {
                    callback(*buf_iter, &stop);
                    if (stop)
                        return;
                }
            }
            else
            {
                area->Rest().Iterate([&](Unit *u, bool *stop2) { callback(u, &stop); *stop2 = stop; });
                if (stop)
                    return;
            }
        }
    }
}

template <class Cb>
void UnitSearchAreaCache::ForNonCachedAreas(const Rect16 &area, Cb callback)
{
    int stride = 1 << width_shift;
    int topleft = ToCacheEntry(area.left, area.top);
    int topright = ToCacheEntry(area.right - 1, area.top);
    int bottomright = ToCacheEntry(area.right - 1, area.bottom - 1);
    int width = topright - topleft + 1;
    for (int pos = topleft; pos <= bottomright; pos += stride)
    {
        for (int i = 0; i < width; i++)
        {
            if (cache[pos + i] == nullptr)
                callback(pos + i);
        }
    }
}

template <class C>
UnitSearchAreaCache::Area<C> *UnitSearchAreaCache::AllocateNewArea(int raw_id, uintptr_t max_size)
{
    // Just uses entry 0 because lazy, just requires every area to be finished before staring new one
    // (Prevents threading though)
    uint8_t *pos = memory[0].MemAlloc(max_size * sizeof(C) + sizeof(Area<C>));
    Area<C> *area = (Area<C> *)pos;
    area->rest.begin = (C *)(pos + sizeof(Area<C>));
    // End etc are expected to be set by caller when it knows their location
    cache[raw_id] = area;
    return area;
}

template <class C>
void UnitSearchAreaCache::FinishArea(Area<C> *area)
{
    memory[0].EndAlloc((uint8_t *)(area->Left().end));
}

template <class C, int N>
void SecondaryAreaCache<C, N>::Clear()
{
    for (int i = 0; i < N; i++)
        caches[i].Clear();
}

// Assumes that search has area cache enabled and that the area cache entries map 1:1
// to entries in secondary cache
template <class C, int N>
template <class Filter, class Key>
void SecondaryAreaCache<C, N>::FillNonCachedAreas(UnitSearchAreaCache *master_cache, const Rect16 &area, Filter filter, Key key)
{
    using AreaBuffer = UnitSearchAreaCache::AreaBuffer<C>;
    // All caches have same areas cached/not, so caches[0] is ok
    caches[0].ForNonCachedAreas(area, [&](int id) {
        auto area = master_cache->FromEntryId(id);
        if (area->IsEmpty())
        {
            for (int i = 0; i < N; i++)
            {
                caches[i].MarkAreaEmpty(id);
            }
        }
        else
        {
            UnitSearchAreaCache::Area<C> *out_areas[N];
            // At least clang was not able to prove it doesn't need to recalculate area->Count()
            // if it was inside this loop
            auto max_size = area->Count();
            for (int i = 0; i < N; i++)
            {
                // Assume here that every area must have enough space allocated for all
                // units in the area.. (There's also completely unrelated awesome template syntax)
                out_areas[i] = caches[i].template AllocateNewArea<C>(id, max_size);
            }
            // ..So one of the area buffers can always be used to sort the units
            // The order is rest, topleft, top, left as required by UnitSearchCache
            // Also why does this have to be so painful ;_;
            FillAreaBuffers(area, out_areas, filter, key, [](auto *buf) -> AreaBuffer & {
                return buf->Rest();
            }, [](auto *buf) { return buf->Rest(); });
            for (int i = 0; i < N; i++)
                out_areas[i]->TopLeft().begin = out_areas[i]->Rest().end;

            FillAreaBuffers(area, out_areas, filter, key, [](auto *buf) -> AreaBuffer & {
                return buf->TopLeft();
            }, [](auto *buf) { return buf->TopLeft(); });
            for (int i = 0; i < N; i++)
                out_areas[i]->Top().begin = out_areas[i]->TopLeft().end;

            FillAreaBuffers(area, out_areas, filter, key, [](auto *buf) -> AreaBuffer & {
                return buf->Top();
            }, [](auto *buf) { return buf->Top(); });
            for (int i = 0; i < N; i++)
                out_areas[i]->Left().begin = out_areas[i]->Top().end;

            FillAreaBuffers(area, out_areas, filter, key, [](auto *buf) -> AreaBuffer & {
                return buf->Left();
            }, [](auto *buf) { return buf->Left(); });

            for (int i = 0; i < N; i++)
            {
                caches[i].FinishArea(out_areas[i]);
            }
        }
    });
}

template <class C, int N>
template <class Filter, class Key, class F, class F2>
void SecondaryAreaCache<C, N>::FillAreaBuffers(const UnitSearchAreaCache::Area<C> *in_area, UnitSearchAreaCache::Area<C> **out_areas, Filter filter, Key key, F GetBuffer, F2 GetConstBuffer)
{
    const auto &input = GetConstBuffer(in_area);
    // Optimization: a lot of area buffers are empty
    // (Quick measuring implies around 80% are empty)
    if (input.Count() == 0)
    {
        for (int i = 0; i < N; i++)
        {
            GetBuffer(out_areas[i]).end = GetBuffer(out_areas[i]).begin;
        }
        return;
    }

    // See comments in FillNonCachedAreas why it is safe to fill the first buffer
    // with entire unit data
    C *buf_begin = GetBuffer(out_areas[0]).begin;
    C *buf_pos = buf_begin;
    input.Iterate([&] (C &value, bool *stop) {
        if (filter(value))
            *buf_pos++ = value;
    });
    C *buf_end = buf_pos;
    std::sort(buf_begin, buf_end, [&](C &a, C &b) {
        return key(a) < key(b);
    });

    buf_pos = buf_begin;
    while (buf_pos != buf_end && key(buf_pos[0]) == 0)
        buf_pos++;
    GetBuffer(out_areas[0]).end = buf_pos;

    for (int i = 1; i < N; i++)
    {
        C *copy_buf = GetBuffer(out_areas[i]).begin;
        while (buf_pos != buf_end && key(buf_pos[0]) == i)
            *copy_buf++ = *buf_pos++;
        GetBuffer(out_areas[i]).end = copy_buf;
    }
}

#endif /* UNITSEARCH_CACHE_HPP */
