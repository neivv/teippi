#include "commands.h"

#include <string.h>
#include <chrono>
#include <ctime>

#include "offsets.h"
#include "yms.h"
#include "selection.h"
#include "targeting.h"
#include "unit.h"
#include "player.h"
#include "save.h"
#include "log.h"
#include "sync.h"
#include "warn.h"

#pragma pack(push, 1)
struct MapDl
{
    uint8_t dc0[0xc];
    uint32_t players;
};
#pragma pack(pop)

const int ProtocolVersion = 0xdaf0;

static bool IsReplayCommand(uint8_t command)
{
    switch (command)
    {
        case commands::KeepAlive:
        case commands::Pause:
        case commands::Resume:
        case commands::Sync:
        case commands::Latency:
        case commands::ReplaySpeed:
            return false;
        default:
            return true;
    }
}

void Command_GameData(const uint8_t *data, int net_player)
{
    if (*bw::in_lobby || net_player != 0)
        return;
    *bw::tileset = *(uint16_t *)(data + 0x1);
    *bw::map_width_tiles = *(uint16_t *)(data + 0x3);
    *bw::map_height_tiles = *(uint16_t *)(data + 0x5);
    for (int i = 0; i < Limits::Players; i++)
    {
        Player *player = &bw::temp_players[i];
        memset(player, 0, sizeof(Player));
        player->id = i;
        player->storm_id = -1;
        player->type = data[0x7 + i];
        player->race = data[0x13 + i];
        player->team = data[0x2b + i];
    }
    if (IsTeamGame())
    {
        int player_id = 0;
        int players_per_team = bw::GetTeamGameTeamSize();
        int team_count = 2;
        if (bw::game_data->got.game_type_id != 0xf) // Tvb
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
    memcpy(bw::player_types_unused.raw_pointer(), data + 0x1f, 0xc);
    memcpy(bw::force_layout.raw_pointer(), data + 0x2b, 0xc);
    memcpy(bw::user_select_slots.raw_pointer(), data + 0x37, 0x8);
    if (!IsReplay())
        memcpy(bw::replay_user_select_slots.raw_pointer(), data + 0x37, 0x8);
    int save_player_id = *bw::loaded_save ? *bw::loaded_local_player_id : 0;
    int save_unique_player_id = *bw::loaded_save ? *bw::loaded_local_unique_player_id : 0;
    int flags = *bw::own_net_player_flags;
    MakeJoinedGameCommand(flags, 1, save_player_id, save_unique_player_id, *bw::saved_seed, false);
    *bw::lobby_state = 3;
}

static void Command_JoinedGame(const uint8_t *data, int net_player, bool creator)
{
    if (!*bw::in_lobby)
        return;
    *bw::download_percentage = 0xff;
    if (!creator && *bw::lobby_state != 4)
    {
        bw::KickPlayer(net_player, 4);
        return;
    }
    uint16_t protocol_version = *(uint16_t *)(data + 0xb);
    if (protocol_version != ProtocolVersion)
    {
        bw::KickPlayer(net_player, 7);
        return;
    }
    int unique_save_slot = data[2];
    int save_slot = data[1];
    if (unique_save_slot)
    {
        uint32_t save_seed = *(uint32_t *)(data + 0x3);
        if (!IsActivePlayer(unique_save_slot) || !IsActivePlayer(save_slot) || *bw::saved_seed != save_seed)
        {
            bw::KickPlayer(net_player, 5);
            return;
        }
    }
    int game_player = NetPlayerToGame(net_player);
    Player *player;
    if (game_player == -1)
    {
        if (IsUms() && *bw::loaded_save == nullptr)
            game_player = bw::GetFreeSlotFromEmptiestTeam();
        else if ((!IsTeamGame() && IsUms()) || *bw::loaded_save == nullptr)
            game_player = bw::GetFirstFreeHumanPlayerId();
        else
            game_player = save_slot;
        player = &bw::players[game_player];
        if (!IsActivePlayer(game_player) || player->type != 6)
        {
            bw::KickPlayer(net_player, 2);
            return;
        }
    }
    else
    {
        player = &bw::players[game_player];
        player->type = 6;
        bw::save_player_to_original[game_player] = Limits::ActivePlayers;
    }
    if (*bw::loaded_save)
    {
        for (int i = 0; i < Limits::ActivePlayers; i++)
        {
            if (bw::save_player_to_original[i] == unique_save_slot)
            {
                bw::KickPlayer(net_player, 2);
                return;
            }
        }
        if (bw::save_player_to_original[game_player] != Limits::ActivePlayers)
        {
            bw::KickPlayer(net_player, 2);
            return;
        }
        bw::save_player_to_original[game_player] = unique_save_slot;
    }
    player->id = game_player;
    player->storm_id = net_player;
    player->type = 2;
    auto success = storm::SNetGetPlayerName(net_player, player->name, sizeof player->name);
    if (!success)
        player->name[0] = 0;
    if (*bw::loaded_save)
        player->race = bw::save_races[save_slot];
    bw::InitPlayerStructs(*bw::local_net_player);
    uint16_t flags = *(uint16_t *)(data + 7) & 0xff03;
    uint16_t unk4 = *(uint16_t *)(data + 9);
    *bw::update_lobby_glue = 1;
    bw::InitNetPlayer(net_player, flags, unk4, protocol_version);
    if (!creator)
    {
        for (int i = 0; i < Limits::ActivePlayers; i++)
        {
            bw::InitPlayerSlot(&bw::players[i]);
            bw::InitNetPlayerInfo(i);
        }
        bw::SendInfoRequestCommand(net_player);
    }
    else
        *bw::lobby_state = 4;

    if (*bw::map_download)
    {
        bw::InitMapDownload(*bw::map_download, net_player);
        (*bw::map_download)->players &= ~(1 << net_player);
    }
    bw::SetFreeSlots(bw::CountFreeSlots());
}

void MakeJoinedGameCommand(int net_player_flags, int net_player_x4,
    int save_player_id, int save_player_unique_id, uint32_t save_hash, bool create)
{
    if (*bw::joined_game_packet == nullptr)
        *bw::joined_game_packet = (uint8_t *)storm::SMemAlloc(0x12, __FILE__, __LINE__, 8);
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
        storm::SMemFree(*bw::joined_game_packet, __FILE__, __LINE__, 0);
        *bw::joined_game_packet = nullptr;
    }
}

