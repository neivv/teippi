#ifndef BUILDING_H
#define BUILDING_H

#include "types.h"

int UpdateBuildingPlacementState(Unit *builder, int player, x32 x_tile, y32 y_tile, int unit_id,
        int placement_state_entry, bool check_vision, bool also_invisible, bool without_vision);
#endif /* BUILDING_H */
