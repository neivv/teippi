#include "game.h"

#include "yms.h"
#include "unit.h"
#include "bullet.h"
#include "flingy.h"
#include "sprite.h"
#include "ai.h"
#include "triggers.h"
#include "perfclock.h"
#include "log.h"
#include "pathing.h"
#include "commands.h"
#include "image.h"
#include "order.h"
#include "replay.h"
#include "rng.h"
#include "sync.h"
#include "commands.h"
#include "dialog.h"
#include "test_game.h"

#include "console/windows_wrap.h"

unsigned int render_wait = 0;
static bool force_render;
float fps;
const bool dont_pause_on_alttab = true;
bool all_visions = false;
// Hack to reduce amount of unnecessarily hooked code, externed only in limits.cpp
bool unitframes_in_progress = false;

void ForceRender()
{
    force_render = true;
}

GameTests *game_tests = nullptr;
Score *score = nullptr;

static Optional<SyncData> old_sync;

static void ProgressAi()
{
    Ai::ProgressScripts();
    bw::Ai_ProgressRegions();
    bw::UpdateResourceAreas();
    bw::Ai_Unk_004A2A40();
}

static void DumpUnits()
{
    if (!unit_dump || *bw::frame_count % 1000 == 0)
    {
        delete unit_dump;
        char filename[256];
        sprintf(filename, "%s\\dump\\units_%d.txt", log_path, *bw::frame_count);
        unit_dump = new DebugLog_Actual(filename);
        unit_dump->AutoFlush(false);
    }
    SyncData sync;
    if (old_sync)
    {
        sync.WriteDiff(old_sync.take(), SyncDumper(unit_dump));
    }
    else
    {
        sync.WriteDiff(SyncData(false), SyncDumper(unit_dump));
    }
    unit_dump->Log("Tw %x, cd %x, W:", *bw::trigger_cycle_count, *bw::countdown_timer);
    for (int i = 0; i < Limits::ActivePlayers; i++)
    {
        unit_dump->Log(" %x", bw::player_waits[i]);
    }
    unit_dump->Log("\n");
    unit_dump->Flush();
    old_sync = move(sync);
}

static uint32_t HashSprite(const Sprite *sprite)
{
    uint32_t hash = sprite->sprite_id << 16 | sprite->player << 8 | sprite->visibility_mask;
    hash ^= sprite->elevation;
    hash ^= sprite->width << 24;
    hash ^= sprite->height << 16;
    hash ^= sprite->position.x << 8;
    hash ^= sprite->position.y;
    return hash;
}

static void LogSync(const SyncHashes *hashes)
{
    sync_log->Log("%08X %08X %08X %08X %08X %08X %08X %08X %08X %08X\n", hashes->main_hash, *bw::rng_seed,
            hashes->units_hash, hashes->bullets_hash, hashes->unit_sprites_hash, hashes->bullet_sprites_hash,
            hashes->paths_hash, hashes->ai_region_hash, hashes->ai_hash, hashes->trigger_hash);

    DumpUnits();
}

