#include "offsets_funcs.h"

#include "patch/func.hpp"

const uintptr_t starcraft_exe_base = 0x00400000;
const uintptr_t storm_dll_base = 0x15000000;

using patch_func::Eax;
using patch_func::Ecx;
using patch_func::Edx;
using patch_func::Ebx;
using patch_func::Esi;
using patch_func::Edi;
using patch_func::Stack;

void InitBwFuncs_1161(Common::PatchManager *exec_heap, uintptr_t current_base_address)
{
    uintptr_t diff = current_base_address - starcraft_exe_base;
    bw::ProgressSecondaryOrder_Hidden.Init<Eax>(exec_heap, 0x004EC120 + diff);
    bw::ProgressUnitMovement.Init<Eax>(exec_heap, 0x0046C480 + diff);
    bw::ProgressAcidSporeTimers.Init<Eax>(exec_heap, 0x004F42C0 + diff);
    bw::ProgressEnergyRegen.Init<Esi>(exec_heap, 0x004EB4B0 + diff);
    bw::ProgressSubunitDirection.Init<Esi, Stack>(exec_heap, 0x004EB660 + diff);

    bw::IsOutOfRange.Init<Eax, Stack>(exec_heap, 0x00476430 + diff);
    bw::HasTargetInRange.Init<Edx>(exec_heap, 0x00442460 + diff);
    bw::AttackUnit.Init<Edi, Eax, Stack, Stack>(exec_heap, 0x00476FC0 + diff);
    bw::Ai_StimIfNeeded.Init<Stack>(exec_heap, 0x0043FFD0 + diff);
    bw::IsReadyToAttack.Init<Eax, Stack>(exec_heap, 0x00476640 + diff);

    bw::CreateUnit.Init<Ecx, Eax, Stack, Stack>(exec_heap, 0x004A09D0 + diff);
    bw::InitializeUnitBase.Init<Ecx, Eax, Stack, Stack, Stack>(exec_heap, 0x004A0320 + diff);
    bw::FinishUnit_Pre.Init<Eax>(exec_heap, 0x004A01F0 + diff);
    bw::FinishUnit.Init<Eax>(exec_heap, 0x0049FA40 + diff);
    bw::TransformUnit.Init<Eax, Stack>(exec_heap, 0x0049FED0 + diff);
    bw::PrepareBuildUnit.Init<Edi, Stack>(exec_heap, 0x00467250 + diff);
    bw::GiveUnit.Init<Ecx, Stack, Stack>(exec_heap, 0x0049EFA0 + diff);

    bw::SetHp.Init<Eax, Ecx>(exec_heap, 0x00467340 + diff);
    bw::GetMissChance.Init<Ecx, Eax>(exec_heap, 0x004765B0 + diff);
    bw::GetBaseMissChance.Init<Eax>(exec_heap, 0x00476210 + diff);
    bw::GetCurrentStrength.Init<Ecx, Eax>(exec_heap, 0x00431730 + diff);
    bw::IsMultiSelectable.Init<Ecx>(exec_heap, 0x0047B770 + diff);

    bw::CheckUnitDatRequirements.Init<Esi, Eax, Stack>(exec_heap, 0x0046E1C0 + diff);
    bw::IsHigherRank.Init<Edi, Esi>(exec_heap, 0x0049A350 + diff);
    bw::IsTooClose.Init<Eax, Esi>(exec_heap, 0x004764D0 + diff);
    bw::IsPowered.Init<Eax, Stack, Stack, Stack>(exec_heap, 0x004936B0 + diff);

    bw::PlaySelectionSound.Init<Stack>(exec_heap, 0x0048F910 + diff);
    bw::GetFormationMovementTarget.Init<Stack, Stack>(exec_heap, 0x0049A500 + diff);
    bw::ShowRClickErrorIfNeeded.Init<Stack>(exec_heap, 0x00455A00 + diff);
    bw::NeedsMoreEnergy.Init<Stack, Stack>(exec_heap, 0x00491480 + diff);

    bw::MoveUnit.Init<Edx, Eax, Ecx>(exec_heap, 0x004EBAE0 + diff);
    bw::MoveUnit_Partial.Init<Eax>(exec_heap, 0x0046AD90 + diff);
    bw::HideUnit.Init<Eax>(exec_heap, 0x004E6340 + diff);
    bw::HideUnit_Partial.Init<Edi, Stack>(exec_heap, 0x00493CA0 + diff);
    bw::ShowUnit.Init<Edi>(exec_heap, 0x004E6490 + diff);
    bw::DisableUnit.Init<Eax>(exec_heap, 0x00492CC0 + diff);
    bw::AcidSporeUnit.Init<Ebx>(exec_heap, 0x004F4480 + diff);

    bw::FinishMoveUnit.Init<Eax>(exec_heap, 0x00494160 + diff);
    bw::PlayYesSoundAnim.Init<Eax>(exec_heap, 0x0048F4D0 + diff);
    bw::GetUnloadPosition.Init<Stack, Eax, Esi>(exec_heap, 0x004E76C0 + diff);
    bw::ModifyUnitCounters.Init<Eax, Stack>(exec_heap, 0x00488BF0 + diff);
    bw::ModifyUnitCounters2.Init<Edi, Ecx, Stack>(exec_heap, 0x00488D50 + diff);
    bw::AddToCompletedUnitLbScore.Init<Ecx>(exec_heap, 0x00460860 + diff);

    bw::CanPlaceBuilding.Init<Ecx, Eax, Edx>(exec_heap, 0x0048DBD0 + diff);
    bw::ClearBuildingTileFlag.Init<Eax, Stack, Stack>(exec_heap, 0x00469EC0 + diff);
    bw::RemoveReferences.Init<Edi, Stack>(exec_heap, 0x0049EB70 + diff);
    bw::StopMoving.Init<Eax>(exec_heap, 0x004EB290 + diff);
    bw::RemoveFromMap.Init<Eax>(exec_heap, 0x0046A560 + diff);
    bw::DropPowerup.Init<Stack>(exec_heap, 0x004F3B70 + diff);

    bw::UpdateVisibility.Init<Esi>(exec_heap, 0x004EBE10 + diff);
    bw::UpdateDetectionStatus.Init<Eax>(exec_heap, 0x00443390 + diff);
    bw::RemoveFromCloakedUnits.Init<Ecx>(exec_heap, 0x004916E0 + diff);
    bw::BeginInvisibility.Init<Eax, Ecx>(exec_heap, 0x0049B5B0 + diff);
    bw::EndInvisibility.Init<Eax, Ebx>(exec_heap, 0x0049B440 + diff);

    bw::Unburrow.Init<Eax>(exec_heap, 0x004E97C0 + diff);
    bw::CancelBuildingMorph.Init<Eax>(exec_heap, 0x0045D410 + diff);
    bw::RefundFullCost.Init<Ecx, Eax>(exec_heap, 0x0042CEC0 + diff);
    bw::RefundFourthOfCost.Init<Ecx, Eax>(exec_heap, 0x0042CE70 + diff);
    bw::DeletePowerupImages.Init<Eax>(exec_heap, 0x004F3900 + diff);
    bw::IsPointAtUnitBorder.Init<Stack, Edx, Stack>(exec_heap, 0x0042DA90 + diff);

    bw::SendCommand.Init<Ecx, Edx>(exec_heap, 0x00485BD0 + diff);
    bw::NextCommandedUnit.Init<>(exec_heap, 0x0049A850 + diff);
    bw::IsOutsideGameScreen.Init<Ecx, Eax>(exec_heap, 0x004D1140 + diff);
    bw::MarkControlDirty.Init<Eax>(exec_heap, 0x0041C400 + diff);
    bw::CopyToFrameBuffer.Init<Esi>(exec_heap, 0x0041D3A0 + diff);

    bw::HasToDodge.Init<Edx, Eax>(exec_heap, 0x0042DF70 + diff);
    bw::InsertContour.Init<Edx, Stack, Stack, Stack>(exec_heap, 0x00421910 + diff);
    bw::PrepareFormationMovement.Init<Edi, Stack>(exec_heap, 0x0049A8C0 + diff);
    bw::GetFacingDirection.Init<Edx, Stack, Edx, Eax>(exec_heap, 0x00495300 + diff);
    bw::GetOthersLocation.Init<Stack, Eax>(exec_heap, 0x004F1A20 + diff);
    bw::GetEnemyAirStrength.Init<Eax, Stack>(exec_heap, 0x00431D00 + diff);
    bw::GetEnemyStrength.Init<Eax, Stack, Stack>(exec_heap, 0x004318E0 + diff);
    bw::CanWalkHere.Init<Eax, Edx, Stack>(exec_heap, 0x0042FA00 + diff);
    bw::AreConnected.Init<Eax, Esi, Ebx>(exec_heap, 0x00437E70 + diff);
    bw::MakePath.Init<Eax, Stack>(exec_heap, 0x0042FE00 + diff);
    bw::UpdateMovementState.Init<Eax, Stack>(exec_heap, 0x0046A940 + diff);
    bw::FindCollidingUnit.Init<Ebx>(exec_heap, 0x004F20D0 + diff);
    bw::TerrainCollision.Init<Eax>(exec_heap, 0x004F1980 + diff);
    bw::DoesBlockPoint.Init<Edx, Stack, Stack, Stack>(exec_heap, 0x0042DA00 + diff);

    bw::WriteCompressed.Init<Eax, Stack, Stack>(exec_heap, 0x004C3450 + diff);
    bw::ReadCompressed.Init<Stack, Stack, Stack>(exec_heap, 0x004C3280 + diff);
    bw::SaveDisappearingCreepChunk.Init<Edi>(exec_heap, 0x0047DA80 + diff);
    bw::SaveDatChunk.Init<Esi>(exec_heap, 0x004BF390 + diff);
    bw::SaveTriggerChunk.Init<Stack>(exec_heap, 0x004899D0 + diff);
    bw::WriteReadableSaveHeader.Init<Ebx, Stack>(exec_heap, 0x004CE950 + diff);
    bw::WriteSaveHeader.Init<Stack, Stack>(exec_heap, 0x004CF160 + diff);
    bw::ReplaceWithShortPath.Init<Esi, Edi>(exec_heap, 0x004CE370 + diff);
    bw::ReplaceWithFullPath.Init<Edi, Esi>(exec_heap, 0x004CE300 + diff);
    bw::FirstCommandUser.Init<>(exec_heap, 0x004C3DB0 + diff);
    bw::IsInvalidFilename.Init<Edx, Ecx, Eax>(exec_heap, 0x004F3F20 + diff);
    bw::GetUserFilePath.Init<Stack, Edx, Eax, Ebx>(exec_heap, 0x004CF2A0 + diff);
    bw::ShowWaitDialog.Init<Esi>(exec_heap, 0x004F5C50 + diff);
    bw::HidePopupDialog.Init<>(exec_heap, 0x004F5930 + diff);

    bw::DeleteAiRegions.Init<>(exec_heap, 0x00436A40 + diff);
    bw::AllocateAiRegions.Init<Eax>(exec_heap, 0x00436A80 + diff);
    bw::LoadDisappearingCreepChunk.Init<Esi>(exec_heap, 0x0047D9D0 + diff);
    bw::LoadTriggerChunk.Init<Stack>(exec_heap, 0x004897E0 + diff);
    bw::LoadDatChunk.Init<Esi, Stack>(exec_heap, 0x004BF020 + diff);
    bw::RestorePylons.Init<>(exec_heap, 0x00494030 + diff);

    bw::PlaySound.Init<Ebx, Esi, Stack, Stack>(exec_heap, 0x0048ED50 + diff);
    bw::PlaySoundAtPos.Init<Ebx, Stack, Stack, Stack>(exec_heap, 0x0048EC10 + diff);
    bw::PrintText.Init<Stack, Stack, Eax>(exec_heap, 0x0048D1C0 + diff);
    bw::ShowInfoMessage.Init<Edi, Esi, Ebx>(exec_heap, 0x0048EE30 + diff);
    bw::ShowErrorMessage.Init<Eax, Ecx, Edx>(exec_heap, 0x0048EF30 + diff);
    bw::PrintInfoMessage.Init<Eax>(exec_heap, 0x0048CCB0 + diff);
    bw::PrintInfoMessageForLocalPlayer.Init<Eax, Ecx>(exec_heap, 0x0048CF00 + diff);

    bw::Distance.Init<Ecx, Stack, Edx, Stack>(exec_heap, 0x0040C360 + diff);
    bw::IsPointInArea.Init<Ecx, Stack, Stack, Eax>(exec_heap, 0x00401240 + diff);
    bw::IsInArea.Init<Ecx, Stack, Stack>(exec_heap, 0x00430F10 + diff);
    bw::ProgressTime.Init<Edx>(exec_heap, 0x00489CC0 + diff);
    bw::ProgressTriggerList.Init<Eax>(exec_heap, 0x00489450 + diff);
    bw::ProgressActions.Init<Esi>(exec_heap, 0x00489130 + diff);
    bw::ApplyVictory.Init<>(exec_heap, 0x0048A200 + diff);
    bw::CheckVictoryState.Init<>(exec_heap, 0x0048A410 + diff);
    bw::DeleteTimer.Init<Esi, Edx>(exec_heap, 0x00416000 + diff);
    bw::FindUnitInLocation.Init<Stack, Stack, Eax>(exec_heap, 0x004C7380 + diff);
    bw::PingMinimap.Init<Stack, Stack, Stack>(exec_heap, 0x004A34C0 + diff);
    bw::Trigger_Portrait.Init<Stack, Esi, Edi, Stack>(exec_heap, 0x0045EDD0 + diff);
    bw::GetChkString.Init<Edx>(exec_heap, 0x004BD0C0 + diff);
    bw::GetTextDisplayTime.Init<Eax>(exec_heap, 0x004C51D0 + diff);
    bw::Trigger_DisplayText.Init<Esi, Stack>(exec_heap, 0x0048CF20 + diff);

    bw::CanHitUnit.Init<Edx, Stack, Eax>(exec_heap, 0x00475CE0 + diff);
    bw::ProgressBulletMovement.Init<Eax>(exec_heap, 0x0048B250 + diff);
    bw::ChangeMovePos.Init<Eax, Esi, Ecx>(exec_heap, 0x00495240 + diff);
    bw::UpdateDamageOverlay.Init<Esi>(exec_heap, 0x004E6090 + diff);
    bw::ShowArea.Init<Eax, Ecx, Stack, Stack, Stack>(exec_heap, 0x004806F0 + diff);

    bw::ChangeDirectionToMoveWaypoint.Init<Esi>(exec_heap, 0x00495840 + diff);
    bw::ProgressSpeed.Init<Esi>(exec_heap, 0x00495CB0 + diff);
    bw::UpdateIsMovingFlag.Init<Eax>(exec_heap, 0x00495080 + diff);
    bw::ChangedDirection.Init<Eax>(exec_heap, 0x00495100 + diff);
    bw::ProgressMoveWith.Init<Ecx, Eax, Edx>(exec_heap, 0x00495980 + diff);
    bw::MoveFlingy.Init<Stack>(exec_heap, 0x00496030 + diff);
    bw::SetSpeed.Init<Ecx, Edx>(exec_heap, 0x00494F60 + diff);

    bw::PrepareDrawSprite.Init<Eax>(exec_heap, 0x004983A0 + diff);
    bw::DrawSprite.Init<Eax>(exec_heap, 0x00498C50 + diff);
    bw::GetAreaVisibility.Init<Stack, Stack, Stack, Edx>(exec_heap, 0x00402500 + diff);
    bw::GetAreaExploration.Init<Stack, Stack, Stack, Edx>(exec_heap, 0x00402570 + diff);
    bw::SetVisibility.Init<Ebx, Stack>(exec_heap, 0x00497480 + diff);
    bw::DrawTransmissionSelectionCircle.Init<Esi, Stack>(exec_heap, 0x004EBBD0 + diff);
    bw::DrawOwnMinimapUnits.Init<Eax>(exec_heap, 0x004A48E0 + diff);
    bw::DrawMinimapDot.Init<Ecx, Eax, Stack, Stack, Stack, Stack>(exec_heap, 0x004A3FD0 + diff);
    bw::DrawNeutralMinimapUnits.Init<Ecx>(exec_heap, 0x004A4650 + diff);
    bw::DrawMinimapUnits.Init<Ecx>(exec_heap, 0x004A47B0 + diff);
    bw::MoveSprite.Init<Ecx, Ebx, Edi>(exec_heap, 0x00497A10 + diff);
    bw::FindFowUnit.Init<Ecx, Eax, Stack>(exec_heap, 0x00487FD0 + diff);
    bw::AddOverlayHighest.Init<Eax, Esi, Stack, Stack, Stack>(exec_heap, 0x00498EA0 + diff);
    bw::AddOverlayBelowMain.Init<Eax, Esi, Stack, Stack, Stack>(exec_heap, 0x00498D70 + diff);

    bw::PrepareDrawImage.Init<Eax>(exec_heap, 0x004D57B0 + diff);
    bw::MarkImageAreaForRedraw.Init<Eax>(exec_heap, 0x004970A0 + diff);
    bw::SetImageDirection32.Init<Eax, Stack>(exec_heap, 0x004D5EA0 + diff);
    bw::SetImageDirection256.Init<Esi, Stack>(exec_heap, 0x004D5F80 + diff);

    bw::MatchesHeight.Init<Eax, Esi>(exec_heap, 0x0045F8D0 + diff);
    bw::GetTerrainHeight.Init<Ecx, Eax>(exec_heap, 0x004BD0F0 + diff);
    bw::UpdateCreepDisappearance.Init<Stack, Edx, Ecx, Eax>(exec_heap, 0x0047DE40 + diff);
    bw::Ai_AreOnConnectedRegions.Init<Eax, Stack, Stack>(exec_heap, 0x00438E70 + diff);
    bw::DoesFitHere.Init<Ebx, Eax, Edi>(exec_heap, 0x0042D810 + diff);
    bw::GetFittingPosition.Init<Eax, Stack, Stack, Stack, Stack, Stack>(exec_heap, 0x0049D3E0 + diff);
    bw::ClipPointInBoundsForUnit.Init<Eax, Edx>(exec_heap, 0x00401FA0 + diff);

    bw::MoveScreen.Init<Eax, Ecx>(exec_heap, 0x0049C440 + diff);
    bw::ClearSelection.Init<Eax>(exec_heap, 0x0049A740 + diff);
    bw::HasTeamSelection.Init<Eax>(exec_heap, 0x0049A110 + diff);
    bw::AddToPlayerSelection.Init<Edi, Ebx, Esi>(exec_heap, 0x0049AF80 + diff);
    bw::UpdateSelectionOverlays.Init<Eax, Stack>(exec_heap, 0x0049AE40 + diff);
    bw::MakeDashedSelectionCircle.Init<Eax>(exec_heap, 0x004E65C0 + diff);
    bw::RemoveDashedSelectionCircle.Init<Eax>(exec_heap, 0x00497590 + diff);
    bw::RemoveFromSelection.Init<Eax, Stack>(exec_heap, 0x0049A170 + diff);
    bw::RemoveFromSelections.Init<Edi>(exec_heap, 0x0049A7F0 + diff);
    bw::RemoveFromClientSelection3.Init<Eax>(exec_heap, 0x0049F7A0 + diff);

    bw::CanUseTech.Init<Edi, Eax, Stack>(exec_heap, 0x0046DD80 + diff);
    bw::CanTargetSpell.Init<Stack, Stack, Stack, Stack, Edx, Edi, Esi>(exec_heap, 0x00492020 + diff);
    bw::CanTargetSpellOnUnit.Init<Stack, Eax, Ebx>(exec_heap, 0x00491E80 + diff);
    bw::SpellOrder.Init<Ebx, Stack, Stack, Stack, Eax>(exec_heap, 0x004926D0 + diff);

    bw::ApplyStasis.Init<Edi, Stack>(exec_heap, 0x004F67B0 + diff);
    bw::ApplyEnsnare.Init<Edi>(exec_heap, 0x004F45E0 + diff);
    bw::ApplyMaelstrom.Init<Edi, Stack>(exec_heap, 0x004553F0 + diff);
    bw::UpdateSpeed.Init<Eax>(exec_heap, 0x00454310 + diff);
    bw::EndStasis.Init<Esi>(exec_heap, 0x004F62D0 + diff);
    bw::EndLockdown.Init<Esi>(exec_heap, 0x00454D90 + diff);
    bw::EndMaelstrom.Init<Esi>(exec_heap, 0x00454D20 + diff);

    bw::Hallucinate.Init<Stack, Ecx>(exec_heap, 0x004F6B90 + diff);
    bw::PlaceHallucination.Init<Eax>(exec_heap, 0x004F66D0 + diff);
    bw::CanIssueOrder.Init<Eax, Ebx, Stack>(exec_heap, 0x0046DC20 + diff);
    bw::CanTargetOrder.Init<Ebx, Esi, Eax, Stack>(exec_heap, 0x00474D90 + diff);
    bw::CanTargetOrderOnFowUnit.Init<Ebx, Ecx, Edx, Edi>(exec_heap, 0x004746D0 + diff);
    bw::DoNextQueuedOrderIfAble.Init<Eax>(exec_heap, 0x00475420 + diff);

    bw::PrepareFlee.Init<Stack, Eax>(exec_heap, 0x00476A50 + diff);
    bw::Flee.Init<Eax, Stack>(exec_heap, 0x0043E400 + diff);
    bw::FindNearestUnitOfId.Init<Eax, Stack>(exec_heap, 0x004410C0 + diff);
    bw::ChangeAiRegionState.Init<Esi, Ebx>(exec_heap, 0x004390A0 + diff);
    bw::Ai_ReturnToNearestBaseForced.Init<Esi>(exec_heap, 0x0043DB50 + diff);
    bw::Ai_Detect.Init<Eax, Stack>(exec_heap, 0x0043C580 + diff);
    bw::Ai_CastReactionSpell.Init<Eax, Stack>(exec_heap, 0x004A13C0 + diff);
    bw::TryBeginDeleteTown.Init<Esi>(exec_heap, 0x00434330 + diff);
    bw::FindNearestAvailableMilitary.Init<Stack, Stack, Ebx, Ecx>(exec_heap, 0x004379B0 + diff);
    bw::Ai_GuardRequest.Init<Eax, Ecx>(exec_heap, 0x00448630 + diff);
    bw::Ai_ShouldKeepTarget.Init<Ecx, Eax>(exec_heap, 0x00476930 + diff);
    bw::Ai_RecalculateRegionStrength.Init<Esi>(exec_heap, 0x0043A390 + diff);
    bw::Ai_PopSpendingRequestResourceNeeds.Init<Ecx, Stack>(exec_heap, 0x00447D00 + diff);
    bw::Ai_PopSpendingRequest.Init<Eax>(exec_heap, 0x004476D0 + diff);
    bw::Ai_DoesHaveResourcesForUnit.Init<Stack, Ecx>(exec_heap, 0x004487B0 + diff);
    bw::Ai_TrainUnit.Init<Eax, Stack, Stack, Stack>(exec_heap, 0x004672F0 + diff);
    bw::InheritAi2.Init<Ecx, Eax>(exec_heap, 0x004A2830 + diff);

    bw::Ai_FindNearestActiveMilitaryAi.Init<Stack, Stack, Stack, Stack, Stack>(exec_heap, 0x0043BC40 + diff);
    bw::Ai_FindNearestMilitaryOrSepContAi.Init<Stack, Stack, Stack, Stack, Stack, Stack>(exec_heap, 0x0043BAC0 + diff);
    bw::Ai_PrepareMovingTo.Init<Stack, Stack, Stack>(exec_heap, 0x004E7420 + diff);
    bw::ProgressMilitaryAi.Init<Stack, Stack, Stack, Stack>(exec_heap, 0x0043D5D0 + diff);
    bw::Ai_UpdateSlowestUnitInRegion.Init<Stack>(exec_heap, 0x00436F70 + diff);
    bw::ProgressAiScript.Init<Stack>(exec_heap, 0x0045B850 + diff);
    bw::RemoveFromAiStructs.Init<Stack, Stack>(exec_heap, 0x00439D60 + diff);
    bw::Ai_UpdateRegionStateUnk.Init<Stack, Stack>(exec_heap, 0x0043A010 + diff);
    bw::Ai_UnloadFailure.Init<Stack>(exec_heap, 0x0043DF30 + diff);
    bw::Ai_AttackTo.Init<Stack, Stack, Stack, Stack, Stack>(exec_heap, 0x0043ABB0 + diff);
    bw::Ai_EndAllMovingToAttack.Init<Stack>(exec_heap, 0x00439500 + diff);

    bw::DrawFlashingSelectionCircles.Init<>(exec_heap, 0x004D9460 + diff);
    bw::Replay_RefershUiIfNeeded.Init<>(exec_heap, 0x004DEED0 + diff);
    bw::ProgressTurns.Init<Ecx>(exec_heap, 0x004D9550 + diff);
    bw::Victory.Init<>(exec_heap, 0x00461640 + diff);

    bw::UpdateCreepDisappearance_Unit.Init<Stack>(exec_heap, 0x0047DF90 + diff);
    bw::TryUpdateCreepDisappear.Init<>(exec_heap, 0x0047E410 + diff);
    bw::ProgressCreepDisappearance.Init<>(exec_heap, 0x0049C4C0 + diff);
    bw::UpdateFog.Init<>(exec_heap, 0x00480880 + diff);
    bw::RevealSightArea.Init<Stack>(exec_heap, 0x004E5F30 + diff);

    bw::Ai_ProgressRegions.Init<>(exec_heap, 0x0043FD80 + diff);
    bw::UpdateResourceAreas.Init<>(exec_heap, 0x00445610 + diff);
    bw::Ai_Unk_004A2A40.Init<>(exec_heap, 0x004A2A40 + diff);
    bw::AddSelectionOverlays.Init<>(exec_heap, 0x00499A60 + diff);
    bw::IsCompletelyHidden.Init<Ecx, Edx, Stack, Stack>(exec_heap, 0x00402380 + diff);
    bw::IsCompletelyUnExplored.Init<Ecx, Edx, Stack, Stack>(exec_heap, 0x00402440 + diff);
    bw::Ui_NeedsRedraw_Unk.Init<>(exec_heap, 0x00424A10 + diff);
    bw::GenericStatus_DoesNeedRedraw.Init<>(exec_heap, 0x00424980 + diff);
    bw::IsOwnedByPlayer.Init<Ecx, Edx, Stack>(exec_heap, 0x0045FEF0 + diff);
    bw::CanControlUnit.Init<Stack>(exec_heap, 0x00401170 + diff);
    bw::AddToRecentSelections.Init<>(exec_heap, 0x004967A0 + diff);

    bw::EndTargeting.Init<>(exec_heap, 0x0048CA10 + diff);
    bw::MarkPlacementBoxAreaDirty.Init<>(exec_heap, 0x0048D9A0 + diff);
    bw::EndBuildingPlacement.Init<>(exec_heap, 0x0048E310 + diff);
    bw::AddOverlayNoIscript.Init<Eax, Stack, Stack, Stack, Stack>(exec_heap, 0x00498F40 + diff);
    bw::SetCursorSprite.Init<Stack>(exec_heap, 0x004843F0 + diff);
    bw::RemoveSelectionCircle.Init<Ecx>(exec_heap, 0x004975D0 + diff);
    bw::DoUnitsBlock.Init<Stack, Stack>(exec_heap, 0x0042E1D0 + diff);
    bw::Notify_UnitWasHit.Init<Stack>(exec_heap, 0x0048F230 + diff);

    bw::STransBind.Init<Stack>(exec_heap, 0x00410316 + diff);
    bw::STrans437.Init<Stack, Stack, Stack, Stack>(exec_heap, 0x00411E1E + diff);
    bw::ContainsDirtyArea.Init<Stack, Stack, Stack, Stack>(exec_heap, 0x0041DE20 + diff);
    bw::CopyGameScreenToFramebuf.Init<>(exec_heap, 0x0041D420 + diff);
    bw::ShowLastError.Init<Stack>(exec_heap, 0x0049E530 + diff);

    bw::Command_Sync_Main.Init<Edi>(exec_heap, 0x0047CDD0 + diff);
    bw::Command_Load.Init<Ebx>(exec_heap, 0x004CF950 + diff);
    bw::Command_Restart.Init<>(exec_heap, 0x004BFB10 + diff);
    bw::Command_Pause.Init<>(exec_heap, 0x004C0BC0 + diff);
    bw::Command_Resume.Init<>(exec_heap, 0x004C0B00 + diff);
    bw::Command_Build.Init<Esi>(exec_heap, 0x004C23C0 + diff);
    bw::Command_MinimapPing.Init<Eax>(exec_heap, 0x004BF7A0 + diff);
    bw::Command_Vision.Init<Eax>(exec_heap, 0x004BF9C0 + diff);
    bw::Command_Ally.Init<Stack>(exec_heap, 0x004C28A0 + diff);
    bw::Command_Cheat.Init<Edx>(exec_heap, 0x004C0AD0 + diff);
    bw::Command_Hotkey.Init<Ecx>(exec_heap, 0x004C2870 + diff);
    bw::Command_CancelBuild.Init<>(exec_heap, 0x004C2EF0 + diff);
    bw::Command_CancelMorph.Init<>(exec_heap, 0x004C2EC0 + diff);
    bw::Command_Stop.Init<Edi>(exec_heap, 0x004C2190 + diff);
    bw::Command_CarrierStop.Init<>(exec_heap, 0x004C1430 + diff);
    bw::Command_ReaverStop.Init<>(exec_heap, 0x004C1240 + diff);
    bw::Command_Order_Nothing.Init<>(exec_heap, 0x004C1050 + diff);
    bw::Command_ReturnCargo.Init<Edi>(exec_heap, 0x004C2040 + diff);
    bw::Command_Train.Init<Eax>(exec_heap, 0x004C1C20 + diff);
    bw::Command_CancelTrain.Init<Stack>(exec_heap, 0x004C0100 + diff);
    bw::Command_Tech.Init<Stack>(exec_heap, 0x004C1BA0 + diff);
    bw::Command_CancelTech.Init<>(exec_heap, 0x004C0070 + diff);
    bw::Command_Upgrade.Init<Stack>(exec_heap, 0x004C1B20 + diff);
    bw::Command_CancelUpgrade.Init<>(exec_heap, 0x004BFFC0 + diff);
    bw::Command_Burrow.Init<Stack>(exec_heap, 0x004C1FA0 + diff);
    bw::Command_Unburrow.Init<>(exec_heap, 0x004C1AC0 + diff);
    bw::Command_Cloak.Init<>(exec_heap, 0x004C0720 + diff);
    bw::Command_Decloak.Init<>(exec_heap, 0x004C0660 + diff);
    bw::Command_UnitMorph.Init<Stack>(exec_heap, 0x004C1990 + diff);
    bw::Command_BuildingMorph.Init<Eax>(exec_heap, 0x004C1910 + diff);
    bw::Command_Unsiege.Init<Stack>(exec_heap, 0x004C1F10 + diff);
    bw::Command_Siege.Init<Stack>(exec_heap, 0x004C1E80 + diff);
    bw::Command_MergeArchon.Init<>(exec_heap, 0x004C0E90 + diff);
    bw::Command_MergeDarkArchon.Init<>(exec_heap, 0x004C0CD0 + diff);
    bw::Command_HoldPosition.Init<Edi>(exec_heap, 0x004C20C0 + diff);
    bw::Command_CancelNuke.Init<>(exec_heap, 0x004BFCD0 + diff);
    bw::Command_Lift.Init<Stack>(exec_heap, 0x004C1620 + diff);
    bw::Command_TrainFighter.Init<>(exec_heap, 0x004C1800 + diff);
    bw::Command_CancelAddon.Init<>(exec_heap, 0x004BFF30 + diff);
    bw::Command_Stim.Init<>(exec_heap, 0x004C2F30 + diff);
    bw::Command_Latency.Init<Eax>(exec_heap, 0x00485E60 + diff);
    bw::Command_LeaveGame.Init<Stack>(exec_heap, 0x004C2E90 + diff);
    bw::Command_UnloadAll.Init<Edi>(exec_heap, 0x004C1CC0 + diff);

    bw::ChangeReplaySpeed.Init<Eax, Ecx, Edx>(exec_heap, 0x004DEB90 + diff);
    bw::AddToReplayData.Init<Eax, Stack, Ebx, Edi>(exec_heap, 0x004CDE70 + diff);
    bw::SetLabel.Init<Eax, Stack, Ecx>(exec_heap, 0x004258B0 + diff);

    bw::FreeTriggerList.Init<Ebx>(exec_heap, 0x00402330 + diff);
    bw::Storm_LeaveGame.Init<Stack>(exec_heap, 0x004C3D20 + diff);
    bw::RemoveDialog.Init<Esi>(exec_heap, 0x00419EA0 + diff);
    bw::ResetGameScreenEventHandlers.Init<>(exec_heap, 0x00484CC0 + diff);
    bw::DeleteDirectSound.Init<>(exec_heap, 0x004BBF50 + diff);
    bw::StopSounds.Init<>(exec_heap, 0x004A5F50 + diff);
    bw::InitOrDeleteRaceSounds.Init<Stack>(exec_heap, 0x0048FB40 + diff);
    bw::FreeMapData.Init<>(exec_heap, 0x004BD190 + diff);
    bw::FreeGameDialogs.Init<>(exec_heap, 0x004C3780 + diff);
    bw::FreeEffectsSCodeUnk.Init<>(exec_heap, 0x00416D90 + diff);
    bw::WindowPosUpdate.Init<>(exec_heap, 0x004D2FF0 + diff);
    bw::ReportGameResult.Init<>(exec_heap, 0x004C4790 + diff);
    bw::ClearNetPlayerData.Init<>(exec_heap, 0x004D4AC0 + diff);
    bw::FreeUnkSound.Init<Ebx>(exec_heap, 0x004015F0 + diff);
    bw::Unpause.Init<Stack>(exec_heap, 0x00488790 + diff);

    bw::DeleteHealthBarImage.Init<Esi>(exec_heap, 0x004D5030 + diff);
    bw::DeleteSelectionCircleImage.Init<Esi>(exec_heap, 0x004D4FA0 + diff);
    bw::SetBuildingTileFlag.Init<Eax, Stack, Stack>(exec_heap, 0x00469F60 + diff);
    bw::CheckUnstack.Init<Eax>(exec_heap, 0x0042D9A0 + diff);
    bw::IncrementAirUnitx14eValue.Init<Esi>(exec_heap, 0x00453300 + diff);
    bw::ForceMoveTargetInBounds.Init<Ecx>(exec_heap, 0x0046A740 + diff);
    bw::ProgressRepulse.Init<Eax>(exec_heap, 0x00453420 + diff);
    bw::FinishRepulse.Init<Eax, Stack>(exec_heap, 0x004535A0 + diff);
    bw::FinishUnitMovement.Init<Eax>(exec_heap, 0x0046AD90 + diff);

    bw::IsInFrontOfMovement.Init<Esi, Ecx>(exec_heap, 0x004F17C0 + diff);
    bw::Iscript_StopMoving.Init<Ecx>(exec_heap, 0x0046A6B0 + diff);
    bw::InstantStop.Init<Eax>(exec_heap, 0x0046BF00 + diff);
    bw::SetSpeed_Iscript.Init<Eax, Edx>(exec_heap, 0x004951C0 + diff);
    bw::GetGluAllString.Init<Stack>(exec_heap, 0x004DDD30 + diff);
    bw::ReadStruct245.Init<Edx, Eax>(exec_heap, 0x004DE9D0 + diff);
    bw::PreloadMap.Init<Stack, Stack, Stack>(exec_heap, 0x004BF5D0 + diff);
    bw::AllocateReplayCommands.Init<>(exec_heap, 0x004CE280 + diff);
    bw::LoadReplayCommands.Init<Ebx>(exec_heap, 0x004CE220 + diff);
    bw::GetThreatLevel.Init<Ebx, Edi>(exec_heap, 0x00442160 + diff);
    bw::Ai_FocusUnit.Init<Eax>(exec_heap, 0x0043FF00 + diff);
    bw::Ai_FocusUnit2.Init<Eax>(exec_heap, 0x0043FF90 + diff);

    bw::Order_JunkYardDog.Init<Eax>(exec_heap, 0x0047C210 + diff);
    bw::Order_Medic.Init<Eax>(exec_heap, 0x00463900 + diff);
    bw::Order_Obscured.Init<Eax>(exec_heap, 0x004F6FA0 + diff);
    bw::Order_Spell.Init<Eax>(exec_heap, 0x00492850 + diff);
    bw::Order_WatchTarget.Init<Eax>(exec_heap, 0x0047BAB0 + diff);
    bw::Order_ReaverAttack.Init<Eax>(exec_heap, 0x00465690 + diff);
    bw::Order_Unload.Init<Eax>(exec_heap, 0x004E80D0 + diff);
    bw::Order_TowerGuard.Init<Eax>(exec_heap, 0x00476F50 + diff);
    bw::Order_TowerAttack.Init<Eax>(exec_heap, 0x00479150 + diff);
    bw::Order_InitCreepGrowth.Init<Eax>(exec_heap, 0x004E96D0 + diff);
    bw::Order_StoppingCreepGrowth.Init<Eax>(exec_heap, 0x004E95E0 + diff);
    bw::Order_Stop.Init<Eax>(exec_heap, 0x0047BBA0 + diff);
    bw::Order_StayInRange.Init<Eax>(exec_heap, 0x0047C4F0 + diff);
    bw::Order_Scan.Init<Eax>(exec_heap, 0x00464E40 + diff);
    bw::Order_ScannerSweep.Init<Eax>(exec_heap, 0x00463D30 + diff);
    bw::Order_ReturnResource.Init<Eax>(exec_heap, 0x004690C0 + diff);
    bw::Order_RescuePassive.Init<Eax>(exec_heap, 0x004A1EF0 + diff);
    bw::Order_RightClick.Init<Eax>(exec_heap, 0x004F6EF0 + diff);
    bw::Order_MoveToInfest.Init<Eax>(exec_heap, 0x004E98E0 + diff);
    bw::Order_InfestMine4.Init<Eax>(exec_heap, 0x004EA290 + diff);
    bw::Order_BuildProtoss2.Init<Eax>(exec_heap, 0x004E4F20 + diff);
    bw::Order_PowerupIdle.Init<Eax>(exec_heap, 0x004F3E10 + diff);
    bw::Order_PlaceMine.Init<Eax>(exec_heap, 0x00464FD0 + diff);
    bw::Order_TransportIdle.Init<Eax>(exec_heap, 0x004E7300 + diff);
    bw::Order_Patrol.Init<Eax>(exec_heap, 0x004780F0 + diff);
    bw::Order_NukePaint.Init<Eax>(exec_heap, 0x00463610 + diff);
    bw::Order_Pickup4.Init<Eax>(exec_heap, 0x004E7B70 + diff);
    bw::Order_LiftingOff.Init<Eax>(exec_heap, 0x00463AC0 + diff);
    bw::Order_InitPylon.Init<Eax>(exec_heap, 0x00493F70 + diff);
    bw::Order_Move.Init<Eax>(exec_heap, 0x0047C950 + diff);
    bw::Order_MoveToMinerals.Init<Eax>(exec_heap, 0x00469240 + diff);
    bw::Order_WaitForMinerals.Init<Eax>(exec_heap, 0x00468F60 + diff);
    bw::Order_WaitForGas.Init<Eax>(exec_heap, 0x00469000 + diff);
    bw::Order_HarvestGas.Init<Eax>(exec_heap, 0x00469980 + diff);
    bw::Order_MoveToHarvest.Init<Eax>(exec_heap, 0x00469500 + diff);
    bw::Order_Follow.Init<Eax>(exec_heap, 0x0047C7B0 + diff);
    bw::Order_Trap.Init<Eax>(exec_heap, 0x0047BF80 + diff);
    bw::Order_HideTrap.Init<Eax>(exec_heap, 0x0047C0A0 + diff);
    bw::Order_RevealTrap.Init<Eax>(exec_heap, 0x0047C1B0 + diff);
    bw::Order_HarassMove.Init<Eax>(exec_heap, 0x00478EC0 + diff);
    bw::Order_UnusedPowerup.Init<Eax>(exec_heap, 0x004F3EA0 + diff);
    bw::Order_EnterTransport.Init<Eax>(exec_heap, 0x004E7CF0 + diff);
    bw::Order_EnterNydus.Init<Eax>(exec_heap, 0x004EA3E0 + diff);
    bw::Order_DroneStartBuild.Init<Eax>(exec_heap, 0x0045CF80 + diff);
    bw::Order_DroneLand.Init<Eax>(exec_heap, 0x004E9AA0 + diff);
    bw::Order_EnableDoodad.Init<Eax>(exec_heap, 0x0047BE80 + diff);
    bw::Order_DisableDoodad.Init<Eax>(exec_heap, 0x0047BD60 + diff);
    bw::Order_OpenDoor.Init<Eax>(exec_heap, 0x0047BCD0 + diff);
    bw::Order_CloseDoor.Init<Eax>(exec_heap, 0x0047BC50 + diff);
    bw::Order_Burrow.Init<Eax>(exec_heap, 0x004E9E60 + diff);
    bw::Order_Burrowed.Init<Eax>(exec_heap, 0x004E9860 + diff);
    bw::Order_Unburrow.Init<Eax>(exec_heap, 0x004EA670 + diff);
    bw::Order_CtfCopInit.Init<Eax>(exec_heap, 0x004E4210 + diff);
    bw::Order_ComputerReturn.Init<Eax>(exec_heap, 0x00478490 + diff);
    bw::Order_CarrierIgnore2.Init<Eax>(exec_heap, 0x00466720 + diff);
    bw::Order_CarrierStop.Init<Eax>(exec_heap, 0x00465910 + diff);
    bw::Order_CarrierAttack.Init<Eax>(exec_heap, 0x00465950 + diff);
    bw::Order_BeingInfested.Init<Eax>(exec_heap, 0x004EA4C0 + diff);
    bw::Order_RechargeShieldsBattery.Init<Eax>(exec_heap, 0x00493990 + diff);
    bw::Order_AiAttackMove.Init<Eax>(exec_heap, 0x00478DE0 + diff);
    bw::Order_AttackFixedRange.Init<Eax>(exec_heap, 0x00477D00 + diff);
    bw::Order_LiftOff.Init<Eax>(exec_heap, 0x004649B0 + diff);
    bw::Order_TerranBuildSelf.Init<Eax>(exec_heap, 0x00467760 + diff);
    bw::Order_ZergBuildSelf.Init<Eax>(exec_heap, 0x0045D500 + diff);
    bw::Order_ConstructingBuilding.Init<Eax>(exec_heap, 0x00467A70 + diff);
    bw::Order_Critter.Init<Eax>(exec_heap, 0x0047C3C0 + diff);
    bw::Order_StopHarvest.Init<Eax>(exec_heap, 0x00468ED0 + diff);
    bw::Order_UnitMorph.Init<Eax>(exec_heap, 0x0045DEA0 + diff);
    bw::Order_NukeTrain.Init<Eax>(exec_heap, 0x004E6700 + diff);
    bw::Order_CtfCop2.Init<Eax>(exec_heap, 0x004E3FB0 + diff);
    bw::Order_TankMode.Init<Eax>(exec_heap, 0x00464AE0 + diff);
    bw::Order_TurretAttack.Init<Eax>(exec_heap, 0x00477980 + diff);
    bw::Order_TurretGuard.Init<Eax>(exec_heap, 0x004777F0 + diff);
    bw::Order_ResetCollision1.Init<Eax>(exec_heap, 0x004671B0 + diff);
    bw::Order_ResetCollision2.Init<Eax>(exec_heap, 0x0042E320 + diff);
    bw::Order_Upgrade.Init<Eax>(exec_heap, 0x004546A0 + diff);
    bw::Order_Birth.Init<Eax>(exec_heap, 0x0045DD60 + diff);
    bw::Order_Heal.Init<Eax>(exec_heap, 0x00464180 + diff);
    bw::Order_Tech.Init<Eax>(exec_heap, 0x004548B0 + diff);
    bw::Order_Repair.Init<Eax>(exec_heap, 0x004673D0 + diff);

    bw::Order_BuildNydusExit.Init<Ebx>(exec_heap, 0x0045DC20 + diff);
    bw::Order_NukeTrack.Init<Ebx>(exec_heap, 0x00479480 + diff);
    bw::Order_MedicHoldPosition.Init<Esi>(exec_heap, 0x00464050 + diff);
    bw::Order_Harvest3.Init<Esi>(exec_heap, 0x00468E80 + diff);
    bw::Order_InitArbiter.Init<Esi>(exec_heap, 0x00493A80 + diff);
    bw::Order_CompletingArchonSummon.Init<Esi>(exec_heap, 0x00493B10 + diff);
    bw::Order_Guard.Init<Esi>(exec_heap, 0x00475B90 + diff);
    bw::Order_CtfCopStarted.Init<Esi>(exec_heap, 0x004E41A0 + diff);
    bw::Order_RechargeShieldsUnit.Init<Edi>(exec_heap, 0x00493DD0 + diff);
    bw::Order_Interrupted.Init<Edi>(exec_heap, 0x00493920 + diff);
    bw::Order_HealToIdle.Init<Edi>(exec_heap, 0x00463740 + diff);
    bw::Order_Reaver.Init<Edi>(exec_heap, 0x004665D0 + diff);
    bw::Order_Neutral.Init<Edi>(exec_heap, 0x004A1C20 + diff);
    bw::Order_PickupBunker.Init<Edi>(exec_heap, 0x004E73B0 + diff);
    bw::Order_PickupTransport.Init<Edi>(exec_heap, 0x004E75D0 + diff);
    bw::Order_Carrier.Init<Edi>(exec_heap, 0x004666A0 + diff);
    bw::Order_WarpIn.Init<Edi>(exec_heap, 0x004E4C70 + diff);
    bw::Order_BuildProtoss1.Init<Edi>(exec_heap, 0x004E4D00 + diff);
    bw::Order_AiPatrol.Init<Esi>(exec_heap, 0x004A1D80 + diff);
    bw::Order_AttackMove.Init<Esi>(exec_heap, 0x00479040 + diff);
    bw::Order_BuildTerran.Init<Stack>(exec_heap, 0x00467FD0 + diff);
    bw::Order_HealMove.Init<Stack>(exec_heap, 0x004637B0 + diff);
    bw::Order_ReaverStop.Init<Stack>(exec_heap, 0x004654B0 + diff);
    bw::Order_DefensiveMatrix.Init<Stack>(exec_heap, 0x004550A0 + diff);
    bw::Order_BuildingMorph.Init<Stack>(exec_heap, 0x0045D0D0 + diff);
    bw::Order_PlaceAddon.Init<Stack>(exec_heap, 0x004E6880 + diff);
    bw::Order_BunkerGuard.Init<Eax>(exec_heap, 0x004790A0 + diff);
    bw::Order_BuildAddon.Init<Eax>(exec_heap, 0x004E6790 + diff);
    bw::Order_TrainFighter.Init<Eax>(exec_heap, 0x00466790 + diff);
    bw::Order_ShieldBattery.Init<Edi>(exec_heap, 0x004932D0 + diff);
    bw::Order_SpawningLarva.Init<Esi>(exec_heap, 0x004EA780 + diff);
    bw::Order_SpreadCreep.Init<Eax>(exec_heap, 0x004EA880 + diff);
    bw::Order_Cloak.Init<Eax>(exec_heap, 0x00491790 + diff);
    bw::Order_Decloak.Init<Eax>(exec_heap, 0x004633E0 + diff);
    bw::Order_CloakNearbyUnits.Init<Edi>(exec_heap, 0x00491C20 + diff);

    bw::CheckFiringAngle.Init<Esi, Ecx, Eax, Stack>(exec_heap, 0x00475BE0 + diff);
    bw::IsMovingToMoveWaypoint.Init<Eax>(exec_heap, 0x00402A90 + diff);
    bw::CalculateBaseStrength.Init<Stack, Eax>(exec_heap, 0x00431270 + diff);
    bw::FinetuneBaseStrength.Init<Eax, Ecx>(exec_heap, 0x00431150 + diff);

    bw::CalculateSpeedChange.Init<Edx, Eax>(exec_heap, 0x0047B5F0 + diff);
    bw::SetDirection.Init<Eax, Ebx>(exec_heap, 0x00495F20 + diff);
    bw::ShouldStopOrderedSpell.Init<Edi>(exec_heap, 0x00492140 + diff);
    bw::Iscript_AttackWith.Init<Eax, Stack>(exec_heap, 0x00479D60 + diff);
    bw::Iscript_UseWeapon.Init<Stack, Esi>(exec_heap, 0x00479C30 + diff);
    bw::GetClosestPointOfTarget.Init<Stack, Stack, Stack>(exec_heap, 0x004762C0 + diff);
    bw::SetMoveTargetToNearbyPoint.Init<Eax, Edx>(exec_heap, 0x004011A0 + diff);
    bw::FireWeapon.Init<Eax, Ebx>(exec_heap, 0x00479D40 + diff);
    bw::SetOffsetToParentsSpecialOverlay.Init<Ecx>(exec_heap, 0x004D5A00 + diff);

    bw::GetPsiPlacementState.Init<Stack, Stack, Esi, Eax>(exec_heap, 0x00473920 + diff);
    bw::GetGasBuildingPlacementState
        .Init<Stack, Stack, Stack, Stack>(exec_heap, 0x00473DB0 + diff);
    bw::UpdateNydusPlacementState
        .Init<Eax, Stack, Stack, Stack, Stack, Stack>(exec_heap, 0x00473150 + diff);
    bw::UpdateCreepBuildingPlacementState
        .Init<Eax, Stack, Stack, Stack, Stack, Stack, Stack>(exec_heap, 0x00473010 + diff);
    bw::UpdateBuildingPlacementState_MapTileFlags
        .Init<Stack, Stack, Stack, Stack, Stack, Stack, Eax>(exec_heap, 0x00473A10 + diff);
    bw::UpdateBuildingPlacementState_Units
        .Init<Stack, Stack, Eax, Stack, Stack, Stack, Stack, Stack, Stack, Stack>(exec_heap, 0x00473720 + diff);

    bw::MoveTowards.Init<Ecx, Eax>(exec_heap, 0x004EB900 + diff);
    bw::MoveToCollide.Init<Edi, Eax>(exec_heap, 0x00495400 + diff);
    bw::InheritAi.Init<Ecx, Eax>(exec_heap, 0x00435770 + diff);
    bw::MutateBuilding.Init<Eax, Stack>(exec_heap, 0x0045D890 + diff);
    bw::ReduceBuildResources.Init<Eax>(exec_heap, 0x0042CE40 + diff);
    bw::CheckSupplyForBuilding.Init<Stack, Stack, Stack>(exec_heap, 0x0042CF70 + diff);
    bw::BeginGasBuilding.Init<Stack, Eax>(exec_heap, 0x004678A0 + diff);
    bw::StartZergBuilding.Init<Eax>(exec_heap, 0x0045D2E0 + diff);

    bw::LetNextUnitMine.Init<Ebx>(exec_heap, 0x00468970 + diff);
    bw::BeginHarvest.Init<Eax, Edi>(exec_heap, 0x00468C70 + diff);
    bw::AddResetHarvestCollisionOrder.Init<Eax>(exec_heap, 0x00468AA0 + diff);
    bw::IsFacingMoveTarget.Init<Esi>(exec_heap, 0x00402BE0 + diff);
    bw::FinishedMining.Init<Ecx, Eax>(exec_heap, 0x00468E40 + diff);
    bw::Ai_CanMineExtra.Init<Eax>(exec_heap, 0x00447A70 + diff);
    bw::CreateResourceOverlay.Init<Stack, Edx, Edi, Esi>(exec_heap, 0x004F3AF0 + diff);
    bw::UpdateMineralAmountAnimation.Init<Ecx>(exec_heap, 0x00468830 + diff);
    bw::MergeArchonStats.Init<Eax, Ecx>(exec_heap, 0x00493180 + diff);

    bw::SpiderMine_FindTarget.Init<Esi>(exec_heap, 0x00441270 + diff);
    bw::Burrow_Generic.Init<Eax>(exec_heap, 0x004E9A30 + diff);
    bw::InstantCloak.Init<Edi>(exec_heap, 0x0049B1E0 + diff);
    bw::Unburrow_Generic.Init<Eax>(exec_heap, 0x004E99D0 + diff);

    bw::DetachAddon.Init<Eax>(exec_heap, 0x00464930 + diff);
    bw::CancelTech.Init<Esi>(exec_heap, 0x00453DD0 + diff);
    bw::CancelUpgrade.Init<Esi>(exec_heap, 0x00454220 + diff);
    bw::EndAddonPlacement.Init<>(exec_heap, 0x0048E6E0 + diff);
    bw::ReplaceSprite.Init<Stack, Stack, Eax>(exec_heap, 0x00499BB0 + diff);

    bw::IsGoodLarvaPosition.Init<Ebx, Stack, Stack>(exec_heap, 0x004E93E0 + diff);
    bw::GetDefaultLarvaPosition.Init<Stack, Stack, Stack>(exec_heap, 0x004E94B0 + diff);
    bw::Ai_UnitSpecific.Init<Eax>(exec_heap, 0x004A2450 + diff);
    bw::Ai_WorkerAi.Init<Stack>(exec_heap, 0x00435210 + diff);
    bw::Ai_TryProgressSpendingQueue.Init<Ecx>(exec_heap, 0x004361A0 + diff);
    bw::Ai_Military.Init<Eax>(exec_heap, 0x0043D910 + diff);
    bw::Ai_IsInAttack.Init<Eax, Stack>(exec_heap, 0x00436E70 + diff);
    bw::Ai_FindNearestRepairer.Init<Eax>(exec_heap, 0x00440770 + diff);
    bw::Ai_SiegeTank.Init<Eax>(exec_heap, 0x004A12C0 + diff);
    bw::Ai_Burrower.Init<Eax>(exec_heap, 0x004A1340 + diff);
    bw::Ai_IsMilitaryAtRegionWithoutState0.Init<Eax>(exec_heap, 0x00436AE0 + diff);

    bw::LoadFighter.Init<Ecx, Eax>(exec_heap, 0x00466270 + diff);

    bw::Command_StartGame.Init<Stack>(exec_heap, 0x00472060 + diff);
    bw::DrawDownloadStatuses.Init<>(exec_heap, 0x00450210 + diff);
    bw::Command_NewNetPlayer.Init<Stack, Edx>(exec_heap, 0x004713E0 + diff);
    bw::Command_ChangeGameSlot.Init<Eax, Ecx>(exec_heap, 0x00471460 + diff);
    bw::Command_ChangeRace.Init<Stack, Stack>(exec_heap, 0x00471300 + diff);
    bw::Command_TeamGameTeam.Init<Ecx, Stack>(exec_heap, 0x00471750 + diff);
    bw::Command_UmsTeam.Init<Stack, Stack>(exec_heap, 0x00471670 + diff);
    bw::Command_MeleeTeam.Init<Ebx, Stack>(exec_heap, 0x00471570 + diff);
    bw::Command_SwapPlayers.Init<Ebx, Stack>(exec_heap, 0x00471860 + diff);
    bw::Command_SavedData.Init<Edx, Eax>(exec_heap, 0x00472110 + diff);
    bw::MakeGamePublic.Init<>(exec_heap, 0x004C46E0 + diff);
    bw::Ctrl_LeftUp.Init<Esi>(exec_heap, 0x00418640 + diff);

    bw::RemoveCreepAtUnit.Init<Ecx, Eax, Edx, Stack>(exec_heap, 0x00414560 + diff);
    bw::GiveSprite.Init<Ecx, Edx>(exec_heap, 0x0049E4E0 + diff);
    bw::RedrawGasBuildingPlacement.Init<Eax>(exec_heap, 0x0048E1E0 + diff);
    bw::PickReachableTarget.Init<Esi>(exec_heap, 0x00442FC0 + diff);
    bw::CanSeeTarget.Init<Esi>(exec_heap, 0x004E5DB0 + diff);

    bw::ReadFile_Overlapped.Init<Eax, Edx, Ecx, Edi>(exec_heap, 0x004D2AA0 + diff);
    bw::OpenGrpFile.Init<Edi>(exec_heap, 0x004D2930 + diff);
    bw::FileError.Init<Ecx, Ebx>(exec_heap, 0x004D2880 + diff);
    bw::ErrorMessageBox.Init<Ebx, Stack>(exec_heap, 0x004212C0 + diff);
    bw::ReadMpqFile.Init<Stack, Stack, Stack, Stack, Stack, Ecx, Eax>(exec_heap, 0x004D2D10 + diff);

    bw::GetClickableUnits.Init<Stack, Eax>(exec_heap, 0x00431030 + diff);
    bw::IsClickablePixel.Init<Stack, Edi, Ebx>(exec_heap, 0x00402A00 + diff);

    bw::KickPlayer.Init<Esi, Eax>(exec_heap, 0x00470480 + diff);
    bw::InitPlayerStructs.Init<Esi>(exec_heap, 0x004A8D40 + diff);
    bw::InitNetPlayer.Init<Stack, Stack, Stack, Stack>(exec_heap, 0x00470D10 + diff);
    bw::InitPlayerSlot.Init<Edi>(exec_heap, 0x00470F90 + diff);
    bw::InitNetPlayerInfo.Init<Eax>(exec_heap, 0x00470EF0 + diff);
    bw::CountFreeSlots.Init<>(exec_heap, 0x004A8CD0 + diff);
    bw::SetFreeSlots.Init<Stack>(exec_heap, 0x004C4160 + diff);
    bw::SendInfoRequestCommand.Init<Eax>(exec_heap, 0x00470250 + diff);
    bw::InitMapDownload.Init<Stack, Stack>(exec_heap, 0x00472AB0 + diff);
    bw::GetFirstFreeHumanPlayerId.Init<>(exec_heap, 0x004A99C0 + diff);
    bw::GetFreeSlotFromEmptiestTeam.Init<>(exec_heap, 0x0045ADE0 + diff);
    bw::GetTeamGameTeamSize.Init<>(exec_heap, 0x00485710 + diff);

    bw::InitFlingies.Init<>(exec_heap, 0x00496520 + diff);
    bw::InitAi.Init<>(exec_heap, 0x004A1EA0 + diff);
    bw::InitText.Init<>(exec_heap, 0x0048CE90 + diff);
    bw::InitTerrain.Init<>(exec_heap, 0x004BD6F0 + diff);
    bw::InitSprites.Init<>(exec_heap, 0x00499900 + diff);
    bw::InitImages.Init<>(exec_heap, 0x004D6930 + diff);
    bw::InitColorCycling.Init<>(exec_heap, 0x004CBED0 + diff);
    bw::UpdateColorPaletteIndices.Init<Stack, Ebx>(exec_heap, 0x0041E450 + diff);
    bw::InitTransparency.Init<>(exec_heap, 0x004C99C0 + diff);
    bw::InitScoreSupply.Init<>(exec_heap, 0x00488F90 + diff);
    bw::LoadMiscDat.Init<>(exec_heap, 0x004CCC80 + diff);
    bw::InitSpriteVisionSync.Init<>(exec_heap, 0x00497110 + diff);
    bw::InitScreenPositions.Init<>(exec_heap, 0x004BD3F0 + diff);
    bw::InitTerrainAi.Init<>(exec_heap, 0x004A13B0 + diff);
    bw::CloseBnet.Init<>(exec_heap, 0x004DCC50 + diff);
    bw::BwError.Init<Ecx, Eax, Edx, Stack>(exec_heap, 0x004BB300 + diff);
    bw::LoadChk.Init<>(exec_heap, 0x004BF520 + diff);
    bw::CreateTeamGameStartingUnits.Init<>(exec_heap, 0x0049D8E0 + diff);
    bw::CreateStartingUnits.Init<>(exec_heap, 0x0049DA40 + diff);
    bw::InitUnitSystem.Init<>(exec_heap, 0x0049F380 + diff);
    bw::InitPylonSystem.Init<>(exec_heap, 0x00493360 + diff);

    bw::LoadDat.Init<Eax, Stack>(exec_heap, 0x004D2E80 + diff);
    bw::GetReplayPath.Init<Stack, Eax, Esi>(exec_heap, 0x004DEF80 + diff);
    bw::WriteReplayData.Init<Esi, Edi>(exec_heap, 0x004CE1C0 + diff);
    bw::ReadChk.Init<Stack, Eax>(exec_heap, 0x004CC6E0 + diff);
    bw::GiveAi.Init<Eax>(exec_heap, 0x00463040 + diff);
    bw::Interceptor_Attack.Init<Eax>(exec_heap, 0x00465810 + diff);
    bw::Interceptor_Move.Init<Eax, Ecx, Stack>(exec_heap, 0x00465D30 + diff);

    bw::AttackRecentAttacker.Init<Eax>(exec_heap, 0x004770E0 + diff);
    bw::NeutralizeUnit.Init<Esi>(exec_heap, 0x0047CB90 + diff);
    bw::RemoveCloakDrawfuncs.Init<Eax>(exec_heap, 0x00497E80 + diff);
    bw::FlyingBuilding_SwitchedState.Init<Eax>(exec_heap, 0x0046A5A0 + diff);
    bw::FlyingBuilding_LiftIfStillBlocked.Init<Eax>(exec_heap, 0x00463640 + diff);
    bw::FindClaimableAddon.Init<Esi>(exec_heap, 0x004404A0 + diff);
    bw::AttachAddon.Init<Edi, Eax>(exec_heap, 0x00463D50 + diff);
    bw::ShowLandingError.Init<Eax>(exec_heap, 0x0048F5A0 + diff);

    bw::Ai_IsUnreachable.Init<Esi, Eax>(exec_heap, 0x00476C50 + diff);
    bw::MoveForMeleeRange.Init<Stack, Eax>(exec_heap, 0x00477F10 + diff);
    bw::BeginTrain.Init<Esi, Edi, Stack>(exec_heap, 0x00468200 + diff);
    bw::GetBuildHpGain.Init<Eax>(exec_heap, 0x00402C40 + diff);
    bw::ProgressBuild.Init<Eax, Edx, Stack>(exec_heap, 0x004679A0 + diff);
    bw::RallyUnit.Init<Ecx, Eax>(exec_heap, 0x00466F50 + diff);
    bw::AiScript_StartTown.Init<Stack, Stack, Stack, Eax>(exec_heap, 0x00434220 + diff);
}

