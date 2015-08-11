#ifndef UNITSEARCH_CACHE_H
#define UNITSEARCH_CACHE_H

#include "types.h"
#include "common/iter.h"
#include "common/assert.h"
#include <vector>
#include <memory>

class MemArea
{
    public:
        MemArea(uint8_t *mem, uint8_t *end_) { pos = beg = mem; end = end_;}
        uint8_t *beg;
        uint8_t *pos;
        uint8_t *end;
};

class MemAllocator
{
    public:
        MemAllocator() {}
        MemArea *NewMemArea(uintptr_t min_size);
        uint8_t *MemAlloc(unsigned int max_size);
        void EndAlloc(uint8_t *pos) { current_area->pos = pos; }
        void Reset();

    private:
        vector<MemArea> memory_areas;
        MemArea *current_area;
};

class UnitSearchCache
{
    protected:
        UnitSearchCache() {}
        UnitSearchCache(UnitSearchCache &&other) = default;
        UnitSearchCache& operator=(UnitSearchCache &&other) = default;

        vector<MemAllocator> memory;
};

// Far too tied into UnitSearch::FindUnits_ChooseTarget
class UnitSearchRegionCache : public UnitSearchCache
{
    public:
        class Entry
        {
            public:
                Entry(uint8_t *p, int pl) : ptr(p), players(pl) {}
                Unit **GetRaw() { return (Unit **)(ptr + sizeof(Unit *) + sizeof(Unit *) * players); }
                uintptr_t Size() { return *(Unit ***)(ptr) - GetRaw(); }
                Array<Unit *> PlayerUnits(int player)
                {
                    Assert(player >= 0 && player < players);
                    Unit ***arr = (Unit ***)ptr;
                    uintptr_t len = arr[player + 1] - arr[player];
                    return Array<Unit *>(arr[player], len);
                }

            private:
                uint8_t *ptr;
                int players;
        };
        UnitSearchRegionCache();
        ~UnitSearchRegionCache();

        void SetSize(uint32_t size_);
        Optional<Entry> Find(uint32_t region, bool ground);
        void Clear();

        Entry FinishEntry(Unit **arr, uint32_t entry, bool ground, uint32_t count);
        Unit **NewEntry(uintptr_t max_size);

    private:
        uintptr_t MakeEntry(uintptr_t region, bool ground);
        vector<uint8_t *> cache;
        uint32_t size;
};

class UnitSearchAreaCache : public UnitSearchCache
{
    public:
        // Could use deque-based implementation? Minor performance loss but avoids requiring
        // large buffers which are rarely used
        template <class C>
        class AreaBuffer
        {
            public:
                AreaBuffer() {}
                AreaBuffer(C *b, C *e) : begin(b), end(e) {}
                template<class Cb> void Iterate(Cb callback) const
                {
                    bool stop = false;
                    for (C *pos = begin; !stop && pos != end; ++pos)
                        callback(*pos, &stop);
                }

                uintptr_t Count() const { return end - begin; }
                C *begin;
                C *end;
        };

        template <class C>
        class Area
        {
            public:
                typedef AreaBuffer<C> Buffer;
                Area() : rest(nullptr, nullptr), topleft(nullptr, nullptr),
                    top(nullptr, nullptr), left(nullptr, nullptr) {}
                Buffer &Rest() { return rest; }
                Buffer &TopLeft() { return topleft; }
                Buffer &Top() { return top; }
                Buffer &Left() { return left; }
                const Buffer &Rest() const { return rest; }
                const Buffer &TopLeft() const { return topleft; }
                const Buffer &Top() const { return top; }
                const Buffer &Left() const { return left; }

                uintptr_t Count() const
                {
                    return left.end - rest.begin;
                }

                bool IsEmpty() const
                {
                    return rest.begin == left.end;
                }

                // These waste slightly memory, as one's end is next's begin
                // Could have just the functions calculate begins on fly
                // (rest.begin == this + sizeof(*this))
                Buffer rest;
                Buffer topleft;
                Buffer top;
                Buffer left;
        };

        template <class C>
        struct AreaInfo
        {
            public:
                // Order is important - see FillEntryColumn comment
                AreaInfo(Area<C> *a, C *pos) : area(a), out_rest(pos), out_topleft(pos), out_top(pos), out_left(pos) {}
                Area<C> *area;
                C *out_rest;
                C *out_topleft;
                C *out_top;
                C *out_left;
        };

        // Does not necessarily have to be constant, but currently Unit::GetAutoTarget cache
        // assumes that it can just take an area owned by this and fill its caches with one
        static const int AreaSize = 128;
        UnitSearchAreaCache() {}
        UnitSearchAreaCache(UnitSearchAreaCache &&other) = default;
        UnitSearchAreaCache& operator=(UnitSearchAreaCache &&other) = default;

        void SetSize(xuint x, yuint y);
        void Clear();

