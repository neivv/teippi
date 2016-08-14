#include "unit.h"

#include <vector>

#include "constants/order.h"
#include "constants/sprite.h"
#include "constants/tech.h"
#include "constants/upgrade.h"
#include "constants/unit.h"

#include "ai.h"
#include "bullet.h"
#include "offsets.h"
#include "sound.h"
#include "yms.h"

static void BuildingCleanup(Unit *unit, ProgressUnitResults *results)
{
    if (unit->SecondaryOrderType() == OrderId::BuildAddon &&
        unit->currently_building != nullptr &&
        ~unit->currently_building->flags & UnitStatus::Completed)
    {
        unit->currently_building->CancelConstruction(results);
    }
    if (unit->building.addon)
        bw::DetachAddon(unit);
    if (TechType(unit->building.tech) != TechId::None)
        bw::CancelTech(unit);
    if (UpgradeType(unit->building.upgrade) != UpgradeId::None)
        bw::CancelUpgrade(unit);
    if (unit->build_queue[unit->current_build_slot] >= UnitId::CommandCenter.Raw())
        unit->CancelTrain(results);
    if (*bw::is_placing_building)
        bw::EndAddonPlacement();
    if (unit->Type().Race() == Race::Zerg)
    {
        if (unit->IsMorphingBuilding())
            unit->flags |= UnitStatus::Completed;
        if (bw::UpdateCreepDisappearance_Unit(unit) == false)
            unit->flags |= UnitStatus::HasDisappearingCreep;
    }
}

static int __stdcall ret_false_s3(int a, int b, int c)
{
    return 0;
}

static void SubunitCleanup(Unit *unit)
{
    // It seems that subunits are not on any list to begin with?
    Assert(unit->next() == nullptr && unit->prev() == nullptr);
    if (*bw::first_dying_unit != nullptr)
    {
        unit->list.Add(*bw::first_dying_unit);
    }
    else
    {
        *bw::first_dying_unit = unit;
        *bw::last_dying_unit = unit;
        unit->next() = nullptr;
        unit->prev() = nullptr;
    }
    unit->player_units.Remove(bw::first_player_unit.index_overflowing(unit->player));
    unit->player_units.next = nullptr;
    unit->player_units.prev = nullptr;
    bw::ModifyUnitCounters(unit, -1);
    unit->subunit = nullptr;
}

static void GasBuildingCleanup(Unit *unit)
{
    switch (unit->Type().Raw())
    {
        case UnitId::Extractor:
        {
            const auto &pos = unit->sprite->position;
            bw::RemoveCreepAtUnit(pos.x, pos.y, unit->Type().Raw(), (void *)&ret_false_s3);
            // Fall through
        }
        case UnitId::Refinery:
        case UnitId::Assimilator:
            if (unit->flags & UnitStatus::Completed)
                bw::ModifyUnitCounters2(unit, -1, 0);
            unit->flags &= ~UnitStatus::Completed;
            unit->resource.first_awaiting_worker = nullptr;
            bw::TransformUnit(unit, UnitId::VespeneGeyser);
            unit->order = unit->Type().HumanIdleOrder();
            unit->order_state = 0;
            unit->order_target_pos = Point(0, 0);
            unit->target = nullptr;
            unit->order_fow_unit = UnitId::None;
            unit->hitpoints = unit->Type().HitPoints();
            bw::GiveUnit(unit, NeutralPlayer, 0);
            bw::GiveSprite(unit, NeutralPlayer);
            unit->flags |= UnitStatus::Completed;
            bw::ModifyUnitCounters2(unit, 1, 1);
            if (*bw::is_placing_building)
                bw::RedrawGasBuildingPlacement(unit);
        break;
    }
}

// Called for all dying, but matters only if in transport
static void TransportedCleanup(Unit *unit)
{
    if (~unit->flags & UnitStatus::InTransport)
        return;

    if (unit->related != nullptr) // Can happen when irra kills in transport
    {
        bw::MoveUnit(unit, unit->sprite->position.x, unit->sprite->position.y); // Weeeell

        if (unit == unit->related->first_loaded)
        {
            unit->related->first_loaded = unit->next_loaded;
        }
        else
        {
            for (Unit *other = unit->related->first_loaded; other != nullptr; other = other->next_loaded)
            {
                if (other->next_loaded == unit)
                {
                    other->next_loaded = unit->next_loaded;
                    break;
                }
            }
        }
        unit->related = nullptr;
    }

    unit->flags &= ~UnitStatus::InTransport; // Pointless?
    unit->DeleteMovement();
    if (unit->HasSubunit())
        unit->subunit->DeleteMovement();
}


