// =============================================================================
//  Acolyte.c
//  - Cataclysm's sAcolyte / bAcolyte / dAcolyte: the linkable fighter with
//    folding wings and a linked missile weapon
//    (cataclysm.exe 0x005ad280-0x005adba0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each).
//      cat_shipIsAcolyte 0x005ad300, cat_shipIsAcolyteOrDerelict 0x005ad360 and cat_acolyteCanFireLinkedWeapon
//      0x005ada50 moved here from ShipSelect.c (their declarations stay in ShipSelect.h for its callers).

#include "Acolyte.h"

#include "Cloak.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "Gun.h"
#include "MeshAnim.h"
#include "SaveGame.h"
#include "ShipSelect.h"
#include "Universe.h"

#define ACOLYTE_LINK_FIRE_RANGE_SQR     16000000.0f     // 0x008c3258: 4000 units

static bool32 acolyteIsAcolyte(Ship *ship)
{
    return (ship->shiptype == sAcolyte) || (ship->shiptype == bAcolyte);
}

void cat_acolyteLinkReset(Ship *ship)                                                       // 0x005ad280
{
    AcolyteSpec *spec = (AcolyteSpec *)ship->ShipSpecifics;

    if (acolyteIsAcolyte(ship))
    {
        spec->linkAttackPending = FALSE;
        spec->linkFire = FALSE;
        spec->linkTarget = NULL;
        ship->gunInfo->guns[2].numMissiles = 1;
        ship->gunInfo->guns[3].numMissiles = 1;
    }
}

void cat_acolyteLinkGunsOff(Ship *ship)                                                     // 0x005ad2d0
{
    if (acolyteIsAcolyte(ship))
    {
        ship->gunInfo->guns[2].numMissiles = 0;
        ship->gunInfo->guns[3].numMissiles = 0;
    }
}

bool32 cat_shipIsAcolyte(Ship *ship)                                                        // 0x005ad300
{
    // sAcolyte and bAcolyte (not dAcolyte)
    return acolyteIsAcolyte(ship) ? TRUE : FALSE;
}

bool32 cat_acolyteCanLink(Ship *ship)                                                       // 0x005ad320
{
    return acolyteIsAcolyte(ship) && cat_shipHasUpgrade(ship, CAT_UPGRADE_AcolyteLinkFire);
}

bool32 cat_shipIsAcolyteOrDerelict(Ship *ship)                                              // 0x005ad360
{
    return (ship->shiptype == sAcolyte) || (ship->shiptype == bAcolyte) || (ship->shiptype == dAcolyte);
}

bool32 cat_acolyteCanFireLinkedWeapon(Ship *ship)                                          // 0x005ada50
{
    // the third or fourth gun (the linked weapon) must hold missiles
    return acolyteIsAcolyte(ship) && cat_shipHasUpgrade(ship, CAT_UPGRADE_AcolyteLinkFire) &&
           ((ship->gunInfo->guns[2].numMissiles > 0) || (ship->gunInfo->guns[3].numMissiles > 0));
}

bool32 cat_acolyteCanLinkFire(Ship *ship)                                                   // 0x005adab0
{
    return cat_acolyteCanFireLinkedWeapon(ship) ? 2 : 0;
}

void AcolyteInit(Ship *ship)                                                                // 0x005ad390
{
    AcolyteSpec *spec = (AcolyteSpec *)ship->ShipSpecifics;

    GenericInterceptorInit(ship);
    spec->link.partner = NULL;
    spec->wingsState = 1;
    spec->wingsOpen = 0;
    spec->wingsPending = 0;
    spec->linkFire = FALSE;
    spec->linkAttackPending = FALSE;
    spec->linkTarget = NULL;
}

void AcolyteStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)     // 0x005ad3d0
{
    GenericInterceptorStaticInit(directory, filename, statinfo);
    statinfo->canSpecialBandBoxFriendlies = FALSE;
    statinfo->unknownFlag212 = TRUE;
    statinfo->unknownFlag213 = FALSE;
}

// the linked weapon: guns 2 and 3 launch their missiles (inlined in AcolyteFire and AcolyteHouseKeep)
static void acolyteFireLinkedWeapon(Ship *ship, SpaceObjRotImpTarg *target)
{
    Gun *gun;
    sdword i;

    for (i = 2; i < 4; i++)
    {
        gun = &ship->gunInfo->guns[i];
        if (cat_gunCanFire(ship, gun) && (gun->numMissiles > 0))
        {
            missileShoot(ship, gun, target);
        }
    }
}

