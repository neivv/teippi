#include "image.h"

#include <atomic>

#include "constants/image.h"
#include "bullet.h"
#include "draw.h"
#include "lofile.h"
#include "log.h"
#include "offsets.h"
#include "perfclock.h"
#include "rng.h"
#include "sprite.h"
#include "strings.h"
#include "unit.h"
#include "warn.h"
#include "yms.h"

bool GrpFrameHeader::IsDecoded() const
{
    return ((uint16_t *)frame)[0] == 0;
}

int GrpFrameHeader::GetWidth() const
{
    if (IsDecoded())
    {
        int width = w;
        // Round upwards to grp_padding_size
        width += (grp_padding_size - 1);
        width &= ~(grp_padding_size - 1);
        // And add safety padding
        width += 2 * (grp_padding_size - 1);
        return width;
    }
    else
        return w;
}

uint8_t GrpFrameHeader::GetPixel(x32 x, y32 y) const
{
    if (IsDecoded())
    {
        return frame[2 + (grp_padding_size - 1) + x + y * GetWidth()];
    }
    else
    {
        uint16_t *line_offsets = (uint16_t *)frame;
        uint8_t *line = frame + line_offsets[y];
        int pos = 0;
        while (true)
        {
            uint8_t val = *line++;
            if (val & 0x80)
            {
                val &= ~0x80;
                if (x < pos + val)
                    return 0;
                pos += val;
            }
            else if (val & 0x40)
            {
                val &= ~0x40;
                uint8_t color = *line++;
                if (x < pos + val)
                    return color;
                pos += val;
            }
            else
            {
                if (x < pos + val)
                    return line[x - pos];
                pos += val;
                line += val;
            }
        }
    }
}

Image::Image()
{
    list.prev = nullptr;
    list.next = nullptr;
}

Image::Image(Sprite *parent, ImageType image_id, int x, int y) :
    image_id(image_id.Raw()), x_off(x), y_off(y), parent(parent)
{
    list.prev = nullptr;
    list.next = nullptr;
    grp = bw::image_grps[image_id];
    flags = 0;
    frameset = 0;
    frame = 0;
    direction = 0;
    grp_bounds = Rect16(0, 0, 0, 0);
    iscript.header = 0;
    iscript.pos = 0;
    iscript.return_pos = 0;
    iscript.animation = 0;
    iscript.wait = 0;

    if (Type().IsTurningGraphic())
        flags |= ImageFlags::CanTurn;
    if (Type().Clickable())
        flags |= ImageFlags::Clickable;
    if (Type().UseFullIscript())
        flags |= ImageFlags::FullIscript;

    SetDrawFunc(Type().DrawFunc(), nullptr);
    if (drawfunc == OverrideColor)
        drawfunc_param = (void *)(uintptr_t)parent->player;
    else if (drawfunc == Remap)
        drawfunc_param = bw::blend_palettes[Type().Remapping()].data;
}

bool Image::InitIscript(Iscript::Context *ctx)
{
    int iscript_header = Type().IscriptHeader();
    bool success = iscript.Initialize(ctx->iscript, iscript_header);
    if (!success)
    {
        Warning("Image %s has an invalid iscript header: %d (0x%x)",
                DebugStr().c_str(), iscript_header, iscript_header);
        return false;
    }
    SetIscriptAnimation(ctx, Iscript::Animation::Init);
    bw::PrepareDrawImage(this);
    return true;
}

#ifdef SYNC
void *Image::operator new(size_t size)
{
    auto ret = new uint8_t[size];
    if (SyncTest)
        ScrambleStruct(ret, size);
    return ret;
}
#endif

void Image::SingleDelete()
{
    if (~*bw::image_flags & 1)
        bw::MarkImageAreaForRedraw(this);

    if (list.next)
        list.next->list.prev = list.prev;
    else
    {
        Assert(parent->last_overlay == this);
        parent->last_overlay = list.prev;
    }

    if (list.prev)
        list.prev->list.next = list.next;
    else
    {
        Assert(parent->first_overlay == this);
        parent->first_overlay = list.next;
    }

    // Bw does not do this and works fine...
    // Actually, bw would crash in some cases otherwise,
    // now they have to be found and fixed ._.
    if (parent->main_image == this)
        parent->main_image = nullptr;

    delete this;
}

