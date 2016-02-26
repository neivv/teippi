#include "limits.h"

#include "offsets_hooks.h"
#include "offsets.h"
#include "patchmanager.h"
#include "image.h"
#include "sprite.h"
#include "unit.h"
#include "bullet.h"
#include "triggers.h"
#include "dialog.h"
#include "commands.h"
#include "order.h"
#include "unitsearch.h"
#include "bunker.h"
#include "tech.h"
#include "ai.h"
#include "log.h"
#include "game.h"
#include "iscript.h"
#include "save.h"
#include "pathing.h"
#include "selection.h"
#include "replay.h"
#include "warn.h"
#include "rng.h"
#include "flingy.h"
#include "init.h"
#include "targeting.h"
#include "player.h"

// Hack from game.cpp
extern bool unitframes_in_progress;

void ShowRallyTarget_Hook();
void __stdcall DamageUnit_Hook(Unit *attacker, int attacking_player, bool show_attacker);

void ShowCursorMarker_Hook()
{
    REG_ECX(int, x);
    REG_EAX(int, y);
    x &= 0xffff;
    y &= 0xffff;
    ShowCursorMarker(x, y);
}

void __stdcall AddMultipleOverlaySprites(int overlay_type, int count, int sprite_id, bool flip)
{
    REG_EBX(Sprite *, sprite);
    REG_EAX(int, base);
    base &= 0xffff;
    return sprite->AddMultipleOverlaySprites(overlay_type, count - base + 1, sprite_id, base, flip);
}

void LoadUnit()
{
    REG_EAX(Unit *, transport);
    REG_ECX(Unit *, unit);
    transport->LoadUnit(unit);
}

int UnloadUnit()
{
    REG_EAX(Unit *, unit);
    return unit->related->UnloadUnit(unit);
}

int __stdcall CanLoadUnit(Unit *unit)
{
    REG_EAX(Unit *, transport);
    return transport->CanLoadUnit(unit);
}

int HasLoadedUnits()
{
    REG_EAX(Unit *, transport);
    return transport->HasLoadedUnits();
}

Unit *GetFirstLoadedUnit()
{
    REG_EAX(Unit *,unit);
    return unit->first_loaded;
}

void SendUnloadCommand()
{
    REG_ESI(Unit *, unit);
    uint8_t buf[5];
    buf[0] = commands::Unload;
    *(uint32_t *)(buf + 1) = unit->lookup_id;
    SendCommand(buf, 5);
}

int IsCarryingFlag()
{
    REG_EAX(Unit *, unit);
    return unit->IsCarryingFlag();
}

Unit **__stdcall FindNearbyUnits(int x, int y)
{
    REG_ECX(Unit *, unit);
    return unit_search->FindCollidingUnits(unit, x, y);
}

void AddToPositionSearch()
{
    REG_ESI(Unit *, unit);
    unit_search->Add(unit);
}

int __stdcall FindUnitPosition(int key)
{
    return 0;
}

Unit ** __stdcall FindUnitsRect(Rect16 *rect)
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

int DoUnitsCollide()
{
    REG_ECX(Unit *, first);
    REG_EAX(Unit *, second);
    return unit_search->DoUnitsCollide(first, second);
}

Unit ** __stdcall CheckMovementCollision(int y)
{
    REG_ECX(Unit *, unit);
    REG_EAX(int, x);
    if (x >= 0x8000)
        x |= 0xffff << 16;
    if (y >= 0x8000)
        y |= 0xffff << 16;
    return unit_search->CheckMovementCollision(unit, x, y);
}

Unit **FindUnitBordersRect()
{
    REG_EAX(Rect16 *, rect);
    Rect16 r = *rect;
    if (r.right < r.left)
        r.left = 0;
    if (r.bottom < r.top)
        r.top = 0;
    return unit_search->FindUnitBordersRect(&r);
}

Unit * __stdcall FindNearestUnit(Unit *a, uint16_t b, uint16_t c, int d, int e, int f, int g, int (__fastcall *h)(const Unit *, void *), void *i)
{
    REG_EAX(Rect16 *, area);
    if (area->left > area->right)
        area->left = 0;
    if (area->top > area->bottom)
        area->top = 0;
    return unit_search->FindNearestUnit(a, Point(b, c), h, i, *area);
}

void ClearPositionSearch()
{
    unit_search->Clear();
}

