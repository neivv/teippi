#include "unit.h"

#include <functional>
#include <algorithm>
#include <string.h>

#include "console/assert.h"

#include "ai.h"
#include "offsets.h"
#include "order.h"
#include "selection.h"
#include "targeting.h"
#include "upgrade.h"
#include "bullet.h"
#include "sprite.h"
#include "image.h"
#include "pathing.h"
#include "sound.h"
#include "yms.h"
#include "player.h"
#include "unitsearch.h"
#include "perfclock.h"
#include "log.h"
#include "tech.h"
#include "lofile.h"
#include "flingy.h"
#include "warn.h"
#include "rng.h"
#include "building.h"
#include "text.h"
#include "bunker.h"
#include "scthread.h"
#include "strings.h"
#include "unit_cache.h"
#include "entity.h"

using std::get;
using std::max;
using std::min;

EnemyUnitCache *enemy_unit_cache;

// Unused static var abuse D:
Unit ** const Unit::id_lookup = (Unit **)bw::unit_positions_x.raw_pointer();
uint32_t Unit::next_id = 1;
DummyListHead<Unit, Unit::offset_of_allocated> first_allocated_unit;
DummyListHead<Unit, Unit::offset_of_allocated> first_movementstate_flyer;
vector<Unit *> Unit::temp_flagged;
const int UNIT_ID_LOOKUP_SIZE = 0x2000;

bool late_unit_frames_in_progress = false;

#ifdef SYNC
void *Unit::operator new(size_t size)
{
    auto ret = new uint8_t[size];
    if (SyncTest)
        ScrambleStruct(ret, size);
    return ret;
}
#endif

void UnitIscriptContext::IscriptToIdle()
{
    unit->sprite->IscriptToIdle(this);
}

void UnitIscriptContext::ProgressIscript()
{
    unit->sprite->ProgressFrame(this);
    unit->order_signal |= order_signal;
    if (cloak_state == Iscript::Context::Cloaked)
        unit->flags |= (UnitStatus::InvisibilityDone | UnitStatus::BeginInvisibility);
    else if (cloak_state == Iscript::Context::Decloaked)
        unit->flags &= ~(UnitStatus::InvisibilityDone | UnitStatus::BeginInvisibility);
}

void UnitIscriptContext::SetIscriptAnimation(int anim, bool force)
{
    unit->sprite->SetIscriptAnimation(this, anim, force);
}

Iscript::CmdResult UnitIscriptContext::HandleCommand(Image *img, Iscript::Script *script,
                                                     const Iscript::Command &cmd)
{
    Iscript::CmdResult result = unit->HandleIscriptCommand(this, img, script, cmd);
    if (result == Iscript::CmdResult::NotHandled)
        unit->WarnUnhandledIscriptCommand(cmd, caller);
    return result;
}

void UnitIscriptContext::NewOverlay(Image *img)
{
    if (img->drawfunc == Image::Normal && unit->flags & UnitStatus::Hallucination)
    {
        if (unit->CanLocalPlayerControl() || IsReplay())
            img->SetDrawFunc(Image::Hallucination, nullptr);
    }
    if (unit->IsInvisible())
    {
        if (images_dat_draw_if_cloaked[img->image_id])
        {
            // Note: main_img may be null if this is some death anim overlay
            // Related to comment in Image::SingleDelete
            auto main_img = img->parent->main_image;
            if (img->drawfunc == Image::Normal && main_img != nullptr)
            {
                if (main_img->drawfunc >= Image::Cloaking &&
                        main_img->drawfunc <= Image::DetectedDecloaking)
                {
                    img->SetDrawFunc(main_img->drawfunc, main_img->drawfunc_param);
                }
            }
        }
        else
            img->Hide();
    }
}


Unit::Unit(bool) { }

Unit::Unit()
{
    prev() = nullptr;
    next() = nullptr;
    unused52 = 0;
    hotkey_groups = 0;

    targeting_bullets = nullptr;
    spawned_bullets = nullptr;
    first_loaded = nullptr;
    next_loaded = nullptr;
    kills = 0;
    ground_strength = 0;
    air_strength = 0;

    lookup_id = next_id++;
    while (lookup_id == 0 || FindById(lookup_id) != 0)
        lookup_id = next_id++;

    allocated.Add(first_allocated_unit);
    AddToLookup();

    // Yeah, sc won't init these
    // Current speed is weird.. Guess it's set to 0 when unit dies or else unitis might warp randomly
    // Also move target, InitializeFlingy doesn't do everything if move_target == Point(x, y), so set it to -1
    // target_direction usually is set when ordering move, but not for subunits (todo fix?)
    // invisibility_effects is uninited during first cloak frame
    // Secondary order values are only reset if secondary order is not Nothing
    move_target = Point(0xffff, 0xffff);
    current_speed = 0;
    target_direction = 0;
    invisibility_effects = 0;
    bullet_spread_seed = 0;
    repulseAngle = 0;
    _repulseUnknown = 0;
    driftPosX = 0;
    driftPosY = 0;
    secondary_order = Order::Fatal;
}

void Unit::AddToLookup()
{
    lookup_list_next = id_lookup[lookup_id % UNIT_ID_LOOKUP_SIZE];
    id_lookup[lookup_id % UNIT_ID_LOOKUP_SIZE] = this;
}

Unit *Unit::RawAlloc()
{
    return new Unit(false);
}

void Unit::SingleDelete()
{
    Unit *next = id_lookup[lookup_id % UNIT_ID_LOOKUP_SIZE];
    if (next == this)
    {
        id_lookup[lookup_id % UNIT_ID_LOOKUP_SIZE] = lookup_list_next;
    }
    else
    {
        while (next->lookup_list_next != this)
            next = next->lookup_list_next;
        next->lookup_list_next = lookup_list_next;
    }

    RemoveFromHotkeyGroups(this);

    allocated.Remove();
    delete this;
}

void Unit::DeleteAll()
{
    auto it = first_allocated_unit.begin();
    auto end = first_allocated_unit.end();
    while (it != end)
    {
        Unit *unit = *it;
        ++it;
        if (unit->ai)
            unit->ai->Delete();
        delete unit;
    }
    first_allocated_unit.Reset();
    next_id = 0;
    for (auto i = 0; i < UNIT_ID_LOOKUP_SIZE; i++)
        id_lookup[i] = nullptr;

    // Some ums maps like using extended players which can have
    // issues if these are not cleared - especially with the group 3.
    // Bw works as these will always point to the unit array, which is
    // readable/writable, but here they may point to deallocated memory
    // from previous game.
    for (int i = 0; i < Limits::Selection; i++)
    {
        bw::client_selection_group[i] = nullptr;
        bw::client_selection_group2[i] = nullptr;
        bw::client_selection_group3[i] = nullptr;
    }
    // Setting just all 256 player unit pointers to null might overwrite data that should not be touched?
    // There may be still a few which need to be cleared, selection group 3 starts at player 0x40,
    // but there's some map terrain data and such before that.
    bw::first_player_unit.index_overflowing(0x12) = nullptr;
    bw::first_player_unit.index_overflowing(0x15) = nullptr;
    bw::first_player_unit.index_overflowing(0x19) = nullptr;
    bw::first_player_unit.index_overflowing(0x1a) = nullptr;
    bw::first_player_unit.index_overflowing(0x1b) = nullptr;
    bw::first_player_unit.index_overflowing(0x1c) = nullptr;
    bw::first_player_unit.index_overflowing(0x1d) = nullptr;
    std::fill(bw::validation_replay_path.begin(), bw::validation_replay_path.end(), 0);
}

void Unit::DeletePath()
{
    path = nullptr;
}

void Unit::DeleteMovement()
{
    flags &= ~UnitStatus::InBuilding;
    flags = (((units_dat_flags[unit_id] >> 0xa) ^ flags) & UnitStatus::Reacts) ^ flags; // Uh..
    // if ((~flags & UnitStatus::Reacts) || !(units_dat_flags[unit_id] & UnitFlags::Reacts))
    //    flags &= ~UnitStauts::Reacts

    DeletePath();
    movement_state = 0;
    if ((sprite->elevation >= 12) || !(pathing_flags & 1))
        pathing_flags &= ~1;
}

// Called for all dying, but matters only if in transport
void Unit::TransportedDeath()
{
    if (~flags & UnitStatus::InTransport)
        return;

    if (related != nullptr) // Can happen when irra kills in transport
    {
        MoveUnit(this, sprite->position.x, sprite->position.y); // Weeeell

        if (this == related->first_loaded)
        {
            related->first_loaded = next_loaded;
        }
        else
        {
            for (Unit *unit = related->first_loaded; unit; unit = unit->next_loaded)
            {
                if (unit->next_loaded == this)
                {
                    unit->next_loaded = next_loaded;
                    break;
                }
            }
        }
        related = nullptr;
    }

    flags ^= UnitStatus::InTransport; // Pointless?
    DeleteMovement();
    if (HasSubunit())
        subunit->DeleteMovement();
}

void Unit::TransportDeath(ProgressUnitResults *results)
{
    TransportedDeath();

    // See DamageUnit()
    Unit *unit, *next = first_loaded;
    while (next)
    {
        unit = next;
        next = unit->next_loaded;

        // Are these useless? Sc seems to check these intentionally and not just as part of inlined unit -> id conversion
        if (unit->sprite && unit->order != Order::Die)
        {
            bool kill = true;
            if (units_dat_flags[unit_id] & UnitFlags::Building)
                kill = UnloadUnit(unit) == false;
            if (kill)
                unit->Kill(results);
        }
    }
}

Unit *Unit::FindById(uint32_t id)
{
    Unit *next = id_lookup[id % UNIT_ID_LOOKUP_SIZE];
    while (next && next->lookup_id != id)
        next = next->lookup_list_next;
    return next;
}

void Unit::RemoveOverlayFromSelfOrSubunit(int first_id, int last_id)
{
    for (Image *img : sprite->first_overlay)
    {
        if (img->image_id >= first_id && img->image_id <= last_id)
        {
            img->SingleDelete();
            return;
        }
    }
    if (subunit)
    {
        for (Image *img : subunit->sprite->first_overlay)
        {
            if (img->image_id >= first_id && img->image_id <= last_id)
            {
                img->SingleDelete();
                return;
            }
        }
    }
}

void Unit::RemoveOverlayFromSelf(int first_id, int last_id)
{
    for (Image *img : sprite->first_overlay)
    {
        if (img->image_id >= first_id && img->image_id <= last_id)
        {
            img->SingleDelete();
            return;
        }
    }
}

void Unit::AddSpellOverlay(int small_overlay_id)
{
    AddOverlayHighest(GetTurret()->sprite.get(), small_overlay_id + GetSize(), 0, 0, 0);
}

// Some ai orders use UpdateAttackTarget, which uses unitsearch region cache, so they are progressed later
void Unit::ProgressOrder_Late(ProgressUnitResults *results)
{
    STATIC_PERF_CLOCK(Unit_ProgressOrder_Late);
    switch (order)
    {
#define Case(s) case Order::s : Order_ ## s(this); break
        Case(TurretAttack);
        Case(TurretGuard);
    }
    if (order_wait != OrderWait)
        return;

    switch (order)
    {
        Case(HarassMove);
        Case(AiAttackMove);
        Case(AiPatrol);
        Case(AttackFixedRange);
        Case(AttackMove);
        Case(ReaverAttack);
        Case(Burrowed);
        //Case(Pickup4); // Awesome: Uses uat and messes with unitsearch and can even be secondary order
        // So it uses just Bw's uat
        // Bunker guard also uses Uat and is secondary order
        Case(CarrierIgnore2);
        Case(CarrierStop);
        Case(CarrierAttack);
        Case(TowerGuard);
        Case(TowerAttack);
        Case(TurretGuard);
        Case(TurretAttack);
        case Order::ComputerReturn:
            Order_ComputerReturn();
        break;
        case Order::AiGuard:
            Order_AiGuard();
        break;
        case Order::PlayerGuard:
            Order_PlayerGuard();
        break;
        case Order::AttackUnit:
            Order_AttackUnit(results);
        break;
        case Order::ComputerAi:
            Order_ComputerAi(results);
        break;
        case Order::HoldPosition:
            Order_HoldPosition(results);
        break;
        case Order::Interceptor:
            Order_Interceptor(results);
        break;
#undef Case
    }
}

void Unit::ProgressOrder_Hidden(ProgressUnitResults *results)
{
    switch (order)
    {
        case Order::Die:
            Order_Die(results);
            return;
        case Order::PlayerGuard: case Order::TurretGuard: case Order::TurretAttack: case Order::EnterTransport:
            if (flags & UnitStatus::InBuilding)
                IssueOrderTargetingNothing(this, Order::BunkerGuard);
            else
                IssueOrderTargetingNothing(this, Order::Nothing);
            return;
        case Order::HarvestGas:
            Order_HarvestGas(this);
            return;
        case Order::NukeLaunch:
            Order_NukeLaunch(results);
            return;
        case Order::InfestMine4:
            Order_InfestMine4(this);
            return;
        case Order::ResetCollision1:
            Order_ResetCollision1(this);
            return;
        case Order::ResetCollision2:
            Order_ResetCollision2(this);
            return;
        case Order::UnusedPowerup:
            Order_UnusedPowerup(this);
            return;
        case Order::PowerupIdle:
            Order_PowerupIdle(this);
            return;
    }
    if (order_wait--) // Ya postfix
        return;

    order_wait = OrderWait;
    switch (order)
    {
        case Order::ComputerAi:
            if (flags & UnitStatus::InBuilding)
                Order_BunkerGuard(this);
        break;
        case Order::BunkerGuard:
            Order_BunkerGuard(this);
        break;
        case Order::Pickup4:
            Order_Pickup4(this);
        break;
        case Order::RescuePassive:
            Order_RescuePassive(this);
        break;
    }
}

void Unit::ProgressOrder(ProgressUnitResults *results)
{
    STATIC_PERF_CLOCK(Unit_ProgressOrder);
    switch (order)
    {
        case Order::ProtossBuildSelf:
            Order_ProtossBuildSelf(results);
            return;
        case Order::WarpIn:
            Order_WarpIn(this);
            return;
        case Order::Die:
            Order_Die(results);
            return;
        case Order::NukeTrack:
            Order_NukeTrack();
            return;
    }
    if (IsDisabled())
    {
        Ai_FocusUnit(this);
        return;
    }
    if (~flags & UnitStatus::Reacts && flags & UnitStatus::UnderDweb)
        Ai_FocusUnit(this);
#define Case(s) case Order::s : Order_ ## s(this); break
    switch (order)
    {
        Case(InitArbiter);
        Case(LiftOff);
        Case(BuildTerran);
        Case(BuildProtoss1);
        Case(TerranBuildSelf);
        Case(ZergBuildSelf);
        Case(ConstructingBuilding);
        Case(Critter);
        Case(Harvest3);
        Case(StopHarvest);
        Case(Heal);
        Case(HealMove);
        Case(HealToIdle);
        Case(MedicHoldPosition);
        Case(UnitMorph);
        Case(NukeTrain);
        Case(CtfCop2);
        Case(RechargeShieldsUnit);
        Case(Repair);
        Case(Tech);
        Case(Upgrade);
        Case(TankMode);
        Case(Interrupted);
        Case(CompletingArchonSummon);
        Case(ResetCollision1);
        Case(ResetCollision2);
        Case(Birth);
        case Order::SelfDestructing:
            Kill(results);
        break;
        case Order::DroneBuild:
            Order_DroneMutate(results);
        break;
        case Order::WarpingArchon:
            Order_WarpingArchon(2, 10, Archon, results);
        break;
        case Order::WarpingDarkArchon:
            Order_WarpingArchon(19, 20, DarkArchon, results);
        break;
        case Order::Scarab:
            Order_Scarab(results);
        break;
        case Order::Land:
            Order_Land(results);
        break;
        case Order::SiegeMode:
            Order_SiegeMode(results);
        break;
    }
    if (order_wait--) // Ya postfix
        return;

    order_wait = OrderWait;
    Ai_FocusUnit2(this);
    ai_spell_flags = 0;
    switch (order)
    {
        Case(Medic);
        Case(JunkYardDog);
        Case(RechargeShieldsBattery);
        Case(BeingInfested);
        Case(Burrow);
        Case(Unburrow);
        Case(CtfCopInit);
        Case(CtfCopStarted);
        Case(DefensiveMatrix);
        Case(OpenDoor);
        Case(CloseDoor);
        Case(Trap);
        Case(HideTrap);
        Case(RevealTrap);
        Case(EnableDoodad);
        Case(DisableDoodad);
        Case(DroneLand);
        Case(DroneStartBuild);
        Case(EnterNydus);
        Case(BuildNydusExit);
        Case(EnterTransport);
        Case(PickupTransport);
        Case(PickupBunker);
        Case(TransportIdle);
        Case(Follow);
        Case(Guard);
        Case(WaitForGas);
        Case(HarvestGas);
        Case(MoveToMinerals);
        Case(WaitForMinerals);
        Case(InitCreepGrowth);
        Case(InitPylon);
        Case(LiftingOff);
        Case(Neutral);
        Case(NukePaint);
        Case(Patrol);
        Case(PlaceAddon);
        Case(PlaceMine);
        Case(PowerupIdle);
        Case(BuildProtoss2);
        Case(MoveToInfest);
        Case(InfestMine4);
        Case(RightClick);
        Case(RescuePassive);
        Case(Pickup4);
        Case(Scan);
        Case(StayInRange);
        Case(Stop);
        Case(StoppingCreepGrowth);
        Case(WatchTarget);
        Case(BuildingMorph);
        Case(ReaverStop);
        case Order::Unload:
            Order_Unload();
        break;
        case Order::Recall:
            Order_Recall();
        break;
        case Order::QueenHoldPosition:
        case Order::SuicideHoldPosition:
            if (order_state == 0)
            {
                StopMoving(this);
                unk_move_waypoint = move_target;
                order_state = 1;
            }
            if (order_queue_begin)
                DoNextQueuedOrder();
        break;
        case Order::Nothing:
            if (order_queue_begin)
                DoNextQueuedOrder();
        break;
        case Order::Harvest:
        case Order::MoveToGas:
            Order_MoveToHarvest(this);
        break;
        case Order::ReturnGas:
        case Order::ReturnMinerals:
            Order_ReturnResource(this);
        break;
        case Order::Move:
        case Order::ReaverCarrierMove:
            Order_Move(this);
        break;
        case Order::MoveUnload:
            Order_MoveUnload();
        break;
        case Order::NukeGround:
            Order_NukeGround();
        break;
        case Order::NukeUnit:
            Order_NukeUnit();
        break;
        case Order::YamatoGun:
        case Order::Lockdown:
        case Order::Parasite:
        case Order::DarkSwarm:
        case Order::SpawnBroodlings:
        case Order::EmpShockwave:
        case Order::PsiStorm:
        case Order::Plague:
        case Order::Irradiate:
        case Order::Consume:
        case Order::StasisField:
        case Order::Ensnare:
        case Order::Restoration:
        case Order::DisruptionWeb:
        case Order::OpticalFlare:
        case Order::Maelstrom:
            Order_Spell(this);
        break;
        case Order::AttackObscured:
        case Order::InfestObscured:
        case Order::RepairObscured:
        case Order::CarrierAttackObscured:
        case Order::ReaverAttackObscured:
        case Order::HarvestObscured:
        case Order::YamatoGunObscured:
            Order_Obscured(this);
        break;
        case Order::Feedback:
            Order_Feedback(results);
        break;
        // These have ReleaseFighter
        case Order::Carrier:
        case Order::CarrierFight:
        case Order::CarrierHoldPosition:
            Order_Carrier(this);
        break;
        case Order::Reaver:
        case Order::ReaverFight:
        case Order::ReaverHoldPosition:
            Order_Reaver(this);
        break;
        case Order::Hallucination:
            Order_Hallucination(results);
        break;
        case Order::SapLocation:
            Order_SapLocation(results);
        break;
        case Order::SapUnit:
            Order_SapUnit(results);
        break;
        case Order::MiningMinerals:
            Order_HarvestMinerals(results);
        break;
        case Order::SpiderMine:
            Order_SpiderMine(results);
        break;
        case Order::ScannerSweep:
            Order_ScannerSweep(results);
        break;
        case Order::Larva:
            Order_Larva(results);
        break;
        case Order::NukeLaunch:
            Order_NukeLaunch(results);
        break;
        case Order::InterceptorReturn:
            Order_InterceptorReturn(results);
        break;
        case Order::MindControl:
            Order_MindControl(results);
        break;
    }
#undef Case
}

void Unit::ProgressSecondaryOrder(ProgressUnitResults *results)
{
    if (secondary_order == Order::Hallucinated)
    {
        Order_Hallucinated(results);
        return;
    }
    if (IsDisabled())
        return;
#define Case(s) case Order::s : Order_ ## s(this); break
    switch (secondary_order)
    {
        Case(BuildAddon);
        Case(TrainFighter);
        Case(ShieldBattery);
        Case(SpawningLarva);
        Case(SpreadCreep);
        Case(Cloak);
        Case(Decloak);
        Case(CloakNearbyUnits);
        case Order::Train:
            Order_Train(results);
        break;
    }
#undef Case
}

void Unit::ProgressTimers(ProgressUnitResults *results)
{
    if (order_timer)
        order_timer--;
    if (ground_cooldown)
        ground_cooldown--;
    if (air_cooldown)
        air_cooldown--;
    if (spell_cooldown)
        spell_cooldown--;
    if (HasShields())
    {
        int32_t max_shields = GetMaxShields() * 256;
        if (shields != max_shields)
        {
            shields += 7;
            if (shields > max_shields)
                shields = max_shields;
            sprite->MarkHealthBarDirty();
        }
    }
    if ((unit_id == Zergling || unit_id == DevouringOne) && ground_cooldown == 0)
        order_wait = 0;

    is_being_healed = 0;
    if (flags & UnitStatus::Completed || !sprite->IsHidden())
    {
        if (++master_spell_timer >= 8)
        {
            master_spell_timer = 0;
            ProgressSpellTimers(results);
        }
    }
    if (flags & UnitStatus::Completed)
    {
        if (units_dat_flags[unit_id] & UnitFlags::Regenerate && hitpoints > 0 && hitpoints != units_dat_hitpoints[unit_id])
        {
            SetHp(this, hitpoints + 4);
        }
        ProgressEnergyRegen(this);
        if (move_target_update_timer)
            move_target_update_timer--;
        if (death_timer)
        {
            if (--death_timer == 0)
            {
                Kill(results);
                return;
            }
        }
        if (GetRace() == Race::Terran)
        {
            if (flags & UnitStatus::Building || units_dat_flags[unit_id] & UnitFlags::FlyingBuilding) // Why not just checl units.dat flags building <.<
            {
                if (IsOnBurningHealth())
                    DamageSelf(0x14, results);
            }
        }
    }
}

