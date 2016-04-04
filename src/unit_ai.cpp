#include "unit.h"
#include "ai.h"
#include "order.h"
#include "sprite.h"
#include "perfclock.h"
#include "constants/tech.h"
#include "bullet.h"
#include "pathing.h"

using namespace Ai;

void Unit::Order_AiGuard()
{
    if (order_timer)
        return;
    STATIC_PERF_CLOCK(Order_AiGuard);
    order_timer = 0xf;
    if (Ai_CastReactionSpell(this, 0))
        return;
    if (UpdateAttackTarget(this, false, true, false) == true)
        return;
    if (unit_id == Observer)
        return;
    if (ai && ai->type == 1)
    {
        GuardAi *guard = (GuardAi *)ai;
        if (guard->home != sprite->position)
        {
            if (unit_id == SiegeTank_Sieged)
                AppendOrderTargetingNothing(Order::TankMode);
            AppendOrderTargetingGround(Order::ComputerReturn, guard->home);
            OrderDone();
            return;
        }
    }
    IssueOrderTargetingNothing(Order::ComputerAi);
}

void Unit::Order_ComputerReturn()
{
    STATIC_PERF_CLOCK(Order_ComputerReturn);
    if (UpdateAttackTarget(this, false, true, false) == true)
        return;

    if (unit_id == Medic)
    {
        Order_MedicIdle(this);
        if (order != Order::ComputerReturn)
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
                IssueOrderTargetingNothing(Order::Guard);
            }
        }
        else
        {
            IssueOrderTargetingNothing(Order::ComputerAi);
        }
    }
}

void Unit::Ai_Cloak()
{
    switch (unit_id)
    {
        case Unit::Ghost:
        case Unit::SarahKerrigan:
        case Unit::SamirDuran:
        case Unit::AlexeiStukov:
        case Unit::InfestedDuran:
        case Unit::InfestedKerrigan:
            if (CanUseTech(Tech::PersonnelCloaking, this, player) == 1)
                Cloak(Tech::PersonnelCloaking);
        break;
        case Unit::Wraith:
        case Unit::TomKazansky:
            if (CanUseTech(Tech::CloakingField, this, player) == 1)
                Cloak(Tech::CloakingField);
        break;
    }
}

bool Unit::Ai_TryReturnHome(bool dont_issue)
{
    STATIC_PERF_CLOCK(Unit_Ai_TryReturnHome);
    if (!ai || ai->type != 1)
        return false;

    GuardAi *guard = (GuardAi *)ai;
    if (GetRace() == Race::Terran && units_dat_flags[unit_id] & UnitFlags::Mechanical && hitpoints != units_dat_hitpoints[unit_id])
        return false;
    if (previous_attacker)
        return false;
    if (GetRegion() == ::GetRegion(guard->home))
        return false;
    if (IsPointInArea(this, 0xc0, guard->home.x, guard->home.y))
        return false;
    if (target && IsInAttackRange(target))
        return false;
    if (order_queue_begin && order_queue_begin->order_id == Order::Patrol)
        return false;
    if (!dont_issue)
        IssueOrderTargetingGround(Order::ComputerReturn, guard->home);
    return true;
}
