#include "unit_type.h"

#include <algorithm>

#include "constants/unit.h"
#include "constants/upgrade.h"
#include "constants/weapon.h"

using namespace UnitId;

uint32_t *UnitType::strength = nullptr;

Optional<UnitType> UnitType::Types::next() {
    if (pos == UnitId::None.Raw()) {
        pos = UnitId::FirstExtendedId;
    }
    if (pos >= end_pos) {
        return Optional<UnitType>();
    } else {
        pos += 1;
        return Optional<UnitType>(UnitType(pos - 1));
    }
}

UnitType::UnitType() : unit_id(UnitId::None)
{
}

bool UnitType::IsValid() const {
    if (unit_id >= Amount())
        return false;
    return unit_id < UnitId::None.Raw() || unit_id >= UnitId::FirstExtendedId.Raw();
}

UpgradeType UnitType::ArmorUpgrade() const
{
    return UpgradeType(UintValue(25));
}

uint32_t UnitType::WhatSound() const
{
    return std::min(UintValue(30), UintValue(31));
}

uint32_t UnitType::WhatSoundCount() const
{
    return abs((int)(UintValue(30) - UintValue(31))) + 1;
}

uint32_t UnitType::AnnoyedSound() const
{
    if (unit_id > UnitId::CommandCenter)
        return 0;
    return std::min(UintValue(32), UintValue(33));
}

uint32_t UnitType::AnnoyedSoundCount() const
{
    if (unit_id > UnitId::CommandCenter)
        return 0;
    return abs((int)(UintValue(32) - UintValue(33))) + 1;
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

bool UnitType::HasNeutralSounds() const
{
    return IsMineralField() || unit_id == VespeneGeyser || IsCritter();
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

uint32_t UnitType::Strength(bool ground) const
{
    Assert(IsValid());
    if (ground)
        return strength[Amount() + unit_id];
    else
        return strength[unit_id];
}

static int GenerateStrength(UnitType unit_id, bool air)
{
    using namespace UnitId;
    UnitType weapon_unit_id;
    WeaponType weapon;
    switch (unit_id.Raw())
    {
        case Larva:
        case Egg:
        case Cocoon:
        case LurkerEgg:
            return 0;
        break;
        case Carrier:
        case Gantrithor:
            weapon_unit_id = Interceptor;
        break;
        case Reaver:
        case Warbringer:
            weapon_unit_id = Scarab;
        break;
        default:
            if (unit_id.Subunit() != UnitId::None)
                weapon_unit_id = unit_id.Subunit();
            else
                weapon_unit_id = unit_id;
        break;
    }
    if (air)
        weapon = weapon_unit_id.AirWeapon();
    else
        weapon = weapon_unit_id.GroundWeapon();
    if (weapon == WeaponId::None)
        return 1;
    // Fixes ai hangs when using zero damage weapons as main weapon
    int strength = bw::FinetuneBaseStrength(unit_id, bw::CalculateBaseStrength(weapon, unit_id));
    return std::max(2, strength);
}

// Fixes ai hangs with some mods (See GenerateStrength comment)
void UnitType::InitializeStrength()
{
    strength = *bw::unit_strength;
    for (UnitType unit_id : UnitType::All())
    {
        int ground_str = GenerateStrength(unit_id, false);
        int air_str = GenerateStrength(unit_id, true);
        // Dunno
        if (air_str == 1 && ground_str > air_str)
            air_str = 0;
        if (ground_str == 1 && air_str > ground_str)
            ground_str = 0;
        strength[unit_id.Raw()] = air_str;
        strength[Amount() + unit_id.Raw()] = ground_str;
    }
}
