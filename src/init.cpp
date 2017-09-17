#include "init.h"

#include <algorithm>

#include "console/windows_wrap.h"
#include "constants/order.h"
#include "constants/sprite.h"
#include "constants/unit.h"
#include "constants/weapon.h"
#include "ai.h"
#include "bullet.h"
#include "image.h"
#include "limits.h"
#include "player.h"
#include "replay.h"
#include "sprite.h"
#include "strings.h"
#include "unit.h"
#include "yms.h"
#include "warn.h"

#include "log.h"

static int GetDecodedImageSize(const void *data_, int padding)
{
    uint8_t *data = (uint8_t *)data_;
    int image_size = 0;
    int frame_count = *(uint16_t *)data;
    GrpFrameHeader *frame = (GrpFrameHeader *)(data + 6);
    for (int i = 0; i < frame_count; i++)
    {
        int padded_w = ((frame->w + (padding - 1)) & ~(padding - 1)) + 2 * (padding - 1);
        image_size += 2 + padded_w * frame->h;
        frame++;
    }
    return 6 + frame_count * sizeof(GrpFrameHeader) + image_size + 1;
}

// The grp gets transparent padding both left and right, to make loop unrolling possible
static bool DecodeGrp(const void *input_, void *out_, int padding)
{
    uint8_t *out = (uint8_t *)out_;
    const uint8_t *input = (const uint8_t *)input_;
    int frame_count = *(const uint16_t *)input;
    const GrpFrameHeader *in_frames = (const GrpFrameHeader *)(input + 6);
    GrpFrameHeader *out_frames = (GrpFrameHeader *)(out + 6);
    uint8_t *out_img = out + 6 + frame_count * sizeof(GrpFrameHeader);
    memcpy(out, input, 6);
    for (int i = 0; i < frame_count; i++)
    {
        const GrpFrameHeader *in_frame = in_frames++;
        GrpFrameHeader *out_frame = out_frames++;
        *out_frame = *in_frame;
        // Bw assumes it has not been converted to pointer yet
        out_frame->frame = (uint8_t *)(out_img - out);
        *out_img++ = 0; // Marks frame as decoded
        *out_img++ = 0;
        int padded_w = ((in_frame->w + (padding - 1)) & ~(padding - 1)) + 2 * (padding - 1);
        uint8_t *frame_end = out_img + padded_w * in_frame->h;
        uint8_t *line_start = out_img + padding - 1;
        uint8_t *line_end = line_start + in_frame->w;
        uint8_t *padded_end = out_img + padded_w;
        const uint16_t *in_frame_lines = (const uint16_t *)(input + (uintptr_t)in_frame->frame);
        while (out_img != frame_end)
        {
            while (out_img != line_start)
                *out_img++ = 0;
            const uint8_t *in_grp = (input + (uintptr_t)in_frame->frame + *in_frame_lines++);
            Assert(out_img < line_end);
            while (out_img != line_end)
            {
                uint8_t val = *in_grp++;
                if (val & 0x80)
                {
                    val &= ~0x80;
                    if (out_img + val > line_end)
                        return false;
                    memset(out_img, 0, val);
                    out_img += val;
                }
                else if (val & 0x40)
                {
                    val &= ~0x40;
                    if (out_img + val > line_end)
                        return false;
                    uint8_t color = *in_grp++;
                    memset(out_img, color, val);
                    out_img += val;
                }
                else
                {
                    if (out_img + val > line_end)
                        return false;
                    memcpy(out_img, in_grp, val);
                    out_img += val;
                    in_grp += val;
                }
            }
            while (out_img != padded_end)
                *out_img++ = 0;
            line_end += padded_w;
            line_start += padded_w;
            padded_end += padded_w;
        }
    }
    return true;
}

static bool IsDecodedDrawFunc(int drawfunc)
{
    switch (drawfunc)
    {
        case Image::Remap:
        case Image::Normal:
        case Image::NormalSpecial:
        // The cloak drawfuncs may be set by mods
        case Image::DetectedCloaking:
        case Image::DetectedCloak:
        case Image::DetectedDecloaking:
        case Image::Cloaking:
        case Image::Cloak:
        case Image::Decloaking:
        case Image::Shadow:
            return true;
        default:
            return false;
    }
}

