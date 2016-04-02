#ifndef GAME_H
#define GAME_H

#include "types.h"

int ProgressFrames();
void ProgressObjects();
void GameEnd();
void BriefingOk(Dialog *dlg, int leave);

struct DoWeaponDamageData
{
    DoWeaponDamageData(Unit *a, int p, Unit *t, int d, int w, int dir) :
        attacker(a), player(p), target(t), damage(d), weapon(w), direction(dir) {}
    Unit *attacker;
    int player;
    Unit *target;
    int damage;
    uint16_t weapon;
    uint16_t direction;
};

struct HallucinationHitData
{
    HallucinationHitData(Unit *a, Unit *t, int dir) : attacker(a), target(t), direction(dir) {}
    Unit *attacker;
    Unit *target;
    int direction;
};


#pragma pack(push)
#pragma pack(1)

struct Got
{
    uint8_t game_type_id;
    uint8_t game_type_unk;
    uint16_t game_type_param;
    uint32_t unk4;
    uint8_t victory_conditions;
    uint8_t start_resource;
    uint8_t dont_use_chk_unitstates;
    uint8_t fog_mode;
    uint8_t starting_units;
    uint8_t start_positions;
    uint8_t flags;
    uint8_t allow_alliances;
    uint8_t team_game_teams;
    uint8_t allow_cheats;
    uint8_t unk_tournament;
    uint8_t dc13[0xd];
};

struct GameData
{
    uint32_t save_time;
    char game_name[0x18];
    struct StatstringData
    {
        uint32_t save_hash;
        uint16_t map_width_tiles;
        uint16_t map_height_tiles;
        uint8_t active_human_players;
        uint8_t human_player_slots;
        uint8_t game_speed;
        uint8_t approval_status;
        uint8_t game_type_id;
        uint8_t game_type_unk;
        uint16_t game_type_param;
        uint32_t cdkey_hash;
        uint16_t tileset;
        uint8_t is_replay;
        uint8_t active_computer_players;
    } statstring_data;
    char host_name[0x19];
    char map_title[0x20];
    Got got;
};

struct PlacementBox
{
    void *data;
    uint32_t unk;
};
#pragma pack(pop)

static_assert(sizeof(GameData) == 0x8d, "sizeof(GameData)");
static_assert(sizeof(Got) == 0x20, "sizeof(Got)");

extern float fps;
extern unsigned int render_wait;
extern bool all_visions;

extern GameTests *game_tests;

#endif // GAME_H
