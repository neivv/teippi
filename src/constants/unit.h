#ifndef CONSTANTS_UNIT_H
#define CONSTANTS_UNIT_H

#include "../unit_type.h"

// From BWAPI sources: http://code.google.com/p/bwapi/source/browse/trunk/bwapi/BWAPI/Source/BW/UnitID.h
namespace UnitId {
    constexpr UnitType Marine(0x00);
    constexpr UnitType Ghost(0x01);
    constexpr UnitType Vulture(0x02);
    constexpr UnitType Goliath(0x03);
    constexpr UnitType GoliathTurret(0x04);
    constexpr UnitType SiegeTankTankMode(0x05);
    constexpr UnitType TankTurretTankMode(0x06);
    constexpr UnitType SCV(0x07);
    constexpr UnitType Wraith(0x08);
    constexpr UnitType ScienceVessel(0x09);
    constexpr UnitType GuiMontag(0x0A);
    constexpr UnitType Dropship(0x0B);
    constexpr UnitType Battlecruiser(0x0C);
    constexpr UnitType SpiderMine(0x0D);
    constexpr UnitType NuclearMissile(0x0E);
    constexpr UnitType Civilian(0x0F);
    constexpr UnitType SarahKerrigan(0x10);
    constexpr UnitType AlanSchezar(0x11);
    constexpr UnitType AlanTurret(0x12);
    constexpr UnitType JimRaynorV(0x13);
    constexpr UnitType JimRaynorM(0x14);
    constexpr UnitType TomKazansky(0x15);
    constexpr UnitType Magellan(0x16);
    constexpr UnitType EdmundDukeT(0x17);
    constexpr UnitType EdmundDukeTTurret(0x18);
    constexpr UnitType EdmundDukeS(0x19);
    constexpr UnitType EdmundDukeSTurret(0x1A);
    constexpr UnitType ArcturusMengsk(0x1B);
    constexpr UnitType Hyperion(0x1C);
    constexpr UnitType NoradII(0x1D);
    constexpr UnitType SiegeTank_Sieged(0x1E);
    constexpr UnitType SiegeTankTurret_Sieged(0x1F);
    constexpr UnitType Firebat(0x20);
    constexpr UnitType ScannerSweep(0x21);
    constexpr UnitType Medic(0x22);
    constexpr UnitType Larva(0x23);
    constexpr UnitType Egg(0x24);
    constexpr UnitType Zergling(0x25);
    constexpr UnitType Hydralisk(0x26);
    constexpr UnitType Ultralisk(0x27);
    constexpr UnitType Broodling(0x28);
    constexpr UnitType Drone(0x29);
    constexpr UnitType Overlord(0x2A);
    constexpr UnitType Mutalisk(0x2B);
    constexpr UnitType Guardian(0x2C);
    constexpr UnitType Queen(0x2D);
    constexpr UnitType Defiler(0x2E);
    constexpr UnitType Scourge(0x2F);
    constexpr UnitType Torrasque(0x30);
    constexpr UnitType Matriarch(0x31);
    constexpr UnitType InfestedTerran(0x32);
    constexpr UnitType InfestedKerrigan(0x33);
    constexpr UnitType UncleanOne(0x34);
    constexpr UnitType HunterKiller(0x35);
    constexpr UnitType DevouringOne(0x36);
    constexpr UnitType KukulzaMutalisk(0x37);
    constexpr UnitType KukulzaGuardian(0x38);
    constexpr UnitType Yggdrasill(0x39);
    constexpr UnitType Valkyrie(0x3A);
    constexpr UnitType Cocoon(0x3B);
    constexpr UnitType Corsair(0x3C);
    constexpr UnitType DarkTemplar(0x3D);
    constexpr UnitType Devourer(0x3E);
    constexpr UnitType DarkArchon(0x3F);
    constexpr UnitType Probe(0x40);
    constexpr UnitType Zealot(0x41);
    constexpr UnitType Dragoon(0x42);
    constexpr UnitType HighTemplar(0x43);
    constexpr UnitType Archon(0x44);
    constexpr UnitType Shuttle(0x45);
    constexpr UnitType Scout(0x46);
    constexpr UnitType Arbiter(0x47);
    constexpr UnitType Carrier(0x48);
    constexpr UnitType Interceptor(0x49);
    constexpr UnitType DarkTemplarHero(0x4A);
    constexpr UnitType Zeratul(0x4B);
    constexpr UnitType TassadarZeratul(0x4C);
    constexpr UnitType FenixZealot(0x4D);
    constexpr UnitType FenixDragoon(0x4E);
    constexpr UnitType Tassadar(0x4F);
    constexpr UnitType Mojo(0x50);
    constexpr UnitType Warbringer(0x51);
    constexpr UnitType Gantrithor(0x52);
    constexpr UnitType Reaver(0x53);
    constexpr UnitType Observer(0x54);
    constexpr UnitType Scarab(0x55);
    constexpr UnitType Danimoth(0x56);
    constexpr UnitType Aldaris(0x57);
    constexpr UnitType Artanis(0x58);
    constexpr UnitType Rhynadon(0x59);
    constexpr UnitType Bengalaas(0x5A);
    constexpr UnitType Unused_CargoShip(0x5B);
    constexpr UnitType Unused_MercenaryGunship(0x5C);
    constexpr UnitType Scantid(0x5D);
    constexpr UnitType Kakaru(0x5E);
    constexpr UnitType Ragnasaur(0x5F);
    constexpr UnitType Ursadon(0x60);
    constexpr UnitType LurkerEgg(0x61);
    constexpr UnitType Raszagal(0x62);
    constexpr UnitType SamirDuran(0x63);
    constexpr UnitType AlexeiStukov(0x64);
    constexpr UnitType MapRevealer(0x65);
    constexpr UnitType GerardDuGalle(0x66);
    constexpr UnitType Lurker(0x67);
    constexpr UnitType InfestedDuran(0x68);
    constexpr UnitType DisruptionWeb(0x69);
    constexpr UnitType CommandCenter(0x6A);
    constexpr UnitType ComsatStation(0x6B);
    constexpr UnitType NuclearSilo(0x6C);
    constexpr UnitType SupplyDepot(0x6D);
    constexpr UnitType Refinery(0x6E);
    constexpr UnitType Barracks(0x6F);
    constexpr UnitType Academy(0x70);
    constexpr UnitType Factory(0x71);
    constexpr UnitType Starport(0x72);
    constexpr UnitType ControlTower(0x73);
    constexpr UnitType ScienceFacility(0x74);
    constexpr UnitType CovertOps(0x75);
    constexpr UnitType PhysicsLab(0x76);
    constexpr UnitType Unused_Starbase(0x77);
    constexpr UnitType MachineShop(0x78);
    constexpr UnitType Unused_RepairBay(0x79);
    constexpr UnitType EngineeringBay(0x7A);
    constexpr UnitType Armory(0x7B);
    constexpr UnitType MissileTurret(0x7C);
    constexpr UnitType Bunker(0x7D);
    constexpr UnitType CrashedNoradII(0x7E);
    constexpr UnitType IonCannon(0x7F);
    constexpr UnitType UrajCrystal(0x80);
    constexpr UnitType KhalisCrystal(0x81);
    constexpr UnitType InfestedCommandCenter(0x82);
    constexpr UnitType Hatchery(0x83);
    constexpr UnitType Lair(0x84);
    constexpr UnitType Hive(0x85);
    constexpr UnitType NydusCanal(0x86);
    constexpr UnitType HydraliskDen(0x87);
    constexpr UnitType DefilerMound(0x88);
    constexpr UnitType GreaterSpire(0x89);
    constexpr UnitType QueensNest(0x8A);
    constexpr UnitType EvolutionChamber(0x8B);
    constexpr UnitType UltraliskCavern(0x8C);
    constexpr UnitType Spire(0x8D);
    constexpr UnitType SpawningPool(0x8E);
    constexpr UnitType CreepColony(0x8F);
    constexpr UnitType SporeColony(0x90);
    constexpr UnitType Unused_ZergBuilding1(0x91);
    constexpr UnitType SunkenColony(0x92);
    constexpr UnitType OvermindWithShell(0x93);
    constexpr UnitType Overmind(0x94);
    constexpr UnitType Extractor(0x95);
    constexpr UnitType MatureChrysalis(0x96);
    constexpr UnitType Cerebrate(0x97);
    constexpr UnitType CerebrateDaggoth(0x98);
    constexpr UnitType Unused_ZergBuilding2(0x99);
    constexpr UnitType Nexus(0x9A);
    constexpr UnitType RoboticsFacility(0x9B);
    constexpr UnitType Pylon(0x9C);
    constexpr UnitType Assimilator(0x9D);
    constexpr UnitType Unused_ProtossBuilding1(0x9E);
    constexpr UnitType Observatory(0x9F);
    constexpr UnitType Gateway(0xA0);
    constexpr UnitType Unused_ProtossBuilding2(0xA1);
    constexpr UnitType PhotonCannon(0xA2);
    constexpr UnitType CitadelOfAdun(0xA3);
    constexpr UnitType CyberneticsCore(0xA4);
    constexpr UnitType TemplarArchives(0xA5);
    constexpr UnitType Forge(0xA6);
    constexpr UnitType Stargate(0xA7);
    constexpr UnitType StasisCellPrison(0xA8);
    constexpr UnitType FleetBeacon(0xA9);
    constexpr UnitType ArbiterTribunal(0xAA);
    constexpr UnitType RoboticsSupportBay(0xAB);
    constexpr UnitType ShieldBattery(0xAC);
    constexpr UnitType KhaydarinCrystalForm(0xAD);
    constexpr UnitType ProtossTemple(0xAE);
    constexpr UnitType XelNagaTemple(0xAF);
    constexpr UnitType MineralPatch1(0xB0);
    constexpr UnitType MineralPatch2(0xB1);
    constexpr UnitType MineralPatch3(0xB2);
    constexpr UnitType Unused_Cave(0xB3);
    constexpr UnitType Unused_CaveIn(0xB4);
    constexpr UnitType Unused_Cantina(0xB5);
    constexpr UnitType Unused_MiningPlatform(0xB6);
    constexpr UnitType Unused_IndependentCC(0xB7);
    constexpr UnitType Unused_IndependentStarport(0xB8);
    constexpr UnitType Unused_IndependentJumpGate(0xB9);
    constexpr UnitType Unused_Ruins(0xBA);
    constexpr UnitType Unused_KhaydarinFormation(0xBB);
    constexpr UnitType VespeneGeyser(0xBC);
    constexpr UnitType WarpGate(0xBD);
    constexpr UnitType PsiDisrupter(0xBE);
    constexpr UnitType Unused_ZergMarker(0xBF);
    constexpr UnitType Unused_TerranMarker(0xC0);
    constexpr UnitType Unused_ProtossMarker(0xC1);
    constexpr UnitType ZergBeacon(0xC2);
    constexpr UnitType TerranBeacon(0xC3);
    constexpr UnitType ProtossBeacon(0xC4);
    constexpr UnitType ZergFlagBeacon(0xC5);
    constexpr UnitType TerranFlagBeacon(0xC6);
    constexpr UnitType ProtossFlagBeacon(0xC7);
    constexpr UnitType PowerGenerator(0xC8);
    constexpr UnitType OvermindCocoon(0xC9);
    constexpr UnitType DarkSwarm(0xCA);
    constexpr UnitType FloorMissileTrap(0xCB);
    constexpr UnitType FloorHatch(0xCC);
    constexpr UnitType LeftUpperLevelDoor(0xCD);
    constexpr UnitType RightUpperLevelDoor(0xCE);
    constexpr UnitType LeftPitDoor(0xCF);
    constexpr UnitType RightPitDoor(0xD0);
    constexpr UnitType FloorGunTrap(0xD1);
    constexpr UnitType LeftWallMissileTrap(0xD2);
    constexpr UnitType LeftWallFlameTrap(0xD3);
    constexpr UnitType RightWallMissileTrap(0xD4);
    constexpr UnitType RightWallFlameTrap(0xD5);
    constexpr UnitType Start_Location(0xD6);
    constexpr UnitType Flag(0xD7);
    constexpr UnitType YoungChrysalis(0xD8);
    constexpr UnitType PsiEmitter(0xD9);
    constexpr UnitType DataDisk(0xDA);
    constexpr UnitType KhaydarinCrystal(0xDB);
    constexpr UnitType Mineral_Chunk1(0xDC);
    constexpr UnitType Mineral_Chunk2(0xDD);
    constexpr UnitType Vespene_Orb1(0xDE);
    constexpr UnitType Vespene_Orb2(0xDF);
    constexpr UnitType Vaspene_Sac1(0xE0);
    constexpr UnitType Vaspene_Sac2(0xE1);
    constexpr UnitType Vespene_Tank1(0xE2);
    constexpr UnitType Vespene_Tank2(0xE3);
    constexpr UnitType None(0xE4);

    // Used by triggers
    constexpr UnitType Trigger_Any(0xE5);
    constexpr UnitType Trigger_Men(0xE6);
    constexpr UnitType Trigger_Buildings(0xE7);
    constexpr UnitType Trigger_Factories(0xE8);
}

#endif
