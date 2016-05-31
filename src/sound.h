#ifndef SOUND_H
#define SOUND_H

#include "types.h"
#include "constants/sound.h"

#pragma pack(push, 1)
struct SoundData
{
    uint8_t unk0[0x8];
    uint32_t duration;
    void *direct_sound_buffer;
};

struct SoundChannel
{
    void *direct_sound_buffer;
    uint8_t _dc4[0x2];
    uint8_t flags;
    uint8_t priority;
    uint8_t _dc8[0x4];
    uint16_t sound;
    uint8_t _dce[0xa];
};
#pragma pack(pop)

void PlaySelectionSound(Unit *unit);

static_assert(sizeof(SoundChannel) == 0x18, "sizeof(SoundChannel)");

#endif // SOUND_H