static void TransportCleanup(Unit *unit, ProgressUnitResults *results)
{
    TransportedCleanup(unit);

    // See DamageUnit()
    Unit *child, *next = unit->first_loaded;
    while (next != nullptr)
    {
        child = next;
        next = child->next_loaded;

        // Are these useless? Sc seems to check these intentionally and not just as part of inlined unit -> id conversion
        if (child->sprite != nullptr && child->OrderType() != OrderId::Die)
        {
            bool kill = true;
            if (unit->Type().IsBuilding())
                kill = unit->UnloadUnit(child) == false;
            if (kill)
                child->Kill(results);
        }
    }
}

static void RemoveFromLists(Unit *unit)
{
    // Some mapping tricks may depend on overflowing player units array
    unit->player_units.Remove(bw::first_player_unit.index_overflowing(unit->player));
    unit->player_units.next = nullptr;
    unit->player_units.prev = nullptr;

    bool is_revealer = unit->Type() == UnitId::ScannerSweep || unit->Type() == UnitId::MapRevealer;
    if (unit->next())
        unit->next()->prev() = unit->prev();
    else
    {
        if (is_revealer)
            *bw::last_revealer = unit->prev();
        else if (unit->sprite->IsHidden())
            *bw::last_hidden_unit = unit->prev();
        else
            *bw::last_active_unit = unit->prev();
    }
    if (unit->prev())
        unit->prev()->next() = unit->next();
    else
    {
        if (is_revealer)
            *bw::first_revealer = unit->next();
        else if (unit->sprite->IsHidden())
            *bw::first_hidden_unit = unit->next();
        else
            *bw::first_active_unit = unit->next();
    }

    if (*bw::first_dying_unit)
    {
        unit->list.Add(*bw::first_dying_unit);
    }
    else
    {
        *bw::first_dying_unit = unit;
        *bw::last_dying_unit = unit;
        unit->next() = nullptr;
        unit->prev() = nullptr;
    }
}

static void RemoveHarvesters(Unit *unit)
{
    Unit *worker = unit->resource.first_awaiting_worker;
    unit->resource.awaiting_workers = 0;
    while (worker != nullptr)
    {
        Unit *next = worker->harvester.harvesters.next;
        worker->harvester.harvesters.prev = nullptr;
        worker->harvester.harvesters.next = nullptr;
        worker->harvester.harvest_target = nullptr;
        worker = next;
    }
}

static void RemoveFromHangar(Unit *unit)
{
    Unit *parent = unit->interceptor.parent;
    if (parent == nullptr)
    {
        unit->interceptor.list.prev = nullptr;
        unit->interceptor.list.next = nullptr;
    }
    else
    {
        // D:
        unit->interceptor.list.Remove([&] {
            return std::ref(unit->interceptor.is_outside_hangar ?
                            parent->carrier.out_child :
                            parent->carrier.in_child);
        }());

        if (unit->interceptor.is_outside_hangar)
            parent->carrier.out_hangar_count--;
        else
            parent->carrier.in_hangar_count--;
    }
}


static void KillHangarUnits(Unit *unit, ProgressUnitResults *results)
{
    while (unit->carrier.in_hangar_count != 0)
    {
        Unit *child = unit->carrier.in_child;
        child->interceptor.parent = nullptr;
        unit->carrier.in_child = child->interceptor.list.next;
        child->Kill(results);
        unit->carrier.in_hangar_count--;
    }
    while (unit->carrier.out_hangar_count != 0)
    {
        Unit *child = unit->carrier.out_child;
        child->interceptor.parent = nullptr;
        if (child->Type() != UnitId::Scarab)
        {
            int death_time = 15 + MainRng()->Rand(31);
            if (!child->death_timer || child->death_timer > death_time)
                child->death_timer = death_time;
        }
        unit->carrier.out_child = child->interceptor.list.next;
        child->interceptor.list.prev = nullptr;
        child->interceptor.list.next = nullptr;
        unit->carrier.out_hangar_count--;
    }
    unit->carrier.in_child = nullptr;
    unit->carrier.out_child = nullptr;
}

