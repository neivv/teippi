#ifndef TECH_H
#define TECH_H

#include "types.h"
#include "constants/tech.h"

int GetTechLevel(int tech, int player);
void SetTechLevel(int tech, int player, int amount);
const char *GetTechName(int tech);

void Maelstrom(Unit *attacker, const Point &position);
void EmpShockwave(Unit *attacker, const Point &position);
void Ensnare(Unit *attacker, const Point &position);
void Plague(Unit *attacker, const Point &position, vector<tuple<Unit *, Unit *>> *unit_was_hit);
void Stasis(Unit *attacker, const Point &position);
void DarkSwarm(int player, const Point &position);
void DisruptionWeb(int player, const Point &position);

void UpdateDwebStatuses();

void DoMatrixDamage(Unit *target, int dmg);

#endif // TECH_H

