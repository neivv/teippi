#include "sound.h"

#include <algorithm>

#include "console/windows_wrap.h"
#include <dsound.h>

#include "constants/sprite.h"
#include "dat.h"
#include "offsets.h"
#include "rng.h"
#include "sprite.h"
#include "unit.h"
#include "yms.h"

using std::get;

static Unit *previous_selection_sound_unit = nullptr;
static int selection_sound_counter = 0;

static bool CanPlaySelectionSound(Unit *unit)
{
    if (IsReplay())
        return false;
    if (unit->player == *bw::local_player_id)
        return true;
    if (unit->player == NeutralPlayer && unit->Type().HasNeutralSounds())
        return true;
    else
        return false;
}

static int WhatSound(UnitType unit_id)
{
    Assert(unit_id.WhatSoundCount() > 0);
    if (unit_id.WhatSoundCount() == 1)
        return unit_id.WhatSound();

    int sound = unit_id.WhatSound() + MainRng()->Rand(unit_id.WhatSoundCount());
    if (sound == *bw::previous_unit_sound)
    {
        if (sound == unit_id.WhatSound() + unit_id.WhatSoundCount() - 1)
            return unit_id.WhatSound();
        else
            return sound + 1;
    }
    else
    {
        return sound;
    }
}

static tuple<int, int> UnitSelectionSound(UnitType unit_id, int sequence)
{
    bool play_annoyed = sequence > 3;
    if (play_annoyed && unit_id.AnnoyedSound() != 0)
    {
        int annoyed_seq = sequence - 4;
        if (annoyed_seq < unit_id.AnnoyedSoundCount())
            return make_tuple(unit_id.AnnoyedSound() + annoyed_seq, sequence + 1);
        else
            return make_tuple(WhatSound(unit_id), 0);
    }
    else
    {
        return make_tuple(WhatSound(unit_id), sequence + 1);
    }
}

static tuple<int, int> SelectionSound(const Unit *unit, int sequence)
{
    auto snd = [=](auto s) { return make_tuple(s, sequence); };
    if (unit->Type().IsBuilding() && unit->flags & UnitStatus::Completed)
    {
        if (unit->IsOnBurningHealth() && unit->Type().GroupFlags() & 0x2)
            return snd(Sound::LargeBuildingFire);
        if (unit->IsOnBurningHealth() && unit->Type().GroupFlags() & 0x4)
            return snd(Sound::SmallBuildingFire);
        if (unit->IsOnYellowHealth() && unit->Type().GroupFlags() & 0x6)
            return snd(Sound::SmallBuildingFire);
        if (unit->IsDisabled())
            return snd(Sound::PowerDown_Zerg + unit->Type().Race());
        else
            return UnitSelectionSound(unit->Type(), 0);
    }
    else
    {
        if (unit->IsDisabled())
            return snd(Sound::Button);
        else
            return UnitSelectionSound(unit->Type(), sequence);
    }
}

void PlaySelectionSound(Unit *unit)
{
    if (*bw::quiet_sounds != 0)
        return;
    if (unit == nullptr || !CanPlaySelectionSound(unit))
    {
        previous_selection_sound_unit = nullptr;
        selection_sound_counter = 0;
        return;
    }
    if (unit == previous_selection_sound_unit && *bw::selection_sound_cooldown != 0)
        return;
    *bw::selection_sound_cooldown = 10;
    if (unit != previous_selection_sound_unit)
    {
        previous_selection_sound_unit = unit;
        selection_sound_counter = 0;
    }
    auto tp = SelectionSound(unit, selection_sound_counter);
    SoundType sound(get<0>(tp));
    selection_sound_counter = get<1>(tp);

    if (unit->Type().IsCritter() && selection_sound_counter >= 13)
    {
        unit->Kill(nullptr);
        Sprite *sprite = lone_sprites->AllocateLone(SpriteId::NukeHit,
                                                    unit->sprite->position,
                                                    unit->player);
        if (sprite != nullptr)
        {
            sprite->elevation = 4;
            sprite->UpdateVisibilityPoint();
        }
    }
    bool unit_sound_option = [=]{
        if (unit->Type().IsBuilding())
            return *bw::options & 0x4;
        else
            return *bw::options & 0x2;
    }();
    if (unit_sound_option)
    {
        auto success = bw::PlaySound(sound.Raw(), unit, 1, 0x63);
        if (success)
        {
            *bw::previous_unit_sound = sound.Raw();
            if (!unit->Type().IsBuilding())
            {
                int time = (*bw::sound_data)[sound.Raw()].duration / 1000 * 1000 +
                    sound.PortraitTimeModifier();
                bw::TalkingPortrait(unit, time, 0, sound.Raw());
            }
        }
    }
}

void DeleteSounds()
{
    bw::DeleteMapSounds();
    for (auto &channel : bw::sound_channels)
    {
        if (channel.flags & 0x8)
        {
            channel.flags &= ~0x8;
            // Bw doesn't do this, and it causes sounds to become unplayable as it thinks
            // they have been queued already. There are still other bugs with sound deletion
            // and the background sound loading thread, but they should only let sound to
            // slip in after mute, while this fixes sounds being permamently muted.
            *(SoundType(channel.sound).Flags()) &= ~0x8;
        }
        else if (channel.direct_sound_buffer != nullptr)
        {
            auto buffer = (IDirectSoundBuffer *)channel.direct_sound_buffer;
            unsigned long status = 0;
            auto result = buffer->GetStatus(&status);
            if (result != 0 && status != 0)
            {
                buffer->Stop();
                if (channel.free_on_finish)
                    buffer->Release();
                else
                    buffer->SetCurrentPosition(0);
            }
        }
    }
}
