#ifndef SYNC_H
#define SYNC_H

#include "types.h"
#include "unit.h"
#include "bullet.h"
#include "pathing.h"
#include "sprite.h"
#include "ai.h"

#include <algorithm>

/// Convenience class for writing sync dumps
/// Writes spaces between data unless there was a newline
class SyncDumper
{
    public:
        SyncDumper(DebugLog_Actual *log_) : log(log_), prev_was_newline(true) {}
        DebugLog_Actual *log;
        bool prev_was_newline;

        template <typename... Args>
        void operator()(const char *format, Args... args)
        {
            if (!prev_was_newline || format[0] != '\n')
            {
                log->Log(format, args...);
                if (format[0] != '\n')
                    log->Log(" ");
                prev_was_newline = format[0] == '\n';
            }
        }
};

struct SyncHashes
{
    SyncHashes(uint32_t main, uint32_t units, uint32_t bullets, uint32_t unit_s, uint32_t bullet_s, uint32_t paths, uint32_t ai_region, uint32_t ai, uint32_t trig) :
        main_hash(main), units_hash(units), bullets_hash(bullets), unit_sprites_hash(unit_s),
        bullet_sprites_hash(bullet_s), paths_hash(paths), ai_region_hash(ai_region), ai_hash(ai),
        trigger_hash(trig) {}
    SyncHashes(SyncHashes &&other) = default;
    SyncHashes &operator=(SyncHashes &&other) = default;

    uint32_t main_hash;
    uint32_t units_hash;
    uint32_t bullets_hash;
    uint32_t unit_sprites_hash;
    uint32_t bullet_sprites_hash;
    uint32_t paths_hash;
    uint32_t ai_region_hash;
    uint32_t ai_hash;
    uint32_t trigger_hash;
};

template <class W>
inline void WriteValDiff(const Point &newer, const Point &older, W &Write, const char *desc)
{
    if (newer != older)
        Write("%s %04hx.%04hx", desc, newer.x, newer.y);
}

template <class W>
inline void WriteValDiff(uint32_t newer, uint32_t older, W &Write, const char *desc)
{
    if (newer != older)
        Write("%s %08x", desc, newer);
}

template <class W>
inline void WriteValDiff(uint16_t newer, uint16_t older, W &Write, const char *desc)
{
    if (newer != older)
        Write("%s %04x", desc, newer);
}

template <class W>
inline void WriteValDiff(uint8_t newer, uint8_t older, W &Write, const char *desc)
{
    if (newer != older)
        Write("%s %02x", desc, newer);
}

template <class W>
inline void WritePtrDiff(const Unit *newer, const Unit *older, W &Write, const char *desc)
{
    if (newer != older)
    {
        if (newer)
            Write("%s %08X", desc, newer->lookup_id);
        else
            Write("%s 0", desc);
    }
}

template <class W>
inline void WriteVal(const Point &newer, W &Write, const char *desc)
{
    Write("%s %04hx.%04hx", desc, newer.x, newer.y);
}

template <class W>
inline void WriteVal(uint32_t newer, W &Write, const char *desc)
{
    Write("%s %08x", desc, newer);
}

template <class W>
inline void WriteVal(uint16_t newer, W &Write, const char *desc)
{
    Write("%s %04x", desc, newer);
}

template <class W>
inline void WriteVal(uint8_t newer, W &Write, const char *desc)
{
    Write("%s %02x", desc, newer);
}

template <class W>
inline void WritePtrVal(const Unit *newer, W &Write, const char *desc)
{
    if (newer)
        Write("%s %08X", desc, newer->lookup_id);
}

struct SyncPath
{
    SyncPath(const Path *in) : start(in->start), next(in->next_pos), end(in->end) {}
    Point start;
    Point next;
    Point end;

    bool operator==(const SyncPath &o) const
    {
        return start == o.start && next == o.next && end == o.end;
    }

    template <class W>
    void WriteAll(W &Write) const
    {
        WriteVal(start, Write, "_s");
        WriteVal(next, Write, "_n");
        WriteVal(end, Write, "_e");
    }

    template <class W>
    void WriteDiff(const SyncPath &previous, W &Write) const
    {
        WriteValDiff(start, previous.start, Write, "_s");
        WriteValDiff(next, previous.next, Write, "_n");
        WriteValDiff(end, previous.end, Write, "_e");
    }
};

