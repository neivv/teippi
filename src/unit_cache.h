#ifndef UNIT_CACHE_H
#define UNIT_CACHE_H

#include "types.h"
#include "unit.h"
#include "unitsearch.h"
#include "unitsearch_cache.h"
#include "bullet.h"
#include "common/assert.h"
#include "perfclock.h"

#include "unitsearch.hpp"
#include "unitsearch_cache.hpp"

class EnemyUnitCache
{
    public:
        EnemyUnitCache() {}
        EnemyUnitCache(EnemyUnitCache &&other) = default;
        EnemyUnitCache &operator=(EnemyUnitCache &&other) = default;
        void Clear()
        {
            cache.Clear();
        }

        void SetSize(xuint x, yuint y)
        {
            cache.SetSize(x, y);
        }

        /// There are no check against calling the callback for own if it can attack self
        template <class Cb>
        void ForAttackableEnemiesInArea(MainUnitSearch *search, const Unit *own, const Rect16 &area_, Cb callback)
        {
            Rect16 area = area_.Clipped(MapBounds());
            auto ground_air = ForAttackableEnemiesInArea_Init(search, own, area);
            bool ground = std::get<0>(ground_air);
            bool air = std::get<1>(ground_air);
            if (!ground && !air)
                return;

            for (int i = 0; i < Limits::ActivePlayers; i++)
            {
                if (bw::alliances[own->player][i] == 0)
                {
                    bool stop = ForAttackableEnemiesInArea(area, own, i, ground, air, callback);
                    if (stop)
                        return;
                }
            }
        }
    private:
        /// Helper function for ForAttackableEnemiesInArea, separated to drastically reduce binary size.
        /// As the parent function is a template, compilers don't realize that most of the code can be shared.
        tuple<bool, bool> ForAttackableEnemiesInArea_Init(MainUnitSearch *search, const Unit *own, const Rect16 &area)
        {
            bool ground;
            bool air;
            // From CanAttackUnit
            // TODO remove this duplication
            switch (own->unit_id)
            {
                case Unit::Carrier:
                case Unit::Gantrithor:
                case Unit::Reaver:
                case Unit::Warbringer:
                case Unit::Queen:
                case Unit::Matriarch:
                    // Can't filter these with simple ground/air split this cache does
                    air = true;
                    ground = true;
                break;
                default:
                    if (own->unit_id == Unit::Arbiter && own->ai != nullptr)
                    {
                        air = false;
                        ground = false;
                    }
                    else
                    {
                        const Unit *turret = own;
                        if (own->HasSubunit())
                            turret = own->subunit;
                        air = turret->GetAirWeapon() != Weapon::None;
                        ground = turret->GetGroundWeapon() != Weapon::None;
                    }
            }
            if (!ground && !air)
                return make_tuple(false, false);

            search->FillSecondaryCache(&cache, area, [](const Unit *unit) {
                // Filter
                // Could filter by unit->CanBeAttacked
                return unit->GetOriginalPlayer() < Limits::ActivePlayers;
            }, [](Unit *unit) {
                // Make key
                if (unit->IsFlying())
                    return unit->GetOriginalPlayer() * 2 + 1;
                else
                    return unit->GetOriginalPlayer() * 2;
            });
            return make_tuple(ground, air);
        }

        template <class Cb>
        bool ForAttackableEnemiesInArea(const Rect16 &area, const Unit *own, int player, bool ground, bool air, Cb callback)
        {
            bool stop = false;
            auto lambda = [&](Unit *unit, bool *stop2) {
                if (!own->CanAttackUnit(unit))
                    return;
                callback(unit, &stop);
                *stop2 = stop;
            };
            if (ground)
                cache.Cache(player * 2).ForEach(area, lambda);
            if (air && !stop)
                cache.Cache(player * 2 + 1).ForEach(area, lambda);
            return stop;
        }

        SecondaryAreaCache<Unit *, Limits::ActivePlayers * 2> cache;
};

extern EnemyUnitCache *enemy_unit_cache; // Declared in unit.cpp

#endif /* UNIT_CACHE_H */
