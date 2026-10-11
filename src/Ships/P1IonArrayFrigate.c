// =============================================================================
//  P1IonArrayFrigate.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/06/1998 by ddunlop
// =============================================================================

#include "P1IonArrayFrigate.h"

#include "Attack.h"
#include "Cloak.h"
#include "Debug.h"
#include "DefaultShip.h"
#include "GenericInterceptor.h"
#include "MaxMultiplayer.h"
#include "SinglePlayer.h"

// CAT: 0x38 bytes in cataclysm.exe (P1IonArrayFrigateHeader sizeofShipSpecifics 56): Cataclysm's cloak block
//      (P1IonArrayFrigateInit 0x005be200 -> cat_cloakInit(ship, ship+0x638)) and the per-player detected flags at
//      +0x18 (sub_53F9F0 0x0053f9f0 reads Ship+0x650); bIonArrayFrigate and p1IonArrayFrigate cloak
typedef struct
{
    CatCloak cloak;                             // +0x00
    bool32 detected[MAX_MULTIPLAYER_PLAYERS];   // +0x18
} P1IonArrayFrigateSpec;
_Static_assert(offsetof(P1IonArrayFrigateSpec, cloak) == CAT_CLOAK_SPECOFS_IONARRAYFRIGATE, "cloak at specifics +0x00");
_Static_assert(offsetof(P1IonArrayFrigateSpec, detected) == CAT_DETECTED_SPECOFS_IONARRAYFRIGATE, "detected at +0x18");
_Static_assert(sizeof(P1IonArrayFrigateSpec) == 0x38, "P1IonArrayFrigateSpec is 0x38 bytes in cataclysm.exe");

typedef struct
{
    real32 frigateGunRange[NUM_TACTICS_TYPES];
    real32 frigateTooCloseRange[NUM_TACTICS_TYPES];
} P1IonArrayFrigateStatics;

// CAT: one block per race that has the header: Beast (0x00a7a940: bIonArrayFrigate), P1 (0x00a7a900), Derelict
//      (0x00a7a920) (P1IonArrayFrigateStaticInit 0x005be1a0); HW1 had one block
P1IonArrayFrigateStatics P1IonArrayFrigateStaticBeast;
P1IonArrayFrigateStatics P1IonArrayFrigateStatic;
P1IonArrayFrigateStatics P1IonArrayFrigateStaticDerelict;
P1IonArrayFrigateStatics P1IonArrayFrigateStaticOther;  // CAT: the exe writes into the statinfo itself for any other race

void P1IonArrayFrigateStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    udword i;
    P1IonArrayFrigateStatics *frigstat;

    // CAT: by Cataclysm race (P1IonArrayFrigateStaticInit 0x005be1a0)
    switch (statinfo->shiprace)
    {
        case RACE_Beast:    frigstat = &P1IonArrayFrigateStaticBeast;    break;
        case RACE_P1:       frigstat = &P1IonArrayFrigateStatic;         break;
        case RACE_Derelict: frigstat = &P1IonArrayFrigateStaticDerelict; break;
        default:
            dbgAssertOrIgnore(FALSE);
            frigstat = &P1IonArrayFrigateStaticOther;
            break;
    }

    statinfo->custstatinfo = frigstat;

    for(i=0;i<NUM_TACTICS_TYPES;i++)
    {
        frigstat->frigateGunRange[i] = statinfo->bulletRange[i];
        frigstat->frigateTooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }
}

void P1IonArrayFrigateInit(Ship *ship)
{
    // CAT: new slot-3 body (P1IonArrayFrigateInit 0x005be200): Cataclysm's cloak, drain and charge rate 0.1
    P1IonArrayFrigateSpec *spec = (P1IonArrayFrigateSpec *)ship->ShipSpecifics;

    cat_cloakInit(ship, &spec->cloak, 0.1f, 0.1f);
}

void P1IonArrayFrigateAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    P1IonArrayFrigateStatics *frigstat = (P1IonArrayFrigateStatics *)shipstaticinfo->custstatinfo;

    // CAT: attack only while visible; a fully cloaked frigate that is told to attack decloaks first, except in
    //      single player, where it stays cloaked (P1IonArrayFrigateAttack 0x005be220)
    if (!cat_shipIsCloakedFully(ship))
    {
        attackStraightForward(ship,target,frigstat->frigateGunRange[ship->tacticstype],frigstat->frigateTooCloseRange[ship->tacticstype]);
        return;
    }
    if (!singlePlayerGame)
    {
        cat_cloakActivate(ship);
    }
}

void P1IonArrayFrigateAttackPassive(Ship *ship,Ship *target,bool32 rotate)
{
    // CAT: the same cloak rule as P1IonArrayFrigateAttack (P1IonArrayFrigateAttackPassive 0x005be280)
    if (cat_shipIsCloakedFully(ship))
    {
        if (!singlePlayerGame)
        {
            cat_cloakActivate(ship);
        }
        return;
    }
    if ((rotate) & ((bool32)((ShipStaticInfo *)(ship->staticinfo))->rotateToRetaliate))
    {
        attackPassiveRotate(ship,target);
    }
    else
    {
        attackPassive(ship,target);
    }
}

// CAT: the exe's header (P1IonArrayFrigateHeader 0x008c5808: bIonArrayFrigate, p1IonArrayFrigate): the cloak
//      slots 7 and 10 are the cloaked fighter's (CloakedFighterSpecialActivate / CloakedFighterHouseKeep).
CustShipHeader P1IonArrayFrigateHeader =
{
    .sizeofShipSpecifics = sizeof(P1IonArrayFrigateSpec),
    .CustShipStaticInit = P1IonArrayFrigateStaticInit,
    .CustShipInit = P1IonArrayFrigateInit,                   // CAT: new
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = P1IonArrayFrigateAttackPassive,
    .CustShipSpecialActivate = CloakedFighterSpecialActivate, // CAT: new
    .CustShipHousekeep = CloakedFighterHouseKeep,            // CAT: new
    .CustShipAttack = P1IonArrayFrigateAttack,
};

