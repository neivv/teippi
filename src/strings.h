#ifndef STRINGS_H
#define STRINGS_H

#include "types.h"
#include "constants/string.h"

#pragma pack(push)
#pragma pack(1)

class Tbl
{
    public:
        const char *GetTblString(int index) const;

    private:
        uint16_t entries;
        uint16_t first_offset;
};

#pragma pack(pop)

#endif // STRINGS_H

