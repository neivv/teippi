#include "iscript.h"

#include "offsets.h"
#include "image.h"
#include "unit.h"
#include "bullet.h"
#include "rng.h"
#include "lofile.h"
#include "sprite.h"
#include "flingy.h"
#include "upgrade.h"
#include "warn.h"
#include "yms.h"

#include "console/assert.h"

std::string Iscript::Command::DebugStr() const
{
    switch (opcode)
    {
        case IscriptOpcode::PlayFram:
            return "playfram";
        case IscriptOpcode::PlayFramTile:
            return "playframtile";
        case IscriptOpcode::SetHorPos:
            return "sethorpos";
        case IscriptOpcode::SetVertPos:
            return "setvertpos";
        case IscriptOpcode::SetPos:
            return "setpos";
        case IscriptOpcode::Wait:
            return "wait";
        case IscriptOpcode::WaitRand:
            return "waitrand";
        case IscriptOpcode::Goto:
            return "goto";
        case IscriptOpcode::ImgOl:
            return "imgol";
        case IscriptOpcode::ImgUl:
            return "imgul";
        case IscriptOpcode::ImgOlOrig:
            return "imgolorig";
        case IscriptOpcode::SwitchUl:
            return "switchul";
        case IscriptOpcode::UnusedC:
            return "unusedc";
        case IscriptOpcode::ImgOlUseLo:
            return "imgoluselo";
        case IscriptOpcode::ImgUlUseLo:
            return "imguluselo";
        case IscriptOpcode::SprOl:
            return "sprol";
        case IscriptOpcode::HighSprOl:
            return "highsprol";
        case IscriptOpcode::LowSprUl:
            return "lowsprul";
        case IscriptOpcode::UflUnstable:
            return "uflunstable";
        case IscriptOpcode::SprUlUseLo:
            return "spruluselo";
        case IscriptOpcode::SprUl:
            return "sprul";
        case IscriptOpcode::SprOlUseLo:
            return "sproluselo";
        case IscriptOpcode::End:
            return "end";
        case IscriptOpcode::SetFlipState:
            return "setflipstate";
        case IscriptOpcode::PlaySnd:
            return "playsnd";
        case IscriptOpcode::PlaySndRand:
            return "playsndrand";
        case IscriptOpcode::PlaySndBtwn:
            return "playsndbtwn";
        case IscriptOpcode::DoMissileDmg:
            return "domissiledmg";
        case IscriptOpcode::AttackMelee:
            return "attackmelee";
        case IscriptOpcode::FollowMainGraphic:
            return "followmaingraphic";
        case IscriptOpcode::RandCondJmp:
            return "randcondjmp";
        case IscriptOpcode::TurnCcWise:
            return "turnccwise";
        case IscriptOpcode::TurnCWise:
            return "turncwise";
        case IscriptOpcode::Turn1CWise:
            return "turn1cwise";
        case IscriptOpcode::TurnRand:
            return "turnrand";
        case IscriptOpcode::SetSpawnFrame:
            return "setspawnframe";
        case IscriptOpcode::SigOrder:
            return "sigorder";
        case IscriptOpcode::AttackWith:
            return "attackwith";
        case IscriptOpcode::Attack:
            return "attack";
        case IscriptOpcode::CastSpell:
            return "castspell";
        case IscriptOpcode::UseWeapon:
            return "useweapon";
        case IscriptOpcode::Move:
            return "move";
        case IscriptOpcode::GotoRepeatAttk:
            return "gotorepeatattk";
        case IscriptOpcode::EngFrame:
            return "engframe";
        case IscriptOpcode::EngSet:
            return "engset";
        case IscriptOpcode::HideCursorMarker:
            return "hidecursormarker";
        case IscriptOpcode::NoBrkCodeStart:
            return "nobrkcodestart";
        case IscriptOpcode::NoBrkCodeEnd:
            return "nobrkcodeend";
        case IscriptOpcode::IgnoreRest:
            return "ignorerest";
        case IscriptOpcode::AttkShiftProj:
            return "attkshiftproj";
        case IscriptOpcode::TmpRmGraphicStart:
            return "tmprmgraphicstart";
        case IscriptOpcode::TmpRmGraphicEnd:
            return "tmprmgraphicend";
        case IscriptOpcode::SetFlDirect:
            return "setfldirect";
        case IscriptOpcode::Call:
            return "call";
        case IscriptOpcode::Return:
            return "return";
        case IscriptOpcode::SetFlSpeed:
            return "setflspeed";
        case IscriptOpcode::CreateGasOverlays:
            return "creategasoverlays";
        case IscriptOpcode::PwrupCondJmp:
            return "pwrupcondjmp";
        case IscriptOpcode::TrgtRangeCondJmp:
            return "trgtrangecondjmp";
        case IscriptOpcode::TrgtArcCondJmp:
            return "trgtarccondjmp";
        case IscriptOpcode::CurDirectCondJmp:
            return "curdirectcondjmp";
        case IscriptOpcode::ImgUlNextId:
            return "imgulnextid";
        case IscriptOpcode::Unused3e:
            return "unused3e";
        case IscriptOpcode::LiftoffCondJmp:
            return "liftoffcondjmp";
        case IscriptOpcode::WarpOverlay:
            return "warpoverlay";
        case IscriptOpcode::OrderDone:
            return "orderdone";
        case IscriptOpcode::GrdSprOl:
            return "grdsprol";
        case IscriptOpcode::Unused43:
            return "unused43";
        case IscriptOpcode::DoGrdDamage:
            return "dogrddamage";
        default:
        {
            char buf[32];
            snprintf(buf, sizeof buf, "Unknown (%x)", opcode);
            return buf;
        }
    }
}

