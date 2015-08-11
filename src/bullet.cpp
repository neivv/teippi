#include "bullet.h"

#include <deque>
#include <tuple>
#include <algorithm>

#include "unit.h"
#include "unitsearch.h"
#include "sprite.h"
#include "yms.h"
#include "image.h"
#include "offsets.h"
#include "order.h"
#include "player.h"
#include "upgrade.h"
#include "patchmanager.h"
#include "ai.h"
#include "log.h"
#include "perfclock.h"
#include "scthread.h"
#include "tech.h"
#include "sound.h"
#include "limits.h"
#include "rng.h"
#include "warn.h"
#include "unit_cache.h"

using std::get;
using std::min;
using std::max;

BulletSystem *bullet_system;
bool bulletframes_in_progress = false; // Protects calling Kill() when FindHelpingUnits may be run

namespace {
const Point32 random_chances[] = { Point32(1, 1), Point32(51, 31), Point32(7, 12), Point32(-36, -52), Point32(-49, 25),
  Point32(40, -46), Point32(-4, -19), Point32(28, 50), Point32(50, -15), Point32(-54, -13), Point32(11, -53),
  Point32(15, -9), Point32(-19, 50), Point32(-17, 8), Point32(0, 0) };
}

TempMemoryPool pbf_memory;

static void ShowShieldHitOverlay(Unit *unit, int direction)
{
    Image *img = unit->sprite->main_image;
    direction = ((direction - 0x7c) >> 3) & 0x1f;
    int8_t *shield_los = images_dat_shield_overlay[img->image_id];
    shield_los = shield_los + *(uint32_t *)(shield_los + 8 + img->direction * 4) + direction * 2; // sigh
    AddOverlayAboveMain(unit->sprite, Image::ShieldOverlay, shield_los[0], shield_los[1], direction);
}

static void UnitKilled(Unit *target, Unit *attacker, int attacking_player, vector<Unit *> *killed_units)
{
    // Kinda stupid that TransportDeath() does about the same...
    // Guess it is necessary if trigger kills the unit,
    // this when we care about kill scores :/
    if (~units_dat_flags[target->unit_id] & UnitFlags::Building) // Let's not kill units inside bunker
    {
        Unit *loaded, *next = target->first_loaded;
        while (next)
        {
            loaded = next;
            next = loaded->next_loaded;
            if (loaded->order != Order::Die) // Necessary?
            {
                killed_units->emplace_back(loaded);
                IncrementKillScores(loaded, attacking_player);
            }
        }
    }
    //debug_log->Log("Unit %p got killed by damage\n", target);
    target->hitpoints = 0;
    killed_units->emplace_back(target);
    IncrementKillScores(target, attacking_player);
    if (attacker)
    {
        if (attacker->IsEnemy(target))
            attacker->IncrementKills();
    }
}

bool DamagedUnit::IsDead() const
{
    return base->dmg_unit.damage >= base->hitpoints;
}

void DamagedUnit::AddDamage(int amt)
{
    base->dmg_unit.damage += amt;
}

DamagedUnit ProgressBulletBufs::GetDamagedUnit(Unit *unit)
{
    if (unit->dmg_unit.valid_frame != *bw::frame_count)
    {
        unit->dmg_unit.damage = 0;
        unit->dmg_unit.valid_frame = *bw::frame_count;
        damaged_units->emplace_back(unit);
    }
    return DamagedUnit(unit);
}

void DamagedUnit::AddHit(uint32_t dmg, int weapon_id, int player, int direction, Unit *attacker, ProgressBulletBufs *bufs)
{
    if (IsDead())
        return;

    dmg = base->GetModifiedDamage(dmg);
    dmg = base->ReduceMatrixDamage(dmg);
    auto dmg_type = weapons_dat_damage_type[weapon_id];
    auto armor_type = units_dat_armor_type[base->unit_id];

    bool damaged_shields = false;
    if (base->HasShields() && base->shields >= 256)
    {
        dmg = base->DamageShields(dmg, dmg_type == 4);
        if (base->shields >= 256)
            ShowShieldHitOverlay(base, direction);
        damaged_shields = true;
    }
    if (dmg_type != 4) // Ignore armor
    {
        int armor_reduction = base->GetArmor() * 256;
        if (dmg > armor_reduction)
            dmg -= armor_reduction;
        else
            dmg = 0;
    }

    dmg = (dmg * bw::damage_multiplier[dmg_type * 5 + armor_type]) / 256;
    if (!damaged_shields && !dmg)
        dmg = 128;

    if (!IsCheatActive(Cheats::Power_Overwhelming) || IsHumanPlayer(player))
        AddDamage(dmg);
    if (IsDead())
    {
        UnitKilled(base, attacker, player, bufs->killed_units);
    }
    else if (attacker)
    {
        if (base->player != attacker->player && weapon_id != Weapon::Irradiate)
            Notify_UnitWasHit(base);
        if (UnitWasHit(base, attacker, weapon_id != Weapon::Irradiate))
            bufs->unit_was_hit->emplace_back(base, attacker);
    }
    if (IsComputerPlayer(base->player) && attacker)
    {
        Ai::EscapeStaticDefence(base, attacker);
    }
}

void ProgressBulletBufs::AddToAiReact(Unit *unit, Unit *attacker, bool main_target_reactions)
{
    if (IsComputerPlayer(unit->player))
        ai_react->emplace_back(unit, attacker, main_target_reactions);
}

BulletSystem::BulletVector *BulletSystem::GetOwningVector(const Bullet *bullet)
{
    auto index = bullet->bulletsystem_entry;
    for (auto vec : Vectors())
    {
        if (vec->size() > index && (*vec)[index].get() == bullet)
            return vec;
    }
    Assert(false);
    return nullptr;
}

const BulletSystem::BulletVector *BulletSystem::GetOwningVector(const Bullet *bullet) const
{
    auto index = bullet->bulletsystem_entry;
    for (auto vec : Vectors())
    {
        if (vec->size() > index && (*vec)[index].get() == bullet)
            return vec;
    }
    Assert(false);
    return nullptr;
}

BulletSystem::BulletVector *BulletSystem::GetStateVector(BulletState state)
{
    switch (state)
    {
        case BulletState::Init:
            return &initstate;
        case BulletState::MoveToPoint:
            return &moving_to_point;
        case BulletState::MoveToTarget:
            return &moving_to_unit;
        case BulletState::Bounce:
            return &bouncing;
        case BulletState::Die:
            return &dying;
        case BulletState::MoveNearUnit:
            return &moving_near;
        case BulletState::GroundDamage:
            return &damage_ground;
        default:
            Assert(false);
            return nullptr;
    }
}

void Bullet::UpdateMoveTarget(const Point &target)
{
    if (move_target != target)
    {
        move_target = target;
        next_move_waypoint = target;
        flingy_flags &= ~0x4;
        flingy_flags |= 0x1;
        move_target_unit = nullptr;
    }
}

void Bullet::Move(const Point &where)
{
    position = where;
    exact_position = Point32(where.x * 256, where.y * 256);
    MoveSprite(sprite.get(), where.x, where.y);
}

