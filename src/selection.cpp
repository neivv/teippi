#include "selection.h"

#include "text.h"
#include "unit.h"
#include "sprite.h"
#include "patchmanager.h"
#include "resolution.h"
#include "dialog.h"
#include "targeting.h"
#include "order.h"
#include "commands.h"
#include "yms.h"
#include "warn.h"

#include <string.h>
#include "console/windows_wrap.h"

Selection client_select(bw::client_selection_group);
Selection client_select3(bw::client_selection_group3);
Selection selections[8] =
{
    {bw::selection_groups[0]}, {bw::selection_groups[1]},
    {bw::selection_groups[2]}, {bw::selection_groups[3]},
    {bw::selection_groups[4]}, {bw::selection_groups[5]},
    {bw::selection_groups[6]}, {bw::selection_groups[7]},
};

Unit *Selection::Get(int pos)
{
    return *(Unit **)(addr + 4 * pos);
}

int Selection::Find(Unit *unit)
{
    for (unsigned i = 0; i < Limits::Selection; i++)
    {
        if (unit == addr[i])
            return i;
    }
    return -1;
}

bool ShouldClearOrderTargeting()
{
    for (Unit *unit : client_select3)
    {
        if (!unit->IsDisabled())
            return false;
    }
    return true;
}

void SendChangeSelectionCommand(int count, Unit **units)
{
    int removed_count = 0, added_count = 0;
    uint32_t rem_stack[20], add_stack[20];
    uint32_t *removed_units = 0, *added_units = 0;
    if (*bw::client_selection_changed != 0)
    {
        added_count = removed_count = count;
    }
    else if (count > 20)
    {
    }
    else
    {
        removed_units = rem_stack;
        added_units = add_stack;
        uint32_t *removed_pos = removed_units, *added_pos = added_units;
        for (int i = 0; i < count; i++)
        {
            bool found = false;
            for (unsigned j = 0; j < Limits::Selection; j++)
            {
                if (bw::client_selection_group2[j] == 0)
                    break;
                else if (bw::client_selection_group2[j] == units[i])
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                added_count++;
                *added_pos++ = units[i]->lookup_id;
            }
        }

        for (unsigned i = 0; i < Limits::Selection; i++)
        {
            if (bw::client_selection_group2[i] == 0)
                break;
            bool found = false;
            for (int j = 0; j < count; j++)
            {
                if (bw::client_selection_group2[i] == units[j])
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                removed_count++;
                *removed_pos++ = bw::client_selection_group2[i]->lookup_id;
            }
        }

    }

    std::fill(bw::client_selection_group2.begin(), bw::client_selection_group2.end(), nullptr);
    for (int i = 0; i < count; i++)
    {
        bw::client_selection_group2[i] = units[i];
    }

    if (removed_count + added_count < count)
    {
        if (removed_count > 20 || added_count > 20)
        {
            // alloc mem
        }
        else
        {
            uint8_t command_buf[166];
            command_buf[1] = 0;
            if (removed_count)
            {
                command_buf[0] = (uint8_t)commands::SelectionRemove;
                *(uint32_t *)(command_buf + 2) = removed_count;
                uint32_t *out = (uint32_t *)(command_buf + 6);
                for (int i = 0; i < count; i++)
                {
                    *out++ = removed_units[i];
                }
                SendCommand(command_buf, 6 + removed_count * 4);
            }
            if (added_count)
            {
                command_buf[0] = (uint8_t)commands::SelectionAdd;
                *(uint32_t *)(command_buf + 2) = added_count;
                uint32_t *out = (uint32_t *)(command_buf + 6);
                for (int i = 0; i < count; i++)
                {
                    *out++ = added_units[i];
                }
                SendCommand(command_buf, 6 + added_count * 4);
            }
        }
    }
    else
    {
        if (count > 20)
        {
            // alloc mem
        }
        else
        {
            uint8_t command_buf[166];
            command_buf[0] = (uint8_t)commands::Select;
            command_buf[1] = 0;
            *(uint32_t *)(command_buf + 2) = count;
            uint32_t *out = (uint32_t *)(command_buf + 6);
            for (int i = 0; i < count; i++)
            {
                *out++ = units[i]->lookup_id;
            }
            SendCommand(command_buf, 6 + count * 4);
        }
    }
}

