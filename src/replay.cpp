#include "replay.h"

#include <string.h>

#include "offsets.h"
#include "game.h"
#include "limits.h"
#include "player.h"
#include "commands.h"
#include "constants/string.h"
#include "mapdirectory.h"
#include "console/windows_wrap.h"
#include "warn.h"

using std::min;

const uint32_t ReplayMagic = 0x4c526572;
const uint32_t OldReplayMagic = 0x53526572;

static bool WriteReplay(File *replay)
{
    uint32_t magic = ReplayMagic;
    if (bw::WriteCompressed(&magic, 4, replay) == 0)
        return false;
    if (bw::WriteCompressed(bw::replay_header.raw_pointer(), sizeof(ReplayHeader), replay) == 0)
        return false;
    // For team game replays
    if (bw::WriteCompressed(bw::save_races.raw_pointer(), Limits::Players, replay) == 0)
        return false;
    if (bw::WriteCompressed(bw::team_game_main_player.raw_pointer(), Limits::Teams, replay) == 0)
        return false;
    if (bw::WriteReplayData(*bw::replay_data, replay) == 0)
        return false;
    uint32_t size;
    void *chk = bw::ReadChk(&size, &bw::map_path[0]);
    if (!chk)
        return false;
    if (bw::WriteCompressed(&size, 4, replay) == 0)
    {
        storm::SMemFree(chk, __FILE__, __LINE__, 0);
        return false;
    }
    bool success = bw::WriteCompressed(chk, size, replay) != 0;
    storm::SMemFree(chk, __FILE__, __LINE__, 0);
    return success;
}

void SaveReplay(const char *name, bool overwrite)
{
    char path[260];
    bw::GetReplayPath(name, path, sizeof path);
    if (overwrite)
    {
        auto result = DeleteFileA(path);
        if (result == 0)
        {
            if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
                return;
        }
    }
    FILE *replay = fopen(path, "wb");
    bw::replay_header->is_bw = *bw::is_bw;
    bw::replay_header->campaign_mission = *bw::campaign_mission;
    bw::replay_header->unk46 = 1;
    bool success = WriteReplay((File *)replay);
    fclose(replay);
    if (!success)
        DeleteFileA(path);
}

bool LoadReplayData(File *filu, uint32_t *error)
{
    uint32_t magic;
    if (!bw::ReadCompressed(&magic, 4, filu))
        return false;
    if (magic == OldReplayMagic)
    {
        *error = 2;
        return false;
    }
    else if (magic != ReplayMagic)
        return false;
    if (!bw::ReadCompressed(bw::replay_header.raw_pointer(), sizeof(ReplayHeader), filu))
        return false;
    if (!bw::ReadCompressed(bw::save_races.raw_pointer(), Limits::Players, filu))
        return false;
    if (!bw::ReadCompressed(bw::team_game_main_player.raw_pointer(), Limits::Teams, filu))
        return false;
    if (bw::replay_header->unk46 == 0)
        return false;
    *error = bw::replay_header->is_bw;
    if (*error && !*bw::is_bw)
        return false;
    *bw::campaign_mission = bw::replay_header->campaign_mission;
    bw::AllocateReplayCommands();
    if (!bw::LoadReplayCommands(filu))
        return false;
    if (!bw::ReadCompressed(bw::scenario_chk_length.raw_pointer(), 4, filu))
        return false;
    if (*bw::scenario_chk)
        storm::SMemFree(*bw::scenario_chk, __FILE__, __LINE__, 0);

    *bw::scenario_chk = storm::SMemAlloc(*bw::scenario_chk_length, __FILE__, __LINE__, 0);
    if (!bw::ReadCompressed(*bw::scenario_chk, *bw::scenario_chk_length, filu))
    {
        storm::SMemFree(*bw::scenario_chk, __FILE__, __LINE__, 0);
        return false;
    }
    return true;
}

