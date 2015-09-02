#ifndef OFFSETS_HOOKS_H
#define OFFSETS_HOOKS_H

#include "offsets.h"

#define Offset offset<uint32_t>

namespace bw
{
    const Offset WinMain = 0x004E0AE0;
    const Offset WindowCreated = 0x004E0842;

    const Offset Sc_fwrite = 0x00411931;
    const Offset Sc_fread = 0x004117DE;
    const Offset Sc_fseek = 0x00411B6E;
    const Offset Sc_fclose = 0x0040D483;
    const Offset Sc_fopen = 0x0040D424;
    const Offset Sc_fgetc = 0x00411BB7;
    const Offset Sc_setvbuf = 0x00411619;

    const Offset DrawScreen = 0x0041E280;

    const Offset ProgressObjects = 0x004D94B0;
    const Offset GameFunc = 0x004D9670;

    const Offset KillSingleUnit = 0x00475710;
    const Offset Unit_Die = 0x004A0740;

    const Offset AiScript_InvalidOpcode = 0x0045C9B3;
    const Offset AiScript_Stop = 0x00403380;
    const Offset AiScript_MoveDt = 0x004A2380;
    const Offset AiScript_SwitchRescue = 0x004A2130;
    const Offset AddMilitaryAi = 0x0043DA20;

    const Offset CreateAiScript = 0x0045AEF0;
    const Offset RemoveAiTownGasReferences = 0x00432760;
    const Offset DeleteGuardAi = 0x00403030;
    const Offset DeleteWorkerAi = 0x00404470;
    const Offset DeleteBuildingAi = 0x00404500;
    const Offset DeleteMilitaryAi = 0x004371D0;
    const Offset CreateAiTown = 0x00432020;
    const Offset AddUnitAi = 0x00433DD0;

    const Offset PreCreateGuardAi = 0x00462670;
    const Offset AddGuardAiToUnit = 0x00462960;
    const Offset RemoveUnitAi = 0x004A1E50;
    const Offset Ai_UpdateGuardNeeds = 0x004630C0;
    const Offset Ai_DeleteGuardNeeds = 0x004626E0;
    const Offset Ai_SuicideMission = 0x0043E050;
    const Offset Ai_SetFinishedUnitAi = 0x00435DB0;
    const Offset Ai_AddToRegionMilitary = 0x0043E2E0;
    const Offset DoesNeedGuardAi = 0x00462880;
    const Offset ForceGuardAiRefresh = 0x00462760;
    const Offset AiSpendReq_TrainUnit = 0x00435F10;

    const Offset CreateBunkerShootOverlay = 0x00477FD0;

    const Offset CancelZergBuilding = 0x0045DA40;

    const Offset StatusScreen_DrawKills = 0x00425DD0;
    const Offset StatusScreenButton = 0x00458220;

    const Offset DrawStatusScreen_LoadedUnits = 0x00424BA0;
    const Offset TransportStatus_DoesNeedRedraw = 0x00424F10;
    const Offset TransportStatus_UpdateDrawnValues = 0x00424FC0;

    const Offset DrawAllMinimapUnits = 0x004A4AC0;

    const Offset TriggerPortraitFinished = 0x0045E610;

    const Offset Trig_KillUnitGeneric = 0x004C7E20;

    const Offset FindUnitInLocation_Check = 0x004C6F70;
    const Offset ChangeInvincibility = 0x004C6B00;
    const Offset TransportDeath = 0x0049FDD0;
    const Offset CanLoadUnit = 0x004E6E00;
    const Offset LoadUnit = 0x004E78E0;
    const Offset UnloadUnit = 0x004E7F70;
    const Offset SendUnloadCommand = 0x004BFDB0;
    const Offset HasLoadedUnits = 0x004E7110;
    const Offset GetFirstLoadedUnit = 0x004E6C90;
    const Offset IsCarryingFlag = 0x004F3A80;
    const Offset ForEachLoadedUnit = 0x004E6D00;
    const Offset AddLoadedUnitsToCompletedUnitLbScore = 0x0045F870;
    const Offset GetUsedSpace = 0x004E7170;