void *LoadGrp(ImageType image_id, uint32_t *images_dat_grp, Tbl *images_tbl, GrpSprite **loaded_grps, void **overlapped, void **out_file)
{
    ImageType::UpdateGrpArray(loaded_grps, image_id);

    uint32_t grp = image_id.GrpId();
    int drawfunc = image_id.DrawFunc();
    bool is_decoded = IsDecodedDrawFunc(drawfunc);
    for (int i = 0; i < image_id; i++)
    {
        ImageType other(i);
        bool other_decoded = IsDecodedDrawFunc(other.DrawFunc());
        if (other.GrpId() == grp && is_decoded == other_decoded)
        {
            return loaded_grps[i];
        }
    }
    char filename[260];
    snprintf(filename, sizeof filename, "unit\\%s", images_tbl->GetTblString(grp));
    void *file = bw::OpenGrpFile(filename);
    uint32_t size = storm::SFileGetFileSize(file, nullptr);
    if (size == 0xffffffff)
        bw::FileError(file, GetLastError());
    if (size == 0)
        bw::ErrorMessageBox(ERROR_NO_MORE_FILES, filename); // Huh?
    void *data = storm::SMemAlloc(size, __FILE__, __LINE__, 0);
    if (!is_decoded)
    {
        overlapped[4] = CreateEvent(NULL, TRUE, FALSE, NULL);
        bw::ReadFile_Overlapped(overlapped, size, data, file);
        *out_file = file;
        return data;
    }
    else
    {
        bw::ReadFile_Overlapped(nullptr, size, data, file);
        int image_size = GetDecodedImageSize(data, grp_padding_size);
        void *decoded = storm::SMemAlloc(image_size, __FILE__, __LINE__, 0);
        bool success = DecodeGrp(data, decoded, grp_padding_size);
        if (!success)
            FatalError("%s appears to be corrupt. Was it created with RetroGRP?", filename);
        storm::SFileCloseFile(file);
        storm::SMemFree(data, __FILE__, __LINE__, 0);
        return decoded;
    }
}

void LoadBlendPalettes(const char *tileset)
{
    for (int i = 0; i < 7; i++)
    {
        BlendPalette *palette = &bw::blend_palettes[i + 1];
        char buf[260];
        snprintf(buf, sizeof buf, "tileset\\%s\\%s.pcx", tileset, palette->name);
        uint32_t size;
        void *pcx = bw::ReadMpqFile(buf, 0, 0, __FILE__, __LINE__, 0, &size);
        uint32_t x, y;
        storm::SBmpDecodeImage(2, pcx, size, 0, nullptr, 0, &x, &y, 0);
        Assert(x == 256);
        int decoded_size = (y + 1) * 256;
        uint8_t *decoded = (uint8_t *)storm::SMemAlloc(decoded_size, __FILE__, __LINE__, 0);
        // Full transparency, not done by bw, but needed with decoded grps
        for (int i = 0; i < 256; i++)
        {
            decoded[i] = i;
        }
        if (storm::SBmpDecodeImage(2, pcx, size, 0, decoded + 256, decoded_size - 256, nullptr, nullptr, 0) == 0)
        {
            // Dunno about GetLastError
            bw::ErrorMessageBox(GetLastError(), buf);
        }
        storm::SMemFree(pcx, __FILE__, __LINE__, 0);
        palette->data = decoded;
    }
}

void InitCursorMarker()
{
    Sprite *cursor_marker = lone_sprites->AllocateLone(SpriteId::CursorMarker, Point(0, 0), 0);
    *bw::cursor_marker = cursor_marker;
    cursor_marker->flags |= 0x20;
    bw::SetVisibility(cursor_marker, 0);
}

static void InitBullets()
{
    *bw::first_active_bullet = nullptr;
    *bw::last_active_bullet = nullptr;
}

