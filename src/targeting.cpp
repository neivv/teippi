#include "targeting.h"

#include "constants/order.h"
#include "constants/tech.h"
#include "constants/weapon.h"

#include "dat.h"
#include "unit.h"
#include "offsets.h"
#include "selection.h"
#include "dialog.h"
#include "pathing.h"
#include "order.h"
#include "tech.h"
#include "bullet.h"
#include "sprite.h"
#include "strings.h"
#include "text.h"
#include "commands.h"
#include "unitsearch.h"
#include "game.h"

Unit *FindUnitAtPoint(int x, int y)
{
    Unit **units = bw::GetClickableUnits(x, y);
    Unit *picked = nullptr;
    uint32_t picked_z = -1;
    int area = INT_MAX;
    for (Unit *unit = *units++; unit != nullptr; unit = *units++)
    {
        if (unit->Type().Flags() & UnitFlags::Subunit)
            unit = unit->subunit;
        if (!unit->CanLocalPlayerSelect() || !unit->Type().IsClickable())
            continue;
        if (picked)
        {
            uint32_t new_z = unit->sprite->GetZCoord();
            bool is_above_picked = false;
            if (picked_z < new_z)
                is_above_picked = true;
            else if (picked_z == new_z)
                is_above_picked = picked->sprite->id < unit->sprite->id;
            if (is_above_picked)
            {
                if (bw::IsClickablePixel(unit, x, y) ||
                        (unit->subunit != nullptr && bw::IsClickablePixel(unit->subunit, x, y)))
                {
                    picked_z = new_z;
                    picked = unit;
                    // Bw uses sprite w/h here <.<
                    area = unit->Type().PlacementBox().Area();
                }
            }
            else
            {
                if (!bw::IsClickablePixel(picked, x, y) &&
                        (picked->subunit == nullptr || !bw::IsClickablePixel(picked->subunit, x, y)))
                {
                    int new_area = unit->Type().PlacementBox().Area();
                    if (new_area < area)
                    {
                        picked_z = new_z;
                        picked = unit;
                        area = new_area;
                    }
                }
            }
        }
        else
        {
            picked_z = unit->sprite->GetZCoord();
            picked = unit;
            area = unit->Type().PlacementBox().Area();
        }
    }
    unit_search->PopResult();
    return picked;
}

void ClearOrderTargeting()
{
    if (*bw::is_placing_building)
    {
        bw::MarkPlacementBoxAreaDirty();
        bw::EndBuildingPlacement();
    }
    if (*bw::is_targeting)
    {
        bw::EndTargeting();
    }
}

void ClearOrderTargetingIfNeeded()
{
    if (ShouldClearOrderTargeting())
    {
        ClearOrderTargeting();
    }
}

void SendRightClickCommand(Unit *target, uint16_t x, uint16_t y, UnitType fow_unit, bool queued)
{
    uint8_t cmd[10];
    cmd[0] = commands::RightClick;
    *(int16_t *)(cmd + 1) = x;
    *(int16_t *)(cmd + 3) = y;
    cmd[9] = queued ? 1 : 0;
    if (target)
    {
        *(uint32_t *)(cmd + 5) = target->lookup_id;
    }
    else if (fow_unit != UnitId::None)
    {
        *(uint16_t *)(cmd + 5) = fow_unit.Raw();
        cmd[9] |= 0x40;
    }
    else
    {
        cmd[9] |= 0x80;
    }
    bw::SendCommand(cmd, 10);
}

