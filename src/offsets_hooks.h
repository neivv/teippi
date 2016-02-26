#ifndef OFFSETS_HOOKS_H
#define OFFSETS_HOOKS_H

#include "offsets.h"

class hook_offset : public offset_base
{
    public:
        constexpr hook_offset(uintptr_t address) : offset_base(address) {}

        inline operator void*() const { return raw_pointer(); }
};

namespace bw
{
    const hook_offset WinMain = 0x004E0AE0;
    const hook_offset WindowCreated = 0x004E0842;

    const hook_offset Sc_fwrite = 0x00411931;
    const hook_offset Sc_fread = 0x004117DE;
    const hook_offset Sc_fseek = 0x00411B6E;
    const hook_offset Sc_fclose = 0x0040D483;
    const hook_offset Sc_fopen = 0x0040D424;
    const hook_offset Sc_fgetc = 0x00411BB7;
    const hook_offset Sc_setvbuf = 0x00411619;

    const hook_offset DrawScreen = 0x0041E280;

    const hook_offset ProgressObjects = 0x004D94B0;
    const hook_offset GameFunc = 0x004D9670;

    const hook_offset KillSingleUnit = 0x00475710;
    const hook_offset Unit_Die = 0x004A0740;

    const hook_offset AiScript_InvalidOpcode = 0x0045C9B3;
    const hook_offset AiScript_Stop = 0x00403380;
    const hook_offset AiScript_MoveDt = 0x004A2380;
    const hook_offset AiScript_SwitchRescue = 0x004A2130;
    const hook_offset AddMilitaryAi = 0x0043DA20;

    const hook_offset CreateAiScript = 0x0045AEF0;
    const hook_offset RemoveAiTownGasReferences = 0x00432760;
    const hook_offset DeleteGuardAi = 0x00403030;
    const hook_offset DeleteWorkerAi = 0x00404470;
    const hook_offset DeleteBuildingAi = 0x00404500;
    const hook_offset DeleteMilitaryAi = 0x004371D0;
    const hook_offset CreateAiTown = 0x00432020;
    const hook_offset AddUnitAi = 0x00433DD0;

    const hook_offset PreCreateGuardAi = 0x00462670;
    const hook_offset AddGuardAiToUnit = 0x00462960;
    const hook_offset RemoveUnitAi = 0x004A1E50;
    const hook_offset Ai_UpdateGuardNeeds = 0x004630C0;
    const hook_offset Ai_DeleteGuardNeeds = 0x004626E0;
    const hook_offset Ai_SuicideMission = 0x0043E050;
    const hook_offset Ai_SetFinishedUnitAi = 0x00435DB0;
    const hook_offset Ai_AddToRegionMilitary = 0x0043E2E0;
    const hook_offset DoesNeedGuardAi = 0x00462880;
    const hook_offset ForceGuardAiRefresh = 0x00462760;
    const hook_offset AiSpendReq_TrainUnit = 0x00435F10;

    const hook_offset CreateBunkerShootOverlay = 0x00477FD0;

    const hook_offset CancelZergBuilding = 0x0045DA40;

    const hook_offset StatusScreen_DrawKills = 0x00425DD0;
    const hook_offset StatusScreenButton = 0x00458220;

    const hook_offset DrawStatusScreen_LoadedUnits = 0x00424BA0;
    const hook_offset TransportStatus_DoesNeedRedraw = 0x00424F10;
    const hook_offset TransportStatus_UpdateDrawnValues = 0x00424FC0;

    const hook_offset DrawAllMinimapUnits = 0x004A4AC0;

    const hook_offset TriggerPortraitFinished = 0x0045E610;

    const hook_offset Trig_KillUnitGeneric = 0x004C7E20;

    const hook_offset FindUnitInLocation_Check = 0x004C6F70;
    const hook_offset ChangeInvincibility = 0x004C6B00;
    const hook_offset TransportDeath = 0x0049FDD0;
    const hook_offset CanLoadUnit = 0x004E6E00;
    const hook_offset LoadUnit = 0x004E78E0;
    const hook_offset UnloadUnit = 0x004E7F70;
    const hook_offset SendUnloadCommand = 0x004BFDB0;
    const hook_offset HasLoadedUnits = 0x004E7110;
    const hook_offset GetFirstLoadedUnit = 0x004E6C90;
    const hook_offset IsCarryingFlag = 0x004F3A80;
    const hook_offset ForEachLoadedUnit = 0x004E6D00;
    const hook_offset AddLoadedUnitsToCompletedUnitLbScore = 0x0045F870;
    const hook_offset GetUsedSpace = 0x004E7170;

