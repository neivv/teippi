#include "limits.h"

#include "patch/patchmanager.h"
#include "ai.h"
#include "building.h"
#include "bullet.h"
#include "bunker.h"
#include "commands.h"
#include "dialog.h"
#include "draw.h"
#include "flingy.h"
#include "game.h"
#include "init.h"
#include "image.h"
#include "iscript.h"
#include "log.h"
#include "offsets_hooks.h"
#include "offsets.h"
#include "order.h"
#include "pathing.h"
#include "player.h"
#include "replay.h"
#include "rng.h"
#include "save.h"
#include "selection.h"
#include "sound.h"
#include "sprite.h"
#include "targeting.h"
#include "tech.h"
#include "triggers.h"
#include "unitsearch.h"
#include "warn.h"
#include "unit.h"

Common::PatchManager *patch_mgr;

// Hack from game.cpp
extern bool unitframes_in_progress;

void DamageUnit_Hook(int damage, Unit *target, Unit *attacker, int attacking_player, int show_attacker)
{
    // Can't do UnitWasHit here, so warn
    if (attacker != nullptr)
        Warning("DamageUnit hooked");
    vector<Unit *> killed_units;
    DamageUnit(damage, target, &killed_units);
    for (Unit *unit : killed_units)
        unit->Kill(nullptr);
}

void AddMultipleOverlaySprites(Sprite *sprite, uint16_t base, int overlay_type, int count, int sprite_id, int flip)
{
    sprite->AddMultipleOverlaySprites(overlay_type, count - base + 1, SpriteType(sprite_id), base, flip);
}

void SendUnloadCommand(const Unit *unit)
{
    uint8_t buf[5];
    buf[0] = commands::Unload;
    *(uint32_t *)(buf + 1) = unit->lookup_id;
    bw::SendCommand(buf, 5);
}

Unit ** FindUnitsRect(const Rect16 *rect)
{
    int tmp;
    Rect16 r = *rect;
    // See unitsearch.h
    r.right += 1;
    r.bottom += 1;
    if (r.right < r.left)
        r.left = 0;
    if (r.bottom < r.top)
        r.top = 0;
    r.right = std::min(r.right, *bw::map_width);
    r.bottom = std::min(r.bottom, *bw::map_height);

    Unit **ret = unit_search->FindUnitsRect(r, &tmp);
    return ret;
}

Unit ** CheckMovementCollision(Unit *unit, int x, int y)
{
    if (x >= 0x8000)
        x |= 0xffff << 16;
    if (y >= 0x8000)
        y |= 0xffff << 16;
    return unit_search->CheckMovementCollision(unit, x, y);
}

Unit **FindUnitBordersRect(const Rect16 *rect)
{
    Rect16 r = *rect;
    if (r.right < r.left)
        r.left = 0;
    if (r.bottom < r.top)
        r.top = 0;
    return unit_search->FindUnitBordersRect(&r);
}

Unit *FindNearestUnit(Rect16 *area, Unit *a, uint16_t b, uint16_t c, int d, int e, int f, int g,
        int (__fastcall *h)(const Unit *, void *), void *i)
{
    if (area->left > area->right)
        area->left = 0;
    if (area->top > area->bottom)
        area->top = 0;
    return unit_search->FindNearestUnit(a, Point(b, c), h, i, *area);
}

void CancelZergBuilding(Unit *unit)
{
    //Warning("Calling Unit::Kill with nullptr from CancelZergBuilding (Extractor)");
    // This *should* not matter, as human cancels are at least done from ProcessCommands
    unit->CancelZergBuilding(nullptr);
}

Order *DeleteOrder_Hook(Order *order, Unit *unit)
{
    unit->DeleteOrder(order);
    // Bw assumes that it returns a order, but with this implementation
    // it would be use-after-free, so just return nullptr and hope
    // that any uses get caught with it.
    return nullptr;
}

void KillSingleUnit(Unit *unit)
{
    if (unitframes_in_progress)
        Warning("Hooked Unit::Kill while unit frames are progressed (unit %x)", unit->unit_id);
    unit->Kill(nullptr);
}

Unit **FindUnitsPoint(uint16_t x, uint16_t y)
{
    Rect16 area(x, y, x + 1, y + 1);
    return unit_search->FindUnitsRect(Rect16(x, y, x + 1, y + 1));
}

