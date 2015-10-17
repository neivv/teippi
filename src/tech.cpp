#include "tech.h"

#include "unit.h"
#include "sprite.h"
#include "offsets.h"
#include "unitsearch.h"
#include "patchmanager.h"
#include "sound.h"
#include "bullet.h"
#include "image.h"
#include "yms.h"
#include "ai.h"
#include "strings.h"
#include "rng.h"
#include "constants/string.h"

int GetTechLevel(int tech, int player)
{
    Assert(tech >= 0 && tech < Tech::None);
    if (tech < 0x18)
        return bw::tech_level_sc[player][tech];
    else
        return bw::tech_level_bw[player][(tech - 0x18)];
}

void SetTechLevel(int tech, int player, int amount)
{
    Assert(tech >= 0 && tech < Tech::None);
    if (tech < 0x18)
        bw::tech_level_sc[player][tech] = amount;
    else
        bw::tech_level_bw[player][(tech - 0x18)] = amount;
}

const char *GetTechName(int tech)
{
    return (*bw::stat_txt_tbl)->GetTblString(techdata_dat_label[tech]);
}

void Stasis(Unit *attacker, const Point &position)
{
    Sprite *stasis = lone_sprites->AllocateLone(Sprite::StasisField, position, 0);
    stasis->elevation = Spell::StasisElevation;
    stasis->UpdateVisibilityPoint();

    Rect16 area(position, Spell::StasisArea);
    unit_search->ForEachUnitInArea(area, [attacker](Unit *unit)
    {
        if (unit == attacker || unit->IsInvincible() || unit->flags & UnitStatus::Burrowed)
            return false;
        if (units_dat_flags[unit->unit_id] & UnitFlags::Building)
            return false;
        ApplyStasis(unit, Spell::StasisTime);
        return false;
    });
}

void Ensnare(Unit *attacker, const Point &position)
{
    Sprite *ensnare = lone_sprites->AllocateLone(Sprite::Ensnare, position, 0);
    ensnare->elevation = Spell::EnsnareElevation;
    ensnare->UpdateVisibilityPoint();

    Rect16 area(position, Spell::EnsnareArea);
    unit_search->ForEachUnitInArea(area, [attacker](Unit *unit)
    {
        if (unit == attacker || unit->IsInvincible() || unit->flags & UnitStatus::Burrowed)
            return false;
        if (units_dat_flags[unit->unit_id] & UnitFlags::Building)
            return false;
        ApplyEnsnare(unit);
        return false;
    });
}

void Maelstrom(Unit *attacker, const Point &position)
{
    Sprite *mael = lone_sprites->AllocateLone(Sprite::Maelstrom, position, 0);
    mael->elevation = Spell::MaelstromElevation;
    mael->UpdateVisibilityPoint();

    PlaySound(Sound::MaelstromHit, attacker, 1, 0);

    Rect16 area(position, Spell::MaelstromArea);
    unit_search->ForEachUnitInArea(area, [attacker](Unit *unit)
    {
        if (unit->flags & UnitStatus::Hallucination)
        {
            unit->Kill(nullptr);
            return false;
        }
        if (unit == attacker || unit->IsInvincible() || unit->flags & UnitStatus::Burrowed)
            return false;
        if (units_dat_flags[unit->unit_id] & UnitFlags::Building)
            return false;
        if (~units_dat_flags[unit->unit_id] & UnitFlags::Organic)
            return false;
        ApplyMaelstrom(unit, Spell::MaelstromTime);
        return false;
    });
}

void UpdateDwebStatuses()
{
    for (Unit *unit = *bw::first_active_unit; unit; unit = unit->next())
    {
        if (unit->IsFlying() && ~unit->flags & UnitStatus::UnderDweb) // Uhm..
            continue;
        unit->flags &= ~UnitStatus::UnderDweb;
        if (unit->subunit != nullptr)
            unit->subunit->flags &= ~UnitStatus::UnderDweb;
        for (Unit *child = unit->first_loaded; child; child = child->next_loaded)
        {
            child->flags &= ~UnitStatus::UnderDweb;
        }
    }
    if (bw::completed_units_count[Unit::DisruptionWeb][NeutralPlayer])
    {
        for (Unit *dweb : bw::first_player_unit[NeutralPlayer])
        {
            if (dweb->unit_id != Unit::DisruptionWeb)
                continue;

            Rect16 area;
            area.left = dweb->sprite->position.x - units_dat_dimensionbox[Unit::DisruptionWeb].left;
            area.top = dweb->sprite->position.y - units_dat_dimensionbox[Unit::DisruptionWeb].top;
            area.right = dweb->sprite->position.x + units_dat_dimensionbox[Unit::DisruptionWeb].right;
            area.bottom = dweb->sprite->position.y + units_dat_dimensionbox[Unit::DisruptionWeb].bottom;
            unit_search->ForEachUnitInArea(area, [](Unit *unit) {
                if (!unit->IsFlying())
                {
                    unit->flags |= UnitStatus::UnderDweb;
                    if (unit->subunit != nullptr)
                        unit->subunit->flags |= UnitStatus::UnderDweb;
                    for (Unit *child = unit->first_loaded; child; child = child->next_loaded)
                    {
                        child->flags |= UnitStatus::UnderDweb;
                    }
                }
                return false;
            });
        }
    }
}

