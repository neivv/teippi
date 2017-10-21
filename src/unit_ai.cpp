#include "unit.h"
#include "ai.h"
#include "order.h"
#include "sprite.h"
#include "perfclock.h"
#include "constants/order.h"
#include "constants/tech.h"
#include "constants/unit.h"
#include "bullet.h"
#include "pathing.h"

using namespace Ai;

void Unit::Order_AiGuard()
{
    if (order_timer)
        return;
    STATIC_PERF_CLOCK(Order_AiGuard);
    order_timer = 0xf;
    if (bw::Ai_CastReactionSpell(this, 0))
        return;
    if (UpdateAttackTarget(this, false, true, false) == true)
        return;
    if (Type() == UnitId::Observer)
        return;
    if (ai && ai->type == 1)
    {
        GuardAi *guard = (GuardAi *)ai;
        if (guard->home != sprite->position)
        {
            if (Type() == UnitId::SiegeTank_Sieged)
                AppendOrderTargetingNothing(OrderId::TankMode);
            AppendOrderTargetingGround(OrderId::ComputerReturn, guard->home);
            OrderDone();
            return;
        }
    }
    IssueOrderTargetingNothing(OrderId::ComputerAi);
}

void Unit::Order_ComputerReturn()
{
    STATIC_PERF_CLOCK(Order_ComputerReturn);
    if (UpdateAttackTarget(this, false, true, false) == true)
        return;

    if (Type() == UnitId::Medic)
    {
        bw::Order_Medic(this);
        if (order != OrderId::ComputerReturn)
            return;
    }
    if (order_state == 0)
    {
        bool ret = ChangeMovementTarget(order_target_pos);
        unk_move_waypoint = order_target_pos;
        if (!ret)
            return;
        order_state = 1;
    }
    if (ai)
    {
        if (ai->type == 1)
        {
            if (IsStandingStill())
            {
                GuardAi *guard = (GuardAi *)ai;
                guard->home = sprite->position;
                IssueOrderTargetingNothing(OrderId::Guard);
            }
        }
        else
        {
            IssueOrderTargetingNothing(OrderId::ComputerAi);
        }
    }
}

void Unit::Ai_Cloak()
{
    using namespace UnitId;
    switch (unit_id)
    {
        case Ghost:
        case SarahKerrigan:
        case SamirDuran:
        case AlexeiStukov:
        case InfestedDuran:
        case InfestedKerrigan:
            if (bw::CanUseTech(TechId::PersonnelCloaking, this, player) == 1)
                Cloak(TechId::PersonnelCloaking);
        break;
        case Wraith:
        case TomKazansky:
            if (bw::CanUseTech(TechId::CloakingField, this, player) == 1)
                Cloak(TechId::CloakingField);
        break;
    }
}

bool Unit::Ai_TryReturnHome(bool dont_issue)
{
    STATIC_PERF_CLOCK(Unit_Ai_TryReturnHome);
    if (!ai || ai->type != 1)
        return false;

    GuardAi *guard = (GuardAi *)ai;
    if (Type().Race() == Race::Terran &&
            Type().Flags() & UnitFlags::Mechanical &&
            hitpoints != Type().HitPoints())
    {
        return false;
    }
    if (previous_attacker)
        return false;
    if (GetRegion() == ::GetRegion(guard->home))
        return false;
    if (bw::IsPointInArea(this, 0xc0, guard->home.x, guard->home.y))
        return false;
    if (target && IsInAttackRange(target))
        return false;
    if (order_queue_begin && order_queue_begin->order_id == OrderId::Patrol)
        return false;
    if (!dont_issue)
        IssueOrderTargetingGround(OrderId::ComputerReturn, guard->home);
    return true;
}

int Unit::Ai_RepairSomething(Ai::Town *town)
{
    Unit *incomplete= nullptr;
    Unit *damaged = nullptr;
    for (auto ai : town->first_building)
    {
        auto building = ai->parent;
        if (building == nullptr || building->related != nullptr)
            continue;
        if (!bw::Ai_IsBuildingSafe(building) || building->Type().Race() != Race::Terran)
            continue;
        if (~building->flags & UnitStatus::Completed)
        {
            incomplete = building;
            break;
        }
        auto hp = building->GetHitPoints();
        if (hp != building->GetMaxHitPoints())
        {
            if (damaged == nullptr || damaged->GetHitPoints() < hp)
            {
                damaged = building;
            }
        }

    }
    Unit *repair_this = nullptr;
    class OrderType repair_order;
    if (incomplete != nullptr)
    {
        repair_this = incomplete;
        repair_order = OrderId::ConstructingBuilding;
    }
    else if (damaged != nullptr)
    {
        repair_this = damaged;
        repair_order = OrderId::Repair;
    }
    else
    {
        if (town->building_scv == nullptr)
        {
            town->building_scv = this;
        }
        if (town->building_scv == this)
        {
            Unit *unit = bw::FindRepairableUnit(this);
            if (unit != nullptr)
            {
                repair_this = unit;
                repair_order = OrderId::Repair;
            }
        }
    }
    if (repair_this != nullptr)
    {
        IssueOrder(repair_order, repair_this, repair_this->sprite->position, UnitId::None);
        repair_this->related = this;
        return 1;
    }
    else
    {
        return 0;
    }
}
