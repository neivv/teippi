#ifndef DAT_H
#define DAT_H

#include "types.h"

#include "common/assert.h"
#include "offsets.h"

#pragma pack(push, 1)
struct DatTable {
    void *data;
    uint32_t entry_size;
    uint32_t entries;
};
#pragma pack(pop)

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
            const auto &dat = bw::orders_dat[index];
            Assert(dat.entries > order_id + offset);
            return *((Type *)dat.data + order_id + offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            const auto &dat = bw::orders_dat[index];
            switch (dat.entry_size) {
                case 1:
                    return Value<uint8_t>(index, offset);
                case 2:
                    return Value<uint16_t>(index, offset);
                case 4:
                    return Value<uint32_t>(index, offset);
                default:
                    Assert(false);
                    return 0;
            }
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
            const auto &dat = bw::techdata_dat[index];
            Assert(dat.entries > tech_id + offset);
            return *((Type *)dat.data + tech_id + offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            const auto &dat = bw::techdata_dat[index];
            switch (dat.entry_size) {
                case 1:
                    return Value<uint8_t>(index, offset);
                case 2:
                    return Value<uint16_t>(index, offset);
                case 4:
                    return Value<uint32_t>(index, offset);
                default:
                    Assert(false);
                    return 0;
            }
        }
};

class ImageType
{
    public:
        constexpr explicit ImageType(int image_id) : image_id(image_id) { }

        uint32_t Grp() const { return UintValue(0); }
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

        constexpr uint16_t Raw() const { return image_id; }
        constexpr operator uint16_t() const { return Raw(); }

    private:
        uint16_t image_id;

        template <class Type>
        const Type &Value(int index, int offset = 0) const {
            const auto &dat = bw::images_dat[index];
            Assert(dat.entries > image_id + offset);
            return *((Type *)dat.data + image_id + offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            const auto &dat = bw::images_dat[index];
            switch (dat.entry_size) {
                case 1:
                    return Value<uint8_t>(index, offset);
                case 2:
                    return Value<uint16_t>(index, offset);
                case 4:
                    return Value<uint32_t>(index, offset);
                default:
                    Assert(false);
                    return 0;
            }
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
            const auto &dat = bw::sprites_dat[index];
            Assert(dat.entries > sprite_id + offset);
            return *((Type *)dat.data + sprite_id + offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            const auto &dat = bw::sprites_dat[index];
            switch (dat.entry_size) {
                case 1:
                    return Value<uint8_t>(index, offset);
                case 2:
                    return Value<uint16_t>(index, offset);
                case 4:
                    return Value<uint32_t>(index, offset);
                default:
                    Assert(false);
                    return 0;
            }
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
            const auto &dat = bw::flingy_dat[index];
            Assert(dat.entries > flingy_id + offset);
            return *((Type *)dat.data + flingy_id + offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            const auto &dat = bw::flingy_dat[index];
            switch (dat.entry_size) {
                case 1:
                    return Value<uint8_t>(index, offset);
                case 2:
                    return Value<uint16_t>(index, offset);
                case 4:
                    return Value<uint32_t>(index, offset);
                default:
                    Assert(false);
                    return 0;
            }
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
            const auto &dat = bw::weapons_dat[index];
            Assert(dat.entries > weapon_id + offset);
            return *((Type *)dat.data + weapon_id + offset);
        }

        uint32_t UintValue(int index, int offset = 0) const {
            const auto &dat = bw::weapons_dat[index];
            switch (dat.entry_size) {
                case 1:
                    return Value<uint8_t>(index, offset);
                case 2:
                    return Value<uint16_t>(index, offset);
                case 4:
                    return Value<uint32_t>(index, offset);
                default:
                    Assert(false);
                    return 0;
            }
        }
};

#endif /* DAT_H */
