#include "bullet.h"

#include <deque>
#include <tuple>
#include <algorithm>

#include "constants/order.h"
#include "constants/sprite.h"
#include "constants/tech.h"
#include "constants/upgrade.h"
#include "constants/unit.h"
#include "unit.h"
#include "unitsearch.h"
#include "sprite.h"
#include "yms.h"
#include "image.h"
#include "offsets.h"
#include "order.h"
#include "player.h"
#include "upgrade.h"
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
#include "damage_calculation.h"
#include "strings.h"
#include "entity.h"

using std::get;
using std::min;
using std::max;

const DamageCalculation default_damage_calculation;

BulletSystem *bullet_system;
bool bulletframes_in_progress = false; // Protects calling Kill() when FindHelpingUnits may be run

namespace {
const Point32 random_chances[] = { Point32(1, 1), Point32(51, 31), Point32(7, 12), Point32(-36, -52), Point32(-49, 25),
  Point32(40, -46), Point32(-4, -19), Point32(28, 50), Point32(50, -15), Point32(-54, -13), Point32(11, -53),
  Point32(15, -9), Point32(-19, 50), Point32(-17, 8), Point32(0, 0) };
}

TempMemoryPool pbf_memory;

static void UnitKilled(Unit *target, Unit *attacker, int attacking_player, vector<Unit *> *killed_units)
{
    // Kinda stupid that TransportDeath() does about the same...
    // Guess it is necessary if trigger kills the unit,
    // this when we care about kill scores :/
    if (!target->Type().IsBuilding()) // Let's not kill units inside bunker
    {
        Unit *loaded, *next = target->first_loaded;
        while (next)
        {
            loaded = next;
            next = loaded->next_loaded;
            if (loaded->OrderType() != OrderId::Die) // Necessary?
            {
                killed_units->emplace_back(loaded);
                score->RecordDeath(loaded, attacking_player);
            }
        }
    }
    //debug_log->Log("Unit %p got killed by damage\n", target);
    target->hitpoints = 0;
    killed_units->emplace_back(target);
    score->RecordDeath(target, attacking_player);
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

int32_t DamagedUnit::GetDamage()
{
    return base->dmg_unit.damage;
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

/// Convenience function for DamageCalculation class which can be annoying
/// (and error-prone) to initialize with all the variables units can have.
static DamageCalculation MakeDamageCalculation(Unit *unit, uint32_t base_damage, WeaponType weapon_id)
{
    int32_t shields = 0;
    if (unit->Type().HasShields() && unit->shields >= 256)
        shields = unit->shields;

    return default_damage_calculation
        .BaseDamage(base_damage)
        .HitPoints(unit->hitpoints)
        .Shields(shields)
        .MatrixHp(unit->matrix_hp)
        .ArmorReduction(unit->GetArmor() * 256)
        .ShieldReduction(GetUpgradeLevel(UpgradeId::ProtossPlasmaShields, unit->player) * 256)
        .ArmorType(unit->Type().ArmorType())
        .DamageType(weapon_id.DamageType())
        .Hallucination(unit->flags & UnitStatus::Hallucination)
        .AcidSpores(unit->acid_spore_count)
        .IgnoreArmor(weapon_id.DamageType() == 4);
}

void DamagedUnit::AddHit(uint32_t dmg, WeaponType weapon_id, int player, int direction, Unit *attacker, ProgressBulletBufs *bufs)
{
    if (IsDead())
        return;

    auto damage = MakeDamageCalculation(base, dmg, weapon_id).Calculate();
    if (damage.shield_damage)
        base->DamageShields(damage.shield_damage, direction);
    if (damage.matrix_damage)
        DoMatrixDamage(base, damage.matrix_damage);

    if (!IsCheatActive(Cheats::Power_Overwhelming) || IsHumanPlayer(player))
        AddDamage(damage.hp_damage);
    if (IsDead())
    {
        UnitKilled(base, attacker, player, bufs->killed_units);
    }
    else if (attacker != nullptr)
    {
        if (base->player != attacker->player && weapon_id != WeaponId::Irradiate)
            bw::Notify_UnitWasHit(base);
        if (UnitWasHit(base, attacker, weapon_id != WeaponId::Irradiate))
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

BulletSystem::BulletContainer *BulletSystem::GetStateContainer(BulletState state)
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

WeaponType Bullet::Type() const
{
    return WeaponType(weapon_id);
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
    bw::MoveSprite(sprite.get(), where.x, where.y);
}

// Spawner is not necessarily parent, in case of subunits/fighters
bool Bullet::Initialize(Unit *spawner, int player_, int direction, WeaponType weapon, const Point &pos)
{
    Assert(!bulletframes_in_progress);
    list.prev = nullptr;
    list.next = nullptr;

    move_target = Point(0xffff, 0xffff);
    current_speed = 0;

    // Yes, bw mixes bullet's images with spawner's unit code.
    // At least wraith's lasers actually depend on this behaviour.
    const char *desc = "Bullet::Initialize (First frame of bullet's animation modifies the unit who spawned it)";
    UnitIscriptContext ctx(spawner, nullptr, desc, MainRng(), false);
    bool success = ((Flingy *)this)->Initialize(&ctx, weapon.Flingy(), player_, direction, pos);
    if (!success)
        return false;

    player = player_;
    weapon_id = weapon.Raw();
    time_remaining = weapon.DeathTime();
    flingy_flags |= 0x8;
    flags = 0;
    bounces_remaining = 0;
    // Bw calls State_Init here, it should just return instantly though as no iscript has been run
    order_signal = 0;
    auto spin = weapon.LaunchSpin();
    if (spin != 0)
    {
        bool spin_positive = MainRng()->Rand(2) == 1;
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

    if (spawner->Type().IsSubunit())
        parent = spawner->subunit;
    else if (spawner->Type() == UnitId::Scarab)
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

    switch (weapon.Behaviour())
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
            if (MainRng()->Rand(0x100) <= bw::GetMissChance(parent, target))
            {
                int x = sprite->position.x - bw::circle[direction][0] * 30 / 256;
                int y = sprite->position.y - bw::circle[direction][1] * 30 / 256;
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
            auto max_range = (Type().MaxRange() + 20) * 256;
            auto x = bw::circle[facing_direction][0] * max_range / 65536;
            auto y = bw::circle[facing_direction][1] * max_range / 65536;
            order_target_pos = spawner->sprite->position + Point(x, y);
            UpdateMoveTarget(order_target_pos);
        }
        break;
        case 0x7: case 0x1: case 0x0: // Bounce, Fly & follow/don't
            bounces_remaining = 3; // Won't matter on others
            if (target && parent)
            {
                if (MainRng()->Rand(0x100) <= bw::GetMissChance(parent, target))
                {
                    int x = order_target_pos.x - bw::circle[direction][0] * 30 / 256;
                    int y = order_target_pos.y - bw::circle[direction][1] * 30 / 256;
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
            Warning("Unknown weapons.dat behaviour %x for weapon %x", Type().Behaviour(), weapon_id);
            return false;
        break;
    }
    return true;
}

Bullet *BulletSystem::AllocateBullet(Unit *parent, int player, int direction, WeaponType weapon, const Point &pos_)
{
    Point pos = pos_;
    if (pos.x > 0xc000)
        pos.x = 0;
    if (pos.y > 0xc000)
        pos.y = 0;
    pos.x = min(pos.x, (x16u)(*bw::map_width - 1));
    pos.y = min(pos.y, (y16u)(*bw::map_height - 1));
    if (parent->flags & UnitStatus::UnderDweb && !weapon.WorksUnderDisruptionWeb())
    {
        return nullptr;
    }
    ptr<Bullet> bullet_ptr = ptr<Bullet>(new Bullet);
    if (bullet_ptr->Initialize(parent, player, direction, weapon, pos) == true)
    {
        initstate.emplace(move(bullet_ptr));
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
        debug_log->Log("Bullet creation failed %x %x.%x\n", weapon.Raw(), pos.x, pos.y);
        if (bullet_ptr->sprite != nullptr)
            bullet_ptr->sprite->Remove();
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
    int left = target->sprite->position.x - target->Type().DimensionBox().left;
    int right = target->sprite->position.x + target->Type().DimensionBox().right - 1;
    int top = target->sprite->position.y - target->Type().DimensionBox().top;
    int bottom = target->sprite->position.y + target->Type().DimensionBox().bottom - 1;
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

int GetWeaponDamage(const Unit *target, WeaponType weapon_id, int player)
{
    if (weapon_id.Effect() == 5) // nuke
    {
        int hp = target->Type().HitPoints() >> 8;
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
        int dmg = weapon_id.Damage();
        dmg += GetUpgradeLevel(weapon_id.Upgrade(), player) * weapon_id.UpgradeBonus();
        return dmg * 256;
    }
}

static int GetWeaponDamage(const Bullet *bullet, const Unit *target)
{
    return GetWeaponDamage(target, bullet->Type(), bullet->player);
}

// Before UnitWasHit did this but now it only constructs a list and extracts it later here,
// skipping cases where target was hit by same attacker multiple times
// Returns true if ai wants to cancel target
bool UnitWasHit_Actual(Unit *target, Unit *attacker, ProgressBulletBufs *bufs)
{
    target->last_attacking_player = attacker->player;
    if (target->ai && !target->previous_attacker)
    {
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

        bool ai_player = IsComputerPlayer(target->player);

        for (Unit *unit = *nearby++; unit; unit = *nearby++)
        {
            const Point &unit_pos = unit->sprite->position;
            const Point &target_pos = target->sprite->position;

            // Hackfix for units without ai owned by ai players.
            // Their nearby_helping_units has larger search area than otherwise,
            // so check for reduced area here.
            // Yes, some units get Ai_AskForHelp and Unit::AskForHelp for same target then
            // No idea if it would even change anything to remove this check, but it's
            // slightly closer to bw behaviour.
            if (ai_player && (abs(unit_pos.x - target_pos.x) > CallFriends_Radius ||
                        abs(unit_pos.y - target_pos.y) > CallFriends_Radius))
            {
                continue;
            }
            if (unit->OrderType() != OrderId::Die)
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
        bw::ShowArea(1, 1 << target->player, attacker->sprite->position.x, attacker->sprite->position.y, attacker->IsFlying());
    }
    return true;
}

void DamageUnit(int damage, Unit *target, vector<Unit *> *killed_units)
{
    target->sprite->MarkHealthBarDirty();
    if (damage < target->hitpoints)
    {
        target->hitpoints -= damage;
        if (target->sprite->main_image->Type().DamageOverlay() && target->flags & UnitStatus::Completed)
            bw::UpdateDamageOverlay(target);
    }
    else
    {
        UnitKilled(target, nullptr, -1, killed_units);
    }
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
            bw::Notify_UnitWasHit(target);
    }
    if (target->shields >= 256 && target->Type().HasShields())
        target->ShowShieldHitOverlay(direction);
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
        dmg_unit.AddHit(dmg, Type(), player, facing_direction, parent, bufs);
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

BulletState Bullet::State_Bounce(BulletStateResults *results)
{
    if (target != nullptr && !DoesMiss())
        bw::ChangeMovePos(this, target->sprite->position.x, target->sprite->position.y);

    bw::ProgressBulletMovement(this);
    if (move_target == position)
    {
        bounces_remaining--;
        if (bounces_remaining)
        {
            Unit *new_target = ChooseBounceTarget();
            previous_target = target;
            if (new_target)
            {
                SetIscriptAnimation(Iscript::Animation::Special1, true, "Bullet::State_Bounce", results);
                results->new_bounce_targets.emplace(this, new_target);
                return BulletState::Bounce;
            }
        }

        order_state = 0;
        order_fow_unit = UnitId::None.Raw();
        if (target)
            order_target_pos = target->sprite->position;
        SetIscriptAnimation(Iscript::Animation::Death, true, "Bullet::State_Bounce", results);
        return BulletState::Die;
    }
    return BulletState::Bounce;
}

BulletState Bullet::State_Init(BulletStateResults *results)
{
    if (~order_signal & 0x1)
        return BulletState::Init;
    order_signal &= ~0x1;

    BulletState state;
    int anim;
    //debug_log->Log("Inited bullet %p %x %x\n", this, weapon_id, sprite->main_image->iscript.pos);
    switch (Type().Behaviour())
    {
        case 0x2:
        case 0x4:
        case 0x5:
        case 0x6:
            state = BulletState::Die;
            anim = Iscript::Animation::Death;
        break;
        case 0x1:
            state = BulletState::MoveToTarget;
            anim = Iscript::Animation::GndAttkInit;
        break;
        case 0x7:
            state = BulletState::Bounce;
            anim = Iscript::Animation::GndAttkInit;
        break;
        case 0x3:
            state = BulletState::GroundDamage;
            anim = Iscript::Animation::Special2;
        break;
        case 0x8:
            state = BulletState::MoveNearUnit;
            anim = Iscript::Animation::GndAttkInit;
        break;
        default:
            state = BulletState::MoveToPoint;
            anim = Iscript::Animation::GndAttkInit;
        break;
    }
    if (target)
        order_target_pos = target->sprite->position;
    order_fow_unit = UnitId::None.Raw();
    order_state = 0;
    SetIscriptAnimation(anim, true, "Bullet::State_Init", results);
    return state;
}

// Psi storm behaviour
BulletState Bullet::State_GroundDamage(BulletStateResults *results)
{
    if (time_remaining-- == 0)
    {
        if (target)
            order_target_pos = target->sprite->position;
        order_state = 0;
        order_fow_unit = UnitId::None.Raw();
        SetIscriptAnimation(Iscript::Animation::Death, true, "Bullet::State_GroundDamage", results);
        return BulletState::Die;
    }
    else if (time_remaining % 7 == 0)
        results->do_missile_dmgs.emplace(this);
    return BulletState::GroundDamage;
}

BulletState Bullet::State_MoveToPoint(BulletStateResults *results)
{
    bw::ProgressBulletMovement(this);
    if (time_remaining-- != 0 && position != move_target)
        return BulletState::MoveToPoint;

    if (target)
        order_target_pos = target->sprite->position;
    SetIscriptAnimation(Iscript::Animation::Death, true, "Bullet::State_MoveToPoint", results);
    return BulletState::Die;
}

BulletState Bullet::State_MoveToUnit(BulletStateResults *results)
{
    if (!target)
        return State_MoveToPoint(results);
    if (DoesMiss())
    {
        order_target_pos = target->sprite->position;
        return State_MoveToPoint(results);
    }
    else
    {
        bw::ChangeMovePos(this, target->sprite->position.x, target->sprite->position.y);
        auto result = State_MoveToPoint(results);
        if (result == BulletState::MoveToPoint)
            return BulletState::MoveToTarget;
        else
            return result;
    }
}

BulletState Bullet::State_MoveNearUnit(BulletStateResults *results)
{
    if (!target)
        return State_MoveToPoint(results);
    if (DoesMiss())
    {
        order_target_pos = target->sprite->position;
        return State_MoveToPoint(results);
    }
    else
    {
        const Point32 &diff = random_chances[spread_seed];
        Point &pos = target->sprite->position;
        int x = min(*bw::map_width - 1, max(0, pos.x - diff.x));
        int y = min(*bw::map_height - 1, max(0, pos.y - diff.y));
        bw::ChangeMovePos(this, x, y);
        auto result = State_MoveToPoint(results);
        if (result == BulletState::MoveToPoint)
            return BulletState::MoveNearUnit;
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
            unit->hitpoints -= dmg_unit.GetDamage();
            if (unit->sprite->main_image->Type().DamageOverlay() && unit->flags & UnitStatus::Completed)
            {
                bw::UpdateDamageOverlay(unit);
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

    if (DoesMiss())
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
        if (Type().Behaviour() == 7 && bounces_remaining < 2) // Bounce
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
    int outer_splash = Type().OuterSplash();
    Rect16 splash_area(sprite->position, outer_splash);

    int unit_amount;
    Unit **units, **units_beg;
    units = units_beg = unit_search->FindUnitsRect(splash_area, &unit_amount);
    bool hit_orig = false;
    bool storm = Type() == WeaponId::PsiStorm;
    Unit **rand_fulldmg_pos = units;
    if (air_splash)
    {
        for (Unit *unit = *units++; unit; unit = *units++)
        {
            if (!hit_own_units && unit->player == player && unit != target)
                continue;
            if (!bw::CanHitUnit(unit, unit, weapon_id))
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
                if (distance <= Type().InnerSplash())
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
            else if (unit->Type() != UnitId::Interceptor) // Yeah..
            {
                *rand_fulldmg_pos++ = unit;

                if (distance <= Type().MiddleSplash())
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
            if (!bw::CanHitUnit(unit, unit, weapon_id))
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
            if (!air_splash && distance <= Type().InnerSplash())
                HitUnit(unit, damage, bufs);
            else if (!air_splash && unit->flags & UnitStatus::Burrowed) // Actually burrowed is fully immune to air spalsh
                continue;
            else if (air_splash && unit->Type() == UnitId::Interceptor) // Yup
                continue;
            else if (distance <= Type().MiddleSplash())
                HitUnit(unit, damage / 2, bufs);
            else
                HitUnit(unit, damage / 4, bufs);
        }
    }
    else if (air_splash && rand_fulldmg_pos != units_beg)
    {
        int count = rand_fulldmg_pos - units_beg;
        Unit *fulldmg = units_beg[MainRng()->Rand(count)];
        int damage = GetWeaponDamage(this, fulldmg);
        HitUnit(fulldmg, damage, bufs);
    }
    unit_search->PopResult();
}

void Bullet::Splash_Lurker(ProgressBulletBufs *bufs)
{
    // Yea inner splash (Even though they are same for default)
    auto area = Rect16(sprite->position, Type().InnerSplash());
    unit_search->ForEachUnitInArea(area, [this, bufs](Unit *unit)
    {
        if (unit->player == player && unit != target)
            return false;
        if (!bw::CanHitUnit(unit, unit, weapon_id)) // attacker is target.. well does not matter
            return false;
        if (GetSplashDistance(this, unit) > Type().InnerSplash())
            return false;

        if (parent)
        {
            for (auto hits : bw::lurker_hits)
            {
                for (auto hit : hits)
                {
                    if (hit[0] == parent && hit[1] == unit)
                    return false;
                }
            }
            int pos = *bw::lurker_hits_used;
            if (pos != 0x10)
            {
                bw::lurker_hits[*bw::lurker_hits_pos][pos][0] = parent;
                bw::lurker_hits[*bw::lurker_hits_pos][pos][1] = unit;
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
        if (unit->player != player && !unit->Type().IsBuilding() && unit->IsFlying())
        {
            if (!unit->IsInvincible())
            {
                if (!unit->IsInvisible() || (unit->detection_status & 1 << player))
                    bw::AcidSporeUnit(unit);
            }
        }
        return false;
    });
}

void Bullet::SpawnBroodlingHit(vector<Unit *> *killed_units) const
{
    score->RecordDeath(target, player);
    killed_units->emplace_back(target);
    parent->IncrementKills();
}

Optional<SpellCast> Bullet::DoMissileDmg(ProgressBulletBufs *bufs)
{
    auto effect = Type().Effect();
    switch (effect)
    {
        case 0x1:
            NormalHit(bufs);
        break;
        case 0x2:
        case 0x3:
        case 0x5:
            if (Type() == WeaponId::SubterraneanSpines)
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
                if (parent && IsComputerPlayer(target->player))
                {
                    target->StartHelperSearch();
                    bufs->AddToAiReact(target, parent, false);
                }
            }
        break;
        case 0x7:
            if (parent && target && !target->IsDying())
            {
                if (IsComputerPlayer(target->player))
                {
                    target->StartHelperSearch();
                    bufs->AddToAiReact(target, parent, true);
                }
                SpawnBroodlingHit(bufs->killed_units);
                if (~target->flags & UnitStatus::Hallucination)
                    return SpellCast(player, target->sprite->position, TechId::SpawnBroodlings, parent);
            }
        break;
        case 0x8:
            EmpShockwave(parent, sprite->position);
        break;
        case 0x9:
            if (target && !target->IsDying())
            {
                bw::PlaySound(Sound::Irradiate, target, 1, 0);
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
            return SpellCast(player, order_target_pos, TechId::DarkSwarm, parent);
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
            return SpellCast(player, order_target_pos, TechId::DisruptionWeb, parent);
        break;
        case 0x12:
            if (target != nullptr && !DoesMiss())
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

void BulletSystem::DeleteBullet(BulletContainer::entry *bullet)
{
    (*bullet)->get()->SingleDelete();
    bullet->swap_erase();
}

void BulletSystem::SwitchBulletState(BulletContainer::entry *bullet, BulletState new_state)
{
    BulletContainer *new_container = GetStateContainer(new_state);
    bullet->move_to(new_container);
}

void BulletSystem::ProgressBulletsForState(BulletContainer *container, BulletStateResults *results,
    BulletState state, BulletState (Bullet::*state_function)(BulletStateResults *))
{
    for (auto entry : container->Entries())
    {
        BulletState result = ((entry)->get()->*state_function)(results);
        if (result != state)
            SwitchBulletState(&entry, result);
    }
}

class BulletIscriptContext : public Iscript::Context
{
    public:
        constexpr BulletIscriptContext(Bullet *bullet, BulletStateResults *results,
                                       const char *caller, Rng *rng, bool can_delete) :
            Iscript::Context(rng, can_delete),
            bullet(bullet), results(results), caller(caller) { }

        Bullet * const bullet;
        BulletStateResults * const results;
        const char * const caller;

        void ProgressIscript() { bullet->sprite->ProgressFrame(this); }
        void SetIscriptAnimation(int anim, bool force) { bullet->sprite->SetIscriptAnimation(this, anim, force); }

        virtual Iscript::CmdResult HandleCommand(Image *img, Iscript::Script *script,
                                                 const Iscript::Command &cmd) override
        {
            Iscript::CmdResult result = HandleIscriptCommand(img, script, cmd);
            if (result == Iscript::CmdResult::NotHandled)
                bullet->WarnUnhandledIscriptCommand(cmd, caller);
            return result;
        }

        /// Calls entity->flingy->sprite->img handlers as needed.
        Iscript::CmdResult HandleIscriptCommand(Image *img, Iscript::Script *script, const Iscript::Command &cmd)
        {
            using Iscript::CmdResult;
            CmdResult result = CmdResult::Handled;
            switch (cmd.opcode)
            {
                case Iscript::Opcode::DoMissileDmg:
                    results->do_missile_dmgs.emplace(bullet);
                break;
                case Iscript::Opcode::SprOl:
                    result = CmdResult::NotHandled;
                    if (bullet->parent != nullptr && bullet->parent->Type().IsGoliath())
                    {
                        Unit *goliath = bullet->parent;
                        bool range_upgrade = GetUpgradeLevel(UpgradeId::CharonBooster, goliath->player) != 0;
                        if (range_upgrade || (goliath->Type().IsHero() && *bw::is_bw))
                        {
                            Sprite::Spawn(img, SpriteId::HaloRocketsTrail, cmd.point, bullet->sprite->elevation + 1);
                            result = CmdResult::Handled;
                        }
                    }
                break;
                default:
                    result = CmdResult::NotHandled;
            }
            if (result == CmdResult::NotHandled)
                result = ((Entity *)bullet)->HandleIscriptCommand(this, img, script, cmd);
            return result;
        }
};

Claimed<BulletStateResults> BulletSystem::ProgressStates()
{
    auto results = state_results_buf.Claim();
    results->clear();
    BulletStateResults *results_ptr = &results.Inner();
    for (BulletContainer::entry bullet_it : ActiveBullets_Entries())
    {
        Bullet *bullet = bullet_it->get();
        bullet->sprite->UpdateVisibilityPoint();

        BulletIscriptContext ctx(bullet, results_ptr, "BulletSystem::ProgressStates", MainRng(), true);
        ctx.ProgressIscript();
        if (ctx.CheckDeleted())
            DeleteBullet(&bullet_it);
    }

    ProgressBulletsForState(&initstate, results_ptr, BulletState::Init, &Bullet::State_Init);
    ProgressBulletsForState(&moving_to_point, results_ptr, BulletState::MoveToPoint, &Bullet::State_MoveToPoint);
    ProgressBulletsForState(&moving_to_unit, results_ptr, BulletState::MoveToTarget, &Bullet::State_MoveToUnit);
    ProgressBulletsForState(&damage_ground, results_ptr, BulletState::GroundDamage, &Bullet::State_GroundDamage);
    ProgressBulletsForState(&moving_near, results_ptr, BulletState::MoveNearUnit, &Bullet::State_MoveNearUnit);
    ProgressBulletsForState(&bouncing, results_ptr, BulletState::Bounce, &Bullet::State_Bounce);

    return results;
}

// Input should not need to be synced
void BulletSystem::ProcessAiReactToHit(vector<tuple<Unit *, Unit *, bool>> input, Ai::HitReactions *hit_reactions)
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

    // Assuming here that it does not make difference if repeating this with same target-attacker pair
    // or repeating without main_target_reactions once main_target_reactions has been true
    // ((It should be sorted so that mtr=true becomes first))
    for (const auto &tp : input.Unique([](const auto &a, const auto &b) {
        return get<0>(a) == get<0>(b) && get<1>(a) == get<1>(b);
    }))
    {
        auto target = get<0>(tp);
        auto attacker = get<1>(tp);
        auto important_hit = get<2>(tp);
        // This clear is be usually unnecessary as it is done for every bullet hit in UnitWasHit, but
        // broodling and parasite hits only trigger Ai::ReactToHit so clear flags here as well
        // Idk if clearing the 0x00c00000 is even necessary but it won't hurt ^_^
        target->hotkey_groups &= ~0x80c00000;
        hit_reactions->NewHit(target, attacker, important_hit);
    }
}

void BulletSystem::ProgressFrames(BulletFramesInput input)
{
    StaticPerfClock::ClearWithLog("Bullet::ProgressFrames");
    unsigned int prev_sleep = 0;
    if (PerfTest)
        prev_sleep = threads->GetSleepCount();
    PerfClock clock, clock2;

    auto state_results = ProgressStates();

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
    for (Bullet *bullet : state_results->do_missile_dmgs)
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

    auto canceled_ai_units = ProcessUnitWasHit(move(*bufs.unit_was_hit), &bufs);
    ProcessAiReactToHit(move(*bufs.ai_react), &input.ai_hit_reactions);
    auto puwh_time = clock.GetTime();
    clock.Start();

    input.ai_hit_reactions.ProcessEverything();
    auto ahr_time = clock.GetTime();
    clock.Start();

    // Sync with child threads. It is possible (but very unlikely) for a child be still busy,
    // if this thread did not want to wait for its result and did the search itself.
    threads->ClearAll();
    pbf_memory.ClearAll();
    threads->ForEachThread([](ScThreadVars *vars) { vars->unit_search_pool.ClearAll(); });
    unit_search->valid_region_cache = false;
    unit_search->DisableAreaCache();
    bulletframes_in_progress = false;

    for (const auto &tuple : state_results->new_bounce_targets)
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
        switch (spell.tech.Raw())
        {
            case TechId::SpawnBroodlings:
                spell.parent->SpawnBroodlings(spell.pos - Point(2, 2));
            break;
            case TechId::DarkSwarm:
                DarkSwarm(spell.player, spell.pos);
            break;
            case TechId::DisruptionWeb:
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
        unit->Kill(nullptr);
    }

    perf_log->Log("Pbf: %f ms + Ph %f ms + Puwh %f ms + Ahr %f ms + Clean %f ms = about %f ms\n", pbf_time, ph_time, puwh_time, ahr_time, clock.GetTime(), clock2.GetTime());
    perf_log->Log("Sleep count: %d\n", threads->GetSleepCount() - prev_sleep);
    perf_log->Indent(2);
    StaticPerfClock::LogCalls();
    perf_log->Indent(-2);
}

void RemoveFromBulletTargets(Unit *unit)
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

void Bullet::SetIscriptAnimation(int anim, bool force, const char *caller, BulletStateResults *results)
{
    BulletIscriptContext(this, results, caller, MainRng(), false).SetIscriptAnimation(anim, force);
}

void Bullet::WarnUnhandledIscriptCommand(const Iscript::Command &cmd, const char *func) const
{
    Warning("Unhandled iscript command %s in %s (Bullet %s)", cmd.DebugStr().c_str(), func, DebugStr().c_str());
}

std::string Bullet::DebugStr() const
{
    char buf[64];
    const char *name = (*bw::stat_txt_tbl)->GetTblString(Type().Label());
    snprintf(buf, sizeof buf / sizeof(buf[0]), "%x [%s]", weapon_id, name);
    return buf;
}
