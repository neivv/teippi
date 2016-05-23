#include "tech.h"

#include "constants/image.h"
#include "constants/sprite.h"
#include "constants/string.h"
#include "constants/tech.h"
#include "constants/weapon.h"
#include "ai.h"
#include "bullet.h"
#include "image.h"
#include "offsets.h"
#include "rng.h"
#include "sound.h"
#include "sprite.h"
#include "strings.h"
#include "unit.h"
#include "unitsearch.h"
#include "yms.h"

int GetTechLevel(TechType tech_, int player)
{
    int tech = tech_.Raw();
    Assert(tech >= 0 && tech < TechId::None.Raw());
    if (tech < 0x18)
        return bw::tech_level_sc[player][tech];
    else
        return bw::tech_level_bw[player][(tech - 0x18)];
}

void SetTechLevel(TechType tech_, int player, int amount)
{
    int tech = tech_.Raw();
    Assert(tech >= 0 && tech < TechId::None.Raw());
    if (tech < 0x18)
        bw::tech_level_sc[player][tech] = amount;
    else
        bw::tech_level_bw[player][(tech - 0x18)] = amount;
}

void Stasis(Unit *attacker, const Point &position)
{
    Sprite *stasis = lone_sprites->AllocateLone(SpriteId::StasisField, position, 0);
    stasis->elevation = Spell::StasisElevation;
    stasis->UpdateVisibilityPoint();

    Rect16 area(position, Spell::StasisArea);
    unit_search->ForEachUnitInArea(area, [attacker](Unit *unit)
    {
        if (unit == attacker || unit->IsInvincible() || unit->flags & UnitStatus::Burrowed)
            return false;
        if (unit->Type().IsBuilding())
            return false;
        bw::ApplyStasis(unit, Spell::StasisTime);
        return false;
    });
}

void Ensnare(Unit *attacker, const Point &position)
{
    Sprite *ensnare = lone_sprites->AllocateLone(SpriteId::Ensnare, position, 0);
    ensnare->elevation = Spell::EnsnareElevation;
    ensnare->UpdateVisibilityPoint();

    Rect16 area(position, Spell::EnsnareArea);
    unit_search->ForEachUnitInArea(area, [attacker](Unit *unit)
    {
        if (unit == attacker || unit->IsInvincible() || unit->flags & UnitStatus::Burrowed)
            return false;
        if (unit->Type().IsBuilding())
            return false;
        bw::ApplyEnsnare(unit);
        return false;
    });
}

