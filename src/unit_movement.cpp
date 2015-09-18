#include "unit.h"

#include "pathing.h"
#include "flingy.h"
#include "sprite.h"
#include "perfclock.h"
#include "unitsearch.h"

void Unit::FinishMovement_Fast()
{
    if (MoveFlingy())
    {
        unit_search->ChangeUnitPosition_Fast(this, *bw::new_flingy_x - *bw::old_flingy_x, *bw::new_flingy_y - *bw::old_flingy_y);
        if (*bw::new_flingy_x / 32 != *bw::old_flingy_x / 32 || *bw::new_flingy_y / 32 != *bw::old_flingy_y / 32)
            *bw::reveal_unit_area = 1;
    }
}

// Position is really signed 16-bit, which would make things a lot easier.
// But if I *someday* wish to have 2048x2048 maps, let's assume it to be unsigned 16 :p
// Then it would make more sense to check the underflow already during ProgressMove,
// and there would be no need for arbitary 0x8000 constants
static bool DidUnderflow(uint16_t new_val, uint16_t old_val)
{
    return new_val > old_val && new_val - old_val > 0x8000;
}

void Unit::MovementState_Flyer()
{
    STATIC_PERF_CLOCK(Unit_MovementState_Flyer);
    ForceMoveTargetInBounds(this);

    *bw::current_flingy_flags = flingy_flags;
    ChangeDirectionToMoveWaypoint(this);
    ProgressSpeed(this);
    UpdateIsMovingFlag(this);
    ProgressMove(this);
    *bw::previous_flingy_flags = flingy_flags;
    flingy_flags = *bw::current_flingy_flags;

    bool repulsed = ProgressRepulse(this);

    Rect16 &dbox = units_dat_dimensionbox[unit_id];
    if (*bw::new_flingy_x - dbox.left < 0 || DidUnderflow(*bw::new_flingy_x, position.x))
        *bw::new_flingy_x = dbox.left;
    else if (*bw::new_flingy_x + dbox.right >= *bw::map_width)
        *bw::new_flingy_x = *bw::map_width - dbox.right - 1;
    if (*bw::new_flingy_y - dbox.top < 0 || DidUnderflow(*bw::new_flingy_y, position.y))
        *bw::new_flingy_y = dbox.top;
    else if (*bw::new_flingy_y + dbox.bottom >= *bw::map_height)
        *bw::new_flingy_y = *bw::map_height - dbox.bottom - 1;

    FinishMovement_Fast();
    FinishRepulse(this, repulsed);
}

int Unit::MovementState13()
{
    if (path)
    {
        *bw::dodge_unit_from_path = path->dodge_unit;
        DeletePath();
    }
    if (IsStandingStill() != 0 || flingy_flags & 0x4)
    {
        movement_state = 0xb;
        // Sc doesn't do this even it seems necessary..
        *bw::dodge_unit_from_path = nullptr;
        return 1;
    }
    if (MakePath(this, move_target.AsDword()))
    {
        movement_state = 0x14;
        *bw::dodge_unit_from_path = nullptr;
        return 0;
    }
    else
    {
        movement_state = 0xf;
        *bw::dodge_unit_from_path = nullptr;
        return 1;
    }
}

int Unit::MovementState17()
{
    if (path)
    {
        *bw::dodge_unit_from_path = path->dodge_unit;
        DeletePath();
    }
    if (MakePath(this, move_target.AsDword()))
        movement_state = 0x19;
    else
        movement_state = 0xf;


    *bw::dodge_unit_from_path = nullptr;
    return 1;
}

// wtf
int Unit::MovementState20()
{
    if (path_frame < 0xff)
        path_frame++;
    if (UpdateMovementState(this, true))
        return 1;

    Unit *dodge_unit = path->dodge_unit;
    flingy_flags |= 0x1;
    target_direction = path->direction;
    ChangedDirection(this);
    ((Flingy *)this)->ProgressTurning();
    sprite->SetDirection256(facing_direction);
    if (flingy_movement_type == 2)
        current_speed = flingy_top_speed;
    else
        ProgressSpeed(this);

    int original_movement_direction = movement_direction;
    auto original_speed = next_speed = current_speed;
    auto loop_speed = current_speed;
    int new_movement_state = 0x20;
    Unit *colliding = nullptr;
    while (loop_speed > 0)
    {
        auto tmp_speed = loop_speed;
        loop_speed -= 256;
        if (tmp_speed > 256)
            tmp_speed = 256;
        flingy_flags &= ~0x1;
        ProgressMoveWith(this, path->direction, tmp_speed);
        if (FindCollidingUnit(this) || TerrainCollision(this))
        {
            new_movement_state = 0x21;
            break;
        }
        MoveUnit_Partial(this);
        movement_direction = original_movement_direction;
        ChangeDirectionToMoveWaypoint(this);
        ProgressMoveWith(this, new_direction, original_speed);
        colliding = FindCollidingUnit(this);
        if (colliding != dodge_unit)
            break;
    }
    new_direction = movement_direction = original_movement_direction;
    speed[0] = bw::circle[original_movement_direction][0] * original_speed / 256;
    speed[1] = bw::circle[original_movement_direction][1] * original_speed / 256;
    if (new_movement_state != 0x20)
    {
        movement_state = new_movement_state;
        return 0;
    }
    if (colliding)
        return 0;
    flingy_flags |= 0x1;
    if (!path || *bw::frame_count - path->start_frame >= 150)
        movement_state = 0x13;
    else
        movement_state = 0x19;
    return 0;
}