void Image::UpdateSpecialOverlayPos()
{
    if (parent->main_image)
        LoFile::GetOverlay(parent->main_image->Type(), 2).SetImageOffset(this);
}

void Image::SetFlipping(bool set)
{
    bool state = (flags & ImageFlags::Flipped) != 0;
    if (state != set)
    {
        flags = flags | (set << 1) | ImageFlags::Redraw;
        SetDrawFunc(drawfunc, drawfunc_param);
    }
}

void Image::UpdateFrameToDirection()
{
    if (frameset + direction != frame)
    {
        frame = frameset + direction;
        flags |= ImageFlags::Redraw;
    }
}

void Image::Show()
{
    if (IsHidden())
    {
        flags &= ~ImageFlags::Hidden;
        flags |= ImageFlags::Redraw;
    }
}

void Image::Hide()
{
    if (!IsHidden())
    {
        if (~*bw::image_flags & 0x1)
            bw::MarkImageAreaForRedraw(this);
        flags |= ImageFlags::Hidden;
    }
}

void Image::SetOffset(int x, int y)
{
    if (x_off != x || y_off != y)
        flags |= ImageFlags::Redraw;
    x_off = x;
    y_off = y;
}

void Image::SetFrame(int new_frame)
{
    if (frameset != new_frame)
    {
        frameset = new_frame;
        if (frame != frameset + direction)
        {
            frame = frameset + direction;
            flags |= ImageFlags::Redraw;
        }
    }
}

void Image::SetDrawFunc(int drawfunc_, void *param)
{
    drawfunc = drawfunc_;
    drawfunc_param = param;
    if (IsFlipped())
        Render = bw::image_renderfuncs[drawfunc].flipped;
    else
        Render = bw::image_renderfuncs[drawfunc].nonflipped;
    if (drawfunc == WarpFlash)
        drawfunc_param = (void *)0x230;
    if (flags & ImageFlags::UseParentLo)
        UpdateSpecialOverlayPos();
    flags |= ImageFlags::Redraw;
}

void Image::MakeDetected()
{
    // See comment in Test_DrawFuncSync why making every image detected is dangerous
    // Then again, disable doodad state cloaks units without animation, so we'll have to allow that
    bool can_detect_noncloaked = parent->main_image != nullptr && parent->main_image->drawfunc == Image::Normal;
    if (can_detect_noncloaked && drawfunc == DrawFunc::Normal)
        SetDrawFunc(DrawFunc::DetectedCloak, 0);
    else if (drawfunc >= DrawFunc::Cloaking && drawfunc <= DrawFunc::Decloaking)
        SetDrawFunc(drawfunc + 3, drawfunc_param);
}

void Image::FollowMainImage()
{
    direction = parent->main_image->direction;
    SetFlipping(parent->main_image->IsFlipped());
    UpdateFrameToDirection();
}

Image *Image::Iscript_AddOverlay(Iscript::Context *ctx, ImageType overlay_id, int x, int y, bool above)
{
    Image *img = new Image(parent, overlay_id, x, y);
    if (above)
    {
        img->list.prev = nullptr;
        img->list.next = parent->first_overlay;
        parent->first_overlay->list.prev = img;
        parent->first_overlay = img;
    }
    else
    {
        img->list.next = nullptr;
        img->list.prev = parent->last_overlay;
        parent->last_overlay->list.next = img;
        parent->last_overlay = img;
    }
    bool success = img->InitIscript(ctx);
    if (!success)
    {
        img->SingleDelete();
        return nullptr;
    }
    if (img->flags & ImageFlags::CanTurn)
    {
        if (IsFlipped())
            bw::SetImageDirection32(img, 32 - direction);
        else
            bw::SetImageDirection32(img, direction);
    }
    if (img->frame != img->frameset + img->direction)
    {
        img->frame = img->frameset + img->direction;
        img->flags |= ImageFlags::Redraw;
    }
    ctx->NewOverlay(img);
    return img;
}

