#ifndef OFFSETS_FUNCS_H
#define OFFSETS_FUNCS_H

#include "types.h"

#include "patch/func.h"

void InitBwFuncs_1161(Common::PatchManager *exec_heap, uintptr_t current_base_address);
void InitStormFuncs_1161(Common::PatchManager *exec_heap, uintptr_t current_base_address);

namespace bw {
    template <class Signature>
    using Func = patch_func::Stdcall<Signature>;

    extern Func<void(Unit *)> ProgressSecondaryOrder_Hidden;
    extern Func<void(Unit *)> ProgressUnitMovement;
    extern Func<void(Unit *)> ProgressAcidSporeTimers;
    extern Func<void(Unit *)> ProgressEnergyRegen;
    extern Func<void(Unit *, int)> ProgressSubunitDirection;

    extern Func<bool(const Unit *, const Unit *)> IsOutOfRange;
    extern Func<bool(Unit *)> HasTargetInRange;
    extern Func<void(Unit *, Unit *, int, int)> AttackUnit;
    extern Func<void(Unit *)> Ai_StimIfNeeded;
    extern Func<bool(Unit *, int)> IsReadyToAttack;

    extern Func<Unit *(int, x32, y32, int)> CreateUnit;
    extern Func<int(Unit *, int, int, int, int)> InitializeUnitBase;
    extern Func<void(Unit *)> FinishUnit_Pre;
    extern Func<void(Unit *)> FinishUnit;
    extern Func<void(Unit *, int)> TransformUnit;
    extern Func<int(Unit *, int)> PrepareBuildUnit;
    extern Func<int(Unit *, int, int)> GiveUnit;

    extern Func<void(Unit *, int)> SetHp;
    extern Func<int(const Unit *, const Unit *)> GetMissChance;
    extern Func<int(const Unit *)> GetBaseMissChance;
    extern Func<int(const Unit *, int)> GetCurrentStrength;
    extern Func<bool(const Unit *)> IsMultiSelectable;

    extern Func<int(Unit *, int, int)> CheckUnitDatRequirements;
    extern Func<bool(const Unit *, const Unit *)> IsHigherRank;
    extern Func<bool(const Unit *, const Unit *)> IsTooClose;
    extern Func<bool(int, x32, y32, int)> IsPowered;

    extern Func<void(Unit *, MovementGroup *)> GetFormationMovementTarget;
    extern Func<int(Unit *)> ShowRClickErrorIfNeeded;
    extern Func<int(Unit *, int)> NeedsMoreEnergy;

    extern Func<void(Unit *, x32, y32)> MoveUnit;
    extern Func<void(Unit *)> MoveUnit_Partial;
    extern Func<void(Unit *)> HideUnit;
    extern Func<void(Unit *, int)> HideUnit_Partial;
    extern Func<void(Unit *)> ShowUnit;
    extern Func<void(Unit *)> DisableUnit;
    extern Func<void(Unit *)> AcidSporeUnit;

    extern Func<void(Unit *)> FinishMoveUnit;
    extern Func<void(Unit *)> PlayYesSoundAnim;
    extern Func<bool(uint16_t *, Unit *, Unit *)> GetUnloadPosition;
    extern Func<void(Unit *, int)> ModifyUnitCounters;
    extern Func<void(Unit *, int, int)> ModifyUnitCounters2;
    extern Func<void(Unit *)> AddToCompletedUnitLbScore;

    extern Func<bool(Unit *, int, int)> CanPlaceBuilding;
    extern Func<void(Unit *, int, int)> ClearBuildingTileFlag;
    extern Func<void(Unit *, int)> RemoveReferences;
    extern Func<void(Unit *)> StopMoving;
    extern Func<void(Unit *)> RemoveFromMap;
    extern Func<void(Unit *)> DropPowerup;

    extern Func<void(Unit *)> UpdateVisibility;
    extern Func<void(Unit *)> UpdateDetectionStatus;
    extern Func<void(Unit *)> RemoveFromCloakedUnits;
    extern Func<void(Unit *, int)> BeginInvisibility;
    extern Func<void(Unit *, int)> EndInvisibility;

    extern Func<void(Unit *)> Unburrow;
    extern Func<void(Unit *)> CancelBuildingMorph;
    extern Func<void(int, int)> RefundFullCost;
    extern Func<void(int, int)> RefundFourthOfCost;
    extern Func<void(Unit *)> DeletePowerupImages;
    extern Func<bool(Unit *, Unit *, int)> IsPointAtUnitBorder;

    extern Func<void(const void *, int)> SendCommand;
    extern Func<Unit *()> NextCommandedUnit;
    extern Func<bool(int, int)> IsOutsideGameScreen;
    extern Func<void(Control *)> MarkControlDirty;
    extern Func<void(Rect32 *)> CopyToFrameBuffer;