Image *Image::Iscript_AddOverlay(const IscriptContext *ctx, int image_id_, int x, int y, bool above)
{
    Image *img = new Image;
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
    InitializeImageFull(image_id_, img, x, y, parent);
    if (img->drawfunc == Normal && ctx->unit && ctx->unit->flags & UnitStatus::Hallucination)
    {
        if (ctx->unit->CanLocalPlayerControl() || IsReplay())
            img->SetDrawFunc(Hallucination, nullptr);
    }
    if (img->flags & 0x8)
    {
        if (IsFlipped())
            SetImageDirection32(img, 32 - direction);
        else
            SetImageDirection32(img, direction);
    }
    if (img->frame != img->frameset + img->direction)
    {
        img->frame = img->frameset + img->direction;
        img->flags |= 0x1;
    }
    if (ctx->unit && ctx->unit->IsInvisible())
    {
        if (images_dat_draw_if_cloaked[img->image_id])
        {
            // Note: main_img may be null if this is some death anim overlay
            // Related to comment in Image::SingleDelete
            auto main_img = parent->main_image;
            if (img->drawfunc == Normal && main_img && main_img->drawfunc >= Cloaking && main_img->drawfunc <= DetectedDecloaking)
                img->SetDrawFunc(main_img->drawfunc, main_img->drawfunc_param);
        }
        else
            img->Hide();
    }
    return img;
}

