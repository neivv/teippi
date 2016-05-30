#include "unit.h"

#include "constants/image.h"
#include "constants/order.h"
#include "constants/unit.h"

#include "ai.h"
#include "iscript.h"
#include "offsets.h"
#include "sound.h"
#include "text.h"
#include "yms.h"

using std::max;

static int GetBuildSpeed()
{
    if (IsCheatActive(Cheats::Operation_Cwal))
        return 10;
    else
        return 1;
}

static void BuildQueueRemove(uint16_t *queue, int first_slot, int slot, Ai::BuildingAi *ai)
{
    int slots_after = slot >= first_slot ?
        4 - (slot - first_slot) :
        4 - (slot + 5 - first_slot);
    for (int i = 0; i < slots_after; i++)
    {
        if (UnitType(queue[slot]) == UnitId::None)
            return;
        int next_slot = (slot + 1) % 5;
        queue[slot] = queue[next_slot];
        if (ai != nullptr)
        {
            ai->train_queue_types[slot] = ai->train_queue_types[next_slot];
            ai->train_queue_values[slot] = ai->train_queue_values[next_slot];
        }
        slot = next_slot;
    }
    queue[slot] = UnitId::None;
    if (ai != nullptr)
    {
        ai->train_queue_types[slot] = 0;
        ai->train_queue_values[slot] = 0;
    }
}

static void BuildQueueCancel(Unit *unit, int slot, ProgressUnitResults *results)
{
    int pos = (unit->current_build_slot + slot) % 5;
    UnitType train_unit_id(unit->build_queue[pos]);
    if (train_unit_id != UnitId::None)
    {
        if (slot == 0 && unit->currently_building != nullptr)
        {
            unit->currently_building->CancelConstruction(results);
            unit->currently_building = nullptr;
        }
        else if (!train_unit_id.IsBuilding())
            bw::RefundFullCost(train_unit_id.Raw(), unit->player);

        BuildQueueRemove(unit->build_queue, unit->current_build_slot, slot, unit->AiAsBuildingAi());
    }
}

static void FinishTrainedUnit(Unit *unit, Unit *parent)
{
    bw::InheritAi2(parent, unit);
    if (unit->Type() == UnitId::NuclearMissile)
        bw::HideUnit(unit);
    else
        bw::RallyUnit(parent, unit);
}

static void FinishTrain(Unit *unit)
{
    FinishTrainedUnit(unit->currently_building, unit);
    BuildQueueRemove(unit->build_queue, unit->current_build_slot, 0, unit->AiAsBuildingAi());
    unit->currently_building = nullptr;
}

static bool ProgressTrain(Unit *unit, ProgressUnitResults *results)
{
    if (unit->currently_building == nullptr)
        return true;
    int good = bw::ProgressBuild(unit->currently_building, bw::GetBuildHpGain(unit->currently_building), 1);
    if (!good)
    {
        BuildQueueCancel(unit, 0, results);
        return true;
    }
    if (unit->currently_building->flags & UnitStatus::Completed)
    {
        FinishTrain(unit);
        return true;
    }
    return false;
}

/// Increases hp and reduces build time (if needed).
/// Might be only be used for protoss buildings.
static void ProgressBuildingConstruction(Unit *unit, int build_speed)
{
    unit->remaining_build_time = std::max((int)unit->remaining_build_time - build_speed, 0);
    bw::SetHp(unit, unit->hitpoints + unit->build_hp_gain * build_speed);
    auto new_shields = unit->shields + unit->build_shield_gain * build_speed;
    unit->shields = std::min(unit->Type().Shields() * 256, new_shields);
}

static bool ProgressProtossConstruction(Unit *unit, ProgressUnitResults *results)
{
    if (unit->remaining_build_time == 0)
    {
        return true;
    }
    else
    {
        ProgressBuildingConstruction(unit, GetBuildSpeed());
        return false;
    }
}

static void FinishProtossBuilding(Unit *unit, ProgressUnitResults *results)
{
    bw::FinishUnit_Pre(unit);
    bw::FinishUnit(unit);
    bw::CheckUnstack(unit);
    if (unit->flags & UnitStatus::Disabled)
    {
        unit->SetIscriptAnimation(Iscript::Animation::Disable, true, "FinishProtossBuilding", results);
    }
    // This heals a bit if the buidling was damaged but is otherwise pointless
    ProgressBuildingConstruction(unit, GetBuildSpeed());
}

void Unit::CancelTrain(ProgressUnitResults *results)
{
    for (int i = 0; i < 5; i++)
    {
        BuildQueueCancel(this, 0, results);
    }
    current_build_slot = 0;
}

