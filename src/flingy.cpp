#include "flingy.h"

#include "sprite.h"
#include "offsets.h"
#include "image.h"
#include "perfclock.h"
#include "warn.h"
#include "rng.h"

Flingy::Flingy()
{
    move_target = Point(0xffff, 0xffff);
    current_speed = 0;
}

Flingy::~Flingy()
{
}

void Flingy::SingleDelete()
{
    if (*bw::last_active_flingy == this)
        *bw::last_active_flingy = list.prev;
    list.Remove(*bw::first_active_flingy);
    delete this;
}

void Flingy::DeleteAll()
{
    Flingy *it = *bw::first_active_flingy;
    while (it)
    {
        Flingy *flingy = it;
        it = it->list.next;
        delete flingy;
    }
}

void Flingy::ProgressFlingy()
{
    *bw::current_flingy_flags = flingy_flags;
    ChangeDirectionToMoveWaypoint(this);
    ProgressSpeed(this);
    UpdateIsMovingFlag(this);
    ProgressMove(this);

    *bw::previous_flingy_flags = flingy_flags;
    flingy_flags = *bw::current_flingy_flags;
}

void Flingy::ProgressFrame()
{
    for (auto &cmd : sprite->ProgressFrame(IscriptContext(), main_rng))
    {
        if (cmd.opcode == IscriptOpcode::End)
        {
            sprite->SingleDelete();
            sprite = nullptr;
        }
        else
            Warning("Unhandled iscript command %x in Flingy::ProgressFrame", cmd.opcode);
    }

    ProgressFlingy();
    MoveFlingy(this);

    if (sprite->position.x >= *bw::map_width || sprite->position.y >= *bw::map_height || move_target == position)
    {
        auto cmds = sprite->SetIscriptAnimation(1, true);
        if (!Empty(cmds))
            Warning("Flingy::ProgressFrame did not handle all iscript commands for sprite %x", sprite->sprite_id);
    }
}

void Flingy::ProgressFrames()
{
    Flingy *it = *bw::first_active_flingy;
    while (it)
    {
        Flingy *flingy = it;
        it = it->list.next;
        flingy->ProgressFrame();
    }
}

bool Flingy::Move(const IscriptContext &ctx)
{
    STATIC_PERF_CLOCK(Flingy_Move);
    bool moved = false;
    if (*bw::new_flingy_x != position.x || *bw::new_flingy_y != position.y)
        moved = true;
    *bw::old_flingy_x = position.x;
    *bw::old_flingy_y = position.y;
    position.x = *bw::new_flingy_x;
    position.y = *bw::new_flingy_y;
    movement_direction = new_direction;
    flingy_flags = *bw::new_flingy_flags;
    exact_position.x = *bw::new_exact_x;
    exact_position.y = *bw::new_exact_y;
    next_speed = current_speed;
    MoveSprite(sprite, position.x, position.y);
    ProgressTurning();
    for (Image *img : sprite->first_overlay)
    {
        SetImageDirection256(img, facing_direction);
    }
    if (*bw::show_endwalk_anim || *bw::show_startwalk_anim)
    {
        Sprite::ProgressFrame_C cmds;
        if (*bw::show_endwalk_anim)
            cmds = sprite->SetIscriptAnimation(IscriptAnim::Idle, false, ctx, main_rng);
        else if (*bw::show_startwalk_anim)
            cmds = sprite->SetIscriptAnimation(IscriptAnim::Walking, true, ctx, main_rng);
        if (!Empty(cmds))
            Warning("Flingy::Move did not handle all iscript commands for sprite %x", sprite->sprite_id);
    }
    return moved;
}

bool Flingy::ProgressTurning()
{
    if (flingy_flags & 0x2 && ~flingy_flags & 0x1)
        return false;
    int turn_need = target_direction - facing_direction;
    if (turn_need > 0x80 || turn_need < 0 - 0x80)
        turn_need = 0 - turn_need;
    if (turn_need > turn_speed)
        facing_direction += turn_speed;
    else if (turn_need < 0 - turn_speed)
        facing_direction -= turn_speed;
    else
        facing_direction = target_direction;
    // Weapon flingies, sigh
    if ((flingy_id >= 0x8d && flingy_id <= 0xab) || (flingy_id >= 0xc9 && flingy_id <= 0xce))
    {
        if (turn_speed < 255)
            turn_speed += 1;
    }
    if (target_direction == movement_direction && target_direction == facing_direction)
        flingy_flags &= ~0x1;
    return true;
}

void Flingy::SetMovementDirectionToTarget()
{
    movement_direction = new_direction;
    ProgressTurning();
    sprite->SetDirection256(facing_direction);
}
