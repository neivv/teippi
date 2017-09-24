#ifdef CONSOLE
#include "scconsole.h"

#include "offsets.h"
#include "draw.h"

#include "constants/weapon.h"
#include "ai.h"
#include "ai_hit_reactions.h"
#include "bullet.h"
#include "game.h"
#include "limits.h"
#include "pathing.h"
#include "player.h"
#include "resolution.h"
#include "selection.h"
#include "sprite.h"
#include "strings.h"
#include "tech.h"
#include "test_game.h"
#include "triggers.h"
#include "unit.h"
#include "unitsearch.h"
#include "upgrade.h"
#include "yms.h"

#include <string>
#include <algorithm>
#include <unordered_set>

using namespace Common;
using std::get;

ScConsole::ScConsole()
{
    show_fps = Debug == true;
    show_frame = false;
    draw_info = false;
    draw_locations = false;
    draw_crects = false;
    draw_ai_towns = false;
    draw_orders = OrderDrawMode::None;
    draw_ai_data = false;
    draw_ai_full = false;
    draw_ai_named = false;
    draw_ai_unit_homes = false;
    for (int i = 0; i < Limits::Players; i++)
        show_ai[i] = 1;
    draw_coords = false;
    draw_range = false;
    draw_bullets = false;
    draw_resource_areas = false;

    AddCommand("heal", &ScConsole::Heal);
    AddCommand("kill", &ScConsole::Kill);
    AddCommand("give", &ScConsole::Give);
    AddCommand("gsw", &ScConsole::Gsw);
    AddCommand("vis", &ScConsole::Vis);
    AddCommand("tcr", &ScConsole::Tcr);
    AddCommand("trigger_speed", &ScConsole::Tcr);
    AddCommand("supplymax", &ScConsole::SupplyMax);
    AddCommand("aiscript", &ScConsole::AiScript);
    AddCommand("airegion", &ScConsole::AiRegion);
    AddCommand("aireg", &ScConsole::AiRegion);
    AddCommand("player", &ScConsole::Player);
    AddCommand("u", &ScConsole::UnitCmd);
    AddCommand("unit", &ScConsole::UnitCmd);
    AddCommand("money", &ScConsole::Money);
    AddCommand("resources", &ScConsole::Money);
    AddCommand("supply", &ScConsole::Supply);
    AddCommand("self", &ScConsole::Self);
    AddCommand("frame", &ScConsole::Frame);
    AddCommand("pause", &ScConsole::Pause);
    AddCommand("show", &ScConsole::Show);
    AddCommand("grid", &ScConsole::Cmd_Grid);
    AddCommand("test", &ScConsole::Test);
    AddCommand("spawn", &ScConsole::Spawn);
    AddCommand("ais_exec", &ScConsole::AiscriptExec);
    commands["dc"] = [this](const auto &a) { return this->Death(a, false, false); };
    commands["dc?"] = [this](const auto &a) { return this->Death(a, true, false); };
    commands["dc;"] = [this](const auto &a) { return this->Death(a, false, true); };
    commands["dc;;"] = [this](const auto &a) { this->death_counters.clear(); return true; };
}

ScConsole::~ScConsole()
{
}

void ScConsole::Hide()
{
    Console::Hide();
    *bw::needs_full_redraw = true;
}

bool ScConsole::Test(const CmdArgs &args)
{
    if (!IsInGame() || !Debug)
        return false;
    int low = -1, high = -1;
    int repeat = 1;
    if (strcmp(args[1], "all") == 0)
    {
        low = 0;
        high = 100000;
    }
    else if (strcmp(args[1], "ai_tgtprio") == 0)
    {
        bool result = Ai::TestBestTargetPicking();
        if (!result) { Assert(result); }
        return true;
    }
    else
    {
        low = atoi(args[1]);
        if (args[2][0] == 0)
            high = low + 1;
        else
            high = atoi(args[2]);
    }
    if (low == -1)
    {
        Print("'test all [_] [repeat]' or 'test <begin_id> [end_id] [repeat]'");
        return false;
    }

    if (args[3][0] != 0)
    {
        repeat = atoi(args[3]);
    }

    delete game_tests;
    game_tests = new GameTests;
    game_tests->RunTests(low, high, repeat);
    return true;
}

bool ScConsole::Cmd_Grid(const CmdArgs &args)
{
    int width = atoi(args[2]);
    int height = atoi(args[3]);
    if (width == 0)
        return false;
    if (height == 0)
        height = width;
    int color = atoi(args[4]);
    if (color == 0)
        color = 0x98;

    Grid grid(width, height, color);
    if (strcmp(args[1], "-") == 0)
    {
        if (args[2][0] == 0)
        {
            grids.clear();
            return true;
        }
        for (int i = 0; i < grids.size(); i++)
        {
            if (grids[i].width == width && grids[i].height == height)
            {
                grids.erase(grids.begin() + i);
                return true;
            }
        }
        return false;
    }
    if (strcmp(args[1], "+") == 0)
    {
        for (int i = 0; i < grids.size(); i++)
        {
            if (grids[i].width == width && grids[i].height == height)
                return false;
        }
        grids.emplace_back(grid);
        return true;
    }
    else
    {
        Printf("grid (+|-) w [h] [color]");
        return false;
    }
}

static vector<UnitType> FindUnitFromName(std::string name)
{
    std::transform(name.begin(), name.end(), name.begin(), tolower);
    vector<UnitType> results;
    for (auto unit_id : UnitType::All())
    {
        std::string unit_name = (*bw::stat_txt_tbl)->GetTblString(unit_id.Raw() + 1);
        std::transform(unit_name.begin(), unit_name.end(), unit_name.begin(), tolower);
        if (unit_name.find(name) != std::string::npos)
            results.emplace_back(unit_id);
    }
    return results;
}