namespace bw {
    Func<void(Unit *)> ProgressSecondaryOrder_Hidden;
    Func<void(Unit *)> ProgressUnitMovement;
    Func<void(Unit *)> ProgressAcidSporeTimers;
    Func<void(Unit *)> ProgressEnergyRegen;
    Func<void(Unit *, int)> ProgressSubunitDirection;

    Func<bool(const Unit *, const Unit *)> IsOutOfRange;
    Func<bool(Unit *)> HasTargetInRange;
    Func<void(Unit *, Unit *, int, int)> AttackUnit;
    Func<void(Unit *)> Ai_StimIfNeeded;
    Func<bool(Unit *, int)> IsReadyToAttack;

    Func<Unit *(int, x32, y32, int)> CreateUnit;
    Func<int(Unit *, int, int, int, int)> InitializeUnitBase;
    Func<void(Unit *)> FinishUnit_Pre;
    Func<void(Unit *)> FinishUnit;
    Func<void(Unit *, int)> TransformUnit;
    Func<int(Unit *, int)> PrepareBuildUnit;
    Func<int(Unit *, int, int)> GiveUnit;

    Func<void(Unit *, int)> SetHp;
    Func<int(const Unit *, const Unit *)> GetMissChance;
    Func<int(const Unit *)> GetBaseMissChance;
    Func<int(const Unit *, int)> GetCurrentStrength;
    Func<bool(const Unit *)> IsMultiSelectable;

