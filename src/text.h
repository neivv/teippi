#ifndef TEXT_H
#define TEXT_H

#include "types.h"
#include "constants/sound.h"
#include "constants/string.h"

#ifdef DEBUG
#define DebugPrint Print
#else
inline void DebugPrint(const char *format, ...) {}
#endif

void Print(const char *format, ...);

#endif // TEXT_H
