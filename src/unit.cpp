#include "unit.h"

#include <functional>
#include <algorithm>
#include <string.h>

#include "console/assert.h"

#include "constants/image.h"
#include "constants/order.h"
#include "constants/sprite.h"
#include "constants/tech.h"
#include "constants/upgrade.h"
#include "constants/weapon.h"

#include "ai.h"
#include "bullet.h"
#include "bunker.h"
#include "entity.h"
#include "flingy.h"
#include "image.h"
#include "lofile.h"
#include "log.h"
#include "offsets.h"
#include "order.h"
#include "pathing.h"
#include "perfclock.h"
#include "player.h"
#include "rng.h"
#include "scthread.h"
#include "selection.h"
#include "sound.h"
#include "sprite.h"
#include "strings.h"
#include "targeting.h"
#include "tech.h"
#include "text.h"
#include "upgrade.h"
#include "unit_cache.h"
#include "unitsearch.h"
#include "yms.h"
#include "warn.h"

using std::get;
using std::max;
using std::min;

EnemyUnitCache *enemy_unit_cache;

uint32_t Unit::next_id = 1;
DummyListHead<Unit, Unit::offset_of_allocated> first_allocated_unit;
DummyListHead<Unit, Unit::offset_of_allocated> first_movementstate_flyer;
vector<Unit *> Unit::temp_flagged;
Unit *Unit::id_lookup[UNIT_ID_LOOKUP_SIZE];

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

Unit::~Unit()
{
    if (Type() == UnitId::Pylon)
    {
        pylon.aura.~unique_ptr<Sprite>();
    }
}

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
        if (img->Type().DrawIfCloaked())
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
    secondary_order = OrderId::Fatal.Raw();
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

Unit *Unit::AllocateAndInit(uint8_t player, int unused_seed, uint16_t x, uint16_t y, uint16_t unit_id)
{
    if (!bw::DoesFitHere(unit_id, x, y))
    {
        *bw::error_message = 0;
        return nullptr;
    }
    Unit *unit = new Unit;
    auto success = bw::InitializeUnitBase(unit, unit_id, x, y, player);
    if (!success)
    {
        *bw::error_message = NetworkString::UnableToCreateUnit;
        return nullptr;
    }
    return unit;
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
    if (Type().Flags() & UnitFlags::CanMove)
        flags |= UnitStatus::Reacts;
    else
        flags &= ~UnitStatus::Reacts;

    DeletePath();
    movement_state = 0;
    if ((sprite->elevation >= 12) || !(pathing_flags & 1))
        pathing_flags &= ~1;
}

Unit *Unit::FindById(uint32_t id)
{
    Unit *next = id_lookup[id % UNIT_ID_LOOKUP_SIZE];
    while (next && next->lookup_id != id)
        next = next->lookup_list_next;
    return next;
}

void Unit::RemoveOverlayFromSelfOrSubunit(ImageType first_id, int id_amount)
{
    ImageType last_id(first_id.Raw() + id_amount);
    for (Image *img : sprite->first_overlay)
    {
        if (img->image_id >= first_id.Raw() && img->image_id <= last_id.Raw())
        {
            img->SingleDelete();
            return;
        }
    }
    if (subunit)
    {
        for (Image *img : subunit->sprite->first_overlay)
        {
            if (img->image_id >= first_id.Raw() && img->image_id <= last_id.Raw())
            {
                img->SingleDelete();
                return;
            }
        }
    }
}

void Unit::RemoveOverlayFromSelf(ImageType first_id, int id_amount)
{
    ImageType last_id(first_id.Raw() + id_amount);
    for (Image *img : sprite->first_overlay)
    {
        if (img->image_id >= first_id.Raw() && img->image_id <= last_id.Raw())
        {
            img->SingleDelete();
            return;
        }
    }
}

void Unit::AddSpellOverlay(ImageType small_overlay_id)
{
    bw::AddOverlayHighest(GetTurret()->sprite.get(), small_overlay_id.Raw() + Type().OverlaySize(), 0, 0, 0);
}

// Some ai orders use UpdateAttackTarget, which uses unitsearch region cache, so they are progressed later
void Unit::ProgressOrder_Late(ProgressUnitResults *results)
{
    STATIC_PERF_CLOCK(Unit_ProgressOrder_Late);
    switch (order)
    {
#define Case(s) case OrderId::s : bw::Order_ ## s(this); break
        Case(TurretAttack);
        Case(TurretGuard);
    }
    if (order_wait != OrderWait)
        return;

    switch (OrderType().Raw())
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
        case OrderId::ComputerReturn:
            Order_ComputerReturn();
        break;
        case OrderId::AiGuard:
            Order_AiGuard();
        break;
        case OrderId::PlayerGuard:
            Order_PlayerGuard();
        break;
        case OrderId::AttackUnit:
            Order_AttackUnit(results);
        break;
        case OrderId::ComputerAi:
            Order_ComputerAi(results);
        break;
        case OrderId::HoldPosition:
            Order_HoldPosition(results);
        break;
        case OrderId::Interceptor:
            Order_Interceptor(results);
        break;
#undef Case
    }
}

void Unit::ProgressOrder_Hidden(ProgressUnitResults *results)
{
    switch (OrderType().Raw())
    {
        case OrderId::Die:
            Order_Die(results);
            return;
        case OrderId::PlayerGuard: case OrderId::TurretGuard: case OrderId::TurretAttack: case OrderId::EnterTransport:
            if (flags & UnitStatus::InBuilding)
                IssueOrderTargetingNothing(OrderId::BunkerGuard);
            else
                IssueOrderTargetingNothing(OrderId::Nothing);
            return;
        case OrderId::HarvestGas:
            bw::Order_HarvestGas(this);
            return;
        case OrderId::NukeLaunch:
            Order_NukeLaunch(results);
            return;
        case OrderId::InfestMine4:
            bw::Order_InfestMine4(this);
            return;
        case OrderId::ResetCollision1:
            bw::Order_ResetCollision1(this);
            return;
        case OrderId::ResetCollision2:
            bw::Order_ResetCollision2(this);
            return;
        case OrderId::UnusedPowerup:
            bw::Order_UnusedPowerup(this);
            return;
        case OrderId::PowerupIdle:
            bw::Order_PowerupIdle(this);
            return;
    }
    if (order_wait--) // Ya postfix
        return;

    order_wait = OrderWait;
    switch (OrderType().Raw())
    {
        case OrderId::ComputerAi:
            if (flags & UnitStatus::InBuilding)
                bw::Order_BunkerGuard(this);
        break;
        case OrderId::BunkerGuard:
            bw::Order_BunkerGuard(this);
        break;
        case OrderId::Pickup4:
            bw::Order_Pickup4(this);
        break;
        case OrderId::RescuePassive:
            bw::Order_RescuePassive(this);
        break;
    }
}

void Unit::ProgressOrder(ProgressUnitResults *results)
{
    STATIC_PERF_CLOCK(Unit_ProgressOrder);
    switch (order)
    {
        case OrderId::ProtossBuildSelf:
            Order_ProtossBuildSelf(results);
            return;
        case OrderId::WarpIn:
            bw::Order_WarpIn(this);
            return;
        case OrderId::Die:
            Order_Die(results);
            return;
        case OrderId::NukeTrack:
            Order_NukeTrack();
            return;
    }
    if (IsDisabled())
    {
        bw::Ai_FocusUnit(this);
        return;
    }
    if (~flags & UnitStatus::Reacts && flags & UnitStatus::UnderDweb)
        bw::Ai_FocusUnit(this);
#define Case(s) case OrderId::s : bw::Order_ ## s(this); break
    switch (OrderType().Raw())
    {
        Case(InitArbiter);
        Case(LiftOff);
        Case(BuildTerran);
        Case(BuildProtoss1);
        Case(TerranBuildSelf);
        Case(ZergBuildSelf);
        Case(ConstructingBuilding);
        Case(Critter);
        Case(MineralHarvestInterrupted);
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
        case OrderId::SelfDestructing:
            Kill(results);
        break;
        case OrderId::DroneBuild:
            Order_DroneMutate(results);
        break;
        case OrderId::WarpingArchon:
            Order_WarpingArchon(2, 10, UnitId::Archon, results);
        break;
        case OrderId::WarpingDarkArchon:
            Order_WarpingArchon(19, 20, UnitId::DarkArchon, results);
        break;
        case OrderId::Scarab:
            Order_Scarab(results);
        break;
        case OrderId::Land:
            Order_Land(results);
        break;
        case OrderId::SiegeMode:
            Order_SiegeMode(results);
        break;
    }
    if (order_wait--) // Ya postfix
        return;

    order_wait = OrderWait;
    bw::Ai_FocusUnit2(this);
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
        case OrderId::Unload:
            Order_Unload();
        break;
        case OrderId::Recall:
            Order_Recall();
        break;
        case OrderId::QueenHoldPosition:
        case OrderId::SuicideHoldPosition:
            if (order_state == 0)
            {
                bw::StopMoving(this);
                unk_move_waypoint = move_target;
                order_state = 1;
            }
            if (order_queue_begin)
                DoNextQueuedOrder();
        break;
        case OrderId::Nothing:
            if (order_queue_begin)
                DoNextQueuedOrder();
        break;
        case OrderId::Harvest:
        case OrderId::MoveToGas:
            bw::Order_MoveToHarvest(this);
        break;
        case OrderId::ReturnGas:
        case OrderId::ReturnMinerals:
            bw::Order_ReturnResource(this);
        break;
        case OrderId::Move:
        case OrderId::ReaverCarrierMove:
            bw::Order_Move(this);
        break;
        case OrderId::MoveUnload:
            Order_MoveUnload();
        break;
        case OrderId::NukeGround:
            Order_NukeGround();
        break;
        case OrderId::NukeUnit:
            Order_NukeUnit();
        break;
        case OrderId::YamatoGun:
        case OrderId::Lockdown:
        case OrderId::Parasite:
        case OrderId::DarkSwarm:
        case OrderId::SpawnBroodlings:
        case OrderId::EmpShockwave:
        case OrderId::PsiStorm:
        case OrderId::Plague:
        case OrderId::Irradiate:
        case OrderId::Consume:
        case OrderId::StasisField:
        case OrderId::Ensnare:
        case OrderId::Restoration:
        case OrderId::DisruptionWeb:
        case OrderId::OpticalFlare:
        case OrderId::Maelstrom:
            bw::Order_Spell(this);
        break;
        case OrderId::AttackObscured:
        case OrderId::InfestObscured:
        case OrderId::RepairObscured:
        case OrderId::CarrierAttackObscured:
        case OrderId::ReaverAttackObscured:
        case OrderId::HarvestObscured:
        case OrderId::YamatoGunObscured:
            bw::Order_Obscured(this);
        break;
        case OrderId::Feedback:
            Order_Feedback(results);
        break;
        // These have ReleaseFighter
        case OrderId::Carrier:
        case OrderId::CarrierFight:
        case OrderId::CarrierHoldPosition:
            bw::Order_Carrier(this);
        break;
        case OrderId::Reaver:
        case OrderId::ReaverFight:
        case OrderId::ReaverHoldPosition:
            bw::Order_Reaver(this);
        break;
        case OrderId::Hallucination:
            Order_Hallucination(results);
        break;
        case OrderId::SapLocation:
            Order_SapLocation(results);
        break;
        case OrderId::SapUnit:
            Order_SapUnit(results);
        break;
        case OrderId::MiningMinerals:
            Order_HarvestMinerals(results);
        break;
        case OrderId::SpiderMine:
            Order_SpiderMine(results);
        break;
        case OrderId::ScannerSweep:
            Order_ScannerSweep(results);
        break;
        case OrderId::Larva:
            Order_Larva(results);
        break;
        case OrderId::NukeLaunch:
            Order_NukeLaunch(results);
        break;
        case OrderId::InterceptorReturn:
            Order_InterceptorReturn(results);
        break;
        case OrderId::MindControl:
            Order_MindControl(results);
        break;
    }