static SyncHashes GetSyncHashes()
{
    uint32_t units_hash = 0, bullets_hash = 0, paths_hash = 0, ai_region_hash = 0, ai_hash = 0;
    uint32_t unit_sprites_hash = 0, bullet_sprites_hash = 0, trigger_hash = 0;
    for (Unit *unit : first_allocated_unit)
    {
        if (unit->sprite && !unit->IsDying())
        {
            units_hash ^= unit->sprite->position.AsDword() << 16 | unit->move_target.AsDword();
            units_hash ^= unit->order_target_pos.AsDword();
            units_hash ^= (unit->order | unit->facing_direction << 8 | unit->movement_direction << 16 | unit->flingy_flags << 24 | unit->movement_state << 4) * 0x12345678;
            units_hash ^= (unit->flags ^ unit->hitpoints);
            units_hash ^= unit->shields;
            units_hash ^= unit->energy << 16 | unit->invisibility_effects << 8 | unit->move_target_update_timer << 12 | unit->sprite->visibility_mask;
            units_hash ^= unit->ground_strength << 16 | unit->air_strength;
            if (unit->target)
                units_hash ^= unit->target->lookup_id;
            if (unit->previous_attacker)
                units_hash ^= unit->previous_attacker->lookup_id;
            if (unit->path)
            {
                paths_hash ^= unit->path->start.AsDword() | unit->path->next_pos.AsDword() | unit->path->end.AsDword();
                paths_hash ^= unit->path->flags << 16;
                unsigned int pos = 0;
                for (int i = 0; i < unit->path->position_count && pos < sizeof unit->path->values; i++, pos += 2)
                    paths_hash ^= unit->path->values[pos] | unit->path->values[pos + 1] << 8;
                for (int i = 0; i < unit->path->unk1c && pos < sizeof unit->path->values; i++, pos += 1)
                    paths_hash ^= unit->path->values[pos] << 16;
                paths_hash = (paths_hash << 1) | (paths_hash >> 31);
            }
            unit_sprites_hash ^= HashSprite(unit->sprite.get());
        }
        units_hash = (units_hash << 1) | (units_hash >> 31);
        unit_sprites_hash = (unit_sprites_hash << 1) | (unit_sprites_hash >> 31);
    }
    for (Bullet *bullet : bullet_system->ActiveBullets())
    {
        if (bullet->sprite)
        {
            bullets_hash ^= bullet->sprite->position.AsDword();
            bullet_sprites_hash ^= HashSprite(bullet->sprite.get());
        }
        bullets_hash = (bullets_hash << 1) | (bullets_hash >> 31);
        bullet_sprites_hash = (bullet_sprites_hash << 1) | (bullet_sprites_hash >> 31);
    }
    for (Ai::Region *region : Ai::GetRegions())
    {
        ai_region_hash ^= (region->target_region_id << 16) | region->flags;
        ai_region_hash ^= (region->state << 24) | region->ground_unit_count;
        ai_region_hash ^= (region->needed_ground_strength << 16) | region->needed_air_strength;
        ai_region_hash ^= (region->enemy_air_strength << 16) | region->enemy_ground_strength;
        ai_region_hash = (ai_region_hash << 1) | (ai_region_hash >> 31);
    }
    for (int i = 0; i < Limits::ActivePlayers; i++)
    {
        if (IsComputerPlayer(i))
        {
            for (int j = 0; j < bw::player_ai[i].request_count; j++)
            {
                auto &req = bw::player_ai[i].requests[j];
                ai_hash ^= req.priority << 24 | req.type << 16 | req.unit_id;
            }
        }
        trigger_hash ^= bw::player_waits[i];
        trigger_hash = (trigger_hash << 4) | (trigger_hash >> 28);
    }
    trigger_hash = *bw::trigger_cycle_count;
    trigger_hash ^= *bw::countdown_timer;
    uint32_t main_hash = *bw::rng_seed;
    main_hash = (main_hash << 5) | (main_hash >> 27);
    main_hash ^= units_hash;
    main_hash = (main_hash << 5) | (main_hash >> 27);
    main_hash ^= bullets_hash;
    main_hash = (main_hash << 5) | (main_hash >> 27);
    main_hash ^= unit_sprites_hash;
    main_hash = (main_hash << 5) | (main_hash >> 27);
    main_hash ^= bullet_sprites_hash;
    main_hash = (main_hash << 5) | (main_hash >> 27);
    main_hash ^= ai_region_hash;
    main_hash = (main_hash << 5) | (main_hash >> 27);
    main_hash ^= ai_hash;
    main_hash = (main_hash << 5) | (main_hash >> 27);
    main_hash ^= trigger_hash;
    return SyncHashes(main_hash, units_hash, bullets_hash, unit_sprites_hash, bullet_sprites_hash, paths_hash, ai_region_hash, ai_hash, trigger_hash);
}