// Spawner is not necessarily parent, in case of subunits/fighters
bool Bullet::Initialize(Unit *spawner, int player_, int direction, int weapon, const Point &pos)
{
    Assert(!bulletframes_in_progress);
    list.prev = nullptr;
    list.next = nullptr;

    move_target = Point(0xffff, 0xffff);
    current_speed = 0;

    auto flingy_id = weapons_dat_flingy[weapon];
    auto result = InitializeFlingy((Flingy *)this, player_, direction, flingy_id, pos.x, pos.y);
    if (result != 0) { Assert(result != 0); } // Avoids an unused var warning
    player = player_;
    weapon_id = weapon;
    time_remaining = weapons_dat_death_time[weapon_id];
    flingy_flags |= 0x8;
    flags = 0;
    bounces_remaining = 0;
    // Bw calls State_Init here, it should just return instantly though as no iscript has been run
    order_signal = 0;
    auto spin = weapons_dat_launch_spin[weapon_id];
    if (spin)
    {
        bool spin_positive = main_rng->Rand(2) == 1;
        // Goliath dual missiles etc, ugh
        static bool last_bullet_spin_positive;
        if (spawner == *bw::last_bullet_spawner)
            spin_positive = !last_bullet_spin_positive;
        last_bullet_spin_positive = spin_positive;
        if (!spin_positive)
            spin = 0 - spin;
        movement_direction += spin;
        facing_direction = movement_direction;
        *bw::last_bullet_spawner = spawner;
    }

    if (units_dat_flags[spawner->unit_id] & UnitFlags::Subunit)
        parent = spawner->subunit;
    else if (spawner->unit_id == Unit::Scarab)
        parent = spawner->interceptor.parent;
    else
        parent = spawner;

    if (parent) // Parent may be nullptr if scara is shot and reaver dies before hit
        spawned.Add(parent->spawned_bullets);
    if (spawner->flags & UnitStatus::Hallucination)
        flags |= 0x2;
    previous_target = nullptr;
    target = spawner->target;
    if (target)
    {
        order_target_pos = target->sprite->position;
        sprite->elevation = target->sprite->elevation + 1;
        SetTarget(target);
    }
    else
    {
        order_target_pos = spawner->order_target_pos;
        sprite->elevation = spawner->sprite->elevation + 1;
    }

    switch (weapons_dat_behaviour[weapon_id])
    {
        case 0x8: // Move near
        {
            spread_seed = spawner->bullet_spread_seed;
            spawner->bullet_spread_seed++;
            if (spawner->bullet_spread_seed >= sizeof random_chances / sizeof(random_chances[0]))
                spawner->bullet_spread_seed = 0;
            const Point32 &diff = random_chances[spread_seed];
            Point &pos = order_target_pos;
            int x = min((int)*bw::map_width - 1, max(0, (int)pos.x - diff.x));
            int y = min((int)*bw::map_height - 1, max(0, (int)pos.y - diff.y));
            order_target_pos = Point(x, y);
            UpdateMoveTarget(order_target_pos);
        }
        break;
        case 0x2: case 0x4: // Appear on target unit / site
        if (target && parent)
        {
            if (main_rng->Rand(0x100) < GetMissChance(parent, target))
            {
                int x = sprite->position.x - bw::circle[direction * 2] * 30 / 256;
                int y = sprite->position.y - bw::circle[direction * 2 + 1] * 30 / 256;
                x = max(0, min((int)*bw::map_width - 1, (int)x));
                y = max(0, min((int)*bw::map_height - 1, (int)y));
                Move(Point(x, y));
                flags |= 0x1;
            }
        }
        break;
        case 0x3: // Persist on target site
            Move(order_target_pos);
        break;
        case 0x6: // Suicide
        if (parent)
        {
            parent->flags |= UnitStatus::SelfDestructing;
            parent->order_flags |= 0x4;
            parent->Kill(nullptr);
        }
        break;
        case 0x9: // Go to max range
        {
            auto max_range = (weapons_dat_max_range[weapon_id] + 20) * 256;
            auto x = bw::circle[facing_direction * 2] * max_range / 65536;
            auto y = bw::circle[facing_direction * 2 + 1] * max_range / 65536;
            order_target_pos = spawner->sprite->position + Point(x, y);
            UpdateMoveTarget(order_target_pos);
        }
        break;
        case 0x7: case 0x1: case 0x0: // Bounce, Fly & follow/don't
            bounces_remaining = 3; // Won't matter on others
            if (target && parent)
            {
                if (main_rng->Rand(0x100) < GetMissChance(parent, target))
                {
                    int x = order_target_pos.x - bw::circle[direction * 2] * 30 / 256;
                    int y = order_target_pos.y - bw::circle[direction * 2 + 1] * 30 / 256;
                    x = max(0, min((int)*bw::map_width - 1, (int)x));
                    y = max(0, min((int)*bw::map_height - 1, (int)y));
                    order_target_pos = Point(x, y);
                    flags |= 0x1;
                }
            }
            UpdateMoveTarget(order_target_pos);
        break;
        case 0x5: // Appear on attacker
        break;
        default:
            Warning("Unknown weapons.dat behaviour %x for weapon %x", weapons_dat_behaviour[weapon_id], weapon_id);
            return false;
        break;
    }
    return true;
}

Bullet *BulletSystem::AllocateBullet(Unit *parent, int player, int direction, int weapon, const Point &pos_)
{
    Point pos = pos_;
    if (pos.x > 0xc000)
        pos.x = 0;
    if (pos.y > 0xc000)
        pos.y = 0;
    pos.x = min(pos.x, (x16u)(*bw::map_width - 1));
    pos.y = min(pos.y, (y16u)(*bw::map_height - 1));
    if (parent->flags & UnitStatus::UnderDweb)
    {
        using namespace Weapon;
        switch (weapon)
        {
            case SpiderMine: case Lockdown: case Weapon::EmpShockwave: case Irradiate: case Venom: case HeroVenom:
            case Suicide: case Parasite: case SpawnBroodlings: case Weapon::Ensnare: case Weapon::DarkSwarm:
            case Weapon::Plague: case Consume: case PsiAssault: case HeroPsiAssault: case Scarab: case StasisField:
            case PsiStorm: case Restoration: case MindControl: case Feedback: case OpticalFlare:
            case Weapon::Maelstrom:
                break;
            default:
                return nullptr;
        }
    }
    ptr<Bullet> bullet_ptr = ptr<Bullet>(new Bullet);
    if (bullet_ptr->Initialize(parent, player, direction, weapon, pos) == true)
    {
        bullet_ptr->bulletsystem_entry = initstate.size();
        initstate.emplace_back(move(bullet_ptr));
        Bullet *bullet = initstate.back().get();

        if (*bw::first_active_bullet)
        {
            bullet->list.prev = *bw::last_active_bullet;
            (*bw::last_active_bullet)->list.next = bullet;
        }
        else
        {
            bullet->list.prev = nullptr;
            *bw::first_active_bullet = bullet;
        }
        *bw::last_active_bullet = bullet;
        bullet->list.next = nullptr;
        return bullet;
    }
    else
    {
        debug_log->Log("Bullet creation failed %x %x.%x\n", weapon, pos.x, pos.y);
        bullet_ptr->sprite->SingleDelete();
        return nullptr;
    }
}