vector<UnitType> ScConsole::ParseUnitId(const char *unit_str, int max_amt)
{
    vector<UnitType> unit_ids;
    char *unit_str_end;
    int unit_id = strtoul(unit_str, &unit_str_end, 16);
    if (unit_str_end[0] != 0 || (unit_id == 0 && unit_str[0] != '0'))
    {
        unit_ids = FindUnitFromName(unit_str);
        if (unit_ids.size() > 30)
        {
            Printf("%d units match '%s'", unit_ids.size(), unit_str);
            return vector<UnitType>();
        }
        else if (unit_ids.size() > max_amt)
        {
            char buf[128];
            snprintf(buf, sizeof buf, "Too many canditates for '%s':", unit_str);
            std::string msg(buf);
            bool first = true;
            for (auto cand : unit_ids)
            {
                snprintf(buf, sizeof buf, " %s (%x)",
                         (*bw::stat_txt_tbl)->GetTblString(cand.Raw() + 1),
                         cand.Raw());
                if (!first)
                    msg.push_back(',');
                msg += buf;
                first = false;
            }
            Printf(msg.c_str());
            return vector<UnitType>();
        }
    }
    else
    {
        unit_ids.emplace_back(UnitType(unit_id));
    }
    if (unit_ids.empty())
        Printf("'%s' is not valid unit id or unit name", unit_str);
    return unit_ids;
}

bool ScConsole::Spawn(const CmdArgs &args)
{
    if (!IsInGame())
        return false;
    auto unit_str = args[1];
    if (unit_str[0] == 0)
    {
        Printf("spawn <unit name or hex id> [amount] [player id]");
        return false;
    }
    vector<UnitType> unit_ids = ParseUnitId(unit_str, 1);

    if (unit_ids.empty())
    {
        return false;
    }
    int amount = 1;
    if (args[2][0] != 0)
        amount = atoi(args[2]);
    int player = *bw::local_player_id;
    if (args[3][0] != 0)
        player = atoi(args[3]);

    Point16 pos = Point16(*bw::screen_x + *bw::mouse_clickpos_x, *bw::screen_y + *bw::mouse_clickpos_y);
    for (int i = 0; i < amount; i++)
    {
        Unit *unit = bw::CreateUnit(unit_ids[0], pos.x, pos.y, player);
        if (unit == nullptr)
            return false;
        bw::FinishUnit_Pre(unit);
        bw::FinishUnit(unit);
        bw::GiveAi(unit);
    }
    return true;
}

bool ScConsole::AiscriptExec(const CmdArgs &args)
{
    if (!IsInGame())
        return false;
    auto player_str = args[1];
    if (args[1][0] == 0)
    {
        Printf("ais_exec <player> <bytes..>");
        return false;
    }
    int player = atoi(player_str);
    vector<uint8_t> data;
    for (int i = 2; args[i][0] != 0; i++) {
        int byte = strtoul(args[i], 0, 16);
        if (byte >= 0x100) {
            return false;
        }
        data.emplace_back(byte);
    }
    // Wait 0x22
    data.emplace_back(0x2);
    data.emplace_back(0x2);
    data.emplace_back(0x2);
    ptr<Ai::Script> script(new Ai::Script(player, 0, false, nullptr));
    auto aiscript = *bw::aiscript_bin;
    *bw::aiscript_bin = data.data();
    bw::ProgressAiScript(script.get());
    *bw::aiscript_bin = aiscript;
    return true;
}

bool ScConsole::Death(const CmdArgs &args, bool print, bool clear)
{
    if (!IsInGame())
        return false;
    auto unit_str = args[1];
    if (unit_str[0] == 0)
    {
        Printf("dc[?|;] <unit name or hex id> [player id, player id, ...]");
        Printf("dc;;");
        return false;
    }
    vector<UnitType> unit_ids = ParseUnitId(unit_str, 10);

    if (unit_ids.empty())
    {
        return false;
    }
    for (auto unit_id : unit_ids)
    {
        auto end = std::remove_if(death_counters.begin(), death_counters.end(), [unit_id](const auto &tp) {
            return get<1>(tp) == unit_id;
        });
        death_counters.erase(end, death_counters.end());
        if (!clear)
        {
            uint16_t player_mask = 0;
            int pos = 2;
            while (args[pos][0] != 0)
            {
                player_mask |= 1 << atoi(args[pos]);
                pos++;
            }
            if (player_mask == 0)
                player_mask = 0xff;

            if (print)
            {
                std::string msg;
                for (int i = 0; i < Limits::Players; i++)
                {
                    if (player_mask & 1 << i)
                    {
                        char buf[16];
                        snprintf(buf, sizeof buf, "%d ", score->Deaths(unit_id, i));
                        msg += buf;
                    }
                }
                Printf(msg.c_str());
            }
            else
                death_counters.emplace_back(player_mask, unit_id);
        }
    }
    return true;
}

Unit *ScConsole::GetUnit()
{
    if (!IsInGame())
        return 0;
    return *bw::primary_selected;
}

bool ScConsole::SupplyMax(const CmdArgs &args)
{
    if (!isdigit(*args[1]))
        return false;

    int max = atoi(args[1]);
    for (unsigned int i = 0; i < Limits::Players; i++)
    {
        bw::zerg_supply_max[i] = max;
        bw::protoss_supply_max[i] = max;
        bw::terran_supply_max[i] = max;
    }
    return true;
}

bool ScConsole::Tcr(const CmdArgs &args)
{
    if (!isdigit(*args[1]))
        return false;

    trigger_check_rate = atoi(args[1]);
    return true;
}

bool ScConsole::Vis(const CmdArgs &args)
{
    if (args[1][0] == 0)
        all_visions = !all_visions;
    else if (strcmp(args[1], "on") == 0)
        all_visions = true;
    else if (strcmp(args[1], "off") == 0)
        all_visions = false;
    else
        return false;
    return true;
}

bool ScConsole::Gsw(const CmdArgs &args)
{
    if (!IsInGame() || !isdigit(*args[1]))
        return false;

    bw::game_speed_waits[*bw::game_speed] = atoi(args[1]);
    if (isdigit(*args[2]))
        render_wait = atoi(args[2]);
    return true;
}

bool ScConsole::Heal(const CmdArgs &args)
{
    if (!IsInGame())
        return false;

    for (Unit *unit : client_select)
    {
        if (unit->hitpoints)
        {
            unit->hitpoints = unit->GetMaxHitPoints() << 8;
            unit->shields = unit->GetMaxShields() << 8;
        }
    }
    return true;
}

bool ScConsole::Kill(const CmdArgs &args)
{
    if (!IsInGame())
        return false;

    for (Unit *unit : client_select)
    {
        unit->Kill(nullptr);
    }
    return true;
}

