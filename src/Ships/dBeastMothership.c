// =============================================================================
//  dBeastMothership.c
//  - Cataclysm's derelict dBeastMothership: the Beast mothership as a mission
//    object, with the infection weapon (cataclysm.exe 0x005b00d0-0x005b0260)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Every body is "if (the ship has the Beast weapon) call the weapon's function"
//      (addresses on each).

#include "dBeastMothership.h"

#include "DefaultShip.h"
#include "Mothership.h"
#include "Universe.h"

static udword dBeastMothershipStatic;   // 0x00ab73a4: a 4-byte block nothing reads

void dBeastMothershipStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)  // 0x005b00d0
{
    (void)directory;
    (void)filename;
    statinfo->custstatinfo = &dBeastMothershipStatic;
    if (cat_staticHasBeastWeapon(statinfo))
    {
        cat_beastWeaponStaticInit();
    }
}

void dBeastMothershipInit(Ship *ship)                                                       // 0x005b0100
{
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponInit(ship);
    }
    // it starts stunned: 1 s from now for 2.5 s, with no spin
    if (ship->shiptype == dBeastMothership)
    {
        ship->catBeastSpin = -1.0f;
        ship->catBeastStunState = 1;
        ship->catBeastStunStart = universe.totaltimeelapsed + 1.0f;
        ship->catBeastStunEnd = ship->catBeastStunStart + 2.5f;    // 0x0072a5b0
    }
}

void dBeastMothershipDied(Ship *ship)                                                       // 0x005b0160
{
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponClose(ship);
    }
}

void dBeastMothershipRemoveShipReferences(Ship *ship,Ship *shiptoremove)                    // 0x005b0180
{
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponRemoveShipReferences(ship, shiptoremove);
    }
}

void dBeastMothershipBulletRemoval(Ship *ship,struct Bullet *bullet)                        // 0x005b01b0
{
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponBulletRemoval(ship, NULL, (Bullet *)bullet);
    }
}

void dBeastMothershipHouseKeep(Ship *ship)                                                  // 0x005b01e0
{
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponHouseKeep(ship);
    }
}

void dBeastMothership_Save(Ship *ship)                                                      // 0x005b0200
{
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponSave(ship);
    }
}

void dBeastMothership_Load(Ship *ship)                                                      // 0x005b0220
{
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponLoad(ship);
    }
}

void dBeastMothership_Fix(Ship *ship)                                                       // 0x005b0240
{
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponFix(ship);
    }
}

// CAT: dBeastMothershipHeader 0x008c3920 (dBeastMothership); slots 8 and 9 are Mothership's, slot 12 the slot-11
//      body as in every header that has it
CustShipHeader dBeastMothershipHeader =
{
    .sizeofShipSpecifics = sizeof(dBeastMothershipSpec),
    .CustShipStaticInit = dBeastMothershipStaticInit,
    .CustShipInit = dBeastMothershipInit,
    .CustShipFire = DefaultShipFire,
    .CustShipSpecialTarget = MothershipSpecialTarget,
    .CustShipOrderChanged = MothershipOrderChanged,
    .CustShipHousekeep = dBeastMothershipHouseKeep,
    .CustShipRemoveShipReferences = dBeastMothershipRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))dBeastMothershipRemoveShipReferences,
    .CustShipBulletRemoval = dBeastMothershipBulletRemoval,
    .CustShipDied = dBeastMothershipDied,
    .CustShip_Save = dBeastMothership_Save,
    .CustShip_Load = dBeastMothership_Load,
    .CustShip_Fix = dBeastMothership_Fix,
};