int Unit::MovementState1c()
{
    if (UpdateMovementState(this, true))
        return 1;
    if (path_frame < 0xff)
        path_frame++;

    Unit *other = path->dodge_unit;
    if (!other || !other->sprite || other->IsDying() || other->sprite->IsHidden() || !NeedsToDodge(other))
    {
        movement_state = 0x19;
        return 0;
    }
    int unk;
    if (move_target_unit && IsInArea(this, 0, move_target_unit))
    {
        unk = 1; // Finished
    }
    else if (DoesBlockPoint(other, unit_id, move_target.x, move_target.y))
    {
        if (other->path_frame < 30 && !DoesCollideAt(move_target, other, other->move_target))
            unk = IsInFrontOfMovement(other, this) ? 4 : 6;
        else
            unk = move_target_unit != nullptr ? 3 : 2;
    }
    else if (DoesBlockPoint(other, unit_id, next_move_waypoint.x, next_move_waypoint.y))
    {
        if (other->path_frame < 30 && !DoesCollideAt(next_move_waypoint, other, other->move_target))
            unk = IsInFrontOfMovement(other, this) ? 4 : 6;
        else
            unk = 3;
    }
    else if (other->IsMovingAwayFrom(this))
    {
        unk = 6;
    }
    else
    {
        int state = other->movement_state;
        if (other->path_frame > 2)
            state = 0x1d;
        if (other->flingy_flags & 0x2 && other->IsStandingStill() == 0)
        {
            if (state == 0x1a || state == 0x19)
                unk = 0x6;
            else if (state == 0x1d)
                unk = unit_search->GetDodgingDirection(this, other) >= 0 ? 0x7 : 0x5;
            else
                unk = 0x4;
        }
        else
            unk = unit_search->GetDodgingDirection(this, other) >= 0 ? 0x7 : 0x5;
    }

    int frames = *bw::frame_count - path->start_frame;
    if (frames < 7 && unk >= 3 && unk <= 5)
    {
        Iscript_StopMoving(this);
        return 0;
    }
    switch (unk)
    {
        case 1: // Finished
            InstantStop(this);
            movement_state = 0xb;
            flags &= ~UnitStatus::MovePosUpdated;
        break;
        case 2:
            InstantStop(this);
            movement_state = 0xb;
        break;
        case 3:
            movement_state = 0x17;
        break;
        case 4:
            movement_state = 0x18;
        break;
        case 5:
            movement_state = 0x13;
            SetSpeed_Iscript(this, 0);
        break;
        case 6:
            movement_state = 0x1d;
        break;
        case 7:
            path->dodge_unit = other; // Oh?
            path->direction = unit_search->GetDodgingDirection(this, other);
            movement_state = 0x1f;
        break;
        case 8:
            movement_state = 0x1e;
        break;
    }
    return 0;
}

bool Unit::NeedsToDodge(const Unit *other) const
{
    if (~pathing_flags & 0x1 || flags & UnitStatus::NoCollision)
        return false;
    if (!other)
        return true; // What
    if (other != this)
        return CanCollideWith(other);
    else
        return false;
}

// Return 1 when reaching target
int Unit::ProgressUnstackMovement()
{
    // IsStillMovingToPathWaypoint causes issues with flingy movement when path is really short
    // Bw decides unit is close enough, and by returning 1 we would just case game calculate
    // another unstack path, which could be exactly same, causing unit to be stuck
    // This bug does not usually happen in vanilla bw due to bug in CreateSimplePath
    //if (IsStillMovingToPathWaypoint(this) == 0)
    //    return 1;

    // This should work unless there are really fast flingies
    if (path->next_pos == sprite->position)
        return 1;
    ((Flingy *)this)->ProgressFlingy();
    auto &dbox = units_dat_dimensionbox[unit_id];
    if (*bw::new_flingy_x - dbox.left < 0 || *bw::new_flingy_x + dbox.right >= *bw::map_width ||
        *bw::new_flingy_y - dbox.top < 0 || *bw::new_flingy_y + dbox.bottom >= *bw::map_height)
    {
        if (IsMovingToMoveWaypoint(this))
            return 1;
        ((Flingy *)this)->ProgressFlingy(); // again???
        ((Flingy *)this)->SetMovementDirectionToTarget();
        return 0;
    }
    FinishUnitMovement(this);
    return 0;
}
