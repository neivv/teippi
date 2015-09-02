#ifndef COMMANDS_H
#define COMMANDS_H

#include "types.h"
void MakeJoinedGameCommand(int net_player_flags, int net_player_x4,
    int save_player_id, int save_player_unique_id, uint32_t save_hash, bool create);
void Command_GameData(uint8_t *data, int net_player);
void ProcessCommands(uint8_t *data, int data_length, int replay_process);

namespace commands
{
    enum ScCommands
    {
        KeepAlive = 0x5,
        Save = 0x6,
        Load = 0x7,
        Restart = 0x8,
        Select = 0x9,
        SelectionAdd = 0xa,
        SelectionRemove = 0xb,
        Build = 0xc,
        Vision = 0xd,
        Ally = 0xe,
        GameSpeed = 0xf,
        Pause = 0x10,
        Resume = 0x11,
        Cheat = 0x12,
        Hotkey = 0x13,
        RightClick = 0x14,
        TargetedOrder = 0x15,
        CancelBuild = 0x18,
        CancelMorph = 0x19,
        Stop = 0x1a,
        CarrierStop = 0x1b,
        ReaverStop = 0x1c,
        Order_Nothing = 0x1d,
        ReturnCargo = 0x1e,
        Train = 0x1f,
        CancelTrain = 0x20,
        Cloak = 0x21,
        Decloak = 0x22,
        UnitMorph = 0x23,
        Unsiege = 0x25,
        Siege = 0x26,
        TrainFighter = 0x27,
        UnloadAll = 0x28,
        Unload = 0x29,
        MergeArchon = 0x2a,
        HoldPosition = 0x2b,
        Burrow = 0x2c,
        Unburrow = 0x2d,
        CancelNuke = 0x2e,
        Lift = 0x2f,
        Tech = 0x30,
        CancelTech = 0x31,
        Upgrade = 0x32,
        CancelUpgrade = 0x33,
        CancelAddon = 0x34,
        BuildingMorph = 0x35,
        Stim = 0x36,
        Sync = 0x37,
        Unk38 = 0x38,
        Unk39 = 0x39,
        Unused3a = 0x3a, // These do nothing but have length of 2
        Unused3b = 0x3b,
        StartGame = 0x3c,
        DownloadPercentage = 0x3d,
        ChangeGameSlot = 0x3e,
        NewNetPlayer = 0x3f,
        JoinedGame = 0x40,
        ChangeRace = 0x41,
        TeamGameTeam = 0x42,
        UmsTeam = 0x43,
        MeleeTeam = 0x44,
        SwapPlayers = 0x45,
        SavedData = 0x48,
        BriefingStart = 0x54,
        Latency = 0x55,
        ReplaySpeed = 0x56,
        LeaveGame = 0x57,
        MinimapPing = 0x58,
        MergeDarkArchon = 0x5a,
        MakeGamePublic = 0x5b,
        Chat = 0x5c,
    };
}

void PatchProcessCommands(Common::PatchContext *patch);
void ProcessLobbyCommands();

void ResetSelectionIter();
int CommandLength(uint8_t *data, int max_length);

#endif