int InitGame()
{
    for (auto msg : bw::chat_messages)
    {
        std::fill(msg.begin(), msg.end(), 0);
    }
    bw::InitText();
    bw::InitAi();
    bw::InitTerrain();
    bw::InitImages();
    bw::InitSprites();
    InitCursorMarker();
    bw::InitFlingies();
    InitBullets();
    // Unlike most other init functions, this does not load any .dat files
    //InitOrders();
    bw::InitColorCycling();
    bw::UpdateColorPaletteIndices(bw::current_palette_rgba.raw_pointer(), bw::FindClosestIndex.raw_pointer());
    bw::InitTransparency();
    if (!*bw::loaded_save)
        bw::InitScoreSupply(); // TODO: Breaks team game replays on single
    bw::InitPylonSystem();
    bw::LoadMiscDat();
    bw::InitUnitSystem();
    UnitType::InitializeStrength();
    bw::InitSpriteVisionSync();
    if (*bw::loaded_save)
        return 1;

    if (bw::LoadChk() != 0)
    {
        // No IsTeamGame(), otherwise team game replays can't be watched in single player
        if (*bw::team_game)
            bw::CreateTeamGameStartingUnits();
        else
            bw::CreateStartingUnits();
        bw::InitScreenPositions();
        bw::InitTerrainAi();
        return 1;
    }
    else
    {
        if (!*bw::error_happened && !*bw::nooks_and_crannies_error)
        {
            bw::Storm_LeaveGame(3);
            *bw::error_happened = 1;
            bw::BwError(nullptr, 0, nullptr, 0x61);
            // Guess this doesn't make sense here?
            // State 3 == ingame
            if (*bw::scmain_state == 3)
            {
                *bw::draw_sprites = 0;
                *bw::next_scmain_state = 4;
                if (!IsReplay())
                    bw::replay_header->replay_end_frame = *bw::frame_count;
            }
            bw::CloseBnet();
        }
        return 0;
    }
}

void InitStartingRacesAndTypes()
{
    for (int i = 0; i < Limits::Players; i++)
    {
        bw::starting_player_types[i] = bw::players[i].type;
        // Replays have starting race saved in them, in order to have team game
        // starting unit creation to work
        if (!IsReplay())
            bw::save_races[i] = bw::players[i].race;
    }
    *bw::loaded_local_player_id = *bw::local_player_id;
}

