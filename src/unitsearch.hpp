#ifndef UNITSEARCH_HPP
#define UNITSEARCH_HPP 

#include "types.h"
#include "common/assert.h"
#include "offsets.h"

template <class Filter, class Key, int N>
void MainUnitSearch::FillSecondaryCache(SecondaryAreaCache<Unit *, N> *cache, const Rect16 &area,
        Filter filter, Key key)
{
    Assert(area.left <= area.right && area.right <= *bw::map_width);
    Assert(area.top <= area.bottom && area.bottom <= *bw::map_height);
    Assert(area_cache_enabled); 
    CacheArea(area);

    cache->FillNonCachedAreas(&area_cache, area, filter, key);
}


#endif /* UNITSEARCH_HPP */
