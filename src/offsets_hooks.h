#ifndef OFFSETS_HOOKS_H
#define OFFSETS_HOOKS_H

#include <initializer_list>
#include <type_traits>

#include "offsets.h"
#include "patch/hook.h"

class hook_offset : public offset_base
{
    public:
        constexpr hook_offset(uintptr_t address) : offset_base(address) {}

        inline operator void*() const { return raw_pointer(); }
};

namespace bw
{
    using hook::Stdcall;
    using hook::Eax;
    using hook::Edx;
    using hook::Ecx;
    using hook::Ebx;
    using hook::Esi;
    using hook::Edi;

    const Stdcall<void()> WinMain = 0x004E0AE0;
    const Stdcall<void()> WindowCreated = 0x004E0842;

    const hook_offset Sc_fwrite = 0x00411931;
    const hook_offset Sc_fread = 0x004117DE;
    const hook_offset Sc_fseek = 0x00411B6E;
    const hook_offset Sc_fclose = 0x0040D483;
    const hook_offset Sc_fopen = 0x0040D424;
    const hook_offset Sc_fgetc = 0x00411BB7;
    const hook_offset Sc_setvbuf = 0x00411619;

    const Stdcall<void()> DrawScreen = 0x0041E280;

    const Stdcall<void ()> ProgressObjects = 0x004D94B0;
    // Actually takes unused arg in ecx
    const Stdcall<int ()> GameFunc = 0x004D9670;

    const Stdcall<void(Eax<Unit *>)> KillSingleUnit = 0x00475710;
    const Stdcall<void()> Unit_Die = 0x004A0740;

    const Stdcall<void(Ai::Script *)> AiScript_InvalidOpcode = 0x0045C9B3;
    const Stdcall<void(Eax<Ai::Script *>)> AiScript_Stop = 0x00403380;
    const Stdcall<void(Eax<uint8_t>, int, int)> AiScript_MoveDt = 0x004A2380;
    const Stdcall<void(Edx<uint8_t>)> AiScript_SwitchRescue = 0x004A2130;
    const Stdcall<void(Ebx<Unit *>, Eax<Ai::Region *>, int)> AddMilitaryAi = 0x0043DA20;

    const Stdcall<Ai::Script *(Esi<const Rect32 *>, uint8_t, uint32_t, int)> CreateAiScript = 0x0045AEF0;
    const Stdcall<void(Ecx<Unit *>)> RemoveAiTownGasReferences = 0x00432760;
    const Stdcall<void(Eax<Ai::WorkerAi *>, Edx<Ai::DataList<Ai::WorkerAi> *>)> DeleteWorkerAi = 0x00404470;
    const Stdcall<void(Eax<Ai::BuildingAi *>, Edx<Ai::DataList<Ai::BuildingAi> *>)> DeleteBuildingAi = 0x00404500;
    const Stdcall<void(Eax<Ai::MilitaryAi *>, Edx<Ai::Region *>)> DeleteMilitaryAi = 0x004371D0;
    const Stdcall<Ai::Town *(Ebx<uint8_t> , uint16_t, uint16_t)> CreateAiTown = 0x00432020;
    const Stdcall<void(Ebx<Unit *>, Edi<Ai::Town *>)> AddUnitAi = 0x00433DD0;

    const Stdcall<void(Esi<uint8_t>, uint16_t, uint16_t, Edi<uint16_t>)> PreCreateGuardAi = 0x00462670;
    const Stdcall<void(Unit *)> AddGuardAiToUnit = 0x00462960;
    const Stdcall<void(Ecx<Unit *>, Edx<int>)> RemoveUnitAi = 0x004A1E50;
    const Stdcall<void(int)> Ai_UpdateGuardNeeds = 0x004630C0;
    const Stdcall<void(Esi<uint8_t>)> Ai_DeleteGuardNeeds = 0x004626E0;
    const Stdcall<void(int, Eax<uint8_t>)> Ai_SuicideMission = 0x0043E050;
    const Stdcall<void(Eax<Unit *>, Ecx<Unit *>)> Ai_SetFinishedUnitAi = 0x00435DB0;
    const Stdcall<int(Eax<Ai::Region *>, int, int)> Ai_AddToRegionMilitary = 0x0043E2E0;
    const Stdcall<int(Ebx<uint8_t>, uint16_t)> DoesNeedGuardAi = 0x00462880;
    const Stdcall<void(Eax<uint8_t>, Edx<uint16_t>)> ForceGuardAiRefresh = 0x00462760;
    const Stdcall<int(int, Unit *, int)> AiSpendReq_TrainUnit = 0x00435F10;