void Bullet::SingleDelete()
{
    if (this == *bw::first_active_bullet)
        *bw::first_active_bullet = list.next;
    if (this == *bw::last_active_bullet)
        *bw::last_active_bullet = list.prev;
    if (list.next)
        list.next->list.prev = list.prev;
    if (list.prev)
        list.prev->list.next = list.next;

    if (target)
    {
        targeting.Remove(target->targeting_bullets);
    }
    if (parent)
    {
        spawned.Remove(parent->spawned_bullets);
    }
}

void BulletSystem::DeleteAll()
{
    initstate.clear();
    moving_to_point.clear();
    moving_to_unit.clear();
    bouncing.clear();
    damage_ground.clear();
    moving_near.clear();
    dying.clear();
    *bw::first_active_bullet = nullptr;
    *bw::last_active_bullet = nullptr;
}

void Bullet::SetTarget(Unit *new_target)
{
    if (target != nullptr && new_target != target) // True when bounce
    {
        targeting.Remove(target->targeting_bullets);
    }

    targeting.Add(new_target->targeting_bullets);
}

int GetSplashDistance(Bullet *bullet, Unit *target)
{
    int left = target->sprite->position.x - units_dat_dimensionbox[target->unit_id].left;
    int right = target->sprite->position.x + units_dat_dimensionbox[target->unit_id].right - 1;
    int top = target->sprite->position.y - units_dat_dimensionbox[target->unit_id].top;
    int bottom = target->sprite->position.y + units_dat_dimensionbox[target->unit_id].bottom - 1;
    Point &pos = bullet->sprite->position;
    if (pos.x > left && pos.x < right && pos.y > top && pos.y < bottom)
        return 0;

    int w, h;
    if (pos.x < left)
        w = (left) - pos.x;
    else if (pos.x > right)
        w = pos.x - right;
    else
        w = 0;
    if (pos.y < top)
        h = (top) - pos.y;
    else if (pos.y > bottom)
        h = pos.y - bottom;
    else
        h = 0;
    return Distance(Point32(0, 0), Point32(w, h));
}

int GetWeaponDamage(const Unit *target, int weapon_id, int player)
{
    if (weapons_dat_effect[weapon_id] == 5) // nuke
    {
        int hp = units_dat_hitpoints[target->unit_id] >> 8;
        if (!hp)
        {
            hp = (target->hitpoints + 255) >> 8;
            if (!hp) // Pointless, this is not called on dead units?
                hp = 1;
        }
        hp += target->GetMaxShields();
        int dmg = hp * 2 / 3;
        if (dmg < 500)
            dmg = 500;
        return dmg * 256;
    }
    else
    {
        int dmg = weapons_dat_damage[weapon_id];
        dmg += GetUpgradeLevel(weapons_dat_upgrade[weapon_id], player) * weapons_dat_upgrade_bonus[weapon_id];
        return dmg * 256;
    }
}

static int GetWeaponDamage(const Bullet *bullet, const Unit *target)
{
    return GetWeaponDamage(target, bullet->weapon_id, bullet->player);
}

bool IsPlayerUnk(int player)
{
    int type = bw::players[player].type;
    switch (type)
    {
        case 1:
        case 2:
            return bw::victory_status[player] != 0;
        case 0xa:
        case 0xb:
            return true;
        default:
            return false;
    }
}

void IncrementKillScores(Unit *target, int attacking_player)
{
    int target_id = target->unit_id;
    if ((target->flags & UnitStatus::Hallucination) || (units_dat_flags[target_id] & UnitFlags::Subunit))
        return;

    int target_player = target->player;
    int group = units_dat_group_flags[target_id];
    if (group & 0x8)
    {
        bw::player_men_deaths[target_player]++;
    }
    else if (group & 0x10)
    {
        bw::player_building_deaths[target_player]++;
        if (group & 0x20)
            bw::player_factory_deaths[target_player]++;
    }
    bw::unit_deaths[target_id * Limits::Players + target_player]++;
    if (target_player != attacking_player && IsActivePlayer(attacking_player) && !IsPlayerUnk(attacking_player))
    {
        if ((~group & 0x8) && (target_id != Unit::Larva) && (target_id != Unit::Egg))
        {
            if (group & 0x10)
            {
                bw::player_building_kills[attacking_player]++;
                bw::player_building_kill_score[attacking_player] += units_dat_kill_score[target_id];
                if (group & 0x20)
                    bw::player_factory_kills[attacking_player]++;
            }
        }
        else
        {
            bw::player_men_kills[attacking_player]++;
            bw::player_men_kill_score[attacking_player] += units_dat_kill_score[target_id];
        }
        bw::unit_kills[target_id * Limits::Players + attacking_player]++;
    }
}

// Before UnitWasHit did this but now it only constructs a list and extracts it later here,
// skipping cases where target was hit by same attacker multiple times
// Returns true if ai wants to cancel target
bool UnitWasHit_Actual(Unit *target, Unit *attacker, ProgressBulletBufs *bufs)
{
    target->last_attacking_player = attacker->player;
    if (target->ai && !target->previous_attacker)
    {
        debug_log->Log("Setting previous attacker for ai unit %08X to %08X as there was not one\n", target->lookup_id, attacker->lookup_id);
        target->SetPreviousAttacker(attacker);
        if (target->HasSubunit())
        {
            target->subunit->last_attacking_player = target->last_attacking_player;
            target->subunit->SetPreviousAttacker(attacker);
        }
    }

    if (attacker->flags & UnitStatus::InBuilding)
        attacker = attacker->related;

    // Could append to help list
    bool cancel_target = Ai::ShouldCancelDamaged(target);
    if (!cancel_target)
    {
        Assert(target->hotkey_groups & 0x80000000);
        bufs->AddToAiReact(target, attacker, true);
    }

    if (target->hitpoints != 0 && target->IsBetterTarget(attacker))
        target->ReactToHit(attacker); // Flee if worker / can't attack attacker / burrowed (And fleeable order)

    if (!target->ai)
    {
        Unit **nearby = target->nearby_helping_units.load(std::memory_order_relaxed);
        while (!nearby)
            nearby = target->nearby_helping_units.load(std::memory_order_relaxed);

        for (Unit *unit = *nearby++; unit; unit = *nearby++)
        {
            if (unit->order != Order::Die)
                unit->AskForHelp(attacker);
        }
    }
    return cancel_target;
}

// Returns true if there's need for UnitWasHit processing
bool UnitWasHit(Unit *target, Unit *attacker, bool notify)
{
    if (target->player == attacker->player)
        return false;

    //debug_log->Log("Uwh: %p\n", target);
    target->StartHelperSearch();
    if (notify && !attacker->IsInvisibleTo(target) && target->player < 8)
    {
        ShowArea(1, 1 << target->player, attacker->sprite->position.x, attacker->sprite->position.y, attacker->IsFlying());
    }
    return true;
}

