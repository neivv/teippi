#ifndef PLAYER_H
#define PLAYER_H

#include "types.h"

#pragma pack(push)
#pragma pack(1)

struct Player
{
    uint32_t id;
    int32_t storm_id;
    uint8_t type;
    uint8_t race;
    uint8_t team;
    char name[0x19];
};

struct NetPlayer
{
    uint8_t state;
    uint8_t unk1;
    uint16_t flags;
    uint8_t dc4[0x4];
    char name[0x19];
    uint8_t padding;
};

#pragma pack(pop)

bool IsHumanPlayer(int player);
bool IsComputerPlayer(int player);
int NetPlayerToGame(int player);
// Mostly returns false only for neutral as the obs slots are enemies by default...
bool HasEnemies(int player);
void Neutralize(int player);

class ActivePlayerIterator
{
    public:
        ActivePlayerIterator() { player = NextActivePlayer(-1); }
        ActivePlayerIterator(int pl) { player = pl; }
        void operator++() { player = NextActivePlayer(player); }
        bool operator!= (const ActivePlayerIterator &other) const { return player != other.player; }
        int operator*() const { return player; }

    private:
        static int NextActivePlayer(int beg);

        int player;
};

class ActivePlayers
{
    public:
        ActivePlayers() {}
        static ActivePlayerIterator begin() { return ActivePlayerIterator(); }
        static ActivePlayerIterator end() { return ActivePlayerIterator(-1); }
};

#endif // PLAYER_H