void Unit::ProgressFrame(ProgressUnitResults *results)
{
    // Sanity check that the helping units search flag is cleared
    Assert(~hotkey_groups & 0x80000000);
    if (~units_dat_flags[unit_id] & UnitFlags::Subunit && !sprite->IsHidden())
    {
        if (player < Limits::Players)
            DrawTransmissionSelectionCircle(sprite.get(), bw::self_alliance_colors[player]);
    }
    ProgressTimers(results);
    //debug_log->Log("Order %x\n", order);
    ProgressOrder(results);
    ProgressSecondaryOrder(results);
    if (HasSubunit())
    {
        *bw::active_iscript_unit = subunit;
        subunit->ProgressFrame(results);
        *bw::active_iscript_unit = this;
    }
}

// This assumes that unit search cache won't be invalidated, so attack orders can cache a lot of stuff
void Unit::ProgressFrame_Late(ProgressUnitResults *results)
{
    // Has to be set if order calls SetIscriptAnimation
    *bw::active_iscript_unit = this;
    ProgressOrder_Late(results);
    if (HasSubunit())
    {
        subunit->ProgressFrame_Late(results);
        *bw::active_iscript_unit = this;
    }
    // ProgressFrame has to be after progressing orders, as iscript may override certain movement values
    // set by order. However, this function is after hidden unit frames. Does it change anything?
    if (sprite != nullptr)
    {
        ProgressIscript("ProgressFrame_Late", results);
    }
}

template <bool flyers>
void Unit::ProgressActiveUnitFrame()
{
    if (!flyers && movement_state == MovementState::Flyer)
    {
        allocated.Change(first_movementstate_flyer);
        return;
    }
    int old_direction = movement_direction;
    if (flyers)
        MovementState_Flyer();
    else
        ProgressUnitMovement(this);
    if (!flyers && movement_state == MovementState::Flyer)
    {
        allocated.Change(first_movementstate_flyer);
        return;
    }

    if (*bw::reveal_unit_area || (flyers && *bw::vision_updated))
        RevealSightArea(this);
    if (HasSubunit() && flags & UnitStatus::Completed)
    {
        int rotation = movement_direction - old_direction;
        if (rotation < 0)
            rotation += 0x100;

        ProgressSubunitDirection(subunit, rotation);
        Point32 lo = LoFile::GetOverlay(sprite->main_image->image_id, Overlay::Special).GetValues(sprite->main_image, 0);
        subunit->exact_position = exact_position;
        subunit->position = Point(exact_position.x >> 8, exact_position.y >> 8);
        MoveSprite(subunit->sprite.get(), subunit->position.x, subunit->position.y);
        Image *subunit_img = subunit->sprite->main_image;
        if (subunit_img->x_off != lo.x || subunit_img->y_off != lo.y)
        {
            subunit_img->x_off = lo.x;
            subunit_img->y_off = lo.y;
            subunit_img->flags |= ImageFlags::Redraw;
        }
        *bw::active_iscript_unit = subunit; // Not changing active iscript flingy?
        if (~flingy_flags & 0x2) // Not moving
        {
            if (subunit->flags & UnitStatus::Unk01000000)
            {
                subunit->flags &= ~UnitStatus::Unk01000000;
                if (flags & UnitStatus::Reacts && ~subunit->flingy_flags & 0x8)
                    subunit->SetIscriptAnimation(Iscript::Animation::Idle, true, "ProgressActiveUnitFrame idle", nullptr);
            }
        }
        else if (~subunit->flags & UnitStatus::Unk01000000)
        {
            subunit->flags |= UnitStatus::Unk01000000;
            if (flags & UnitStatus::Reacts && ~subunit->flingy_flags & 0x8)
                subunit->SetIscriptAnimation(Iscript::Animation::Walking, true, "ProgressActiveUnitFrame walking", nullptr);
        }
        subunit->ProgressActiveUnitFrame<flyers>();
        *bw::active_iscript_unit = this;
    }
}

void Unit::ProgressFrame_Hidden(ProgressUnitResults *results)
{
    if (HasSubunit())
    {
        *bw::active_iscript_unit = subunit;
        subunit->ProgressFrame_Hidden(results);
        *bw::active_iscript_unit = this;
    }
    ProgressUnitMovement(this); // Huh?
    ProgressTimers(results);
    ProgressOrder_Hidden(results);
    ProgressSecondaryOrder_Hidden(this);
    if (sprite != nullptr)
    {
        ProgressIscript("ProgressFrame_Late", results);
    }
}

bool Unit::ProgressFrame_Dying(ProgressUnitResults *results)
{
    if (sprite)
    {
        if (sprite->IsHidden())
        {
            sprite->Remove();
            sprite = nullptr;
        }
        else
        {
            ProgressIscript("ProgressFrame_Late", results);
        }
    }
    if (flags & UnitStatus::HasDisappearingCreep)
    {
        if (*bw::dying_unit_creep_disappearance_count == 0)
        {
            *bw::dying_unit_creep_disappearance_count = 3;
            if (UpdateCreepDisappearance_Unit(this) == false)
                return false;
        }
        else
        {
            (*bw::dying_unit_creep_disappearance_count)--;
            return false;
        }
    }
    if (!sprite)
    {
        if (*bw::last_dying_unit == this)
            *bw::last_dying_unit = list.prev;
        list.Remove(*bw::first_dying_unit);
        SingleDelete();
        return true;
    }
    return false;
}

void Unit::ProgressFrames_Invisible()
{
    bool refresh = false;
    for (Unit *next = *bw::first_invisible_unit; next;)
    {
        Unit *unit = next;
        next = unit->invisible_list.next;
        if (!unit->invisibility_effects)
        {
            unit->invisible_list.Remove(*bw::first_invisible_unit);
            unit->invisible_list.next = nullptr;
            unit->invisible_list.prev = nullptr;
            unit->flags &= ~UnitStatus::FreeInvisibility;
            EndInvisibility(unit, Sound::Decloak);
            refresh = true;
        }
        else
        {
            // Remove free cloak from ghosts which have walked out of arbiter range
            if (~unit->flags & UnitStatus::Burrowed && unit->secondary_order == Order::Cloak)
            {
                if (unit->invisibility_effects == 1 && unit->flags & UnitStatus::FreeInvisibility)
                {
                    unit->flags &= ~UnitStatus::FreeInvisibility;
                    refresh = true;
                }
            }
            if (~unit->flags & UnitStatus::BeginInvisibility)
            {
                BeginInvisibility(unit, Sound::Cloak);
                refresh = true;
            }
        }
    }
    if (refresh)
        RefreshUi();
}

void Unit::UpdatePoweredStates()
{
    if (!*bw::pylon_refresh)
        return;
    for (Unit *unit : *bw::first_active_unit)
    {
        if (~unit->flags & UnitStatus::Building || unit->GetRace() != 2)
            continue;
        if (bw::players[unit->player].type == 3)
            continue;

        if (IsPowered(unit->unit_id, unit->sprite->position.x, unit->sprite->position.y, unit->player))
        {
            if (unit->flags & UnitStatus::Disabled)
            {
                unit->flags &= ~UnitStatus::Disabled;
                if (unit->flags & UnitStatus::Completed)
                {
                    unit->SetIscriptAnimation(Iscript::Animation::Enable, true, "UpdatePoweredStates enable", nullptr);
                    // No, it does not check if there is unit being built
                    if (/*unit->IsBuildingProtossAddon() || */unit->IsUpgrading())
                        unit->SetIscriptAnimation(Iscript::Animation::Working, true, "UpdatePoweredStates working", nullptr);
                }
            }
        }
        else
        {
            unit->IscriptToIdle();
            if (~unit->flags & UnitStatus::Disabled)
            {
                unit->flags |= UnitStatus::Disabled;
                // Incomplete buildings become disabled once finished
                if (unit->flags & UnitStatus::Completed)
                    unit->SetIscriptAnimation(Iscript::Animation::Disable, true, "UpdatePoweredStates disable", nullptr);
            }
        }
    }
    *bw::pylon_refresh = 0;
    RefreshUi();
}

ProgressUnitResults Unit::ProgressFrames()
{
    StaticPerfClock::ClearWithLog("Unit::ProgressFrames");
    PerfClock klokki;
    ProgressUnitResults results;

    *bw::ai_interceptor_target_switch = 0;
    ClearOrderTargetingIfNeeded();
    if (--*bw::order_wait_reassign == 0)
    {
        int new_timer = 0;
        for (Unit *unit : *bw::first_active_unit)
        {
            unit->order_wait = new_timer;
            new_timer = (new_timer + 1) & 0x7;
        }
        *bw::order_wait_reassign = 150;
    }

    if (--*bw::secondary_order_wait_reassign == 0)
    {
        int new_timer = 0;
        for (Unit *unit : *bw::first_active_unit)
        {
            unit->secondary_order_wait = new_timer;
            if (++new_timer >= 30)
                new_timer = 0;
        }
        *bw::secondary_order_wait_reassign = 300;
    }
    UpdateDwebStatuses();
    bool vision_updated = *bw::vision_updated;
    for (Unit *next = *bw::first_dying_unit; next;)
    {
        Unit *unit = next;
        next = unit->list.next;
        *bw::active_iscript_unit = unit;
        bool deleted = unit->ProgressFrame_Dying(&results);
        // If unit has disappearing creep it will not be deleted even if sprite has been
        if (!deleted && unit->sprite)
        {
            if (vision_updated && IsHumanPlayer(unit->player))
                ShowArea(1, bw::visions[unit->player], unit->sprite->position.x, unit->sprite->position.y, unit->IsFlying());

            UpdateVisibility(unit);
        }
    }
    auto pre_time = klokki.GetTime();
    for (Unit *unit : *bw::first_active_unit)
    {
        *bw::active_iscript_unit = unit;
        unit->ProgressActiveUnitFrame<false>(); // This does movement as well
    }
    // Flyer optimization as their movement is slow when there's large stack of them
    for (Unit *unit : first_movementstate_flyer)
    {
        *bw::active_iscript_unit = unit;
        unit->ProgressActiveUnitFrame<true>();
    }
    first_movementstate_flyer.MergeTo(first_allocated_unit);
    unit_search->ChangeUnitPosition_Finish();

    auto movement_time = klokki.GetTime();
    if (vision_updated)
    {
        for (Unit *unit : *bw::first_revealer)
        {
            RevealSightArea(unit);
        }
    }
    for (Unit *unit : *bw::first_active_unit)
    {
        UpdateVisibility(unit);
        if (unit->IsInvisible())
        {
            unit->invisibility_effects = 0;
            if (unit->secondary_order_wait == 0)
            {
                UpdateDetectionStatus(unit);
                unit->secondary_order_wait = 30;
            }
            else
                unit->secondary_order_wait--;
        }
    }

    auto misc_time = klokki.GetTime();
    for (Unit *next = *bw::first_active_unit; next;)
    {
        Unit *unit = next;
        next = unit->list.next;
        *bw::active_iscript_unit = unit;
        unit->ProgressFrame(&results);
    }
    auto active_frames_time = klokki.GetTime();
    for (Unit *next = *bw::first_hidden_unit; next;)
    {
        Unit *unit = next;
        next = unit->list.next;
        if (unit->IsInvisible())
            unit->invisibility_effects = 0;

        *bw::active_iscript_unit = unit;
        unit->ProgressFrame_Hidden(&results);
    }

    // This has been reordered just in case it touches unit search cache
    // Originally used to be where ProgressFrame_Late for revealers is now
    // Shouldn't matter though
    for (Unit *next = *bw::first_revealer; next;)
    {
        Unit *unit = next;
        next = unit->list.next;
        *bw::active_iscript_unit = unit;
        unit->ProgressFrame(&results);
    }
    unit_search->EnableAreaCache();
    late_unit_frames_in_progress = true;
    enemy_unit_cache->Clear();
    for (Unit *next = *bw::first_active_unit; next;)
    {
        Unit *unit = next;
        next = unit->list.next;
        unit->ProgressFrame_Late(&results);
    }

    ProgressFrames_Invisible();
    UpdatePoweredStates();

    *bw::lurker_hits_pos = (*bw::lurker_hits_pos + 1) & 0x1f;
    for (auto pair_ref : bw::lurker_hits[*bw::lurker_hits_pos])
    {
        pair_ref[0] = nullptr;
        pair_ref[1] = nullptr;
    }
    *bw::lurker_hits_used = 0;

    for (Unit *next = *bw::first_revealer; next;)
    {
        Unit *unit = next;
        next = unit->list.next;
        unit->ProgressFrame_Late(&results);
    }
    late_unit_frames_in_progress = false;
    auto post_time = klokki.GetTime();

    *bw::active_iscript_unit = nullptr;

    post_time -= active_frames_time;
    active_frames_time -= misc_time;
    misc_time -= movement_time;
    movement_time -= pre_time;
    perf_log->Log("ProgressUnitFrames: Pre %f ms + Movement %f ms + Misc %f ms + Active main %f ms + post %f ms = about %f ms\n",
                 pre_time, movement_time, misc_time, active_frames_time, post_time, klokki.GetTime());
    perf_log->Indent(2);
    StaticPerfClock::LogCalls();
    perf_log->Indent(-2);
    return results;
}

int Unit::GetRace() const
{
    uint32_t group_flags = units_dat_group_flags[unit_id];
    if (group_flags & 1)
        return Race::Zerg;
    if (group_flags & 4)
        return Race::Protoss;
    if (group_flags & 2)
        return Race::Terran;
    return Race::Neutral;
}

bool Unit::IsDisabled() const
{
    if (flags & UnitStatus::Disabled || mael_timer || lockdown_timer || stasis_timer)
        return true;
    return false;
}

int Unit::GetMaxShields() const
{
    if (!HasShields())
        return 0;
    return units_dat_shields[unit_id];
}

int Unit::GetShields() const
{
    if (!HasShields())
        return 0;
    return shields >> 8;
}

int Unit::GetMaxHealth() const
{
    int health = GetMaxShields() + GetMaxHitPoints();
    if (!health)
        return 1;
    return health;
}

int Unit::GetHealth() const
{
    return GetShields() + (hitpoints >> 8);
}

int Unit::GetMaxEnergy() const
{
    if (units_dat_flags[unit_id] & UnitFlags::Hero)
        return Spell::HeroEnergy;

    switch (unit_id)
    {
        case Ghost:
            return Spell::DefaultEnergy + GetUpgradeLevel(Upgrade::MoebiusReactor, player) * Spell::EnergyBonus;
        case Wraith:
            return Spell::DefaultEnergy + GetUpgradeLevel(Upgrade::ApolloReactor, player) * Spell::EnergyBonus;
        case ScienceVessel:
            return Spell::DefaultEnergy + GetUpgradeLevel(Upgrade::TitanReactor, player) * Spell::EnergyBonus;
        case Battlecruiser:
            return Spell::DefaultEnergy + GetUpgradeLevel(Upgrade::ColossusReactor, player) * Spell::EnergyBonus;
        case Medic:
            return Spell::DefaultEnergy + GetUpgradeLevel(Upgrade::CaduceusReactor, player) * Spell::EnergyBonus;
        case Queen:
            return Spell::DefaultEnergy + GetUpgradeLevel(Upgrade::GameteMeiosis, player) * Spell::EnergyBonus;
        case Defiler:
            return Spell::DefaultEnergy + GetUpgradeLevel(Upgrade::MetasynapticNode, player) * Spell::EnergyBonus;
        case Corsair:
            return Spell::DefaultEnergy + GetUpgradeLevel(Upgrade::ArgusJewel, player) * Spell::EnergyBonus;
        case DarkArchon:
            return Spell::DefaultEnergy + GetUpgradeLevel(Upgrade::ArgusTalisman, player) * Spell::EnergyBonus;
        case HighTemplar:
            return Spell::DefaultEnergy + GetUpgradeLevel(Upgrade::KhaydarinAmulet, player) * Spell::EnergyBonus;
        case Arbiter:
            return Spell::DefaultEnergy + GetUpgradeLevel(Upgrade::KhaydarinCore, player) * Spell::EnergyBonus;
        default:
            return Spell::DefaultEnergy;
    }
}

int Unit::GetArmorUpgrades() const
{
    if (*bw::is_bw && (unit_id == Unit::Ultralisk || unit_id == Unit::Torrasque))
    {
        if ((units_dat_flags[unit_id] & UnitFlags::Hero))
            return GetUpgradeLevel(units_dat_armor_upgrade[unit_id], player) + 2;
        else
            return GetUpgradeLevel(units_dat_armor_upgrade[unit_id], player) + 2 * GetUpgradeLevel(Upgrade::ChitinousPlating, player);
    }
    return GetUpgradeLevel(units_dat_armor_upgrade[unit_id], player);
}

int Unit::GetOriginalPlayer() const
{
    if (player == 11)
        return sprite->player;
    return player;
}

bool Unit::IsEnemy(const Unit *other) const
{
    if (player >= Limits::Players || other->player >= Limits::Players)
        return false;
    return bw::alliances[player][other->GetOriginalPlayer()] == 0;
}

bool Unit::IsOnBurningHealth() const
{
    int max_hp = GetMaxHealth();
    int hp = (hitpoints + 0xff) >> 8;
    return (hp * 100 / max_hp) <= 33;
}

int Unit::GetWeaponRange(bool ground) const
{
    int range;
    if (ground)
        range = weapons_dat_max_range[GetTurret()->GetGroundWeapon()];
    else
        range = weapons_dat_max_range[GetTurret()->GetAirWeapon()];

    if (flags & UnitStatus::InBuilding)
        range += 0x40;
    switch (unit_id)
    {
        case Unit::Marine:
            return range + GetUpgradeLevel(Upgrade::U_238Shells, player) * 0x20;
        case Unit::Hydralisk:
            return range + GetUpgradeLevel(Upgrade::GroovedSpines, player) * 0x20;
        case Unit::Dragoon:
            return range + GetUpgradeLevel(Upgrade::SingularityCharge, player) * 0x40;
        case Unit::FenixDragoon: // o.o
            if (*bw::is_bw)
                return range + 0x40;
            return range;
        case Unit::Goliath:
        case Unit::GoliathTurret:
            if (ground || *bw::is_bw == 0)
                return range;
            else
                return range + GetUpgradeLevel(Upgrade::CharonBooster, player) * 0x60;
        case Unit::AlanSchezar:
        case Unit::AlanTurret:
            if (ground || *bw::is_bw == 0)
                return range;
            else
                return range + 0x60;
        default:
            return range;
    }
}

int Unit::GetSightRange(bool dont_check_blind) const
{
    if (flags & UnitStatus::Building && ~flags & UnitStatus::Completed && !IsMorphingBuilding())
        return 4;

    if (!dont_check_blind && blind)
        return 2;

    switch (unit_id)
    {
        case Ghost:
             if (GetUpgradeLevel(Upgrade::OcularImplants, player) != 0)
                return 11;
        break;
        case Overlord:
             if (GetUpgradeLevel(Upgrade::Antennae, player) != 0)
                return 11;
        break;
        case Observer:
             if (GetUpgradeLevel(Upgrade::SensorArray, player) != 0)
                return 11;
        break;
        case Scout:
             if (GetUpgradeLevel(Upgrade::ApialSensors, player) != 0)
                return 11;
        break;
    }
    return units_dat_sight_range[unit_id];
}

int Unit::GetTargetAcquisitionRange() const
{
    switch (unit_id)
    {
        case Ghost:
        case AlexeiStukov:
        case SamirDuran:
        case SarahKerrigan:
        case InfestedDuran:
            if (IsInvisible() && order == Order::HoldPosition)
                return 0;
            else
                return units_dat_target_acquisition_range[unit_id];
        break;
        case Marine:
            return units_dat_target_acquisition_range[unit_id] + GetUpgradeLevel(Upgrade::U_238Shells, player);
        break;
        case Unit::Hydralisk:
            return units_dat_target_acquisition_range[unit_id] + GetUpgradeLevel(Upgrade::GroovedSpines, player);
        break;
        case Unit::Dragoon:
            return units_dat_target_acquisition_range[unit_id] + GetUpgradeLevel(Upgrade::SingularityCharge, player) * 2;
        break;
        case Unit::FenixDragoon: // o.o
            if (*bw::is_bw)
                return units_dat_target_acquisition_range[unit_id] + 2;
            else
                return units_dat_target_acquisition_range[unit_id];
        break;
        case Unit::Goliath:
        case Unit::GoliathTurret:
            if (*bw::is_bw)
                return units_dat_target_acquisition_range[unit_id] + GetUpgradeLevel(Upgrade::CharonBooster, player) * 3;
            else
                return units_dat_target_acquisition_range[unit_id];
        break;
        case Unit::AlanSchezar:
        case Unit::AlanTurret:
            if (*bw::is_bw)
                return units_dat_target_acquisition_range[unit_id] + 3;
            else
                return units_dat_target_acquisition_range[unit_id];
        break;
        default:
            return units_dat_target_acquisition_range[unit_id];
    }
}

void Unit::IncrementKills()
{
    if (kills != 0xffff)
        kills++;
    if (unit_id == Unit::Interceptor)
    {
        if (interceptor.parent && interceptor.parent->kills != 0xffff)
            interceptor.parent->kills++;
    }
}

bool Unit::CanCollideWith(const Unit *other) const
{
    if (other->flags & UnitStatus::NoEnemyCollide)
    {
        if (IsEnemy(other))
            return false;
    }
    if (unit_id == Larva)
    {
        if (~other->flags & UnitStatus::Building)
            return false;
    }
    else if (other->unit_id == Larva)
        return false;
    if (flags & UnitStatus::Harvesting && ~other->flags & UnitStatus::Building)
    {
        if (other->flags & UnitStatus::Harvesting && this->order != Order::ReturnGas && other->order == Order::WaitForGas)
            return true;
        else
            return false;
    }
    return true;
}

