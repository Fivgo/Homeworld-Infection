// =============================================================================
//  ModuleShip.c
//  - Cataclysm's module ships: the parts clamped to a Sect or Beast mothership,
//    a carrier or a dreadnought (cataclysm.exe 0x005af4c0-0x005afef0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each). The
//      motherships' modules are made by cat_univCreateMothershipModules (UnivUpdate.c), the dreadnought's cannon by
//      cat_dreadnoughtCreateCannon (Dreadnought.c).

#include "ModuleShip.h"

#include "Bounties.h"
#include "Clamp.h"
#include "ClassDefs.h"
#include "Debug.h"
#include "MeshAnim.h"
#include "NIS.h"
#include "Randy.h"
#include "ShipDisable.h"
#include "UnivUpdate.h"
#include "Universe.h"
#include "Vector.h"

ModuleShipStatics ModuleShipStatic[TOTAL_NUM_SHIPS];   // CAT: the exe's per-type blocks 0x00a7626c .. 0x00a762bc

#define MODULE_SPIN_STEP            0.05f       // 0x008c35f8
#define MODULE_SPIN_WRAP            6.2831855f  // 0x0072a594
#define MODULE_EJECT_FORCE_MIN      50000.0f    // 0x008c35fc
#define MODULE_EJECT_FORCE_MAX      70000.0f    // 0x008c3600
#define MODULE_RANDOM_SCALE         2.3283064e-10f  // 0x0072a4e0 = 2^-32
#define MODULE_ORPHAN_RANDOM_SCALE  4.656613e-10f   // 0x0072afb8 = 2^-31

// 0x008c3608: per eject direction, (min, max) along the host's up vector, then twice along its right vector (the exe
// reads the right vector for both of the last two pairs: coordsys +0xA4, 0x005afa92 and 0x005afb7b)
static const real32 moduleEjectRanges[7][3][2] =
{
    { {  0.4f,   0.5f }, { -0.3f, 0.3f }, { -0.02f,  0.02f } },
    { { -0.4f,  -0.5f }, { -0.3f, 0.3f }, { -0.02f,  0.02f } },
    { {  0.15f,  0.2f }, { -0.3f, 0.3f }, { -0.2f,  -0.4f } },
    { { -0.15f, -0.2f }, { -0.3f, 0.3f }, { -0.2f,  -0.4f } },
    { {  0.1f,   0.2f }, { -0.3f, 0.3f }, {  0.1f,   0.3f } },
    { { -0.1f,  -0.2f }, { -0.3f, 0.3f }, {  0.1f,   0.3f } },
    { { -0.4f,  -0.5f }, { -0.3f, 0.3f }, { -0.02f,  0.02f } },
};

// 0x008c36b0: the eject direction of each module type (the exe ends the search at the dword before ModuleShipHeader)
static const struct { ShipType type; sdword direction; } moduleEjectDirections[] =
{
    { sMothershipSpecial, 0 }, { sMothershipEngineering, 4 }, { sMothershipArmour, 0 }, { sMothershipMicro, 0 },
    { sMothershipWeapons, 0 }, { sMothershipDockingBay, 5 }, { sMothershipSupport, 0 }, { sMothershipEnginesLower, 3 },
    { sMothershipEnginesUpper, 2 }, { sMothershipLowerSpine, 1 }, { sMothershipBigGun, 6 },
    { sMothershipOreCanArmour, 0 }, { sCarrierSupport, 1 }, { sRecon, 0 },
};

ModuleShipStatics *cat_moduleShipGetStatics(ShipType type)                                 // 0x005af4c0
{
    // CAT: the exe returns one fixed block per module type, and the type number itself (as a pointer) for any other
    //      type; ModuleShipHeader is only on module types, so the second case never runs. Here: one block per type.
    return &ModuleShipStatic[type];
}

void ModuleShipStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)   // 0x005af5e0
{
    ModuleShipStatics *modulestat = cat_moduleShipGetStatics(statinfo->shiptype);

    (void)directory;
    (void)filename;
    switch (statinfo->shiptype)
    {
        case sDreadnoughtCannon:
        case sCarrierSupport:           modulestat->orphanDelay = 1.0f;  break;
        case sMothershipSpecial:        modulestat->orphanDelay = 4.0f;  break;
        case sMothershipEngineering:    modulestat->orphanDelay = 4.5f;  break;
        case sMothershipArmour:
        case bMothershipDockingBay:
        case bSectMoship:               modulestat->orphanDelay = 2.0f;  break;
        case sMothershipMicro:          modulestat->orphanDelay = 3.5f;  break;
        case sMothershipWeapons:        modulestat->orphanDelay = 2.25f; break;
        case sMothershipDockingBay:     modulestat->orphanDelay = 1.6f;  break;
        case sMothershipSupport:        modulestat->orphanDelay = 1.25f; break;
        case sMothershipCommand:        modulestat->orphanDelay = 1.75f; break;
        case sMothershipEnginesLower:   modulestat->orphanDelay = 5.25f; break;
        case sMothershipEnginesUpper:   modulestat->orphanDelay = 5.0f;  break;
        case sMothershipLowerSpine:     modulestat->orphanDelay = 3.25f; break;
        case sMothershipBigGun:
        case bMothershipSupport:        modulestat->orphanDelay = 2.75f; break;
        case sMothershipOreCanArmour:   modulestat->orphanDelay = 2.5f;  break;
        case bMothershipEnginesLower:   modulestat->orphanDelay = 3.0f;  break;
        default:                                                         break;
    }
    statinfo->custstatinfo = modulestat;
    statinfo->canSpecialBandBoxFriendlies = TRUE;
    statinfo->unknownFlag212 = TRUE;
    statinfo->unknownFlag213 = TRUE;
}

void ModuleShipInit(Ship *ship)                                                             // 0x005af720
{
    ModuleShipSpec *spec = (ModuleShipSpec *)ship->ShipSpecifics;

    spec->ejecting = 0;
    spec->ejectEndTime = 0.0f;
    spec->spinning = FALSE;
    // a Beast module starts stunned (the exe's switch also lists bHeavyCruiser and bCarrier, which never have this
    // header)
    switch (ship->shiptype)
    {
        case bHeavyCruiser:
        case bCarrier:
        case bMothershipDockingBay:
        case bMothershipEnginesLower:
            if (ship->shiprace == RACE_Beast)
            {
                ship->catBeastSpin = -1.0f;
                ship->catBeastStunState = 1;
                ship->catBeastStunStart = universe.totaltimeelapsed + 1.0f;
                ship->catBeastStunEnd = ship->catBeastStunStart + 2.5f;
            }
            break;
        default:
            break;
    }
    if (ship->shiptype == sMothershipCommand)
    {
        // CAT-TODO(stage 3): the exe starts animation number 0 looping (madAnimationStart(ship, 0, 1)); this tree's
        //      madAnimationStart takes the header index and no loop argument (MeshAnim.c)
        cat_madAnimationStartNumber(ship, 0);
    }
}

void ModuleShipRemoveShipReferences(Ship *ship,Ship *shiptoremove)                          // 0x005af7c0
{
    if (shiptoremove == ship->dockvars.dockship)
    {
        ship->dockvars.dockship = NULL;
        bitClear(ship->header.flags, SOF_CatDocking);
        bitClear(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatNoAvoidance);
        unClampObj((SpaceObjRotImpTargGuidance *)ship);
    }
}

void ModuleShipHouseKeep(Ship *ship)                                                        // 0x005af800
{
    ModuleShipSpec *spec = (ModuleShipSpec *)ship->ShipSpecifics;
    ModuleShipStatics *modulestat;

    if (spec->ejecting == 1)
    {
        if (universe.totaltimeelapsed <= spec->ejectEndTime)
        {
            ship->obj.posinfo.force.x = spec->ejectForce.x + ship->obj.posinfo.force.x;
            ship->obj.posinfo.force.y = spec->ejectForce.y + ship->obj.posinfo.force.y;
            ship->obj.posinfo.force.z = spec->ejectForce.z + ship->obj.posinfo.force.z;
            spec->spinAngle = MODULE_SPIN_STEP + spec->spinAngle;
            if (spec->spinAngle >= MODULE_SPIN_WRAP)
            {
                spec->spinAngle = 0.0f;
            }
        }
        else if (!spec->explodeWhenDone)
        {
            bitSet(ship->header.flags, SOF_DontApplyPhysics);
        }
        else
        {
            // a module that lost its host is destroyed when the push ends (no player killed it)
            ship->howDidIDie = 0x309;
            ship->whoKilledMe = 99;
            bountyShipWasKilled(ship);
            ship->playerowner->totalships--;
            AddTargetToDeleteList((SpaceObjRotImpTarg *)ship, GS_LargeEnergyCannon, 5);
        }
    }
    // a module that lost its host (outside an NIS) drifts away after its delay plus up to 2 s
    if ((nisIsRunning == 0) && (ship->guidance.clampInfo == NULL) && (spec->ejecting != 1))
    {
        modulestat = cat_moduleShipGetStatics(ship->staticinfo->shiptype);
        spec->ejecting = 1;
        spec->ejectEndTime = (real32)ranRandom(RANDOM_CAT_SHIPS) * MODULE_ORPHAN_RANDOM_SCALE + modulestat->orphanDelay +
                             universe.totaltimeelapsed;
        spec->ejectStartTime = universe.totaltimeelapsed;
        spec->explodeWhenDone = TRUE;
    }
}