    extern Func<bool(const Unit *, const Unit *)> HasToDodge;
    extern Func<void(Contour *, PathingData *, Contour **, Contour **)> InsertContour;
    extern Func<void(MovementGroup *, int)> PrepareFormationMovement;
    extern Func<int(x32, y32, x32, y32)> GetFacingDirection;
    extern Func<int(const Unit *, const Unit *)> GetOthersLocation;
    extern Func<int(int, int)> GetEnemyAirStrength;
    extern Func<int(int, int, int)> GetEnemyStrength;
    extern Func<bool(Unit *, int, int)> CanWalkHere;
    extern Func<bool(int, int, int)> AreConnected;
    extern Func<int(Unit *, uint32_t)> MakePath;
    extern Func<bool(Unit *, int)> UpdateMovementState;
    extern Func<Unit *(Unit *)> FindCollidingUnit;
    extern Func<int(Unit *)> TerrainCollision;
    extern Func<bool(const Unit *, int, int, int)> DoesBlockPoint;

    extern Func<bool(const void *, int, File *)> WriteCompressed;
    extern Func<bool(void *, int, File *)> ReadCompressed;
    extern Func<void(File *)> SaveDisappearingCreepChunk;
    extern Func<void(File *)> SaveDatChunk;
    extern Func<void(File *)> SaveTriggerChunk;
    extern Func<void(File *, const char *)> WriteReadableSaveHeader;
    extern Func<void(uint32_t, File *)> WriteSaveHeader;
    extern Func<void(char *, int)> ReplaceWithShortPath;
    extern Func<void(char *, int)> ReplaceWithFullPath;
    extern Func<uint32_t()> FirstCommandUser;
    extern Func<bool(const char *, int, int)> IsInvalidFilename;
    extern Func<bool(const char *, char *, int, int)> GetUserFilePath;
    extern Func<void(const char *)> ShowWaitDialog;
    extern Func<void()> HidePopupDialog;

    extern Func<void()> DeleteAiRegions;
    extern Func<void(int)> AllocateAiRegions;
    extern Func<bool(File *)> LoadDisappearingCreepChunk;
    extern Func<bool(File *)> LoadTriggerChunk;
    extern Func<bool(File *, int)> LoadDatChunk;
    extern Func<void()> RestorePylons;

    extern Func<int(int, Unit *, int, int)> PlaySound;
    extern Func<void(int, uint32_t, int, int)> PlaySoundAtPos;
    extern Func<void(const char *, int, int)> PrintText;
    extern Func<void(int, int, int)> ShowInfoMessage;
    extern Func<void(const char *, int, Unit *)> ShowErrorMessage;
    extern Func<void(const char *)> PrintInfoMessage;
    extern Func<void(const char *, int)> PrintInfoMessageForLocalPlayer;
    extern Func<void()> ToggleSound;
    extern Func<void(Unit *, int, int, int)> TalkingPortrait;

    extern Func<int(x32, y32, x32, y32)> Distance;
    extern Func<bool(const Unit *, int, x32, y32)> IsPointInArea;
    extern Func<bool(const Unit *, int, const Unit *)> IsInArea;
    extern Func<void(int)> ProgressTime;
    extern Func<void(TriggerList *)> ProgressTriggerList;
    extern Func<void(Trigger *)> ProgressActions;
    extern Func<void()> ApplyVictory;
    extern Func<void()> CheckVictoryState;
    extern Func<void(Control *, int)> DeleteTimer;
    extern Func<Unit *(int, int, int)> FindUnitInLocation;
    extern Func<void(int, int, int)> PingMinimap;
    extern Func<void(int, int, int, int)> Trigger_Portrait;
    extern Func<const char *(int)> GetChkString;
    extern Func<uint32_t(const char *)> GetTextDisplayTime;
    extern Func<void(const char *, int)> Trigger_DisplayText;

    extern Func<bool(Unit *, Unit *, int)> CanHitUnit;
    extern Func<void(Bullet *)> ProgressBulletMovement;
    extern Func<void(void *, int, int)> ChangeMovePos;
    extern Func<void(Unit *)> UpdateDamageOverlay;
    extern Func<void(int, int, x32, y32, int)> ShowArea;

    extern Func<void(void *)> ChangeDirectionToMoveWaypoint;
    extern Func<void(void *)> ProgressSpeed;
    extern Func<void(void *)> UpdateIsMovingFlag;
    extern Func<void(void *)> ChangedDirection;
    extern Func<void(void *, int, int)> ProgressMoveWith;
    extern Func<bool(Flingy *)> MoveFlingy;
    extern Func<void(Flingy *, int)> SetSpeed;

