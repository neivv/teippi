#include "dialog.h"
#include "offsets.h"
#include "unit.h"
#include "strings.h"
#include <stdio.h>
#include <vector>

Unit *ui_transported_units[8] = {0, 0, 0, 0, 0, 0, 0, 0};

void TransportStatus_UpdateDrawnValues()
{
    Unit *transport = *bw::primary_selected;
    int i = 0;
    for (Unit *unit = transport->first_loaded; unit; unit = unit->next_loaded, i++)
    {
        ui_transported_units[i] = unit;
        bw::ui_transported_unit_hps[i] = unit->hitpoints;
    }
    *bw::redraw_transport_ui = Ui_NeedsRedraw_Unk(); // ???
    *bw::ui_hitpoints = transport->hitpoints;
    *bw::ui_shields = transport->shields >> 8;
    *bw::ui_energy = transport->energy;
}

int TransportStatus_DoesNeedRedraw()
{
    Unit *transport = *bw::primary_selected;
    if (*bw::redraw_transport_ui)
        return true;
    if (!*bw::is_replay && transport->player != *bw::local_player_id)
        return GenericStatus_DoesNeedRedraw();

    int i = 0;
    for (Unit *unit = transport->first_loaded; unit; unit = unit->next_loaded, i++)
    {
        if (ui_transported_units[i] != unit)
            return true;
        if (bw::ui_transported_unit_hps[i] != unit->hitpoints)
            return true;
    }

    return GenericStatus_DoesNeedRedraw();
}

void DrawStatusScreen_LoadedUnits(Dialog *dlg)
{
    Unit *transport = *bw::primary_selected;
    Control *first_button = dlg->FindChild(StatusScreen::TransportLargeUnit);
    Control *large_unit_button, *medium_unit_button, *small_unit_button;
    int remaining_space = transport->Type().SpaceProvided();
    if (remaining_space < 8)
    {
        large_unit_button = dlg->FindChild(StatusScreen::TransportLargeUnit4, first_button);
        medium_unit_button = dlg->FindChild(StatusScreen::TransportMediumUnit4, large_unit_button);
        small_unit_button = dlg->FindChild(StatusScreen::TransportSmallUnit4, medium_unit_button);
    }
    else
    {
        remaining_space = 8;
        large_unit_button = first_button;
        medium_unit_button = dlg->FindChild(StatusScreen::TransportMediumUnit, large_unit_button);
        small_unit_button = dlg->FindChild(StatusScreen::TransportSmallUnit, medium_unit_button);
    }
    for (Control *ctrl = first_button; ctrl->id != StatusScreen::TransportSmallUnit + 8; ctrl = ctrl->next)
    {
        ctrl->val = 0;
    }
    for (Unit *unit = transport->first_loaded; unit; unit = unit->next_loaded)
    {
        int space = unit->Type().SpaceRequired();
        remaining_space -= space;
        if (remaining_space < 0)
            break;

        Control *current_button;
        switch (space)
        {
            case 1:
                current_button = small_unit_button;
                small_unit_button = small_unit_button->next;
            break;
            case 2:
                current_button = medium_unit_button;
                medium_unit_button = medium_unit_button->next;
                small_unit_button = small_unit_button->next->next;
            break;
            case 4:
                current_button = large_unit_button;
                large_unit_button = large_unit_button->next;
                medium_unit_button = medium_unit_button->next->next;
                small_unit_button = small_unit_button->next->next->next->next;
            break;
            default:
                current_button = 0;
            break;
        }

        current_button->val = unit;
        int border_id;
        if (unit->flags & UnitStatus::Hallucination)
            border_id = StatusScreen::SmallUnitBorderHallucination + ((space >> 1) * 2);
        else if (unit->Type().Flags() & UnitFlags::Hero)
            border_id = StatusScreen::SmallUnitBorderHero + ((space >> 1) * 2);
        else if (unit->parasites)
            border_id = StatusScreen::SmallUnitBorderParasite + ((space >> 1) * 2);
        else
            border_id = StatusScreen::SmallUnitBorder + ((space >> 1) * 2);

        current_button->button_icon = border_id;
        current_button->Show();
        current_button->MarkDirty();
    }
    for (Control *ctrl = first_button; ctrl->id != StatusScreen::TransportSmallUnit + 8; ctrl = ctrl->next)
    {
        if (!ctrl->val)
            ctrl->Hide();
    }
}

static char ss_kills[0x20];

void StatusScreen_DrawKills(Dialog *dlg)
{
    Unit *unit = *bw::primary_selected;
    auto unit_id = unit->Type();
    if (unit_id == UnitId::Scourge || unit_id == UnitId::InfestedTerran || unit->IsKnownHallucination() || (!unit->HasWayOfAttacking() && !unit->kills))
    {
        dlg->FindChild(StatusScreen::Kills)->Hide();
    }
    else
    {
        snprintf(ss_kills, sizeof ss_kills, "%s %d", (*bw::stat_txt_tbl)->GetTblString(String::Kills), unit->kills);
        SetLabel(dlg, ss_kills, StatusScreen::Kills);
    }
}

void Control::MarkDirty()
{
    if (~flags & 0x1)
    {
        flags |= 0x1;
        MarkControlDirty(this);
    }
}

void Control::Show()
{
    if (flags & 0x8)
        return;

    flags |= 0x8;
    Event event;
    event.ext_type = 0xd;
    event.type = 0xe;
    event.x = *bw::mouse_clickpos_x;
    event.y = *bw::mouse_clickpos_y;
    event.unk4 = 0;

    if ((*EventHandler)(this, &event))
        MarkDirty();
}

void Control::Hide()
{
    if (~flags & 0x8)
        return;

    Event event;
    event.ext_type = 0xe;
    event.type = 0xe;
    event.x = *bw::mouse_clickpos_x;
    event.y = *bw::mouse_clickpos_y;
    event.unk4 = 0;
    if ((*EventHandler)(this, &event))
    {
        event.ext_type = 0x6;
        (*EventHandler)(this, &event);
        MarkDirty();
    }
}

Control *Control::FindChild(int id, Control *search_beg) const
{
    if (type == 0)
    {
        Dialog *dlg = (Dialog *)this;
        return dlg->FindChild(id, search_beg);
    }
    else
        return parent->FindChild(id, search_beg);
}

Control *Dialog::FindChild(int id, Control *search_beg) const
{
    if (search_beg)
    {
        for (Control *ctrl = search_beg; ctrl; ctrl = ctrl->next)
        {
            if (ctrl->id == id)
                return ctrl;
        }
    }
    else
    {
        for (Control *ctrl = dialog.first_child; ctrl; ctrl = ctrl->next)
        {
            if (ctrl->id == id)
                return ctrl;
        }
    }
    return 0;
}