Iscript::CmdResult Image::HandleIscriptCommand(Iscript::Context *ctx, Iscript::Script *script,
                                               const Iscript::Command &cmd)
{
    using namespace Iscript::Opcode;
    switch (cmd.opcode)
    {
        case PlayFram:
            if (cmd.val + direction < grp->frame_count)
                SetFrame(cmd.val);
            else
                Warning("Iscript for image %s sets image to invalid frame %x", DebugStr().c_str(), cmd.val);
        break;
        case PlayFramTile:
            if (cmd.val + *bw::tileset < grp->frame_count)
                SetFrame(cmd.val + *bw::tileset);
        break;
        case SetHorPos:
            SetOffset(cmd.point.x, y_off);
        break;
        case SetVertPos:
            SetOffset(x_off, cmd.point.y);
        break;
        case SetPos:
            SetOffset(cmd.point.x, cmd.point.y);
        break;
        case ImgOl:
            Iscript_AddOverlay(ctx, ImageType(cmd.val), x_off + cmd.point.x, y_off + cmd.point.y, true);
        break;
        case ImgUl:
            Iscript_AddOverlay(ctx, ImageType(cmd.val), x_off + cmd.point.x, y_off + cmd.point.y, false);
        break;
        case ImgOlOrig:
        case SwitchUl:
        {
            Image *other = Iscript_AddOverlay(ctx, ImageType(cmd.val), 0, 0, cmd.opcode == ImgOlOrig);
            if (other != nullptr && ~other->flags & ImageFlags::UseParentLo)
            {
                other->flags |= ImageFlags::UseParentLo;
                bw::SetOffsetToParentsSpecialOverlay(other);
            }
        }
        break;
        case ImgOlUseLo:
        case ImgUlUseLo:
        {
            // Yeah, it's not actually point
            Point32 point = LoFile::GetOverlay(Type(), cmd.point.x).GetValues(this, cmd.point.y);
            bool above = cmd.opcode == ImgOlUseLo;
            Iscript_AddOverlay(ctx, ImageType(cmd.val), point.x + x_off, point.y + y_off, above);
        }
        break;
        case ImgUlNextId:
            Iscript_AddOverlay(ctx, ImageType(image_id + 1), cmd.point.x + x_off, cmd.point.y + y_off, false);
        break;
        case SprOl:
            // Bullet's iscript handler has an override for goliath range upgrade
            Sprite::Spawn(this, SpriteType(cmd.val), cmd.point, parent->elevation + 1);
        break;
        case HighSprOl:
            Sprite::Spawn(this, SpriteType(cmd.val), cmd.point, parent->elevation - 1);
        break;
        case LowSprUl:
            Sprite::Spawn(this, SpriteType(cmd.val), cmd.point, 1);
        break;
        case UflUnstable:
        {
            Warning("Flingy creation not implemented (image %s)", DebugStr().c_str());
            /*Flingy *flingy = CreateFlingy(cmd.val, parent->player, cmd.point);
            if (flingy)
            {
                flingy->GiveRandomMoveTarget(ctx->rng);
                flingy->sprite->UpdateVisibilityPoint();
            }*/
        }
        break;
        case SprUlUseLo:
        case SprUl:
        {
            int elevation = parent->elevation;
            if (cmd.opcode == SprUl)
                elevation -= 1;
            Sprite *sprite = Sprite::Spawn(this, SpriteType(cmd.val), cmd.point, elevation);
            if (sprite != nullptr)
            {
                if (IsFlipped())
                    sprite->SetDirection32(32 - direction);
                else
                    sprite->SetDirection32(direction);
            }
        }
        break;
        case SprOlUseLo:
        {
            // Again using the "point" for additional storage
            Point32 point = LoFile::GetOverlay(Type(), cmd.point.x).GetValues(this, 0);
            Sprite *sprite = Sprite::Spawn(this, SpriteType(cmd.val), point.ToPoint16(), parent->elevation + 1);
            if (sprite)
            {
                if (IsFlipped())
                    sprite->SetDirection32(32 - direction);
                else
                    sprite->SetDirection32(direction);
            }
        }
        break;
        case SetFlipState:
            SetFlipping(cmd.val);
        break;
        case PlaySnd:
        case PlaySndRand:
        case PlaySndBtwn:
        {
            int sound_id;
            if (cmd.opcode == PlaySnd)
                sound_id = cmd.val;
            else if (cmd.opcode == PlaySndRand)
                sound_id = *(uint16_t *)(cmd.data + 1 + ctx->rng->Rand(cmd.data[0]) * 2);
            else // PlaySndBtwn
                sound_id = cmd.val1() + ctx->rng->Rand(cmd.val2() - cmd.val1() + 1);
            bw::PlaySoundAtPos(sound_id, parent->position.AsDword(), 1, 0);
        }
        break;
        case FollowMainGraphic:
            if (parent->main_image != nullptr)
            {
                Image *main_img = parent->main_image;
                if (main_img->frame != frame || main_img->IsFlipped() != IsFlipped())
                {
                    int new_frame = main_img->frameset + main_img->direction;
                    if (new_frame >= grp->frame_count)
                    {
                        Warning("Iscript for image %s requested to play frame %x with followmaingraphic",
                                DebugStr().c_str(), new_frame);
                    }
                    else
                    {
                        frameset = main_img->frameset;
                        FollowMainImage();
                    }
                }
            }
        break;
        case EngFrame:
        case EngSet:
            if (cmd.opcode == EngFrame)
                frameset = cmd.val;
            else // EndSet
                frameset = parent->main_image->frameset + parent->main_image->grp->frame_count * cmd.val;
            FollowMainImage();
        break;
        case HideCursorMarker:
            *bw::draw_cursor_marker = 0;
        break;
        case TmpRmGraphicStart:
            Hide();
        break;
        case TmpRmGraphicEnd:
            Show();
        break;
        case WarpOverlay:
            flags |= ImageFlags::Redraw;
            drawfunc_param = (void *)cmd.val;
        break;
        case GrdSprOl:
        {
            int x = parent->position.x + x_off + cmd.point.x;
            int y = parent->position.y + y_off + cmd.point.y;
            // Yes, it checks if unit id 0 can fit there
            if (bw::DoesFitHere(UnitId::Marine, x, y))
                Sprite::Spawn(this, SpriteType(cmd.val), cmd.point, parent->elevation + 1);
        }
        break;
        default:
            return ConstIscriptCommand(ctx, script, cmd);
        break;
    }
    return Iscript::CmdResult::Handled;
}

