#include "commands.h"

#include <string.h>
#include <chrono>
#include <ctime>

#include "offsets.h"
#include "patchmanager.h"
#include "yms.h"
#include "selection.h"
#include "targeting.h"
#include "unit.h"
#include "player.h"
#include "save.h"
#include "log.h"
#include "sync.h"

#pragma pack(push, 1)
struct MapDl
{
    uint8_t dc0[0xc];
    uint32_t players;
};
#pragma pack(pop)

const int ProtocolVersion = 0xdaef;

static bool IsReplayCommand(uint8_t command)
{
    switch (command)
    {
        case commands::KeepAlive:
        case commands::Pause:
        case commands::Resume:
        case commands::Sync:
        case commands::Unk38:
        case commands::Unk39:
        case commands::Latency:
        case commands::ReplaySpeed:
            return false;
        default:
            return true;
    }
}

void Command_GameData(uint8_t *data, int net_player)
{
    if (*bw::in_lobby || net_player != 0)
        return;
    *bw::tileset = *(uint16_t *)(data + 0x1);
    *bw::map_width_tiles = *(uint16_t *)(data + 0x3);
    *bw::map_height_tiles = *(uint16_t *)(data + 0x5);
    memset(bw::temp_players.v(), 0, sizeof(Player) * Limits::Players);
    for (int i = 0; i < Limits::Players; i++)
    {
        Player *player = &bw::temp_players[i];
        player->id = i;
        player->storm_id = -1;
        player->type = data[0x7 + i];
        player->race = data[0x13 + i];
        player->team = data[0x2b + i];
    }
    if (IsTeamGame())
    {
        int player_id = 0;
        int players_per_team = GetTeamGameTeamSize();
        int team_count = 2;
        if (bw::game_data[0].got.game_type_id != 0xf) // Tvb
            team_count = *bw::team_game;
        for (int i = 0; i < team_count; i++)
        {
            for (int j = 0; j < players_per_team; j++)
            {
                if (!IsActivePlayer(player_id))
                    break;
                Player *player = &bw::temp_players[player_id];
                player->team = i + 1;
                player_id += 1;
            }
        }
    }
    memcpy(bw::player_types_unused.v(), data + 0x1f, 0xc);
    memcpy(bw::force_layout.v(), data + 0x2b, 0xc);
    memcpy(bw::user_select_slots.v(), data + 0x37, 0x8);
    if (!IsReplay())
        memcpy(bw::replay_user_select_slots.v(), data + 0x37, 0x8);
    int save_player_id = *bw::loaded_save ? *bw::loaded_local_player_id : 0;
    int save_unique_player_id = *bw::loaded_save ? *bw::loaded_local_unique_player_id : 0;
    int flags = *bw::own_net_player_flags;
    MakeJoinedGameCommand(flags, 1, save_player_id, save_unique_player_id, *bw::saved_seed, false);
    *bw::lobby_state = 3;
}