void Command_RightClick(const uint8_t *buf)
{
    uint16_t x = *(uint16_t *)(buf + 1);
    uint16_t y = *(uint16_t *)(buf + 3);
    uint8_t queued = *(uint8_t *)(buf + 9);
    if (x < *bw::map_width && y < *bw::map_height)
    {
        MovementGroup formation;
        formation.target[0] = x;
        formation.target[1] = y;
        bw::PrepareFormationMovement(&formation, true);

        Unit *target;
        UnitType fow_unit = UnitId::None;
        if (queued & 0x80) // target ground
        {
            target = nullptr;
            queued ^= 0x80;
        }
        else if (queued & 0x40) // target fow
        {
            queued ^= 0x40;
            if (!((*bw::map_tile_flags)[*bw::map_width_tiles * (y / 32) + (x / 32)] & (1 << (*bw::command_user)))) // If there's vis due to latency
                target = bw::FindFowUnit(x, y, *(uint16_t *)(buf + 5));
            else
            {
                formation.current_target[0] = formation.target[0];
                formation.current_target[1] = formation.target[1];
                fow_unit = UnitType(*(uint16_t *)(buf + 5));
                target = nullptr;
            }
        }
        else // target unit
        {
            target = Unit::FindById(*(uint32_t *)(buf + 5));
            if (target && target->IsDying())
                target = nullptr;
        }


        ResetSelectionIter();
        for (Unit *unit = bw::NextCommandedUnit(); unit; unit = bw::NextCommandedUnit())
        {
            int action = unit->GetRClickAction();

            if (unit->flags & UnitStatus::Hallucination && (action == RightClickAction::Harvest || action == RightClickAction::HarvestAndRepair))
                action = RightClickAction::MoveAndAttack;

            OrderType order = OrderType(bw::GetRightClickOrder[action](unit, x, y, &target, fow_unit));

            if (order == OrderId::RallyPointUnit || order == OrderId::RallyPointTile)
            {
                if (unit->Type().HasRally() && *bw::command_user == unit->player)
                {
                    if (order == OrderId::RallyPointUnit)
                    {
                        unit->rally.unit = target;
                        unit->rally.position = target->sprite->position;
                    }
                    else
                    {
                        unit->rally.unit = nullptr;
                        unit->rally.position = Point(x, y);
                    }
                }
            }
            else if (order != OrderId::Nothing && target != unit)
            {
                if (order == OrderId::Attack)
                {
                    Unit *subunit = unit->subunit;
                    if (subunit != nullptr)
                    {
                        OrderType subunit_order(subunit->Type().AttackUnitOrder());
                        subunit->TargetedOrder(subunit_order, target, Point(0, 0), UnitId::None, queued);
                    }
                    order = unit->Type().AttackUnitOrder();
                }
                if (bw::CanIssueOrder(unit, order, *bw::command_user) == 1)
                {
                    if (target != nullptr)
                    {
                        unit->TargetedOrder(order, target, Point(0, 0), UnitId::None, queued);
                    }
                    else
                    {
                        if (fow_unit == UnitId::None)
                        {
                            bw::GetFormationMovementTarget(unit, &formation);
                        }
                        Point pos(formation.current_target[0], formation.current_target[1]);
                        unit->TargetedOrder(order, nullptr, pos, fow_unit, queued);
                    }
                    unit->order_flags |= 2;
                }
            }
        }
    }
}