void Unit::Order_Train(ProgressUnitResults *results)
{
    if (IsDisabled())
        return;
    // Some later added hackfix
    if (Type().Race() == Race::Zerg && Type() != UnitId::InfestedCommandCenter)
        return;
    switch (secondary_order_state)
    {
        case 0:
        case 1:
        {
            int train_unit_id = build_queue[current_build_slot];
            if (UnitType(train_unit_id) == UnitId::None)
            {
                IssueSecondaryOrder(OrderId::Nothing);
                SetIscriptAnimation(Iscript::Animation::WorkingToIdle, true, "Unit::Order_Train", results);
            }
            else
            {
                currently_building = bw::BeginTrain(this, train_unit_id, secondary_order_state == 0);
                if (currently_building == nullptr)
                {
                    secondary_order_state = 1;
                }
                else
                {
                    SetIscriptAnimation(Iscript::Animation::Working, true, "Unit::Order_Train", results);
                    secondary_order_state = 2;
                }
            }
        }
        break;
        case 2:
        {
            bool finished = ProgressTrain(this, results);
            if (finished)
            {
                secondary_order_state = 0;
                RefreshUi();
            }
        }
        break;
    }
}

void Unit::Order_ProtossBuildSelf(ProgressUnitResults *results)
{
    switch (order_state)
    {
        case 0:
        {
            bool finished = ProgressProtossConstruction(this, results);
            if (finished)
            {
                SetIscriptAnimation(Iscript::Animation::Special1, true, "Order_ProtossBuildSelf", results);
                bw::PlaySound(Sound::ProtossBuildingFinishing, this, 1, 0);
                order_state = 1;
            }
        }
        break;
        case 1:
            if (order_signal & 0x1)
            {
                order_signal &= ~0x1;
                bw::ReplaceSprite(sprite->Type().Image().Raw(), 0, sprite.get());
                Image *image = sprite->main_image;
                UnitIscriptContext ctx(this, results, "Order_ProtossBuildSelf", MainRng(), false);
                // Bw actually has iscript header hardcoded as 193
                image->ExternalAnimation(&ctx, ImageId::WarpTexture, Iscript::Animation::Init);
                image->SetDrawFunc(Image::UseWarpTexture, image->drawfunc_param);
                // Why?
                image->iscript.ProgressFrame(&ctx, image);
                order_state = 2;
            }
        break;
        case 2:
            if (order_signal & 0x1)
            {
                order_signal &= ~0x1;
                // Wait, why again?
                bw::ReplaceSprite(sprite->Type().Image().Raw(), 0, sprite.get());
                SetIscriptAnimation(Iscript::Animation::WarpIn, true, "Order_ProtossBuildSelf", results);
                order_state = 3;
            }
        break;
        case 3:
            if (order_signal & 0x1)
            {
                order_signal &= ~0x1;
                FinishProtossBuilding(this, results);
            }
        break;
    }
}

static void MutateExtractor(Unit *unit, ProgressUnitResults *results)
{
    Unit *extractor = bw::BeginGasBuilding(UnitId::Extractor, unit);
    if (extractor != nullptr)
    {
        bw::InheritAi(unit, extractor);
        unit->Remove(results);
        bw::StartZergBuilding(extractor);
        bw::AddOverlayBelowMain(extractor->sprite.get(), ImageId::VespeneGeyserUnderlay, 0, 0, 0);
    }
    else
    {
        if (unit->sprite->last_overlay->drawfunc == Image::Shadow)
        {
            unit->sprite->last_overlay->SetOffset(unit->sprite->last_overlay->x_off, 7);
            unit->sprite->last_overlay->flags |= 0x4;
        }
        unit->unk_move_waypoint = unit->sprite->position;
        unit->PrependOrderTargetingNothing(OrderId::ResetCollision1);
        unit->DoNextQueuedOrder();
    }
}

static int GetBuildingPlacementError(Unit *builder, int player, x32 x_tile, y32 y_tile, UnitType unit_id, bool check_vision)
{
    int width = unit_id.PlacementBox().width / 32;
    int height = unit_id.PlacementBox().height / 32;
    // If units.dat placement box < 32, bw allows to place the unit anywhere
    // However, ai code calls this function with past-limit x/y, so we need to check both
    // x_tile < width && x_tile + w_tiles <= width
    int map_width = *bw::map_width_tiles;
    int map_height = *bw::map_height_tiles;
    if (x_tile < 0 || y_tile < 0 || x_tile >= map_width || y_tile >= map_height ||
            x_tile + width > map_width || y_tile + height > map_height)
    {
        return 0x3;
    }
    if (builder && !(builder->flags & (UnitStatus::Building | UnitStatus::Air)))
    {
        if (!bw::CanWalkHere(builder, x_tile * 32, y_tile * 32))
            return 0x7;
    }
    if (unit_id.IsGasBuilding())
        return bw::GetGasBuildingPlacementState(x_tile, y_tile, player, check_vision);
    if (unit_id.Flags() & UnitFlags::RequiresPsi)
        return bw::GetPsiPlacementState(x_tile, y_tile, unit_id.Raw(), player);
    return 0;
}

