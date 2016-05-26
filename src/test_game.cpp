#ifdef DEBUG
#include "test_game.h"

#include <algorithm>

#include "common/assert.h"
#include "console/windows_wrap.h"

#include "constants/image.h"
#include "constants/order.h"
#include "constants/tech.h"
#include "constants/unit.h"
#include "constants/weapon.h"
#include "ai.h"
#include "ai_hit_reactions.h"
#include "bullet.h"
#include "commands.h"
#include "dialog.h"
#include "limits.h"
#include "offsets.h"
#include "order.h"
#include "player.h"
#include "selection.h"
#include "targeting.h"
#include "tech.h"
#include "text.h"
#include "triggers.h"
#include "unit.h"
#include "unitsearch.h"
#include "yms.h"

#include "possearch.hpp"

using std::min;
using std::get;

#define TestAssert(s) if (!(s)) { if(IsDebuggerPresent()) { INT3(); } Fail(#s); return; }

// This file has special permission for different brace style =)

static void SendCommand_CreateHotkeyGroup(uint8_t group) {
    uint8_t buf[] = { commands::Hotkey, 0, group };
    bw::SendCommand(buf, sizeof buf);
}

static void SendCommand_Select(std::initializer_list<Unit *> units) {
    Unit *arr[12];
    int pos = 0;
    for (auto unit : units) {
        arr[pos] = unit;
        pos += 1;
    }
    bw::UpdateSelectionOverlays(arr, pos);
    SendChangeSelectionCommand(pos, arr);
    *bw::client_selection_changed = 1;
}

static void SendCommand_Select(Unit *unit) {
    SendCommand_Select({ unit });
}

static void SendCommand_PlaceBuilding(Unit *unit,
                                      UnitType building,
                                      int x_tile,
                                      int y_tile,
                                      OrderType order) {

    SendCommand_Select(unit);
    uint16_t building_id = building.Raw();
    uint8_t cmd[8];
    cmd[0] = commands::Build;
    cmd[1] = order.Raw();
    memcpy(cmd + 2, &x_tile, 2);
    memcpy(cmd + 4, &y_tile, 2);
    memcpy(cmd + 6, &building_id, 2);
    bw::SendCommand(cmd, sizeof cmd);
}

static void CommandToBuild(Unit *builder, UnitType building, const Point &pos, OrderType order) {
    uint16_t x_tile = (pos.x - building.PlacementBox().width / 2) / 32;
    uint16_t y_tile = (pos.y - building.PlacementBox().height / 2) / 32;
    SendCommand_PlaceBuilding(builder, building, x_tile, y_tile, order);
}

static void SendCommand_Liftoff(Unit *unit) {
    SendCommand_Select(unit);
    uint8_t cmd[] = { commands::Lift, 0, 0, 0, 0 };
    bw::SendCommand(cmd, sizeof cmd);
}

static void SendCommand_Land(Unit *unit, int x_tile, int y_tile) {
    SendCommand_PlaceBuilding(unit, unit->Type(), x_tile, y_tile, OrderId::Land);
}

static void SendCommand_BuildAddon(Unit *unit, UnitType addon, int x_tile, int y_tile) {
    SendCommand_PlaceBuilding(unit, addon, x_tile, y_tile, OrderId::PlaceAddon);
}

static void SendCommand_RightClick(Unit *unit, const Point &pos, bool queued) {
    SendCommand_Select(unit);
    SendRightClickCommand(nullptr, pos.x, pos.y, UnitId::None, queued);
}

static void SendCommand_TargetedUnit(Unit *unit, OrderType order, Unit *target, bool queued) {
    SendCommand_Select(unit);
    Test_SendTargetedOrderCommand(order,
                                  target->sprite->position,
                                  target,
                                  UnitId::None,
                                  queued);
}

static void SendCommand_Burrow(Unit *unit) {
    SendCommand_Select(unit);
    uint8_t cmd[] = { commands::Burrow, 0 };
    bw::SendCommand(cmd, sizeof cmd);
}

static void ClearTriggers() {
    for (int i = 0; i < Limits::ActivePlayers; i++)
        bw::FreeTriggerList(&bw::triggers[i]);
}

Unit *GameTest::CreateUnitForTest(UnitType unit_id, int player) {
    return CreateUnitForTestAt(unit_id, player, Point(100, 100));
}

Unit *GameTest::CreateUnitForTestAt(UnitType unit_id, int player, const Point &point) {
    Unit *unit = bw::CreateUnit(unit_id.Raw(), point.x, point.y, player);
    Assert(unit != nullptr);
    bw::FinishUnit_Pre(unit);
    bw::FinishUnit(unit);
    bw::GiveAi(unit);
    unit->energy = unit->GetMaxEnergy();
    return unit;
}

static void ClearUnits() {
    for (Unit *unit : *bw::first_active_unit) {
        Unit *loaded = unit->first_loaded;
        while (loaded) {
            Unit *next = loaded->next_loaded;
            loaded->Remove(nullptr);
            loaded = next;
        }
        unit->Remove(nullptr);
    }
    for (Unit *unit : *bw::first_revealer) {
        unit->Remove(nullptr);
    }
}