static void CreateResourceAreas(vector<uint8_t> *res_states, int state) {
    struct PotentialResArea {
        Rect16 area;
        int val;
        Unit *unit;
    };
    vector<PotentialResArea> potential_resource_areas;
    potential_resource_areas.reserve(100);
    auto i = 0;
    for (Unit *unit : *bw::first_active_unit) {
        if ((*res_states)[i] == state) {
            Rect16 rect(unit->sprite->position, 0x3c0);
            int topleft = (rect.top << 16) | rect.left;
            int botright = (rect.bottom << 16) | rect.right;
            potential_resource_areas.emplace_back(PotentialResArea {
                rect,
                bw::HighestResourceConcentration(unit, topleft, botright),
                unit,
            });
        }
        i += 1;
    }
    for (auto i = 0; i < potential_resource_areas.size(); i++) {
        auto largest = potential_resource_areas[0].val;
        auto largest_index = 0;
        for (auto j = 1; j < i; j++) {
            if (potential_resource_areas[j].val > largest) {
                largest = potential_resource_areas[j].val;
                largest_index = j;
            }
        }
        potential_resource_areas[largest_index].val = 0;
        x32 x_sum = 0;
        y32 y_sum = 0;
        int count = 0;
        for (Unit *unit : *bw::first_active_unit) {
            const auto &pos = unit->sprite->position;
            const auto &rect = potential_resource_areas[largest_index].area;
            uint32_t topleft = (rect.top << 16) | rect.left;
            uint32_t botright = (rect.bottom << 16) | rect.right;
            if (bw::IsOnResourceArea(unit, pos.x, pos.y, topleft, botright)) {
                x_sum += pos.x;
                y_sum += pos.y;
                count += 1;
            }
        }
        if (count != 0) {
            if (bw::resource_areas->used_count >= 0xf9) {
                return;
            }
            bw::resource_areas->used_count += 1;
            int area_id = bw::resource_areas->used_count;
            auto &area = bw::resource_areas->areas[area_id];
            x32 x = x_sum / count;
            y32 y = y_sum / count;
            Unit *unit = potential_resource_areas[largest_index].unit;
            Unit *center = bw::FindNearestMineral(res_states->data(), unit, x, y);
            area.position = center->sprite->position;
            potential_resource_areas[largest_index].unit = center;
            potential_resource_areas[largest_index].area = Rect16(center->sprite->position, 0x3c0);
            auto min_x = area.position.x;
            auto max_x = area.position.x;
            auto min_y = area.position.y;
            auto max_y = area.position.y;
            auto i = 0;
            for (Unit *unit : *bw::first_active_unit) {
                const auto &pos = unit->sprite->position;
                const auto &rect = potential_resource_areas[largest_index].area;
                const auto &area_pos = area.position;
                uint32_t topleft = (rect.top << 16) | rect.left;
                uint32_t botright = (rect.bottom << 16) | rect.right;
                if (bw::IsOnResourceArea(unit, area_pos.x, area_pos.y, topleft, botright)) {
                    if ((*res_states)[i] == 1) {
                        (*res_states)[i] = 3;
                        area.mineral_field_count += 1;
                        area.total_minerals += unit->resource.resource_amount;
                    } else if ((*res_states)[i] == 2) {
                        (*res_states)[i] = 4;
                        area.geyser_count += 1;
                        area.total_gas += unit->resource.resource_amount;
                    }
                    unit->resource.resource_area = area_id;
                    max_x = std::max(max_x, pos.x);
                    max_y = std::max(max_y, pos.y);
                    min_x = std::min(min_x, pos.x);
                    min_y = std::min(min_y, pos.y);
                }
                i += 1;
            }
            area.position.x = (max_x + min_x) / 2;
            area.position.y = (max_y + min_y) / 2;
        }
    }
}

void InitResourceAreas() {
    memset(&*bw::resource_areas, 0, sizeof(Ai::ResourceAreaArray));
    for (int i = 0; i < Limits::ActivePlayers; i++) {
        if (bw::start_positions[i] != Point(0, 0)) {
            bool is_active_player = IsHumanPlayer(i) || IsComputerPlayer(i);
            int flags = is_active_player ? 1 : 0;
            // Yes, 0 becomes unused
            bw::resource_areas->used_count += 1;
            int area_id = bw::resource_areas->used_count;
            bw::resource_areas->areas[area_id].position = bw::start_positions[i];
            bw::resource_areas->areas[area_id].is_start_location = 1;
            bw::resource_areas->areas[area_id].flags = flags;
        }
    }
    // 0 = irrelevant, 1 = unallocated mine, 2 = unallocated gas, 3 = used mine, 4 = used gas
    vector<uint8_t> type_buf;
    type_buf.reserve(1000);
    for (Unit *unit : *bw::first_active_unit) {
        if (unit->sprite == nullptr || unit->order == OrderId::Die) {
            type_buf.emplace_back(0);
        } else if (~unit->Type().Flags() & UnitFlags::ResourceContainer) {
            type_buf.emplace_back(0);
        } else {
            if (unit->Type().IsMineralField()) {
                type_buf.emplace_back(1);
            } else {
                type_buf.emplace_back(2);
            }
            int area_id = bw::FindMatchingResourceArea(unit);
            unit->resource.resource_area = area_id;
            if (area_id != 0) {
                auto &area = bw::resource_areas->areas[area_id];
                if (unit->Type().IsMineralField()) {
                    area.mineral_field_count += 1;
                    area.total_minerals += unit->resource.resource_amount;
                    type_buf[type_buf.size() - 1] = 3;
                } else {
                    area.geyser_count += 1;
                    area.total_gas += unit->resource.resource_amount;
                    type_buf[type_buf.size() - 1] = 4;
                }
            }
        }
    }
    CreateResourceAreas(&type_buf, 2);
    CreateResourceAreas(&type_buf, 1);
}
