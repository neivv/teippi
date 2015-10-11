#ifndef PATHING_H
#define PATHING_H

#include "types.h"

const unsigned int PATH_LIMIT = 0x400;

int GetRegion(const Point &pos);
namespace Pathing {
    inline int GetRegion(const Point &pos) { return ::GetRegion(pos); }
}

void DrawPathingInfo(uint8_t *framebuf, xuint w, yuint h);
Path *AllocatePath(uint16_t *region_count, uint16_t *position_count);
void CreateSimplePath(Unit *unit, const Point &next_pos, const Point &end);
Pathing::PathingSystem *GetPathingSystem();

extern bool draw_region_borders;
extern bool draw_region_data;
extern bool draw_paths;

#pragma pack(push)
#pragma pack(1)

class Path // 0x80
{
    public:
        Point start;
        Point next_pos;
        Point end;
        uint32_t start_frame;
        Unit *dodge_unit;
        uint32_t x_y_speed;
        uint8_t flags;
        uint8_t unk_count;
        uint8_t direction;
    	uint8_t total_region_count;
    	uint8_t unk1c;
    	uint8_t unk1d;
    	uint8_t position_count;
    	uint8_t position_index;

        uint16_t values[0x30];

#ifdef SYNC
        void *operator new(size_t size);
#endif
        Path();
        ~Path();

        template <class Archive>
        void serialize(Archive &archive);
};

static_assert(sizeof(Path) == 0x80, "sizeof(Path");

struct MovementGroup
{
    uint16_t target[2];
    uint16_t current_target[2];
    uint8_t dc8[0x18];
};

struct Contour
{
    uint16_t coords[3];
    uint8_t type;
    uint8_t info;
};

struct PathingData
{
    uint8_t dc0[0x20];
    int16_t unit_size[4];
    uint8_t dc28[0x10];
    int16_t unit_pos[2];
    Unit *self;
    int16_t unk_pos[2];
    Unit *always_dodge;
    uint8_t dc48[0x18];
    Rect16 area;
    uint8_t dc68[0x268];
    uint16_t wall_counts[4];
    Contour top_walls[0x80];
    Contour right_walls[0x80];
    Contour bottom_walls[0x80];
    Contour left_walls[0x80];
};

namespace Pathing
{
    enum e
    {
        top = 0,
        right,
        bottom,
        left
    };

    struct Region
    {
        uint16_t unk0;
        uint16_t group;
        uint8_t unk4[0x2];
        uint8_t group_neighbour_count;
        uint8_t all_neighbour_count;
        void *temp_val;
        uint16_t *neighbour_ids;
        uint32_t x; // 0x10
        uint32_t y;
        Rect16 area;
        uint8_t unk20[0x20]; // 0x20
    };

    struct SplitRegion
    {
        uint16_t minitile_flags;
        uint16_t region_false;
        uint16_t region_true;
    };

    struct ContourData
    {
        Contour *top_contours;
        Contour *right_contours;
        Contour *bottom_contours;
        Contour *left_contours;
        uint16_t top_contour_count;
        uint16_t right_contour_count;
        uint16_t bottom_contour_count;
        uint16_t left_contour_count;
        uint8_t dc18[0x20];
    };

    struct PathingSystem
    {
        uint16_t region_count;
        uint16_t unk2;
        uint16_t *unk_ids;
        SplitRegion *unk;
        uint16_t map_tile_regions[0x100 * 0x100];
        SplitRegion split_regions[25000]; // Todnäk ei nuin paljon... Siinä on jotai muutaki
        Region regions[5000];
        uint16_t many_region_neighbour_ids[10000];
        ContourData *contours;
    };
    static_assert(sizeof(PathingSystem) == 0x97a20, "Sizeof pathing");
    static_assert(sizeof(ContourData) == 0x38, "sizeof(ContourData)");
}


#pragma pack(pop)

#endif // PATHING_H

