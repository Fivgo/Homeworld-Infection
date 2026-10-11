// =============================================================================
//  Swarmer.c
//  - Cataclysm's sSwarmer / bSwarmer: the fighters a hive frigate launches
//    (cataclysm.exe 0x005c7930-0x005c7ac0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each).
//      SwarmerAttack 0x005c79b0 is also the slot-21 body the exe's linker folded into the HeavyCorvette and
//      MultiGunCorvette headers (docs/m2-r3-ships-a.md "ICF-folded bodies"); those files keep their own copies.

#include "Swarmer.h"

#include "CommandDefs.h"
#include "CommandLayer.h"
#include "DefaultShip.h"
#include "SaveGame.h"
#include "StatScript.h"
#include "Universe.h"

AttackSideStepParameters SwarmerStaticSect;     // 0x00a7a4c0: sSwarmer
AttackSideStepParameters SwarmerStaticBeast;    // 0x00a7a4b0: bSwarmer

void SwarmerStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)       // 0x005c7930
{
    AttackSideStepParameters *swarmerstat = (statinfo->shiprace != RACE_Sect) ? &SwarmerStaticBeast : &SwarmerStaticSect;

    swarmerstat->repositionTime = 0.0f;
    swarmerstat->circleRange = 0.0f;
    swarmerstat->fullMovementFreedom = FALSE;
    scriptSetStruct(directory,filename,AttackSideStepParametersScriptTable,(ubyte *)swarmerstat);
    statinfo->custstatinfo = swarmerstat;
}

void SwarmerInit(Ship *ship)                                                                // 0x005c7980
{
    SwarmerSpec *spec = (SwarmerSpec *)ship->ShipSpecifics;

    attackSideStepInit(&spec->attacksidestep);
    bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_STAY_TILL_EXPLICITLAUNCH);
    spec->state = CAT_SWARMER_Docked;
}

void SwarmerAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)                    // 0x005c79b0
{
    SwarmerSpec *spec = (SwarmerSpec *)ship->ShipSpecifics;

    (void)maxdist;
    attackSideStep(ship, target, &spec->attacksidestep, (AttackSideStepParameters *)ship->staticinfo->custstatinfo);
}

sdword cat_swarmerGetState(Ship *ship)                                                      // 0x005c79e0
{
    SwarmerSpec *spec = (SwarmerSpec *)ship->ShipSpecifics;
    sdword state = spec->state;

    // a swarmer outside the ship list (inside its hive) is docked
    if (ship->shiplink.belongto == NULL)
    {
        return CAT_SWARMER_Docked;
    }
    if (ship->command == NULL)
    {
        if ((state == CAT_SWARMER_Launching) || (state == CAT_SWARMER_Returning) || (state == CAT_SWARMER_Attacking))
        {
            return CAT_SWARMER_Free;
        }
        return state;
    }
    if (ship->command->ordertype.order == COMMAND_ATTACK)
    {
        return CAT_SWARMER_Attacking;
    }
    if (ship->command->ordertype.order != COMMAND_DOCK)
    {
        return CAT_SWARMER_Free;
    }
    if (state == CAT_SWARMER_Docked)
    {
        return CAT_SWARMER_Launching;
    }
    if ((state == CAT_SWARMER_Free) || (state == CAT_SWARMER_Attacking))
    {
        return CAT_SWARMER_Returning;
    }
    return state;
}

void cat_swarmerSetState(Ship *ship, sdword state)                                          // 0x005c7a50
{
    ((SwarmerSpec *)ship->ShipSpecifics)->state = state;
}

void SwarmerRemoveShipReferences(Ship *ship,Ship *shiptoremove)                             // 0x005c7a60
{
    SwarmerSpec *spec = (SwarmerSpec *)ship->ShipSpecifics;

    if (spec->hive == shiptoremove)
    {
        spec->hive = NULL;
    }
}

void Swarmer_PreFix(Ship *ship)                                                             // 0x005c7a80
{
    SwarmerSpec *spec = (SwarmerSpec *)ship->ShipSpecifics;

    spec->hive = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->hive);
}

void Swarmer_Fix(Ship *ship)                                                                // 0x005c7aa0
{
    SwarmerSpec *spec = (SwarmerSpec *)ship->ShipSpecifics;

    spec->hive = SpaceObjRegistryGetShip((sdword)(smemsize)spec->hive);
}

// CAT: SwarmerHeader 0x008c6ab0 (sSwarmer, bSwarmer); slot 10 is the exe's nullsub; no slot 12
CustShipHeader SwarmerHeader =
{
    .sizeofShipSpecifics = sizeof(SwarmerSpec),
    .CustShipStaticInit = SwarmerStaticInit,
    .CustShipInit = SwarmerInit,
    .CustShipFire = DefaultShipFire,
    .CustShipHousekeep = nullsubShip,
    .CustShipRemoveShipReferences = SwarmerRemoveShipReferences,
    .CustShip_PreFix = Swarmer_PreFix,
    .CustShip_Fix = Swarmer_Fix,
    .CustShipAttack = SwarmerAttack,
};