void ProgressObjects()
{
    PerfClock clock;
    perf_log->Indent(2);

    EnableRng(true);
    bw::TryUpdateCreepDisappear();
    ProgressAi();

    if (*bw::vision_update_count == 0)
        *bw::vision_update_count = 100;

    *bw::vision_updated = *bw::vision_update_count == 1;
    *bw::vision_update_count -= 1;
    if (*bw::vision_updated)
    {
        bw::ProgressCreepDisappearance();
        bw::UpdateFog();
    }

    auto pre_time = clock.GetTime();
    auto total_time = pre_time;
    unitframes_in_progress = true;
    auto unit_results = Unit::ProgressFrames();
    unitframes_in_progress = false;
    auto unit_time = clock.GetTime() - total_time;
    total_time += unit_time;
    BulletFramesInput bullet_input(move(unit_results.weapon_damages), move(unit_results.hallucination_hits),
            move(unit_results.ai_hit_reactions));
    bullet_system->ProgressFrames(move(bullet_input));
    auto bullet_time = clock.GetTime() - total_time;
    total_time += bullet_time;
    Flingy::ProgressFrames();
    lone_sprites->ProgressFrames();

    EnableRng(false);
    auto rest_time = clock.GetTime() - total_time;
    perf_log->Indent(-2);
    perf_log->Log("ProgressObjects: Pre %f ms + rest %f ms (+ units + bullets) = about %f ms\n", pre_time, rest_time, clock.GetTime());
}

inline void SetFrameState(int state)
{
    *bw::unk_frame_state = state;
}

static void CheckUnitAi()
{
    for (Unit *unit : *bw::first_active_unit)
    {
        if (unit->ai != nullptr)
        {
            switch (unit->ai->type)
            {
                case 1:
                    Assert(((Ai::GuardAi *)unit->ai)->parent == unit);
                break;
                case 2:
                    Assert(((Ai::WorkerAi *)unit->ai)->parent == unit);
                break;
                case 3:
                    Assert(((Ai::BuildingAi *)unit->ai)->parent == unit);
                break;
                case 4:
                    Assert(((Ai::MilitaryAi *)unit->ai)->parent == unit);
                break;
            }
        }
    }
}

static bool ProgressFrame()
{
    uint32_t lag_tick = GetTickCount() + 2000;
    uint32_t frames_remaining = *bw::frames_progressed_at_once;
    int objects_progressed = 0;
    SetFrameState(0);

    while (frames_remaining--)
    {
        if (IsReplay())
            ProgressReplay();

        uint32_t unk_sync;
        if (!bw::ProgressTurns(&unk_sync))
        {
            SetFrameState(1);
            break;
        }

        if (IsReplay())
        {
            if (*bw::replay_paused || false) // arg ecx is always 0
            {
                bw::Replay_RefershUiIfNeeded();
                SetFrameState(2);
                break;
            }
        }

        if (!*bw::draw_sprites)
        {
            SetFrameState(4);
            break;
        }

        if ((dont_pause_on_alttab && bw::game_speed_waits[*bw::game_speed] < 2) || IsMultiplayer() || *bw::window_active)
        {
            *bw::image_flags |= 0x2;
            if (IsPaused())
            {
                bw::DrawFlashingSelectionCircles();
            }
            else
            {
                (*bw::frame_count)++;
                if (Debug)
                    CheckUnitAi();
                if (Debug && game_tests)
                    game_tests->NextFrame();

                objects_progressed++;
                ProgressObjects();

                auto hashes = GetSyncHashes();
                *bw::sync_hash = hashes.main_hash >> (8 * (*bw::frame_count & 0x3));
                if (SyncTest)
                    LogSync(&hashes);
            }
        }
        EnableRng(true);
        ProgressTriggers();
        EnableRng(false);

        if (IsReplay())
            bw::Replay_RefershUiIfNeeded();

        *bw::next_frame_tick += bw::game_speed_waits[*bw::game_speed];
        uint32_t tick = GetTickCount();
        if (tick < *bw::next_frame_tick)
        {
            SetFrameState(5);
            break;
        }
        if (lag_tick < tick)
        {
            SetFrameState(6);
            break;
        }
        if (unk_sync)
        {
            SetFrameState(7);
            break;
        }
    }
    return objects_progressed;
}