Iscript::CmdResult Image::ConstIscriptCommand(Iscript::Context *ctx, Iscript::Script *script,
                                              const Iscript::Command &cmd) const
{
    using namespace Iscript::Opcode;
    using Iscript::CmdResult;
    switch (cmd.opcode)
    {
        case Goto:
            script->pos = cmd.pos;
        break;
        case Call:
            script->return_pos = script->pos;
            script->pos = cmd.pos;
        break;
        case Return:
            script->pos = script->return_pos;
        break;
        case RandCondJmp:
            if (cmd.val > ctx->rng->Rand(256))
                script->pos = cmd.pos;
        break;
        case Wait:
            script->wait = cmd.val - 1;
            return CmdResult::Stop;
        break;
        case WaitRand:
            script->wait = cmd.val1() + ctx->rng->Rand(cmd.val2() - cmd.val1() + 1);
            return CmdResult::Stop;
        break;
        case End:
            // This generally gets handled by sprite, but needs to be doen here as well
            // for the speed prediction.
            return CmdResult::Stop;
        break;
        case PwrupCondJmp:
            if (parent && parent->main_image != this)
                script->pos = cmd.pos;
        break;
        default:
            return CmdResult::NotHandled;
        break;
    }
    return CmdResult::Handled;
}

void Image::SetIscriptAnimation(Iscript::Context *ctx, int anim)
{
    using namespace Iscript::Animation;
    if (anim == Death && iscript.animation == Death)
        return;
    if (~flags & ImageFlags::FullIscript && anim != Death && anim != Init) // 0x10 = full iscript
        return;
    if (anim == iscript.animation && (anim == Walking || anim == Working))
        return;
    if (anim == GndAttkRpt)
    {
        if (iscript.animation != GndAttkRpt && iscript.animation != GndAttkInit)
            anim = GndAttkInit;
    }
    else if (anim == AirAttkRpt)
    {
        if (iscript.animation != AirAttkRpt && iscript.animation != AirAttkInit)
            anim = AirAttkInit;
    }
    uint8_t *pos = *bw::iscript + iscript.header;
    if (*(int32_t *)(pos + 4) >= anim)
    {
        int anim_off = *(uint16_t *)(pos + 8 + anim * sizeof(uint16_t));
        if (anim_off != 0)
        {
            iscript.animation = anim;
            iscript.pos = anim_off;
            iscript.return_pos = 0;
            iscript.wait = 0;
            ProgressFrame(ctx);
            return;
        }
    }
    Warning("SetIscriptAnimation: Image %s does not have animation %s",
            DebugStr().c_str(), Iscript::Script::AnimationName(anim));
}