    const Stdcall<void(Eax<Unit *>)> CreateBunkerShootOverlay = 0x00477FD0;
    const Stdcall<void(Unit *)> CancelZergBuilding = 0x0045DA40;

    const Stdcall<void(Ebx<Dialog *>)> StatusScreen_DrawKills = 0x00425DD0;
    const Stdcall<void(Edx<Control *>)> StatusScreenButton = 0x00458220;

    const Stdcall<void(Eax<Dialog *>)> DrawStatusScreen_LoadedUnits = 0x00424BA0;
    const Stdcall<int()> TransportStatus_DoesNeedRedraw = 0x00424F10;
    const Stdcall<void()> TransportStatus_UpdateDrawnValues = 0x00424FC0;

    const Stdcall<void()> DrawAllMinimapUnits = 0x004A4AC0;

    const Stdcall<void(Ecx<Control *>, Edx<int>)> TriggerPortraitFinished = 0x0045E610;
    const Stdcall<int(Ecx<Unit *>, Edx<KillUnitArgs *>)> Trig_KillUnitGeneric = 0x004C7E20;

    const Stdcall<int(Ecx<Unit *>, Edx<FindUnitLocationParam *>)> FindUnitInLocation_Check = 0x004C6F70;
    const Stdcall<int(Ecx<Unit *>, Edx<ChangeInvincibilityParam *>)> ChangeInvincibility = 0x004C6B00;
    const Stdcall<int(Eax<const Unit *>, const Unit *)> CanLoadUnit = 0x004E6E00;
    const Stdcall<void(Eax<Unit *>, Ecx<Unit *>)> LoadUnit = 0x004E78E0;
    const Stdcall<int(Eax<Unit *>)> UnloadUnit = 0x004E7F70;
    const Stdcall<void(Esi<const Unit *>)> SendUnloadCommand = 0x004BFDB0;
    const Stdcall<int(Eax<const Unit *>)> HasLoadedUnits = 0x004E7110;
    const Stdcall<Unit *(Eax<Unit *>)> GetFirstLoadedUnit = 0x004E6C90;
    const Stdcall<int(Eax<const Unit *>)> IsCarryingFlag = 0x004F3A80;
    const Stdcall<int(Eax<Unit *>, int(__fastcall *)(Unit *, void *), void *)> ForEachLoadedUnit = 0x004E6D00;
    const Stdcall<void(Eax<Unit *>)> AddLoadedUnitsToCompletedUnitLbScore = 0x0045F870;
    const Stdcall<int(Ecx<const Unit *>)> GetUsedSpace = 0x004E7170;

    const Stdcall<void(int, Unit **)> SendChangeSelectionCommand = 0x004C0860;
    const Stdcall<void(uint8_t)> CenterOnSelectionGroup = 0x004967E0;
    const Stdcall<void(uint8_t)> SelectHotkeyGroup = 0x00496B40;
    const Stdcall<int(Unit *)> TrySelectRecentHotkeyGroup = 0x00496D30;
    const Stdcall<void(Eax<uint8_t>, int)> Command_SaveHotkeyGroup = 0x004965D0;
    const Stdcall<void(uint8_t)> Command_SelectHotkeyGroup = 0x00496940;

    const Stdcall<void(Edi<Dialog *>, int)> BriefingOk = 0x0046D090;
    const Stdcall<void()> ProcessLobbyCommands = 0x00486530;

    const Stdcall<void(Ecx<Event *>)> GameScreenRClickEvent = 0x004564E0;
    const Stdcall<void(Ecx<Event *>)> GameScreenLClickEvent_Targeting = 0x004BD500;

