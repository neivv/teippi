#include "dat.h"

#include "constants/order.h"
#include "constants/tech.h"
#include "constants/upgrade.h"
#include "constants/weapon.h"

#include "strings.h"
#include "upgrade.h"

GrpSprite **ImageType::grp_array = nullptr;
uint32_t ImageType::grp_array_size = 0;
int8_t **ImageType::overlays = nullptr;
int8_t **ImageType::shield_overlay = nullptr;

OrderType::OrderType() : order_id(OrderId::None.Raw())
{
}

bool OrderType::CanQueueOn() const
{
    switch (order_id)
    {
        case OrderId::Guard:
        case OrderId::PlayerGuard:
        case OrderId::Nothing:
        case OrderId::TransportIdle:
        case OrderId::Patrol:
        case OrderId::Medic:
            return false;
        default:
            return true;
    }
}

bool OrderType::IsTargetable() const
{
    using namespace OrderId;

    switch (order_id)
    {
        case Die: case BeingInfested: case SpiderMine: case DroneStartBuild: case InfestMine4: case BuildTerran:
        case BuildProtoss1: case BuildProtoss2: case ConstructingBuilding: case PlaceAddon: case BuildNydusExit:
        case Land: case LiftOff: case DroneLiftOff: case HarvestObscured: case MoveToGas:
        case WaitForGas: case HarvestGas: case MoveToMinerals: case WaitForMinerals: case MiningMinerals:
        case MineralHarvestInterrupted: case StopHarvest: case CtfCop2:
            return false;
        default:
            return true;
    }
}

WeaponType OrderType::Weapon() const
{
    return WeaponType(UintValue(13));
}

TechType OrderType::EnergyTech() const
{
    return TechType(UintValue(14));
}

OrderType OrderType::Obscured() const
{
    return OrderType(UintValue(18));
}

const char *TechType::Name() const
{
    return (*bw::stat_txt_tbl)->GetTblString(Label());
}

int8_t *ImageType::ShieldOverlay() const
{
    Assert(image_id < Amount());
    return shield_overlay[image_id];
}

int8_t *ImageType::Overlay(int overlay_type) const
{
    Assert(image_id < Amount());
    return overlays[overlay_type * Amount() + image_id];
}

WeaponType::WeaponType() : weapon_id(WeaponId::None.Raw())
{
}

UpgradeType WeaponType::Upgrade() const
{
    return UpgradeType(UintValue(6));
}

bool WeaponType::WorksUnderDisruptionWeb() const
{
    using namespace WeaponId;
    switch (weapon_id)
    {
        case SpiderMine:
        case Lockdown:
        case EmpShockwave:
        case Irradiate:
        case Venom:
        case HeroVenom:
        case Suicide:
        case Parasite:
        case SpawnBroodlings:
        case Ensnare:
        case DarkSwarm:
        case Plague:
        case Consume:
        case PsiAssault:
        case HeroPsiAssault:
        case Scarab:
        case StasisField:
        case PsiStorm:
        case Restoration:
        case MindControl:
        case Feedback:
        case OpticalFlare:
        case Maelstrom:
            return true;
        default:
            return false;
    }
}
