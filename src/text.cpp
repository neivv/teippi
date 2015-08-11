#include "text.h"

#include <stdarg.h>
#include <stdio.h>

#include "offsets.h"

void Print(const char *format, ...)
{
    va_list varg;
    va_start(varg, format);
    char buf[256];
    vsnprintf(buf, 256, format, varg);
    va_end(varg);

    PrintText(buf, 0, 8);
}