uint32_t LoadReplayData(const char *filename, uint32_t *error)
{
    uint32_t tmp;
    if (!error)
        error = &tmp;

    *error = 0;
    File *filu = (File *)fopen(filename, "rb");
    if (!filu)
        return 0;
    uint32_t ret = LoadReplayData(filu, error);
    fclose((FILE *)filu);
    return ret;
}

void LoadReplayMapDirEntry(MapDirEntry *mde)
{
    *bw::is_replay = 1;
    if (mde->fully_loaded)
        return;
    mde->fully_loaded = 1;
    mde->unk25c = 1;
    snprintf(mde->title, sizeof mde->title, "%s", bw::GetGluAllString(GluAll::ReplayTitle));

    uint32_t error;
    if (!LoadReplayData(mde->full_path, &error))
    {
        if (error == 1)
        {
            snprintf(mde->description, sizeof mde->description, "%s",
                     bw::GetGluAllString(GluAll::MapRequiresBw));
        }
        else if (error == 2)
        {
            snprintf(mde->description, sizeof mde->description, "%s",
                     "This replay has been recorded without mods and cannot be watched");
        }
        else
        {
            snprintf(mde->description, sizeof mde->description, "%s",
                     bw::GetGluAllString(GluAll::InvalidScenarioDesc));
        }
        return;
    }

    Player players[Limits::Players];
    mde->computer_players = 0;
    mde->human_players = 0;
    bw::ReadStruct245(&mde->game_data, players);
    for (Player &player : players)
    {
        if (player.type == 1)
            mde->computer_players++;
        else if (player.type == 2)
            mde->human_players++;
    }
    mde->player_slots = Limits::ActivePlayers;
    mde->tileset = mde->game_data.statstring_data.tileset;
    mde->map_width_tiles = mde->game_data.statstring_data.map_width_tiles;
    mde->map_height_tiles = mde->game_data.statstring_data.map_height_tiles;
    mde->unk478 = 1;
    mde->campaign_mission = bw::replay_header->campaign_mission;
    uint8_t tmp[0x28];
    bool success;
    if (mde->campaign_mission)
        success = bw::PreloadMap(bw::campaign_map_names[*bw::campaign_mission], tmp, 1);
    else
        success = bw::PreloadMap(mde->full_path, tmp, 0);

    if (!success)
    {
        if (tmp[0x18] <= 0x3b)
        {
            snprintf(mde->description, sizeof mde->description, "%s",
                     bw::GetGluAllString(GluAll::InvalidScenarioDesc));
        }
        else
        {
            snprintf(mde->description, sizeof mde->description, "%s",
                     bw::GetGluAllString(GluAll::MapRequiresBw));
            mde->unk25c = 2;
        }
        return;
    }
    snprintf(mde->description, sizeof mde->description, bw::GetGluAllString(GluAll::ReplayDescFormat), mde->game_data.map_title);
    strncat(mde->description, "\n\n", sizeof mde->description - strlen(mde->description));
    snprintf(mde->name, sizeof mde->name, "%s", mde->filename);
    snprintf(mde->x478_as_number, sizeof mde->x478_as_number,
             bw::GetGluAllString(GluAll::MapDirEntryUnk478Format), mde->unk478);
    snprintf(mde->map_dimension_string, sizeof mde->map_dimension_string,
             bw::GetGluAllString(GluAll::MapDimensionFormat), mde->map_width_tiles, mde->map_height_tiles);
    snprintf(mde->computer_players_string, sizeof mde->computer_players_string,
             bw::GetGluAllString(GluAll::ComputerPlayersFormat), mde->computer_players);
    snprintf(mde->human_players_string, sizeof mde->human_players_string,
             bw::GetGluAllString(GluAll::HumanPlayersFormat), mde->human_players);
    snprintf(mde->tileset_string, sizeof mde->tileset_string, "%s",
             bw::GetGluAllString(GluAll::Badlands + mde->tileset));
    mde->unk25c = 0;
}