    Func<int(Unit *, int, int)> CheckUnitDatRequirements;
    Func<bool(const Unit *, const Unit *)> IsHigherRank;
    Func<bool(const Unit *, const Unit *)> IsTooClose;
    Func<bool(int, x32, y32, int)> IsPowered;

    Func<void(Unit *)> PlaySelectionSound;
    Func<void(Unit *, MovementGroup *)> GetFormationMovementTarget;
    Func<int(Unit *)> ShowRClickErrorIfNeeded;
    Func<int(Unit *, int)> NeedsMoreEnergy;

    Func<void(Unit *, x32, y32)> MoveUnit;
    Func<void(Unit *)> MoveUnit_Partial;
    Func<void(Unit *)> HideUnit;
    Func<void(Unit *, int)> HideUnit_Partial;
    Func<void(Unit *)> ShowUnit;
    Func<void(Unit *)> DisableUnit;
    Func<void(Unit *)> AcidSporeUnit;

    Func<void(Unit *)> FinishMoveUnit;
    Func<void(Unit *)> PlayYesSoundAnim;
    Func<bool(uint16_t *, Unit *, Unit *)> GetUnloadPosition;
    Func<void(Unit *, int)> ModifyUnitCounters;
    Func<void(Unit *, int, int)> ModifyUnitCounters2;
    Func<void(Unit *)> AddToCompletedUnitLbScore;