    const Offset SendChangeSelectionCommand = 0x004C0860;
    const Offset CenterOnSelectionGroup = 0x004967E0;
    const Offset SelectHotkeyGroup = 0x00496B40;
    const Offset TrySelectRecentHotkeyGroup = 0x00496D30;
    const Offset Command_SaveHotkeyGroup = 0x004965D0;
    const Offset Command_SelectHotkeyGroup = 0x00496940;

    const Offset BriefingOk = 0x0046D090;
    const Offset ProcessLobbyCommands = 0x00486530;

    const Offset GameScreenRClickEvent = 0x004564E0;
    const Offset GameScreenLClickEvent_Targeting = 0x004BD500;

    const Offset DoTargetedCommand = 0x0046F5B0;

    const Offset CreateOrder = 0x0048C510;
    const Offset DeleteOrder = 0x004742D0;
    const Offset DeleteSpecificOrder = 0x00474400;
    const Offset DoNextQueuedOrder = 0x00475000;

    const Offset GetEmptyImage = 0x004D4E30;
    const Offset DeleteImage = 0x004D4CE0;
    const Offset IscriptEndRetn = 0x004D7BD2;
    const Offset CreateSprite = 0x004990F0;
    const Offset ProgressSpriteFrame = 0x00497920;
    const Offset DeleteSprite = 0x00497B40;

    const Offset InitSprites_JmpSrc = 0x00499940;
    const Offset InitSprites_JmpDest = 0x00499A05;

    const Offset AddMultipleOverlaySprites = 0x00499660;

    const Offset GetEmptyUnitHook = 0x004A06F5;
    const Offset GetEmptyUnitNop = 0x004A071B;
    const Offset UnitLimitJump = 0x004A06CD;
    const Offset InitUnitSystem = 0x0049F380;
    const Offset InitSpriteSystem = 0x00499900;

    const Offset SetIscriptAnimation = 0x004D8470;

    const Offset Order_AttackMove_ReactToAttack = 0x00478370;
    const Offset Order_AttackMove_TryPickTarget = 0x00477820;

    const Offset ProgressFlingyTurning = 0x00494FE0;
    const Offset SetMovementDirectionToTarget = 0x00496140;

    const Offset GameEnd = 0x004EE8C0;

    const Offset ZeroOldPosSearch = 0x0049F46B;
    const Offset AddToPositionSearch = 0x0046A3A0;
    const Offset FindUnitPosition = 0x00469B00;
    const Offset FindNearbyUnits = 0x00430190;
    const Offset FindUnitsRect = 0x004308A0;
    const Offset DoUnitsCollide = 0x00469B60;
    const Offset CheckMovementCollision = 0x004304D0;
    const Offset FindUnitBordersRect = 0x0042FF80;
    const Offset ClearPositionSearch = 0x0042FEE0;
    const Offset ChangeUnitPosition = 0x0046A000;
    const Offset FindNearestUnit = 0x004E8320;
    const Offset GetNearbyBlockingUnits = 0x00422160;
    const Offset RemoveFromPosSearch = 0x0046A300;
    const Offset GetDodgingDirection = 0x004F2A70;
    const Offset DoesBlockArea = 0x0042E0E0;
    const Offset FindUnitsPoint = 0x004300E0;
    const Offset IsTileBlockedBy = 0x00473300;
    const Offset DoesBuildingBlock = 0x00473410;

    const Offset UnitToIndex = 0x0047B1D0;
    const Offset IndexToUnit = 0x0047B210;

    const Offset PathingInited = 0x0048433E;

    const Offset MakeDrawnSpriteList_Call = 0x0041CA06;
    const Offset PrepareDrawSprites = 0x00498CB0;
    const Offset DrawSprites = 0x00498D40;