bool Unit::DoesCollideAt(const Point &own_pos, const Unit *other, const Point &other_pos) const
{
    Rect16 &own_collision = units_dat_dimensionbox[unit_id];
    Rect16 &other_collision = units_dat_dimensionbox[other->unit_id];
    if (own_pos.x + own_collision.right < other_pos.x - other_collision.left)
        return false;
    if (own_pos.x - own_collision.left > other_pos.x + other_collision.right)
        return false;
    if (own_pos.y + own_collision.bottom < other_pos.y - other_collision.top)
        return false;
    if (own_pos.y - own_collision.top > other_pos.y + other_collision.bottom)
        return false;
    return true;
}

bool Unit::IsMovingAwayFrom(const Unit *other) const
{
    if (~flingy_flags & 0x2)
        return false;
    switch (GetOthersLocation(this, other))
    {
        case 0:
            return movement_direction > 0x40 && movement_direction < 0xc0;
        break;
        case 1:
            return movement_direction > 0x80;
        break;
        case 2:
            return movement_direction < 0x40 || movement_direction > 0xc0;
        break;
        case 3:
            return movement_direction > 0x0 && movement_direction < 0x80;
        break;
    }
    return false;
}


bool Unit::HasRally() const
{
    switch (unit_id)
    {
        case Unit::CommandCenter:
        case Unit::Barracks:
        case Unit::Factory:
        case Unit::Starport:
        case Unit::InfestedCommandCenter:
        case Unit::Hatchery:
        case Unit::Lair:
        case Unit::Hive:
        case Unit::Nexus:
        case Unit::Gateway:
        case Unit::RoboticsFacility:
        case Unit::Stargate:
            return true;
        default:
            return false;
    }
}

bool Unit::IsClickable(int unit_id)
{
    // It would be cool to just use images.dat clickable, but
    // nuke is set clickable there <.<
    switch (unit_id)
    {
        case Unit::NuclearMissile:
        case Unit::Scarab:
        case Unit::DisruptionWeb:
        case Unit::DarkSwarm:
        case Unit::LeftUpperLevelDoor:
        case Unit::RightUpperLevelDoor:
        case Unit::LeftPitDoor:
        case Unit::RightPitDoor:
            return false;
        default:
            return true;
    }
}

bool Unit::IsCarryingFlag() const
{
    if (IsWorker() && worker.powerup && (worker.powerup->unit_id == Unit::Flag))
        return true;
    for (Unit *unit = first_loaded; unit; unit = unit->next_loaded)
    {
        if (unit->IsCarryingFlag())
            return true;
    }
    return false;
}

bool Unit::DoesAcceptRClickCommands() const
{
    if (IsDisabled())
        return false;
    if ((flags & UnitStatus::Burrowed) && unit_id != Unit::Lurker)
        return false;
    if (unit_id == Unit::SCV && order == Order::ConstructingBuilding)
        return false;
    if (unit_id == Unit::Ghost && order == Order::NukeTrack) // Bw checks also, seems to be inlined IsGhost()
        return false;
    if (unit_id == Unit::Archon && order == Order::CompletingArchonSummon) // No da? o.o
        return false;
    return GetRClickAction() != 0;
}

bool Unit::CanTargetSelf(int order) const
{
    if (flags & UnitStatus::Hallucination)
        return false;
    if (flags & UnitStatus::Building)
        return order == Order::RallyPointUnit || order == Order::RallyPointTile;
    if (order == Order::DarkSwarm && (unit_id == Unit::Defiler || unit_id == Unit::UncleanOne))
        return true;
    if (IsTransport())
        return order == Order::MoveUnload;
    else
        return false;
}

bool Unit::CanUseTargetedOrder(int order) const
{
    if (flags & UnitStatus::Building && (order == Order::RallyPointUnit || order == Order::RallyPointTile ||
                                         order == Order::RechargeShieldsBattery || order == Order::PickupBunker))
    {
        return true;
    }
    return GetRClickAction() != RightClickAction::None; // Weird
}

bool Unit::Reaver_CanAttackUnit(const Unit *enemy) const
{
    if (enemy->IsFlying())
        return false;
    return (*bw::pathing)->regions[GetRegion()].group == (*bw::pathing)->regions[GetRegion()].group;
}

bool Unit::CanBeInfested() const
{
    if (~flags & UnitStatus::Completed || unit_id != CommandCenter)
        return false;
    int maxhp = units_dat_hitpoints[unit_id] / 256;
    if (!maxhp)
        maxhp = 1;
    if (hitpoints / 256 * 100 / maxhp >= 50) // If hp >= maxhp * 0,5
        return false;
    return true;
}

int Unit::GetGroundWeapon() const
{
    if ((unit_id == Unit::Lurker) && !(flags & UnitStatus::Burrowed))
        return Weapon::None;
    return units_dat_ground_weapon[unit_id];
}

int Unit::GetAirWeapon() const
{
    return units_dat_air_weapon[unit_id];
}

int Unit::GetCooldown(int weapon_id) const
{
    int cooldown = weapons_dat_cooldown[weapon_id];
    if (acid_spore_count)
    {
        if (cooldown / 8 < 3)
            cooldown += 3 * acid_spore_count;
        else
            cooldown += (cooldown / 8) * acid_spore_count;
    }
    int bonus = 0;
    if (stim_timer)
        bonus++;
    if (flags & UnitStatus::AttackSpeedUpgrade)
        bonus++;
    if (ensnare_timer)
        bonus--;
    if (bonus > 0)
        cooldown /= 2;
    else if (bonus < 0)
        cooldown += cooldown / 4;

    if (cooldown > 250)
        return 250;
    if (cooldown < 5)
        return 5;
    return cooldown;
}

bool Unit::CanAttackFowUnit(int fow_unit) const
{
    if (fow_unit == Unit::None)
        return false;
    if (units_dat_flags[fow_unit] & UnitFlags::Invincible)
        return false;
    if (unit_id == Unit::Carrier || unit_id == Unit::Gantrithor ||
        unit_id == Unit::Reaver || unit_id == Unit::Warbringer)
    {
        return true;
    }
    if (GetGroundWeapon() == Weapon::None)
    {
        if (!subunit || subunit->GetGroundWeapon() == Weapon::None)
            return false;
    }
    return true;
}

int Unit::GetSize() const
{
    uint32_t flags = units_dat_flags[unit_id];
    if (flags & UnitFlags::MediumOverlays)
        return 1;
    else if (flags & UnitFlags::LargeOverlays)
        return 2;
    else
        return 0;
}

bool Unit::HasSubunit() const
{
    if (!subunit)
        return false;
    if (units_dat_flags[subunit->unit_id] & UnitFlags::Subunit)
        return true;
    return false;
}

Unit *Unit::GetTurret()
{
    if (HasSubunit())
        return subunit;
    return this;
}

const Unit *Unit::GetTurret() const
{
    if (HasSubunit())
        return subunit;
    return this;
}

int Unit::GetRClickAction() const
{
    if (unit_id == Unit::Lurker && flags & UnitStatus::Burrowed)
        return RightClickAction::Attack;
    else if (flags & UnitStatus::Building && HasRally() && units_dat_rclick_action[unit_id] == RightClickAction::None)
        return RightClickAction::Move;
    else
        return units_dat_rclick_action[unit_id];
}

bool Unit::CanRClickGround() const
{
    int action = GetRClickAction();
    return (action != RightClickAction::None) && (action != RightClickAction::Attack);
}

bool Unit::CanMove() const
{
    if (flags & UnitStatus::Building)
        return false;
    if ((unit_id != Lurker || ~flags & UnitStatus::Burrowed) && units_dat_rclick_action[unit_id] == RightClickAction::None)
        return false;
    return GetRClickAction() != RightClickAction::Attack;
}

int Unit::GetUsedSpace() const
{
    int space = 0;
    for (Unit *unit = first_loaded; unit; unit = unit->next_loaded)
        space += units_dat_space_required[unit->unit_id];
    return space;
}

bool Unit::IsTriggerUnitId(int trig_unit_id) const
{
    switch (trig_unit_id)
    {
        case Unit::Trigger_Any:
            return true;
        case Unit::Trigger_Men:
            return units_dat_group_flags[unit_id] & 0x8;
        case Unit::Trigger_Buildings:
            return units_dat_group_flags[unit_id] & 0x10;
        case Unit::Trigger_Factories:
            return units_dat_group_flags[unit_id] & 0x20;
        default:
            return trig_unit_id == unit_id;
    }
}

int Unit::CalculateStrength(bool ground) const
{
    int multiplier = bw::unit_strength[ground ? 1 : 0][unit_id];
    if (unit_id == Unit::Bunker)
        multiplier *= GetUsedSpace();
    if ((~flags & UnitStatus::Hallucination) && (units_dat_flags[unit_id] & UnitFlags::Spellcaster))
        multiplier += energy >> 9;

    return multiplier * GetHealth() / GetMaxHealth();
}

void Unit::UpdateStrength()
{
    if (unit_id == Unit::Larva || unit_id == Unit::Egg || unit_id == Unit::Cocoon || unit_id == Unit::LurkerEgg)
        return;
    if (flags & UnitStatus::Hallucination && GetHealth() < GetMaxHealth()) // ?
    {
        air_strength = 0;
        ground_strength = 0;
    }
    else
    {
        air_strength = CalculateStrength(false);
        ground_strength = CalculateStrength(true);
    }
}

bool Unit::CanLoadUnit(Unit *unit) const
{
    if (~flags & UnitStatus::Completed || IsDisabled() || (unit->flags & UnitStatus::Burrowed) || (player != unit->player))
        return false;

    if (units_dat_flags[unit_id] & UnitFlags::Building)
    {
        if (units_dat_space_required[unit->unit_id] != 1 || unit->GetRace() != 1)
            return false;
    }
    return units_dat_space_provided[unit_id] - GetUsedSpace() >= units_dat_space_required[unit->unit_id];
}

// Unlike bw, new units are always last
// If one would unload unit and load another same-sized in its place, it would replace previous's slot
void Unit::LoadUnit(Unit *unit)
{
    Unit *cmp = first_loaded, *prev = 0;
    int unit_space = units_dat_space_required[unit->unit_id];
    while (cmp && units_dat_space_required[cmp->unit_id] >= unit_space)
    {
        prev = cmp;
        cmp = cmp->next_loaded;
    }

    unit->next_loaded = cmp;
    if (prev)
        prev->next_loaded = unit;
    else
        first_loaded = unit;

    PlaySound(Sound::LoadUnit_Zerg + GetRace(), this, 1, 0);
    if (unit->unit_id == Unit::SCV && unit->ai)
    {
        if (((Ai::WorkerAi *)unit->ai)->town->building_scv == unit)
            ((Ai::WorkerAi *)unit->ai)->town->building_scv = nullptr;
    }
    unit->related = this;
    unit->flags |= UnitStatus::InTransport;
    HideUnit(unit);
    unit->SetIscriptAnimation(Iscript::Animation::Idle, true, "LoadUnit", nullptr);
    RefreshUi();

    if (flags & UnitStatus::Building)
    {
        unit->flags &= ~UnitStatus::Reacts;
        unit->flags |= UnitStatus::InBuilding;
        unit->DeletePath();
        unit->movement_state = 0;
        if ((unit->sprite->elevation >= 12) || !(unit->pathing_flags & 1))
            unit->pathing_flags &= ~1;
        if (unit->HasSubunit())
        {
            Unit *subunit = unit->subunit;
            MoveUnit(subunit, sprite->position.x, sprite->position.y);

            subunit->flags &= ~UnitStatus::Reacts;
            subunit->flags |= UnitStatus::InBuilding;
            subunit->DeletePath();
            if ((subunit->sprite->elevation >= 12) || !(subunit->pathing_flags & 1))
                subunit->pathing_flags &= ~1;

        }
        else
            MoveUnit(unit, sprite->position.x, sprite->position.y);
    }
}

int Unit::GetIdleOrder() const
{
    if (ai)
        return Order::ComputerAi;
    return units_dat_return_to_idle_order[unit_id];
}

void Unit::OrderDone()
{
    if (order_queue_begin)
    {
        order_flags |= 0x1;
        DoNextQueuedOrder();
    }
    else
    {
        IssueOrderTargetingNothing(this, GetIdleOrder());
    }
}

bool Unit::UnloadUnit(Unit *unit)
{
    if (order_timer && !(flags & UnitStatus::Building))
        return false;
    if (IsDisabled())
        return false;

    uint16_t position[2];
    if (!GetUnloadPosition(position, this, unit))
        return false;

    order_timer = 0xf;
    MoveUnit(unit, position[0], position[1]);
    if (unit->subunit)
        MoveUnit(unit->subunit, position[0], position[1]);
    unit->IscriptToIdle();

    Unit *prev = nullptr, *cur = first_loaded;
    while (cur != unit)
    {
        prev = cur;
        cur = cur->next_loaded;
    }
    if (prev)
        prev->next_loaded = unit->next_loaded;
    else
        first_loaded = unit->next_loaded;
    unit->next_loaded = 0;

    PlaySound(Sound::UnloadUnit_Zerg + GetRace(), this, 1, 0);
    ShowUnit(unit);
    unit->flags &= ~(UnitStatus::InTransport | UnitStatus::InBuilding);
    unit->related = nullptr;
    unit->DeleteMovement();
    if (unit->HasSubunit())
        unit->subunit->DeleteMovement();

    IssueOrderTargetingNothing(unit, unit->GetIdleOrder());

    RefreshUi();
    if (~flags & UnitStatus::Building)
    {
        if (unit->unit_id == Unit::Reaver)
        {
            unit->order_timer = 0x1e;
        }
        else
        {
            int weapon = unit->GetGroundWeapon();
            if (weapon != Weapon::None)
                unit->ground_cooldown = unit->GetCooldown(weapon);
            weapon = unit->GetAirWeapon();
            if (weapon != Weapon::None)
                unit->air_cooldown = unit->GetCooldown(weapon);
        }
    }
    return true;
}

void Unit::SetButtons(int buttonset)
{
    if (IsDisabled() && !(units_dat_flags[unit_id] & UnitFlags::Building) && (buttonset != 0xe4))
        return;
    buttons = buttonset;
    RefreshUi();
}

void Unit::DeleteOrder(Order *order)
{
    if (orders_dat_highlight[order->order_id] != 0xffff)
        highlighted_order_count--;
    if (highlighted_order_count == 0xff) // Pointless
        highlighted_order_count = 0;

    if (order->list.next)
        order->list.next->list.prev = order->list.prev;
    else
        order_queue_end = order->list.prev;
    if (order->list.prev)
        order->list.prev->list.next = order->list.next;
    else
        order_queue_begin = order->list.next;

    order->SingleDelete();
}

void Unit::Order_Unload()
{
    bool building = flags & UnitStatus::Building;
    if (order_timer && !building)
        return;

    bool success = false;
    if (building)
    {
        Unit *unit = first_loaded, *next;
        while (unit)
        {
            next = unit->next_loaded;
            success = UnloadUnit(unit);
            unit = next;
        }
    }
    else if (first_loaded)
        success = UnloadUnit(first_loaded);
    else
        success = true; // Ai Can do this

    if (!success)
    {
        if (ai && !building)
        {
            Ai_UnloadFailure(this);
            return;
        }
    }
    else if (first_loaded)
    {
        return;
    }

    // Out of units, lets do something else
    order_flags |= 0x1;
    if (order_queue_begin)
    {
        DoNextQueuedOrder();
    }
    else if (ai)
    {
        while (order_queue_end)
        {
            Order *order = order_queue_end;
            if (!orders_dat_interruptable[order->order_id] && order->order_id != Order::ComputerAi)
                break;
            DeleteOrder(order);
        }
        AddOrder(this, Order::ComputerAi, nullptr, Unit::None, 0, nullptr);
        DoNextQueuedOrder();
    }
    else
    {
        IssueOrderTargetingNothing(this, units_dat_return_to_idle_order[unit_id]);
    }
}

void Unit::Order_MoveUnload()
{
    if (order_state == 0 && target)
    {
        order_state = 1;
        order_target_pos = target->sprite->position;
    }
     // Always false if non-ai, because target is then own pos
    int distance = Distance(exact_position, Point32(order_target_pos) * 256) & 0xffffff00;
    if (distance > 0x1000)
    {
        ChangeMovementTarget(order_target_pos);
        unk_move_waypoint = order_target_pos;
        if (ai && distance <= 0xca000)
        {
            if (GetTerrainHeight(sprite->position.x, sprite->position.y) != GetTerrainHeight(order_target_pos.x, order_target_pos.y))
                return;
            if (!Ai_AreOnConnectedRegions(this, order_target_pos.x, order_target_pos.y))
                return;

            bool unk = IsPointInArea(this, 0x200, order_target_pos.x, order_target_pos.y);
            for (Unit *unit = first_loaded; unit; unit = unit->next_loaded)
            {
                if (unit->unit_id == Reaver && !unk)
                    return;
                if (unit->IsWorker())
                    return;
                uint16_t pos[2];
                if (!GetUnloadPosition(pos, this, unit))
                    return;
            }
            PrependOrderTargetingNothing(this, Order::Unload);
            DoNextQueuedOrder();
        }
    }
    else
    {
        PrependOrderTargetingNothing(this, Order::Unload);
        DoNextQueuedOrder();
    }
}

bool Unit::IsTransport() const
{
    if (flags & UnitStatus::Hallucination)
        return false;
    if (unit_id == Unit::Overlord && GetUpgradeLevel(Upgrade::VentralSacs, player) == 0)
        return false;
    return units_dat_space_provided[unit_id] != 0;
}

bool Unit::HasWayOfAttacking() const
{
    if (GetAirWeapon() != Weapon::None || GetGroundWeapon() != Weapon::None)
        return true;

    switch (unit_id)
    {
        case Unit::Carrier:
        case Unit::Gantrithor:
        case Unit::Reaver:
        case Unit::Warbringer:
            if (carrier.in_hangar_count || carrier.out_hangar_count)
                return true;
        break;
    }
    if (subunit)
    {
        if (subunit->GetAirWeapon() != Weapon::None || subunit->GetGroundWeapon() != Weapon::None)
            return true;
    }
    return false;
}

void Unit::AskForHelp(Unit *attacker)
{
    if ((flags & UnitStatus::Burrowed) || IsWorker())
        return;

    if (unit_id == Unit::Arbiter || unit_id == Unit::Danimoth)
        return;

    if (!HasWayOfAttacking())
        return;
    if (!IsBetterTarget(attacker))
        return;

    SetPreviousAttacker(attacker);
    if (HasSubunit())
    {
        subunit->SetPreviousAttacker(attacker);
        subunit->last_attacking_player = last_attacking_player; // ???
    }
}

// Poor name? Maybe more like IsMoreImportantTarget
// Called only from UnitWasHit and cache cache assumes it works like that
bool Unit::IsBetterTarget(const Unit *other) const
{
    //STATIC_PERF_CLOCK(Unit_IsBetterTarget);

    if (other == previous_attacker)
        return false;
    if (!IsEnemy(other))
        return false;
    if (!previous_attacker)
        return true;

    if ((hotkey_groups & 0x01000000) == 0)
    {
        hotkey_groups |= 0x01000000;
        hotkey_groups &= ~(0x00400000 | 0x00800000);
        if (CanAttackUnit(previous_attacker))
        {
            hotkey_groups |= 0x00400000;
            if (IsInAttackRange(previous_attacker))
                hotkey_groups |= 0x00800000;
        }
    }

    if (~hotkey_groups & 0x00400000)
        return true;
    if (!IsEnemy(previous_attacker))
        return true;
    if (!IsInAttackRange(other))
        return false;
    if (~hotkey_groups & 0x00800000)
        return true;
    else
        return false;
}

bool Unit::IsAtHome() const
{
    if (!ai || ai->type != 1)
        return true;
    return ((Ai::GuardAi *)ai)->home == sprite->position;
}

void Unit::ReactToHit(Unit *attacker)
{
    if (unit_id == Larva)
        return;

    STATIC_PERF_CLOCK(Unit_ReactToHit);
    if (flags & UnitStatus::Burrowed && !irradiate_timer && unit_id != Lurker)
    {
        if (units_dat_flags[unit_id] & UnitFlags::Burrowable) // Huh?
            Unburrow(this);
    }
    else if (~units_dat_flags[unit_id] & UnitFlags::Building && (!CanAttackUnit(attacker) || IsWorker()))
    {
        if (unit_id == Lurker && flags & UnitStatus::Burrowed)
        {
            if (!ai || irradiate_timer)
                return;
        }
        Unit *self = this;
        if (units_dat_flags[unit_id] & UnitFlags::Subunit)
            self = subunit;
        if (orders_dat_fleeable[self->order] && self->flags & UnitStatus::Reacts && self->IsAtHome())
        {
            if (GetBaseMissChance(self) != 0xff) // Not under dark swarm
            {
                uint32_t flee_pos = PrepareFlee(self, attacker);
                IssueOrder(self, Order::Move, flee_pos, 0);
            }
        }
    }
    else
    {
        SetPreviousAttacker(attacker);
        if (HasSubunit())
        {
            subunit->last_attacking_player = last_attacking_player;
            subunit->SetPreviousAttacker(attacker);
        }
    }

}

Unit *Unit::GetActualTarget(Unit *target) const
{
    if (target->unit_id != Unit::Interceptor || !target->interceptor.parent)
        return target;
    if ((flags & UnitStatus::Reacts) || IsInAttackRange(target->interceptor.parent))
        return target->interceptor.parent;
    return target;
}

void Unit::Attack(Unit *enemy)
{
    order_flags |= 0x1;
    Point pos(0, 0);
    if (enemy)
        pos = enemy->sprite->position;
    AppendOrder(this, units_dat_attack_unit_order[unit_id], pos.AsDword(), enemy, None, 1);
    DoNextQueuedOrder();
}

bool Unit::IsInvisibleTo(const Unit *unit) const
{
    if (!IsInvisible())
        return false;
    return (detection_status & (1 << unit->player)) == 0;
}

void Unit::Cloak(int tech)
{
    if (flags & UnitStatus::BeginInvisibility)
        return;
    int energy_cost = techdata_dat_energy_cost[tech] * 256;
    if (!IsCheatActive(Cheats::The_Gathering) && energy < energy_cost)
        return;
    if (!IsCheatActive(Cheats::The_Gathering))
        energy -= energy_cost;

    IssueSecondaryOrder(Order::Cloak);
}