void Command_SelectionRemove(uint8_t *buf)
{
    if (buf[1] != 0)
    {
        Warning("Bad select: %d", buf[1]);
    }
    else
    {
        int player = *bw::select_command_user;
        uint32_t count = *(uint32_t *)(buf + 2);
        uint32_t *unit_ids = (uint32_t *)(buf + 6);
        auto selection = bw::selection_groups[player];
        int remaining = 0;
        for (uint32_t i = 0; i < count && selection[i]; i++)
        {
            Unit *unit = Unit::FindById(unit_ids[i]);
            if (!unit)
                continue;
            Sprite *sprite = unit->sprite.get();
            if (unit->IsDying() || sprite->IsHidden() || unit->player != player)
                continue;

            if (HasTeamSelection(player) && (sprite->flags & 0x6))
            {
                // ???
                int flag = sprite->flags;
                int flag2 = flag >> 1;
                if (flag2 & 3)
                {
                    flag2--;
                    flag2 = flag2 << 1;
                    sprite->flags = ((flag2 ^ flag) & 6) ^ flag;
                    if (sprite->flags & 6)
                        RemoveDashedSelectionCircle(sprite);
                }
            }
            remaining = RemoveFromSelection(unit, player);
        }
        if (remaining > 1)
            AddToRecentSelections();

    }
}

