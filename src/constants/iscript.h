#ifndef CONST_ISCRIPT_H
#define CONST_ISCRIPT_H

namespace IscriptAnim
{
    const int Init = 0x0;
    const int Death = 0x1;
    const int GndAttkInit = 0x2;
    const int AirAttkInit = 0x3;
    const int GndAttkRpt = 0x5;
    const int AirAttkRpt = 0x6;
    const int CastSpell = 0x7;
    const int GndAttkToIdle = 0x8;
    const int AirAttkToIdle = 0x9;
    const int Walking = 0xb;
    const int Idle = 0xc;
    const int Special1 = 0xd;
    const int Special2 = 0xe;
    const int AlmostBuilt = 0xf; // Also worker is harvesting
    const int Built = 0x10;
    const int Landing = 0x11;
    const int Liftoff = 0x12;
    const int Working = 0x13;
    const int WorkingToIdle = 0x14;
    const int WarpIn = 0x15;
    const int Disable = 0x18;
    const int Burrow = 0x19;
    const int Unburrow = 0x1a;
    const int Enable = 0x1b;
}

namespace IscriptOpcode
{
    const int PlayFram = 0x0;
    const int PlayFramTile = 0x1;
    const int SetHorPos = 0x2;
    const int SetVertPos = 0x3;
    const int SetPos = 0x4;
    const int Wait = 0x5;
    const int WaitRand = 0x6;
    const int Goto = 0x7;
    const int ImgOl = 0x8;
    const int ImgUl = 0x9;
    const int ImgOlOrig = 0xa;
    const int SwitchUl = 0xb;
    const int UnusedC = 0xc;
    const int ImgOlUseLo = 0xd;
    const int ImgUlUseLo = 0xe;
    const int SprOl = 0xf;
    const int HighSprOl = 0x10;
    const int LowSprUl = 0x11;
    const int UflUnstable = 0x12;
    const int SprUlUseLo = 0x13;
    const int SprUl = 0x14;
    const int SprOlUseLo = 0x15;
    const int End = 0x16;
    const int SetFlipState = 0x17;
    const int PlaySnd = 0x18;
    const int PlaySndRand = 0x19;
    const int PlaySndBtwn = 0x1a;
    const int DoMissileDmg = 0x1b;
    const int AttackMelee = 0x1c;
    const int FollowMainGraphic = 0x1d;
    const int RandCondJmp = 0x1e;
    const int TurnCcWise = 0x1f;
    const int TurnCWise = 0x20;
    const int Turn1CWise = 0x21;
    const int TurnRand = 0x22;
    const int SetSpawnFrame = 0x23;
    const int SigOrder = 0x24;
    const int AttackWith = 0x25;
    const int Attack = 0x26;
    const int CastSpell = 0x27;
    const int UseWeapon = 0x28;
    const int Move = 0x29;
    const int GotoRepeatAttk = 0x2a;
    const int EngFrame = 0x2b;
    const int EngSet = 0x2c;
    const int HideCursorMarker = 0x2d;
    const int NoBrkCodeStart = 0x2e;
    const int NoBrkCodeEnd = 0x2f;
    const int IgnoreRest = 0x30;
    const int AttkShiftProj = 0x31;
    const int TmpRmGraphicStart = 0x32;
    const int TmpRmGraphicEnd = 0x33;
    const int SetFlDirect = 0x34;
    const int Call = 0x35;
    const int Return = 0x36;
    const int SetFlSpeed = 0x37;
    const int CreateGasOverlays = 0x38;
    const int PwrupCondJmp = 0x39;
    const int TrgtRangeCondJmp = 0x3a;
    const int TrgtArcCondJmp = 0x3b;
    const int CurDirectCondJmp = 0x3c;
    const int ImgUlNextId = 0x3d;
    const int Unused3e = 0x3e;
    const int LiftoffCondJmp = 0x3f;
    const int WarpOverlay = 0x40;
    const int OrderDone = 0x41;
    const int GrdSprOl = 0x42;
    const int Unused43 = 0x43;
    const int DoGrdDamage = 0x44;
}
#endif /* CONST_ISCRIPT_H */