bool Unit::IsMorphingBuilding() const
{
    if (flags & UnitStatus::Completed)
        return false;
    switch (build_queue[current_build_slot])
    {
        case Unit::Lair:
        case Unit::Hive:
        case Unit::GreaterSpire:
        case Unit::SunkenColony:
        case Unit::SporeColony:
            return true;
        default:
            return false;
    }
}

bool Unit::IsResourceDepot() const
{
    if (~flags & UnitStatus::Building)
        return false;
    if (~flags & UnitStatus::Completed && !IsMorphingBuilding())
        return false;
    return units_dat_flags[unit_id] & UnitFlags::ResourceDepot;
}

// Hack fix for now.. Because unit might get deleted before UnitWasHit gets to be done for it
static void Puwh_Dying(Unit *target, Unit *attacker, Unit **nearby, ProgressUnitResults *results)
{
    if (attacker->flags & UnitStatus::InBuilding)
        attacker = attacker->related;
    // Kind of hacky as the whole point of Ai::HitUnit is to collect stuff for UnitWasHit,
    // but as target is dying it might just be dropped
    results->ai_hit_reactions.NewHit(target, attacker, true);
    if (target->ai == nullptr)
    {
        for (Unit *unit = *nearby++; unit; unit = *nearby++)
        {
            if (unit->order != Order::Die)
                unit->AskForHelp(attacker);
        }
    }
}

// Returns all attackers with which UnitWasHit has to be called
vector<Unit *> Unit::RemoveFromResults(ProgressUnitResults *results)
{
    vector<Unit *> ret;
    std::for_each(results->weapon_damages.begin(), results->weapon_damages.end(), [this, &ret](auto &a) {
        if (a.attacker == this)
            a.attacker = nullptr;
        else if (a.target == this && a.attacker != nullptr)
            ret.emplace_back(a.attacker);
    });
    auto wpn_last = std::remove_if(results->weapon_damages.begin(), results->weapon_damages.end(), [this](const auto &a) {
        return a.target == this;
    });
    results->weapon_damages.erase(wpn_last, results->weapon_damages.end());
    std::for_each(results->hallucination_hits.begin(), results->hallucination_hits.end(), [this, &ret](auto &a) {
        if (a.attacker == this)
            a.attacker = nullptr;
        else if (a.target == this && a.attacker != nullptr)
            ret.emplace_back(a.attacker);
    });
    auto hallu_last = std::remove_if(results->hallucination_hits.begin(), results->hallucination_hits.end(), [this](const auto &a) {
        return a.target == this;
    });
    results->hallucination_hits.erase(hallu_last, results->hallucination_hits.end());
    std::sort(ret.begin(), ret.end(), [](const Unit *a, const Unit *b){ return a->lookup_id < b->lookup_id; });
    auto end = std::unique(ret.begin(), ret.end());
    ret.erase(end, ret.end());
    return ret;
}

void Unit::Remove(ProgressUnitResults *results)
{
    if (HasSubunit())
    {
        subunit->Remove(results);
        subunit = nullptr;
    }
    Die(results);
    sprite->Remove();
    sprite = nullptr;
}

// results may be nullptr but should only be when called from BulletSystem
// Still, it cannot be called when the nearby_helping_untits threads are running,
// as it removes the ai
void Unit::Kill(ProgressUnitResults *results)
{
    // Commented out as it is recent change but should be decommented if unitframes_in_progress won't be deleted
    // If the assertion fires, an unit which was damaged in same frame as Kill() was called, would not do Puwh_Dying
    //Assert(results || !unitframes_in_progress);

    // This is more important, as having bulletframes_in_progress is desyncing race condition
    Assert(!bulletframes_in_progress);
    if (results)
    {
        for (Unit *attacker : RemoveFromResults(results))
        {
            if (hotkey_groups & 0x80000000)
            {
                // Since this search might be really late in thread task queue, this thread does the search
                // if it has not been finished
                Unit **null = nullptr, **used = (Unit **)0x1;
                if (nearby_helping_units.compare_exchange_strong(null, used, std::memory_order_release, std::memory_order_relaxed) == true)
                {
                    nearby_helping_units.store(FindNearbyHelpingUnits(this, &pbf_memory), std::memory_order_relaxed);
                }
            }
            else
                nearby_helping_units.store(FindNearbyHelpingUnits(this, &pbf_memory), std::memory_order_relaxed);
            Puwh_Dying(this, attacker, nearby_helping_units.load(std::memory_order_relaxed), results);
        }
    }
    DropPowerup(this);
    while (order_queue_begin)
        DeleteOrder(order_queue_begin);

    order_flags |= 0x1;
    AddOrder(this, Order::Die, nullptr, Unit::None, order_target_pos.AsDword(), nullptr);
    DoNextQueuedOrder();
    Ai::RemoveUnitAi(this, false);
}

void Unit::RemoveFromHangar()
{
    Unit *parent = interceptor.parent;
    if (!parent)
    {
        interceptor.list.prev = nullptr;
        interceptor.list.next = nullptr;
    }
    else
    {
        interceptor.list.Remove([&]() { return std::ref(interceptor.is_outside_hangar ? parent->carrier.out_child : parent->carrier.in_child); } ()); // =D

        if (interceptor.is_outside_hangar)
            parent->carrier.out_hangar_count--;
        else
            parent->carrier.in_hangar_count--;
    }
}

void Unit::RemoveHarvesters()
{
    Unit *worker = resource.first_awaiting_worker;
    resource.awaiting_workers = 0;
    while (worker)
    {
        Unit *next = worker->harvester.harvesters.next;
        worker->harvester.harvesters.prev = nullptr;
        worker->harvester.harvesters.next = nullptr;
        worker->harvester.previous_harvested = nullptr;
        worker = next;
    }
}

void Unit::KillHangarUnits(ProgressUnitResults *results)
{
    while (carrier.in_hangar_count)
    {
        Unit *child = carrier.in_child;
        child->interceptor.parent = nullptr;
        carrier.in_child = child->interceptor.list.next;
        child->Kill(results);
        carrier.in_hangar_count--;
    }
    while (carrier.out_hangar_count)
    {
        Unit *child = carrier.out_child;
        child->interceptor.parent = nullptr;
        if (child->unit_id != Scarab)
        {
            int death_time = 15 + MainRng()->Rand(31);
            if (!child->death_timer || child->death_timer > death_time)
                child->death_timer = death_time;
        }
        carrier.out_child = child->interceptor.list.next;
        child->interceptor.list.prev = nullptr;
        child->interceptor.list.next = nullptr;
        carrier.out_hangar_count--;
    }
    carrier.in_child = nullptr;
    carrier.out_child = nullptr;
}

void Unit::KillChildren(ProgressUnitResults *results)
{
    switch (unit_id)
    {
        case Unit::Carrier:
        case Unit::Reaver:
        case Unit::Gantrithor:
        case Unit::Warbringer:
            KillHangarUnits(results);
            return;
        case Unit::Interceptor:
        case Unit::Scarab:
            if (flags & UnitStatus::Completed)
                RemoveFromHangar();
            return;
        case Unit::Ghost:
            if (ghost.nukedot != nullptr)
            {
                ghost.nukedot->SetIscriptAnimation_Lone(Iscript::Animation::Death, true, MainRng(), "Unit::KillChildren");
            }
            return;
        case Unit::NuclearSilo:
            if (silo.nuke)
            {
                silo.nuke->Kill(results);
                silo.nuke = nullptr;
            }
            return;
        case Unit::NuclearMissile:
            if (related && related->unit_id == Unit::NuclearSilo)
            {
                related->silo.nuke = nullptr;
                related->silo.has_nuke = 0;
            }
            return;
        case Unit::Pylon:
            if (pylon.aura)
            {
                pylon.aura->Remove();
                pylon.aura = nullptr;
            }
            // Incompleted pylons are not in list, but maybe it can die as well before being added to the list (Order_InitPylon adds them)
            if (pylon_list.list.prev == nullptr && pylon_list.list.next == nullptr && *bw::first_pylon != this)
                return;

            pylon_list.list.Remove(*bw::first_pylon);
            *bw::pylon_refresh = 1;
            return;
        case Unit::NydusCanal:
        {
            Unit *exit = nydus.exit;
            if (exit)
            {
                exit->nydus.exit = nullptr;
                nydus.exit = nullptr;
                exit->Kill(results);
            }
            return;
        }

        case Unit::Refinery:
        case Unit::Extractor:
        case Unit::Assimilator:
            if (~flags & UnitStatus::Completed)
                return;
            RemoveHarvesters();
            return;
        case Unit::MineralPatch1:
        case Unit::MineralPatch2:
        case Unit::MineralPatch3:
            RemoveHarvesters();
            return;
    }
}

void Unit::RemoveFromLists()
{
    // Soem mapping tricks may depend on overflowing player units array
    player_units.Remove(bw::first_player_unit.index_overflowing(player));
    player_units.next = nullptr;
    player_units.prev = nullptr;

    if (next())
        next()->prev() = prev();
    else
    {
        if (unit_id == Unit::ScannerSweep || unit_id == Unit::MapRevealer)
            *bw::last_revealer = prev();
        else if (sprite->IsHidden())
            *bw::last_hidden_unit = prev();
        else
            *bw::last_active_unit = prev();
    }
    if (prev())
        prev()->next() = next();
    else
    {
        if (unit_id == Unit::ScannerSweep || unit_id == Unit::MapRevealer)
            *bw::first_revealer = next();
        else if (sprite->IsHidden())
            *bw::first_hidden_unit = next();
        else
            *bw::first_active_unit = next();
    }

    if (*bw::first_dying_unit)
    {
        list.Add(*bw::first_dying_unit);
    }
    else
    {
        *bw::first_dying_unit = this;
        *bw::last_dying_unit = this;
        next() = nullptr;
        prev() = nullptr;
    }
}

void Unit::BuildingDeath(ProgressUnitResults *results)
{
    if (secondary_order == Order::BuildAddon && currently_building && ~currently_building->flags & UnitStatus::Completed)
        currently_building->CancelConstruction(results);
    if (building.addon)
        DetachAddon(this);
    if (building.tech != Tech::None)
        CancelTech(this);
    if (building.upgrade != Upgrade::None)
        CancelUpgrade(this);
    if (build_queue[current_build_slot] >= CommandCenter)
        CancelTrain(results);
    if (*bw::is_placing_building)
        EndAddonPlacement();
    if (GetRace() == Race::Zerg)
    {
        if (IsMorphingBuilding())
            flags |= UnitStatus::Completed;
        if (UpdateCreepDisappearance_Unit(this) == false)
            flags |= UnitStatus::HasDisappearingCreep;
    }
}

static int __stdcall ret_false_s3(int a, int b, int c)
{
    return 0;
}

bool Unit::RemoveSubunitOrGasContainer()
{
    if (units_dat_flags[unit_id] & UnitFlags::Subunit)
    {
        // It seems that subunits are not on any list to begin with?
        Assert(next() == nullptr && prev() == nullptr);
        if (*bw::first_dying_unit)
        {
            list.Add(*bw::first_dying_unit);
        }
        else
        {
            *bw::first_dying_unit = this;
            *bw::last_dying_unit = this;
            next() = nullptr;
            prev() = nullptr;
        }
        player_units.Remove(bw::first_player_unit.index_overflowing(player));
        player_units.next = nullptr;
        player_units.prev = nullptr;
        ModifyUnitCounters(this, -1);
        subunit = nullptr;
        return true;
    }
    if (sprite->IsHidden())
        return false;
    switch (unit_id)
    {
        case Extractor:
            RemoveCreepAtUnit(sprite->position.x, sprite->position.y, unit_id, (void *)&ret_false_s3);
            // No break
        case Refinery:
        case Assimilator:
            if (flags & UnitStatus::Completed)
                ModifyUnitCounters2(this, -1, 0);
            flags &= ~UnitStatus::Completed;
            resource.first_awaiting_worker = nullptr;
            TransformUnit(this, VespeneGeyser);
            order = units_dat_human_idle_order[unit_id];
            order_state = 0;
            order_target_pos = Point(0, 0);
            target = nullptr;
            order_fow_unit = None;
            hitpoints = units_dat_hitpoints[unit_id];
            GiveUnit(this, NeutralPlayer, 0);
            GiveSprite(this, NeutralPlayer);
            flags |= UnitStatus::Completed;
            ModifyUnitCounters2(this, 1, 1);
            if (*bw::is_placing_building)
                RedrawGasBuildingPlacement(this);
            return true;
        break;
        break;
        default:
            return false;
    }
}

void Unit::Die(ProgressUnitResults *results)
{
    if (units_dat_flags[unit_id] & UnitFlags::Building)
        *bw::ignore_unit_flag80_clear_subcount = 1;
    KillChildren(results);
    TransportDeath(results);
    RemoveReferences(this, 1);
    // Hak fix
    for (Unit *unit : first_allocated_unit)
    {
        if (unit->path && unit->path->dodge_unit == this)
            unit->path->dodge_unit = nullptr;
    }

    RemoveFromBulletTargets(this);
    Ai::RemoveUnitAi(this, false);
    if (RemoveSubunitOrGasContainer())
        return;

    StopMoving(this);
    if (!sprite->IsHidden())
        RemoveFromMap(this);

    if (~units_dat_flags[unit_id] & UnitFlags::Building)
    {
        invisibility_effects = 0;
        RemoveFromCloakedUnits(this);
        if (flags & UnitStatus::BeginInvisibility)
        {
            EndInvisibility(this, Sound::Decloak);
        }
    }
    if (flags & UnitStatus::Building)
        BuildingDeath(results);
    if ((units_dat_flags[unit_id] & UnitFlags::FlyingBuilding) && (building.is_landing != 0))
        ClearBuildingTileFlag(this, order_target_pos.x, order_target_pos.y);

    DeletePath();
    if (currently_building)
    {
        if (secondary_order == Order::Train || secondary_order == Order::TrainFighter)
            currently_building->Kill(results);
        currently_building = nullptr;
    }
    IssueSecondaryOrder(Order::Nothing);
    DropPowerup(this);
    RemoveFromSelections(this);
    RemoveFromClientSelection3(this);
    RemoveSelectionCircle(sprite.get());
    ModifyUnitCounters(this, -1);
    if (flags & UnitStatus::Completed)
        ModifyUnitCounters2(this, -1, 0);

    if ((*bw::is_placing_building) && !IsReplay() && (*bw::local_player_id == player))
    {
        if (!CanPlaceBuilding(*bw::primary_selected, *bw::placement_unit_id, *bw::placement_order))
        {
            MarkPlacementBoxAreaDirty();
            EndBuildingPlacement();
        }
    }
    RemoveFromLists();
    RefreshUi();
}

void TransferMainImage(Sprite *dest, Sprite *src)
{
    if (!src)
        return;

    Image *img = src->main_image;
    src->main_image = nullptr;
    if (src->first_overlay == img)
        src->first_overlay = img->list.next;
    if (src->last_overlay == img)
        src->last_overlay = img->list.prev;
    Image *first = dest->first_overlay;
    dest->first_overlay = img;
    if (img->list.prev)
        img->list.prev->list.next = img->list.next;
    if (img->list.next)
        img->list.next->list.prev = img->list.prev;
    img->list.prev = nullptr;
    img->list.next = first;
    first->list.prev = img;
    // Why?
    img->flags &= ~ImageFlags::CanTurn;
    img->parent = dest;
}

void Unit::IssueSecondaryOrder(int order_id)
{
    if (secondary_order == order_id)
        return;
    secondary_order = order_id;
    secondary_order_state = 0;
    currently_building = nullptr;
    unke8 = 0;
    unkea = 0;
    // Not touching the secondary order timer..
}

void Unit::Order_Die(ProgressUnitResults *results)
{
    Assert(sprite->first_overlay);
    if (order_flags & 0x4) // Remove death
        HideUnit(this);
    if (subunit)
    {
        TransferMainImage(sprite.get(), subunit->sprite.get());
        subunit->Remove(results);
        subunit = nullptr;
    }
    order_state = 1;
    for (Unit *attacker : RemoveFromResults(results))
    {
        Assert(~hotkey_groups & 0x80000000);
        nearby_helping_units.store(FindNearbyHelpingUnits(this, &pbf_memory), std::memory_order_relaxed);
        Puwh_Dying(this, attacker, nearby_helping_units.load(std::memory_order_relaxed), results);
    }
    if (!sprite->IsHidden())
    {
        if (~flags & UnitStatus::Hallucination || flags & UnitStatus::SelfDestructing)
        {
            SetIscriptAnimation(Iscript::Animation::Death, true, "Order_Die", results);
            Die(results);
            return;
        }
        else
        {
            PlaySound(Sound::HallucinationDeath, this, 1, 0);
            Sprite *death = lone_sprites->AllocateLone(Sprite::HallucinationDeath, sprite->position, player);
            death->elevation = sprite->elevation + 1;
            SetVisibility(death, sprite->visibility_mask);
            HideUnit(this);
            Remove(results);
        }
    }
    else
    {
        Remove(results);
    }
}

void Unit::CancelConstruction(ProgressUnitResults *results)
{
    if (!sprite || order == Order::Die || flags & UnitStatus::Completed)
        return;
    if (unit_id == Guardian || unit_id == Lurker || unit_id == Devourer || unit_id == Mutalisk || unit_id == Hydralisk)
        return;
    if (unit_id == NydusCanal && nydus.exit)
        return;
    if (flags & UnitStatus::Building)
    {
        if (GetRace() == Race::Zerg)
        {
            CancelZergBuilding(results);
            return;
        }
        else
            RefundFourthOfCost(player, unit_id);
    }
    else
    {
        int constructed_id = unit_id;
        if (unit_id == Egg || unit_id == Cocoon || unit_id == LurkerEgg)
            constructed_id = build_queue[current_build_slot];
        RefundFullCost(constructed_id, player);
    }
    if (unit_id == Cocoon || unit_id == LurkerEgg)
    {
        if (unit_id == Cocoon)
            TransformUnit(this, Mutalisk);
        else
            TransformUnit(this, Hydralisk);
        build_queue[current_build_slot] = None;
        remaining_build_time = 0;
        int old_image = sprites_dat_image[flingy_dat_sprite[units_dat_flingy[previous_unit_id]]];
        ReplaceSprite(old_image, 0, sprite.get());
        order_signal &= ~0x4;
        SetIscriptAnimation(Iscript::Animation::Special2, true, "CancelConstruction", results);
        IssueOrderTargetingNothing(this, Order::Birth);
    }
    else
    {
        if (unit_id == NuclearMissile)
        {
            if (related)
            {
                related->silo.nuke = nullptr;
                related->order_state = 0;
            }
            RefreshUi();
        }
        Kill(results);
    }
}

void Unit::CancelZergBuilding(ProgressUnitResults *results)
{
    if (~flags & UnitStatus::Completed && IsMorphingBuilding())
    {
        CancelBuildingMorph(this);
    }
    else
    {
        RefundFourthOfCost(player, unit_id);
        bw::building_count[player]--;
        bw::men_score[player] += units_dat_build_score[Drone];
        if (unit_id == Extractor)
        {
            Unit *drone = CreateUnit(Drone, sprite->position.x, sprite->position.y, player);
            FinishUnit_Pre(drone);
            FinishUnit(drone);
            // Bw bug, uses uninitialized(?) value because extractor is separately created
            // So it gets set to set max hp anyways
            //SetHp(drone, previous_hp * 256);
            Kill(results);
        }
        else
        {
            int prev_hp = previous_hp * 256, old_id = unit_id;
            Sprite *spawn = lone_sprites->AllocateLone(Sprite::ZergBuildingSpawn_Small, sprite->position, player);
            spawn->elevation = sprite->elevation + 1;
            spawn->UpdateVisibilityPoint();
            PlaySound(Sound::ZergBuildingCancel, this, 1, 0);
            TransformUnit(this, Drone);
            if (bw::players[player].type == 1 && ai)
            {
                Ai::BuildingAi *bldgai = (Ai::BuildingAi *)ai;
                Ai::Town *town = bldgai->town;
                bldgai->list.Remove(town->first_building);
                Ai::AddUnitAi(this, town);
            }
            FinishUnit_Pre(this);
            FinishUnit(this);
            Image *lowest = sprite->last_overlay;
            if (lowest->drawfunc == Image::Shadow && lowest->y_off != 7)
            {
                lowest->y_off = 7;
                lowest->flags |= 0x1;
            }
            PrepareDrawSprite(sprite.get());
            IssueOrderTargetingNothing(this, Order::ResetCollision1);
            AppendOrder(this, units_dat_return_to_idle_order[unit_id], 0, 0, None, 0);
            SetHp(this, prev_hp);
            UpdateCreepDisappearance(old_id, sprite->position.x, sprite->position.y, 0);
        }
    }
}

// Maybe more like HasReachedDestination()
int Unit::IsStandingStill() const
{
    if (move_target != sprite->position)
        return 0;
    if (flags & UnitStatus::MovePosUpdated)
        return 2;
    else
        return 1;
}

bool Unit::IsUnreachable(const Unit *other) const
{
    if (pathing_flags & 0x1)
        return false;
    int own_id = GetRegion();
    int other_id = other->GetRegion();
    Pathing::Region *own_region = (*bw::pathing)->regions + own_id;
    Pathing::Region *other_region = (*bw::pathing)->regions + other_id;
    if (own_region->group != other_region->group)
        return true;
    return AreConnected(player, own_id, other_id) == false;
}

bool Unit::IsCritter() const
{
    switch (unit_id)
    {
        case Bengalaas:
        case Rhynadon:
        case Scantid:
        case Kakaru:
        case Ragnasaur:
        case Ursadon:
            return true;
        default:
            return false;
    }
}

// Some of the logic is duped in EnemyUnitCache :/
bool Unit::CanAttackUnit(const Unit *enemy, bool check_detection) const
{
    //STATIC_PERF_CLOCK(Unit_CanAttackUnit);
    if (IsDisabled())
        return false;
    if (!enemy->CanBeAttacked())
        return false;

    if (check_detection && enemy->IsInvisibleTo(this))
        return false;

    switch (unit_id)
    {
        case Carrier:
        case Gantrithor:
            return true;
        break;
        case Reaver:
        case Warbringer:
            return Reaver_CanAttackUnit(enemy);
        break;
        case Queen:
        case Matriarch:
            return enemy->CanBeInfested();
        break;
        case Arbiter:
            if (ai)
                return false;
        break;
        default:
        break;
    }

    const Unit *turret = GetTurret();

    if (enemy->IsFlying())
        return turret->GetAirWeapon() != Weapon::None;
    else
        return turret->GetGroundWeapon() != Weapon::None;
}

