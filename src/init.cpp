#include "init.h"

#include "image.h"
#include "strings.h"
#include "sprite.h"
#include "console/windows_wrap.h"
#include "yms.h"
#include "unit.h"
#include "bullet.h"
#include "replay.h"
#include "player.h"
#include "limits.h"
#include "warn.h"

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

void *LoadGrp(int image_id, uint32_t *images_dat_grp, Tbl *images_tbl, GrpSprite **loaded_grps, void **overlapped, void **out_file)
{
    uint32_t grp = images_dat_grp[image_id];
    int drawfunc = images_dat_drawfunc[image_id];
    bool is_decoded = IsDecodedDrawFunc(drawfunc);
    for (int i = 0; i < image_id; i++)
    {
        bool other_decoded = IsDecodedDrawFunc(images_dat_drawfunc[i]);
        if (images_dat_grp[i] == grp && is_decoded == other_decoded)
        {
            return loaded_grps[i];
        }
    }
    char filename[260];
    snprintf(filename, sizeof filename, "unit\\%s", images_tbl->GetTblString(grp));
    void *file = OpenGrpFile(filename);
    uint32_t size = SFileGetFileSize(file, nullptr);
    if (size == 0xffffffff)
        FileError(file, GetLastError());
    if (size == 0)
        ErrorMessageBox(ERROR_NO_MORE_FILES, filename); // Huh?
    void *data = SMemAlloc(size, __FILE__, __LINE__, 0);
    if (!is_decoded)
    {
        overlapped[4] = CreateEvent(NULL, TRUE, FALSE, NULL);
        ReadFile_Overlapped(overlapped, size, data, file);
        *out_file = file;
        return data;
    }
    else
    {
        ReadFile_Overlapped(nullptr, size, data, file);
        int image_size = GetDecodedImageSize(data, grp_padding_size);
        void *decoded = SMemAlloc(image_size, __FILE__, __LINE__, 0);
        bool success = DecodeGrp(data, decoded, grp_padding_size);
        if (!success)
            FatalError("%s appears to be corrupt. Was it created with RetroGRP?", filename);
        SFileCloseFile(file);
        SMemFree(data, __FILE__, __LINE__, 0);
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
        void *pcx = ReadMpqFile(buf, 0, 0, __FILE__, __LINE__, 0, &size);
        uint32_t x, y;
        SBmpDecodeImage(2, pcx, size, 0, nullptr, 0, &x, &y, 0);
        Assert(x == 256);
        int decoded_size = (y + 1) * 256;
        uint8_t *decoded = (uint8_t *)SMemAlloc(decoded_size, __FILE__, __LINE__, 0);
        // Full transparency, not done by bw, but needed with decoded grps
        for (int i = 0; i < 256; i++)
        {
            decoded[i] = i;
        }
        if (SBmpDecodeImage(2, pcx, size, 0, decoded + 256, decoded_size - 256, nullptr, nullptr, 0) == 0)
        {
            // Dunno about GetLastError
            ErrorMessageBox(GetLastError(), buf);
        }
        SMemFree(pcx, __FILE__, __LINE__, 0);
        palette->data = decoded;
    }
}

void InitCursorMarker()
{
    Sprite *cursor_marker = lone_sprites->AllocateLone(Sprite::CursorMarker, Point(0, 0), 0);
    *bw::cursor_marker = cursor_marker;
    cursor_marker->flags |= 0x20;
    SetVisibility(cursor_marker, 0);
}

static int GenerateStrength(int unit_id, uint8_t *weapon_arr)
{
    switch (unit_id)
    {
        case Unit::Larva:
        case Unit::Egg:
        case Unit::Cocoon:
        case Unit::LurkerEgg:
            return 0;
        case Unit::Carrier:
        case Unit::Gantrithor:
            return GenerateStrength(Unit::Interceptor, weapon_arr);
        case Unit::Reaver:
        case Unit::Warbringer:
            return GenerateStrength(Unit::Scarab, weapon_arr);
        default:
            if (units_dat_subunit[unit_id] != Unit::None)
                unit_id = units_dat_subunit[unit_id];
            int weapon = weapon_arr[unit_id];
            if (weapon == Weapon::None)
                return 1;
            // Fixes ai hangs when using zero damage weapons as main weapon
            return std::max(2, (int)FinetuneBaseStrength(unit_id, CalculateBaseStrength(weapon, unit_id)));
    }
}

// Fixes ai hangs with some mods (See GenerateStrength comment)
static void GenerateStrengthTable()
{
    for (int i = 0; i < Unit::None; i++)
    {
        int ground_str = GenerateStrength(i, &*units_dat_ground_weapon);
        int air_str = GenerateStrength(i, &*units_dat_air_weapon);
        // Dunno
        if (air_str == 1 && ground_str > air_str)
            air_str = 0;
        bw::unit_strength[i] = air_str;
        if (ground_str == 1 && air_str > ground_str)
            ground_str = 0;
        bw::unit_strength[Unit::None + i] = ground_str;
    }
}

static void InitBullets()
{
    *bw::first_active_bullet = nullptr;
    *bw::last_active_bullet = nullptr;
}

int InitGame()
{
    memset(bw::chat_messages.v(), 0, 218 * 13);
    InitText();
    InitAi();
    InitTerrain();
    InitImages();
    InitSprites();
    InitCursorMarker();
    InitFlingies();
    InitBullets();
    // Unlike most other init functions, this does not load any .dat files
    //InitOrders();
    InitColorCycling();
    UpdateColorPaletteIndices(bw::current_palette_rgba.v(), bw::FindClosestIndex.v());
    InitTransparency();
    if (!*bw::loaded_save)
        InitScoreSupply(); // TODO: Breaks team game replays on single
    InitPylonSystem();
    LoadMiscDat();
    InitUnitSystem();
    GenerateStrengthTable();
    InitSpriteVisionSync();
    if (*bw::loaded_save)
        return 1;

    if (LoadChk() != 0)
    {
        // No IsTeamGame(), otherwise team game replays can't be watched in single player
        if (*bw::team_game)
            CreateTeamGameStartingUnits();
        else
            CreateStartingUnits();
        InitScreenPositions();
        InitTerrainAi();
        return 1;
    }
    else
    {
        if (!*bw::error_happened && !*bw::nooks_and_crannies_error)
        {
            Storm_LeaveGame(3);
            *bw::error_happened = 1;
            BwError(nullptr, 0, nullptr, 0x61);
            // Guess this doesn't make sense here?
            // State 3 == ingame
            if (*bw::scmain_state == 3)
            {
                *bw::draw_sprites = 0;
                *bw::next_scmain_state = 4;
                if (!IsReplay())
                    bw::replay_header[0].replay_end_frame = *bw::frame_count;
            }
            CloseBnet();
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