    extern Func<void(Sprite *)> PrepareDrawSprite;
    extern Func<void(Sprite *)> DrawSprite;
    extern Func<uint8_t(int, int, int, int)> GetAreaVisibility;
    extern Func<uint8_t(int, int, int, int)> GetAreaExploration;
    extern Func<void(Sprite *, int)> SetVisibility;
    extern Func<void(Sprite *, int)> DrawTransmissionSelectionCircle;
    extern Func<void(int)> DrawOwnMinimapUnits;
    extern Func<void(int, x32, y32, int, int, int)> DrawMinimapDot;
    extern Func<void(int)> DrawNeutralMinimapUnits;
    extern Func<void(int)> DrawMinimapUnits;
    extern Func<void(Sprite *, int, int)> MoveSprite;
    extern Func<Unit *(x32, y32, int)> FindFowUnit;
    extern Func<void(Sprite *, int, int, int, int)> AddOverlayHighest;
    extern Func<void(Sprite *, int, int, int, int)> AddOverlayBelowMain;

    extern Func<void(Image *)> PrepareDrawImage;
    extern Func<void(Image *)> MarkImageAreaForRedraw;
    extern Func<void(Image *, int)> SetImageDirection32;
    extern Func<void(Image *, int)> SetImageDirection256;

    extern Func<int(const Unit *, int)> MatchesHeight;
    extern Func<int(int, int)> GetTerrainHeight;
    extern Func<void(int, x32, y32, int)> UpdateCreepDisappearance;
    extern Func<bool(Unit *, int, int)> Ai_AreOnConnectedRegions;
    extern Func<bool(int, x32, y32)> DoesFitHere;
    extern Func<bool(Rect16 *, Unit *, uint16_t *, uint16_t *, int, int)> GetFittingPosition;
    extern Func<int(int, uint16_t *)> ClipPointInBoundsForUnit;

    extern Func<void(int, int)> MoveScreen;
    extern Func<void(int)> ClearSelection;
    extern Func<bool(int)> HasTeamSelection;
    extern Func<bool(int, int, Unit *)> AddToPlayerSelection;
    extern Func<bool(Unit * const *, int)> UpdateSelectionOverlays;
    extern Func<void(Unit *)> MakeDashedSelectionCircle;
    extern Func<void(Sprite *)> RemoveDashedSelectionCircle;
    extern Func<int(Unit *, int)> RemoveFromSelection;
    extern Func<void(Unit *)> RemoveFromSelections;
    extern Func<void(Unit *)> RemoveFromClientSelection3;

    extern Func<int(int, Unit *, int)> CanUseTech;
    extern Func<int(Unit *, x32, y32, int16_t *, int, Unit *, int)> CanTargetSpell;
    extern Func<int(int, Unit *, int)> CanTargetSpellOnUnit;
    extern Func<int(Unit *, int, int, uint16_t *, int)> SpellOrder;

    extern Func<void(Unit *, int)> ApplyStasis;
    extern Func<void(Unit *)> ApplyEnsnare;
    extern Func<void(Unit *, int)> ApplyMaelstrom;
    extern Func<void(Unit *)> UpdateSpeed;
    extern Func<void(Unit *)> EndStasis;
    extern Func<void(Unit *)> EndLockdown;
    extern Func<void(Unit *)> EndMaelstrom;

    extern Func<Unit*(int, Unit *)> Hallucinate;
    extern Func<int(Unit *)> PlaceHallucination;
    extern Func<int(Unit *, int, int)> CanIssueOrder;
    extern Func<bool(Unit *, Unit *, int, int16_t *)> CanTargetOrder;
    extern Func<bool(Unit *, int, int, int16_t *)> CanTargetOrderOnFowUnit;
    extern Func<void(Unit *)> DoNextQueuedOrderIfAble;

    extern Func<uint32_t(Unit *, const Unit *)> PrepareFlee;
    extern Func<bool(Unit *, Unit *)> Flee;
    extern Func<Unit *(Unit *, int)> FindNearestUnitOfId;
    extern Func<void(Ai::Region *, int)> ChangeAiRegionState;
    extern Func<bool(Unit *)> Ai_ReturnToNearestBaseForced;
    extern Func<void(Unit *, Unit *)> Ai_Detect;
    extern Func<bool(Unit *, int)> Ai_CastReactionSpell;
    extern Func<bool(Ai::Town *)> TryBeginDeleteTown;
    extern Func<Unit *(int, x32, y32, int)> FindNearestAvailableMilitary;
    extern Func<void(Ai::GuardAi *, int)> Ai_GuardRequest;
    extern Func<bool(Unit *, Unit *)> Ai_ShouldKeepTarget;
    extern Func<void(Ai::Region *)> Ai_RecalculateRegionStrength;
    extern Func<void(int, int)> Ai_PopSpendingRequestResourceNeeds;
    extern Func<void(int)> Ai_PopSpendingRequest;
    extern Func<bool(int, int)> Ai_DoesHaveResourcesForUnit;
    extern Func<bool(Unit *, int, int, void *)> Ai_TrainUnit;
    extern Func<void(Unit *, Unit*)> InheritAi2;