static void Command_JoinedGame(uint8_t *data, int net_player, bool creator)
{
    if (!*bw::in_lobby)
        return;
    *bw::download_percentage = 0xff;
    if (!creator && *bw::lobby_state != 4)
    {
        KickPlayer(net_player, 4);
        return;
    }
    uint16_t protocol_version = *(uint16_t *)(data + 0xb);
    if (protocol_version != ProtocolVersion)
    {
        KickPlayer(net_player, 7);
        return;
    }
    int unique_save_slot = data[2];
    int save_slot = data[1];
    if (unique_save_slot)
    {
        uint32_t save_seed = *(uint32_t *)(data + 0x3);
        if (!IsActivePlayer(unique_save_slot) || !IsActivePlayer(save_slot) || *bw::saved_seed != save_seed)
        {
            KickPlayer(net_player, 5);
            return;
        }
    }
    int game_player = NetPlayerToGame(net_player);
    Player *player = &bw::players[game_player];
    if (game_player == -1)
    {
        Got &got = bw::game_data[0].got;
        // Ums-like?
        bool basic_got = got.victory_conditions == 0 && got.starting_units == 0 && got.unk_tournament == 0;
        if (basic_got && *bw::loaded_save == nullptr)
            game_player = GetFreeSlotFromEmptiestTeam();
        else if ((!IsTeamGame() && basic_got) || *bw::loaded_save == nullptr)
            game_player = GetFirstFreeHumanPlayerId();
        else
            game_player = save_slot;
        player = &bw::players[game_player];
        if (!IsActivePlayer(game_player) || player->type != 6)
        {
            KickPlayer(net_player, 2);
            return;
        }
    }
    else
    {
        player->type = 6;
        bw::save_player_to_original[game_player] = Limits::ActivePlayers;
    }
    if (*bw::loaded_save)
    {
        for (int i = 0; i < Limits::ActivePlayers; i++)
        {
            if (bw::save_player_to_original[i] == unique_save_slot)
            {
                KickPlayer(net_player, 2);
                return;
            }
        }
        if (bw::save_player_to_original[game_player] != Limits::ActivePlayers)
        {
            KickPlayer(net_player, 2);
            return;
        }
        bw::save_player_to_original[game_player] = unique_save_slot;
    }
    player->id = game_player;
    player->storm_id = net_player;
    player->type = 2;
    auto success = SNetGetPlayerName(net_player, player->name, sizeof player->name);
    if (!success)
        player->name[0] = 0;
    if (*bw::loaded_save)
        player->race = bw::save_races[save_slot];
    InitPlayerStructs(*bw::local_net_player);
    uint16_t flags = *(uint16_t *)(data + 7) & 0xff03;
    uint16_t unk4 = *(uint16_t *)(data + 9);
    *bw::update_lobby_glue = 1;
    InitNetPlayer(net_player, flags, unk4, protocol_version);
    if (!creator)
    {
        for (int i = 0; i < Limits::ActivePlayers; i++)
        {
            InitPlayerSlot(&bw::players[i]);
            InitNetPlayerInfo(i);
        }
        SendInfoRequestCommand(net_player);
    }
    else
        *bw::lobby_state = 4;

    if (*bw::map_download)
    {
        InitMapDownload(*bw::map_download, net_player);
        (*bw::map_download)->players &= ~(1 << net_player);
    }
    SetFreeSlots(CountFreeSlots());
}

void MakeJoinedGameCommand(int net_player_flags, int net_player_x4,
    int save_player_id, int save_player_unique_id, uint32_t save_hash, bool create)
{
    if (*bw::joined_game_packet == nullptr)
        *bw::joined_game_packet = (uint8_t *)SMemAlloc(0x12, __FILE__, __LINE__, 8);
    uint8_t *buf = *bw::joined_game_packet;
    buf[0] = commands::JoinedGame;
    buf[1] = save_player_id;
    buf[2] = save_player_unique_id;
    *(uint32_t *)(buf + 0x3) = save_hash;
    *(uint16_t *)(buf + 0x7) = net_player_flags;
    *(uint16_t *)(buf + 0x9) = net_player_x4;
    *(uint16_t *)(buf + 0xb) = ProtocolVersion;
    if (create)
    {
        Command_JoinedGame(buf, *bw::local_net_player, true);
        SMemFree(*bw::joined_game_packet, __FILE__, __LINE__, 0);
        *bw::joined_game_packet = nullptr;
    }
}

static void Command_KeepAlive(uint8_t *data)
{
    *bw::keep_alive_count += 1;
}

static void Command_GameSpeed(uint8_t *data)
{
    int speed = data[1];
    if (!IsMultiplayer() && speed <= 6)
        *bw::game_speed = speed;
}

static void LogSyncData()
{
    error_log->Log("Desync:\n");
    uint8_t *dataptr = &*bw::sync_data;
    for (int i = 0; i < 0x10; i++)
    {
        for (int j = 0; j < 0xc; j++)
        {
            error_log->Log("%02x ", dataptr[0]);
            dataptr++;
        }
        dataptr += 0x100;
        error_log->Log("\n");
    }
    SyncData sync;
    sync.WriteDiff(SyncData(false), SyncDumper(error_log));
}

