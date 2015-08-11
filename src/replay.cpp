#include "replay.h"

#include <string.h>

#include "offsets.h"
#include "game.h"
#include "patchmanager.h"
#include "limits.h"
#include "player.h"
#include "commands.h"
#include "constants/string.h"
#include "mapdirectory.h"
#include "console/windows_wrap.h"

const uint32_t ReplayMagic = 0x4c526572;
const uint32_t OldReplayMagic = 0x53526572;

static bool WriteReplay(File *replay)
{
    uint32_t magic = ReplayMagic;
    if (WriteCompressed(replay, &magic, 4) == 0)
        return false;
    if (WriteCompressed(replay, bw::replay_header.v(), sizeof(ReplayHeader)) == 0)
        return false;
    // For team game replays
    if (WriteCompressed(replay, bw::save_races.v(), Limits::Players) == 0)
        return false;
    if (WriteCompressed(replay, bw::team_game_main_player.v(), Limits::Teams) == 0)
        return false;
    if (WriteReplayData(*bw::replay_data, replay) == 0)
        return false;
    uint32_t size;
    void *chk = ReadChk(&size, &*bw::map_path);
    if (!chk)
        return false;
    if (WriteCompressed(replay, &size, 4) == 0)
    {
        SMemFree(chk, __FILE__, __LINE__, 0);
        return false;
    }
    bool success = WriteCompressed(replay, chk, size) != 0;
    SMemFree(chk, __FILE__, __LINE__, 0);
    return success;
}

void SaveReplay(const char *name, bool overwrite)
{
    char path[260];
    GetReplayPath(name, path, sizeof path);
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
    if (!ReadCompressed(filu, &magic, 4))
        return false;
    if (magic == OldReplayMagic)
    {
        *error = 2;
        return false;
    }
    else if (magic != ReplayMagic)
        return false;
    if (!ReadCompressed(filu, bw::replay_header.v(), sizeof(ReplayHeader)))
        return false;
    if (!ReadCompressed(filu, bw::save_races.v(), Limits::Players))
        return false;
    if (!ReadCompressed(filu, bw::team_game_main_player.v(), Limits::Teams))
        return false;
    if (bw::replay_header[0].unk46 == 0)
        return false;
    *error = bw::replay_header[0].is_bw;
    if (*error && !*bw::is_bw)
        return false;
    *bw::campaign_mission = bw::replay_header[0].campaign_mission;
    AllocateReplayCommands();
    if (!LoadReplayCommands(filu))
        return false;
    if (!ReadCompressed(filu, bw::scenario_chk_length.v(), 4))
        return false;
    if (*bw::scenario_chk)
        SMemFree(*bw::scenario_chk, __FILE__, __LINE__, 0);

    *bw::scenario_chk = SMemAlloc(*bw::scenario_chk_length, __FILE__, __LINE__, 0);
    if (!ReadCompressed(filu, *bw::scenario_chk, *bw::scenario_chk_length))
    {
        SMemFree(*bw::scenario_chk, __FILE__, __LINE__, 0);
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

void LoadReplayMapDirEntry()
{
    REG_EAX(MapDirEntry *, mde);
    *bw::is_replay = 1;
    if (mde->fully_loaded)
        return;
    mde->fully_loaded = 1;
    mde->unk25c = 1;
    snprintf(mde->title, sizeof mde->title, "%s", GetGluAllString(GluAll::ReplayTitle));

    uint32_t error;
    if (!LoadReplayData(mde->full_path, &error))
    {
        if (error == 1)
            snprintf(mde->description, sizeof mde->description, "%s", GetGluAllString(GluAll::MapRequiresBw));
        else if (error == 2)
            snprintf(mde->description, sizeof mde->description, "%s", "This replay has been recorded without mods and cannot be watched");
        else
            snprintf(mde->description, sizeof mde->description, "%s", GetGluAllString(GluAll::InvalidScenarioDesc));
        return;
    }

    Player players[Limits::Players];
    mde->computer_players = 0;
    mde->human_players = 0;
    ReadStruct245(&mde->game_data, players);
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
    mde->campaign_mission = bw::replay_header[0].campaign_mission;
    uint8_t tmp[0x28];
    bool success;
    if (mde->campaign_mission)
        success = PreloadMap(bw::campaign_map_names[*bw::campaign_mission], tmp, 1);
    else
        success = PreloadMap(mde->full_path, tmp, 0);

    if (!success)
    {
        if (tmp[0x18] <= 0x3b)
            snprintf(mde->description, sizeof mde->description, "%s", GetGluAllString(GluAll::InvalidScenarioDesc));
        else
        {
            snprintf(mde->description, sizeof mde->description, "%s", GetGluAllString(GluAll::MapRequiresBw));
            mde->unk25c = 2;
        }
        return;
    }
    snprintf(mde->description, sizeof mde->description, GetGluAllString(GluAll::ReplayDescFormat), mde->game_data.map_title);
    strncat(mde->description, "\n\n", sizeof mde->description - strlen(mde->description));
    snprintf(mde->name, sizeof mde->name, "%s", mde->filename);
    snprintf(mde->x478_as_number, sizeof mde->x478_as_number, GetGluAllString(GluAll::MapDirEntryUnk478Format), mde->unk478);
    snprintf(mde->map_dimension_string, sizeof mde->map_dimension_string, GetGluAllString(GluAll::MapDimensionFormat), mde->map_width_tiles,
            mde->map_height_tiles);
    snprintf(mde->computer_players_string, sizeof mde->computer_players_string, GetGluAllString(GluAll::ComputerPlayersFormat), mde->computer_players);
    snprintf(mde->human_players_string, sizeof mde->human_players_string, GetGluAllString(GluAll::HumanPlayersFormat), mde->human_players);
    snprintf(mde->tileset_string, sizeof mde->tileset_string, "%s", GetGluAllString(GluAll::Badlands + mde->tileset));
    mde->unk25c = 0;
}

int __stdcall ExtractNextReplayFrame(uint32_t *cmd_amount, uint8_t *players, uint8_t *cmd_data, uint32_t *cmd_lengths)
{
    REG_ESI(ReplayData *, commands);
    if (!commands->unk4)
        return -1;
    if (commands->pos >= commands->beg + commands->length_bytes)
        return -1;

    uint32_t frame = ((uint32_t *)commands->pos)[0];
    int i = 0;
    while (frame == ((uint32_t *)commands->pos)[0])
    {
        commands->pos += 4;
        uint8_t len = *commands->pos++;
        int read = 0;
        while (read < len)
        {
            uint8_t player = *commands->pos++;
            players[i] = player;
            read++;
            int cmd_len = CommandLength(commands->pos, len - read);
            if (cmd_len > len - read)
                return -1;
            memcpy(cmd_data, commands->pos, cmd_len);
            cmd_data += cmd_len;
            cmd_lengths[i] = cmd_len;
            commands->pos += cmd_len;
            read += cmd_len;
            i += 1;
        }
        if (commands->pos >= commands->beg + commands->length_bytes)
            return -1;
    }
    *cmd_amount = i;
    return frame;
}