struct SyncSprite
{
    SyncSprite(const Sprite *in) : position(in->position), sprite_id(in->sprite_id), player(in->player),
        visibility_mask(in->visibility_mask), elevation(in->elevation), width(in->width), height(in->height) {}
    Point position;
    uint16_t sprite_id;
    uint8_t player;
    uint8_t visibility_mask;
    uint8_t elevation;
    uint8_t width;
    uint8_t height;

    bool operator==(const SyncSprite &o) const
    {
        return position == o.position && sprite_id == o.sprite_id && player == o.player && visibility_mask == o.visibility_mask &&
            elevation == o.elevation && width == o.width && height == o.height;
    }

    template <class W>
    void WriteAll(W &Write) const
    {
        WriteVal(position, Write, "p");
        WriteVal(sprite_id, Write, "$id");
        WriteVal(player, Write, "$p");
        WriteVal(visibility_mask, Write, "$v");
        WriteVal(elevation, Write, "$e");
        WriteVal(width, Write, "$w");
        WriteVal(height, Write, "$h");
    }

    template <class W>
    void WriteDiff(const SyncSprite &previous, W &Write) const
    {
        WriteValDiff(position, previous.position, Write, "p");
        WriteValDiff(sprite_id, previous.sprite_id, Write, "$id");
        WriteValDiff(player, previous.player, Write, "$p");
        WriteValDiff(visibility_mask, previous.visibility_mask, Write, "$v");
        WriteValDiff(elevation, previous.elevation, Write, "$e");
        WriteValDiff(width, previous.width, Write, "$w");
        WriteValDiff(height, previous.height, Write, "$h");
    }
};

struct SyncAiData
{
    struct Request
    {
        Request(const Ai::SpendingRequest &in) : priority(in.priority), type(in.type), unit_id(in.unit_id),
            val(in.val) {}
        bool operator==(const Request &o) const
        {
            return priority == o.priority && type == o.type && unit_id == o.unit_id && val == o.val;
        }
        uint8_t priority;
        uint8_t type;
        uint16_t unit_id;
        void *val;
    };
    SyncAiData(int pl) : player(pl)
    {
        requests.reserve(bw::player_ai[player].request_count);
        for (int i = 0; i < bw::player_ai[player].request_count; i++)
        {
            requests.emplace_back(bw::player_ai[player].requests[i]);
        }
    }

    bool operator==(const SyncAiData &o) const
    {
        return requests == o.requests;
    }
    template <class W>
    void WriteDiff(const SyncAiData &previous, W &Write) const
    {
        if (previous == *this)
            return;
        Write("AD%02X", player);
        for (const auto &req : requests)
        {
            WriteVal(req.priority, Write, "p");
            WriteVal(req.type, Write, "t");
            WriteVal(req.unit_id, Write, "u");
            Write("v %p", req.val);
        }
    }
    vector<Request> requests;
    int player;
};

struct SyncAiRegion
{
    SyncAiRegion(const Ai::Region *in) : target_region_id(in->target_region_id), state(in->state),
    flags(in->flags), ground_unit_count(in->ground_unit_count), needed_ground(in->needed_ground_strength),
    needed_air(in->needed_air_strength), enemy_air_strength(in->enemy_air_strength),
    enemy_ground_strength(in->enemy_ground_strength), player(in->player), region_id(in->region_id) {}
    uint16_t target_region_id;
    uint8_t state;
    uint16_t flags; // 0x8
    uint16_t ground_unit_count;
    uint16_t needed_ground;
    uint16_t needed_air;
    uint16_t enemy_air_strength;
    uint16_t enemy_ground_strength;
    uint8_t player;
    uint16_t region_id;

    bool operator==(const SyncAiRegion &o) const
    {
        return target_region_id == o.target_region_id && state == o.state && flags == o.flags &&
            ground_unit_count == o.ground_unit_count && needed_ground == o.needed_ground &&
            needed_air == o.needed_air && enemy_air_strength == o.enemy_air_strength &&
            enemy_ground_strength == o.enemy_ground_strength && player == o.player && region_id == o.region_id;
    }

