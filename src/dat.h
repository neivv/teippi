#ifndef DAT_H
#define DAT_H

#include "types.h"

#include "common/assert.h"
#include "common/iter.h"
#include "offsets.h"

#pragma pack(push, 1)
struct DatTable {
    void *data;
    uint32_t entry_size;
    uint32_t entries;
};
#pragma pack(pop)

namespace Dat {
    template <class Type>
    const Type &Value(const DatTable *dat, int entry, int offset = 0) {
        Assert(dat->entries > entry + offset);
        return *((const Type *)dat->data + entry + offset);
    }

    inline uint32_t UintValue(const DatTable *dat, int entry, int offset = 0) {
        switch (dat->entry_size) {
            case 1:
                return Value<uint8_t>(dat, entry, offset);
            case 2:
                return Value<uint16_t>(dat, entry, offset);
            case 4:
                return Value<uint32_t>(dat, entry, offset);
            default:
                Assert(false);
                return 0;
        }
    }
}

class OrderType
{
    public:
        OrderType();
        constexpr explicit OrderType(int order_id) : order_id(order_id) { }

        bool UseWeaponTargeting() const { return Value<uint8_t>(1); }
        bool SubunitInheritance() const { return Value<uint8_t>(4); }
        bool Interruptable() const { return Value<uint8_t>(6); }
        bool KeepWaypointSpeed() const { return Value<uint8_t>(7) == 0; }
        bool CanBeQueued() const { return Value<uint8_t>(8); }
        bool TerrainClip() const { return Value<uint8_t>(10); }
        bool Fleeable() const { return Value<uint8_t>(11); }
        WeaponType Weapon() const;
        TechType EnergyTech() const;
        uint16_t Highlight() const { return UintValue(16); }
        OrderType Obscured() const;

        /// Returns false for some orders that never end, so queuing on them doesn't make sense.
        bool CanQueueOn() const;
        bool IsTargetable() const;

        constexpr uint16_t Raw() const { return order_id; }
        constexpr operator uint16_t() const { return Raw(); }

    private:
        uint16_t order_id;

        template <class Type>
        const Type &Value(int index, int offset = 0) const {
            return Dat::Value<Type>(&bw::orders_dat[index], order_id, offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            return Dat::UintValue(&bw::orders_dat[index], order_id, offset);
        }
};

class SoundType
{
    public:
        constexpr explicit SoundType(int sound_id) : sound_id(sound_id) { }

        int16_t PortraitTimeModifier() const { return Value<int16_t>(3); }

        constexpr uint16_t Raw() const { return sound_id; }
        constexpr operator uint16_t() const { return Raw(); }

    private:
        uint16_t sound_id;

        template <class Type>
        const Type &Value(int index, int offset = 0) const {
            return Dat::Value<Type>(&bw::sfxdata_dat[index], sound_id, offset);
        }
};

class TechType
{
    public:
        constexpr explicit TechType(int tech_id) : tech_id(tech_id) { }

        uint32_t EnergyCost() const { return UintValue(3); }
        uint32_t Label() const { return UintValue(7); }

        const char *Name() const;

        constexpr uint16_t Raw() const { return tech_id; }
        constexpr operator uint16_t() const { return Raw(); }

    private:
        uint16_t tech_id;

        template <class Type>
        const Type &Value(int index, int offset = 0) const {
            return Dat::Value<Type>(&bw::techdata_dat[index], tech_id, offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            return Dat::UintValue(&bw::techdata_dat[index], tech_id, offset);
        }
};

class ImageType
{
    public:
        constexpr explicit ImageType(int image_id) : image_id(image_id) { }

        static uint32_t Amount() { return grp_array_size; }

        class Types : public Common::Iterator<Types, ImageType> {
            public:
                constexpr Types(uint32_t beg, uint32_t end_pos) : pos(beg), end_pos(end_pos) { }

                Optional<ImageType> next() {
                    if (pos == end_pos) {
                        return Optional<ImageType>();
                    } else {
                        pos += 1;
                        return Optional<ImageType>(ImageType(pos - 1));
                    }
                }

            private:
                uint32_t pos;
                uint32_t end_pos;
        };
        static Types All() { return Types(0, Amount()); }

        uint32_t GrpId() const { return UintValue(0); }
        bool IsTurningGraphic() const { return Value<uint8_t>(1) & 0x1; }
        bool Clickable() const { return Value<uint8_t>(2) & 0x1; }
        bool UseFullIscript() const { return Value<uint8_t>(3) & 0x1; }
        bool DrawIfCloaked() const { return Value<uint8_t>(4); }
        uint32_t DrawFunc() const { return UintValue(5); }
        uint32_t Remapping() const { return UintValue(6); }
        uint32_t IscriptHeader() const { return UintValue(7); }
        uint32_t DamageOverlay() const { return UintValue(10); }