void DamageUnit(int damage, Unit *target, vector<Unit *> *killed_units)
{
    target->sprite->MarkHealthBarDirty();
    if (damage < target->hitpoints)
    {
        target->hitpoints -= damage;
        if (images_dat_damage_overlay[target->sprite->main_image->image_id] && target->flags & UnitStatus::Completed)
            UpdateDamageOverlay(target);
    }
    else
    {
        UnitKilled(target, nullptr, -1, killed_units);
    }
}

void __stdcall DamageUnit_Hook(Unit *attacker, int attacking_player, bool show_attacker)
{
    REG_EAX(int, damage);
    REG_ECX(Unit *, target);
    // Can't do UnitWasHit here, so warn
    if (attacker)
        Warning("DamageUnit hooked");
    vector<Unit *> killed_units;
    DamageUnit(damage, target, &killed_units);
    for (Unit *unit : killed_units)
        unit->Kill(nullptr);
}

void HallucinationHit(Unit *target, Unit *attacker, int direction, vector<tuple<Unit *, Unit *>> *unit_was_hit)
{
    if (!target->hitpoints)
        return;
    if (target->IsInvincible())
        return;
    if (attacker)
    {
        if (UnitWasHit(target, attacker, true))
        {
            unit_was_hit->emplace_back(target, attacker);
            if (IsComputerPlayer(target->player))
            {
                Ai::Hide(target);
                if (attacker)
                    Ai::EscapeStaticDefence(target, attacker);
            }
        }
        if (attacker->player != target->player)
            Notify_UnitWasHit(target);
    }
    if (target->shields >= 128 && target->HasShields())
        ShowShieldHitOverlay(target, direction);
}

void Bullet::HitUnit(Unit *target, int dmg, ProgressBulletBufs *bufs)
{
    if (flags & 2) // hallu
    {
        HallucinationHit(target, parent, facing_direction, bufs->unit_was_hit);
    }
    else if (!target->IsInvincible())
    {
        DamagedUnit dmg_unit = bufs->GetDamagedUnit(target);
        dmg_unit.AddHit(dmg, weapon_id, player, facing_direction, parent, bufs);
    }
}

Unit *Bullet::ChooseBounceTarget()
{
    if (parent == nullptr || parent->IsDisabled())
        return nullptr;

    Unit *found = nullptr;
    Rect16 area(sprite->position, 0x60);
    enemy_unit_cache->ForAttackableEnemiesInArea(unit_search, parent, area, [&](Unit *other, bool *stop)
    {
        if (other == target || other == previous_target)
            return;
        found = other;
        *stop = true;
    });
    return found;
}

tuple<BulletState, int, Unit *> Bullet::State_Bounce()
{
    if (target && ~flags & 0x1)
        ChangeMovePos(this, target->sprite->position.x, target->sprite->position.y);

    ProgressBulletMovement(this);
    int do_missile_dmgs = 0;
    if (move_target == position)
    {
        bounces_remaining--;
        if (bounces_remaining)
        {
            Unit *new_target = ChooseBounceTarget();
            previous_target = target;
            if (new_target)
            {
                for (auto &cmd : SetIscriptAnimation(IscriptAnim::Special1, true))
                {
                    if (cmd.opcode == IscriptOpcode::DoMissileDmg)
                        do_missile_dmgs++;
                    else
                        Warning("Bullet::State_Bounce did not handle all iscript commands for bullet %x", weapon_id);
                }
                return make_tuple(BulletState::Bounce, do_missile_dmgs, new_target);
            }
        }

        order_state = 0;
        order_fow_unit = Unit::None;
        if (target)
            order_target_pos = target->sprite->position;
        for (auto &cmd : SetIscriptAnimation(IscriptAnim::Death, true))
        {
            if (cmd.opcode == IscriptOpcode::DoMissileDmg)
                do_missile_dmgs++;
            else
                Warning("Bullet::State_Bounce did not handle all iscript commands for bullet %x", weapon_id);
        }
        return make_tuple(BulletState::Die, do_missile_dmgs, nullptr);
    }
    return make_tuple(BulletState::Bounce, do_missile_dmgs, nullptr);
}

tuple<BulletState, int> Bullet::State_Init()
{
    int do_missile_dmgs = 0;
    if (~order_signal & 0x1)
        return make_tuple(BulletState::Init, do_missile_dmgs);
    order_signal &= ~0x1;

    BulletState state;
    int anim;
    //debug_log->Log("Inited bullet %p %x %x\n", this, weapon_id, sprite->main_image->iscript.pos);
    switch (weapons_dat_behaviour[weapon_id])
    {
        case 0x2:
        case 0x4:
        case 0x5:
        case 0x6:
            state = BulletState::Die;
            anim = IscriptAnim::Death;
        break;
        case 0x1:
            state = BulletState::MoveToTarget;
            anim = IscriptAnim::GndAttkInit;
        break;
        case 0x7:
            state = BulletState::Bounce;
            anim = IscriptAnim::GndAttkInit;
        break;
        case 0x3:
            state = BulletState::GroundDamage;
            anim = IscriptAnim::Special2;
        break;
        case 0x8:
            state = BulletState::MoveNearUnit;
            anim = IscriptAnim::GndAttkInit;
        break;
        default:
            state = BulletState::MoveToPoint;
            anim = IscriptAnim::GndAttkInit;
        break;
    }
    if (target)
        order_target_pos = target->sprite->position;
    order_fow_unit = Unit::None;
    order_state = 0;
    for (auto &cmd : SetIscriptAnimation(anim, true))
    {
        if (cmd.opcode == IscriptOpcode::DoMissileDmg)
            do_missile_dmgs++;
        else
            Warning("Bullet::State_Init did not handle iscript command %x for bullet %x", cmd.opcode, weapon_id);
    }
    return make_tuple(state, do_missile_dmgs);
}

// Psi storm behaviour
tuple<BulletState, int> Bullet::State_GroundDamage()
{
    int do_missile_dmgs = 0;
    if (time_remaining-- == 0)
    {
        if (target)
            order_target_pos = target->sprite->position;
        order_state = 0;
        order_fow_unit = Unit::None;
        for (auto &cmd : SetIscriptAnimation(IscriptAnim::Death, true))
        {
            if (cmd.opcode == IscriptOpcode::DoMissileDmg)
                do_missile_dmgs++;
            else
                Warning("Bullet::State_GroundDamage did not handle iscript command %x for bullet %x", cmd.opcode, weapon_id);
        }
        return make_tuple(BulletState::Die, do_missile_dmgs);
    }
    else if (time_remaining % 7 == 0)
        do_missile_dmgs++;
    return make_tuple(BulletState::GroundDamage, do_missile_dmgs);
}

tuple<BulletState, int> Bullet::State_MoveToPoint()
{
    ProgressBulletMovement(this);
    if (time_remaining-- != 0 && position != move_target)
        return make_tuple(BulletState::MoveToPoint, 0);

    if (target)
        order_target_pos = target->sprite->position;
    int do_missile_dmgs = 0;
    for (auto &cmd : SetIscriptAnimation(IscriptAnim::Death, true))
    {
        if (cmd.opcode == IscriptOpcode::DoMissileDmg)
            do_missile_dmgs++;
        else
            Warning("Bullet::State_MoveToPoint did not handle iscript command %x for bullet %x", cmd.opcode, weapon_id);
    }
    return make_tuple(BulletState::Die, do_missile_dmgs);
}

