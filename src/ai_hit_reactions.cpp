#include "ai_hit_reactions.h"

#include <windows.h>

#include "constants/order.h"
#include "constants/unit.h"
#include "constants/weapon.h"
#include "ai.h"
#include "bullet.h"
#include "limits.h"
#include "log.h"
#include "offsets.h"
#include "order.h"
#include "pathing.h"
#include "perfclock.h"
#include "player.h"
#include "unit.h"
#include "unitsearch.h"
#include "yms.h"

using std::get;

namespace Ai {

/// Wraps the target prioritization and storing it inside unit.
/// Separate class mostly for testing the logic without having
/// to modify any global state like HitReactions does.
class BestPickedTarget
{
    public:
        constexpr BestPickedTarget(Unit *unit) : unit(unit) { }
        Unit *GetPicked() { return unit->ai_reaction_private.picked_target; }
        void UpdatePicked(Unit *attacker) {
            if (unit->Ai_IsBetterTarget(attacker, GetPicked()))
                unit->ai_reaction_private.picked_target = attacker;
        }
        void Reset() { unit->ai_reaction_private.picked_target = nullptr; }

    private:
        Unit * const unit;
};


static bool IsUsableSpellOrder(OrderType order)
{
    using namespace OrderId;
    switch (order.Raw())
    {
        case YamatoGun:
        case Lockdown:
        case DarkSwarm:
        case SpawnBroodlings:
        case EmpShockwave:
        case PsiStorm:
        case Irradiate:
        case Plague:
        case Ensnare:
        case StasisField:
            return true;
        default:
            return false;
    }
}

void HitReactions::Reset()
{
    int region_count = (*bw::pathing)->region_count;
    ask_for_help_regions.clear();
    ask_for_help_regions.resize(region_count * Limits::ActivePlayers, nullptr);
    region_enemy_strength_updates.clear();
    region_enemy_strength_updates.resize(region_count * Limits::ActivePlayers, 0);
    helpers.clear();
    update_attack_targets.clear();
    is_valid = true;
}

void HitReactions::AskForHelp_AddRegion(int player, UnitList<Region *> ** region_list, int region, Region *attacker_region)
{
    if (!*region_list)
    {
        *region_list = pbf_memory.Allocate<UnitList<Region *>>();
        Region *this_region = bw::player_ai_regions[player] + region;
        this_region->flags |= 0x20;
    }
    (*region_list)->Add(attacker_region, &pbf_memory);
}

bool HitReactions::AskForHelp_CheckRegion(int player, int own_region, int region, Region *enemy_region, UnitList<Region *> *** region_list)
{
    if (region == own_region)
        return false;

    int region_count = (*bw::pathing)->region_count;
    *region_list = &ask_for_help_regions[player * region_count + region];
    if (**region_list)
    {
        auto it = (**region_list)->Begin();
        for (Region *reg = *it; reg; ++it, reg = *it)
        {
            if (reg == enemy_region)
            {
                return false;
            }
        }
    }
    return true;
}

bool HitReactions::AskForHelp_IsGood(Unit *unit, Unit *enemy, bool attacking_military)
{
    if (attacking_military)
    {
        if (unit->ai == nullptr || unit->ai->type != 4 || !IsInAttack(unit))
            return false;
        if (!unit->CanAttackUnit(enemy, true))
        {
            if ((unit->flags & UnitStatus::Burrowed) && !enemy->CanAttackUnit(unit, true)) // Might reaction spell?
                return false;
        }
        return true;
    }
    else
    {
        if (unit->ai && unit->ai->type == 1)
        {
            if (!unit->CanAttackUnit(enemy, true))
                return false;
            // At least have to do this for carrier/reaver
            if (enemy->IsFlying() && unit->GetTurret()->GetAirWeapon() == WeaponId::None )
                return false;
            if (!enemy->IsFlying() && unit->GetTurret()->GetGroundWeapon() == WeaponId::None )
                return false;

            int dist = Distance(enemy->sprite->position, ((Ai::GuardAi *)unit->ai)->home);
            if (dist > unit->GetWeaponRange(!enemy->IsFlying()) + 0xc0)
                return false;
        }
        return true;
    }
}

/// Practically checks if UnitWasHit would be no-op.
/// If UnitWasHit logic is ever modified, this has to be modified as well :s
bool HitReactions::AskForHelp_CheckIfDoesAnything(Unit *own)
{
    if (own->ai != nullptr)
    {
        if (own->ai->type == 4)
        {
            if (IsInAttack(own))
                return false;
        }
        else if (own->ai->type != 1 || !(bw::player_ai[own->player].flags & 0x20))
        {
            return false;
        }
    }
    return true;
}

void HitReactions::AskForHelp_CheckUnits(Unit *own, Unit *enemy, bool attacking_military, Unit **units)
{
    STATIC_PERF_CLOCK(Ai_AskForHelp_CheckUnits);
    Region *attacker_region = GetAiRegion(own->player, enemy->sprite->position);
    int own_region = own->GetRegion();
    for (Unit *unit = *units++; unit; unit = *units++)
    {
        if (unit->hitpoints == 0)
            continue;

        int region;
        UnitList<Region *> ** region_list;
        bool uninterruptable = unit->IsInUninterruptableState();
        // Optimization: Calling AddReaction with uninterruptable state barely does anything, so do it here
        // Not doing for buildings because there is some unk town stuff in start of AddReaction
        if (uninterruptable && (!unit->Type().IsBuilding() || unit->ai != nullptr))
        {
            region = unit->GetRegion();
            if (!AskForHelp_CheckRegion(unit->player, own_region, region, attacker_region, &region_list))
                break;

            if (AskForHelp_IsGood(unit, enemy, attacking_military))
            {
                if (unit->ai && unit->ai->type == 4)
                {
                    Region *ai_region = ((MilitaryAi *)unit->ai)->region;
                    // This building check is copied from ReactToHit, dunno if necessary
                    if (ai_region->state == 1 && !unit->Type().IsBuilding())
                        bw::ChangeAiRegionState(ai_region, 2);
                }
                if (AskForHelp_CheckIfDoesAnything(unit))
                {
                    AskForHelp_AddRegion(unit->player, region_list, region, attacker_region);
                    Region *unit_ai_region = bw::player_ai_regions[unit->player] + region;
                    if (unit_ai_region->state == 4 || unit_ai_region->state == 5)
                        bw::ChangeAiRegionState(unit_ai_region, 6);
                    if (attacker_region->state == 0 && unit_ai_region->state != 0 && *bw::elapsed_seconds > 60)
                        bw::ChangeAiRegionState(attacker_region, 5);
                }
            }
        }
        else
        {
            if (AskForHelp_IsGood(unit, enemy, attacking_military))
            {
                helpers.emplace_back(enemy, unit);
                if (AskForHelp_CheckIfDoesAnything(unit))
                {
                    region = unit->GetRegion();
                    if (AskForHelp_CheckRegion(unit->player, own_region, region, attacker_region, &region_list))
                        AskForHelp_AddRegion(unit->player, region_list, region, attacker_region);
                }
            }
        }
    }
}

// This is kinda different (faster, less bugs), but some unk ai region flags are not set if unit can't attack enemy
void HitReactions::AskForHelp(Unit *own, Unit *enemy, bool attacking_military)
{
    if (enemy->Type().IsWorker())
    {
        int search_radius = CallFriends_Radius;
        if (own->Type().IsBuilding())
            search_radius *= 2;
        if (bw::player_ai[own->player].flags & 0x20)
            search_radius *= 2;
        vector<Unit *> helping_workers;
        helping_workers.reserve(32);
        unit_search->ForEachUnitInArea(Rect16(own->sprite->position, search_radius), [&](Unit *unit) {
            if (unit->Type().IsWorker() && unit->ai && unit != own && unit->player == own->player)
                helping_workers.emplace_back(unit);
            return false;
        });
        helping_workers.emplace_back(nullptr);
        AskForHelp_CheckUnits(own, enemy, attacking_military, helping_workers.data());
    }
    Unit **units = own->nearby_helping_units.load(std::memory_order_relaxed);
    while (units == nullptr)
    {
        STATIC_PERF_CLOCK(Ai_AskForHelp_Hang);
        SwitchToThread(); // Bad?
        units = own->nearby_helping_units.load(std::memory_order_relaxed);
    }
    AskForHelp_CheckUnits(own, enemy, attacking_military, units);
}

/// See AskForHelp_CheckIfDoesAnything() if you are modifying the logic here.
bool HitReactions::UnitWasHit(Unit *own, Unit *attacker, bool important_hit, bool call_help)
{
    auto attacker_region = GetAiRegion(own->player, attacker->sprite->position);
    int player = own->player;

    bool ignore_transport = false;
    bool attacking = false;
    if (own->ai)
    {
        if (own->ai->type == 4) // Military
        {
            Region *ai_region = ((MilitaryAi *)own->ai)->region;
            if (ai_region->state == 1 && own->Type().IsBuilding())
                bw::ChangeAiRegionState(ai_region, 2);
            if (IsInAttack(own))
                attacking = true;
        }

        if (important_hit && own->IsTransport() && (own->flags & UnitStatus::Reacts))
        {
            if (own->HasLoadedUnits())
            {
                if (own->order != OrderId::Unload)
                    own->IssueOrderTargetingNothing(OrderId::Unload);
                return true;
            }
            else
            {
                if (!own->Ai_TryReturnHome(false))
                {
                    if (bw::Ai_ReturnToNearestBaseForced(own))
                        return true;
                }
                if (bw::player_ai[player].flags & 0x20)
                    ignore_transport = true;
            }
        }
    }

    Region *target_region = GetAiRegion(own);
    if (!ignore_transport)
    {
        if (call_help)
            AskForHelp(own, attacker, attacking);

        if (!attacking)
        {
            if (own->ai && own->ai->type != 4) // || !IsInAttack(own), but it is always false
            {
                if (own->ai->type != 1 || !(bw::player_ai[player].flags & 0x20))
                {
                    if (target_region->state == 4 || target_region->state == 5)
                        bw::ChangeAiRegionState(target_region, 6);
                    if (target_region->state != 0 && attacker_region->state == 0 && *bw::elapsed_seconds > 60)
                        bw::ChangeAiRegionState(attacker_region, 5);
                }
            }
            if (call_help)
            {
                int region_count = (*bw::pathing)->region_count;
                region_enemy_strength_updates[player * region_count + attacker_region->region_id] = 1;
                if (target_region->needed_ground_strength < attacker_region->needed_ground_strength)
                    target_region->needed_ground_strength = attacker_region->needed_ground_strength;
                if (target_region->needed_air_strength < attacker_region->needed_air_strength)
                    target_region->needed_air_strength = attacker_region->needed_air_strength;
            }
        }
    }
    target_region->flags |= 0x20;
    return false;
}

void HitReactions::UpdatePickedTarget(Unit *own, Unit *attacker)
{
    const UpdateAttackTargetContext ctx(own, true, own->order == OrderId::Pickup4);

    if (ctx.CheckPreviousAttackerValid(attacker) != nullptr)
    {
        BestPickedTarget picked_target(own);
        Unit *previous_picked = picked_target.GetPicked();
        picked_target.UpdatePicked(attacker);
        if (previous_picked == nullptr)
            update_attack_targets.emplace_back(own);
    }
}

void HitReactions::React(Unit *own, Unit *attacker, bool important_hit)
{
    bool uninterruptable = own->IsInUninterruptableState();
    if (uninterruptable && !important_hit)
        return;

    OrderType order(own->order);

    if (~own->flags & UnitStatus::Completed || order == OrderId::CompletingArchonSummon ||
        order == OrderId::ResetCollision1 || order == OrderId::ConstructingBuilding)
    {
        return;
    }
    if (!important_hit && IsUsableSpellOrder(order))
        return;
    if (own->Type() == UnitId::Marine && order == OrderId::EnterTransport) // What?
        return;

    if (TryReactionSpell(own, important_hit))
        return;

    if (order == OrderId::RechargeShieldsUnit || order == OrderId::Move)
        return;

    if (important_hit)
    {
        if (TryDamagedFlee(own, attacker))
            return;
    }

    if (own->target == attacker)
        return;

    if (important_hit && own->ai && attacker->IsInvisibleTo(own))
        bw::Ai_Detect(own, attacker);
    if (uninterruptable)
        return;

    if (own->CanAttackUnit(attacker, true))
    {
        UpdatePickedTarget(own, attacker);
    }
    else if (own->ai)
    {
        if (order == OrderId::AiPatrol)
        {
            uint32_t flee_pos_dword = bw::PrepareFlee(own, attacker);
            Point flee_pos(flee_pos_dword & 0xffff, flee_pos_dword >> 16);
            if (flee_pos != own->sprite->position)
            {
                own->IssueOrderTargetingGround(OrderId::Move, flee_pos);
                return;
            }
        }
        if (important_hit && own->Type() != UnitId::Medic)
        {
            if (!own->target || !own->CanAttackUnit(own->target, true))
            {
                bw::Flee(own, attacker);
            }
        }
    }
    return;
}

void HitReactions::AddReaction(Unit *own, Unit *attacker, bool important_hit, bool call_help)
{
    bool skip = UnitWasHit(own, attacker, important_hit, call_help);
    if (!skip) {
        React(own, attacker, important_hit);
    }
}

void HitReactions::NewHit(Unit *own, Unit *attacker, bool important_hit)
{
    if (!IsComputerPlayer(own->player))
        return;

    STATIC_PERF_CLOCK(AiHitReactions_NewHit);
    if ((attacker->flags & UnitStatus::FreeInvisibility) && !(attacker->flags & UnitStatus::Burrowed))
    {
        attacker = bw::FindNearestUnitOfId(own, UnitId::Arbiter.Raw()); // No danimoth <.<
        if (!attacker)
            return;
    }
    else
        attacker = own->GetActualTarget(attacker);

    AddReaction(own, attacker, important_hit, true);
}

void HitReactions::ProcessEverything()
{
    ProcessAskForHelp();
    ProcessUpdateAttackTarget();
    UpdateRegionEnemyStrengths();
    is_valid = false;
}

void HitReactions::ProcessAskForHelp()
{
    std::sort(helpers.begin(), helpers.end(), [](const auto &a, const auto &b) {
        if (get<1>(a)->lookup_id == get<1>(b)->lookup_id) // Helper
            return get<0>(a)->lookup_id < get<0>(b)->lookup_id; // Enemy
        return get<1>(a)->lookup_id < get<1>(b)->lookup_id;
    });
    for (const auto &pair : helpers.Unique())
    {
        Unit *enemy = get<0>(pair);
        Unit *helper = get<1>(pair);
        AddReaction(helper, enemy, false, false);
    }
}

void HitReactions::ProcessUpdateAttackTarget()
{
    for (Unit *own : update_attack_targets)
    {
        BestPickedTarget picked_target(own);
        Unit *picked = picked_target.GetPicked();
        if (picked != nullptr)
        {
            // The picked unit may have become invalid as a previous_attacker
            // under rare conditions. One such case would be giving attack to
            // a dropship, but having an ai dropship be hit on the same frame,
            // so it starts unloading units and loses its target.
            // Should either delay the unloading, allow UpdateAttackTarget skip
            // the validity check, or have a way to pick another unit.
            own->SetPreviousAttacker(picked);
            if (own->HasSubunit())
                own->subunit->SetPreviousAttacker(picked);
            UpdateAttackTarget(own, false, true, false);
            picked_target.Reset();
        }
    }
}

void HitReactions::UpdateRegionEnemyStrengths()
{
    int region_count = (*bw::pathing)->region_count;
    for (uint32_t player = 0; player < Limits::ActivePlayers; player++)
    {
        uint32_t i = player * region_count;
        for (uint32_t region_id = 0; region_id < region_count; region_id++)
        {
            if (region_enemy_strength_updates[i + region_id] != 0)
            {
                Region *region = bw::player_ai_regions[player] + region_id;
                region->enemy_air_strength = bw::GetEnemyAirStrength(region_id, player);
                region->enemy_ground_strength = bw::GetEnemyStrength(region_id, player, false);
            }
        }
    }
}

Unit *GetBestTarget(Unit *unit, const vector<Unit *> &units)
{
    BestPickedTarget picked_target(unit);
    for (Unit *other : units)
    {
        picked_target.UpdatePicked(other);
    }
    Unit *ret = picked_target.GetPicked();
    picked_target.Reset();
    return ret;
}

/// For each unit, picks all their enemies and confirms that BestPickedTarget
/// uses same logic as just calling UpdateAttackTarget repeatedly.
/// Logs and debug breaks any cases where it is not true, and obviously
/// causes all units to attack enemies by their logic.
bool TestBestTargetPicking()
{
    vector<Unit *> units;
    for (Unit *unit : *bw::first_active_unit)
    {
        if (!IsComputerPlayer(unit->player) || unit->ai == nullptr)
            continue;
        if (unit->Type().IsWorker() || ~unit->flags & UnitStatus::Completed)
            continue;

        units.clear();
        for (Unit *other : *bw::first_active_unit)
        {
            const UpdateAttackTargetContext ctx(unit, true, unit->order == OrderId::Pickup4);
            if (unit->IsEnemy(other) && ctx.CheckPreviousAttackerValid(other) != nullptr)
                units.emplace_back(other);
        }
        Unit *assumed = GetBestTarget(unit, units);
        Unit *old_target = unit->target;
        Unit *air_target = unit->Ai_ChooseAirTarget();
        Unit *ground_target = unit->Ai_ChooseGroundTarget();
        for (Unit *other : units)
        {
            unit->SetPreviousAttacker(other);
            if (unit->HasSubunit())
                unit->subunit->SetPreviousAttacker(other);
            UpdateAttackTarget(unit, false, true, false);
        }
        unit_search->valid_region_cache = false;
        Unit *picked = unit->target;
        // If the auto target is not a threat, it can cause UpdateAttackTarget switch
        // back and forth weirdly (see Test_AiTargetPriority).
        if (ground_target != nullptr && unit->IsThreat(ground_target))
            continue;
        if (air_target != nullptr && unit->IsThreat(air_target))
            continue;

        if (picked != assumed && picked != old_target && picked != air_target && picked != ground_target)
        {
            debug_log->Log("Best target picking error for unit %08X (%p)\n", unit->lookup_id, unit);
            int pos = 0;
            for (Unit *other : units)
            {
                if (pos == 8)
                {
                    debug_log->Log("\n");
                    pos = 0;
                }
                debug_log->Log("%08X ", other->lookup_id);
                pos++;
            }
            debug_log->Log("\n");
            uint32_t picked_id = 0, assumed_id = 0;
            if (picked != nullptr)
                picked_id = picked->lookup_id;
            if (assumed != nullptr)
                assumed_id = assumed->lookup_id;
            debug_log->Log("Picked %08X (%p), assumed %08X (%p)\n", picked_id, picked, assumed_id, assumed);
            return false;
        }
    }
    return true;
}

} // namespace Ai
