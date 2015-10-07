#ifndef SPRITE_H
#define SPRITE_H

#include "types.h"
#include "offsets.h"
#include "list.h"
#include "image.h"
#include "unsorted_list.h"
#include "common/iter.h"
#include "rng.h"
#include <tuple>

void ShowRallyTarget(Unit *unit);
void RefreshCursorMarker();
void DrawCursorMarker();
void ShowCursorMarker(int x, int y);
void DrawMinimapUnits();
Sprite *ShowCommandResponse(int x, int y, Sprite *alternate);
Sprite *__stdcall FindBlockingFowResource(int x_tile, int y_tile, int radius);

extern "C" void __stdcall SetSpriteDirection(int direction);

#pragma pack(push)
#pragma pack(1)

class Sprite
{
    friend class LoneSpriteSystem;
    public:
        RevListEntry<Sprite, 0x0> list;
        uint16_t sprite_id;
        uint8_t player;
        uint8_t selectionIndex;
        uint8_t visibility_mask;
        uint8_t elevation;
        uint8_t flags;

        uint8_t selection_flash_timer;
        uint16_t index;
        uint8_t width;
        uint8_t height;
        Point position;
        Image *main_image;
        RevListHead<Image, 0x0> first_overlay;
        ListHead<Image, 0x0> last_overlay;

        // -----------

        uint32_t id; // 0x24
        uint32_t sort_order; // 0x28

        void Serialize(Save *save);
        static ptr<Sprite> Deserialize(Load *load);
        ~Sprite() {}

        static std::pair<int, Sprite *> SaveAllocate(uint8_t *in, uint32_t size);
        static Sprite *Allocate(int sprite_id, const Point &pos, int player);

        Sprite *SpawnLoneSpriteAbove(int sprite_id);
        static Sprite *Spawn(Image *spawner, uint16_t sprite_id, const Point &pos, int elevation_level);

        void Remove();
        static void DeleteAll();

        void SetDirection32(int direction);
        void SetDirection256(int direction);
        void SetFlipping(bool set);

        bool IsHidden() const { return flags & 0x20; }

        bool UpdateVisibilityPoint();
        void UpdateVisibilityArea();

        uint32_t GetZCoord() const;

        static void DrawSprites();
        static void CreateDrawSpriteListFullRedraw();
        static void CreateDrawSpriteList();
        static Sprite *FindFowTarget(int x, int y);

        void MarkHealthBarDirty();

        void AddMultipleOverlaySprites(int overlay_type, int count, int sprite_id, int base, bool flip);
        void AddDamageOverlay();

        static void RemoveAllSelectionOverlays();
        void RemoveSelectionOverlays();
        static int DrawnSprites() { return draw_order_amount; }

    private:
#ifdef SYNC
        void *operator new(size_t size);
#endif
        Sprite();

        /// Initializes the sprite, returns false if unable and nothing was changed.
        /// Static to emphasize the fact it manipulates global state.
        static bool Initialize(Sprite *sprite, int sprite_id, const Point &pos, int player);

        void AddToHlines();

        static void PackIds();

        static uint32_t next_id;
        static uint32_t count;

        static Sprite **draw_order;
        static int draw_order_amount;
        static uint32_t draw_order_limit;

    public:
#include "constants/sprite.h"

        class ProgressFrame_C : public Iterator<ProgressFrame_C, Iscript::Command>
        {
            public:
                ProgressFrame_C() : anim() {}
                ProgressFrame_C(const ProgressFrame_C &other) = delete;
                ProgressFrame_C(ProgressFrame_C &&o) :
                    cmds(move(o.cmds)),
                    sprite(o.sprite),
                    img(o.img),
                    ctx(o.ctx),
                    rng(o.rng),
                    anim(o.anim) {}
                ProgressFrame_C &operator=(ProgressFrame_C &&o)
                {
                    cmds = move(o.cmds);
                    sprite = o.sprite;
                    img = o.img;
                    ctx = o.ctx;
                    rng = o.rng;
                    anim = o.anim;
                    return *this;
                }

                ProgressFrame_C(Sprite *s, int a, const IscriptContext &ctx_, Rng *rng_) :
                    sprite(s),
                    ctx(ctx_),
                    rng(rng_),
                    anim(a)
                {
                    if (sprite != nullptr)
                    {
                        img = sprite->first_overlay;
                        TakeCmds();
                    }
                }
                Optional<Iscript::Command> next()
                {
                    if (sprite == nullptr || img == nullptr)
                        return Optional<Iscript::Command>();
                    auto cmd = cmds.next();
                    while (!cmd || cmd.take().opcode == IscriptOpcode::End)
                    {
                        auto old = img;
                        img = img->list.next;
                        if (cmd && cmd.take().opcode == IscriptOpcode::End)
                        {
                            old->SingleDelete();
                            if (!sprite->first_overlay)
                                return Optional<Iscript::Command>(IscriptOpcode::End);
                        }
                        if (!img)
                            return Optional<Iscript::Command>();

                        TakeCmds();
                        cmd = cmds.next();
                    }
                    return cmd;
                }

            private:
                void TakeCmds()
                {
                    if (anim != -1)
                        cmds = img->SetIscriptAnimation(anim, &ctx, rng);
                    else
                        cmds = img->ProgressFrame(&ctx, rng, false, nullptr);
                }
                Image::ProgressFrame_C cmds;
                Sprite *sprite;
                Image *img;
                IscriptContext ctx;
                Rng *rng;
                int anim;
        };
        ProgressFrame_C ProgressFrame(const IscriptContext &ctx, Rng *rng) { return ProgressFrame_C(this, -1, ctx, rng); }
        ProgressFrame_C SetIscriptAnimation(int anim, bool force, const IscriptContext &ctx, Rng *rng)
        {
            if (!force && flags & 0x80)
                return ProgressFrame_C(nullptr, -1, ctx, rng);
            return ProgressFrame_C(this, anim, ctx, rng);
        }
        ProgressFrame_C SetIscriptAnimation(int anim, bool force)
        {
            return SetIscriptAnimation(anim, force, IscriptContext(), main_rng);
        }
        ProgressFrame_C IscriptToIdle(const IscriptContext &ctx, Rng *rng);
};

class LoneSpriteSystem
{
    public:
        void DeleteAll();
        void ProgressFrames();
        Sprite *AllocateLone(int sprite_id, const Point &pos, int player);
        Sprite *AllocateFow(Sprite *base, int unit_id);

        void Serialize(Save *save);
        void Deserialize(Load *load);
        template <class Cb>
        void MakeSaveIdMapping(Cb callback) const;

        UnsortedList<ptr<Sprite>, 128> lone_sprites;
        UnsortedList<ptr<Sprite>> fow_sprites;
};

extern LoneSpriteSystem *lone_sprites;

#pragma pack(pop)

#endif // SPRITE_H

