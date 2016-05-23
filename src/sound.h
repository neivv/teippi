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
#pragma pack(pop)

void PlaySelectionSound(Unit *unit);


#endif // SOUND_H

