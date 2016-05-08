#ifndef C_TECH_H
#define C_TECH_H

#include "../dat.h"

// @BWAPI: http://code.google.com/p/bwapi/source/browse/trunk/bwapi/BWAPI/Source/BW/TechID.h

namespace TechId
{
    constexpr TechType Stimpacks(0x00);
    constexpr TechType Lockdown(0x01);
    constexpr TechType EmpShockwave(0x02);
    constexpr TechType SpiderMines(0x03);
    constexpr TechType ScannerSweep(0x04);
    constexpr TechType SiegeMode(0x05);
    constexpr TechType DefensiveMatrix(0x06);
    constexpr TechType Irradiate(0x07);
    constexpr TechType YamatoGun(0x08);
    constexpr TechType CloakingField(0x09);
    constexpr TechType PersonnelCloaking(0x0A);
    constexpr TechType Burrowing(0x0B);
    constexpr TechType Infestation(0x0C);
    constexpr TechType SpawnBroodlings(0x0D);
    constexpr TechType DarkSwarm(0x0E);
    constexpr TechType Plague(0x0F);
    constexpr TechType Consume(0x10);
    constexpr TechType Ensnare(0x11);
    constexpr TechType Parasite(0x12);
    constexpr TechType PsionicStorm(0x13);
    constexpr TechType Hallucination(0x14);
    constexpr TechType Recall(0x15);
    constexpr TechType StasisField(0x16);
    constexpr TechType ArchonWarp(0x17);
    constexpr TechType Restoration(0x18);
    constexpr TechType DisruptionWeb(0x19);
    constexpr TechType UnusedTech26(0x1A);
    constexpr TechType MindControl(0x1B);
    constexpr TechType DarkArchonMeld(0x1C);
    constexpr TechType Feedback(0x1D);
    constexpr TechType OpticalFlare(0x1E);
    constexpr TechType Maelstrom(0x1F);
    constexpr TechType LurkerAspect(0x20);
    constexpr TechType UnusedTech33(0x21);
    constexpr TechType Healing(0x22);
    constexpr TechType UnusedTech35(0x23);
    constexpr TechType UnusedTech36(0x24);
    constexpr TechType UnusedTech37(0x25);
    constexpr TechType UnusedTech38(0x26);
    constexpr TechType UnusedTech39(0x27);
    constexpr TechType UnusedTech40(0x28);
    constexpr TechType UnusedTech41(0x29);
    constexpr TechType UnusedTech42(0x2A);
    constexpr TechType UnusedTech43(0x2B);
    constexpr TechType None(0x2C);
    constexpr TechType Unknown(0x2D);
    constexpr TechType NuclearStrike(0x2E);
}

namespace Spell
{
    const int DefaultEnergy = 200 * 0x100;
    const int EnergyBonus = 50 * 0x100;
    const int HeroEnergy = 250 * 0x100;
    const int ConsumeEnergy = 50 * 0x100;

    const int StasisArea = 0x30;
    const int StasisTime = 0x83;
    const int StasisElevation = 0x11;
    const int EnsnareArea = 0x40;
    const int EnsnareElevation = 0x13;
    const int MaelstromArea = 0x30;
    const int MaelstromTime = 0x16;
    const int MaelstromElevation = 0x11;
    const int IrradiateArea = 0xa0;
    const int LockdownTime = 0x83;
    const int BroodlingCount = 0x2;
    const int BroodlingDeathTimer = 0x708;
    const int PlagueArea = 0x40;
    const int PlagueTime = 0x4b;
    const int DarkSwarmElevation = 0xb;
    const int DarkSwarmTime = 0x384;
    const int DisruptionWebElevation = 0xb;
    const int DisruptionWebTime = 0x168;
    const int RecallTime = 0x16;
    const int RecallArea = 0x40;
    const int HallucinationCount = 2;

    const int AcidSporeArea = 0x40; // Not technically a spell but close enough
}

#endif // C_TECH_H