void Image::DrawFunc_ProgressFrame(Iscript::Context *ctx)
{
    if (drawfunc == DrawFunc::Cloaking || drawfunc == DrawFunc::DetectedCloaking)
    {
        uint32_t val = ((uint32_t)drawfunc_param);
        uint8_t counter = (val & 0xff00) >> 8;
        uint8_t state = val & 0xff;
        if (counter-- != 0)
        {
            drawfunc_param = (void *)((counter << 8) | state);
            return;
        }
        drawfunc_param = (void *)((state + 1) | 0x300);
        flags |= ImageFlags::Redraw;
        if (state + 1 >= 8)
        {
            SetDrawFunc(drawfunc + 1, 0);
            ctx->cloak_state = Iscript::Context::Cloaked;
        }
    }
    else if (drawfunc == DrawFunc::Decloaking || drawfunc == DrawFunc::DetectedDecloaking)
    {
        uint32_t val = ((uint32_t)drawfunc_param);
        uint8_t counter = (val & 0xff00) >> 8;
        uint8_t state = val & 0xff;
        if (counter-- != 0)
        {
            drawfunc_param = (void *)((counter << 8) | state);
            return;
        }
        drawfunc_param = (void *)((state - 1) | 0x300);
        flags |= ImageFlags::Redraw;
        if (state - 1 == 0)
        {
            SetDrawFunc(DrawFunc::Normal, 0);
            ctx->cloak_state = Iscript::Context::Decloaked;
        }
    }
    else if (drawfunc == DrawFunc::WarpFlash)
    {
        uint32_t val = ((uint32_t)drawfunc_param);
        uint8_t counter = (val & 0xff00) >> 8;
        uint8_t state = val & 0xff;
        if (counter-- != 0)
        {
            drawfunc_param = (void *)((counter << 8) | state);
            return;
        }
        flags |= ImageFlags::Redraw;
        if (state < 0x3f)
        {
            drawfunc_param = (void *)((state + 1) | 0x300);
            return;
        }
        ctx->order_signal |= 0x1;
        SetIscriptAnimation(ctx, Iscript::Animation::Death);
    }
}

static std::atomic<Tbl *> images_tbl;
/// Thread-safely loads/gives loaded images.tbl and keeps it in memory forever.
static Tbl *GetImagesTbl()
{
    const auto release = std::memory_order_release;
    const auto acquire = std::memory_order_acquire;
    Tbl *tbl = images_tbl.load(acquire);
    if (tbl == nullptr)
    {
        uint32_t size;
        Tbl *read_tbl = (Tbl *)bw::ReadMpqFile("arr\\images.tbl", 0, 0, "storm", 0, 0, &size);
        Assert(read_tbl != nullptr);
        if (images_tbl.compare_exchange_strong(tbl, read_tbl, release, acquire) == false)
            storm::SMemFree(read_tbl, __FILE__, __LINE__, 0);
        else
            tbl = read_tbl;
    }
    return tbl;
}
std::string Image::DebugStr() const
{
    Tbl *tbl = GetImagesTbl();
    int grp_id = Type().Grp();
    char buf[128];
    snprintf(buf, sizeof buf / sizeof(buf[0]), "%x [unit\\%s]", image_id, tbl->GetTblString(grp_id));
    return buf;
}