#undef Case
}

void Unit::ProgressSecondaryOrder(ProgressUnitResults *results)
{
    if (SecondaryOrderType() == OrderId::Hallucinated)
    {
        Order_Hallucinated(results);
        return;
    }
    if (IsDisabled())
        return;
#define Case(s) case OrderId::s : bw::Order_ ## s(this); break
    switch (SecondaryOrderType().Raw())
    {
        Case(BuildAddon);
        Case(TrainFighter);
        Case(ShieldBattery);
        Case(SpawningLarva);
        Case(SpreadCreep);
        Case(Cloak);
        Case(Decloak);
        Case(CloakNearbyUnits);
        case OrderId::Train:
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
    if (Type().HasShields())
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
    if ((Type() == UnitId::Zergling || Type() == UnitId::DevouringOne) && ground_cooldown == 0)
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
        if (Type().Flags() & UnitFlags::Regenerate && hitpoints > 0 && hitpoints != Type().HitPoints())
        {
            bw::SetHp(this, hitpoints + 4);
        }
        bw::ProgressEnergyRegen(this);
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
        if (Type().Race() == Race::Terran)
        {
            // Why not just check units.dat flags building <.<
            if (flags & UnitStatus::Building || Type().Flags() & UnitFlags::FlyingBuilding)
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
    if (!Type().IsSubunit() && !sprite->IsHidden())
    {
        if (player < Limits::Players)
            bw::DrawTransmissionSelectionCircle(sprite.get(), bw::self_alliance_colors[player]);
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
        bw::ProgressUnitMovement(this);
    if (!flyers && movement_state == MovementState::Flyer)
    {
        allocated.Change(first_movementstate_flyer);
        return;
    }

    if (*bw::reveal_unit_area || (flyers && *bw::vision_updated))
        bw::RevealSightArea(this);
    if (HasSubunit() && flags & UnitStatus::Completed)
    {
        int rotation = movement_direction - old_direction;
        if (rotation < 0)
            rotation += 0x100;

        bw::ProgressSubunitDirection(subunit, rotation);
        Point32 lo = LoFile::GetOverlay(sprite->main_image->Type(), Overlay::Special)
            .GetValues(sprite->main_image, 0);
        subunit->exact_position = exact_position;
        subunit->position = Point(exact_position.x >> 8, exact_position.y >> 8);
        bw::MoveSprite(subunit->sprite.get(), subunit->position.x, subunit->position.y);
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
    bw::ProgressUnitMovement(this); // Huh?
    ProgressTimers(results);
    ProgressOrder_Hidden(results);
    bw::ProgressSecondaryOrder_Hidden(this);
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
            if (bw::UpdateCreepDisappearance_Unit(this) == false)
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
            bw::EndInvisibility(unit, Sound::Decloak);
            refresh = true;
        }
        else
        {
            // Remove free cloak from ghosts which have walked out of arbiter range
            if (~unit->flags & UnitStatus::Burrowed && unit->SecondaryOrderType() == OrderId::Cloak)
            {
                if (unit->invisibility_effects == 1 && unit->flags & UnitStatus::FreeInvisibility)
                {
                    unit->flags &= ~UnitStatus::FreeInvisibility;
                    refresh = true;
                }
            }
            if (~unit->flags & UnitStatus::BeginInvisibility)
            {
                bw::BeginInvisibility(unit, Sound::Cloak);
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
        if (~unit->flags & UnitStatus::Building || unit->Type().Race() != 2)
            continue;
        if (bw::players[unit->player].type == 3)
            continue;

        if (bw::IsPowered(unit->unit_id, unit->sprite->position.x, unit->sprite->position.y, unit->player))
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
            {
                auto pos = unit->sprite->position;
                bw::ShowArea(1, bw::visions[unit->player], pos.x, pos.y, unit->IsFlying());
            }

            bw::UpdateVisibility(unit);
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
            bw::RevealSightArea(unit);
        }
    }
    for (Unit *unit : *bw::first_active_unit)
    {
        bw::UpdateVisibility(unit);
        if (unit->IsInvisible())
        {
            unit->invisibility_effects = 0;
            if (unit->secondary_order_wait == 0)
            {
                bw::UpdateDetectionStatus(unit);
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

bool Unit::IsDisabled() const
{
    if (flags & UnitStatus::Disabled || mael_timer || lockdown_timer || stasis_timer)
        return true;
    return false;
}

int Unit::GetMaxShields() const
{
    if (!Type().HasShields())
        return 0;
    return Type().Shields();
}

int Unit::GetShields() const
{
    if (!Type().HasShields())
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
    if (Type().IsHero())
        return Spell::HeroEnergy;

    auto energy_upgrade = Type().EnergyUpgrade();
    if (energy_upgrade == UpgradeId::None)
        return Spell::DefaultEnergy;
    else
        return Spell::DefaultEnergy + GetUpgradeLevel(energy_upgrade, player) * Spell::EnergyBonus;
}

int Unit::GetArmorUpgrades() const
{
    if (*bw::is_bw && (Type() == UnitId::Ultralisk || Type() == UnitId::Torrasque))
    {
        if (Type().IsHero())
            return GetUpgradeLevel(Type().ArmorUpgrade(), player) + 2;
        else
        {
            return GetUpgradeLevel(Type().ArmorUpgrade(), player) + 2 *
                GetUpgradeLevel(UpgradeId::ChitinousPlating, player);
        }
    }
    return GetUpgradeLevel(Type().ArmorUpgrade(), player);
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
    int max_hp = GetMaxHitPoints();
    int hp = (hitpoints + 0xff) >> 8;
    return (hp * 100 / max_hp) <= 33;
}

bool Unit::IsOnYellowHealth() const
{
    int max_hp = GetMaxHitPoints();
    int hp = (hitpoints + 0xff) >> 8;
    int percentage = (hp * 100 / max_hp);
    return percentage < 66 && percentage > 33;
}

int Unit::GetWeaponRange(bool ground) const
{
    using namespace UnitId;

    int range;
    if (ground)
        range = GetTurret()->GetGroundWeapon().MaxRange();
    else
        range = GetTurret()->GetAirWeapon().MaxRange();

    if (flags & UnitStatus::InBuilding)
        range += 0x40;
    switch (unit_id)
    {
        case Marine:
            return range + GetUpgradeLevel(UpgradeId::U_238Shells, player) * 0x20;
        case Hydralisk:
            return range + GetUpgradeLevel(UpgradeId::GroovedSpines, player) * 0x20;
        case Dragoon:
            return range + GetUpgradeLevel(UpgradeId::SingularityCharge, player) * 0x40;
        case FenixDragoon: // o.o
            if (*bw::is_bw)
                return range + 0x40;
            return range;
        case Goliath:
        case GoliathTurret:
            if (ground || *bw::is_bw == 0)
                return range;
            else
                return range + GetUpgradeLevel(UpgradeId::CharonBooster, player) * 0x60;
        case AlanSchezar:
        case AlanTurret:
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

    auto upgrade = Type().SightUpgrade();
    if (upgrade != UpgradeId::None && GetUpgradeLevel(upgrade, player) != 0)
        return 11;
    else
        return Type().SightRange();
}

int Unit::GetTargetAcquisitionRange() const
{
    using namespace UnitId;

    int base_range = Type().TargetAcquisitionRange();
    switch (unit_id)
    {
        case Ghost:
        case AlexeiStukov:
        case SamirDuran:
        case SarahKerrigan:
        case InfestedDuran:
            if (IsInvisible() && OrderType() == OrderId::HoldPosition)
                return 0;
            else
                return base_range;
        break;
        case Marine:
            return base_range + GetUpgradeLevel(UpgradeId::U_238Shells, player);
        break;
        case Hydralisk:
            return base_range + GetUpgradeLevel(UpgradeId::GroovedSpines, player);
        break;
        case Dragoon:
            return base_range + GetUpgradeLevel(UpgradeId::SingularityCharge, player) * 2;
        break;
        case FenixDragoon: // o.o
            if (*bw::is_bw)
                return base_range + 2;
            else
                return base_range;
        break;
        case Goliath:
        case GoliathTurret:
            if (*bw::is_bw)
                return base_range + GetUpgradeLevel(UpgradeId::CharonBooster, player) * 3;
            else
                return base_range;
        break;
        case AlanSchezar:
        case AlanTurret:
            if (*bw::is_bw)
                return base_range + 3;
            else
                return base_range;
        break;
        default:
            return base_range;
    }
}

void Unit::IncrementKills()
{
    if (kills != 0xffff)
        kills++;
    if (Type() == UnitId::Interceptor)
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
    if (Type() == UnitId::Larva)
    {
        if (~other->flags & UnitStatus::Building)
            return false;
    }
    else if (other->Type() == UnitId::Larva)
        return false;
    if (flags & UnitStatus::Harvesting && ~other->flags & UnitStatus::Building)
    {
        return other->flags & UnitStatus::Harvesting &&
            OrderType() != OrderId::ReturnGas &&
            other->OrderType() == OrderId::WaitForGas;
    }
    return true;
}

bool Unit::DoesCollideAt(const Point &own_pos, const Unit *other, const Point &other_pos) const
{
    const Rect16 &own_collision = Type().DimensionBox();
    const Rect16 &other_collision = other->Type().DimensionBox();
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
    switch (bw::GetOthersLocation(this, other))
    {
        case Pathing::top:
            return movement_direction > 0x40 && movement_direction < 0xc0;
        break;
        case Pathing::right:
            return movement_direction > 0x80;
        break;
        case Pathing::bottom:
            return movement_direction < 0x40 || movement_direction > 0xc0;
        break;
        case Pathing::left:
            return movement_direction > 0x0 && movement_direction < 0x80;
        break;
    }
    return false;
}

bool Unit::IsCarryingFlag() const
{
    if (Type().IsWorker() && worker.powerup && (worker.powerup->Type() == UnitId::Flag))
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
    if ((flags & UnitStatus::Burrowed) && Type() != UnitId::Lurker)
        return false;
    if (Type() == UnitId::SCV && OrderType() == OrderId::ConstructingBuilding)
        return false;
    if (Type() == UnitId::Ghost && OrderType() == OrderId::NukeTrack)
        return false;
    if (Type() == UnitId::Archon && OrderType() == OrderId::CompletingArchonSummon) // No da? o.o
        return false;
    return GetRClickAction() != 0;
}

bool Unit::CanTargetSelf(class OrderType order) const
{
    if (flags & UnitStatus::Hallucination)
        return false;
    if (flags & UnitStatus::Building)
        return OrderType() == OrderId::RallyPointUnit || OrderType() == OrderId::RallyPointTile;
    if (OrderType() == OrderId::DarkSwarm && (Type() == UnitId::Defiler || Type() == UnitId::UncleanOne))
        return true;
    if (IsTransport())
        return OrderType() == OrderId::MoveUnload;
    else
        return false;
}

bool Unit::CanUseTargetedOrder(class OrderType order_id) const
{
    if (flags & UnitStatus::Building && (order_id == OrderId::RallyPointUnit ||
                                         order_id == OrderId::RallyPointTile ||
                                         order_id == OrderId::RechargeShieldsBattery ||
                                         order_id == OrderId::PickupBunker))
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
    if (~flags & UnitStatus::Completed || Type() != UnitId::CommandCenter)
        return false;
    int maxhp = Type().HitPoints() / 256;
    if (!maxhp)
        maxhp = 1;
    if (hitpoints / 256 * 100 / maxhp >= 50) // If hp >= maxhp * 0,5
        return false;
    return true;
}

WeaponType Unit::GetGroundWeapon() const
{
    if ((Type() == UnitId::Lurker) && !(flags & UnitStatus::Burrowed))
        return WeaponId::None;
    return Type().GroundWeapon();
}

WeaponType Unit::GetAirWeapon() const
{
    return Type().AirWeapon();
}

int Unit::GetCooldown(WeaponType weapon) const
{
    int cooldown = weapon.Cooldown();
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

bool Unit::CanAttackFowUnit(UnitType fow_unit) const
{
    if (fow_unit == UnitId::None)
        return false;
    if (fow_unit.Flags() & UnitFlags::Invincible)
        return false;
    if (Type().HasHangar())
    {
        return true;
    }
    if (GetGroundWeapon() == WeaponId::None)
    {
        if (!subunit || subunit->GetGroundWeapon() == WeaponId::None)
            return false;
    }
    return true;
}

bool Unit::HasSubunit() const
{
    if (!subunit)
        return false;
    if (subunit->Type().IsSubunit())
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
    if (Type() == UnitId::Lurker && flags & UnitStatus::Burrowed)
        return RightClickAction::Attack;
    else if (flags & UnitStatus::Building && Type().HasRally() && Type().RightClickAction() == RightClickAction::None)
        return RightClickAction::Move;
    else
        return Type().RightClickAction();
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
    if ((Type() != UnitId::Lurker || ~flags & UnitStatus::Burrowed) &&
            Type().RightClickAction() == RightClickAction::None)
    {
        return false;
    }
    return GetRClickAction() != RightClickAction::Attack;
}

int Unit::GetUsedSpace() const
{
    int space = 0;
    for (Unit *unit = first_loaded; unit; unit = unit->next_loaded)
        space += unit->Type().SpaceRequired();
    return space;
}

int Unit::CalculateStrength(bool ground) const
{
    uint32_t multiplier = Type().Strength(ground);
    if (Type() == UnitId::Bunker)
        multiplier *= GetUsedSpace();
    if ((~flags & UnitStatus::Hallucination) && (Type().Flags() & UnitFlags::Spellcaster))
        multiplier += energy >> 9;

    return multiplier * GetHealth() / GetMaxHealth();
}

void Unit::UpdateStrength()
{
    if (Type() == UnitId::Larva || Type().IsEgg())
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

bool Unit::CanLoadUnit(const Unit *unit) const
{
    if (~flags & UnitStatus::Completed || IsDisabled() || (unit->flags & UnitStatus::Burrowed) || (player != unit->player))
        return false;

    if (Type().IsBuilding())
    {
        if (unit->Type().SpaceRequired() != 1 || unit->Type().Race() != Race::Terran)
            return false;
    }
    return Type().SpaceProvided() - GetUsedSpace() >= unit->Type().SpaceRequired();
}

// Unlike bw, new units are always last
// If one would unload unit and load another same-sized in its place, it would replace previous's slot
void Unit::LoadUnit(Unit *unit)
{
    Unit *cmp = first_loaded, *prev = nullptr;
    int unit_space = unit->Type().SpaceRequired();
    while (cmp && cmp->Type().SpaceRequired() >= unit_space)
    {
        prev = cmp;
        cmp = cmp->next_loaded;
    }

    unit->next_loaded = cmp;
    if (prev)
        prev->next_loaded = unit;
    else
        first_loaded = unit;

    bw::PlaySound(Sound::LoadUnit_Zerg + Type().Race(), this, 1, 0);
    if (unit->Type() == UnitId::SCV && unit->ai != nullptr)
    {
        if (((Ai::WorkerAi *)unit->ai)->town->building_scv == unit)
            ((Ai::WorkerAi *)unit->ai)->town->building_scv = nullptr;
    }
    unit->related = this;
    unit->flags |= UnitStatus::InTransport;
    bw::HideUnit(unit);
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
            bw::MoveUnit(subunit, sprite->position.x, sprite->position.y);

            subunit->flags &= ~UnitStatus::Reacts;
            subunit->flags |= UnitStatus::InBuilding;
            subunit->DeletePath();
            if ((subunit->sprite->elevation >= 12) || !(subunit->pathing_flags & 1))
                subunit->pathing_flags &= ~1;

        }
        else
            bw::MoveUnit(unit, sprite->position.x, sprite->position.y);
    }
}

class OrderType Unit::GetIdleOrder() const
{
    if (ai != nullptr)
        return OrderId::ComputerAi;
    return Type().ReturnToIdleOrder();
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
        IssueOrderTargetingNothing(GetIdleOrder());
    }
}

bool Unit::UnloadUnit(Unit *unit)
{
    if (order_timer && !(flags & UnitStatus::Building))
        return false;
    if (IsDisabled())
        return false;

    uint16_t position[2];
    if (!bw::GetUnloadPosition(position, this, unit))
        return false;

    order_timer = 0xf;
    bw::MoveUnit(unit, position[0], position[1]);
    if (unit->subunit)
        bw::MoveUnit(unit->subunit, position[0], position[1]);
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

    bw::PlaySound(Sound::UnloadUnit_Zerg + Type().Race(), this, 1, 0);
    bw::ShowUnit(unit);
    unit->flags &= ~(UnitStatus::InTransport | UnitStatus::InBuilding);
    unit->related = nullptr;
    unit->DeleteMovement();
    if (unit->HasSubunit())
        unit->subunit->DeleteMovement();

    unit->IssueOrderTargetingNothing(unit->GetIdleOrder());

    RefreshUi();
    if (~flags & UnitStatus::Building)
    {
        if (unit->Type() == UnitId::Reaver)
        {
            unit->order_timer = 0x1e;
        }
        else
        {
            auto weapon = unit->GetGroundWeapon();
            if (weapon != WeaponId::None)
                unit->ground_cooldown = unit->GetCooldown(weapon);
            weapon = unit->GetAirWeapon();
            if (weapon != WeaponId::None)
                unit->air_cooldown = unit->GetCooldown(weapon);
        }
    }
    return true;
}

void Unit::SetButtons(int buttonset)
{
    if (IsDisabled() && !Type().IsBuilding() && (buttonset != 0xe4))
        return;
    buttons = buttonset;
    RefreshUi();
}

void Unit::DeleteOrder(Order *order)
{
    if (order->Type().Highlight() != 0xffff)
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
            bw::Ai_UnloadFailure(this);
            return;
        }
    }
    else if (first_loaded == nullptr)
    {
        OrderDone();
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
    int distance = Distance(exact_position, Point32(order_target_pos) * 256 + Point32(128, 128)) & 0xffffff00;
    if (distance > 0x1000)
    {
        ChangeMovementTarget(order_target_pos);
        unk_move_waypoint = order_target_pos;
        if (ai && distance <= 0xca000)
        {
            if (bw::GetTerrainHeight(sprite->position.x, sprite->position.y) !=
                    bw::GetTerrainHeight(order_target_pos.x, order_target_pos.y))
            {
                return;
            }
            if (!bw::Ai_AreOnConnectedRegions(this, order_target_pos.x, order_target_pos.y))
                return;

            bool unk = bw::IsPointInArea(this, 0x200, order_target_pos.x, order_target_pos.y);
            for (Unit *unit = first_loaded; unit; unit = unit->next_loaded)
            {
                if (unit->Type() == UnitId::Reaver && !unk)
                    return;
                if (unit->Type().IsWorker())
                    return;
                uint16_t pos[2];
                if (!bw::GetUnloadPosition(pos, this, unit))
                    return;
            }
            PrependOrderTargetingNothing(OrderId::Unload);
            DoNextQueuedOrder();
        }
    }
    else
    {
        PrependOrderTargetingNothing(OrderId::Unload);
        DoNextQueuedOrder();
    }
}

bool Unit::IsTransport() const
{
    if (flags & UnitStatus::Hallucination)
        return false;
    if (Type() == UnitId::Overlord && GetUpgradeLevel(UpgradeId::VentralSacs, player) == 0)
        return false;
    return Type().SpaceProvided() != 0;
}

bool Unit::HasWayOfAttacking() const
{
    if (GetAirWeapon() != WeaponId::None || GetGroundWeapon() != WeaponId::None)
        return true;

    switch (Type().Raw())
    {
        case UnitId::Carrier:
        case UnitId::Gantrithor:
        case UnitId::Reaver:
        case UnitId::Warbringer:
            if (carrier.in_hangar_count || carrier.out_hangar_count)
                return true;
        break;
    }
    if (subunit)
    {
        if (subunit->GetAirWeapon() != WeaponId::None || subunit->GetGroundWeapon() != WeaponId::None)
            return true;
    }
    return false;
}

void Unit::AskForHelp(Unit *attacker)
{
    if ((flags & UnitStatus::Burrowed) || Type().IsWorker())
        return;

    if (Type() == UnitId::Arbiter || Type() == UnitId::Danimoth)
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
    if (Type() == UnitId::Larva)
        return;

    STATIC_PERF_CLOCK(Unit_ReactToHit);
    if (flags & UnitStatus::Burrowed && !irradiate_timer && Type() != UnitId::Lurker)
    {
        if (Type().Flags() & UnitFlags::Burrowable) // Huh?
            bw::Unburrow(this);
    }
    else if (!Type().IsBuilding() && (!CanAttackUnit(attacker) || Type().IsWorker()))
    {
        if (Type() == UnitId::Lurker && flags & UnitStatus::Burrowed)
        {
            if (!ai || irradiate_timer)
                return;
        }
        Unit *self = this;
        if (Type().IsSubunit())
            self = subunit;
        if (self->OrderType().Fleeable() && self->flags & UnitStatus::Reacts && self->IsAtHome())
        {
            if (bw::GetBaseMissChance(self) != 0xff) // Not under dark swarm
            {
                uint32_t flee_pos = bw::PrepareFlee(self, attacker);
                IssueOrderTargetingGround(OrderId::Move, Point(flee_pos & 0xffff, flee_pos >> 16));
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
    if (target->Type() != UnitId::Interceptor || !target->interceptor.parent)
        return target;
    if ((flags & UnitStatus::Reacts) || IsInAttackRange(target->interceptor.parent))
        return target->interceptor.parent;
    return target;
}

bool Unit::IsInvisibleTo(const Unit *unit) const
{
    if (!IsInvisible())
        return false;
    return (detection_status & (1 << unit->player)) == 0;
}

void Unit::Cloak(TechType tech)
{
    if (flags & UnitStatus::BeginInvisibility)
        return;
    int energy_cost = tech.EnergyCost() * 256;
    if (!IsCheatActive(Cheats::The_Gathering) && energy < energy_cost)
        return;
    if (!IsCheatActive(Cheats::The_Gathering))
        energy -= energy_cost;

    IssueSecondaryOrder(OrderId::Cloak);
}

bool Unit::IsMorphingBuilding() const
{
    if (flags & UnitStatus::Completed)
        return false;
    return UnitType(build_queue[current_build_slot]).IsBuildingMorphUpgrade();
}

bool Unit::IsResourceDepot() const
{
    if (~flags & UnitStatus::Building)
        return false;
    if (~flags & UnitStatus::Completed && !IsMorphingBuilding())
        return false;
    return Type().Flags() & UnitFlags::ResourceDepot;
}

void Unit::IssueSecondaryOrder(class OrderType order_id)
{
    if (secondary_order == order_id.Raw())
        return;
    secondary_order = order_id.Raw();
    secondary_order_state = 0;
    currently_building = nullptr;
    unke8 = 0;
    unkea = 0;
    // Not touching the secondary order timer..
}

void Unit::CancelConstruction(ProgressUnitResults *results)
{
    if (sprite == nullptr || OrderType() == OrderId::Die || flags & UnitStatus::Completed)
        return;
    if (Type() == UnitId::Guardian || Type() == UnitId::Lurker || Type() == UnitId::Devourer)
       return;
    if (Type() == UnitId::Mutalisk || Type() == UnitId::Hydralisk)
        return;
    if (Type() == UnitId::NydusCanal && nydus.exit != nullptr)
        return;
    if (flags & UnitStatus::Building)
    {
        if (Type().Race() == Race::Zerg)
        {
            CancelZergBuilding(results);
            return;
        }
        else
            bw::RefundFourthOfCost(player, unit_id);
    }
    else
    {
        int constructed_id = unit_id;
        if (Type().IsEgg())
            constructed_id = build_queue[current_build_slot];
        bw::RefundFullCost(constructed_id, player);
    }
    if (Type().EggCancelUnit() != UnitId::None)
    {
        bw::TransformUnit(this, Type().EggCancelUnit().Raw());
        build_queue[current_build_slot] = UnitId::None.Raw();
        remaining_build_time = 0;
        int old_image = Type().EggCancelUnit().Flingy().Sprite().Image().Raw();
        bw::ReplaceSprite(old_image, 0, sprite.get());
        order_signal &= ~0x4;
        SetIscriptAnimation(Iscript::Animation::Special2, true, "CancelConstruction", results);
        IssueOrderTargetingNothing(OrderId::Birth);
    }
    else
    {
        if (Type() == UnitId::NuclearMissile)
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
        bw::CancelBuildingMorph(this);
    }
    else
    {
        bw::RefundFourthOfCost(player, unit_id);
        bw::building_count[player]--;
        bw::men_score[player] += UnitId::Drone.BuildScore();
        if (Type() == UnitId::Extractor)
        {
            Unit *drone = bw::CreateUnit(UnitId::Drone, sprite->position.x, sprite->position.y, player);
            bw::FinishUnit_Pre(drone);
            bw::FinishUnit(drone);
            // Bw bug, uses uninitialized(?) value because extractor is separately created
            // So it gets set to set max hp anyways
            //bw::SetHp(drone, previous_hp * 256);
            Kill(results);
        }
        else
        {
            int prev_hp = previous_hp * 256, old_id = unit_id;
            Sprite *spawn = lone_sprites->AllocateLone(SpriteId::ZergBuildingSpawn_Small, sprite->position, player);
            spawn->elevation = sprite->elevation + 1;
            spawn->UpdateVisibilityPoint();
            bw::PlaySound(Sound::ZergBuildingCancel, this, 1, 0);
            bw::TransformUnit(this, UnitId::Drone);
            if (bw::players[player].type == 1 && ai)
            {
                Ai::BuildingAi *bldgai = (Ai::BuildingAi *)ai;
                Ai::Town *town = bldgai->town;
                bldgai->list.Remove(town->first_building);
                Ai::AddUnitAi(this, town);
            }
            bw::FinishUnit_Pre(this);
            bw::FinishUnit(this);
            Image *lowest = sprite->last_overlay;
            if (lowest->drawfunc == Image::Shadow && lowest->y_off != 7)
            {
                lowest->y_off = 7;
                lowest->flags |= 0x1;
            }
            bw::PrepareDrawSprite(sprite.get());
            IssueOrderTargetingNothing(OrderId::ResetCollision1);
            AppendOrderTargetingNothing(Type().ReturnToIdleOrder());
            bw::SetHp(this, prev_hp);
            bw::UpdateCreepDisappearance(old_id, sprite->position.x, sprite->position.y, 0);
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
    return bw::AreConnected(player, own_id, other_id) == false;
}

// Some of the logic is duped in EnemyUnitCache :/
bool Unit::CanAttackUnit(const Unit *enemy, bool check_detection) const
{
    using namespace UnitId;

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
            if (ai != nullptr)
                return false;
        break;
        default:
        break;
    }

    const Unit *turret = GetTurret();

    if (enemy->IsFlying())
        return turret->GetAirWeapon() != WeaponId::None;
    else
        return turret->GetGroundWeapon() != WeaponId::None;
}

bool Unit::CanBeAttacked() const
{
    if (sprite->IsHidden() || IsInvincible() || Type().IsSubunit())
        return false;
    return true;
}

bool Unit::CanAttackUnit_ChooseTarget(const Unit *enemy, bool check_detection) const
{
    using namespace UnitId;

    if (check_detection && enemy->IsInvisibleTo(this))
        return false;

    switch (Type().Raw())
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
    if (ai && Type() == UnitId::Scourge)
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
    else if (ai && Type().SightRange() > max_range)
        max_range = Type().SightRange();
    max_range *= 32;

    int min_range;
    if (GetGroundWeapon() == WeaponId::None && GetAirWeapon() == WeaponId::None)
        min_range = 0;
    else if (GetGroundWeapon() == WeaponId::None && GetAirWeapon() != WeaponId::None)
        min_range = GetAirWeapon().MinRange();
    else if (GetGroundWeapon() != WeaponId::None && GetAirWeapon() == WeaponId::None)
        min_range = GetGroundWeapon().MinRange();
    else
        min_range = min(GetAirWeapon().MinRange(), GetGroundWeapon().MinRange());

    Rect16 area = Rect16(sprite->position, max_range + 0x40);
    Unit *possible_targets[0x6 * 0x10];
    int possible_target_count[0x6] = { 0, 0, 0, 0, 0, 0 };
    enemy_unit_cache->ForAttackableEnemiesInArea(unit_search, this, area, [&](Unit *other, bool *stop)
    {
        if (~other->sprite->visibility_mask & (1 << player))
            return;
        if ((min_range && bw::IsInArea(this, min_range, other)) || !bw::IsInArea(this, max_range, other))
            return;
        const Unit *turret = GetTurret();
        if (~turret->flags & UnitStatus::FullAutoAttack)
        {
            auto &pos = other->sprite->position;
            if (!bw::CheckFiringAngle(turret, turret->Type().GroundWeapon().Raw(), pos.x, pos.y))
                return;
        }
        if (Ai_ShouldStopChasing(other))
            return;
        int threat_level = bw::GetThreatLevel(this, other);
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
            if ((check_alliance && !IsEnemy(unit)) || unit->Type().IsCritter())
                continue;
        }
        else if ((check_alliance && !IsEnemy(unit)) && !unit->Type().IsCritter())
        {
            continue;
        }

        int strength = 0;
        if (Type() == UnitId::Wraith)
        {
            strength = bw::GetCurrentStrength(unit, ground);
            if (strength + 1000 < ret_strength)
                return make_tuple(strength, unit);
            if (unit->flags & UnitStatus::Reacts && unit->CanDetect())
                strength += 1000;
            if (strength < ret_strength)
                continue;
        }

        if (!CanAttackUnit_ChooseTarget(unit, true))
            continue;

        if (!IsFlying() && !IsInAttackRange(unit) && flags & UnitStatus::Unk80)
            continue;
        if (home_region != -1)
        {
            if (unit->GetRegion() != home_region)
            {
                Ai::GuardAi *guard = (Ai::GuardAi *)ai;
                auto sprite_pos_32 = Point32(unit->sprite->position) * 256 + Point32(128, 128);
                if (((Distance(sprite_pos_32, exact_position)) & 0xFFFFFF00) > 0xc000)
                    continue;
                if (!bw::IsPointInArea(this, 0xc0, guard->home.x, guard->home.y))
                    continue;
            }
        }
        else
        {
            if (unit->GetRegion() != region_id)
            {
                if (!bw::IsInArea(this, 0x120, unit))
                    continue;
            }
        }
        if (Type() != UnitId::Wraith)
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
    if (~Type().Flags() & UnitFlags::Detector)
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
    if (Type().IsReaver() || GetAirWeapon() == WeaponId::None)
        return nullptr;

    if (ai->type != 4)
        return ChooseTarget(false);
    return ((Ai::MilitaryAi *)ai)->region->air_target;
}

Unit *Unit::Ai_ChooseGroundTarget()
{
    if (GetGroundWeapon() == WeaponId::None)
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
    Assert(OrderType() != OrderId::Die || order_state != 1);
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
    auto flingy_type = AsFlingy()->Type();
    if (!next_speed || flingy_movement_type != 0)
        return 0;
    if (next_speed == flingy_type.TopSpeed() && acceleration == flingy_type.Acceleration())
        return flingy_type.HaltDistance();
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
    return bw::IsInArea(this, range, other);
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

    WeaponType weapon;
    const Unit *turret = GetTurret();
    if (other->IsFlying())
        weapon = turret->GetAirWeapon();
    else
        weapon = turret->GetGroundWeapon();

    if (weapon == WeaponId::None)
        return false;

    int min_range = weapon.MinRange();
    if (min_range && bw::IsInArea(this, min_range, other))
        return false;

    return WillBeInAreaAtStop(other, GetWeaponRange(!other->IsFlying()));
}

int Unit::GetDistanceToUnit(const Unit *other) const
{
    //STATIC_PERF_CLOCK(Unit_GetDistanceToUnit);
    Rect16 other_rect;
    const Rect16 &other_dbox = other->Type().DimensionBox();
    other_rect.left = other->sprite->position.x - other_dbox.left;
    other_rect.top = other->sprite->position.y - other_dbox.top;
    other_rect.right = other->sprite->position.x + other_dbox.right + 1;
    other_rect.bottom = other->sprite->position.y + other_dbox.bottom + 1;
    const Unit *self = this;
    if (Type().IsSubunit())
        self = subunit;

    Rect16 self_rect;
    const Rect16 &own_dbox = self->Type().DimensionBox();
    self_rect.left = self->sprite->position.x - own_dbox.left;
    self_rect.top = self->sprite->position.y - own_dbox.top;
    self_rect.right = self->sprite->position.x + own_dbox.right + 1;
    self_rect.bottom = self->sprite->position.y + own_dbox.bottom + 1;

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
        return !bw::IsOutOfRange(other, this);
    }
    else if (other->Type() == UnitId::Bunker && other->first_loaded)
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
    if (unit->Type() == UnitId::Interceptor && unit->interceptor.parent != nullptr)
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
        if (cmp->Type() != UnitId::Bunker || cmp->first_loaded == nullptr)
        {
            return prev;
        }
    }
    if (!prev_can_attack && cmp_can_attack)
    {
        if (prev->Type() != UnitId::Bunker || prev->first_loaded == nullptr)
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
int Unit::Order_AttackMove_ReactToAttack(class OrderType order)
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
                bw::StopMoving(this);
                PrependOrder(order, target, order_target_pos, UnitId::None);
                PrependOrderTargetingUnit(Type().AttackUnitOrder(), previous_attacker);
                previous_attacker = nullptr;
                bw::DoNextQueuedOrderIfAble(this);
                AllowSwitchingTarget();
                return 0;
            }
        }
    }
    return 1;
}

void Unit::Order_AttackMove_TryPickTarget(class OrderType order)
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
            bw::StopMoving(this);
            PrependOrder(order, target, order_target_pos, UnitId::None);
            PrependOrderTargetingUnit(Type().AttackUnitOrder(), auto_target);
            bw::DoNextQueuedOrderIfAble(this);
            AllowSwitchingTarget();
        }
    }
}

bool Unit::ChangeMovementTargetToUnit(Unit *new_target)
{
    Point new_target_pos = new_target->sprite->position;
    if (new_target == move_target_unit)
    {
        if (move_target == new_target_pos || bw::IsPointAtUnitBorder(this, new_target, move_target.AsDword()))
        {
            flags &= ~UnitStatus::MovePosUpdated;
            return true;
        }
    }
    if (ai && OrderType() != OrderId::Pickup4 && bw::Ai_PrepareMovingTo(this, new_target_pos.x, new_target_pos.y))
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
    if (order_queue_begin == nullptr || !order_queue_begin->Type().KeepWaypointSpeed())
        flingy_flags &= ~0x20;
    else
        flingy_flags |= 0x20;
    return true;
}

bool Unit::ChangeMovementTarget(const Point &pos)
{
    if (pos == move_target)
        return true;
    if (ai && bw::Ai_PrepareMovingTo(this, pos.x, pos.y))
        return false;
    if (path)
        path->flags |= 0x1;

    Point clipped_pos = pos;
    bw::ClipPointInBoundsForUnit(unit_id, (uint16_t *)&clipped_pos);
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
    if (order_queue_begin == nullptr || !order_queue_begin->Type().KeepWaypointSpeed())
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
    const Rect16 dbox = Type().DimensionBox();
    const Point &pos = GetPosition();
    return Rect16(pos.x - dbox.left, pos.y - dbox.top, pos.x + dbox.right + 1, pos.y + dbox.bottom + 1);
}

int Unit::GetRegion() const
{
    return ::GetRegion(sprite->position);
}

bool Unit::IsUpgrading() const
{
    return building.upgrade != UpgradeId::None;
}

bool Unit::IsBuildingAddon() const
{
    return SecondaryOrderType() == OrderId::BuildAddon && flags & UnitStatus::Building &&
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
    if (ai != nullptr && next != nullptr && flags & UnitStatus::Burrowed && next->Type() != OrderId::Die && next->Type() != OrderId::Burrowed)
    {
        if (next->Type() != OrderId::Unburrow &&
                next->Type() != OrderId::ComputerAi &&
                Type() != UnitId::SpiderMine)
        {
            if (Type() != UnitId::Lurker ||
                    (next->Type() != OrderId::Guard && next->Type() != OrderId::AttackFixedRange))
            {
                InsertOrderBefore(OrderId::Unburrow, nullptr, sprite->position, UnitId::None, next);
                flags &= ~UnitStatus::UninterruptableOrder;
                OrderDone();
                return;
            }
        }
    }
    if (!next)
        return;
    if (flags & (UnitStatus::UninterruptableOrder | UnitStatus::Nobrkcodestart) && next->Type() != OrderId::Die)
        return;

    order_flags &= ~0x1;
    move_target_update_timer = 0;
    order_wait = 0;
    if (!next->Type().Interruptable())
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
        order_fow_unit = UnitId::None.Raw();
    }
    DeleteOrder(next);
    if (!ai)
        previous_attacker = nullptr;
    IscriptToIdle();
    if (HasSubunit())
    {
        class OrderType subunit_order;
        if (OrderType() == Type().ReturnToIdleOrder())
            subunit_order = subunit->Type().ReturnToIdleOrder();
        else if (OrderType() == Type().AttackUnitOrder())
            subunit_order = subunit->Type().AttackUnitOrder();
        else if (OrderType() == Type().AttackMoveOrder())
            subunit_order = Type().AttackMoveOrder();
        else if (OrderType().SubunitInheritance())
            subunit_order = OrderType();
        else
            return;
        if (target != nullptr)
            subunit->IssueOrder(subunit_order, target, next_order_pos, UnitId::None);
        else
            subunit->IssueOrder(subunit_order, nullptr, next_order_pos, UnitType(order_fow_unit));
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
    int8_t *shield_los = img->Type().ShieldOverlay();
    shield_los = shield_los + *(uint32_t *)(shield_los + 8 + img->direction * 4) + direction * 2; // sigh
    UnitIscriptContext ctx(this, nullptr, "ShowShieldHitOverlay", MainRng(), false);
    sprite->AddOverlayAboveMain(&ctx, ImageId::ShieldOverlay, shield_los[0], shield_los[1], direction);
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
        IssueOrderTargetingGround(OrderId::Move, target->sprite->position);
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
                if (bw::IsInArea(this, 4, target) || IsStandingStill() == 2)
                {
                    bw::StopMoving(this);
                    int radius = WeaponId::Suicide.OuterSplash();
                    if (!bw::IsInArea(this, radius, target))
                    {
                        // What???
                        OrderDone();
                        return;
                    }
                }
                else if (bw::IsInArea(this, 0x100, target) && target->flingy_flags & 2)
                {
                    bw::MoveToCollide(this, target);
                    return;
                }
                else
                {
                    bw::MoveTowards(this, target);
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
                int area = WeaponId::Suicide.OuterSplash();
                if (Distance(exact_position, Point32(order_target_pos) * 256 + Point32(128, 128)) / 256 > area)
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
            picked_threat_level = bw::GetThreatLevel(this, target);
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
            int threat_level = bw::GetThreatLevel(this, previous_attacker);
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
            int threat_level = bw::GetThreatLevel(this, auto_target);
            if (threat_level < picked_threat_level)
                picked_target = auto_target;
        }
    }
    if (picked_target != nullptr && CanAttackUnit(picked_target))
    {
        target = picked_target;
        if (subunit != nullptr)
        {
            if (subunit->OrderType() == subunit->Type().AttackUnitOrder() ||
                    subunit->OrderType() != OrderId::HoldPosition)
            {
                subunit->target = target;
            }
        }
    }
}

void Unit::Order_AttackUnit(ProgressUnitResults *results)
{
    if (IsComputerPlayer(player))
    {
        if (!target || !target->Type().IsBuilding())
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
            bw::StopMoving(this);
        }
        else if (ai == nullptr)
        {
            PrependOrderTargetingGround(OrderId::Move, target->sprite->position);
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
                if (bw::IsOutOfRange(this, target))
                {
                    if (!ChangeMovementTargetToUnit(target))
                        return;
                }
                else if (FlingyType(flingy_id).MovementType() == 2)
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
                    bw::Ai_StimIfNeeded(this);
                DoAttack(results, Iscript::Animation::GndAttkRpt);
            break;
            // Have multiple states to prevent units from getting stuck, kind of messy
            // State 1 = Out of range started, 2 = Inside range started - may need to stop, 3 = Continuing
            case 1: case 2: case 3:
            {
                int old_state = order_state;
                order_state = 3;
                if (bw::IsOutOfRange(this, target))
                {
                    if (old_state == 2) {
                        bw::StopMoving(this);
                    }
                    WeaponType weapon = Type().GroundWeapon();
                    bool melee = Type() == UnitId::Scourge ||
                        (weapon != WeaponId::None && weapon.Flingy().Raw() == 0);
                    if (ai == nullptr)
                    {
                        if (~flags & UnitStatus::CanSwitchTarget || IsFlying() ||
                            flags & UnitStatus::Disabled2 || !target->IsFlying())
                        {
                            if (IsStandingStill() != 2)
                            {
                                if (melee)
                                    bw::MoveForMeleeRange(this, target);
                                else
                                    bw::MoveTowards(this, target);
                                return;
                            }
                        }
                        bw::StopMoving(this);
                        unk_move_waypoint = move_target;
                        OrderDone();
                    }
                    else
                    {
                        if (bw::Ai_IsUnreachable(this, target) || IsStandingStill() == 2)
                        {
                            if (!Ai::UpdateAttackTarget(this, false, true, false))
                            {
                                bw::StopMoving(this);
                                unk_move_waypoint = move_target;
                                OrderDone();
                            }
                        }
                        else
                        {
                            if (melee)
                                bw::MoveForMeleeRange(this, target);
                            else
                                bw::MoveTowards(this, target);
                        }
                    }
                    return;
                }
                else
                {
                    if (Type() == UnitId::Scourge)
                        bw::MoveForMeleeRange(this, target);
                    else
                        bw::StopMoving(this);
                    if (ai != nullptr)
                        bw::Ai_StimIfNeeded(this);
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

void Unit::DoAttack_Main(WeaponType weapon, int iscript_anim, bool air, ProgressUnitResults *results)
{
    if (weapon == WeaponId::None)
        return;
    uint8_t &cooldown = air ? air_cooldown : ground_cooldown;
    if (cooldown)
    {
        order_wait = std::min(order_wait, cooldown);
        return;
    }
    if (!bw::IsReadyToAttack(this, weapon))
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
    bw::PlaySoundAtPos(sound, sprite->position.AsDword(), 1, 0);
}

void Unit::Order_HarvestMinerals(ProgressUnitResults *results)
{
    if (target == nullptr || !target->Type().IsMineralField())
    {
        // I don't think this check can ever be true, as RemoveReferences() will
        // clean up all harvesters from the mineral, and ordering will cancel this
        // order and do the same thing in Order_MineralHarvestInterrupted.
        if (worker.is_harvesting)
        {
            bw::FinishedMining(harvester.harvest_target, this);
        }
        DeleteSpecificOrder(OrderId::MineralHarvestInterrupted);
        IssueOrderTargetingNothing(OrderId::MoveToMinerals);
        return;
    }
    bw::AddResetHarvestCollisionOrder(this);
    switch (order_state)
    {
        case 0:
        {
            if (!bw::IsFacingMoveTarget(AsFlingy()))
                return;

            auto x = bw::circle[facing_direction][0] * 20 / 256;
            auto y = bw::circle[facing_direction][1] * 20 / 256;
            // To get iscript useweapon spawn the bullet properly
            order_target_pos = sprite->position + Point(x, y);
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
            bw::FinishedMining(target, this);
            DeleteSpecificOrder(OrderId::MineralHarvestInterrupted);
            if (carried_powerup_flags)
                IssueOrderTargetingNothing(OrderId::ReturnMinerals);
            else
                IssueOrderTargetingNothing(OrderId::MoveToMinerals); // Huh?
        }
        break;
    }
}

void Unit::DeleteSpecificOrder(class OrderType order_id)
{
    for (Order *order : order_queue_begin)
    {
        if (order->Type() == order_id)
        {
            DeleteOrder(order);
            return;
        }
    }
}

void Unit::AcquireResource(Unit *resource, ProgressUnitResults *results)
{
    using namespace UnitId;

    ImageType image = ImageId::MineralChunk;
    switch (resource->Type().Raw())
    {
        case Refinery:
            image = ImageId::TerranGasTank;
        break;
        case Extractor:
            image = ImageId::ZergGasSac;
        break;
        case Assimilator:
            image = ImageId::ProtossGasOrb;
        break;
        case MineralPatch1:
        case MineralPatch2:
        case MineralPatch3:
            image = ImageId::MineralChunk;
        break;
        default:
            Warning("Unit::AcquireResource called with non-resource resource (%x)", resource->unit_id);
            return;
        break;
    }
    int amount = resource->MineResource(results);
    if (amount < 8)
        image = ImageType(image.Raw() + 1);
    if (!amount)
        return;
    if (carried_powerup_flags & 0x3)
    {
        bw::DeletePowerupImages(this);
        carried_powerup_flags = 0;
    }
    if (bw::Ai_CanMineExtra(this))
        amount++;
    bw::CreateResourceOverlay(amount, resource->Type().IsMineralField(), this, image);
}

int Unit::MineResource(ProgressUnitResults *results)
{
    if (resource.resource_amount <= 8)
    {
        if (Type().IsMineralField())
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
        if (Type().IsMineralField())
            bw::UpdateMineralAmountAnimation(this);
        else if (resource.resource_amount < 8)
            bw::ShowInfoMessage(String::GeyserDepleted, Sound::GeyserDepleted, player);
        return 8;
    }
}

// Does both archons
void Unit::Order_WarpingArchon(int merge_distance, int close_distance, int result_unit, ProgressUnitResults *results)
{
    if (!target || target->player != player || target->order != order || target->target != this)
    {
        bw::StopMoving(this);
        unk_move_waypoint = move_target;
        OrderDone();
        return;
    }
    if (flags & UnitStatus::Collides && bw::IsInArea(this, current_speed * 2 / 256, target))
    {
        flags &= ~UnitStatus::Collides;
        PrependOrderTargetingNothing(OrderId::ResetCollision1);
    }
    int distance = Distance(sprite->position, target->sprite->position);
    if (distance > merge_distance)
    {
        if (order_state != 0 && IsStandingStill() == 2)
        {
            bw::StopMoving(this);
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
        bw::MergeArchonStats(this, target);
        bool was_permamently_cloaked = Type().Flags() & UnitFlags::PermamentlyCloaked;
        bw::TransformUnit(this, result_unit);
        if (was_permamently_cloaked)
            bw::EndInvisibility(this, Sound::Decloak);

        sprite->flags &= ~SpriteFlags::Nobrkcodestart;
        SetIscriptAnimation(Iscript::Animation::Special1, true, "Order_WarpingArchon", results);

        SetButtons(UnitId::None.Raw());
        kills += target->kills;
        target->Remove(results);
        flags &= ~UnitStatus::Reacts;
        DeletePath();
        movement_state = 0;
        if ((sprite->elevation >= 12) || !(pathing_flags & 1))
            pathing_flags &= ~1;

        IssueOrderTargetingNothing(OrderId::CompletingArchonSummon);
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
                bw::Burrow_Generic(this);
                bw::InstantCloak(this);
            }
            order_state = 3;
            // Fall through
        case 3:
        {
            Unit *victim = bw::SpiderMine_FindTarget(this);
            if (!victim)
                return;
            target = victim;
            SetIscriptAnimation(Iscript::Animation::Unburrow, true, "Order_SpiderMine (unburrow)", results);
            sprite->flags &= ~SpriteFlags::Unk40;
            IssueSecondaryOrder(OrderId::Nothing);
            order_state = 4;
        } // Fall through
        case 4:
            if (~order_signal & 0x4)
                return;
            order_signal &= ~0x4;
            sprite->elevation = Type().Elevation();
            flags &= ~UnitStatus::NoCollision;
            if (!target)
                order_state = 1;
            else
            {
                bw::MoveToCollide(this, target);
                order_state = 5;
            }
        break;
        case 5:
            bw::Unburrow_Generic(this);
            if (!target || !bw::IsInArea(this, 0x240, target))
            {
                bw::StopMoving(this);
                order_state = 1;
                return;
            }

            bw::MoveToCollide(this, target);
            if (!bw::IsInArea(this, 30, target))
            {
                if (IsStandingStill() == 2)
                {
                    bw::StopMoving(this);
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
            if (bw::IsInArea(this, 0x100, target) && target->flingy_flags & 0x2)
                bw::MoveToCollide(this, target);
            else
                bw::MoveTowards(this, target);
            if (bw::IsInArea(this, Type().GroundWeapon().InnerSplash() / 2, target))
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
    if (related && !bw::IsInArea(this, 10, related))
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
    if (!bw::IsGoodLarvaPosition(this, new_pos.x, new_pos.y))
    {
        if (bw::IsGoodLarvaPosition(this, sprite->position.x, sprite->position.y))
            return;
        uint32_t default_x;
        uint32_t default_y;
        bw::GetDefaultLarvaPosition(this, &default_x, &default_y);
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
    if (Type() == UnitId::Larva)
    {
        Order_Larva(results);
        if (sprite == nullptr || OrderType() == OrderId::Die)
            return;
    }
    else if (Type() == UnitId::Medic)
    {
        if (bw::Ai_IsMilitaryAtRegionWithoutState0(this))
            bw::Ai_ReturnToNearestBaseForced(this);
        else
            IssueOrderTargetingNothing(OrderId::Medic);
        return;
    }
    if (ai == nullptr)
    {
        IssueOrderTargetingNothing(Type().AiIdleOrder());
        return;
    }
    if (bw::Ai_UnitSpecific(this))
        return;
    switch (ai->type)
    {
        case 1:
            IssueOrderTargetingNothing(Type().AiIdleOrder());
        break;
        case 2:
            bw::Ai_WorkerAi(this);
        break;
        case 3:
            if (bw::Ai_TryProgressSpendingQueue(this))
                order_timer = 45;
            else
                order_timer = 3;
        break;
        case 4:
            bw::Ai_Military(this);
            order_timer = 3;
        break;
    }
    if (Type().Race() == Race::Terran &&
            Type().Flags() & UnitFlags::Mechanical &&
            !bw::Ai_IsInAttack(this, false))
    {
        if (GetMaxHitPoints() != (hitpoints + 255) / 256 && flags & UnitStatus::Reacts)
        {
           if (!plague_timer && !irradiate_timer && !Type().IsWorker())
           {
               Unit *scv = bw::Ai_FindNearestRepairer(this);
               if (scv)
                   IssueOrderTargetingUnit(OrderId::Follow, scv);
               return;
           }
        }
    }
    if (Type() == UnitId::SiegeTankTankMode)
        bw::Ai_SiegeTank(this);
    else if (flags & UnitFlags::Burrowable)
        bw::Ai_Burrower(this);
}

void Unit::Order_Interceptor(ProgressUnitResults *results)
{
    if (interceptor.parent && shields < GetMaxShields() * 256 / 2)
    {
        IssueOrderTargetingNothing(OrderId::InterceptorReturn);
        return;
    }
    if (bw::Interceptor_Attack(this) == 0)
        return;
    switch (order_state)
    {
        case 0:
            if (!interceptor.parent)
            {
                Kill(results);
                return;
            }
            bw::PlaySound(Sound::InterceptorLaunch, this, 1, 0);
            bw::Interceptor_Move(this, 3, order_target_pos.AsDword());
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
                    bw::Interceptor_Move(this, 1, order_target_pos.AsDword());
                    order_state = 4;
                    return;
                }
            }
            ChangeMovementTarget(order_target_pos);
            unk_move_waypoint = order_target_pos;
        break;
        case 4:
            if (!bw::IsPointInArea(this, 50, move_target.x, move_target.y))
                return;
            bw::Interceptor_Move(this, 2, order_target_pos.AsDword());
            order_state = 5;
        break;
        case 5:
            if (!bw::IsPointInArea(this, 50, move_target.x, move_target.y))
                return;
            bw::Interceptor_Move(this, 2, order_target_pos.AsDword());
            order_state = 3;
        break;
    }

}

void Unit::Order_InterceptorReturn(ProgressUnitResults *results)
{
    Assert(Type() == UnitId::Interceptor);
    if (interceptor.parent == nullptr)
    {
        Kill(results);
        return;
    }
    auto parent_pos = interceptor.parent->sprite->position;
    int parent_distance = Distance(exact_position, Point32(parent_pos) * 256 + Point32(128, 128)) >> 8;
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
        bw::LoadFighter(interceptor.parent, this);
        IssueOrderTargetingNothing(OrderId::Nothing);
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
    if (target == nullptr || !bw::CanSeeTarget(this))
        return false;
    if (!CanAttackUnit(target, true))
        return false;
    order_target_pos = target->sprite->position;
    if (HasSubunit())
        return true;
    bool ground = !target->IsFlying();
    WeaponType weapon;
    int cooldown;
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
    int min_range = weapon.MinRange();
    if ((min_range && min_range > dist) || GetWeaponRange(ground) < dist)
        return false;
    int target_dir = bw::GetFacingDirection(sprite->position.x, sprite->position.y, order_target_pos.x, order_target_pos.y);
    int angle_diff = target_dir - facing_direction;
    if (angle_diff < 0)
        angle_diff += 256;
    if (angle_diff > 128)
        angle_diff = 256 - angle_diff;
    if (weapon.AttackAngle() < angle_diff)
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
            bw::StopMoving(this);
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
                    target = bw::PickReachableTarget(this);
                    if (!target)
                        order_wait = 0;
                }
            }
            else if (!HasSubunit() || Type().IsGoliath())
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
        if (unit->Type().IsBuilding())
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
    if (bw::AttackRecentAttacker(this) != 0)
        return;
    if (order_timer != 0)
        return;
    order_timer = 15;
    // Ever true?
    if (Type().IsSubunit())
        unk_move_waypoint = subunit->unk_move_waypoint;
    if (GetTargetAcquisitionRange() != 0)
    {
        Unit *auto_target = GetAutoTarget();
        if (auto_target != nullptr)
            bw::AttackUnit(this, auto_target, true, false);
    }
}

void Unit::Order_Land(ProgressUnitResults *results)
{
    if (flags & UnitStatus::Building) // Is already landed
    {
        IssueOrderTargetingNothing(OrderId::LiftOff);
        AppendOrder(OrderId::Land, target, order_target_pos, UnitId::None, false);
        AppendOrderTargetingNothing(Type().ReturnToIdleOrder());
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
            xuint x_tile = (order_target_pos.x - Type().PlacementBox().width / 2) / 32;
            yuint y_tile = (order_target_pos.y - Type().PlacementBox().height / 2) / 32;
            int result = UpdateBuildingPlacementState(this,
                                                      player,
                                                      x_tile,
                                                      y_tile,
                                                      Type(),
                                                      0,
                                                      false,
                                                      true,
                                                      false);
            if (result != 0)
            {
                bw::ShowLandingError(this);
                if (order_queue_begin != nullptr && order_queue_begin->Type() == OrderId::PlaceAddon)
                    DeleteOrder(order_queue_begin);
                OrderDone();
            }
            else
            {
                bw::SetBuildingTileFlag(this, order_target_pos.x, order_target_pos.y);
                building.is_landing = 1;
                ChangeMovementTarget(order_target_pos);
                unk_move_waypoint = order_target_pos;
                if (sprite->last_overlay->drawfunc == Image::Shadow)
                    sprite->last_overlay->FreezeY();
                flags |= UnitStatus::UninterruptableOrder;
                flingy_top_speed = 256;
                bw::SetSpeed_Iscript(this, 256);
                order_state = 2;
            }
        }
        break;
        case 2:
            if (position != unk_move_waypoint && bw::GetFacingDirection(sprite->position.x, sprite->position.y, unk_move_waypoint.x, unk_move_waypoint.y) != movement_direction)
                return;
            SetIscriptAnimation(Iscript::Animation::Landing, true, "Order_Land", results);
            order_state = 3;
            // No break
        case 3:
        {
            if (IsStandingStill() == 0 || ~order_signal & 0x10)
                return;
            order_signal &= ~0x10;
            bw::ClearBuildingTileFlag(this, order_target_pos.x, order_target_pos.y);
            bw::RemoveFromMap(this);
            flags &= ~(UnitStatus::UninterruptableOrder | UnitStatus::Reacts | UnitStatus::Air);
            flags |= UnitStatus::Building;
            DeletePath();
            movement_state = 0;
            sprite->elevation = 4;
            pathing_flags |= 0x1;
            bw::MoveUnit(this, order_target_pos.x, order_target_pos.y);
            bw::FlyingBuilding_SwitchedState(this);
            // (There was leftover code from beta which removed landed buildings from multiselection)
            if (sprite->last_overlay->drawfunc == Image::Shadow)
            {
                sprite->last_overlay->SetOffset(sprite->last_overlay->x_off, 0);
                sprite->last_overlay->ThawY();
            }
            if (LoFile::GetOverlay(sprite->main_image->Type(), Overlay::Land).IsValid())
            {
                sprite->AddMultipleOverlaySprites(Overlay::Land, 8, SpriteId::LandingDust1, 0, false);
                sprite->AddMultipleOverlaySprites(Overlay::Land, 8, SpriteId::LandingDust1, 16, true);
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
            if (order_queue_begin != nullptr)
            {
                if (order_queue_begin->Type() == OrderId::Move || order_queue_begin->Type() == OrderId::Follow)
                {
                    PrependOrderTargetingNothing(OrderId::LiftOff);
                }
                else if (order_queue_begin->Type() != OrderId::PlaceAddon)
                {
                    while (order_queue_end != nullptr)
                    {
                        DeleteOrder(order_queue_end);
                    }
                    IssueOrderTargetingNothing(Type().ReturnToIdleOrder());
                }
            }
            // Should never do anything
            bw::FlyingBuilding_LiftIfStillBlocked(this);
            ForceOrderDone();
            Unit *addon = bw::FindClaimableAddon(this);
            if (addon)
                bw::AttachAddon(this, addon);
            if (*bw::is_placing_building)
            {
                bw::EndAddonPlacement();
                if (!bw::CanPlaceBuilding(*bw::primary_selected, *bw::placement_unit_id, *bw::placement_order))
                {
                    bw::MarkPlacementBoxAreaDirty();
                    bw::EndBuildingPlacement();
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
            if (Type() != UnitId::SiegeTankTankMode && Type() != UnitId::EdmundDukeT)
            {
                OrderDone();
                return;
            }
            if (flingy_flags & 0x2)
            {
                bw::StopMoving(this);
                unk_move_waypoint = move_target;
            }
            order_state = 1;
            // No break
        case 1:
        {
            if (flingy_flags & 0x2 || subunit->flingy_flags & 0x2 || subunit->flags & UnitStatus::Nobrkcodestart)
                return;
            bw::SetMoveTargetToNearbyPoint(Type().SpawnDirection(), AsFlingy());
            subunit->IssueOrderTargetingUnit(OrderId::Nothing3, this);
            bw::SetMoveTargetToNearbyPoint(subunit->Type().SpawnDirection(), subunit->AsFlingy());
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
            if (!bw::IsFacingMoveTarget(AsFlingy()) || !bw::IsFacingMoveTarget(subunit->AsFlingy()))
                return;
            int sieged = Type() == UnitId::SiegeTankTankMode ? UnitId::SiegeTank_Sieged : UnitId::EdmundDukeS;
            bw::TransformUnit(this, sieged);
            order_state = 3;
            // No break
        }
        case 3:
            if (~order_signal & 0x1)
                return;
            order_signal &= ~0x1;
            if (order_queue_begin != nullptr && order_queue_begin->Type() != OrderId::WatchTarget)
            {
                IssueOrderTargetingNothing(Type().ReturnToIdleOrder());
            }
            else if (order_queue_begin == nullptr)
            {
                AppendOrderTargetingNothing(Type().ReturnToIdleOrder());
            }
            ForceOrderDone();
            if (subunit->order_queue_begin == nullptr)
                subunit->AppendOrderTargetingNothing(subunit->Type().ReturnToIdleOrder());
            subunit->ForceOrderDone();
    }
}

static void TransferUpgrade(UpgradeType upgrade, int from_player, int to_player)
{
    if (GetUpgradeLevel(upgrade, from_player) > GetUpgradeLevel(upgrade, to_player))
    {
        SetUpgradeLevel(upgrade, to_player, GetUpgradeLevel(upgrade, from_player));
        UnitType unit_id = upgrade.MovementSpeedUpgradeUnit();
        if (unit_id != UnitId::None)
        {
            for (Unit *unit : bw::first_player_unit[to_player])
            {
                if (unit->unit_id == unit_id)
                {
                    unit->flags |= UnitStatus::SpeedUpgrade;
                    bw::UpdateSpeed(unit);
                }
            }
        }
        unit_id = upgrade.AttackSpeedUpgradeUnit();
        if (unit_id != UnitId::None)
        {
            for (Unit *unit : bw::first_player_unit[to_player])
            {
                if (unit->unit_id == unit_id)
                {
                    unit->flags |= UnitStatus::AttackSpeedUpgrade;
                    bw::UpdateSpeed(unit);
                }
            }
        }
    }
}

static void TransferTech(TechType tech, int from_player, int to_player)
{
    if (GetTechLevel(tech, from_player) > GetTechLevel(tech, to_player))
    {
        SetTechLevel(tech, to_player, GetTechLevel(tech, from_player));
    }
}

void Unit::TransferTechsAndUpgrades(int new_player)
{
    using namespace UnitId;

    switch (Type().Raw())
    {
        case Marine:
            TransferUpgrade(UpgradeId::U_238Shells, player, new_player);
            TransferTech(TechId::Stimpacks, player, new_player);
        break;
        case Firebat:
            TransferTech(TechId::Stimpacks, player, new_player);
        break;
        case Ghost:
            TransferUpgrade(UpgradeId::OcularImplants, player, new_player);
            TransferUpgrade(UpgradeId::MoebiusReactor, player, new_player);
            TransferTech(TechId::Lockdown, player, new_player);
            TransferTech(TechId::PersonnelCloaking, player, new_player);
        break;
        case Medic:
            TransferUpgrade(UpgradeId::CaduceusReactor, player, new_player);
            TransferTech(TechId::Healing, player, new_player);
            TransferTech(TechId::Restoration, player, new_player);
            TransferTech(TechId::OpticalFlare, player, new_player);
        break;
        case Vulture:
            TransferUpgrade(UpgradeId::IonThrusters, player, new_player);
            TransferTech(TechId::SpiderMines, player, new_player);
        break;
        case SiegeTank_Sieged:
        case SiegeTankTankMode:
            TransferTech(TechId::SiegeMode, player, new_player);
        break;
        case Goliath:
            TransferUpgrade(UpgradeId::CharonBooster, player, new_player);
        break;
        case Wraith:
            TransferUpgrade(UpgradeId::ApolloReactor, player, new_player);
            TransferTech(TechId::CloakingField, player, new_player);
        break;
        case ScienceVessel:
            TransferUpgrade(UpgradeId::TitanReactor, player, new_player);
            TransferTech(TechId::DefensiveMatrix, player, new_player);
            TransferTech(TechId::EmpShockwave, player, new_player);
            TransferTech(TechId::Irradiate, player, new_player);
        break;
        case Battlecruiser:
            TransferUpgrade(UpgradeId::ColossusReactor, player, new_player);
            TransferTech(TechId::YamatoGun, player, new_player);
        break;
        case Overlord:
            TransferUpgrade(UpgradeId::VentralSacs, player, new_player);
            TransferUpgrade(UpgradeId::Antennae, player, new_player);
            TransferUpgrade(UpgradeId::PneumatizedCarapace, player, new_player);
        break;
        case Drone:
        case InfestedTerran:
            TransferTech(TechId::Burrowing, player, new_player);
        break;
        case Zergling:
            TransferUpgrade(UpgradeId::AdrenalGlands, player, new_player);
            TransferUpgrade(UpgradeId::MetabolicBoost, player, new_player);
            TransferTech(TechId::Burrowing, player, new_player);
        break;
        case Hydralisk:
            TransferUpgrade(UpgradeId::MuscularAugments, player, new_player);
            TransferUpgrade(UpgradeId::GroovedSpines, player, new_player);
            TransferTech(TechId::Burrowing, player, new_player);
            TransferTech(TechId::LurkerAspect, player, new_player);
        break;
        case Lurker:
            TransferTech(TechId::LurkerAspect, player, new_player);
        break;
        case Ultralisk:
            TransferUpgrade(UpgradeId::AnabolicSynthesis, player, new_player);
            TransferUpgrade(UpgradeId::ChitinousPlating, player, new_player);
        break;
        case Queen:
            TransferUpgrade(UpgradeId::GameteMeiosis, player, new_player);
            TransferTech(TechId::Infestation, player, new_player);
            TransferTech(TechId::Parasite, player, new_player);
            TransferTech(TechId::SpawnBroodlings, player, new_player);
            TransferTech(TechId::Ensnare, player, new_player);
        break;
        case Defiler:
            TransferUpgrade(UpgradeId::MetasynapticNode, player, new_player);
            TransferTech(TechId::DarkSwarm, player, new_player);
            TransferTech(TechId::Plague, player, new_player);
            TransferTech(TechId::Consume, player, new_player);
            TransferTech(TechId::Burrowing, player, new_player);
        break;
        case Dragoon:
            TransferUpgrade(UpgradeId::SingularityCharge, player, new_player);
        break;
        case Zealot:
            TransferUpgrade(UpgradeId::LegEnhancements, player, new_player);
        break;
        case Reaver:
            TransferUpgrade(UpgradeId::ScarabDamage, player, new_player);
            TransferUpgrade(UpgradeId::ReaverCapacity, player, new_player);
        break;
        case Shuttle:
            TransferUpgrade(UpgradeId::GraviticDrive, player, new_player);
        break;
        case Observer:
            TransferUpgrade(UpgradeId::SensorArray, player, new_player);
            TransferUpgrade(UpgradeId::GraviticBoosters, player, new_player);
        break;
        case HighTemplar:
            TransferUpgrade(UpgradeId::KhaydarinAmulet, player, new_player);
            TransferTech(TechId::PsionicStorm, player, new_player);
            TransferTech(TechId::Hallucination, player, new_player);
            TransferTech(TechId::ArchonWarp, player, new_player);
        break;
        case DarkTemplar:
            TransferTech(TechId::DarkArchonMeld, player, new_player);
        break;
        case DarkArchon:
            TransferUpgrade(UpgradeId::ArgusTalisman, player, new_player);
            TransferTech(TechId::Feedback, player, new_player);
            TransferTech(TechId::MindControl, player, new_player);
            TransferTech(TechId::Maelstrom, player, new_player);
        break;
        case Scout:
            TransferUpgrade(UpgradeId::ApialSensors, player, new_player);
            TransferUpgrade(UpgradeId::GraviticThrusters, player, new_player);
        break;
        case Carrier:
            TransferUpgrade(UpgradeId::CarrierCapacity, player, new_player);
        break;
        case Arbiter:
            TransferUpgrade(UpgradeId::KhaydarinCore, player, new_player);
            TransferTech(TechId::Recall, player, new_player);
            TransferTech(TechId::StasisField, player, new_player);
        break;
        case Corsair:
            TransferUpgrade(UpgradeId::ArgusJewel, player, new_player);
            TransferTech(TechId::DisruptionWeb, player, new_player);
        break;
    }
}

void Unit::GiveTo(int new_player, ProgressUnitResults *results)
{
    TransferTechsAndUpgrades(new_player);
    bw::RemoveFromSelections(this);
    bw::RemoveFromClientSelection3(this);
    if (flags & UnitStatus::Building)
    {
        if (UnitType(build_queue[current_build_slot % 5]) < UnitId::CommandCenter)
        {
            CancelTrain(results);
            SetIscriptAnimation(Iscript::Animation::WorkingToIdle, true, "Unit::GiveTo", results);
        }
        if (building.tech != TechId::None)
            bw::CancelTech(this);
        if (building.upgrade != UpgradeId::None)
            bw::CancelUpgrade(this);
    }
    if (Type().HasHangar())
        CancelTrain(results);
    bw::GiveUnit(this, new_player, 1);
    if (IsActivePlayer(new_player))
        bw::GiveSprite(this, new_player);
    if (IsBuildingAddon() || ~flags & UnitStatus::Completed || Type().Flags() & UnitFlags::SingleEntity)
        return;
    if (Type() == UnitId::Interceptor || Type() == UnitId::Scarab || Type() == UnitId::NuclearMissile)
        return;
    if (flags & UnitStatus::InTransport)
        return;
    switch (bw::players[player].type)
    {
        case 1:
            IssueOrderTargetingNothing(Type().AiIdleOrder());
        break;
        case 3:
            IssueOrderTargetingNothing(OrderId::RescuePassive);
        break;
        case 7:
            IssueOrderTargetingNothing(OrderId::Neutral);
        break;
        default:
            IssueOrderTargetingNothing(Type().HumanIdleOrder());
        break;
    }
}

void Unit::Trigger_GiveUnit(int new_player, ProgressUnitResults *results)
{
    using namespace UnitId;

    if (new_player == 0xd)
        new_player = *bw::trigger_current_player;
    if (new_player >= Limits::Players || flags & UnitStatus::Hallucination || Type().Flags() & UnitFlags::Addon || new_player == player)
        return; // Bw would also SErrSetLastError if invalid player
    switch (Type().Raw())
    {
        case MineralPatch1:
        case MineralPatch2:
        case MineralPatch3:
        case VespeneGeyser:
        case Interceptor:
        case Scarab:
            return;
    }
    if (Type().IsGasBuilding() && flags & UnitStatus::Completed && player < Limits::Players)
    {
        for (Unit *unit : bw::first_player_unit[player])
        {
            if (unit->OrderType() == OrderId::HarvestGas && unit->Type().IsWorker() && unit->sprite->IsHidden() && unit->target == this)
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
    if (Type().HasHangar())
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
        if (Type() == UnitId::NydusCanal && nydus.exit != nullptr)
        {
            nydus.exit->GiveTo(new_player, results);
        }
    }
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
            if (!IsInvisible() || ImageType(cmd.val).DrawIfCloaked())
                result = CmdResult::NotHandled;
        break;
        case Move:
            // Note that in some cases bw tries to predict the speed,
            // the handling for that is in a hook
            bw::SetSpeed_Iscript(this, bw::CalculateSpeedChange(this, cmd.val * 256));
        break;
        case LiftoffCondJmp:
            if (IsFlying())
                script->pos = cmd.pos;
        break;
        case AttackWith:
            bw::Iscript_AttackWith(this, cmd.val);
        break;
        case Iscript::Opcode::Attack:
            if (target == nullptr || target->IsFlying())
                bw::Iscript_AttackWith(this, 0);
            else
                bw::Iscript_AttackWith(this, 1);
        break;
        case CastSpell:
            if (OrderType().Weapon() != WeaponId::None && !bw::ShouldStopOrderedSpell(this))
            {
                bw::FireWeapon(this, OrderType().Weapon());
            }
        break;
        case UseWeapon:
            bw::Iscript_UseWeapon(cmd.val, this);
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
        {
            // Sigh... Changes the x offset
            const auto &weapons = bw::weapons_dat[20];
            *((uint8_t *)(weapons.data) + GetGroundWeapon().Raw()) = cmd.val;
            bw::Iscript_AttackWith(this, 1);
        }
        break;
        case CreateGasOverlays:
        {
            ImageType smoke_img(ImageId::VespeneSmokeOverlay1.Raw() + cmd.val);
            // Bw can be misused to have this check for loaded nuke and such
            // Even though resource_amount is word, it won't report incorrect
            // values as unit array starts from 0x0059CCA8
            // (The lower word is never 0 if the union contains unit)
            // But with dynamic allocation, that is not the case
            if (Type().Flags() & UnitFlags::ResourceContainer)
            {
                if (resource.resource_amount == 0)
                    smoke_img = ImageType(ImageId::VespeneSmallSmoke1.Raw() + cmd.val);
            }
            else
            {
                if (silo.nuke == nullptr)
                    smoke_img = ImageType(ImageId::VespeneSmallSmoke1.Raw() + cmd.val);
            }
            Point pos = LoFile::GetOverlay(img->Type(), Overlay::Special).GetValues(img, cmd.val).ToPoint16();

            Image *gas_overlay = new Image(sprite.get(), smoke_img, pos.x + img->x_off, pos.y + img->y_off);
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
            bool success = gas_overlay->InitIscript(ctx);
            if (!success)
                gas_overlay->SingleDelete();
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

void Unit::TargetedOrder(class OrderType new_order, Unit *target, const Point &pos, UnitType fow_unit, bool queued)
{
    if (queued && new_order.CanBeQueued() && OrderType().CanQueueOn())
    {
        if (highlighted_order_count > 8)
        {
            const char *string = (*bw::stat_txt_tbl)->GetTblString(String::WaypointListFull);
            bw::PrintInfoMessageForLocalPlayer(string, player);
            return;
        }
        InsertOrderAfter(new_order, target, pos, fow_unit, nullptr);
    }
    else
    {
        IssueOrder(new_order, target, pos, fow_unit);
    }
}

void Unit::IssueOrder(class OrderType order, Unit *target, const Point &pos, UnitType fow_unit)
{
    order_flags |= 0x1;
    AppendOrder(order, target, pos, fow_unit, true);
    DoNextQueuedOrder();
}

void Unit::AppendOrder(class OrderType new_order, Unit *order_target, const Point &pos, UnitType fow_unit, bool clear_others)
{
    auto &current_target = target;
    if (OrderType() == OrderId::Die)
        return;

    while (order_queue_end != nullptr)
    {
        Order *order = order_queue_end;
        if (clear_others && order->Type().Interruptable())
            DeleteOrder(order);
        else if (order->order_id == new_order)
            DeleteOrder(order);
        else
            break;
    }
    if (new_order == OrderId::Cloak)
    {
        PrependOrder(OrderType(), current_target, order_target_pos, UnitType(order_fow_unit));
        PrependOrder(OrderId::Cloak, order_target, pos, UnitId::None);
        ForceOrderDone();
    }
    else
    {
        InsertOrderAfter(new_order, order_target, pos, fow_unit, nullptr);
    }
}

void Unit::PrependOrder(class OrderType order, Unit *target, const Point &pos, UnitType fow_unit)
{
    if (order_queue_begin != nullptr)
        InsertOrderBefore(order, target, pos, fow_unit, order_queue_begin);
    else
        InsertOrderAfter(order, target, pos, fow_unit, nullptr);
}

void Unit::InsertOrderAfter(class OrderType order_id, Unit *target, const Point &pos, UnitType fow_unit, Order *insert_after)
{
    Order *new_order = new Order(order_id, pos, target, fow_unit);
    if (order_id.Highlight() != 0xffff)
        highlighted_order_count += 1;

    if (insert_after == nullptr)
    {
        if (order_queue_end == nullptr)
        {
            order_queue_begin = new_order;
            order_queue_end = new_order;
        }
        else
        {
            order_queue_end->list.next = new_order;
            new_order->list.prev = order_queue_end;
            order_queue_end = new_order;
        }
    }
    else
    {
        if (order_queue_end == insert_after)
            order_queue_end = new_order;
        new_order->list.prev = insert_after;
        new_order->list.next = insert_after->list.next;
        if (insert_after->list.next != nullptr)
            insert_after->list.next->list.prev = new_order;
        insert_after->list.next = new_order;
    }
}

void Unit::InsertOrderBefore(class OrderType order_id, Unit *target, const Point &pos, UnitType fow_unit, Order *insert_before)
{
    if (ai != nullptr && highlighted_order_count > 8)
        return;

    Order *new_order = new Order(order_id, pos, target, fow_unit);
    if (order_id.Highlight() != 0xffff)
        highlighted_order_count += 1;

    if (order_queue_begin == insert_before)
        order_queue_begin = new_order;
    new_order->list.prev = insert_before->list.prev;
    new_order->list.next = insert_before;
    if (insert_before->list.prev != nullptr)
        insert_before->list.prev->list.next = new_order;
    insert_before->list.prev = new_order;
}

Ai::BuildingAi *Unit::AiAsBuildingAi()
{
    if (ai != nullptr && ai->type == 3)
        return (Ai::BuildingAi *)ai;
    return nullptr;
}