    Func<bool(Unit *, int, int)> CanPlaceBuilding;
    Func<void(Unit *, int, int)> ClearBuildingTileFlag;
    Func<void(Unit *, int)> RemoveReferences;
    Func<void(Unit *)> StopMoving;
    Func<void(Unit *)> RemoveFromMap;
    Func<void(Unit *)> DropPowerup;

    Func<void(Unit *)> UpdateVisibility;
    Func<void(Unit *)> UpdateDetectionStatus;
    Func<void(Unit *)> RemoveFromCloakedUnits;
    Func<void(Unit *, int)> BeginInvisibility;
    Func<void(Unit *, int)> EndInvisibility;

    Func<void(Unit *)> Unburrow;
    Func<void(Unit *)> CancelBuildingMorph;
    Func<void(int, int)> RefundFullCost;
    Func<void(int, int)> RefundFourthOfCost;
    Func<void(Unit *)> DeletePowerupImages;
    Func<bool(Unit *, Unit *, int)> IsPointAtUnitBorder;

    Func<void(const void *, int)> SendCommand;
    Func<Unit *()> NextCommandedUnit;
    Func<bool(int, int)> IsOutsideGameScreen;
    Func<void(Control *)> MarkControlDirty;
    Func<void(Rect32 *)> CopyToFrameBuffer;

