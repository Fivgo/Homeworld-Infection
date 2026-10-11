// =============================================================================
//  MultiBeamFrigate.c
//  - Cataclysm's sMultiBeamFrigate / bMultiBeamFrigate: a frigate that closes
//    in, steadies and sweeps its beams over every enemy in range
//    (cataclysm.exe 0x005bd730-0x005bdb30)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each).

#include "MultiBeamFrigate.h"

#include "AIShip.h"
#include "AITrack.h"
#include "Cloak.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "DefaultShip.h"
#include "FastMath.h"
#include "Gun.h"
#include "Physics.h"
#include "Randy.h"
#include "StatScript.h"
#include "UnivUpdate.h"
#include "Universe.h"

// the attack states (Ship.aistateattack)
#define MBF_STATE_INIT          0
#define MBF_STATE_APPROACH      1
#define MBF_STATE_FIRING        3

#define MBF_CLOSE_IN_FACTOR     0.75f       // 0x0072a670: it closes in to 3/4 of the beam range

MultiBeamFrigateStatics MultiBeamFrigateStaticSect;     // 0x00a7a9a0: sMultiBeamFrigate
MultiBeamFrigateStatics MultiBeamFrigateStaticBeast;    // 0x00a7a990: bMultiBeamFrigate

// 0x008c5588
scriptStructEntry MultiBeamFrigateScriptTable[] =
{
    { "MultiBeamRangeRatio", scriptSetReal32CB, &(MultiBeamFrigateStaticSect.multiBeamRangeRatio), &(MultiBeamFrigateStaticSect) },

    END_SCRIPT_STRUCT_ENTRY
};

void MultiBeamFrigateStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)     // 0x005bd730
{
    MultiBeamFrigateStatics *frigstat = (statinfo->shiprace != RACE_Sect) ? &MultiBeamFrigateStaticBeast
                                                                           : &MultiBeamFrigateStaticSect;

    statinfo->custstatinfo = frigstat;
    scriptSetStruct(directory,filename,MultiBeamFrigateScriptTable,(ubyte *)frigstat);
    scriptSetStruct(directory,filename,AttackSideStepParametersScriptTable,(ubyte *)&frigstat->sidestepParameters);
}

void MultiBeamFrigateInit(Ship *ship)                                                       // 0x005bd780
{
    MultiBeamFrigateSpec *spec = (MultiBeamFrigateSpec *)ship->ShipSpecifics;

    ship->aistateattack = MBF_STATE_INIT;
    attackSideStepInit(&spec->attacksidestep);
}

void MultiBeamFrigateFire(Ship *ship,SpaceObjRotImpTarg *target)                           // 0x005bd910
{
    MultiBeamFrigateStatics *frigstat = (MultiBeamFrigateStatics *)ship->staticinfo->custstatinfo;
    GunInfo *gunInfo = ship->gunInfo;
    SpaceObjRotImpTarg *picked = NULL;
    SelectCommand *enemies;
    vector trajectory;
    real32 range;
    sdword i, j;
    Gun *gun = NULL;

    aishipGetTrajectory(ship, target, &trajectory);
    range = RangeToTargetGivenDist(ship, target, fsqrt(trajectory.x * trajectory.x + trajectory.y * trajectory.y +
                                                       trajectory.z * trajectory.z));
    // the target first, with the first gun that can fire, has it in range and can turn to it
    for (i = 0; i < gunInfo->numGuns; i++)
    {
        gun = &gunInfo->guns[i];
        if (cat_gunCanFire(ship, gun) && (range < gun->gunstatic->bulletrange * frigstat->multiBeamRangeRatio) &&
            gunOrientGimbleGun(ship, gun, target))
        {
            picked = target;
            if (target != NULL)
            {
                goto fire;
            }
            break;
        }
    }
    // else any enemy in the first gun's beam range that a gun that can fire can turn to
    enemies = getEnemiesWithinProximity(ship, gunInfo->guns[0].gunstatic->bulletrange * frigstat->multiBeamRangeRatio);
    if (enemies == NULL)
    {
        return;
    }
    for (i = 0; i < gunInfo->numGuns; i++)
    {
        gun = &gunInfo->guns[i];
        if (cat_gunCanFire(ship, gun))
        {
            for (j = 0; j < enemies->numShips; j++)
            {
                if (gunOrientGimbleGun(ship, gun, (SpaceObjRotImpTarg *)enemies->ShipPtr[j]))
                {
                    picked = (SpaceObjRotImpTarg *)enemies->ShipPtr[j];
                    break;
                }
            }
            if (picked != NULL)
            {
                goto fire;
            }
        }
    }
    if (picked == NULL)
    {
        return;
    }
fire:
    if (!cat_shipIsCloakedFully((Ship *)picked))
    {
        gunShoot(ship, gun, picked);
        // firing at its own target, it steadies (unless the order moves while attacking) and holds still
        if ((ship->aistateattack != MBF_STATE_FIRING) && (target == picked))
        {
            if ((ship->command != NULL) &&
                !(ship->command->ordertype.attributes & COMMAND_MASK_ATTACKING_AND_MOVING))
            {
                aitrackSteadyShip(ship);
            }
            ship->aistateattack = MBF_STATE_FIRING;
        }
    }
}

void MultiBeamFrigateAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)          // 0x005bd7a0
{
    MultiBeamFrigateSpec *spec = (MultiBeamFrigateSpec *)ship->ShipSpecifics;
    MultiBeamFrigateStatics *frigstat = (MultiBeamFrigateStatics *)ship->staticinfo->custstatinfo;
    real32 closeIn = ship->gunInfo->guns[0].gunstatic->bulletrange * frigstat->multiBeamRangeRatio * MBF_CLOSE_IN_FACTOR;
    vector trajectory;

    (void)maxdist;
    switch (ship->aistateattack)
    {
        case MBF_STATE_INIT:
            ship->aistateattack = MBF_STATE_APPROACH;
            // fall through
        case MBF_STATE_APPROACH:
            // close in; once in range (or arrived) steady and strafe sideways (thrust right or left, 3:1)
            aishipGetTrajectory(ship, target, &trajectory);
            if ((RangeToTarget(ship, target, &trajectory) < closeIn) ||
                aishipFlyToPointAvoidingObjsFunc(ship, &target->obj.posinfo.position, 10, -10.0f,
                                                 &target->obj.posinfo.velocity))
            {
                aitrackSteadyShip(ship);
                ship->aistateattack = MBF_STATE_FIRING;
                spec->attacksidestep.aidirection = 3 - (((ranRandom(RANDOM_GAME) & 3) != 0) ? 1 : 0);
            }
            ship->staticinfo->custshipheader.CustShipFire(ship, target);
            break;

        case MBF_STATE_FIRING:
            aishipGetTrajectory(ship, target, &trajectory);
            physApplyForceToObj(&ship->obj, ship->guidance.nonstatvars.thruststrength[spec->attacksidestep.aidirection],
                                (uword)spec->attacksidestep.aidirection);
            aishipFlyToPointAvoidingObjsFunc(ship, NULL, 0, 0.0f, &ship->obj.posinfo.velocity);
            ship->staticinfo->custshipheader.CustShipFire(ship, target);
            if (RangeToTarget(ship, target, &trajectory) > closeIn)
            {
                ship->aistateattack = MBF_STATE_APPROACH;
            }
            break;

        default:
            break;
    }
}

void MultiBeamFrigateAttackPassive(Ship *ship,Ship *target,bool32 rotate)                  // 0x005bdb10
{
    (void)rotate;
    ship->staticinfo->custshipheader.CustShipFire(ship, (SpaceObjRotImpTarg *)target);
}

// CAT: MultiBeamFrigateHeader 0x008c55a8 (sMultiBeamFrigate, bMultiBeamFrigate); slot 10 is the exe's nullsub
CustShipHeader MultiBeamFrigateHeader =
{
    .sizeofShipSpecifics = sizeof(MultiBeamFrigateSpec),
    .CustShipStaticInit = MultiBeamFrigateStaticInit,
    .CustShipInit = MultiBeamFrigateInit,
    .CustShipFire = MultiBeamFrigateFire,
    .CustShipAttackPassive = MultiBeamFrigateAttackPassive,
    .CustShipHousekeep = nullsubShip,
    .CustShipAttack = MultiBeamFrigateAttack,
};
