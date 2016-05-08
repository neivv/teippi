#include "building.h"

#include <algorithm>

#include "unit.h"

using std::min;
using std::max;

static int GetBuildingPlacementError(Unit *builder, int player, x32 x_tile, y32 y_tile, UnitType unit_id, bool check_vision)
{
    int width = unit_id.PlacementBox().width / 32;
    int height = unit_id.PlacementBox().height / 32;
    // If units.dat placement box < 32, bw allows to place the unit anywhere
    // However, ai code calls this function with past-limit x/y, so we need to check both
    // x_tile < width && x_tile + w_tiles <= width
    int map_width = *bw::map_width_tiles;
    int map_height = *bw::map_height_tiles;
    if (x_tile < 0 || y_tile < 0 || x_tile >= map_width || y_tile >= map_height ||
            x_tile + width > map_width || y_tile + height > map_height)
    {
        return 0x3;
    }
    if (builder && !(builder->flags & (UnitStatus::Building | UnitStatus::Air)))
    {
        if (!CanWalkHere(builder, x_tile * 32, y_tile * 32))
            return 0x7;
    }
    if (unit_id.IsGasBuilding())
        return GetGasBuildingPlacementState(x_tile, y_tile, player, check_vision);
    if (unit_id.Flags() & UnitFlags::RequiresPsi)
        return GetPsiPlacementState(x_tile, y_tile, unit_id.Raw(), player);
    return 0;
}

int UpdateBuildingPlacementState(Unit *builder, int player, x32 x_tile, y32 y_tile, UnitType unit_id,
        int placement_state_entry, bool check_vision, bool also_invisible, bool without_vision)
{
    x_tile &= 0xffff;
    y_tile &= 0xffff;
    player &= 0xff;
    int error = GetBuildingPlacementError(builder, player, x_tile, y_tile, unit_id, check_vision);
    if (error || unit_id.IsGasBuilding())
    {
        uint8_t *placement_data = &*bw::building_placement_state + placement_state_entry * 0x30;
        memset(placement_data, error, 0x30);
        return error;
    }
    int width = unit_id.PlacementBox().width / 32;
    int height = unit_id.PlacementBox().height / 32;
    unsigned int size_wh = height << 16 | width;
    int specific_state;
    if (builder && builder->Type() == UnitId::NydusCanal)
        specific_state = UpdateNydusPlacementState(player, x_tile, y_tile, size_wh, placement_state_entry, check_vision);
    else if (unit_id.Flags() & UnitFlags::RequiresCreep)
        specific_state = UpdateCreepBuildingPlacementState(player, x_tile, y_tile, size_wh, placement_state_entry, check_vision, without_vision);
    else
        specific_state = UpdateBuildingPlacementState_MapTileFlags(x_tile, y_tile, player, size_wh, placement_state_entry, check_vision, unit_id.Raw());

    if (*bw::ai_building_placement_hack)
        builder = *bw::ai_building_placement_hack;
    int generic_state = UpdateBuildingPlacementState_Units(builder, x_tile, y_tile, player, unit_id.Raw(), size_wh, placement_state_entry, ~unit_id.Flags() & UnitFlags::Addon, also_invisible, without_vision);
    return max(generic_state, specific_state);
}