    Func<bool(const Unit *, const Unit *)> HasToDodge;
    Func<void(Contour *, PathingData *, Contour **, Contour **)> InsertContour;
    Func<void(MovementGroup *, int)> PrepareFormationMovement;
    Func<int(x32, y32, x32, y32)> GetFacingDirection;
    Func<int(const Unit *, const Unit *)> GetOthersLocation;
    Func<int(int, int)> GetEnemyAirStrength;
    Func<int(int, int, int)> GetEnemyStrength;
    Func<bool(Unit *, int, int)> CanWalkHere;
    Func<bool(int, int, int)> AreConnected;
    Func<int(Unit *, uint32_t)> MakePath;
    Func<bool(Unit *, int)> UpdateMovementState;
    Func<Unit *(Unit *)> FindCollidingUnit;
    Func<int(Unit *)> TerrainCollision;
    Func<bool(const Unit *, int, int, int)> DoesBlockPoint;

    Func<bool(const void *, int, File *)> WriteCompressed;
    Func<bool(void *, int, File *)> ReadCompressed;
    Func<void(File *)> SaveDisappearingCreepChunk;
    Func<void(File *)> SaveDatChunk;
    Func<void(File *)> SaveTriggerChunk;
    Func<void(File *, const char *)> WriteReadableSaveHeader;
    Func<void(uint32_t, File *)> WriteSaveHeader;
    Func<void(char *, int)> ReplaceWithShortPath;
    Func<void(char *, int)> ReplaceWithFullPath;
    Func<uint32_t()> FirstCommandUser;
    Func<bool(const char *, int, int)> IsInvalidFilename;
    Func<bool(const char *, char *, int, int)> GetUserFilePath;
    Func<void(const char *)> ShowWaitDialog;
    Func<void()> HidePopupDialog;

