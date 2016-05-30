#ifndef C_STRING_H
#define C_STRING_H

namespace GluAll
{
    const int MapDirEntryUnk478Format = 0x1e;
    const int MapDimensionFormat = 0x20;
    const int ComputerPlayersFormat = 0x23;
    const int HumanPlayersFormat = 0x25;
    const int Badlands = 0x27;
    const int ReplayDescFormat = 0x30;
    const int MapRequiresBw = 0x3c;
    const int InvalidScenario = 0x3d;
    const int InvalidScenarioDesc = 0x3f;
    const int ReplayTitle = 0xb2;
}

namespace NetworkString
{
    const int UnableToCreateUnit = 0x3e;
    const int SaveDialogMsg = 0x6d;
}

namespace String // Note: 1-based
{
    const int Kills = 0x2fc;
    const int NotEnoughMinerals = 0x352;
    const int NotEnoughGas = 0x353;
    const int CantBuildHere = 0x358;
    const int NotEnoughEnergy_Zerg = 0x360;
    const int NotEnoughEnergy_Terran = 0x361;
    const int NotEnoughEnergy_Protoss = 0x362;
    const int NotEnoughEnergy = NotEnoughEnergy_Zerg;
    const int WaypointListFull = 0x367;
    const int GeyserDepleted = 0x36b;
    const int Error_InvalidTarget = 0x36c;
    const int Error_InvalidTargetStructure = 0x36d;
    const int Error_UnableToAttackTarget = 0x36f;
    const int Error_OutOfRange = 0x378;
    const int Error_TooClose = 0x379;
    const int Error_HallucinationTarget = 0x383;
    const int Error_StasisTarget = 0x384;
    const int NuclearLaunchDetected = 0x417;
    const int Error_MustTargetSpellcaster = 0x532;
    const int Error_MindControlTarget = 0x52e;
}

#endif // C_STRING_H

