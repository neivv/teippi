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