tuple<BulletState, int> Bullet::State_MoveToUnit()
{
    if (!target)
        return State_MoveToPoint();
    if (flags & 0x1) // Stop following
    {
        order_target_pos = target->sprite->position;
        return State_MoveToPoint();
    }
    else
    {
        ChangeMovePos(this, target->sprite->position.x, target->sprite->position.y);
        auto result = State_MoveToPoint();
        if (get<BulletState>(result) == BulletState::MoveToPoint)
            return make_tuple(BulletState::MoveToTarget, get<int>(result));
        else
            return result;
    }
}

tuple<BulletState, int> Bullet::State_MoveNearUnit()
{
    if (!target)
        return State_MoveToPoint();
    if (flags & 0x1) // Stop following
    {
        order_target_pos = target->sprite->position;
        return State_MoveToPoint();
    }
    else
    {
        const Point32 &diff = random_chances[spread_seed];
        Point &pos = target->sprite->position;
        int x = min(*bw::map_width - 1, max(0, pos.x - diff.x));
        int y = min(*bw::map_height - 1, max(0, pos.y - diff.y));
        ChangeMovePos(this, x, y);
        auto result = State_MoveToPoint();
        if (get<BulletState>(result) == BulletState::MoveToPoint)
            return make_tuple(BulletState::MoveNearUnit, get<int>(result));
        else
            return result;
    }
}

vector<Unit *> BulletSystem::ProcessUnitWasHit(vector<tuple<Unit *, Unit *>> hits, ProgressBulletBufs *bufs)
{
    vector<Unit *> canceled_ai_units;
    // Ideally this shouldn't require sorting same for every player but safety never hurts
    std::sort(hits.begin(), hits.end(), [](const auto &a, const auto &b) {
        if (get<0>(a)->lookup_id == get<0>(b)->lookup_id)
            return get<1>(a)->lookup_id < get<1>(b)->lookup_id;
        return get<0>(a)->lookup_id < get<0>(b)->lookup_id;
    });

    Unit *previous = nullptr;
    bool cancel_unit = false;
    for (const auto &pair : hits.Unique())
    {
        Unit *target = get<0>(pair);
        Unit *attacker = get<1>(pair);
        //debug_log->Log("Proc %p %p\n", target, attacker);
        if (target != previous)
        {
            if (previous)
                previous->hotkey_groups &= ~0x80c00000;
            if (cancel_unit)
                canceled_ai_units.emplace_back(previous);
            cancel_unit = false;
        }
        cancel_unit |= UnitWasHit_Actual(target, attacker, bufs);
        previous = target;
    }
    if (previous)
        previous->hotkey_groups &= ~0x80c00000;
    if (cancel_unit)
        canceled_ai_units.emplace_back(previous);
    return canceled_ai_units;
}

// DamagedUnits() does not need to return synced vec?
void BulletSystem::ProcessHits(ProgressBulletBufs *bufs)
{
    for (auto &dmg_unit : *bufs->DamagedUnits())
    {
        Unit *unit = dmg_unit.base;
        if (!dmg_unit.IsDead())
        {
            unit->hitpoints -= unit->dmg_unit.damage;
            if (images_dat_damage_overlay[unit->sprite->main_image->image_id] &&
                    unit->flags & UnitStatus::Completed)
            {
                UpdateDamageOverlay(unit);
            }
            if (IsComputerPlayer(unit->player))
                Ai::Hide(unit);
        }
        unit->sprite->MarkHealthBarDirty();
        unit->UpdateStrength();
    }
}

void Bullet::NormalHit(ProgressBulletBufs *bufs)
{
    if (!target)
        return;

    if (flags & 0x1) // Don't follow target
    {
        if (target->ai && parent)
        {
            if (UnitWasHit(target, parent, true))
                bufs->unit_was_hit->emplace_back(target, parent);
        }
    }
    else
    {
        auto damage = GetWeaponDamage(this, target);
        if (weapons_dat_behaviour[weapon_id] == 7 && bounces_remaining < 2) // Bounce
        {
            if (bounces_remaining == 0)
                damage /= 9;
            if (bounces_remaining == 1)
                damage /= 3;
        }
        HitUnit(target, damage, bufs);
    }
}

template <bool air_splash>
void Bullet::Splash(ProgressBulletBufs *bufs, bool hit_own_units)
{
    int outer_splash = weapons_dat_outer_splash[weapon_id];
    Rect16 splash_area(sprite->position, outer_splash);

    int unit_amount;
    Unit **units, **units_beg;
    units = units_beg = unit_search->FindUnitsRect(splash_area, &unit_amount);
    bool hit_orig = false;
    bool storm = weapon_id == Weapon::PsiStorm;
    Unit **rand_fulldmg_pos = units;
    if (air_splash)
    {
        for (Unit *unit = *units++; unit; unit = *units++)
        {
            if (!hit_own_units && unit->player == player && unit != target)
                continue;
            if (!CanHitUnit(unit, unit, weapon_id))
                continue;
            if (!storm && unit == parent)
                continue;
            int distance = GetSplashDistance(this, unit);
            if (distance > outer_splash)
                continue;
            if (storm)
            {
                if (unit->is_under_storm)
                    continue;
                unit->is_under_storm = 1;
            }

            int damage = GetWeaponDamage(this, unit);
            if (unit == target)
            {
                if (distance <= weapons_dat_inner_splash[weapon_id])
                {
                    HitUnit(unit, damage, bufs);
                    hit_orig = true;
                    break;
                }
                else
                {
                    *rand_fulldmg_pos++ = unit;
                }
            }
            else if (unit->unit_id != Unit::Interceptor) // Yeah..
            {
                *rand_fulldmg_pos++ = unit;

                if (distance <= weapons_dat_middle_splash[weapon_id])
                    HitUnit(unit, damage / 2, bufs);
                else
                    HitUnit(unit, damage / 4, bufs);
            }
        }
    }
    if (!air_splash || hit_orig) // Simpler copy from previous loop
    {
        for (Unit *unit = *units++; unit; unit = *units++)
        {
            if (!hit_own_units && unit->player == player && unit != target)
                continue;
            if (!CanHitUnit(unit, unit, weapon_id))
                continue;
            if (!storm && unit == parent)
                continue;
            int distance = GetSplashDistance(this, unit);
            if (distance > outer_splash)
                continue;
            if (storm)
            {
                if (unit->is_under_storm)
                    continue;
                unit->is_under_storm = 1;
            }

            int damage = GetWeaponDamage(this, unit);
            if (!air_splash && distance <= weapons_dat_inner_splash[weapon_id])
                HitUnit(unit, damage, bufs);
            else if (!air_splash && unit->flags & UnitStatus::Burrowed) // Actually burrowed is fully immune to air spalsh
                continue;
            else if (air_splash && unit->unit_id == Unit::Interceptor) // Yup
                continue;
            else if (distance <= weapons_dat_middle_splash[weapon_id])
                HitUnit(unit, damage / 2, bufs);
            else
                HitUnit(unit, damage / 4, bufs);
        }
    }
    else if (air_splash && rand_fulldmg_pos != units_beg)
    {
        int count = rand_fulldmg_pos - units_beg;
        Unit *fulldmg;
        if (count < 0x8000)
            fulldmg = units_beg[Rand(0x3a) % count];
        else
            fulldmg = units_beg[(Rand(0x3a) | Rand(0x3a) << 0xf) % count];

        int damage = GetWeaponDamage(this, fulldmg);
        HitUnit(fulldmg, damage, bufs);
    }
    unit_search->PopResult();
}