void Command_SelectionAdd(uint8_t *buf)
{
    if (buf[1] != 0)
    {
        Warning("Bad select: %d", buf[1]);
    }
    else
    {
        int player = *bw::select_command_user;
        uint32_t count = *(uint32_t *)(buf + 2);
        uint32_t *unit_ids = (uint32_t *)(buf + 6);
        auto selection = bw::selection_groups[player];

        unsigned int original_count = 0, selection_index;
        while (original_count < Limits::Selection && selection[original_count])
            original_count++;

        selection_index = original_count;
        if (original_count < Limits::Selection)
        {
            for (uint32_t i = 0; i < count && selection_index < Limits::Selection; i++)
            {
                Unit *unit = Unit::FindById(unit_ids[i]);
                if (!unit || unit->IsDying() || unit->sprite->IsHidden() || unit->player != player)
                    continue;
                if (original_count != 0 && !IsMultiSelectable(unit))
                    continue;

                bool found = false;
                for (unsigned i = 0; i < original_count; i++)
                {
                    if (selection[i] == unit)
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                    continue;

                selection[selection_index++] = unit;
                if (HasTeamSelection(player))
                    MakeDashedSelectionCircle(unit);
            }
            if (count > 1)
                AddToRecentSelections();
        }
    }
}

void Command_Select(uint8_t *buf)
{
    if (buf[1] != 0)
    {
        Warning("Bad select: %d", buf[1]);
    }
    else
    {
        int player = *bw::select_command_user;
        uint32_t count = *(uint32_t *)(buf + 2);
        uint32_t *unit_ids = (uint32_t *)(buf + 6);

        ClearSelection(player);
        int selection_index = 0;
        for (uint32_t i = 0; i < count; i++)
        {
            Unit *unit = Unit::FindById(unit_ids[i]);
            if (!unit || unit->IsDying() || unit->sprite->IsHidden())
                continue;
            // Didn't bother preventing selecting nuke >:D
            if (!AddToPlayerSelection(player, selection_index, unit))
                continue;

            selection_index++;
        }
        if (count > 1)
            AddToRecentSelections();
    }
}

void CenterOnSelectionGroup(uint8_t group_id)
{
    int self = *bw::local_unique_player_id;
    auto group = bw::selection_hotkeys[self][group_id];
    unsigned int count = 0;
    while (count < Limits::Selection && group[count] != nullptr)
        count++;

    if (!count)
        return;

    int x = 0, y = 0;
    for (unsigned i = 0; i < count; i++)
    {
        Unit *unit = group[i];
        if (unit->IsDying() || unit->sprite->IsHidden())
            continue;
        if (IsMultiSelectable(unit) || count == 1)
        {
            x += unit->sprite->position.x;
            y += unit->sprite->position.y;
        }
    }

    MoveScreen((x / count - resolution::game_width / 2) & ~8, (y / count - resolution::game_height / 2) & ~8);
}

static uint32_t hotkey_select_tick = 0;
static char prev_group = -1;
void SelectHotkeyGroup(uint8_t group_id)
{
    int self = *bw::local_unique_player_id;
    auto group = bw::selection_hotkeys[self][group_id];
    Unit *valid_units[Limits::Selection];
    unsigned int count = 0;
    while (count < Limits::Selection && group[count] != nullptr)
        count++;

    if (!count)
        return;

    int valid_units_count = 0;
    for (unsigned i = 0; i < count; i++)
    {
        Unit *unit = group[i];
        // Todo: Why sc compare command_user instead of self_player_id2 o.o
        if (unit->IsDying() || unit->sprite->IsHidden() || unit->player != *bw::command_user)
        {
            continue;
        }
        if (IsMultiSelectable(unit) || count == 1)
            valid_units[valid_units_count++] = unit;

    }
    if (valid_units_count == 1 && valid_units[0]->HasRally())
        ShowRallyTarget(valid_units[valid_units_count]);

    ClearOrderTargeting();

    UpdateSelectionOverlays(valid_units, valid_units_count);
    Unit *highest_rank = 0;
    for (int i = 0; i < valid_units_count; i++)
    {
        if (IsHigherRank(valid_units[i], highest_rank))
            highest_rank = valid_units[i];
    }
    PlaySelectionSound(highest_rank);
    *bw::client_selection_changed = 1;
    *bw::force_portrait_refresh = 1;
    *bw::force_button_refresh = 1;
    *bw::ui_refresh = 1;

    uint8_t cmdbuf[3] = { commands::Hotkey, 1, group_id };
    SendCommand(cmdbuf, 3);

    std::fill(bw::client_selection_group2.begin(), bw::client_selection_group2.end(), nullptr);
    for (int i = 0; i < valid_units_count; i++)
    {
        bw::client_selection_group2[i] = valid_units[i];
    }

    uint32_t tick = GetTickCount();
    if (tick - hotkey_select_tick < 500 && group_id == prev_group)
        CenterOnSelectionGroup(group_id);
    hotkey_select_tick = tick;
    prev_group = group_id;
}

void Command_LoadHotkeyGroup(int group_id)
{
    int player = *bw::select_command_user;
    auto group = bw::selection_hotkeys[player][group_id];
    auto selection = bw::selection_groups[player];
    unsigned int count = 0;
    while (count < Limits::Selection && group[count] != nullptr)
        count++;

    if (!count)
        return;

    ClearSelection(player);

    unsigned int valid_units_count = 0;
    for (unsigned i = 0; i < count; i++)
    {
        Unit *unit = group[i];
        if (
            (unit->IsDying()) ||
            (unit->sprite->IsHidden()) ||
            (unit->player != *bw::command_user) ||
            (!IsMultiSelectable(unit) && valid_units_count > 1)
            ) {
            continue;
        }

        selection[valid_units_count++] = unit;
        if (HasTeamSelection(player))
            MakeDashedSelectionCircle(unit);
    }
    if (valid_units_count < Limits::Selection)
        selection[valid_units_count] = nullptr;
    if (group_id >= 10)
        bw::recent_selection_times[player][(group_id - 10)] = *bw::frame_counter;
    else
        AddToRecentSelections();
}

void Command_SaveHotkeyGroup(int group_id, bool shift_add)
{
    auto group = bw::selection_hotkeys[*bw::select_command_user][group_id];
    auto selection = bw::selection_groups[*bw::select_command_user];
    unsigned int count = 0;

    if (selection[0] && selection[0]->player != *bw::select_command_user)
        return; // sc actually checks that later but this works?

    if (shift_add)
    {
        Unit *unit = group[0];
        if (unit)
        {
            // IsMultiSelectable requires alive unit I guess
            if (!unit->IsDying() && !IsMultiSelectable(unit))
                return;
            count++;
            while (count < Limits::Selection && group[count] != nullptr)
                count++;
        }
    }
    for (unsigned i = count; i < Limits::Selection && group[i] != nullptr; i++)
    {
        group[i] = nullptr;
    }

    for (unsigned i = 0; i < Limits::Selection && count < Limits::Selection && selection[i] != nullptr; i++)
    {
        Unit *unit = selection[i];
        if (shift_add)
        {
            bool skip = false;
            for (unsigned j = 0; j < Limits::Selection && group[j] != nullptr; j++)
            {
                if (group[j] == unit)
                {
                    skip = true;
                    break;
                }
            }
            if (skip)
                continue;
        }
        if (count == 0 || IsMultiSelectable(unit))
        {
            group[count++] = unit;
        }
    }
}

int TrySelectRecentHotkeyGroup(Unit *unit)
{
    int result_group = -1;
    int player = *bw::local_unique_player_id;
    for (int group_id = 10; group_id < 18; group_id++)
    {
        auto group = bw::selection_hotkeys[player][group_id];
        for (unsigned entry = 0; entry < Limits::Selection && group[entry] != nullptr; entry++)
        {
            if (group[entry] == unit)
            {
                if (result_group == -1 || bw::recent_selection_times[player][group_id - 10] > bw::recent_selection_times[player][result_group - 10])
                {
                    result_group = group_id;
                    break;
                }
            }
        }
    }
    if (result_group != -1)
    {
        SelectHotkeyGroup(result_group);
        return 1;
    }
    return 0;
}

int SelectCommandLength(uint8_t *data)
{
    return *(uint32_t *)(data + 2) * 4 + 6;
}

static void RemoveFromHotkeyGroup(Unit *unit, int player, int group_id)
{
    auto group = bw::selection_hotkeys[player][group_id];
    for (unsigned i = 0; i < Limits::Selection; i++)
    {
        if (group[i] == unit)
        {
            std::move(group.begin() + i + 1, group.end(), group.begin() + i);
            group[Limits::Selection - 1] = nullptr;
            return;
        }
    }
}

void RemoveFromHotkeyGroups(Unit *unit)
{
    for (int player = 0; player < Limits::ActivePlayers; player++)
    {
        for (int group = 0; group < 0x12; group++)
            RemoveFromHotkeyGroup(unit, player, group);
    }
}

void StatusScreenButton(Control *clicked_button)
{
    std::vector<Unit *> units;
    units.reserve(12);
    if (*bw::shift_down)
    {
        Control *button = clicked_button->FindChild(StatusScreen::FirstSmallButton, 0);
        for (int i = 0; i < 12; i++)
        {
            if (~button->flags & 0x8)
                continue;

            if (clicked_button != button)
            {
                Unit *unit = *(Unit **)button->val;
                if (unit)
                    units.push_back(unit);
            }
            button = button->next;
        }
    }
    else if (*bw::ctrl_down)
    {
        int select_unit_id = (*(Unit **)clicked_button->val)->unit_id;
        Control *button = clicked_button->FindChild(StatusScreen::FirstSmallButton, 0);
        for (int i = 0; i < 12; i++)
        {
            if (~button->flags & 0x8)
                continue;

            Unit *unit = *(Unit **)button->val;
            if (unit->unit_id == select_unit_id)
                units.push_back(unit);

            button = button->next;
        }
    }
    else
    {
        units.push_back(*(Unit **)clicked_button->val);
    }
    if (!units.empty() && *bw::alt_down)
    {
        if (TrySelectRecentHotkeyGroup(units[0]))
            return;
    }

    UpdateSelectionOverlays(units.data(), units.size());
    SendChangeSelectionCommand(units.size(), units.data());
    *bw::client_selection_changed = 1;
    RefreshUi();
}