    const Stdcall<void(uint16_t, uint16_t, Unit *, uint16_t)> DoTargetedCommand = 0x0046F5B0;

    const Stdcall<Order *(Ecx<uint8_t>, uint32_t, Unit *, Edx<uint16_t>)> CreateOrder = 0x0048C510;
    const Stdcall<Order *(Eax<Order *>, Ecx<Unit *>)> DeleteOrder = 0x004742D0;
    const Stdcall<void(Ecx<Unit *>, Edx<uint8_t>)> DeleteSpecificOrder = 0x00474400;
    const Stdcall<void(Ecx<Unit *>)> DoNextQueuedOrder = 0x00475000;

    const Stdcall<Image *()> GetEmptyImage = 0x004D4E30;
    const Stdcall<void(Esi<Image *>)> DeleteImage = 0x004D4CE0;
    const Stdcall<Sprite *(uint16_t, uint16_t, Edi<uint16_t>, uint8_t)> CreateSprite = 0x004990F0;
    const Stdcall<void(Esi<Sprite *>)> ProgressSpriteFrame = 0x00497920;
    const Stdcall<void(Edi<Sprite *>)> DeleteSprite = 0x00497B40;

    const Stdcall<void(Ebx<Sprite *>, Eax<uint16_t>, int, int, int, int)> AddMultipleOverlaySprites = 0x00499660;

    const Stdcall<Unit *(uint8_t player, int, Esi<uint16_t>, Edx<uint16_t>, Edi<uint16_t>)>
        AllocateUnit = 0x004A06C0;
    const Stdcall<void()> InitUnitSystem_Hook = 0x0049F380;
    const Stdcall<void()> InitSpriteSystem = 0x00499900;

    const Stdcall<void(Ecx<Image *>, uint8_t)> SetIscriptAnimation = 0x004D8470;
    const Stdcall<void(Ecx<Image *>, Iscript::Script *, int, uint32_t *)> ProgressIscriptFrame = 0x004D74C0;

    const Stdcall<int(Eax<Unit *>, int)> Order_AttackMove_ReactToAttack = 0x00478370;
    const Stdcall<void(Eax<Unit *>, int)> Order_AttackMove_TryPickTarget = 0x00477820;

    const Stdcall<int(Ecx<Flingy *>)> ProgressFlingyTurning = 0x00494FE0;
    const Stdcall<void(Ecx<Flingy *>)> SetMovementDirectionToTarget = 0x00496140;
    const Stdcall<void(Esi<Flingy *>)> ProgressMove = 0x004956C0;

    const Stdcall<void()> GameEnd = 0x004EE8C0;

    const Stdcall<void(Esi<Unit *>)> AddToPositionSearch = 0x0046A3A0;
    const Stdcall<int(int)> FindUnitPosition = 0x00469B00;
    const Stdcall<Unit **(Ecx<Unit *>, int, int)> FindNearbyUnits = 0x00430190;
    const Stdcall<Unit **(const Rect16 *)> FindUnitsRect = 0x004308A0;

    const Stdcall<int(Ecx<const Unit *>, Eax<const Unit *>)> DoUnitsCollide = 0x00469B60;
    const Stdcall<Unit **(Ecx<Unit *>, Eax<int>, int)> CheckMovementCollision = 0x004304D0;
    const Stdcall<Unit **(Eax<const Rect16 *>)> FindUnitBordersRect = 0x0042FF80;
    const Stdcall<void()> ClearPositionSearch = 0x0042FEE0;
    const Stdcall<void(Esi<Unit *>, Eax<int>, int)> ChangeUnitPosition = 0x0046A000;
    const Stdcall<Unit *(Eax<Rect16 *>, Unit *, uint16_t, uint16_t, int, int, int, int,
           int (__fastcall *)(const Unit *, void *), void *)>
        FindNearestUnit = 0x004E8320;
    const Stdcall<void(Esi<PathingData *>)> GetNearbyBlockingUnits = 0x00422160;
    const Stdcall<void(Esi<Unit *>)> RemoveFromPosSearch = 0x0046A300;
    const Stdcall<int(Edi<const Unit *>, Ebx<const Unit *>)> GetDodgingDirection = 0x004F2A70;
    const Stdcall<int(Eax<const Unit *>, Edi<const CollisionArea *>)> DoesBlockArea = 0x0042E0E0;
    const Stdcall<Unit **(Eax<uint16_t>, uint16_t)> FindUnitsPoint = 0x004300E0;
    const Stdcall<int(Eax<Unit **>, Unit *, int, int, int, int)> IsTileBlockedBy = 0x00473300;
    const Stdcall<int(Eax<Unit *>, Edx<int>, Ecx<int>)> DoesBuildingBlock = 0x00473410;