bool Unit::CanBeAttacked() const
{
    if (sprite->IsHidden() || IsInvincible() || units_dat_flags[unit_id] & UnitFlags::Subunit)
        return false;
    return true;
}

bool Unit::CanAttackUnit_Fast(const Unit *enemy, bool check_detection) const
{
    if (check_detection && enemy->IsInvisibleTo(this))
        return false;

    switch (unit_id)
    {
        case Carrier:
        case Gantrithor:
            // Nothing
        break;
        case Reaver:
        case Warbringer:
            if (!Reaver_CanAttackUnit(enemy))
                return false;
        break;
        case Queen:
        case Matriarch:
            if (!enemy->CanBeInfested())
                return false;
        break;
        case Arbiter:
            if (ai)
                return false;
        break;
        default:
        break;
    }
    return true;
}

Unit *Unit::PickBestTarget(Unit **targets, int amount) const
{
    if (ai && unit_id == Scourge)
        return *std::max_element(targets, targets + amount, [](const Unit *a, const Unit *b) { return a->GetHealth() < b->GetHealth(); });
    else
        return *std::min_element(targets, targets + amount, [this](const Unit *a, const Unit *b)
            { return Distance(sprite->position, a->sprite->position) < Distance(sprite->position, b->sprite->position); });
}

Unit *Unit::GetAutoTarget() const
{
    // The cache is valid only for so long as:
    //   - Alliances don't change
    //   - Units don't move
    //   - Units don't switch between ground/air
    //   - Units don't die
    //   - Units don't switch players
    //   - Units don't get new weapons
    // Otherwise some inconsistencies may occur.
    // Should not be a desyncing issue though.
    Assert(late_unit_frames_in_progress || bulletframes_in_progress);
    STATIC_PERF_CLOCK(Unit_GetAutoTarget);
    if (player >= Limits::Players)
        return nullptr;

    int max_range = GetTargetAcquisitionRange();
    if (flags & UnitStatus::InBuilding)
        max_range += 2;
    else if (ai && units_dat_sight_range[unit_id] > max_range)
        max_range = units_dat_sight_range[unit_id];
    max_range *= 32;

    int min_range;
    if (GetGroundWeapon() == Weapon::None && GetAirWeapon() == Weapon::None)
        min_range = 0;
    else if (GetGroundWeapon() == Weapon::None && GetAirWeapon() != Weapon::None)
        min_range = weapons_dat_min_range[GetAirWeapon()];
    else if (GetGroundWeapon() != Weapon::None && GetAirWeapon() == Weapon::None)
        min_range = weapons_dat_min_range[GetGroundWeapon()];
    else
        min_range = min(weapons_dat_min_range[GetAirWeapon()], weapons_dat_min_range[GetGroundWeapon()]);

    Rect16 area = Rect16(sprite->position, max_range + 0x40);
    Unit *possible_targets[0x6 * 0x10];
    int possible_target_count[0x6] = { 0, 0, 0, 0, 0, 0 };
    enemy_unit_cache->ForAttackableEnemiesInArea(unit_search, this, area, [&](Unit *other, bool *stop)
    {
        if (~other->sprite->visibility_mask & (1 << player))
            return;
        if ((min_range && IsInArea(this, min_range, other)) || !IsInArea(this, max_range, other))
            return;
        const Unit *turret = GetTurret();
        if (~turret->flags & UnitStatus::FullAutoAttack)
        {
            if (!CheckFiringAngle(turret, units_dat_ground_weapon[turret->unit_id], other->sprite->position.x, other->sprite->position.y))
                return;
        }
        if (Ai_ShouldStopChasing(other))
            return;
        int threat_level = GetThreatLevel(this, other);
        if (possible_target_count[threat_level] != 0x10)
            possible_targets[threat_level * 0x10 + possible_target_count[threat_level]++] = other;
        if (possible_target_count[0] == 0x10)
            *stop = true;
    });
    int threat_level = 0;
    for (; threat_level < 6; threat_level++)
    {
        if (possible_target_count[threat_level])
            return PickBestTarget(possible_targets + 0x10 * threat_level, possible_target_count[threat_level]);
    }
    return nullptr;
}

// Ai only, see ChooseTarget_Player for more info
// Skips players that are allies (though neutral has to be checked separately)
// Also assumes players 9-11 won't exist..
Unit *Unit::ChooseTarget(UnitSearchRegionCache::Entry units, int region_id, bool ground)
{
    int ret_strength = -1;
    Unit *ret = nullptr;
    for (auto i = 0; i < Limits::ActivePlayers; i++)
    {
        if (bw::alliances[player][i] != 0)
            continue;
        auto tp = ChooseTarget_Player(false, units.PlayerUnits(i), region_id, ground);
        if (get<int>(tp) > ret_strength)
            ret = get<Unit *>(tp);
    }
    auto tp = ChooseTarget_Player(true, units.PlayerUnits(11), region_id, ground);
    if (get<int>(tp) > ret_strength)
        ret = get<Unit *>(tp);
    return ret;
}

// Ai only
// The units arr has to be sorted by GetCurrentStrength(ground) descending,
// so that once acceptable unit has been found, this function can (usually) instantly return
// There's still that ridiculous hardcoded wraith check
tuple<int, Unit *> Unit::ChooseTarget_Player(bool check_alliance, Array<Unit *> units, int region_id, bool ground)
{
    int ret_strength = -1;
    Unit *ret = nullptr;
    int home_region = -1;
    if (ai && ai->type == 1)
    {
        Ai::GuardAi *guard = (Ai::GuardAi *)ai;
        home_region = ::GetRegion(guard->home);
    }
    bool ignore_critters = bw::player_ai[player].flags & 0x20;
    for (Unit *unit : units)
    {
        if (unit->IsDying())
            continue;
        if (ignore_critters)
        {
            if ((check_alliance && !IsEnemy(unit)) || unit->IsCritter())
                continue;
        }
        else if ((check_alliance && !IsEnemy(unit)) && !unit->IsCritter())
        {
            continue;
        }

        int strength = 0;
        if (unit_id == Wraith)
        {
            strength = GetCurrentStrength(unit, ground);
            if (strength + 1000 < ret_strength)
                return make_tuple(strength, unit);
            if (unit->flags & UnitStatus::Reacts && unit->CanDetect())
                strength += 1000;
            if (strength < ret_strength)
                continue;
        }

        if (!CanAttackUnit_Fast(unit, true))
            continue;

        if (!IsFlying() && !IsInAttackRange(unit) && flags & UnitStatus::Unk80)
            continue;
        if (home_region != -1)
        {
            if (unit->GetRegion() != home_region)
            {
                Ai::GuardAi *guard = (Ai::GuardAi *)ai;
                if (((Distance(Point32(unit->sprite->position) * 256, exact_position)) & 0xFFFFFF00) > 0xc000)
                    continue;
                if (!IsPointInArea(this, 0xc0, guard->home.x, guard->home.y))
                    continue;
            }
        }
        else
        {
            if (unit->GetRegion() != region_id)
            {
                if (!IsInArea(this, 0x120, unit))
                    continue;
            }
        }
        if (unit_id != Wraith)
            return make_tuple(strength, unit);
        else
        {
            ret_strength = strength;
            ret = unit;
        }
    }
    return make_tuple(ret_strength, ret);
}

bool Unit::CanDetect() const
{
    if (~units_dat_flags[unit_id] & UnitFlags::Detector)
        return false;
    if (~flags & UnitStatus::Completed)
        return false;
    if (IsDisabled())
        return false;
    if (blind)
        return false;
    return true;
}

Unit *Unit::ChooseTarget(bool ground)
{
    STATIC_PERF_CLOCK(Unit_ChooseTarget);
    if (IsDisabled())
        return 0;

    int region = GetRegion();
    auto units = unit_search->FindUnits_ChooseTarget(region, ground);
    return ChooseTarget(units, region, ground);
}

Unit *Unit::Ai_ChooseAirTarget()
{
    if (IsReaver() || GetAirWeapon() == Weapon::None)
        return nullptr;

    if (ai->type != 4)
        return ChooseTarget(false);
    return ((Ai::MilitaryAi *)ai)->region->air_target;
}

Unit *Unit::Ai_ChooseGroundTarget()
{
    if (GetGroundWeapon() == Weapon::None)
        return nullptr;

    if (ai->type != 4)
        return ChooseTarget(true);
    return ((Ai::MilitaryAi *)ai)->region->ground_target;
}

void Unit::ClearTarget()
{
    target = nullptr;
    if (HasSubunit())
        subunit->target = nullptr;
}

void Unit::SetPreviousAttacker(Unit *attacker)
{
    Assert(order != Order::Die || order_state != 1);
    previous_attacker = attacker;
    hotkey_groups &= ~0x00c00000;
}

// NOTE: Sc version allow other == null, in which case it'll check order_target_pos vision
bool Unit::CanSeeUnit(const Unit *other) const
{
    int player_mask = 1 << player;
    if (other->IsInvisibleTo(this))
        return false;

    return (other->sprite->visibility_mask & player_mask);
}

uint32_t Unit::GetHaltDistance() const
{
    if (!next_speed || flingy_movement_type != 0)
        return 0;
    if (next_speed == flingy_dat_top_speed[flingy_id] && acceleration == flingy_dat_acceleration[flingy_id])
        return flingy_dat_halt_distance[flingy_id];
    else
        return (next_speed * next_speed) / (acceleration * 2);
}

// Sc ver uses target if other == null
bool Unit::WillBeInAreaAtStop(const Unit *other, int range) const
{
    if (flingy_flags & 0x2)
    {
        if (other->flingy_flags & 0x2)
        {
            uint32_t direction_diff = (other->movement_direction - movement_direction) & 0xff;
            if (direction_diff > 0x80) // abs()
                direction_diff = 0x100 - direction_diff;
            if (direction_diff > 0x20)
                range += GetHaltDistance() / 256;
        }
        else
            range += GetHaltDistance() / 256;
    }
    return IsInArea(this, range, other);
}

// Returns always false for reavers/carriers, but that's how bw works
bool Unit::IsInAttackRange(const Unit *other) const
{
    //STATIC_PERF_CLOCK(Unit_IsInAttackRange);
    if (!other)
    {
        other = target;
        if (!other)
            return true;
    }
    if (!CanSeeUnit(other))
        return false;

    int weapon;
    const Unit *turret = GetTurret();
    if (other->IsFlying())
        weapon = turret->GetAirWeapon();
    else
        weapon = turret->GetGroundWeapon();

    if (weapon == Weapon::None)
        return false;

    int min_range = weapons_dat_min_range[weapon];
    if (min_range && IsInArea(this, min_range, other))
        return false;

    return WillBeInAreaAtStop(other, GetWeaponRange(!other->IsFlying()));
}

int Unit::GetDistanceToUnit(const Unit *other) const
{
    //STATIC_PERF_CLOCK(Unit_GetDistanceToUnit);
    Rect16 other_rect;
    other_rect.left = other->sprite->position.x - units_dat_dimensionbox[other->unit_id].left;
    other_rect.top = other->sprite->position.y - units_dat_dimensionbox[other->unit_id].top;
    other_rect.right = other->sprite->position.x + units_dat_dimensionbox[other->unit_id].right + 1;
    other_rect.bottom = other->sprite->position.y + units_dat_dimensionbox[other->unit_id].bottom + 1;
    const Unit *self = this;
    if (units_dat_flags[unit_id] & UnitFlags::Subunit)
        self = subunit;

    Rect16 self_rect;
    self_rect.left = self->sprite->position.x - units_dat_dimensionbox[self->unit_id].left;
    self_rect.top = self->sprite->position.y - units_dat_dimensionbox[self->unit_id].top;
    self_rect.right = self->sprite->position.x + units_dat_dimensionbox[self->unit_id].right + 1;
    self_rect.bottom = self->sprite->position.y + units_dat_dimensionbox[self->unit_id].bottom + 1;

    int x = self_rect.left - other_rect.right;
    if (x < 0)
    {
        x = other_rect.left - self_rect.right;
        if (x < 0)
            x = 0;
    }
    int y = self_rect.top - other_rect.bottom;
    if (y < 0)
    {
        y = other_rect.top - self_rect.bottom;
        if (y < 0)
            y = 0;
    }
    return Distance(Point32(0,0), Point32(x, y));
}

// Sc ver uses target if other == null
bool Unit::IsThreat(const Unit *other) const
{
    if (!IsEnemy(other))
        return false;

    if (other->flags & UnitStatus::Hallucination)
    {
        if (other->GetHealth() != other->GetMaxHealth())
            return false;
    }
    if (other->CanAttackUnit(this))
    {
        return !IsOutOfRange(other, this);
    }
    else if (other->unit_id == Bunker && other->first_loaded)
    {
        return true;
    }
    else
    {
        return false;
    }
}

const Unit *Unit::GetBetterTarget(const Unit *unit) const
{
    if (unit->unit_id == Interceptor && unit->interceptor.parent)
    {
        if (flags & UnitStatus::Reacts || IsInAttackRange(unit->interceptor.parent))
            return unit->interceptor.parent;
    }
    return unit;
}

const Unit *Unit::ValidateTarget(const Unit *unit) const
{
    if (IsComputerPlayer(player) && !IsFlying() && unit->flags & UnitStatus::Unk80 && !IsInAttackRange(unit))
        return nullptr;
    if (!CanAttackUnit(unit, true))
        return nullptr;
    return unit;
}

const Unit *Unit::Ai_ChooseBetterTarget(const Unit *cmp, const Unit *prev) const
{
    if (prev == nullptr)
        return cmp;
    if (cmp == nullptr)
        return prev;

    //STATIC_PERF_CLOCK(Unit_ChooseBetterTarget);

    cmp = ValidateTarget(GetBetterTarget(cmp));
    if (cmp == nullptr)
        return prev;
    prev = ValidateTarget(GetBetterTarget(prev));
    if (prev == nullptr)
        return cmp;

    bool prev_can_attack = prev->HasWayOfAttacking();
    bool cmp_can_attack = cmp->HasWayOfAttacking();

    if (!cmp_can_attack && prev_can_attack)
    {
        if (cmp->unit_id != Bunker || !cmp->first_loaded)
        {
            return prev;
        }
    }
    if (!prev_can_attack && cmp_can_attack)
    {
        if (prev->unit_id != Bunker || !prev->first_loaded)
        {
            return cmp;
        }
    }
    if (!IsThreat(prev) && IsThreat(cmp))
        return cmp;

    if (IsInAttackRange(prev))
        return prev;
    if (IsInAttackRange(cmp))
        return cmp;

    if (GetDistanceToUnit(cmp) < GetDistanceToUnit(prev))
        return cmp;
    else
        return prev;
}

void Unit::ReduceEnergy(int amt)
{
    if (!IsCheatActive(Cheats::The_Gathering))
        energy -= amt;
}

// Well Order_AttackMove_Generic
int Unit::Order_AttackMove_ReactToAttack(int order)
{
    //STATIC_PERF_CLOCK(Order_AttackMove_ReactToAttack);
    if (order_state == 0)
    {
        bool ret = ChangeMovementTarget(order_target_pos);
        unk_move_waypoint = order_target_pos;
        if (!ret)
            return 0;
        order_state = 1;
    }
    if (previous_attacker != nullptr && IsEnemy(previous_attacker) && !previous_attacker->sprite->IsHidden())
    {
        if (~previous_attacker->flags & UnitStatus::Hallucination && previous_attacker->IsInvisibleTo(this))
        {
            if (ai != nullptr)
            {
                return Ai::UpdateAttackTarget(this, false, false, false) == false;
            }
            else
            {
                StopMoving(this);
                PrependOrder(order, target, order_target_pos);
                InsertOrder(this, units_dat_attack_unit_order[unit_id], previous_attacker, previous_attacker->sprite->position.AsDword(), None, order_queue_begin.AsRawPointer());
                previous_attacker = nullptr;
                DoNextQueuedOrderIfAble(this);
                AllowSwitchingTarget();
                return 0;
            }
        }
    }
    return 1;
}

void Unit::Order_AttackMove_TryPickTarget(int order)
{
    //STATIC_PERF_CLOCK(Order_AttackMove_TryPickTarget);
    if (GetTargetAcquisitionRange() == 0)
        return;

    if (ai)
    {
        Ai::UpdateAttackTarget(this, false, false, false);
    }
    else
    {
        Unit *auto_target = GetAutoTarget();
        if (auto_target)
        {
            StopMoving(this);
            PrependOrder(order, target, order_target_pos);
            InsertOrder(this, units_dat_attack_unit_order[unit_id], auto_target, auto_target->sprite->position.AsDword(), None, order_queue_begin.AsRawPointer());
            DoNextQueuedOrderIfAble(this);
            AllowSwitchingTarget();
        }
    }
}

bool Unit::ChangeMovementTargetToUnit(Unit *new_target)
{
    Point new_target_pos = new_target->sprite->position;
    if (new_target == move_target_unit)
    {
        if (move_target == new_target_pos || IsPointAtUnitBorder(this, new_target, move_target.AsDword()))
        {
            flags &= ~UnitStatus::MovePosUpdated;
            return true;
        }
    }
    if (ai && order != Order::Pickup4 && Ai_PrepareMovingTo(this, new_target_pos.x, new_target_pos.y))
        return false;
    if (path)
        path->flags |= 0x1;
    if (new_target_pos != move_target || new_target != move_target_unit)
    {
        move_target = new_target_pos;
        move_target_unit = new_target;
        flingy_flags &= ~0x4;
        flingy_flags |= 0x1;
        next_move_waypoint = move_target;
    }
    flags &= ~UnitStatus::MovePosUpdated;
    move_target_update_timer = 0xf;
    if (!order_queue_begin || orders_dat_unknown7[order_queue_begin->order_id])
        flingy_flags &= ~0x20;
    else
        flingy_flags |= 0x20;
    return true;
}

bool Unit::ChangeMovementTarget(const Point &pos)
{
    if (pos == move_target)
        return true;
    if (ai && Ai_PrepareMovingTo(this, pos.x, pos.y))
        return false;
    if (path)
        path->flags |= 0x1;

    Point clipped_pos = pos;
    ClipPointInBoundsForUnit(unit_id, (uint16_t *)&clipped_pos);
    if (clipped_pos != move_target)
    {
        move_target = clipped_pos;
        move_target_unit = 0;
        flingy_flags &= ~0x4;
        flingy_flags |= 0x1;
        next_move_waypoint = move_target;
    }
    flags &= ~0x00080000;
    move_target_update_timer = 0xf;
    if (!order_queue_begin || orders_dat_unknown7[order_queue_begin->order_id])
        flingy_flags &= ~0x20;
    else
        flingy_flags |= 0x20;
    return true;
}

bool Unit::IsKnownHallucination() const
{
    if (IsReplay() || *bw::local_player_id == player)
        return flags & UnitStatus::Hallucination;
    return false;
}

const Point &Unit::GetPosition() const
{
    return sprite->position;
}

Rect16 Unit::GetCollisionRect() const
{
    const Rect16 &dbox = units_dat_dimensionbox[unit_id];
    const Point &pos = GetPosition();
    return Rect16(pos.x - dbox.left, pos.y - dbox.top, pos.x + dbox.right + 1, pos.y + dbox.bottom + 1);
}

int Unit::GetRegion() const
{
    return ::GetRegion(sprite->position);
}

bool Unit::IsUpgrading() const
{
    return building.upgrade != Upgrade::None;
}

bool Unit::IsBuildingAddon() const
{
    return secondary_order == Order::BuildAddon && flags & UnitStatus::Building &&
        currently_building != nullptr && ~currently_building->flags & UnitStatus::Completed;
}

bool Unit::TempFlagCheck()
{
    if (unused52 & 0x2)
        return false;
    unused52 |= 0x2;
    temp_flagged.push_back(this);
    return true;
}

void Unit::ClearTempFlags()
{
    for (Unit *unit : temp_flagged)
        unit->unused52 &= ~2;
    temp_flagged.clear();
}

const char *Unit::GetName() const
{
    return (*bw::stat_txt_tbl)->GetTblString(unit_id + 1);
}

std::string Unit::DebugStr() const
{
    char buf[256];
    snprintf(buf, sizeof buf, "%s (id %x)", GetName(), unit_id);
    return std::string(buf);
}

void Unit::DoNextQueuedOrder()
{
    Order *next = order_queue_begin;
    if (ai && next && flags & UnitStatus::Burrowed && next->order_id != Order::Die && next->order_id != Order::Burrowed)
    {
        if (next->order_id != Order::Unburrow && next->order_id != Order::ComputerAi && unit_id != SpiderMine)
        {
            if (unit_id != Lurker || (next->order_id != Order::Guard && next->order_id != Order::AttackFixedRange))
            {
                InsertOrderTargetingGround(sprite->position.x, sprite->position.y, this, Order::Unburrow, next);
                flags &= ~UnitStatus::UninterruptableOrder;
                OrderDone();
                return;
            }
        }
    }
    if (!next)
        return;
    if (flags & (UnitStatus::UninterruptableOrder | UnitStatus::Nobrkcodestart) && next->order_id != Order::Die)
        return;

    order_flags &= ~0x1;
    move_target_update_timer = 0;
    order_wait = 0;
    if (orders_dat_interruptable[next->order_id] == 0)
        flags |= UnitStatus::UninterruptableOrder;
    flags &= ~(UnitStatus::CanSwitchTarget | UnitStatus::Disabled2);

    order = next->order_id;
    order_state = 0;
    target = next->target;
    auto next_order_pos = next->position;
    if (!target)
    {
        order_target_pos = next->position;
        order_fow_unit = next->fow_unit;
    }
    else
    {
        order_target_pos = target->sprite->position;
        order_fow_unit = None;
    }
    DeleteOrder(next);
    if (!ai)
        previous_attacker = nullptr;
    IscriptToIdle();
    if (HasSubunit())
    {
        int subunit_order;
        if (order == units_dat_return_to_idle_order[unit_id])
            subunit_order = units_dat_return_to_idle_order[subunit->unit_id];
        else if (order == units_dat_attack_unit_order[unit_id])
            subunit_order = units_dat_attack_unit_order[subunit->unit_id];
        else if (order == units_dat_attack_move_order[unit_id])
            subunit_order = units_dat_attack_move_order[subunit->unit_id];
        else if (order == units_dat_attack_move_order[unit_id])
            subunit_order = units_dat_attack_move_order[subunit->unit_id];
        else if (orders_dat_subunit_inheritance[order])
            subunit_order = order;
        else
            return;
        if (target)
            IssueOrder(subunit, subunit_order, next_order_pos.AsDword(), target);
        else
            IssueOrderTargetingGround(subunit, subunit_order, order_fow_unit, next_order_pos.x, next_order_pos.y);
    }
}

