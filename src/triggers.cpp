#include "triggers.h"

#include "offsets.h"
#include "unit.h"
#include "player.h"
#include "order.h"
#include "resolution.h"

#include <string.h>
#include <algorithm>

using std::max;

#pragma pack(push)
#pragma pack(1)

int trigger_check_rate = 0x1e;

struct FindUnitLocationParam
{
    uint32_t player;
    uint16_t unit_id;
    uint16_t location_flags;
    bool check_height;
};

struct ChangeInvincibilityParam
{
    uint32_t player;
    uint16_t unit_id;
    uint16_t location_flags;
    uint8_t state;
    uint8_t pad[0x3];
    bool check_height;
};
#pragma pack(pop)

void ProgressTriggers()
{
    if (*bw::dont_progress_frame && !*bw::trigger_pause)
        return;

    int msecs = 0x2a;
    CheckVictoryState();
    ProgressTime(msecs);
    bool progressed_something = false;

    if (trigger_check_rate == 0 || (*bw::trigger_cycle_count)-- == 0)
    {
        *bw::trigger_cycle_count = trigger_check_rate;
        *bw::leaderboard_needs_update = 0;
        std::fill(bw::player_victory_status.begin(), bw::player_victory_status.end(), 0);
        for (int player : ActivePlayers())
        {
            TriggerList *triggers = &bw::triggers[player];
            if (triggers->first)
            {
                progressed_something = true;
                *bw::trigger_current_player = player;
                ProgressTriggerList(triggers);
            }
        }
    }
    if (progressed_something)
        ApplyVictory();
}

static int FindUnitInLocation_Check_Main(Unit *unit, FindUnitLocationParam *param)
{
    if (unit->IsWorker() && unit->worker.powerup)
    {
        if (FindUnitInLocation_Check_Main(unit->worker.powerup, param))
            return 1;
    }
    if (!IsOwnedByPlayer(param->player, 0, unit))
        return 0;
    for (Unit *loaded = unit->first_loaded; loaded; loaded = loaded->next_loaded)
    {
        if (FindUnitInLocation_Check_Main(loaded, param))
            return 1;
    }

    return unit->IsTriggerUnitId(param->unit_id);
}

int FindUnitInLocation_Check(Unit *unit, FindUnitLocationParam *param)
{
    if (param->location_flags && MatchesHeight(unit, param->location_flags)) // location height flags are reversed
        return 0;

    return FindUnitInLocation_Check_Main(unit, param);
}

static void ChangeInvincibility_Main(Unit *unit, ChangeInvincibilityParam *param)
{
    if (unit->flags & UnitStatus::Hallucination)
        return;
    if (!IsOwnedByPlayer(param->player, 0, unit))
        return;

    for (Unit *loaded = unit->first_loaded; loaded; loaded = loaded->next_loaded)
    {
        ChangeInvincibility_Main(loaded, param);
    }

    if (unit->IsTriggerUnitId(param->unit_id))
    {
        switch (param->state)
        {
            case 4:
                unit->flags |= UnitStatus::Invincible;
            break;
            case 5:
                if (~units_dat_flags[unit->unit_id] & UnitFlags::Invincible)
                    unit->flags &= ~UnitStatus::Invincible;
            break;
            case 6:
                if (~units_dat_flags[unit->unit_id] & UnitFlags::Invincible)
                    unit->flags ^= UnitStatus::Invincible;
            break;
        }
    }
}

int ChangeInvincibility(Unit *unit, ChangeInvincibilityParam *param)
{
    if (param->location_flags && MatchesHeight(unit, param->location_flags)) // location height flags are reversed
        return 0;

    ChangeInvincibility_Main(unit, param);
    return 0;
}