    const Stdcall<uint32_t(Unit *)> UnitToIndex = 0x0047B1D0;
    const Stdcall<Unit *(uint32_t)> IndexToUnit = 0x0047B210;

    const Stdcall<void()> PathingInited = 0x0048433E;

    const Stdcall<void()> MakeDrawnSpriteList = 0x004BD3A0;
    const Stdcall<void()> PrepareDrawSprites = 0x00498CB0;
    const Stdcall<void()> DrawSprites = 0x00498D40;

    const Stdcall<Sprite *(uint16_t, uint16_t, Edi<uint16_t>, uint8_t)> CreateLoneSprite = 0x00488210;
    const Stdcall<Sprite *(uint16_t, Sprite *)> CreateFowSprite = 0x00488410;
    const Stdcall<void()> InitLoneSprites = 0x00488550;
    const Stdcall<int(void *, Eax<int>)> VisionSync = 0x0047CCD0;
    const Stdcall<void()> FullRedraw = 0x004BD595;
    const Stdcall<void(Eax<Sprite *>, int)> SetSpriteDirection = 0x00401140;
    const Stdcall<Sprite *(int, int, int)> FindBlockingFowResource = 0x00487B00;

    const Stdcall<void()> GenerateFog = 0x0047FC50;

    const Stdcall<void()> DrawCursorMarker = 0x00488180;
    const Stdcall<void(Eax<Unit *>)> ShowRallyTarget = 0x00468670;
    const Stdcall<void(Ecx<uint16_t>, Eax<uint16_t>)> ShowCursorMarker = 0x00488660;

    const Stdcall<Bullet *(Eax<Unit *>, int, int, uint8_t, uint8_t, Ecx<uint8_t>)>
        CreateBullet = 0x0048C260;

    const Stdcall<void(Eax<int>, Ecx<Unit *>, Unit *, int, int)> DamageUnit = 0x004797B0;
    const Stdcall<void(Ecx<Unit *>)> RemoveUnitFromBulletTargets = 0x0048AAC0;

    const hook_offset RngSeedPatch = 0x004EF192;

    const Stdcall<int(Eax<Unit *>)> ProgressUnstackMovement = 0x004F2160;
    const Stdcall<int(Eax<Unit *>)> MovementState13 = 0x0046BCC0;
    const Stdcall<int(Eax<Unit *>)> MovementState17 = 0x0046BC30;
    const Stdcall<int(Eax<Unit *>)> MovementState20 = 0x0046B000;
    const Stdcall<int(Eax<Unit *>)> MovementState1c = 0x0046BF60;
    const Stdcall<int(Eax<Unit *>)> MovementState_FollowPath = 0x0046B950;
    const Stdcall<int(Eax<Unit *>)> MovementState_Flyer = 0x0046B400;

    const Stdcall<int(Esi<Unit *>, Ebx<uint16_t>, Edi<uint16_t>)> ChangeMovementTarget = 0x004EB820;
    const Stdcall<int(Esi<Unit *>, Edi<Unit *>)> ChangeMovementTargetToUnit = 0x004EB720;

    const Stdcall<void(Ecx<const void *>)> ReplayCommands_Nothing = 0x004CDCE0;
    const Stdcall<int(int, Rect32 *, uint8_t **, int *, int)> SDrawLockSurface = 0x00411E4E;
    const Stdcall<int(int, uint8_t *, int, int)> SDrawUnlockSurface = 0x00411E48;
    const Stdcall<void(Eax<const uint8_t *>, int, int)> ProcessCommands = 0x004865D0;

