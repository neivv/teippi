#ifndef DIALOG_H
#define DIALOG_H

#include "types.h"

namespace StatusScreen
{
    const int TransportLargeUnit = 0x12;
    const int TransportLargeUnit4 = TransportLargeUnit + 2;
    const int TransportMediumUnit = TransportLargeUnit4 + 1;
    const int TransportMediumUnit4 = TransportMediumUnit + 1;
    const int TransportSmallUnit = TransportMediumUnit + 4;
    const int TransportSmallUnit4 = TransportSmallUnit + 2;

    const int SmallUnitBorder = 0xd;
    const int MediumUnitBorder = 0xf;
    const int LargeUnitBorder = 0x11;

    const int SmallUnitBorderHero = 0x13;
    const int SmallUnitBorderParasite = 0x19;
    const int SmallUnitBorderHallucination = 0x1f;

    const int FirstSmallButton = 0x21;

    const int Kills = 0xffeb;
}

#pragma pack(push)
#pragma pack(1)

struct Event
{
    uint32_t ext_type;
    uint32_t unk4;
    uint8_t dc8[4];
    uint16_t type;
    uint16_t x;
    uint16_t y;
};

class Control
{
    public:
        Control *next;
        Rect16 area;
        uint8_t dcc[0x8];
        char *string;
        uint8_t flags;
        uint8_t dc19[0x7];
        uint16_t id; //0x20
        uint16_t type;
        uint16_t button_icon;
        void *val;
        int (__fastcall *EventHandler)(Control *, Event *);
        void (__fastcall *Draw)(Control *);
        Dialog *parent;
        union
        {
            struct
            {
                Control *scrollbar;
                uint8_t dc40[0x8];
                void **data;
                uint8_t entries;
                uint8_t unk47;
                uint8_t selected_entry;
                uint8_t entry_height;
                uint8_t visible_entries;
                uint8_t max_draw_begin_entry;
                uint8_t flags;
                uint8_t draw_begin_entry;
            } list;
            struct
            {
                uint8_t dc36[0xc];
                Control *first_child;
                uint8_t dc46[0x4];
                void *OnceInFrame;
            } dialog;
        };

        Control() {}
        ~Control() {}

        void Show();
        void Hide();
        void MarkDirty();
        void Freeze() { flags |= 0x80000000; }
        void Thaw() { flags &= ~0x80000000; }
        Control *FindChild(int id, Control *search_beg = 0) const;
};

class Dialog : public Control
{
    public:
        Dialog() {}
        ~Dialog() {}
        Control *FindChild(int id, Control *search_beg = 0) const;
};

#pragma pack(pop)

void TransportStatus_UpdateDrawnValues();
void DrawStatusScreen_LoadedUnits();
int TransportStatus_DoesNeedRedraw();
void StatusScreen_DrawKills();


#endif // DIALOG_H