bool ScConsole::Give(const CmdArgs &args)
{
    if (!IsInGame())
        return false;

    int player;
    if (!isdigit(*args[1]))
        player = *bw::local_player_id;
    else
        player = atoi(args[1]);

    for (Unit *unit : client_select)
    {
        bw::GiveUnit(unit, player, false);
    }
    return true;
}

bool ScConsole::AiScript(const CmdArgs &args)
{
    if (!IsInGame())
        return false;

    const char *cmd = args[1];
    if (strcmp(cmd, "count") == 0)
    {
        uint32_t player = 0xffffffff;
        if (isdigit(*args[2]))
            player = atoi(args[2]);
        uint32_t count = 0;

        for (Ai::Script *script : *bw::first_active_ai_script)
        {
            if (player == 0xffffffff || script->player == player)
                count++;
        }
        Printf("%d", count);
    }
    else if (strcmp(cmd, "info") == 0)
    {
        if (!isdigit(*args[2]))
            return false;
        uint32_t player = atoi(args[2]);
        uint32_t count = 0;

        for (Ai::Script *script : *bw::first_active_ai_script)
        {
            if (script->player == player)
            {
                uint8_t opcode;
                if (script->flags & 0x1) // bwscript
                    opcode = (*bw::bwscript_bin)[script->pos];
                else
                    opcode = (*bw::aiscript_bin)[script->pos];

                Printf("%d @ %p: Pos %x (%02x), wait %d, flagit %x", count++, script, script->pos, opcode, script->wait, script->flags);
            }
        }
    }
    else
    {
        Print("count, info");
        return false;
    }
    return true;
}

bool ScConsole::AiRegion(const CmdArgs &args)
{
    if (!IsInGame() || !isdigit(*args[1]) || !isxdigit(*args[2]))
        return false;

    unsigned int player = atoi(args[1]), region_id = strtoul(args[2], 0, 16);
    if (player >= Limits::ActivePlayers || region_id > (*bw::pathing)->region_count)
        return false;

    Ai::Region *region = bw::player_ai_regions[player] + region_id;
    char buf[64];
    sprintf(buf, "State %d, flags %02x", region->state, region->flags);
    Print(buf);
    return true;
}

bool ScConsole::Player(const CmdArgs &args)
{
    Unit *unit = GetUnit();
    if (!unit)
        return false;

    Printf("%d", unit->player);
    return true;
}

const char *flag_desc[] =
{
    "Completed", "Building", "Air", "Disabled?", "Burrowed", "In building", "In transport", "Unk", "Invisibility done", "Begin invisibility", "Disabled",
    "Free invisibility", "Uninterruptable order", "Nobrkcodestart", "Has disappearing creep", "Under disruption web", "Auto attack?", "Reacts",
    "Ignore collision?", "Move target moved?", "Collides?", "No collision", "Enemy collision?", "Harvesting", "Unk", "Unk", "Invincible",
    "Hold position", "Movement speed upgrade", "Attack speed upgrade", "Hallucination", "Self destructing"
};

bool ScConsole::UnitCmd(const CmdArgs &args)
{
    Unit *unit = GetUnit();
    if (!unit)
        return false;

    std::string cmd = args[1];
    if (cmd == "ai")
    {
        if (!unit->ai)
            Print("No ai");
        else
            Printf("%d", unit->ai->type);
    }
    else if (cmd == "flags")
    {
        Printf("0x%08x", unit->flags);
        if (strcmp(args[2], "v") == 0)
        {
            std::string buf;
            uint32_t flags = unit->flags, i = 0;
            while (flags)
            {
                if (flags & 0x1)
                {
                    if (!buf.empty())
                        buf += ", ";
                    buf += flag_desc[i];
                    char hex_str[32];
                    if (i < 16)
                        sprintf(hex_str, " (0x%04x)", 1 << i);
                    else
                        sprintf(hex_str, " (0x%08x)", 1 << i);
                    buf += hex_str;
                }
                flags = flags >> 1;
                i++;
            }
            Print(buf);
        }
    }
    else if (cmd == "id")
    {
        Printf("Unit id: %02x, lookup id: %08x", unit->unit_id, unit->lookup_id);
    }
    else if (cmd == "find" || cmd == "f")
    {
        unsigned int lookup_id = strtoul(args[2], 0, 16);
        if (lookup_id != 0)
        {
            Unit *unit = Unit::FindById(lookup_id);
            if (unit != nullptr && unit->sprite != nullptr)
            {
                int x = unit->sprite->position.x - resolution::game_width / 2;
                int y = unit->sprite->position.y - resolution::game_height / 2;
                bw::MoveScreen(std::max(0, x), std::max(0, y));
                return true;
            }
        }
        return false;
    }
    else
    {
        Print("ai, flags (v), id, f/find <lookup_id>");
        return false;
    }
    return true;
}

bool ScConsole::Money(const CmdArgs &args)
{
    if (!IsInGame())
        return false;

    if (strcmp(args[1], "set") == 0)
    {
        if (!isdigit(*args[2]) || !isdigit(*args[3]) || !isdigit(*args[4]))
            return false;
        int player = atoi(args[2]), minerals = atoi(args[3]), gas = atoi(args[4]);
        if (!IsActivePlayer(player))
            return false;
        bw::minerals[player] = minerals;
        bw::gas[player] = gas;
    }
    else
    {
        int player;
        if (isdigit(*args[1]))
        {
            player = atoi(args[1]);
        }
        else
        {
            if (!GetUnit())
                return false;
            player = GetUnit()->player;
        }
        if (!IsActivePlayer(player))
            return false;

        Printf("Minerals: %d, Gas %d", bw::minerals[player], bw::gas[player]);
    }
    return true;
}

static void PrintSupply(char *buf, int used, int available)
{
    sprintf(buf + strlen(buf), " %d", used / 2);
    if (used & 1)
        strcat(buf, ".5");
    sprintf(buf + strlen(buf), "/%d", available / 2);
    if (used & 1)
        strcat(buf, ".5");
}