int Trig_KillUnitGeneric(Unit *unit, KillUnitArgs *args, bool check_height, bool killing_additional_unit)
{
    if (check_height && MatchesHeight(unit, args->flags))
        return 0;

    for (Unit *loaded = unit->first_loaded; loaded; loaded = loaded->next_loaded)
    {
        Trig_KillUnitGeneric(loaded, args, false, true);
    }
    if (unit->IsWorker() && unit->worker.powerup)
        Trig_KillUnitGeneric(unit->worker.powerup, args, false, true);

    if (!unit->sprite || unit->order == Order::Die || !((*args->IsValid)(args->player, args->unit_id, unit)))
        return 0;
    if (units_dat_flags[unit->unit_id] & UnitFlags::SingleEntity && unit->powerup.carrying_unit)
    {
        Unit *worker = unit->powerup.carrying_unit;
        if (worker->carried_powerup_flags)
        {
            DeletePowerupImages(worker);
            if (worker->worker.powerup)
            {
                worker->worker.powerup->order_flags |= 0x4;
                worker->worker.powerup->Kill(nullptr);
                worker->worker.powerup = 0;
            }
            worker->carried_powerup_flags = 0;
        }
    }
    else if (unit->IsTransport())
    {
        if (*bw::trig_remove_unit_active)
            unit->order_flags |= 0x4;
    }
    else
    {
        if (*bw::trig_remove_unit_active)
            HideUnit(unit);
    }
    unit->Kill(nullptr);
    if (!killing_additional_unit)
        *bw::trig_kill_unit_count -= 1;
    return *bw::trig_kill_unit_count == 0;
}

// This does not impmlement the slow moving which happens in singleplayer,
// as it caused desync in replay playback as it messed with waits
// (Also I found it annoying)
int __fastcall TrigAction_CenterView(TriggerAction *action)
{
    int player = *bw::trigger_current_player;
    if (action->location == 0 || !IsHumanPlayer(player))
        return 1;
    if (player == *bw::local_player_id)
    {
        Location *loc = &bw::locations[action->location - 1];
        int x = (loc->area.left + loc->area.right - resolution::game_width) / 2;
        int y = (loc->area.top + loc->area.bottom - resolution::game_height) / 2;
        MoveScreen(x, y);
    }
    return 1;
}

// No singleplayer wait manipulation
int __fastcall TrigAction_Transmission(TriggerAction *action)
{
    int player = *bw::trigger_current_player;
    if (action->location == 0 || !IsHumanPlayer(player))
        return 1;
    if (bw::player_wait_active[player])
        return 0;
    if (action->flags & 0x1)
    {
        action->flags &= ~0x1;
        return 1;
    }
    if ((*bw::current_trigger)->flags & 0x10)
        return 1;

    auto time = action->time;
    switch (action->amount)
    {
        case 7:
            time = action->misc;
        break;
        case 8:
            time += action->misc;
        break;
        case 9:
            if (time < action->misc)
                time = 0;
            else
                time -= action->misc;
        break;
    }
    action->flags |= 0x1;
    bw::player_waits[player] = time;
    bw::player_wait_active[player] = 1;
    if (player == *bw::local_player_id)
    {
        // Play wav
        bw::trigger_actions[0x8](action);
        Unit *unit = FindUnitInLocation(action->unit_id, AllPlayers, action->location - 1);
        if (unit != nullptr)
        {
            unit->sprite->selection_flash_timer = 45;
            PingMinimap(unit->sprite->position.x, unit->sprite->position.y, AllPlayers);
            Trigger_Portrait(action->unit_id, time, unit->sprite->position.x, unit->sprite->position.y);
        }
        else
        {
            Trigger_Portrait(action->unit_id, time, 0xffffffff, 0xffffffff);
        }
        // Check for subtitles or always display
        if (*bw::options & 0x400 || action->flags & 0x4)
        {
            const char *str = GetChkString(action->string_id);
            auto text_time = GetTextDisplayTime(str);
            Trigger_DisplayText(str, max(text_time, time));
        }
    }
    return 0;
}
