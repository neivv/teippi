#ifndef TRIGGERS_H
#define TRIGGERS_H

#include "types.h"

#pragma pack(push)
#pragma pack(1)

struct TriggerList
{
    uint8_t dc0[0x8];
    Trigger *first;
};

struct KillUnitArgs
{
    bool (__fastcall *IsValid)(int player, int unit_id, Unit *unit);
    uint32_t player;
    uint16_t unit_id;
    uint16_t flags;
    uint32_t check_height;
};

struct Location
{
    Rect32 area;
    uint16_t unk;
    uint16_t flags;
};

// Incomplete
struct Trigger
{
    uint8_t dc0[0x948];
    uint32_t flags;
};

struct TriggerAction
{
    uint8_t location;
    uint8_t dc1[0x3];
    uint32_t string_id;
    uint32_t dc8;
    uint32_t time;
    uint32_t playeR;
    uint32_t misc;
    uint16_t unit_id;
    uint8_t id;
    uint8_t amount;
    uint8_t flags;
    uint8_t dc1d[0x3];
};
#pragma pack(pop)

static_assert(sizeof(TriggerAction) == 0x20, "sizeof(TriggerAction)");

extern int trigger_check_rate;

void ProgressTriggers();

struct FindUnitLocationParam;
struct ChangeInvincibilityParam;
int __fastcall FindUnitInLocation_Check(Unit *unit, FindUnitLocationParam *param);
int __fastcall ChangeInvincibility(Unit *unit, ChangeInvincibilityParam *param);

int Trig_KillUnitGeneric(Unit *unit, KillUnitArgs *args, bool check_height, bool killing_additional_unit);

int __fastcall TrigAction_Transmission(TriggerAction *action);
int __fastcall TrigAction_CenterView(TriggerAction *action);

#endif // TRIGGERS_H