    Func<void()> DeleteAiRegions;
    Func<void(int)> AllocateAiRegions;
    Func<bool(File *)> LoadDisappearingCreepChunk;
    Func<bool(File *)> LoadTriggerChunk;
    Func<bool(File *, int)> LoadDatChunk;
    Func<void()> RestorePylons;

    Func<void(int, Unit *, int, int)> PlaySound;
    Func<void(int, uint32_t, int, int)> PlaySoundAtPos;
    Func<void(const char *, int, int)> PrintText;
    Func<void(int, int, int)> ShowInfoMessage;
    Func<void(const char *, int, Unit *)> ShowErrorMessage;
    Func<void(const char *)> PrintInfoMessage;
    Func<void(const char *, int)> PrintInfoMessageForLocalPlayer;

    Func<int(x32, y32, x32, y32)> Distance;
    Func<bool(const Unit *, int, x32, y32)> IsPointInArea;
    Func<bool(const Unit *, int, const Unit *)> IsInArea;
    Func<void(int)> ProgressTime;
    Func<void(TriggerList *)> ProgressTriggerList;
    Func<void(Trigger *)> ProgressActions;
    Func<void()> ApplyVictory;
    Func<void()> CheckVictoryState;
    Func<void(Control *, int)> DeleteTimer;
    Func<Unit *(int, int, int)> FindUnitInLocation;
    Func<void(int, int, int)> PingMinimap;
    Func<void(int, int, int, int)> Trigger_Portrait;
    Func<const char *(int)> GetChkString;
    Func<uint32_t(const char *)> GetTextDisplayTime;
    Func<void(const char *, int)> Trigger_DisplayText;

    Func<bool(Unit *, Unit *, int)> CanHitUnit;
    Func<void(Bullet *)> ProgressBulletMovement;
    Func<void(void *, int, int)> ChangeMovePos;
    Func<void(Unit *)> UpdateDamageOverlay;
    Func<void(int, int, x32, y32, int)> ShowArea;

    Func<void(void *)> ChangeDirectionToMoveWaypoint;
    Func<void(void *)> ProgressSpeed;
    Func<void(void *)> UpdateIsMovingFlag;
    Func<void(void *)> ChangedDirection;
    Func<void(void *, int, int)> ProgressMoveWith;
    Func<bool(Flingy *)> MoveFlingy;
    Func<void(Flingy *, int)> SetSpeed;

    Func<void(Sprite *)> PrepareDrawSprite;
    Func<void(Sprite *)> DrawSprite;
    Func<uint8_t(int, int, int, int)> GetAreaVisibility;
    Func<uint8_t(int, int, int, int)> GetAreaExploration;
    Func<void(Sprite *, int)> SetVisibility;
    Func<void(Sprite *, int)> DrawTransmissionSelectionCircle;
    Func<void(int)> DrawOwnMinimapUnits;
    Func<void(int, x32, y32, int, int, int)> DrawMinimapDot;
    Func<void(int)> DrawNeutralMinimapUnits;
    Func<void(int)> DrawMinimapUnits;
    Func<void(Sprite *, int, int)> MoveSprite;
    Func<Unit *(x32, y32, int)> FindFowUnit;
    Func<void(Sprite *, int, int, int, int)> AddOverlayHighest;
    Func<void(Sprite *, int, int, int, int)> AddOverlayBelowMain;

    Func<void(Image *)> PrepareDrawImage;
    Func<void(Image *)> MarkImageAreaForRedraw;
    Func<void(Image *, int)> SetImageDirection32;
    Func<void(Image *, int)> SetImageDirection256;

    Func<int(const Unit *, int)> MatchesHeight;
    Func<int(int, int)> GetTerrainHeight;
    Func<void(int, x32, y32, int)> UpdateCreepDisappearance;
    Func<bool(Unit *, int, int)> Ai_AreOnConnectedRegions;
    Func<bool(int, x32, y32)> DoesFitHere;
    Func<bool(Rect16 *, Unit *, uint16_t *, uint16_t *, int, int)> GetFittingPosition;
    Func<int(int, uint16_t *)> ClipPointInBoundsForUnit;

    Func<void(int, int)> MoveScreen;
    Func<void(int)> ClearSelection;
    Func<bool(int)> HasTeamSelection;
    Func<bool(int, int, Unit *)> AddToPlayerSelection;
    Func<bool(Unit * const *, int)> UpdateSelectionOverlays;
    Func<void(Unit *)> MakeDashedSelectionCircle;
    Func<void(Sprite *)> RemoveDashedSelectionCircle;
    Func<int(Unit *, int)> RemoveFromSelection;
    Func<void(Unit *)> RemoveFromSelections;
    Func<void(Unit *)> RemoveFromClientSelection3;

    Func<int(int, Unit *, int)> CanUseTech;
    Func<int(Unit *, x32, y32, int16_t *, int, Unit *, int)> CanTargetSpell;
    Func<int(int, Unit *, int)> CanTargetSpellOnUnit;
    Func<int(Unit *, int, int, uint16_t *, int)> SpellOrder;

    Func<void(Unit *, int)> ApplyStasis;
    Func<void(Unit *)> ApplyEnsnare;
    Func<void(Unit *, int)> ApplyMaelstrom;
    Func<void(Unit *)> UpdateSpeed;
    Func<void(Unit *)> EndStasis;
    Func<void(Unit *)> EndLockdown;
    Func<void(Unit *)> EndMaelstrom;

    Func<Unit*(int, Unit *)> Hallucinate;
    Func<int(Unit *)> PlaceHallucination;
    Func<int(Unit *, int, int)> CanIssueOrder;
    Func<bool(Unit *, Unit *, int, int16_t *)> CanTargetOrder;
    Func<bool(Unit *, int, int, int16_t *)> CanTargetOrderOnFowUnit;
    Func<void(Unit *)> DoNextQueuedOrderIfAble;

