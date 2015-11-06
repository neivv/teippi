#ifdef DEBUG
#include "test_game.h"

#include "unit.h"
#include "offsets.h"
#include "limits.h"
#include "player.h"
#include "common/assert.h"
#include "tech.h"
#include "order.h"
#include "text.h"
#include "bullet.h"
#include "unitsearch.h"
#include "targeting.h"
#include "dialog.h"
#include "yms.h"
#include "ai.h"
#include "ai_hit_reactions.h"
#include "triggers.h"

#include "possearch.hpp"

#include <algorithm>
#include "console/windows_wrap.h"

using std::min;
using std::get;

#define TestAssert(s) if (!(s)) { if(IsDebuggerPresent()) { INT3(); } Fail(#s); return; }

// This file has special permission for different brace style =)

static void ClearTriggers() {
    for (int i = 0; i < Limits::ActivePlayers; i++)
        FreeTriggerList(&bw::triggers[i]);
}

Unit *GameTest::CreateUnitForTest(int unit_id, int player) {
    return CreateUnitForTestAt(unit_id, player, Point(100, 100));
}

Unit *GameTest::CreateUnitForTestAt(int unit_id, int player, const Point &point) {
    Unit *unit = CreateUnit(unit_id, point.x, point.y, player);
    Assert(unit != nullptr);
    FinishUnit_Pre(unit);
    FinishUnit(unit);
    GiveAi(unit);
    unit->energy = unit->GetMaxEnergy();
    return unit;
}

static void ClearUnits() {
    for (Unit *unit : *bw::first_active_unit) {
        Unit *loaded = unit->first_loaded;
        while (loaded) {
            Unit *next = loaded->next_loaded;
            loaded->order_flags |= 0x4;
            loaded->Kill(nullptr);
            loaded = next;
        }
        unit->order_flags |= 0x4;
        unit->Kill(nullptr);
    }
    for (Unit *unit : *bw::first_revealer) {
        unit->order_flags |= 0x4;
        unit->Kill(nullptr);
    }
}

static Unit *FindUnit(int unit_id) {
    for (Unit *unit : *bw::first_active_unit) {
        if (unit->unit_id == unit_id)
            return unit;
    }
    return nullptr;
}

static int UnitCount() {
    int count = 0;
    // Clang gives unused var warning for pretty for loop ;_;
    for (Unit *unit = *bw::first_active_unit; unit != nullptr; unit = unit->list.next) {
        count++;
    }
    return count;
}

static void AiPlayer(int player) {
    bw::players[player].type = 1;
}

static void NoAi() {
    for (int i = 0; i < Limits::ActivePlayers; i++)
        bw::players[i].type = 2;
}

static void Visions() {
    for (int i = 0; i < Limits::Players; i++) {
        bw::visions[i] = 0xff;
    }
}

static void ResetVisions() {
    for (int i = 0; i < Limits::Players; i++) {
        bw::visions[i] = 1 << i;
    }
}

static void SetEnemy(int player, int enemy) {
    bw::alliances[player][enemy] = 0;
}

static void AllyPlayers() {
    for (int i = 0; i < Limits::Players; i++) {
        for (int j = 0; j < Limits::Players; j++) {
            bw::alliances[i][j] = 1;
        }
    }
}

static bool NoUnits() {
    return *bw::first_active_unit == nullptr && *bw::first_revealer == nullptr;
}

static void GiveAllTechs() {
    for (int i = 0; i < Tech::None; i++) {
        for (int player = 0; player < Limits::Players; player++)
            SetTechLevel(i, player, 1);
    }
}

static void GiveTech(int tech, int player) {
    SetTechLevel(tech, player, 1);
}

static void ClearTechs() {
    for (int i = 0; i < Tech::None; i++) {
        for (int player = 0; player < Limits::Players; player++)
            SetTechLevel(i, player, 0);
    }
}

struct Test_Dummy : public GameTest {
    void Init() override {
        Pass();
    }
    void NextFrame() override {
    }
};

struct Test_Hallucination : public GameTest {
    Unit *ht;
    Unit *real;
    Unit *outsider;
    Unit *hallu;
    int hallu_dmg;
    int real_dmg;

    void Init() override {
        hallu_dmg = -1;
        real_dmg = -1;
        hallu = nullptr;
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                state++;
            break; case 1:
                ht = CreateUnitForTest(Unit::HighTemplar, 0);
                real = CreateUnitForTest(Unit::Marine, 1);
                outsider = CreateUnitForTest(Unit::Marine, 2);
                IssueOrderTargetingUnit_Simple(ht, Order::Hallucination, real);
                state++;
            break; case 2: {
                int hallu_count = 0;
                for (Unit *unit : *bw::first_active_unit) {
                    if (unit->flags & UnitStatus::Hallucination) {
                        unit->death_timer -= 50;
                        hallu_count++;
                        hallu = unit;
                    }
                }
                if (hallu != nullptr) {
                    TestAssert(hallu_count == Spell::HallucinationCount);
                    TestAssert(hallu->player == ht->player);
                    TestAssert(hallu->ai == nullptr);
                    IssueOrderTargetingUnit_Simple(hallu, Order::AttackUnit, outsider);
                    IssueOrderTargetingUnit_Simple(real, Order::AttackUnit, hallu);
                    IssueOrderTargetingUnit_Simple(outsider, Order::AttackUnit, real);
                    state++;
                }
            } break; case 3: {
                if (real_dmg == -1 && real->GetHitPoints() != real->GetMaxHitPoints())
                    real_dmg = real->GetMaxHitPoints() - real->GetHitPoints();
                if (hallu_dmg == -1 && hallu->GetHitPoints() != hallu->GetMaxHitPoints())
                    hallu_dmg = hallu->GetMaxHitPoints() - hallu->GetHitPoints();
                if (real_dmg != -1 && hallu_dmg != -1)
                {
                    TestAssert(real_dmg * 2 == hallu_dmg);
                    Print("Testing for death -- takes a while");
                    state++;
                }
            } break; case 4: { // Test that they die
                for (Unit *unit : *bw::first_active_unit) {
                    if (unit->flags & UnitStatus::Hallucination)
                        return;
                }
                TestAssert(outsider->GetHitPoints() == outsider->GetMaxHitPoints());
                Pass();
            }
        }
    }
};

struct Test_Plague : public GameTest {
    Unit *defi;
    Unit *target;
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                state++;
            break; case 1:
                defi = CreateUnitForTest(Unit::Defiler, 0);
                target = CreateUnitForTest(Unit::Marine, 0);
                IssueOrderTargetingUnit_Simple(defi, Order::Plague, target);
                state++;
            break; case 2: {
                int dmg = target->GetMaxHitPoints() - target->GetHitPoints();
                if (dmg != 0) {
                    TestAssert(target->plague_timer != 0);
                    TestAssert(defi->plague_timer == 0);
                    TestAssert(dmg == weapons_dat_damage[Weapon::Plague] / (Spell::PlagueTime + 1));
                    SetHp(target, 5 * 256); // Test that it doesn't kill
                    state++;
                }
            } break; case 3: {
                TestAssert(!target->IsDying());
                if (target->plague_timer == 0)
                    Pass();
            }
        }
    }
};

