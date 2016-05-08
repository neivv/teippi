#ifndef LOFILE_H
#define LOFILE_H

#include "types.h"

namespace Overlay
{
    static const int Attack = 0;
    static const int Damage = 1;
    static const int Special = 2;
    static const int Land = 3;
    static const int Liftoff = 4;
    static const int Shields = 5;
}

class LoFile
{
    public:
        static LoFile GetOverlay(ImageType image_id, int type);
        Point32 GetValues(Image *img, int index);
        Point GetPosition(Image *img, int index);

        void SetImageOffset(Image *img);
        bool IsValid() const { return addr != nullptr; }

    private:
        LoFile(void * addr_) { addr = (int8_t *)addr_; }
        int8_t *addr;

};

#endif // LOFILE_H
