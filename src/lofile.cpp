#include "lofile.h"

#include "offsets.h"
#include "image.h"
#include "sprite.h"

LoFile LoFile::GetOverlay(ImageType image_id, int type)
{
    return LoFile(image_id.Overlay(type));
}

Point32 LoFile::GetValues(Image *img, int index)
{
    int8_t x, y;
    int offset = *(int *)(addr + (8 + img->frame * 4));

    x = addr[offset + index * 2 + 0];
    y = addr[offset + index * 2 + 1];
    if (img->IsFlipped())
        x = 0 - x;
    return Point32(x, y);
}

Point LoFile::GetPosition(Image *img, int index)
{
    Point32 lo = GetValues(img, index);
    if (lo == Point32(127, 127))
        return Point(0xffff, 0xffff); // I hope it is unused..
    return Point(img->parent->position.x + lo.x, img->parent->position.y + lo.y);
}

void LoFile::SetImageOffset(Image *img)
{
    int8_t x, y;
    int offset = *(int *)(addr + 8 + img->parent->main_image->frame * 4);
    x = addr[offset + 0];
    y = addr[offset + 1];
    if (img->IsFlipped())
        x = 0 - x;

    img->SetOffset(x, y);
}