int IsTileBlockedBy(Unit **units, Unit *builder, int x_tile, int y_tile, int dont_ignore_reacting, int also_invisible)
{
    int xpos = x_tile * 32;
    int ypos = y_tile * 32;
    for (Unit *unit = *units++; unit; unit = *units++)
    {
        if (unit == builder)
            continue;
        if (unit->flags & (UnitStatus::Building | UnitStatus::Air))
            continue;
        if (unit->Type() == UnitId::DarkSwarm || unit->Type() == UnitId::DisruptionWeb)
            continue;
        if (!dont_ignore_reacting && unit->flags & UnitStatus::Reacts)
            continue;
        bool invisible = false;
        if (builder && unit->IsInvisibleTo(builder))
        {
            if (!also_invisible)
                continue;
            else
                invisible = true;
        }
        if (unit->sprite->IsHidden())
            continue;
        Rect16 crect = unit->GetCollisionRect();
        if (crect.left < xpos + 32 && crect.right > xpos)
        {
            if (crect.top < ypos + 32 && crect.bottom > ypos)
            {
                if (invisible)
                    return 0;
                else
                    return 4;
            }
        }
    }
    return 0;
}

// Horribly misnamed
int DoesBuildingBlock(Unit *builder, int x_tile, int y_tile)
{
    if (!builder || ~builder->flags & UnitStatus::Building || builder->Type() == UnitId::NydusCanal)
        return true;
    if (builder->sprite->IsHidden())
        return true;
    Rect16 crect = builder->GetCollisionRect();
    if (crect.left >= (x_tile + 1) * 32 || crect.right <= x_tile * 32)
        return true;
    if (crect.top >= (y_tile + 1) * 32 || crect.bottom <= y_tile * 32)
        return true;
    return false;
}

class SimpleIscriptContext : public Iscript::Context
{
    public:
        constexpr SimpleIscriptContext(Rng *rng) : Iscript::Context(rng, false) { }

        virtual Iscript::CmdResult HandleCommand(Image *img, Iscript::Script *script,
                                                 const Iscript::Command &cmd) override
        {
            auto result = img->HandleIscriptCommand(this, script, cmd);
            if (result == Iscript::CmdResult::NotHandled)
            {
                Warning("Could not handle iscript command %s for image %s from SetIscriptAnimation hook",
                        cmd.DebugStr().c_str(), img->DebugStr().c_str());
            }
            return result;
        }
};

void SetIscriptAnimation(Image *img, uint8_t anim)
{
    if (*bw::active_iscript_unit != nullptr)
        (*bw::active_iscript_unit)->SetIscriptAnimationForImage(img, anim);
    else
    {
        // This is unable to handle any unit-specific commands
        SimpleIscriptContext ctx(MainRng());
        img->SetIscriptAnimation(&ctx, anim);
    }
}

class MovementIscriptContext : public Iscript::Context
{
    public:
        constexpr MovementIscriptContext(Unit *unit, uint32_t *out_speed, Rng *rng) :
            Iscript::Context(rng, false), unit(unit), out_speed(out_speed) { }

        Unit * const unit;
        uint32_t *out_speed;

        virtual Iscript::CmdResult HandleCommand(Image *img, Iscript::Script *script,
                                                 const Iscript::Command &cmd) override
        {
            if (cmd.opcode == Iscript::Opcode::Move)
            {
                auto speed = bw::CalculateSpeedChange(unit, cmd.val * 256);
                *out_speed = speed;
            }
            auto result = img->ConstIscriptCommand(this, script, cmd);
            if (result == Iscript::CmdResult::NotHandled)
                return Iscript::CmdResult::Handled;
            return result;
        }
};

static void ProgressIscriptFrame_Hook(Image *image, Iscript::Script *script, int test_run, uint32_t *out_speed)
{
    // Shouldn't be hooked from elsewhere
    Assert(*bw::active_iscript_unit != nullptr);
    if (test_run)
    {
        Assert(out_speed != nullptr);
        MovementIscriptContext ctx(*bw::active_iscript_unit, out_speed, MainRng());
        script->ProgressFrame(&ctx, image);
    }
    else
    {
        // What to do? Could try determining if we are using unit, bullet, sprite or what,
        // but this shouldn't be called anyways
        Warning("ProgressIscriptFrame hooked");
    }
}