void Bullet::Splash_Lurker(ProgressBulletBufs *bufs)
{
    // Yea inner splash (Even though they are same for default)
    auto area = Rect16(sprite->position, weapons_dat_inner_splash[weapon_id]);
    unit_search->ForEachUnitInArea(area, [this, bufs](Unit *unit)
    {
        if (unit->player == player && unit != target)
            return false;
        if (!CanHitUnit(unit, unit, weapon_id)) // attacker is target.. well does not matter
            return false;
        if (GetSplashDistance(this, unit) > weapons_dat_inner_splash[weapon_id])
            return false;

        if (parent)
        {
            for (int i = 0; i < 0x200; i++)
            {
                if (bw::lurker_hits[i * 2] == parent && bw::lurker_hits[i * 2 + 1] == target)
                    return false;
            }
            int pos = *bw::lurker_hits_used;
            if (pos != 0x10)
            {
                bw::lurker_hits[*bw::lurker_hits_pos * 0x20 + pos * 2] = parent;
                bw::lurker_hits[*bw::lurker_hits_pos * 0x20 + pos * 2 + 1] = target;
                *bw::lurker_hits_used = pos + 1;
            }
        }
        HitUnit(unit, GetWeaponDamage(this, unit), bufs);
        return false;
    });
}

void Bullet::AcidSporeHit() const
{
    Rect16 area(sprite->position, Spell::AcidSporeArea);
    unit_search->ForEachUnitInArea(area, [this](Unit *unit)
    {
        if (unit->player != player && ~units_dat_flags[unit->unit_id] & UnitFlags::Building && unit->IsFlying())
        {
            if (!unit->IsInvincible())
            {
                if (!unit->IsInvisible() || (unit->detection_status & 1 << player))
                    AcidSporeUnit(unit);
            }
        }
        return false;
    });
}

void Bullet::SpawnBroodlingHit(vector<Unit *> *killed_units) const
{
    IncrementKillScores(target, player);
    killed_units->emplace_back(target);
    parent->IncrementKills();
}

Optional<SpellCast> Bullet::DoMissileDmg(ProgressBulletBufs *bufs)
{
    auto effect = weapons_dat_effect[weapon_id];
    switch (effect)
    {
        case 0x1:
            NormalHit(bufs);
        break;
        case 0x2:
        case 0x3:
        case 0x5:
            if (weapon_id == Weapon::SubterraneanSpines)
                Splash_Lurker(bufs);
            else
                Splash<false>(bufs, effect != 0x3);
        break;
        case 0x18:
            Splash<true>(bufs, false);
        break;
        case 0x4:
            if (target && !target->IsDying())
                target->Lockdown(Spell::LockdownTime);
        break;
        case 0x6:
            if (target && !target->IsDying())
            {
                target->Parasite(player);
                if (parent)
                {
                    target->StartHelperSearch();
                    bufs->AddToAiReact(target, parent, false);
                }
            }
        break;
        case 0x7:
            if (parent && target && !target->IsDying())
            {
                target->StartHelperSearch();
                bufs->AddToAiReact(target, parent, true);
                SpawnBroodlingHit(bufs->killed_units);
                if (~target->flags & UnitStatus::Hallucination)
                    return SpellCast(player, target->sprite->position, Tech::SpawnBroodlings, parent);
            }
        break;
        case 0x8:
            EmpShockwave(parent, sprite->position);
        break;
        case 0x9:
            if (target && !target->IsDying())
            {
                PlaySound(Sound::Irradiate, target, 1, 0);
                target->Irradiate(parent, player);
            }
        break;
        case 0xa:
            Ensnare(parent, sprite->position);
        break;
        case 0xb:
            Plague(parent, order_target_pos, bufs->unit_was_hit); // Why otp for some spells?
        break;
        case 0xc:
            if (order_target_pos != Point(0,0)) // Dunno why only this spell does this check
                Stasis(parent, order_target_pos);
        break;
        case 0xd:
            return SpellCast(player, order_target_pos, Tech::DarkSwarm, parent);
        break;
        case 0xe:
            if (parent && target && !target->IsDying())
                parent->Consume(target, bufs->killed_units);
        break;
        case 0xf:
            if (target)
                HitUnit(target, GetWeaponDamage(this, target), bufs);
        break;
        case 0x10:
            if (target && !target->IsDying())
                target->Restoration();
        break;
        case 0x11:
            return SpellCast(player, order_target_pos, Tech::DisruptionWeb, parent);
        break;
        case 0x12:
            if (target && ~flags & 0x1) // Miss
            {
                HitUnit(target, GetWeaponDamage(this, target), bufs);
                if (~flags & 0x2) // Hallu
                    AcidSporeHit();
            }
        break;
        case 0x13:
        case 0x14:
            // Unused (Mind control and feedback), sc crashes but we stay silent..
        break;
        case 0x15:
            if (target && !target->IsDying())
                target->OpticalFlare(player);
        break;
        case 0x16:
            if (order_target_pos != Point(0, 0))
                Maelstrom(parent, order_target_pos);
        break;
        case 0x17:
            // Unused as well
        break;
    }
    return Optional<SpellCast>();
}

void BulletSystem::DeleteBullet(Bullet *bullet)
{
    bullet->SingleDelete();
    auto owning_vector = GetOwningVector(bullet);
    Assert(owning_vector->back()->bulletsystem_entry == owning_vector->size() - 1);
    owning_vector->back()->bulletsystem_entry = bullet->bulletsystem_entry;
    owning_vector->erase_at(bullet->bulletsystem_entry);
}

void BulletSystem::SwitchBulletState(ptr<Bullet> &bullet, BulletState old_state, BulletState new_state)
{
    if (old_state == new_state)
        return;
    auto old_vec = GetStateVector(old_state);
    auto new_vec = GetStateVector(new_state);
    old_vec->back()->bulletsystem_entry = bullet->bulletsystem_entry;
    auto bullet_ptr = old_vec->erase_at(bullet->bulletsystem_entry);
    bullet_ptr->bulletsystem_entry = new_vec->size();
    new_vec->emplace_back(move(bullet_ptr));
}

