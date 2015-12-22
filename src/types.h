#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include "common/iter.h"

#include "console/types.h"

typedef Common::Point16 Point;
using Common::Point32;
using Common::Rect16;
using Common::Rect32;
using Common::Rect;
using Common::x16u;
using Common::x32;
using Common::y16u;
using Common::y32;
using xuint = Common::xint<unsigned int>;
using yuint = Common::yint<unsigned int>;
using Common::Array;

using Common::reference;
using Common::ptr;
using Common::vector;
using Common::Iterator;
using Common::Optional;
using Common::Empty;
using std::tuple;
using std::make_tuple;
using std::move;
using std::make_unique;
using std::ref;
using std::cref;

#ifdef CONSOLE
const bool UseConsole = true;
#else
const bool UseConsole = false;
#endif
#ifdef PERFORMANCE_DEBUG
const bool PerfTest = true;
#else
const bool PerfTest = false;
#endif
#ifdef SYNC
const bool SyncTest = true;
#else
const bool SyncTest = false;
#endif
#ifdef DEBUG
const bool Debug = true;
#else
const bool Debug = false;
#endif
#ifdef STATIC_SEED
const uint32_t StaticSeed = STATIC_SEED;
#else
const uint32_t StaticSeed = 0;
#endif

const unsigned int NeutralPlayer = 0xb;
const unsigned int AllPlayers = 0x11;

class Unit;
class Sprite;
class LoneSpriteSystem;
class Image;
class Bullet;
class BulletSystem;
class DamagedUnit;
class Flingy;
class Entity;
class Tbl;
class Path;
class Order;
class Control;
class Dialog;
class TempMemoryPool;
class Rng;
class Save;
class Load;
class GrpFrameHeader;
class GameTests;

struct Contour;
struct PathingData;
struct Surface;
struct ReplayData;
struct MovementGroup;
struct NetPlayer;
struct Player;
struct ImgRenderFuncs;
struct ImgUpdateFunc;
struct BlendPalette;
struct Surface;
struct DrawLayer;
struct CollisionArea;
struct Trigger;
struct TriggerList;
struct TriggerAction;
struct ReplayData;
struct GameData;
struct File;
struct PlacementBox;
struct GrpSprite;
struct ReplayHeader;
struct Location;
struct MapDirEntry;
struct MapDl;
struct Event;
struct CycleStruct;

template <class Type, unsigned size = 15> class UnitList;
template <class C, unsigned offset> class RevListHead;
template <class C, unsigned offset> class ListHead;

namespace Common
{
    class PatchContext;
}

namespace Ai
{
    struct PlayerData;
    struct Region;
    class Script;
    class Town;
    class UnitAi;
    class GuardAi;
    class WorkerAi;
    class BuildingAi;
    class MilitaryAi;
    class HitUnit;
    template <class C> class DataList;
    struct ResourceArea;
    struct ResourceAreaArray;
    class BestPickedTarget;
}

namespace Pathing
{
    struct Region;
    struct PathingSystem;
}

namespace Iscript
{
    class Script;
}
class UnitIscriptContext;
class BulletIscriptContext;

#endif // TYPES_H