    extern Func<Ai::UnitAi *(Ai::Region *, int, int, int, Ai::Region *)> Ai_FindNearestActiveMilitaryAi;
    extern Func<Ai::UnitAi *(Ai::Region *, int, int, int, int, Ai::Region *)> Ai_FindNearestMilitaryOrSepContAi;
    extern Func<bool(Unit *, int, int)> Ai_PrepareMovingTo;
    extern Func<void(Unit *, int, int, int)> ProgressMilitaryAi;
    extern Func<void(Ai::Region *)> Ai_UpdateSlowestUnitInRegion;
    extern Func<void(Ai::Script *)> ProgressAiScript;
    extern Func<void(Unit *, int)> RemoveFromAiStructs;
    extern Func<void(int, Unit *)> Ai_UpdateRegionStateUnk;
    extern Func<void(Unit *)> Ai_UnloadFailure;
    extern Func<bool(int, int, int, int, int)> Ai_AttackTo;
    extern Func<void(int)> Ai_EndAllMovingToAttack;

    extern Func<void()> DrawFlashingSelectionCircles;
    extern Func<void()> Replay_RefershUiIfNeeded;
    extern Func<bool(uint32_t *)> ProgressTurns;
    extern Func<void()> Victory;

    extern Func<bool(Unit *)> UpdateCreepDisappearance_Unit;
    extern Func<void()> TryUpdateCreepDisappear;
    extern Func<void()> ProgressCreepDisappearance;
    extern Func<void()> UpdateFog;
    extern Func<void(Unit *)> RevealSightArea;

    extern Func<void()> Ai_ProgressRegions;
    extern Func<void()> UpdateResourceAreas;
    extern Func<void()> Ai_Unk_004A2A40;
    extern Func<void()> AddSelectionOverlays;
    extern Func<int(int, int, int, int)> IsCompletelyHidden;
    extern Func<int(int, int, int, int)> IsCompletelyUnExplored;
    extern Func<int()> Ui_NeedsRedraw_Unk;
    extern Func<int()> GenericStatus_DoesNeedRedraw;
    extern Func<int(int, int, Unit *)> IsOwnedByPlayer;
    extern Func<int(Unit *)> CanControlUnit;
    extern Func<void()> AddToRecentSelections;

    extern Func<void()> EndTargeting;
    extern Func<void()> MarkPlacementBoxAreaDirty;
    extern Func<void()> EndBuildingPlacement;
    extern Func<Image *(Sprite *, int, int, int, int)> AddOverlayNoIscript;
    extern Func<void(int)> SetCursorSprite;
    extern Func<void(Sprite *)> RemoveSelectionCircle;
    extern Func<int(const Unit *, uint32_t)> DoUnitsBlock;
    extern Func<void(Unit *)> Notify_UnitWasHit;
    extern Func<int(void *)> STransBind;
    extern Func<int(void *, uint8_t *, int, void **)> STrans437;
    extern Func<int(int, int, int, int)> ContainsDirtyArea;
    extern Func<void()> CopyGameScreenToFramebuf;
    extern Func<void(int)> ShowLastError;

    extern Func<bool(const void *)> Command_Sync_Main;
    extern Func<void(const void *)> Command_Load;
    extern Func<void()> Command_Restart;
    extern Func<void()> Command_Pause;
    extern Func<void()> Command_Resume;
    extern Func<void(const void *)> Command_Build;
    extern Func<void(const void *)> Command_MinimapPing;
    extern Func<void(const void *)> Command_Vision;
    extern Func<void(const void *)> Command_Ally;
    extern Func<void(const void *)> Command_Cheat;
    extern Func<void(const void *)> Command_Hotkey;
    extern Func<void()> Command_CancelBuild;
    extern Func<void()> Command_CancelMorph;
    extern Func<void(const void *)> Command_Stop;
    extern Func<void()> Command_CarrierStop;
    extern Func<void()> Command_ReaverStop;
    extern Func<void()> Command_Order_Nothing;
    extern Func<void(const void *)> Command_ReturnCargo;
    extern Func<void(const void *)> Command_Train;
    extern Func<void(const void *)> Command_CancelTrain;
    extern Func<void(const void *)> Command_Tech;
    extern Func<void()> Command_CancelTech;
    extern Func<void(const void *)> Command_Upgrade;
    extern Func<void()> Command_CancelUpgrade;
    extern Func<void(const void *)> Command_Burrow;
    extern Func<void()> Command_Unburrow;
    extern Func<void()> Command_Cloak;
    extern Func<void()> Command_Decloak;
    extern Func<void(const void *)> Command_UnitMorph;
    extern Func<void(const void *)> Command_BuildingMorph;
    extern Func<void(const void *)> Command_Unsiege;
    extern Func<void(const void *)> Command_Siege;
    extern Func<void()> Command_MergeArchon;
    extern Func<void()> Command_MergeDarkArchon;
    extern Func<void(const void *)> Command_HoldPosition;
    extern Func<void()> Command_CancelNuke;
    extern Func<void(const void *)> Command_Lift;
    extern Func<void()> Command_TrainFighter;
    extern Func<void()> Command_CancelAddon;
    extern Func<void()> Command_Stim;
    extern Func<void(const void *)> Command_Latency;
    extern Func<void(const void *)> Command_LeaveGame;
    extern Func<void(const void *)> Command_UnloadAll;