struct Test_Storm : public GameTest {
    Unit *ht1;
    Unit *ht2;
    Unit *target;
    Rect16 storm_area;
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                state++;
            break; case 1: {
                ht1 = CreateUnitForTestAt(Unit::HighTemplar, 0, Point(100, 100));
                auto crect = ht1->GetCollisionRect();
                ht2 = CreateUnitForTestAt(Unit::HighTemplar, 0, Point(100 + crect.Width(), 100));
                target = CreateUnitForTestAt(Unit::Battlecruiser, 0, Point(300, 100));
                IssueOrderTargetingUnit_Simple(ht1, Order::PsiStorm, target);
                state++;
            // Cases 2 and 3 should behave same, no matter if 2 storms are casted or 1
            } break; case 2: case 3: {
                int dmg = target->GetMaxHitPoints() - target->GetHitPoints();
                int storm_dmg = weapons_dat_damage[Weapon::PsiStorm];
                TestAssert(dmg % storm_dmg == 0);
                if (dmg != 0) {
                    TestAssert(dmg < storm_dmg * 10);
                    bool storms_active = bullet_system->BulletCount() != 0;
                    // The 2 hts don't cast in perfect sync, which might give an extra cycle of damage
                    bool dmg_done = (dmg == storm_dmg * 8) || (state == 3 && dmg == storm_dmg * 9);
                    if (!dmg_done) {
                        TestAssert(storms_active);
                    } else if (state == 2 && !storms_active) {
                        target = CreateUnitForTestAt(Unit::Battlecruiser, 0, Point(100, 300));
                        IssueOrderTargetingUnit_Simple(ht1, Order::PsiStorm, target);
                        IssueOrderTargetingUnit_Simple(ht2, Order::PsiStorm, target);
                        state++;
                    } else if (state == 3 && !storms_active) {
                        ht1->energy = 200 * 256;
                        IssueOrderTargetingUnit_Simple(ht1, Order::Hallucination, target);
                        state++;
                    }
                }
            } break; case 4:
                for (Unit *unit : *bw::first_active_unit) {
                    if (unit->flags & UnitStatus::Hallucination) {
                        target = unit;
                        auto &pos = ht1->sprite->position;
                        IssueOrderTargetingGround(target, Order::Move, pos.x, pos.y);
                        state++;
                        break;
                    }
                }
            break; case 5:
                if (target->order != Order::Move) {
                    TestAssert(target->GetCollisionRect().top < ht2->sprite->position.y);
                    IssueOrderTargetingUnit_Simple(ht2, Order::PsiStorm, target);
                    storm_area = Rect16(target->sprite->position, weapons_dat_outer_splash[Weapon::PsiStorm]);
                    state++;
                }
            break; case 6: { // The units are so close that both hts die and the hallu as well
                if (ht1->IsDying()) {
                    TestAssert(ht2->IsDying());
                    bool area_clear = true;
                    unit_search->ForEachUnitInArea(storm_area, [&](Unit *u) {
                        area_clear = false;
                        return true;
                    });
                    TestAssert(area_clear);
                    Pass();
                }
            }
        }
    }
};

struct Test_ShieldOverlay : public GameTest {
    Unit *attacker;
    Unit *target;
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                state++;
            break; case 1:
                attacker = CreateUnitForTest(Unit::Marine, 0);
                target = CreateUnitForTest(Unit::Zealot, 0);
                IssueOrderTargetingUnit_Simple(attacker, Order::AttackUnit, target);
                state++;
            break; case 2: {
                // Fail if taken hp dmg and still hasn't found overlay
                TestAssert(target->GetHitPoints() == target->GetMaxHitPoints());
                for (Image *img : target->sprite->first_overlay) {
                    if (img->image_id == Image::ShieldOverlay)
                        state++;
                }
            } break; case 3: {
                TestAssert(attacker->target != nullptr);
                target->shields = 0;
                for (Image *img : target->sprite->first_overlay) {
                    if (img->image_id == Image::ShieldOverlay)
                        return;
                }
                state++;
            } break; case 4: {
                if (attacker->target == nullptr)
                    Pass();
                else {
                    for (Image *img : target->sprite->first_overlay) {
                        TestAssert(img->image_id != Image::ShieldOverlay);
                    }
                }
            }
        }
    }
};

struct Test_ShieldOverlayHallu : public Test_ShieldOverlay {
    void Init() override {
        Test_ShieldOverlay::Init();
    }
    void NextFrame() override {
        switch (state) {
            default:
                Test_ShieldOverlay::NextFrame();
            break; case 1: {
                attacker = CreateUnitForTest(Unit::Marine, 0);
                target = CreateUnitForTest(Unit::Zealot, 0);
                Unit *ht = CreateUnitForTest(Unit::HighTemplar, 0);
                IssueOrderTargetingUnit_Simple(ht, Order::Hallucination, target);
                state = 100;
            } break; case 100: {
                for (Unit *unit : *bw::first_active_unit) {
                    if (unit->flags & UnitStatus::Hallucination) {
                        target = unit;
                        IssueOrderTargetingUnit_Simple(attacker, Order::AttackUnit, target);
                        state = 2;
                    }
                }
            }
        }
    }
};

struct Test_ShieldDamage : public GameTest {
    Unit *attacker;
    Unit *target;
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                state++;
            break; case 1: {
                attacker = CreateUnitForTest(Unit::Marine, 0);
                target = CreateUnitForTest(Unit::Zealot, 0);
                IssueOrderTargetingUnit_Simple(attacker, Order::AttackUnit, target);
                state++;
            } break; case 2: {
                // Test that unit will not take hp damage as long as it has shields
                if (target->shields < 256) {
                    Pass();
                } else {
                    TestAssert(target->GetHitPoints() == target->GetMaxHitPoints());
                }
            }
        }
    }
};

struct Test_LurkerAi : public GameTest {
    Unit *flyer;
    Unit *lurker;
    Point lurker_pos;
    void Init() override {
        Visions();
        AiPlayer(1);
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                state++;
            break; case 1: {
                flyer = CreateUnitForTest(Unit::Scout, 0);
                lurker = CreateUnitForTest(Unit::Lurker, 1);
                TestAssert(lurker->ai != nullptr);
                IssueOrderTargetingNothing(lurker, Order::Burrow);
                IssueOrderTargetingUnit_Simple(flyer, Order::AttackUnit, lurker);
                state++;
            } break; case 2: case 3: {
                TestAssert(flyer->target != nullptr);
                TestAssert(lurker->sprite->main_image->drawfunc == 0);
                if (state == 2 && lurker->flags & UnitStatus::Burrowed)
                    state++;
                if (state == 3 && lurker->flags & ~UnitStatus::Burrowed) {
                    lurker_pos = lurker->sprite->position;
                    state++;
                }
            } break; case 4: {
                TestAssert(flyer->target != nullptr);
                TestAssert(lurker->sprite->main_image->drawfunc == 0);
                // Pass if lurker flees
                if (lurker_pos != lurker->sprite->position)
                    Pass();
            }
        }
    }
};

struct Test_BurrowerAi : public GameTest {
    Unit *flyer;
    Unit *hydra;
    void Init() override {
        Visions();
        SetEnemy(1, 0);
        AiPlayer(1);
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                state++;
            break; case 1: {
                flyer = CreateUnitForTestAt(Unit::Valkyrie, 0, Point(100, 500));
                hydra = CreateUnitForTest(Unit::Hydralisk, 1);
                state++;
            } break; case 2: {
                if (hydra->flags & UnitStatus::Burrowed) {
                    auto &pos = hydra->sprite->position;
                    IssueOrderTargetingGround(flyer, Order::Move, pos.x, pos.y);
                    state++;
                }
            } break; case 3: {
                // Pass if the ai hydra is able to unburrow and attack
                if (flyer->GetHitPoints() != flyer->GetMaxHitPoints() && hydra->target == flyer)
                    Pass();
            }
        }
    }
};

struct Test_Vision : public GameTest {
    Unit *unit;
    int begin_frame;
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                state++;
            break; case 1: {
                unit = CreateUnitForTest(Unit::Overlord, *bw::local_player_id);
                begin_frame = *bw::frame_count;
                state++;
            } break; case 2: {
                TestAssert(unit->sprite->visibility_mask != 0);
                if (*bw::frame_count - begin_frame > 400)
                    Pass();
            }
        }
    }
};

struct Test_Carrier : public GameTest {
    Unit *carrier;
    Unit *target;
    Unit *interceptor;
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                state++;
            break; case 1: {
                carrier = CreateUnitForTest(Unit::Carrier, 0);
                target = CreateUnitForTest(Unit::Overlord, 0);
                carrier->IssueSecondaryOrder(Order::TrainFighter);
                carrier->build_queue[carrier->current_build_slot] = Unit::Interceptor;
                state++;
            } break; case 2: {
                if (carrier->carrier.in_child != nullptr) {
                    interceptor = carrier->carrier.in_child;
                    IssueOrderTargetingUnit_Simple(carrier, Order::CarrierAttack, target);
                    state++;
                }
            } break; case 3: {
                // Test interceptor healing
                if (target->GetHitPoints() != target->GetMaxHitPoints()) {
                    interceptor->shields /= 4;
                    state++;
                }
            } break; case 4: {
                if (interceptor->order == Order::InterceptorReturn)
                    state++;
            } break; case 5: {
                if (carrier->target == nullptr) {
                    TestAssert(carrier->carrier.in_child == nullptr);
                    TestAssert(interceptor->GetShields() == interceptor->GetMaxShields());
                    carrier->Kill(nullptr);
                    state++;
                }
            } break; case 6: {
                if (interceptor->IsDying())
                    Pass();
            }
        }
    }
};