// one component of the eject push: (max - min) * 2^-32 * r + min
static real32 moduleEjectRandom(real32 min, real32 max, udword r)
{
    return (max - min) * MODULE_RANDOM_SCALE * (real32)r + min;
}

void cat_moduleShipEject(Ship *module, bool32 explodeWhenDone, real32 delay, real32 duration)  // 0x005af940
{
    ModuleShipSpec *spec = (ModuleShipSpec *)module->ShipSpecifics;
    Ship *host;
    vector up, right;
    const real32 (*range)[2];
    sdword direction = -1;
    udword r0, r1, r2;
    sdword i;

    spec->ejecting = 1;
    spec->ejectStartTime = universe.totaltimeelapsed + delay;
    spec->ejectEndTime = spec->ejectStartTime + duration;
    for (i = 0; i < (sdword)(sizeof(moduleEjectDirections) / sizeof(moduleEjectDirections[0])); i++)
    {
        if (moduleEjectDirections[i].type == module->shiptype)
        {
            direction = moduleEjectDirections[i].direction;
            break;
        }
    }
    if (direction < 0)
    {
        // CAT-FIX: for a type the table does not list (sDreadnoughtCannon, sMothershipCommand and the four Beast
        //      modules) the exe takes the low byte of the module's address as the direction and reads its range
        //      table out of bounds (0x005af97a); the port uses direction 0
        dbgAssertOrIgnore(FALSE);
        direction = 0;
    }
    range = moduleEjectRanges[direction];
    host = (Ship *)module->guidance.clampInfo->host;
    up.x = host->rotatable.rotinfo.coordsys.m11;
    up.y = host->rotatable.rotinfo.coordsys.m21;
    up.z = host->rotatable.rotinfo.coordsys.m31;
    right.x = host->rotatable.rotinfo.coordsys.m12;
    right.y = host->rotatable.rotinfo.coordsys.m22;
    right.z = host->rotatable.rotinfo.coordsys.m32;
    r0 = ranRandom(RANDOM_GAME); r1 = ranRandom(RANDOM_GAME); r2 = ranRandom(RANDOM_GAME);
    spec->ejectForce.x = moduleEjectRandom(range[0][0], range[0][1], r0) * up.x;
    spec->ejectForce.y = moduleEjectRandom(range[0][0], range[0][1], r1) * up.y;
    spec->ejectForce.z = moduleEjectRandom(range[0][0], range[0][1], r2) * up.z;
    r0 = ranRandom(RANDOM_GAME); r1 = ranRandom(RANDOM_GAME); r2 = ranRandom(RANDOM_GAME);
    spec->ejectForce.x = moduleEjectRandom(range[1][0], range[1][1], r0) * right.x + spec->ejectForce.x;
    spec->ejectForce.y = moduleEjectRandom(range[1][0], range[1][1], r1) * right.y + spec->ejectForce.y;
    spec->ejectForce.z = moduleEjectRandom(range[1][0], range[1][1], r2) * right.z + spec->ejectForce.z;
    r0 = ranRandom(RANDOM_GAME); r1 = ranRandom(RANDOM_GAME); r2 = ranRandom(RANDOM_GAME);
    spec->ejectForce.x = moduleEjectRandom(range[2][0], range[2][1], r0) * right.x + spec->ejectForce.x;
    spec->ejectForce.y = moduleEjectRandom(range[2][0], range[2][1], r1) * right.y + spec->ejectForce.y;
    spec->ejectForce.z = moduleEjectRandom(range[2][0], range[2][1], r2) * right.z + spec->ejectForce.z;
    vecNormalize(&spec->ejectForce);
    spec->ejectForce.x = moduleEjectRandom(MODULE_EJECT_FORCE_MIN, MODULE_EJECT_FORCE_MAX, ranRandom(RANDOM_GAME)) * spec->ejectForce.x;
    spec->ejectForce.y = moduleEjectRandom(MODULE_EJECT_FORCE_MIN, MODULE_EJECT_FORCE_MAX, ranRandom(RANDOM_GAME)) * spec->ejectForce.y;
    spec->ejectForce.z = moduleEjectRandom(MODULE_EJECT_FORCE_MIN, MODULE_EJECT_FORCE_MAX, ranRandom(RANDOM_GAME)) * spec->ejectForce.z;
    spec->spinning = TRUE;
    spec->spinAxis.x = module->rotatable.rotinfo.coordsys.m11;
    spec->spinAxis.y = module->rotatable.rotinfo.coordsys.m21;
    spec->spinAxis.z = module->rotatable.rotinfo.coordsys.m31;
    vecNormalize(&spec->spinAxis);
    spec->spinAngle = 0.0f;
    unClampObj((SpaceObjRotImpTargGuidance *)module);
    cat_shipSetDisableReason(module, 1, 8);
    bitSet(module->header.flags, SOF_CatDocking);
    spec->explodeWhenDone = explodeWhenDone;
}