void Unit::ForceOrderDone()
{
    flags &= ~UnitStatus::UninterruptableOrder;
    OrderDone();
}

bool Unit::CanLocalPlayerControl() const
{
    if (IsReplay())
        return false;
    return player == *bw::local_player_id;
}

bool Unit::CanLocalPlayerSelect() const
{
    int visions = *bw::player_visions;
    if (IsReplay())
    {
        if (*bw::replay_show_whole_map)
            return true;
        visions = *bw::replay_visions;
    }
    if ((sprite->visibility_mask & visions) == 0)
        return false;
    if (IsInvisible() && (detection_status & visions) == 0)
        return false;
    return true;
}

int Unit::GetModifiedDamage(int dmg) const
{
    if (flags & UnitStatus::Hallucination)
        dmg *= 2;
    dmg += acid_spore_count * 256;
    if (dmg < 128)
        dmg = 128;
    return dmg;
}

void Unit::ShowShieldHitOverlay(int direction)
{
    Image *img = sprite->main_image;
    direction = ((direction - 0x7c) >> 3) & 0x1f;
    int8_t *shield_los = images_dat_shield_overlay[img->image_id];
    shield_los = shield_los + *(uint32_t *)(shield_los + 8 + img->direction * 4) + direction * 2; // sigh
    UnitIscriptContext ctx(this, nullptr, "ShowShieldHitOverlay", MainRng(), false);
    sprite->AddOverlayAboveMain(&ctx, Image::ShieldOverlay, shield_los[0], shield_los[1], direction);
}

void Unit::DamageShields(int32_t dmg, int direction)
{
    Assert(dmg <= shields);
    shields -= dmg;
    if (shields != 0)
        ShowShieldHitOverlay(direction);
}

void Unit::DamageSelf(int dmg, ProgressUnitResults *results)
{
    if (hitpoints != 0)
    {
        vector<Unit *> killed_units;
        // Ignore return value as there's no attacker
        DamageUnit(dmg, this, &killed_units);
        for (Unit *unit : killed_units)
            unit->Kill(results);
    }
}

void Unit::Order_SapUnit(ProgressUnitResults *results)
{
    if (!target)
        OrderDone();
    else if (!CanAttackUnit(target, true))
        IssueOrderTargetingGround(this, Order::Move, target->sprite->position.x, target->sprite->position.y);
    else
    {
        switch (order_state)
        {
            case 0:
                if (!ChangeMovementTargetToUnit(target))
                    return;
                order_state = 1;
                // Fall through
            case 1:
            {
                if (IsInArea(this, 4, target) || IsStandingStill() == 2)
                {
                    StopMoving(this);
                    int radius = weapons_dat_outer_splash[Weapon::Suicide];
                    if (!IsInArea(this, radius, target))
                    {
                        // What???
                        OrderDone();
                        return;
                    }
                }
                else if (IsInArea(this, 0x100, target) && target->flingy_flags & 2)
                {
                    MoveToCollide(this, target);
                    return;
                }
                else
                {
                    MoveTowards(this, target);
                    return;
                }
                target = this;
                SetIscriptAnimation(Iscript::Animation::Special1, true, "Order_SapLocation", results);
                flags |= UnitStatus::UninterruptableOrder;
                order_state = 2;
            } // Fall through
            case 2:
            if (order_signal & 0x1)
            {
                flags |= UnitStatus::SelfDestructing;
                Kill(results);
            }
        }
    }
}

void Unit::Order_SapLocation(ProgressUnitResults *results)
{
    switch (order_state)
    {
        case 0:
            ChangeMovementTarget(order_target_pos);
            unk_move_waypoint = order_target_pos;
            order_state = 1;
            // Fall through
        case 1:
        switch (IsStandingStill())
        {
            case 0:
                return;
            case 1:
            break;
            case 2:
            {
                int area = weapons_dat_outer_splash[Weapon::Suicide];
                if (Distance(exact_position, Point32(order_target_pos) * 256) / 256 > area)
                {
                    // What???
                    OrderDone();
                    return;
                }
            }
            break;
        }
        {
            target = this;
            SetIscriptAnimation(Iscript::Animation::Special1, true, "Order_SapLocation", results);
            flags |= UnitStatus::UninterruptableOrder;
            order_state = 2;
        }
            // Fall through
        case 2:
        if (order_signal & 0x1)
        {
            flags |= UnitStatus::SelfDestructing;
            Kill(results);
        }
    }
}

bool Unit::Ai_ShouldStopChasing(const Unit *other) const
{
    if (IsComputerPlayer(player) && !IsFlying())
    {
        if (other->flags & UnitStatus::Unk80 && !IsInAttackRange(other))
            return true;
    }
    return false;
}

void Unit::PickNewAttackTargetIfNeeded()
{
    if (~flags & UnitStatus::CanSwitchTarget)
        return;
    Unit *picked_target = target;
    int picked_threat_level = INT_MAX;
    if (target != nullptr)
    {
        if (ai != nullptr && !IsEnemy(target))
            return;
        if (!CanAttackUnit(target) || Ai_ShouldStopChasing(target))
            picked_target = nullptr;
        else
        {
            picked_threat_level = GetThreatLevel(this, target);
            if (picked_threat_level == 0)
            {
                if (IsInAttackRange(target))
                    return; // Why?
                picked_threat_level = 1;
            }
        }
    }
    if (previous_attacker)
    {
        if (!CanAttackUnit(previous_attacker) || Ai_ShouldStopChasing(previous_attacker) || !IsEnemy(previous_attacker))
            previous_attacker = nullptr;
        else
        {
            int threat_level = GetThreatLevel(this, previous_attacker);
            if (threat_level == 0 && !IsInAttackRange(previous_attacker))
                threat_level = 1;
            if (threat_level < picked_threat_level)
            {
                picked_threat_level = threat_level;
                picked_target = previous_attacker;
            }
        }
    }
    if (picked_target == nullptr || picked_threat_level != 0)
    {
        if (ai != nullptr)
        {
            int picked = Ai::UpdateAttackTarget(this, false, true, false);
            if (picked)
                return;
        }
        Unit *auto_target = GetAutoTarget();
        if (auto_target != nullptr)
        {
            int threat_level = GetThreatLevel(this, auto_target);
            if (threat_level < picked_threat_level)
                picked_target = auto_target;
        }
    }
    if (picked_target != nullptr && CanAttackUnit(picked_target))
    {
        target = picked_target;
        if (subunit != nullptr)
        {
            auto subunit_attack_unit_order = units_dat_attack_unit_order[subunit->unit_id];
            if (subunit->order == subunit_attack_unit_order || subunit->order != Order::HoldPosition)
                subunit->target = target;
        }
    }
}

void Unit::Order_AttackUnit(ProgressUnitResults *results)
{
    if (IsComputerPlayer(player))
    {
        if (!target || ~units_dat_flags[target->unit_id] & UnitFlags::Building)
        {
            if (Ai_TryReturnHome(false))
                return;
        }
    }
    PickNewAttackTargetIfNeeded();
    // PickNewAttackTargetIfNeeded may remove target if it cannot be attacked
    // Order_AttackUnit_Main is supposed to be able to handle that
    Assert(target == nullptr || !target->IsDying());
    if (target == nullptr || !CanAttackUnit(target)) // Target not ok
    {
        if (target == nullptr || flags & UnitStatus::CanSwitchTarget)
        {
            StopMoving(this);
        }
        else if (ai == nullptr)
        {
            PrependOrderTargetingGround(this, Order::Move, target->sprite->position.x, target->sprite->position.y);
        }
        OrderDone();
        return;
    }
    else // Target still ok
    {
        unk_move_waypoint = target->sprite->position;
        switch (order_state)
        {
            case 0:
                if (IsOutOfRange(this, target))
                {
                    if (!ChangeMovementTargetToUnit(target))
                        return;
                }
                else if (flingy_dat_movement_type[flingy_id] == 2)
                {
                    // This causes different behaviour from bw, prevents some stuck units
                    // But only units using iscript movement should be stuck so do this only for them
                    // See Test_Attack for more details
                    // So stop is done if state == 2 and out of range
                    order_state = 1;
                }
                // State == 1 if out of range, 2 if in range
                order_state += 1;
                if (ai != nullptr)
                    Ai_StimIfNeeded(this);
                DoAttack(results, Iscript::Animation::GndAttkRpt);
            break;
            // Have multiple states to prevent units from getting stuck, kind of messy
            // State 1 = Out of range started, 2 = Inside range started - may need to stop, 3 = Continuing
            case 1: case 2: case 3:
            {
                int old_state = order_state;
                order_state = 3;
                if (IsOutOfRange(this, target))
                {
                    if (old_state == 2) {
                        StopMoving(this);
                    }
                    int weapon = units_dat_ground_weapon[unit_id];
                    bool melee = unit_id == Scourge || (weapon != Weapon::None && weapons_dat_flingy[weapon] == 0);
                    if (ai == nullptr)
                    {
                        if (~flags & UnitStatus::CanSwitchTarget || IsFlying() ||
                            flags & UnitStatus::Disabled2 || !target->IsFlying())
                        {
                            if (IsStandingStill() != 2)
                            {
                                if (melee)
                                    MoveForMeleeRange(this, target);
                                else
                                    MoveTowards(this, target);
                                return;
                            }
                        }
                        StopMoving(this);
                        unk_move_waypoint = move_target;
                        OrderDone();
                    }
                    else
                    {
                        if (Ai_IsUnreachable(this, target) || IsStandingStill() == 2)
                        {
                            if (!Ai::UpdateAttackTarget(this, false, true, false))
                            {
                                StopMoving(this);
                                unk_move_waypoint = move_target;
                                OrderDone();
                            }
                        }
                        else
                        {
                            if (melee)
                                MoveForMeleeRange(this, target);
                            else
                                MoveTowards(this, target);
                        }
                    }
                    return;
                }
                else
                {
                    if (unit_id == Scourge)
                        MoveForMeleeRange(this, target);
                    else
                        StopMoving(this);
                    if (ai != nullptr)
                        Ai_StimIfNeeded(this);
                    DoAttack(results, Iscript::Animation::GndAttkRpt);
                }
            }
            break;
        }
    }
}

void Unit::DoAttack(ProgressUnitResults *results, int iscript_anim)
{
    *bw::last_bullet_spawner = nullptr;
    if (flags & UnitStatus::UnderDweb)
        return;
    if (target && target->IsFlying())
        DoAttack_Main(GetAirWeapon(), iscript_anim + 1, true, results);
    else
        DoAttack_Main(GetGroundWeapon(), iscript_anim, false, results);
}

void Unit::DoAttack_Main(int weapon, int iscript_anim, bool air, ProgressUnitResults *results)
{
    if (weapon == Weapon::None)
        return;
    uint8_t &cooldown = air ? air_cooldown : ground_cooldown;
    if (cooldown)
    {
        order_wait = std::min(order_wait, cooldown);
        return;
    }
    if (!IsReadyToAttack(this, weapon))
        return;
    flingy_flags |= 0x8;
    cooldown = GetCooldown(weapon) + MainRng()->Rand(3) - 1;
    SetIscriptAnimation(iscript_anim, true, "DoAttack_Main", results);
}

void Unit::AttackMelee(int sound_amt, uint16_t *sounds, ProgressUnitResults *results)
{
    if (target)
    {
        if (flags & UnitStatus::Hallucination)
        {
            results->hallucination_hits.emplace_back(this, target, facing_direction);
        }
        else
        {
            auto weapon = GetGroundWeapon();
            auto damage = GetWeaponDamage(target, weapon, player);
            results->weapon_damages.emplace_back(this, player, target, damage, weapon, facing_direction);
        }
    }
    int sound = sounds[MainRng()->Rand(sound_amt)];
    PlaySoundAtPos(sound, sprite->position.AsDword(), 1, 0);
}

void Unit::Order_DroneMutate(ProgressUnitResults *results)
{
    if (sprite->position == order_target_pos)
    {
        int building = build_queue[current_build_slot];
        int x_tile = (sprite->position.x - (units_dat_placement_box[building][0] / 2)) / 32;
        int y_tile = (sprite->position.y - (units_dat_placement_box[building][1] / 2)) / 32;
        if (UpdateBuildingPlacementState(this, player, x_tile, y_tile, building, 0, false, true, true) == 0)
        {
            bw::player_build_minecost[player] = units_dat_mine_cost[building];
            bw::player_build_gascost[player] = units_dat_gas_cost[building];
            if (CheckSupplyForBuilding(player, building, 1) != 0)
            {
                if (bw::minerals[player] < bw::player_build_minecost[player])
                    ShowInfoMessage(String::NotEnoughMinerals, Sound::NotEnoughMinerals + *bw::player_race, player);
                else if (bw::gas[player] < bw::player_build_gascost[player])
                    ShowInfoMessage(String::NotEnoughGas, Sound::NotEnoughGas + *bw::player_race, player);
                else // All checks succeeded
                {
                    ReduceBuildResources(player);
                    Unit *powerup = worker.powerup;
                    if (powerup)
                        DropPowerup(this);
                    if (building == Extractor)
                        MutateExtractor(results);
                    else
                        MutateBuilding(this, building);
                    if (powerup)
                        MoveUnit(powerup, powerup->powerup.origin_point.x, powerup->powerup.origin_point.y);
                    return;
                }
            }
        }
    }
    // Any kind of error happened
    if (sprite->last_overlay->drawfunc == Image::Shadow)
        sprite->last_overlay->SetOffset(sprite->last_overlay->x_off, 7);
    PrepareDrawSprite(sprite.get()); // ?
    PrependOrderTargetingNothing(this, Order::ResetCollision1);
    DoNextQueuedOrder();
}

void Unit::MutateExtractor(ProgressUnitResults *results)
{
    Unit *extractor = BeginGasBuilding(Extractor, this);
    if (extractor)
    {
        InheritAi(this, extractor);
        order_flags |= 0x4; // Remove death
        Kill(results);
        StartZergBuilding(extractor);
        AddOverlayBelowMain(extractor->sprite.get(), Image::VespeneGeyserUnderlay, 0, 0, 0);
    }
    else
    {
        if (sprite->last_overlay->drawfunc == Image::Shadow)
        {
            sprite->last_overlay->SetOffset(sprite->last_overlay->x_off, 7);
            sprite->last_overlay->flags |= 0x4;
        }
        unk_move_waypoint = sprite->position;
        PrependOrderTargetingNothing(this, Order::ResetCollision1);
        DoNextQueuedOrder();
    }
}

void Unit::Order_HarvestMinerals(ProgressUnitResults *results)
{
    if (target && target->IsMineralField())
    {
        AddResetHarvestCollisionOrder(this);
        switch (order_state)
        {
            case 0:
            {
                if (!IsFacingMoveTarget((Flingy *)this))
                    return;

                auto x = bw::circle[facing_direction][0] * 20 / 256;
                auto y = bw::circle[facing_direction][1] * 20 / 256;
                order_target_pos = sprite->position + Point(x, y); // To get iscript useweapon spawn the bullet properly
                order_state = 4;
            } // Fall through
            case 4:
            {
                SetIscriptAnimation(Iscript::Animation::AlmostBuilt, true, "Order_HarvestMinerals", results);
                order_state = 5;
                order_timer = 75;
            }
            break;
            case 5:
            if (order_timer == 0)
            {
                SetIscriptAnimation(Iscript::Animation::GndAttkToIdle, true, "Order_HarvestMinerals", results);
                AcquireResource(target, results);
                FinishedMining(target, this);
                DeleteSpecificOrder(Order::Harvest3);
                if (carried_powerup_flags)
                    IssueOrderTargetingNothing(this, Order::ReturnMinerals);
                else
                    IssueOrderTargetingNothing(this, Order::MoveToMinerals); // Huh?
            }
            break;
        }
    }
    else
    {
        if (worker.is_carrying)
        {
            worker.is_carrying = 0;
            Unit *previous_harvested = harvester.previous_harvested;
            if (previous_harvested)
            {
                harvester.previous_harvested = nullptr;
                previous_harvested->resource.awaiting_workers--;
                if (LetNextUnitMine(previous_harvested) != 0)
                    BeginHarvest(this, previous_harvested);
            }
        }
        DeleteSpecificOrder(Order::Harvest3);
        order_flags |= 0x1;
        if (order != Order::Die)
        {
            while (order_queue_end)
            {
                Order *order = order_queue_end;
                if (orders_dat_interruptable[order->order_id] || order->order_id == Order::MoveToMinerals)
                    DeleteOrder(order);
                else
                    break;
            }
            AddOrder(this, Order::MoveToMinerals, nullptr, None, 0, nullptr);
            DoNextQueuedOrder();
        }
    }
}

void Unit::DeleteSpecificOrder(int order_id)
{
    for (Order *order : order_queue_begin)
    {
        if (order->order_id == order_id)
        {
            DeleteOrder(order);
            return;
        }
    }
}

void Unit::AcquireResource(Unit *resource, ProgressUnitResults *results)
{
    int image = 0;
    switch (resource->unit_id)
    {
        case Refinery:
            image = Image::TerranGasTank;
        break;
        case Extractor:
            image = Image::ZergGasSac;
        break;
        case Assimilator:
            image = Image::ProtossGasOrb;
        break;
        case MineralPatch1:
        case MineralPatch2:
        case MineralPatch3:
            image = Image::MineralChunk;
        break;
        default:
            Warning("Unit::AcquireResource called with non-resource resource (%x)", resource->unit_id);
            return;
        break;
    }
    int amount = resource->MineResource(results);
    if (amount < 8)
        image += 1;
    if (!amount)
        return;
    if (carried_powerup_flags & 0x3)
    {
        DeletePowerupImages(this);
        carried_powerup_flags = 0;
    }
    if (Ai_CanMineExtra(this))
        amount++;
    CreateResourceOverlay(amount, resource->IsMineralField(), this, image);
}

int Unit::MineResource(ProgressUnitResults *results)
{
    if (resource.resource_amount <= 8)
    {
        if (IsMineralField())
        {
            Kill(results);
            return resource.resource_amount;
        }
        else
        {
            resource.resource_amount = 0;
            return 2;
        }
    }
    else
    {
        resource.resource_amount -= 8;
        if (IsMineralField())
            UpdateMineralAmountAnimation(this);
        else if (resource.resource_amount < 8)
            ShowInfoMessage(String::GeyserDepleted, Sound::GeyserDepleted, player);
        return 8;
    }
}

// Does both archons
void Unit::Order_WarpingArchon(int merge_distance, int close_distance, int result_unit, ProgressUnitResults *results)
{
    if (!target || target->player != player || target->order != order || target->target != this)
    {
        StopMoving(this);
        unk_move_waypoint = move_target;
        OrderDone();
        return;
    }
    if (flags & UnitStatus::Collides && IsInArea(this, current_speed * 2 / 256, target))
    {
        flags &= ~UnitStatus::Collides;
        PrependOrderTargetingNothing(this, Order::ResetCollision1);
    }
    int distance = Distance(sprite->position, target->sprite->position);
    if (distance > merge_distance)
    {
        if (order_state != 0 && IsStandingStill() == 2)
        {
            StopMoving(this);
            unk_move_waypoint = move_target;
            OrderDone();
        }
        else
        {
            order_state = 1;
            if (distance <= close_distance)
            {
                int avg_x = ((int)sprite->position.x + target->sprite->position.x) / 2;
                int avg_y = ((int)sprite->position.y + target->sprite->position.y) / 2;
                if (move_target_update_timer == 0)
                    ChangeMovementTarget(Point(avg_x, avg_y));
            }
            else
            {
                if (move_target_update_timer == 0)
                    ChangeMovementTarget(target->sprite->position);
            }
        }
    }
    else
    {
        MergeArchonStats(this, target);
        bool was_permamently_cloaked = units_dat_flags[unit_id] & UnitFlags::PermamentlyCloaked;
        TransformUnit(this, result_unit);
        if (was_permamently_cloaked)
            EndInvisibility(this, Sound::Decloak);

        sprite->flags &= ~SpriteFlags::Nobrkcodestart;
        SetIscriptAnimation(Iscript::Animation::Special1, true, "Order_WarpingArchon", results);

        SetButtons(None);
        kills += target->kills;
        target->order_flags |= 0x4;
        target->Kill(results);
        flags &= ~UnitStatus::Reacts;
        DeletePath();
        movement_state = 0;
        if ((sprite->elevation >= 12) || !(pathing_flags & 1))
            pathing_flags &= ~1;

        IssueOrderTargetingNothing(this, Order::CompletingArchonSummon);
    }
}

void Unit::Order_SpiderMine(ProgressUnitResults *results)
{
    switch (order_state)
    {
        case 0:
            ground_cooldown = 60;
            order_state = 1;
            // Fall through
        case 1:
            if (ground_cooldown != 0)
                return;
            SetIscriptAnimation(Iscript::Animation::Burrow, true, "Order_SpiderMine (burrow)", results);
            flags |= UnitStatus::NoCollision;
            order_state = 2;
            // Fall through
        case 2:
            if (~order_signal & 0x4)
                return;
            order_signal &= ~0x4;
            if (~flags & UnitStatus::Hallucination)
            {
                Burrow_Generic(this);
                InstantCloak(this);
            }
            order_state = 3;
            // Fall through
        case 3:
        {
            Unit *victim = SpiderMine_FindTarget(this);
            if (!victim)
                return;
            target = victim;
            SetIscriptAnimation(Iscript::Animation::Unburrow, true, "Order_SpiderMine (unburrow)", results);
            sprite->flags &= ~SpriteFlags::Unk40;
            IssueSecondaryOrder(Order::Nothing);
            order_state = 4;
        } // Fall through
        case 4:
            if (~order_signal & 0x4)
                return;
            order_signal &= ~0x4;
            sprite->elevation = units_dat_elevation_level[unit_id];
            flags &= ~UnitStatus::NoCollision;
            if (!target)
                order_state = 1;
            else
            {
                MoveToCollide(this, target);
                order_state = 5;
            }
        break;
        case 5:
            Unburrow_Generic(this);
            if (!target || !IsInArea(this, 0x240, target))
            {
                StopMoving(this);
                order_state = 1;
                return;
            }

            MoveToCollide(this, target);
            if (!IsInArea(this, 30, target))
            {
                if (IsStandingStill() == 2)
                {
                    StopMoving(this);
                    order_state = 1;
                }
                return;
            }
            target = nullptr;
            order_target_pos = sprite->position;
            SetIscriptAnimation(Iscript::Animation::Special1, true, "Order_SpiderMine (explode)", results);
            order_state = 6;
            // Fall through
        case 6:
            if (~order_signal & 0x1)
                return;
            Kill(results);
        break;
    }
}

