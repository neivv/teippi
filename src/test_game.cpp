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

#include <algorithm>
#include "console/windows_wrap.h"

using std::min;

#define TestAssert(s) if (!(s)) { if(IsDebuggerPresent()) { INT3(); } Fail(#s); return; }

// This file has special permission for different brace style =)

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
    bw::alliances[player * Limits::Players + enemy] = 0;
}

static void AllyPlayers() {
    for (int i = 0; i < Limits::Players; i++) {
        for (int j = 0; j < Limits::Players; j++) {
            bw::alliances[i * Limits::Players + j] = 1;
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
            break; case 1:
                ht1 = CreateUnitForTest(Unit::HighTemplar, 0);
                ht2 = CreateUnitForTest(Unit::HighTemplar, 0);
                target = CreateUnitForTestAt(Unit::Battlecruiser, 0, Point(300, 100));
                IssueOrderTargetingUnit_Simple(ht1, Order::PsiStorm, target);
                state++;
            // Cases 2 and 3 should behave same, no matter if 2 storms are casted or 1
            break; case 2: case 3: {
                int dmg = target->GetMaxHitPoints() - target->GetHitPoints();
                int storm_dmg = weapons_dat_damage[Weapon::PsiStorm];
                TestAssert(dmg % storm_dmg == 0);
                if (dmg != 0) {
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
                        IssueOrderTargetingGround(ht2, Order::Move, pos.x, pos.y);
                        state++;
                        break;
                    }
                }
            break; case 5:
                if (target->IsStandingStill() != 0) {
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
                bw::selection_groups[player * Limits::Selection] = building;
                bw::selection_groups[player * Limits::Selection + 1] = nullptr;
                bw::client_selection_group2[0] = building;
                bw::client_selection_group[0] = building;
                bw::selection_rank_order[0] = building;
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
    void Init() override {
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
                    Pass();
                } else {
                    TestAssert(da->order == Order::MindControl);
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