// Could only have one padding between lines,
// it can work as both left/right padding
template <class Operation>
void Render_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, Operation op)
{
    const int loop_unroll_count = grp_padding_size;

    Surface *surface = *bw::current_canvas;
    uint8_t *surface_pos = surface->image + x + y * surface->w;
    x32 skip = rect->left;
    x32 draw_width = ((rect->right + (loop_unroll_count - 1)) & ~(loop_unroll_count - 1));
    int surface_add = surface->w - draw_width;
    uint8_t *img = frame_header->frame;
    int img_width = frame_header->GetWidth();
    int img_add = img_width - draw_width;
    uint8_t *image_pos = img + 2 + img_width * rect->top + loop_unroll_count - 1; // + 2 is for the two zeroes signifying decoded img
    image_pos += skip;
    if (x + draw_width >= surface->w)
    {
        int sub = x + draw_width - surface->w;
        image_pos -= sub;
        surface_pos -= sub;
    }
    for (y32 line_count = rect->bottom; line_count != 0; line_count--)
    {
        for (x32 x = 0; x < draw_width; x += loop_unroll_count)
        {
            // Clang seems only unroll up to 8 iterations, which might be better
            for (int i = 0; i < loop_unroll_count / 2; i++)
            {
                op(image_pos, surface_pos);
                image_pos++;
                surface_pos++;
            }
            for (int i = 0; i < loop_unroll_count / 2; i++)
            {
                op(image_pos, surface_pos);
                image_pos++;
                surface_pos++;
            }
        }
        image_pos += img_add;
        surface_pos += surface_add;
    }
}

template <class Operation>
void Render_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, Operation op)
{
    const int loop_unroll_count = grp_padding_size;

    Surface *surface = *bw::current_canvas;
    x32 skip = rect->left;
    x32 draw_width = ((rect->right + (loop_unroll_count - 1)) & ~(loop_unroll_count - 1));
    uint8_t *surface_pos = surface->image + x + y * surface->w;
    int surface_add = surface->w - draw_width;
    uint8_t *img = frame_header->frame;
    int img_width = frame_header->GetWidth();
    int img_add = img_width + draw_width;
    uint8_t *image_pos = img + 2 + img_width * rect->top + loop_unroll_count - 1 + frame_header->w - 1; // + 2 is for the two zeroes signifying decoded img
    image_pos -= skip;
    if (x + draw_width >= surface->w)
    {
        int sub = x + draw_width - surface->w;
        image_pos += sub;
        surface_pos -= sub;
    }
    for (y32 line_count = rect->bottom; line_count != 0; line_count--)
    {
        for (x32 x = 0; x < draw_width; x += loop_unroll_count)
        {
            for (int i = 0; i < loop_unroll_count / 2; i++)
            {
                op(image_pos, surface_pos);
                image_pos--;
                surface_pos++;
            }
            for (int i = 0; i < loop_unroll_count / 2; i++)
            {
                op(image_pos, surface_pos);
                image_pos--;
                surface_pos++;
            }
        }
        image_pos += img_add;
        surface_pos += surface_add;
    }
}

void __fastcall DrawBlended_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *param)
{
    uint8_t *blend_table = (uint8_t *)param;
    Render_NonFlipped(x, y, frame_header, rect, [&](uint8_t *in, uint8_t *out) {
        *out = blend_table[*in << 8 | *out];
    });
}

void __fastcall DrawBlended_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *param)
{
    uint8_t *blend_table = (uint8_t *)param;
    Render_Flipped(x, y, frame_header, rect, [&](uint8_t *in, uint8_t *out) {
        *out = blend_table[*in << 8 | *out];
    });
}

void __fastcall DrawNormal_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused)
{
    STATIC_PERF_CLOCK(Dn2);
    uint8_t *remap = (uint8_t *)bw::default_grp_remap.raw_pointer();
    Render_NonFlipped(x, y, frame_header, rect, [&](uint8_t *in, uint8_t *out) {
        *out = *in ? remap[*in] : *out;
    });
}

void __fastcall DrawNormal_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused)
{
    STATIC_PERF_CLOCK(Dn2);
    uint8_t *remap = (uint8_t *)bw::default_grp_remap.raw_pointer();
    Render_Flipped(x, y, frame_header, rect, [&](uint8_t *in, uint8_t *out) {
        *out = *in ? remap[*in] : *out;
    });
}

void DrawUncloakedPart_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, int state)
{
    uint8_t *remap = (uint8_t *)bw::default_grp_remap.raw_pointer();
    Render_NonFlipped(x, y, frame_header, rect, [&](uint8_t *in, uint8_t *out) {
        uint8_t color = remap[*in];
        if (bw::cloak_distortion[color] > state)
            *out = color;
    });
}

