#include "ai.h"

#include <string.h>

#include "constants/order.h"
#include "constants/tech.h"
#include "constants/unit.h"
#include "patch/patchmanager.h"
#include "assert.h"
#include "bullet.h"
#include "limits.h"
#include "log.h"
#include "offsets_hooks.h"
#include "offsets.h"
#include "order.h"
#include "pathing.h"
#include "player.h"
#include "rng.h"
#include "sprite.h"
#include "tech.h"
#include "thread.h"
#include "unit.h"
#include "unitlist.h"
#include "unitsearch.h"
#include "yms.h"

#include "perfclock.h"

using std::get;

namespace Ai
{

static int RemoveWorkerOrBuildingAi(Unit *unit, bool only_building);

Region *GetRegion(int player, int region)
{
    return bw::player_ai_regions[player] + region;
}

static void MedicRemove(Unit *medic)
{
    if (!IsActivePlayer(medic->player))
        return;
    if (medic->Type() == UnitId::Medic && medic == bw::player_ai[medic->player].free_medic)
    {
        bw::player_ai[medic->player].free_medic = nullptr;
        for (Unit *unit : bw::first_player_unit[medic->player])
        {
            if (unit->Type() == UnitId::Medic && unit->ai != nullptr)
            {
                bw::player_ai[medic->player].free_medic = unit;
                break;
            }
        }
    }
}

ListHead<GuardAi, 0x0> needed_guards[0x8];

bool IsInAttack(Unit *unit)
{
    switch (((Ai::MilitaryAi *)unit->ai)->region->state)
    {
        case 1:
        case 2:
        case 8:
        case 9:
            return true;
        default:
            return false;
    }
}

static bool IsNotChasing(Unit *unit) // dunno
{
    if (unit->ai->type != 4)
        return true;
    MilitaryAi *ai = (MilitaryAi *)unit->ai;
    if (unit->GetRegion() != ai->region->region_id)
        return true;
    return ai->region->state != 7;
}

static void SetAttackTarget(Unit *unit, Unit *new_target, bool accept_if_sieged)
{
    if (unit->flags & UnitStatus::InBuilding)
        unit->target = new_target;
    else
        bw::AttackUnit(unit, new_target, true, accept_if_sieged);

    if (unit->Type().HasHangar() && !unit->IsInAttackRange(new_target))
        unit->flags |= UnitStatus::Disabled2;
}

bool UpdateAttackTarget(Unit *unit, bool accept_if_sieged, bool accept_critters, bool must_reach)
{
    // Unit::GetAutoTarget() requires this, but it is called rarely from here, so copy the
    // assertion here as well
    Assert(late_unit_frames_in_progress || bulletframes_in_progress);
    STATIC_PERF_CLOCK(Ai_UpdateAttackTarget);

    if (unit->ai == nullptr || !unit->HasWayOfAttacking())
        return false;
    if (unit->order_queue_begin && unit->order_queue_begin->order_id == OrderId::Patrol)
        return false;
    if (unit->OrderType() == OrderId::Pickup4)
    {
        must_reach = true;
        if (unit->previous_attacker == nullptr &&
                unit->order_queue_begin != nullptr &&
                unit->order_queue_begin->Type() == OrderId::AttackUnit)
        {
            return false;
        }
    }
    if (unit->target != nullptr && unit->flags & UnitStatus::Reacts && unit->move_target_unit != nullptr)
    {
        if (unit->IsEnemy(unit->move_target_unit) && unit->IsStandingStill() == 2)
        {
            unit->move_target = unit->sprite->position;
            unit->flags &= ~UnitStatus::MovePosUpdated;
            unit->flags |= UnitStatus::Unk80;
            if (unit->target == unit->previous_attacker)
                unit->previous_attacker = nullptr;
            unit->ClearTarget();
        }
    }

    const UpdateAttackTargetContext ctx(unit, accept_critters, must_reach);
    Unit *target;
    if (unit->IsFlying())
    {
        target = ctx.CheckValid(unit->Ai_ChooseAirTarget());
        if (target == nullptr)
            target = ctx.CheckValid(unit->Ai_ChooseGroundTarget());
    }
    else
    {
        target = ctx.CheckValid(unit->Ai_ChooseGroundTarget());
        if (target == nullptr)
            target = ctx.CheckValid(unit->Ai_ChooseAirTarget());
    }

    Unit *previous_attack_target = nullptr;
    if (OrderType(unit->order).UseWeaponTargeting() && unit->target != nullptr)
        previous_attack_target = ctx.CheckValid(unit->target);

    if (unit->previous_attacker != nullptr)
        unit->previous_attacker = ctx.CheckPreviousAttackerValid(unit->previous_attacker);

    Unit *new_target = previous_attack_target;
    if (unit->Ai_IsBetterTarget(target, previous_attack_target))
        new_target = target;
    if (unit->Ai_IsBetterTarget(unit->previous_attacker, new_target))
        new_target = unit->previous_attacker;

    if (new_target == nullptr)
    {
        if (!IsNotChasing(unit))
            return false;
        if (unit->Ai_TryReturnHome(true))
            return false;

        new_target = ctx.CheckValid(unit->GetAutoTarget());
        if (new_target == nullptr)
            return false;
    }
    if (new_target != unit->target)
    {
        SetAttackTarget(unit, new_target, accept_if_sieged);
    }

    return true;
}

Script::Script(uint32_t player_, uint32_t pos_, bool bwscript, const Rect32 *area_) : pos(pos_), player(player_)
{
    flags = bwscript ? 0x1 : 0x0;
    list.Add(*bw::first_active_ai_script);
    wait = 0;
    town = nullptr;
    if (area_)
    {
        memcpy(&area, area_, 0x10);
        center[0] = (area.right - area.left) / 2 + area.left;
        center[1] = (area.bottom - area.top) / 2 + area.top;
    }
    else
    {
        memset(&area, 0, 0x10);
        center[0] = 0;
        center[1] = 0;
    }
}

Script::~Script()
{
    list.Remove(*bw::first_active_ai_script);
}

void ProgressScripts()
{
    Script *script = *bw::first_active_ai_script, *next;
    while (script)
    {
        Assert(bw::players[script->player].type == 1 || !script->town);
        next = script->list.next;
        if (script->wait-- == 0)
        {
            if (script->flags & 0x4)
                delete script;
            else
                bw::ProgressAiScript(script);
        }
        script = next;
    }
}

void RemoveTownGasReferences(Unit *unit)
{
    if (unit->Type().Flags() & UnitFlags::ResourceContainer)
    {
        for (unsigned int i = 0; i < Limits::ActivePlayers; i++)
        {
            for (Town *town = bw::active_ai_towns[i]; town; town = town->list.next)
            {
                if (town->mineral == unit)
                    town->mineral = nullptr;
                for (int j = 0; j < 3; j++)
                {
                    if (town->gas_buildings[j] == unit)
                        town->gas_buildings[j] = nullptr;
                }
            }
        }
    }
}

void RemoveUnitAi(Unit *unit, bool unk)
{
    // If ai is not guard, it will be deleted by following funcs
    bool is_guard = unit->ai && unit->ai->type == 1;
    bw::RemoveFromAiStructs(unit, unk);
    RemoveWorkerOrBuildingAi(unit, false);
    if (is_guard)
    {
        GuardAi *ai = (GuardAi *)unit->ai;
        if (ai->unk_count < 100)
            ai->unk_count++;

        ai->parent = nullptr;
        ai->list.Change(bw::first_guard_ai[unit->player], needed_guards[unit->player]);
    }
    MedicRemove(unit);
    unit->ai = nullptr;
}

GuardAi *CreateGuardAi(int player, Unit *unit, int unit_id, Point pos)
{
    GuardAi *ai = new GuardAi;
    ai->type = 1;
    ai->parent = unit;
    ai->home = pos;
    ai->unk_pos = pos;
    ai->unit_id = unit_id;
    ai->unk_count = 0;
    ai->previous_update = 0;
    return ai;
}

void PreCreateGuardAi(uint8_t player, uint16_t unit_id, uint16_t x, uint16_t y)
{
    GuardAi *ai = CreateGuardAi(player, 0, unit_id, Point(x, y));
    ai->list.Add(needed_guards[player]);
}

void AddGuardAiToUnit(Unit *unit)
{
    if (!IsActivePlayer(unit->player))
        return;
    if (bw::players[unit->player].type != 1)
        return;
    if (unit->Type().AiFlags() & 0x2)
        return;

    for (GuardAi *ai = needed_guards[unit->player]; ai; ai = ai->list.next)
    {
        if (ai->unit_id == unit->unit_id && ai->unk_pos == unit->sprite->position)
        {
            unit->ai = (UnitAi *)ai;
            ai->list.Change(needed_guards[unit->player], bw::first_guard_ai[unit->player]);

            ai->home = unit->sprite->position;
            ai->parent = unit;
            ai->unk_count = 0;
            return;
        }
    }

    GuardAi *ai = CreateGuardAi(unit->player, unit, unit->unit_id, unit->sprite->position);
    unit->ai = (UnitAi *)ai;
    ai->list.Add(bw::first_guard_ai[unit->player]);
}

void UpdateGuardNeeds(int player)
{
    GuardAi *ai = bw::first_guard_ai[player];
    GuardAi *next;
    while (ai != nullptr)
    {
        next = ai->list.next;
        if (ai->parent->flags & UnitStatus::Hallucination)
        {
            ai->list.Remove(bw::first_guard_ai[player]);
            // Bw does not touch parent, which is a bug
            ai->parent->ai = nullptr;
            delete ai;
        }
        ai = next;
    }
    for (GuardAi *ai = needed_guards[player]; ai != nullptr; ai = next)
    {
        next = ai->list.next;
        if (bw::player_ai[player].flags & 0x20 && ai->unk_count >=3)
        {
            ai->list.Remove(needed_guards[player]);
            delete ai;
        }
        else
        {
            if (ai->previous_update)
            {
                uint32_t time;
                if (ai->unk_count)
                {
                    UnitType unit_id(ai->unit_id);
                    if (unit_id == UnitId::SiegeTank_Sieged)
                        unit_id = UnitId::SiegeTankTankMode;
                    time = unit_id.BuildTime();
                    switch (unit_id.Raw())
                    {
                        case UnitId::Guardian:
                        case UnitId::Devourer:
                            time += UnitId::Mutalisk.BuildTime();
                        break;
                        case UnitId::Hydralisk:
                            time += UnitId::Hydralisk.BuildTime();
                        break;
                        case UnitId::Archon:
                            time += UnitId::HighTemplar.BuildTime();
                        break;
                        case UnitId::DarkArchon:
                            time += UnitId::DarkTemplar.BuildTime();
                        break;
                    }
                    time = ai->previous_update + time / 15 + 5;
                }
                else
                {
                    time = ai->previous_update + 300;
                }
                if (*bw::elapsed_seconds <= time)
                    continue;
            }
            Region *region = GetAiRegion(player, ai->unk_pos);
            if (region->state != 3 && !region->air_target && !region->ground_target && !(region->flags & 0x20))
            {
                if (UnitType(ai->unit_id) == UnitId::Zergling || UnitType(ai->unit_id) == UnitId::Scourge)
                {
                    Unit *unit = bw::FindNearestAvailableMilitary(ai->unit_id,
                                                                  ai->unk_pos.x,
                                                                  ai->unk_pos.y,
                                                                  player);
                    if (unit)
                    {
                        ai->list.Change(needed_guards[player], bw::first_guard_ai[player]);
                        RemoveUnitAi(unit, false);
                        ai->home = ai->unk_pos;
                        ai->parent = unit;
                        unit->ai = (UnitAi *)ai;
                        unit->IssueOrderTargetingNothing(OrderId::ComputerAi);
                        continue;
                    }
                }
                bw::Ai_GuardRequest(ai, player);
                ai->previous_update = *bw::elapsed_seconds;
            }
        }
    }
}

void DeleteGuardNeeds(int player)
{
    GuardAi *next;
    for (GuardAi *ai = needed_guards[player]; ai; ai = next)
    {
        next = ai->list.next;
        delete ai;
    }
    needed_guards[player] = 0;
}

WorkerAi::WorkerAi()
{
    type = 2;
    unk9 = 1;
    unka = 0;
    wait_timer = 0;
    unk_count = *bw::elapsed_seconds;
}

BuildingAi::BuildingAi()
{
    type = 3;
    unke[0] = 0;
    unke[1] = 0; // Most likely just padding
    for (int i = 0; i < 5; i++)
    {
        train_queue_values[i] = 0;
        train_queue_types[i] = 0;
    }
}

MilitaryAi::MilitaryAi()
{
    type = 4;
}

void AddUnitAi(Unit *unit, Town *town)
{
    if (unit->Type().IsWorker())
    {
        WorkerAi *ai = new WorkerAi;
        ai->list.Add(town->first_worker);
        town->worker_count++;
        ai->parent = unit;
        ai->town = town;
        unit->ai = (UnitAi *)ai;
        unit->worker.current_harvest_target = 0;
    }
    else if ((unit->Type().IsBuilding() && unit->Type() != UnitId::VespeneGeyser) ||
             unit->Type() == UnitId::Larva ||
             unit->Type() == UnitId::Egg ||
             unit->Type() == UnitId::Overlord)
    {
        BuildingAi *ai = new BuildingAi;
        ai->list.Add(town->first_building);
        ai->parent = unit;
        ai->town = town;
        unit->ai = (UnitAi *)ai;

        if (unit->Type() == UnitId::Hatchery || unit->Type() == UnitId::Lair || unit->Type() == UnitId::Hive)
        {
            if (unit->flags & UnitStatus::Completed)
            {
                unit->IssueOrderTargetingNothing(OrderId::ComputerAi);
                unit->IssueSecondaryOrder(OrderId::SpreadCreep);
            }
        }
        if (~bw::player_ai[town->player].flags & 0x20)
        {
            bw::Ai_UpdateRegionStateUnk(town->player, unit);
        }
        else if ((unit->flags & UnitStatus::Building))
        {
            switch (unit->Type().Raw())
            {
                case UnitId::MissileTurret:
                case UnitId::Bunker:
                case UnitId::CreepColony:
                case UnitId::SunkenColony:
                case UnitId::SporeColony:
                case UnitId::Pylon:
                case UnitId::PhotonCannon:
                break;
                default:
                    bw::Ai_UpdateRegionStateUnk(town->player, unit);
                break;
            }
        }
        if (!town->inited)
        {
            town->inited = 1;
            town->mineral = 0;
            town->gas_buildings[0] = 0;
            town->gas_buildings[1] = 0;
            town->gas_buildings[2] = 0;
        }
        UnitType main_building = UnitId::None;
        switch (bw::players[town->player].race)
        {
            case 0:
                main_building = UnitId::Hatchery;
            break;
            case 1:
                main_building = UnitId::CommandCenter;
            break;
            case 2:
                main_building = UnitId::Nexus;
            break;
        }
        if (unit->Type() == main_building || unit->IsResourceDepot())
        {
            if (!town->main_building)
                town->main_building = unit;
        }
        else if (unit->Type().IsGasBuilding())
        {
            for (int i = 0; i < 3; i++)
            {
                if (!town->gas_buildings[i])
                {
                    town->gas_buildings[i] = unit;
                    break;
                }
            }
            if (town->resource_area)
                town->unk1d = 1;
        }
    }
}

void DeleteWorkerAi(WorkerAi *ai, DataList<WorkerAi> *first_ai)
{
    ai->list.Remove(*first_ai);
    delete ai;
}

void DeleteBuildingAi(BuildingAi *ai, DataList<BuildingAi> *first_ai)
{
    ai->list.Remove(*first_ai);
    delete ai;
}

void DeleteMilitaryAi(MilitaryAi *ai, Region *region)
{
    if (!ai->parent->IsFlying())
        region->ground_unit_count--;

    ai->list.Remove(region->military);
    ai->parent->ai = nullptr;
    delete ai;
}

void AddMilitaryAi(Unit *unit, Region *region, bool always_use_this_region)
{
    if (region->state == 3 && !always_use_this_region)
    {
        region = GetAiRegion(unit);
    }
    MilitaryAi *ai = new MilitaryAi;
    ai->list.Add(region->military);
    if (!unit->IsFlying())
        region->ground_unit_count++;

    ai->region = region;
    ai->parent = unit;
    unit->ai = (UnitAi *)ai;
    Pathing::Region *pathing_region = (*bw::pathing)->regions + region->region_id;
    OrderType order = unit->Type() == UnitId::Medic ? OrderId::HealMove : OrderId::AiAttackMove;
    bw::ProgressMilitaryAi(unit, order.Raw(), pathing_region->x >> 8, pathing_region->y >> 8);
    if (IsInAttack(unit))
        bw::Ai_UpdateSlowestUnitInRegion(region);
}

void UnitAi::Delete()
{
    switch (type)
    {
        case 1:
            delete (GuardAi *)this;
        break;
        case 2:
            delete (WorkerAi *)this;
        break;
        case 3:
            delete (BuildingAi *)this;
        break;
        case 4:
            delete (MilitaryAi *)this;
        break;
    }
}

Town *CreateTown(uint8_t player, uint16_t x, uint16_t y)
{
    Town *town = new Town;
    memset(town, 0, sizeof(Town));

    town->list.Add(bw::active_ai_towns[player]);

    town->player = player;
    town->position = Point(x, y);
    return town;
}

void DeleteTown(Town *town)
{
    Script *script = *bw::first_active_ai_script;
    while (script)
    {
        Script *next = script->list.next;
        if (script->town == town)
            delete script;
        script = next;
    }

    for (Unit *unit = *bw::first_active_unit; unit; unit = unit->next())
    {
        if (unit->Type().Flags() & UnitFlags::ResourceContainer)
            unit->resource.ai_unk = 0;
    }
    if (town->resource_area)
        (*bw::resource_areas).areas[town->resource_area].flags &= ~0x2;

    town->list.Remove(bw::active_ai_towns[town->player]);
    delete town;
}

static void DeleteTownIfNeeded(Town *town)
{
    if (!bw::TryBeginDeleteTown(town))
        return;
    DeleteTown(town);
}

void DeleteAll()
{
    debug_log->Log("Ai delete all\n");
    for (unsigned int i = 0; i < Limits::ActivePlayers; i++)
    {
        Town *town = bw::active_ai_towns[i];
        while (town)
        {
            Town *tbd = town;
            town = town->list.next;
            delete tbd;
        }
        bw::active_ai_towns[i] = nullptr;
    }
    Script *script = *bw::first_active_ai_script;
    while (script)
    {
        Script *tbd = script;
        script = script->list.next;
        delete tbd;
    }
    *bw::first_active_ai_script = nullptr;
    for (unsigned int i = 0; i < Limits::ActivePlayers; i++)
    {
        DeleteGuardNeeds(i);
    }
}

void SetSuicideTarget(Unit *unit)
{
    if ((~unit->flags & UnitStatus::Reacts && ~unit->flags & UnitStatus::Burrowed) || unit->IsDisabled())
        return;
    if (unit->Type() == UnitId::Arbiter && unit->ai != nullptr)
        return;

    int target_player_count = 0;
    int target_players[12];
    if (unit->target && unit->IsEnemy(unit->target))
    {
        target_player_count = 1;
        target_players[0] = unit->target->GetOriginalPlayer();
    }
    else
    {
        for (int i = 0; i < Limits::Players; i++)
        {
            if (bw::alliances[unit->player][i] == 0 && bw::first_player_unit[i])
            {
                target_players[target_player_count++] = i;
            }
        }
    }
    if (target_player_count == 0)
        return;

    Unit *target = nullptr;
    auto own_id = unit->Type();
    bool can_attack = unit->HasWayOfAttacking();
    for (int i = Rand(0x36) % target_player_count; i < target_player_count; i++)
    {
        int player = target_players[i];
        uint32_t dist = 0xffffffff;

        for (Unit *enemy : bw::first_player_unit[player])
        {
            if (!enemy->sprite || enemy->OrderType() == OrderId::Die)
                continue;
            if (can_attack && !unit->CanAttackUnit(enemy))
                continue;

            int w = enemy->sprite->position.x - unit->sprite->position.x, h = enemy->sprite->position.y - unit->sprite->position.y;
            uint32_t cur_dist = w * w + h * h;
            if (cur_dist < dist)
            {
                dist = cur_dist;
                target = enemy;
            }
        }
        if (target)
            break;
    }
    if (!target)
        return;

    if (can_attack)
    {
        if (target->Type() == UnitId::Interceptor && target->interceptor.parent != nullptr)
            target = target->interceptor.parent;
        unit->Attack(target);
        if (unit->HasSubunit())
            unit->subunit->Attack(target);

        // Is this called ever because sieged tanks don't seem to react?
        if (unit->ai && own_id == UnitId::SiegeTank_Sieged && unit->OrderType() != OrderId::TankMode && !unit->IsInAttackRange(target))
        {
            if (bw::HasTargetInRange(unit))
            {
                UpdateAttackTarget(unit, true, true, false);
            }
            else
            {
                unit->IssueOrderTargetingNothing(OrderId::TankMode);
                unit->AppendOrderTargetingUnit(UnitId::SiegeTankTankMode.AttackUnitOrder(), target);
            }
        }
    }
    else
    {
        unit->IssueOrderTargetingUnit(OrderId::Move, target);
    }

    RemoveUnitAi(unit, false);
}

void SetFinishedUnitAi(Unit *unit, Unit *parent)
{
    if (unit->Type() == UnitId::Guardian || unit->Type() == UnitId::Devourer)
        return;

    if (unit->flags & UnitStatus::Hallucination)
    {
        AddMilitaryAi(unit, GetAiRegion(unit), true);
        return;
    }
    else if (unit->Type() == UnitId::Lurker ||
            unit->Type() == UnitId::Archon ||
            unit->Type() == UnitId::DarkArchon ||
            unit->Type() == UnitId::NuclearMissile ||
            unit->Type() == UnitId::Observer)
    {
        return;
    }
    if (parent->ai == nullptr)
    {
        return;
    }

    BuildingAi *ai = (BuildingAi *)parent->ai;
    Assert(ai->type == 3);
    int queue_type = ai->train_queue_types[0];
    void *queue_value = ai->train_queue_values[0];
    if (unit == parent) // Morph
    {
        RemoveTownGasReferences(unit);
        ai->list.Remove(ai->town->first_building);
        delete ai;
    }
    else if (unit->unit_id != parent->unit_id) // Anything but 2 units in 1 egg morph
    {
        queue_type = ai->train_queue_types[parent->current_build_slot];
        queue_value = ai->train_queue_values[parent->current_build_slot];
    }

    if (queue_type == 2) // Guard train
    {
        GuardAi *guard = (GuardAi *)queue_value;
        if (!guard->parent)
        {
            guard->home = guard->unk_pos;
            guard->parent = unit;
            unit->ai = (UnitAi *)guard;
            guard->list.Change(needed_guards[unit->player], bw::first_guard_ai[unit->player]);
        }
        else
        {
            AddMilitaryAi(unit, GetAiRegion(unit), false);
        }
    }
    else if (queue_type == 1) // Train military
    {
        AddMilitaryAi(unit, (Region *)queue_value, false);
    }
    else
    {
        AddGuardAiToUnit(unit);
    }
}

int AddToRegionMilitary(Region *region, int unit_id, int unkx6)
{
    int player = region->player;
    Region *other = bw::player_ai_regions[player] + region->target_region_id;
    Pathing::Region *pathreg = (*bw::pathing)->regions + region->region_id;
    int x = pathreg->x >> 8, y = pathreg->y >> 8;
    UnitAi *base_ai = bw::Ai_FindNearestActiveMilitaryAi(region, unit_id, x, y, other);
    if (!base_ai)
    {
        base_ai = bw::Ai_FindNearestMilitaryOrSepContAi(region, unkx6, unit_id, x, y, other);
        if (!base_ai)
            return 0;
    }

    Region *old_region;
    if (base_ai->type == 1)
    {
        GuardAi *ai = (GuardAi *)base_ai;
        old_region = GetAiRegion(ai->parent); // But this is not necessarily same?
        AddMilitaryAi(ai->parent, region, true);
        ai->list.Change(bw::first_guard_ai[player], needed_guards[player]);
        ai->parent = 0;
    }
    else if (base_ai->type == 4)
    {
        MilitaryAi *ai = (MilitaryAi *)base_ai;
        Unit *unit = ai->parent;
        old_region = ai->region;
        DeleteMilitaryAi(ai, ai->region);
        AddMilitaryAi(unit, region, true);
        if (unit->flags & UnitStatus::InBuilding)
            unit->related->UnloadUnit(unit);
    }
    else
        return 0;
    bw::Ai_RecalculateRegionStrength(old_region);
    return 1;
}

int DoesNeedGuardAi(int player, int unit_id)
{
    for (GuardAi *ai = needed_guards[player]; ai; ai = ai->list.next)
    {
        if (ai->unit_id != unit_id)
            continue;
        Region *region = GetAiRegion(player, ai->unk_pos);
        if (region->state == 3 || region->air_target || region->ground_target)
            continue;
        return true;
    }
    return false;
}

void ForceGuardAiRefresh(uint8_t player, uint16_t unit_id)
{
    for (GuardAi *ai = needed_guards[player]; ai; ai = ai->list.next)
    {
        if (!ai->previous_update)
            continue;
        UnitType ai_unit_id(ai->unit_id);
        if (ai_unit_id == UnitId::SiegeTank_Sieged)
            ai_unit_id = UnitId::SiegeTankTankMode;
        if (UnitType(unit_id) != ai_unit_id)
            continue;
        ai->previous_update = 1;
    }
}

Unit *FindAvailableUnit(int player, UnitType unit_id)
{
    for (Unit *unit : bw::first_player_unit[player])
    {
        if (unit->Type() != unit_id)
            continue;
        if (unit->sprite == nullptr || unit->OrderType() == OrderId::Die)
            continue;
        if (unit->target != nullptr || unit->previous_attacker != nullptr || unit->ai == nullptr)
            continue;
        if (unit->ai->type == 1)
        {
            return unit;
        }
        else if (unit->ai->type == 4)
        {
            int region_state = ((MilitaryAi *)unit->ai)->region->state;
            if (region_state == 0 || region_state == 4 || region_state == 5)
                return unit;
        }
    }
    return nullptr;
}

void PopSpendingRequest(int player, bool also_available_resources)
{
    bw::Ai_PopSpendingRequestResourceNeeds(player, also_available_resources);
    bw::Ai_PopSpendingRequest(player);
}

bool Merge(int player, UnitType unit_id, OrderType order, const bool check_energy)
{
    int count = 0;
    Unit *units[2];
    Unit *high_energy = 0;
    for (Unit *unit : bw::first_player_unit[player])
    {
        if (unit->Type() != unit_id || unit->OrderType() == order)
            continue;
        if (unit->sprite == nullptr || unit->OrderType() == OrderId::Die)
            continue;
        if (unit->target != nullptr || unit->previous_attacker != nullptr || unit->ai == nullptr)
            continue;
        if (~unit->flags & UnitStatus::Completed)
            continue;
        if (unit->flags & UnitStatus::InTransport)
            continue;

        if (unit->ai->type == 1)
        {
            units[count++] = unit;
        }
        else if (unit->ai->type == 4)
        {
            int region_state = ((MilitaryAi *)unit->ai)->region->state;
            if (region_state == 0 || region_state == 4 || region_state == 5 || region_state == 8 || region_state == 9)
            {
                if (check_energy && count)
                {
                    if (units[0]->energy > unit->energy)
                    {
                        high_energy = units[0];
                        units[0] = unit;
                    }
                    else
                    {
                        if (high_energy)
                        {
                            if (high_energy->energy > unit->energy)
                                high_energy = unit;
                        }
                        else
                            high_energy = unit;
                    }
                }
                else
                {
                    units[count++] = unit;
                }
            }
        }
        if (count == 2)
            break;
    }
    if (count != 2)
    {
        if (high_energy)
            units[1] = high_energy;
        else
            return false;
    }

    units[0]->IssueOrderTargetingUnit(order, units[1]);
    units[1]->IssueOrderTargetingUnit(order, units[0]);
    return true;
}

static bool IsAtBuildLimit(int player, UnitType unit_id)
{
    int limit = bw::player_ai[player].unit_build_limits[unit_id.Raw()];
    if (!limit)
        return false;
    if (limit == 255)
        return true;
    int count = bw::all_units_count[unit_id.Raw()][player];
    if (unit_id == UnitId::SiegeTankTankMode)
        count += bw::all_units_count[UnitId::SiegeTank_Sieged.Raw()][player];
    return count >= limit;
}

int SpendReq_TrainUnit(int player, Unit *unit, int no_retry)
{
    using namespace UnitId;

    SpendingRequest *req = bw::player_ai[player].requests;
    Unit *parent = unit;
    switch (req->unit_id)
    {
        case Guardian:
        case Devourer:
        case Lurker:
            if (UnitType(req->unit_id) == Lurker)
                parent = FindAvailableUnit(player, Hydralisk);
            else
                parent = FindAvailableUnit(player, Mutalisk);
            if (parent == nullptr)
            {
                PopSpendingRequest(player, false);
                return 0;
            }
        break;
        case Archon:
            if (!Merge(player, HighTemplar, OrderId::WarpingArchon, true))
                PopSpendingRequest(player, false);
            else
                PopSpendingRequest(player, true);
            return 0;
        break;
        case DarkArchon:
            if (!Merge(player, DarkTemplar, OrderId::WarpingDarkArchon, false))
                PopSpendingRequest(player, false);
            else
                PopSpendingRequest(player, true);
            return 0;
        break;
    }

    if (bw::CheckUnitDatRequirements(parent, req->unit_id, player) == 1)
    {
        if (!bw::Ai_DoesHaveResourcesForUnit(player, req->unit_id))
        {
            if (~bw::player_ai[player].flags & 0x20)
                PopSpendingRequest(player, false);
            return 0;
        }
        if (IsAtBuildLimit(player, UnitType(req->unit_id)))
        {
            PopSpendingRequest(player, false);
            return 0;
        }
        if (parent == unit)
        {
            if (!bw::Ai_TrainUnit(parent, req->unit_id, req->type, req->val))
                return 0;
        }
        else // lurk/guard/devo morph
        {
            if (!bw::PrepareBuildUnit(parent, req->unit_id))
                return 0;
            RemoveUnitAi(parent, false);
            if (req->type == 2)
            {
                GuardAi *ai = (GuardAi *)req->val;
                ai->list.Change(needed_guards[player], bw::first_guard_ai[player]);
                ai->home = ai->unk_pos;
                ai->parent = parent;
                parent->ai = (UnitAi *)ai;
            }
            else
            {
                Assert(req->type == 1);
                AddMilitaryAi(parent, (Region *)req->val, true);
            }
        }
        PopSpendingRequest(player, true);
        if (parent->Type() == Larva || parent->Type() == Hydralisk || parent->Type() == Mutalisk)
            parent->IssueOrderTargetingNothing(OrderId::UnitMorph);
        else
            parent->IssueSecondaryOrder(OrderId::Train);
        return 1;
    }
    else
    {
        switch (*bw::last_error)
        {
            case 0x2:
            case 0x7:
            case 0x8:
            case 0xd:
            case 0x15:
            case 0x17:
                PopSpendingRequest(player, false);
                // True for only call
//                if (!no_retry)
//                    ProgressSpendingQueue(...)
        }
        return 0;
    }
}

void SuicideMission(int player, uint8_t random)
{
    if (random)
    {
        for (Unit *unit : bw::first_player_unit[player])
        {
            if (!unit->sprite || unit->OrderType() == OrderId::Die || unit->Type().IsSubunit())
                continue;
            if (unit->Type() == UnitId::NuclearMissile || unit->Type() == UnitId::Larva)
                continue;
            if (unit->sprite->IsHidden())
                continue;
            if (bw::Ai_ShouldKeepTarget(unit, nullptr))
                continue;
            SetSuicideTarget(unit);
        }
    }
    else if (!bw::player_ai[player].strategic_suicide)
    {
        Region *unk_region = nullptr;
        for (Unit *unit : bw::first_player_unit[player])
        {
            if (!unit->sprite || unit->OrderType() == OrderId::Die || !unit->ai || unit->IsTransport())
                continue;
            if (unit->flags & UnitStatus::InBuilding && bw::player_ai[player].flags & 0x20)
                continue;

            if (unit->ai->type == 2 || unit->ai->type == 3)
                continue;
            if (unit->ai->type == 1)
            {
                GuardAi *ai = (GuardAi *)unit->ai;
                ai->parent = nullptr;
                ai->list.Change(bw::first_guard_ai[player], needed_guards[player]);
                Region *region = GetAiRegion(unit);
                AddMilitaryAi(unit, region, true);
            }
            if (IsInAttack(unit))
                continue;
            if (!unk_region)
            {
                memset(bw::player_ai[player].unit_ids, 0, 0x40 * 2);
                bw::player_ai[player].unk_region = 0;
                bw::Ai_EndAllMovingToAttack(player);
                if (bw::Ai_AttackTo(player, unit->sprite->position.x, unit->sprite->position.y, 0, 1))
                    continue;
                bw::player_ai[player].strategic_suicide = 0x18;
                unk_region = bw::player_ai_regions[player] + (bw::player_ai[player].unk_region - 1);
            }
            RemoveUnitAi(unit, 0);
            AddMilitaryAi(unit, unk_region, true);
        }
    }
}

void AiScript_SwitchRescue(uint8_t player)
{
    bw::players[player].type = 3;
    for (unsigned i = 0; i < Limits::Players; i++)
    {
        bw::alliances[player][i] = 1;
        bw::alliances[i][player] = 1;
    }
    for (Unit *unit : bw::first_player_unit[player])
    {
        if (!unit->sprite || unit->OrderType() == OrderId::Die)
            continue;
        RemoveUnitAi(unit, false);
        unit->IssueOrderTargetingNothing(OrderId::RescuePassive);
    }
}

static void AiScript_MoveDt(uint8_t player, int x, int y)
{
    for (Unit *unit : bw::first_player_unit[player])
    {
        if (unit->sprite == nullptr || unit->OrderType() == OrderId::Die)
            continue;
        // O.o Checks normal dt
        if (unit->Type() != UnitId::DarkTemplar && unit->Type() != UnitId::DarkTemplarHero)
            continue;
        if (unit->ai != nullptr)
            RemoveUnitAi(unit, false);
        AddMilitaryAi(unit, GetAiRegion(unit), true);
    }
}

static int RemoveWorkerOrBuildingAi(Unit *unit, bool only_building)
{
    // Seems to be an unused arg
    Assert(!only_building);
    RemoveTownGasReferences(unit);
    if (unit->ai == nullptr || (unit->ai->type != 2 && unit->ai->type != 3))
        return 0;
    Town *town;
    if (!only_building && unit->Type().IsWorker())
    {
        WorkerAi *ai = (WorkerAi *)unit->ai;
        Assert(ai->type == 2);
        town = ai->town;
        ai->list.Remove(ai->town->first_worker);
        town->worker_count -= 1;
        delete ai;
    }
    else
    {
        BuildingAi *ai = (BuildingAi *)unit->ai;
        town = ai->town;
        ai->list.Remove(ai->town->first_building);
        delete ai;
    }
    if (unit == town->building_scv)
        town->building_scv = nullptr;
    if (unit == town->main_building)
        town->main_building = nullptr;
    if (!only_building)
        DeleteTownIfNeeded(town);
    unit->ai = nullptr;
    return 1;
}

void RemoveLimits(Common::PatchContext *patch)
{
    patch->Hook(bw::CreateAiScript, [](const Rect32 *area, uint8_t player, uint32_t pos, int is_bwscript) {
        return new Script(player, pos, is_bwscript, area);
    });
    patch->Hook(bw::RemoveAiTownGasReferences, RemoveTownGasReferences);
    patch->Hook(bw::DeleteWorkerAi, DeleteWorkerAi);
    patch->Hook(bw::DeleteBuildingAi, DeleteBuildingAi);
    patch->Hook(bw::AddUnitAi, AddUnitAi);
    patch->Hook(bw::DeleteMilitaryAi, DeleteMilitaryAi);
    patch->Hook(bw::CreateAiTown, CreateTown);
    patch->Hook(bw::PreCreateGuardAi, PreCreateGuardAi);
    patch->Hook(bw::AddGuardAiToUnit, AddGuardAiToUnit);
    patch->Hook(bw::RemoveUnitAi, [](Unit *unit, int unk) { RemoveUnitAi(unit, unk); });
    patch->Hook(bw::RemoveWorkerOrBuildingAi, [](Unit *a, int b) { return RemoveWorkerOrBuildingAi(a, b); });

    patch->Hook(bw::Ai_DeleteGuardNeeds, [](uint8_t player) { DeleteGuardNeeds(player); });
    patch->Hook(bw::Ai_UpdateGuardNeeds, UpdateGuardNeeds);
    patch->Hook(bw::Ai_SetFinishedUnitAi, SetFinishedUnitAi);
    patch->Hook(bw::Ai_AddToRegionMilitary, AddToRegionMilitary);
    patch->Hook(bw::DoesNeedGuardAi, [](uint8_t player, uint16_t unit_id) -> int {
        return DoesNeedGuardAi(player, unit_id);
    });
    patch->Hook(bw::ForceGuardAiRefresh, ForceGuardAiRefresh);
    patch->Hook(bw::AiSpendReq_TrainUnit, SpendReq_TrainUnit);

    patch->Hook(bw::AiScript_MoveDt, AiScript_MoveDt);
    patch->Hook(bw::AddMilitaryAi, [](Unit *a, Region *b, int c) { AddMilitaryAi(a, b, c); });
    patch->Hook(bw::AiScript_InvalidOpcode, [](Script *script) { delete script; });
    patch->Hook(bw::AiScript_Stop, [](Script *script) { delete script; });

    patch->Hook(bw::Ai_SuicideMission, SuicideMission);
    patch->Hook(bw::MedicRemove, MedicRemove);
    patch->Hook(bw::AiScript_SwitchRescue, AiScript_SwitchRescue);
}

bool ShouldCancelDamaged(const Unit *unit)
{
    if (unit->player >= Limits::ActivePlayers || bw::players[unit->player].type != 1)
        return false;
    if (unit->Type().IsBuilding() && unit->ai != nullptr)
    {
        bw::player_ai[unit->player].unk_count = *bw::elapsed_seconds;
        ((BuildingAi *)unit->ai)->town->building_was_hit = 1;
        if (~unit->flags & UnitStatus::Completed)
        {
            if (unit->GetHealth() < unit->GetMaxHealth() / 4)
                return true;
        }
    }
    return false;
}

void EscapeStaticDefence(Unit *own, Unit *attacker)
{
    Assert(IsComputerPlayer(own->player));
    if (own->OrderType() == OrderId::Move)
        return;
    if (~attacker->flags & UnitStatus::Reacts && !(bw::player_ai[own->player].flags & 0x20))
    {
        switch (own->unit_id)
        {
            case UnitId::Arbiter:
            case UnitId::Wraith:
            case UnitId::Mutalisk:
                if (own->GetHealth() < own->GetMaxHealth() / 4)
                {
                    if (bw::Ai_ReturnToNearestBaseForced(own))
                        return;
                }
            break;
            case UnitId::Battlecruiser:
            case UnitId::Scout:
            case UnitId::Carrier:
                if (own->GetHealth() < own->GetMaxHealth() * 2 / 3)
                {
                    if (bw::Ai_ReturnToNearestBaseForced(own))
                        return;
                }
            break;
        }
    }
}

bool TryDamagedFlee(Unit *own, Unit *attacker)
{
    Assert(IsComputerPlayer(own->player));
    if (own->OrderType() == OrderId::Move)
        return false;
    if (own->Type() == UnitId::Carrier && (own->carrier.in_hangar_count + own->carrier.out_hangar_count) <= 2)
    {
        if (bw::Ai_ReturnToNearestBaseForced(own))
            return true;
    }
    if (own->Type() == UnitId::Lurker && !(own->flags & UnitStatus::Burrowed))
    {
        if (own->OrderType() != OrderId::Burrow && own->OrderType() != OrderId::Unburrow)
        {
            if (bw::Flee(own, attacker))
                return true;
        }
    }
    if (!own->IsInUninterruptableState() && (own->flags & UnitStatus::Reacts))
    {
        if (!bw::CanWalkHere(own, attacker->sprite->position.x, attacker->sprite->position.y)) // ._.
        {
            if (bw::Flee(own, attacker))
                return true;
        }
    }
    if (own->Type().IsWorker() && own->target != attacker)
    {
        if (!attacker->Type().IsWorker() && attacker->Type() != UnitId::Zergling)
        {
            // Sc calls this but it bugs and always return false
            // if (!FightsWithWorkers(player))
            return bw::Flee(own, attacker);
        }
    }
    return false;
}

bool TryReactionSpell(Unit *own, bool was_damaged)
{
    Assert(IsComputerPlayer(own->player));
    if (!own->IsInUninterruptableState())
        return bw::Ai_CastReactionSpell(own, was_damaged);
    else
        return false;
}

void Hide(Unit *own)
{
    if (own->Type() == UnitId::Lurker) {
        if (~own->flags & UnitStatus::Burrowed && own->OrderType() != OrderId::Burrow)
            own->IssueOrderTargetingNothing(OrderId::Burrow);
    } else {
        own->Ai_Cloak();
    }
}

} // namespace ai
