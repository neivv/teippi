#include "nuke.h"

#include "constants/iscript.h"
#include "constants/order.h"
#include "constants/sprite.h"
#include "constants/sound.h"
#include "ai.h"
#include "offsets.h"
#include "order.h"
#include "strings.h"
#include "sprite.h"
#include "unit.h"
#include "yms.h"
#include "warn.h"

#include <algorithm>

void Unit::Order_NukeUnit()
{
    if (target)
    {
        order_target_pos = target->sprite->position;
        Order_NukeGround();
    }
    else
    {
        bw::StopMoving(this);
        OrderDone();
    }
}

void Unit::Order_NukeTrack()
{
    if (order_state == 0)
    {
        SetIscriptAnimation(Iscript::Animation::Special1, true, "Order_NukeTrack state 0", nullptr);
        ghost.nukedot = lone_sprites->AllocateLone(SpriteId::NukeDot, related->order_target_pos, player);
        ghost.nukedot->elevation = sprite->elevation + 1;
        ghost.nukedot->UpdateVisibilityPoint();
        order_state = 6;
    }
    else if (order_state == 6)
    {
        if (related && related->order_state != 5)
            return;
        if (order_queue_begin == nullptr)
            AppendOrderTargetingNothing(Type().ReturnToIdleOrder());
        bw::DoNextQueuedOrderIfAble(this);
        SetButtons(unit_id);
        SetIscriptAnimation(Iscript::Animation::Idle, true, "Order_NukeTrack state 6", nullptr);
        ghost.nukedot->SetIscriptAnimation_Lone(Iscript::Animation::Death, true, MainRng(), "Unit::Order_NukeTrack");
        ghost.nukedot = nullptr;
        bw::Ai_ReturnToNearestBaseForced(this);
    }
}

void Unit::Order_NukeGround()
{
    unk_move_waypoint = order_target_pos;
    int sight_range = GetSightRange(false) * 32;
    int dist = Distance(exact_position, Point32(order_target_pos) * 256 + Point32(128, 128)) / 256;
    if (ai && dist <= sight_range * 3)
        Ai_Cloak();
    if (dist > sight_range)
    {
        if (move_target_update_timer == 0)
            ChangeMovementTarget(order_target_pos); // What, it is not moving?
    }
    else
    {
        bw::StopMoving(this);
        if (position != unk_move_waypoint)
        {
            auto pos = sprite->position;
            int diff = facing_direction -
                bw::GetFacingDirection(pos.x, pos.y, unk_move_waypoint.x, unk_move_waypoint.y);
            if (diff < -1 || diff > 1)
                return;
        }
        Unit *silo = nullptr;
        for (Unit *unit : bw::first_player_unit[player])
        {
            if (unit->Type() == UnitId::NuclearSilo && unit->silo.has_nuke)
            {
                silo = unit;
                break;
            }
        }
        if (!silo)
        {
            OrderDone();
            return;
        }
        Unit *nuke = silo->silo.nuke;
        silo->silo.nuke = nullptr;
        silo->silo.has_nuke = 0;
        bw::PlaySound(Sound::NukeLaser, this, 1, 0);
        nuke->IssueOrderTargetingGround(OrderId::NukeLaunch, order_target_pos);
        related = nuke;
        nuke->related = this;
        nuke->sprite->SetDirection32(0);
        bw::ShowUnit(nuke);
        IssueOrderTargetingGround(OrderId::NukeTrack, sprite->position);
        if (!IsDisabled() || Type().IsBuilding()) // Huh?
            buttons = 0xed;
        RefreshUi();
    }
}

void Unit::Order_NukeLaunch(ProgressUnitResults *results)
{
    if (order_state < 5 && !related)
    {
        Kill(results);
        return;
    }
    switch (order_state)
    {
        case 0:
            bw::PlaySound(Sound::NukeLaunch, this, 1, 0);
            // Wtf
            ChangeMovementTarget(Point(sprite->position.x, UnitId::NuclearMissile.DimensionBox().top));
            unk_move_waypoint = Point(sprite->position.x, UnitId::NuclearMissile.DimensionBox().top);
            order_timer = 90;
            order_state = 1;
        break;
        case 1:
            if (order_timer > 45 && IsStandingStill() == 0)
                return;
            bw::PlaySound(Sound::Advisor_NukeLaunch + *bw::player_race, nullptr, 1, 0);
            bw::PrintInfoMessage((*bw::stat_txt_tbl)->GetTblString(String::NuclearLaunchDetected));
            order_state = 2;
        break;
        case 2:
            if (order_timer && IsStandingStill() == 0)
                return;
            bw::HideUnit(this);
            related->related = this;
            bw::StopMoving(this);
            order_state = 3;
        break;
        case 3:
            if (flingy_flags & 0x2)
                return;
            SetIscriptAnimation(Iscript::Animation::WarpIn, true, "Order_NukeLaunch state 3", results);
            order_state = 4;
        break;
        case 4:
        {
            if (~order_signal & 0x2)
                return;
            order_signal &= ~0x2;
            Point new_pos = order_target_pos;
            new_pos.x = std::max(UnitId::NuclearMissile.DimensionBox().left, new_pos.x);
            new_pos.y = std::max((int)UnitId::NuclearMissile.DimensionBox().top, new_pos.y - 0x140);
            bw::MoveUnit(this, new_pos.x, new_pos.y);
            bw::SetDirection(AsFlingy(), 0x80);
            ChangeMovementTarget(order_target_pos);
            unk_move_waypoint = order_target_pos;
            bw::ShowUnit(this);
            order_state = 5;
        }
        break;
        case 5:
            if (!bw::IsPointInArea(this, 10, move_target.x, move_target.y))
                return;
            target = this;
            SetIscriptAnimation(Iscript::Animation::Special1, true, "Order_NukeLaunch state 5", results);
            order_state = 6;
        break;
        case 6:
            if (~order_signal & 0x1)
                return;
            order_signal &= ~0x1;
            Remove(results);
        break;
    }
}
