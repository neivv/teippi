#ifndef C_TECH_H
#define C_TECH_H

// @BWAPI: http://code.google.com/p/bwapi/source/browse/trunk/bwapi/BWAPI/Source/BW/TechID.h

namespace Tech
{
    enum Enum
    {
        Stimpacks         = 0x00,
        Lockdown          = 0x01,
        EmpShockwave      = 0x02,
        SpiderMines       = 0x03,
        ScannerSweep      = 0x04, // (default)
        SiegeMode         = 0x05,
        DefensiveMatrix   = 0x06, // (default)
        Irradiate         = 0x07,
        YamatoGun         = 0x08,
        CloakingField     = 0x09,
        PersonnelCloaking = 0x0A,
        Burrowing         = 0x0B,
        Infestation       = 0x0C, // (default)
        SpawnBroodlings   = 0x0D,
        DarkSwarm         = 0x0E, // (default)
        Plague            = 0x0F,
        Consume           = 0x10,
        Ensnare           = 0x11,
        Parasite          = 0x12, // (default)
        PsionicStorm      = 0x13,
        Hallucination     = 0x14,
        Recall            = 0x15,
        StasisField       = 0x16,
        ArchonWarp        = 0x17, // (default)
        Restoration       = 0x18,
        DisruptionWeb     = 0x19,
        UnusedTech26      = 0x1A, // (unused)
        MindControl       = 0x1B,
        DarkArchonMeld    = 0x1C, // (default)
        Feedback          = 0x1D, // (default)
        OpticalFlare      = 0x1E,
        Maelstrom         = 0x1F,
        LurkerAspect      = 0x20,
        UnusedTech33      = 0x21, // (default; possibly liftoff)
        Healing           = 0x22, // (default)
        UnusedTech35      = 0x23, // (unused)
        UnusedTech36      = 0x24, // (unused)
        UnusedTech37      = 0x25, // (unused)
        UnusedTech38      = 0x26, // (unused)
        UnusedTech39      = 0x27, // (unused)
        UnusedTech40      = 0x28, // (unused)
        UnusedTech41      = 0x29, // (unused)
        UnusedTech42      = 0x2A, // (unused)
        UnusedTech43      = 0x2B, // (unused)
        None              = 0x2C,
        Unknown           = 0x2D,
        NuclearStrike     = 0x2E
    };
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

