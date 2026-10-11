// =============================================================================
//  Cloak.h
//  - Cataclysm's ship cloak: a charge that drains while cloaked and refills
//    while not, with a random fade in/out (cataclysm.exe 0x00435f10-0x004363b0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___CLOAK_H
#define ___CLOAK_H

#include "SpaceObj.h"
#include "Types.h"

// CAT: new file. Cataclysm's own cloak (bCloakedFighter, bIonArrayFrigate, p1IonArrayFrigate), which replaces HW1's
//      fuel-based cloaked fighter and the cloak-generator field for these ships. Names and addresses from the r5
//      export; the state names are chosen here.

// cloak states (CatCloak.state)
#define CAT_CLOAK_Off           0   // visible; the charge refills
#define CAT_CLOAK_On            1   // fully cloaked (SOF_Cloaked set); the charge drains
#define CAT_CLOAK_FadingOut     2   // decloaking: the fade falls from 1 to 0 after a random delay
#define CAT_CLOAK_FadingIn      3   // cloaking: the fade rises from 0 to 1 after a random delay

#define CAT_CLOAK_MAX_CHARGE    1000.0f     // 0x447a0000 in cat_cloakInit / cat_cloakUpdate
#define CAT_CLOAK_MIN_CHARGE    100.0f      // 0x0072a464: below this, cat_cloakActivate refuses to cloak

// 0x18 bytes, inside the ship specifics (cat_cloakInit 0x00435f10 writes 6 dwords)
typedef struct CatCloak
{
    sdword state;           // +0x00 CAT_CLOAK_*
    real32 charge;          // +0x04 0..CAT_CLOAK_MAX_CHARGE
    real32 chargeRate;      // +0x08 added per update while off (cat_cloakInit 4th argument)
    real32 drainRate;       // +0x0C taken per update while on (cat_cloakInit 3rd argument)
    real32 fadeStartTime;   // +0x10 universe time at which the fade starts moving
    real32 fade;            // +0x14 0 = visible .. 1 = invisible
} CatCloak;
_Static_assert(sizeof(CatCloak) == 0x18, "the cloak block is 0x18 bytes in cataclysm.exe");

// where each cloaking type keeps its cloak and its per-player "detected" flags, as offsets into Ship.ShipSpecifics
// (exe Ship+0x638): bCloakedFighter cloak +0x18 (Ship+0x650), detected +0x30 (Ship+0x668); bIonArrayFrigate and
// p1IonArrayFrigate cloak +0x00 (Ship+0x638), detected +0x18 (Ship+0x650) (cat_cloakActivate 0x00435f50,
// sub_53F9F0 0x0053f9f0, cat_shipStealthDetectedSet 0x0053fad0)
#define CAT_CLOAK_SPECOFS_CLOAKEDFIGHTER        0x18
#define CAT_DETECTED_SPECOFS_CLOAKEDFIGHTER     0x30
#define CAT_CLOAK_SPECOFS_IONARRAYFRIGATE       0x00
#define CAT_DETECTED_SPECOFS_IONARRAYFRIGATE    0x18

CatCloak *cat_cloakGet(Ship *ship);
bool32 *cat_cloakDetectedFlags(Ship *ship);

void cat_cloakInit(Ship *ship, CatCloak *cloak, real32 drainRate, real32 chargeRate);
bool32 cat_cloakActivate(Ship *ship);
bool32 cat_shipIsCloakedFully(Ship *ship);
bool32 cat_cloakGetFadeState(Ship *ship, real32 *fade);
real32 cat_cloakGetChargeFraction(Ship *ship);
void cat_cloakUpdate(Ship *ship);

double cat_randomExpDelay(void);                     // CAT: inlined in the exe (M2-R4 ships-b)

#endif