    Func<uint32_t(Unit *, const Unit *)> PrepareFlee;
    Func<bool(Unit *, Unit *)> Flee;
    Func<Unit *(Unit *, int)> FindNearestUnitOfId;
    Func<void(Ai::Region *, int)> ChangeAiRegionState;
    Func<bool(Unit *)> Ai_ReturnToNearestBaseForced;
    Func<void(Unit *, Unit *)> Ai_Detect;
    Func<bool(Unit *, int)> Ai_CastReactionSpell;
    Func<bool(Ai::Town *)> TryBeginDeleteTown;
    Func<Unit *(int, x32, y32, int)> FindNearestAvailableMilitary;
    Func<void(Ai::GuardAi *, int)> Ai_GuardRequest;
    Func<bool(Unit *, Unit *)> Ai_ShouldKeepTarget;
    Func<void(Ai::Region *)> Ai_RecalculateRegionStrength;
    Func<void(int, int)> Ai_PopSpendingRequestResourceNeeds;
    Func<void(int)> Ai_PopSpendingRequest;
    Func<bool(int, int)> Ai_DoesHaveResourcesForUnit;
    Func<bool(Unit *, int, int, void *)> Ai_TrainUnit;
    Func<void(Unit *, Unit*)> InheritAi2;

    Func<Ai::UnitAi *(Ai::Region *, int, int, int, Ai::Region *)> Ai_FindNearestActiveMilitaryAi;
    Func<Ai::UnitAi *(Ai::Region *, int, int, int, int, Ai::Region *)> Ai_FindNearestMilitaryOrSepContAi;
    Func<bool(Unit *, int, int)> Ai_PrepareMovingTo;
    Func<void(Unit *, int, int, int)> ProgressMilitaryAi;
    Func<void(Ai::Region *)> Ai_UpdateSlowestUnitInRegion;
    Func<void(Ai::Script *)> ProgressAiScript;
    Func<void(Unit *, int)> RemoveFromAiStructs;
    Func<void(int, Unit *)> Ai_UpdateRegionStateUnk;
    Func<void(Unit *)> Ai_UnloadFailure;
    Func<bool(int, int, int, int, int)> Ai_AttackTo;
    Func<void(int)> Ai_EndAllMovingToAttack;

    Func<void()> DrawFlashingSelectionCircles;
    Func<void()> Replay_RefershUiIfNeeded;
    Func<bool(uint32_t *)> ProgressTurns;
    Func<void()> Victory;

    Func<bool(Unit *)> UpdateCreepDisappearance_Unit;
    Func<void()> TryUpdateCreepDisappear;
    Func<void()> ProgressCreepDisappearance;
    Func<void()> UpdateFog;
    Func<void(Unit *)> RevealSightArea;

    Func<void()> Ai_ProgressRegions;
    Func<void()> UpdateResourceAreas;
    Func<void()> Ai_Unk_004A2A40;
    Func<void()> AddSelectionOverlays;
    Func<int(int, int, int, int)> IsCompletelyHidden;
    Func<int(int, int, int, int)> IsCompletelyUnExplored;
    Func<int()> Ui_NeedsRedraw_Unk;
    Func<int()> GenericStatus_DoesNeedRedraw;
    Func<int(int, int, Unit *)> IsOwnedByPlayer;
    Func<int(Unit *)> CanControlUnit;
    Func<void()> AddToRecentSelections;

    Func<void()> EndTargeting;
    Func<void()> MarkPlacementBoxAreaDirty;
    Func<void()> EndBuildingPlacement;
    Func<Image *(Sprite *, int, int, int, int)> AddOverlayNoIscript;
    Func<void(int)> SetCursorSprite;
    Func<void(Sprite *)> RemoveSelectionCircle;
    Func<int(const Unit *, uint32_t)> DoUnitsBlock;
    Func<void(Unit *)> Notify_UnitWasHit;
    Func<int(void *)> STransBind;
    Func<int(void *, uint8_t *, int, void **)> STrans437;
    Func<int(int, int, int, int)> ContainsDirtyArea;
    Func<void()> CopyGameScreenToFramebuf;
    Func<void(int)> ShowLastError;

    Func<bool(const void *)> Command_Sync_Main;
    Func<void(const void *)> Command_Load;
    Func<void()> Command_Restart;
    Func<void()> Command_Pause;
    Func<void()> Command_Resume;
    Func<void(const void *)> Command_Build;
    Func<void(const void *)> Command_MinimapPing;
    Func<void(const void *)> Command_Vision;
    Func<void(const void *)> Command_Ally;
    Func<void(const void *)> Command_Cheat;
    Func<void(const void *)> Command_Hotkey;
    Func<void()> Command_CancelBuild;
    Func<void()> Command_CancelMorph;
    Func<void(const void *)> Command_Stop;
    Func<void()> Command_CarrierStop;
    Func<void()> Command_ReaverStop;
    Func<void()> Command_Order_Nothing;
    Func<void(const void *)> Command_ReturnCargo;
    Func<void(const void *)> Command_Train;
    Func<void(const void *)> Command_CancelTrain;
    Func<void(const void *)> Command_Tech;
    Func<void()> Command_CancelTech;
    Func<void(const void *)> Command_Upgrade;
    Func<void()> Command_CancelUpgrade;
    Func<void(const void *)> Command_Burrow;
    Func<void()> Command_Unburrow;
    Func<void()> Command_Cloak;
    Func<void()> Command_Decloak;
    Func<void(const void *)> Command_UnitMorph;
    Func<void(const void *)> Command_BuildingMorph;
    Func<void(const void *)> Command_Unsiege;
    Func<void(const void *)> Command_Siege;
    Func<void()> Command_MergeArchon;
    Func<void()> Command_MergeDarkArchon;
    Func<void(const void *)> Command_HoldPosition;
    Func<void()> Command_CancelNuke;
    Func<void(const void *)> Command_Lift;
    Func<void()> Command_TrainFighter;
    Func<void()> Command_CancelAddon;
    Func<void()> Command_Stim;
    Func<void(const void *)> Command_Latency;
    Func<void(const void *)> Command_LeaveGame;
    Func<void(const void *)> Command_UnloadAll;

    Func<void(int, int, int)> ChangeReplaySpeed;
    Func<void(ReplayData *, int, const void *, int)> AddToReplayData;
    Func<void(Control *, const char *, int)> SetLabel;

    Func<void(TriggerList *)> FreeTriggerList;
    Func<void(uint32_t)> Storm_LeaveGame;
    Func<void(Control *)> RemoveDialog;
    Func<void()> ResetGameScreenEventHandlers;
    Func<void()> DeleteDirectSound;
    Func<void()> StopSounds;
    Func<void(int)> InitOrDeleteRaceSounds;
    Func<void()> FreeMapData;
    Func<void()> FreeGameDialogs;
    Func<void()> FreeEffectsSCodeUnk;
    Func<void()> WindowPosUpdate;
    Func<void()> ReportGameResult;
    Func<void()> ClearNetPlayerData;
    Func<void(void *)> FreeUnkSound;
    Func<void(int)> Unpause;

    Func<void(Image *)> DeleteHealthBarImage;
    Func<void(Image *)> DeleteSelectionCircleImage;
    Func<void(Unit *, int, int)> SetBuildingTileFlag;
    Func<void(Unit *)> CheckUnstack;
    Func<void(Unit *)> IncrementAirUnitx14eValue;
    Func<void(Unit *)> ForceMoveTargetInBounds;
    Func<bool(Unit *)> ProgressRepulse;
    Func<void(Unit *, int)> FinishRepulse;
    Func<void(Unit *)> FinishUnitMovement;

    Func<bool(Unit *, Unit *)> IsInFrontOfMovement;
    Func<void(Unit *)> Iscript_StopMoving;
    Func<void(Unit *)> InstantStop;
    Func<void(Unit *, int)> SetSpeed_Iscript;

    Func<const char *(int)> GetGluAllString;
    Func<void(GameData *, Player *)> ReadStruct245;
    Func<bool(const char *, void *, int)> PreloadMap;
    Func<void()> AllocateReplayCommands;
    Func<bool(File *)> LoadReplayCommands;

    Func<int(const Unit *, const Unit *)> GetThreatLevel;
    Func<void(Unit *)> Ai_FocusUnit;
    Func<void(Unit *)> Ai_FocusUnit2;

    Func<void(Unit *)> Order_JunkYardDog;
    Func<void(Unit *)> Order_Medic;
    Func<void(Unit *)> Order_Obscured;
    Func<void(Unit *)> Order_Spell;
    Func<void(Unit *)> Order_WatchTarget;
    Func<void(Unit *)> Order_ReaverAttack;
    Func<void(Unit *)> Order_Unload;
    Func<void(Unit *)> Order_TowerGuard;
    Func<void(Unit *)> Order_TowerAttack;
    Func<void(Unit *)> Order_InitCreepGrowth;
    Func<void(Unit *)> Order_StoppingCreepGrowth;
    Func<void(Unit *)> Order_Stop;
    Func<void(Unit *)> Order_StayInRange;
    Func<void(Unit *)> Order_Scan;
    Func<void(Unit *)> Order_ScannerSweep;
    Func<void(Unit *)> Order_ReturnResource;
    Func<void(Unit *)> Order_RescuePassive;
    Func<void(Unit *)> Order_RightClick;
    Func<void(Unit *)> Order_MoveToInfest;
    Func<void(Unit *)> Order_InfestMine4;
    Func<void(Unit *)> Order_BuildProtoss2;
    Func<void(Unit *)> Order_PowerupIdle;
    Func<void(Unit *)> Order_PlaceMine;
    Func<void(Unit *)> Order_TransportIdle;
    Func<void(Unit *)> Order_Patrol;
    Func<void(Unit *)> Order_NukePaint;
    Func<void(Unit *)> Order_Pickup4;
    Func<void(Unit *)> Order_LiftingOff;
    Func<void(Unit *)> Order_InitPylon;
    Func<void(Unit *)> Order_Move;
    Func<void(Unit *)> Order_MoveToMinerals;
    Func<void(Unit *)> Order_WaitForMinerals;
    Func<void(Unit *)> Order_WaitForGas;
    Func<void(Unit *)> Order_HarvestGas;
    Func<void(Unit *)> Order_MoveToHarvest;
    Func<void(Unit *)> Order_Follow;
    Func<void(Unit *)> Order_Trap;
    Func<void(Unit *)> Order_HideTrap;
    Func<void(Unit *)> Order_RevealTrap;
    Func<void(Unit *)> Order_HarassMove;
    Func<void(Unit *)> Order_UnusedPowerup;
    Func<void(Unit *)> Order_EnterTransport;
    Func<void(Unit *)> Order_EnterNydus;
    Func<void(Unit *)> Order_DroneStartBuild;
    Func<void(Unit *)> Order_DroneLand;
    Func<void(Unit *)> Order_EnableDoodad;
    Func<void(Unit *)> Order_DisableDoodad;
    Func<void(Unit *)> Order_OpenDoor;
    Func<void(Unit *)> Order_CloseDoor;
    Func<void(Unit *)> Order_Burrow;
    Func<void(Unit *)> Order_Burrowed;
    Func<void(Unit *)> Order_Unburrow;
    Func<void(Unit *)> Order_CtfCopInit;
    Func<void(Unit *)> Order_ComputerReturn;
    Func<void(Unit *)> Order_CarrierIgnore2;
    Func<void(Unit *)> Order_CarrierStop;
    Func<void(Unit *)> Order_CarrierAttack;
    Func<void(Unit *)> Order_BeingInfested;
    Func<void(Unit *)> Order_RechargeShieldsBattery;
    Func<void(Unit *)> Order_AiAttackMove;
    Func<void(Unit *)> Order_AttackFixedRange;
    Func<void(Unit *)> Order_LiftOff;
    Func<void(Unit *)> Order_TerranBuildSelf;
    Func<void(Unit *)> Order_ZergBuildSelf;
    Func<void(Unit *)> Order_ConstructingBuilding;
    Func<void(Unit *)> Order_Critter;
    Func<void(Unit *)> Order_StopHarvest;
    Func<void(Unit *)> Order_UnitMorph;
    Func<void(Unit *)> Order_NukeTrain;
    Func<void(Unit *)> Order_CtfCop2;
    Func<void(Unit *)> Order_TankMode;
    Func<void(Unit *)> Order_TurretAttack;
    Func<void(Unit *)> Order_TurretGuard;
    Func<void(Unit *)> Order_ResetCollision1;
    Func<void(Unit *)> Order_ResetCollision2;
    Func<void(Unit *)> Order_Upgrade;
    Func<void(Unit *)> Order_Birth;
    Func<void(Unit *)> Order_Heal;
    Func<void(Unit *)> Order_Tech;
    Func<void(Unit *)> Order_Repair;
    Func<void(Unit *)> Order_BuildNydusExit;
    Func<void(Unit *)> Order_NukeTrack;
    Func<void(Unit *)> Order_MedicHoldPosition;
    Func<void(Unit *)> Order_Harvest3;
    Func<void(Unit *)> Order_InitArbiter;
    Func<void(Unit *)> Order_CompletingArchonSummon;
    Func<void(Unit *)> Order_Guard;
    Func<void(Unit *)> Order_CtfCopStarted;
    Func<void(Unit *)> Order_RechargeShieldsUnit;
    Func<void(Unit *)> Order_Interrupted;
    Func<void(Unit *)> Order_HealToIdle;
    Func<void(Unit *)> Order_Reaver;
    Func<void(Unit *)> Order_Neutral;
    Func<void(Unit *)> Order_PickupBunker;
    Func<void(Unit *)> Order_PickupTransport;
    Func<void(Unit *)> Order_Carrier;
    Func<void(Unit *)> Order_WarpIn;
    Func<void(Unit *)> Order_BuildProtoss1;
    Func<void(Unit *)> Order_AiPatrol;
    Func<void(Unit *)> Order_AttackMove;
    Func<void(Unit *)> Order_BuildTerran;
    Func<void(Unit *)> Order_HealMove;
    Func<void(Unit *)> Order_ReaverStop;
    Func<void(Unit *)> Order_DefensiveMatrix;
    Func<void(Unit *)> Order_BuildingMorph;
    Func<void(Unit *)> Order_PlaceAddon;
    Func<void(Unit *)> Order_BunkerGuard;
    Func<void(Unit *)> Order_BuildAddon;
    Func<void(Unit *)> Order_TrainFighter;
    Func<void(Unit *)> Order_ShieldBattery;
    Func<void(Unit *)> Order_SpawningLarva;
    Func<void(Unit *)> Order_SpreadCreep;
    Func<void(Unit *)> Order_Cloak;
    Func<void(Unit *)> Order_Decloak;
    Func<void(Unit *)> Order_CloakNearbyUnits;