static Unit *FindUnit(UnitType unit_id) {
    for (Unit *unit : *bw::first_active_unit) {
        if (unit->Type() == unit_id) {
            return unit;
        }
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
    for (int i = 0; i < TechId::None.Raw(); i++) {
        for (int player = 0; player < Limits::Players; player++) {
            SetTechLevel(TechType(i), player, 1);
        }
    }
}

static void GiveTech(TechType tech, int player) {
    SetTechLevel(tech, player, 1);
}

static void ClearTechs() {
    for (int i = 0; i < TechId::None.Raw(); i++) {
        for (int player = 0; player < Limits::Players; player++) {
            SetTechLevel(TechType(i), player, 0);
        }
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
                ht = CreateUnitForTest(UnitId::HighTemplar, 0);
                real = CreateUnitForTest(UnitId::Marine, 1);
                outsider = CreateUnitForTest(UnitId::Marine, 2);
                ht->IssueOrderTargetingUnit(OrderId::Hallucination, real);
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
                    hallu->IssueOrderTargetingUnit(OrderId::AttackUnit, outsider);
                    real->IssueOrderTargetingUnit(OrderId::AttackUnit, hallu);
                    outsider->IssueOrderTargetingUnit(OrderId::AttackUnit, real);
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
                defi = CreateUnitForTest(UnitId::Defiler, 0);
                target = CreateUnitForTest(UnitId::Marine, 0);
                defi->IssueOrderTargetingUnit(OrderId::Plague, target);
                state++;
            break; case 2: {
                int dmg = target->GetMaxHitPoints() - target->GetHitPoints();
                if (dmg != 0) {
                    TestAssert(target->plague_timer != 0);
                    TestAssert(defi->plague_timer == 0);
                    TestAssert(dmg == WeaponId::Plague.Damage() / (Spell::PlagueTime + 1));
                    bw::SetHp(target, 5 * 256); // Test that it doesn't kill
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
                ht1 = CreateUnitForTestAt(UnitId::HighTemplar, 0, Point(100, 100));
                auto crect = ht1->GetCollisionRect();
                ht2 = CreateUnitForTestAt(UnitId::HighTemplar, 0, Point(100 + crect.Width(), 100));
                target = CreateUnitForTestAt(UnitId::Battlecruiser, 0, Point(300, 100));
                ht1->IssueOrderTargetingUnit(OrderId::PsiStorm, target);
                state++;
            // Cases 2 and 3 should behave same, no matter if 2 storms are casted or 1
            } break; case 2: case 3: {
                int dmg = target->GetMaxHitPoints() - target->GetHitPoints();
                int storm_dmg = WeaponId::PsiStorm.Damage();
                TestAssert(dmg % storm_dmg == 0);
                if (dmg != 0) {
                    TestAssert(dmg < storm_dmg * 10);
                    bool storms_active = bullet_system->BulletCount() != 0;
                    // The 2 hts don't cast in perfect sync, which might give an extra cycle of damage
                    bool dmg_done = (dmg == storm_dmg * 8) || (state == 3 && dmg == storm_dmg * 9);
                    if (!dmg_done) {
                        TestAssert(storms_active);
                    } else if (state == 2 && !storms_active) {
                        target = CreateUnitForTestAt(UnitId::Battlecruiser, 0, Point(100, 300));
                        ht1->IssueOrderTargetingUnit(OrderId::PsiStorm, target);
                        ht2->IssueOrderTargetingUnit(OrderId::PsiStorm, target);
                        state++;
                    } else if (state == 3 && !storms_active) {
                        ht1->energy = 200 * 256;
                        ht1->IssueOrderTargetingUnit(OrderId::Hallucination, target);
                        state++;
                    }
                }
            } break; case 4:
                for (Unit *unit : *bw::first_active_unit) {
                    if (unit->flags & UnitStatus::Hallucination) {
                        target = unit;
                        target->IssueOrderTargetingGround(OrderId::Move, ht1->sprite->position);
                        state++;
                        break;
                    }
                }
            break; case 5:
                if (target->order != OrderId::Move) {
                    TestAssert(target->GetCollisionRect().top < ht2->sprite->position.y);
                    ht2->IssueOrderTargetingUnit(OrderId::PsiStorm, target);
                    storm_area = Rect16(target->sprite->position, WeaponId::PsiStorm.OuterSplash());
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
                attacker = CreateUnitForTest(UnitId::Marine, 0);
                target = CreateUnitForTest(UnitId::Zealot, 0);
                attacker->IssueOrderTargetingUnit(OrderId::AttackUnit, target);
                state++;
            break; case 2: {
                // Fail if taken hp dmg and still hasn't found overlay
                TestAssert(target->GetHitPoints() == target->GetMaxHitPoints());
                for (Image *img : target->sprite->first_overlay) {
                    if (img->image_id == ImageId::ShieldOverlay)
                        state++;
                }
            } break; case 3: {
                TestAssert(attacker->target != nullptr);
                target->shields = 0;
                for (Image *img : target->sprite->first_overlay) {
                    if (img->image_id == ImageId::ShieldOverlay)
                        return;
                }
                state++;
            } break; case 4: {
                if (attacker->target == nullptr)
                    Pass();
                else {
                    for (Image *img : target->sprite->first_overlay) {
                        TestAssert(img->image_id != ImageId::ShieldOverlay);
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
                attacker = CreateUnitForTest(UnitId::Marine, 0);
                target = CreateUnitForTest(UnitId::Zealot, 0);
                Unit *ht = CreateUnitForTest(UnitId::HighTemplar, 0);
                ht->IssueOrderTargetingUnit(OrderId::Hallucination, target);
                state = 100;
            } break; case 100: {
                for (Unit *unit : *bw::first_active_unit) {
                    if (unit->flags & UnitStatus::Hallucination) {
                        target = unit;
                        attacker->IssueOrderTargetingUnit(OrderId::AttackUnit, target);
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
                attacker = CreateUnitForTest(UnitId::Marine, 0);
                target = CreateUnitForTest(UnitId::Zealot, 0);
                attacker->IssueOrderTargetingUnit(OrderId::AttackUnit, target);
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
                flyer = CreateUnitForTest(UnitId::Scout, 0);
                lurker = CreateUnitForTest(UnitId::Lurker, 1);
                TestAssert(lurker->ai != nullptr);
                lurker->IssueOrderTargetingNothing(OrderId::Burrow);
                flyer->IssueOrderTargetingUnit(OrderId::AttackUnit, lurker);
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
                flyer = CreateUnitForTestAt(UnitId::Valkyrie, 0, Point(100, 500));
                hydra = CreateUnitForTest(UnitId::Hydralisk, 1);
                state++;
            } break; case 2: {
                if (hydra->flags & UnitStatus::Burrowed) {
                    flyer->IssueOrderTargetingGround(OrderId::Move, hydra->sprite->position);
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
                unit = CreateUnitForTest(UnitId::Overlord, *bw::local_player_id);
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
                carrier = CreateUnitForTest(UnitId::Carrier, 0);
                target = CreateUnitForTest(UnitId::Overlord, 0);
                carrier->IssueSecondaryOrder(OrderId::TrainFighter);
                carrier->build_queue[carrier->current_build_slot] = UnitId::Interceptor;
                state++;
            } break; case 2: {
                if (carrier->carrier.in_child != nullptr) {
                    interceptor = carrier->carrier.in_child;
                    carrier->IssueOrderTargetingUnit(OrderId::CarrierAttack, target);
                    state++;
                }
            } break; case 3: {
                // Test interceptor healing
                if (target->GetHitPoints() != target->GetMaxHitPoints()) {
                    interceptor->shields /= 4;
                    state++;
                }
            } break; case 4: {
                if (interceptor->order == OrderId::InterceptorReturn)
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
                marine = CreateUnitForTest(UnitId::Marine, 0);
                bunker = CreateUnitForTest(UnitId::Bunker, 0);
                enemy = CreateUnitForTest(UnitId::Zergling, 1);
                marine->IssueOrderTargetingUnit(OrderId::EnterTransport, bunker);
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
                    bunker->IssueOrderTargetingNothing(OrderId::DisableDoodad);
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
                if (FindUnit(UnitId::Marine) == nullptr) {
                    state++;
                } else {
                    TestAssert(FindUnit(UnitId::Marine)->related != nullptr);
                    TestAssert(!FindUnit(UnitId::Bunker)->IsDying());
                }
            } break; case 7: {
                // Check that unloading doesn't break anything,
                // and check that bunker's load command works.
                marine = CreateUnitForTest(UnitId::Marine, 0);
                bunker = CreateUnitForTest(UnitId::Bunker, 0);
                SendCommand_Select(bunker);
                Test_SendTargetedOrderCommand(OrderId::PickupBunker,
                                              marine->sprite->position,
                                              marine,
                                              UnitId::None,
                                              false);
                state++;
            } break; case 8: {
                if (marine->flags & UnitStatus::InBuilding) {
                    SendCommand_Select(bunker);
                    uint8_t cmd[] = {commands::UnloadAll, 0};
                    bw::SendCommand(cmd, sizeof cmd);
                    state++;
                }
            } break; case 9: {
                if (~marine->flags & UnitStatus::InBuilding) {
                    marine->IssueOrderTargetingUnit(OrderId::EnterTransport, bunker);
                    state++;
                }
            } break; case 10: {
                if (marine->flags & UnitStatus::InBuilding) {
                    Pass();
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
                arbiter = CreateUnitForTest(UnitId::Arbiter, 0);
                archon = CreateUnitForTest(UnitId::Archon, 0);
                archon->sprite->main_image->drawfunc = 1;
                archon->IssueOrderTargetingGround(OrderId::Move, Point(100, 500));
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
                vision = CreateUnitForTest(UnitId::Wraith, 0);
                arbiter = CreateUnitForTest(UnitId::Arbiter, 1);
                archon = CreateUnitForTest(UnitId::Archon, 1);
                archon->IssueOrderTargetingGround(OrderId::Move, Point(100, 500));
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
                arbiter = CreateUnitForTest(UnitId::Arbiter, 1);
                archon = CreateUnitForTest(UnitId::Archon, 1);
                archon->sprite->main_image->drawfunc = 1;
                state++;
            } break; case 8: {
                TestAssert(archon->invisibility_effects == 1);
                vision = CreateUnitForTest(UnitId::Overlord, 0);
                archon->IssueOrderTargetingGround(OrderId::Move, Point(100, 500));
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
    TechType tech;
    UnitType caster_unit_id;
    UnitType target_unit_id;
    UnitType attacker_unit_id;
};

const AiSpell revenge_spells[] = {
    { TechId::Lockdown, UnitId::Ghost, UnitId::Wraith, UnitId::None },
    // Requires detector with hp > 80
    { TechId::OpticalFlare, UnitId::Medic, UnitId::Overlord, UnitId::Marine },
    { TechId::Irradiate, UnitId::ScienceVessel, UnitId::Mutalisk, UnitId::None },
    // Requires unit which can attack the vessel and has over 200 shields
    { TechId::EmpShockwave, UnitId::ScienceVessel, UnitId::Archon, UnitId::None },
    { TechId::Ensnare, UnitId::Queen, UnitId::Wraith, UnitId::None },
    { TechId::SpawnBroodlings, UnitId::Queen, UnitId::Hydralisk, UnitId::None },
    { TechId::Plague, UnitId::Defiler, UnitId::Marine, UnitId::None },
    { TechId::PsionicStorm, UnitId::HighTemplar, UnitId::Marine, UnitId::None },
    { TechId::Feedback, UnitId::DarkArchon, UnitId::Ghost, UnitId::None },
    { TechId::Maelstrom, UnitId::DarkArchon, UnitId::Ultralisk, UnitId::None },
    { TechId::StasisField, UnitId::Arbiter, UnitId::Battlecruiser, UnitId::None },
    { TechId::DisruptionWeb, UnitId::Corsair, UnitId::Dragoon, UnitId::None },
    { TechId::None, UnitId::None, UnitId::None, UnitId::None }
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
                if (spell->tech == TechId::None) {
                    Pass();
                    return;
                }
                ClearTechs();
                spellcaster = CreateUnitForTestAt(spell->caster_unit_id, 1, spawn_pos + Point(100, 100));
                CreateUnitForTestAt(spell->target_unit_id, 0, spawn_pos);
                if (spell->attacker_unit_id != UnitId::None)
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
    UnitType unit_id;
};

static AiCloakVariation ai_cloak_variations[] = {
    { UnitId::Zealot },
    { UnitId::Goliath },
    { UnitId::Wraith },
    { UnitId::None }
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
                if (variation->unit_id == UnitId::None) {
                    state = 3;
                    return;
                }
                cloaker = CreateUnitForTestAt(UnitId::InfestedKerrigan, 1, Point(100, 100));
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
                cloaker = CreateUnitForTestAt(UnitId::InfestedKerrigan, 1, Point(100, 100));
                attacker = CreateUnitForTestAt(UnitId::Scout, 0, Point(150, 100));
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
                building = CreateUnitForTestAt(UnitId::CommandCenter, 0, Point(100, 100));
                tank = CreateUnitForTestAt(UnitId::SiegeTank_Sieged, 0, Point(200, 200));
                burrower = CreateUnitForTestAt(UnitId::Zergling, 0, Point(200, 200));
                burrower->IssueOrderTargetingNothing(OrderId::Burrow);
                building->IssueOrderTargetingNothing(OrderId::LiftOff);
                state++;
            break; case 1: {
                if (burrower->order == OrderId::Burrowed) {
                    building->IssueOrderTargetingGround(OrderId::Land, Point(100, 100));
                    state++;
                }
            } break; case 2: {
                if (building->order == OrderId::Land && building->order_state == 3)
                {
                    bw::MoveUnit(tank, 100, 100);
                    bw::MoveUnit(burrower, 100, 100);
                    state++;
                }
            } break; case 3: {
                if (UnitCount() == 1 && building->order != OrderId::Land) {
                    TestAssert(building->order == building->Type().ReturnToIdleOrder());
                    TestAssert((building->sprite->last_overlay->flags & 4) == 0);
                    // Test lifting once more
                    building->IssueOrderTargetingNothing(OrderId::LiftOff);
                    state++;
                } else {
                    TestAssert(building->order == OrderId::Land);
                }
            } break; case 4: {
                if (~building->flags & UnitStatus::Building) {
                    building->IssueOrderTargetingGround(OrderId::Land, Point(100, 100));
                    state++;
                }
            } break; case 5: {
                if (building->order == OrderId::Land)
                    state++;
            } break; case 6: {
                if (building->order != OrderId::Land) {
                    TestAssert(!building->IsFlying());
                    TestAssert((building->sprite->last_overlay->flags & 4) == 0);
                    building->IssueSecondaryOrder(OrderId::Train);
                    building->build_queue[building->current_build_slot] = UnitId::SCV;
                    state++;
                }
            } break; case 7: {
                Unit *scv = FindUnit(UnitId::SCV);
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
                building = CreateUnitForTestAt(UnitId::CommandCenter, 0, Point(100, 100));
                tank = CreateUnitForTestAt(UnitId::SiegeTankTankMode, 0, Point(100, 100));
                tank->IssueOrderTargetingNothing(OrderId::SiegeMode);
                state++;
            break; case 1: {
                if (UnitCount() == 1) {
                    building->Kill(nullptr);
                    tank = CreateUnitForTestAt(UnitId::SiegeTankTankMode, 0, Point(100, 100));
                    target = CreateUnitForTestAt(UnitId::Marine, 0, Point(250, 100));
                    tank->IssueOrderTargetingNothing(OrderId::SiegeMode);
                    state++;
                }
            } break; case 2: {
                if (tank->unit_id == UnitId::SiegeTank_Sieged && tank->order != OrderId::SiegeMode) {
                    tank->IssueOrderTargetingUnit(OrderId::WatchTarget, target);
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
                muta = CreateUnitForTestAt(UnitId::Mutalisk, 0, Point(100, 100));
                target = CreateUnitForTestAt(UnitId::Marine, 1, Point(100, 100));
                other = CreateUnitForTestAt(UnitId::Marine, 1, Point(100, 100));
                muta->IssueOrderTargetingUnit(OrderId::AttackUnit, target);
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
    UnitType next_unit_id;
    void Init() override {
        SetEnemy(1, 0);
        next_unit_id = UnitType(0);
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                if (!NoUnits())
                    return;

                NextUnitId();
                if (next_unit_id == UnitId::None) {
                    Pass();
                    return;
                }
                corsair = CreateUnitForTestAt(UnitId::Corsair, 0, Point(100, 100));
                corsair->IssueOrderTargetingGround(OrderId::DisruptionWeb, Point(150, 150));
                state++;
            break; case 1: {
                if (FindUnit(UnitId::DisruptionWeb) != nullptr) {
                    enemy = CreateUnitForTestAt(next_unit_id, 1, Point(150, 150));
                    next_unit_id = UnitType(next_unit_id.Raw() + 1);
                    state++;
                }
            } break; case 2: {
                if (FindUnit(UnitId::DisruptionWeb) == nullptr) {
                    ClearUnits();
                    state = 0;
                } else {
                    TestAssert(corsair->GetHealth() == corsair->GetMaxHealth());
                }
            }
        }
    }
    void NextUnitId() {
        while (next_unit_id != UnitId::None) {
            if (next_unit_id.Elevation() == 4 &&
                    !next_unit_id.IsSubunit() &&
                    ~next_unit_id.Flags() & UnitFlags::Air &&
                    (next_unit_id.GroupFlags() & 0x7) != 0 && // Require an race
                    (next_unit_id.AirWeapon() != WeaponId::None ||
                        (next_unit_id.Subunit() != UnitId::None &&
                         next_unit_id.Subunit().AirWeapon() != WeaponId::None)))
            {
                return;
            }
            next_unit_id = UnitType(next_unit_id.Raw() + 1);
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
                building = CreateUnitForTestAt(UnitId::CommandCenter, player, Point(100, 100));
                state++;
            } break; case 1: {
                // Screen position is rounded down to eights,
                // so MoveScreen(300, 200) would do same thing
                bw::MoveScreen(296, 200);
                frames_remaining = 50;
                SendCommand_Select(building);
                ForceRender();
                state++;
            } break; case 2: {
                Event event;
                event.ext_type = 0;
                event.type = 0xe;
                event.unk4 = 0;
                event.x = 14;
                event.y = 10;
                GameScreenRClickEvent(&event);
                state++;
            } break; case 3: {
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
                attacker = CreateUnitForTestAt(UnitId::Marine, 0, Point(100, 100));
                enemy = CreateUnitForTestAt(UnitId::Battlecruiser, 1, Point(150, 100));
                attacker->IssueOrderTargetingNothing(OrderId::HoldPosition);
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
                attacker = CreateUnitForTestAt(UnitId::Hydralisk, 0, Point(100, 100));
                enemy = CreateUnitForTestAt(UnitId::Guardian, 0, Point(300, 100));
                attacker->IssueOrderTargetingGround(OrderId::Move, Point(600, 100));
                enemy->IssueOrderTargetingGround(OrderId::Move, Point(600, 100));
                state++;
            } break; case 1: {
                if (attacker->sprite->position.x > 200 + variant) {
                    attacker->IssueOrderTargetingUnit(OrderId::AttackUnit, enemy);
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
                target = CreateUnitForTestAt(UnitId::Nexus, 0, Point(100, 150));
                attacker = CreateUnitForTestAt(UnitId::InfestedTerran, 0, Point(100, 300));
                attacker->IssueOrderTargetingUnit(OrderId::SapUnit, target);
                state++;
            } break; case 1: {
                if (target->GetHealth() != target->GetMaxHealth()) {
                    int dmg = UnitId::InfestedTerran.GroundWeapon().Damage();
                    TestAssert(target->GetMaxHealth() - target->GetHealth() > dmg * 3 / 4);
                    ClearUnits();
                    target = CreateUnitForTestAt(UnitId::SupplyDepot, 1, Point(100, 100));
                    second_target = CreateUnitForTestAt(UnitId::SupplyDepot, 1, Point(100, 100));
                    attacker = CreateUnitForTestAt(UnitId::Lurker, 0, Point(100, 150));
                    attacker->IssueOrderTargetingNothing(OrderId::Burrow);
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
                ai_ling = CreateUnitForTestAt(UnitId::Zergling, 1, Point(100, 100));
                target = CreateUnitForTestAt(UnitId::Reaver, 0, Point(400, 100));
                state++;
                frames_remaining = 300;
            } break; case 1: {
                TestAssert(ai_ling->ai != nullptr && ai_ling->ai->type == 1);
                TestAssert(ai_ling->order != OrderId::AttackUnit);
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
                da = CreateUnitForTestAt(UnitId::DarkArchon, 0, Point(100, 100));
                target = CreateUnitForTestAt(UnitId::Marine, 1, Point(100, 100));
                da->IssueOrderTargetingUnit(OrderId::MindControl, target);
                state++;
            } break; case 1: {
                if (da->order == OrderId::MindControl)
                    state++;
            } break; case 2: {
                if (target->player == da->player) {
                    TestAssert(da->shields == 0);
                    ClearUnits();
                    da = CreateUnitForTestAt(UnitId::DarkArchon, 0, Point(100, 100));
                    target = CreateUnitForTest(UnitId::Carrier, 1);
                    target->IssueSecondaryOrder(OrderId::TrainFighter);
                    target->build_queue[target->current_build_slot] = UnitId::Interceptor;
                    state++;
                } else {
                    TestAssert(da->order == OrderId::MindControl);
                }
            } break; case 3: {
                if (target->carrier.in_child != nullptr || target->carrier.out_child != nullptr) {
                    da->IssueOrderTargetingUnit(OrderId::MindControl, target);
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
                unit = CreateUnitForTestAt(UnitId::Marine, 0, Point(100, 100));
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
                unit = CreateUnitForTestAt(UnitId::Devourer, 1, Point(100, 100));
                enemy = CreateUnitForTestAt(UnitId::Scout, 0, Point(120, 120));
                state++;
            } break; case 1: {
                TestAssert(!unit->IsDying() && !enemy->IsDying());
                if (unit->target == enemy) {
                    CreateUnitForTestAt(UnitId::Scout, 0, Point(80, 80));
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
                unit = CreateUnitForTestAt(UnitId::Guardian, 0, Point(100, 100));
                enemy = CreateUnitForTestAt(UnitId::HunterKiller, 1, Point(400, 100));
                enemy2 = CreateUnitForTestAt(UnitId::HunterKiller, 1, Point(400, 100));
                unit->IssueOrderTargetingGround(OrderId::AttackMove, Point(400, 100));
                state++;
            } break; case 1: {
                bw::SetHp(unit, 100 * 256);
                bw::SetHp(enemy, 100 * 256);
                bw::SetHp(enemy2, 100 * 256);
                if (unit->target != nullptr) {
                    target = unit->target;
                    target->IssueOrderTargetingGround(OrderId::Move, Point(1000, 100));
                    state++;
                }
            } break; case 2: {
                if (target->order == OrderId::Move) {
                    state++;
                }
            } break; case 3: {
                bw::SetHp(unit, 100 * 256);
                bw::SetHp(enemy, 100 * 256);
                bw::SetHp(enemy2, 100 * 256);
                TestAssert(target->order == OrderId::Move);
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

        // In UnitId::ProgressFrames for invisible units
        const int cloak_wait = 30;

        switch (state) {
            case 0: {
                cloaked = CreateUnitForTestAt(UnitId::Observer, 0, Point(100, 100));
                detector = CreateUnitForTestAt(UnitId::Marine, 1, Point(100, 100));
                state++;
                wait = cloak_wait;
            } break; case 1: {
                TestAssert(cloaked->IsInvisibleTo(detector));
                detector->Kill(nullptr);
                detector = CreateUnitForTestAt(UnitId::Overlord, 1, Point(100, 100));
                wait = cloak_wait;
                state++;
            } break; case 2: {
                TestAssert(!cloaked->IsInvisibleTo(detector));
                detector->Kill(nullptr);
                detector = CreateUnitForTestAt(UnitId::Queen, 1, Point(100, 100));
                detector->IssueOrderTargetingGround(OrderId::Ensnare, Point(100, 100));
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
                detector = CreateUnitForTestAt(UnitId::Defiler, 1, Point(100, 100));
                detector->IssueOrderTargetingGround(OrderId::Plague, Point(100, 100));
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
                detector = CreateUnitForTestAt(UnitId::Devourer, 1, Point(100, 100));
                Unit *secondary = CreateUnitForTestAt(UnitId::Devourer, 1, Point(100, 100));
                secondary->IssueOrderTargetingUnit(OrderId::AttackUnit, detector);
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
    UnitType next_unit_id;
    void Init() override {
        // Do some fighting as well
        SetEnemy(0, 1);
        SetEnemy(1, 1);
        AiPlayer(1);
        next_unit_id = UnitType(0);
        player = 0;
    }
    void NextFrame() override {
        switch (state) {
            case 0:
                NextUnitId();
                if (next_unit_id == UnitId::None) {
                    next_unit_id = UnitType(0);
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
                next_unit_id = UnitType(next_unit_id.Raw() + 1);
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
        while (next_unit_id != UnitId::None) {
            if (!next_unit_id.IsSubunit() &&
                    next_unit_id.HitPoints() > 1 &&
                    next_unit_id.ArmorType() != 0) // Skip independent
            {
                return;
            }
            next_unit_id = UnitType(next_unit_id.Raw() + 1);
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
    void SetupNext(int queen_player, UnitType target_unit, UnitType other_unit)
    {
        ClearUnits();
        int other_player = queen_player == 0 ? 1 : 0;
        queen = CreateUnitForTestAt(UnitId::Queen, queen_player, Point(180, 100));
        target = CreateUnitForTestAt(target_unit, other_player, Point(600, 100));
        if (other_unit != UnitId::None)
            other = CreateUnitForTestAt(other_unit, other_player, Point(530, 100));
            queen->IssueOrderTargetingUnit(OrderId::Parasite, target);
        state++;
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                SetupNext(1, UnitId::Marine, UnitId::None);
            } break; case 1: {
                if (target->parasites == 0)
                    return;
                // Human owned units don't care
                TestAssert(target->target == nullptr);
                SetupNext(0, UnitId::Marine, UnitId::None);
            } break; case 2: {
                if (target->parasites == 0)
                    return;
                // Ai owned units don't care from a single parasite
                TestAssert(target->target == nullptr);
                SetupNext(0, UnitId::Marine, UnitId::Marine);
            } break; case 3: {
                if (queen->target == nullptr) {
                    queen->IssueOrderTargetingUnit(OrderId::Parasite, target);
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
                    SetupNext(0, UnitId::Goliath, UnitId::Goliath);
                }
            } break; case 4: {
                if (queen->target == nullptr) {
                    queen->IssueOrderTargetingUnit(OrderId::Parasite, target);
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
                    Unit *unit = CreateUnitForTestAt(UnitId::Marine, 1, Point(100, 100 + 20 * i));
                    Unit *enemy = CreateUnitForTestAt(UnitId::Overlord, 0, Point(120, 100 + 20 * i));
                    unit->IssueOrderTargetingUnit(OrderId::AttackUnit, enemy);
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
    OrderType order;
    UnitType caster_unit;
    UnitType target_unit;
};
const OverlaySpell overlay_spells[] = {
    { OrderId::Lockdown, UnitId::Ghost, UnitId::Goliath },
    { OrderId::Restoration, UnitId::Medic, UnitId::SiegeTankTankMode },
    { OrderId::OpticalFlare, UnitId::Medic, UnitId::Goliath },
    { OrderId::DefensiveMatrix, UnitId::ScienceVessel, UnitId::SiegeTankTankMode },
    { OrderId::Irradiate, UnitId::ScienceVessel, UnitId::Goliath, },
    { OrderId::Ensnare, UnitId::Queen, UnitId::SiegeTankTankMode },
    { OrderId::Plague, UnitId::Defiler, UnitId::Goliath },
    // Note: If feedback kills it spawns a sprite instead of a image
    { OrderId::Feedback, UnitId::DarkArchon, UnitId::Battlecruiser },
    { OrderId::Maelstrom, UnitId::DarkArchon, UnitId::Ultralisk },
    { OrderId::MindControl, UnitId::DarkArchon, UnitId::Goliath },
    { OrderId::StasisField, UnitId::Arbiter, UnitId::SiegeTankTankMode },
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
                    spellcaster->IssueOrderTargetingUnit(spell->order, target);
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
                turret = CreateUnitForTestAt(UnitId::MissileTurret, 0, Point(100, 100));
                target = CreateUnitForTestAt(UnitId::Scout, 0, Point(100, 150));
                state++;
            } break; case 1: {
                if (turret->facing_direction == 0) {
                    turret->IssueOrderTargetingUnit(OrderId::AttackUnit, target);
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
                Unit *vessel = CreateUnitForTestAt(UnitId::ScienceVessel, 0, Point(100, 100));
                target = CreateUnitForTestAt(UnitId::Scout, 0, Point(100, 150));
                vessel->IssueOrderTargetingUnit(OrderId::DefensiveMatrix, target);
                state++;
            } break; case 1: {
                if (target->matrix_timer != 0) {
                    Unit *ht = CreateUnitForTestAt(UnitId::HighTemplar, 0, Point(100, 100));
                    ht->IssueOrderTargetingUnit(OrderId::PsiStorm, target);
                    frames_remaining = 250;
                    state++;
                }
            } break; case 2: {
                int hp_dmg = target->Type().HitPoints() - target->hitpoints;
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
                    TestAssert(img->image_id != ImageId::ShieldOverlay);
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
                Unit *first = nullptr;
                if (state == 0)
                    first = CreateUnitForTestAt(UnitId::Marine, 0, positions[0]);
                Unit *second = CreateUnitForTestAt(UnitId::Marine, 0, positions[1]);
                if (state == 1)
                    first = CreateUnitForTestAt(UnitId::Marine, 0, positions[0]);
                Unit *unit = CreateUnitForTestAt(UnitId::Zergling, 1, Point(300, 100));
                first->IssueOrderTargetingUnit(OrderId::AttackUnit, unit);
                second->IssueOrderTargetingUnit(OrderId::AttackUnit, unit);
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
                first = CreateUnitForTestAt(UnitId::Guardian, 0, positions[0]);
                Unit *second = CreateUnitForTestAt(UnitId::Zergling, 0, positions[1]);
                Unit *unit = CreateUnitForTestAt(UnitId::Marine, 1, Point(350, 100));
                first->IssueOrderTargetingUnit(OrderId::AttackUnit, unit);
                second->IssueOrderTargetingUnit(OrderId::AttackUnit, unit);
                TestAssert(Ai::GetBestTarget(unit, { first, second }) == second);
                TestAssert(Ai::GetBestTarget(unit, { second, first }) == first);
                ClearUnits();
                state++;
            } break; case 3: {
                /// Test some of the internal logic
                Unit *ai = CreateUnitForTestAt(UnitId::Zergling, 1, Point(100, 100));
                Ai::UpdateAttackTargetContext uat(ai, false, false);
                Ai::UpdateAttackTargetContext allowing_critters(ai, true, false);
                // The unit is not targeting ai's units
                // So other fails and other succeeds
                Unit *other = CreateUnitForTestAt(UnitId::Marine, 0, Point(100, 100));
                TestAssert(other->target == nullptr);
                TestAssert(uat.CheckPreviousAttackerValid(other) == nullptr);
                TestAssert(uat.CheckValid(other) == other);
                // The unit is targeting ai's units
                other = CreateUnitForTestAt(UnitId::Marine, 0, Point(100, 100));
                other->IssueOrderTargetingUnit(OrderId::AttackUnit, ai);
                TestAssert(other->target == ai);
                TestAssert(uat.CheckPreviousAttackerValid(other) == other);
                TestAssert(uat.CheckValid(other) == other);
                // Can't attack that unit
                other = CreateUnitForTestAt(UnitId::Wraith, 0, Point(100, 100));
                other->IssueOrderTargetingUnit(OrderId::AttackUnit, ai);
                TestAssert(other->target == ai);
                TestAssert(uat.CheckPreviousAttackerValid(other) == nullptr);
                TestAssert(uat.CheckValid(other) == nullptr);
                // Test critter bool
                other = CreateUnitForTestAt(UnitId::Bengalaas, 0, Point(100, 100));
                TestAssert(uat.CheckValid(other) == nullptr);
                TestAssert(allowing_critters.CheckValid(other) == other);
                TestAssert(allowing_critters.CheckPreviousAttackerValid(other) == nullptr);
                Pass();
            }
        }
    }
};

struct TransmissionTest {
    UnitType unit_id;
    Point pos;
    bool ok;
};
// Well, these variants mostly test finding specific unit at a location but that's nice too
const TransmissionTest transmission_tests[] = {
    { UnitId::Marine, Point(100, 100), false },
    { UnitId::Zergling, Point(100, 100), true },
    { UnitId::Zergling, Point(400, 100), false },
};

struct Test_Transmission : public GameTest {
    Unit *unit;
    const TransmissionTest *variant;
    void Init() override {
        bw::locations[0] = Location { Rect32(50, 50, 150, 150), 0, 0 };
        variant = transmission_tests;
    }
    void NextFrame() override {
        // May be initialized incorrectly, as the structure is incomplete
        Trigger trigger;
        memset(&trigger, 0, sizeof(Trigger));
        trigger.actions[0].location = 1;
        trigger.actions[0].amount = 7;
        trigger.actions[0].misc = 500;
        trigger.actions[0].unit_id = UnitId::Zergling;
        trigger.actions[0].sound_id = 0;
        trigger.actions[0].action_id = 0x7;
        switch (state) {
            case 0: {
                unit = CreateUnitForTestAt(variant->unit_id, 0, variant->pos);
                *bw::current_trigger = &trigger;
                *bw::trigger_current_player = 0;
                bw::ProgressActions(&trigger);
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
        CreateUnitForTestAt(UnitId::Nexus, player, pos);
        helper = CreateUnitForTestAt(UnitId::Probe, player, pos + Point(0, 100));
        Unit *mineral = CreateUnitForTestAt(UnitId::MineralPatch1, NeutralPlayer, pos + Point(0, 200));
        mineral->resource.resource_amount = 1500;
        bw::AiScript_StartTown(pos.x, pos.y, 1, player);
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                CreateAiTown(Point(100, 100), 1);
                target = CreateUnitForTestAt(UnitId::Carrier, 1, Point(100, 100));
                enemy = CreateUnitForTestAt(UnitId::Hydralisk, 0, Point(100, 600));
                target->IssueSecondaryOrder(OrderId::TrainFighter);
                target->build_queue[target->current_build_slot] = UnitId::Interceptor;
                state++;
            } break; case 1: {
                if (target->carrier.in_child != nullptr) {
                    enemy->IssueOrderTargetingGround(OrderId::Move, Point(100, 100));
                    state++;
                }
            } break; case 2: {
                bw::SetHp(target, target->GetMaxHitPoints() * 256);
                bw::SetHp(enemy, enemy->GetMaxHitPoints() * 256);
                if (target->carrier.out_child != nullptr) {
                    enemy->IssueOrderTargetingUnit(OrderId::AttackUnit, target->carrier.out_child);
                    state++;
                }
            } break; case 3: {
                bw::SetHp(target, target->GetMaxHitPoints() * 256);
                bw::SetHp(enemy, enemy->GetMaxHitPoints() * 256);
                TestAssert(helper->target != enemy);
                if (enemy->target == nullptr || enemy->target->unit_id != UnitId::Interceptor) {
                    enemy->IssueOrderTargetingGround(OrderId::Move, Point(100, 100));
                    state--;
                } else if (enemy->target->GetHealth() != enemy->target->GetMaxHealth()) {
                    // This test makes only sense if interceptors have no ai
                    TestAssert(enemy->target->ai == nullptr);
                    if (bw::IsInArea(enemy->target, CallFriends_Radius, helper)) {
                        frames_remaining = 50;
                        state++;
                    }
                }
            } break; case 4: {
                TestAssert(helper->target != enemy);
                if (frames_remaining == 1) {
                    enemy->Kill(nullptr);
                    frames_remaining = 5000;
                    enemy = CreateUnitForTestAt(UnitId::SCV, 0, Point(100, 500));
                    target = FindUnit(UnitId::Nexus);
                    enemy->IssueOrderTargetingUnit(OrderId::AttackUnit, target);
                    state++;
                }
            } break; case 5: {
                bw::SetHp(target, target->GetMaxHitPoints() * 256);
                bw::SetHp(enemy, enemy->GetMaxHitPoints() * 256);
                if (enemy->target->GetHealth() != enemy->target->GetMaxHealth()) {
                    if (bw::IsInArea(enemy->target, CallFriends_Radius, helper)) {
                        frames_remaining = 50;
                        state++;
                    }
                }
            } break; case 6: {
                bw::SetHp(target, target->GetMaxHitPoints() * 256);
                if (frames_remaining == 1) {
                    TestAssert(helper->target == enemy);
                    Pass();
                }
            }
        }
    }
};

/// Checks that probes path correctly between two gateways.
/// Bw has a pathing issue when a flingy.dat movement unit (probe) can't
/// turn sharply enough to get in a gap between two units (gateways),
/// even if the path planned to go there and the unit would fit. Bw would
/// make the probe to dodge one of the gateways, as the flingy momementum
/// "threw" the probe into gateway and it thought it was in way. This
/// dodging would sometimes choose a suboptimal path around the gateway,
/// sometimes even causing the probe to get completely stuck if there were
/// even more obstacles that made dodging the gateway difficult.
struct Test_PathingFlingyGap : public GameTest {
    int variant;
    Unit *probe;
    // If the probe goes either past the gap or to wrong direction from start,
    // something went wrong.
    int y_min;
    int y_max;
    void Init() override {
        variant = 0;
        // Create 5 gateways and 3 probes to block the path between 3 highest
        // and 2 lowest (Just so that the fastest path goes clearly between the
        // gateways -- for some reason it likes to path above more than below,
        // but that is not something this test is going to check/fix).
        for (auto i = 0; i < 5; i++) {
            CreateUnitForTestAt(UnitId::Gateway, 0, Point(0x100, 0x30 + i * 0x60));
        }
        CreateUnitForTestAt(UnitId::Probe, 0, Point(0x100, 0x64));
        CreateUnitForTestAt(UnitId::Probe, 0, Point(0x100, 0xc4));
        CreateUnitForTestAt(UnitId::Probe, 0, Point(0x100, 0x184));
    }

    void NextFrame() override {
        switch (state) {
            case 0: {
                bool left = variant == 0 || variant == 1;
                bool top = variant == 1 || variant == 3;
                int x;
                int y;
                int target_x;
                int target_y;
                const auto probe_dbox = UnitId::Probe.DimensionBox();
                const auto gateway_dbox = UnitId::Gateway.DimensionBox();
                if (top) {
                    y = 0x120 - gateway_dbox.top - gateway_dbox.bottom;
                    y_min = y - 16;
                    y_max = 0xf0 + gateway_dbox.bottom + 1 + probe_dbox.top + 16;
                } else {
                    y = 0x120 + gateway_dbox.top + gateway_dbox.bottom;
                    y_max = y + 16;
                    y_min = 0xf0 + gateway_dbox.bottom + 1 + probe_dbox.top - 16;
                }
                target_y = y;
                if (left) {
                    x = 0x100 - gateway_dbox.left - probe_dbox.right - 1;
                    target_x = 0x100 + gateway_dbox.right + probe_dbox.left + 2;
                } else {
                    x = 0x100 + gateway_dbox.right + probe_dbox.left + 1;
                    target_x = 0x100 - gateway_dbox.left - probe_dbox.right - 2;
                }
                probe = CreateUnitForTestAt(UnitId::Probe, 0, Point(x, y));
                probe->IssueOrderTargetingGround(OrderId::Move, Point(target_x, target_y));
                state++;
            } break; case 1: {
                const auto &pos = probe->sprite->position;
                TestAssert(pos.y <= y_max);
                TestAssert(pos.y >= y_min);
                if (pos.x > 0xf0 && pos.x < 0x110) {
                    // It should not be dodging either of the gateways
                    TestAssert(probe->path != nullptr);
                    TestAssert(probe->path->dodge_unit == nullptr);

                    probe->Kill(nullptr);
                    state = 0;
                    variant++;
                    if (variant == 4) {
                        Pass();
                    }
                }
            }
        }
    }
};

struct Test_RallyPoint : public GameTest {
    Unit *unit;
    Point old_rally;
    void Init() override {
        unit = nullptr;
    }
    void Rally() {
        old_rally = unit->rally.position;
        SendCommand_Select(unit);
        Test_SendTargetedOrderCommand(OrderId::RallyPointTile, Point(50, 50), nullptr, UnitId::None, false);
        frames_remaining = 50;
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                unit = CreateUnitForTestAt(UnitId::Nexus, 0, Point(100, 100));
                Rally();
                state++;
            } break; case 1: {
                if (unit->rally.position != old_rally) {
                    // Shouldn't be able to change pylon rally.
                    unit = CreateUnitForTestAt(UnitId::Pylon, 0, Point(200, 100));
                    Rally();
                    state++;
                }
            } break; case 2: {
                Assert(unit->rally.position == old_rally);
                if (frames_remaining == 1) {
                    // Shouldn't be able to change marine rally.
                    unit = CreateUnitForTestAt(UnitId::Marine, 0, Point(300, 100));
                    Rally();
                    state++;
                }
            } break; case 3: {
                Assert(unit->rally.position == old_rally);
                if (frames_remaining == 1) {
                    Pass();
                }
            }
        }
    }
};

struct Test_Extractor : public GameTest {
    void Init() override {
        bw::minerals[0] = 50;
        bw::gas[0] = 0;
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                auto gas_pos = Point(0x100, 0x100);
                Unit *drone = CreateUnitForTestAt(UnitId::Drone, 0, Point(100, 100));
                CreateUnitForTestAt(UnitId::VespeneGeyser, NeutralPlayer, gas_pos);
                CommandToBuild(drone, UnitId::Extractor, gas_pos, OrderId::DroneStartBuild);
                state++;
            } break; case 1: {
                if (FindUnit(UnitId::Drone) == nullptr) {
                    TestAssert(FindUnit(UnitId::VespeneGeyser) == nullptr);
                    TestAssert(FindUnit(UnitId::Extractor) != nullptr);
                    TestAssert(bw::minerals[0] == 0);
                    SendCommand_Select(FindUnit(UnitId::Extractor));
                    uint8_t cmd[] = {commands::CancelMorph};
                    bw::SendCommand(cmd, sizeof cmd);
                    state++;
                }
            } break; case 2: {
                if (FindUnit(UnitId::Drone) != nullptr) {
                    TestAssert(FindUnit(UnitId::VespeneGeyser) == nullptr);
                    TestAssert(FindUnit(UnitId::Extractor) != nullptr);
                    TestAssert(bw::minerals[0] == 37);
                    Pass();
                }
            }
        }
    }
};

struct Test_CritterExplosion : public GameTest {
    void Init() override {
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                Unit *critter = CreateUnitForTestAt(UnitId::Kakaru, 0, Point(100, 100));
                SendCommand_Select(critter);
                SendCommand_CreateHotkeyGroup(1);
                state++;
            } break; case 1: {
                if (UnitCount() == 0) {
                    Pass();
                } else {
                    SelectHotkeyGroup(1);
                    *bw::selection_sound_cooldown = 0;
                    bw::ToggleSound();
                    bw::ToggleSound();
                }
            }
        }
    }
};

struct Test_BuildingLandDeath : public GameTest {
    Unit *building;
    void Init() override {
    }
    void NextFrame() override {
        auto BuildingFlag = [](auto x_tile, auto y_tile) {
            return (*bw::map_tile_flags)[*bw::map_width_tiles * y_tile + x_tile] & 0x08000000;
        };
        switch (state) {
            case 0: {
                building = CreateUnitForTestAt(UnitId::CommandCenter, 0, Point(0x60, 0x50));
                building->IssueOrderTargetingNothing(OrderId::LiftOff);
                state++;
            } break; case 1: {
                if (building->order != OrderId::LiftOff) {
                    TestAssert(!BuildingFlag(1, 1));
                    building->IssueOrderTargetingGround(OrderId::Land, Point(0x60, 0x50));
                    state++;
                }
            } break; case 2: {
                if (building->building.is_landing)
                {
                    TestAssert(BuildingFlag(1, 1));
                    TestAssert(((*bw::map_tile_flags)[*bw::map_width_tiles + 1] & 0x08000000) != 0);
                    building->Kill(nullptr);
                    state++;
                }
                else
                {
                    TestAssert(!BuildingFlag(1, 1));
                }
            } break; case 3: {
                if (UnitCount() == 0) {
                    TestAssert(!BuildingFlag(1, 1));
                    Pass();
                }
            }
        }
    }
};

struct Test_BuildingLandQueuing : public GameTest {
    Unit *building;
    void Init() override {
        bw::minerals[0] = 50;
        bw::gas[0] = 50;
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                building = CreateUnitForTestAt(UnitId::CommandCenter, 0, Point(0x60, 0x50));
                SendCommand_Liftoff(building);
                state++;
            } break; case 1: {
                if (building->order != OrderId::LiftOff && ~building->flags & UnitStatus::Building) {
                    SendCommand_Land(building, 5, 5);
                    SendCommand_RightClick(building, Point(200, 200), true);
                    state++;
                }
            } break; case 2: {
                if (building->order == OrderId::Land) {
                    CreateUnitForTestAt(UnitId::Marine, 0, Point(5 * 32, 5 * 32));
                    state++;
                }
            } break; case 3: {
                if (building->order == OrderId::Move) {
                    state++;
                }
            } break; case 4: {
                if (building->order != OrderId::Move) {
                    TestAssert(building->sprite->position == Point(200, 200));
                    ClearUnits();
                    state++;
                }
            } break; case 5: {
                building = CreateUnitForTestAt(UnitId::CommandCenter, 0, Point(0x60, 0x50));
                CreateUnitForTestAt(UnitId::Academy, 0, Point(0x160, 0x50));
                SendCommand_Liftoff(building);
                state++;
            } break; case 6: {
                if (building->order != OrderId::LiftOff && ~building->flags & UnitStatus::Building) {
                    SendCommand_BuildAddon(building, UnitId::ComsatStation, 9, 6);
                    SendCommand_RightClick(building, Point(200, 200), true);
                    state++;
                }
            } break; case 7: {
                if (building->order == OrderId::Land) {
                    // Queuing a order before the land order is issued causes it to be lost.
                    // (Maybe should just change the behaviour here?)
                    TestAssert(building->order_queue_begin != nullptr);
                    TestAssert(building->order_queue_begin->Type() == OrderId::PlaceAddon);
                    TestAssert(building->order_queue_end == building->order_queue_begin);
                    ClearUnits();
                    state++;
                }
            } break; case 8: {
                building = CreateUnitForTestAt(UnitId::CommandCenter, 0, Point(0x60, 0x50));
                CreateUnitForTestAt(UnitId::Academy, 0, Point(0x160, 0x50));
                SendCommand_Liftoff(building);
                state++;
            } break; case 9: {
                if (building->order != OrderId::LiftOff && ~building->flags & UnitStatus::Building) {
                    SendCommand_BuildAddon(building, UnitId::ComsatStation, 9, 6);
                    state++;
                }
            } break; case 10: {
                if (building->order == OrderId::Land) {
                    // This is supposed get queued and executed after the landing fails.
                    SendCommand_RightClick(building, Point(200, 200), true);
                    CreateUnitForTestAt(UnitId::Marine, 0, Point(5 * 32, 5 * 32));
                    state++;
                }
            } break; case 11: {
                if (building->order == OrderId::Move) {
                    state++;
                }
            } break; case 12: {
                if (building->order != OrderId::Move) {
                    TestAssert(building->sprite->position == Point(200, 200));
                    TestAssert(FindUnit(UnitId::ComsatStation) == nullptr);
                    Pass();
                }
            }
        }
    }
};

struct Test_Irradiate : public GameTest {
    Unit *vessel;
    Unit *target;
    Unit *other;
    Unit *goon;
    uint32_t target_uid;
    void Init() override {
        bw::minerals[0] = 50;
        bw::gas[0] = 50;
    }
    void NextFrame() override {
        switch (state) {
            case 0: {
                vessel = CreateUnitForTestAt(UnitId::ScienceVessel, 0, Point(100, 100));
                target = CreateUnitForTestAt(UnitId::Lurker, 0, Point(100, 100));
                target_uid = target->lookup_id;
                SendCommand_TargetedUnit(vessel, OrderId::Irradiate, target, false);
                state++;
            } break; case 1: {
                // It can't do too much damage at once
                if (target->GetHealth() > 50 && target->GetHealth() < 55) {
                    state++;
                }
            } break; case 2: {
                // But it'll kill the target eventually.
                if (target->IsDying()) {
                    // Test for taking damage from another unit's irra.
                    target = CreateUnitForTestAt(UnitId::Marine, 0, Point(100, 100));
                    target_uid = target->lookup_id;
                    other = CreateUnitForTestAt(UnitId::ScienceVessel, 0, Point(100, 100));
                    vessel->energy = 150 * 256;
                    SendCommand_TargetedUnit(vessel, OrderId::Irradiate, other, false);
                    state++;
                }
            } break; case 3: {
                if (target->IsDying()) {
                    // Units shouldn't take damage if they're inside an irradiated dropship.
                    target = CreateUnitForTestAt(UnitId::Marine, 0, Point(400, 100));
                    target_uid = target->lookup_id;
                    other = CreateUnitForTestAt(UnitId::Dropship, 0, Point(400, 100));
                    vessel->energy = 150 * 256;
                    SendCommand_TargetedUnit(vessel, OrderId::Irradiate, other, false);
                    SendCommand_TargetedUnit(target, OrderId::EnterTransport, other, false);
                    state++;
                }
            } break; case 4: {
                TestAssert(Unit::FindById(target_uid) != nullptr);
                if (other->irradiate_timer != 0) {
                    TestAssert(other->irradiated_by == vessel);
                    TestAssert(other->irradiate_player == vessel->player);
                    state++;
                }
            } break; case 5: {
                TestAssert(Unit::FindById(target_uid) != nullptr);
                if (other->irradiate_timer == 0) {
                    // Irradiated units inside dropship should die though.
                    target = CreateUnitForTestAt(UnitId::Marine, 0, Point(600, 100));
                    target_uid = target->lookup_id;
                    other = CreateUnitForTestAt(UnitId::Dropship, 0, Point(600, 100));
                    vessel->energy = 150 * 256;
                    SendCommand_TargetedUnit(vessel, OrderId::Irradiate, target, false);
                    state++;
                }
            } break; case 6: {
                if (target->irradiate_timer != 0) {
                    SendCommand_TargetedUnit(target, OrderId::EnterTransport, other, false);
                    state++;
                }
            } break; case 7: {
                if (Unit::FindById(target_uid) == nullptr) {
                    // Irradiate deals damage to other loaded units.
                    target = CreateUnitForTestAt(UnitId::Marine, 0, Point(800, 100));
                    target_uid = target->lookup_id;
                    other = CreateUnitForTestAt(UnitId::Dropship, 0, Point(800, 100));
                    goon = CreateUnitForTestAt(UnitId::Dragoon, 0, Point(800, 100));
                    vessel->energy = 150 * 256;
                    SendCommand_TargetedUnit(vessel, OrderId::Irradiate, goon, false);
                    SendCommand_TargetedUnit(target, OrderId::EnterTransport, other, false);
                    state++;
                }
            } break; case 8: {
                TestAssert(Unit::FindById(target_uid) != nullptr);
                if (goon->irradiate_timer != 0) {
                    SendCommand_TargetedUnit(goon, OrderId::EnterTransport, other, false);
                    state++;
                }
            } break; case 9: {
                if (Unit::FindById(target_uid) == nullptr) {
                    // No splash when the unit is burrowed
                    target = CreateUnitForTestAt(UnitId::Marine, 0, Point(1000, 100));
                    target_uid = target->lookup_id;
                    other = CreateUnitForTestAt(UnitId::Lurker, 0, Point(1000, 100));
                    vessel->energy = 150 * 256;
                    SendCommand_TargetedUnit(vessel, OrderId::Irradiate, other, false);
                    SendCommand_Burrow(other);
                    state++;
                }
            } break; case 10: {
                TestAssert(!target->IsDying())
                SendCommand_RightClick(target, other->sprite->position, false);
                if (other->IsDying()) {
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
    AddTest("Pathing small gap w/ flingy movement", new Test_PathingFlingyGap);
    AddTest("Rally point", new Test_RallyPoint);
    AddTest("Morph extractor", new Test_Extractor);
    AddTest("Critter explosion", new Test_CritterExplosion);
    AddTest("Building land death", new Test_BuildingLandDeath);
    AddTest("Building land queuing", new Test_BuildingLandQueuing);
    AddTest("Irradiate", new Test_Irradiate);
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