struct Test_Bunker : public GameTest {
    Unit *bunker;
    Unit *marine;
    Unit *enemy;
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                state++;
            break; case 1: {
                marine = CreateUnitForTest(Unit::Marine, 0);
                bunker = CreateUnitForTest(Unit::Bunker, 0);
                enemy = CreateUnitForTest(Unit::Zergling, 1);
                IssueOrderTargetingUnit_Simple(marine, Order::EnterTransport, bunker);
                state++;
            } break; case 2: {
                if (marine->flags & UnitStatus::InBuilding) {
                    SetEnemy(0, 1);
                    state++;
                }
            } break; case 3: {
                if (enemy->GetHealth() != enemy->GetMaxHealth()) {
                    state++;
                }
            } break; case 4: {
                if (enemy->IsDying()) {
                    IssueOrderTargetingNothing(bunker, Order::DisableDoodad);
                    state++;
                } else {
                    TestAssert(marine->target == enemy);
                    TestAssert(marine->order_wait <= 8);
                }
            } break; case 5: {
                if (bunker->sprite->main_image->drawfunc == Image::DrawFunc::DetectedCloak) {
                    bunker->Kill(nullptr);
                    state++;
                }
            } break; case 6: {
                // The marine should die as well as it cannot be unloaded
                if (FindUnit(Unit::Marine) == nullptr) {
                    Pass();
                } else {
                    TestAssert(FindUnit(Unit::Marine)->related != nullptr);
                    TestAssert(!FindUnit(Unit::Bunker)->IsDying());
                }
            }
        }
    }
};

// Bw has this behaviour where creating an cloaked unit which does not have drawfunc 0 in main image,
// causes it's child overlays also not appear as cloaked. However, when the unit is detected, the
// child overlays get their drawfunc changed so they become cloaked. This desync between owner,
// who doesn't ever detect it and has drawfunc 0, and detecting player, who has detected drawfunc,
// causes issues as Image::Drawfunc_ProgressFrame controls whether units actually are cloaked or not
struct Test_DrawfuncSync : public GameTest {
    Unit *vision;
    Unit *arbiter;
    Unit *archon;
    int drawfunc_seen;
    int drawfunc_cloaked;
    int drawfunc_detected;
    const int ArchonBeing = 135;
    void Init() override {
        ResetVisions();
    }
    void NextFrame() override {
        switch (state) {
            case 0: case 3: case 6:
                if (NoUnits()) { state++; }
            break; case 1: {
                arbiter = CreateUnitForTest(Unit::Arbiter, 0);
                archon = CreateUnitForTest(Unit::Archon, 0);
                archon->sprite->main_image->drawfunc = 1;
                IssueOrderTargetingGround(archon, Order::Move, 100, 500);
                state++;
            } break; case 2: {
                if (archon->invisibility_effects == 0) {
                    for (Image *img : archon->sprite->first_overlay) {
                        if (img->image_id == ArchonBeing) {
                            drawfunc_seen = img->drawfunc;
                            ClearUnits();
                            state++;
                        }
                    }
                }
            } break; case 4: {
                vision = CreateUnitForTest(Unit::Wraith, 0);
                arbiter = CreateUnitForTest(Unit::Arbiter, 1);
                archon = CreateUnitForTest(Unit::Archon, 1);
                IssueOrderTargetingGround(archon, Order::Move, 100, 500);
                archon->sprite->main_image->drawfunc = 1;
                state++;
            } break; case 5: {
                if (archon->invisibility_effects == 0) {
                    for (Image *img : archon->sprite->first_overlay) {
                        if (img->image_id == ArchonBeing) {
                            drawfunc_cloaked = img->drawfunc;
                            ClearUnits();
                            state++;
                        }
                    }
                }
            } break; case 7: {
                arbiter = CreateUnitForTest(Unit::Arbiter, 1);
                archon = CreateUnitForTest(Unit::Archon, 1);
                archon->sprite->main_image->drawfunc = 1;
                state++;
            } break; case 8: {
                TestAssert(archon->invisibility_effects == 1);
                vision = CreateUnitForTest(Unit::Overlord, 0);
                IssueOrderTargetingGround(archon, Order::Move, 100, 500);
                state++;
            } break; case 9: {
                if (archon->invisibility_effects == 0) {
                    for (Image *img : archon->sprite->first_overlay) {
                        if (img->image_id == ArchonBeing) {
                            drawfunc_detected = img->drawfunc;
                            state++;
                        }
                    }
                }
            } break; case 10: {
                // Either having consistently same drawfunc or behaving as one would expect cloak is allowed
                // (As it currently just keeps drawfunc 0)
                if (drawfunc_seen >= Image::DrawFunc::Cloaking && drawfunc_seen <= Image::DrawFunc::DetectedDecloaking) {
                    TestAssert(drawfunc_seen == drawfunc_detected);
                    TestAssert(drawfunc_cloaked == drawfunc_detected + 3);
                } else {
                    TestAssert(drawfunc_seen == drawfunc_detected);
                    TestAssert(drawfunc_cloaked == drawfunc_detected);
                }
                Pass();
            }
        }
    }
};

struct AiSpell {
    int tech;
    int caster_unit_id;
    int target_unit_id;
    int attacker_unit_id;
};

const AiSpell revenge_spells[] = {
    { Tech::Lockdown, Unit::Ghost, Unit::Wraith, Unit::None },
    // Requires detector with hp > 80
    { Tech::OpticalFlare, Unit::Medic, Unit::Overlord, Unit::Marine },
    { Tech::Irradiate, Unit::ScienceVessel, Unit::Mutalisk, Unit::None },
    // Requires unit which can attack the vessel and has over 200 shields
    { Tech::EmpShockwave, Unit::ScienceVessel, Unit::Archon, Unit::None },
    { Tech::Ensnare, Unit::Queen, Unit::Wraith, Unit::None },
    { Tech::SpawnBroodlings, Unit::Queen, Unit::Hydralisk, Unit::None },
    { Tech::Plague, Unit::Defiler, Unit::Marine, Unit::None },
    { Tech::PsionicStorm, Unit::HighTemplar, Unit::Marine, Unit::None },
    { Tech::Feedback, Unit::DarkArchon, Unit::Ghost, Unit::None },
    { Tech::Maelstrom, Unit::DarkArchon, Unit::Ultralisk, Unit::None },
    { Tech::StasisField, Unit::Arbiter, Unit::Battlecruiser, Unit::None },
    { Tech::DisruptionWeb, Unit::Corsair, Unit::Dragoon, Unit::None },
    { -1, -1, -1 }
};

// Imperfect, can detect idle casts as revenge casts
struct Test_AiSpell : public GameTest {
    Unit *spellcaster;
    const AiSpell *spell;
    Point spawn_pos; // Because units like to get caught to spell cast in previous test
    void Init() override {
        ResetVisions();
        SetEnemy(0, 1);
        SetEnemy(1, 0);
        AiPlayer(1);
        spell = revenge_spells;
        spawn_pos = Point(200, 200);
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                state++;
            break; case 1: {
                if (spell->tech == -1) {
                    Pass();
                    return;
                }
                ClearTechs();
                spellcaster = CreateUnitForTestAt(spell->caster_unit_id, 1, spawn_pos + Point(100, 100));
                CreateUnitForTestAt(spell->target_unit_id, 0, spawn_pos);
                if (spell->attacker_unit_id != Unit::None)
                    CreateUnitForTestAt(spell->attacker_unit_id, 0, spawn_pos);

                GiveTech(spell->tech, 1);
                state++;
            } break; case 2: {
                TestAssert(!spellcaster->IsDying());
                if (spellcaster->energy != spellcaster->GetMaxEnergy()) {
                    ClearUnits();
                    spawn_pos.x ^= 256;
                    spell += 1;
                    state = 0;
                }
            }
        }
    }
};

struct AiCloakVariation {
    int unit_id;
};

static AiCloakVariation ai_cloak_variations[] = {
    { Unit::Zealot },
    { Unit::Goliath },
    { Unit::Wraith },
    { -1 }
};

struct Test_AiCloak : public GameTest {
    Unit *cloaker;
    Unit *attacker;
    AiCloakVariation const *variation;

    void Init() override {
        ResetVisions();
        SetEnemy(0, 1);
        SetEnemy(1, 0);
        AiPlayer(1);
        variation = ai_cloak_variations;
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                state++;
            break; case 1: {
                if (variation->unit_id == -1) {
                    state = 3;
                    return;
                }
                cloaker = CreateUnitForTestAt(Unit::InfestedKerrigan, 1, Point(100, 100));
                attacker = CreateUnitForTestAt(variation->unit_id, 0, Point(150, 100));
                state++;
            } break; case 2: {
                TestAssert(!cloaker->IsDying() && !attacker->IsDying());
                if (cloaker->IsInvisible()) {
                    ClearUnits();
                    variation += 1;
                    state = 0;
                }
            } break; case 3: {
                cloaker = CreateUnitForTestAt(Unit::InfestedKerrigan, 1, Point(100, 100));
                attacker = CreateUnitForTestAt(Unit::Scout, 0, Point(150, 100));
                cloaker->energy = 0;
                cloaker->hitpoints = 50 * 256;
                state++;
            } break; case 4: {
                if (cloaker->IsDying())
                    Pass();
                cloaker->energy = 0;
                TestAssert(!cloaker->IsInvisible());
            }
        }
    }
};