bool ScConsole::Supply(const CmdArgs &args)
{
    if (!IsInGame())
        return false;

    int player;
    if (isdigit(*args[1]))
    {
        player = atoi(args[1]);
    }
    else
    {
        if (!GetUnit())
            return false;
        player = GetUnit()->player;
    }
    if (!IsActivePlayer(player))
        return false;

    char buf[64] = "Zerg";
    PrintSupply(buf, bw::zerg_supply_used[player], bw::zerg_supply_available[player]);
    strcat(buf, ", Terran");
    PrintSupply(buf, bw::terran_supply_used[player], bw::terran_supply_available[player]);
    strcat(buf, ", Protoss");
    PrintSupply(buf, bw::protoss_supply_used[player], bw::protoss_supply_available[player]);
    Printf(buf);
    return true;
}

bool ScConsole::Self(const CmdArgs &args)
{
    if (*bw::local_player_id == *bw::local_unique_player_id)
        Printf("Game %d, Net %d", *bw::local_player_id, *bw::self_net_player);
    else
        Printf("Shared %d, Unique %d, Net %d", *bw::local_player_id, *bw::local_unique_player_id, *bw::self_net_player);

    return true;
}

bool ScConsole::Frame(const CmdArgs &args)
{
    if (!IsInGame())
        return false;
    Printf("Frame %d", *bw::frame_count);
    return true;
}

bool ScConsole::Pause(const CmdArgs &args)
{
    if (!IsInGame())
        return false;

    *bw::is_paused ^= 1;
    return true;
}

static int CountImages(Sprite *sprite) {
    int count = 0;
    for (Image *img : sprite->first_overlay) {
        (void)img; // Intentionally unused
        count += 1;
    }
    return count;
}

static int CountUnitImages(Unit *unit)
{
    int count = 0;
    if (unit->sprite != nullptr) {
        count += CountImages(unit->sprite.get());
        if (unit->subunit != nullptr && unit->subunit->sprite != nullptr) {
            count += CountImages(unit->subunit->sprite.get());
        }
    }
    return count;
}

void ScConsole::ConstructInfoLines()
{
    info_lines.clear();
    if (show_fps)
    {
        char fps_str[32];
        sprintf(fps_str, "Fps: %.1f", fps);
        info_lines.emplace_back(fps_str);
    }
    if (show_frame)
    {
        char str[32];
        sprintf(str, "Frame: %d", *bw::frame_count);
        info_lines.emplace_back(str);
    }
    if (draw_info)
    {
        char str[32];
        int unit_count = 0;
        for (auto unit_id : UnitType::All())
        {
            for (int player = 0; player < Limits::Players; player++)
                unit_count += score->AllUnits(unit_id, player);
        }
        snprintf(str, sizeof str, "Units: %d", unit_count);
        info_lines.emplace_back(str);
        snprintf(str, sizeof str, "Bullets: %d", bullet_system->BulletCount());
        info_lines.emplace_back(str);
        info_lines.emplace_back("Sprites: Drawn/lone/fow");
        snprintf(str, sizeof str, "         %d/%d/%d", Sprite::DrawnSprites(),
                lone_sprites->lone_sprites.size(), lone_sprites->fow_sprites.size());
        info_lines.emplace_back(str);
        int unit_images = 0;
        int lone_images = 0;
        int fow_images = 0;
        int bullet_images = 0;
        for (Unit *unit : *bw::first_active_unit) {
            unit_images += CountUnitImages(unit);
        }
        for (Unit *unit : *bw::first_hidden_unit) {
            unit_images += CountUnitImages(unit);
        }
        for (Unit *unit : *bw::first_dying_unit) {
            unit_images += CountUnitImages(unit);
        }
        for (Unit *unit : *bw::first_revealer) {
            unit_images += CountUnitImages(unit);
        }
        for (Bullet *bullet : bullet_system->ActiveBullets()) {
            bullet_images += CountImages(bullet->sprite.get());
        }
        for (const auto &sprite : lone_sprites->lone_sprites) {
            lone_images += CountImages(sprite.get());
        }
        for (const auto &sprite : lone_sprites->fow_sprites) {
            fow_images += CountImages(sprite.get());
        }
        info_lines.emplace_back("Images: Total/unit/bullet/lone/fow");
        snprintf(str, sizeof str, "        %d/%d/%d/%d/%d", unit_images + lone_images + fow_images +
                bullet_images, unit_images, bullet_images, lone_images, fow_images);
        info_lines.emplace_back(str);
    }
}

