#ifndef UPGRADE_H
#define UPGRADE_H

#include "dat.h"

int GetUpgradeLevel(UpgradeType upgrade, int player);
void SetUpgradeLevel(UpgradeType upgrade, int player, int amount);

class UpgradeType
{
    public:
        constexpr explicit UpgradeType(int upgrade_id) : upgrade_id(upgrade_id) { }

        uint32_t Label() const { return UintValue(8); }
        const char *Name() const;

        UnitType MovementSpeedUpgradeUnit();
        UnitType AttackSpeedUpgradeUnit();

        constexpr uint16_t Raw() const { return upgrade_id; }
        constexpr operator uint16_t() const { return Raw(); }

    private:
        uint16_t upgrade_id;

        const DatTable &GetDat(int index) const;
        template <class Type>
        const Type &Value(int index, int offset = 0) const {
            return Dat::Value<Type>(&GetDat(index), upgrade_id, offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            return Dat::UintValue(&GetDat(index), upgrade_id, offset);
        }
};

#endif // UPGRADE_H

