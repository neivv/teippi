#include "yms.h"

static bool IsColorChar(char c)
{
    switch (c)
    {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 14:
        case 15:
        case 16:
        case 17:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
            return true;
        default:
            return false;
    }
}
static bool IsControlChar(char c)
{
    switch (c)
    {
        case 0x9:
        case 0xa:
        case 0xb:
        case 0xc:
        case 0xd:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x1a:
            return true;
        default:
            return false;
    }
}

int Sc_strlen(const char *str, int maxlen1, int maxlen2, bool accept_color_chars, bool accept_control_chars)
{
    int a = maxlen1 < maxlen2 ? maxlen1 : maxlen2;
    int len = 0;
    while (a-- > 0)
    {
        len++;
        if (*str == 0)
            return len;
        if (!accept_control_chars && IsColorChar(*str))
            return 0;
        if (!accept_control_chars && IsControlChar(*str))
            return 0;
        str++;
    }
    return 0;
}
