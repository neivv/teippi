#ifndef UPGRADE_H
#define UPGRADE_H

#include "constants/upgrade.h"
int GetUpgradeLevel(int upgrade, int player);
void SetUpgradeLevel(int upgrade, int player, int amount);
int MovementSpeedUpgradeUnit(int upgrade);
int AttackSpeedUpgradeUnit(int upgrade);
const char *GetUpgradeName(int upgrade);

#endif // UPGRADE_H