static void Command_KeepAlive(const uint8_t *data)
{
    *bw::keep_alive_count += 1;
}

static void Command_GameSpeed(const uint8_t *data)
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

static void Command_Sync(const uint8_t *data)
{
    if (!IsReplay())
    {
        uint8_t *sync_data = &*bw::sync_data + 0x10c * (data[1] >> 4);
        if (sync_data[8] != data[5] || !bw::Command_Sync_Main(data))
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
                    SaveGame(buf, (uint32_t)time);
                }
            }
            *bw::desync_happened = 1;
            bw::net_player_flags[*bw::lobby_command_user] = 0x10000; // Jep, ei or
        }
    }
    *bw::keep_alive_count += 1;
}

static void Command_Unload(const uint8_t *buf)
{
    Unit *unit = Unit::FindById(*(uint32_t *)(buf + 1));
    if (unit && (unit->flags & UnitStatus::InTransport) && unit->player == *bw::command_user)
    {
        unit->related->UnloadUnit(unit);
    }
}

static void Command_ReplaySpeed(const uint8_t *buf)
{
    bool paused = buf[1];
    int speed = *(int *)(buf + 2);
    int multiplier = *(int *)(buf + 6);
    bw::ChangeReplaySpeed(speed, multiplier, paused);
}

static void Command_Chat(const uint8_t *buf, bool replay_unk)
{
    char copy[129];
    copy[128] = 0;
    memcpy(copy, buf, 128);
    bw::PrintText(copy + 2, replay_unk, copy[1]);
}

/// Returns -1 for invalid data
int CommandLength(const uint8_t *data, int max_length)
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
                return -1;
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