    extern Func<void(int, int, int)> ChangeReplaySpeed;
    extern Func<void(ReplayData *, int, const void *, int)> AddToReplayData;
    extern Func<void(Control *, const char *, int)> SetLabel;

    extern Func<void(TriggerList *)> FreeTriggerList;
    extern Func<void(uint32_t)> Storm_LeaveGame;
    extern Func<void(Control *)> RemoveDialog;
    extern Func<void()> ResetGameScreenEventHandlers;
    extern Func<void()> DeleteDirectSound;
    extern Func<void()> StopSounds;
    extern Func<void(int)> InitOrDeleteRaceSounds;
    extern Func<void()> FreeMapData;
    extern Func<void()> FreeGameDialogs;
    extern Func<void()> FreeEffectsSCodeUnk;
    extern Func<void()> WindowPosUpdate;
    extern Func<void()> ReportGameResult;
    extern Func<void()> ClearNetPlayerData;
    extern Func<void(void *)> FreeUnkSound;
    extern Func<void(int)> Unpause;

    extern Func<void(Image *)> DeleteHealthBarImage;
    extern Func<void(Image *)> DeleteSelectionCircleImage;
    extern Func<void(Unit *, int, int)> SetBuildingTileFlag;
    extern Func<void(Unit *)> CheckUnstack;
    extern Func<void(Unit *)> IncrementAirUnitx14eValue;
    extern Func<void(Unit *)> ForceMoveTargetInBounds;
    extern Func<bool(Unit *)> ProgressRepulse;
    extern Func<void(Unit *, int)> FinishRepulse;
    extern Func<void(Unit *)> FinishUnitMovement;

    extern Func<bool(Unit *, Unit *)> IsInFrontOfMovement;
    extern Func<void(Unit *)> Iscript_StopMoving;
    extern Func<void(Unit *)> InstantStop;
    extern Func<void(Unit *, int)> SetSpeed_Iscript;

    extern Func<const char *(int)> GetGluAllString;
    extern Func<void(GameData *, Player *)> ReadStruct245;
    extern Func<bool(const char *, void *, int)> PreloadMap;
    extern Func<void()> AllocateReplayCommands;
    extern Func<bool(File *)> LoadReplayCommands;

    extern Func<int(const Unit *, const Unit *)> GetThreatLevel;
    extern Func<void(Unit *)> Ai_FocusUnit;
    extern Func<void(Unit *)> Ai_FocusUnit2;