void Unit::Order_Feedback(ProgressUnitResults *results)
{
    // Sc seems to actually check if (target && target->HasEnergy())
    // Which prevents casting on hallucinations
    // Well there's no hallu killing code because I was too lazy to do it
    if (target && units_dat_flags[target->unit_id] & UnitFlags::Spellcaster && ~target->flags & UnitStatus::Hallucination)
    {
        uint16_t energy_cost;
        if (SpellOrder(this, Tech::Feedback, GetSightRange(true) * 32, &energy_cost, String::Error_MustTargetSpellcaster))
        {
            // Hallu kill code would be here
            if (target->energy)
            {
                bool dead = target->GetHealth() << 8 <= target->energy;
                results->weapon_damages.emplace_back(this, player, target, target->energy, Weapon::Feedback, 1);
                target->energy = 0;
                ReduceEnergy(energy_cost);
                PlaySound(Sound::Feedback, this, 1, 0);
                if (dead)
                    target->sprite->SpawnLoneSpriteAbove(Sprite::FeedbackHit_Small + target->GetSize());
                else
                {
                    target->AddSpellOverlay(Image::FeedbackHit_Small);
                }
            }
            OrderDone();
        }
    }
    else
    {
        OrderDone();
    }
}

// Returns true if there's need for unit_was_hit
static void IrradiateHit(Unit *irradiated, Unit *victim, ProgressUnitResults *results)
{
    int unit_id = victim->unit_id;
    if (~units_dat_flags[unit_id] & UnitFlags::Organic || units_dat_flags[unit_id] & UnitFlags::Building)
        return;
    if (unit_id == Unit::Larva || unit_id == Unit::Egg || unit_id == Unit::LurkerEgg)
        return;
    if (victim->flags & UnitStatus::Burrowed && irradiated != victim)
        return;
    if (~irradiated->flags & UnitStatus::InTransport && !IsInArea(irradiated, 0x20, victim))
        return;
    int damage = weapons_dat_damage[Weapon::Irradiate] * 256 / weapons_dat_cooldown[Weapon::Irradiate];
    results->weapon_damages.emplace_back(irradiated->irradiated_by, irradiated->irradiate_player, victim, damage, Weapon::Irradiate, 0);
}

void DoMatrixDamage(Unit *target, int dmg)
{
    if (dmg < target->matrix_hp)
    {
        target->matrix_hp -= dmg;
        if (~target->flags & UnitStatus::Burrowed)
        {
            target->AddSpellOverlay(Image::DefensiveMatrixHit_Small);
        }
    }
    else
    {
        target->matrix_hp = 0;
        target->matrix_timer = 0;
        target->RemoveOverlayFromSelfOrSubunit(Image::DefensiveMatrixFront_Small, Image::DefensiveMatrixFront_Small + 2);
        target->RemoveOverlayFromSelf(Image::DefensiveMatrixBack_Small, Image::DefensiveMatrixBack_Small + 2);
    }
}

void Unit::DoIrradiateDamage(ProgressUnitResults *results)
{
    if (flags & UnitStatus::Burrowed)
    {
        IrradiateHit(this, this, results);
    }
    else if (flags & UnitStatus::InTransport)
    {
        for (Unit *unit = related->first_loaded; unit; unit = unit->next_loaded)
            IrradiateHit(this, unit, results);
    }
    else
    {
        Rect16 area(sprite->position, Spell::IrradiateArea);
        Unit **units = unit_search->FindUnitBordersRect(&area);
        for (Unit *unit = *units++; unit; unit = *units++)
            IrradiateHit(this, unit, results);
        unit_search->PopResult();
    }
}