static void KillChildren(Unit *unit, ProgressUnitResults *results)
{
    using namespace UnitId;

    switch (unit->Type().Raw())
    {
        case Carrier:
        case Reaver:
        case Gantrithor:
        case Warbringer:
            KillHangarUnits(unit, results);
            return;
        case Interceptor:
        case Scarab:
            if (unit->flags & UnitStatus::Completed)
                RemoveFromHangar(unit);
            return;
        case Ghost:
            if (unit->ghost.nukedot != nullptr)
            {
                unit->ghost.nukedot->SetIscriptAnimation_Lone(Iscript::Animation::Death,
                                                              true,
                                                              MainRng(),
                                                              "Unit KillChildren");
            }
            return;
        case NuclearSilo:
            if (unit->silo.nuke)
            {
                unit->silo.nuke->Kill(results);
                unit->silo.nuke = nullptr;
            }
            return;
        case NuclearMissile:
            if (unit->related != nullptr && unit->related->Type() == UnitId::NuclearSilo)
            {
                unit->related->silo.nuke = nullptr;
                unit->related->silo.has_nuke = 0;
            }
            return;
        case Pylon:
            if (unit->pylon.aura)
            {
                unit->pylon.aura->Remove();
                unit->pylon.aura = nullptr;
            }
            // Incompleted pylons are not in list, but maybe it can die as well before being
            // added to the list (Order_InitPylon adds them)
            if (unit->pylon_list.list.prev != nullptr ||
                unit->pylon_list.list.next != nullptr ||
                *bw::first_pylon == unit)
            {
                unit->pylon_list.list.Remove(*bw::first_pylon);
                *bw::pylon_refresh = 1;
            }
            return;
        case NydusCanal:
        {
            Unit *exit = unit->nydus.exit;
            if (exit != nullptr)
            {
                exit->nydus.exit = nullptr;
                unit->nydus.exit = nullptr;
                exit->Kill(results);
            }
            return;
        }
        case Refinery:
        case Extractor:
        case Assimilator:
            if (~unit->flags & UnitStatus::Completed)
                return;
            RemoveHarvesters(unit);
            return;
        case MineralPatch1:
        case MineralPatch2:
        case MineralPatch3:
            RemoveHarvesters(unit);
            return;
    }
}

static void Die(Unit *unit, ProgressUnitResults *results)
{
    if (unit->Type().IsBuilding())
        *bw::ignore_unit_flag80_clear_subcount = 1;
    KillChildren(unit, results);
    TransportCleanup(unit, results);
    bw::RemoveReferences(unit, 1);
    // Hak fix
    for (Unit *other : first_allocated_unit)
    {
        if (other->path && other->path->dodge_unit == unit)
            other->path->dodge_unit = nullptr;
    }

    RemoveFromBulletTargets(unit);
    Ai::RemoveUnitAi(unit, false);
    if (unit->Type().IsSubunit())
    {
        SubunitCleanup(unit);
        return;
    }
    if (unit->Type().IsGasBuilding() && !unit->sprite->IsHidden())
    {
        GasBuildingCleanup(unit);
        return;
    }

    bw::StopMoving(unit);
    if (!unit->sprite->IsHidden())
        bw::RemoveFromMap(unit);

    if (!unit->Type().IsBuilding())
    {
        unit->invisibility_effects = 0;
        bw::RemoveFromCloakedUnits(unit);
        if (unit->flags & UnitStatus::BeginInvisibility)
        {
            bw::EndInvisibility(unit, Sound::Decloak);
        }
    }
    if (unit->flags & UnitStatus::Building)
        BuildingCleanup(unit, results);
    if ((unit->Type().Flags() & UnitFlags::FlyingBuilding) && (unit->building.is_landing != 0))
        bw::ClearBuildingTileFlag(unit, unit->order_target_pos.x, unit->order_target_pos.y);

    unit->DeletePath();
    if (unit->currently_building != nullptr)
    {
        if (unit->SecondaryOrderType() == OrderId::Train ||
            unit->SecondaryOrderType() == OrderId::TrainFighter)
        {
            unit->currently_building->Kill(results);
        }
        unit->currently_building = nullptr;
    }
    unit->IssueSecondaryOrder(OrderId::Nothing);
    bw::DropPowerup(unit);
    bw::RemoveFromSelections(unit);
    bw::RemoveFromClientSelection3(unit);
    bw::RemoveSelectionCircle(unit->sprite.get());
    bw::ModifyUnitCounters(unit, -1);
    if (unit->flags & UnitStatus::Completed)
        bw::ModifyUnitCounters2(unit, -1, 0);

    if ((*bw::is_placing_building) && !IsReplay() && (*bw::local_player_id == unit->player))
    {
        if (!bw::CanPlaceBuilding(*bw::primary_selected, *bw::placement_unit_id, *bw::placement_order))
        {
            bw::MarkPlacementBoxAreaDirty();
            bw::EndBuildingPlacement();
        }
    }
    RemoveFromLists(unit);
    RefreshUi();
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
            if (unit->OrderType() != OrderId::Die)
                unit->AskForHelp(attacker);
        }
    }
}

