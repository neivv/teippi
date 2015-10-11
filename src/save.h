#ifndef SAVE_H
#define SAVE_H

#include "types.h"

void Command_Save(const uint8_t *data);
int LoadGameObjects();
void SaveGame(const char *filename, uint32_t time);

#endif // SAVE_H
