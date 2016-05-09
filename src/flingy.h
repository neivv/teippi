#ifndef FLINGY_H
#define FLINGY_H

#include "types.h"
#include "list.h"
#include "sprite.h"

/// For getting rid of bw's global variables used with flingy functions.
/// Not too much here (yet?).
struct FlingyMoveResults
{
    constexpr FlingyMoveResults() : moved_speed(0) { }
    int32_t moved_speed;
};

#pragma pack(push, 1)
class Flingy
{
    public:
        RevListEntry<Flingy, 0x0> list;
        uint32_t value;
        ptr<Sprite> sprite;

        Point move_target;
        Unit *move_target_unit;

        Point next_move_waypoint;
        Point unk_move_waypoint;

        // 0x20
        uint8_t flingy_flags;
        uint8_t facing_direction;
        uint8_t turn_speed;
        uint8_t movement_direction;
        uint16_t flingy_id;
        uint8_t _unknown_0x026;
        uint8_t flingy_movement_type;
        Point position;
        Point32 exact_position; // 0x2c
        uint32_t top_speed; // 0x34
        int32_t current_speed;
        int32_t next_speed; // 0x3c

        int32_t speed[2]; // 0x40
        uint16_t acceleration; // 0x48
        uint8_t new_direction;
        uint8_t target_direction;

        /// Initializes the flingy and runs the first frame of iscript animation.
        /// Bw generally behaves as if ctx actually modified the unit which created this flingy,
        /// and in some cases even depends on it.
        /// Returns false on failure.
        bool Initialize(Iscript::Context *ctx,
                        FlingyType flingy_id, int player, int direction, const Point &pos);

        static void ProgressFrames();
        void ProgressFrame();
        FlingyMoveResults ProgressFlingy();

        static void DeleteAll();

        FlingyType Type() const;

        bool ProgressTurning();
        void SetMovementDirectionToTarget();
        void ProgressMove(FlingyMoveResults *ret);

        /// Moves the flingy, based on global variables set by other flingy functions.
        bool Move();

        /// Returns false if the command could not be handled.
        Iscript::CmdResult HandleIscriptCommand(Iscript::Context *ctx, Image *img,
                                                Iscript::Script *script, const Iscript::Command &cmd)
        {
            using namespace Iscript::Opcode;
            switch (cmd.opcode)
            {
                case TurnCcWise:
                    bw::SetDirection(this, facing_direction - cmd.val * 8);
                break;
                case TurnCWise:
                    bw::SetDirection(this, facing_direction + cmd.val * 8);
                break;
                case SetFlDirect:
                    bw::SetDirection(this, cmd.val * 8);
                break;
                case TurnRand:
                    if (ctx->rng->Rand(4) == 0)
                        bw::SetDirection(this, facing_direction - cmd.val * 8);
                    else
                        bw::SetDirection(this, facing_direction + cmd.val * 8);
                break;
                case SetSpawnFrame:
                    bw::SetMoveTargetToNearbyPoint(cmd.val, this);
                break;
                case SetFlSpeed:
                    top_speed = cmd.val;
                break;
                case CurDirectCondJmp:
                    if (abs(facing_direction - cmd.val1()) < cmd.val2())
                        script->pos = cmd.pos;
                break;
                default:
                    Iscript::CmdResult result = sprite->HandleIscriptCommand(ctx, img, script, cmd);
                    if (ctx->deleted)
                    {
                        sprite->Remove();
                        sprite = nullptr;
                    }
                    return result;
                break;
            }
            return Iscript::CmdResult::Handled;
        }

    private:
        Flingy();
        ~Flingy();
        void SingleDelete();
};
#pragma pack(pop)

#endif // FLINGY_H