// the module iterator's state (0x00a762c0 host, 0x00a762c4 count, 0x00a762c8 index)
static Ship *moduleIterHost = NULL;
static sdword moduleIterCount = 0;
static sdword moduleIterIndex = 0;

// the next module ship (a ship of CLASS_Component) clamped to the host; a new host starts over
static Ship *cat_moduleIterate(Ship *host)                                                  // 0x005afd70
{
    CatClampedList *clamped;
    Ship *module;

    if (host != NULL)
    {
        moduleIterHost = host;
        moduleIterIndex = 0;
        if ((host->guidance.clampInfo == NULL) || (host->guidance.clampInfo->clampedObjs == NULL))
        {
            moduleIterCount = 0;
            return NULL;
        }
        moduleIterCount = host->guidance.clampInfo->clampedObjs->numShips;
    }
    if (moduleIterCount == 0)
    {
        return NULL;
    }
    clamped = moduleIterHost->guidance.clampInfo->clampedObjs;
    while (moduleIterIndex < clamped->numShips)
    {
        module = clamped->ShipPtr[moduleIterIndex];
        moduleIterIndex++;
        if ((module->header.objtype == OBJ_ShipType) && (module->staticinfo->shipclass == CLASS_Component))
        {
            return module;
        }
    }
    return NULL;
}

Ship *cat_moduleIterFirst(Ship *host)                                                       // 0x005afed0
{
    return cat_moduleIterate(host);
}

Ship *cat_moduleIterNext(void)                                                              // 0x005afee0
{
    return cat_moduleIterate(NULL);
}

void ModuleShipDied(Ship *ship)                                                             // 0x005afe30
{
    Ship *host;
    CatClampedList *clamped;
    sdword i;

    // the docking bay of a Sect mothership takes the ships being built at it along
    if ((ship->shiptype != sMothershipDockingBay) || (ship->guidance.clampInfo == NULL))
    {
        return;
    }
    host = (Ship *)ship->guidance.clampInfo->host;
    if ((host == NULL) || (host->header.objtype != OBJ_ShipType) || (host->shiptype != sMothership))
    {
        return;
    }
    // CAT-TODO(M2-R4 #5): cat_cmCancelJobsOfDeadFactory(host) 0x00455ea0 (the construction manager's jobs)
    clamped = host->guidance.clampInfo->clampedObjs;
    for (i = 0; i < clamped->numShips; i++)
    {
        if (bitTest(clamped->ShipPtr[i]->shipOrDerelict.specialFlags2, SPECIAL_2_CatExternalBuildTimer))
        {
            AddTargetToDeleteList((SpaceObjRotImpTarg *)clamped->ShipPtr[i], GS_LargeEnergyCannon, 0xf5);
        }
    }
}

// CAT: ModuleShipHeader 0x008c3720 (sDreadnoughtCannon, sCarrierSupport, the 13 sMothership modules, bMothershipDockingBay,
//      bMothershipEnginesLower, bSectMoship, bMothershipSupport); no slot 12
CustShipHeader ModuleShipHeader =
{
    .sizeofShipSpecifics = sizeof(ModuleShipSpec),
    .CustShipStaticInit = ModuleShipStaticInit,
    .CustShipInit = ModuleShipInit,
    .CustShipHousekeep = ModuleShipHouseKeep,
    .CustShipRemoveShipReferences = ModuleShipRemoveShipReferences,
    .CustShipDied = ModuleShipDied,
};