Claimed<vector<Bullet *>> BulletSystem::ProgressStates(vector<tuple<Bullet *, Unit *>> *new_bounce_targets)
{
    Claimed<vector<Bullet *>> do_missile_dmgs = bullet_buf.Claim();
    do_missile_dmgs->clear();
    for (Bullet *bullet : ActiveBullets())
    {
        bullet->sprite->UpdateVisibilityPoint();

        for (auto &cmd : bullet->sprite->ProgressFrame(IscriptContext(bullet), main_rng))
        {
            if (cmd.opcode == IscriptOpcode::End)
            {
                bullet->sprite->Remove();
                DeleteBullet(bullet);
            }
            else if (cmd.opcode == IscriptOpcode::DoMissileDmg)
                do_missile_dmgs->push_back(bullet);
            else
                Warning("Unhandled iscript command %x in BulletSystem::ProgressStates, weapon %x", cmd.opcode, bullet->weapon_id);
        }
    }

    for (ptr<Bullet> &bullet : initstate.SafeIter())
    {
        auto result = bullet->State_Init();
        int dmd_count = get<int>(result);
        while (dmd_count--)
            do_missile_dmgs->push_back(bullet.get());
        SwitchBulletState(bullet, BulletState::Init, get<BulletState>(result));
    }
    for (ptr<Bullet> &bullet : moving_to_point.SafeIter())
    {
        auto result = bullet->State_MoveToPoint();
        int dmd_count = get<int>(result);
        while (dmd_count--)
            do_missile_dmgs->push_back(bullet.get());
        SwitchBulletState(bullet, BulletState::MoveToPoint, get<BulletState>(result));
    }
    for (ptr<Bullet> &bullet : moving_to_unit.SafeIter())
    {
        auto result = bullet->State_MoveToUnit();
        int dmd_count = get<int>(result);
        while (dmd_count--)
            do_missile_dmgs->push_back(bullet.get());
        SwitchBulletState(bullet, BulletState::MoveToTarget, get<BulletState>(result));
    }
    for (ptr<Bullet> &bullet : damage_ground.SafeIter())
    {
        auto result = bullet->State_GroundDamage();
        int dmd_count = get<int>(result);
        while (dmd_count--)
            do_missile_dmgs->push_back(bullet.get());
        SwitchBulletState(bullet, BulletState::GroundDamage, get<BulletState>(result));
    }
    for (ptr<Bullet> &bullet : moving_near.SafeIter())
    {
        auto result = bullet->State_MoveNearUnit();
        int dmd_count = get<int>(result);
        while (dmd_count--)
            do_missile_dmgs->push_back(bullet.get());
        SwitchBulletState(bullet, BulletState::MoveNearUnit, get<BulletState>(result));
    }
    for (ptr<Bullet> &bullet : bouncing.SafeIter())
    {
        auto result = bullet->State_Bounce();
        int dmd_count = get<int>(result);
        while (dmd_count--)
            do_missile_dmgs->push_back(bullet.get());
        Unit *new_target = get<Unit *>(result);
        if (new_target != nullptr)
            new_bounce_targets->emplace_back(bullet.get(), new_target);
        SwitchBulletState(bullet, BulletState::Bounce, get<BulletState>(result));
    }

    return do_missile_dmgs;
}

// Input should not need to be synced
vector<Ai::HitUnit> BulletSystem::ProcessAiReactToHit(vector<tuple<Unit *, Unit *, bool>> input, Ai::HelpingUnitVec *helping_units)
{
    STATIC_PERF_CLOCK(BulletSystem_Parth);
    // I would not dare to have it sort by pointer here ever, Ai::ReactToHit is really complicated piece of code
    std::sort(input.begin(), input.end(), [](const auto &a, const auto &b) {
        if (get<0>(a)->lookup_id != get<0>(b)->lookup_id)
            return get<0>(a)->lookup_id < get<0>(b)->lookup_id;
        else if (get<1>(a)->lookup_id != get<1>(b)->lookup_id)
            return get<1>(a)->lookup_id < get<1>(b)->lookup_id;
        else
            return get<2>(a) > get<2>(b);
    });

    Unit *previous = nullptr;
    Ai::HitUnit *hit_unit = nullptr;
    vector<Ai::HitUnit> hit_units;
    helping_units->reserve(input.size());
    // Assuming here that it does not make difference if repeating this with same target-attacker pair
    // or repeating without main_target_reactions once main_target_reactions has been true
    // ((It should be sorted so that mtr=true becomes first))
    for (const auto &tp : input.Unique([](const auto &a, const auto &b) {
        return get<0>(a) == get<0>(b) && get<1>(a) == get<1>(b);
    }))
    {
        auto target = get<0>(tp);
        auto attacker = get<1>(tp);
        auto main_target_reactions = get<2>(tp);
        if (previous != target)
        {
            hit_units.emplace_back(target);
            hit_unit = &hit_units.back();
            previous = target;
            // This clear is be usually unnecessary as it is done for every bullet hit in UnitWasHit, but
            // broodling and parasite hits only trigger Ai::ReactToHit so clear flags here as well
            // Idk if clearing the 0x00c00000 is even necessary but it won't hurt ^_^
            target->hotkey_groups &= ~0x80c00000;
        }
        Ai::ReactToHit(hit_unit, attacker, main_target_reactions, helping_units);
    }
    return hit_units;
}

// input should be sorted so the iteration order is consistent across all players' computers
// It should also not have duplicates
// And it yields objects which have functions Own() -> Unit * and Enemies -> Iterator<ref<Unit>>
template<class Iterable>
void ProcessAiUpdateAttackTarget(Iterable input)
{
    for (auto first : input)
    {
        Unit *own = first.Own();
        bool must_reach = own->order == Order::Pickup4;

        Unit *picked = nullptr;
        for (const ref<Unit> &other_ : first.Enemies())
        {
            Unit *other = &other_.get();
            if (other->hitpoints == 0)
                continue;
            if (must_reach && own->IsUnreachable(other))
                continue;

            if (!other->IsDisabled())
            {
                if ((other->unit_id != Unit::Bunker) && (!other->target || other->target->player != own->player))
                    continue;
                else if (!own->CanAttackUnit(other, true))
                    continue;
            }
            picked = own->ChooseBetterTarget(other, picked);
        }

        if (picked)
        {
            own->SetPreviousAttacker(picked);
            if (own->HasSubunit())
                own->subunit->SetPreviousAttacker(picked);
            Ai::UpdateAttackTarget(own, false, true, false);
        }
    }
}

class IterateUatValue
{
    friend class enemies;
    public:
        class enemies : public Common::Iterator<enemies, ref<Unit>> {
            public:
                enemies(Unit *f, Unit *s) : first(f) {
                    if (s == nullptr) {
                        second = move(Optional<ref<Unit>>());
                    } else {
                        second = move(Optional<ref<Unit>>(ref<Unit>(*s)));
                    }
                }
                Optional<ref<Unit>> next() {
                    if (first != nullptr)
                    {
                        auto ret = first;
                        first = nullptr;
                        return Optional<ref<Unit>>(*ret);
                    }
                    else
                    {
                        auto ret = move(second);
                        second = Optional<ref<Unit>>();
                        return ret;
                    }
                }

            private:
                Unit *first;
                Optional<ref<Unit>> second;
        };

        IterateUatValue(Ai::HitUnit *f, Unit *s) : first(f), second(s) {}
        IterateUatValue(Ai::HitUnit *f) : first(f), second(nullptr) {}

        Unit *Own() { return first->unit; }
        enemies Enemies() {
            return enemies(first->picked_target, second);
        }

