#include "ai_hit_reactions.h"

#include "offsets.h"
#include "limits.h"
#include "perfclock.h"
#include "yms.h"
#include "ai.h"
#include "bullet.h"
#include "unit.h"
#include "unitsearch.h"
#include "order.h"
#include "pathing.h"

#include <windows.h>

using std::get;

namespace Ai {

static bool IsUsableSpellOrder(int order)
{
    switch (order)
    {
        case Order::YamatoGun:
        case Order::Lockdown:
        case Order::DarkSwarm:
        case Order::SpawnBroodlings:
        case Order::EmpShockwave:
        case Order::PsiStorm:
        case Order::Irradiate:
        case Order::Plague:
        case Order::Ensnare:
        case Order::StasisField:
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
            if (enemy->IsFlying() && unit->GetTurret()->GetAirWeapon() == Weapon::None )
                return false;
            if (!enemy->IsFlying() && unit->GetTurret()->GetGroundWeapon() == Weapon::None )
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
    if (own->ai)
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
        if (uninterruptable && (~units_dat_flags[unit->unit_id] & UnitFlags::Building || unit->ai))
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
                    if (ai_region->state == 1 && ~units_dat_flags[unit->unit_id] & UnitFlags::Building)
                        ChangeAiRegionState(ai_region, 2);
                }
                if (AskForHelp_CheckIfDoesAnything(unit))
                {
                    AskForHelp_AddRegion(unit->player, region_list, region, attacker_region);
                    Region *unit_ai_region = bw::player_ai_regions[unit->player] + region;
                    if (unit_ai_region->state == 4 || unit_ai_region->state == 5)
                        ChangeAiRegionState(unit_ai_region, 6);
                    if (attacker_region->state == 0 && unit_ai_region->state != 0 && *bw::elapsed_seconds > 60)
                        ChangeAiRegionState(attacker_region, 5);
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
    if (enemy->IsWorker())
    {
        int search_radius = CallFriends_Radius;
        if (units_dat_flags[own->unit_id] & UnitFlags::Building)
            search_radius *= 2;
        if (bw::player_ai[own->player].flags & 0x20)
            search_radius *= 2;
        vector<Unit *> helping_workers;
        helping_workers.reserve(32);
        unit_search->ForEachUnitInArea(Rect16(own->sprite->position, search_radius), [&](Unit *unit) {
            if (unit->IsWorker() && unit->ai && unit != own && unit->player == own->player)
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
void HitReactions::UnitWasHit(Unit *own, Unit *attacker, bool important_hit, bool call_help)
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
            if (ai_region->state == 1 && !(units_dat_flags[own->unit_id] & UnitFlags::Building))
                ChangeAiRegionState(ai_region, 2);
            if (IsInAttack(own))
                attacking = true;
        }

        if (important_hit && own->IsTransport() && (own->flags & UnitStatus::Reacts))
        {
            if (own->HasLoadedUnits())
            {
                if (own->order != Order::Unload)
                    IssueOrderTargetingNothing(own, Order::Unload);
                return;
            }
            else
            {
                if (!own->Ai_TryReturnHome(false))
                {
                    if (Ai_ReturnToNearestBaseForced(own))
                        return;
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
                        ChangeAiRegionState(target_region, 6);
                    if (target_region->state != 0 && attacker_region->state == 0 && *bw::elapsed_seconds > 60)
                        ChangeAiRegionState(attacker_region, 5);
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
    return;
}

void HitReactions::UpdatePickedTarget(Unit *own, Unit *attacker)
{
    bool must_reach = own->order == Order::Pickup4;
    if (must_reach && own->IsUnreachable(attacker))
        return;

    if (!attacker->IsDisabled())
    {
        if ((attacker->unit_id != Unit::Bunker) && (attacker->target == nullptr || attacker->target->player != own->player))
            return;
        if (!own->CanAttackUnit(attacker, true))
            return;
    }

    Unit *previous_picked = own->ai_reaction_private.picked_target;
    if (own->Ai_IsBetterTarget(attacker, previous_picked))
        own->ai_reaction_private.picked_target = attacker;
    if (previous_picked == nullptr)
        update_attack_targets.emplace_back(own);
}

void HitReactions::React(Unit *own, Unit *attacker, bool important_hit)
{
    bool uninterruptable = own->IsInUninterruptableState();
    if (uninterruptable && !important_hit)
        return;

    int order = own->order;

    if (~own->flags & UnitStatus::Completed || order == Order::CompletingArchonSummon ||
        order == Order::ResetCollision1 || order == Order::ConstructingBuilding)
    {
        return;
    }
    if (!important_hit && IsUsableSpellOrder(order))
        return;
    if (own->unit_id == Unit::Marine && order == Order::EnterTransport) // What?
        return;

    if (TryReactionSpell(own, important_hit))
        return;

    if (order == Order::RechargeShieldsUnit || order == Order::Move)
        return;

    if (important_hit)
    {
        if (TryDamagedFlee(own, attacker))
            return;
    }

    if (own->target == attacker)
        return;

    if (important_hit && own->ai && attacker->IsInvisibleTo(own))
        Ai_Detect(own, attacker);
    if (uninterruptable)
        return;

    if (own->CanAttackUnit(attacker, true))
    {
        UpdatePickedTarget(own, attacker);
    }
    else if (own->ai)
    {
        if (order == Order::AiPatrol)
        {
            uint32_t flee_pos = PrepareFlee(own, attacker);
            if ((flee_pos & 0xffff) != own->sprite->position.x || (flee_pos >> 16) != own->sprite->position.y)
            {
                IssueOrder(own, Order::Move, flee_pos, 0);
                return;
            }
        }
        if (important_hit && own->unit_id != Unit::Medic)
        {
            if (!own->target || !own->CanAttackUnit(own->target, true))
            {
                Flee(own, attacker);
            }
        }
    }
    return;
}

void HitReactions::AddReaction(Unit *own, Unit *attacker, bool important_hit, bool call_help)
{
    UnitWasHit(own, attacker, important_hit, call_help);
    React(own, attacker, important_hit);
}

void HitReactions::NewHit(Unit *own, Unit *attacker, bool important_hit)
{
    if (!IsComputerPlayer(own->player))
        return;

    STATIC_PERF_CLOCK(AiHitReactions_NewHit);
    if ((attacker->flags & UnitStatus::FreeInvisibility) && !(attacker->flags & UnitStatus::Burrowed))
    {
        attacker = FindNearestUnitOfId(own, Unit::Arbiter); // No danimoth <.<
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
        Unit *picked = own->ai_reaction_private.picked_target;
        if (picked != nullptr)
        {
            own->SetPreviousAttacker(picked);
            if (own->HasSubunit())
                own->subunit->SetPreviousAttacker(picked);
            UpdateAttackTarget(own, false, true, false);
            own->ai_reaction_private.picked_target = nullptr;
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
                region->enemy_air_strength = GetEnemyAirStrength(region_id, player);
                region->enemy_ground_strength = GetEnemyStrength(region_id, player, false);
            }
        }
    }
}

} // namespace Ai