    const hook_offset SendChangeSelectionCommand = 0x004C0860;
    const hook_offset CenterOnSelectionGroup = 0x004967E0;
    const hook_offset SelectHotkeyGroup = 0x00496B40;
    const hook_offset TrySelectRecentHotkeyGroup = 0x00496D30;
    const hook_offset Command_SaveHotkeyGroup = 0x004965D0;
    const hook_offset Command_SelectHotkeyGroup = 0x00496940;

    const hook_offset BriefingOk = 0x0046D090;
    const hook_offset ProcessLobbyCommands = 0x00486530;

    const hook_offset GameScreenRClickEvent = 0x004564E0;
    const hook_offset GameScreenLClickEvent_Targeting = 0x004BD500;

    const hook_offset DoTargetedCommand = 0x0046F5B0;

    const hook_offset CreateOrder = 0x0048C510;
    const hook_offset DeleteOrder = 0x004742D0;
    const hook_offset DeleteSpecificOrder = 0x00474400;
    const hook_offset DoNextQueuedOrder = 0x00475000;

    const hook_offset GetEmptyImage = 0x004D4E30;
    const hook_offset DeleteImage = 0x004D4CE0;
    const hook_offset IscriptEndRetn = 0x004D7BD2;
    const hook_offset CreateSprite = 0x004990F0;
    const hook_offset ProgressSpriteFrame = 0x00497920;
    const hook_offset DeleteSprite = 0x00497B40;

    const hook_offset InitSprites_JmpSrc = 0x00499940;
    const hook_offset InitSprites_JmpDest = 0x00499A05;

    const hook_offset AddMultipleOverlaySprites = 0x00499660;

    const hook_offset GetEmptyUnitHook = 0x004A06F5;
    const hook_offset GetEmptyUnitNop = 0x004A071B;
    const hook_offset UnitLimitJump = 0x004A06CD;
    const hook_offset InitUnitSystem = 0x0049F380;
    const hook_offset InitSpriteSystem = 0x00499900;

    const hook_offset SetIscriptAnimation = 0x004D8470;
    const hook_offset ProgressIscriptFrame = 0x004D74C0;

    const hook_offset Order_AttackMove_ReactToAttack = 0x00478370;
    const hook_offset Order_AttackMove_TryPickTarget = 0x00477820;

    const hook_offset ProgressFlingyTurning = 0x00494FE0;
    const hook_offset SetMovementDirectionToTarget = 0x00496140;
    const hook_offset ProgressMove = 0x004956C0;

    const hook_offset GameEnd = 0x004EE8C0;

    const hook_offset ZeroOldPosSearch = 0x0049F46B;
    const hook_offset AddToPositionSearch = 0x0046A3A0;
    const hook_offset FindUnitPosition = 0x00469B00;
    const hook_offset FindNearbyUnits = 0x00430190;
    const hook_offset FindUnitsRect = 0x004308A0;
    const hook_offset DoUnitsCollide = 0x00469B60;
    const hook_offset CheckMovementCollision = 0x004304D0;
    const hook_offset FindUnitBordersRect = 0x0042FF80;
    const hook_offset ClearPositionSearch = 0x0042FEE0;
    const hook_offset ChangeUnitPosition = 0x0046A000;
    const hook_offset FindNearestUnit = 0x004E8320;
    const hook_offset GetNearbyBlockingUnits = 0x00422160;
    const hook_offset RemoveFromPosSearch = 0x0046A300;
    const hook_offset GetDodgingDirection = 0x004F2A70;
    const hook_offset DoesBlockArea = 0x0042E0E0;
    const hook_offset FindUnitsPoint = 0x004300E0;
    const hook_offset IsTileBlockedBy = 0x00473300;
    const hook_offset DoesBuildingBlock = 0x00473410;

    const hook_offset UnitToIndex = 0x0047B1D0;
    const hook_offset IndexToUnit = 0x0047B210;

    const hook_offset PathingInited = 0x0048433E;

