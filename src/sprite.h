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

namespace SpriteFlags
{
    const int HasSelectionCircle = 0x1;
    const int DashedSelectionMask = 0x6; // Team can have max 4 players, so max 0..3 dashed circles
    const int HasHealthBar = 0x8;
    const int Unk10 = 0x10; // Draw sort?
    const int Hidden = 0x20;
    const int Unk40 = 0x40; // Became uncloaked?
    const int Nobrkcodestart = 0x80;
}

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
        ~Sprite();

        static std::pair<int, Sprite *> SaveAllocate(uint8_t *in, uint32_t size);
        /// Allocates a new sprite. May fail and return nullptr.
        /// As this causes the first frame of iscript animation to be executed,
        /// it requires an Iscript::Context.
        static ptr<Sprite> Allocate(Iscript::Context *ctx, int sprite_id, const Point &pos, int player);
        /// Allocates a new sprite, using the generic sprite Iscript::Context for
        /// runnig the first frame of an animation.
        /// Compatibility hack for a bw hook. Use Allocate() or LoneSpriteSystem::AllocateLone()
        /// instead.
        static Sprite *AllocateWithBasicIscript(int sprite_id, const Point &pos, int player);

        Sprite *SpawnLoneSpriteAbove(int sprite_id);
        static Sprite *Spawn(Image *spawner, uint16_t sprite_id, const Point &pos, int elevation_level);

        void Remove();
        static void DeleteAll();

        void SetDirection32(int direction);
        void SetDirection256(int direction);
        void SetFlipping(bool set);

        void Hide();
        bool IsHidden() const { return flags & SpriteFlags::Hidden; }

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
        void AddOverlayAboveMain(Iscript::Context *ctx, int image_id, int x, int y, int direction);

        static void RemoveAllSelectionOverlays();
        void RemoveSelectionOverlays();
        static int DrawnSprites() { return draw_order_amount; }

    private:
#ifdef SYNC
        void *operator new(size_t size);
#endif
        Sprite();

        /// Initializes the sprite, returns false if unable and nothing was changed.
        bool Initialize(Iscript::Context *ctx, int sprite_id, const Point &pos, int player);

        void AddToHlines();

        static void PackIds();

        static uint32_t next_id;
        static uint32_t count;

        static Sprite **draw_order;
        static int draw_order_amount;
        static uint32_t draw_order_limit;

    public:
        void ProgressFrame(Iscript::Context *ctx) {
            Image *next;
            for (Image *img = first_overlay; img != nullptr; img = next) {
                next = img->list.next;
                img->ProgressFrame(ctx);
            }
        }

        void SetIscriptAnimation(Iscript::Context *ctx, int anim, bool force) {
            if (!force && flags & SpriteFlags::Nobrkcodestart)
                return;
            for (Image *img : first_overlay) {
                img->SetIscriptAnimation(ctx, anim);
            }
        }

        Iscript::CmdResult HandleIscriptCommand(Iscript::Context *ctx, Image *img,
                                                Iscript::Script *script, const Iscript::Command &cmd) {
            switch (cmd.opcode) {
                case Iscript::Opcode::End:
                    script->pos -= cmd.Size(); // Infloop on end if we couldn't delete it now
                    if (ctx->can_delete) {
                        img->SingleDelete();
                        ctx->deleted = first_overlay == nullptr;
                    }
                    return Iscript::CmdResult::Stop;
                break;
                default:
                    return img->HandleIscriptCommand(ctx, script, cmd);
                break;
            }
        }

        /// Sets the iscript animation and warns if any commands were not handled.
        /// Meant to be used with lone sprites, as they really can't do any extra handling.
        void SetIscriptAnimation_Lone(int anim, bool force, Rng *rng, const char *caller);

        void IscriptToIdle(Iscript::Context *ctx);

        #include "constants/sprite.h"
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

