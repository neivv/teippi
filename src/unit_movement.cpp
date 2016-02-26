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

    AsFlingy()->ProgressFlingy();

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
        movement_state = MovementState::FollowPath;
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
    AsFlingy()->ProgressTurning();
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
        movement_state = MovementState::FollowPath;
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
        movement_state = MovementState::FollowPath;
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
            if (state == 0x1a || state == MovementState::FollowPath)
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
    // Bw decides unit is close enough, and by returning 1 we would just cause game to calculate
    // another unstack path, which could be exactly same, causing the unit to be stuck
    // This bug does not usually happen in vanilla bw due to a bug in CreateSimplePath
    //if (IsStillMovingToPathWaypoint(this) == 0)
    //    return 1;

    // This should work unless there are really fast flingies
    if (path->next_pos == sprite->position)
        return 1;
    AsFlingy()->ProgressFlingy();
    auto &dbox = units_dat_dimensionbox[unit_id];
    if (*bw::new_flingy_x - dbox.left < 0 || *bw::new_flingy_x + dbox.right >= *bw::map_width ||
        *bw::new_flingy_y - dbox.top < 0 || *bw::new_flingy_y + dbox.bottom >= *bw::map_height)
    {
        if (IsMovingToMoveWaypoint(this))
            return 1;
        AsFlingy()->ProgressFlingy(); // again???
        AsFlingy()->SetMovementDirectionToTarget();
        return 0;
    }
    FinishUnitMovement(this);
    return 0;
}

// State 19
int Unit::MovementState_FollowPath()
{
    if (UpdateMovementState(this, true))
        return 1;
    if (MakePath(this, move_target.AsDword()) == 0)
    {
        movement_state = 0x16;
        return 1;
    }

    auto unk_speed = next_speed;
    auto result = AsFlingy()->ProgressFlingy();
    if (flingy_movement_type == 0) // Flingy.dat
        unk_speed = current_speed;

    Unit *colliding = nullptr;
    if (flags & UnitStatus::Collides)
        colliding = FindCollidingUnit(this);
    bool terrain_collision = TerrainCollision(this) != 0;

    if (terrain_collision)
    {
        path->x_y_speed = unk_speed;
        if (colliding == nullptr)
        {
            movement_state = 0x22;
        }
        else
        {
            path->dodge_unit = colliding;
            movement_state = 0x1c;
        }
        return 1;
    }
    if (colliding != nullptr)
    {
        // Tries to move shorter distance to avoid collision.
        //
        // Logic difference from bw:
        // Bw checked if unit was moving to right or bottom,
        // and only tried shorter speeds then. We do it always
        // for consistency, might have some bad consequences?

        // Only not equal when the unit had reached a move waypoint
        if (result.moved_speed == current_speed)
        {
            auto orig_exact_x = *bw::new_exact_x;
            auto orig_exact_y = *bw::new_exact_y;
            auto orig_x = *bw::new_flingy_x;
            auto orig_y = *bw::new_flingy_y;

            *bw::new_exact_x = exact_position.x + speed[0] / 2;
            *bw::new_exact_y = exact_position.y + speed[1] / 2;
            *bw::new_flingy_x = *bw::new_exact_x / 256;
            *bw::new_flingy_y = *bw::new_exact_y / 256;
            if (FindCollidingUnit(this) == nullptr)
            {
                colliding = nullptr;
            }
            else
            {
                *bw::new_exact_x = exact_position.x + speed[0] / 4;
                *bw::new_exact_y = exact_position.y + speed[1] / 4;
                *bw::new_flingy_x = *bw::new_exact_x / 256;
                *bw::new_flingy_y = *bw::new_exact_y / 256;
                if (FindCollidingUnit(this) == nullptr)
                {
                    colliding = nullptr;
                }
                else
                {
                    *bw::new_exact_x = orig_exact_x;
                    *bw::new_exact_y = orig_exact_y;
                    *bw::new_flingy_x = orig_x;
                    *bw::new_flingy_y = orig_y;
                }
            }
        }
        if (colliding != nullptr)
        {
            // Change from bw: If the unit is being swung around
            // by the momentum, and the colliding unit would not
            // block if this unit were moving in correct direction,
            // don't dodge, don't move, just let the unit turn more.
            auto ShouldKeepTurning = [=] {
               if (movement_direction == target_direction)
                    return false;
               if (FindCollidingWithDirection(target_direction) == colliding)
                   return false;
               return true;
            };
            if (ShouldKeepTurning())
            {
                *bw::new_exact_x = exact_position.x;
                *bw::new_exact_y = exact_position.y;
                *bw::new_flingy_x = position.x;
                *bw::new_flingy_y = position.y;
            }
            else
            {
                path->x_y_speed = unk_speed;
                path->dodge_unit = colliding;
                movement_state = 0x1c;
                return 1;
            }
        }
    }

    if (*bw::new_flingy_x != sprite->position.x || *bw::new_flingy_y != sprite->position.y)
    {
        if (path_frame > 2)
        {
            path_frame = 2;
        }
        else if (path_frame != 0)
        {
            path_frame -= 1;
        }
    }
    FinishUnitMovement(this);
    if (IsStandingStill() == 0)
    {
        if (path->unk_count != 0)
            path->unk_count -= 1;
        else
        {
            path->unk_count = 0x1e;
            movement_state = 0x1a;
        }
    }
    else
    {
        movement_state = 0xb;
    }
    return 0;
}

const Unit *Unit::FindCollidingWithDirection(int direction)
{
    auto orig_exact_x = *bw::new_exact_x;
    auto orig_exact_y = *bw::new_exact_y;
    auto orig_x = *bw::new_flingy_x;
    auto orig_y = *bw::new_flingy_y;

    int x_speed = bw::circle[direction][0] * current_speed / 256;
    int y_speed = bw::circle[direction][1] * current_speed / 256;
    *bw::new_exact_x = exact_position.x + x_speed;
    *bw::new_exact_y = exact_position.y + y_speed;
    *bw::new_flingy_x = *bw::new_exact_x >> 8;
    *bw::new_flingy_y = *bw::new_exact_y >> 8;
    Unit *ret = FindCollidingUnit(this);

    *bw::new_exact_x = orig_exact_x;
    *bw::new_exact_y = orig_exact_y;
    *bw::new_flingy_x = orig_x;
    *bw::new_flingy_y = orig_y;
    return ret;
}