    const Stdcall<int()> LoadGameObjects = 0x004CFEF0;

    const Stdcall<Path *(Edx<uint16_t *>, Esi<uint16_t *>)> AllocatePath = 0x004E42F0;
    const Stdcall<void(Esi<Unit *>)> DeletePath = 0x004E42A0;
    const Stdcall<void(Esi<Unit *>)> DeletePath2 = 0x0042F740;
    const Stdcall<void(Eax<Unit *>, uint32_t, uint32_t)> CreateSimplePath = 0x0042F830;
    const Stdcall<void()> InitPathArray = 0x0042F4F0;

    const Stdcall<void(Eax<MapDirEntry *>)> LoadReplayMapDirEntry = 0x004A7C30;
    const Stdcall<uint32_t(Eax<const char *>, Edi<uint32_t *>)> LoadReplayData = 0x004DF570;

    const Stdcall<int(Unit *, int, int, int, uint16_t, int, int, int, int)> UpdateBuildingPlacementState = 0x00473FB0;

    const Stdcall<void(Eax<Unit *>)> MedicRemove = 0x004477C0;
    const Stdcall<int(Edi<Unit *>, int)> RemoveWorkerOrBuildingAi = 0x00434C90;

    const Stdcall<void *(Ecx<int>, Eax<uint32_t *>, Edx<Tbl *>, GrpSprite **, void **, void **)>
        LoadGrp = 0x0047ABE0;
    const Stdcall<int(Eax<GrpFrameHeader *>, Edx<int>, Ecx<int>)> IsDrawnPixel = 0x004D4DB0;
    const Stdcall<void(const char *)> LoadBlendPalettes = 0x004BDE60;
    const Stdcall<Unit *(int, int)> FindUnitAtPoint = 0x0046F3A0;
    const Stdcall<void(Ecx<int>, Edx<int>, GrpFrameHeader *, Rect32 *, uint8_t *)>
        DrawImage_Detected = 0x0040B596;
    const Stdcall<void(Ecx<int>, Edx<int>, GrpFrameHeader *, Rect32 *, uint8_t *)>
        DrawImage_Detected_Flipped = 0x0040BCA3;
    const Stdcall<void(Ecx<int>, Edx<int>, GrpFrameHeader *, Rect32 *, int)>
        DrawUncloakedPart = 0x0040AFD5;
    const Stdcall<void(Ecx<int>, Edx<int>, GrpFrameHeader *, Rect32 *, int)>
        DrawUncloakedPart_Flipped = 0x0040B824;
    const Stdcall<void(Ecx<int>, Edx<int>, GrpFrameHeader *, Rect32 *, void *)>
        DrawImage_Cloaked = 0x0040B155;
    const Stdcall<void(Ecx<int>, Edx<int>, GrpFrameHeader *, Rect32 *, void *)>
        DrawImage_Cloaked_Flipped = 0x0040B9A9;
    const hook_offset DrawGrp = 0x0040ABBE;
    const hook_offset DrawGrp_Flipped = 0x0040BF60;

    const Stdcall<void(int, int, int, int, int, uint32_t, int)> MakeJoinedGameCommand = 0x00471FB0;
    const Stdcall<void(const uint8_t *, int)> Command_GameData = 0x00470840;

    const Stdcall<int()> InitGame = 0x004EEE00;
    const Stdcall<void()> InitStartingRacesAndTypes = 0x0049CC40;

    const Stdcall<void(Eax<uint8_t>)> NeutralizePlayer = 0x00489FC0;
    const Stdcall<void(Eax<Sprite *>)> MakeDetected = 0x00497ED0;

    const Stdcall<void(Sprite *)> AddDamageOverlay = 0x004993C0;
    const Stdcall<void(Unit *)> PlaySelectionSound = 0x0048F910;
}


#endif // OFFSETS_HOOKS_H