int ProgressFrames()
{
    static unsigned int last_tick;
    static int fps_count;
    int objects_progressed = 0;

    unsigned int new_tick = GetTickCount();
    if (new_tick - last_tick > 500)
    {
        fps = 1000.0f / (new_tick - last_tick) * fps_count;
        fps_count = 0;
        last_tick = new_tick;
    }

    do
    {
        auto ret = ProgressFrame();
        if (!ret)
            break;
        fps_count++;
    } while (IsInGame() && GetTickCount() - new_tick < render_wait && !force_render);
    force_render = false;

    // Bw has these rare cases when it should RefreshUi but doesn't
    // (Taking scv away from building that is being constructed).
    // But it calls RefreshUi at so many places the issue is rarely seen.
    // So just RefreshUi every frame, it was done almost every frame anyways.
    RefreshUi();
    return objects_progressed;
}

static void FreeAllObjects()
{
    Unit::DeleteAll();
    Sprite::DeleteAll();
    lone_sprites->DeleteAll();
    bullet_system->DeleteAll();
    Order::DeleteAll();
    Ai::DeleteAll();
}

void GameEnd()
{
    FreeAllObjects();
    if (*bw::is_ingame2)
    {
        *bw::leave_game_tick = GetTickCount();
        *bw::is_ingame2 = 0;
    }
    if (IsMultiplayer() && *bw::snp_id == 0x424e4554) // BNET
        bw::ReportGameResult();
    bw::Storm_LeaveGame(0x40000001);
    if (*bw::scmain_state == 4 && *bw::menu_screen_id == 0) // IsInMenu && ?
        bw::ClearNetPlayerData();
    for (auto &color_cycle_data : bw::cycle_colors)
    {
        memset(&color_cycle_data, 0, sizeof(CycleStruct));
    }
    if (!IsMultiplayer())
        bw::Unpause(1);
    if (*bw::popup_dialog)
    {
        bw::RemoveDialog(*bw::popup_dialog);
        *bw::popup_dialog = 0;
        *bw::popup_dialog_active = 0;
        if (*bw::scmain_state == 3)
        {
            *bw::active_accelerators = *bw::previous_accelerators;
            *bw::wm_command_handler = *bw::previous_wm_command_handler;
        }
    }
    if (*bw::has_effects_scode)
        bw::FreeEffectsSCodeUnk();
    for(int i = 0; i < 3; i++)
        bw::unk_placement_box[i][0] = 0;

    bw::FreeUnkSound(&*bw::unk_sound);
    for (int i = 0; i < Limits::ActivePlayers; i++)
        bw::FreeTriggerList(&bw::triggers[i]);
    for (int i = 0; i < 2; i++)
    {
        if (bw::placement_boxes[i].data)
        {
            storm::SMemFree(bw::placement_boxes[i].data, __FILE__, __LINE__, 0);
            bw::placement_boxes[i].data = 0;
        }
    }
    if (*bw::is_placing_building)
        bw::EndBuildingPlacement();
    if (*bw::is_targeting)
        bw::EndTargeting();
    bw::ResetGameScreenEventHandlers();
    bw::FreeGameDialogs();
    bw::FreeMapData();
    if (*bw::pathing != nullptr)
    {
        if ((*bw::pathing)->contours)
            storm::SMemFree((*bw::pathing)->contours, __FILE__, __LINE__, 0);
        storm::SMemFree(*bw::pathing, __FILE__, __LINE__, 0);
        *bw::pathing = 0;
    }
    if (*bw::aiscript_bin != nullptr)
    {
        storm::SMemFree(*bw::aiscript_bin, __FILE__, __LINE__, 0);
        *bw::aiscript_bin = 0;
    }
    if (*bw::bwscript_bin != nullptr)
    {
        storm::SMemFree(*bw::bwscript_bin, __FILE__, __LINE__, 0);
        *bw::bwscript_bin = 0;
    }
    bw::DeleteAiRegions();
    bw::DeleteDirectSound();
    bw::StopSounds();
    bw::DeleteDirectSound(); // well wtf
    bw::InitOrDeleteRaceSounds(0);
    bw::WindowPosUpdate();
    if (*bw::pylon_power_mask != nullptr)
    {
        storm::SMemFree(*bw::pylon_power_mask, __FILE__, __LINE__, 0);
        *bw::pylon_power_mask = 0;
    }
    if (*bw::scenario_chk_STR != nullptr)
    {
        storm::SMemFree(*bw::scenario_chk_STR, __FILE__, __LINE__, 0);
        *bw::scenario_chk_STR = 0;
    }
    if (*bw::map_mpq_handle != nullptr)
    {
        storm::SFileCloseArchive(*bw::map_mpq_handle);
        *bw::map_mpq_handle = 0;
    }
    if (IsReplay())
    {
        (*bw::replay_data)->unk4 = 0;
        if (*bw::playback_commands)
        {
            storm::SMemFree(*bw::playback_commands, __FILE__, __LINE__, 0);
            *bw::playback_commands = 0;
        }
        *bw::is_replay = 0;
    }
    else
    {
        if (*bw::replay_data)
            SaveReplay("LastReplay", true);
//        if (*bw::league) // Nobody cares
//        {
//            char out_name[MAX_PATH];
//            CopyLeagueReplay(out_name);
//            ...
//        }
    }
    *bw::gameid = 0;
}

