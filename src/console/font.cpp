#include "font.h"

#include "windows_wrap.h"

namespace Common
{

FT_Library Font::freetype;
bool Font::inited;

void Font::Construct()
{
    if (!inited)
    {
        FT_Init_FreeType(&freetype);
        inited = true;
    }
    last_error = 0;
}

Font::Font()
{
    Construct();
}

Font::Font(const char *filename, int height, bool monochrome_)
{
    Construct();
    Load(filename, height, monochrome_);
}

void Font::Load(const char *filename, int height, bool monochrome_)
{
    monochrome = monochrome_;
    faces.emplace_back();
    auto &face = *faces.rbegin();
    last_error = FT_New_Face(freetype, filename, 0, &face);
    if (!last_error)
    {
        last_error = FT_Set_Pixel_Sizes(face, 0, height);
    }
}

Optional<const Character *> Font::GetChar(FT_ULong code)
{
    for (auto &chara : character_cache)
    {
        if (code == chara.code)
            return &chara;
    }
    FT_Int32 flags = FT_LOAD_RENDER;
    if (monochrome)
        flags |= FT_LOAD_MONOCHROME;

    for (auto &face : faces)
    {
        last_error = FT_Load_Char(face, code, flags);
        if (!last_error)
        {
            auto glyph = face->glyph;
            auto bmp = glyph->bitmap;
            if (character_cache.size() >= character_cache_size)
                character_cache.pop_front();
            character_cache.emplace_back();
            Character &next = character_cache.back();
            next.code = code;
            next.left = glyph->bitmap_left;
            next.top = 0 - glyph->bitmap_top;
            next.width = bmp.width;
            next.full_width = glyph->advance.x >> 6;
            next.height = bmp.rows;
            next.data.reserve(bmp.width * bmp.rows);
            for (int y = 0; y < bmp.rows; y++)
            {
                for (int x = 0; x < bmp.width; x++)
                {
                    next.data.emplace_back(bmp.buffer[y * bmp.pitch + x / 8] & (1 << (7 - (x & 7))));
                }
            }
            return &next;
        }
    }
    return Optional<const Character *>();
}

int Font::TextLength(const std::string &str)
{
    int len = 0;
    wchar_t buf[256];
    int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), buf, sizeof buf);
    for (int i = 0; i < count; i++)
    {
        Optional<const Character *> character = GetChar(buf[i]);
        if (character)
        {
            len += character.take()->full_width;
        }
    }
    return len;
}

Font::~Font()
{
}

} // Namespace Common