void Unit::ProgressSpellTimers(ProgressUnitResults *results)
{
    if (stasis_timer && --stasis_timer == 0)
        EndStasis(this);
    if (stim_timer && --stim_timer == 0)
        UpdateSpeed(this);
    if (ensnare_timer && --ensnare_timer == 0)
    {
        RemoveOverlayFromSelfOrSubunit(Image::Ensnare_Small, Image::Ensnare_Small + 2);
        UpdateSpeed(this);
    }
    if (matrix_timer && --matrix_timer == 0)
        DoMatrixDamage(this, matrix_hp);
    if (irradiate_timer)
    {
        irradiate_timer--;
        DoIrradiateDamage(results);
        if (irradiate_timer == 0)
        {
            RemoveOverlayFromSelfOrSubunit(Image::Irradiate_Small, Image::Irradiate_Small + 2);
            irradiated_by = nullptr;
            irradiate_player = 8;
        }
    }
    if (lockdown_timer && --lockdown_timer == 0)
        EndLockdown(this);
    if (mael_timer && --mael_timer == 0)
        EndMaelstrom(this);
    if (plague_timer)
    {
        plague_timer--;
        if (!IsInvincible())
        {
            int damage = weapons_dat_damage[Weapon::Plague] / (Spell::PlagueTime + 1) * 256;
            if (damage < hitpoints)
                DamageSelf(damage, results);
        }
        if (plague_timer == 0)
        {
            RemoveOverlayFromSelfOrSubunit(Image::Plague_Small, Image::Plague_Small + 2);
        }
    }
    if (is_under_storm)
        is_under_storm--;
    ProgressAcidSporeTimers(this);
}

void Unit::Lockdown(int time)
{
    if (!lockdown_timer)
        AddSpellOverlay(Image::Lockdown_Small);

    if (lockdown_timer < time)
        lockdown_timer = time;
    DisableUnit(this);
}


void Unit::Parasite(int player)
{
    PlaySound(Sound::Parasite, target, 1, 0);
    parasites |= 1 << player;
    RefreshUi();
}

void Unit::SpawnBroodlings(const Point &pos_)
{
    Point pos = pos_;
    for (int i = 0; i < Spell::BroodlingCount; i++)
    {
        if (!DoesFitHere(Unit::Broodling, pos.x, pos.y))
        {
            Rect16 area(pos, 0x20);
            uint16_t new_pos[2], old_pos[2] = { pos.x, pos.y };
            if (!GetFittingPosition(&area, this, old_pos, new_pos, 0, 0)) // No clue why it uses the queen <.<
                continue;
            pos.x = new_pos[0];
            pos.y = new_pos[1];
        }
        uint16_t pos_arr[2] = { pos.x, pos.y };
        ClipPointInBoundsForUnit(Unit::Broodling, pos_arr);

        Unit *broodling = CreateUnit(Unit::Broodling, pos_arr[0], pos_arr[1], player);
        if (!broodling)
        {
            ShowLastError(player);
            continue;
        }
        FinishUnit_Pre(broodling);
        FinishUnit(broodling);
        broodling->death_timer = Spell::BroodlingDeathTimer;
        if (ai)
            InheritAi2(this, broodling);

        pos += Point(4, 4);
    }
}

void EmpShockwave(Unit *attacker, const Point &position)
{
    Rect16 area(position, weapons_dat_inner_splash[Weapon::EmpShockwave]);
    // Dunno why bw does area checks here but not with other spells
    if (area.top > area.bottom)
        area.top = 0;
    else if (area.bottom > *bw::map_height)
        area.bottom = *bw::map_height;
    if (area.left > area.right)
        area.left = 0;
    else if (area.right > *bw::map_width)
        area.right = *bw::map_width;

    unit_search->ForEachUnitInArea(area, [attacker](Unit *unit)
    {
        if (unit != attacker || !attacker || unit != attacker->subunit)
        {
            if (unit->flags & UnitStatus::Hallucination)
                unit->Kill(nullptr);
            else if (unit->stasis_timer == 0)
            {
                unit->shields = 0;
                unit->energy = 0;
                unit->sprite->MarkHealthBarDirty();
            }
        }
        return false;
    });
}

void Unit::Irradiate(Unit *attacker, int attacking_player)
{
    if (!irradiate_timer && ~flags & UnitStatus::Burrowed)
    {
        AddSpellOverlay(Image::Irradiate_Small);
    }
    irradiate_timer = weapons_dat_cooldown[Weapon::Irradiate];
    irradiated_by = attacker;
    irradiate_player = attacking_player;
}

