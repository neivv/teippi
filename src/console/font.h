#ifndef FONT_H
#define FONT_H

#include <ft2build.h>
#include FT_FREETYPE_H
#include <vector>
#include <string>
#include <deque>

#include "../common/optional.h"

namespace Common
{

struct Character
{
    FT_ULong code;
    int left;
    int top;
    int width;
    int full_width;
    int height;
    std::vector<uint8_t> data;
};

class Font
{
    public:
        Font();
        Font(const char *filename, int height, bool monochrome);
        void Load(const char *filename, int height, bool monochrome);

        ~Font();

        Optional<const Character *> GetChar(FT_ULong code);

        int TextLength(const std::string &str);

        mutable int last_error;
        bool monochrome;

    private:
        void Construct();

        static FT_Library freetype;
        static bool inited;
        std::vector<FT_Face> faces;
        std::vector<Character> character_cache;
        static const int character_cache_size = 0x100;
};

}

#endif // FONT_H

