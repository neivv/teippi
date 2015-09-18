#include "targeting.h"

#include "unit.h"
#include "patchmanager.h"
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

Unit *FindUnitAtPoint(int x, int y)
{
    Unit **units = GetClickableUnits(x, y);
    Unit *picked = nullptr;
    uint32_t picked_z = -1;
    int area = INT_MAX;
    for (Unit *unit = *units++; unit != nullptr; unit = *units++)
    {
        if (units_dat_flags[unit->unit_id] & UnitFlags::Subunit)
            unit = unit->subunit;
        if (!unit->CanLocalPlayerSelect() || !Unit::IsClickable(unit->unit_id))
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
                if (IsClickablePixel(unit, x, y) || (unit->subunit && IsClickablePixel(unit->subunit, x, y)))
                {
                    picked_z = new_z;
                    picked = unit;
                    // Bw uses sprite w/h here <.<
                    area = units_dat_placement_box[unit->unit_id][0] * units_dat_placement_box[unit->unit_id][1];
                }
            }
            else
            {
                if (!IsClickablePixel(picked, x, y) && (!picked->subunit || !IsClickablePixel(picked->subunit, x, y)))
                {
                    int new_area = units_dat_placement_box[unit->unit_id][0] * units_dat_placement_box[unit->unit_id][1];
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
            area = units_dat_placement_box[unit->unit_id][0] * units_dat_placement_box[unit->unit_id][1];
        }
    }
    unit_search->PopResult();
    return picked;
}

void ClearOrderTargeting()
{
    if (*bw::is_placing_building)
    {
        MarkPlacementBoxAreaDirty();
        EndBuildingPlacement();
    }
    if (*bw::is_targeting)
    {
        EndTargeting();
    }
}

void ClearOrderTargetingIfNeeded()
{
    if (ShouldClearOrderTargeting())
    {
        ClearOrderTargeting();
    }
}

void SendRightClickCommand(Unit *target, uint16_t x, uint16_t y, uint16_t fow_unit, uint8_t queued)
{
    uint8_t cmd[10];
    cmd[0] = commands::RightClick;
    *(int16_t *)(cmd + 1) = x;
    *(int16_t *)(cmd + 3) = y;
    if (target)
    {
        *(uint32_t *)(cmd + 5) = target->lookup_id;
    }
    else if (fow_unit != Unit::None)
    {
        *(uint16_t *)(cmd + 5) = fow_unit;
        queued |= 0x40;
    }
    else
    {
        queued |= 0x80;
    }
    cmd[9] = queued;
    SendCommand(cmd, 10);
}

