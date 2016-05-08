#ifndef BULLET_H
#define BULLET_H

#include "types.h"
#include "list.h"
#include "sprite.h"
#include "game.h"
#include "unsorted_list.h"
#include "common/claimable.h"
#include "ai_hit_reactions.h"
#include <tuple>
#include <array>

void RemoveFromBulletTargets(Unit *unit);
// Returns true if there's need for UnitWasHit processing
bool UnitWasHit(Unit *target, Unit *attacker, bool notify);

Unit **FindNearbyHelpingUnits(Unit *unit, TempMemoryPool *allocation_pool);

// Prefer DamagedUnit::AddHit or add more functionality to DamagedUnit as AddHit requires weapon_id
// This is only for bw compatibility, or if you have really good reason to deal damage outside
// BulletSystem::ProgressFrames(). It is an
void DamageUnit(int damage, Unit *target, vector<Unit *> *killed_units);

void HallucinationHit(Unit *target, Unit *attacker, int direction);
// Passing -1 as attacking_player is allowed to indicate no attacker
void IncrementKillScores(Unit *target, int attacking_player);

int GetWeaponDamage(const Unit *target, WeaponType weapon_id, int player);

extern TempMemoryPool pbf_memory;
extern bool bulletframes_in_progress;
const int CallFriends_Radius = 0x60;

enum class BulletState
{
    Init,
    MoveToPoint,
    MoveToTarget,
    Bounce,
    Die,
    MoveNearUnit,
    GroundDamage
};

struct SpellCast
{
    SpellCast(int pl, const Point &p, int t, Unit *pa) : player(pl), tech(t), pos(p), parent(pa) {}
    uint16_t player;
    TechType tech;
    Point pos;
    Unit *parent;
};

struct BulletHit
{
    BulletHit(Unit *a, Bullet *b, int32_t d) : target(a), bullet(b), damage(d) {}
    Unit *target;
    Bullet *bullet;
    int32_t damage;
};

struct BulletFramesInput
{
    BulletFramesInput(vector<DoWeaponDamageData> w, vector<HallucinationHitData> h, Ai::HitReactions hu) :
        weapon_damages(move(w)), hallucination_hits(move(h)), ai_hit_reactions(move(hu)) {}
    BulletFramesInput(const BulletFramesInput &other) = delete;
    BulletFramesInput(BulletFramesInput &&other) = default;
    BulletFramesInput& operator=(const BulletFramesInput &other) = delete;
    BulletFramesInput& operator=(BulletFramesInput &&other) = default;
    vector<DoWeaponDamageData> weapon_damages;
    vector<HallucinationHitData> hallucination_hits;
    Ai::HitReactions ai_hit_reactions;
};

struct ProgressBulletBufs;

// This class carries some extra information which is used for handling unit damage during bullet frames.
// Only one DamagedUnit should exist per Unit damaged in this frame.
// AddHit() does what bw function DoWeaponDamage does, with some additions which allow
// simpler code in Ai::UnitWasHit. Ideally UnitWasHit should be called only once per unit,
// and DamagedUnit deciding the most dangerous of the current attackers etc. Currently it does
// not do that though. See poorly named BulletSystem::ProcessHits() (todo refactor that) for what is done currently.
// There is also function DamageUnit() which should only be used when there is no attacker
// (And preferably not even then, but otherwise burning/plague/etc damage would have to be passed to
// BulletSystem::ProgressFrames(). The dead units are still passed there, so it might not even be worth it)
class DamagedUnit
{
    public:
        DamagedUnit(Unit *b) : base(b) {}
        Unit *base;

        bool IsDead() const;
        void AddHit(uint32_t dmg, WeaponType weapon_id, int player, int direction, Unit *attacker, ProgressBulletBufs *bufs);
        int32_t GetDamage();

    private:
        void AddDamage(int dmg);
};

struct ProgressBulletBufs
{
    ProgressBulletBufs(vector<DamagedUnit> *a, vector<tuple<Unit *, Unit *>> *b,
        vector<tuple<Unit *, Unit *, bool>> *c, vector<Unit *> *d) :
        unit_was_hit(b), ai_react(c), killed_units(d), damaged_units(a) {}
    ProgressBulletBufs(ProgressBulletBufs &&other) = default;
    ProgressBulletBufs& operator=(ProgressBulletBufs &&other) = default;

    vector<tuple<Unit *, Unit *>> *unit_was_hit;
    vector<tuple<Unit *, Unit *, bool>> *ai_react;
    vector<Unit *> *killed_units;
    vector<DamagedUnit> *damaged_units;
    // Don't call help always false
    void AddToAiReact(Unit *unit, Unit *attacker, bool main_target_reactions);

    /// Returns DamagedUnit handle corresponding to the input unit.
    /// Handle is valid during the ongoing ProgressBulletFrames call,
    /// during which the same handle is always returned for unit
    /// Current implementation just stores the data in unit struct with the frame it is valid for,
    /// but "ideal" implementation would be some kind of Unit * -> DamagedUnit map.
    /// As such calling another ProgressBulletFrames during same frame will not behave as expected,
    /// even if there were completely separate BulletSystem
    DamagedUnit GetDamagedUnit(Unit *unit);
    vector<DamagedUnit> *DamagedUnits() { return damaged_units; }
};