    private:
        Ai::HitUnit *first;
        Unit * second;
};

class IterateUat : public Common::Iterator<IterateUat, IterateUatValue>
{
    public:
        using value = IterateUatValue;
        IterateUat(vector<Ai::HitUnit> &&f, vector<Ai::HitUnit> &&s) : first(move(f)), second(move(s)) {
            std::sort(first.begin(), first.end(),
                    [](const auto &a, const auto &b) { return a.unit->lookup_id > b.unit->lookup_id; });
            std::sort(second.begin(), second.end(),
                    [](const auto &a, const auto &b) { return a.unit->lookup_id > b.unit->lookup_id; });
            // Dirty tricks - final entry having lookup id 0 and sorting reverse is slightly more efficent
            // (Should be)
            Unit *fake_unit = (Unit *)fake_unit_buf;
            fake_unit->lookup_id = 0;
            first.emplace_back(fake_unit);
            second.emplace_back(fake_unit);
            first_pos = first.begin();
            second_pos = second.begin();
        }
        IterateUat(IterateUat &&other) = default;
        IterateUat& operator=(IterateUat &&other) = default;

        Optional<value> next()
        {
            Optional<value> ret;
            if (first_pos->unit->lookup_id == second_pos->unit->lookup_id) {
                if (first_pos->unit->lookup_id == 0)
                    return Optional<value>();
                ret = Optional<value>(value(&*first_pos, second_pos->picked_target));
                ++first_pos;
                ++second_pos;
            }
            else if (first_pos->unit->lookup_id > second_pos->unit->lookup_id) {
                ret = Optional<value>(value(&*first_pos));
                ++first_pos;
            }
            else {
                ret = Optional<value>(value(&*second_pos));
                ++second_pos;
            }
            return ret;
        }

    private:
        vector<Ai::HitUnit> first;
        vector<Ai::HitUnit> second;
        vector<Ai::HitUnit>::iterator first_pos;
        vector<Ai::HitUnit>::iterator second_pos;
        char fake_unit_buf[sizeof(Unit)];
};

void BulletSystem::ProgressFrames(BulletFramesInput input)
{
    StaticPerfClock::ClearWithLog("Bullet::ProgressFrames");
    unsigned int prev_sleep = 0;
    if (PerfTest)
        prev_sleep = threads->GetSleepCount();
    PerfClock clock, clock2;

    auto new_bounce_targets = bounce_target_buf.Claim();
    new_bounce_targets->clear();
    auto do_missile_dmgs = ProgressStates(&new_bounce_targets.Inner());

    auto dmg_units = dmg_unit_buf.Claim();
    auto spells = spell_buf.Claim();
    auto unit_was_hit = unit_was_hit_buf.Claim();
    auto ai_react = ai_react_buf.Claim();
    auto killed_units = killed_units_buf.Claim();
    ProgressBulletBufs bufs(&dmg_units.Inner(), &unit_was_hit.Inner(), &ai_react.Inner(), &killed_units.Inner());
    dmg_units->clear();
    spells->clear();
    killed_units->clear();
    bulletframes_in_progress = true;
    for (Bullet *bullet : do_missile_dmgs.Inner())
    {
        auto spell = bullet->DoMissileDmg(&bufs);
        if (spell)
            spells->emplace_back(move(spell.take()));
    }
    auto pbf_time = clock.GetTime();
    clock.Start();

    // Currently only melee attacks, as they do not create bullets, but use weapons.dat
    for (auto &i : input.weapon_damages)
    {
        // Necessary check? Needs tests if removed
        if (!i.target->IsInvincible())
        {
            DamagedUnit dmg_unit = bufs.GetDamagedUnit(i.target);
            dmg_unit.AddHit(i.damage, i.weapon, i.player, i.direction, i.attacker, &bufs);
        }
    }
    for (auto &i : input.hallucination_hits)
    {
        HallucinationHit(i.target, i.attacker, i.direction, bufs.unit_was_hit);
    }
    ProcessHits(&bufs);
    auto ph_time = clock.GetTime();
    clock.Start();

    Ai::UpdateRegionEnemyStrengths();

    auto canceled_ai_units = ProcessUnitWasHit(move(*bufs.unit_was_hit), &bufs);
    auto update_attack_targets = ProcessAiReactToHit(move(*bufs.ai_react), &input.helping_units);
    auto puwh_time = clock.GetTime();
    clock.Start();

    auto more_uat = Ai::ProcessAskForHelp(move(input.helping_units));
    auto afh_time = clock.GetTime();

    Ai::ClearRegionChangeList();
    clock.Start();

    ProcessAiUpdateAttackTarget(IterateUat(move(update_attack_targets), move(more_uat)));

    threads->ClearAll();
    pbf_memory.ClearAll();
    threads->ForEachThread([](ScThreadVars *vars) { vars->unit_search_pool.ClearAll(); });
    unit_search->valid_region_cache = false;
    unit_search->DisableAreaCache();
    bulletframes_in_progress = false;

    for (const auto &tuple : new_bounce_targets.Inner())
    {
        Bullet *bullet = get<Bullet *>(tuple);
        Unit *new_target = get<Unit *>(tuple);
        Assert(new_target != bullet->target);
        bullet->SetTarget(new_target);
        bullet->target = new_target;
    }

    // These two are delayed as they invalidate unit search caches
    for (const auto &spell : spells.Inner())
    {
        switch (spell.tech)
        {
            case Tech::SpawnBroodlings:
                spell.parent->SpawnBroodlings(spell.pos - Point(2, 2));
            break;
            case Tech::DarkSwarm:
                DarkSwarm(spell.player, spell.pos);
            break;
            case Tech::DisruptionWeb:
                DisruptionWeb(spell.player, spell.pos);
            break;
        }
    }
    for (Unit *unit : canceled_ai_units)
    {
        unit->CancelConstruction(nullptr);
    }
    for (Unit *unit : *bufs.killed_units)
    {
        debug_log->Log("Killed %08X\n", unit->lookup_id);
        unit->Kill(nullptr);
    }

    perf_log->Log("Pbf: %f ms + Ph %f ms + Puwh %f ms + Afh %f ms + Uaat %f ms = about %f ms\n", pbf_time, ph_time, puwh_time, afh_time, clock.GetTime(), clock2.GetTime());
    perf_log->Log("Sleep count: %d\n", threads->GetSleepCount() - prev_sleep);
    perf_log->Indent(2);
    StaticPerfClock::LogCalls();
    perf_log->Indent(-2);
}

void __fastcall RemoveFromBulletTargets(Unit *unit)
{
    for (Bullet *bullet : unit->targeting_bullets)
    {
        bullet->target = nullptr;
        bullet->order_target_pos = unit->sprite->position;
    }
    for (Bullet *bullet : unit->spawned_bullets)
    {
        bullet->parent = nullptr;
    }
    unit->targeting_bullets = nullptr;
    unit->spawned_bullets = nullptr;
}

Sprite::ProgressFrame_C Bullet::SetIscriptAnimation(int anim, bool force)
{
    return sprite->SetIscriptAnimation(anim, force, IscriptContext(this), main_rng);
}