void ScConsole::DrawLocations(uint8_t *framebuf, xuint w, yuint h)
{
    if (!draw_locations)
        return;

    Common::Surface surface(framebuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (Location &location : bw::locations)
    {
        Rect32 &area = location.area;
        surface.DrawLine(Point32(area.left, area.top) - screen_pos, Point32(area.right, area.top) - screen_pos, 0x7c,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        surface.DrawLine(Point32(area.left, area.top) - screen_pos, Point32(area.left, area.bottom) - screen_pos, 0x7c,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        surface.DrawLine(Point32(area.right, area.top) - screen_pos, Point32(area.right, area.bottom) - screen_pos, 0x7c,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        surface.DrawLine(Point32(area.left, area.bottom) - screen_pos, Point32(area.right, area.bottom) - screen_pos, 0x7c,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
    }
}

void ScConsole::DrawCrects(uint8_t *framebuf, xuint w, yuint h)
{
    if (!draw_crects)
        return;

    Common::Surface surface(framebuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    unit_search->ForEachUnitInArea(Rect16(screen_pos.x, screen_pos.y,
                screen_pos.x + resolution::game_width, screen_pos.y + resolution::game_height), [&](Unit *unit)
    {
        Rect16 crect = unit->GetCollisionRect();
        surface.DrawLine(Point32(crect.left, crect.top) - screen_pos, Point32(crect.right, crect.top) - screen_pos, 0x74,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        surface.DrawLine(Point32(crect.left, crect.top) - screen_pos, Point32(crect.left, crect.bottom) - screen_pos, 0x74,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        surface.DrawLine(Point32(crect.right, crect.top) - screen_pos, Point32(crect.right, crect.bottom) - screen_pos, 0x74,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        surface.DrawLine(Point32(crect.left, crect.bottom) - screen_pos, Point32(crect.right, crect.bottom) - screen_pos, 0x74,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        return false;
    });
}

static const char *RequestStr(int req)
{
    switch (req)
    {
        case 1:
            return "Train";
        case 2:
            return "Guard";
        case 3:
            return "Build";
        case 4:
            return "Worker";
        case 5:
            return "Upgrade";
        case 6:
            return "Tech";
        case 7:
            return "Addon";
        case 8:
            return "Observer";
        default:
            return "Error";
    }
}

static int CountScripts(int player)
{
    int count = 0;
    for (Ai::Script *script : *bw::first_active_ai_script)
    {
        if (script->player == player)
            count++;
    }
    return count;
}

void ScConsole::GetTownRequests(uint32_t *out, int len, uint32_t *in)
{
    int pos = 0;
    while (pos != len && in[pos] != 0)
    {
        auto req = in[pos++];
        auto unit_id = req >> 16;
        auto amt = (req & 0xf8) >> 3;
        bool skip = false;
        for (int i = 0; i < len && in[i] != 0; i++)
        {
            auto other_amt = (in[i] & 0xf8) >> 3;
            if (in[i] >> 16 == unit_id && amt < other_amt && (in[i] & 0x6) == (req & 0x6))
            {
                if (~in[i] & 0x1 || req & 0x1)
                {
                    skip = true;
                    break;
                }
            }
        }
        if (!skip || draw_ai_full)
            *out++ = req;
    }
    *out = 0;
}

void ScConsole::DrawAiRegions(int player, Common::Surface *text_surf, const Point32 &pos)
{
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (int i = 0; i < (*bw::pathing)->region_count; i++)
    {
        Pathing::Region *p_region = (*bw::pathing)->regions + i;
        Point32 draw_pos = Point32(p_region->x / 0x100, p_region->y / 0x100) - screen_pos + pos;
        if (draw_pos.x < 0 || draw_pos.y < 0)
           continue;
        if (draw_pos.x >= resolution::game_width || draw_pos.y >= resolution::game_height)
           continue;
        Ai::Region *region = Ai::GetRegion(player, i);
        char buf[128];
        snprintf(buf, sizeof buf, "State %x target %x", region->state, region->target_region_id);
        text_surf->DrawText(&font, buf, draw_pos, 0x55);
        draw_pos += Point32(0, 10);
        snprintf(buf, sizeof buf, "Need %d/%d, Current %d/%d",
                region->needed_ground_strength, region->needed_air_strength,
                region->local_ground_strength, region->local_air_strength);
        text_surf->DrawText(&font, buf, draw_pos, 0x55);
        draw_pos += Point32(0, 10);
        snprintf(buf, sizeof buf, "All %d/%d, Enemy %d/%d",
                region->all_ground_strength, region->all_air_strength,
                region->enemy_ground_strength, region->enemy_air_strength);
        text_surf->DrawText(&font, buf, draw_pos, 0x55);
    }
}

void ScConsole::DrawAiInfo(uint8_t *textbuf, uint8_t *framebuf, xuint w, yuint h)
{
    if (!draw_ai_towns)
        return;
    Common::Surface surface(framebuf, w, h);
    Common::Surface text_surface(textbuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    Point32 info_pos(10, 30);
    Point32 region_text_pos(0, 10);
    for (unsigned int i = 0; i < Limits::ActivePlayers; i++)
    {
        if (bw::players[i].type != 1 || show_ai[i] == 0)
            continue;
        for (Ai::Town *town = bw::active_ai_towns[i]; town; town = town->list.next)
        {
            Rect32 rect = Rect32(Point32(town->position), 5).OffsetBy(screen_pos.Negate());
            surface.DrawRect(rect, 0xb9);
            if (town->mineral != nullptr)
                surface.DrawLine(town->mineral->sprite->position - screen_pos, town->position - screen_pos, 0x80);
            for (int j = 0; j < 3; j++)
            {
                if (town->gas_buildings[j] != nullptr)
                    surface.DrawLine(town->gas_buildings[j]->sprite->position - screen_pos, town->position - screen_pos, 0xba);
            }
            if (town->building_scv != nullptr)
                surface.DrawLine(town->building_scv->sprite->position - screen_pos, town->position - screen_pos, 0x71);
            char str[64];
            snprintf(str, sizeof str, "Inited: %d, workers %d / %d", town->inited, town->worker_count, town->unk1b);
            Point32 draw_pos = town->position - screen_pos + Point32(0 - strlen(str) * 3, 20);
            text_surface.DrawText(&font, str, draw_pos, 0x55);
            draw_pos += Point32(0, 10);
            std::string req_str = "Requests: ";
            uint32_t requests[0x65];
            requests[0x64] = 0;
            GetTownRequests(requests, 0x64, town->build_requests);
            for (int i = 0; requests[i] != 0; )
            {
                int line_len = draw_ai_named ? 4 : 8;
                for (int j = 0; j < line_len && requests[i] != 0; j++, i++)
                {
                    if (j != 0)
                        req_str.append(", ");
                    char buf[128];
                    char name_buf[128];
                    uint32_t request = requests[i];
                    int unit_id = request >> 16;
                    if (draw_ai_named && request & 0x2)
                        snprintf(name_buf, sizeof name_buf, "%s", UpgradeType(unit_id).Name());
                    else if (draw_ai_named && request & 0x4)
                        snprintf(name_buf, sizeof name_buf, "%s", TechType(unit_id).Name());
                    else if (draw_ai_named)
                        snprintf(name_buf, sizeof name_buf, "%s", (*bw::stat_txt_tbl)->GetTblString(unit_id + 1));
                    else
                        snprintf(name_buf, sizeof name_buf, "%x:%x", (request & 0x6) >> 1, unit_id);
                    if (request & 1)
                        snprintf(buf, sizeof buf, "%s (%d, if needed)", name_buf, (request & 0xf8) >> 3);
                    else
                        snprintf(buf, sizeof buf, "%s (%d)", name_buf, (request & 0xf8) >> 3);
                    req_str.append(buf);
                }
                text_surface.DrawText(&font, req_str, draw_pos + Point32(10, 0), 0x55);
                draw_pos += Point32(0, 10);
                req_str = "";
            }
        }
        char str[128];
        auto &ai_data = bw::player_ai[i];
        snprintf(str, sizeof str, "Player %d: money %d / %d - need %d / %d / %d - available %d / %d / %d", i,
                bw::minerals[i], bw::gas[i], ai_data.mineral_need, ai_data.gas_need, ai_data.supply_need,
                ai_data.minerals_available, ai_data.gas_available, ai_data.supply_available);
        text_surface.DrawText(&font, str, info_pos, 0x55);
        info_pos += Point32(0, 10);
        snprintf(str, sizeof str, "Request count %d, wanted unit %d, liftoff cooldown (?) %d, Script count %d",
                ai_data.request_count, ai_data.wanted_unit, ai_data.liftoff_cooldown, CountScripts(i));
        text_surface.DrawText(&font, str, info_pos, 0x55);
        info_pos += Point32(0, 10);
        if (ai_data.attack_grouping_region != 0)
        {
            snprintf(
                str,
                sizeof str,
                "Attack region %x, started %d ago, failed %d",
                ai_data.attack_grouping_region - 1,
                *bw::elapsed_seconds - ai_data.last_attack_seconds,
                ai_data.attack_failed
            );
            text_surface.DrawText(&font, str, info_pos, 0x55);
            info_pos += Point32(0, 10);
        }
        if (ai_data.request_count)
        {
            std::string str = "Requests: ";
            std::unordered_set<uint32_t> collapsed_requests;
            for (int i = 0; i < ai_data.request_count;)
            {
                int line_len = draw_ai_named ? 4 : 8;
                for (int j = 0; j < line_len && i < ai_data.request_count;)
                {
                    bool skip = false;
                    auto unit_id = ai_data.requests[i].unit_id;
                    auto type = ai_data.requests[i].type;
                    int amt = 1;
                    uint32_t hashset_key = (unit_id << 16) | type;
                    if (i >= 4 && !draw_ai_full) {
                        if (collapsed_requests.count(hashset_key) != 0) {
                            skip = true;
                        } else {
                            for (int k = i + 1; k < ai_data.request_count; k++) {
                                auto other_req = ai_data.requests[k];
                                if (other_req.unit_id == unit_id && other_req.type == type) {
                                    amt += 1;
                                }
                            }
                            collapsed_requests.emplace(hashset_key);
                        }
                    }
                    if (!skip) {
                        if (j != 0)
                            str.append(", ");
                        char buf[64];
                        const char *desc = RequestStr(type);
                        if (draw_ai_named && ai_data.requests[i].type == 5) {
                            snprintf(buf, sizeof buf, "%s %s", desc, UpgradeType(unit_id).Name());
                        } else if (draw_ai_named && ai_data.requests[i].type == 6) {
                            snprintf(buf, sizeof buf, "%s %s", desc, TechType(unit_id).Name());
                        } else if (draw_ai_named) {
                            auto name = (*bw::stat_txt_tbl)->GetTblString(unit_id + 1);
                            snprintf(buf, sizeof buf, "%s %s", desc, name);
                        } else {
                            snprintf(buf, sizeof buf, "%s %x", desc, unit_id);
                        }
                        str.append(buf);
                        if (amt != 1) {
                            snprintf(buf, sizeof buf, " (x%d)", amt);
                            str.append(buf);
                        }
                        j += 1;
                    }
                    i += 1;
                }
                text_surface.DrawText(&font, str, info_pos + Point32(10, 0), 0x55);
                info_pos += Point32(0, 10);
                str = "";
            }
        }
        if (show_ai[i] == 2)
        {
            DrawAiRegions(i, &text_surface, region_text_pos);
            region_text_pos += Point32(0, 30);
        }
    }
}

void ScConsole::DrawAiUnitHomes(uint8_t *framebuf, xuint w, yuint h)
{
    if (!draw_ai_unit_homes)
        return;

    Common::Surface surface(framebuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (Unit *unit : *bw::first_active_unit)
    {
        if (unit->ai != nullptr && show_ai[unit->player] != 0)
        {
            auto sprite_pos_screen = unit->sprite->position - screen_pos;
            switch (unit->ai->type)
            {
                case 1: {
                    auto ai = (Ai::GuardAi *)unit->ai;
                    surface.DrawLine(ai->home - screen_pos, sprite_pos_screen, 0x80);
                    if (ai->home != ai->unk_pos)
                    {
                        surface.DrawLine(ai->unk_pos - screen_pos, sprite_pos_screen, 0xba);
                    }
                } break;
                case 2: {
                    auto ai = (Ai::WorkerAi *)unit->ai;
                    surface.DrawLine(ai->town->position - screen_pos, sprite_pos_screen, 0x74);
                } break;
                case 3: {
                    auto ai = (Ai::BuildingAi *)unit->ai;
                    surface.DrawLine(ai->town->position - screen_pos, sprite_pos_screen, 0x74);
                } break;
                case 4: {
                    auto ai = (Ai::MilitaryAi *)unit->ai;
                    auto region = (*bw::pathing)->regions + ai->region->region_id;
                    auto region_pos = Point32(region->x / 0x100, region->y / 0x100);
                    surface.DrawLine(region_pos - screen_pos, sprite_pos_screen, 0x7c);
                } break;
            }
        }
    }
}

void ScConsole::DrawResourceAreas(uint8_t *textbuf, uint8_t *framebuf, xuint w, yuint h)
{
    if (!draw_resource_areas)
        return;

    Common::Surface surface(framebuf, w, h);
    Common::Surface text_surface(textbuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (int i = 0; i < bw::resource_areas->used_count; i++)
    {
        // First entry is not used
        const auto &area = bw::resource_areas->areas[i + 1];
        int x = area.position.x - screen_pos.x;
        int y = area.position.y - screen_pos.y;
        if (x >= 0 && x < w && y >= 0 && y < h)
        {
            char buf[256];
            snprintf(buf, sizeof buf, "Area %x: Mine %d in %d, Gas %d in %d, flags %02x", i + 1,
                    area.total_minerals, area.mineral_field_count,
                    area.total_gas, area.geyser_count, area.flags);
            text_surface.DrawText(&font, buf, Point32(x - 50, y + 20), 0x55);
            snprintf(buf, sizeof buf, "Unk: %02x %08x %08x %08x %08x", area.is_start_location,
                    area.unk10[0], area.unk10[1], area.unk10[2], area.unk10[3]);
            text_surface.DrawText(&font, buf, Point32(x - 50, y + 30), 0x55);
            snprintf(buf, sizeof buf, "%08x %08x %08x %08x",
                    area.unk10[4], area.unk10[5], area.unk10[6], area.unk10[7]);
            text_surface.DrawText(&font, buf, Point32(x - 50, y + 40), 0x55);
            Rect32 rect = Rect32(Point32(area.position), 15).OffsetBy(screen_pos.Negate());
            surface.DrawRect(rect, 0xb9);
        }
    }
}

void ScConsole::DrawOrders(uint8_t *framebuf, xuint w, yuint h)
{
    if (draw_orders == OrderDrawMode::None)
        return;

    Common::Surface surface(framebuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    if (draw_orders == OrderDrawMode::All)
    {
        for (Unit *unit : *bw::first_active_unit)
        {
            if (unit->target)
                surface.DrawLine(unit->target->sprite->position - screen_pos, unit->sprite->position - screen_pos, 0xa4);
        }
    }
    else if (draw_orders == OrderDrawMode::Selected)
    {
        for (Unit *unit : client_select)
        {
            if (unit->target)
                surface.DrawLine(unit->target->sprite->position - screen_pos, unit->sprite->position - screen_pos, 0xa4);
        }
    }
}

void ScConsole::DrawCoords(uint8_t *framebuf, xuint w, yuint h)
{
    if (!draw_coords)
        return;

    char buf[32];
    snprintf(buf, sizeof buf, "Game: %04hx.%04hx", *bw::screen_x + *bw::mouse_clickpos_x,
            *bw::screen_y + *bw::mouse_clickpos_y);
    info_lines.emplace_back(buf);
    snprintf(buf, sizeof buf, "Mouse: %04hx.%04hx", (int)*bw::mouse_clickpos_x, (int)*bw::mouse_clickpos_y);
    info_lines.emplace_back(buf);
    snprintf(buf, sizeof buf, "Screen: %04hx.%04hx", (int)*bw::screen_x, (int)*bw::screen_y);
    info_lines.emplace_back(buf);
}

void ScConsole::DrawDeaths(uint8_t *framebuf, xuint w, yuint h)
{
    Common::Surface surface(framebuf, w, h);
    Point32 draw_pos = Point32(20, 50);
    for (const auto &tp : death_counters)
    {
        UnitType unit_id = get<1>(tp);
        int player_mask = get<0>(tp);
        char buf[64];
        snprintf(buf, sizeof buf, "%s:", (*bw::stat_txt_tbl)->GetTblString(unit_id.Raw() + 1));
        std::string msg(buf);
        int player = 0;
        while (player_mask)
        {
            if (player_mask & 1)
            {
                snprintf(buf, sizeof buf, " %d", score->Deaths(unit_id, player));
                msg += buf;
            }
            player++;
            player_mask >>= 1;
        }
        surface.DrawText(&font, msg, draw_pos, 0x55);
        draw_pos += Point32(0, 10);
    }
}

void ScConsole::DrawRange(uint8_t *framebuf, xuint w, yuint h)
{
    if (!draw_range)
        return;

    Common::Surface surface(framebuf, w, resolution::game_height);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (Unit *unit : *bw::first_active_unit)
    {
        Point32 pos = Point32(unit->sprite->position) - screen_pos;
        WeaponType ground_weapon = unit->GetGroundWeapon();
        WeaponType air_weapon = unit->GetAirWeapon();
        const auto dbox = unit->Type().DimensionBox();
        int unit_radius_approx = (dbox.top + dbox.bottom + dbox.left + dbox.right) / 4 + 1;
        if (ground_weapon != WeaponId::None)
            surface.DrawCircle(pos, unit->GetWeaponRange(true) + unit_radius_approx, 0x75,
                    [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        if (air_weapon != WeaponId::None && ground_weapon != air_weapon)
            surface.DrawCircle(pos, unit->GetWeaponRange(false) + unit_radius_approx, 0x7a,
                    [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
    }
}

void ScConsole::DrawGrids(uint8_t *framebuf, xuint w, yuint h)
{
    if (grids.empty())
        return;

    Common::Surface surface(framebuf, w, resolution::game_height);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    x32 x_end = screen_pos.x + w;
    y32 y_end = screen_pos.y + h;
    for (const auto &grid : grids)
    {
        x32 x = 0 - screen_pos.x % grid.width - 1;
        while (x < x_end)
        {
            surface.DrawLine(Point32(x, 0), Point32(x, h), grid.color);
            x += grid.width;
        }
        y32 y =  0 - screen_pos.y % grid.height - 1;
        while (y < y_end)
        {
            surface.DrawLine(Point32(0, y), Point32(w, y), grid.color);
            y += grid.height;
        }
    }
}

void ScConsole::DrawBullets(uint8_t *framebuf, xuint w, yuint h)
{
    if (!draw_bullets)
        return;
    Common::Surface surface(framebuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (Bullet *bullet : bullet_system->ActiveBullets())
    {
        Rect32 rect = Rect32(Point32(bullet->sprite->position), 5).OffsetBy(screen_pos.Negate());
        surface.DrawRect(rect, 0x23);
    }
}

void ScConsole::DrawDebugInfo(uint8_t *framebuf, xuint w, yuint h)
{
    ConstructInfoLines();
    uint8_t buffer[resolution::screen_width * resolution::screen_height];
    uint8_t text_buf[resolution::screen_width * resolution::screen_height];
    memset(buffer, 0, sizeof buffer);
    memset(text_buf, 0, sizeof text_buf);
    DrawLocations(framebuf, w, h);
    DrawCrects(framebuf, w, h);
    DrawGrids(buffer, resolution::screen_width, resolution::screen_height);
    DrawAiInfo(text_buf, buffer, resolution::screen_width, resolution::screen_height);
    DrawAiUnitHomes(buffer, resolution::screen_width, resolution::screen_height);
    DrawResourceAreas(text_buf, buffer, resolution::screen_width, resolution::screen_height);
    DrawOrders(buffer, resolution::screen_width, resolution::screen_height);
    DrawCoords(buffer, resolution::screen_width, resolution::screen_height);
    DrawDeaths(text_buf, resolution::screen_width, resolution::screen_height);
    DrawRange(buffer, resolution::screen_width, resolution::screen_height);
    DrawBullets(buffer, resolution::screen_width, resolution::screen_height);
    if (!info_lines.empty())
    {
        int info_lines_width = font.TextLength(*std::max_element(info_lines.begin(), info_lines.end(),
            [this](const auto &a, const auto &b) {
            return font.TextLength(a) < font.TextLength(b);
        }));
        Point32 info_pos(resolution::screen_width - info_lines_width - 10, 40);
        Common::Surface surf(buffer, resolution::screen_width, resolution::screen_height);
        for (const auto &line : info_lines)
        {
            surf.DrawText(&font, line, info_pos, 0x55);
            info_pos += Point32(0, 10);
        }
    }
    for (unsigned y = 0; y < resolution::game_height; y++)
    {
        for (unsigned x = 0; x < resolution::game_width; x += 4)
        {
            if (*(uint32_t *)(buffer + y * resolution::screen_width + x) == 0)
                continue;
            if (buffer[y * resolution::screen_width + x] != 0 && !bw::IsOutsideGameScreen(x, y))
                framebuf[y * w + x] = buffer[y * resolution::screen_width + x];
            if (buffer[y * resolution::screen_width + x + 1] != 0 && !bw::IsOutsideGameScreen(x + 1, y))
                framebuf[y * w + x + 1] = buffer[y * resolution::screen_width + x + 1];
            if (buffer[y * resolution::screen_width + x + 2] != 0 && !bw::IsOutsideGameScreen(x + 2, y))
                framebuf[y * w + x + 2] = buffer[y * resolution::screen_width + x + 2];
            if (buffer[y * resolution::screen_width + x + 3] != 0 && !bw::IsOutsideGameScreen(x + 3, y))
                framebuf[y * w + x + 3] = buffer[y * resolution::screen_width + x + 3];
        }
    }
    for (unsigned y = 1; y < resolution::screen_height - 1; y++)
    {
        for (unsigned x = 1; x < resolution::screen_width - 1; x++)
        {
            auto color = text_buf[y * resolution::screen_width + x];
            if (color != 0)
            {
                if (text_buf[y * resolution::screen_width + x - 1] == 0)
                {
                    framebuf[y * w + x - 1] = 0;
                    if (x > 1 && text_buf[y * resolution::screen_width + x - 2] == 0)
                        framebuf[y * w + x - 2] = 0;
                }
                if (text_buf[y * resolution::screen_width + x + 1] == 0)
                    framebuf[y * w + x + 1] = 0;
                if (text_buf[(y + 1) * resolution::screen_width + x] == 0)
                    framebuf[(y + 1) * w + x] = 0;
                if (text_buf[(y - 1) * resolution::screen_width + x] == 0)
                    framebuf[(y - 1) * w + x] = 0;
                framebuf[y * w + x] = text_buf[y * resolution::screen_width + x];
            }
        }
    }
}

bool ScConsole::Show(const CmdArgs &args)
{
    std::string what(args[1]);
    if (what == "nothing")
    {
        draw_locations = draw_paths = draw_crects = draw_coords = draw_info =
            draw_range = draw_region_borders = draw_region_data = draw_ai_data = draw_ai_towns =
            show_fps = show_frame = draw_bullets = draw_resource_areas = false;
        draw_orders = OrderDrawMode::None;
    }
    else if (what == "fps")
        show_fps = !show_fps;
    else if (what == "frame")
        show_frame = !show_frame;
    else if (what == "locations")
        draw_locations = !draw_locations;
    else if (what == "paths")
        draw_paths = !draw_paths;
    else if (what == "collision")
        draw_crects = !draw_crects;
    else if (what == "coords")
        draw_coords = !draw_coords;
    else if (what == "info")
        draw_info = !draw_info;
    else if (what == "range")
        draw_range = !draw_range;
    else if (what == "bullets")
        draw_bullets = !draw_bullets;
    else if (what == "resareas")
        draw_resource_areas = !draw_resource_areas;
    else if (what == "regions")
    {
        draw_region_borders = !draw_region_borders;
        draw_region_data = draw_region_borders;
    }
    else if (what == "orders")
    {
        std::string more(args[2]);
        if (more == "selected")
            draw_orders = OrderDrawMode::Selected;
        else if (more == "all")
            draw_orders = OrderDrawMode::All;
        else if (more != "")
            return false;
        else
        {
            if (draw_orders == OrderDrawMode::None)
                draw_orders = OrderDrawMode::All;
            else
                draw_orders = OrderDrawMode::None;
        }
    }
    else if (what == "ai")
    {
        std::string more(args[2]);
        if (more == "full")
            draw_ai_full = true;
        else if (more == "simple")
            draw_ai_full = false;
        else if (more == "named")
            draw_ai_named = true;
        else if (more == "raw")
            draw_ai_named = false;
        else if (more == "player")
        {
            if (std::string(args[3]) == "all")
            {
                for (int i = 0; i < Limits::Players; i++)
                    show_ai[i] = 1;
            }
            else
            {
                int player = atoi(args[3]);
                if (IsActivePlayer(player))
                {
                    for (int i = 0; i < Limits::Players; i++)
                        show_ai[i] = 0;
                    show_ai[player] = 2;
                }
            }
        }
        else if (more == "units")
        {
            draw_ai_unit_homes = !draw_ai_unit_homes;
        }
        else if (more == "")
        {
            draw_ai_towns = !draw_ai_towns;
            draw_ai_data = !draw_ai_data;
        }
        else
        {
            return false;
        }
        if (more != "" && more != "units")
        {
            draw_ai_towns = true;
            draw_ai_data = true;
        }
    }
    else
    {
        Printf("show <nothing|fps|frame|regions|locations|paths|collision|coords|range|info|bullets|resareas>");
        Printf("show ai [full|simple|named|raw|units|(player <player|all>>)]");
        Printf("show orders [all|selected]");
        return false;
    }
    return true;
}

static void DrawHook(uint8_t *framebuf, xuint w, yuint h)
{
    if (console)
    {
        if (IsInGame())
            ((ScConsole *)console)->DrawDebugInfo(framebuf, w, h);
        console->Draw(framebuf, w, h);
    }
}

void PatchConsole()
{
    console = new ScConsole;
    if (!console->IsOk())
        return;
    AddDrawHook(&DrawHook, 500);
    AddDrawHook(&DrawPathingInfo, 450);
}
#endif