bool Image::IscriptCmd(const Iscript::Command &cmd, IscriptContext *ctx, Rng *rng)
{
    using namespace IscriptOpcode;
    switch (cmd.opcode)
    {
        case PlayFram:
            if (cmd.val + direction < grp->frame_count)
                SetFrame(cmd.val);
            else
                Warning("Iscript for image %x sets image to invalid frame %x", image_id, cmd.val);
        break;
        case PlayFramTile:
            if (cmd.val + *bw::tileset < grp->frame_count)
                SetFrame(cmd.val + *bw::tileset);
        break;
        case SetHorPos:
            SetOffset(cmd.point.x, y_off);
        break;
        case SetVertPos:
            if (!ctx->unit || !ctx->unit->IsInvisible())
                SetOffset(x_off, cmd.point.y);
        break;
        case SetPos:
            SetOffset(cmd.point.x, cmd.point.y);
        break;
        case Wait:
            iscript.wait = cmd.val - 1;
        break;
        case WaitRand:
            iscript.wait = cmd.val1() + rng->Rand(cmd.val2() - cmd.val1() + 1);
        break;
        case ImgOl:
        case ImgUl:
            Iscript_AddOverlay(ctx, cmd.val, x_off + cmd.point.x, y_off + cmd.point.y, cmd.opcode == ImgOl);
        break;
        case ImgOlOrig:
        case SwitchUl:
        {
            Image *other = Iscript_AddOverlay(ctx, cmd.val, 0, 0, cmd.opcode == ImgOlOrig);
            if (other && ~other->flags & 0x80)
            {
                other->flags |= 0x80;
                SetOffsetToParentsSpecialOverlay(other);
            }
        }
        break;
        case ImgOlUseLo:
        case ImgUlUseLo:
        {
            // Yeah, it's not actually point
            Point32 point = LoFile::GetOverlay(image_id, cmd.point.x).GetValues(this, cmd.point.y);
            Iscript_AddOverlay(ctx, cmd.val, point.x + x_off, point.y + y_off, cmd.opcode == ImgOlUseLo);
        }
        break;
        case ImgUlNextId:
            Iscript_AddOverlay(ctx, image_id + 1, cmd.point.x + x_off, cmd.point.y + y_off, false);
        break;
        case SprOl:
        {
            int sprite_id = cmd.val;
            if (ctx->bullet && ctx->bullet->parent && ctx->bullet->parent->IsGoliath())
            {
                Unit *goliath = ctx->bullet->parent;
                if (GetUpgradeLevel(Upgrade::CharonBooster, goliath->player) ||
                        (units_dat_flags[goliath->unit_id] & UnitFlags::Hero && *bw::is_bw))
                {
                    sprite_id = Sprite::HaloRocketsTrail;
                }
            }
            Sprite::Spawn(this, sprite_id, cmd.point, parent->elevation + 1);
        }
        break;
        case HighSprOl:
            Sprite::Spawn(this, cmd.val, cmd.point, parent->elevation - 1);
        break;
        case LowSprUl:
            Sprite::Spawn(this, cmd.val, cmd.point, 1);
        break;
        case UflUnstable:
        {
            Warning("Flingy creation not implemented (image %x)", image_id);
            /*Flingy *flingy = CreateFlingy(cmd.val, parent->player, cmd.point);
            if (flingy)
            {
                flingy->GiveRandomMoveTarget(rng);
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
            if (!ctx->unit || !ctx->unit->IsInvisible() || images_dat_draw_if_cloaked[cmd.val])
            {
                Sprite *sprite = Sprite::Spawn(this, cmd.val, cmd.point, elevation);
                if (sprite)
                {
                    if (flags & 0x2)
                        sprite->SetDirection32(32 - direction);
                    else
                        sprite->SetDirection32(direction);
                }
            }
        }
        break;
        case SprOlUseLo:
        {
            // Again using the "point" for additional storage
            Point32 point = LoFile::GetOverlay(image_id, cmd.point.x).GetValues(this, 0);
            Sprite *sprite = Sprite::Spawn(this, cmd.val, point.ToPoint16(), parent->elevation + 1);
            if (sprite)
            {
                if (flags & 0x2)
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
            PlaySoundAtPos(cmd.val, parent->position.AsDword(), 1, 0);
        break;
        case PlaySndRand:
        {
            int num = rng->Rand(cmd.data[0]);
            int sound = *(uint16_t *)(cmd.data + 1 + num * 2);
            PlaySoundAtPos(sound, parent->position.AsDword(), 1, 0);
        }
        break;
        case PlaySndBtwn:
        {
            int num = rng->Rand(cmd.val2() - cmd.val1() + 1);
            PlaySoundAtPos(cmd.val1() + num, parent->position.AsDword(), 1, 0);
        }
        break;
        case FollowMainGraphic:
            if (parent->main_image)
            {
                Image *main_img = parent->main_image;
                if (main_img->frame != frame || (main_img->flags & 0x2) != (flags & 0x2))
                {
                    int new_frame = main_img->frameset + main_img->direction;
                    if (new_frame >= grp->frame_count)
                        Warning("Iscript for image %x requested to play frame %x with followmaingraphic", image_id, new_frame);
                    else
                    {
                        frameset = main_img->frameset;
                        direction = main_img->direction;
                        SetFlipping(main_img->flags & 0x2);
                        UpdateFrameToDirection();
                    }
                }
            }
        break;
        case TurnCcWise:
        case TurnCWise:
        case Turn1CWise:
        {
            Unit *entity = ctx->unit != nullptr ? (Unit *)ctx->unit : (Unit *)ctx->bullet;
            if (entity == nullptr)
            {
                Warning("Iscript for image %x uses turn opcode without parent object", image_id);
            }
            else if (cmd.opcode == Turn1CWise && entity->target != nullptr)
            {
                // Nothing. Allows missile turret to pick targets more quickly
                // as it can turn to its target without iscript overriding it ._.
            }
            else
            {
                int direction = 1;
                if (cmd.opcode == TurnCcWise)
                    direction = 0 - cmd.val;
                else if (cmd.opcode == TurnCWise)
                    direction = cmd.val;
                SetDirection((Flingy *)entity, entity->facing_direction + direction * 8);
            }
        }
        break;
        case SetFlDirect:
        {
            Flingy *entity = ctx->unit != nullptr ? (Flingy *)ctx->unit : (Flingy *)ctx->bullet;
            if (entity == nullptr)
            {
                Warning("Iscript for image %x uses setfldirect without parent object", image_id);
            }
            else
            {
                SetDirection(entity, cmd.val * 8);
            }
        }
        break;
        case TurnRand:
        {
            Flingy *entity = ctx->unit != nullptr ? (Flingy *)ctx->unit : (Flingy *)ctx->bullet;
            if (entity == nullptr)
            {
                Warning("Iscript for image %x uses turnrand without parent object", image_id);
            }
            else
            {
                int num = rng->Rand(4);
                if (num == 0)
                    SetDirection(entity, entity->facing_direction - cmd.val * 8);
                else
                    SetDirection(entity, entity->facing_direction + cmd.val * 8);
            }
        }
        break;
        case SetSpawnFrame:
            SetMoveTargetToNearbyPoint(cmd.val, ctx->unit);
        break;
        case SigOrder:
        case OrderDone:
        {
            Unit *entity = ctx->unit != nullptr ? ctx->unit : (Unit *)ctx->bullet;
            if (entity == nullptr)
            {
                Warning("Iscript for image %x uses sigorder/orderdone without parent object", image_id);
            }
            else
            {
                if (cmd.opcode == SigOrder)
                    entity->order_signal |= cmd.val;
                else
                    entity->order_signal &= ~cmd.val;
            }
        }
        break;
        case AttackWith:
            Iscript_AttackWith(ctx->unit, cmd.val);
        break;
        case Attack:
            if (!ctx->unit->target || ctx->unit->target->IsFlying())
                Iscript_AttackWith(ctx->unit, 0);
            else
                Iscript_AttackWith(ctx->unit, 1);
        break;
        case CastSpell:
            if (orders_dat_targeting_weapon[ctx->unit->order] != Weapon::None && !ShouldStopOrderedSpell(ctx->unit))
                FireWeapon(ctx->unit, orders_dat_targeting_weapon[ctx->unit->order]);
        break;
        case UseWeapon:
            Iscript_UseWeapon(cmd.val, ctx->unit);
        break;
        case GotoRepeatAttk:
            if (ctx->unit)
                ctx->unit->flingy_flags &= ~0x8;
        break;
        case EngFrame:
            frameset = cmd.val;
            direction = parent->main_image->direction;
            SetFlipping(parent->main_image->IsFlipped());
            UpdateFrameToDirection();
        break;
        case EngSet:
            frameset = parent->main_image->frameset + parent->main_image->grp->frame_count * cmd.val;
            direction = parent->main_image->direction;
            SetFlipping(parent->main_image->IsFlipped());
            UpdateFrameToDirection();
        break;
        case HideCursorMarker:
            *bw::draw_cursor_marker = 0;
        break;
        case NoBrkCodeStart:
            ctx->unit->flags |= UnitStatus::Nobrkcodestart;
            ctx->unit->sprite->flags |= 0x80;
        break;
        case NoBrkCodeEnd:
        if (ctx->unit)
        {
            ctx->unit->flags &= ~UnitStatus::Nobrkcodestart;
            ctx->unit->sprite->flags &= ~0x80;
            if (ctx->unit->order_queue_begin && ctx->unit->order_flags & 0x1)
            {
                ctx->unit->IscriptToIdle();
                ctx->unit->DoNextQueuedOrder();
            }
        }
        // This actually is feature ._. bunkers can create lone flamethrower sprites
        // whose default iscript has nobreakcodeend
        //else
            //Warning("Iscript for image %x used nobrkcodeend without having unit", image_id);
        break;
        case IgnoreRest:
            if (ctx->unit->target == nullptr)
                ctx->unit->IscriptToIdle();
            else
            {
                iscript.wait = 10;
                iscript.pos -= cmd.Size(); // Loop on this cmd
            }
        break;
        case AttkShiftProj:
            // Sigh
            weapons_dat_x_offset[ctx->unit->GetGroundWeapon()] = cmd.val;
            Iscript_AttackWith(ctx->unit, 1);
        break;
        case TmpRmGraphicStart:
            Hide();
        break;
        case TmpRmGraphicEnd:
            Show();
        break;
        case SetFlSpeed:
            ctx->unit->flingy_top_speed = cmd.val;
        break;
        case CreateGasOverlays:
        {
            Image *img = new Image;
            if (parent->first_overlay == this)
            {
                Assert(list.prev == nullptr);
                parent->first_overlay = img;
            }
            img->list.prev = list.prev;
            img->list.next = this;
            if (list.prev)
                list.prev->list.next = img;
            list.prev = img;
            int smoke_img = VespeneSmokeOverlay1 + cmd.val;
            // Bw can be misused to have this check for loaded nuke and such
            // Even though resource_amount is word, it won't report incorrect values as unit array starts from 0x0059CCA8
            // (The lower word is never 0 if the union contains unit)
            // But with dynamic allocation, that is not the case
            if (units_dat_flags[ctx->unit->unit_id] & UnitFlags::ResourceContainer)
            {
                if (ctx->unit->resource.resource_amount == 0)
                    smoke_img = VespeneSmallSmoke1 + cmd.val;
            }
            else
            {
                if (ctx->unit->silo.nuke == nullptr)
                    smoke_img = VespeneSmallSmoke1 + cmd.val;
            }
            Point pos = LoFile::GetOverlay(image_id, Overlay::Special).GetValues(this, cmd.val).ToPoint16();
            InitializeImageFull(smoke_img, img, pos.x + x_off, pos.y + y_off, parent);
        }
        break;
        case WarpOverlay:
            flags |= 0x1;
            drawfunc_param = (void *)cmd.val;
        break;
        case GrdSprOl:
        {
            int x = parent->position.x + x_off + cmd.point.x;
            int y = parent->position.y + y_off + cmd.point.y;
            // Yes, it checks if unit id 0 can fit there
            if (DoesFitHere(Unit::Marine, x, y))
                Sprite::Spawn(this, cmd.val, cmd.point, parent->elevation + 1);
        }
        break;
        default:
            return false;
    }
    return true;
}

Iscript::Command Iscript::ProgressUntilCommand(const IscriptContext *ctx, Rng *rng)
{
    using namespace IscriptOpcode;
    Command cmd = Decode(ctx->iscript + pos);
    pos += cmd.Size();
    switch (cmd.opcode)
    {
        case Goto:
            pos = cmd.pos;
        break;
        case PwrupCondJmp:
            if (ctx->img->parent && ctx->img->parent->main_image != ctx->img)
                pos = cmd.pos;
        break;
        case LiftoffCondJmp:
            if (ctx->unit && ctx->unit->IsFlying())
                pos = cmd.pos;
        break;
        case TrgtRangeCondJmp:
        {
            Unit *entity = ctx->unit != nullptr ? (Unit *)ctx->unit : (Unit *)ctx->bullet;
            if (entity == nullptr)
            {
                Warning("Iscript for image %x uses trgtrangecondjmp without parent object", ctx->img->image_id);
            }
            // Bw checks also for !ctx->constant but why should it?
            else if (entity->target)
            {
                uint32_t x, y;
                GetClosestPointOfTarget(entity, &x, &y);
                if (IsPointInArea(entity, cmd.val, x, y))
                    pos = cmd.pos;
            }
        }
        break;
        case TrgtArcCondJmp:
        {
            Unit *entity = ctx->unit != nullptr ? (Unit *)ctx->unit : (Unit *)ctx->bullet;
            if (entity == nullptr)
            {
                Warning("Iscript for image %x uses trgtarccondjmp without parent object", ctx->img->image_id);
            }
            else
            {
                // Here would also be if (!ctx->constant)
                const Point *own = &entity->sprite->position;
                const Unit *target = entity->target;
                if (target)
                {
                    int dir = GetFacingDirection(own->x, own->y, target->sprite->position.x,
                                                 target->sprite->position.y);
                    if (abs(dir - cmd.val1()) < cmd.val2())
                        pos = cmd.pos;
                }
            }
        }
        break;
        case CurDirectCondJmp:
        {
            Flingy *entity = ctx->unit != nullptr ? (Flingy *)ctx->unit : (Flingy *)ctx->bullet;
            if (entity == nullptr)
            {
                Warning("Iscript for image %x uses curdirectcondjmp without parent object", ctx->img->image_id);
            }
            else
            {
                if (abs(ctx->unit->facing_direction - cmd.val1()) < cmd.val2())
                    pos = cmd.pos;
            }
        }
        break;
        case Call:
            return_pos = pos;
            pos = cmd.pos;
        break;
        case Return:
            pos = return_pos;
        break;
        case RandCondJmp:
            if (cmd.val > rng->Rand(256))
                pos = cmd.pos;
        break;
        default:
            return cmd;
    }
    return ProgressUntilCommand(ctx, rng);
}

int Iscript::Command::ParamsLength() const
{
    using namespace IscriptOpcode;
    switch (opcode)
    {
        case PlayFram: case PlayFramTile: case SetPos: case WaitRand:
        case Goto: case ImgOlOrig: case SwitchUl: case UflUnstable:
        case SetFlSpeed: case PwrupCondJmp: case ImgUlNextId: case LiftoffCondJmp:
        case PlaySnd: case Call: case WarpOverlay:
            return 2;
        case SetHorPos: case SetVertPos: case Wait: case SetFlipState:
        case TurnCcWise: case TurnCWise: case TurnRand: case SetSpawnFrame:
        case SigOrder: case AttackWith: case UseWeapon: case Move:
        case AttkShiftProj: case SetFlDirect: case CreateGasOverlays: case OrderDone:
        case EngFrame: case EngSet:
            return 1;
        case ImgOl: case ImgUl: case ImgUlUseLo: case ImgOlUseLo: case SprOl:
        case SprUl: case LowSprUl: case HighSprOl: case SprUlUseLo:
        case PlaySndBtwn: case TrgtRangeCondJmp: case GrdSprOl:
            return 4;
        case TrgtArcCondJmp: case CurDirectCondJmp:
            return 6;
        case UnusedC: case End: case DoMissileDmg: case FollowMainGraphic:
        case Turn1CWise: case Attack: case CastSpell: case GotoRepeatAttk:
        case HideCursorMarker: case NoBrkCodeStart: case NoBrkCodeEnd: case IgnoreRest:
        case TmpRmGraphicStart: case TmpRmGraphicEnd: case Return: case Unused3e:
        case Unused43: case DoGrdDamage:
            return 0;
        case RandCondJmp: case SprOlUseLo:
            return 3;
        case PlaySndRand: case AttackMelee:
            return 1 + 2 * data[0];
        default:
            return 0;
    }
}

Iscript::Command Iscript::Decode(const uint8_t *data) const
{
    Command cmd(data[0]);
    using namespace IscriptOpcode;
    switch (cmd.opcode)
    {
        case WaitRand:
            cmd.vals[0] = data[1];
            cmd.vals[1] = data[2];
        break;
        case SetPos: case ImgUlNextId:
            cmd.point.x = *(int8_t *)(data + 1);
            cmd.point.y = *(int8_t *)(data + 2);
        break;
        case PlayFram: case PlayFramTile: case ImgOlOrig: case SwitchUl:
        case UflUnstable: case PlaySnd: case SetFlSpeed: case WarpOverlay:
            cmd.val = *(int16_t *)(data + 1);
        break;
        case EngFrame: case EngSet:
            cmd.val = *(uint8_t *)(data + 1);
        break;
        case PwrupCondJmp:  case LiftoffCondJmp: case Call: case Goto:
            cmd.pos = *(uint16_t *)(data + 1);
        break;
        case SetHorPos:
            cmd.point = Point(*(int8_t *)(data + 1), 0);
        break;
        case SetVertPos:
            cmd.point = Point(0, *(int8_t *)(data + 1));
        break;
        case Wait: case SetFlipState:
        case TurnCcWise: case TurnCWise: case TurnRand: case SetSpawnFrame:
        case SigOrder: case AttackWith: case UseWeapon: case Move:
        case AttkShiftProj: case SetFlDirect: case OrderDone:
            cmd.val = data[1];
        break;
        case CreateGasOverlays:
            cmd.val = *(int8_t *)(data + 1);
        break;
        case ImgOl: case ImgUl: case ImgUlUseLo: case ImgOlUseLo: case SprOl:
        case SprUl: case LowSprUl: case HighSprOl: case SprUlUseLo:
        case SprOlUseLo: case GrdSprOl:
            cmd.val = *(uint16_t *)(data + 1);
            cmd.point.x = *(int8_t *)(data + 3);
            cmd.point.y = *(int8_t *)(data + 4);
        break;
        case PlaySndBtwn:
            cmd.vals[0] = *(uint16_t *)(data + 1);
            cmd.vals[1] = *(uint16_t *)(data + 3);
        break;
        case TrgtRangeCondJmp:
            cmd.val = *(uint16_t *)(data + 1);
            cmd.pos = *(uint16_t *)(data + 3);
        break;
        case TrgtArcCondJmp: case CurDirectCondJmp:
            cmd.vals[0] = *(uint16_t *)(data + 1);
            cmd.vals[1] = *(uint16_t *)(data + 3);
            cmd.pos = *(uint16_t *)(data + 5);
        break;
        case UnusedC: case End: case DoMissileDmg: case FollowMainGraphic:
        case Turn1CWise: case Attack: case CastSpell: case GotoRepeatAttk:
        case HideCursorMarker: case NoBrkCodeStart: case NoBrkCodeEnd: case IgnoreRest:
        case TmpRmGraphicStart: case TmpRmGraphicEnd: case Return: case Unused3e:
        case Unused43:
        break;
        case DoGrdDamage:
            cmd.opcode = DoMissileDmg;
        break;
        case RandCondJmp:
            cmd.val = data[1];
            cmd.pos = *(uint16_t *)(data + 2);
        break;
        case PlaySndRand: case AttackMelee:
            cmd.data = data + 1;
        break;
        default:
        break;
    }
    return cmd;
}

bool Iscript::GetCommands_C::IgnoreRestCheck(const Iscript::Command &cmd) const
{
    return cmd.opcode == IscriptOpcode::IgnoreRest && ctx->unit->target != nullptr;
}