void Plague(Unit *attacker, const Point &position, vector<tuple<Unit *, Unit *>> *unit_was_hit)
{
    unit_search->ForEachUnitInArea(Rect16(position, Spell::PlagueArea), [attacker, unit_was_hit](Unit *unit)
    {
        if (unit != attacker && !unit->IsInvincible() && ~unit->flags & UnitStatus::Burrowed)
        {
            if (!unit->plague_timer)
            {
                unit->AddSpellOverlay(Image::Plague_Small);
            }
            unit->plague_timer = Spell::PlagueTime;
            if (attacker && UnitWasHit(unit, attacker, true))
                unit_was_hit->emplace_back(unit, attacker);
        }
        return false;
    });
}

void DarkSwarm(int player, const Point &position)
{
    uint16_t pos[2] = { position.x, position.y };
    ClipPointInBoundsForUnit(Unit::DarkSwarm, pos);
    Unit *swarm = CreateUnit(Unit::DarkSwarm, pos[0], pos[1], NeutralPlayer);
    if (!swarm)
    {
        ShowLastError(player);
        return;
    }
    swarm->sprite->elevation = Spell::DarkSwarmElevation;
    swarm->flags |= UnitStatus::NoCollision;
    FinishUnit_Pre(swarm);
    FinishUnit(swarm);
    swarm->death_timer = Spell::DarkSwarmTime;
}

void DisruptionWeb(int player, const Point &position)
{
    uint16_t pos[2] = { position.x, position.y };
    ClipPointInBoundsForUnit(Unit::DisruptionWeb, pos);
    Unit *dweb = CreateUnit(Unit::DisruptionWeb, pos[0], pos[1], NeutralPlayer);
    if (!dweb)
    {
        ShowLastError(player);
        return;
    }
    dweb->sprite->elevation = Spell::DisruptionWebElevation;
    dweb->flags |= UnitStatus::NoCollision;
    FinishUnit_Pre(dweb);
    FinishUnit(dweb);
    dweb->death_timer = Spell::DisruptionWebTime;
}

void Unit::Consume(Unit *target, vector<Unit *> *killed_units)
{
    if (!target->IsInvincible() && CanTargetSpellOnUnit(Tech::Consume, target, player))
    {
        IncrementKillScores(target, player);
        killed_units->emplace_back(target);
        if (~flags & UnitStatus::Hallucination)
        {
            energy += Spell::ConsumeEnergy;
            if (energy > GetMaxEnergy())
                energy = GetMaxEnergy();
        }
    }
}

void Unit::RemoveAcidSpores()
{
    if (!acid_spore_count)
        return;
    acid_spore_count = 0;
    memset(acid_spore_timers, 0, 9);
    RemoveOverlayFromSelfOrSubunit(Image::AcidSpore_Small1, Image::AcidSpore_Small1 + 11);
}

void Unit::Restoration()
{
    PlaySound(Sound::Restoration, this, 1, 0);
    if (flags & UnitStatus::Hallucination)
    {
        Kill(nullptr);
        return;
    }
    AddSpellOverlay(Image::Restoration_Small);
    if (ensnare_timer)
    {
        RemoveOverlayFromSelfOrSubunit(Image::Ensnare_Small, Image::Ensnare_Small + 2);
        ensnare_timer = 0;
        UpdateSpeed(this);
    }
    if (plague_timer)
    {
        RemoveOverlayFromSelfOrSubunit(Image::Plague_Small, Image::Plague_Small + 2);
        plague_timer = 0;
    }
    if (irradiate_timer)
    {
        RemoveOverlayFromSelfOrSubunit(Image::Irradiate_Small, Image::Irradiate_Small + 2);
        irradiate_timer = 0;
        irradiated_by = 0;
        irradiate_player = 8;
    }
    if (lockdown_timer)
        EndLockdown(this);
    if (mael_timer)
        EndMaelstrom(this);
    if (acid_spore_count)
        RemoveAcidSpores();

    parasites = 0;
    blind = 0;
    RefreshUi();
}

void Unit::OpticalFlare(int attacking_player)
{
    if (flags & UnitStatus::Hallucination)
    {
        Kill(nullptr);
        return;
    }
    blind |= 1 << attacking_player; // Wow
    PlaySound(Sound::OpticalFlare, this, 1, 0);
    AddSpellOverlay(Image::OpticalFlareHit_Small);
    RefreshUi();
}

