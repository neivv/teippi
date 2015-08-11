#ifndef MAPDIRECTORY_H
#define MAPDIRECTORY_H

#include "game.h"

#pragma pack(push, 1)
struct MapDirEntry
{
    MapDirEntry *prev;
    MapDirEntry *next;
    char name[0x41];
    char title[0x20];
    char description[0x100];
    char dc169[0x3c];
    char x478_as_number[0x23];
    char computer_players_string[0x23];
    char human_players_string[0x23];
    char map_dimension_string[0x23];
    char tileset_string[0x23];
    uint8_t dc254[0x4];
    uint32_t fully_loaded;
    uint32_t unk25c;
    uint8_t dc260[0x4];
    uint32_t save_hash;
    uint8_t flags;
    char full_path[0x104];
    char filename[0x104];
    uint8_t dc471;
    uint16_t map_width_tiles;
    uint16_t map_height_tiles;
    uint16_t tileset;
    uint8_t unk478;
    uint8_t player_slots;
    uint8_t computer_players;
    uint8_t human_players;
    uint8_t dc47c[0x10];
    GameData game_data;
    uint8_t dc519[0x27];
    uint32_t campaign_mission;
};
#pragma pack(pop)

static_assert(sizeof(MapDirEntry) == 0x544, "Sizeof(MapDirEntry)");

#endif /* MAPDIRECTORY_H */