void Unit::Order_ScannerSweep(ProgressUnitResults *results)
{
    if (order_signal & 0x4)
    {
        order_signal &= ~0x4;
        Kill(results);
    }
}

void Unit::Order_Scarab(ProgressUnitResults *results)
{
    switch (order_state)
    {
        case 0:
            if (!death_timer || death_timer > 90)
                death_timer = 90;
            flags |= UnitStatus::NoCollision;
            order_timer = 7;
            if (target)
            {
                ChangeMovementTargetToUnit(target);
                order_state = 2;
            }
            else
            {
                if (ChangeMovementTarget(order_target_pos))
                    order_state = 2;
                unk_move_waypoint = order_target_pos;
            }
        break;
        case 2:
        if (order_timer == 0)
        {
            flags &= ~UnitStatus::NoCollision;
            order_state = 3;
        } // Fall through
        case 3:
        if (!target)
        {
            if (IsStandingStill())
            {
                order_target_pos = sprite->position;
                SetIscriptAnimation(Iscript::Animation::Special1, true, "Order_Scarab", results);
                order_state = 6;
            }
        }
        else
        {
            if (IsInArea(this, 0x100, target) && target->flingy_flags & 0x2)
                MoveToCollide(this, target);
            else
                MoveTowards(this, target);
            if (IsInArea(this, weapons_dat_inner_splash[units_dat_ground_weapon[unit_id]] / 2, target))
            {
                order_target_pos = sprite->position;
                SetIscriptAnimation(Iscript::Animation::Special1, true, "Order_Scarab", results);
                order_state = 6;
            }
        }
        break;
        case 6:
        if (order_signal & 0x1)
        {
            flags |= UnitStatus::SelfDestructing;
            Kill(results);
        }
        break;
    }
}

void Unit::Order_Larva(ProgressUnitResults *results)
{
    if (related && !IsInArea(this, 10, related))
    {
        ChangeMovementTargetToUnit(related);
        unk_move_waypoint = related->sprite->position;
    }
    if (IsStandingStill() == 0)
    {
        if (!move_target_unit || move_target_unit->sprite->position == move_target)
            return;
        auto &pos = sprite->position;
        if (~(*bw::map_tile_flags)[pos.y / 32 * *bw::map_width_tiles + pos.x / 32] & 0x00400000)
            return;
    }
    if (!related || move_target_unit != related)
    {
        int x_tile = sprite->position.x / 32;
        int y_tile = sprite->position.y / 32;
        if (~(*bw::map_tile_flags)[y_tile * *bw::map_width_tiles + x_tile] & 0x00400000)
        {
            Kill(results);
            return;
        }
    }
    auto seed = MainRng()->Rand(0x8000);
    Point new_pos = sprite->position;
    new_pos.x += seed & 8 ? 10 : -10;
    new_pos.y += seed & 0x80 ? 10 : -10;
    if (new_pos.x >= *bw::map_width || new_pos.y >= *bw::map_height)
        return;
    if (~(*bw::map_tile_flags)[new_pos.y / 32 * *bw::map_width_tiles + new_pos.x / 32] & 0x00400000)
        return;
    if (!IsGoodLarvaPosition(this, new_pos.x, new_pos.y))
    {
        if (IsGoodLarvaPosition(this, sprite->position.x, sprite->position.y))
            return;
        uint32_t default_x;
        uint32_t default_y;
        GetDefaultLarvaPosition(this, &default_x, &default_y);
        new_pos = Point(default_x, default_y);
    }
    ChangeMovementTarget(new_pos);
    unk_move_waypoint = new_pos;
}

void Unit::Order_ComputerAi(ProgressUnitResults *results)
{
    if (order_timer != 0)
        return;
    order_timer = 15;
    if (unit_id == Larva)
    {
        Order_Larva(results);
        if (!sprite || order == Order::Die)
            return;
    }
    else if (unit_id == Medic)
    {
        if (Ai_IsMilitaryAtRegionWithoutState0(this))
            Ai_ReturnToNearestBaseForced(this);
        else
            IssueOrderTargetingNothing(this, Order::Medic);
        return;
    }
    if (!ai)
    {
        IssueOrderTargetingNothing(this, units_dat_ai_idle_order[unit_id]);
        return;
    }
    if (Ai_UnitSpecific(this))
        return;
    switch (ai->type)
    {
        case 1:
            IssueOrderTargetingNothing(this, units_dat_ai_idle_order[unit_id]);
        break;
        case 2:
            Ai_WorkerAi(this);
        break;
        case 3:
            if (Ai_TryProgressSpendingQueue(this))
                order_timer = 45;
            else
                order_timer = 3;
        break;
        case 4:
            Ai_Military(this);
            order_timer = 3;
        break;
    }
    if (GetRace() == Race::Terran && units_dat_flags[unit_id] & UnitFlags::Mechanical && !Ai_IsInAttack(this, false))
    {
        if (GetMaxHitPoints() != (hitpoints + 255) / 256 && flags & UnitStatus::Reacts)
        {
           if (!plague_timer && !irradiate_timer && !IsWorker())
           {
               Unit *scv = Ai_FindNearestRepairer(this);
               if (scv)
                   IssueOrderTargetingUnit_Simple(this, Order::Follow, scv);
               return;
           }
        }
    }
    if (unit_id == SiegeTankTankMode)
        Ai_SiegeTank(this);
    else if (flags & UnitFlags::Burrowable)
        Ai_Burrower(this);
}

void Unit::Order_Interceptor(ProgressUnitResults *results)
{
    if (interceptor.parent && shields < GetMaxShields() * 256 / 2)
    {
        IssueOrderTargetingNothing(this, Order::InterceptorReturn);
        return;
    }
    if (Interceptor_Attack(this) == 0)
        return;
    switch (order_state)
    {
        case 0:
            if (!interceptor.parent)
            {
                Kill(results);
                return;
            }
            PlaySound(Sound::InterceptorLaunch, this, 1, 0);
            Interceptor_Move(this, 3, order_target_pos.AsDword());
            order_timer = 15;
            order_state = 1;
            // Fall through
        case 1:
            if (order_timer != 0)
                return;
            sprite->elevation += 1;
            order_state = 3;
            // Fall through
        case 3:
            if (target)
            {
                if (IsInAttackRange(target))
                {
                    DoAttack(results, Iscript::Animation::GndAttkRpt);
                }
                if (WillBeInAreaAtStop(target, 50))
                {
                    Interceptor_Move(this, 1, order_target_pos.AsDword());
                    order_state = 4;
                    return;
                }
            }
            ChangeMovementTarget(order_target_pos);
            unk_move_waypoint = order_target_pos;
        break;
        case 4:
            if (!IsPointInArea(this, 50, move_target.x, move_target.y))
                return;
            Interceptor_Move(this, 2, order_target_pos.AsDword());
            order_state = 5;
        break;
        case 5:
            if (!IsPointInArea(this, 50, move_target.x, move_target.y))
                return;
            Interceptor_Move(this, 2, order_target_pos.AsDword());
            order_state = 3;
        break;
    }

}

void Unit::Order_InterceptorReturn(ProgressUnitResults *results)
{
    Assert(unit_id == Interceptor);
    if (!interceptor.parent)
    {
        Kill(results);
        return;
    }
    int parent_distance = Distance(exact_position, Point32(interceptor.parent->sprite->position) * 256) >> 8;
    if (order_state == 0)
    {
        if (parent_distance < 0x3c)
        {
            order_state = 1;
            sprite->elevation = interceptor.parent->sprite->elevation - 2;
        }
    }
    if (parent_distance < 0xa)
    {
        LoadFighter(interceptor.parent, this);
        IssueOrderTargetingNothing(this, Order::Nothing);
        RefreshUi();
    }
    else
    {
        ChangeMovementTarget(interceptor.parent->sprite->position);
        unk_move_waypoint = interceptor.parent->sprite->position;
    }
}

bool Unit::AttackAtPoint(ProgressUnitResults *results)
{
    if (target == nullptr)
        return false;
    PickNewAttackTargetIfNeeded();
    // PickNewAttackTargetIfNeeded may remove target if it cannot be attacked
    if (target == nullptr || !CanSeeTarget(this))
        return false;
    if (!CanAttackUnit(target, true))
        return false;
    order_target_pos = target->sprite->position;
    if (HasSubunit())
        return true;
    bool ground = !target->IsFlying();
    int weapon, cooldown;
    if (!ground)
    {
        weapon = GetAirWeapon();
        cooldown = air_cooldown;
    }
    else
    {
        weapon = GetGroundWeapon();
        cooldown = ground_cooldown;
    }
    if (cooldown)
    {
        order_wait = std::min(cooldown - 1, (int)order_wait);
        return true;
    }
    if (flingy_flags & 0x8)
        return true;
    int dist = GetDistanceToUnit(target);
    int min_range = weapons_dat_min_range[weapon];
    if ((min_range && min_range > dist) || GetWeaponRange(ground) < dist)
        return false;
    int target_dir = GetFacingDirection(sprite->position.x, sprite->position.y, order_target_pos.x, order_target_pos.y);
    int angle_diff = target_dir - facing_direction;
    if (angle_diff < 0)
        angle_diff += 256;
    if (angle_diff > 128)
        angle_diff = 256 - angle_diff;
    if (weapons_dat_attack_angle[weapon] < angle_diff)
    {
        if (flags & UnitStatus::FullAutoAttack)
        {
            order_timer = 0;
            return true;
        }
        else
        {
            return false;
        }
    }
    if (flags & UnitStatus::UnderDweb)
        return false;
    if (flags & UnitStatus::InBuilding)
        CreateBunkerShootOverlay(this);
    flingy_flags |= 0x8;
    *bw::last_bullet_spawner = nullptr;
    ground_cooldown = air_cooldown = GetCooldown(weapon) + MainRng()->Rand(3) - 1;
    int anim = ground ? Iscript::Animation::GndAttkRpt : Iscript::Animation::AirAttkRpt;
    SetIscriptAnimation(anim, true, "AttackAtPoint", results);
    return true;
}

void Unit::Order_HoldPosition(ProgressUnitResults *results)
{
    switch (order_state)
    {
        case 0:
            StopMoving(this);
            unk_move_waypoint = move_target;
            AllowSwitchingTarget();
            order_state = 1;
            // No break
        case 1:
            if (AttackAtPoint(results) == false)
            {
                if (order_timer == 0)
                {
                    order_timer = 15;
                    target = PickReachableTarget(this);
                    if (!target)
                        order_wait = 0;
                }
            }
            else if (!HasSubunit() || IsGoliath())
                unk_move_waypoint = order_target_pos;
    }
}

void Unit::Order_Hallucinated(ProgressUnitResults *results)
{
    if (matrix_hp || ensnare_timer || lockdown_timer || irradiate_timer || stasis_timer ||
            plague_timer || is_under_storm || stim_timer /* Yes */ || parasites ||
            blind || mael_timer)
    {
        Kill(results);
    }
}

Unit **FindNearbyHelpingUnits(Unit *unit, TempMemoryPool *allocation_pool)
{
    Rect16 area;
    if (!unit->ai)
    {
        area = Rect16(unit->sprite->position, CallFriends_Radius);
    }
    else
    {
        int search_radius = CallFriends_Radius;
        if (units_dat_flags[unit->unit_id] & UnitFlags::Building)
            search_radius *= 2;
        if (bw::player_ai[unit->player].flags & 0x20)
            search_radius *= 2;
        area = Rect16(unit->sprite->position, search_radius);
    }
    return unit_search->FindHelpingUnits(unit, area, allocation_pool);
}

void FindNearbyHelpingUnits_Threaded(ScThreadVars *tvars, Unit *unit)
{
    // Main thread didn't want to wait
    if (unit->nearby_helping_units.load(std::memory_order_relaxed) != nullptr)
        return;
    Unit **null = nullptr;
    // It is also possible that main thread took this one while the search was going on,
    // in which case we just discard the result
    unit->nearby_helping_units.compare_exchange_strong(null, FindNearbyHelpingUnits(unit, &tvars->unit_search_pool), std::memory_order_release, std::memory_order_relaxed);
}

void Unit::StartHelperSearch()
{
    // Optimization for neutral etc units
    // There could be cleaner way to have nearby_helping_units be empty array though <.<
    static Unit *null = nullptr;
    Assert(bulletframes_in_progress);
    if (~hotkey_groups & 0x80000000)
    {
        hotkey_groups |= 0x80000000;
        if (ai || HasEnemies(player))
        {
            nearby_helping_units.store(nullptr, std::memory_order_relaxed);
            threads->AddTask(&FindNearbyHelpingUnits_Threaded, this);
        }
        else
            nearby_helping_units.store(&null, std::memory_order_relaxed);
    }
}

void Unit::Order_PlayerGuard()
{
    if (AttackRecentAttacker(this) != 0)
        return;
    if (order_timer != 0)
        return;
    order_timer = 15;
    // Ever true?
    if (units_dat_flags[unit_id] & UnitFlags::Subunit)
        unk_move_waypoint = subunit->unk_move_waypoint;
    if (GetTargetAcquisitionRange() != 0)
    {
        Unit *auto_target = GetAutoTarget();
        if (auto_target != nullptr)
            AttackUnit(this, auto_target, true, false);
    }
}

void Unit::Order_Land(ProgressUnitResults *results)
{
    if (flags & UnitStatus::Building) // Is already landed
    {
        order_flags |= 0x1;
        if (order != Order::Die)
        {
            while (order_queue_end)
            {
                Order *order = order_queue_end;
                if (orders_dat_interruptable[order->order_id] || order->order_id == Order::LiftOff)
                    DeleteOrder(order);
                else
                    break;
            }
            AddOrder(this, Order::LiftOff, nullptr, None, 0, nullptr);
            DoNextQueuedOrder();
        }
        if (order != Order::Die)
        {
            // Why it deletes only if last is land ;_;
            while (order_queue_end)
            {
                Order *order = order_queue_end;
                if (order->order_id == Order::Land)
                    DeleteOrder(order);
                else
                    break;
            }
            AddOrder(this, Order::Land, nullptr, None, order_target_pos.AsDword(), target);
            AppendOrder(this, units_dat_return_to_idle_order[unit_id], 0, 0, Unit::None, 0);
        }
        return;
    }
    switch (order_state)
    {
        case 0:
        {
            Point new_pos = order_target_pos - Point(0, sprite->last_overlay->y_off);
            ChangeMovementTarget(new_pos);
            unk_move_waypoint = new_pos;
            order_state = 1;
        }
        break;
        case 1:
        {
            if (IsStandingStill() == 0)
                return;
            xuint x_tile = (order_target_pos.x - units_dat_placement_box[unit_id][0] / 2) / 32;
            yuint y_tile = (order_target_pos.y - units_dat_placement_box[unit_id][1] / 2) / 32;
            int result = UpdateBuildingPlacementState(this, player, x_tile, y_tile, unit_id, 0, false, true, false);
            if (result != 0)
            {
                ShowLandingError(this);
                if (order_queue_begin != nullptr && order_queue_begin->order_id == Order::PlaceAddon)
                    DeleteOrder(order_queue_end);
                OrderDone();
            }
            else
            {
                SetBuildingTileFlag(this, order_target_pos.x, order_target_pos.y);
                building.is_landing = 1;
                ChangeMovementTarget(order_target_pos);
                unk_move_waypoint = order_target_pos;
                if (sprite->last_overlay->drawfunc == Image::Shadow)
                    sprite->last_overlay->FreezeY();
                flags |= UnitStatus::UninterruptableOrder;
                flingy_top_speed = 256;
                SetSpeed_Iscript(this, 256);
                order_state = 2;
            }
        }
        break;
        case 2:
            if (position != unk_move_waypoint && GetFacingDirection(sprite->position.x, sprite->position.y, unk_move_waypoint.x, unk_move_waypoint.y) != movement_direction)
                return;
            SetIscriptAnimation(Iscript::Animation::Landing, true, "Order_Land", results);
            order_state = 3;
            // No break
        case 3:
        {
            if (IsStandingStill() == 0 || ~order_signal & 0x10)
                return;
            order_signal &= ~0x10;
            ClearBuildingTileFlag(this, order_target_pos.x, order_target_pos.y);
            RemoveFromMap(this);
            flags &= ~(UnitStatus::UninterruptableOrder | UnitStatus::Reacts | UnitStatus::Air);
            flags |= UnitStatus::Building;
            DeletePath();
            movement_state = 0;
            sprite->elevation = 4;
            pathing_flags |= 0x1;
            MoveUnit(this, order_target_pos.x, order_target_pos.y);
            FlyingBuilding_SwitchedState(this);
            // (There was leftover code from beta which removed landed buildings from multiselection)
            if (sprite->last_overlay->drawfunc == Image::Shadow)
            {
                sprite->last_overlay->SetOffset(sprite->last_overlay->x_off, 0);
                sprite->last_overlay->ThawY();
            }
            if (LoFile::GetOverlay(sprite->main_image->image_id, Overlay::Land).IsValid())
            {
                sprite->AddMultipleOverlaySprites(Overlay::Land, 8, Sprite::LandingDust1, 0, false);
                sprite->AddMultipleOverlaySprites(Overlay::Land, 8, Sprite::LandingDust1, 16, true);
            }
            building.is_landing = 0;
            unit_search->ForEachUnitInArea(GetCollisionRect(), [&](Unit *other) {
                if (other == this || IsEnemy(other))
                    return false;
                if (!other->CanMove() && other->HasWayOfAttacking())
                    other->Kill(results);
                else if (other->flags & UnitStatus::Burrowed)
                    other->Kill(results);
                return false;
            });
            if (order_queue_begin)
            {
                if (order_queue_begin->order_id == Order::Move || order_queue_begin->order_id == Order::Follow)
                    InsertOrderTargetingNothing(this, Order::LiftOff, order_queue_begin);
                else if (order_queue_begin->order_id != Order::PlaceAddon)
                {
                    while (order_queue_end != nullptr)
                        DeleteOrder(order_queue_end);
                    IssueOrderTargetingNothing(this, units_dat_return_to_idle_order[unit_id]);
                }
            }
            // Should never do anything
            FlyingBuilding_LiftIfStillBlocked(this);
            ForceOrderDone();
            Unit *addon = FindClaimableAddon(this);
            if (addon)
                AttachAddon(this, addon);
            if (*bw::is_placing_building)
            {
                EndAddonPlacement();
                if (!CanPlaceBuilding(*bw::primary_selected, *bw::placement_unit_id, *bw::placement_order))
                {
                    MarkPlacementBoxAreaDirty();
                    EndBuildingPlacement();
                }
            }
            RefreshUi();
        }
    }
}

void Unit::Order_SiegeMode(ProgressUnitResults *results)
{
    Assert(subunit != nullptr);
    switch (order_state)
    {
        case 0:
            if (unit_id != SiegeTankTankMode && unit_id != EdmundDukeT)
            {
                OrderDone();
                return;
            }
            if (flingy_flags & 0x2)
            {
                StopMoving(this);
                unk_move_waypoint = move_target;
            }
            order_state = 1;
            // No break
        case 1:
        {
            if (flingy_flags & 0x2 || subunit->flingy_flags & 0x2 || subunit->flags & UnitStatus::Nobrkcodestart)
                return;
            SetMoveTargetToNearbyPoint(units_dat_direction[unit_id], (Flingy *)this);
            IssueOrderTargetingUnit2(subunit, Order::Nothing3, this);
            SetMoveTargetToNearbyPoint(units_dat_direction[subunit->unit_id], (Flingy *)subunit);
            subunit->SetIscriptAnimation(Iscript::Animation::Special1, true, "Order_SiegeMode", results);
            bool killed = false;
            unit_search->ForEachUnitInArea(GetCollisionRect(), [&](Unit *other) {
                if (other->flags & UnitStatus::Building && ~other->flags & UnitStatus::NoCollision)
                {
                    Kill(results);
                    killed = true;
                    return true;
                }
                return false;
            });
            if (killed)
                return;
            order_state = 2;
            // No break
        }
        case 2:
        {
            if (!IsFacingMoveTarget((Flingy *)this) || !IsFacingMoveTarget((Flingy *)subunit))
                return;
            int sieged = unit_id == SiegeTankTankMode ? SiegeTank_Sieged : EdmundDukeS;
            TransformUnit(this, sieged);
            order_state = 3;
            // No break
        }
        case 3:
            if (~order_signal & 0x1)
                return;
            order_signal &= ~0x1;
            if (order_queue_begin && order_queue_begin->order_id != Order::WatchTarget)
            {
                IssueOrderTargetingNothing(this, units_dat_return_to_idle_order[unit_id]);
            }
            else if (order_queue_begin == nullptr)
            {
                AppendOrder(this, units_dat_return_to_idle_order[unit_id], 0, 0, Unit::None, 0);
            }
            ForceOrderDone();
            if (subunit->order_queue_begin == nullptr)
                AppendOrder(subunit, units_dat_return_to_idle_order[subunit->unit_id], 0, 0, Unit::None, 0);
            subunit->ForceOrderDone();
    }
}

void Unit::CancelTrain(ProgressUnitResults *results)
{
    for (int i = 0; i < 5; i++)
    {
        int build_unit = build_queue[(current_build_slot + i) % 5];
        if (build_unit != None)
        {
            if (i == 0 && currently_building != nullptr)
            {
                currently_building->CancelConstruction(results);
            }
            else if (~units_dat_flags[build_unit] & UnitFlags::Building)
            {
                RefundFullCost(build_unit, player);
            }
        }
    }
    for (int i = 0; i < 5; i++)
    {
        build_queue[i] = None;
    }
    current_build_slot = 0;
}