void Unit::Order_Recall()
{
    if (order_state == 0)
    {
        if (IsCheatActive(Cheats::The_Gathering) || *bw::current_energy_req * 256 <= energy)
        {
            ReduceEnergy(*bw::current_energy_req * 256);
            if (target)
                order_target_pos = target->sprite->position;
            Sprite *recall = lone_sprites->AllocateLone(Sprite::Recall, order_target_pos, player);
            recall->elevation = sprite->elevation + 1;
            recall->UpdateVisibilityPoint();
            PlaySoundAtPos(Sound::Recall + Rand(0x11) % 2, order_target_pos.AsDword(), 1, 0);
            order_timer = Spell::RecallTime;
            order_state = 1;
        }
        else
        {
            if (player == *bw::local_player_id && *bw::select_command_user == *bw::local_unique_player_id)
                PrintInfoMessage((*bw::stat_txt_tbl)->GetTblString(String::NotEnoughEnergy + GetRace()));
            OrderDone();
        }
    }
    else if (!order_timer)
    {
        bool recalled_something = false;
        unit_search->ForEachUnitInArea(Rect16(order_target_pos, Spell::RecallArea), [this, &recalled_something](Unit *unit)
        {
            if (unit->sprite->IsHidden() || player != unit->player || unit->IsInvincible())
                return false;
            if (units_dat_flags[unit->unit_id] & UnitFlags::Building || unit->flags & UnitStatus::Burrowed)
                return false;
            if (unit->unit_id == Larva || unit->unit_id == Egg || unit->unit_id == LurkerEgg)
                return false;
            if (unit == this || unit->flags & UnitStatus::Hallucination)
                return false;
            Recall(unit);
            recalled_something = true;
            return false;
        });
        if (recalled_something)
            PlaySound(Sound::Recalled + Rand(0x11) % 2, this, 1, 0);
        OrderDone();
    }
}

void Unit::Recall(Unit *other)
{
    Point old_pos = other->sprite->position;
    MoveUnit(other, sprite->position.x, sprite->position.y);
    uint16_t pos[2] = { sprite->position.x, sprite->position.y }, new_pos[2];
    if (GetFittingPosition(nullptr, other, pos, new_pos, 0, 0) == 0)
    {
        MoveUnit(other, old_pos.x, old_pos.y);
        return;
    }
    HideUnit_Partial(other, 0);
    other->target = nullptr;
    MoveUnit(other, new_pos[0], new_pos[1]); // Huh?
    FinishMoveUnit(other);
    if (other->unit_id != Cocoon)
        IssueOrderTargetingNothing(other, units_dat_return_to_idle_order[other->unit_id]);
    Sprite *recall = lone_sprites->AllocateLone(Sprite::Recall, Point(new_pos[0], new_pos[1]), player);
    recall->elevation = other->sprite->elevation + 1;
    recall->UpdateVisibilityPoint();
    if (other->unit_id == Ghost && other->related && other->related->unit_id == NuclearMissile)
    {
        other->related->related = nullptr;
        other->related = nullptr;
    }
}

void Unit::Order_Hallucination(ProgressUnitResults *results)
{
    uint16_t energy_cost;
    if (SpellOrder(this, Tech::Hallucination, GetSightRange(true) * 32, &energy_cost, String::Error_HallucinationTarget) == 0)
        return;

    for (int i = 0; i < Spell::HallucinationCount; i++)
    {
        Unit *hallu = Hallucinate(player, target);
        Assert(hallu);
        if (PlaceHallucination(hallu) == 0)
        {
            hallu->Remove(results);
            break;
        }
    }
    ReduceEnergy(energy_cost);
    PlaySound(Sound::Hallucination, target, 1, 0);
    AddOverlayHighest(target->GetTurret()->sprite, Image::HallucinationSmoke, 0, 0, 0);
    OrderDone();
}

void Unit::Order_MindControl(ProgressUnitResults *results)
{
    if (target == nullptr || target->player == player)
    {
        OrderDone();
        return;
    }
    uint16_t energy_cost;
    if (SpellOrder(this, Tech::MindControl, GetSightRange(true) * 32, &energy_cost, String::Error_MindControlTarget) == 0)
        return;

    if (target->flags & UnitStatus::Hallucination)
    {
        target->Kill(results);
    }
    else
    {
        target->AddSpellOverlay(Image::MindControl_Small);
        target->Trigger_GiveUnit(player, results);
        if (target->unit_id == DarkArchon)
            target->energy = 0;
        target->OrderDone();
    }
    shields = 0;
    ReduceEnergy(energy_cost);
    PlaySound(Sound::MindControl, target, 1, 0);
    OrderDone();
}