void Command_RightClick(uint8_t *buf)
{
    uint16_t x = *(uint16_t *)(buf + 1);
    uint16_t y = *(uint16_t *)(buf + 3);
    uint8_t queued = *(uint8_t *)(buf + 9);
    if (x < *bw::map_width && y < *bw::map_height)
    {
        MovementGroup formation;
        formation.target[0] = x;
        formation.target[1] = y;
        PrepareFormationMovement(&formation, true);

        Unit *target;
        int fow_unit = Unit::None;
        if (queued & 0x80) // target ground
        {
            target = nullptr;
            queued ^= 0x80;
        }
        else if (queued & 0x40) // target fow
        {
            queued ^= 0x40;
            if (!((*bw::map_tile_flags)[*bw::map_width_tiles * (y / 32) + (x / 32)] & (1 << (*bw::command_user)))) // If there's vis due to latency
                target = FindFowUnit(x, y, *(uint16_t *)(buf + 5));
            else
            {
                formation.current_target[0] = formation.target[0];
                formation.current_target[1] = formation.target[1];
                fow_unit = *(uint16_t *)(buf + 5);
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
        for (Unit *unit = NextCommandedUnit(); unit; unit = NextCommandedUnit())
        {
            int action = unit->GetRClickAction();

            if (unit->flags & UnitStatus::Hallucination && (action == RightClickAction::Harvest || action == RightClickAction::HarvestAndRepair))
                action = RightClickAction::MoveAndAttack;

            int order = bw::GetRightClickOrder[action](unit, x, y, &target, fow_unit);

            if (order == Order::RallyPointUnit || order == Order::RallyPointTile)
            {
                if (unit->HasRally() && *bw::command_user == unit->player)
                {
                    if (order == Order::RallyPointUnit)
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
            else if (order != Order::Nothing && target != unit)
            {
                if (order == Order::Attack)
                {
                    Unit *subunit = unit->subunit;
                    if (subunit)
                        IssueOrderTargetingUnit(subunit, target, queued, units_dat_attack_unit_order[subunit->unit_id]);

                    order = units_dat_attack_unit_order[unit->unit_id];
                }
                if (CanIssueOrder(unit, order, *bw::command_user) == 1)
                {
                    if (target)
                    {
                        IssueOrderTargetingUnit(unit, target, queued, order);
                    }
                    else
                    {
                        if (fow_unit == Unit::None)
                        {
                            GetFormationMovementTarget(unit, &formation);
                        }
                        IssueOrderTargetingGround2(unit, order, formation.current_target[0], formation.current_target[1], fow_unit, queued);
                    }
                    unit->order_flags |= 2;
                }
            }
        }
    }
}

void Command_Targeted(uint8_t *buf)
{
    uint16_t x = *(uint16_t *)(buf + 1);
    uint16_t y = *(uint16_t *)(buf + 3);
    uint8_t queued = *(uint8_t *)(buf + 9);
    uint8_t order = *(uint8_t *)(buf + 10);
    if (x >= *bw::map_width || y >= *bw::map_height || !IsTargetableOrder(order))
        return;

    MovementGroup formation;
    formation.target[0] = x;
    formation.target[1] = y;
    PrepareFormationMovement(&formation, orders_dat_terrain_clip[order]);

    Unit *target;
    int fow_unit = Unit::None;
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
        fow_unit = *(uint16_t *)(buf + 5);
        target = nullptr;
    }
    else // target unit
    {
        target = Unit::FindById(*(uint32_t *)(buf + 5));
        if (target && target->IsDying())
            target = nullptr;
    }

    int spell_tech = orders_dat_energy_tech[order];

    ResetSelectionIter();
    for (Unit *unit = NextCommandedUnit(); unit; unit = NextCommandedUnit())
    {
        if (spell_tech != Tech::None)
        {
            if (CanUseTech(spell_tech, unit, *bw::command_user) != 1)
                continue;
        }
        else
        {
            if (CanIssueOrder(unit, order, *bw::command_user) != 1)
                continue;
        }

        if (target == unit && !unit->CanTargetSelf(order))
            continue;
        if (!unit->CanUseTargetedOrder(order))
            continue;
        if (unit->order == Order::NukeLaunch && order != Order::Die)
            continue;

        int current_order = order;
        int subunit_order = Order::Nothing;

        if (unit->unit_id == Unit::Medic && (order == Order::Attack || order == Order::AttackMove))
            current_order = Order::HealMove;
        if (current_order == Order::Attack)
        {
            if (target)
                current_order = units_dat_attack_unit_order[unit->unit_id];
            else
                current_order = units_dat_attack_move_order[unit->unit_id];
            if (current_order == Order::Nothing)
                continue;

            if (unit->HasSubunit())
            {
                if (target)
                    subunit_order = units_dat_attack_unit_order[unit->subunit->unit_id];
                else
                    subunit_order = units_dat_attack_move_order[unit->subunit->unit_id];
            }
        }
        if (current_order == Order::RallyPointUnit)
        {
            Unit *rally_target = target;
            if (!rally_target)
                rally_target = unit;
            unit->rally.unit = rally_target;
            unit->rally.position = rally_target->sprite->position;
            continue;
        }
        else if (current_order == Order::RallyPointTile)
        {
            unit->rally.unit = 0;
            unit->rally.position = Point(x, y);
            continue;
        }

        if (target)
        {
            IssueOrderTargetingUnit(unit, target, queued, current_order);
            if (subunit_order != Order::Nothing)
                IssueOrderTargetingUnit(unit->subunit, target, queued, subunit_order);
        }
        else
        {
            if (fow_unit == Unit::None)
            {
                GetFormationMovementTarget(unit, &formation);
            }
            IssueOrderTargetingGround2(unit, current_order, formation.current_target[0], formation.current_target[1], fow_unit, queued);
            if (subunit_order != Order::Nothing) // Is this even possible?
                IssueOrderTargetingGround2(unit->subunit, subunit_order, formation.current_target[0], formation.current_target[1], fow_unit, queued);
        }
    }
}

void SendTargetedOrderCommand(uint8_t order, int x, int y, Unit *target, int fow_unit, uint8_t queued)
{
    uint8_t cmd[11];
    cmd[0] = commands::TargetedOrder;
    *(int16_t *)(cmd + 1) = x;
    *(int16_t *)(cmd + 3) = y;
    if (target)
    {
        *(uint32_t *)(cmd + 5) = target->lookup_id;
    }
    else if (fow_unit != Unit::None)
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
    SendCommand(cmd, 11);
}

void GetAttackErrorMessage(Unit *target, int targeting_weapon, int16_t *error)
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
        if (targeting_weapon != Weapon::None)
            *error = weapons_dat_error_msg[targeting_weapon];
    }
}

void DoTargetedCommand(int x, int y, Unit *target, int fow_unit)
{
    if (!*bw::is_targeting)
        return;
    EndTargeting();

    int16_t order, error = -1;
    bool can_attack = false, can_cast_spell = false, has_energy = false;

    if (target)
        order = *bw::unit_order_id;
    else if (fow_unit != Unit::None)
        order = *bw::obscured_unit_order_id;
    else
        order = *bw::ground_order_id;

    int targeting_weapon = orders_dat_targeting_weapon[order];
    int weapon_targeting = orders_dat_use_weapon_targeting[order];
    int energy = 0, tech = orders_dat_energy_tech[order];
    if (tech != Tech::None)
        energy = techdata_dat_energy_cost[tech] * 256;

    if (orders_dat_obscured[order] == Order::None)
        fow_unit = Unit::None;

    for (unsigned i = 0; i < Limits::Selection; i++)
    {
        Unit *unit = bw::client_selection_group[i];
        if (!unit)
            break;
        if (unit->IsDisabled())
            continue;
        if (weapon_targeting)
        {
            if (targeting_weapon == Weapon::None)
            {
                if (!target)
                {
                    if (unit->unit_id == Unit::InfestedTerran || unit->CanAttackFowUnit(fow_unit))
                        can_attack = true;
                }
                else
                {
                    if (unit->GetRClickAction() == RightClickAction::Attack) // sieged tank, towerit, etc
                    {
                        if (IsOutOfRange(unit, target))
                        {
                            error = String::Error_OutOfRange;
                            continue;
                        }
                        else if (IsTooClose(unit, target))
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
                if (!CanHitUnit(target, unit, targeting_weapon) && (fow_unit >= Unit::None || units_dat_flags[fow_unit] & UnitFlags::Invincible))
                {
                    continue;
                }
                if (!NeedsMoreEnergy(unit, energy))
                    has_energy = true;
                can_attack = true;
            }

        }
        else if (tech != Tech::None)
        {
            if (CanTargetSpell(unit, x, y, &error, tech, target, fow_unit))
            {
                if (!NeedsMoreEnergy(unit, energy))
                    has_energy = true;
                can_cast_spell = true;
            }
        }
        else if (fow_unit == Unit::None)
        {
            CanTargetOrder(unit, target, order, &error); // Not checking anything, just to get error
        }
        else
        {
            CanTargetOrderOnFowUnit(unit, fow_unit, order, &error); // Same
        }
    }

    Unit *unit = *bw::primary_selected;

    if (weapon_targeting)
    {
        if (!can_attack || error != -1)
        {
            GetAttackErrorMessage(target, targeting_weapon, &error);
            ShowInfoMessage(error, Sound::Error_Zerg + unit->GetRace(), unit->player);
            if (targeting_weapon == Weapon::None)
                SendTargetedOrderCommand(order, x, y, target, fow_unit, *bw::is_queuing_command); // So it'll move there anyways
            return;
        }
        else if (energy && !has_energy)
        {
            int race = unit->GetRace();
            ShowInfoMessage(String::NotEnoughEnergy + race, Sound::NotEnoughEnergy + race, unit->player);
            return;
        }
    }
    else if (energy)
    {
        if (!can_cast_spell)
        {
            if (error == -1)
                error = String::Error_InvalidTarget;
            ShowErrorMessage((*bw::stat_txt_tbl)->GetTblString(error), *bw::command_user, unit);
            return;
        }
        else if (/*energy && */!has_energy)
        {
            int race = unit->GetRace();
            ShowInfoMessage(String::NotEnoughEnergy + race, Sound::NotEnoughEnergy + race, unit->player);
            return;
        }
    }
    if (error != -1)
    {
        ShowErrorMessage((*bw::stat_txt_tbl)->GetTblString(error), unit->player, unit);
    }
    else
    {
        PlayYesSoundAnim(*bw::primary_selected);
        SendTargetedOrderCommand(order, x, y, target, fow_unit, *bw::is_queuing_command);
    }
}

void __fastcall GameScreenLClickEvent_Targeting(Event *event)
{
    if (IsOutsideGameScreen(event->x, event->y))
        return;

    int x = event->x + *bw::screen_x;
    int y = event->y + *bw::screen_y;
    Unit *target = FindUnitAtPoint(x, y);
    Sprite *fow;
    if (!target)
        fow = ShowCommandResponse(x, y, 0);
    else
        fow = ShowCommandResponse(x, y, target->sprite);

    if (fow)
        DoTargetedCommand(x, y, target, fow->index);
    else
        DoTargetedCommand(x, y, target, Unit::None);

    SetCursorSprite(0);
}

void __fastcall GameScreenRClickEvent(Event *event)
{
    if (IsOutsideGameScreen(event->x, event->y) || bw::client_selection_group2[0] == nullptr)
        return;
    Unit *highest_ranked = *bw::primary_selected;
    if (highest_ranked == nullptr || !CanControlUnit(highest_ranked))
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
        if (!highest_ranked->HasRally())
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
            SendRightClickCommand(0, x, y, fow->index, *bw::is_queuing_command);
        else
            SendRightClickCommand(0, x, y, Unit::None, *bw::is_queuing_command);
    }
    else
    {
        ShowCommandResponse(x, y, target->sprite);
        SendRightClickCommand(target, x, y, Unit::None, *bw::is_queuing_command);
    }

    if (ShowRClickErrorIfNeeded(target) == 1)
        PlayYesSoundAnim(highest_ranked);
}
