#ifndef SURFACE_H
#define SURFACE_H

#include "types.h"
#include "font.h"
#include <string>
#include <functional>
#include "windows_wrap.h"

namespace Common
{

class RetTrueSurfacedraw
{
    public:
        constexpr RetTrueSurfacedraw() {}
        bool operator()(xint<int> a, yint<int> b) const { return true; }
};
static constexpr auto ret_true_surfacedraw = RetTrueSurfacedraw();

class Surface
{
    public:
        Surface() { buf = 0; own_buf = false; }
        Surface(uint8_t *buf_, xint<int> w, yint<int> h)
        {
            buf = buf_;
            width = w;
            height = h;
            own_buf = false;
        }

        ~Surface() { if (own_buf) delete[] buf; }
        void Init(int w, int h)
        {
            width = w;
            height = h;
            buf = new uint8_t[width * height];
            own_buf = true;
        }

        bool IsInBounds(const Point32 &pos)
        {
            return pos.x >= 0 && pos.y >= 0 && pos.x < width && pos.y < height;
        }

        template<class Func = decltype(ret_true_surfacedraw)>
        void DrawDot(const Common::Point32 &pos, uint8_t color, Func IsValid = ret_true_surfacedraw)
        {
            if (IsValid(pos.x, pos.y) && IsInBounds(pos))
                buf[pos.x + pos.y * width] = color;
        }

        template<class Func = decltype(ret_true_surfacedraw)>
        void DrawLine(const Common::Point32 *pos1, const Common::Point32 *pos2, uint8_t color, Func IsValid = ret_true_surfacedraw)
        {
            if ((pos1->x < 0 && pos2->x < 0) || (pos1->y < 0 && pos2->y < 0))
                return;
            if ((pos1->x >= width && pos2->x >= width) || (pos1->y >= height && pos2->y >= height))
                return;

//            if (pos1->x < 0 && pos2->x > 0 && pos1->y > 0 && pos2->y > 0 && pos1->x < width && pos2->x < width)
//                asm("int3");
            if (pos2->y < pos1->y)
            {
                const Common::Point32 *tmp = pos2;
                pos2 = pos1;
                pos1 = tmp;
            }
            Point draw_pos = *pos1;
            if (pos1->y == pos2->y)
            {
                int x_inc = 1;
                if (draw_pos.x > pos2->x)
                    x_inc = -1;
                while (!IsInBounds(draw_pos))
                    draw_pos.x += x_inc;
                while (draw_pos.x != pos2->x && IsInBounds(draw_pos))
                {
                    DrawDot(draw_pos, color, IsValid);
                    draw_pos.x += x_inc;
                }
            }
            else
            {
                double angle = (double)((pos1->x - pos2->x)) / (pos2->y - pos1->y);
                double x_inc = 0;

                if (draw_pos.y < 0)
                {
                    x_inc -= angle * draw_pos.y;
                    draw_pos.x -= (int)x_inc;
                    x_inc = x_inc - (int)x_inc;
                    draw_pos.y = 0;
                }
                // IsInBounds() ois et lopettaa ku menny ruudun ulkopuolelle,
                // mutta jos on jo aluksi ulkopuolella nii ei piirtäs mitää D: (todo?)
                while (draw_pos.y != pos2->y /* && IsInBounds(draw_pos) */)
                {
                    DrawDot(draw_pos, color, IsValid);
                    x_inc += angle;
                    while (x_inc >= 1)
                    {
                        draw_pos.x -= 1;
                        if (x_inc >= 2)
                            DrawDot(draw_pos, color, IsValid);
                        x_inc -= 1;
                    }
                    while (x_inc <= -1)
                    {
                        draw_pos.x += 1;
                        if (x_inc <= -2)
                            DrawDot(draw_pos, color, IsValid);
                        x_inc += 1;
                    }
                    draw_pos.y++;
                }
                while (draw_pos.x != pos2->x /* && IsInBounds(draw_pos) */)
                {
                    DrawDot(draw_pos, color, IsValid);
                    if (angle > 0)
                        draw_pos.x -= 1;
                    else
                        draw_pos.x += 1;
                }
                DrawDot(draw_pos, color, IsValid);
            }
        }
        template<class Func = decltype(ret_true_surfacedraw)>
        void DrawLine(const Common::Point32 &pos1, const Common::Point32 &pos2, uint8_t color, Func IsValid = ret_true_surfacedraw)
        {
            DrawLine(&pos1, &pos2, color, IsValid);
        }

