#ifndef UNIT_SEARCH_H
#define UNIT_SEARCH_H

#include "types.h"
#include "unit.h"
#include "unitsearch_cache.h"

#pragma pack(push)
#pragma pack(1)
struct UnitPositions
{
    Unit *unit;
    uint16_t pos;
    uint16_t type;
};

struct CollisionArea
{
    Rect16 area;
    Unit *self;
};

#pragma pack(pop)

class LeftSortHelper;
class RightSortHelper;

template <class Type>
class PosSearch
{
    public:
        PosSearch() :max_width(0) {}
        PosSearch(PosSearch &&other) = default;
        void Clear();
        void RemoveAt(uintptr_t pos);

        unsigned Size() const { return left_to_value.size(); }
        void Find(const Rect16 &rect, Type *out, Type **out_end);
        template <class Func1, class Func2>
        Type *FindNearest(const Point &pos, const Rect16 &area, Func1 IsValid, Func2 Position);
        void Add(uintptr_t pos, Type &&val, const Rect16 &box);

    protected:
        vector<Type> left_to_value;
        vector<x32> left_positions;
        vector<x32> left_to_right;
        vector<y32> left_to_top;
        vector<y32> left_to_bottom;

        int NewFind(x32 left);

        x32 max_width;
};

class UnitSearch : protected PosSearch<Unit *>
{
    public:
        UnitSearch() {}
        UnitSearch(UnitSearch &&other) = default;

        template <class Func>
        Unit *FindNearest(const Point &pos, const Rect16 &area, Func IsValid) {
            // PosSearch::FindNearest returns nullptr/pointer to unit pointer,
            // so it is flattened to nullptr/unit pointer here
            Unit **closest = PosSearch::FindNearest(pos, area, IsValid, [](const Unit *a) {
                return a->sprite->position;
            });
            return closest ? *closest : nullptr;
        }

        void Init();
        unsigned Size() const { return PosSearch::Size(); }
        bool DoesBlockArea(const Unit *unit, const CollisionArea *area) const;
};

// While units may be included in multiple UnitSearches, they may only be part of one MainUnitSearch
// (MainUnitSearch uses unit->search_left, allowing faster/more operations)
// Also includes bw shims and search caches
class MainUnitSearch : public UnitSearch
{
    friend class LeftSortHelper;
    public:
        typedef UnitSearchAreaCache::AreaBuffer<Unit *> AreaCacheBuf;
        MainUnitSearch();
        MainUnitSearch(MainUnitSearch &&other) = default;
        ~MainUnitSearch();

        void Clear();

        void Init();
        void Add(Unit *unit);
        Unit **FindCollidingUnits(Unit *unit, int x_add, int y_add);

        // Bw FindUnitsRect includes rect.right and rect.bottom in it's search, this one won't,
        // possibly causing bugs (or maybe fixing some) in precise (=collision?) code
        // The FindUnitsRect hook adds 1 to rect.right and rect.bottom for this reason, so only redone code may have issues
        // FindUnitBordersRect didn't include right and bottom coords in bw, so it won't cause issues
        // (No clue about more specialized functions)
        Unit **FindUnitsRect(const Rect16 &rect, int *amount = nullptr, Unit **out = nullptr);

        // Bw compatibility functions
        Unit **FindUnitBordersRect(const Rect16 *rect);
        Unit **CheckMovementCollision(Unit *unit, int x_off, int y_off);
        int DoUnitsCollide(const Unit *first, const Unit *second);
        int GetDodgingDirection(const Unit *self, const Unit *other);
        void ChangeUnitPosition(Unit *unit, int x_diff, int y_diff);

        // Allows changing multiple positions faster, but doing any searching before Finish will not work
        void ChangeUnitPosition_Fast(Unit *unit, int x_diff, int y_diff);
        void ChangeUnitPosition_Finish();

        // Bw-compatible signature
        Unit *FindNearestUnit(Unit *self, const Point &pos, int (__fastcall *IsValid)(const Unit *, void *), void *func_param, const Rect16 &area_);

        void GetNearbyBlockingUnits(PathingData *pd);
        void Remove(Unit *unit);

        /// Bw requires that unitsearch writes to buffer which is large enough to hold all unit pointers
        /// four times. NewEntry() and PopResult() allocate/free from that buffer. They can also
        /// be used to just get some space for results, although one has to remember to call PopResult
        Unit **NewEntry();
        void PopResult();

        UnitSearchRegionCache::Entry FindUnits_ChooseTarget(int region, bool ground);
        Unit **FindHelpingUnits(Unit *unit, const Rect16 &rect, TempMemoryPool *allocation_pool, bool ai);
        void ClearRegionCache();
        void EnableAreaCache();
        void DisableAreaCache();

        // Public for micro-optimizations, though FindUnitsRect should be good enough
        // out and bufs must be inited with arrays which is large enough,
        // that is all unit search units for out and AreaCacheAmount for bufs
        void AreaCacheFind(const Rect16 &rect, Unit **out, Unit ***out_end, AreaCacheBuf *out_bufs, AreaCacheBuf **out_bufs_end);

        template <class Func>
        void ForEachUnitInArea(const Rect16 &rect, Func func)
        {
            Unit **units;
            int amt;
            units = FindUnitsRect(rect, &amt);
            Unit **end = units + amt;

            while (units != end)
            {
                if (func(*units++) == true)
                    break;
            }
            PopResult();
        }

        // If area cache is enabled, it works in FindUnitsRect
        // Region cache works _always_ in FindUnits_ChooseTarget
        bool valid_region_cache;
        UnitSearchRegionCache region_cache;

        template <class Filter, class Key, int N>
        void FillSecondaryCache(SecondaryAreaCache<Unit *, N> *cache, const Rect16 &area, Filter filter, Key key);

        void CacheArea(const Rect16 &area);

    private:
        unsigned capacity;
        Unit **result_units_beg;

        bool area_cache_enabled;
        UnitSearchAreaCache area_cache;

        void Validate();

        AreaCacheBuf reasonable_area_cache_buf[32 * 32];

        // ChangeUnitPosition_Fast uses these
        x32 left_low_invalid;
        x32 left_high_invalid;
};

extern MainUnitSearch *unit_search;

#endif // UNIT_SEARCH_H

