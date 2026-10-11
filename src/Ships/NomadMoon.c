// =============================================================================
//  NomadMoon.c
//  - Cataclysm's dNomadMoon and its two dNomadFieldEmitter ships: a mission
//    object that sends out a shock wave while big-gun shells fly
//    (cataclysm.exe 0x005bdbb0-0x005be1a0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each). The
//      charging beam is drawn by cat_nomadMoonRenderBeam 0x005be120 (cat_nomadMoonBeamEffect, the renderer: stage 3).

#include "NomadMoon.h"

#include "Attack.h"
#include "Blast.h"
#include "DefaultShip.h"
#include "NIS.h"
#include "SaveGame.h"
#include "SoundEvent.h"
#include "UnivUpdate.h"
#include "Universe.h"

#define NOMADMOON_EMITTER_OFFSET    3925.0f     // 0x008c56d8: the emitters sit this far above and below the moon
#define NOMADMOON_CHARGE_TIME       6.0f        // 0x008c56dc
#define NOMADMOON_CHARGE_TIME_NIS   5.0f        // 0x008c56e0: while an NIS runs
#define NOMADMOON_COOLDOWN_TIME     11.0f       // 0x008c56e4

// the number of emitters left (the exe inlines cat_nomadMoonEmitterCount everywhere)
static sdword nomadMoonEmitters(NomadMoonSpec *spec)
{
    return (spec->emitter[0] != NULL ? 1 : 0) + (spec->emitter[1] != NULL ? 1 : 0);
}

// start charging (the exe inlines this in cat_nomadMoonFire, NomadMoonHouseKeep and NomadMoonFire)
static void nomadMoonStartCharge(NomadMoonSpec *spec)
{
    cat_timerStartGameTime(&spec->fieldTimer, (nisIsRunning == 1) ? NOMADMOON_CHARGE_TIME_NIS : NOMADMOON_CHARGE_TIME);
    spec->fieldState = 1;
}

void NomadMoonInit(Ship *ship)                                                              // 0x005bdbb0
{
    NomadMoonSpec *spec = (NomadMoonSpec *)ship->ShipSpecifics;
    vector pos;

    spec->emitter[0] = NULL;
    spec->emitter[1] = NULL;
    // CAT: the exe's univCreateShip 0x00542320 adds the emitter to the universe and records the moon as its creator
    //      (7 arguments: ..., built 0, creator moon, 0); this tree's univAddShip does the first, the second is set here
    pos = ship->obj.posinfo.position;
    pos.z = NOMADMOON_EMITTER_OFFSET + ship->obj.posinfo.position.z;
    spec->emitter[0] = univAddShip(dNomadFieldEmitter, ship->shiprace, &pos, ship->playerowner, 0);
    spec->emitter[0]->catCreator = ship;
    pos = ship->obj.posinfo.position;
    pos.z = ship->obj.posinfo.position.z - NOMADMOON_EMITTER_OFFSET;
    spec->emitter[1] = univAddShip(dNomadFieldEmitter, ship->shiprace, &pos, ship->playerowner, 0);
    spec->emitter[1]->catCreator = ship;
    spec->fieldState = 0;
}

Ship *cat_nomadMoonFind(void)                                                               // 0x005bdc80
{
    Node *node;
    Ship *moon = NULL;

    // the last dNomadMoon of the ship list
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        if (((Ship *)listGetStructOfNode(node))->shiptype == dNomadMoon)
        {
            moon = (Ship *)listGetStructOfNode(node);
        }
    }
    return moon;
}

bool32 cat_nomadMoonCanFire(Ship *moon)                                                     // 0x005bdcb0
{
    NomadMoonSpec *spec = (NomadMoonSpec *)moon->ShipSpecifics;

    return (nomadMoonEmitters(spec) != 0) && (spec->fieldState == 0);
}

bool32 cat_nomadMoonFire(Ship *moon)                                                        // 0x005bdcf0
{
    NomadMoonSpec *spec = (NomadMoonSpec *)moon->ShipSpecifics;

    if ((spec->fieldState != 0) || (nomadMoonEmitters(spec) == 0))
    {
        return FALSE;
    }
    nomadMoonStartCharge(spec);
    return TRUE;
}

sdword cat_nomadMoonEmitterCount(Ship *moon)                                                // 0x005bdd80
{
    return nomadMoonEmitters((NomadMoonSpec *)moon->ShipSpecifics);
}

