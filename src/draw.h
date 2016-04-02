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
void AddDrawHook(void (*func)(uint8_t *, xuint, yuint), int priority);

int SDrawLockSurface_Hook(int surface_id, Rect32 *a2, uint8_t **surface, int *width, int unused);
int SDrawUnlockSurface_Hook(int surface_id, uint8_t *surface, int a3, int a4);

void GenerateFog();

extern std::atomic<uintptr_t> draw_counter;

#endif // DRAW_H