struct Test_Liftoff : public GameTest {
    Unit *building;
    Unit *tank;
    Unit *burrower;

    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                building = CreateUnitForTestAt(Unit::CommandCenter, 0, Point(100, 100));
                tank = CreateUnitForTestAt(Unit::SiegeTank_Sieged, 0, Point(200, 200));
                burrower = CreateUnitForTestAt(Unit::Zergling, 0, Point(200, 200));
                IssueOrderTargetingNothing(burrower, Order::Burrow);
                IssueOrderTargetingNothing(building, Order::LiftOff);
                state++;
            break; case 1: {
                if (burrower->order == Order::Burrowed) {
                    IssueOrderTargetingGround(building, Order::Land, 100, 100);
                    state++;
                }
            } break; case 2: {
                if (building->order == Order::Land && building->order_state == 3)
                {
                    MoveUnit(tank, 100, 100);
                    MoveUnit(burrower, 100, 100);
                    state++;
                }
            } break; case 3: {
                if (UnitCount() == 1 && building->order != Order::Land) {
                    TestAssert(building->order == units_dat_return_to_idle_order[building->unit_id]);
                    TestAssert((building->sprite->last_overlay->flags & 4) == 0);
                    // Test lifting once more
                    IssueOrderTargetingNothing(building, Order::LiftOff);
                    state++;
                } else {
                    TestAssert(building->order == Order::Land);
                }
            } break; case 4: {
                if (~building->flags & UnitStatus::Building) {
                    IssueOrderTargetingGround(building, Order::Land, 100, 100);
                    state++;
                }
            } break; case 5: {
                if (building->order == Order::Land)
                    state++;
            } break; case 6: {
                if (building->order != Order::Land) {
                    TestAssert(!building->IsFlying());
                    TestAssert((building->sprite->last_overlay->flags & 4) == 0);
                    building->IssueSecondaryOrder(Order::Train);
                    building->build_queue[building->current_build_slot] = Unit::SCV;
                    state++;
                }
            } break; case 7: {
                Unit *scv = FindUnit(Unit::SCV);
                if (scv != nullptr) {
                    Pass();
                }
            }
        }
    }
};

struct Test_Siege : public GameTest {
    Unit *building;
    Unit *tank;
    Unit *target;
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                building = CreateUnitForTestAt(Unit::CommandCenter, 0, Point(100, 100));
                tank = CreateUnitForTestAt(Unit::SiegeTankTankMode, 0, Point(100, 100));
                IssueOrderTargetingNothing(tank, Order::SiegeMode);
                state++;
            break; case 1: {
                if (UnitCount() == 1) {
                    building->Kill(nullptr);
                    tank = CreateUnitForTestAt(Unit::SiegeTankTankMode, 0, Point(100, 100));
                    target = CreateUnitForTestAt(Unit::Marine, 0, Point(250, 100));
                    IssueOrderTargetingNothing(tank, Order::SiegeMode);
                    state++;
                }
            } break; case 2: {
                if (tank->unit_id == Unit::SiegeTank_Sieged && tank->order != Order::SiegeMode) {
                    IssueOrderTargetingUnit_Simple(tank, Order::WatchTarget, target);
                    state++;
                }
            } break; case 3: {
                if (UnitCount() == 1) {
                    Pass();
                } else {
                    TestAssert(tank->target == target);
                }
            }
        }
    }
};

struct Test_Bounce : public GameTest {
    Unit *muta;
    Unit *target;
    Unit *other;
    void Init() override {
        SetEnemy(0, 1);
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                muta = CreateUnitForTestAt(Unit::Mutalisk, 0, Point(100, 100));
                target = CreateUnitForTestAt(Unit::Marine, 1, Point(100, 100));
                other = CreateUnitForTestAt(Unit::Marine, 1, Point(100, 100));
                IssueOrderTargetingUnit_Simple(muta, Order::AttackUnit, target);
                state++;
            break; case 1: {
                TestAssert(muta->target == target);
                if (other->GetHealth() != other->GetMaxHealth()) {
                    TestAssert(target->GetHealth() != target->GetMaxHealth());
                    state++;
                }
            } break; case 2: {
                if (muta->target != target) {
                    TestAssert(other->GetHealth() > other->GetMaxHealth() / 2);
                    Pass();
                }
            }
        }
    }
};

struct Test_Dweb : public GameTest {
    Unit *corsair;
    Unit *enemy;
    int next_unit_id;
    void Init() override {
        SetEnemy(1, 0);
        next_unit_id = 0;
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                if (!NoUnits())
                    return;

                NextUnitId();
                if (next_unit_id == Unit::None) {
                    Pass();
                    return;
                }
                corsair = CreateUnitForTestAt(Unit::Corsair, 0, Point(100, 100));
                IssueOrderTargetingGround(corsair, Order::DisruptionWeb, 150, 150);
                state++;
            break; case 1: {
                if (FindUnit(Unit::DisruptionWeb) != nullptr) {
                    enemy = CreateUnitForTestAt(next_unit_id, 1, Point(150, 150));
                    next_unit_id += 1;
                    state++;
                }
            } break; case 2: {
                if (FindUnit(Unit::DisruptionWeb) == nullptr) {
                    ClearUnits();
                    state = 0;
                } else {
                    TestAssert(corsair->GetHealth() == corsair->GetMaxHealth());
                }
            }
        }
    }
    void NextUnitId() {
        while (next_unit_id != Unit::None) {
            if (units_dat_elevation_level[next_unit_id] == 4 &&
                    ~units_dat_flags[next_unit_id] & UnitFlags::Subunit &&
                    ~units_dat_flags[next_unit_id] & UnitFlags::Air &&
                    (units_dat_group_flags[next_unit_id] & 0x7) != 0 && // Require an race
                    (units_dat_air_weapon[next_unit_id] != Weapon::None ||
                        (units_dat_subunit[next_unit_id] != Unit::None &&
                         units_dat_air_weapon[units_dat_subunit[next_unit_id]] != Weapon::None)))
            {
                return;
            }
            next_unit_id++;
        }
    }
};

struct Test_RightClick : public GameTest {
    Unit *building;
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                int player = *bw::local_unique_player_id;
                building = CreateUnitForTestAt(Unit::CommandCenter, player, Point(100, 100));
                state++;
            } break; case 1: {
                // Screen position is rounded down to eights,
                // so MoveScreen(300, 200) would do same thing
                MoveScreen(296, 200);
                int player = *bw::local_unique_player_id;
                frames_remaining = 50;
                bw::selection_groups[player][0] = building;
                bw::selection_groups[player][1] = nullptr;
                bw::client_selection_group2[0] = building;
                bw::client_selection_group[0] = building;
                *bw::primary_selected = building;
                for (int i = 1; i < Limits::Selection; i++) {
                    bw::client_selection_group2[i] = nullptr;
                    bw::client_selection_group[i] = nullptr;
                }
                *bw::client_selection_changed = 1;
                RefreshUi();
                Event event;
                event.ext_type = 0;
                event.type = 0xe;
                event.unk4 = 0;
                event.x = 14;
                event.y = 10;
                GameScreenRClickEvent(&event);
                state++;
            } break; case 2: {
                if (building->rally.position == Point(310, 210)) {
                    Pass();
                }
            }
        }
    }
};

struct Test_HoldPosition : public GameTest {
    Unit *attacker;
    Unit *enemy;
    int variant;
    void Init() override {
        SetEnemy(0, 1);
        variant = 1;
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                attacker = CreateUnitForTestAt(Unit::Marine, 0, Point(100, 100));
                enemy = CreateUnitForTestAt(Unit::Battlecruiser, 1, Point(150, 100));
                IssueOrderTargetingNothing(attacker, Order::HoldPosition);
                state++;
            } break; case 1: {
                // There was an crash when unit holding position was targeting unit which suddenly
                // became unattackable, so test for it becoming unattackable at different times
                if (variant == 7) {
                    Pass();
                    return;
                }
                if (enemy->GetHealth() != enemy->GetMaxHealth()) {
                    if (attacker->order_wait == variant) {
                        if (enemy->IsInvincible()) {
                            enemy->flags &= ~UnitStatus::Invincible;
                            enemy->hitpoints = enemy->GetMaxHitPoints() * 256;
                            variant++;
                        } else {
                            enemy->flags |= UnitStatus::Invincible;
                        }
                    }
                }
            }
        }
    }
};