static void Command_Sync(uint8_t *data)
{
    if (!IsReplay())
    {
        uint8_t *sync_data = &*bw::sync_data + 0x10c * (data[1] >> 4);
        if (sync_data[8] != data[5] || !Command_Sync_Main(data))
        {
            if (*bw::desync_happened == 0)
            {
                LogSyncData();
                // Saving is kind of pointless as the game cannot be reloaded with all players
                // but it might give some info
                if (Debug)
                {
                    char buf[260];
                    char timestr[50];
                    auto now = std::chrono::system_clock::now();
                    auto timet = std::chrono::system_clock::to_time_t(now);
                    std::strftime(timestr, sizeof timestr, "%m-%d %H-%M", std::localtime(&timet));

                    snprintf(buf, sizeof buf, "%s.%s",
                            bw::net_players[*bw::self_net_player].name, timestr);
                    auto time = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
                    SaveGame(buf, time);
                }
            }
            *bw::desync_happened = 1;
            bw::net_player_flags[*bw::lobby_command_user] = 0x10000; // Jep, ei or
        }
    }
    *bw::keep_alive_count += 1;
}

static void Command_Unload(uint8_t *buf)
{
    Unit *unit = Unit::FindById(*(uint32_t *)(buf + 1));
    if (unit && (unit->flags & UnitStatus::InTransport) && unit->player == *bw::command_user)
    {
        unit->related->UnloadUnit(unit);
    }
}

static void Command_Unk3839(uint8_t *buf)
{
    unsigned player = *bw::select_command_user;
    if (player != *bw::local_player_id && bw::net_players[player].state != 1)
    {
        if (buf[0] == commands::Unk38)
            bw::net_players[player].flags &= ~0x2;
        else
            bw::net_players[player].flags |= 0x2;
    }
}

static void Command_ReplaySpeed(uint8_t *buf)
{
    bool paused = buf[1];
    int speed = *(int *)(buf + 2);
    int multiplier = *(int *)(buf + 6);
    ChangeReplaySpeed(speed, multiplier, paused);
}

static void Command_Chat(uint8_t *buf, bool replay_unk)
{
    buf[129] = 0;
    PrintText((char *)buf + 2, replay_unk, buf[1]);
}

int CommandLength(uint8_t *data, int max_length)
{
    switch (data[0])
    {
        case commands::KeepAlive: return 1;
        case commands::Sync: return 7;
        case commands::Save: case commands::Load:
            return 5 + Sc_strlen((char *)data + 5, max_length - 5, 0x1c, false, false);
        case commands::Restart: return 1;
        case commands::Select: case commands::SelectionAdd: case commands::SelectionRemove:
            if (max_length < 6)
                return INT_MAX;
            return SelectCommandLength(data);
        case commands::Build: return 8;
        case commands::TargetedOrder: return 11;
        case commands::MinimapPing: return 5;
        case commands::RightClick: return 10;
        case commands::Vision: return 3;
        case commands::Ally: return 5;
        case commands::GameSpeed: return 2;
        case commands::Pause: return 1;
        case commands::Resume: return 1;
        case commands::Cheat: return 5;
        case commands::Hotkey: return 3;
        case commands::CancelBuild: return 1;
        case commands::CancelMorph: return 1;
        case commands::Stop: return 2;
        case commands::CarrierStop: return 1;
        case commands::ReaverStop: return 1;
        case commands::Order_Nothing: return 1;
        case commands::ReturnCargo: return 2;
        case commands::Train: return 3;
        case commands::CancelTrain: return 3;
        case commands::Tech: return 2;
        case commands::CancelTech: return 1;
        case commands::Upgrade: return 2;
        case commands::CancelUpgrade: return 1;
        case commands::Burrow: return 2;
        case commands::Unburrow: return 2;
        case commands::Cloak: return 2;
        case commands::Decloak: return 2;
        case commands::UnitMorph: return 3;
        case commands::BuildingMorph: return 3;
        case commands::Unsiege: return 2;
        case commands::Siege: return 2;
        case commands::UnloadAll: return 2;
        case commands::Unload: return 5;
        case commands::MergeArchon: return 1;
        case commands::MergeDarkArchon: return 1;
        case commands::HoldPosition: return 2;
        case commands::CancelNuke: return 1;
        case commands::Lift: return 5;
        case commands::TrainFighter: return 1;
        case commands::CancelAddon: return 1;
        case commands::Stim: return 1;
        case commands::Unk38: case commands::Unk39: return 1;
        case commands::Unused3b: case commands::Unused3a: return 2;
        case commands::Latency: return 2;
        case commands::ReplaySpeed: return 10;
        case commands::LeaveGame: return 2;
        case commands::Chat: return 82;
        case commands::StartGame: return 1;
        case commands::DownloadPercentage: return 2;
        case commands::ChangeGameSlot: return 6;
        case commands::NewNetPlayer: return 8;
        case commands::JoinedGame: return 18;
        case commands::ChangeRace: return 3;
        case commands::TeamGameTeam: return 2;
        case commands::UmsTeam: return 2;
        case commands::MeleeTeam: return 3;
        case commands::SwapPlayers: return 3;
        case commands::SavedData: return 13;
        case commands::MakeGamePublic: return 1;
        case commands::BriefingStart: return 5;
        default: return 1;
    }
}

