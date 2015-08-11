#ifndef IMAGE_H
#define IMAGE_H

#include "types.h"
#include "offsets.h"
#include "list.h"
#include "iscript.h"

// While this constant is referred at several places, it has to same everywhere
// It affects the optimization of drawing functions, padding grp width to 16
// Has to be power of two, 8 was slightly slower and 32 did not cause notable
// difference on my cpu
const int grp_padding_size = 16;

void __fastcall DrawBlended_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *blend_table);
void __fastcall DrawBlended_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *blend_table);
void __fastcall DrawNormal_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused);
void __fastcall DrawNormal_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused);
void __fastcall DrawUncloakedPart_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, int state);
void __fastcall DrawUncloakedPart_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, int state);
void __fastcall DrawCloaked_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused);
void __fastcall DrawCloaked_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused);
void __fastcall DrawWarpTexture_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *texture_frame);
void __fastcall DrawWarpTexture_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *texture_frame);
void __fastcall DrawShadow_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused);
void __fastcall DrawShadow_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused);

#pragma pack(push)
#pragma pack(1)

class GrpFrameHeader
{
    public:
        bool IsDecoded() const;
        uint8_t GetPixel(x32 x, y32 y) const;
        // Includes possible decoding padding
        int GetWidth() const;

        uint8_t x;
        uint8_t y;
        uint8_t w;
        uint8_t h;
        // If first two pixels of the frame are zero, it is considered decoded image
        uint8_t *frame;
};

struct ImgRenderFuncs
{
    uint32_t id;
    void (__fastcall *nonflipped)(int, int, GrpFrameHeader *, Rect32 *, void *);
    void (__fastcall *flipped)(int, int, GrpFrameHeader *, Rect32 *, void *);
};

struct ImgUpdateFunc
{
    uint32_t id;
    void (__fastcall *func)(Image *);
};

struct BlendPalette
{
    uint32_t id;
    uint8_t *data;
    char name[0xc];
};

struct GrpSprite
{
    uint16_t frame_count;
};

class Image
{
    public:
        static const size_t offset_of_allocated = 0x40;

        RevListEntry<Image, 0x0> list;
        uint16_t image_id;
        uint8_t drawfunc;
        uint8_t direction;
        uint16_t flags;
        int8_t x_off;
        int8_t y_off;
        Iscript iscript;
        uint16_t frameset;
        uint16_t frame;
        Point map_position;
        Point screen_position;
        uint16_t grpBounds[4];
        GrpSprite *grp;
        void *drawfunc_param;
        void (__fastcall *Render)(int, int, GrpFrameHeader *, Rect32 *, void *);
        void (__fastcall *Update)(Image *); // Unused, used to do what DrawFunc_ProgressFrame does
        Sprite *parent; // 0x3c

        // ---------------

        DummyListEntry<Image, offset_of_allocated> allocated; // 0x40

#ifdef SYNC
        void *operator new(size_t size);
#endif
        Image();
        ~Image() {}
        static Image *Allocate();


        void SingleDelete();
        static void DeleteAll();
        static void FreeMemory(int count);

        void SetFlipping(bool set);
        bool IsFlipped() const { return flags & 0x2; }
        bool IsHidden() const { return flags & 0x40; }
        void FreezeY() { flags |= 0x4; }
        void ThawY() { flags |= 0x4; }
        void UpdateFrameToDirection();
        void Show();
        void Hide();
        void SetOffset(int x_off, int y_off);
        void SetFrame(int frame);
        void SetDrawFunc(int drawfunc, void *param);
        void MakeDetected();

        bool IscriptCmd(const Iscript::Command &cmd, IscriptContext *ctx, Rng *rng);

        class ProgressFrame_C : public Iterator<ProgressFrame_C, Iscript::Command>
        {
            typedef Iscript::GetCommands_C internal_iterator;
            public:
                Optional<Iscript::Command> next()
                {
                    if (!rng)
                        return Optional<Iscript::Command>();
                    while (true)
                    {
                        auto option = cmds.next();
                        if (!option)
                            return option;
                        auto cmd = option.take();
                        if (cmd.opcode == IscriptOpcode::Move)
                        {
                            auto speed = CalculateSpeedChange(ctx.unit, cmd.val * 256);
                            if (out_speed)
                                *out_speed = speed;
                            if (!test_run)
                                SetSpeed_Iscript(ctx.unit, speed);
                        }
                        else if (!test_run)
                        {
                            if (!ctx.img->IscriptCmd(cmd, &ctx, rng))
                                return option;
                        }
                    }
                }

                ProgressFrame_C(IscriptContext *c, Rng *r, bool t, uint32_t *o) :
                    ctx(*c),
                    rng(r),
                    test_run(t),
                    out_speed(o),
                    cmds(ctx.img->iscript.GetCommands(&ctx, rng)) {}

                // Do nothing -constructor
                ProgressFrame_C() :
                    rng(nullptr),
                    test_run(false),
                    out_speed(nullptr),
                    cmds(ctx.img->iscript.GetCommands(nullptr, nullptr)) {}

                ProgressFrame_C(const ProgressFrame_C &other) = delete;
                ProgressFrame_C(ProgressFrame_C &&o) :
                    ctx(o.ctx),
                    rng(o.rng),
                    test_run(o.test_run),
                    out_speed(o.out_speed),
                    cmds(o.cmds)
                {
                    cmds.SetContext(&ctx);
                }
                ProgressFrame_C& operator=(ProgressFrame_C &&o)
                {
                    ctx = o.ctx;
                    rng = o.rng;
                    test_run = o.test_run;
                    out_speed = o.out_speed;
                    cmds = o.cmds;
                    cmds.SetContext(&ctx);
                    return *this;
                }

            private:
                IscriptContext ctx;
                Rng *rng;
                bool test_run;
                uint32_t *out_speed;
                Iscript::GetCommands_C cmds;
        };
        ProgressFrame_C ProgressFrame();
        ProgressFrame_C ProgressFrame(IscriptContext *ctx, Rng *rng, bool test_run, uint32_t *out_speed)
        {
            DrawFunc_ProgressFrame(ctx, rng);
            if (iscript.wait-- != 0)
                return ProgressFrame_C();
            ctx->img = this;
            return ProgressFrame_C(ctx, rng, test_run, out_speed);
        }
        ProgressFrame_C SetIscriptAnimation(int anim, IscriptContext *ctx, Rng *rng);

#include "constants/image.h"
        enum DrawFunc
        {
            Normal = 0x0,
            NormalSpecial = 0x1,
            Cloaking = 0x2,
            Cloak = 0x3,
            Decloaking = 0x4,
            DetectedCloaking = 0x5,
            DetectedCloak = 0x6,
            DetectedDecloaking = 0x7,
            Remap = 0x9,
            Shadow = 0xa,
            HpBar = 0xb,
            // Bad: constants/image.h defines WarpTexture
            UseWarpTexture = 0xc,
            SelectionCircle = 0xd,
            Hallucination = 0x10,
            WarpFlash = 0x11
        };

        template <bool saving> void SaveConvert();

    private:
        void DrawFunc_ProgressFrame(IscriptContext *ctx, Rng *rng);
        void SaveRestore();
        void UpdateSpecialOverlayPos();
        Image *Iscript_AddOverlay(const IscriptContext *ctx, int image_id, int x, int y, bool above);
};

extern DummyListHead<Image, Image::offset_of_allocated> first_allocated_image;

static_assert(Image::offset_of_allocated == offsetof(Image, allocated), "Image::allocated offset");

#pragma pack(pop)

#endif // IMAGE_H