void Command_Targeted(const uint8_t *buf)
{
    uint16_t x = *(uint16_t *)(buf + 1);
    uint16_t y = *(uint16_t *)(buf + 3);
    uint8_t queued = *(uint8_t *)(buf + 9);
    OrderType order = OrderType(*(uint8_t *)(buf + 10));
    if (x >= *bw::map_width || y >= *bw::map_height || !order.IsTargetable())
        return;

    MovementGroup formation;
    formation.target[0] = x;
    formation.target[1] = y;
    bw::PrepareFormationMovement(&formation, order.TerrainClip());

    Unit *target;
    UnitType fow_unit = UnitId::None;
    if (queued & 0x80) // target ground
    {
        target = nullptr;
        queued ^= 0x80;
    }
    else if (queued & 0x40) // target fow
    {
        queued ^= 0x40; // Won't do vis update like rclick
        formation.current_target[0] = formation.target[0];
        formation.current_target[1] = formation.target[1];
        fow_unit = UnitType(*(uint16_t *)(buf + 5));
        target = nullptr;
    }
    else // target unit
    {
        target = Unit::FindById(*(uint32_t *)(buf + 5));
        if (target && target->IsDying())
            target = nullptr;
    }

    TechType spell_tech = order.EnergyTech();

    ResetSelectionIter();
    for (Unit *unit = bw::NextCommandedUnit(); unit; unit = bw::NextCommandedUnit())
    {
        if (spell_tech != TechId::None)
        {
            if (bw::CanUseTech(spell_tech.Raw(), unit, *bw::command_user) != 1)
                continue;
        }
        else
        {
            if (bw::CanIssueOrder(unit, order.Raw(), *bw::command_user) != 1)
                continue;
        }

        if (target == unit && !unit->CanTargetSelf(order))
            continue;
        if (!unit->CanUseTargetedOrder(order))
            continue;
        if (unit->OrderType() == OrderId::NukeLaunch && order != OrderId::Die)
            continue;

        OrderType current_order = order;
        OrderType subunit_order = OrderId::Nothing;

        if (unit->unit_id == UnitId::Medic && (order == OrderId::Attack || order == OrderId::AttackMove))
            current_order = OrderId::HealMove;
        if (current_order == OrderId::Attack)
        {
            if (target)
                current_order = unit->Type().AttackUnitOrder();
            else
                current_order = unit->Type().AttackMoveOrder();
            if (current_order == OrderId::Nothing)
                continue;

            if (unit->HasSubunit())
            {
                if (target)
                    subunit_order = unit->subunit->Type().AttackUnitOrder();
                else
                    subunit_order = unit->subunit->Type().AttackMoveOrder();
            }
        }
        if (unit->Type().HasRally() && *bw::command_user == unit->player)
        {
            if (current_order == OrderId::RallyPointUnit)
            {
                Unit *rally_target = target;
                if (!rally_target)
                    rally_target = unit;
                unit->rally.unit = rally_target;
                unit->rally.position = rally_target->sprite->position;
                continue;
            }
            else if (current_order == OrderId::RallyPointTile)
            {
                unit->rally.unit = nullptr;
                unit->rally.position = Point(x, y);
                continue;
            }
        }

        if (target != nullptr)
        {
            unit->TargetedOrder(current_order, target, Point(0, 0), UnitId::None, queued);
            if (subunit_order != OrderId::Nothing)
                unit->subunit->TargetedOrder(subunit_order, target, Point(0, 0), UnitId::None, queued);
        }
        else
        {
            if (fow_unit == UnitId::None)
            {
                bw::GetFormationMovementTarget(unit, &formation);
            }
            Point pos(formation.current_target[0], formation.current_target[1]);
            unit->TargetedOrder(current_order, nullptr, pos, fow_unit, queued);
            if (subunit_order != OrderId::Nothing) // Is this even possible?
            {
                unit->subunit->TargetedOrder(subunit_order, nullptr, pos, fow_unit, queued);
            }
        }
    }
}

void SendTargetedOrderCommand(uint8_t order, int x, int y, Unit *target, UnitType fow_unit, uint8_t queued)
{
    uint8_t cmd[11];
    cmd[0] = commands::TargetedOrder;
    *(int16_t *)(cmd + 1) = x;
    *(int16_t *)(cmd + 3) = y;
    if (target)
    {
        *(uint32_t *)(cmd + 5) = target->lookup_id;
    }
    else if (fow_unit != UnitId::None)
    {
        *(uint16_t *)(cmd + 5) = fow_unit;
        queued |= 0x40;
    }
    else
    {
        queued |= 0x80;
    }
    cmd[9] = queued;
    cmd[10] = order;
    bw::SendCommand(cmd, 11);
}