void DrawUncloakedPart_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, int state)
{
    uint8_t *remap = (uint8_t *)bw::default_grp_remap.raw_pointer();
    Render_Flipped(x, y, frame_header, rect, [&](uint8_t *in, uint8_t *out) {
        uint8_t color = remap[*in];
        if (bw::cloak_distortion[color] > state)
            *out = color;
    });
}

void DrawCloaked_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused)
{
    uint8_t *remap = (uint8_t *)bw::cloak_remap_palette.raw_pointer();
    uint8_t *surface = (*bw::current_canvas)->image;
    uint8_t *surface_end = surface + resolution::screen_width * resolution::screen_height;
    Render_NonFlipped(x, y, frame_header, rect, [&](uint8_t *in, uint8_t *out) {
        uint8_t pos = remap[*in];
        uint8_t *out_pos = out + pos;
        if (out_pos >= surface_end)
            out_pos -= resolution::screen_width * resolution::screen_height;
        *out = *out_pos;
    });
}

void DrawCloaked_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused)
{
    uint8_t *remap = (uint8_t *)bw::cloak_remap_palette.raw_pointer();
    uint8_t *surface = (*bw::current_canvas)->image;
    uint8_t *surface_end = surface + resolution::screen_width * resolution::screen_height;
    Render_Flipped(x, y, frame_header, rect, [&](uint8_t *in, uint8_t *out) {
        uint8_t pos = remap[*in];
        uint8_t *out_pos = out + pos;
        if (out_pos >= surface_end)
            out_pos -= resolution::screen_width * resolution::screen_height;
        *out = *out_pos;
    });
}

static GrpFrameHeader *GetGrpFrameHeader(int image_id, int frame)
{
    GrpSprite *sprite = bw::image_grps[image_id];
    return (GrpFrameHeader *)(((uint8_t *)sprite) + 6 + frame * sizeof(GrpFrameHeader));
}

// TODO: million slow divisions here
void __fastcall DrawWarpTexture_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *param)
{
    Assert(frame_header->IsDecoded());
    int texture_frame = (int)param;
    GrpFrameHeader *warp_texture_header = GetGrpFrameHeader(ImageId::WarpTexture, texture_frame);
    int warp_texture_width = warp_texture_header->GetWidth();
    int frame_width = frame_header->GetWidth();
    Render_NonFlipped(x, y, warp_texture_header, rect, [&](uint8_t *in, uint8_t *out) {
        int y = (in - warp_texture_header->frame - 2) / warp_texture_width;
        int x = (in - warp_texture_header->frame - 2) % warp_texture_width;
        *out = frame_header->frame[2 + y * frame_width + x] ? *in : *out;
    });
}

void __fastcall DrawWarpTexture_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *param)
{
    Assert(frame_header->IsDecoded());
    int texture_frame = (int)param;
    GrpFrameHeader *warp_texture_header = GetGrpFrameHeader(ImageId::WarpTexture, texture_frame);
    int warp_texture_width = warp_texture_header->GetWidth();
    int frame_width = frame_header->GetWidth();
    Render_Flipped(x, y, warp_texture_header, rect, [&](uint8_t *in, uint8_t *out) {
        int y = (in - warp_texture_header->frame - 2) / warp_texture_width;
        int x = (in - warp_texture_header->frame - 2) % warp_texture_width;
        *out = frame_header->frame[2 + y * frame_width + x] ? *in : *out;
    });
}

void __fastcall DrawShadow_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused)
{
    STATIC_PERF_CLOCK(Ds1);
    // Dark.pcx
    uint8_t *remap = (uint8_t *)bw::shadow_remap.raw_pointer();
    Render_NonFlipped(x, y, frame_header, rect, [&](uint8_t *in, uint8_t *out) {
        *out = *in ? remap[*out] : *out;
    });
}

void __fastcall DrawShadow_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused)
{
    STATIC_PERF_CLOCK(Ds2);
    uint8_t *remap = (uint8_t *)bw::shadow_remap.raw_pointer();
    Render_Flipped(x, y, frame_header, rect, [&](uint8_t *in, uint8_t *out) {
        *out = *in ? remap[*out] : *out;
    });
}
