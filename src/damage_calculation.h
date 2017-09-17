#ifndef DAMAGE_CALCULATION_H
#define DAMAGE_CALCULATION_H

#include <algorithm>

typedef int32_t damage_int;

/// Contains the logic for damage calculation. It is meant to be used by having
/// global/const default object, which is copied and modified with inputs
/// (hp, shields, damage, armor, etc) specific to the hit being calculated.
/// Default values are ones used normally by bw. The returned results are in
/// struct Result, received from Calculate().
///
/// All values are in internal hp (256 times the displayed value). Also while bw
/// has ignore armor hardcoded to damage type 4, it must be specified here separately.
///
/// (All methods could be constexpr in c++14)
class DamageCalculation
{
    public:
        struct Result {
            constexpr Result(damage_int hp, damage_int shield, damage_int matrix) : hp_damage(hp),
                shield_damage(shield), matrix_damage(matrix) { }
            damage_int hp_damage;
            damage_int shield_damage;
            damage_int matrix_damage;
        };
        constexpr DamageCalculation() : hitpoints(0), shields(0), matrix_hp(0), base_damage(0),
            acid_spore_bonus(256), armor_reduction(0), shield_reduction(0),
            hallucination_multiplier(512), acid_spores(0), armor_type(0), damage_type(0),
            hallucination(false), ignore_armor(false) { }

#define SET_METHOD(name, type, var) DamageCalculation name(type val) const {\
    DamageCalculation ret(*this);\
    ret.var = val;\
    return ret;\
}
        SET_METHOD(HitPoints, damage_int, hitpoints)
        SET_METHOD(Shields, damage_int, shields)
        SET_METHOD(MatrixHp, damage_int, matrix_hp)
        SET_METHOD(BaseDamage, damage_int, base_damage)
        SET_METHOD(ArmorType, uint8_t, armor_type)
        SET_METHOD(DamageType, uint8_t, damage_type)
        SET_METHOD(Hallucination, bool, hallucination)
        SET_METHOD(IgnoreArmor, bool, ignore_armor)
        /// 256 == 1x, 512 == 2x, 128 == 0.5x etc.
        SET_METHOD(HallucinationMultiplier, int, hallucination_multiplier)
        SET_METHOD(AcidSpores, int, acid_spores)
        /// Additional damage per acid spore
        SET_METHOD(AcidSporeBonus, damage_int, acid_spore_bonus)
        SET_METHOD(ArmorReduction, damage_int, armor_reduction)
        SET_METHOD(ShieldReduction, damage_int, shield_reduction)
#undef SET_METHOD

        Result Calculate() const {
            auto damage = base_damage;
            if (hallucination)
                damage = damage * hallucination_multiplier / 256;
            damage += acid_spores * acid_spore_bonus;

            int matrix_damage = std::min(matrix_hp, damage);
            damage -= matrix_damage;

            auto shield_damage = damage;
            if (shields < 256) {
                shield_damage = 0;
            } else {
                if (!ignore_armor)
                    shield_damage = std::max(128, shield_damage - shield_reduction);
                if (shield_damage <= shields) {
                    damage = 0;
                } else {
                    shield_damage = std::min(shield_damage, shields);
                    damage -= shield_damage;
                }
            }

            if (!ignore_armor)
            {
                if (damage > armor_reduction)
                    damage -= armor_reduction;
                else
                    damage = 0;
            }

            if (shield_damage == 0 && damage == 0)
                damage = 128;

            const int multiplier[5][4] = {
                // Armor types are independent, small, medium, large;
                // Weapon types are independent, explosive, concussive, normal, ignore armor
                { 0, 0, 0, 0 },
                { 0, 0x80, 0xc0, 0x100 },
                { 0, 0x100, 0x80, 0x40 },
                { 0, 0x100, 0x100, 0x100 },
                { 0, 0x100, 0x100, 0x100 },
            };
            damage = damage * multiplier[damage_type][armor_type] / 256;
            return Result(damage, shield_damage, matrix_damage);
        }

    private:
        damage_int hitpoints;
        damage_int shields;
        damage_int matrix_hp;
        damage_int base_damage;
        damage_int acid_spore_bonus;
        damage_int armor_reduction;
        damage_int shield_reduction;
        int hallucination_multiplier;
        int acid_spores;
        uint8_t armor_type;
        uint8_t damage_type;
        bool hallucination;
        bool ignore_armor;
};



#endif /* DAMAGE_CALCULATION_H */
