#ifndef C_SOUND_H
#define C_SOUND_H

namespace Sound
{
    const int ZergBuildingCancel = 0x4;
    const int Button = 0xf;
    const int SmallBuildingFire = 0x12;
    const int GeyserDepleted = 0x14;

    const int LoadUnit_Zerg = 0x28;
    const int LoadUnit_Terran = 0x29;
    const int LoadUnit_Protoss = 0x2a;
    const int UnloadUnit_Zerg = 0x2b;
    const int UnloadUnit_Terran = 0x2c;
    const int UnloadUnit_Protoss = 0x2d;

    const int NukeLaunch = 0x54;
    const int PowerDown_Zerg = 0x72;
    const int Advisor_NukeLaunch = 0x7f;

    const int Error_Zerg = 0x8a;
    const int Error_Terran = 0x8b;
    const int Error_Protoss = 0x8c;

    const int NotEnoughMinerals = 0x93;
    const int NotEnoughGas = 0x96;
    const int NotEnoughEnergy_Zerg = 0x9c;
    const int NotEnoughEnergy_Terran = 0x9d;
    const int NotEnoughEnergy_Protoss = 0x9e;
    const int NotEnoughEnergy = NotEnoughEnergy_Zerg;

    const int NukeLaser = 0xef;

    const int Cloak = 0x111;
    const int Decloak = 0x112;

    const int Irradiate = 0x15f;

    const int LargeBuildingFire = 0x193;

    const int ProtossBuildingFinishing = 0x211;

    const int Recall = 0x226;
    const int Recalled = 0x228;
    const int InterceptorLaunch = 0x268;
    const int Hallucination = 0x26a;
    const int HallucinationDeath = 0x26b;
    const int Parasite = 0x399;
    const int Restoration = 0x3e6;
    const int OpticalFlare = 0x3fb;

    const int Feedback = 0x425;
    const int MindControl = 0x426;
    const int MaelstromHit = 0x428;
}

#endif // C_SOUND_H