void BriefingOk(Dialog *dlg, int leave)
{
    dlg->val = nullptr; // No clue what this clears
    bw::Ctrl_LeftUp(dlg->FindChild(0xd));
    if (IsMultiplayer())
        dlg->FindChild(0xfff4)->Show();
    if (leave)
    {
        bw::Storm_LeaveGame(3);
        dlg->dialog.OnceInFrame = nullptr;
        if (*bw::briefing_state == 1)
            *bw::briefing_state = 0;
    }
    else if (*bw::briefing_state == 1)
    {
        char buf[5];
        buf[0] = commands::BriefingStart;
        *(uint32_t *)(buf + 1) = bw::player_objectives_string_id[*bw::local_player_id];
        bw::SendCommand(buf, sizeof buf);
        *bw::briefing_state = 2;
    }
}

void Score::Initialize()
{
    completed_units_count = *bw::completed_units_count;
    all_units_count = *bw::all_units_count;
    unit_deaths = *bw::unit_deaths;
    unit_kills = *bw::unit_kills;
}

uint32_t Score::CompletedUnits(UnitType unit_id, int player) const
{
    Assert(unit_id.IsValid());
    if (player >= Limits::Players)
        return 0;
    return completed_units_count[unit_id.Raw() * Limits::Players + player];
}

uint32_t Score::AllUnits(UnitType unit_id, int player) const
{
    Assert(unit_id.IsValid());
    if (player >= Limits::Players)
        return 0;
    return all_units_count[unit_id.Raw() * Limits::Players + player];
}

uint32_t Score::Deaths(UnitType unit_id, int player) const
{
    Assert(unit_id.IsValid());
    if (player >= Limits::Players)
        return 0;
    return unit_deaths[unit_id.Raw() * Limits::Players + player];
}

uint32_t Score::Kills(UnitType unit_id, int player) const
{
    Assert(unit_id.IsValid());
    if (player >= Limits::Players)
        return 0;
    return unit_kills[unit_id.Raw() * Limits::Players + player];
}

static bool IsPlayerUnk(int player)
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

void Score::RecordDeath(Unit *target, int attacking_player)
{
    if ((target->flags & UnitStatus::Hallucination) || target->Type().IsSubunit())
        return;

    Assert(target->Type().IsValid());

    int target_player = target->player;
    if (attacking_player >= Limits::Players || target_player >= Limits::Players)
        return;

    int group = target->Type().GroupFlags();
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
    unit_deaths[target->unit_id * Limits::Players + target_player] += 1;
    if (target_player != attacking_player && IsActivePlayer(attacking_player) && !IsPlayerUnk(attacking_player))
    {
        if ((~group & 0x8) && (target->Type() != UnitId::Larva) && (target->Type() != UnitId::Egg))
        {
            if (group & 0x10)
            {
                bw::player_building_kills[attacking_player]++;
                bw::player_building_kill_score[attacking_player] += target->Type().KillScore();
                if (group & 0x20)
                    bw::player_factory_kills[attacking_player]++;
            }
        }
        else
        {
            bw::player_men_kills[attacking_player]++;
            bw::player_men_kill_score[attacking_player] += target->Type().KillScore();
        }
        unit_kills[target->unit_id * Limits::Players + attacking_player] += 1;
    }
}