    Func<bool(const Unit *unit, int, int, int)> CheckFiringAngle;
    Func<bool(Unit *)> IsMovingToMoveWaypoint;
    Func<uint32_t(int, int)> CalculateBaseStrength;
    Func<uint32_t(int, int)> FinetuneBaseStrength;
    Func<uint32_t(Unit *, int)> CalculateSpeedChange;
    Func<void(Flingy *, int)> SetDirection;
    Func<bool(Unit *)> ShouldStopOrderedSpell;
    Func<void(Unit *, int)> Iscript_AttackWith;
    Func<void(int, Unit *)> Iscript_UseWeapon;
    Func<void(Unit *, uint32_t *, uint32_t *)> GetClosestPointOfTarget;
    Func<void(int, Flingy *)> SetMoveTargetToNearbyPoint;
    Func<void(Unit *, int)> FireWeapon;
    Func<void(Image *img)> SetOffsetToParentsSpecialOverlay;

    Func<uint32_t(int, int, int, int)> GetPsiPlacementState;
    Func<uint32_t(int, int, int, int)> GetGasBuildingPlacementState;
    Func<uint32_t(int, int, int, int, int, int)> UpdateNydusPlacementState;
    Func<uint32_t(int, int, int, int, int, int, int)> UpdateCreepBuildingPlacementState;
    Func<uint32_t(int, int, int, int, int, int, int)> UpdateBuildingPlacementState_MapTileFlags;
    Func<uint32_t(Unit *, int, int, int, int, int, int, int, int, int)>
        UpdateBuildingPlacementState_Units;

    Func<void(Unit *, Unit *)> MoveTowards;
    Func<void(Unit *, Unit *)> MoveToCollide;
    Func<void(Unit *, Unit *)> InheritAi;
    Func<void(Unit *, int)> MutateBuilding;
    Func<void(int)> ReduceBuildResources;
    Func<int(int, int, int)> CheckSupplyForBuilding;
    Func<Unit *(int, Unit *)> BeginGasBuilding;
    Func<void(Unit *)> StartZergBuilding;

    Func<int(Unit *)> LetNextUnitMine;
    Func<void(Unit *, Unit *)> BeginHarvest;
    Func<void(Unit *)> AddResetHarvestCollisionOrder;
    Func<bool(Flingy *)> IsFacingMoveTarget;
    Func<void(Unit *, Unit *)> FinishedMining;
    Func<bool(Unit *)> Ai_CanMineExtra;
    Func<void(int, int, Unit *, int)> CreateResourceOverlay;
    Func<void(Unit *)> UpdateMineralAmountAnimation;
    Func<void(Unit *, Unit *)> MergeArchonStats;

    Func<Unit *(Unit *)> SpiderMine_FindTarget;
    Func<void(Unit *)> Burrow_Generic;
    Func<void(Unit *)> InstantCloak;
    Func<void(Unit *)> Unburrow_Generic;

    Func<void(Unit *)> DetachAddon;
    Func<void(Unit *)> CancelTech;
    Func<void(Unit *)> CancelUpgrade;
    Func<void()> EndAddonPlacement;
    Func<void(int, int, Sprite *)> ReplaceSprite;

    Func<bool(Unit *, int, int)> IsGoodLarvaPosition;
    Func<void(Unit *, uint32_t *, uint32_t *)> GetDefaultLarvaPosition;
    Func<bool(Unit *)> Ai_UnitSpecific;
    Func<void(Unit *)> Ai_WorkerAi;
    Func<bool(Unit *)> Ai_TryProgressSpendingQueue;
    Func<void(Unit *)> Ai_Military;
    Func<bool(Unit *, int)> Ai_IsInAttack;
    Func<Unit *(Unit *)> Ai_FindNearestRepairer;
    Func<void(Unit *)> Ai_SiegeTank;
    Func<void(Unit *)> Ai_Burrower;
    Func<bool(Unit *)> Ai_IsMilitaryAtRegionWithoutState0;

    Func<void(Unit *, Unit *)> LoadFighter;

    Func<void(int)> Command_StartGame;
    Func<void()> DrawDownloadStatuses;
    Func<void(int, const void *)> Command_NewNetPlayer;
    Func<void(int, const void *)> Command_ChangeGameSlot;
    Func<void(const void *, int)> Command_ChangeRace;
    Func<void(const void *, int)> Command_TeamGameTeam;
    Func<void(const void *, int)> Command_UmsTeam;
    Func<void(const void *, int)> Command_MeleeTeam;
    Func<void(const void *, int)> Command_SwapPlayers;
    Func<void(const void *, int)> Command_SavedData;
    Func<void()> MakeGamePublic;
    Func<void(Control *)> Ctrl_LeftUp;

    Func<void(int, int, int, void *)> RemoveCreepAtUnit;
    Func<void(Unit *, int)> GiveSprite;
    Func<void(Unit *)> RedrawGasBuildingPlacement;
    Func<Unit *(Unit *)> PickReachableTarget;
    Func<bool(Unit *)> CanSeeTarget;

    Func<void(void *, int, void *, void *)> ReadFile_Overlapped;
    Func<void *(char *)> OpenGrpFile;
    Func<void(void *, int)> FileError;
    Func<void(int, const char *)> ErrorMessageBox;
    Func<void *(const char *, int, int, const char *, int, int, uint32_t *)> ReadMpqFile;

    Func<Unit **(int, int)> GetClickableUnits;
    Func<bool(Unit *, int, int)> IsClickablePixel;

    Func<void(int, int)> KickPlayer;
    Func<void(int, int, int, int)> InitNetPlayer;
    Func<void(int)> InitPlayerStructs;
    Func<void(Player *)> InitPlayerSlot;
    Func<void(int)> InitNetPlayerInfo;
    Func<uint32_t()> CountFreeSlots;
    Func<void(int)> SetFreeSlots;
    Func<void(int)> SendInfoRequestCommand;
    Func<void(MapDl *, int)> InitMapDownload;
    Func<int()> GetFirstFreeHumanPlayerId;
    Func<int()> GetFreeSlotFromEmptiestTeam;
    Func<int()> GetTeamGameTeamSize;

    Func<void()> InitFlingies;
    Func<void()> InitAi;
    Func<void()> InitText;
    Func<void()> InitTerrain;
    Func<void()> InitSprites;
    Func<void()> InitImages;
    Func<void()> InitColorCycling;
    Func<void(void *, void *)> UpdateColorPaletteIndices;
    Func<void()> InitTransparency;
    Func<void()> InitScoreSupply;
    Func<void()> LoadMiscDat;
    Func<void()> InitSpriteVisionSync;
    Func<void()> InitScreenPositions;
    Func<void()> InitTerrainAi;
    Func<void()> CloseBnet;
    Func<void(const char *, int, const char *, int)> BwError;
    Func<uint32_t()> LoadChk;
    Func<void()> CreateTeamGameStartingUnits;
    Func<void()> CreateStartingUnits;
    Func<void()> InitUnitSystem;
    Func<void()> InitPylonSystem;

    Func<void(void *, const char *)> LoadDat;
    Func<void(const char *, char *, int)> GetReplayPath;
    Func<bool(ReplayData *, File *)> WriteReplayData;
    Func<void *(uint32_t *, const char *)> ReadChk;
    Func<void(Unit *)> GiveAi;

    Func<bool(Unit *)> Interceptor_Attack;
    Func<void(Unit *, int, uint32_t)> Interceptor_Move;

    Func<bool(Unit *)> AttackRecentAttacker;
    Func<void(Unit *)> NeutralizeUnit;
    Func<void(Sprite *)> RemoveCloakDrawfuncs;

    Func<void(Unit *)> FlyingBuilding_SwitchedState;
    Func<void(Unit *)> FlyingBuilding_LiftIfStillBlocked;
    Func<Unit *(Unit *)> FindClaimableAddon;
    Func<void(Unit *, Unit *)> AttachAddon;
    Func<void(Unit *)> ShowLandingError;

    Func<bool(Unit *, Unit *)> Ai_IsUnreachable;
    Func<void(Unit *, Unit *)> MoveForMeleeRange;
    Func<Unit *(Unit *, int, int)> BeginTrain;
    Func<int(Unit *)> GetBuildHpGain;
    Func<int(Unit *, int, int)> ProgressBuild;
    Func<void(Unit *, Unit *)> RallyUnit;
    Func<void(int, int, int, int)> AiScript_StartTown;
}

void InitStormFuncs_1161(Common::PatchManager *exec_heap, uintptr_t current_base_address)
{
    uintptr_t diff = current_base_address - storm_dll_base;

    storm::SMemAlloc.Init<Stack, Stack, Stack, Stack>(exec_heap, 0x15020ED0 + diff);
    storm::SMemFree.Init<Stack, Stack, Stack, Stack>(exec_heap, 0x150205D0 + diff);
    storm::SFileCloseArchive.Init<Stack>(exec_heap, 0x15014A80 + diff);
    storm::SFileCloseFile.Init<Stack>(exec_heap, 0x150152B0 + diff);
    storm::SNetInitializeProvider.Init<Stack, Stack, Stack, Stack, Stack>(exec_heap, 0x1503E560 + diff);
    storm::SNetGetPlayerName.Init<Stack, Stack, Stack>(exec_heap, 0x150370C0 + diff);
    storm::SFileGetFileSize.Init<Stack, Stack>(exec_heap, 0x15013F50 + diff);
    storm::SBmpDecodeImage
        .Init<Stack, Stack, Stack, Stack, Stack, Stack, Stack, Stack, Stack>(exec_heap, 0x15024D50 + diff);
}

namespace storm {
    Func<void *(uint32_t, const char *, uint32_t, uint32_t)> SMemAlloc;
    Func<void(void *, const char *, uint32_t, uint32_t)> SMemFree;
    Func<void(void *)> SFileCloseArchive;
    Func<void(void *)> SFileCloseFile;
    Func<uint32_t(void *, void *, void *, void *, void *)> SNetInitializeProvider;
    Func<uint32_t(int, char *, int)> SNetGetPlayerName;
    Func<uint32_t(void *, uint32_t *)> SFileGetFileSize;
    Func<uint32_t(int, const void *, int, int, void *, int, uint32_t *, uint32_t *, int)> SBmpDecodeImage;
}
