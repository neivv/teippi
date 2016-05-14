#ifndef TARGETING_H
#define TARGETING_H

#include "types.h"

Unit *FindUnitAtPoint(int x, int y);

void SendRightClickCommand(Unit *unit, uint16_t x, uint16_t y, UnitType unit_id, bool queued);

void ClearOrderTargetingIfNeeded();
void ClearOrderTargeting();

void Command_RightClick(const uint8_t *buf);
void Command_Targeted(const uint8_t *buf);

void GameScreenRClickEvent(Event *event);
void GameScreenLClickEvent_Targeting(Event *event);

void DoTargetedCommand(int x, int y, Unit *target, UnitType fow_unit);

void Test_SendTargetedOrderCommand(uint8_t order, const Point &pos, Unit *target, UnitType fow_unit, uint8_t queued);

namespace RightClickAction
{
    static const int None = 0;
    static const int MoveAndAttack = 1;
    static const int Move = 2;
    static const int Attack = 3;
    static const int Harvest = 4;
    static const int HarvestAndRepair = 5;
    static const int None2 = 6;
}

#endif // TARGETING_H