void Maelstrom(Unit *attacker, const Point &position)
{
    Sprite *mael = lone_sprites->AllocateLone(SpriteId::Maelstrom, position, 0);
    mael->elevation = Spell::MaelstromElevation;
    mael->UpdateVisibilityPoint();

    bw::PlaySound(Sound::MaelstromHit, attacker, 1, 0);

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
        if (unit->Type().IsBuilding())
            return false;
        if (~unit->Type().Flags() & UnitFlags::Organic)
            return false;
        bw::ApplyMaelstrom(unit, Spell::MaelstromTime);
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
    if (score->CompletedUnits(UnitId::DisruptionWeb, NeutralPlayer) != 0)
    {
        for (Unit *dweb : bw::first_player_unit[NeutralPlayer])
        {
            if (dweb->unit_id != UnitId::DisruptionWeb)
                continue;

            Rect16 area;
            const Rect16 &dweb_dbox = UnitId::DisruptionWeb.DimensionBox();
            area.left = dweb->sprite->position.x - dweb_dbox.left;
            area.top = dweb->sprite->position.y - dweb_dbox.top;
            area.right = dweb->sprite->position.x + dweb_dbox.right;
            area.bottom = dweb->sprite->position.y + dweb_dbox.bottom;
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
    if (target && target->Type().Flags() & UnitFlags::Spellcaster && ~target->flags & UnitStatus::Hallucination)
    {
        uint16_t energy_cost;
        if (bw::SpellOrder(this, TechId::Feedback, GetSightRange(true) * 32, &energy_cost, String::Error_MustTargetSpellcaster))
        {
            // Hallu kill code would be here
            if (target->energy)
            {
                bool dead = target->GetHealth() << 8 <= target->energy;
                results->weapon_damages.emplace_back(this, player, target, target->energy, WeaponId::Feedback, 1);
                target->energy = 0;
                ReduceEnergy(energy_cost);
                bw::PlaySound(Sound::Feedback, this, 1, 0);
                if (dead)
                {
                    SpriteType sprite_id(SpriteId::FeedbackHit_Small.Raw() + target->Type().OverlaySize());
                    target->sprite->SpawnLoneSpriteAbove(sprite_id);
                }

                else
                {
                    target->AddSpellOverlay(ImageId::FeedbackHit_Small);
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
    auto victim_type = victim->Type();
    if (~victim_type.Flags() & UnitFlags::Organic || victim_type.IsBuilding())
        return;
    if (victim_type == UnitId::Larva || victim_type == UnitId::Egg || victim_type == UnitId::LurkerEgg)
        return;
    if (victim->flags & UnitStatus::Burrowed && irradiated != victim)
        return;
    if (~irradiated->flags & UnitStatus::InTransport && !bw::IsInArea(irradiated, 0x20, victim))
        return;
    int damage = WeaponId::Irradiate.Damage() * 256 / WeaponId::Irradiate.Cooldown();
    results->weapon_damages.emplace_back(irradiated->irradiated_by,
                                         irradiated->irradiate_player,
                                         victim,
                                         damage,
                                         WeaponId::Irradiate,
                                         0);
}

void DoMatrixDamage(Unit *target, int dmg)
{
    if (dmg < target->matrix_hp)
    {
        target->matrix_hp -= dmg;
        if (~target->flags & UnitStatus::Burrowed)
        {
            target->AddSpellOverlay(ImageId::DefensiveMatrixHit_Small);
        }
    }
    else
    {
        target->matrix_hp = 0;
        target->matrix_timer = 0;
        target->RemoveOverlayFromSelfOrSubunit(ImageId::DefensiveMatrixFront_Small, 2);
        target->RemoveOverlayFromSelf(ImageId::DefensiveMatrixBack_Small, 2);
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
        bw::EndStasis(this);
    if (stim_timer && --stim_timer == 0)
        bw::UpdateSpeed(this);
    if (ensnare_timer && --ensnare_timer == 0)
    {
        RemoveOverlayFromSelfOrSubunit(ImageId::Ensnare_Small, 2);
        bw::UpdateSpeed(this);
    }
    if (matrix_timer && --matrix_timer == 0)
        DoMatrixDamage(this, matrix_hp);
    if (irradiate_timer)
    {
        irradiate_timer--;
        DoIrradiateDamage(results);
        if (irradiate_timer == 0)
        {
            RemoveOverlayFromSelfOrSubunit(ImageId::Irradiate_Small, 2);
            irradiated_by = nullptr;
            irradiate_player = 8;
        }
    }
    if (lockdown_timer && --lockdown_timer == 0)
        bw::EndLockdown(this);
    if (mael_timer && --mael_timer == 0)
        bw::EndMaelstrom(this);
    if (plague_timer)
    {
        plague_timer--;
        if (!IsInvincible())
        {
            int damage = WeaponId::Plague.Damage() / (Spell::PlagueTime + 1) * 256;
            if (damage < hitpoints)
                DamageSelf(damage, results);
        }
        if (plague_timer == 0)
        {
            RemoveOverlayFromSelfOrSubunit(ImageId::Plague_Small, 2);
        }
    }
    if (is_under_storm)
        is_under_storm--;
    bw::ProgressAcidSporeTimers(this);
}

void Unit::Lockdown(int time)
{
    if (!lockdown_timer)
        AddSpellOverlay(ImageId::Lockdown_Small);
    if (lockdown_timer < time)
        lockdown_timer = time;
    bw::DisableUnit(this);
}


void Unit::Parasite(int player)
{
    bw::PlaySound(Sound::Parasite, target, 1, 0);
    parasites |= 1 << player;
    RefreshUi();
}

void Unit::SpawnBroodlings(const Point &pos_)
{
    Point pos = pos_;
    for (int i = 0; i < Spell::BroodlingCount; i++)
    {
        if (!bw::DoesFitHere(UnitId::Broodling.Raw(), pos.x, pos.y))
        {
            Rect16 area(pos, 0x20);
            uint16_t new_pos[2], old_pos[2] = { pos.x, pos.y };
            if (!bw::GetFittingPosition(&area, this, old_pos, new_pos, 0, 0)) // No clue why it uses the queen <.<
                continue;
            pos.x = new_pos[0];
            pos.y = new_pos[1];
        }
        uint16_t pos_arr[2] = { pos.x, pos.y };
        bw::ClipPointInBoundsForUnit(UnitId::Broodling.Raw(), pos_arr);

        Unit *broodling = bw::CreateUnit(UnitId::Broodling.Raw(), pos_arr[0], pos_arr[1], player);
        if (!broodling)
        {
            bw::ShowLastError(player);
            continue;
        }
        bw::FinishUnit_Pre(broodling);
        bw::FinishUnit(broodling);
        broodling->death_timer = Spell::BroodlingDeathTimer;
        if (ai != nullptr)
            bw::InheritAi2(this, broodling);

        pos += Point(4, 4);
    }
}

void EmpShockwave(Unit *attacker, const Point &position)
{
    // Dunno why bw does area checks here but not with other spells
    Rect16 area = Rect16(position, WeaponId::EmpShockwave.InnerSplash()).Clipped(MapBounds());

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
        AddSpellOverlay(ImageId::Irradiate_Small);
    }
    irradiate_timer = WeaponId::Irradiate.Cooldown();
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
                unit->AddSpellOverlay(ImageId::Plague_Small);
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
    bw::ClipPointInBoundsForUnit(UnitId::DarkSwarm.Raw(), pos);
    Unit *swarm = bw::CreateUnit(UnitId::DarkSwarm.Raw(), pos[0], pos[1], NeutralPlayer);
    if (!swarm)
    {
        bw::ShowLastError(player);
        return;
    }
    swarm->sprite->elevation = Spell::DarkSwarmElevation;
    swarm->flags |= UnitStatus::NoCollision;
    bw::FinishUnit_Pre(swarm);
    bw::FinishUnit(swarm);
    swarm->death_timer = Spell::DarkSwarmTime;
}

void DisruptionWeb(int player, const Point &position)
{
    uint16_t pos[2] = { position.x, position.y };
    bw::ClipPointInBoundsForUnit(UnitId::DisruptionWeb.Raw(), pos);
    Unit *dweb = bw::CreateUnit(UnitId::DisruptionWeb.Raw(), pos[0], pos[1], NeutralPlayer);
    if (dweb == nullptr)
    {
        bw::ShowLastError(player);
        return;
    }
    dweb->sprite->elevation = Spell::DisruptionWebElevation;
    dweb->flags |= UnitStatus::NoCollision;
    bw::FinishUnit_Pre(dweb);
    bw::FinishUnit(dweb);
    dweb->death_timer = Spell::DisruptionWebTime;
}

void Unit::Consume(Unit *target, vector<Unit *> *killed_units)
{
    if (!target->IsInvincible() && bw::CanTargetSpellOnUnit(TechId::Consume, target, player))
    {
        score->RecordDeath(target, player);
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
    RemoveOverlayFromSelfOrSubunit(ImageId::AcidSpore_Small1, 11);
}

void Unit::Restoration()
{
    bw::PlaySound(Sound::Restoration, this, 1, 0);
    if (flags & UnitStatus::Hallucination)
    {
        Kill(nullptr);
        return;
    }
    AddSpellOverlay(ImageId::Restoration_Small);
    if (ensnare_timer)
    {
        RemoveOverlayFromSelfOrSubunit(ImageId::Ensnare_Small, 2);
        ensnare_timer = 0;
        bw::UpdateSpeed(this);
    }
    if (plague_timer)
    {
        RemoveOverlayFromSelfOrSubunit(ImageId::Plague_Small, 2);
        plague_timer = 0;
    }
    if (irradiate_timer)
    {
        RemoveOverlayFromSelfOrSubunit(ImageId::Irradiate_Small, 2);
        irradiate_timer = 0;
        irradiated_by = 0;
        irradiate_player = 8;
    }
    if (lockdown_timer)
        bw::EndLockdown(this);
    if (mael_timer)
        bw::EndMaelstrom(this);
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
    bw::PlaySound(Sound::OpticalFlare, this, 1, 0);
    AddSpellOverlay(ImageId::OpticalFlareHit_Small);
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
            Sprite *recall = lone_sprites->AllocateLone(SpriteId::Recall, order_target_pos, player);
            recall->elevation = sprite->elevation + 1;
            recall->UpdateVisibilityPoint();
            bw::PlaySoundAtPos(Sound::Recall + Rand(0x11) % 2, order_target_pos.AsDword(), 1, 0);
            order_timer = Spell::RecallTime;
            order_state = 1;
        }
        else
        {
            if (player == *bw::local_player_id && *bw::select_command_user == *bw::local_unique_player_id)
                bw::PrintInfoMessage((*bw::stat_txt_tbl)->GetTblString(String::NotEnoughEnergy + Type().Race()));
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
            if (unit->Type().IsBuilding() || unit->flags & UnitStatus::Burrowed)
                return false;
            if (unit->Type() == UnitId::Larva || unit->Type() == UnitId::Egg || unit->Type() == UnitId::LurkerEgg)
                return false;
            if (unit == this || unit->flags & UnitStatus::Hallucination)
                return false;
            Recall(unit);
            recalled_something = true;
            return false;
        });
        if (recalled_something)
            bw::PlaySound(Sound::Recalled + Rand(0x11) % 2, this, 1, 0);
        OrderDone();
    }
}

void Unit::Recall(Unit *other)
{
    Point old_pos = other->sprite->position;
    bw::MoveUnit(other, sprite->position.x, sprite->position.y);
    uint16_t pos[2] = { sprite->position.x, sprite->position.y }, new_pos[2];
    if (bw::GetFittingPosition(nullptr, other, pos, new_pos, 0, 0) == 0)
    {
        bw::MoveUnit(other, old_pos.x, old_pos.y);
        return;
    }
    bw::HideUnit_Partial(other, 0);
    other->target = nullptr;
    bw::MoveUnit(other, new_pos[0], new_pos[1]); // Huh?
    bw::FinishMoveUnit(other);
    if (other->Type() != UnitId::Cocoon)
        other->IssueOrderTargetingNothing(other->Type().ReturnToIdleOrder());
    Sprite *recall = lone_sprites->AllocateLone(SpriteId::Recall, Point(new_pos[0], new_pos[1]), player);
    recall->elevation = other->sprite->elevation + 1;
    recall->UpdateVisibilityPoint();
    if (other->Type() == UnitId::Ghost &&
            other->related != nullptr &&
            other->related->Type() == UnitId::NuclearMissile)
    {
        other->related->related = nullptr;
        other->related = nullptr;
    }
}

void Unit::Order_Hallucination(ProgressUnitResults *results)
{
    uint16_t energy_cost;
    if (bw::SpellOrder(this, TechId::Hallucination.Raw(), GetSightRange(true) * 32, &energy_cost, String::Error_HallucinationTarget) == 0)
        return;

    for (int i = 0; i < Spell::HallucinationCount; i++)
    {
        Unit *hallu = bw::Hallucinate(player, target);
        if (hallu == nullptr) {
            break;
        }
        if (bw::PlaceHallucination(hallu) == 0)
        {
            hallu->Remove(results);
            break;
        }
    }
    ReduceEnergy(energy_cost);
    bw::PlaySound(Sound::Hallucination, target, 1, 0);
    bw::AddOverlayHighest(target->GetTurret()->sprite.get(), ImageId::HallucinationSmoke.Raw(), 0, 0, 0);
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
    if (bw::SpellOrder(this, TechId::MindControl.Raw(), GetSightRange(true) * 32, &energy_cost, String::Error_MindControlTarget) == 0)
        return;

    if (target->flags & UnitStatus::Hallucination)
    {
        target->Kill(results);
    }
    else
    {
        target->AddSpellOverlay(ImageId::MindControl_Small);
        target->Trigger_GiveUnit(player, results);
        if (target->Type() == UnitId::DarkArchon)
            target->energy = 0;
        target->OrderDone();
    }
    shields = 0;
    ReduceEnergy(energy_cost);
    bw::PlaySound(Sound::MindControl, target, 1, 0);
    OrderDone();
}