void AcolyteFire(Ship *ship,SpaceObjRotImpTarg *target)                                    // 0x005ad400
{
    AcolyteSpec *spec = (AcolyteSpec *)ship->ShipSpecifics;

    // a derelict Acolyte fires only with its wings open
    if ((ship->shiptype != dAcolyte) || ((spec->wingsState == 0) && cat_madAnimationFinished(ship)))
    {
        GenericInterceptorFire(ship, target);
    }
    if (acolyteIsAcolyte(ship) && (spec->wingsState == 0) && cat_madAnimationFinished(ship) &&
        (spec->linkFire == TRUE) && cat_acolyteCanFireLinkedWeapon(ship))
    {
        acolyteFireLinkedWeapon(ship, target);
    }
    ship->shipisattacking = TRUE;
}

bool32 AcolyteSpecialTarget(Ship *ship,void *custom)                                        // 0x005ad510
{
    AcolyteSpec *spec = (AcolyteSpec *)ship->ShipSpecifics;
    SpecialCommand *targets = (SpecialCommand *)custom;

    if (targets->numTargets > 0)
    {
        spec->linkFire = TRUE;
        spec->linkAttackPending = TRUE;
        spec->linkTarget = (Ship *)targets->TargetPtr[0];
    }
    return TRUE;    // eax = 1 on both paths (0x005ad51c)
}

// the Acolytes of the ship's formation that wait to attack the same target attack it together
static void cat_acolyteLinkedAttack(Ship *ship)                                             // 0x005ad540
{
    AcolyteSpec *spec = (AcolyteSpec *)ship->ShipSpecifics;
    static MaxSelection selectionBuffer;
    SelectCommand *selection = (SelectCommand *)&selectionBuffer;
    SelectCommand *formation;
    AttackCommand attackone;
    AcolyteSpec *otherspec;
    Ship *other;
    sdword i;

    if ((ship->command == NULL) || !(ship->command->ordertype.attributes & COMMAND_MASK_FORMATION) ||
        ((formation = ship->command->selection) == NULL) || (formation->numShips <= 1))
    {
        return;
    }
    selection->numShips = 0;
    selection->ShipPtr[0] = NULL;
    for (i = 0; i < formation->numShips; i++)
    {
        other = formation->ShipPtr[i];
        otherspec = (AcolyteSpec *)other->ShipSpecifics;
        if (acolyteIsAcolyte(other) && otherspec->linkAttackPending && otherspec->linkFire &&
            (otherspec->linkTarget == spec->linkTarget))
        {
            selection->ShipPtr[selection->numShips++] = other;
            otherspec->linkAttackPending = FALSE;
        }
    }
    attackone.numTargets = 1;
    attackone.TargetPtr[0] = (TargetPtr)spec->linkTarget;
    clAttack(&universe.mainCommandLayer, selection, &attackone);
}

void AcolyteHouseKeep(Ship *ship)                                                           // 0x005ad620
{
    AcolyteSpec *spec = (AcolyteSpec *)ship->ShipSpecifics;
    SpaceObjRotImpTarg *target;
    SelectCommand selectone;
    AttackCommand attackone;
    vector diff;
    double delay;

    // a due linked attack: with the formation's Acolytes, else alone
    if (spec->linkAttackPending == TRUE)
    {
        cat_acolyteLinkedAttack(ship);
        if (spec->linkAttackPending == TRUE)
        {
            attackone.TargetPtr[0] = (TargetPtr)spec->linkTarget;
            selectone.numShips = 1;
            selectone.ShipPtr[0] = ship;
            attackone.numTargets = 1;
            spec->linkAttackPending = FALSE;
            clAttack(&universe.mainCommandLayer, &selectone, &attackone);
        }
    }
    // inside 4000 units of its link target the linked weapon fires
    if (acolyteIsAcolyte(ship) && spec->linkFire && (spec->linkTarget != NULL) && cat_acolyteCanFireLinkedWeapon(ship))
    {
        target = (SpaceObjRotImpTarg *)spec->linkTarget;
        diff.x = ship->obj.posinfo.position.x - target->obj.posinfo.position.x;
        diff.y = ship->obj.posinfo.position.y - target->obj.posinfo.position.y;
        diff.z = ship->obj.posinfo.position.z - target->obj.posinfo.position.z;
        if ((diff.x * diff.x + diff.y * diff.y + diff.z * diff.z < ACOLYTE_LINK_FIRE_RANGE_SQR) &&
            (spec->wingsState == 0) && cat_madAnimationFinished(ship) && (spec->linkFire == TRUE) &&
            cat_acolyteCanFireLinkedWeapon(ship))
        {
            acolyteFireLinkedWeapon(ship, target);
        }
    }
    // the wings: open a random 0 .. 2.6 s after it starts attacking, close 2 .. 4.6 s after it stops
    if (!ship->shipisattacking)
    {
        if (!spec->wingsOpen)
        {
            if (!spec->wingsPending)
            {
                return;
            }
        }
        else if (spec->wingsState != 1)
        {
            if (spec->wingsPending)
            {
                if (universe.totaltimeelapsed <= spec->wingsTime)
                {
                    return;
                }
                // CAT-TODO(stage 3): the exe passes a third argument, 2
                cat_madAnimationStartNumber(ship, 1);
                spec->wingsState = 1;
                spec->wingsOpen = 0;
                spec->wingsPending = 0;
                return;
            }
            delay = cat_randomExpDelay() * 3.0 + 2.0;      // 0x0072bcc0, 0x0072a5c0
            spec->wingsPending = 1;
            spec->wingsTime = (real32)((double)universe.totaltimeelapsed + delay);
            return;
        }
    }
    else if (spec->wingsState != 0)
    {
        if (spec->wingsPending)
        {
            if (universe.totaltimeelapsed <= spec->wingsTime)
            {
                return;
            }
            // CAT-TODO(stage 3): the exe passes a third argument, 0
            cat_madAnimationStartNumber(ship, 0);
            spec->wingsState = 0;
            spec->wingsOpen = 1;
            spec->wingsPending = 0;
            return;
        }
        delay = cat_randomExpDelay() * 3.0;
        spec->wingsPending = 1;
        spec->wingsTime = (real32)((double)universe.totaltimeelapsed + delay);
        return;
    }
    spec->wingsPending = 0;
}

