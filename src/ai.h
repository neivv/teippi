#ifndef AI_H
#define AI_H

#include "types.h"
#include "list.h"
#include "pathing.h"
#include "limits.h"
#include "player.h"

namespace Ai
{
    struct Region;
    extern ListHead<GuardAi, 0x0> needed_guards[0x8];

    // Used to gather data (picked_target) for UpdateAttackTarget from UnitWasHit, hence the name
    class HitUnit
    {
        public:
            HitUnit(Unit *u) : unit(u), picked_target(nullptr) {}
            HitUnit(HitUnit &&other) = default;
            HitUnit& operator=(HitUnit &&other) = default;

            Unit *GetUnit() { return unit; }
            bool Empty() const { return picked_target == nullptr; }

            Unit *unit;
            Unit *picked_target;
    };

    vector<HitUnit> ProcessAskForHelp(HelpingUnitVec input);
    void ClearRegionChangeList();
    Region *GetRegion(int player, int region);
    // Returns iterator for all of the regions in game.
    // Order is not guaranteed to be consistent
    class IterateRegions
    {
        public:
            class Iterator
            {
                public:
                    Iterator(int pl, int reg) : player(pl), region(reg) {
                        while (player < Limits::ActivePlayers && !IsComputerPlayer(player))
                            player++;
                    }
                    bool operator==(const Iterator &other) const { return player == other.player && region == other.region; }
                    bool operator!=(const Iterator &other) const { return !(*this == other); }
                    Iterator &operator++()
                    {
                        region++;
                        if (region == GetPathingSystem()->region_count)
                        {
                            player++;
                            while (player < Limits::ActivePlayers && !IsComputerPlayer(player))
                                player++;
                            region = 0;
                        }
                        return *this;
                    }
                    Ai::Region *operator*() { return GetRegion(player, region); }
                private:
                    int player;
                    int region;
            };

            IterateRegions() {}
            Iterator begin() { return Iterator(0, 0); }
            Iterator end() { return Iterator(Limits::ActivePlayers, 0); }
    };
    inline IterateRegions GetRegions() { return IterateRegions(); }

#pragma pack(push)
#pragma pack(1)

    struct SpendingRequest
    {
        uint8_t priority;
        uint8_t type;
        uint16_t unit_id;
        void *val;
    };

    struct PlayerData
    {
        uint32_t mineral_need;
        uint32_t gas_need;
        uint32_t supply_need;
        uint32_t minerals_available;
        uint32_t gas_available;
        uint32_t supply_available;
        SpendingRequest requests[0x3f];
        uint8_t request_count;
        uint8_t liftoff_cooldown;
        uint8_t unk212[0x6];
        uint16_t flags; // 0x218
        uint8_t dc21a[0x4];
        uint16_t unk_region;
        uint16_t wanted_unit;
        uint8_t dc222[0x2];
        uint32_t unk_count; // 0x224
        uint8_t unk228[0x4];
        uint8_t strategic_suicide; // 0x22c
        uint8_t unk22d[0x3];
        uint16_t unit_ids[0x40]; // 0x230
        uint8_t unk2b0[0x140];
        uint8_t unit_build_limits[0xe4]; // 0x3f0
        Unit *free_medic; // 0x4d4
        uint8_t unk4d8[0x10];
    };

    template <class C>
    class DataList
    {
        public:
            DataList<C> &operator=(C *c) { first = c; return *this;}
            operator ListHead<C, 0x0>&() { return first; }
            operator C*&() { return first; }
            C **operator&() { return &first; }

            ListIterator<C, 0x0, false> begin() const { return first.begin(); }
            ListIterator<C, 0x0, false> end() const { return first.end(); }

        private:
            void *unused;
            ListHead<C, 0x0> first;
    };

    struct ResourceArea
    {
        Point position;
        uint8_t mineral_field_count;
        uint8_t geyser_count;
        uint8_t dc6;
        uint8_t flags;
        uint32_t total_minerals;
        uint32_t total_gas;
        uint8_t dc10[0x20];
    };
    struct ResourceAreaArray
    {
        ResourceArea areas[0xfa];
        uint32_t used_count;
        uint32_t frames_till_update;
    };

    struct Region
    {
        uint16_t region_id;
        uint16_t target_region_id;
        uint8_t player; // 0x4
        uint8_t state;
        uint16_t unk6;
        uint16_t flags; // 0x8
        uint16_t ground_unit_count;
        uint16_t needed_ground_strength;
        uint16_t needed_air_strength;
        uint16_t local_ground_strength;
        uint16_t local_air_strength;
        uint16_t all_ground_strength;
        uint16_t all_air_strength;
        uint16_t enemy_air_strength;
        uint16_t enemy_ground_strength;
        Unit *air_target;
        Unit *ground_target;
        Unit *slowest_military;
        Unit *first_important; // dunno, may have detectors
        DataList<MilitaryAi> military;
    };

