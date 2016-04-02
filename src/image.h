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
void DrawUncloakedPart_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, int state);
void DrawUncloakedPart_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, int state);
void DrawCloaked_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused);
void DrawCloaked_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused);
void __fastcall DrawWarpTexture_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *texture_frame);
void __fastcall DrawWarpTexture_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *texture_frame);
void __fastcall DrawShadow_NonFlipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused);
void __fastcall DrawShadow_Flipped(int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, void *unused);

namespace ImageFlags
{
    const int Redraw = 0x1;
    const int Flipped = 0x2;
    const int FreezeY = 0x4;
    const int CanTurn = 0x8;
    const int FullIscript = 0x10;
    const int Clickable = 0x20;
    const int Hidden = 0x40;
    const int UseParentLo = 0x80;
}

#pragma pack(push)
#pragma pack(1)

struct CycleStruct
{
    uint8_t active;
    uint8_t speed; // Frames between updates
    uint8_t update_counter;
    uint8_t palette_entry_low;
    uint8_t adv_cycle_pos;
    uint8_t palette_entry_high;
    uint8_t _padding6[0x2];
    void *advanced_cycle_data;
    uint8_t adv_cycle_count;
    uint8_t _paddingd[0x3];
};

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
    x16u width;
    y16u height;
};

class Image
{
    public:
        RevListEntry<Image, 0x0> list;
        uint16_t image_id;
        uint8_t drawfunc;
        uint8_t direction;
        uint16_t flags;
        int8_t x_off;
        int8_t y_off;
        Iscript::Script iscript;
        uint16_t frameset;
        uint16_t frame;
        Point map_position;
        Point screen_position;
        Rect16 grp_bounds;
        GrpSprite *grp;
        void *drawfunc_param;
        void (__fastcall *Render)(int, int, GrpFrameHeader *, Rect32 *, void *);
        void (__fastcall *Update)(Image *); // Unused, used to do what DrawFunc_ProgressFrame does
        Sprite *parent; // 0x3c

        // ---------------

#ifdef SYNC
        void *operator new(size_t size);
#endif
        /// Does no real initialization. Useful when bw is going to initialize it
        Image();
        /// Initializes the image, but does not add it to parent's list.
        /// Does not run the initial iscript animation either, as it can behave
        /// differently based on where in parent's overlay list the image is.
        /// The iscript animation should be ran afterwards with InitIscript().
        Image(Sprite *parent, int image_id, int x, int y);
        ~Image() {}

        /// Resets the image's iscript.
        /// Returns false if image has invalid iscript.
        bool InitIscript(Iscript::Context *ctx);
        void SingleDelete();

        void SetFlipping(bool set);
        bool IsFlipped() const { return flags & ImageFlags::Flipped; }
        bool IsHidden() const { return flags & ImageFlags::Hidden; }
        void FreezeY() { flags |= ImageFlags::FreezeY; }
        void ThawY() { flags &= ~ImageFlags::FreezeY; }
        void UpdateFrameToDirection();
        void Show();
        void Hide();
        void SetOffset(int x_off, int y_off);
        void SetFrame(int frame);
        void SetDrawFunc(int drawfunc, void *param);
        void MakeDetected();

        /// Progresses image's animation by a frame
        void ProgressFrame(Iscript::Context *ctx)
        {
            DrawFunc_ProgressFrame(ctx);
            iscript.ProgressFrame(ctx, this);
        }

        /// Handles an iscript command, returning false if the command could not be handled.
        /// Generally just called from a Sprite::HandleIscriptCommand.
        Iscript::CmdResult HandleIscriptCommand(Iscript::Context *ctx, Iscript::Script *script,
                                                const Iscript::Command &cmd);
        /// Like HandleIscriptCommand, but does not modify the image
        /// (Rng, ctx and script may be modified though).
        /// Useful for iscript speed prediction.
        Iscript::CmdResult ConstIscriptCommand(Iscript::Context *ctx, Iscript::Script *script,
                                               const Iscript::Command &cmd) const;

        /// Changes the iscript animation and runs the script for a frame.
        void SetIscriptAnimation(Iscript::Context *ctx, int anim);

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
            OverrideColor = 0xe, // Flag
            Hallucination = 0x10,
            WarpFlash = 0x11
        };

        template <bool saving> void SaveConvert();

        /// Note: The first call to the function will permamently load arr\images.tbl
        /// using some memory.
        /// The global tbl is loaded in a thread-safe way.
        std::string DebugStr() const;

    private:
        // Returns iscript animation which *must* be switched to, or -1 if none.
        void DrawFunc_ProgressFrame(Iscript::Context *ctx);
        void SaveRestore();
        void UpdateSpecialOverlayPos();
        /// Common function used in iscript code to add overlays.
        /// May return nullptr if something fails during initialization.
        Image *Iscript_AddOverlay(Iscript::Context *ctx, int image_id, int x, int y, bool above);

        /// Sets direction (and flipping) to the one of parent->main_image
        void FollowMainImage();
};

static_assert(sizeof(CycleStruct) == 0x10, "sizeof(CycleStruct)");

#pragma pack(pop)

#endif // IMAGE_H