void Acolyte_PreFix(Ship *ship)                                                             // 0x005ad980
{
    AcolyteSpec *spec = (AcolyteSpec *)ship->ShipSpecifics;

    spec->linkTarget = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->linkTarget);
    spec->link.partner = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->link.partner);
    if (spec->link.effect != NULL)
    {
        dbgAssertOrIgnore(FALSE);       // the exe's nullsubReturnFalse: the link effect is not saved
        spec->link.effect = NULL;
    }
}

void Acolyte_Fix(Ship *ship)                                                                // 0x005ad9e0
{
    AcolyteSpec *spec = (AcolyteSpec *)ship->ShipSpecifics;

    spec->linkTarget = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->linkTarget);
    spec->link.partner = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->link.partner);
}

// clear the linked attack (inlined in HaltCommand, SpecialActivate, Attack and RemoveShipReferences)
static void acolyteClearLinkAttack(AcolyteSpec *spec)
{
    spec->linkAttackPending = FALSE;
    spec->linkFire = FALSE;
    spec->linkTarget = NULL;
}

bool32 AcolyteHaltCommand(Ship *ship)                                                       // 0x005ada10
{
    AcolyteSpec *spec = (AcolyteSpec *)ship->ShipSpecifics;

    acolyteClearLinkAttack(spec);
    if ((ship->command != NULL) && (ship->command->ordertype.order == COMMAND_CAT_LINK))
    {
        return FALSE;
    }
    return spec->link.partner == NULL;
}

bool32 AcolyteSpecialActivate(Ship *ship)                                                   // 0x005adb10
{
    acolyteClearLinkAttack((AcolyteSpec *)ship->ShipSpecifics);
    return TRUE;
}

void AcolyteAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)                   // 0x005adb30
{
    AcolyteSpec *spec = (AcolyteSpec *)ship->ShipSpecifics;

    if ((Ship *)target != spec->linkTarget)
    {
        acolyteClearLinkAttack(spec);
    }
    GenericInterceptorAttack(ship, target, maxdist);
}

void AcolyteRemoveShipReferences(Ship *ship,Ship *shiptoremove)                             // 0x005adb70
{
    AcolyteSpec *spec = (AcolyteSpec *)ship->ShipSpecifics;

    if (spec->linkTarget == shiptoremove)
    {
        acolyteClearLinkAttack(spec);
    }
}

// CAT: AcolyteHeader 0x008c3260 (sAcolyte, bAcolyte, dAcolyte); slot 20 (move) is SpecialActivate as well
CustShipHeader AcolyteHeader =
{
    .sizeofShipSpecifics = sizeof(AcolyteSpec),
    .CustShipStaticInit = AcolyteStaticInit,
    .CustShipInit = AcolyteInit,
    .CustShipFire = AcolyteFire,
    .CustShipAttackPassive = GenericInterceptorPassiveAttack,
    .CustShipSpecialActivate = AcolyteSpecialActivate,
    .CustShipSpecialTarget = AcolyteSpecialTarget,
    .CustShipHousekeep = AcolyteHouseKeep,
    .CustShipRemoveShipReferences = AcolyteRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))AcolyteRemoveShipReferences,
    .CustShip_PreFix = Acolyte_PreFix,
    .CustShip_Fix = Acolyte_Fix,
    .CustShipMoveCommand = AcolyteSpecialActivate,
    .CustShipAttack = AcolyteAttack,
    .CustShipHaltCommand = AcolyteHaltCommand,
};