void ProcessCommands(const uint8_t *data, int data_length, int replay_process)
{
    while (data_length > 0)
    {
        uint8_t command = *data;
        int length = CommandLength(data, data_length);
        data_length -= length;
        if (data_length < 0 || length <= 0)
        {
            const char *name = bw::players[*bw::select_command_user].name;
            Warning("Player %d (%s) sent an invalid command", *bw::select_command_user, name);
            return;
        }
        if (IsReplay() && !replay_process && IsReplayCommand(command))
        {
            continue;
        }
        switch (command)
        {
            case commands::KeepAlive: Command_KeepAlive(data); break;
            case commands::Sync: Command_Sync(data); break;
            case commands::Save: Command_Save(data); break;
            case commands::Load: if (!IsMultiplayer()) { bw::Command_Load(data + 5); } break;
            case commands::Restart: bw::Command_Restart(); break;
            case commands::Select: Command_Select(data); break;
            case commands::SelectionAdd: Command_SelectionAdd(data); break;
            case commands::SelectionRemove: Command_SelectionRemove(data); break;
            case commands::Build: bw::Command_Build(data); break;
            case commands::TargetedOrder: Command_Targeted(data); break;
            case commands::MinimapPing: bw::Command_MinimapPing(data); break;
            case commands::RightClick: Command_RightClick(data); break;
            case commands::Vision: bw::Command_Vision(data); break;
            case commands::Ally: bw::Command_Ally(data); break;
            case commands::GameSpeed: Command_GameSpeed(data); break;
            case commands::Pause: bw::Command_Pause(); break;
            case commands::Resume: bw::Command_Resume(); break;
            case commands::Cheat: bw::Command_Cheat(data); break;
            case commands::Hotkey: bw::Command_Hotkey(data); break;
            case commands::CancelBuild: bw::Command_CancelBuild(); break;
            case commands::CancelMorph: bw::Command_CancelMorph(); break;
            case commands::Stop: bw::Command_Stop(data); break;
            case commands::CarrierStop: bw::Command_CarrierStop(); break;
            case commands::ReaverStop: bw::Command_ReaverStop(); break;
            case commands::Order_Nothing: bw::Command_Order_Nothing(); break;
            case commands::ReturnCargo: bw::Command_ReturnCargo(data); break;
            case commands::Train: bw::Command_Train(data); break;
            case commands::CancelTrain: bw::Command_CancelTrain(data); break;
            case commands::Tech: bw::Command_Tech(data); break;
            case commands::CancelTech: bw::Command_CancelTech(); break;
            case commands::Upgrade: bw::Command_Upgrade(data); break;
            case commands::CancelUpgrade: bw::Command_CancelUpgrade(); break;
            case commands::Burrow: bw::Command_Burrow(data); break;
            case commands::Unburrow: bw::Command_Unburrow(); break;
            case commands::Cloak: bw::Command_Cloak(); break;
            case commands::Decloak: bw::Command_Decloak(); break;
            case commands::UnitMorph: bw::Command_UnitMorph(data); break;
            case commands::BuildingMorph: bw::Command_BuildingMorph(data); break;
            case commands::Unsiege: bw::Command_Unsiege(data); break;
            case commands::Siege: bw::Command_Siege(data); break;
            case commands::UnloadAll: bw::Command_UnloadAll(data); break;
            case commands::Unload: Command_Unload(data); break;
            case commands::MergeArchon: bw::Command_MergeArchon(); break;
            case commands::MergeDarkArchon: bw::Command_MergeDarkArchon(); break;
            case commands::HoldPosition: bw::Command_HoldPosition(data); break;
            case commands::CancelNuke: bw::Command_CancelNuke(); break;
            case commands::Lift: bw::Command_Lift(data); break;
            case commands::TrainFighter: bw::Command_TrainFighter(); break;
            case commands::CancelAddon: bw::Command_CancelAddon(); break;
            case commands::Stim: bw::Command_Stim(); break;
            case commands::Latency: bw::Command_Latency(data); break;
            case commands::ReplaySpeed: Command_ReplaySpeed(data); break;
            case commands::LeaveGame: bw::Command_LeaveGame(data); break;
            case commands::Chat: Command_Chat(data, replay_process); break;
        }

        bw::AddToReplayData(*bw::replay_data, *bw::lobby_command_user, data, length);
        data += length;
    }
}

static void ProcessPlayerLobbyCommands(int player, const uint8_t *data, int data_len)
{
    while (data_len > 0)
    {
        int length = CommandLength(data, data_len);
        data_len -= length;
        if (data_len < 0 || length <= 0)
        {
            const char *name = "???";
            int player_index = NetPlayerToGame(player);
            if (player_index != -1)
                name = bw::players[player_index].name;
            Warning("Player %d (%s) sent an invalid command", player, name);
            return;
        }
        switch (data[0])
        {
            case commands::StartGame:
                if (player == 0)
                    bw::Command_StartGame(player);
            break;
            case commands::DownloadPercentage:
            if (bw::player_download[player] != data[1])
            {
                bw::player_download[player] = data[1];
                bw::DrawDownloadStatuses();
            }
            break;
            case commands::ChangeGameSlot:
                bw::Command_ChangeGameSlot(player, data);
            break;
            case commands::NewNetPlayer:
                bw::Command_NewNetPlayer(player, data);
            break;
            case commands::JoinedGame:
                Command_JoinedGame(data, player, false);
            break;
            case commands::ChangeRace:
                bw::Command_ChangeRace(data, player);
            break;
            case commands::TeamGameTeam:
                bw::Command_TeamGameTeam(data, player);
            break;
            case commands::UmsTeam:
                bw::Command_UmsTeam(data, player);
            break;
            case commands::MeleeTeam:
                bw::Command_MeleeTeam(data, player);
            break;
            case commands::SwapPlayers:
                bw::Command_SwapPlayers(data, player);
            break;
            case commands::SavedData:
                bw::Command_SavedData(data, player);
            break;
            case commands::MakeGamePublic:
            if (player == 0)
            {
                bw::MakeGamePublic();
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

void ResetSelectionIter()
{
    *bw::selection_iterator = 0;
}