struct Test_Attack : public GameTest {
    Unit *attacker;
    Unit *enemy;
    int variant;
    void Init() override {
        variant = 1;
    }
    void NextFrame() override {
        switch (state) {
            // Bw has a bug where an unit ordered to attack will get stuck under following conditions:
            // - Attacker was moving before the order
            // - Enemy is barely in range
            // - Enemy is no longer in range after the first frame of attack order
            // - Attacker uses iscript.bin movement and its attack animation has no move commands
            // This test recreates those conditions
            case 0: {
                if (variant == 100) {
                    Pass();
                    return;
                }
                attacker = CreateUnitForTestAt(Unit::Hydralisk, 0, Point(100, 100));
                enemy = CreateUnitForTestAt(Unit::Guardian, 0, Point(300, 100));
                IssueOrderTargetingGround(attacker, Order::Move, 600, 100);
                IssueOrderTargetingGround(enemy, Order::Move, 600, 100);
                state++;
            } break; case 1: {
                if (attacker->sprite->position.x > 200 + variant) {
                    IssueOrderTargetingUnit_Simple(attacker, Order::AttackUnit, enemy);
                    state++;
                }
            } break; case 2: {
                if (enemy->GetHealth() < enemy->GetMaxHealth() * 3 / 4) {
                    ClearUnits();
                    state = 0;
                    variant++;
                    frames_remaining = 5000;
                }
            }
        }
    }
};

struct Test_Splash : public GameTest {
    Unit *attacker;
    Unit *target;
    Unit *second_target;
    void Init() override {
        SetEnemy(0, 1);
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                target = CreateUnitForTestAt(Unit::Nexus, 0, Point(100, 150));
                attacker = CreateUnitForTestAt(Unit::InfestedTerran, 0, Point(100, 300));
                IssueOrderTargetingUnit_Simple(attacker, Order::SapUnit, target);
                state++;
            } break; case 1: {
                if (target->GetHealth() != target->GetMaxHealth()) {
                    int dmg = weapons_dat_damage[units_dat_ground_weapon[Unit::InfestedTerran]];
                    TestAssert(target->GetMaxHealth() - target->GetHealth() > dmg * 3 / 4);
                    ClearUnits();
                    target = CreateUnitForTestAt(Unit::SupplyDepot, 1, Point(100, 100));
                    second_target = CreateUnitForTestAt(Unit::SupplyDepot, 1, Point(100, 100));
                    attacker = CreateUnitForTestAt(Unit::Lurker, 0, Point(100, 150));
                    IssueOrderTargetingNothing(attacker, Order::Burrow);
                    state++;
                }
            } break; case 2: {
                TestAssert(target->GetHealth() == second_target->GetHealth());
                if (target->GetHealth() < target->GetMaxHealth() / 2) {
                    Pass();
                }
            }
        }
    }
};

struct Test_AiAggro : public GameTest {
    Unit *ai_ling;
    Unit *target;
    void Init() override {
        AiPlayer(1);
        SetEnemy(1, 0);
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                // Should not aggro
                ai_ling = CreateUnitForTestAt(Unit::Zergling, 1, Point(100, 100));
                target = CreateUnitForTestAt(Unit::Reaver, 0, Point(400, 100));
                state++;
                frames_remaining = 300;
            } break; case 1: {
                TestAssert(ai_ling->ai != nullptr && ai_ling->ai->type == 1);
                TestAssert(ai_ling->order != Order::AttackUnit);
                if (frames_remaining < 100)
                    Pass();
            }
        }
    }
};

struct Test_MindControl : public GameTest {
    Unit *da;
    Unit *target;
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                da = CreateUnitForTestAt(Unit::DarkArchon, 0, Point(100, 100));
                target = CreateUnitForTestAt(Unit::Marine, 1, Point(100, 100));
                IssueOrderTargetingUnit_Simple(da, Order::MindControl, target);
                state++;
            } break; case 1: {
                if (da->order == Order::MindControl)
                    state++;
            } break; case 2: {
                if (target->player == da->player) {
                    TestAssert(da->shields == 0);
                    ClearUnits();
                    da = CreateUnitForTestAt(Unit::DarkArchon, 0, Point(100, 100));
                    target = CreateUnitForTest(Unit::Carrier, 1);
                    target->IssueSecondaryOrder(Order::TrainFighter);
                    target->build_queue[target->current_build_slot] = Unit::Interceptor;
                    state++;
                } else {
                    TestAssert(da->order == Order::MindControl);
                }
            } break; case 3: {
                if (target->carrier.in_child != nullptr || target->carrier.out_child != nullptr) {
                    IssueOrderTargetingUnit_Simple(da, Order::MindControl, target);
                    state++;
                }
            } break; case 4: {
                if (target->player == da->player) {
                    Unit *interceptor = target->carrier.in_child;
                    if (interceptor == nullptr)
                        interceptor = target->carrier.out_child;
                    TestAssert(interceptor && interceptor->player == da->player);
                    Pass();
                }
            }
        }
    }
};

struct Test_PosSearch : public GameTest {
    Unit *unit;
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                unit = CreateUnitForTestAt(Unit::Marine, 0, Point(100, 100));
                state++;
            } break; case 1: {
                Unit *result = unit_search->FindNearest(Point(100, 150),
                        Rect16(Point(100, 150), 5), [](const auto *a) { return true; });
                TestAssert(result == nullptr);
                result = unit_search->FindNearest(Point(150, 100),
                        Rect16(Point(150, 100), 5), [](const auto *a) { return true; });
                TestAssert(result == nullptr);
                result = unit_search->FindNearest(Point(110, 110),
                        Rect16(Point(110, 100), 25), [](const auto *a) { return true; });
                TestAssert(result == unit);
                result = unit_search->FindNearest(Point(110, 100),
                        Rect16(Point(110, 100), 9), [](const auto *a) { return true; });
                TestAssert(result == nullptr);
                result = unit_search->FindNearest(Point(110, 100),
                        Rect16(Point(110, 100), 10), [](const auto *a) { return true; });
                TestAssert(result == unit);
                Pass();
            }
        }
    }
};

struct Test_AiTarget : public GameTest {
    Unit *unit;
    Unit *enemy;
    void Init() override {
        SetEnemy(0, 1);
        SetEnemy(1, 0);
        AiPlayer(1);
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                unit = CreateUnitForTestAt(Unit::Devourer, 1, Point(100, 100));
                enemy = CreateUnitForTestAt(Unit::Scout, 0, Point(120, 120));
                state++;
            } break; case 1: {
                TestAssert(!unit->IsDying() && !enemy->IsDying());
                if (unit->target == enemy) {
                    CreateUnitForTestAt(Unit::Scout, 0, Point(80, 80));
                    state++;
                }
            } break; case 2: {
                if (unit->GetHealth() == 0 || enemy->GetHealth() == 0) {
                    Pass();
                } else {
                    TestAssert(unit->target == enemy);
                }
            }
        }
    }
};

struct Test_AttackMove : public GameTest {
    Unit *unit;
    Unit *enemy;
    Unit *enemy2;
    Unit *target;
    void Init() override {
        SetEnemy(0, 1);
        SetEnemy(1, 0);
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                unit = CreateUnitForTestAt(Unit::Guardian, 0, Point(100, 100));
                enemy = CreateUnitForTestAt(Unit::HunterKiller, 1, Point(400, 100));
                enemy2 = CreateUnitForTestAt(Unit::HunterKiller, 1, Point(400, 100));
                IssueOrderTargetingGround(unit, Order::AttackMove, 400, 100);
                state++;
            } break; case 1: {
                SetHp(unit, 100 * 256);
                SetHp(enemy, 100 * 256);
                SetHp(enemy2, 100 * 256);
                if (unit->target != nullptr) {
                    target = unit->target;
                    IssueOrderTargetingGround(target, Order::Move, 1000, 100);
                    state++;
                }
            } break; case 2: {
                if (target->order == Order::Move) {
                    state++;
                }
            } break; case 3: {
                SetHp(unit, 100 * 256);
                SetHp(enemy, 100 * 256);
                SetHp(enemy2, 100 * 256);
                TestAssert(target->order == Order::Move);
                if (unit->target != target && unit->target != nullptr) {
                    Pass();
                }
            }
        }
    }
};

