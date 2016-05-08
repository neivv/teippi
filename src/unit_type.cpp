#include "unit_type.h"

#include "constants/unit.h"
#include "constants/upgrade.h"

using namespace UnitId;

UnitType::UnitType() : unit_id(UnitId::None)
{
}

UpgradeType UnitType::ArmorUpgrade() const
{
    return UpgradeType(UintValue(25));
}

bool UnitType::IsGoliath() const
{
    return *this == Goliath || *this == AlanSchezar;
}

bool UnitType::IsCarrier() const
{
    return *this == Carrier || *this == Gantrithor;
}

bool UnitType::IsReaver() const
{
    return *this == Reaver || *this == Warbringer;
}

bool UnitType::HasHangar() const
{
    return IsCarrier() || IsReaver();
}

bool UnitType::IsEgg() const
{
    return *this == Egg || *this == LurkerEgg || *this == Cocoon;
}

bool UnitType::IsMineralField() const
{
    return *this == MineralPatch1 || *this == MineralPatch2 || *this == MineralPatch3;
}

bool UnitType::IsGasBuilding() const
{
    return *this == Assimilator || *this == Refinery || *this == Extractor;
}

bool UnitType::HasRally() const
{
    switch (unit_id)
    {
        case CommandCenter:
        case Barracks:
        case Factory:
        case Starport:
        case InfestedCommandCenter:
        case Hatchery:
        case Lair:
        case Hive:
        case Nexus:
        case Gateway:
        case RoboticsFacility:
        case Stargate:
            return true;
        default:
            return false;
    }
}

bool UnitType::IsCritter() const
{
    switch (unit_id) {
        case Bengalaas:
        case Rhynadon:
        case Scantid:
        case Kakaru:
        case Ragnasaur:
        case Ursadon:
            return true;
        default:
            return false;
    }
}

bool UnitType::IsBuildingMorphUpgrade() const
{
    switch (unit_id) {
        case Lair:
        case Hive:
        case GreaterSpire:
        case SunkenColony:
        case SporeColony:
            return true;
        default:
            return false;
    }
}

bool UnitType::IsClickable() const
{
    // It would be cool to just use images.dat clickable, but
    // nuke is set clickable there <.<
    switch (unit_id) {
        case NuclearMissile:
        case Scarab:
        case DisruptionWeb:
        case DarkSwarm:
        case LeftUpperLevelDoor:
        case RightUpperLevelDoor:
        case LeftPitDoor:
        case RightPitDoor:
            return false;
        default:
            return true;
    }
}

int UnitType::OverlaySize() const
{
    if (Flags() & UnitFlags::MediumOverlays)
        return 1;
    else if (Flags() & UnitFlags::LargeOverlays)
        return 2;
    else
        return 0;
}

bool UnitType::MatchesTriggerUnitId(UnitType trig_unit_id) const
{
    switch (trig_unit_id.Raw())
    {
        case Trigger_Any:
            return true;
        case Trigger_Men:
            return GroupFlags() & 0x8;
        case Trigger_Buildings:
            return GroupFlags() & 0x10;
        case Trigger_Factories:
            return GroupFlags() & 0x20;
        default:
            return trig_unit_id == unit_id;
    }
}

UnitType UnitType::EggCancelUnit() const
{
    switch (unit_id)
    {
        case Cocoon:
            return Mutalisk;
        case LurkerEgg:
            return Hydralisk;
        default:
            return None;
    }
}

UpgradeType UnitType::EnergyUpgrade() const
{
    using namespace UpgradeId;

    switch (unit_id)
    {
        case Ghost:
            return MoebiusReactor;
        case Wraith:
            return ApolloReactor;
        case ScienceVessel:
            return TitanReactor;
        case Battlecruiser:
            return ColossusReactor;
        case Medic:
            return CaduceusReactor;
        case Queen:
            return GameteMeiosis;
        case Defiler:
            return MetasynapticNode;
        case Corsair:
            return ArgusJewel;
        case DarkArchon:
            return ArgusTalisman;
        case HighTemplar:
            return KhaydarinAmulet;
        case Arbiter:
            return KhaydarinCore;
        default:
            return UpgradeId::None;
    }
}

UpgradeType UnitType::SightUpgrade() const
{
    using namespace UpgradeId;

    switch (unit_id)
    {
        case Ghost:
            return OcularImplants;
        case Overlord:
            return Antennae;
        case Observer:
            return SensorArray;
        case Scout:
            return ApialSensors;
        default:
            return UpgradeId::None;
    }
}
