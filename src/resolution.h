#ifndef RESOLUTION_H
#define RESOLUTION_H

#include "types.h"

namespace resolution
{
    constexpr xuint screen_width = 640;
    constexpr yuint screen_height = 480;
    constexpr xuint game_width = screen_width;
    constexpr yuint game_height = 400;
    constexpr yuint game_height_tiles = game_height / 32 + 1; // + 1 as 400 / 32 = 12.5
}

#endif // RESOLUTION_H

