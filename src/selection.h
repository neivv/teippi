#ifndef SELECTION_H
#define SELECTION_H

#include "offsets.h"
#include "limits.h"

bool ShouldClearOrderTargeting();

int SelectCommandLength(uint8_t *data);

void RemoveFromHotkeyGroups(Unit *unit);

void Command_Select(uint8_t *buf);
void Command_SelectionAdd(uint8_t *buf);
void Command_SelectionRemove(uint8_t *buf);

void StatusScreenButton();

void SendChangeSelectionCommand(int count, Unit **units);
void CenterOnSelectionGroup(uint8_t group_id);
void SelectHotkeyGroup(uint8_t group_id);
void Command_SaveHotkeyGroup(int group, bool shift_add);
void Command_LoadHotkeyGroup(int group_id);
int TrySelectRecentHotkeyGroup(Unit *unit);
void StatusScreenButton(Control *clicked_button);

class Unit;

class Selection
{
    public:
        class iterator
        {
            public:
                iterator(offset<Unit *> addr) : value((Unit **)addr.v()) {}
                Unit *operator*() { return *value; }
                bool operator==(const iterator &other) { return other.value == value; }
                bool operator!=(const iterator &other) { return other.value != value; }
                iterator &operator++() { value++; return *this; }
                Unit **value;
        };

        iterator begin() { return iterator(addr); }
        iterator end() { return iterator(addr + Count() * 4); }
        int Find(Unit *unit);

        Selection(offset<Unit *> address) : addr(address) {}
        Unit *Get(int pos);
        Unit *operator[](int pos) { return Get(pos); }

        int Count() { int count = Find(0); if (count < 0) return Limits::Selection; return count; }

    private:
        offset<Unit *> addr;
};

extern Selection client_select;
extern Selection client_select3;
extern Selection selections[8];

#endif // SELECTION_H
