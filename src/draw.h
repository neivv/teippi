#ifndef DRAW_H
#define DRAW_H

#include "types.h"
#include "resolution.h"
#include <atomic>

#pragma pack(push)
#pragma pack(1)
struct Surface
{
    x16u w;
    y16u h;
    uint8_t *image;
};

struct DrawParam
{
    Rect16 area;
    x16u w;
    y16u h;
};

struct DrawLayer
{
    uint8_t draw;
    uint8_t flags;
    Rect<int16_t> area;
    uint16_t unka;
    void *func_param;
    void (__fastcall *Draw)(int, int, void *func_param, DrawParam *area);
};

#pragma pack(pop)

void DrawScreen();
void PatchDraw(Common::PatchContext *patch);
void AddDrawHook(void (*func)(uint8_t *, xuint, yuint), int priority);

void GenerateFog();

extern std::atomic<uintptr_t> draw_counter;

#endif // DRAW_H

