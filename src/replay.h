#ifndef REPLAY_H
#define REPLAY_H

#include "types.h"

uint32_t LoadReplayData(const char *filename, uint32_t *error);
void LoadReplayMapDirEntry();
void ProgressReplay();
void SaveReplay(const char *name, bool overwrite);

#pragma pack(push, 1)
struct ReplayHeader
{
    uint8_t is_bw;
    uint32_t replay_end_frame;
    uint16_t campaign_mission;
    uint8_t dc7[0x3f];
    uint8_t unk46;
    uint8_t dc47[0x232];
};

struct ReplayData
{
    uint8_t unk0[0x4];
    uint32_t unk4;
    uint8_t *beg; // byte player, sitten byte cmd_data[]
    uint32_t length_bytes;
    uint8_t unk10[0x4];
    uint8_t *last_pos;
    uint32_t latest_command_frame;
    uint8_t *pos;
};
#pragma pack(pop)
static_assert(sizeof(ReplayHeader) == 0x279, "sizeof(ReplayHeader)");
static_assert(sizeof(ReplayData) == 0x20, "sizeof(ReplayData)");

#endif /* REPLAY_H */