void NomadMoonRemoveShipReferences(Ship *ship,Ship *shiptoremove)                           // 0x005bddb0
{
    NomadMoonSpec *spec = (NomadMoonSpec *)ship->ShipSpecifics;

    if (spec->emitter[0] == shiptoremove)
    {
        spec->emitter[0] = NULL;
    }
    if (spec->emitter[1] == shiptoremove)
    {
        spec->emitter[1] = NULL;
    }
    if ((nomadMoonEmitters(spec) == 0) && (spec->fieldState != 0))
    {
        spec->fieldState = 0;
    }
}

void NomadMoon_Fix(Ship *ship)                                                              // 0x005bde10
{
    NomadMoonSpec *spec = (NomadMoonSpec *)ship->ShipSpecifics;

    spec->emitter[0] = SpaceObjRegistryGetShip((sdword)(smemsize)spec->emitter[0]);
    spec->emitter[1] = SpaceObjRegistryGetShip((sdword)(smemsize)spec->emitter[1]);
}

void NomadMoon_PreFix(Ship *ship)                                                           // 0x005bde40
{
    NomadMoonSpec *spec = (NomadMoonSpec *)ship->ShipSpecifics;

    spec->emitter[0] = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->emitter[0]);
    spec->emitter[1] = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->emitter[1]);
}

void NomadMoonHouseKeep(Ship *ship)                                                         // 0x005bde70
{
    NomadMoonSpec *spec = (NomadMoonSpec *)ship->ShipSpecifics;
    vector pos;

    if (spec->fieldState == 1)
    {
        // charged: the shock wave (a type-5 blast) and the cool-down
        if (cat_timerExpired(&spec->fieldTimer))
        {
            cat_timerStartGameTime(&spec->fieldTimer, NOMADMOON_COOLDOWN_TIME);
            spec->fieldState = 2;
            cat_blastCreate(5, ship->obj.posinfo.position.x, ship->obj.posinfo.position.y,
                            ship->obj.posinfo.position.z, ship->playerowner->playerIndex);
            soundEventPlay(ship, 0x815, NULL);
        }
    }
    else if ((spec->fieldState == 2) && cat_timerExpired(&spec->fieldTimer))
    {
        spec->fieldState = 0;
    }
    // during an NIS the emitters follow the moon
    if (nisIsRunning == 1)
    {
        if ((spec->emitter[0] == NULL) || (spec->emitter[1] == NULL))
        {
            return;
        }
        pos = ship->obj.posinfo.position;
        pos.z = NOMADMOON_EMITTER_OFFSET + ship->obj.posinfo.position.z;
        cat_univSetShipPosition(spec->emitter[0], &pos);
        pos = ship->obj.posinfo.position;
        pos.z = ship->obj.posinfo.position.z - NOMADMOON_EMITTER_OFFSET;
        cat_univSetShipPosition(spec->emitter[1], &pos);
    }
    // outside an NIS it charges whenever big-gun shells fly
    if ((nisIsRunning == 0) && cat_bigGunBulletsInFlight() && (nomadMoonEmitters(spec) != 0) &&
        (spec->fieldState == 0))
    {
        nomadMoonStartCharge(spec);
    }
}

void NomadMoonAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)                  // 0x005be050
{
    (void)maxdist;
    attackPassive(ship, (Ship *)target);
}

void NomadMoonFire(Ship *ship,SpaceObjRotImpTarg *target)                                   // 0x005be070
{
    NomadMoonSpec *spec = (NomadMoonSpec *)ship->ShipSpecifics;

    // only the KAS's "fire with no target" charges the field
    if ((target == NULL) && (nomadMoonEmitters(spec) != 0) && (spec->fieldState == 0))
    {
        nomadMoonStartCharge(spec);
    }
}

// CAT: NomadMoonHeader 0x008c56e8 (dNomadMoon); slot 1 is the exe's nullsub
CustShipHeader NomadMoonHeader =
{
    .sizeofShipSpecifics = sizeof(NomadMoonSpec),
    .CustShipStaticInit = nullsubStaticInit,
    .CustShipInit = NomadMoonInit,
    .CustShipFire = NomadMoonFire,
    .CustShipHousekeep = NomadMoonHouseKeep,
    .CustShipRemoveShipReferences = NomadMoonRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))NomadMoonRemoveShipReferences,
    .CustShip_PreFix = NomadMoon_PreFix,
    .CustShip_Fix = NomadMoon_Fix,
    .CustShipAttack = NomadMoonAttack,
};

// CAT: NomadFieldEmitterHeader 0x008c5778 (dNomadFieldEmitter): slots 1 and 3 are the exe's nullsub
CustShipHeader NomadFieldEmitterHeader =
{
    .sizeofShipSpecifics = sizeof(NomadFieldEmitterSpec),
    .CustShipStaticInit = nullsubStaticInit,
    .CustShipInit = nullsubShip,
};
