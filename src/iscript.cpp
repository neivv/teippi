#include "iscript.h"

#include <string>

namespace Iscript {

std::string Command::DebugStr() const
{
    using namespace Opcode;
    switch (opcode)
    {
        case PlayFram:
            return "playfram";
        case PlayFramTile:
            return "playframtile";
        case SetHorPos:
            return "sethorpos";
        case SetVertPos:
            return "setvertpos";
        case SetPos:
            return "setpos";
        case Wait:
            return "wait";
        case WaitRand:
            return "waitrand";
        case Goto:
            return "goto";
        case ImgOl:
            return "imgol";
        case ImgUl:
            return "imgul";
        case ImgOlOrig:
            return "imgolorig";
        case SwitchUl:
            return "switchul";
        case UnusedC:
            return "unusedc";
        case ImgOlUseLo:
            return "imgoluselo";
        case ImgUlUseLo:
            return "imguluselo";
        case SprOl:
            return "sprol";
        case HighSprOl:
            return "highsprol";
        case LowSprUl:
            return "lowsprul";
        case UflUnstable:
            return "uflunstable";
        case SprUlUseLo:
            return "spruluselo";
        case SprUl:
            return "sprul";
        case SprOlUseLo:
            return "sproluselo";
        case End:
            return "end";
        case SetFlipState:
            return "setflipstate";
        case PlaySnd:
            return "playsnd";
        case PlaySndRand:
            return "playsndrand";
        case PlaySndBtwn:
            return "playsndbtwn";
        case DoMissileDmg:
            return "domissiledmg";
        case AttackMelee:
            return "attackmelee";
        case FollowMainGraphic:
            return "followmaingraphic";
        case RandCondJmp:
            return "randcondjmp";
        case TurnCcWise:
            return "turnccwise";
        case TurnCWise:
            return "turncwise";
        case Turn1CWise:
            return "turn1cwise";
        case TurnRand:
            return "turnrand";
        case SetSpawnFrame:
            return "setspawnframe";
        case SigOrder:
            return "sigorder";
        case AttackWith:
            return "attackwith";
        case Attack:
            return "attack";
        case CastSpell:
            return "castspell";
        case UseWeapon:
            return "useweapon";
        case Move:
            return "move";
        case GotoRepeatAttk:
            return "gotorepeatattk";
        case EngFrame:
            return "engframe";
        case EngSet:
            return "engset";
        case HideCursorMarker:
            return "hidecursormarker";
        case NoBrkCodeStart:
            return "nobrkcodestart";
        case NoBrkCodeEnd:
            return "nobrkcodeend";
        case IgnoreRest:
            return "ignorerest";
        case AttkShiftProj:
            return "attkshiftproj";
        case TmpRmGraphicStart:
            return "tmprmgraphicstart";
        case TmpRmGraphicEnd:
            return "tmprmgraphicend";
        case SetFlDirect:
            return "setfldirect";
        case Call:
            return "call";
        case Return:
            return "return";
        case SetFlSpeed:
            return "setflspeed";
        case CreateGasOverlays:
            return "creategasoverlays";
        case PwrupCondJmp:
            return "pwrupcondjmp";
        case TrgtRangeCondJmp:
            return "trgtrangecondjmp";
        case TrgtArcCondJmp:
            return "trgtarccondjmp";
        case CurDirectCondJmp:
            return "curdirectcondjmp";
        case ImgUlNextId:
            return "imgulnextid";
        case Unused3e:
            return "unused3e";
        case LiftoffCondJmp:
            return "liftoffcondjmp";
        case WarpOverlay:
            return "warpoverlay";
        case OrderDone:
            return "orderdone";
        case GrdSprOl:
            return "grdsprol";
        case Unused43:
            return "unused43";
        case DoGrdDamage:
            return "dogrddamage";
        default:
        {
            char buf[32];
            snprintf(buf, sizeof buf, "Unknown (%x)", opcode);
            return buf;
        }
    }
}

const char *Script::AnimationName(int anim)
{
    using namespace Iscript::Animation;
    switch (anim)
    {
        case Init: return "Init";
        case Death: return "Death";
        case GndAttkInit: return "GndAttkInit";
        case AirAttkInit: return "AirAttkInit";
        case GndAttkRpt: return "GndAttkRpt";
        case AirAttkRpt: return "AirAttkRpt";
        case CastSpell: return "CastSpell";
        case GndAttkToIdle: return "GndAttkToIdle";
        case AirAttkToIdle: return "AirAttkToIdle";
        case Walking: return "Walking";
        case Idle: return "Idle";
        case Special1: return "Special1";
        case Special2: return "Special2";
        case AlmostBuilt: return "AlmostBuilt";
        case Built: return "Built";
        case Landing: return "Landing";
        case Liftoff: return "Liftoff";
        case Working: return "Working";
        case WorkingToIdle: return "WorkingToIdle";
        case WarpIn: return "WarpIn";
        case Disable: return "Disable";
        case Burrow: return "Burrow";
        case Unburrow: return "Unburrow";
        case Enable: return "Enable";
        default: return "Unknown";
    }
}

int Command::ParamsLength() const
{
    using namespace Opcode;
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

Command Script::Decode(const uint8_t *data) const
{
    Command cmd(data[0]);
    using namespace Opcode;
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

void Script::ProgressFrame(Context *ctx, Image *img)
{
    if (wait-- != 0)
        return;

    while (true)
    {
        const Command cmd = Decode(ctx->iscript + pos);
        pos += cmd.Size();
        CmdResult result = ctx->HandleCommand(img, this, cmd);
        if (result == CmdResult::Stop)
            return;
    }
}

bool Script::Initialize(const uint8_t *iscript, int iscript_header_id)
{
    uint32_t header_offset = *(const uint32_t *)iscript;
    const OffsetSize *headers = (const OffsetSize *)(iscript + header_offset);
    while (headers[0] != 0xffff)
    {
        if (headers[0] == iscript_header_id)
        {
            header = headers[1];
            return true;
        }
        headers += 2;
    }
    return false;
}

} // namespace Iscript
