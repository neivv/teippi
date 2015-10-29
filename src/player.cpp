#include "player.h"

#include "offsets.h"
#include "limits.h"
#include "yms.h"
#include "unit.h"
#include "order.h"
#include "ai.h"

bool IsHumanPlayer(int player)
{
    if (player >= 0 && player < Limits::ActivePlayers)
        return bw::players[player].type == 2;
    else
        return false;
}

bool IsComputerPlayer(int player)
{
    if (player >= 0 && player < Limits::ActivePlayers)
        return bw::players[player].type == 1;
    else
        return false;
}

bool HasEnemies(int player)
{
    Assert(player >= 0 && player < Limits::Players);
    for (int i = 0; i < Limits::ActivePlayers; i++)
    {
        if (bw::alliances[player][i] == 0)
            return true;
    }
    return false;
}

static bool IsActive(int player)
{
    int type = bw::players[player].type;
    switch (type)
    {
        case 1:
        case 2:
            return bw::victory_status[player] == 0;
        case 3:
        case 4:
        case 7:
            return true;
        default:
            return false;
    }
}

int ActivePlayerIterator::NextActivePlayer(int beg)
{
    if (*bw::team_game && *bw::is_multiplayer)
    {
        for (int i = beg + 1; i < 4; i++)
        {
            unsigned int player = bw::team_game_main_player[i];
            if (player >= Limits::ActivePlayers)
                continue;
            if (IsActive(player))
                return i;
        }
        return -1;
    }
    else
    {
        for (int i = beg + 1; i < 8; i++)
        {
            if (IsActive(i))
                return i;
        }
        return -1;
    }
}

int NetPlayerToGame(int net_player)
{
    for (int i = 0; i < Limits::ActivePlayers; i++)
    {
        if (bw::players[i].storm_id == net_player)
            return i;
    }
    return -1;
}

static void MarkPlayerNeutral(int player)
{
    if (bw::players[player].type == 0x1)
    {
        // Bw does not guarantee that dead ai has no towns
        Ai::Town *town = bw::active_ai_towns[player];
        while (town != nullptr)
        {
            Assert(town->first_worker == nullptr && town->first_building == nullptr);
            Ai::Town *delete_me = town;
            town = town->list.next;
            Ai::DeleteTown(delete_me);
        }
        bw::active_ai_towns[player] = nullptr;
        bw::players[player].type = 0xb;
    }
    else if (bw::players[player].type == 0x2)
        bw::players[player].type = 0xa;
}

void Neutralize(int player)
{
    Assert(player >= 0 && player < Limits::Players);
    if (IsCheatActive(Cheats::Staying_Alive) && IsHumanPlayer(player))
        return;

    if (bw::game_data->got.victory_conditions == 4 || bw::game_data->got.victory_conditions == 5)
    {
        Unit *next = bw::first_player_unit[player];
        while (next != nullptr)
        {
            Unit *unit = next;
            next = unit->player_units.next;
            if (units_dat_flags[unit->unit_id] & UnitFlags::Subunit || unit->sprite == nullptr)
                continue;
            if (unit->order == Order::Die)
                continue;
            if (units_dat_flags[unit->unit_id] & UnitFlags::SingleEntity && unit->powerup.carrying_unit != nullptr)
            {
                Unit *worker = unit->powerup.carrying_unit;
                if (worker->carried_powerup_flags != 0)
                {
                    DeletePowerupImages(worker);
                    if (worker->worker.powerup)
                    {
                        worker->worker.powerup->order_flags |= 0x4;
                        worker->worker.powerup->Kill(nullptr);
                    }
                    worker->worker.powerup = nullptr;
                    worker->carried_powerup_flags = 0;
                }
            }
            else
            {
                unit->Kill(nullptr);
            }
        }
    }
    else
    {
        Unit *next = bw::first_player_unit[player];
        while (next != nullptr)
        {
            Unit *unit = next;
            next = unit->player_units.next;
            NeutralizeUnit(unit);
        }
        RefreshUi();
    }
    if (*bw::team_game)
    {
        for (int i = 0; i < Limits::ActivePlayers; i++)
        {
            if (bw::players[player].team == bw::players[i].team)
                MarkPlayerNeutral(i);
        }
    }
    else
    {
        MarkPlayerNeutral(player);
    }
    if (IsReplay())
    {
        *bw::replay_visions &= ~(1 << player);
        *bw::player_visions &= ~(1 << player);
        *bw::player_exploration_visions &= ~(0x100 << player);
    }
}