    class Script
    {
        public:
            ListEntry<Script, 0x0> list;
            uint32_t pos;
            uint32_t wait;
            uint32_t player; // 0x10
            Rect32 area;
            uint32_t center[0x2]; // 0x24
            Town *town;
            uint32_t flags; // 0x30

            Script(uint32_t player, uint32_t pos, bool bwscript, Rect32 *area);
            static Script *RawAlloc() { return new Ai::Script; }
            ~Script();

        private:
            Script() {}
    };
    class Town
    {
        public:
            ListEntry<Town, 0x0> list;
            uint32_t unused8; // Free worker ais
            ListHead<WorkerAi, 0x0> first_worker;
            uint32_t unused10; // 0x10, Free building ais
            ListHead<BuildingAi, 0x0> first_building;
            uint8_t player;
            uint8_t inited; // emt
            uint8_t worker_count;
            uint8_t unk1b;
            uint8_t resource_area;
            uint8_t unk1d;
            uint8_t building_was_hit;
            uint8_t unk1f;
            Point position;
            Unit *main_building;
            Unit *building_scv;
            Unit *mineral;
            Unit *gas_buildings[0x3];
            uint32_t build_requests[0x64];
    };
    class UnitAi
    {
        public:
            ListEntry<UnitAi, 0x0> list;
            uint8_t type;

            void Delete();
    };
    class GuardAi
    {
        public:
            ListEntry<GuardAi, 0x0> list;
            uint8_t type;
            uint8_t unk_count;
            uint8_t unka[0x2];
            Unit *parent;
            uint16_t unit_id; // 0x10
            Point home;
            Point unk_pos;
            uint8_t unk1a[0x2];
            uint32_t previous_update;
    };
    class WorkerAi
    {
        public:
            ListEntry<WorkerAi, 0x0> list;
            uint8_t type;
            uint8_t unk9;
            uint8_t unka;
            uint8_t wait_timer;
            uint32_t unk_count;
            Unit *parent; // 0x10
            Town *town;

            WorkerAi();
            static WorkerAi *RawAlloc() { return new WorkerAi(true); }

        private:
            WorkerAi(bool) {}
    };
    class BuildingAi
    {
        public:
            ListEntry<BuildingAi, 0x0> list;
            uint8_t type;
            uint8_t train_queue_types[0x5];
            uint8_t unke[0x2];
            Unit *parent; // 0x10
            Town *town;
            void *train_queue_values[0x5];

            BuildingAi();
            static BuildingAi *RawAlloc() { return new BuildingAi(true); }

        private:
            BuildingAi(bool) {}
    };
    class MilitaryAi
    {
        public:
            ListEntry<MilitaryAi, 0x0> list;
            uint8_t type;
            uint8_t padding[0x3];
            Unit *parent;
            Region *region;

            MilitaryAi();
    };

    static_assert(sizeof(PlayerData) == 0x4e8, "sizeof(PlayerData)");
    static_assert(sizeof(Region) == 0x34, "sizeof(Ai::Region)");
    static_assert(sizeof(Script) == 0x34, "sizeof(Script)");
    static_assert(sizeof(Town) == 0x1cc, "sizeof(Town)");
    static_assert(sizeof(GuardAi) == 0x20, "sizeof(GuardAi)");
    static_assert(sizeof(WorkerAi) == 0x18, "sizeof(WorkerAi)");
    static_assert(sizeof(BuildingAi) == 0x2c, "sizeof(BuildingAi)");
    static_assert(sizeof(MilitaryAi) == 0x14, "sizeof(MilitaryAi)");
#pragma pack(pop)
    void RemoveLimits(Common::PatchContext *patch);
    void DeleteAll();
    void DeleteTown(Town *town);

    void AddUnitAi(Unit *unit, Town *town);

    bool ShouldCancelDamaged(const Unit *unit);
    // main_target_reactions maybe more like serious_hit or did_damage
    void ReactToHit(HitUnit *own_base, Unit *attacker, bool main_target_reactions, HelpingUnitVec *helpers);
    void UpdateRegionEnemyStrengths();
    void __fastcall RemoveUnitAi(Unit *unit, bool unk);
    void ProgressScripts();

    bool UpdateAttackTarget(Unit *unit, bool accept_if_sieged, bool accept_critters, bool must_reach);
    // Originally this was part of ReactToHit, but now it is called from Bullet::HitUnit,
    // which is somewhat earlier. This is so ReactToHit can be modified to be called only once per unit,
    // no matter how many enemies happen to hit this single unit in same frame.
    // Not sure if it actually is escaping static defence when it checks for React flag.
    // own must be owned by an ai player.
    void EscapeStaticDefence(Unit *own, Unit *attacker);
    // own starts fleeing if necessary, as it was damaged by attacker.
    // Returns true if starteed fleeing
    // own must be owned by an ai player.
    bool TryDamagedFlee(Unit *own, Unit *attacker);
    // Returns true if something was cast (Maybe not always???)
    bool TryReactionSpell(Unit *own, bool was_damaged);
    // Cloaks an unit if possible, or burrows it if it is lurker
    void Hide(Unit *own);

} // namespace ai

#endif // AI_H