    template <class W>
    void WriteDiff(const SyncAiRegion &previous, W &Write) const
    {
        if (previous == *this)
            return;
        Write("AR%02X:%04X", player, region_id);
        WriteValDiff(target_region_id, previous.target_region_id, Write, "t");
        WriteValDiff(state, previous.state, Write, "s");
        WriteValDiff(flags, previous.flags, Write, "f");
        WriteValDiff(ground_unit_count, previous.ground_unit_count, Write, "g");
        WriteValDiff(needed_ground, previous.needed_ground, Write, "c");
        WriteValDiff(needed_air, previous.needed_air, Write, "e");
        WriteValDiff(enemy_air_strength, previous.enemy_air_strength, Write, "ea");
        WriteValDiff(enemy_ground_strength, previous.enemy_ground_strength, Write, "eg");
        WriteValDiff(player, previous.player, Write, "p");
        WriteValDiff(region_id, previous.region_id, Write, "r");
    }
};

struct SyncBullet
{
    SyncBullet(const Bullet *in) : sprite(in->sprite.get()), ptr(in), parent(in->parent), weapon_id(in->weapon_id) {}
    SyncSprite sprite;
    const Bullet *ptr;
    const Unit *parent;
    uint8_t weapon_id;

    bool operator==(const SyncBullet &o) const
    {
        return sprite == o.sprite && ptr == o.ptr && parent == o.parent && weapon_id == o.weapon_id;
    }

    template <class W>
    void WriteDiff(const SyncBullet &previous, W &Write) const
    {
        if (previous == *this)
            return;
        Write("B%08X", ptr);
        sprite.WriteDiff(previous.sprite, Write);
        WritePtrDiff(parent, previous.parent, Write, "P");
        WriteValDiff(weapon_id, previous.weapon_id, Write, "w");
    }

    template <class W>
    void WriteAll(W &Write) const
    {
        Write("B%08X", ptr);
        sprite.WriteAll(Write);
        WritePtrVal(parent, Write, "P");
        WriteVal(weapon_id, Write, "w");
    }
};

struct SyncUnit
{
    SyncUnit(const Unit *in) : sprite(in->sprite), ptr(in), target(in->target), subunit(in->subunit),
        previous_attacker(in->previous_attacker), lookup_id(in->lookup_id), hp(in->hitpoints), shields(in->shields),
        current_speed(in->current_speed), next_speed(in->next_speed), flags(in->flags), move_target(in->move_target),
        order_target_pos(in->order_target_pos), energy(in->energy), invisibility_effects(in->invisibility_effects),
        facing_direction(in->facing_direction), movement_direction(in->movement_direction), target_direction(in->target_direction),
        order(in->order), secondary_order(in->secondary_order), movement_state(in->movement_state), flingy_flags(in->flingy_flags),
        move_target_update_timer(in->move_target_update_timer), ground_strength(in->ground_strength),
        air_strength(in->air_strength)
    {
        if (in->path)
            path = SyncPath(in->path);
    }
    SyncSprite sprite;
    Optional<SyncPath> path;
    const Unit *ptr;
    const Unit *target;
    const Unit *subunit;
    const Unit *previous_attacker;
    uint32_t lookup_id;
    int32_t hp;
    uint32_t shields;
    uint32_t current_speed;
    uint32_t next_speed;
    uint32_t flags;
    Point move_target;
    Point order_target_pos;
    uint16_t energy;
    uint8_t invisibility_effects;
    uint8_t facing_direction;
    uint8_t movement_direction;
    uint8_t target_direction;
    uint8_t order;
    uint8_t secondary_order;
    uint8_t movement_state;
    uint8_t flingy_flags;
    uint8_t move_target_update_timer;
    uint16_t ground_strength;
    uint16_t air_strength;

    bool operator==(const SyncUnit &o) const
    {
        return sprite == o.sprite && path == o.path && target == o.target && subunit == o.subunit &&
            previous_attacker == o.previous_attacker && lookup_id == o.lookup_id && hp == o.hp && shields == o.shields &&
            current_speed == o.current_speed && next_speed == o.next_speed && flags == o.flags && move_target == o.move_target &&
            energy == o.energy && invisibility_effects == o.invisibility_effects && facing_direction == o.facing_direction &&
            movement_direction == o.movement_direction && target_direction == o.target_direction && order == o.order &&
            secondary_order == o.secondary_order && movement_state == o.movement_state && flingy_flags == o.flingy_flags &&
            move_target_update_timer == o.move_target_update_timer && ground_strength == o.ground_strength &&
            air_strength == o.air_strength && order_target_pos == o.order_target_pos;
    }