int ForEachLoadedUnit(Unit *transport, int (__fastcall *Func)(Unit *unit, void *param), void *param)
{
    for (Unit *unit = transport->first_loaded; unit; unit = unit->next_loaded)
    {
        if ((*Func)(unit, param))
            return 1;
    }
    return 0;
}

void AddLoadedUnitsToCompletedUnitLbScore(Unit *transport)
{
    for (Unit *unit = transport->first_loaded; unit; unit = unit->next_loaded)
    {
        bw::AddToCompletedUnitLbScore(unit);
    }
}

void TriggerPortraitFinished_Hook(Control *ctrl, int timer_id)
{
    bw::DeleteTimer(ctrl, timer_id);
    *bw::trigger_portrait_active = 0;
    // Not including the code which clears waits in singleplayer, as it causes replays to desync
}

FILE *fopen_hook(const char *a, const char *b)
{
    return fopen(a, b);
}

void fclose_hook(FILE *a)
{
    fclose(a);
}

int fread_hook(void *a, int b, int c, FILE *d)
{
    return fread(a, b, c, d);
}

int fwrite_hook(void *a, int b, int c, FILE *d)
{
    return fwrite(a, b, c, d);
}

int fgetc_hook(FILE *a)
{
    return fgetc(a);
}

int fseek_hook(FILE *a, int b, int c)
{
    return fseek(a, b, c);
}

int setvbuf_hook(FILE *a, char *b, int c, int d)
{
    return setvbuf(a, b, c, d);
}

void CreateSimplePath_Hook(Unit *unit, uint32_t waypoint_xy, uint32_t end_xy)
{
    CreateSimplePath(unit, Point(waypoint_xy & 0xffff, waypoint_xy >> 16), Point(end_xy & 0xffff, end_xy >> 16));
}

static void ProgressMove_Hook(Flingy *flingy)
{
    FlingyMoveResults unused;
    flingy->ProgressMove(&unused);
}

static int IsDrawnPixel(GrpFrameHeader *frame, int x, int y)
{
    if (x < 0 || x >= frame->w)
        return 0;
    if (y < 0 || y >= frame->h)
        return 0;
    return frame->GetPixel(x, y) != 0;
}

static bool __fastcall DrawGrp_Hook(int x, int y, GrpFrameHeader * frame_header, Rect32 * rect, void *unused)
{
    if (frame_header->IsDecoded())
    {
        DrawNormal_NonFlipped(x, y, frame_header, rect, unused);
        return true;
    }
    else
        return false;
}

static bool __fastcall DrawGrp_Flipped_Hook(int x, int y, GrpFrameHeader * frame_header, Rect32 * rect, void *unused)
{
    if (frame_header->IsDecoded())
    {
        DrawNormal_Flipped(x, y, frame_header, rect, unused);
        return true;
    }
    else
        return false;
}

static void MakeDetected_Hook(Sprite *sprite)
{
    if (sprite->flags & 0x40)
        bw::RemoveCloakDrawfuncs(sprite);
    else
    {
        for (Image *img : sprite->first_overlay)
            img->MakeDetected();
    }
}

void PatchDraw(Common::PatchContext *patch)
{
    patch->Hook(bw::SDrawLockSurface, SDrawLockSurface_Hook);
    patch->Hook(bw::SDrawUnlockSurface, SDrawUnlockSurface_Hook);

    patch->Hook(bw::DrawScreen, DrawScreen);
}