struct Test_Detection : public GameTest {
    Unit *cloaked;
    Unit *detector;
    int wait;
    void Init() override {
        ResetVisions();
        wait = 0;
    }
    void NextFrame() override {
        if (wait != 0) {
            wait -= 1;
            return;
        }

        // In Unit::ProgressFrames for invisible units
        const int cloak_wait = 30;

        switch (state) {
            case 0: {
                cloaked = CreateUnitForTestAt(Unit::Observer, 0, Point(100, 100));
                detector = CreateUnitForTestAt(Unit::Marine, 1, Point(100, 100));
                state++;
                wait = cloak_wait;
            } break; case 1: {
                TestAssert(cloaked->IsInvisibleTo(detector));
                detector->Kill(nullptr);
                detector = CreateUnitForTestAt(Unit::Overlord, 1, Point(100, 100));
                wait = cloak_wait;
                state++;
            } break; case 2: {
                TestAssert(!cloaked->IsInvisibleTo(detector));
                detector->Kill(nullptr);
                detector = CreateUnitForTestAt(Unit::Queen, 1, Point(100, 100));
                IssueOrderTargetingGround(detector, Order::Ensnare, 100, 100);
                state++;
            } break; case 3: {
                if (cloaked->ensnare_timer != 0) {
                    wait = cloak_wait;
                    state++;
                }
            } break; case 4: {
                TestAssert(!cloaked->IsInvisibleTo(detector));
                state++;
            } break; case 5: {
                if (cloaked->ensnare_timer == 0) {
                    wait = cloak_wait;
                    state++;
                }
            } break; case 6: {
                TestAssert(cloaked->IsInvisibleTo(detector));
                detector->Kill(nullptr);
                detector = CreateUnitForTestAt(Unit::Defiler, 1, Point(100, 100));
                IssueOrderTargetingGround(detector, Order::Plague, 100, 100);
                state++;
            } break; case 7: {
                if (cloaked->plague_timer != 0) {
                    wait = cloak_wait;
                    state++;
                }
            } break; case 8: {
                TestAssert(!cloaked->IsInvisibleTo(detector));
                state++;
            } break; case 9: {
                if (cloaked->plague_timer == 0) {
                    wait = cloak_wait;
                    state++;
                }
            } break; case 10: {
                TestAssert(cloaked->IsInvisibleTo(detector));
                detector->Kill(nullptr);
                detector = CreateUnitForTestAt(Unit::Devourer, 1, Point(100, 100));
                Unit *secondary = CreateUnitForTestAt(Unit::Devourer, 1, Point(100, 100));
                IssueOrderTargetingUnit_Simple(secondary, Order::AttackUnit, detector);
                state++;
            } break; case 11: {
                if (detector->GetHealth() != detector->GetMaxHealth()) {
                    wait = cloak_wait;
                    state++;
                }
            } break; case 12: {
                // Acid spores should only apply to already detected units
                TestAssert(cloaked->IsInvisibleTo(detector));
                Pass();
            }
        }
    }
};

struct Test_Death : public GameTest {
    int player;
    int next_unit_id;
    void Init() override {
        // Do some fighting as well
        SetEnemy(0, 1);
        SetEnemy(1, 1);
        AiPlayer(1);
        next_unit_id = 0;
        player = 0;
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                NextUnitId();
                if (next_unit_id == Unit::None) {
                    next_unit_id = 0;
                    player += 1;
                    if (player == 2)
                        player = NeutralPlayer;
                    if (player == Limits::Players) {
                        for (Unit *unit : *bw::first_active_unit)
                            unit->Kill(nullptr);
                        state++;
                    }
                    return;
                }
                CreateUnitForTestAt(next_unit_id, player, Point(300, 300));
                next_unit_id += 1;
            break; case 1: {
                // Kill vespene geysers
                for (Unit *unit : *bw::first_active_unit)
                    unit->Kill(nullptr);
                if (NoUnits())
                    Pass();
            }
        }
    }
    void NextUnitId() {
        while (next_unit_id != Unit::None) {
            if (~units_dat_flags[next_unit_id] & UnitFlags::Subunit &&
                    units_dat_hitpoints[next_unit_id] > 1 &&
                    units_dat_armor_type[next_unit_id] != 0) // Skip independent
            {
                return;
            }
            next_unit_id++;
        }
    }
};

/// More ai aggro stuff.. Parasite has barely any reactions, but if the queen's
/// target at the moment the parasite bullet hits is same player as the unit being
/// parasited, then the ai will attack it. (As Ai::UpdateAttackTarget() requires
/// previous_attacker having target of same player) Additionally parasite has such a
/// long range, that Ai::AskForHelp() may not even bother helping.
struct Test_ParasiteAggro : public GameTest {
    Unit *queen;
    Unit *target;
    Unit *other;
    void Init() override {
        Visions();
        AiPlayer(1);
        SetEnemy(1, 0);
        SetEnemy(0, 1);
    }
    void SetupNext(int queen_player, int target_unit, int other_unit)
    {
        ClearUnits();
        int other_player = queen_player == 0 ? 1 : 0;
        queen = CreateUnitForTestAt(Unit::Queen, queen_player, Point(180, 100));
        target = CreateUnitForTestAt(target_unit, other_player, Point(600, 100));
        if (other_unit != Unit::None)
            other = CreateUnitForTestAt(other_unit, other_player, Point(530, 100));
            IssueOrderTargetingUnit_Simple(queen, Order::Parasite, target);
        state++;
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                SetupNext(1, Unit::Marine, Unit::None);
            } break; case 1: {
                if (target->parasites == 0)
                    return;
                // Human owned units don't care
                TestAssert(target->target == nullptr);
                SetupNext(0, Unit::Marine, Unit::None);
            } break; case 2: {
                if (target->parasites == 0)
                    return;
                // Ai owned units don't care from a single parasite
                TestAssert(target->target == nullptr);
                SetupNext(0, Unit::Marine, Unit::Marine);
            } break; case 3: {
                if (queen->target == nullptr) {
                    IssueOrderTargetingUnit_Simple(queen, Order::Parasite, target);
                    // If it was just parasited, try again as the frames aligned just poorly
                    // (Should maybe have random variance?)
                    target->parasites = 0;
                    queen->energy = 150 * 256;
                    TestAssert(target->target == nullptr);
                    return;
                } else if (target->parasites != 0) {
                    // Ai cares if queen is targeting its unit at the time of hit
                    TestAssert(target->target == queen);
                    // But the queen should be far away enough for the other marine
                    TestAssert(other->target == nullptr);
                    SetupNext(0, Unit::Goliath, Unit::Goliath);
                }
            } break; case 4: {
                if (queen->target == nullptr) {
                    IssueOrderTargetingUnit_Simple(queen, Order::Parasite, target);
                    target->parasites = 0;
                    queen->energy = 150 * 256;
                    TestAssert(target->target == nullptr);
                    return;
                } else if (target->parasites != 0) {
                    TestAssert(target->target == queen);
                    // Goliaths have range long enough to aggro even from parasite range
                    TestAssert(other->target == queen);
                    Pass();
                }
            }
        }
    }
};

struct Test_HitChance : public GameTest {
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                for (int i = 0; i < 32; i++) {
                    Unit *unit = CreateUnitForTestAt(Unit::Marine, 1, Point(100, 100 + 20 * i));
                    Unit *enemy = CreateUnitForTestAt(Unit::Overlord, 0, Point(120, 100 + 20 * i));
                    IssueOrderTargetingUnit_Simple(unit, Order::AttackUnit, enemy);
                }
                state++;
            } break; case 1: {
                for (Bullet *bullet : bullet_system->ActiveBullets()) {
                    if (bullet->DoesMiss()) {
                        Pass();
                    }
                }
                for (Unit *unit : *bw::first_active_unit) {
                    unit->hitpoints = unit->GetMaxHitPoints() * 256;
                }
            }
        }
    }
};

struct OverlaySpell {
    int order;
    int caster_unit;
    int target_unit;
};
const OverlaySpell overlay_spells[] = {
    { Order::Lockdown, Unit::Ghost, Unit::Goliath },
    { Order::Restoration, Unit::Medic, Unit::SiegeTankTankMode },
    { Order::OpticalFlare, Unit::Medic, Unit::Goliath },
    { Order::DefensiveMatrix, Unit::ScienceVessel, Unit::SiegeTankTankMode },
    { Order::Irradiate, Unit::ScienceVessel, Unit::Goliath, },
    { Order::Ensnare, Unit::Queen, Unit::SiegeTankTankMode },
    { Order::Plague, Unit::Defiler, Unit::Goliath },
    // Note: If feedback kills it spawns a sprite instead of a image
    { Order::Feedback, Unit::DarkArchon, Unit::Battlecruiser },
    { Order::Maelstrom, Unit::DarkArchon, Unit::Ultralisk },
    { Order::MindControl, Unit::DarkArchon, Unit::Goliath },
    { Order::StasisField, Unit::Arbiter, Unit::SiegeTankTankMode },
};

