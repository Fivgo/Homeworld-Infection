// =============================================================================
//  Mothership.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#ifndef ___MOTHERSHIP_H
#define ___MOTHERSHIP_H

#include "BeastWeapon.h"
#include "Salvage.h"
#include "SpaceObj.h"
#include "Timer.h"
#include "Types.h"

// CAT: the Sect mothership keeps 7 module slots, each a module ship type and a count (MothershipInit 0x005bb840
//      fills them for sMothership; cat_mothershipModuleSlotGet 0x005bd670 searches them)
#define CAT_MOTHERSHIP_MODULE_SLOTS     7
typedef struct CatModuleSlot
{
    sdword moduleType;
    sdword count;
} CatModuleSlot;

// CAT: 0x1F0 bytes in cataclysm.exe (MothershipHeader sizeofShipSpecifics 496), shared by sMothership, bMothership and
//      trGhostship; HW1's door cargo, drop targets and captured-ship build queue are gone (salvage goes to the depot
//      block). Offsets are of the specifics (exe Ship+0x638); field names are chosen here from their users.
typedef struct
{
    CatBeastWeaponSpec beastWeapon;     // +0x00 bMothership's infection weapon (BeastWeapon.c)
    sdword beastSystemState;            // +0x18 bMothership: 0 off, 1 on, 2 on and draining the beast guns (beastMothershipSelfDamage 0x005bc900)
    bool32 beastSystemActive;           // +0x1C bMothership
    vector bigGunTarget;                // +0x20 sMothership big gun: the aim point (SectMothershipHouseKeep 0x005bc430)
    vector bigGunFirePosition;          // +0x2C sMothership big gun: where the mothership stood when it fired
    struct Bullet *bigGunBullet;        // +0x38 sMothership big gun: the shell in flight
    ubyte bigGunState;                  // +0x3C sMothership big gun: CAT_BIGGUN_*
    ubyte pad3D[3];
    real32 bigGunChargeStartTime;       // +0x40
    udword unknown44;                   // +0x44 cleared by MothershipInit
    struct Effect *bigGunChargeEffect;  // +0x48 (MothershipEffectRemoval 0x005bce80 clears it)
    ubyte bigGunChargeEffectState;      // +0x4C 1 charging, 2 released
    ubyte pad4D[3];
    bool32 bigGunAimTimerRunning;       // +0x50
    CatTimer bigGunAimTimer;            // +0x54
    CatModuleSlot moduleSlots[CAT_MOTHERSHIP_MODULE_SLOTS];    // +0x64 sMothership
    ubyte deathState;                   // +0x9C 0 alive, 1 dying, 2 exploding (cat_mothershipStartDeathSequence 0x005bd5f0)
    ubyte pad9D[3];
    real32 deathExplodeTime;            // +0xA0
    real32 deathDeleteTime;             // +0xA4
    sdword deathReason;                 // +0xA8 index into the death explosion table (0x0087ac64)
    CatSalvageDepot depot;              // +0xAC every mothership is a salvage depot (Salvage.c)
} MothershipSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(MothershipSpec, bigGunState) == 0x3C, "big gun state at Ship+0x674 in cataclysm.exe");
_Static_assert(offsetof(MothershipSpec, moduleSlots) == 0x64, "module slots at Ship+0x69C in cataclysm.exe");
_Static_assert(offsetof(MothershipSpec, deathState) == 0x9C, "death state at Ship+0x6D4 in cataclysm.exe");
_Static_assert(offsetof(MothershipSpec, depot) == 0xAC, "salvage depot at Ship+0x6E4 in cataclysm.exe");
_Static_assert(sizeof(MothershipSpec) == 0x1F0, "MothershipSpec is 0x1F0 bytes in cataclysm.exe");
#endif

// CAT: the big gun states (MothershipAttack 0x005bbc50, SectMothershipHouseKeep 0x005bc430)
#define CAT_BIGGUN_Idle         0
#define CAT_BIGGUN_Aiming       1
#define CAT_BIGGUN_Firing       2
#define CAT_BIGGUN_Stabilizing  3
#define CAT_BIGGUN_Charging     4
#define CAT_BIGGUN_Scattering   5

// CAT: the statics are the two range arrays only (MothershipStaticInit 0x005bb730: one block, 0x00a7a9c0); HW1's
//      special-door offsets and their key table are gone
typedef struct
{
    real32 mothershipGunRange[NUM_TACTICS_TYPES];
    real32 mothershipTooCloseRange[NUM_TACTICS_TYPES];
} MothershipStatics;

extern CustShipHeader MothershipHeader;

// CAT: the depot slots the exe shares with CarrierHeader / HeavyCruiserHeader / P1MothershipHeader
void Mothership_PreFix(Ship *ship);
void Mothership_Save(Ship *ship);
void Mothership_Load(Ship *ship);
bool32 MothershipSpecialTarget(Ship *ship, void *custom);
void MothershipOrderChanged(Ship *ship);

// CAT: Cataclysm mothership queries
Gun *cat_mothershipGetBigGun(Ship *ship);
bool32 cat_mothershipBigGunFiring(Ship *ship);
bool32 cat_mothershipHasBigGunModule(Ship *ship);
void cat_mothershipStartDeathSequence(Ship *ship, sdword reason);
sdword cat_mothershipModuleSlotGet(Ship *mothership, sdword moduleType);
bool32 cat_isBigGunBullet(SpaceObj *obj);

// HW1 door-cargo helpers: Cataclysm has no door cargo; kept, inert, for their unported HW1 callers (see Mothership.c)
void MothershipAttachObjectToDoor(Ship *ship,ShipOrDerelict *object);
void mothershipCleanDoorForHSInstant(Ship *ship);
void MothershipDettachObjectFromDoor(Ship *ship);
void MothershipDoorUpKeep(Ship *ship);
void mothershipGetCargoPosition(Ship *ship,ShipOrDerelict *cargo, vector *position, matrix *coordsys, vector *heading, vector *up,vector *right);

#endif