/// Results from Bullet::State_XYZ() that need to be handled later
struct BulletStateResults
{
    // Needed as DoMissileDmg should be done with old target
    // There is previous_target as well which could have been used
    UnsortedList<tuple<Bullet *, Unit *>> new_bounce_targets;
    UnsortedList<Bullet *> do_missile_dmgs;

    void clear() {
        new_bounce_targets.clear_keep_capacity();
        do_missile_dmgs.clear_keep_capacity();
    }
};


#pragma pack(push)
#pragma pack(1)

class Bullet
{
    friend class BulletSystem; // BulletSystem is only allowed to create bullets
    public:
        RevListEntry<Bullet, 0x0> list;
        uint32_t hitpoints;
        ptr<Sprite> sprite;

        Point move_target;
        Unit *move_target_unit;

        Point next_move_waypoint;
        Point unk_move_waypoint;

        // 0x20
        uint8_t flingy_flags;
        uint8_t facing_direction;
        uint8_t flingyTurnRadius;
        uint8_t movement_direction;
        uint16_t flingy_id;
        uint8_t _unknown_0x026;
        uint8_t flingyMovementType;
        Point position;
        Point32 exact_position;
        uint32_t flingyTopSpeed;
        int32_t current_speed;
        int32_t next_speed;

        int32_t speed[2]; // 0x40
        uint16_t acceleration;
        uint8_t pathing_direction;
        uint8_t unk4b;
        uint8_t player;
        uint8_t order;
        uint8_t order_state;
        uint8_t order_signal;
        uint16_t order_fow_unit;
        uint16_t unused52;
        uint8_t order_timer;
        uint8_t ground_cooldown;
        uint8_t air_cooldown;
        uint8_t spell_cooldown;
        Point order_target_pos;
        Unit *target;

        uint8_t weapon_id;
        uint8_t time_remaining;
        uint8_t flags;
        uint8_t bounces_remaining;
        Unit *parent; // 0x64
        Unit *previous_target;
        uint8_t spread_seed;
        uint8_t padding[0x3];


        ListEntry<Bullet, 0x70> targeting; // 0x70
        ListEntry<Bullet, 0x78> spawned; // 0x78

        void SingleDelete();

        WeaponType Type() const;

        void SetTarget(Unit *new_target);

        void ProgressFrame();
        // Retuns the new state, or old if no switch
        BulletState State_Init(BulletStateResults *results);
        BulletState State_GroundDamage(BulletStateResults *results);
        BulletState State_Bounce(BulletStateResults *results);
        BulletState State_MoveToPoint(BulletStateResults *results);
        BulletState State_MoveToUnit(BulletStateResults *results);
        BulletState State_MoveNearUnit(BulletStateResults *results);
        Optional<SpellCast> DoMissileDmg(ProgressBulletBufs *bufs);

        void UpdateMoveTarget(const Point &target);
        void Move(const Point &where);
        /// Returns true if the random hit chance roll in State_Init failed
        bool DoesMiss() const { return flags & 0x1; }

        void Serialize(Save *save, const BulletSystem *parent);
        template <bool saving, class T> void SaveConvert(SaveBase<T> *save, const BulletSystem *parent);
        ~Bullet() {}
        Bullet(Bullet &&other) = default;

        void WarnUnhandledIscriptCommand(const Iscript::Command &cmd, const char *func) const;
        std::string DebugStr() const;

    private:
        Bullet() {}
        void NormalHit(ProgressBulletBufs *bufs);
        void HitUnit(Unit *target, int dmg, ProgressBulletBufs *bufs);
        void AcidSporeHit() const;
        template <bool air_splash> void Splash(ProgressBulletBufs *bufs, bool hit_own_units);
        void Splash_Lurker(ProgressBulletBufs *bufs);
        void SpawnBroodlingHit(vector<Unit *> *killed_units) const;

        Unit *ChooseBounceTarget();

        // Delete bullet if returns false
        bool Initialize(Unit *parent_, int player_, int direction, WeaponType weapon, const Point &pos);

        static vector<std::pair<Unit *, Point>> broodling_spawns;

        // Results must not be null
        void SetIscriptAnimation(int anim, bool force, const char *caller, BulletStateResults *results);
};

/// Contains and controls bullets of the game
/// In theory, one could have multiple completely independed BulletSystems, but obviously it would
/// require rewriting even more bw code - also ProgressBulletBufs::GetDamagedUnit has to be changed
/// if that is ever desired
class BulletSystem
{
    typedef UnsortedList<ptr<Bullet>, 128> BulletContainer;
    public:
        BulletSystem() {}
        void Deserialize(Load *load);
        void Serialize(Save *save);
        void FinishLoad(Load *load);
        template <class Cb>
        void MakeSaveIdMapping(Cb callback) const;