int UpdateBuildingPlacementState(Unit *builder, int player, x32 x_tile, y32 y_tile, UnitType unit_id,
    int placement_state_entry, bool check_vision, bool also_invisible, bool without_vision)
{
    x_tile &= 0xffff;
    y_tile &= 0xffff;
    player &= 0xff;
    int error = GetBuildingPlacementError(builder, player, x_tile, y_tile, unit_id, check_vision);
    if (error || unit_id.IsGasBuilding())
    {
        uint8_t *placement_data = &*bw::building_placement_state + placement_state_entry * 0x30;
        memset(placement_data, error, 0x30);
        return error;
    }
    int width = unit_id.PlacementBox().width / 32;
    int height = unit_id.PlacementBox().height / 32;
    unsigned int size_wh = height << 16 | width;
    int specific_state;
    if (builder && builder->Type() == UnitId::NydusCanal)
    {
        specific_state = bw::UpdateNydusPlacementState(player,
                                                       x_tile,
                                                       y_tile,
                                                       size_wh,
                                                       placement_state_entry,
                                                       check_vision);
    }
    else if (unit_id.Flags() & UnitFlags::RequiresCreep)
    {
        specific_state = bw::UpdateCreepBuildingPlacementState(player,
                                                               x_tile,
                                                               y_tile,
                                                               size_wh,
                                                               placement_state_entry,
                                                               check_vision,
                                                               without_vision);
    }
    else
    {
        specific_state = bw::UpdateBuildingPlacementState_MapTileFlags(x_tile,
                                                                       y_tile,
                                                                       player,
                                                                       size_wh,
                                                                       placement_state_entry,
                                                                       check_vision,
                                                                       unit_id.Raw());
    }

    if (*bw::ai_building_placement_hack)
        builder = *bw::ai_building_placement_hack;
    int generic_state = bw::UpdateBuildingPlacementState_Units(builder,
                                                               x_tile,
                                                               y_tile,
                                                               player,
                                                               unit_id.Raw(),
                                                               size_wh,
                                                               placement_state_entry,
                                                               ~unit_id.Flags() & UnitFlags::Addon,
                                                               also_invisible,
                                                               without_vision);
    return max(generic_state, specific_state);
}

static bool BuildingCreationCheck(Unit *builder, UnitType building, const Point &pos)
{
    int player = builder->player;
    int x_tile = (pos.x - (building.PlacementBox().width / 2)) / 32;
    int y_tile = (pos.y - (building.PlacementBox().height / 2)) / 32;
    int error = UpdateBuildingPlacementState(builder, player, x_tile, y_tile, building,
                                             0, false, true, true);
    if (error != 0)
    {
        bw::BuildErrorMessage(builder, String::CantBuildHere);
        return false;
    }

    bw::player_build_minecost[player] = building.MineralCost();
    bw::player_build_gascost[player] = building.GasCost();
    if (bw::CheckSupplyForBuilding(player, building.Raw(), 1) == 0)
        return false;

    if (bw::minerals[player] < bw::player_build_minecost[player])
    {
        bw::ShowInfoMessage(String::NotEnoughMinerals, Sound::NotEnoughMinerals + *bw::player_race, player);
        return false;
    }
    else if (bw::gas[player] < bw::player_build_gascost[player])
    {
        bw::ShowInfoMessage(String::NotEnoughGas, Sound::NotEnoughGas + *bw::player_race, player);
        return false;
    }
    return true;
}

void Unit::Order_DroneMutate(ProgressUnitResults *results)
{
    if (sprite->position != order_target_pos)
        return;

    UnitType building(build_queue[current_build_slot]);
    if (!BuildingCreationCheck(this, building, sprite->position))
    {
        if (sprite->last_overlay->drawfunc == Image::Shadow)
            sprite->last_overlay->SetOffset(sprite->last_overlay->x_off, 7);
        bw::PrepareDrawSprite(sprite.get()); // ?
        PrependOrderTargetingNothing(OrderId::ResetCollision1);
        DoNextQueuedOrder();
    }
    else
    {
        bw::ReduceBuildResources(player);
        Unit *powerup = worker.powerup;
        if (powerup)
            bw::DropPowerup(this);

        if (building.Raw() == UnitId::Extractor)
            MutateExtractor(this, results);
        else
            bw::MutateBuilding(this, building.Raw());

        if (powerup)
            bw::MoveUnit(powerup, powerup->powerup.origin_point.x, powerup->powerup.origin_point.y);
        return;
    }
}