        int8_t *ShieldOverlay() const;
        int8_t *Overlay(int overlay_type) const;

        GrpSprite *Grp() const {
            Assert(image_id < grp_array_size);
            return grp_array[image_id];
        }

        constexpr uint16_t Raw() const { return image_id; }
        constexpr operator uint16_t() const { return Raw(); }

        /// Sets grp array or guarantees that it hasn't changed in a way we can't handle.
        static void UpdateGrpArray(GrpSprite **array, ImageType image_id) {
            if (image_id.Raw() == 0) {
                grp_array = array;
                InitOverlayArrayPointers();
            } else {
                Assert(grp_array == array);
            }
            if (image_id.Raw() + 1 > grp_array_size) {
                grp_array_size = image_id.Raw() + 1;
            }
        }

    private:
        static GrpSprite **grp_array;
        static uint32_t grp_array_size;
        static int8_t **overlays;
        static int8_t **shield_overlay;

        uint16_t image_id;

        static void InitOverlayArrayPointers() {
            overlays = *bw::image_overlays;
            shield_overlay = *bw::image_shield_overlay;
        }

        template <class Type>
        const Type &Value(int index, int offset = 0) const {
            return Dat::Value<Type>(&bw::images_dat[index], image_id, offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            return Dat::UintValue(&bw::images_dat[index], image_id, offset);
        }
};

class SpriteType
{
    public:
        constexpr explicit SpriteType(int sprite_id) : sprite_id(sprite_id) { }

        ImageType Image() const { return ImageType(UintValue(0)); }
        bool StartAsVisible() const { return Value<uint8_t>(3); }

        constexpr uint16_t Raw() const { return sprite_id; }
        constexpr operator uint16_t() const { return Raw(); }

    private:
        uint16_t sprite_id;

        template <class Type>
        const Type &Value(int index, int offset = 0) const {
            return Dat::Value<Type>(&bw::sprites_dat[index], sprite_id, offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            return Dat::UintValue(&bw::sprites_dat[index], sprite_id, offset);
        }
};

class FlingyType
{
    public:
        constexpr explicit FlingyType(int flingy_id) : flingy_id(flingy_id) { }

        SpriteType Sprite() const { return SpriteType(UintValue(0)); }
        int32_t TopSpeed() const { return Value<int32_t>(1); }
        uint32_t Acceleration() const { return UintValue(2); }
        int32_t HaltDistance() const { return Value<int32_t>(3); }
        uint32_t TurnSpeed() const { return UintValue(4); }
        uint32_t MovementType() const { return UintValue(6); }

        constexpr uint16_t Raw() const { return flingy_id; }

    private:
        uint16_t flingy_id;

        template <class Type>
        const Type &Value(int index, int offset = 0) const {
            return Dat::Value<Type>(&bw::flingy_dat[index], flingy_id, offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            return Dat::UintValue(&bw::flingy_dat[index], flingy_id, offset);
        }
};

class WeaponType
{
    public:
        WeaponType();
        constexpr explicit WeaponType(int weapon_id) : weapon_id(weapon_id) { }

        uint32_t Label() const { return UintValue(0); }
        FlingyType Flingy() const { return FlingyType(UintValue(1)); }
        uint32_t MinRange() const { return UintValue(4); }
        uint32_t MaxRange() const { return UintValue(5); }
        UpgradeType Upgrade() const;
        uint32_t DamageType() const { return UintValue(7); }
        uint32_t Behaviour() const { return UintValue(8); }
        uint32_t DeathTime() const { return UintValue(9); }
        uint32_t Effect() const { return UintValue(10); }
        uint32_t InnerSplash() const { return UintValue(11); }
        uint32_t MiddleSplash() const { return UintValue(12); }
        uint32_t OuterSplash() const { return UintValue(13); }
        uint32_t Damage() const { return UintValue(14); }
        uint32_t UpgradeBonus() const { return UintValue(15); }
        uint32_t Cooldown() const { return UintValue(16); }
        uint32_t AttackAngle() const { return UintValue(18); }
        uint32_t LaunchSpin() const { return UintValue(19); }
        uint32_t OffsetX() const { return UintValue(20); }
        uint32_t ErrorMessage() const { return UintValue(22); }

        bool WorksUnderDisruptionWeb() const;

        constexpr uint16_t Raw() const { return weapon_id; }
        constexpr operator uint16_t() const { return Raw(); }

    private:
        uint16_t weapon_id;

        template <class Type>
        const Type &Value(int index, int offset = 0) const {
            return Dat::Value<Type>(&bw::weapons_dat[index], weapon_id, offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            return Dat::UintValue(&bw::weapons_dat[index], weapon_id, offset);
        }
};

#endif /* DAT_H */