void __stdcall ProcessCommands(int data_length, int replay_process)
{
    REG_EAX(uint8_t *, data);
    while (data_length > 0)
    {
        uint8_t command = *data;
        int length = CommandLength(data, data_length);
        data_length -= length;
        if (data_length < 0)
            return;
        if (IsReplay() && !replay_process && IsReplayCommand(command))
        {
            continue;
        }
        switch (command)
        {
            case commands::KeepAlive: Command_KeepAlive(data); break;
            case commands::Sync: Command_Sync(data); break;
            case commands::Save: Command_Save(data); break;
            case commands::Load: if (!IsMultiplayer()) { Command_Load(data + 5); } break;
            case commands::Restart: Command_Restart(); break;
            case commands::Select: Command_Select(data); break;
            case commands::SelectionAdd: Command_SelectionAdd(data); break;
            case commands::SelectionRemove: Command_SelectionRemove(data); break;
            case commands::Build: Command_Build(data); break;
            case commands::TargetedOrder: Command_Targeted(data); break;
            case commands::MinimapPing: Command_MinimapPing(data); break;
            case commands::RightClick: Command_RightClick(data); break;
            case commands::Vision: Command_Vision(data); break;
            case commands::Ally: Command_Ally(data); break;
            case commands::GameSpeed: Command_GameSpeed(data); break;
            case commands::Pause: Command_Pause(); break;
            case commands::Resume: Command_Resume(); break;
            case commands::Cheat: Command_Cheat(data); break;
            case commands::Hotkey: Command_Hotkey(data); break;
            case commands::CancelBuild: Command_CancelBuild(); break;
            case commands::CancelMorph: Command_CancelMorph(); break;
            case commands::Stop: Command_Stop(data); break;
            case commands::CarrierStop: Command_CarrierStop(); break;
            case commands::ReaverStop: Command_ReaverStop(); break;
            case commands::Order_Nothing: Command_Order_Nothing(); break;
            case commands::ReturnCargo: Command_ReturnCargo(data); break;
            case commands::Train: Command_Train(data); break;
            case commands::CancelTrain: Command_CancelTrain(data); break;
            case commands::Tech: Command_Tech(data); break;
            case commands::CancelTech: Command_CancelTech(); break;
            case commands::Upgrade: Command_Upgrade(data); break;
            case commands::CancelUpgrade: Command_CancelUpgrade(); break;
            case commands::Burrow: Command_Burrow(data); break;
            case commands::Unburrow: Command_Unburrow(); break;
            case commands::Cloak: Command_Cloak(); break;
            case commands::Decloak: Command_Decloak(); break;
            case commands::UnitMorph: Command_UnitMorph(data); break;
            case commands::BuildingMorph: Command_BuildingMorph(data); break;
            case commands::Unsiege: Command_Unsiege(data); break;
            case commands::Siege: Command_Siege(data); break;
            case commands::UnloadAll: Command_UnloadAll(data); break;
            case commands::Unload: Command_Unload(data); break;
            case commands::MergeArchon: Command_MergeArchon(); break;
            case commands::MergeDarkArchon: Command_MergeDarkArchon(); break;
            case commands::HoldPosition: Command_HoldPosition(data); break;
            case commands::CancelNuke: Command_CancelNuke(); break;
            case commands::Lift: Command_Lift(data); break;
            case commands::TrainFighter: Command_TrainFighter(); break;
            case commands::CancelAddon: Command_CancelAddon(); break;
            case commands::Stim: Command_Stim(); break;
            case commands::Unk38: case commands::Unk39: Command_Unk3839(data); break;
            case commands::Latency: Command_Latency(data); break;
            case commands::ReplaySpeed: Command_ReplaySpeed(data); break;
            case commands::LeaveGame: Command_LeaveGame(data); break;
            case commands::Chat: Command_Chat(data, replay_process); break;
        }

        AddToReplayData(*bw::replay_data, *bw::lobby_command_user, data, length);
        data += length;
    }
}