// Returns all attackers for which UnitWasHit has to be called.
static vector<Unit *> RemoveFromResults(Unit *unit, ProgressUnitResults *results)
{
    vector<Unit *> ret;
    std::for_each(results->weapon_damages.begin(), results->weapon_damages.end(), [unit, &ret](auto &a) {
        if (a.attacker == unit)
            a.attacker = nullptr;
        else if (a.target == unit && a.attacker != nullptr)
            ret.emplace_back(a.attacker);
    });
    auto wpn_last = std::remove_if(results->weapon_damages.begin(),
                                   results->weapon_damages.end(),
                                   [unit](const auto &a) { return a.target == unit; });
    results->weapon_damages.erase(wpn_last, results->weapon_damages.end());

    std::for_each(results->hallucination_hits.begin(),
                  results->hallucination_hits.end(),
                  [unit, &ret](auto &a)
    {
        if (a.attacker == unit)
            a.attacker = nullptr;
        else if (a.target == unit && a.attacker != nullptr)
            ret.emplace_back(a.attacker);
    });
    auto hallu_last = std::remove_if(results->hallucination_hits.begin(),
                                     results->hallucination_hits.end(),
                                     [unit](const auto &a) { return a.target == unit; });
    results->hallucination_hits.erase(hallu_last, results->hallucination_hits.end());

    std::sort(ret.begin(), ret.end(), [](const Unit *a, const Unit *b){ return a->lookup_id < b->lookup_id; });
    auto end = std::unique(ret.begin(), ret.end());
    ret.erase(end, ret.end());
    return ret;
}

static void TransferMainImage(Sprite *dest, Sprite *src)
{
    if (src == nullptr)
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

void Unit::Order_Die(ProgressUnitResults *results)
{
    Assert(sprite->first_overlay != nullptr);
    if (order_flags & 0x4) // Remove death
        bw::HideUnit(this);
    if (subunit != nullptr)
    {
        TransferMainImage(sprite.get(), subunit->sprite.get());
        subunit->Destroy(results);
        subunit = nullptr;
    }
    order_state = 1;
    for (Unit *attacker : RemoveFromResults(this, results))
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
            Die(this, results);
            return;
        }
        else
        {
            bw::PlaySound(Sound::HallucinationDeath, this, 1, 0);
            Sprite *death = lone_sprites->AllocateLone(SpriteId::HallucinationDeath, sprite->position, player);
            death->elevation = sprite->elevation + 1;
            bw::SetVisibility(death, sprite->visibility_mask);
            bw::HideUnit(this);
            Die(this, results);
        }
    }
    else
    {
        Die(this, results);
    }
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
        for (Unit *attacker : RemoveFromResults(this, results))
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
    bw::DropPowerup(this);
    while (order_queue_begin != nullptr)
    {
        DeleteOrder(order_queue_begin);
    }

    IssueOrderTargetingGround(OrderId::Die, order_target_pos);
    Ai::RemoveUnitAi(this, false);
}

void Unit::Remove(ProgressUnitResults *results)
{
    order_flags |= 0x4;
    Kill(results);
}

void Unit::Destroy(ProgressUnitResults *results)
{
    if (HasSubunit())
    {
        subunit->Destroy(results);
        subunit = nullptr;
    }
    Die(this, results);
    sprite->Remove();
    sprite = nullptr;
}