void Test_SendTargetedOrderCommand(uint8_t order, const Point &pos, Unit *target, UnitType fow_unit, uint8_t queued)
{
    if (game_tests != nullptr) {
        SendTargetedOrderCommand(order, pos.x, pos.y, target, fow_unit, queued);
    }
}

void GetAttackErrorMessage(Unit *target, WeaponType targeting_weapon, int16_t *error)
{
    if (target && target->IsInvincible())
    {
        if (target->stasis_timer)
            *error = String::Error_StasisTarget;
        else if (target->flags & UnitStatus::Building)
            *error = String::Error_InvalidTargetStructure;
        else
            *error = String::Error_InvalidTarget;
    }
    else
    {
        if (*error == -1)
            *error = String::Error_UnableToAttackTarget;
        if (targeting_weapon != WeaponId::None)
            *error = targeting_weapon.ErrorMessage();
    }
}

void DoTargetedCommand(int x, int y, Unit *target, UnitType fow_unit)
{
    if (!*bw::is_targeting)
        return;
    bw::EndTargeting();

    int16_t error = -1;
    OrderType order;
    bool can_attack = false, can_cast_spell = false, has_energy = false;

    if (target)
        order = OrderType(*bw::unit_order_id);
    else if (fow_unit != UnitId::None)
        order = OrderType(*bw::obscured_unit_order_id);
    else
        order = OrderType(*bw::ground_order_id);

    WeaponType targeting_weapon = order.Weapon();
    bool weapon_targeting = order.UseWeaponTargeting();
    int energy = 0;
    TechType tech = order.EnergyTech();
    if (tech != TechId::None)
        energy = tech.EnergyCost() * 256;

    if (order.Obscured() == OrderId::None)
        fow_unit = UnitId::None;

    for (unsigned i = 0; i < Limits::Selection; i++)
    {
        Unit *unit = bw::client_selection_group[i];
        if (!unit)
            break;
        if (unit->IsDisabled())
            continue;
        if (weapon_targeting)
        {
            if (targeting_weapon == WeaponId::None)
            {
                if (!target)
                {
                    if (unit->Type() == UnitId::InfestedTerran || unit->CanAttackFowUnit(fow_unit))
                        can_attack = true;
                }
                else
                {
                    if (unit->GetRClickAction() == RightClickAction::Attack) // sieged tank, towers, etc.
                    {
                        if (bw::IsOutOfRange(unit, target))
                        {
                            error = String::Error_OutOfRange;
                            continue;
                        }
                        else if (bw::IsTooClose(unit, target))
                        {
                            error = String::Error_TooClose;
                            continue;
                        }
                    }
                    can_attack |= unit->CanAttackUnit(target, true);
                }
            }
            else
            {
                if (!bw::CanHitUnit(target, unit, targeting_weapon.Raw()) &&
                        (fow_unit.Raw() >= UnitId::None || fow_unit.Flags() & UnitFlags::Invincible))
                {
                    continue;
                }
                if (!bw::NeedsMoreEnergy(unit, energy))
                    has_energy = true;
                can_attack = true;
            }

        }
        else if (tech != TechId::None)
        {
            if (bw::CanTargetSpell(unit, x, y, &error, tech.Raw(), target, fow_unit.Raw()))
            {
                if (!bw::NeedsMoreEnergy(unit, energy))
                    has_energy = true;
                can_cast_spell = true;
            }
        }
        else if (fow_unit == UnitId::None)
        {
            bw::CanTargetOrder(unit, target, order.Raw(), &error); // Not checking anything, just to get error
        }
        else
        {
            bw::CanTargetOrderOnFowUnit(unit, fow_unit, order.Raw(), &error); // Same
        }
    }

    Unit *unit = *bw::primary_selected;

    if (weapon_targeting)
    {
        if (!can_attack || error != -1)
        {
            GetAttackErrorMessage(target, targeting_weapon, &error);
            bw::ShowInfoMessage(error, Sound::Error_Zerg + unit->Type().Race(), unit->player);
            if (targeting_weapon == WeaponId::None)
                SendTargetedOrderCommand(order, x, y, target, fow_unit, *bw::is_queuing_command); // So it'll move there anyways
            return;
        }
        else if (energy && !has_energy)
        {
            int race = unit->Type().Race();
            bw::ShowInfoMessage(String::NotEnoughEnergy + race, Sound::NotEnoughEnergy + race, unit->player);
            return;
        }
    }
    else if (energy)
    {
        if (!can_cast_spell)
        {
            if (error == -1)
                error = String::Error_InvalidTarget;
            bw::ShowErrorMessage((*bw::stat_txt_tbl)->GetTblString(error), *bw::command_user, unit);
            return;
        }
        else if (/*energy && */!has_energy)
        {
            int race = unit->Type().Race();
            bw::ShowInfoMessage(String::NotEnoughEnergy + race, Sound::NotEnoughEnergy + race, unit->player);
            return;
        }
    }
    if (error != -1)
    {
        bw::ShowErrorMessage((*bw::stat_txt_tbl)->GetTblString(error), unit->player, unit);
    }
    else
    {
        bw::PlayYesSoundAnim(*bw::primary_selected);
        SendTargetedOrderCommand(order, x, y, target, fow_unit, *bw::is_queuing_command);
    }
}