static void ProcessPlayerLobbyCommands(int player, uint8_t *data, int data_len)
{
    while (data_len > 0)
    {
        int length = CommandLength(data, data_len);
        data_len -= length;
        if (data_len < 0)
            return;
        switch (data[0])
        {
            case commands::StartGame:
                if (player == 0)
                    Command_StartGame(player);
            break;
            case commands::DownloadPercentage:
            if (bw::player_download[player] != data[1])
            {
                bw::player_download[player] = data[1];
                DrawDownloadStatuses();
            }
            break;
            case commands::ChangeGameSlot:
                Command_ChangeGameSlot(player, data);
            break;
            case commands::NewNetPlayer:
                Command_NewNetPlayer(player, data);
            break;
            case commands::JoinedGame:
                Command_JoinedGame(data, player, false);
            break;
            case commands::ChangeRace:
                Command_ChangeRace(data, player);
            break;
            case commands::TeamGameTeam:
                Command_TeamGameTeam(data, player);
            break;
            case commands::UmsTeam:
                Command_UmsTeam(data, player);
            break;
            case commands::MeleeTeam:
                Command_MeleeTeam(data, player);
            break;
            case commands::SwapPlayers:
                Command_SwapPlayers(data, player);
            break;
            case commands::SavedData:
                Command_SavedData(data, player);
            break;
            case commands::MakeGamePublic:
            if (player == 0)
            {
                MakeGamePublic();
                *bw::public_game = 1;
            }
            break;
            case commands::BriefingStart:
                bw::net_players_watched_briefing[player] = 1;
                bw::player_objectives_string_id[NetPlayerToGame(player)] = *(uint32_t *)(data + 1);
            break;
        }
        data += length;
    }
}

void ProcessLobbyCommands()
{
    for (int player = 7; player >= 0; player--)
    {
        *bw::lobby_command_user = player;
        if (bw::net_player_flags[player] & 0x00020000)
            ProcessPlayerLobbyCommands(player, bw::player_turns[player], bw::player_turn_size[player]);
    }
    *bw::lobby_command_user = 8;
}

void PatchProcessCommands(Common::PatchContext *patch)
{
    patch->JumpHook(bw::ProcessCommands, ProcessCommands);

    char retn = 0xc3;
    patch->Patch(bw::ReplayCommands_Nothing, &retn, 1, PATCH_REPLACE);
}

void ResetSelectionIter()
{
    *bw::selection_iterator = 0;
}
