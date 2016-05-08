#ifndef WEAPON_H
#define WEAPON_H

#include "../dat.h"

namespace WeaponId
{
    constexpr WeaponType SpiderMine(0x06);
    constexpr WeaponType Lockdown(0x20);
    constexpr WeaponType EmpShockwave(0x21);
    constexpr WeaponType Irradiate(0x22);
    constexpr WeaponType Venom(0x32);
    constexpr WeaponType HeroVenom(0x33);
    constexpr WeaponType Suicide(0x36);
    constexpr WeaponType Parasite(0x38);
    constexpr WeaponType SpawnBroodlings(0x39);
    constexpr WeaponType Ensnare(0x3a);
    constexpr WeaponType DarkSwarm(0x3b);
    constexpr WeaponType Plague(0x3c);
    constexpr WeaponType Consume(0x3d);
    constexpr WeaponType PsiAssault(0x44);
    constexpr WeaponType HeroPsiAssault(0x45);
    constexpr WeaponType Scarab(0x52);
    constexpr WeaponType StasisField(0x53);
    constexpr WeaponType PsiStorm(0x54);
    constexpr WeaponType Restoration(0x66);
    constexpr WeaponType MindControl(0x69);
    constexpr WeaponType Feedback(0x6a);
    constexpr WeaponType OpticalFlare(0x6b);
    constexpr WeaponType Maelstrom(0x6c);
    constexpr WeaponType SubterraneanSpines(0x6d);
    constexpr WeaponType None(0x82);
}
#endif /* WEAPON_H */