void __stdcall ChangeUnitPosition(int y_diff)
{
    REG_ESI(Unit *, unit);
    REG_EAX(int, x_diff);
    unit_search->ChangeUnitPosition(unit, x_diff, y_diff);
}

void GetNearbyBlockingUnits()
{
    REG_ESI(PathingData *, pd);
    unit_search->GetNearbyBlockingUnits(pd);
}

int GetDodgingDirection()
{
    REG_EDI(Unit *, self);
    REG_EBX(Unit *, other);
    return unit_search->GetDodgingDirection(self, other);
}

void RemoveFromPosSearch()
{
    REG_ESI(Unit *, unit);
    unit_search->Remove(unit);
}

Image *AllocateImage()
{
    return new Image;
}

void DeleteImage()
{
    REG_ESI(Image *, image);
    image->SingleDelete();
}

void __stdcall CancelZergBuilding(Unit *unit)
{
    //Warning("Calling Unit::Kill with nullptr from CancelZergBuilding (Extractor)");
    // This *should* not matter, as human cancels are at least done from ProcessCommands
    unit->CancelZergBuilding(nullptr);
}

Sprite * __stdcall CreateSprite(int sprite_id, int x, int player)
{
    REG_EDI(int, y);
    y &= 0xffff;
    return Sprite::AllocateWithBasicIscript(sprite_id, Point(x, y), player);
}

Sprite * __stdcall CreateLoneSprite(int sprite_id, int x, int player)
{
    REG_EDI(int, y);
    y &= 0xffff;
    return lone_sprites->AllocateLone(sprite_id, Point(x, y), player);
}

Sprite * __stdcall CreateFowSprite(int unit_id, Sprite *base)
{
    return lone_sprites->AllocateFow(base, unit_id);
}

void DeleteSprite()
{
    REG_EDI(Sprite *, sprite);
    sprite->Remove();
    delete sprite;
}

Bullet * __stdcall CreateBullet(int x, int y, int player, int direction)
{
    REG_EAX(Unit *, parent);
    REG_ECX(int, weapon_id);
    player &= 0xff; // With uint8_t player gcc wants to copy it and "accidentaly" overwrites eax; now it won't even and

    auto ret = bullet_system->AllocateBullet(parent, player, direction, weapon_id, Point(x, y));
    return ret;
}

extern "C" Unit *AllocateUnit()
{
    return new Unit;
}

Order * __stdcall CreateOrder(uint32_t position_xy, Unit *target)
{
    REG_ECX(int, order);
    REG_EDX(int, fow_unit_id);
    order &= 0xff;
    fow_unit_id &= 0xffff;
    return Order::Allocate(order & 0xff, position_xy, target, fow_unit_id & 0xffff);
}

Order *DeleteOrder()
{
    REG_EAX(Order *, order);
    REG_ECX(Unit *, unit);
    unit->DeleteOrder(order);
    return order; // Wait what
}

void DeleteSpecificOrder()
{
    REG_EDX(int, order_id);
    REG_ECX(Unit *, unit);
    order_id &= 0xff;
    unit->DeleteSpecificOrder(order_id);
}

void KillSingleUnit()
{
    REG_EAX(Unit *, unit);
    if (unitframes_in_progress)
        Warning("Hooked Unit::Kill while unit frames are progressed (unit %x)", unit->unit_id);
    unit->Kill(nullptr);
}

void Unit_Die()
{
    Warning("Hooked Unit::Die, not doing anything");
}

int DoesBlockArea()
{
    REG_EAX(Unit *, unit);
    REG_EDI(CollisionArea *, area);
    return unit_search->DoesBlockArea(unit, area);
}

Unit ** __stdcall FindUnitsPoint(uint16_t y)
{
    REG_EAX(int, x);
    x &= 0xffff;
    Rect16 area(x, y, x + 1, y + 1);
    return unit_search->FindUnitsRect(Rect16(x, y, x + 1, y + 1));
}