struct ReplayCommand
{
    constexpr ReplayCommand(int net_player, const uint8_t *beg, const uint8_t *end) :
        net_player(net_player), beg(beg), end(end) { }
    int net_player;
    const uint8_t *beg;
    const uint8_t *end;
};

/// Currently meant to be created each time when replay commands are to be run,
/// and advances the pointers in the ReplayData that gets passed to it (if needed).
struct ReplayCommands
{
    /// Modifies the pointers of replay_data.
    /// Assumes that replay_data is at either current frame or a future frame.
    ReplayCommands(ReplayData *replay_data, uint32_t frame)
    {
        const uint8_t *data_end = replay_data->beg + replay_data->length_bytes;
        if (!replay_data->unk4 || replay_data->pos + sizeof(uint32_t) > data_end)
            return;

        uint32_t cmd_frame = *(const uint32_t *)replay_data->pos;
        if (cmd_frame < frame)
        {
            Warn("Corrupted replay? Got frame %d, expected at least %d\n", cmd_frame, frame);
            return;
        }
        if (cmd_frame > frame)
            return;

        // Is most likely enough most of the cases.
        commands.reserve(8);
        while (replay_data->pos + sizeof(uint32_t) + 1 < data_end && frame == *(const uint32_t *)replay_data->pos)
        {
            replay_data->pos += 4;
            int replay_cmd_len = *replay_data->pos;
            replay_data->pos += 1;
            const uint8_t *cmd_end = replay_data->pos + replay_cmd_len;
            cmd_end = std::min(cmd_end, data_end);
            while (replay_data->pos < cmd_end)
            {
                int player = *replay_data->pos;
                replay_data->pos += 1;
                if (replay_data->pos == cmd_end)
                {
                    Warn("Corrupted replay? Command ends suddenly\n");
                    return;
                }
                int cmd_len = CommandLength(replay_data->pos, cmd_end - replay_data->pos);
                if (replay_data->pos + cmd_len > cmd_end || cmd_len <= 0)
                {
                    Warn("Corrupted replay? Invalid length %d for command %x\n", cmd_len, replay_data->pos[0]);
                    return;
                }
                commands.emplace_back(player, replay_data->pos, replay_data->pos + cmd_len);
                replay_data->pos += cmd_len;
            }
        }
    }

    template <class... Args>
    void Warn(Args &&... args)
    {
        bw::ChangeReplaySpeed(*bw::game_speed, *bw::replay_speed_multiplier, 1);
        Warning(std::forward<Args>(args)...);
    }

    ReplayCommands(ReplayCommands &&other) = default;
    ReplayCommands& operator=(ReplayCommands &&other) = default;
    vector<ReplayCommand> commands;
};

void ProgressReplay()
{
    if (!*bw::is_ingame)
        return;
    if (*bw::frame_count >= bw::replay_header->replay_end_frame)
    {
        bw::ChangeReplaySpeed(*bw::game_speed, *bw::replay_speed_multiplier, 1);
        bw::Victory();
        return;
    }
    ReplayCommands cmds(*bw::replay_data, *bw::frame_count);
    *bw::use_rng = 1;
    for (const auto &cmd : cmds.commands)
    {
        int player_id = -1;
        int unique_id = -1;
        for (int i = 0; i < Limits::ActivePlayers; i++)
        {
            if (bw::players[i].storm_id == cmd.net_player)
            {
                unique_id = i;
                if (*bw::team_game)
                    player_id = bw::team_game_main_player[bw::players[i].team - 1];
                else
                    player_id = i;
                break;
            }
        }
        if (player_id == -1)
            continue;
        *bw::command_user = player_id;
        *bw::select_command_user = unique_id;
        // Unnecessary?
        *bw::keep_alive_count = 0;
        ProcessCommands(cmd.beg, cmd.end - cmd.beg, 1);
    }
    *bw::use_rng = 0;
}