        /// May only be called once per frame, see ProgressBulletBufs::GetDamagedUnit
        void ProgressFrames(BulletFramesInput in);
        void DeleteAll();
        Bullet *AllocateBullet(Unit *parent, int player, int direction, WeaponType weapon, const Point &pos);
        uintptr_t BulletCount() const
        {
            uintptr_t count = 0;
            for (auto vec : Containers())
                count += vec->size();
            return count;
        }

    private:
        Claimed<BulletStateResults> ProgressStates();
        void ProcessHits(ProgressBulletBufs *bufs);
        vector<Unit *> ProcessUnitWasHit(vector<tuple<Unit *, Unit *>> hits, ProgressBulletBufs *bufs);
        void ProcessAiReactToHit(vector<tuple<Unit *, Unit *, bool>> input, Ai::HitReactions *hit_reactions);

        void DeleteBullet(BulletContainer::entry *bullet);
        std::array<BulletContainer *, 7> Containers()
        {
            return { { &initstate, &moving_to_point, &moving_to_unit, &bouncing, &damage_ground, &moving_near, &dying } };
        }
        std::array<const BulletContainer *, 7> Containers() const
        {
            return { { &initstate, &moving_to_point, &moving_to_unit, &bouncing, &damage_ground, &moving_near, &dying } };
        }

        void SwitchBulletState(BulletContainer::entry *bullet, BulletState new_state);
        BulletContainer *GetStateContainer(BulletState state);

        void ProgressBulletsForState(BulletContainer *container, BulletStateResults *results,
            BulletState state, BulletState (Bullet::*state_function)(BulletStateResults *));

        BulletContainer initstate;
        BulletContainer moving_to_point;
        BulletContainer moving_to_unit;
        BulletContainer bouncing;
        BulletContainer damage_ground;
        BulletContainer moving_near;
        BulletContainer dying;
        Claimable<vector<DamagedUnit>> dmg_unit_buf;
        Claimable<vector<SpellCast>> spell_buf;
        // target, attacker
        Claimable<vector<tuple<Unit *, Unit *>>> unit_was_hit_buf;
        // target, attacker, main_target_reactions
        Claimable<vector<tuple<Unit *, Unit *, bool>>> ai_react_buf;
        Claimable<vector<Unit *>> killed_units_buf;

        /// Filled by and returned from ProgressStates(),
        Claimable<BulletStateResults> state_results_buf;

    public:
        class ActiveBullets_ : public Common::Iterator<ActiveBullets_, Bullet *> {
            public:
                ActiveBullets_(BulletSystem *p) : parent(p), container_index(0),
                    pos(parent->Containers().front()->begin()), first(true) {
                    CheckEndOfVec();
                }
                Optional<Bullet *> next() {
                    if (!first) {
                        if (pos != parent->Containers()[container_index]->end())
                            ++pos;
                        CheckEndOfVec();
                    }
                    if (pos == parent->Containers().back()->end())
                        return Optional<Bullet *>();
                    first = false;
                    return pos->get();
                }
            private:
                void CheckEndOfVec() {
                    while (pos == parent->Containers()[container_index]->end() &&
                            container_index < parent->Containers().size() - 1) {
                        container_index += 1;
                        pos = parent->Containers()[container_index]->begin();
                    }
                }

                BulletSystem *parent;
                unsigned container_index;
                BulletContainer::iterator pos;
                bool first;
        };
        ActiveBullets_ ActiveBullets() { return ActiveBullets_(this); }
        friend class ActiveBullets_;

    private:
        class ActiveBullets_Entries_ : public Common::Iterator<ActiveBullets_Entries_, BulletContainer::entry> {
            public:
                ActiveBullets_Entries_(BulletSystem *p) : parent(p), container_index(0),
                    pos(parent->Containers().front()->entries_begin()), first(true) {
                    CheckEndOfVec();
                }
                Optional<BulletContainer::entry> next() {
                    if (!first) {
                        if (pos != parent->Containers()[container_index]->entries_end())
                            ++pos;
                        CheckEndOfVec();
                    }
                    if (pos == parent->Containers().back()->entries_end())
                        return Optional<BulletContainer::entry>();
                    first = false;
                    return *pos;
                }
            private:
                void CheckEndOfVec() {
                    while (pos == parent->Containers()[container_index]->entries_end() &&
                            container_index < parent->Containers().size() - 1) {
                        container_index += 1;
                        pos = parent->Containers()[container_index]->entries_begin();
                    }
                }

                BulletSystem *parent;
                unsigned container_index;
                BulletContainer::entry_iterator pos;
                bool first;
        };
        ActiveBullets_Entries_ ActiveBullets_Entries() { return ActiveBullets_Entries_(this); }
        friend class ActiveBullets_Entries_;
};

extern BulletSystem *bullet_system;

static_assert(sizeof(Bullet) == 0x80, "Sizeof bullet");

#pragma pack(pop)

#endif // BULLET_H