    const Offset CreateLoneSprite = 0x00488210;
    const Offset CreateFowSprite = 0x00488410;
    const Offset InitLoneSprites = 0x00488550;
    const Offset DisableVisionSync = 0x0047CDE5;
    const Offset FullRedraw = 0x004BD595;
    const Offset SetSpriteDirection = 0x00401140;
    const Offset FindBlockingFowResource = 0x00487B00;

    const Offset GenerateFog = 0x0047FC50;

    const Offset DrawCursorMarker = 0x00488180;
    const Offset ShowRallyTarget = 0x00468670;
    const Offset ShowCursorMarker = 0x00488660;

    const Offset CreateBullet = 0x0048C260;

    const Offset DamageUnit = 0x004797B0;
    const Offset RemoveUnitFromBulletTargets = 0x0048AAC0;

    const Offset RngSeedPatch = 0x004EF192;

    const Offset ProgressUnstackMovement = 0x004F2160;
    const Offset MovementState13 = 0x0046BCC0;
    const Offset MovementState17 = 0x0046BC30;
    const Offset MovementState20 = 0x0046B000;
    const Offset MovementState1c = 0x0046BF60;
    const Offset MovementState_Flyer = 0x0046B400;

    const Offset ChangeMovementTarget = 0x004EB820;
    const Offset ChangeMovementTargetToUnit = 0x004EB720;

    const Offset ReplayCommands_Nothing = 0x004CDCE0;
    const Offset SDrawLockSurface = 0x00411E4E;
    const Offset SDrawUnlockSurface = 0x00411E48;
    const Offset ProcessCommands = 0x004865D0;

    const Offset LoadGameObjects = 0x004CFEF0;

    const Offset AllocatePath = 0x004E42F0;
    const Offset DeletePath = 0x004E42A0;
    const Offset DeletePath2 = 0x0042F740;
    const Offset CreateSimplePath = 0x0042F830;
    const Offset InitPathArray = 0x0042F4F0;

    const Offset LoadReplayMapDirEntry = 0x004A7C30;
    const Offset LoadReplayData = 0x004DF570;
    const Offset ExtractNextReplayFrame = 0x004CDFF0;

    const Offset UpdateBuildingPlacementState = 0x00473FB0;

    const Offset MedicRemove = 0x004477C0;
    const Offset RemoveWorkerOrBuildingAi = 0x00434C90;

    const Offset LoadGrp = 0x0047ABE0;
    const Offset IsDrawnPixel = 0x004D4DB0;
    const Offset LoadBlendPalettes = 0x004BDE60;
    const Offset FindUnitAtPoint = 0x0046F3A0;
    const Offset DrawImage_Detected = 0x0040B596;
    const Offset DrawImage_Detected_Flipped = 0x0040BCA3;
    const Offset DrawUncloakedPart = 0x0040AFD5;
    const Offset DrawUncloakedPart_Flipped = 0x0040B824;
    const Offset DrawImage_Cloaked = 0x0040B155;
    const Offset DrawImage_Cloaked_Flipped = 0x0040B9A9;
    const Offset MaskWarpTexture = 0x0040AD04;
    const Offset MaskWarpTexture_Flipped = 0x0040AE63;
    const Offset DrawGrp = 0x0040ABBE;
    const Offset DrawGrp_Flipped = 0x0040BF60;

    const Offset MakeJoinedGameCommand = 0x00471FB0;
    const Offset Command_GameData = 0x00470840;

    const Offset InitGame = 0x004EEE00;
    const Offset InitStartingRacesAndTypes = 0x0049CC40;

    const Offset NeutralizePlayer = 0x00489FC0;
    const Offset MakeDetected = 0x00497ED0;

    const Offset AddDamageOverlay = 0x004993C0;
}

#undef Offset

#endif // OFFSETS_HOOKS_H