    extern Func<void(Unit *)> Order_JunkYardDog;
    extern Func<void(Unit *)> Order_Medic;
    extern Func<void(Unit *)> Order_Obscured;
    extern Func<void(Unit *)> Order_Spell;
    extern Func<void(Unit *)> Order_WatchTarget;
    extern Func<void(Unit *)> Order_ReaverAttack;
    extern Func<void(Unit *)> Order_Unload;
    extern Func<void(Unit *)> Order_TowerGuard;
    extern Func<void(Unit *)> Order_TowerAttack;
    extern Func<void(Unit *)> Order_InitCreepGrowth;
    extern Func<void(Unit *)> Order_StoppingCreepGrowth;
    extern Func<void(Unit *)> Order_Stop;
    extern Func<void(Unit *)> Order_StayInRange;
    extern Func<void(Unit *)> Order_Scan;
    extern Func<void(Unit *)> Order_ScannerSweep;
    extern Func<void(Unit *)> Order_ReturnResource;
    extern Func<void(Unit *)> Order_RescuePassive;
    extern Func<void(Unit *)> Order_RightClick;
    extern Func<void(Unit *)> Order_MoveToInfest;
    extern Func<void(Unit *)> Order_InfestMine4;
    extern Func<void(Unit *)> Order_BuildProtoss2;
    extern Func<void(Unit *)> Order_PowerupIdle;
    extern Func<void(Unit *)> Order_PlaceMine;
    extern Func<void(Unit *)> Order_TransportIdle;
    extern Func<void(Unit *)> Order_Patrol;
    extern Func<void(Unit *)> Order_NukePaint;
    extern Func<void(Unit *)> Order_Pickup4;
    extern Func<void(Unit *)> Order_LiftingOff;
    extern Func<void(Unit *)> Order_InitPylon;
    extern Func<void(Unit *)> Order_Move;
    extern Func<void(Unit *)> Order_MoveToMinerals;
    extern Func<void(Unit *)> Order_WaitForMinerals;
    extern Func<void(Unit *)> Order_WaitForGas;
    extern Func<void(Unit *)> Order_HarvestGas;
    extern Func<void(Unit *)> Order_MoveToHarvest;
    extern Func<void(Unit *)> Order_Follow;
    extern Func<void(Unit *)> Order_Trap;
    extern Func<void(Unit *)> Order_HideTrap;
    extern Func<void(Unit *)> Order_RevealTrap;
    extern Func<void(Unit *)> Order_HarassMove;
    extern Func<void(Unit *)> Order_UnusedPowerup;
    extern Func<void(Unit *)> Order_EnterTransport;
    extern Func<void(Unit *)> Order_EnterNydus;
    extern Func<void(Unit *)> Order_DroneStartBuild;
    extern Func<void(Unit *)> Order_DroneLand;
    extern Func<void(Unit *)> Order_EnableDoodad;
    extern Func<void(Unit *)> Order_DisableDoodad;
    extern Func<void(Unit *)> Order_OpenDoor;
    extern Func<void(Unit *)> Order_CloseDoor;
    extern Func<void(Unit *)> Order_Burrow;
    extern Func<void(Unit *)> Order_Burrowed;
    extern Func<void(Unit *)> Order_Unburrow;
    extern Func<void(Unit *)> Order_CtfCopInit;
    extern Func<void(Unit *)> Order_ComputerReturn;
    extern Func<void(Unit *)> Order_CarrierIgnore2;
    extern Func<void(Unit *)> Order_CarrierStop;
    extern Func<void(Unit *)> Order_CarrierAttack;
    extern Func<void(Unit *)> Order_BeingInfested;
    extern Func<void(Unit *)> Order_RechargeShieldsBattery;
    extern Func<void(Unit *)> Order_AiAttackMove;
    extern Func<void(Unit *)> Order_AttackFixedRange;
    extern Func<void(Unit *)> Order_LiftOff;
    extern Func<void(Unit *)> Order_TerranBuildSelf;
    extern Func<void(Unit *)> Order_ZergBuildSelf;
    extern Func<void(Unit *)> Order_ConstructingBuilding;
    extern Func<void(Unit *)> Order_Critter;
    extern Func<void(Unit *)> Order_StopHarvest;
    extern Func<void(Unit *)> Order_UnitMorph;
    extern Func<void(Unit *)> Order_NukeTrain;
    extern Func<void(Unit *)> Order_CtfCop2;
    extern Func<void(Unit *)> Order_TankMode;
    extern Func<void(Unit *)> Order_TurretAttack;
    extern Func<void(Unit *)> Order_TurretGuard;
    extern Func<void(Unit *)> Order_ResetCollision1;
    extern Func<void(Unit *)> Order_ResetCollision2;
    extern Func<void(Unit *)> Order_Upgrade;
    extern Func<void(Unit *)> Order_Birth;
    extern Func<void(Unit *)> Order_Heal;
    extern Func<void(Unit *)> Order_Tech;
    extern Func<void(Unit *)> Order_Repair;
    extern Func<void(Unit *)> Order_BuildNydusExit;
    extern Func<void(Unit *)> Order_NukeTrack;
    extern Func<void(Unit *)> Order_MedicHoldPosition;
    extern Func<void(Unit *)> Order_MineralHarvestInterrupted;
    extern Func<void(Unit *)> Order_InitArbiter;
    extern Func<void(Unit *)> Order_CompletingArchonSummon;
    extern Func<void(Unit *)> Order_Guard;
    extern Func<void(Unit *)> Order_CtfCopStarted;
    extern Func<void(Unit *)> Order_RechargeShieldsUnit;
    extern Func<void(Unit *)> Order_Interrupted;
    extern Func<void(Unit *)> Order_HealToIdle;
    extern Func<void(Unit *)> Order_Reaver;
    extern Func<void(Unit *)> Order_Neutral;
    extern Func<void(Unit *)> Order_PickupBunker;
    extern Func<void(Unit *)> Order_PickupTransport;
    extern Func<void(Unit *)> Order_Carrier;
    extern Func<void(Unit *)> Order_WarpIn;
    extern Func<void(Unit *)> Order_BuildProtoss1;
    extern Func<void(Unit *)> Order_AiPatrol;
    extern Func<void(Unit *)> Order_AttackMove;
    extern Func<void(Unit *)> Order_BuildTerran;
    extern Func<void(Unit *)> Order_HealMove;
    extern Func<void(Unit *)> Order_ReaverStop;
    extern Func<void(Unit *)> Order_DefensiveMatrix;
    extern Func<void(Unit *)> Order_BuildingMorph;
    extern Func<void(Unit *)> Order_PlaceAddon;
    extern Func<void(Unit *)> Order_BunkerGuard;
    extern Func<void(Unit *)> Order_BuildAddon;
    extern Func<void(Unit *)> Order_TrainFighter;
    extern Func<void(Unit *)> Order_ShieldBattery;
    extern Func<void(Unit *)> Order_SpawningLarva;
    extern Func<void(Unit *)> Order_SpreadCreep;
    extern Func<void(Unit *)> Order_Cloak;
    extern Func<void(Unit *)> Order_Decloak;
    extern Func<void(Unit *)> Order_CloakNearbyUnits;