struct Test_SpellOverlay : public GameTest {
    vector<tuple<Unit *, int, bool>> targets;
    void Init() override {
        targets.clear();
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                int overlay_spell_count = sizeof overlay_spells / sizeof(overlay_spells[0]);
                Point pos(100, 100);
                for (int i = 0; i < overlay_spell_count; i++) {
                    pos.y += 120;
                    if (pos.y > 32 * 60) {
                        pos.y = 100;
                        pos.x += 200;
                    }
                    auto spell = &overlay_spells[i];
                    Unit *spellcaster = CreateUnitForTestAt(spell->caster_unit, 1, pos);
                    Unit *target = CreateUnitForTestAt(spell->target_unit, 0, pos + Point(30, 0));
                    IssueOrderTargetingUnit_Simple(spellcaster, spell->order, target);
                    int default_overlay = target->GetTurret()->sprite->first_overlay->image_id;
                    targets.emplace_back(target, default_overlay, false);
                }
                frames_remaining = 1000;
                state++;
            } break; case 1: {
                for (auto &tp : targets) {
                    Unit *turret = get<Unit *>(tp)->GetTurret();
                    if (turret->sprite->first_overlay->image_id != get<int>(tp)) {
                        get<bool>(tp) = true;
                    }
                }
                if (std::all_of(targets.begin(), targets.end(), [](const auto &t) { return get<bool>(t); })) {
                    Pass();
                }
            }
        }
    }
};

/// Turn1CWise is not supposed to do anything if the unit has a target.
struct Test_Turn1CWise : public GameTest {
    Unit *turret;
    Unit *target;
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                turret = CreateUnitForTestAt(Unit::MissileTurret, 0, Point(100, 100));
                target = CreateUnitForTestAt(Unit::Scout, 0, Point(100, 150));
                state++;
            } break; case 1: {
                if (turret->facing_direction == 0) {
                    IssueOrderTargetingUnit_Simple(turret, Order::AttackUnit, target);
                    frames_remaining = 10;
                    state++;
                }
            } break; case 2: {
                if (bullet_system->BulletCount() == 1)
                    Pass();
            }
        }
    }
};

/// When an attack which ignores armor gets absorbed by defensive matrix, the attack
/// deals 128 damage to hitpoints, even if the unit had shields.
struct Test_MatrixStorm : public GameTest {
    Unit *target;
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                Unit *vessel = CreateUnitForTestAt(Unit::ScienceVessel, 0, Point(100, 100));
                target = CreateUnitForTestAt(Unit::Scout, 0, Point(100, 150));
                IssueOrderTargetingUnit_Simple(vessel, Order::DefensiveMatrix, target);
                state++;
            } break; case 1: {
                if (target->matrix_timer != 0) {
                    Unit *ht = CreateUnitForTestAt(Unit::HighTemplar, 0, Point(100, 100));
                    IssueOrderTargetingUnit_Simple(ht, Order::PsiStorm, target);
                    frames_remaining = 250;
                    state++;
                }
            } break; case 2: {
                int hp_dmg = units_dat_hitpoints[target->unit_id] - target->hitpoints;
                int shield_dmg = target->GetMaxShields() - target->GetShields();
                TestAssert(shield_dmg == 0);
                TestAssert(hp_dmg == 0 || hp_dmg == 128);
                if (hp_dmg == 128) {
                    state++;
                }
            } break; case 3: {
                if (bullet_system->BulletCount() == 0)
                    Pass();
                // There should not be a shield overlay either
                for (Image *img : target->sprite->first_overlay) {
                    TestAssert(img->image_id != Image::ShieldOverlay);
                }
            }
        }
    }
};

/// Tests Ai::UpdateAttackTarget and related code
struct Test_AiTargetPriority : public GameTest {
    void Init() override {
        AiPlayer(1);
        SetEnemy(1, 0);
    }
    void NextFrame() override {
        switch (state) {
            case 0: case 1: {
                // Should prioritize the closer one, regardless of the creation order.
                const Point positions[] = { Point(100, 100), Point(200, 100) };
                Unit *first;
                if (state == 0)
                    first = CreateUnitForTestAt(Unit::Marine, 0, positions[0]);
                Unit *second = CreateUnitForTestAt(Unit::Marine, 0, positions[1]);
                if (state == 1)
                    first = CreateUnitForTestAt(Unit::Marine, 0, positions[0]);
                Unit *unit = CreateUnitForTestAt(Unit::Zergling, 1, Point(300, 100));
                IssueOrderTargetingUnit_Simple(first, Order::AttackUnit, unit);
                IssueOrderTargetingUnit_Simple(second, Order::AttackUnit, unit);
                TestAssert(Ai::GetBestTarget(unit, { first, second }) == second);
                ClearUnits();
                state++;
            } break; case 2: {
                // Here the guardian threatens marine (Marine is inside range), and zergling doesn't,
                // but zergling is inside marine's range and guardian isn't.
                // As such, the result will be one being active last.
                // That is bw's logic, but having the decision be consistent regardless of
                // order wouldn't be bad either. Often hits like these are in different frames,
                // and the ai could skip back and forth, which may be a "desired feature".
                //
                // In fact, hits over a single frame are more consistent than in bw, as
                // Ai::HitReactions does only one check, and in case of a "tie" like here,
                // the "best attacker of current frame" beats auto target.
                // If (x -> y -> z) means (Ai_ChooseBetterTarget(z, Ai_ChooseBetterTarget(y, x))),
                // vanilla bw would basically compare like this:
                // old target -> auto target -> attacker -> auto target -> attacker #2 -> auto target -> ...
                // \ Ai::UpdateAttackTarget #1           / \ Ai::UpdateAttackTarget #2 / \ ... #3
                // whereas teippi does the following:
                // old target -> auto target -> (attacker -> attacker #2 -> attacker #3 -> ...)
                //                              \ #1      / \ #2        /   \ #3      /   \ ...
                //                              \ Ai::HitReactions::AddHit (UpdatePickedTarget) /
                // \ Ai::UpdateAttackTarget from Ai::HitReactions::UpdateAttackTargets /
                // It could be closer to bw's behaviour if Ai::HitReactions cached the auto target,
                // but bw's behaviour can be considered to be buggy. For example, if auto target
                // and attacker #1 "tie", but attacker #2 loses to attacker #1 while beating auto target,
                // bw would pick attacker #2 where we pick attacker #1.
                //
                // Anyways, those tie cases are rare, as they require the auto target to be a unit
                // which cannot attack
                const Point positions[] = { Point(100, 100), Point(270, 100) };
                Unit *first;
                first = CreateUnitForTestAt(Unit::Guardian, 0, positions[0]);
                Unit *second = CreateUnitForTestAt(Unit::Zergling, 0, positions[1]);
                Unit *unit = CreateUnitForTestAt(Unit::Marine, 1, Point(350, 100));
                IssueOrderTargetingUnit_Simple(first, Order::AttackUnit, unit);
                IssueOrderTargetingUnit_Simple(second, Order::AttackUnit, unit);
                TestAssert(Ai::GetBestTarget(unit, { first, second }) == second);
                TestAssert(Ai::GetBestTarget(unit, { second, first }) == first);
                ClearUnits();
                state++;
            } break; case 3: {
                Pass();
            }
        }
    }
};

struct TransmissionTest {
    int unit_id;
    Point pos;
    bool ok;
};
// Well, these variants mostly test finding specific unit at a location but that's nice too
const TransmissionTest transmission_tests[] = {
    { Unit::Marine, Point(100, 100), false },
    { Unit::Zergling, Point(100, 100), true },
    { Unit::Zergling, Point(400, 100), false },
};

struct Test_Transmission : public GameTest {
    Unit *unit;
    const TransmissionTest *variant;
    void Init() override {
        bw::locations[0] = Location { Rect32(50, 50, 150, 150), 0, 0 };
        variant = transmission_tests;
    }
    void NextFrame() override {
        // Maybe is initialized incorrectly, as the structure is incomplete
        Trigger trigger;
        memset(&trigger, 0, sizeof(Trigger));
        trigger.actions[0].location = 1;
        trigger.actions[0].amount = 7;
        trigger.actions[0].misc = 500;
        trigger.actions[0].unit_id = Unit::Zergling;
        trigger.actions[0].sound_id = 0;
        trigger.actions[0].action_id = 0x7;
        switch (state) {
            case 0: {
                unit = CreateUnitForTestAt(variant->unit_id, 0, variant->pos);
                *bw::current_trigger = &trigger;
                *bw::trigger_current_player = 0;
                ProgressActions(&trigger);
                TestAssert(bw::player_wait_active[0] != 0);
                state++;
            } break; case 1: {
                if (bw::player_wait_active[0] == 0) {
                    bool circle_flashing = unit->sprite->selection_flash_timer != 0;
                    TestAssert(circle_flashing == variant->ok);
                    variant++;
                    const TransmissionTest *test_end = transmission_tests +
                        sizeof transmission_tests / sizeof(transmission_tests[0]);
                    ClearUnits();
                    state = 0;
                    if (variant == test_end) {
                        Pass();
                    }
                }
            }
        }
    }
};