    const hook_offset MakeDrawnSpriteList_Call = 0x0041CA06;
    const hook_offset PrepareDrawSprites = 0x00498CB0;
    const hook_offset DrawSprites = 0x00498D40;

    const hook_offset CreateLoneSprite = 0x00488210;
    const hook_offset CreateFowSprite = 0x00488410;
    const hook_offset InitLoneSprites = 0x00488550;
    const hook_offset DisableVisionSync = 0x0047CDE5;
    const hook_offset FullRedraw = 0x004BD595;
    const hook_offset SetSpriteDirection = 0x00401140;
    const hook_offset FindBlockingFowResource = 0x00487B00;

    const hook_offset GenerateFog = 0x0047FC50;

    const hook_offset DrawCursorMarker = 0x00488180;
    const hook_offset ShowRallyTarget = 0x00468670;
    const hook_offset ShowCursorMarker = 0x00488660;

    const hook_offset CreateBullet = 0x0048C260;

    const hook_offset DamageUnit = 0x004797B0;
    const hook_offset RemoveUnitFromBulletTargets = 0x0048AAC0;

    const hook_offset RngSeedPatch = 0x004EF192;

    const hook_offset ProgressUnstackMovement = 0x004F2160;
    const hook_offset MovementState13 = 0x0046BCC0;
    const hook_offset MovementState17 = 0x0046BC30;
    const hook_offset MovementState20 = 0x0046B000;
    const hook_offset MovementState1c = 0x0046BF60;
    const hook_offset MovementState_FollowPath = 0x0046B950;
    const hook_offset MovementState_Flyer = 0x0046B400;

    const hook_offset ChangeMovementTarget = 0x004EB820;
    const hook_offset ChangeMovementTargetToUnit = 0x004EB720;

    const hook_offset ReplayCommands_Nothing = 0x004CDCE0;
    const hook_offset SDrawLockSurface = 0x00411E4E;
    const hook_offset SDrawUnlockSurface = 0x00411E48;
    const hook_offset ProcessCommands = 0x004865D0;

    const hook_offset LoadGameObjects = 0x004CFEF0;

    const hook_offset AllocatePath = 0x004E42F0;
    const hook_offset DeletePath = 0x004E42A0;
    const hook_offset DeletePath2 = 0x0042F740;
    const hook_offset CreateSimplePath = 0x0042F830;
    const hook_offset InitPathArray = 0x0042F4F0;

    const hook_offset LoadReplayMapDirEntry = 0x004A7C30;
    const hook_offset LoadReplayData = 0x004DF570;

    const hook_offset UpdateBuildingPlacementState = 0x00473FB0;

    const hook_offset MedicRemove = 0x004477C0;
    const hook_offset RemoveWorkerOrBuildingAi = 0x00434C90;

    const hook_offset LoadGrp = 0x0047ABE0;
    const hook_offset IsDrawnPixel = 0x004D4DB0;
    const hook_offset LoadBlendPalettes = 0x004BDE60;
    const hook_offset FindUnitAtPoint = 0x0046F3A0;
    const hook_offset DrawImage_Detected = 0x0040B596;
    const hook_offset DrawImage_Detected_Flipped = 0x0040BCA3;
    const hook_offset DrawUncloakedPart = 0x0040AFD5;
    const hook_offset DrawUncloakedPart_Flipped = 0x0040B824;
    const hook_offset DrawImage_Cloaked = 0x0040B155;
    const hook_offset DrawImage_Cloaked_Flipped = 0x0040B9A9;
    const hook_offset MaskWarpTexture = 0x0040AD04;
    const hook_offset MaskWarpTexture_Flipped = 0x0040AE63;
    const hook_offset DrawGrp = 0x0040ABBE;
    const hook_offset DrawGrp_Flipped = 0x0040BF60;

    const hook_offset MakeJoinedGameCommand = 0x00471FB0;
    const hook_offset Command_GameData = 0x00470840;

    const hook_offset InitGame = 0x004EEE00;
    const hook_offset InitStartingRacesAndTypes = 0x0049CC40;

    const hook_offset NeutralizePlayer = 0x00489FC0;
    const hook_offset MakeDetected = 0x00497ED0;

    const hook_offset AddDamageOverlay = 0x004993C0;
}


#endif // OFFSETS_HOOKS_H

