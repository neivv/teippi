#include "bunker.h"

#include "constants/unit.h"
#include "constants/sprite.h"
#include "image.h"
#include "lofile.h"
#include "offsets.h"
#include "unit.h"
#include "sprite.h"

void CreateBunkerShootOverlay(Unit *unit)
{
    Unit *bunker = unit->related;
    Sprite *sprite;
    int direction;
    Image *bunker_img = bunker->sprite->main_image;

    LoFile lo = LoFile::GetOverlay(bunker_img->Type(), Overlay::Attack);
    int lo_direction = (((unit->facing_direction + 16) / 32) & 0x7);
    Point pos = lo.GetPosition(bunker_img, lo_direction);

    if (unit->Type() == UnitId::Firebat || unit->Type() == UnitId::GuiMontag)
    {
        direction = (((unit->facing_direction + 8) / 16) & 0xf) * 16;
        sprite = lone_sprites->AllocateLone(SpriteId::FlameThrower, pos, unit->player);
    }
    else
    {
        direction = lo_direction * 32;
        sprite = lone_sprites->AllocateLone(SpriteId::BunkerOverlay, pos, unit->player);
    }
    sprite->elevation = bunker->sprite->elevation + 1;
    sprite->SetDirection256(direction);
    sprite->UpdateVisibilityPoint();
}