    template <class W>
    void WriteDiff(const SyncUnit &previous, W &Write) const
    {
        if (previous == *this)
            return;
        Write("U%08X", lookup_id);
        sprite.WriteDiff(previous.sprite, Write);
        if (path)
        {
            if (previous.path)
                path.take().WriteDiff(previous.path.take(), Write);
            else
                path.take().WriteAll(Write);
        }
        else if (previous.path)
            Write("Dp");
        WritePtrDiff(target, previous.target, Write, "T");
        WritePtrDiff(subunit, previous.subunit, Write, "S");
        WritePtrDiff(previous_attacker, previous.previous_attacker, Write, "PA");
        WriteValDiff((uint32_t)hp, previous.hp, Write, "h");
        WriteValDiff(shields, previous.shields, Write, "s");
        WriteValDiff(current_speed, previous.current_speed, Write, "cs");
        WriteValDiff(next_speed, previous.next_speed, Write, "ns");
        WriteValDiff(flags, previous.flags, Write, "f");
        WriteValDiff(move_target, previous.move_target, Write, "mt");
        WriteValDiff(order_target_pos, previous.order_target_pos, Write, "ot");
        WriteValDiff(energy, previous.energy, Write, "e");
        WriteValDiff(invisibility_effects, previous.invisibility_effects, Write, "ie");
        WriteValDiff(facing_direction, previous.facing_direction, Write, "fd");
        WriteValDiff(movement_direction, previous.movement_direction, Write, "md");
        WriteValDiff(target_direction, previous.target_direction, Write, "td");
        WriteValDiff(order, previous.order, Write, "o");
        WriteValDiff(secondary_order, previous.secondary_order, Write, "o2");
        WriteValDiff(movement_state, previous.movement_state, Write, "ms");
        WriteValDiff(flingy_flags, previous.flingy_flags, Write, "ff");
        WriteValDiff(move_target_update_timer, previous.move_target_update_timer, Write, "mtt");
        WriteValDiff(ground_strength, previous.ground_strength, Write, "gs");
        WriteValDiff(air_strength, previous.air_strength, Write, "as");
    }

    template <class W>
    void WriteAll(W &Write) const
    {
        Write("U%08X", lookup_id);
        sprite.WriteAll(Write);
        if (path)
            path.take().WriteAll(Write);
        WritePtrVal(target, Write, "T");
        WritePtrVal(subunit, Write, "S");
        WritePtrVal(previous_attacker, Write, "PA");
        WriteVal((uint32_t)hp, Write, "h");
        WriteVal(shields, Write, "s");
        WriteVal(current_speed, Write, "cs");
        WriteVal(next_speed, Write, "ns");
        WriteVal(flags, Write, "f");
        WriteVal(move_target, Write, "mt");
        WriteVal(order_target_pos, Write, "ot");
        WriteVal(energy, Write, "e");
        WriteVal(invisibility_effects, Write, "ie");
        WriteVal(facing_direction, Write, "fd");
        WriteVal(movement_direction, Write, "md");
        WriteVal(target_direction, Write, "td");
        WriteVal(order, Write, "o");
        WriteVal(secondary_order, Write, "o2");
        WriteVal(movement_state, Write, "ms");
        WriteVal(flingy_flags, Write, "ff");
        WriteVal(move_target_update_timer, Write, "mtt");
        WriteVal(ground_strength, Write, "gs");
        WriteVal(air_strength, Write, "as");
    }
};