        template<class Func = decltype(ret_true_surfacedraw)>
        void DrawRect(const Common::Rect32 &rect, uint8_t color, Func IsValid = ret_true_surfacedraw)
        {
            DrawLine(Point32(rect.left, rect.top), Point32(rect.right, rect.top), color, IsValid);
            DrawLine(Point32(rect.left, rect.top), Point32(rect.left, rect.bottom), color, IsValid);
            DrawLine(Point32(rect.right, rect.top), Point32(rect.right, rect.bottom), color, IsValid);
            DrawLine(Point32(rect.left, rect.bottom), Point32(rect.right, rect.bottom), color, IsValid);
        }

        template<class Func = decltype(ret_true_surfacedraw)>
        void DrawCircle(const Point32 &pos, int radius, uint8_t color, Func IsValid = ret_true_surfacedraw)
        {
            if (pos.x + radius < 0 || pos.x - radius >= width || pos.y + radius < 0 || pos.y - radius >= height)
                return;
            int x = radius;
            int y = 0;
            int radius_error = 1 - x;
            while (y <= x)
            {
                DrawDot(pos + Point32(x, y), color, IsValid);
                DrawDot(pos + Point32(y, x), color, IsValid);
                DrawDot(pos + Point32(-x, y), color, IsValid);
                DrawDot(pos + Point32(-y, x), color, IsValid);
                DrawDot(pos + Point32(-x, -y), color, IsValid);
                DrawDot(pos + Point32(-y, -x), color, IsValid);
                DrawDot(pos + Point32(x, -y), color, IsValid);
                DrawDot(pos + Point32(y, -x), color, IsValid);
                y += 1;
                if (radius_error < 0)
                    radius_error += 2 * y + 1;
                else
                {
                    x -= 1;
                    radius_error += 2 * (y - x) + 1;
                }
            }
        }

        template<class Func = decltype(ret_true_surfacedraw)>
        void DrawText(Font *font, const std::string &line, const Common::Point32 &pos, uint8_t color, Func IsValid = ret_true_surfacedraw)
        {
            if (pos.x >= width || pos.y >= height)
                return;
            Point32 draw_pos = pos;
            wchar_t buf[256];
            int count = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), line.length(), buf, sizeof buf);
            for (int i = 0; i < count; i++)
            {
                Optional<const Character *> character = font->GetChar(buf[i]);
                if (character)
                {
                    auto chara = character.take();
                    DrawChar<decltype(IsValid)>(chara, draw_pos + Point32(chara->left, chara->top), color, IsValid);
                    draw_pos += Point32(chara->full_width, 0);
                }
            }
        }
        template<class Func = decltype(ret_true_surfacedraw)>
        void DrawChar(const Character *bmp, const Common::Point32 &draw_pos, uint8_t color, Func IsValid = ret_true_surfacedraw)
        {
            if (draw_pos.x >= width || draw_pos.y >= height)
                return;
            if (draw_pos.x + bmp->width <= 0 || draw_pos.y + bmp->height <= 0)
                return;
            int bmp_row_pos = 0;
            int out_row_pos = draw_pos.y < 0 ? 0 : draw_pos.y * width;
            int draw_pos_x_skip = draw_pos.x < 0 ? (0 - draw_pos.x) : 0;
            int draw_pos_y_skip = draw_pos.y < 0 ? (0 - draw_pos.y) : 0;
            int x_limit = bmp->width < width - draw_pos.x ? bmp->width : width - draw_pos.x;
            int y_limit = bmp->height < height - draw_pos.y ? bmp->height : height - draw_pos.y;
            for (int row = draw_pos_y_skip; row < y_limit; row++)
            {
                uint8_t *out_pos = buf + out_row_pos + (draw_pos.x < 0 ? 0 : (int)draw_pos.x);
                const uint8_t *in_pos = bmp->data.data() + bmp_row_pos + draw_pos_x_skip;
                for (int x = draw_pos_x_skip; x < x_limit; x++)
                {
                    if (*in_pos != 0 && IsValid(draw_pos.x + x, draw_pos.y + row))
                    {
                        *out_pos = color;
                    }
                    out_pos += 1;
                    in_pos += 1;
                }
                bmp_row_pos += bmp->width;
                out_row_pos += width;
            }
        }

        uint8_t *buf;
        xint<int> width;
        yint<int> height;
        bool own_buf;
};

}

#endif // SURFACE_H