int __stdcall IsTileBlockedBy(Unit *builder, int x_tile, int y_tile, int dont_ignore_reacting, int also_invisible)
{
    REG_EAX(Unit **, units);

    int xpos = x_tile * 32;
    int ypos = y_tile * 32;
    for (Unit *unit = *units++; unit; unit = *units++)
    {
        if (unit == builder)
            continue;
        if (unit->flags & (UnitStatus::Building | UnitStatus::Air))
            continue;
        if (unit->unit_id == Unit::DarkSwarm || unit->unit_id == Unit::DisruptionWeb)
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
int DoesBuildingBlock()
{
    REG_EAX(Unit *, builder);
    // Tiles are 32 bit
    REG_EDX(int, x_tile);
    REG_ECX(int, y_tile);

    if (!builder || ~builder->flags & UnitStatus::Building || builder->unit_id == Unit::NydusCanal)
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

void InitPosSearch()
{
    // Won't be called when loading save tho
    unit_search->Init();
}

void ProgressSpriteFrame()
{
    REG_ESI(Sprite *, sprite);
    // Shouldn't be hooked from elsewhere
    Assert(*bw::active_iscript_unit != nullptr);
    (*bw::active_iscript_unit)->ProgressIscript("ProgressSpriteFrame hook", nullptr);
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

void __stdcall SetIscriptAnimation(int anim)
{
    REG_ECX(Image *, img);
    anim &= 0xff;
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
                auto speed = CalculateSpeedChange(unit, cmd.val * 256);
                *out_speed = speed;
            }
            auto result = img->ConstIscriptCommand(this, script, cmd);
            if (result == Iscript::CmdResult::NotHandled)
                return Iscript::CmdResult::Handled;
            return result;
        }
};

static void __stdcall ProgressIscriptFrame_Hook(Iscript::Script *script, int test_run, uint32_t *out_speed)
{
    REG_ECX(Image *, image);
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

int __stdcall Order_AttackMove_ReactToAttack(uint8_t order)
{
    REG_EAX(Unit *, unit);
    return unit->Order_AttackMove_ReactToAttack(order);
}

void __stdcall Order_AttackMove_TryPickTarget(uint8_t order)
{
    REG_EAX(Unit *, unit);
    unit->Order_AttackMove_TryPickTarget(order);
}

int __stdcall ForEachLoadedUnit(int (__fastcall *Func)(Unit *unit, void *param), void *param)
{
    REG_EAX(Unit *, transport);
    for (Unit *unit = transport->first_loaded; unit; unit = unit->next_loaded)
    {
        if ((*Func)(unit, param))
            return 1;
    }
    return 0;
}

void AddLoadedUnitsToCompletedUnitLbScore()
{
    REG_EAX(Unit *, transport);
    for (Unit *unit = transport->first_loaded; unit; unit = unit->next_loaded)
    {
        AddToCompletedUnitLbScore(unit);
    }
}

int GetUsedSpace_Hook()
{
    REG_ECX(Unit *, transport);
    return transport->GetUsedSpace();
}

int __fastcall Trig_KillUnitGeneric_Hook(Unit *unit, KillUnitArgs *args)
{
    return Trig_KillUnitGeneric(unit, args, args->check_height, false);
}

void __fastcall TriggerPortraitFinished_Hook(Control *ctrl, int timer_id)
{
    DeleteTimer(ctrl, timer_id);
    *bw::trigger_portrait_active = 0;
    // Not including the code which clears waits in singleplayer, as it causes replays to desync
}

int MovementState13()
{
    REG_EAX(Unit *, unit);
    return unit->MovementState13();
}

int MovementState17()
{
    REG_EAX(Unit *, unit);
    return unit->MovementState17();
}

int MovementState20()
{
    REG_EAX(Unit *, unit);
    return unit->MovementState20();
}

int MovementState1c()
{
    REG_EAX(Unit *, unit);
    return unit->MovementState1c();
}

int MovementState_FollowPath()
{
    REG_EAX(Unit *, unit);
    return unit->MovementState_FollowPath();
}

int MovementState_Flyer()
{
    return 0;
}

int ProgressUnstackMovement()
{
    REG_EAX(Unit *, unit);
    return unit->ProgressUnstackMovement();
}

int ChangeMovementTargetToUnit()
{
    REG_ESI(Unit *, unit);
    REG_EDI(Unit *, target);
    //debug_log->Log("Cmttu %p %p %p\n", unit, target, __builtin_return_address(0));
    return unit->ChangeMovementTargetToUnit(target);
}

int ChangeMovementTarget()
{
    REG_ESI(Unit *, unit);
    REG_EBX(int, x);
    REG_EDI(int, y);
    x &= 0xffff;
    y &= 0xffff;
    //debug_log->Log("Cmt %p %d:%d %p\n", unit, x, y, __builtin_return_address(0));
    return unit->ChangeMovementTarget(Point(x, y));
}

Unit *UnitToIndex()
{
    REG_ESI(Unit *, unit);
    return unit;
}

Unit *IndexToUnit()
{
    REG_EAX(Unit *, unit);
    return unit;
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

void DeletePath()
{
    REG_ESI(Unit *, unit);
    unit->DeletePath();
}

Path *AllocatePath_Hook()
{
    REG_EDX(uint16_t *, region_count);
    REG_ESI(uint16_t *, position_count);
    return AllocatePath(region_count, position_count);
}

void __stdcall CreateSimplePath_Hook(uint32_t waypoint_xy, uint32_t end_xy)
{
    REG_EAX(Unit *, unit);
    CreateSimplePath(unit, Point(waypoint_xy & 0xffff, waypoint_xy >> 16), Point(end_xy & 0xffff, end_xy >> 16));
}

uint32_t LoadReplayData_Hook()
{
    REG_EAX(char *, filename);
    REG_EDI(uint32_t *, is_bw);
    return LoadReplayData(filename, is_bw);
}

static void DoNextQueuedOrder()
{
    REG_ECX(Unit *, unit);
    unit->DoNextQueuedOrder();
}

static void InitUnitSystem_Hook()
{
    Unit::DeleteAll();
}

static void InitSpriteSystem_Hook()
{
    lone_sprites->DeleteAll();
}

static void __stdcall BriefingOk_Hook(int leave)
{
    REG_EDI(Dialog *, ctrl);
    BriefingOk(ctrl, leave);
}

static void CreateBunkerShootOverlay_Hook()
{
    REG_EAX(Unit *, unit);
    CreateBunkerShootOverlay(unit);
}

static int ProgressFlingyTurning_Hook()
{
    REG_ECX(Flingy *, flingy);
    return flingy->ProgressTurning();
}

static void SetMovementDirectionToTarget()
{
    REG_ECX(Flingy *, flingy);
    flingy->SetMovementDirectionToTarget();
}

static void ProgressMove_Hook()
{
    REG_ESI(Flingy *, flingy);
    FlingyMoveResults unused;
    flingy->ProgressMove(&unused);
}

static void * __stdcall LoadGrp_Hook(GrpSprite **loaded_grps, void **overlapped, void **out_file)
{
    REG_ECX(int, image_id);
    REG_EAX(uint32_t *, grp_file_arr);
    REG_EDX(Tbl *, images_tbl);
    return LoadGrp(image_id, grp_file_arr, images_tbl, loaded_grps, overlapped, out_file);
}

static int IsDrawnPixel()
{
    REG_EAX(GrpFrameHeader *, frame);
    REG_EDX(int, x);
    REG_ECX(int, y);
    if (x < 0 || x >= frame->w)
        return 0;
    if (y < 0 || y >= frame->h)
        return 0;
    return frame->GetPixel(x, y) != 0;
}

static void __stdcall LoadBlendPalettes_Hook(const char *tileset)
{
    LoadBlendPalettes(tileset);
}

static void __fastcall DrawImage_Detected_Hook(int x, int y, GrpFrameHeader * frame_header, Rect32 * rect, uint8_t * blend_table)
{
    DrawBlended_NonFlipped(x, y, frame_header, rect, blend_table);
}

static void __fastcall DrawImage_Detected_Flipped_Hook(int x, int y, GrpFrameHeader * frame_header, Rect32 * rect, uint8_t * blend_table)
{
    DrawBlended_Flipped(x, y, frame_header, rect, blend_table);
}

static void __fastcall DrawUncloakedPart_Hook(int x, int y, GrpFrameHeader * frame_header, Rect32 * rect, int state)
{
    DrawUncloakedPart_NonFlipped(x, y, frame_header, rect, state & 0xff);
}

static void __fastcall DrawUncloakedPart_Flipped_Hook(int x, int y, GrpFrameHeader * frame_header, Rect32 * rect, int state)
{
    DrawUncloakedPart_Flipped(x, y, frame_header, rect, state & 0xff);
}

static void __fastcall DrawImage_Cloaked_Hook(int x, int y, GrpFrameHeader * frame_header, Rect32 * rect, void *unused)
{
    DrawCloaked_NonFlipped(x, y, frame_header, rect, unused);
}

static void __fastcall DrawImage_Cloaked_Flipped_Hook(int x, int y, GrpFrameHeader * frame_header, Rect32 * rect, void *unused)
{
    DrawCloaked_Flipped(x, y, frame_header, rect, unused);
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

static Unit * __stdcall FindUnitAtPoint_Hook(int x, int y)
{
    return FindUnitAtPoint(x, y);
}

static void __stdcall MakeJoinedGameCommand_Hook(uint32_t a, uint32_t b, uint32_t c,
        uint32_t d, uint32_t e, uint32_t f, bool create)
{
    MakeJoinedGameCommand(a, b, e, d, f, create);
}

static void __stdcall Command_GameData_Hook(uint8_t *data, int net_player)
{
    Command_GameData(data, net_player);
}


static void Neutralize_Hook()
{
    REG_EAX(int, player);
    Neutralize(player & 0xff);
}

static void MakeDetected_Hook()
{
    REG_EAX(Sprite *, sprite);
    if (sprite->flags & 0x40)
        RemoveCloakDrawfuncs(sprite);
    else
    {
        for (Image *img : sprite->first_overlay)
            img->MakeDetected();
    }
}

static void __stdcall AddDamageOverlay_Hook(Sprite *sprite)
{
    sprite->AddDamageOverlay();
}

static void __stdcall DoTargetedCommand_Hook(int x, int y, Unit *target, int fow_unit)
{
    x &= 0xffff;
    y &= 0xffff;
    fow_unit &= 0xffff;
    DoTargetedCommand(x, y, target, fow_unit);
}

static void __stdcall SendChangeSelectionCommand_Hook(int count, Unit **units)
{
    SendChangeSelectionCommand(count, units);
}

static void CenterOnSelectionGroup_Hook()
{
    REG_ECX(int, group);
    group &= 0xff;
    CenterOnSelectionGroup(group);
}

static void __stdcall SelectHotkeyGroup_Hook(int group)
{
    group &= 0xff;
    SelectHotkeyGroup(group);
}

static void __stdcall Command_SaveHotkeyGroup_Hook(int create_new)
{
    REG_EAX(int, group_id);
    group_id &= 0xff;
    Command_SaveHotkeyGroup(group_id, create_new == 0);
}

static void __stdcall Command_SelectHotkeyGroup_Hook(int group)
{
    group &= 0xff;
    Command_LoadHotkeyGroup(group);
}

static int __stdcall TrySelectRecentHotkeyGroup_Hook(Unit *unit)
{
    return TrySelectRecentHotkeyGroup(unit);
}

static void StatusScreenButton_Hook()
{
    REG_EDX(Control *, clicked_button);
    StatusScreenButton(clicked_button);
}

static void __stdcall ProcessCommands_Hook(int length, int replay_process)
{
    REG_EAX(uint8_t *, data);
    ProcessCommands(data, length, replay_process);
}

static void RemoveFromBulletTargets_Hook()
{
    REG_ECX(Unit *, unit);
    RemoveFromBulletTargets(unit);
}

void RemoveLimits(Common::PatchContext *patch)
{
    Ai::RemoveLimits(patch);

    patch->JumpHook(bw::ProgressObjects, ProgressObjects);
    patch->JumpHook(bw::GameFunc, ProgressFrames);

    patch->JumpHook(bw::CreateOrder, CreateOrder);
    patch->JumpHook(bw::DeleteOrder, DeleteOrder);
    patch->JumpHook(bw::DeleteSpecificOrder, DeleteSpecificOrder);

    patch->JumpHook(bw::GetEmptyImage, AllocateImage);
    patch->JumpHook(bw::DeleteImage, DeleteImage);
    patch->Patch(bw::IscriptEndRetn, (uint8_t *)bw::IscriptEndRetn.raw_pointer() + 0xe, 0, PATCH_JMPHOOK);

    patch->JumpHook(bw::CreateSprite, CreateSprite);
    patch->JumpHook(bw::ProgressSpriteFrame, ProgressSpriteFrame);
    patch->JumpHook(bw::DeleteSprite, DeleteSprite);
    patch->JumpHook(bw::DrawAllMinimapUnits, ::DrawMinimapUnits);
    patch->JumpHook(bw::CreateLoneSprite, CreateLoneSprite);
    patch->JumpHook(bw::CreateFowSprite, CreateFowSprite);
    patch->JumpHook(bw::InitLoneSprites, InitCursorMarker);
    patch->JumpHook(bw::DrawCursorMarker, DrawCursorMarker);
    patch->JumpHook(bw::ShowRallyTarget, ShowRallyTarget_Hook);
    patch->JumpHook(bw::ShowCursorMarker, ShowCursorMarker_Hook);
    patch->JumpHook(bw::SetSpriteDirection, SetSpriteDirection);
    patch->JumpHook(bw::FindBlockingFowResource, FindBlockingFowResource);

    patch->Patch(bw::InitSprites_JmpSrc, bw::InitSprites_JmpDest, 0, PATCH_JMPHOOK);
    patch->JumpHook(bw::CreateBunkerShootOverlay, CreateBunkerShootOverlay_Hook);

    uint8_t unithook_asm[6] = { 0x52, 0x89, 0xc1, 0x5a, 0x90, 0x90 };
    patch->Patch(bw::GetEmptyUnitHook, unithook_asm, 6, PATCH_REPLACE);
    patch->Patch((uint8_t *)bw::GetEmptyUnitHook.raw_pointer() + 1, (void *)&AllocateUnit, 0, PATCH_CALLHOOK | PATCH_HOOKBEFORE);
    patch->Patch(bw::GetEmptyUnitNop, 0, 5, PATCH_NOP);
    uint8_t jmp = 0xeb;
    patch->Patch(bw::UnitLimitJump, &jmp, 1, PATCH_REPLACE);
    patch->Patch(bw::InitUnitSystem, (void *)&InitUnitSystem_Hook, 0, PATCH_SAFECALLHOOK);
    patch->Patch(bw::InitSpriteSystem, (void *)&InitSpriteSystem_Hook, 0, PATCH_SAFECALLHOOK);

    patch->JumpHook(bw::CreateBullet, CreateBullet);

    patch->JumpHook(bw::GameEnd, GameEnd);

    delete unit_search;
    unit_search = new MainUnitSearch;
    patch->JumpHook(bw::AddToPositionSearch, AddToPositionSearch);
    patch->JumpHook(bw::FindUnitPosition, FindUnitPosition);
    patch->JumpHook(bw::FindUnitsRect, FindUnitsRect);
    patch->JumpHook(bw::FindNearbyUnits, FindNearbyUnits);
    patch->JumpHook(bw::DoUnitsCollide, DoUnitsCollide);
    patch->JumpHook(bw::CheckMovementCollision, CheckMovementCollision);
    patch->JumpHook(bw::FindUnitBordersRect, FindUnitBordersRect);
    patch->Patch(bw::ClearPositionSearch, (void *)&ClearPositionSearch, 0, PATCH_SAFECALLHOOK);
    patch->JumpHook(bw::ChangeUnitPosition, ChangeUnitPosition);
    patch->JumpHook(bw::FindNearestUnit, FindNearestUnit);
    patch->JumpHook(bw::GetNearbyBlockingUnits, GetNearbyBlockingUnits);
    patch->JumpHook(bw::RemoveFromPosSearch, RemoveFromPosSearch);
    patch->JumpHook(bw::FindUnitsPoint, FindUnitsPoint);
    uint8_t zero_unitsearch_asm[] = { 0x31, 0xc0, 0x90 };
    patch->Patch(bw::ZeroOldPosSearch, &zero_unitsearch_asm, 3, PATCH_REPLACE);
    patch->Patch((uint8_t *)bw::ZeroOldPosSearch.raw_pointer() + 0xf, &zero_unitsearch_asm, 3, PATCH_NOP);
    patch->JumpHook(bw::GetDodgingDirection, GetDodgingDirection);
    patch->JumpHook(bw::DoesBlockArea, DoesBlockArea);
    patch->JumpHook(bw::IsTileBlockedBy, IsTileBlockedBy);
    patch->JumpHook(bw::DoesBuildingBlock, DoesBuildingBlock);

    patch->JumpHook(bw::UnitToIndex, UnitToIndex);
    patch->JumpHook(bw::IndexToUnit, IndexToUnit);

    patch->Patch(bw::MakeDrawnSpriteList_Call, 0, 5, PATCH_NOP);
    patch->Patch(bw::PrepareDrawSprites, (void *)&Sprite::CreateDrawSpriteList, 0, PATCH_JMPHOOK);
    patch->Patch(bw::FullRedraw, (void *)&Sprite::CreateDrawSpriteListFullRedraw, 0, PATCH_CALLHOOK);
    patch->Patch(bw::DrawSprites, (void *)&Sprite::DrawSprites, 0, PATCH_JMPHOOK);
    patch->Patch(bw::DisableVisionSync, (uint8_t *)bw::DisableVisionSync.raw_pointer() + 12, 0, PATCH_JMPHOOK);

    patch->JumpHook(bw::RemoveUnitFromBulletTargets, RemoveFromBulletTargets_Hook);
    patch->JumpHook(bw::DamageUnit, DamageUnit_Hook);

    patch->JumpHook(bw::FindUnitInLocation_Check, FindUnitInLocation_Check);
    patch->JumpHook(bw::ChangeInvincibility, ChangeInvincibility);
    patch->JumpHook(bw::CanLoadUnit, CanLoadUnit);
    patch->JumpHook(bw::LoadUnit, LoadUnit);
    patch->JumpHook(bw::HasLoadedUnits, HasLoadedUnits);
    patch->JumpHook(bw::IsCarryingFlag, IsCarryingFlag);
    patch->JumpHook(bw::UnloadUnit, UnloadUnit);
    patch->JumpHook(bw::SendUnloadCommand, SendUnloadCommand);
    patch->JumpHook(bw::GetFirstLoadedUnit, GetFirstLoadedUnit);
    patch->JumpHook(bw::ForEachLoadedUnit, ForEachLoadedUnit);
    patch->JumpHook(bw::AddLoadedUnitsToCompletedUnitLbScore, AddLoadedUnitsToCompletedUnitLbScore);
    patch->JumpHook(bw::GetUsedSpace, GetUsedSpace_Hook);

    patch->JumpHook(bw::DrawStatusScreen_LoadedUnits, DrawStatusScreen_LoadedUnits);
    patch->JumpHook(bw::TransportStatus_UpdateDrawnValues, TransportStatus_UpdateDrawnValues);
    patch->JumpHook(bw::TransportStatus_DoesNeedRedraw, TransportStatus_DoesNeedRedraw);
    patch->JumpHook(bw::StatusScreen_DrawKills, StatusScreen_DrawKills);
    patch->JumpHook(bw::AddMultipleOverlaySprites, AddMultipleOverlaySprites);

    patch->JumpHook(bw::KillSingleUnit, KillSingleUnit);
    patch->JumpHook(bw::Unit_Die, Unit_Die);
    patch->JumpHook(bw::CancelZergBuilding, CancelZergBuilding);

    patch->JumpHook(bw::SetIscriptAnimation, SetIscriptAnimation);
    patch->JumpHook(bw::ProgressIscriptFrame, ProgressIscriptFrame_Hook);

    patch->JumpHook(bw::Order_AttackMove_ReactToAttack, Order_AttackMove_ReactToAttack);
    patch->JumpHook(bw::Order_AttackMove_TryPickTarget, Order_AttackMove_TryPickTarget);

    patch->Patch(bw::PathingInited, (void *)&InitPosSearch, 0, PATCH_SAFECALLHOOK);

    patch->JumpHook(bw::ProgressUnstackMovement, ProgressUnstackMovement);
    patch->JumpHook(bw::MovementState13, MovementState13);
    patch->JumpHook(bw::MovementState17, MovementState17);
    patch->JumpHook(bw::MovementState20, MovementState20);
    patch->JumpHook(bw::MovementState1c, MovementState1c);
    patch->JumpHook(bw::MovementState_FollowPath, MovementState_FollowPath);
    patch->JumpHook(bw::MovementState_Flyer, MovementState_Flyer);

    patch->JumpHook(bw::Trig_KillUnitGeneric, Trig_KillUnitGeneric_Hook);
    patch->JumpHook(bw::TriggerPortraitFinished, TriggerPortraitFinished_Hook);
    bw::trigger_actions[0x7] = TrigAction_Transmission;
    bw::trigger_actions[0xa] = TrigAction_CenterView;

    patch->JumpHook(bw::ChangeMovementTargetToUnit, ChangeMovementTargetToUnit);
    patch->JumpHook(bw::ChangeMovementTarget, ChangeMovementTarget);

    patch->JumpHook(bw::Sc_fclose, fclose_hook);
    patch->JumpHook(bw::Sc_fopen, fopen_hook);
    patch->JumpHook(bw::Sc_fwrite, fwrite_hook);
    patch->JumpHook(bw::Sc_fread, fread_hook);
    patch->JumpHook(bw::Sc_fgetc, fgetc_hook);
    patch->JumpHook(bw::Sc_fseek, fseek_hook);
    patch->JumpHook(bw::Sc_setvbuf, setvbuf_hook);
    patch->JumpHook(bw::LoadGameObjects, LoadGameObjects);

    patch->JumpHook(bw::AllocatePath, AllocatePath_Hook);
    patch->JumpHook(bw::DeletePath, DeletePath);
    patch->JumpHook(bw::DeletePath2, DeletePath);
    patch->JumpHook(bw::CreateSimplePath, CreateSimplePath_Hook);
    patch->Patch(bw::InitPathArray, (void *)0xc3, 1, PATCH_REPLACE_DWORD);

    patch->JumpHook(bw::StatusScreenButton, StatusScreenButton_Hook);
    patch->JumpHook(bw::LoadReplayMapDirEntry, LoadReplayMapDirEntry);
    patch->JumpHook(bw::LoadReplayData, LoadReplayData_Hook);

    patch->JumpHook(bw::DoNextQueuedOrder, DoNextQueuedOrder);
    patch->JumpHook(bw::ProcessLobbyCommands, ProcessLobbyCommands);
    patch->JumpHook(bw::BriefingOk, BriefingOk_Hook);

    patch->JumpHook(bw::ProgressFlingyTurning, ProgressFlingyTurning_Hook);
    patch->JumpHook(bw::SetMovementDirectionToTarget, SetMovementDirectionToTarget);
    patch->JumpHook(bw::ProgressMove, ProgressMove_Hook);

    patch->JumpHook(bw::LoadGrp, LoadGrp_Hook);
    patch->JumpHook(bw::IsDrawnPixel, IsDrawnPixel);
    patch->JumpHook(bw::LoadBlendPalettes, LoadBlendPalettes_Hook);
    patch->JumpHook(bw::DrawImage_Detected, DrawImage_Detected_Hook);
    patch->JumpHook(bw::DrawImage_Detected_Flipped, DrawImage_Detected_Flipped_Hook);
    patch->JumpHook(bw::DrawUncloakedPart, DrawUncloakedPart_Hook);
    patch->JumpHook(bw::DrawUncloakedPart_Flipped, DrawUncloakedPart_Flipped_Hook);
    patch->JumpHook(bw::DrawImage_Cloaked, DrawImage_Cloaked_Hook);
    patch->JumpHook(bw::DrawImage_Cloaked_Flipped, DrawImage_Cloaked_Flipped_Hook);
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

    patch->JumpHook(bw::FindUnitAtPoint, FindUnitAtPoint_Hook);
    patch->JumpHook(bw::MakeJoinedGameCommand, MakeJoinedGameCommand_Hook);
    patch->JumpHook(bw::Command_GameData, Command_GameData_Hook);
    patch->JumpHook(bw::InitGame, InitGame);
    patch->JumpHook(bw::InitStartingRacesAndTypes, InitStartingRacesAndTypes);

    patch->JumpHook(bw::NeutralizePlayer, Neutralize_Hook);
    patch->JumpHook(bw::MakeDetected, MakeDetected_Hook);

    patch->JumpHook(bw::AddDamageOverlay, AddDamageOverlay_Hook);

    patch->JumpHook(bw::GameScreenRClickEvent, GameScreenRClickEvent);
    patch->JumpHook(bw::GameScreenLClickEvent_Targeting, GameScreenLClickEvent_Targeting);
    patch->JumpHook(bw::DoTargetedCommand, DoTargetedCommand_Hook);

    patch->JumpHook(bw::SendChangeSelectionCommand, SendChangeSelectionCommand_Hook);
    patch->JumpHook(bw::CenterOnSelectionGroup, CenterOnSelectionGroup_Hook);
    patch->JumpHook(bw::SelectHotkeyGroup, SelectHotkeyGroup_Hook);
    patch->JumpHook(bw::Command_SaveHotkeyGroup, Command_SaveHotkeyGroup_Hook);
    patch->JumpHook(bw::Command_SelectHotkeyGroup, Command_SelectHotkeyGroup_Hook);
    patch->JumpHook(bw::TrySelectRecentHotkeyGroup, TrySelectRecentHotkeyGroup_Hook);

    patch->JumpHook(bw::ProcessCommands, ProcessCommands_Hook);

    char retn = 0xc3;
    patch->Patch(bw::ReplayCommands_Nothing, &retn, 1, PATCH_REPLACE);
}