static void TransferUpgrade(int upgrade, int from_player, int to_player)
{
    if (GetUpgradeLevel(upgrade, from_player) > GetUpgradeLevel(upgrade, to_player))
    {
        SetUpgradeLevel(upgrade, to_player, GetUpgradeLevel(upgrade, from_player));
        int unit_id = MovementSpeedUpgradeUnit(upgrade);
        if (unit_id != Unit::None)
        {
            for (Unit *unit : bw::first_player_unit[to_player])
            {
                if (unit->unit_id == unit_id)
                {
                    unit->flags |= UnitStatus::SpeedUpgrade;
                    UpdateSpeed(unit);
                }
            }
        }
        unit_id = AttackSpeedUpgradeUnit(upgrade);
        if (unit_id != Unit::None)
        {
            for (Unit *unit : bw::first_player_unit[to_player])
            {
                if (unit->unit_id == unit_id)
                {
                    unit->flags |= UnitStatus::AttackSpeedUpgrade;
                    UpdateSpeed(unit);
                }
            }
        }
    }
}

static void TransferTech(int tech, int from_player, int to_player)
{
    if (GetTechLevel(tech, from_player) > GetTechLevel(tech, to_player))
    {
        SetTechLevel(tech, to_player, GetTechLevel(tech, from_player));
    }
}

void Unit::TransferTechsAndUpgrades(int new_player)
{
    switch (unit_id)
    {
        case Marine:
            TransferUpgrade(Upgrade::U_238Shells, player, new_player);
            TransferTech(Tech::Stimpacks, player, new_player);
        break;
        case Firebat:
            TransferTech(Tech::Stimpacks, player, new_player);
        break;
        case Ghost:
            TransferUpgrade(Upgrade::OcularImplants, player, new_player);
            TransferUpgrade(Upgrade::MoebiusReactor, player, new_player);
            TransferTech(Tech::Lockdown, player, new_player);
            TransferTech(Tech::PersonnelCloaking, player, new_player);
        break;
        case Medic:
            TransferUpgrade(Upgrade::CaduceusReactor, player, new_player);
            TransferTech(Tech::Healing, player, new_player);
            TransferTech(Tech::Restoration, player, new_player);
            TransferTech(Tech::OpticalFlare, player, new_player);
        break;
        case Vulture:
            TransferUpgrade(Upgrade::IonThrusters, player, new_player);
            TransferTech(Tech::SpiderMines, player, new_player);
        break;
        case SiegeTank_Sieged:
        case SiegeTankTankMode:
            TransferTech(Tech::SiegeMode, player, new_player);
        break;
        case Goliath:
            TransferUpgrade(Upgrade::CharonBooster, player, new_player);
        break;
        case Wraith:
            TransferUpgrade(Upgrade::ApolloReactor, player, new_player);
            TransferTech(Tech::CloakingField, player, new_player);
        break;
        case ScienceVessel:
            TransferUpgrade(Upgrade::TitanReactor, player, new_player);
            TransferTech(Tech::DefensiveMatrix, player, new_player);
            TransferTech(Tech::EmpShockwave, player, new_player);
            TransferTech(Tech::Irradiate, player, new_player);
        break;
        case Battlecruiser:
            TransferUpgrade(Upgrade::ColossusReactor, player, new_player);
            TransferTech(Tech::YamatoGun, player, new_player);
        break;
        case Overlord:
            TransferUpgrade(Upgrade::VentralSacs, player, new_player);
            TransferUpgrade(Upgrade::Antennae, player, new_player);
            TransferUpgrade(Upgrade::PneumatizedCarapace, player, new_player);
        break;
        case Drone:
        case InfestedTerran:
            TransferTech(Tech::Burrowing, player, new_player);
        break;
        case Zergling:
            TransferUpgrade(Upgrade::AdrenalGlands, player, new_player);
            TransferUpgrade(Upgrade::MetabolicBoost, player, new_player);
            TransferTech(Tech::Burrowing, player, new_player);
        break;
        case Hydralisk:
            TransferUpgrade(Upgrade::MuscularAugments, player, new_player);
            TransferUpgrade(Upgrade::GroovedSpines, player, new_player);
            TransferTech(Tech::Burrowing, player, new_player);
            TransferTech(Tech::LurkerAspect, player, new_player);
        break;
        case Lurker:
            TransferTech(Tech::LurkerAspect, player, new_player);
        break;
        case Ultralisk:
            TransferUpgrade(Upgrade::AnabolicSynthesis, player, new_player);
            TransferUpgrade(Upgrade::ChitinousPlating, player, new_player);
        break;
        case Queen:
            TransferUpgrade(Upgrade::GameteMeiosis, player, new_player);
            TransferTech(Tech::Infestation, player, new_player);
            TransferTech(Tech::Parasite, player, new_player);
            TransferTech(Tech::SpawnBroodlings, player, new_player);
            TransferTech(Tech::Ensnare, player, new_player);
        break;
        case Defiler:
            TransferUpgrade(Upgrade::MetasynapticNode, player, new_player);
            TransferTech(Tech::DarkSwarm, player, new_player);
            TransferTech(Tech::Plague, player, new_player);
            TransferTech(Tech::Consume, player, new_player);
            TransferTech(Tech::Burrowing, player, new_player);
        break;
        case Dragoon:
            TransferUpgrade(Upgrade::SingularityCharge, player, new_player);
        break;
        case Zealot:
            TransferUpgrade(Upgrade::LegEnhancements, player, new_player);
        break;
        case Reaver:
            TransferUpgrade(Upgrade::ScarabDamage, player, new_player);
            TransferUpgrade(Upgrade::ReaverCapacity, player, new_player);
        break;
        case Shuttle:
            TransferUpgrade(Upgrade::GraviticDrive, player, new_player);
        break;
        case Observer:
            TransferUpgrade(Upgrade::SensorArray, player, new_player);
            TransferUpgrade(Upgrade::GraviticBoosters, player, new_player);
        break;
        case HighTemplar:
            TransferUpgrade(Upgrade::KhaydarinAmulet, player, new_player);
            TransferTech(Tech::PsionicStorm, player, new_player);
            TransferTech(Tech::Hallucination, player, new_player);
            TransferTech(Tech::ArchonWarp, player, new_player);
        break;
        case DarkTemplar:
            TransferTech(Tech::DarkArchonMeld, player, new_player);
        break;
        case DarkArchon:
            TransferUpgrade(Upgrade::ArgusTalisman, player, new_player);
            TransferTech(Tech::Feedback, player, new_player);
            TransferTech(Tech::MindControl, player, new_player);
            TransferTech(Tech::Maelstrom, player, new_player);
        break;
        case Scout:
            TransferUpgrade(Upgrade::ApialSensors, player, new_player);
            TransferUpgrade(Upgrade::GraviticThrusters, player, new_player);
        break;
        case Carrier:
            TransferUpgrade(Upgrade::CarrierCapacity, player, new_player);
        break;
        case Arbiter:
            TransferUpgrade(Upgrade::KhaydarinCore, player, new_player);
            TransferTech(Tech::Recall, player, new_player);
            TransferTech(Tech::StasisField, player, new_player);
        break;
        case Corsair:
            TransferUpgrade(Upgrade::ArgusJewel, player, new_player);
            TransferTech(Tech::DisruptionWeb, player, new_player);
        break;
    }
}

void Unit::GiveTo(int new_player, ProgressUnitResults *results)
{
    TransferTechsAndUpgrades(new_player);
    RemoveFromSelections(this);
    RemoveFromClientSelection3(this);
    if (flags & UnitStatus::Building)
    {
        if (build_queue[current_build_slot % 5] < CommandCenter)
        {
            CancelTrain(results);
            SetIscriptAnimation(Iscript::Animation::WorkingToIdle, true, "Unit::GiveTo", results);
        }
        if (building.tech != Tech::None)
            CancelTech(this);
        if (building.upgrade != Upgrade::None)
            CancelUpgrade(this);
    }
    if (HasHangar())
        CancelTrain(results);
    GiveUnit(this, new_player, 1);
    if (IsActivePlayer(new_player))
        GiveSprite(this, new_player);
    if (IsBuildingAddon() || ~flags & UnitStatus::Completed || units_dat_flags[unit_id] & UnitFlags::SingleEntity)
        return;
    if (unit_id == Interceptor || unit_id == Scarab || unit_id == NuclearMissile)
        return;
    if (flags & UnitStatus::InTransport)
        return;
    switch (bw::players[player].type)
    {
        case 1:
            IssueOrderTargetingNothing(this, units_dat_ai_idle_order[unit_id]);
        break;
        case 3:
            IssueOrderTargetingNothing(this, Order::RescuePassive);
        break;
        case 7:
            IssueOrderTargetingNothing(this, Order::Neutral);
        break;
        default:
            IssueOrderTargetingNothing(this, units_dat_human_idle_order[unit_id]);
        break;
    }
}

void Unit::Trigger_GiveUnit(int new_player, ProgressUnitResults *results)
{
    if (new_player == 0xd)
        new_player = *bw::trigger_current_player;
    if (new_player >= Limits::Players || flags & UnitStatus::Hallucination || units_dat_flags[unit_id] & UnitFlags::Addon || new_player == player)
        return; // Bw would also SErrSetLastError if invalid player
    switch (unit_id)
    {
        case MineralPatch1:
        case MineralPatch2:
        case MineralPatch3:
        case VespeneGeyser:
        case Interceptor:
        case Scarab:
            return;
    }
    if (IsGasBuilding(unit_id) && flags & UnitStatus::Completed && player < Limits::Players)
    {
        for (Unit *unit : bw::first_player_unit[player])
        {
            if (unit->order == Order::HarvestGas && unit->IsWorker() && unit->sprite->IsHidden() && unit->target == this)
            {
                unit->GiveTo(new_player, results);
                break;
            }
        }
    }
    GiveTo(new_player, results);
    for (Unit *unit = first_loaded; unit != nullptr; unit = unit->next_loaded)
    {
        unit->GiveTo(new_player, results);
    }
    if (HasHangar())
    {
        for (Unit *unit : carrier.in_child)
            unit->GiveTo(new_player, results);
        for (Unit *unit : carrier.out_child)
            unit->GiveTo(new_player, results);
    }
    if (flags & UnitStatus::Building)
    {
        if (building.addon != nullptr)
        {
            building.addon->GiveTo(new_player, results);
        }
        if (IsBuildingAddon())
        {
            currently_building->GiveTo(new_player, results);
        }
        if (unit_id == NydusCanal && nydus.exit != nullptr)
        {
            nydus.exit->GiveTo(new_player, results);
        }
    }
}

void Unit::Order_Train(ProgressUnitResults *results)
{
    if (IsDisabled())
        return;
    // Some later added hackfix
    if (GetRace() == Race::Zerg && unit_id != InfestedCommandCenter)
        return;
    switch (secondary_order_state)
    {
        case 0:
        case 1:
        {
            int train_unit_id = build_queue[current_build_slot];
            if (train_unit_id == None)
            {
                IssueSecondaryOrder(Order::Nothing);
                SetIscriptAnimation(Iscript::Animation::WorkingToIdle, true, "Unit::Order_Train", results);
            }
            else
            {
                currently_building = BeginTrain(this, train_unit_id, secondary_order_state == 0);
                if (currently_building == nullptr)
                {
                    secondary_order_state = 1;
                }
                else
                {
                    SetIscriptAnimation(Iscript::Animation::Working, true, "Unit::Order_Train", results);
                    secondary_order_state = 2;
                }
            }
        }
        break;
        case 2:
            if (currently_building != nullptr)
            {
                int good  = ProgressBuild(currently_building, GetBuildHpGain(currently_building), 1);
                if (good && ~currently_building->flags & UnitStatus::Completed)
                    return;
                if (good)
                {
                    InheritAi2(this, currently_building);
                    if (currently_building->unit_id == NuclearMissile)
                        HideUnit(currently_building);
                    else
                        RallyUnit(this, currently_building);
                    if (ai && ai->type == 3)
                    {
                        Ai::BuildingAi *building = (Ai::BuildingAi *)ai;
                        building->train_queue_types[current_build_slot] = 0;
                        building->train_queue_values[current_build_slot] = 0;
                    }
                    build_queue[current_build_slot] = None;
                    current_build_slot = (current_build_slot + 1) % 5;
                }
                else if (build_queue[current_build_slot] != None)
                {
                    int train_unit_id = build_queue[current_build_slot];
                    if (currently_building != nullptr)
                        currently_building->CancelConstruction(results);
                    else if (~units_dat_flags[train_unit_id] & UnitFlags::Building)
                        RefundFullCost(train_unit_id, player);
                    int slot = current_build_slot;
                    for (int i = 0; i < 5; i++)
                    {
                        if (build_queue[slot] == None)
                            break;
                        int next_slot = (slot + 1) % 5;
                        build_queue[slot] = build_queue[next_slot];
                        if (ai && ai->type == 3)
                        {
                            Ai::BuildingAi *building = (Ai::BuildingAi *)ai;
                            building->train_queue_types[slot] = building->train_queue_types[next_slot];
                            building->train_queue_values[slot] = building->train_queue_values[next_slot];
                        }
                        slot = next_slot;
                    }
                }
            }
            RefreshUi();
            secondary_order_state = 0;
            currently_building = nullptr;
        break;
    }
}

void Unit::Order_ProtossBuildSelf(ProgressUnitResults *results)
{
    switch (order_state)
    {
        case 0:
            if (remaining_build_time == 0)
            {
                SetIscriptAnimation(Iscript::Animation::Special1, true, "Order_ProtossBuildSelf", results);
                PlaySound(Sound::ProtossBuildingFinishing, this, 1, 0);
                order_state = 1;
            }
            else
            {
                ProgressBuildingConstruction();
            }
        break;
        case 1:
            if (order_signal & 0x1)
            {
                order_signal &= ~0x1;
                ReplaceSprite(sprites_dat_image[sprite->sprite_id], 0, sprite.get());
                Image *image = sprite->main_image;
                // Bw actually has iscript header hardcoded as 193
                image->iscript.Initialize(*bw::iscript, images_dat_iscript_header[Image::WarpTexture]);
                UnitIscriptContext ctx(this, results, "Order_ProtossBuildSelf", MainRng(), false);
                image->SetIscriptAnimation(&ctx, Iscript::Animation::Init);
                image->iscript.Initialize(*bw::iscript, images_dat_iscript_header[image->image_id]);
                // Now the image is still executing the warp texture iscript, even though
                // any future SetIscriptAnimation() calls cause it to use original iscript.
                image->SetDrawFunc(Image::UseWarpTexture, image->drawfunc_param);
                // Why?
                image->iscript.ProgressFrame(&ctx, image);
                order_state = 2;
            }
        break;
        case 2:
            if (order_signal & 0x1)
            {
                order_signal &= ~0x1;
                // Wait, why again?
                ReplaceSprite(sprites_dat_image[sprite->sprite_id], 0, sprite.get());
                SetIscriptAnimation(Iscript::Animation::WarpIn, true, "Order_ProtossBuildSelf", results);
                order_state = 3;
            }
        break;
        case 3:
            if (order_signal & 0x1)
            {
                order_signal &= ~0x1;
                FinishUnit_Pre(this);
                FinishUnit(this);
                CheckUnstack(this);
                if (flags & UnitStatus::Disabled)
                {
                    SetIscriptAnimation(Iscript::Animation::Disable, true, "Order_ProtossBuildSelf", results);
                }
                // This heals a bit if the buidling was damaged but is otherwise pointless
                ProgressBuildingConstruction();
            }
        break;
    }
}

void Unit::ProgressBuildingConstruction()
{
    int build_speed = 1;
    if (IsCheatActive(Cheats::Operation_Cwal))
        build_speed = 10;
    remaining_build_time = std::max((int)remaining_build_time - build_speed, 0);
    SetHp(this, hitpoints + build_hp_gain * build_speed);
    shields = std::min(units_dat_shields[unit_id] * 256, shields + build_shield_gain * build_speed);
}

Entity *Unit::AsEntity()
{
    return (Entity *)this;
}

Iscript::CmdResult Unit::HandleIscriptCommand(UnitIscriptContext *ctx, Image *img,
                                              Iscript::Script *script, const Iscript::Command &cmd)
{
    using namespace Iscript::Opcode;
    using Iscript::CmdResult;

    CmdResult result = CmdResult::Handled;
    switch (cmd.opcode)
    {
        case SetVertPos:
            // Cloaked air units don't fly up and down..
            if (!IsInvisible())
                result = CmdResult::NotHandled;
        break;
        case SprUlUseLo:
        case SprUl:
            // Similar check like with SetVertPos.
            // (Spruluselo is a poorly named counterpart to sprul)
            if (!IsInvisible() || images_dat_draw_if_cloaked[cmd.val])
                result = CmdResult::NotHandled;
        break;
        case Move:
            // Note that in some cases bw tries to predict the speed,
            // the handling for that is in a hook
            SetSpeed_Iscript(this, CalculateSpeedChange(this, cmd.val * 256));
        break;
        case LiftoffCondJmp:
            if (IsFlying())
                script->pos = cmd.pos;
        break;
        case AttackWith:
            Iscript_AttackWith(this, cmd.val);
        break;
        case Iscript::Opcode::Attack:
            if (target == nullptr || target->IsFlying())
                Iscript_AttackWith(this, 0);
            else
                Iscript_AttackWith(this, 1);
        break;
        case CastSpell:
            if (orders_dat_targeting_weapon[order] != Weapon::None && !ShouldStopOrderedSpell(this))
                FireWeapon(this, orders_dat_targeting_weapon[order]);
        break;
        case UseWeapon:
            Iscript_UseWeapon(cmd.val, this);
        break;
        case GotoRepeatAttk:
            flingy_flags &= ~0x8;
        break;
        case NoBrkCodeStart:
            flags |= UnitStatus::Nobrkcodestart;
            sprite->flags |= 0x80;
        break;
        case NoBrkCodeEnd:
            flags &= ~UnitStatus::Nobrkcodestart;
            sprite->flags &= ~0x80;
            if (order_queue_begin != nullptr && order_flags & 0x1)
            {
                IscriptToIdle();
                DoNextQueuedOrder();
            }
        break;
        case IgnoreRest:
            if (target == nullptr)
                IscriptToIdle();
            else
            {
                script->wait = 10;
                script->pos -= cmd.Size(); // Loop on this cmd
                result = CmdResult::Stop;
            }
        break;
        case AttkShiftProj:
            // Sigh
            weapons_dat_x_offset[GetGroundWeapon()] = cmd.val;
            Iscript_AttackWith(this, 1);
        break;
        case CreateGasOverlays:
        {
            Image *gas_overlay = new Image;
            if (sprite->first_overlay == img)
            {
                Assert(img->list.prev == nullptr);
                sprite->first_overlay = gas_overlay;
            }
            gas_overlay->list.prev = img->list.prev;
            gas_overlay->list.next = img;
            if (img->list.prev != nullptr)
                img->list.prev->list.next = gas_overlay;
            img->list.prev = gas_overlay;
            int smoke_img = Image::VespeneSmokeOverlay1 + cmd.val;
            // Bw can be misused to have this check for loaded nuke and such
            // Even though resource_amount is word, it won't report incorrect
            // values as unit array starts from 0x0059CCA8
            // (The lower word is never 0 if the union contains unit)
            // But with dynamic allocation, that is not the case
            if (units_dat_flags[unit_id] & UnitFlags::ResourceContainer)
            {
                if (resource.resource_amount == 0)
                    smoke_img = Image::VespeneSmallSmoke1 + cmd.val;
            }
            else
            {
                if (silo.nuke == nullptr)
                    smoke_img = Image::VespeneSmallSmoke1 + cmd.val;
            }
            Point pos = LoFile::GetOverlay(img->image_id, Overlay::Special).GetValues(img, cmd.val).ToPoint16();
            InitializeImageFull(smoke_img, gas_overlay, pos.x + img->x_off, pos.y + img->y_off, sprite.get());
        }
        break;
        case Iscript::Opcode::AttackMelee:
            if (ctx->results == nullptr)
                result = CmdResult::NotHandled;
            else
                AttackMelee(cmd.data[0], (uint16_t *)(cmd.data + 1), ctx->results);
        break;
        default:
            result = CmdResult::NotHandled;
        break;
    }

    // Compilers are able to generate better code when the HandleIscriptCommand is not
    // called at multiple different switch cases
    if (result == CmdResult::NotHandled)
        result = AsEntity()->HandleIscriptCommand(ctx, img, script, cmd);
    return result;
}

void Unit::WarnUnhandledIscriptCommand(const Iscript::Command &cmd, const char *caller) const
{
    Warning("%s did not handle all iscript commands for unit %s, command %s",
        caller, DebugStr().c_str(), cmd.DebugStr().c_str());
}

void Unit::ProgressIscript(const char *caller, ProgressUnitResults *results)
{
    UnitIscriptContext ctx(this, results, caller, MainRng(), true);
    ctx.ProgressIscript();
    ctx.CheckDeleted(); // Safe? No idea, needs tests, but works with dying units
}

void Unit::SetIscriptAnimation(int anim, bool force, const char *caller, ProgressUnitResults *results)
{
    UnitIscriptContext(this, results, caller, MainRng(), false).SetIscriptAnimation(anim, force);
}

void Unit::SetIscriptAnimationForImage(Image *img, int anim)
{
    UnitIscriptContext ctx(this, nullptr, "SetIscriptAnimation hook", MainRng(), false);
    img->SetIscriptAnimation(&ctx, anim);
}

bool Unit::MoveFlingy()
{
    bool result = AsEntity()->flingy.Move();
    if (*bw::show_endwalk_anim)
        SetIscriptAnimation(Iscript::Animation::Idle, false, "MoveFlingy", nullptr);
    else if (*bw::show_startwalk_anim)
        SetIscriptAnimation(Iscript::Animation::Walking, true, "MoveFlingy", nullptr);
    return result;
}

void Unit::IscriptToIdle()
{
    flags &= ~UnitStatus::Nobrkcodestart;
    sprite->flags &= ~SpriteFlags::Nobrkcodestart;
    UnitIscriptContext(this, nullptr, "IscriptToIdle", MainRng(), false).IscriptToIdle();
    flingy_flags &= ~0x8;
}