/// There was a bug where attackking interceptor caused workers to come help.
/// Test that it no longer happens, but that attacking a building with a
/// worker still works.
struct Test_NearbyHelpers : public GameTest {
    Unit *helper;
    Unit *target;
    Unit *enemy;
    const TransmissionTest *variant;
    void Init() override {
        AiPlayer(1);
        SetEnemy(0, 1);
        SetEnemy(1, 0);
    }
    void CreateAiTown(const Point &pos, int player) {
        CreateUnitForTestAt(Unit::Nexus, player, pos);
        helper = CreateUnitForTestAt(Unit::Probe, player, pos + Point(0, 100));
        Unit *mineral = CreateUnitForTestAt(Unit::MineralPatch1, NeutralPlayer, pos + Point(0, 200));
        mineral->resource.resource_amount = 1500;
        AiScript_StartTown(pos.x, pos.y, 1, player);
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                CreateAiTown(Point(100, 100), 1);
                target = CreateUnitForTestAt(Unit::Carrier, 1, Point(100, 100));
                enemy = CreateUnitForTestAt(Unit::Hydralisk, 0, Point(100, 600));
                target->IssueSecondaryOrder(Order::TrainFighter);
                target->build_queue[target->current_build_slot] = Unit::Interceptor;
                state++;
            } break; case 1: {
                if (target->carrier.in_child != nullptr) {
                    IssueOrderTargetingGround(enemy, Order::Move, 100, 100);
                    state++;
                }
            } break; case 2: {
                SetHp(target, target->GetMaxHitPoints() * 256);
                SetHp(enemy, enemy->GetMaxHitPoints() * 256);
                if (target->carrier.out_child != nullptr) {
                    IssueOrderTargetingUnit_Simple(enemy, Order::AttackUnit, target->carrier.out_child);
                    state++;
                }
            } break; case 3: {
                SetHp(target, target->GetMaxHitPoints() * 256);
                SetHp(enemy, enemy->GetMaxHitPoints() * 256);
                TestAssert(helper->target != enemy);
                if (enemy->target == nullptr || enemy->target->unit_id != Unit::Interceptor) {
                    IssueOrderTargetingGround(enemy, Order::Move, 100, 100);
                    state--;
                } else if (enemy->target->GetHealth() != enemy->target->GetMaxHealth()) {
                    // This test makes only sense if interceptors have no ai
                    TestAssert(enemy->target->ai == nullptr);
                    if (IsInArea(enemy->target, CallFriends_Radius, helper)) {
                        frames_remaining = 50;
                        state++;
                    }
                }
            } break; case 4: {
                TestAssert(helper->target != enemy);
                if (frames_remaining == 1) {
                    enemy->Kill(nullptr);
                    frames_remaining = 5000;
                    enemy = CreateUnitForTestAt(Unit::SCV, 0, Point(100, 500));
                    target = FindUnit(Unit::Nexus);
                    IssueOrderTargetingUnit_Simple(enemy, Order::AttackUnit, target);
                    state++;
                }
            } break; case 5: {
                SetHp(target, target->GetMaxHitPoints() * 256);
                SetHp(enemy, enemy->GetMaxHitPoints() * 256);
                if (enemy->target->GetHealth() != enemy->target->GetMaxHealth()) {
                    if (IsInArea(enemy->target, CallFriends_Radius, helper)) {
                        frames_remaining = 50;
                        state++;
                    }
                }
            } break; case 6: {
                SetHp(target, target->GetMaxHitPoints() * 256);
                if (frames_remaining == 1) {
                    TestAssert(helper->target == enemy);
                    Pass();
                }
            }
        }
    }
};

GameTests::GameTests()
{
    current_test = -1;
    AddTest("Test test", new Test_Dummy);
    AddTest("Hallucination", new Test_Hallucination);
    AddTest("Plague", new Test_Plague);
    AddTest("Storm", new Test_Storm);
    AddTest("Shield overlay", new Test_ShieldOverlay);
    AddTest("Shield overlay - hallucinated", new Test_ShieldOverlayHallu);
    AddTest("Shield damage", new Test_ShieldDamage);
    AddTest("Lurker ai", new Test_LurkerAi);
    AddTest("Burrower ai", new Test_BurrowerAi);
    AddTest("Vision", new Test_Vision);
    AddTest("Carrier", new Test_Carrier);
    AddTest("Bunker", new Test_Bunker);
    AddTest("Unusual drawfunc sync", new Test_DrawfuncSync);
    AddTest("Ai spellcast", new Test_AiSpell);
    AddTest("Ai cloak", new Test_AiCloak);
    AddTest("Liftoff", new Test_Liftoff);
    AddTest("Siege mode", new Test_Siege);
    AddTest("Bounce", new Test_Bounce);
    AddTest("Disruption web", new Test_Dweb);
    AddTest("Right click", new Test_RightClick);
    AddTest("Hold position", new Test_HoldPosition);
    AddTest("Attack", new Test_Attack);
    AddTest("Splash", new Test_Splash);
    AddTest("Ai aggro", new Test_AiAggro);
    AddTest("Mind control", new Test_MindControl);
    AddTest("Pos search", new Test_PosSearch);
    AddTest("Ai targeting", new Test_AiTarget);
    AddTest("Attack move", new Test_AttackMove);
    AddTest("Detection", new Test_Detection);
    AddTest("Death", new Test_Death);
    AddTest("Parasite aggro", new Test_ParasiteAggro);
    AddTest("Hit chance", new Test_HitChance);
    AddTest("Spell overlays", new Test_SpellOverlay);
    AddTest("Iscript turn1cwise", new Test_Turn1CWise);
    AddTest("Matrix + storm", new Test_MatrixStorm);
    AddTest("Ai target priority", new Test_AiTargetPriority);
    AddTest("Transmission trigger", new Test_Transmission);
    AddTest("Nearby helpers", new Test_NearbyHelpers);
}

void GameTests::AddTest(const char *name, GameTest *test)
{
    test->name = name;
    test->id = tests.size();
    tests.emplace_back(test);
}

void GameTests::RunTests(int first, int last)
{
    current_test = first;
    last_test = last;
    if (last_test > tests.size())
        last_test = tests.size();

    NextTest();
}

void GameTests::NextTest()
{
    if (current_test == last_test)
    {
        Print("All tests passed!");
        current_test = -1;
        return;
    }

    tests[current_test]->state = -1;
    ClearUnits();
    if (CanStartTest()) {
        StartTest();
    }
}

void GameTests::StartTest() {
    Print("Running test %d: %s", current_test, tests[current_test]->name);
    NoAi();
    AllyPlayers();
    GiveAllTechs();
    ClearTriggers();
    *bw::cheat_flags = 0;
    tests[current_test]->state = 0;
    tests[current_test]->Init();
    CheckTest();
}

bool GameTests::CanStartTest() {
    return bullet_system->BulletCount() == 0 && NoUnits();
}

void GameTests::CheckTest()
{
    if (tests[current_test]->status == GameTest::Status::Failed)
    {
        Print("Test failed %d: %s (%s)", current_test, tests[current_test]->name,
                tests[current_test]->fail_reason.c_str());
        bw::game_speed_waits[*bw::game_speed] = 42;
        *bw::is_paused = 1;
        current_test = -1;
    }
    else if (tests[current_test]->status == GameTest::Status::Passed)
    {
        Print("Test passed %d: %s", current_test, tests[current_test]->name);
        current_test++;
        NextTest();
    }
}

void GameTests::NextFrame()
{
    if (current_test == -1)
        return;

    if (tests[current_test]->state == -1) {
        if (CanStartTest()) {
            StartTest();
        }
        return; // Always return
    }
    tests[current_test]->frames_remaining -= 1;
    tests[current_test]->NextFrame();
    CheckTest();
    if (current_test != -1 && tests[current_test]->frames_remaining == 0)
    {
        Print("Test timeout %d: %s", current_test, tests[current_test]->name);
        bw::game_speed_waits[*bw::game_speed] = 42;
        *bw::is_paused = 1;
        current_test = -1;
        if (IsDebuggerPresent())
            INT3();
    }
}

#endif