    extern Func<bool(const Unit *unit, int, int, int)> CheckFiringAngle;
    extern Func<bool(Unit *)> IsMovingToMoveWaypoint;
    extern Func<uint32_t(int, int)> CalculateBaseStrength;
    extern Func<uint32_t(int, int)> FinetuneBaseStrength;
    extern Func<uint32_t(Unit *, int)> CalculateSpeedChange;
    extern Func<void(Flingy *, int)> SetDirection;
    extern Func<bool(Unit *)> ShouldStopOrderedSpell;
    extern Func<void(Unit *, int)> Iscript_AttackWith;
    extern Func<void(int, Unit *)> Iscript_UseWeapon;
    extern Func<void(Unit *, uint32_t *, uint32_t *)> GetClosestPointOfTarget;
    extern Func<void(int, Flingy *)> SetMoveTargetToNearbyPoint;
    extern Func<void(Unit *, int)> FireWeapon;
    extern Func<void(Image *img)> SetOffsetToParentsSpecialOverlay;

    extern Func<uint32_t(int, int, int, int)> GetPsiPlacementState;
    extern Func<uint32_t(int, int, int, int)> GetGasBuildingPlacementState;
    extern Func<uint32_t(int, int, int, int, int, int)> UpdateNydusPlacementState;
    extern Func<uint32_t(int, int, int, int, int, int, int)> UpdateCreepBuildingPlacementState;
    extern Func<uint32_t(int, int, int, int, int, int, int)> UpdateBuildingPlacementState_MapTileFlags;
    extern Func<uint32_t(Unit *, int, int, int, int, int, int, int, int, int)>
        UpdateBuildingPlacementState_Units;

    extern Func<void(Unit *, Unit *)> MoveTowards;
    extern Func<void(Unit *, Unit *)> MoveToCollide;
    extern Func<void(Unit *, Unit *)> InheritAi;
    extern Func<void(Unit *, int)> MutateBuilding;
    extern Func<void(int)> ReduceBuildResources;
    extern Func<int(int, int, int)> CheckSupplyForBuilding;
    extern Func<Unit *(int, Unit *)> BeginGasBuilding;
    extern Func<void(Unit *)> StartZergBuilding;

    extern Func<void(Unit *, Unit *)> BeginHarvest;
    extern Func<void(Unit *)> AddResetHarvestCollisionOrder;
    extern Func<bool(Flingy *)> IsFacingMoveTarget;
    extern Func<void(Unit *, Unit *)> FinishedMining;
    extern Func<bool(Unit *)> Ai_CanMineExtra;
    extern Func<void(int, int, Unit *, int)> CreateResourceOverlay;
    extern Func<void(Unit *)> UpdateMineralAmountAnimation;
    extern Func<void(Unit *, Unit *)> MergeArchonStats;

    extern Func<Unit *(Unit *)> SpiderMine_FindTarget;
    extern Func<void(Unit *)> Burrow_Generic;
    extern Func<void(Unit *)> InstantCloak;
    extern Func<void(Unit *)> Unburrow_Generic;

    extern Func<void(Unit *)> DetachAddon;
    extern Func<void(Unit *)> CancelTech;
    extern Func<void(Unit *)> CancelUpgrade;
    extern Func<void()> EndAddonPlacement;
    extern Func<void(int, int, Sprite *)> ReplaceSprite;

    extern Func<bool(Unit *, int, int)> IsGoodLarvaPosition;
    extern Func<void(Unit *, uint32_t *, uint32_t *)> GetDefaultLarvaPosition;
    extern Func<bool(Unit *)> Ai_UnitSpecific;
    extern Func<void(Unit *)> Ai_WorkerAi;
    extern Func<bool(Unit *)> Ai_TryProgressSpendingQueue;
    extern Func<void(Unit *)> Ai_Military;
    extern Func<bool(Unit *, int)> Ai_IsInAttack;
    extern Func<Unit *(Unit *)> Ai_FindNearestRepairer;
    extern Func<void(Unit *)> Ai_SiegeTank;
    extern Func<void(Unit *)> Ai_Burrower;
    extern Func<bool(Unit *)> Ai_IsMilitaryAtRegionWithoutState0;

    extern Func<void(Unit *, Unit *)> LoadFighter;

    extern Func<void(int)> Command_StartGame;
    extern Func<void()> DrawDownloadStatuses;
    extern Func<void(int, const void *)> Command_NewNetPlayer;
    extern Func<void(int, const void *)> Command_ChangeGameSlot;
    extern Func<void(const void *, int)> Command_ChangeRace;
    extern Func<void(const void *, int)> Command_TeamGameTeam;
    extern Func<void(const void *, int)> Command_UmsTeam;
    extern Func<void(const void *, int)> Command_MeleeTeam;
    extern Func<void(const void *, int)> Command_SwapPlayers;
    extern Func<void(const void *, int)> Command_SavedData;
    extern Func<void()> MakeGamePublic;
    extern Func<void(Control *)> Ctrl_LeftUp;

