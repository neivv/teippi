#ifndef FLINGY_H
#define FLINGY_H

#include "types.h"
#include "list.h"

struct IscriptContext;

class Flingy
{
    public:
        RevListEntry<Flingy, 0x0> list;
        uint32_t value;
        Sprite *sprite;

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
        uint32_t flingyTopSpeed; // 0x34
        int32_t current_speed;
        int32_t next_speed; // 0x3c

        int32_t speed[2]; // 0x40
        uint16_t acceleration; // 0x48
        uint8_t new_direction;
        uint8_t target_direction;

        static void ProgressFrames();
        void ProgressFrame();
        void ProgressFlingy();

        static void DeleteAll();

        bool Move(const IscriptContext &ctx);
        bool ProgressTurning();
        void SetMovementDirectionToTarget();

    private:
        Flingy();
        ~Flingy();
        void SingleDelete();
};

#endif // FLINGY_H