class SyncData
{
    public:
        SyncData()
        {
            units.reserve(1000);
            unit_allocated_order.reserve(1000);
            ai_regions.reserve(1000);
            for (Unit *unit : first_allocated_unit)
            {
                if (unit->sprite)
                    units.emplace_back(unit);
                unit_allocated_order.emplace_back(unit->lookup_id);
            }
            std::sort(units.begin(), units.end(), [](const auto &a, const auto &b) { return a.lookup_id < b.lookup_id; });
            bullets.reserve(bullet_system->BulletCount());
            for (Bullet *bullet : bullet_system->ActiveBullets())
            {
                bullets.emplace_back(bullet);
            }
            std::sort(bullets.begin(), bullets.end(),
                    [](const auto &a, const auto &b) { return (a.ptr) /* The magic parentheses */ < b.ptr; });
            for (Ai::Region *region : Ai::GetRegions())
            {
                ai_regions.emplace_back(region);
            }
            std::sort(ai_regions.begin(), ai_regions.end(), [](const auto &a, const auto &b)
            {
                if (a.player == b.player)
                    return a.region_id < b.region_id;
                return a.player < b.player;
            });
            for (int i = 0; i < Limits::ActivePlayers; i++)
            {
                if (IsComputerPlayer(i))
                {
                    player_ai.emplace_back(i);
                }
            }
        }
        SyncData(bool this_is_empty_constructor)
        {
        }
        SyncData(SyncData &&other) = default;
        SyncData &operator=(SyncData &&other) = default;

        template <class W>
        void WriteDiff(const SyncData &previous, W Write) const
        {
            {
                auto pos = units.begin();
                for (auto &old_unit : previous.units)
                {
                    while (pos != units.end() && pos->lookup_id < old_unit.lookup_id)
                        ++pos;

                    auto &new_unit = *pos;
                    if (pos == units.end() || new_unit.lookup_id != old_unit.lookup_id)
                        Write("Du %08X", old_unit.lookup_id);
                    else
                    {
                        new_unit.WriteDiff(old_unit, Write);
                        ++pos;
                    }
                    Write("\n");
                }
                while (pos != units.end())
                {
                    Write("Nu %p ", pos->ptr);
                    pos->WriteAll(Write);
                    Write("\n");
                    ++pos;
                }
                int count = 0;
                auto new_order_pos = unit_allocated_order.begin();
                for (uint32_t lookup_id : previous.unit_allocated_order)
                {
                    if (new_order_pos == unit_allocated_order.end())
                        break;
                    if (*new_order_pos != lookup_id)
                    {
                        Write("-%08X ", lookup_id);
                        if (++count == 10)
                        {
                            count = 0;
                            Write("\n");
                        }
                    }
                    else
                        ++new_order_pos;
                }
                while (new_order_pos != unit_allocated_order.end())
                {
                    Write("+%08X ", *new_order_pos);
                    ++new_order_pos;
                    if (++count == 10)
                    {
                        count = 0;
                        Write("\n");
                    }
                }
                if (count != 0)
                    Write("\n");
            }
            {
                auto pos = bullets.begin();
                for (auto &old_bullet : previous.bullets)
                {
                    while (pos != bullets.end() && pos->ptr < old_bullet.ptr)
                        ++pos;

                    if (pos == bullets.end() || pos->ptr != old_bullet.ptr)
                        Write("Db %p", old_bullet.ptr);
                    else
                    {
                        pos->WriteDiff(old_bullet, Write);
                        ++pos;
                    }
                    Write("\n");
                }
                for (const auto &bullet : bullets)
                {
                    if (!std::binary_search(previous.bullets.begin(), previous.bullets.end(), bullet,
                                [](const SyncBullet &a, const SyncBullet &b) { return a.ptr < b.ptr; }))
                    {
                        Write("Nb");
                        bullet.WriteAll(Write);
                        Write("\n");
                        ++pos;
                    }
                }
            }
            {
                auto pos = ai_regions.begin();
                for (const auto &old_region : previous.ai_regions)
                {
                    if (pos == ai_regions.end())
                        break;
                    if (old_region.player != pos->player || old_region.region_id != pos->region_id)
                        continue;
                    pos->WriteDiff(old_region, Write);
                    ++pos;
                    Write("\n");
                }
            }
            {
                auto pos = player_ai.begin();
                for (const auto &old : previous.player_ai)
                {
                    if (pos == player_ai.end())
                        break;
                    if (pos->player != old.player)
                        continue;
                    pos->WriteDiff(old, Write);
                    ++pos;
                    Write("\n");
                }
            }
        }

    private:
        vector<SyncUnit> units;
        vector<uint32_t> unit_allocated_order;
        vector<SyncBullet> bullets;
        vector<SyncAiRegion> ai_regions;
        vector<SyncAiData> player_ai;
};

#endif /* SYNC_H */
