#ifndef AI_HIT_REACTIONS_H
#define AI_HIT_REACTIONS_H

#include "types.h"
#include <deque>

struct ClearOnMoveBool
{
    constexpr ClearOnMoveBool() : val(true) { }
    ClearOnMoveBool(ClearOnMoveBool &&other) : val(other.val) { other.val = false; }
    ClearOnMoveBool &operator=(ClearOnMoveBool &&other)
    {
        if (*this != other)
        {
            val = other.val;
            other.val = false;
        }
        return *this;
    }
    ClearOnMoveBool &operator=(bool v) { val = v; return *this; }
    operator bool() const { return val; }

    bool val;
};

namespace Ai
{
    /// Maintains some state caused by units getting hit, and only fully updates
    /// some ai region etc things when ProcessEverything is called (Or the state
    /// is destroyed)
    /// There is a comment in Test_AiTargetPriority which explains slight differences
    /// between this and bw's implementation.
    /// NOTE: Some of the unit-specific data is stored inside unit struct, so only a
    /// single HitReactions may be active at a time.
    class HitReactions
    {
        public:
            HitReactions() { Reset(); }
            /// If there is data not fully processed, processes it.
            ~HitReactions()
            {
                if (is_valid)
                    ProcessEverything();
            }
            HitReactions(HitReactions &&other) = default;
            HitReactions& operator=(HitReactions &&other) = default;

            /// Resets the internal buffers for reusing. Only necessary if
            /// the object is kept alive for reusing after ProcessEverything()
            void Reset();

            /// Adds an another hit, doing some of the processing right now
            /// and queues some until ProcessEverything() is called.
            /// important_hit is generally true, but having it false reduces
            /// the ai reaction against the attacker.
            /// The unit must have its helper search at least started or else
            /// this function will hang.
            void NewHit(Unit *hit_unit, Unit *attacker, bool important_hit);

            /// Processes and updates everything that was buffered for later.
            /// The class becomes unusable unless Reset() is called afterwards.
            void ProcessEverything();

        private:
            /// Does both UnitWasHit and React. The distinction between those is somewhat arbitary.
            void AddReaction(Unit *own, Unit *attacker, bool important_hit, bool call_help);
            /// Updates/buffers for updating the ai-global state related to hitting units,
            /// and possibly orders nearby units to help
            void UnitWasHit(Unit *own, Unit *attacker, bool important_hit, bool call_help);
            /// Does unit-specific reactions for the hit. Note: There are some cases when it modifies
            /// global ai state as well, at least with Ai_Detect.
            void React(Unit *own, Unit *attacker, bool important_hit);

            /// Adds attacker to the list of possible new targets
            /// (Well, checks if it is better new target than the previously best)
            void UpdatePickedTarget(Unit *own, Unit *attacker);

            /// Buffers helping unit-attacker pairs to the helpers vector.
            /// attacking_military causes only the military units which are part of
            /// attack to help. (Any attack, though I don't think ai can have more than
            /// one attack at once)
            void AskForHelp(Unit *own, Unit *enemy, bool attacking_military);
            void AskForHelp_CheckUnits(Unit *own, Unit *enemy, bool attacking_military, Unit **units);
            /// Checks if the unit should help against enemy.
            bool AskForHelp_IsGood(Unit *unit, Unit *enemy, bool attacking_military);
            /// Optimization
            bool AskForHelp_CheckIfDoesAnything(Unit *own);
            /// Does some kind of check for ai region state for AskForHelp.
            /// Prevents same own_region-enemy_region pair being processed twice
            /// (Which is not bad, but unnecessary).
            /// TODO: Is it really worth it to avoid a few ai region operations?
            bool AskForHelp_CheckRegion(int player, int own_region, int region, Region *enemy_region,
                                        UnitList<Region *> *** region_list);
            /// Adds own_region-enemy_region pair to the region_list for later processing
            void AskForHelp_AddRegion(int player, UnitList<Region *> ** region_list, int region,
                                      Region *attacker_region);

            /// --- ProcessEverything subfunctions ---
            /// Some kind of flowchart: Functions higher up are done first,
            /// and lines indicate how they give input to functions below
            ///         NewHit()---------------
            ///        /              |       |
            /// ProcessAskForHelp     |       |
            ///         \             |       |
            ///  ProcessUpdateAttackTarget    |
            ///                               /
            ///              UpdateRegionEnemyStrengths

            void ProcessAskForHelp();
            void ProcessUpdateAttackTarget();
            /// Updates the regions specified in region_enemy_strength_updates.
            void UpdateRegionEnemyStrengths();

            typedef vector<tuple<Unit *, Unit *>> HelpingUnitVec;
            HelpingUnitVec helpers;
            std::deque<Unit *> update_attack_targets;
            vector<UnitList<Region *> *> ask_for_help_regions;
            vector<uint8_t> region_enemy_strength_updates;
            ClearOnMoveBool is_valid;
    };

    /// For testing
    bool TestBestTargetPicking();
    Unit *GetBestTarget(Unit *unit, const vector<Unit *> &units);
}
#endif /* AI_HIT_REACTIONS_H */