void RemoveLimits(Common::PatchContext *patch)
{
    delete unit_search;
    unit_search = new MainUnitSearch;

    Ai::RemoveLimits(patch);

    if (UseConsole)
    {
        patch->Hook(bw::GenerateFog, GenerateFog);
    }

    patch->Hook(bw::ProgressObjects, ProgressObjects);
    patch->Hook(bw::GameFunc, ProgressFrames);

    patch->Hook(bw::CreateOrder, [](uint8_t order, uint32_t pos, Unit *target, uint16_t fow) {
        return new Order(OrderType(order), Point(pos & 0xffff, pos >> 16), target, UnitType(fow));
    });
    patch->Hook(bw::DeleteOrder, DeleteOrder_Hook);
    patch->Hook(bw::DeleteSpecificOrder, [](Unit *unit, uint8_t order) {
        return unit->DeleteSpecificOrder(OrderType(order));
    });

    patch->Hook(bw::GetEmptyImage, []{ return new Image; });
    patch->Hook(bw::DeleteImage, &Image::SingleDelete);

    patch->Hook(bw::CreateSprite, Sprite::AllocateWithBasicIscript_Hook);
    patch->Hook(bw::DeleteSprite, [](Sprite *sprite) {
        sprite->Remove();
        delete sprite;
    });
    patch->Hook(bw::ProgressSpriteFrame, [](Sprite *sprite) {
        // Shouldn't be hooked from elsewhere
        Assert(*bw::active_iscript_unit != nullptr);
        (*bw::active_iscript_unit)->ProgressIscript("ProgressSpriteFrame hook", nullptr);
    });

    patch->Hook(bw::CreateLoneSprite, [](uint16_t sprite_id, uint16_t x, uint16_t y, uint8_t player) {
        return lone_sprites->AllocateLone(SpriteType(sprite_id), Point(x, y), player);
    });
    patch->Hook(bw::CreateFowSprite, [](uint16_t unit_id, Sprite *base) {
        return lone_sprites->AllocateFow(base, UnitType(unit_id));
    });

    patch->Hook(bw::InitLoneSprites, InitCursorMarker);
    patch->Hook(bw::DrawCursorMarker, DrawCursorMarker);
    patch->Hook(bw::ShowRallyTarget, ShowRallyTarget);
    patch->Hook(bw::ShowCursorMarker, ShowCursorMarker);

    patch->Hook(bw::SetSpriteDirection, &Sprite::SetDirection32);
    patch->Hook(bw::FindBlockingFowResource, FindBlockingFowResource);
    patch->Hook(bw::DrawAllMinimapUnits, DrawMinimapUnits);
    patch->Hook(bw::CreateBunkerShootOverlay, CreateBunkerShootOverlay);

    patch->Hook(bw::AllocateUnit, &Unit::AllocateAndInit);
    patch->CallHook(bw::InitUnitSystem_Hook, []{
        Unit::DeleteAll();
        // Hack to do it here but oh well.
        score->Initialize();
    });
    patch->Hook(bw::InitSpriteSystem, Sprite::InitSpriteSystem);

    patch->Hook(bw::CreateBullet,
            [](Unit *parent, int x, int y, uint8_t player, uint8_t direction, uint8_t weapon_id) {
        return bullet_system->AllocateBullet(parent, player, direction, WeaponType(weapon_id), Point(x, y));
    });
    patch->Hook(bw::GameEnd, GameEnd);

    patch->Hook(bw::AddToPositionSearch, [](Unit *unit) { unit_search->Add(unit); } );
    patch->Hook(bw::FindUnitPosition, [](int) { return 0; });
    patch->Hook(bw::FindUnitsRect, FindUnitsRect);
    patch->Hook(bw::FindNearbyUnits, [](Unit *u, int x, int y) {
        return unit_search->FindCollidingUnits(u, x, y);
    });
    patch->Hook(bw::DoUnitsCollide, [](const Unit *a, const Unit *b) { return unit_search->DoUnitsCollide(a, b); });
    patch->Hook(bw::CheckMovementCollision, CheckMovementCollision);
    patch->Hook(bw::FindUnitBordersRect, FindUnitBordersRect);
    patch->CallHook(bw::ClearPositionSearch, []{ unit_search->Clear(); });

    patch->Hook(bw::ChangeUnitPosition, [](Unit *unit, int x_diff, int y_diff) {
        unit_search->ChangeUnitPosition(unit, x_diff, y_diff);
    });
    patch->Hook(bw::FindNearestUnit, FindNearestUnit);
    patch->Hook(bw::GetNearbyBlockingUnits, [](PathingData *pd) { unit_search->GetNearbyBlockingUnits(pd); });
    patch->Hook(bw::RemoveFromPosSearch, [](Unit *unit) { unit_search->Remove(unit); });
    patch->Hook(bw::FindUnitsPoint, FindUnitsPoint);

    patch->Hook(bw::GetDodgingDirection, [](const Unit *self, const Unit *other) {
        return unit_search->GetDodgingDirection(self, other);
    });
    patch->Hook(bw::DoesBlockArea, [](const Unit *unit, const CollisionArea *area) -> int {
        return unit_search->DoesBlockArea(unit, area);
    });
    patch->Hook(bw::IsTileBlockedBy, IsTileBlockedBy);
    patch->Hook(bw::DoesBuildingBlock, DoesBuildingBlock);

    patch->Hook(bw::UnitToIndex, [](Unit *val) { return (uint32_t)val; });
    patch->Hook(bw::IndexToUnit, [](uint32_t val) { return (Unit *)val; });

    patch->Hook(bw::MakeDrawnSpriteList, [] {});
    patch->Hook(bw::PrepareDrawSprites, Sprite::CreateDrawSpriteList);
    patch->CallHook(bw::FullRedraw, Sprite::CreateDrawSpriteListFullRedraw);
    patch->Hook(bw::DrawSprites, Sprite::DrawSprites);
    // Disabled as I can't be bothered to figure it out.
    patch->Hook(bw::VisionSync, [](void *, int) { return 1; });

    patch->Hook(bw::RemoveUnitFromBulletTargets, RemoveFromBulletTargets);
    patch->Hook(bw::DamageUnit, DamageUnit_Hook);

    patch->Hook(bw::FindUnitInLocation_Check, FindUnitInLocation_Check);
    patch->Hook(bw::ChangeInvincibility, ChangeInvincibility);

    patch->Hook(bw::CanLoadUnit, [](const Unit *a, const Unit *b) -> int { return a->CanLoadUnit(b); });
    patch->Hook(bw::LoadUnit, &Unit::LoadUnit);
    patch->Hook(bw::HasLoadedUnits, [](const Unit *unit) -> int { return unit->HasLoadedUnits(); });
    patch->Hook(bw::UnloadUnit, [](Unit *unit) -> int { return unit->related->UnloadUnit(unit); });
    patch->Hook(bw::SendUnloadCommand, SendUnloadCommand);
    patch->Hook(bw::GetFirstLoadedUnit, [](Unit *unit) { return unit->first_loaded; });
    patch->Hook(bw::ForEachLoadedUnit, ForEachLoadedUnit);
    patch->Hook(bw::AddLoadedUnitsToCompletedUnitLbScore, AddLoadedUnitsToCompletedUnitLbScore);
    patch->Hook(bw::GetUsedSpace, &Unit::GetUsedSpace);
    patch->Hook(bw::IsCarryingFlag, [](const Unit *unit) -> int { return unit->IsCarryingFlag(); });

    patch->Hook(bw::DrawStatusScreen_LoadedUnits, DrawStatusScreen_LoadedUnits);
    patch->Hook(bw::TransportStatus_UpdateDrawnValues, TransportStatus_UpdateDrawnValues);
    patch->Hook(bw::TransportStatus_DoesNeedRedraw, TransportStatus_DoesNeedRedraw);
    patch->Hook(bw::StatusScreen_DrawKills, StatusScreen_DrawKills);
    patch->Hook(bw::AddMultipleOverlaySprites, AddMultipleOverlaySprites);

    patch->Hook(bw::KillSingleUnit, KillSingleUnit);
    patch->Hook(bw::Unit_Die, [] { Warning("Hooked Unit::Die, not doing anything"); });
    patch->Hook(bw::CancelZergBuilding, CancelZergBuilding);

    patch->Hook(bw::SetIscriptAnimation, SetIscriptAnimation);
    patch->Hook(bw::ProgressIscriptFrame, ProgressIscriptFrame_Hook);

    patch->Hook(bw::Order_AttackMove_ReactToAttack, [](Unit *unit, int order) {
        return unit->Order_AttackMove_ReactToAttack(OrderType(order));
    });
    patch->Hook(bw::Order_AttackMove_TryPickTarget, [](Unit *unit, int order) {
        unit->Order_AttackMove_TryPickTarget(OrderType(order));
    });

    // Won't be called when loading save though.
    patch->CallHook(bw::PathingInited, [] { unit_search->Init(); });

    patch->Hook(bw::ProgressUnstackMovement, &Unit::ProgressUnstackMovement);

    patch->Hook(bw::MovementState13, &Unit::MovementState13);
    patch->Hook(bw::MovementState17, &Unit::MovementState17);
    patch->Hook(bw::MovementState20, &Unit::MovementState20);
    patch->Hook(bw::MovementState1c, &Unit::MovementState1c);
    patch->Hook(bw::MovementState_FollowPath, &Unit::MovementState_FollowPath);
    patch->Hook(bw::MovementState_Flyer, [](Unit *) { return 0; });

    patch->Hook(bw::Trig_KillUnitGeneric, [](Unit *unit, KillUnitArgs *args) {
        return Trig_KillUnitGeneric(unit, args, args->check_height, false);
    });
    patch->Hook(bw::TriggerPortraitFinished, TriggerPortraitFinished_Hook);
    bw::trigger_actions[0x7] = TrigAction_Transmission;
    bw::trigger_actions[0xa] = TrigAction_CenterView;

    patch->Hook(bw::ChangeMovementTargetToUnit, [](Unit *unit, Unit *target) -> int {
        return unit->ChangeMovementTargetToUnit(target);
    });
    patch->Hook(bw::ChangeMovementTarget, [](Unit *unit, uint16_t x, uint16_t y) -> int {
        return unit->ChangeMovementTarget(Point(x, y));
    });

    patch->JumpHook(bw::Sc_fclose, fclose_hook);
    patch->JumpHook(bw::Sc_fopen, fopen_hook);
    patch->JumpHook(bw::Sc_fwrite, fwrite_hook);
    patch->JumpHook(bw::Sc_fread, fread_hook);
    patch->JumpHook(bw::Sc_fgetc, fgetc_hook);
    patch->JumpHook(bw::Sc_fseek, fseek_hook);
    patch->JumpHook(bw::Sc_setvbuf, setvbuf_hook);
    patch->Hook(bw::LoadGameObjects, LoadGameObjects);

    patch->Hook(bw::AllocatePath, AllocatePath);
    patch->Hook(bw::DeletePath, &Unit::DeletePath);
    patch->Hook(bw::DeletePath2, &Unit::DeletePath);
    patch->Hook(bw::CreateSimplePath, CreateSimplePath_Hook);
    patch->Hook(bw::InitPathArray, [] {});

    patch->Hook(bw::StatusScreenButton, StatusScreenButton);
    patch->Hook(bw::LoadReplayMapDirEntry, LoadReplayMapDirEntry);
    patch->Hook(bw::LoadReplayData, LoadReplayData);

    patch->Hook(bw::DoNextQueuedOrder, &Unit::DoNextQueuedOrder);
    patch->Hook(bw::ProcessLobbyCommands, ProcessLobbyCommands);
    patch->Hook(bw::BriefingOk, BriefingOk);

    patch->Hook(bw::ProgressFlingyTurning, [](Flingy *f) -> int { return f->ProgressTurning(); });
    patch->Hook(bw::SetMovementDirectionToTarget, &Flingy::SetMovementDirectionToTarget);
    patch->Hook(bw::ProgressMove, ProgressMove_Hook);

    patch->Hook(bw::LoadGrp,
        [](int image_id, uint32_t *grps, Tbl *tbl, GrpSprite **loaded_grps, void **overlapped, void **out_file) {
        return LoadGrp(ImageType(image_id), grps, tbl, loaded_grps, overlapped, out_file);
    });
    patch->Hook(bw::IsDrawnPixel, IsDrawnPixel);
    patch->Hook(bw::LoadBlendPalettes, LoadBlendPalettes);
    patch->Hook(bw::DrawImage_Detected,
        [](int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, uint8_t *blend_table) {
        DrawBlended_NonFlipped(x, y, frame_header, rect, blend_table);
    });
    patch->Hook(bw::DrawImage_Detected_Flipped,
        [](int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, uint8_t *blend_table) {
        DrawBlended_Flipped(x, y, frame_header, rect, blend_table);
    });
    patch->Hook(bw::DrawUncloakedPart,
        [](int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, int state) {
        DrawUncloakedPart_NonFlipped(x, y, frame_header, rect, state & 0xff);
    });
    patch->Hook(bw::DrawUncloakedPart_Flipped,
        [](int x, int y, GrpFrameHeader *frame_header, Rect32 *rect, int state) {
        DrawUncloakedPart_Flipped(x, y, frame_header, rect, state & 0xff);
    });
    patch->Hook(bw::DrawImage_Cloaked, DrawCloaked_NonFlipped);
    patch->Hook(bw::DrawImage_Cloaked_Flipped, DrawCloaked_Flipped);
    bw::image_renderfuncs[Image::Normal].nonflipped = &DrawNormal_NonFlipped;
    bw::image_renderfuncs[Image::Normal].flipped = &DrawNormal_Flipped;
    bw::image_renderfuncs[Image::NormalSpecial].nonflipped = &DrawNormal_NonFlipped;
    bw::image_renderfuncs[Image::NormalSpecial].flipped = &DrawNormal_Flipped;
    bw::image_renderfuncs[Image::Remap].nonflipped = &DrawBlended_NonFlipped;
    bw::image_renderfuncs[Image::Remap].flipped = &DrawBlended_Flipped;
    bw::image_renderfuncs[Image::Shadow].nonflipped = &DrawShadow_NonFlipped;
    bw::image_renderfuncs[Image::Shadow].flipped = &DrawShadow_Flipped;
    bw::image_renderfuncs[Image::UseWarpTexture].nonflipped = &DrawWarpTexture_NonFlipped;
    bw::image_renderfuncs[Image::UseWarpTexture].flipped = &DrawWarpTexture_Flipped;
    patch->Patch(bw::DrawGrp, (void *)&DrawGrp_Hook, 12, PATCH_OPTIONALHOOK | PATCH_SAFECALLHOOK);
    patch->Patch(bw::DrawGrp_Flipped, (void *)&DrawGrp_Flipped_Hook, 12, PATCH_OPTIONALHOOK | PATCH_SAFECALLHOOK);

    patch->Hook(bw::FindUnitAtPoint, FindUnitAtPoint);
    patch->Hook(bw::MakeJoinedGameCommand, [](int flags, int x4, int proto_ver, int save_uniq_player,
                                              int save_player, uint32_t save_hash, int create) {
        MakeJoinedGameCommand(flags, x4, save_player, save_uniq_player, save_hash, create != 0);
    });
    patch->Hook(bw::Command_GameData, Command_GameData);
    patch->Hook(bw::InitGame, InitGame);
    patch->Hook(bw::InitStartingRacesAndTypes, InitStartingRacesAndTypes);

    patch->Hook(bw::NeutralizePlayer, [](uint8_t player) { Neutralize(player); });
    patch->Hook(bw::MakeDetected, MakeDetected_Hook);

    patch->Hook(bw::AddDamageOverlay, &Sprite::AddDamageOverlay);

    patch->Hook(bw::GameScreenRClickEvent, GameScreenRClickEvent);
    patch->Hook(bw::GameScreenLClickEvent_Targeting, GameScreenLClickEvent_Targeting);
    patch->Hook(bw::DoTargetedCommand, [](uint16_t x, uint16_t y, Unit *target, uint16_t fow_unit) {
        DoTargetedCommand(x, y, target, UnitType(fow_unit));
    });

    patch->Hook(bw::SendChangeSelectionCommand, SendChangeSelectionCommand);
    patch->Hook(bw::CenterOnSelectionGroup, CenterOnSelectionGroup);
    patch->Hook(bw::SelectHotkeyGroup, SelectHotkeyGroup);
    patch->Hook(bw::Command_SaveHotkeyGroup, [](uint8_t group, int create) {
        Command_SaveHotkeyGroup(group, create == 0);
    });
    patch->Hook(bw::Command_SelectHotkeyGroup, Command_LoadHotkeyGroup);
    patch->Hook(bw::TrySelectRecentHotkeyGroup, TrySelectRecentHotkeyGroup);
    patch->Hook(bw::ProcessCommands, ProcessCommands);

    patch->Hook(bw::ReplayCommands_Nothing, [](const void *) {});

    patch->Hook(bw::UpdateBuildingPlacementState,
        [](Unit *a, int b, int c, int d, uint16_t e, int f, int g, int h, int i) {
        return UpdateBuildingPlacementState(a, b, c, d, UnitType(e), f, g, h, i);
    });

    patch->Hook(bw::PlaySelectionSound, PlaySelectionSound);
}
