#ifndef UNITTYPE_H
#define UNITTYPE_H

#include "types.h"
#include "common/iter.h"
#include "dat.h"

/// Dat/other values that are constant for a single unit id

namespace UnitFlags
{
    const unsigned int Building = 0x1;
    const unsigned int Addon = 0x2;
    const unsigned int Air = 0x4;
    const unsigned int Worker = 0x8;
    const unsigned int Subunit = 0x10;
    const unsigned int FlyingBuilding = 0x20;
    const unsigned int Hero = 0x40;
    const unsigned int Regenerate = 0x80;
    const unsigned int SingleEntity = 0x0800;
    const unsigned int ResourceDepot = 0x1000;
    const unsigned int ResourceContainer = 0x2000;
    const unsigned int Detector = 0x8000;
    const unsigned int Organic = 0x00010000;
    const unsigned int RequiresCreep = 0x00020000;
    const unsigned int RequiresPsi = 0x00080000;
    const unsigned int Burrowable = 0x00100000;
    const unsigned int Spellcaster = 0x00200000;
    const unsigned int PermamentlyCloaked = 0x00400000;
    const unsigned int MediumOverlays = 0x02000000;
    const unsigned int LargeOverlays = 0x04000000;
    const unsigned int CanMove = 0x08000000;
    const unsigned int Invincible = 0x20000000;
    const unsigned int Mechanical = 0x40000000;
}

namespace Race
{
    enum e
    {
        Zerg = 0,
        Terran,
        Protoss,
        Unused, // Yep
        Neutral
    };
}

struct WidthHeight
{
    constexpr WidthHeight(x16u w, y16u h) : width(w), height(h) { }
    constexpr int Area() const { return (int)width * (int)height; }
    x16u width;
    y16u height;
};

class UnitType
{
    public:
        UnitType();
        constexpr explicit UnitType(int unit_id) : unit_id(unit_id) { }

        static uint32_t Amount() { return bw::units_dat[0].entries; }
        class Types : public Common::Iterator<Types, UnitType> {
            public:
                constexpr Types(uint32_t beg, uint32_t end_pos) : pos(beg), end_pos(end_pos) { }

                Optional<UnitType> next();

            private:
                uint32_t pos;
                uint32_t end_pos;
        };
        static Types All() { return Types(0, Amount()); }

        bool IsValid() const;

        FlingyType Flingy() const { return FlingyType(UintValue(0)); };
        UnitType Subunit() const { return UnitType(UintValue(1)); };
        uint8_t SpawnDirection() const { return UintValue(5); }
        bool HasShields() const { return UintValue(6); }
        int32_t Shields() const { return UintValue(7); }
        int32_t HitPoints() const { return UintValue(8); }
        uint8_t Elevation() const { return UintValue(9); }
        OrderType AiIdleOrder() const { return OrderType(UintValue(12)); }
        OrderType HumanIdleOrder() const { return OrderType(UintValue(13)); }
        OrderType ReturnToIdleOrder() const { return OrderType(UintValue(14)); }
        OrderType AttackUnitOrder() const { return OrderType(UintValue(15)); }
        OrderType AttackMoveOrder() const { return OrderType(UintValue(16)); }
        WeaponType GroundWeapon() const { return WeaponType(UintValue(17)); }
        WeaponType AirWeapon() const { return WeaponType(UintValue(19)); }
        uint8_t AiFlags() const { return UintValue(21); }
        uint32_t Flags() const { return UintValue(22); }
        uint8_t TargetAcquisitionRange() const { return UintValue(23); }
        uint8_t SightRange() const { return UintValue(24); }
        UpgradeType ArmorUpgrade() const;
        uint8_t ArmorType() const { return UintValue(26); }
        uint8_t Armor() const { return UintValue(27); }
        uint8_t RightClickAction() const { return UintValue(28); }
        uint16_t MineralCost() const { return UintValue(40); }
        uint16_t GasCost() const { return UintValue(41); }
        uint16_t BuildTime() const { return UintValue(42); }
        uint8_t GroupFlags() const { return UintValue(44); }
        uint8_t SpaceRequired() const { return UintValue(47); }
        uint8_t SpaceProvided() const { return UintValue(48); }
        uint16_t BuildScore() const { return UintValue(49); }
        uint16_t KillScore() const { return UintValue(50); }

        WidthHeight PlacementBox() const {
            const auto &dat = bw::units_dat[36];
            Assert(dat.entries > unit_id * 2);
            x16u w = *((uint16_t *)dat.data + unit_id * 2 + 0);
            y16u h = *((uint16_t *)dat.data + unit_id * 2 + 1);
            WidthHeight val(w, h);
            return val;
        }

        Rect16 DimensionBox() const {
            const auto &dat = bw::units_dat[38];
            Assert(dat.entries > unit_id * 4);
            x16u left = *((uint16_t *)dat.data + unit_id * 4 + 0);
            y16u top = *((uint16_t *)dat.data + unit_id * 4 + 1);
            x16u right = *((uint16_t *)dat.data + unit_id * 4 + 2);
            y16u bottom = *((uint16_t *)dat.data + unit_id * 4 + 3);
            return Rect16(left, top, right, bottom);
        }

        int Race() const {
            auto group_flags = GroupFlags();
            if (group_flags & 1)
                return Race::Zerg;
            if (group_flags & 4)
                return Race::Protoss;
            if (group_flags & 2)
                return Race::Terran;
            return Race::Neutral;
        }

        bool IsBuilding() const { return Flags() & UnitFlags::Building; }
        bool IsSubunit() const { return Flags() & UnitFlags::Subunit; }
        bool IsWorker() const { return Flags() & UnitFlags::Worker; }
        bool IsHero() const { return Flags() & UnitFlags::Hero; }

        bool HasRally() const;
        bool IsCritter() const;
        bool IsBuildingMorphUpgrade() const;
        bool IsClickable() const;
        int OverlaySize() const;

        bool IsGoliath() const;
        bool IsCarrier() const;
        bool IsReaver() const;
        bool HasHangar() const;

        /// Is any of the three zerg egg units.
        bool IsEgg() const;
        /// If the unit is a egg which doesn't die on cancel, returns the unit id it becomes.
        UnitType EggCancelUnit() const;

        bool IsMineralField() const;
        bool IsGasBuilding() const;
        bool MatchesTriggerUnitId(UnitType trig_unit_id) const;

        UpgradeType EnergyUpgrade() const;
        UpgradeType SightUpgrade() const;

        uint32_t Strength(bool ground) const;

        constexpr uint16_t Raw() const { return unit_id; }
        constexpr operator uint16_t() const { return Raw(); }

        static void InitializeStrength();

    private:
        uint16_t unit_id;

        static uint32_t *strength;

        template <class Type>
        const Type &Value(int index, int offset = 0) const {
            return Dat::Value<Type>(&bw::units_dat[index], unit_id, offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            return Dat::UintValue(&bw::units_dat[index], unit_id, offset);
        }
};
#endif /* UNITTYPE_H */
