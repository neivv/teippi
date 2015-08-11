#include "nuke.h"

#include "ai.h"
#include "unit.h"
#include "sprite.h"
#include "order.h"
#include "offsets.h"
#include "yms.h"
#include "warn.h"
#include "strings.h"

#include "constants/sound.h"
#include "constants/iscript.h"

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
        StopMoving(this);
        OrderDone();
    }
}

void Unit::Order_NukeTrack()
{
    if (order_state == 0)
    {
        auto cmds = sprite->SetIscriptAnimation(IscriptAnim::Special1, true);
        if (!Empty(cmds))
            Warning("Unit::Order_NukeTrack did not handle all iscript commands for unit %x while setting anim to special 1", unit_id);
        building.ghost.nukedot = lone_sprites->AllocateLone(Sprite::NukeDot, related->order_target_pos, player);
        building.ghost.nukedot->elevation = sprite->elevation + 1;
        building.ghost.nukedot->UpdateVisibilityPoint();
        order_state = 6;
    }
    else if (order_state == 6)
    {
        if (related && related->order_state != 5)
            return;
        if (!order_queue_begin)
            AppendOrderTargetingGround(this, units_dat_return_to_idle_order[unit_id], 0);
        DoNextQueuedOrderIfAble(this);
        SetButtons(unit_id);
        auto cmds = sprite->SetIscriptAnimation(IscriptAnim::Idle, true);
        if (!Empty(cmds))
            Warning("Unit::Order_NukeTrack did not handle all iscript commands for unit %x while setting anim to idle", unit_id);
        cmds = building.ghost.nukedot->SetIscriptAnimation(IscriptAnim::Death, true);
        if (!Empty(cmds))
            Warning("Unit::Order_NukeTrack did not handle all iscript commands for nukedot (%x) while setting anim to death", building.ghost.nukedot->sprite_id);
        building.ghost.nukedot = nullptr;
        Ai_ReturnToNearestBaseForced(this);
    }
}

void Unit::Order_NukeGround()
{
    unk_move_waypoint = order_target_pos;
    int sight_range = GetSightRange(false) * 32;
    int dist = Distance(exact_position, Point32(order_target_pos) * 256) / 256;
    if (ai && dist <= sight_range * 3)
        Ai_Cloak();
    if (dist > sight_range)
    {
        if (move_target_update_timer == 0)
            ChangeMovementTarget(order_target_pos); // What, it is not moving?
    }
    else
    {
        StopMoving(this);
        if (position != unk_move_waypoint)
        {
            int diff = facing_direction - GetFacingDirection(sprite->position.x, sprite->position.y, unk_move_waypoint.x, unk_move_waypoint.y);
            if (diff < -1 || diff > 1)
                return;
        }
        Unit *silo = nullptr;
        for (Unit *unit : bw::first_player_unit[player])
        {
            if (unit->unit_id == NuclearSilo && unit->building.silo.has_nuke)
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
        Unit *nuke = silo->building.silo.nuke;
        silo->building.silo.nuke = nullptr;
        silo->building.silo.has_nuke = 0;
        PlaySound(Sound::NukeLaser, this, 1, 0);
        IssueOrderTargetingGround(nuke, Order::NukeLaunch, order_target_pos.x, order_target_pos.y);
        related = nuke;
        nuke->related = this;
        nuke->sprite->SetDirection32(0);
        ShowUnit(nuke);
        IssueOrderTargetingGround(this, Order::NukeTrack, sprite->position.x, sprite->position.y);
        if (!IsDisabled() || units_dat_flags[unit_id] & UnitFlags::Building) // Huh?
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
            PlaySound(Sound::NukeLaunch, this, 1, 0);
            // Wtf
            ChangeMovementTarget(Point(sprite->position.x, units_dat_dimensionbox[NuclearMissile].top));
            unk_move_waypoint = Point(sprite->position.x, units_dat_dimensionbox[NuclearMissile].top);
            order_timer = 90;
            order_state = 1;
        break;
        case 1:
            if (order_timer > 45 && IsStandingStill() == 0)
                return;
            PlaySound(Sound::Advisor_NukeLaunch + *bw::player_race, nullptr, 1, 0);
            PrintInfoMessage((*bw::stat_txt_tbl)->GetTblString(String::NuclearLaunchDetected));
            order_state = 2;
        break;
        case 2:
            if (order_timer && IsStandingStill() == 0)
                return;
            HideUnit(this);
            related->related = this;
            StopMoving(this);
            order_state = 3;
        break;
        case 3:
            if (flingy_flags & 0x2)
                return;
            SetIscriptAnimation_NoHandling(IscriptAnim::WarpIn, true, "Order_NukeLaunch state 3", results);
            order_state = 4;
        break;
        case 4:
        {
            if (~order_signal & 0x2)
                return;
            order_signal &= ~0x2;
            Point new_pos = order_target_pos;
            new_pos.x = std::max(units_dat_dimensionbox[NuclearMissile].left, new_pos.x);
            new_pos.y = std::max((int)units_dat_dimensionbox[NuclearMissile].top, new_pos.y - 0x140);
            MoveUnit(this, new_pos.x, new_pos.y);
            SetDirection((Flingy *)this, 0x80);
            ChangeMovementTarget(order_target_pos);
            unk_move_waypoint = order_target_pos;
            ShowUnit(this);
            order_state = 5;
        }
        break;
        case 5:
            if (!IsPointInArea(this, 10, move_target.x, move_target.y))
                return;
            target = this;
            SetIscriptAnimation_NoHandling(IscriptAnim::Special1, true, "Order_NukeLaunch state 5", results);
            order_state = 6;
        break;
        case 6:
            if (~order_signal & 0x1)
                return;
            order_signal &= ~0x1;
            order_flags |= 0x4;
            Kill(results);
        break;
    }
}
