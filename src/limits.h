#ifndef LIMIT_H
#define LIMIT_H

#include "types.h"

namespace Limits
{
    const unsigned MapWidth_Tiles = 256;
    const unsigned MapHeight_Tiles = 256;
    const unsigned ActivePlayers = 8;
    const unsigned Players = 12;
    const unsigned Selection = 12;
    const unsigned ImageTypes = 999;
    const unsigned RemapPalettes = 8;
    const unsigned Teams = 4;
}

inline bool IsActivePlayer(int player) { return player >= 0 && player < Limits::ActivePlayers; }

void RemoveLimits(Common::PatchContext *patch);

#endif // LIMIT_H

