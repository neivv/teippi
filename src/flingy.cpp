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

bool Flingy::Initialize(Iscript::Context *ctx, int flingy_id_, int player, int direction, const Point &pos)
{
    flingy_id = flingy_id_;
    position = pos;
    exact_position = Point32(pos.x * 256, pos.y * 256);

    flingy_flags = 0x1;
    next_speed = 0;
    current_speed = 0;
    speed[0] = 0;
    speed[1] = 0;
    facing_direction = direction;
    movement_direction = direction;
    new_direction = direction;
    target_direction = direction;
    _unknown_0x026 = 0;
    value = 1;

    move_target = pos;
    move_target_unit = nullptr;
    next_move_waypoint = pos;
    unk_move_waypoint = pos;

    turn_speed = flingy_dat_turn_speed[flingy_id];
    flingy_movement_type = flingy_dat_movement_type[flingy_id];
    acceleration = flingy_dat_acceleration[flingy_id];
    top_speed = flingy_dat_top_speed[flingy_id];

    sprite = Sprite::Allocate(ctx, flingy_dat_sprite[flingy_id], pos, player);
    if (sprite == nullptr)
        return false;

    for (Image *img : sprite->first_overlay)
    {
        SetImageDirection256(img, direction);
    }
    return true;
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

class FlingyIscriptContext : public Iscript::Context
{
    public:
        constexpr FlingyIscriptContext(Flingy *flingy, Rng *rng) :
            Iscript::Context(rng, true), flingy(flingy) { }

        Flingy * const flingy;

        void ProgressIscript() { flingy->sprite->ProgressFrame(this); }
        void SetIscriptAnimation(int anim, bool force) { flingy->sprite->SetIscriptAnimation(this, anim, force); }

        virtual Iscript::CmdResult HandleCommand(Image *img, Iscript::Script *script,
                                                 const Iscript::Command &cmd) override
        {
            Iscript::CmdResult result = flingy->HandleIscriptCommand(this, img, script, cmd);
            if (result == Iscript::CmdResult::NotHandled)
            {
                Warning("Unhandled iscript command %s in Flingy::ProgressFrame, image %s",
                        cmd.DebugStr().c_str(), img->DebugStr().c_str());
            }
            return result;
        }
};

void Flingy::ProgressFrame()
{
    FlingyIscriptContext(this, MainRng()).ProgressIscript();

    ProgressFlingy();
    MoveFlingy(this);

    if (sprite->position.x >= *bw::map_width || sprite->position.y >= *bw::map_height || move_target == position)
    {
        FlingyIscriptContext(this, MainRng()).SetIscriptAnimation(Iscript::Animation::Death, true);
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

bool Flingy::Move()
{
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
    MoveSprite(sprite.get(), position.x, position.y);
    ProgressTurning();
    for (Image *img : sprite->first_overlay)
    {
        SetImageDirection256(img, facing_direction);
    }

    return moved;
}
