#ifndef ENTITY_H
#define ENTITY_H

/// Entity is a struct that is shared between units and bullets.
/// It is rather nonsensical, and some fields (such as order)
/// do not even make sense without knowing the actual class.
/// Technically units and bullets should just be inheriting/composed from
/// Entity, but composition requires fixing up a lot of code and
/// I've had bad results with inheriting with packed structures
/// in the past, so I'm scared of it :(


#include "types.h"
#include "flingy.h"
#include "rng.h"

#pragma pack(push, 1)
class Entity
{
    public:
        Flingy flingy;
        uint8_t player;
        uint8_t order;
        uint8_t order_state;
        uint8_t order_signal;
        uint16_t order_fow_unit;
        uint8_t unused52;
        uint8_t unused53;
        uint8_t order_timer;
        uint8_t ground_cooldown;
        uint8_t air_cooldown;
        uint8_t spell_cooldown;
        Point order_target_pos;
        Unit *target;

        /// Returns false if the command could not be handled.
        Iscript::CmdResult HandleIscriptCommand(Iscript::Context *ctx, Image *img,
                                                Iscript::Script *script, const Iscript::Command &cmd)
        {
            using namespace Iscript::Opcode;
            switch (cmd.opcode)
            {
                case TrgtRangeCondJmp:
                {
                    if (target != nullptr)
                    {
                        // These work but are casted to Unit in order to
                        // keep most of the calls accepting Units.
                        uint32_t x, y;
                        GetClosestPointOfTarget((Unit *)this, &x, &y);
                        if (IsPointInArea((Unit *)this, cmd.val, x, y))
                            script->pos = cmd.pos;
                    }
                }
                break;
                case TrgtArcCondJmp:
                {
                    const Point &own = flingy.sprite->position;
                    if (target != nullptr)
                    {
                        auto target_position = target->sprite->position;
                        int dir = GetFacingDirection(own.x, own.y, target_position.x, target_position.y);
                        if (abs(dir - cmd.val1()) < cmd.val2())
                            script->pos = cmd.pos;
                    }
                }
                break;
                case Turn1CWise:
                    // Allows missile turret to pick targets more quickly
                    // as it can turn to its target without iscript overriding it ._.
                    if (target == nullptr)
                        SetDirection(&flingy, flingy.facing_direction + 8);
                break;
                case SigOrder:
                    order_signal |= cmd.val;
                break;
                case OrderDone:
                    order_signal &= ~cmd.val;
                break;
                default:
                    return flingy.HandleIscriptCommand(ctx, img, script, cmd);
                break;
            }
            return Iscript::CmdResult::Handled;
        }
};

#pragma pack(pop)
#endif /* ENTITY_H */