void GameScreenLClickEvent_Targeting(Event *event)
{
    if (bw::IsOutsideGameScreen(event->x, event->y))
        return;

    int x = event->x + *bw::screen_x;
    int y = event->y + *bw::screen_y;
    Unit *target = FindUnitAtPoint(x, y);
    Sprite *fow;
    if (!target)
        fow = ShowCommandResponse(x, y, nullptr);
    else
        fow = ShowCommandResponse(x, y, target->sprite.get());

    if (fow)
        DoTargetedCommand(x, y, target, UnitType(fow->index));
    else
        DoTargetedCommand(x, y, target, UnitId::None);

    bw::SetCursorSprite(0);
}

void GameScreenRClickEvent(Event *event)
{
    if (bw::IsOutsideGameScreen(event->x, event->y) || bw::client_selection_group2[0] == nullptr)
        return;
    Unit *highest_ranked = *bw::primary_selected;
    if (highest_ranked == nullptr || !bw::CanControlUnit(highest_ranked))
        return;

    unsigned i;
    for (i = 0; i < Limits::Selection; i++)
    {
        Unit *unit = bw::client_selection_group[i];
        if (unit == nullptr)
            return;
        if (unit->DoesAcceptRClickCommands())
            break;
    }
    if (i == Limits::Selection)
        return;

    int x = event->x + *bw::screen_x;
    int y = event->y + *bw::screen_y;
    Unit *target = FindUnitAtPoint(x, y);
    if (target == highest_ranked && *bw::client_selection_count == 1)
    {
        if (~highest_ranked->flags & UnitStatus::Building)
            return;
        if (!highest_ranked->Type().HasRally())
            return;
    }
    else if (target == nullptr)
    {
        for (i = 0; i < Limits::Selection; i++)
        {
            Unit *unit = bw::client_selection_group[i];
            if (unit == nullptr)
                return;
            if (unit->CanRClickGround())
                break;
        }
        if (i == Limits::Selection)
            return;
    }
    if (target == nullptr)
    {
        Sprite *fow = ShowCommandResponse(x, y, 0);
        if (fow)
            SendRightClickCommand(0, x, y, UnitType(fow->index), *bw::is_queuing_command);
        else
            SendRightClickCommand(0, x, y, UnitId::None, *bw::is_queuing_command);
    }
    else
    {
        ShowCommandResponse(x, y, target->sprite.get());
        SendRightClickCommand(target, x, y, UnitId::None, *bw::is_queuing_command);
    }

    if (bw::ShowRClickErrorIfNeeded(target) == 1)
        bw::PlayYesSoundAnim(highest_ranked);
}
