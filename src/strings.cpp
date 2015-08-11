#include "strings.h"

const char *Tbl::GetTblString(int index) const
{
    index--;
    if (index >= entries)
        return "";

    uint16_t offset = *(&first_offset + index);
    return ((const char *)&entries) + offset;
}