        unsigned int AreaAmount(const Rect16 &rect) const
        {
            // Rounds left and top upwards if not 0, right and bottom downwards if not AreaSize -1
            // So it only returns complete areas
            unsigned int w = (((rect.right + 1) & ~(AreaSize - 1)) - (((rect.left - 1) | (AreaSize - 1)) + 1)) / AreaSize;
            unsigned int h = (((rect.bottom + 1) & ~(AreaSize - 1)) - (((rect.top - 1) | (AreaSize - 1)) + 1)) / AreaSize;
            // * 4 is overcautious, as only topleft corner can use 4 area bufs
            return w * h * 4;
        }

        Area<Unit *> *&GetEntry(xuint x, yuint y) { return cache[ToCacheEntry(x, y)]; }
        const Area<Unit *> *FromEntryId(int id) const { return cache[id]; }
        unsigned int ToCacheEntry(xuint x, yuint y) const { return (x / AreaSize) + (y / AreaSize << width_shift); }

        Rect16 GetNonCachedArea(const Rect16 &rect) const;
        template <class Cb>
        void ForNonCachedAreas(const Rect16 &area, Cb callback);

        void FillCache(const Rect16 &rect, Array<Unit *> units);
        void Find(const Rect16 &rect, Unit **out, Unit ***out_end, AreaBuffer<Unit *> *out_bufs, AreaBuffer<Unit *> **out_bufs_end);
        template <class Cb>
        void ForEach(const Rect16 &rect, Cb callback);

        /// Should only be used in cases where you know better and wish to fill the area by yourself
        /// All pointers in the returned area point to same block of memory, which _must_ to be used to
        /// fill the area in order specified by comment in FillEntryColumn.
        /// Before allocating more areas, the allocated area must be finished with FinishArea
        template <class C>
        Area<C> *AllocateNewArea(int raw_id, uintptr_t max_size);
        /// Finishes an allocated area
        template <class C>
        void FinishArea(Area<C> *area);

        // Makes area point to global empty area, obviously assumes that areas will not
        // be modified after creation, they currently are not
        void MarkAreaEmpty(int raw_id)
        {
            cache[raw_id] = &empty_area;
        }

    private:
        static Area<Unit *> empty_area;

        template <int directions>
        Unit **AddMatchingUnits(const Rect16 &rect, const AreaBuffer<Unit *> &in, Unit **out);
        void FillEntry(unsigned int entry, Array<Unit *> units);

        template <class C>
        vector<AreaInfo<C>> AllocateAreaInfos(unsigned int first, unsigned int count, unsigned int max_size);
        template <class C>
        void FinishAreas(unsigned int first, vector<AreaInfo<C>> &&areas);
        void FillEntryColumn(unsigned int first, unsigned int count, Array<Unit *> units);

        vector<Area<Unit *> *> cache;
        // Could be used for other stuff as well
        // Vector is not used due to slower push_back (it always checks if it needs to realloc)
        Common::OwnedArray<Unit *> for_each_buf;
        uint32_t width_shift;
        uint32_t cache_size;
};

/// Cache that is designed to be used in more specialized situations than normal AreaCache,
/// such as caching only flying zerg units owned by players, splitting them to ai/human caches
/// which can be checked separately.
/// N specifies the amount of buckets the cached data is split into.
/// The splitting function is passed separately every time to FillNonCachedAreas.
/// Using the previous example, one would have C = Unit *, N = 2 and call FillNonCachedAreas with
///   filter = [](Unit *u) { return unit->IsFlying() && unit->GetRace == Race::Zerg; }
///   key = [](Unit *u) { return unit->ai ? 0 : 1; }
/// (Though this one considers neutrals etc "non-ai", one would maybe filter them as well)
/// Additionaly, having no filter and having key return values >= N will also filter them out,
/// though with different performance (Might be better if filtering and keying use same, slow operation)
/// (Maybe having separate filter function is a bad idea? Also is STL's sort conservative on key creation?
/// Currently it might call key function unnecessarily often and it would be just better to generate key once
/// during sortable array construction/filter phase)
/// (Also current implementation is just simple wrapper of multiple AreaCaches, a bit wasteful as
/// they always have same areas valid/same sizes etc)
template <class C, int N>
class SecondaryAreaCache
{
    public:
        SecondaryAreaCache() {}
        SecondaryAreaCache(SecondaryAreaCache &&other) = default;
        SecondaryAreaCache& operator=(SecondaryAreaCache &&other) = default;
        void Clear();
        void SetSize(xuint x, yuint y);

        template <class Filter, class Key>
        void FillNonCachedAreas(UnitSearchAreaCache *master_cache, const Rect16 &area, Filter filter, Key key);
        UnitSearchAreaCache &Cache(int n) { return caches[n]; }
    private:

        template <class Filter, class Key, class F, class F2>
        void FillAreaBuffers(const UnitSearchAreaCache::Area<C> *in_area, UnitSearchAreaCache::Area<C> **out_areas, Filter filter, Key key, F GetBuffer, F2 GetConstBuffer);
        UnitSearchAreaCache caches[N];
};



#endif // UNITSEARCH_CACHE_H

