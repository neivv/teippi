#include "ai.h"

#include "offsets_hooks.h"
#include "offsets.h"
#include "unit.h"
#include "order.h"
#include "player.h"
#include "tech.h"
#include "pathing.h"
#include "sprite.h"
#include "yms.h"
#include "patchmanager.h"
#include "bullet.h"
#include "unitlist.h"
#include "thread.h"
#include "assert.h"
#include "limits.h"
#include "rng.h"
#include "unitsearch.h"

#include "log.h"
#include "perfclock.h"

#include <string.h>

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
    if (medic->unit_id == Unit::Medic && medic == bw::player_ai[medic->player].free_medic)
    {
        bw::player_ai[medic->player].free_medic = 0;
        for (Unit *unit : bw::first_player_unit[medic->player])
        {
            if (unit->unit_id == Unit::Medic && unit->ai)
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
        AttackUnit(unit, new_target, true, accept_if_sieged);

    if (unit->HasHangar() && !unit->IsInAttackRange(new_target))
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
    if (unit->order_queue_begin && unit->order_queue_begin->order_id == Order::Patrol)
        return false;
    if (unit->order == Order::Pickup4)
    {
        must_reach = true;
        if (unit->previous_attacker == nullptr && unit->order_queue_begin && unit->order_queue_begin->order_id == Order::AttackUnit)
            return false;
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
    if (orders_dat_use_weapon_targeting[unit->order] && unit->target != nullptr)
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

Script::Script(uint32_t player_, uint32_t pos_, bool bwscript, Rect32 *area_) : pos(pos_), player(player_)
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

Script *__stdcall CreateScript(uint32_t player, uint32_t pos, bool bwscript)
{
    REG_ESI(Rect32 *, area);
    return new Script(player, pos, bwscript, area);
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
                ProgressAiScript(script);
        }
        script = next;
    }
}

void RemoveTownGasReferences(Unit *unit)
{
    if (units_dat_flags[unit->unit_id] & UnitFlags::ResourceContainer)
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

void __fastcall RemoveUnitAi(Unit *unit, bool unk)
{
    // If ai is not guard, it will be deleted by following funcs
    bool is_guard = unit->ai && unit->ai->type == 1;
    RemoveFromAiStructs(unit, unk);
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

void __stdcall PreCreateGuardAi(int unit_id, int x)
{
    REG_EDI(int, y);
    REG_ESI(int, player);
    y &= 0xffff;
    x &= 0xffff;
    player &= 0xff;
    unit_id &= 0xffff;

    GuardAi *ai = CreateGuardAi(player, 0, unit_id, Point(x, y));
    ai->list.Add(needed_guards[player]);
}

void AddGuardAiToUnit(Unit *unit)
{
    if (!IsActivePlayer(unit->player))
        return;
    if (bw::players[unit->player].type != 1)
        return;
    if (units_dat_ai_flags[unit->unit_id] & 0x2)
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

void __stdcall UpdateGuardNeeds(int player)
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
                    int unit_id = ai->unit_id;
                    if (unit_id == Unit::SiegeTank_Sieged)
                        unit_id = Unit::SiegeTankTankMode;
                    time = units_dat_build_time[unit_id];
                    switch (unit_id)
                    {
                        case Unit::Guardian:
                        case Unit::Devourer:
                            time += units_dat_build_time[Unit::Mutalisk];
                        break;
                        case Unit::Hydralisk:
                            time += units_dat_build_time[Unit::Hydralisk];
                        break;
                        case Unit::Archon:
                            time += units_dat_build_time[Unit::HighTemplar];
                        break;
                        case Unit::DarkArchon:
                            time += units_dat_build_time[Unit::DarkTemplar];
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
                if (ai->unit_id == Unit::Zergling || ai->unit_id == Unit::Scourge)
                {
                    Unit *unit = FindNearestAvailableMilitary(ai->unit_id, ai->unk_pos.x, ai->unk_pos.y, player);
                    if (unit)
                    {
                        ai->list.Change(needed_guards[player], bw::first_guard_ai[player]);
                        RemoveUnitAi(unit, false);
                        ai->home = ai->unk_pos;
                        ai->parent = unit;
                        unit->ai = (UnitAi *)ai;
                        IssueOrderTargetingNothing(unit, Order::ComputerAi);
                        continue;
                    }
                }
                Ai_GuardRequest(ai, player);
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

void Hook_DeleteGuardNeeds()
{
    REG_ESI(int, player);
    player &= 0xff;
    DeleteGuardNeeds(player);
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
    if (unit->IsWorker())
    {
        WorkerAi *ai = new WorkerAi;
        ai->list.Add(town->first_worker);
        town->worker_count++;
        ai->parent = unit;
        ai->town = town;
        unit->ai = (UnitAi *)ai;
        unit->worker.current_harvest_target = 0;
    }
    else if (((units_dat_flags[unit->unit_id] & UnitFlags::Building) && (unit->unit_id != Unit::VespeneGeyser)) ||
             (unit->unit_id == Unit::Larva) || (unit->unit_id == Unit::Egg) || (unit->unit_id == Unit::Overlord))
    {
        BuildingAi *ai = new BuildingAi;
        ai->list.Add(town->first_building);
        ai->parent = unit;
        ai->town = town;
        unit->ai = (UnitAi *)ai;

        if (unit->unit_id == Unit::Hatchery || unit->unit_id == Unit::Lair || unit->unit_id == Unit::Hive)
        {
            if (unit->flags & UnitStatus::Completed)
            {
                IssueOrderTargetingNothing(unit, Order::ComputerAi);
                unit->IssueSecondaryOrder(Order::SpreadCreep);
            }
        }
        if (~bw::player_ai[town->player].flags & 0x20)
        {
            Ai_UpdateRegionStateUnk(town->player, unit);
        }
        else if ((unit->flags & UnitStatus::Building))
        {
            switch (unit->unit_id)
            {
                case Unit::MissileTurret:
                case Unit::Bunker:
                case Unit::CreepColony:
                case Unit::SunkenColony:
                case Unit::SporeColony:
                case Unit::Pylon:
                case Unit::PhotonCannon:
                break;
                default:
                    Ai_UpdateRegionStateUnk(town->player, unit);
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
        int main_building = Unit::None;
        switch (bw::players[town->player].race)
        {
            case 0:
                main_building = Unit::Hatchery;
            break;
            case 1:
                main_building = Unit::CommandCenter;
            break;
            case 2:
                main_building = Unit::Nexus;
            break;
        }
        if (unit->unit_id == main_building || unit->IsResourceDepot())
        {
            if (!town->main_building)
                town->main_building = unit;
        }
        else if (unit->unit_id == Unit::Extractor || unit->unit_id == Unit::Refinery || unit->unit_id == Unit::Assimilator)
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

void AddUnitAi_Hook()
{
    REG_EBX(Unit *, unit);
    REG_EDI(Town *, town);
    AddUnitAi(unit, town);
}

void DeleteWorkerAi()
{
    REG_EAX(WorkerAi *, ai);
    REG_EDX(DataList<WorkerAi> *, first_ai);
    ai->list.Remove(*first_ai);
    delete ai;
}

void DeleteBuildingAi()
{
    REG_EAX(BuildingAi *, ai);
    REG_EDX(DataList<BuildingAi> *, first_ai);
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

void Hook_DeleteMilitaryAi()
{
    REG_EAX(MilitaryAi *, ai);
    REG_EDX(Region *, region);
    DeleteMilitaryAi(ai, region);
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
    int order;
    unit->unit_id == Unit::Medic ? order = Order::HealMove : order = Order::AiAttackMove;
    ProgressMilitaryAi(unit, order, pathing_region->x >> 8, pathing_region->y >> 8);
    if (IsInAttack(unit))
        Ai_UpdateSlowestUnitInRegion(region);
}

void __stdcall Hook_AddMilitaryAi(bool always_use_this_region)
{
    REG_EBX(Unit *, unit);
    REG_EAX(Region *, region);
    AddMilitaryAi(unit, region, always_use_this_region);
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

Town *__stdcall CreateTown(int x, int y)
{
    REG_EBX(int, player);
    player &= 0xff;
    x &= 0xffff;
    y &= 0xffff;
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
        if (units_dat_flags[unit->unit_id] & UnitFlags::ResourceContainer)
            unit->resource.ai_unk = 0;
    }
    if (town->resource_area)
        (*bw::resource_areas).areas[town->resource_area].flags &= ~0x2;

    town->list.Remove(bw::active_ai_towns[town->player]);
    delete town;
}

static void DeleteTownIfNeeded(Town *town)
{
    if (!TryBeginDeleteTown(town))
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
    if (unit->unit_id == Unit::Arbiter && unit->ai)
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
    Unit *target = 0;
    int own_id = unit->unit_id;
    bool can_attack = unit->HasWayOfAttacking();
    for (int i = Rand(0x36) % target_player_count; i < target_player_count; i++)
    {
        int player = target_players[i];
        uint32_t dist = 0xffffffff;

        for (Unit *enemy : bw::first_player_unit[player])
        {
            if (!enemy->sprite || enemy->order == Order::Die)
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
        if (target->unit_id == Unit::Interceptor && target->interceptor.parent)
            target = target->interceptor.parent;
        unit->Attack(target);
        if (unit->HasSubunit())
            unit->subunit->Attack(target);

        // Is this called ever because sieged tanks don't seem to react?
        if (unit->ai && own_id == Unit::SiegeTank_Sieged && unit->order != Order::TankMode && !unit->IsInAttackRange(target))
        {
            if (HasTargetInRange(unit))
            {
                UpdateAttackTarget(unit, true, true, false);
            }
            else
            {
                IssueOrderTargetingNothing(unit, Order::TankMode);
                AppendOrder(unit, units_dat_attack_unit_order[Unit::SiegeTankTankMode], target->sprite->position.AsDword(), target, Unit::None, 0);
            }
        }
    }
    else
    {
        unit->order_flags |= 0x1;
        if (unit->order == Order::Die) // ???
        {
            for (Order *order = unit->order_queue_end; order; order = unit->order_queue_end)
            {
                if (!orders_dat_interruptable[order->order_id] && order->order_id != Order::Move)
                    break;
                unit->DeleteOrder(order);
            }
            AddOrder(unit, Order::Move, nullptr, Unit::None, target->sprite->position.AsDword(), target);
        }
        unit->DoNextQueuedOrder();
    }

    RemoveUnitAi(unit, false);
}

void SetFinishedUnitAi()
{
    REG_EAX(Unit *, unit);
    REG_ECX(Unit *, parent);
    if (unit->unit_id == Unit::Guardian || unit->unit_id == Unit::Devourer)
        return;

    if (unit->flags & UnitStatus::Hallucination)
    {
        AddMilitaryAi(unit, GetAiRegion(unit), true);
        return;
    }
    else if (unit->unit_id == Unit::Lurker || unit->unit_id == Unit::Archon || unit->unit_id == Unit::DarkArchon
             || unit->unit_id == Unit::NuclearMissile || unit->unit_id == Unit::Observer)
    {
        return;
    }
    if (!parent->ai)
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

int __stdcall AddToRegionMilitary(int unit_id, int unkx6)
{
    REG_EAX(Region *, region);
    int player = region->player;
    Region *other = bw::player_ai_regions[player] + region->target_region_id;
    Pathing::Region *pathreg = (*bw::pathing)->regions + region->region_id;
    int x = pathreg->x >> 8, y = pathreg->y >> 8;
    UnitAi *base_ai = Ai_FindNearestActiveMilitaryAi(region, unit_id, x, y, other);
    if (!base_ai)
    {
        base_ai = Ai_FindNearestMilitaryOrSepContAi(region, unkx6, unit_id, x, y, other);
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
    Ai_RecalculateRegionStrength(old_region);
    return 1;
}

bool __stdcall DoesNeedGuardAi(int unit_id)
{
    REG_EBX(int, player);
    player &= 0xff;
    unit_id &= 0xffff;
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

void ForceGuardAiRefresh()
{
    REG_EAX(int, player);
    REG_EDX(int, unit_id);
    player &= 0xff;
    unit_id &= 0xffff;
    for (GuardAi *ai = needed_guards[player]; ai; ai = ai->list.next)
    {
        if (!ai->previous_update)
            continue;
        int ai_unit_id = ai->unit_id;
        if (ai_unit_id == Unit::SiegeTank_Sieged)
            ai_unit_id = Unit::SiegeTankTankMode;
        if (unit_id != ai_unit_id)
            continue;
        ai->previous_update = 1;
    }
}

Unit *FindAvailableUnit(int player, int unit_id)
{
    for (Unit *unit : bw::first_player_unit[player])
    {
        if (unit->unit_id != unit_id)
            continue;
        if (!unit->sprite || unit->order == Order::Die || unit->target || unit->previous_attacker || !unit->ai)
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
    Ai_PopSpendingRequestResourceNeeds(player, also_available_resources);
    Ai_PopSpendingRequest(player);
}

bool Merge(int player, int unit_id, int order, const bool check_energy)
{
    int count = 0;
    Unit *units[2];
    Unit *high_energy = 0;
    for (Unit *unit : bw::first_player_unit[player])
    {
        if (unit->unit_id != unit_id || unit->order == order)
            continue;
        if (!unit->sprite || unit->order == Order::Die || unit->target || unit->previous_attacker || !unit->ai)
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

    IssueOrderTargetingUnit_Simple(units[0], order, units[1]);
    IssueOrderTargetingUnit_Simple(units[1], order, units[0]);
    return true;
}

static bool IsAtBuildLimit(int player, int unit_id)
{
    int limit = bw::player_ai[player].unit_build_limits[unit_id];
    if (!limit)
        return false;
    if (limit == 255)
        return true;
    int count = bw::all_units_count[unit_id][player];
    if (unit_id == Unit::SiegeTankTankMode)
        count += bw::all_units_count[Unit::SiegeTank_Sieged][player];
    return count >= limit;
}

int __stdcall SpendReq_TrainUnit(int player, Unit *unit, bool no_retry)
{
    SpendingRequest *req = bw::player_ai[player].requests;
    Unit *parent = unit;
    switch (req->unit_id)
    {
        case Unit::Guardian:
        case Unit::Devourer:
        case Unit::Lurker:
            if (req->unit_id == Unit::Lurker)
                parent = FindAvailableUnit(player, Unit::Hydralisk);
            else
                parent = FindAvailableUnit(player, Unit::Mutalisk);
            if (!parent)
            {
                PopSpendingRequest(player, false);
                return 0;
            }
        break;
        case Unit::Archon:
            if (!Merge(player, Unit::HighTemplar, Order::WarpingArchon, true))
                PopSpendingRequest(player, false);
            else
                PopSpendingRequest(player, true);
            return 0;
        break;
        case Unit::DarkArchon:
            if (!Merge(player, Unit::DarkTemplar, Order::WarpingDarkArchon, false))
                PopSpendingRequest(player, false);
            else
                PopSpendingRequest(player, true);
            return 0;
        break;
    }

    if (CheckUnitDatRequirements(parent, req->unit_id, player) == 1)
    {
        if (!Ai_DoesHaveResourcesForUnit(player, req->unit_id))
        {
            if (~bw::player_ai[player].flags & 0x20)
                PopSpendingRequest(player, false);
            return 0;
        }
        if (IsAtBuildLimit(player, req->unit_id))
        {
            PopSpendingRequest(player, false);
            return 0;
        }
        if (parent == unit)
        {
            if (!Ai_TrainUnit(parent, req->unit_id, req->type, req->val))
                return 0;
        }
        else // lurk/guard/devo morph
        {
            if (!PrepareBuildUnit(parent, req->unit_id))
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
        if (parent->unit_id == Unit::Larva || parent->unit_id == Unit::Hydralisk || parent->unit_id == Unit::Mutalisk)
            IssueOrderTargetingNothing(parent, Order::UnitMorph);
        else
            parent->IssueSecondaryOrder(Order::Train);
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

void __stdcall SuicideMission(int player)
{
    REG_EAX(int, random);
    random &= 0xff;
    if (random)
    {
        for (Unit *unit : bw::first_player_unit[player])
        {
            if (!unit->sprite || unit->order == Order::Die || units_dat_flags[unit->unit_id] & UnitFlags::Subunit)
                continue;
            if (unit->unit_id == Unit::NuclearMissile || unit->unit_id == Unit::Larva || unit->sprite->IsHidden())
                continue;
            if (Ai_ShouldKeepTarget(unit, nullptr))
                continue;
            SetSuicideTarget(unit);
        }
    }
    else if (!bw::player_ai[player].strategic_suicide)
    {
        Region *unk_region = nullptr;
        for (Unit *unit : bw::first_player_unit[player])
        {
            if (!unit->sprite || unit->order == Order::Die || !unit->ai || unit->IsTransport())
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
                Ai_EndAllMovingToAttack(player);
                if (Ai_AttackTo(player, unit->sprite->position.x, unit->sprite->position.y, 0, 1))
                    continue;
                bw::player_ai[player].strategic_suicide = 0x18;
                unk_region = bw::player_ai_regions[player] + (bw::player_ai[player].unk_region - 1);
            }
            RemoveUnitAi(unit, 0);
            AddMilitaryAi(unit, unk_region, true);
        }
    }
}

void AiScript_SwitchRescue()
{
    REG_EDX(int, player);
    player &= 0xff;
    bw::players[player].type = 3;
    for (unsigned i = 0; i < Limits::Players; i++)
    {
        bw::alliances[player][i] = 1;
        bw::alliances[i][player] = 1;
    }
    for (Unit *unit : bw::first_player_unit[player])
    {
        if (!unit->sprite || unit->order == Order::Die)
            continue;
        RemoveUnitAi(unit, false);
        unit->order_flags |= 0x1;
        while (unit->order_queue_end)
        {
            Order *order = unit->order_queue_end;
            if (!orders_dat_interruptable[order->order_id] && order->order_id != Order::RescuePassive)
                break;
            unit->DeleteOrder(order);
        }
        AddOrder(unit, Order::RescuePassive, nullptr, Unit::None, 0, nullptr);
        unit->DoNextQueuedOrder();
    }
}

static void __stdcall AiScript_MoveDt(int x, int y)
{
    REG_EAX(int, player);
    player &= 0xff;
    for (Unit *unit : bw::first_player_unit[player])
    {
        if (!unit->sprite || unit->order == Order::Die)
            continue;
        if (unit->unit_id != Unit::DarkTemplar && unit->unit_id != Unit::DarkTemplarHero) // O.o Checks normal dt
            continue;
        if (unit->ai)
            RemoveUnitAi(unit, false);
        AddMilitaryAi(unit, GetAiRegion(unit), true);
    }
}

static void AiScript_Stop()
{
    REG_EAX(Script *, script);
    delete script;
}

extern "C" void __stdcall AiScript_InvalidOpcode(Script * script)
{
    delete script;
}

static void MedicRemoveHook()
{
    REG_EAX(Unit *, medic);
    MedicRemove(medic);
}

static int RemoveWorkerOrBuildingAi(Unit *unit, bool only_building)
{
    // Seems to be an unused arg
    Assert(!only_building);
    RemoveTownGasReferences(unit);
    if (!unit->ai || (unit->ai->type != 2 && unit->ai->type != 3))
        return 0;
    Town *town;
    if (!only_building && unit->IsWorker())
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

static int __stdcall RemoveWorkerOrBuildingAi_Hook(bool only_building)
{
    REG_EDI(Unit *, unit);
    return RemoveWorkerOrBuildingAi(unit, only_building);
}

static void __stdcall AddGuardAiToUnit_Hook(Unit *unit)
{
    AddGuardAiToUnit(unit);
}

static void RemoveTownGasReferences_Hook()
{
    REG_ECX(Unit *, unit);
    RemoveTownGasReferences(unit);
}

void RemoveLimits(Common::PatchContext *patch)
{
    patch->JumpHook(bw::CreateAiScript, CreateScript);
    patch->JumpHook(bw::RemoveAiTownGasReferences, RemoveTownGasReferences_Hook);
    patch->JumpHook(bw::DeleteWorkerAi, DeleteWorkerAi);
    patch->JumpHook(bw::DeleteBuildingAi, DeleteBuildingAi);
    patch->JumpHook(bw::AddUnitAi, AddUnitAi_Hook);
    patch->JumpHook(bw::DeleteMilitaryAi, Hook_DeleteMilitaryAi);
    patch->JumpHook(bw::CreateAiTown, CreateTown);
    patch->JumpHook(bw::PreCreateGuardAi, PreCreateGuardAi);
    patch->JumpHook(bw::AddGuardAiToUnit, AddGuardAiToUnit_Hook);
    patch->JumpHook(bw::RemoveUnitAi, RemoveUnitAi);
    patch->JumpHook(bw::RemoveWorkerOrBuildingAi, RemoveWorkerOrBuildingAi_Hook);

    patch->JumpHook(bw::Ai_DeleteGuardNeeds, Hook_DeleteGuardNeeds);
    patch->JumpHook(bw::Ai_UpdateGuardNeeds, UpdateGuardNeeds);
    patch->JumpHook(bw::Ai_SetFinishedUnitAi, SetFinishedUnitAi);
    patch->JumpHook(bw::Ai_AddToRegionMilitary, AddToRegionMilitary);
    patch->JumpHook(bw::DoesNeedGuardAi, DoesNeedGuardAi);
    patch->JumpHook(bw::ForceGuardAiRefresh, ForceGuardAiRefresh);
    patch->JumpHook(bw::AiSpendReq_TrainUnit, SpendReq_TrainUnit);

    patch->JumpHook(bw::AiScript_MoveDt, AiScript_MoveDt);
    patch->JumpHook(bw::AddMilitaryAi, Hook_AddMilitaryAi);
    patch->JumpHook(bw::AiScript_InvalidOpcode, AiScript_InvalidOpcode);
    patch->JumpHook(bw::AiScript_Stop, AiScript_Stop);

    patch->JumpHook(bw::Ai_SuicideMission, SuicideMission);
    patch->JumpHook(bw::MedicRemove, MedicRemoveHook);
    patch->JumpHook(bw::AiScript_SwitchRescue, AiScript_SwitchRescue);

    unsigned char patch1[] = { 0x56 };
    unsigned char patch2[] = { 0xeb, 0xef };
    patch->Patch(bw::AiScript_InvalidOpcode, patch1, 1, PATCH_REPLACE);
    patch->Patch((uint8_t *)(bw::AiScript_InvalidOpcode.raw_pointer()) + 1, patch2, 5, PATCH_NOP);
    patch->Patch((uint8_t *)(bw::AiScript_InvalidOpcode.raw_pointer()) + 1, (void *)&AiScript_InvalidOpcode, 5, PATCH_CALLHOOK);
    patch->Patch((uint8_t *)(bw::AiScript_InvalidOpcode.raw_pointer()) + 6, patch2, 2, PATCH_REPLACE);
}

bool ShouldCancelDamaged(const Unit *unit)
{
    if (unit->player >= Limits::ActivePlayers || bw::players[unit->player].type != 1)
        return false;
    if ((units_dat_flags[unit->unit_id] & UnitFlags::Building) && unit->ai)
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
    if (own->order == Order::Move)
        return;
    if (~attacker->flags & UnitStatus::Reacts && !(bw::player_ai[own->player].flags & 0x20))
    {
        switch (own->unit_id)
        {
            case Unit::Arbiter:
            case Unit::Wraith:
            case Unit::Mutalisk:
                if (own->GetHealth() < own->GetMaxHealth() / 4)
                {
                    if (Ai_ReturnToNearestBaseForced(own))
                        return;
                }
            break;
            case Unit::Battlecruiser:
            case Unit::Scout:
            case Unit::Carrier:
                if (own->GetHealth() < own->GetMaxHealth() * 2 / 3)
                {
                    if (Ai_ReturnToNearestBaseForced(own))
                        return;
                }
            break;
        }
    }
}

bool TryDamagedFlee(Unit *own, Unit *attacker)
{
    Assert(IsComputerPlayer(own->player));
    if (own->order == Order::Move)
        return false;
    if (own->unit_id == Unit::Carrier && (own->carrier.in_hangar_count + own->carrier.out_hangar_count) <= 2)
    {
        if (Ai_ReturnToNearestBaseForced(own))
            return true;
    }
    if (own->unit_id == Unit::Lurker && !(own->flags & UnitStatus::Burrowed))
    {
        if (own->order != Order::Burrow && own->order != Order::Unburrow)
        {
            if (Flee(own, attacker))
                return true;
        }
    }
    if (!own->IsInUninterruptableState() && (own->flags & UnitStatus::Reacts))
    {
        if (!CanWalkHere(own, attacker->sprite->position.x, attacker->sprite->position.y)) // ._.
        {
            if (Flee(own, attacker))
                return true;
        }
    }
    if (own->IsWorker() && own->target != attacker)
    {
        if (!attacker->IsWorker() && attacker->unit_id != Unit::Zergling)
        {
            // Sc calls this but it bugs and always return false
            // if (!FightsWithWorkers(player))
            return Flee(own, attacker);
        }
    }
    return false;
}

bool TryReactionSpell(Unit *own, bool was_damaged)
{
    Assert(IsComputerPlayer(own->player));
    if (!own->IsInUninterruptableState())
        return Ai_CastReactionSpell(own, was_damaged);
    else
        return false;
}

void Hide(Unit *own)
{
    switch (own->unit_id)
    {
        case Unit::Ghost:
        case Unit::SarahKerrigan:
        case Unit::SamirDuran:
        case Unit::AlexeiStukov:
        case Unit::InfestedDuran:
        case Unit::InfestedKerrigan:
            if (CanUseTech(Tech::PersonnelCloaking, own, own->player) == 1)
                own->Cloak(Tech::PersonnelCloaking);
        break;
        case Unit::Wraith:
        case Unit::TomKazansky:
            if (CanUseTech(Tech::CloakingField, own, own->player) == 1)
                own->Cloak(Tech::CloakingField);
        break;
        case Unit::Lurker:
            if (~own->flags & UnitStatus::Burrowed && own->order != Order::Burrow)
                IssueOrderTargetingNothing(own, Order::Burrow);
        break;
    }
}

} // namespace ai
