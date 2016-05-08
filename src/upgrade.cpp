#include "upgrade.h"
#include "offsets.h"
#include "strings.h"
#include "unit.h"

#include "constants/upgrade.h"

int GetUpgradeLevel(UpgradeType upgrade_, int player)
{
    int upgrade = upgrade_.Raw();
    Assert(upgrade >= 0 && upgrade < UpgradeId::None.Raw());
    if (upgrade < 0x2e)
        return bw::upgrade_level_sc[player][upgrade];
    else
        return bw::upgrade_level_bw[player][upgrade - 0x2e];
}

void SetUpgradeLevel(UpgradeType upgrade_, int player, int amount)
{
    int upgrade = upgrade_.Raw();
    Assert(upgrade >= 0 && upgrade < UpgradeId::None.Raw());
    if (upgrade < 0x2e)
        bw::upgrade_level_sc[player][upgrade] = amount;
    else
        bw::upgrade_level_bw[player][upgrade - 0x2e] = amount;
}

const DatTable &UpgradeType::GetDat(int index) const
{
    return bw::upgrades_dat[index];
}

const char *UpgradeType::Name() const
{
    return (*bw::stat_txt_tbl)->GetTblString(Label());
}

UnitType UpgradeType::MovementSpeedUpgradeUnit()
{
    using namespace UpgradeId;
    using namespace UnitId;

    switch (upgrade_id)
    {
        case IonThrusters:
            return Vulture;
        case LegEnhancements:
            return Zealot;
        case GraviticThrusters:
            return Scout;
        case GraviticBoosters:
            return Observer;
        case GraviticDrive:
            return Shuttle;
        case MetabolicBoost:
            return Zergling;
        case PneumatizedCarapace:
            return Overlord;
        case MuscularAugments:
            return Hydralisk;
        case AnabolicSynthesis:
            return Ultralisk;
        default:
            return UnitId::None;
    }
}

UnitType UpgradeType::AttackSpeedUpgradeUnit()
{
    if (upgrade_id == UpgradeId::AdrenalGlands)
        return UnitId::Zergling;
    return UnitId::None;
}
