#ifndef INIT_H
#define INIT_H

#include "types.h"

void *LoadGrp(ImageType image_id, uint32_t *images_dat_grp, Tbl *images_tbl, GrpSprite **loaded_grps, void **overlapped, void **out_file);
void LoadBlendPalettes(const char *tileset_name);

void InitLoneSprites();
int InitGame();
void InitCursorMarker();
void InitStartingRacesAndTypes();
void InitResourceAreas();

#endif /* INIT_H */