    extern Func<void(int, int, int, void *)> RemoveCreepAtUnit;
    extern Func<void(Unit *, int)> GiveSprite;
    extern Func<void(Unit *)> RedrawGasBuildingPlacement;
    extern Func<Unit *(Unit *)> PickReachableTarget;
    extern Func<bool(Unit *)> CanSeeTarget;

    extern Func<void(void *, int, void *, void *)> ReadFile_Overlapped;
    extern Func<void *(char *)> OpenGrpFile;
    extern Func<void(void *, int)> FileError;
    extern Func<void(int, const char *)> ErrorMessageBox;
    extern Func<void *(const char *, int, int, const char *, int, int, uint32_t *)> ReadMpqFile;

    extern Func<Unit **(int, int)> GetClickableUnits;
    extern Func<bool(Unit *, int, int)> IsClickablePixel;

    extern Func<void(int, int)> KickPlayer;
    extern Func<void(int, int, int, int)> InitNetPlayer;
    extern Func<void(int)> InitPlayerStructs;
    extern Func<void(Player *)> InitPlayerSlot;
    extern Func<void(int)> InitNetPlayerInfo;
    extern Func<uint32_t()> CountFreeSlots;
    extern Func<void(int)> SetFreeSlots;
    extern Func<void(int)> SendInfoRequestCommand;
    extern Func<void(MapDl *, int)> InitMapDownload;
    extern Func<int()> GetFirstFreeHumanPlayerId;
    extern Func<int()> GetFreeSlotFromEmptiestTeam;
    extern Func<int()> GetTeamGameTeamSize;

    extern Func<void()> InitFlingies;
    extern Func<void()> InitAi;
    extern Func<void()> InitText;
    extern Func<void()> InitTerrain;
    extern Func<void()> InitSprites;
    extern Func<void()> InitImages;
    extern Func<void()> InitColorCycling;
    extern Func<void(void *, void *)> UpdateColorPaletteIndices;
    extern Func<void()> InitTransparency;
    extern Func<void()> InitScoreSupply;
    extern Func<void()> LoadMiscDat;
    extern Func<void()> InitSpriteVisionSync;
    extern Func<void()> InitScreenPositions;
    extern Func<void()> InitTerrainAi;
    extern Func<void()> CloseBnet;
    extern Func<void(const char *, int, const char *, int)> BwError;
    extern Func<uint32_t()> LoadChk;
    extern Func<void()> CreateTeamGameStartingUnits;
    extern Func<void()> CreateStartingUnits;
    extern Func<void()> InitUnitSystem;
    extern Func<void()> InitPylonSystem;

    extern Func<void(void *, const char *)> LoadDat;
    extern Func<void(const char *, char *, int)> GetReplayPath;
    extern Func<bool(ReplayData *, File *)> WriteReplayData;
    extern Func<void *(uint32_t *, const char *)> ReadChk;
    extern Func<void(Unit *)> GiveAi;

    extern Func<bool(Unit *)> Interceptor_Attack;
    extern Func<void(Unit *, int, uint32_t)> Interceptor_Move;

    extern Func<bool(Unit *)> AttackRecentAttacker;
    extern Func<void(Unit *)> NeutralizeUnit;
    extern Func<void(Sprite *)> RemoveCloakDrawfuncs;

    extern Func<void(Unit *)> FlyingBuilding_SwitchedState;
    extern Func<void(Unit *)> FlyingBuilding_LiftIfStillBlocked;
    extern Func<Unit *(Unit *)> FindClaimableAddon;
    extern Func<void(Unit *, Unit *)> AttachAddon;
    extern Func<void(Unit *)> ShowLandingError;

    extern Func<bool(Unit *, Unit *)> Ai_IsUnreachable;
    extern Func<void(Unit *, Unit *)> MoveForMeleeRange;
    extern Func<Unit *(Unit *, int, int)> BeginTrain;
    extern Func<int(Unit *)> GetBuildHpGain;
    extern Func<int(Unit *, int, int)> ProgressBuild;
    extern Func<void(Unit *, Unit *)> RallyUnit;
    extern Func<void(int, int, int, int)> AiScript_StartTown;
}

namespace storm {
    template <class Signature>
    using Func = patch_func::Stdcall<Signature>;

    extern Func<void *(uint32_t, const char *, uint32_t, uint32_t)> SMemAlloc;
    extern Func<void(void *, const char *, uint32_t, uint32_t)> SMemFree;
    extern Func<void(void *)> SFileCloseArchive;
    extern Func<void(void *)> SFileCloseFile;
    extern Func<uint32_t(void *, void *, void *, void *, void *)> SNetInitializeProvider;
    extern Func<uint32_t(int, char *, int)> SNetGetPlayerName;
    extern Func<uint32_t(void *, uint32_t *)> SFileGetFileSize;
    extern Func<uint32_t(int, const void *, int, int, void *, int, uint32_t *, uint32_t *, int)> SBmpDecodeImage;
}

#endif /* OFFSETS_FUNCS_H */
