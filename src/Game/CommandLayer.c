// =============================================================================
//  CommandLayer.c
//  - This is the ship command layer of Homeworld. All commands to ships go
//    through this layer, whether the commands originate from across a network,
//    a computer player, a human player, etc.
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 7/5/1997 by gshaw
// =============================================================================

#include "CommandLayer.h"
#include "FOW.h"

#include "AIPlayer.h"
#include "AIShip.h"
#include "AITrack.h"
#include "Alliance.h"
#include "Battle.h"
#include "Clamp.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "CommandNetwork.h"
#include "CommandWrap.h"
#include "ConsMgr.h"
#include "FastMath.h"
#include "Gun.h"
#include "Leech.h"
#include "ACV.h"
#include "Dreadnought.h"
#include "FEReg.h"
#include "ResearchGUI.h"
#include "ResearchAPI.h"
#include "Worker.h"
#include "Mothership.h"
#include "MadLinkIn.h"
#include "Memory.h"
#include "NetCheck.h"
#include "Ping.h"
#include "Randy.h"
#include "Salvage.h"
#include "Select.h"
#include "Ships.h"
#include "StringSupport.h"
#include "ShipSelect.h"
#include "Sentinel.h"
#include "ShipLink.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "SpeechEvent.h"
#include "StatScript.h"
#include "Tactics.h"
#include "Tutor.h"
#include "Tweak.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "OracleDump.h"    // HWI-FIX(release-1.0): declared, no longer an implicit int function


#define DEBUG_PASSIVE_ATTACK  0
#define DEBUG_LAUNCH_SHIP     0
#define DEBUG_COMMAND_LAYER   0

/*-----------------------------------------------------------------------------
    CAT (M2-R4 command, M2-R5 command-tactics): state of this file against cataclysm.exe. Every MODIFIED body of the
    exe's command layer is ported (each marked CAT at the change; docs/m2-r5-command-tactics.md lists them): the
    CommandToDo of 0x330 bytes with the waypoint MoveCommand, the move, hyperspace, attack and passive-attack order
    families, the cleanup functions with their reason codes, clProcess with orders 11-16, the link flight
    (Dock.c), clSpecial, clHalt*, clRUTransfer, the parade and holding-pattern code, clPostProcess, delegateCommand,
    processAttackToDoInFormation, LaunchAllInternalShipsOfPlayer, and the cat_cl* orders 0x00444060..0x0044e150.
    Left to other stages (with their CAT-TODOs at the call): the command messages (stage 3), the network packets of
    the new orders (stage 5). The speech and sound ids are the exe's (Cataclysm's speech runtime, M2-R5 sound).
-----------------------------------------------------------------------------*/

/*=============================================================================
    Private Functions:
=============================================================================*/

static void FormationCleanup(struct CommandToDo *formationtodo);
static void MoveCleanup(struct CommandToDo *movetodo);
static void MpHyperspaceCleanup(struct CommandToDo *movetodo);
static void RemoveShipFromMoving(Ship *ship);
//static void RemoveShipFromAttacking(Ship *ship);
//static void RemoveShipReferencesFromExtraAttackInfo(Ship *shiptoremove,CommandToDo *todo);
//static void RemoveAttackTargetFromExtraAttackInfo(SpaceObjRotImpTarg *targettoremove,CommandToDo *todo);
static SpaceObjRotImpTarg *AttackClosestOpponent(Ship *ship,AttackCommand *attack);
static void AttackCleanup(struct CommandToDo *attacktodo);
static AttackTargets *allocateMultipleAttackTargets(Ship *ship);
static void pickMultipleAttackTargets(Ship *ship,AttackCommand *attack);
void catAttackSelfCheck(const char *where, SelectCommand *sel, AttackCommand *att);   // HWI-TEST (M3-R4 nan-positions)
void ChangeOrderToHalt(CommandToDo *alreadycommand);
void CalculateMoveToPoints(SelectCommand *selection,vector from,vector to);
bool32 processMoveToDo(CommandToDo *movetodo,bool32 passiveAttacked,sdword shipIndex,bool32 inFormation);
bool32 processMpHyperspaceingToDo(CommandToDo *movetodo);
void clMoveThese(CommandLayer *comlayer,SelectCommand *selectcom,sdword pathType,vector from,sdword numPoints,vector *points,bool32 catSplit);
void clMpHyperspaceThese(CommandLayer *comlayer,SelectCommand *selectcom,vector from,vector to);
bool32 isSelectionExclusivlyStrikeCraft(SelectCommand *selection);
bool32 isSelectionExclusivlyStrikeCraftorNoMoveAndAttackShips(SelectCommand *selection);
void removeShipFromMpHyperspaceing(Ship *ship);

/*=============================================================================
    Tweakables:
=============================================================================*/

#define DISTMINRATIO        1.0f
real32 DISTMAXRATIO = 5.0f;
real32 SHRINKMIN = 0.1f;
real32 SHRINKMAX = 1.0f;

real32 MAX_DIST_JOIN_GROUP_SQR = (2000.0f*2000.0f);
real32 PROTECT_GET_THIS_CLOSE_CAP = 2000.0f;
real32 PROTECT_GET_THIS_CLOSE_STRIKE = 2000.0f;
real32 PROTECT_STILL_VELOCITY = 20.0f;

real32 WINGMAN_JOIN_MAX_DIST     = (200.0f*200.0f);

real32 FIGHTER_MAX_THREAT_DIST[NUM_TACTICS_TYPES] = { (2000.0f*2000.0f),(2000.0f*2000.0f),(2000.0f*2000.0f) };

udword FIGHTER_PAIRUP_PROB       = 255;

udword FIGHTER_PAIR_CHECK_RATE[NUM_TACTICS_TYPES] = { 7,7,7 };
udword FIGHTER_SINGLE_CHECK_RATE[NUM_TACTICS_TYPES] = { 31,31,31 };

udword MULTIPLETARGETS_RETARGET_RATE = 31;

bool8 HYPERSPACE = FALSE;

real32 GLOBAL_SHIP_HEALTH_MODIFIER = 1.0f;

real32 RESCONTROLLER_FOLLOW_BEHIND = 1.5f;

extern sdword FIGHTER_BREAK_ANGLE_MIN;
extern sdword FIGHTER_BREAK_ANGLE_MAX;

extern sdword FIGHTER_BREAK_VERTICAL_ANGLE_MIN;
extern sdword FIGHTER_BREAK_VERTICAL_ANGLE_MAX;

extern bool32 BombersUseBombingRun;

extern real32 FLY_INTO_WORLD_PERCENT_DIST;

scriptEntry CommandLayerTweaks[] =
{
    makeEntry(MAX_DIST_JOIN_GROUP_SQR,scriptSetReal32SqrCB),
    makeEntry(PROTECT_GET_THIS_CLOSE_CAP,scriptSetReal32CB),
    makeEntry(PROTECT_GET_THIS_CLOSE_STRIKE,scriptSetReal32CB),
    makeEntry(PROTECT_STILL_VELOCITY,scriptSetReal32CB),
    makeEntry(HYPERSPACE,scriptSetBool8),
    makeEntry(WINGMAN_JOIN_MAX_DIST,scriptSetReal32SqrCB),
    makeEntry(FIGHTER_PAIRUP_PROB,scriptSetUdwordCB),
    makeEntry(FIGHTER_PAIR_CHECK_RATE[Evasive],scriptSetUdwordCB),
    makeEntry(FIGHTER_PAIR_CHECK_RATE[Neutral],scriptSetUdwordCB),
    makeEntry(FIGHTER_PAIR_CHECK_RATE[Aggressive],scriptSetUdwordCB),
    makeEntry(FIGHTER_SINGLE_CHECK_RATE[Evasive],scriptSetUdwordCB),
    makeEntry(FIGHTER_SINGLE_CHECK_RATE[Neutral],scriptSetUdwordCB),
    makeEntry(FIGHTER_SINGLE_CHECK_RATE[Aggressive],scriptSetUdwordCB),
    makeEntry(FIGHTER_MAX_THREAT_DIST[Evasive],scriptSetReal32SqrCB),
    makeEntry(FIGHTER_MAX_THREAT_DIST[Neutral],scriptSetReal32SqrCB),
    makeEntry(FIGHTER_MAX_THREAT_DIST[Aggressive],scriptSetReal32SqrCB),
    makeEntry(MULTIPLETARGETS_RETARGET_RATE,scriptSetUdwordCB),
    makeEntry(GLOBAL_SHIP_HEALTH_MODIFIER,scriptSetReal32CB),
    makeEntry(FIGHTER_BREAK_ANGLE_MIN,scriptSetSdwordCB),
    makeEntry(FIGHTER_BREAK_ANGLE_MAX,scriptSetSdwordCB),
    makeEntry(FIGHTER_BREAK_VERTICAL_ANGLE_MIN,scriptSetSdwordCB),
    makeEntry(FIGHTER_BREAK_VERTICAL_ANGLE_MAX,scriptSetSdwordCB),
    makeEntry(BombersUseBombingRun,scriptSetBool),
    makeEntry(DISTMAXRATIO,scriptSetReal32CB),
    makeEntry(SHRINKMIN,scriptSetReal32CB),
    makeEntry(SHRINKMAX,scriptSetReal32CB),
    makeEntry(FLY_INTO_WORLD_PERCENT_DIST,scriptSetReal32CB),
    makeEntry(RESCONTROLLER_FOLLOW_BEHIND,scriptSetReal32CB),

    END_SCRIPT_ENTRY
};

/*-----------------------------------------------------------------------------
    Name        : RemoveShipsFromCommand
    Description : removes ships from command
    Inputs      : command
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void RemoveShipsFromCommand(CommandToDo *command)
{
    sdword i;
    SelectCommand *selection = command->selection;

    for (i=0;i<selection->numShips;i++)
    {
        selection->ShipPtr[i]->command = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : RemoveShipFromCommand
    Description : removes a ship from command
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void RemoveShipFromCommand(Ship *ship)
{
    ship->command = NULL;
}

/*-----------------------------------------------------------------------------
    Name        : PrepareShipsForCommand
    Description : prepares ships for executing a new command
    Inputs      : command, rowClear - flag indicating if rightOfWay should be removed
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void PrepareShipsForCommand(CommandToDo *command,bool32 rowClear)
{
    sdword i;
    SelectCommand *selection = command->selection;
    Ship *ship;

    for (i=0;i<selection->numShips;i++)
    {
        ship = selection->ShipPtr[i];
        ship->command = command;
        ship->shipOrDerelict.catRetreating = FALSE;    // CAT: a new command ends the retreat (PrepareShipsForCommand 0x00441000)
        if ((rowClear) && (ship->shipOrDerelict.specialFlags & SPECIAL_rowGettingOutOfWay))
        {
            rowRemoveShipFromGettingOutOfWay(ship);
        }
        // CAT: a Leech given a special order restarts its state machine at state 14 (PrepareShipsForCommand 0x00441000;
        //      the same lines are inlined in every cl* creator)
        if ((ship->shiptype == sLeech) && (command->ordertype.order == COMMAND_SPECIAL))
        {
            ((LeechSpec *)ship->ShipSpecifics)->state = LEECH_Done;
        }
    }
    tacticsMakeShipsNotLookForOtherShips(command->selection);
}

/*-----------------------------------------------------------------------------
    Name        : PrepareOneShipForCommand
    Description : prepares ship for executing a new command
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void PrepareOneShipForCommand(Ship *ship,CommandToDo *command,bool32 rowClear)
{
    ship->command = command;
    ship->shipOrDerelict.catRetreating = FALSE;    // CAT: PrepareOneShipForCommand 0x00441080
    if ((rowClear) && (ship->shipOrDerelict.specialFlags & SPECIAL_rowGettingOutOfWay))
    {
        rowRemoveShipFromGettingOutOfWay(ship);
    }
}

/*-----------------------------------------------------------------------------
    Name        : InitShipsForAI
    Description : Initializes selection ships AI state variables
    Inputs      : fresh (if we're starting a new command, this should be TRUE
                  otherwise, it should be false
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void InitShipsForAI(SelectCommand *selection,bool32 fresh)
{
    sdword i;
    Ship *ship;

    for (i=0;i<selection->numShips;i++)
    {
        ship = selection->ShipPtr[i];
        ship->shipOrDerelict.aistate = 0;
        ship->aistateattack = 0;
        if(fresh)
            ship->aistatecommand = 0;
    }
}

void InitShipsForAIDontResetAttack(SelectCommand *selection,bool32 fresh)
{
    sdword i;
    Ship *ship;

    for (i=0;i<selection->numShips;i++)
    {
        ship = selection->ShipPtr[i];
        ship->shipOrDerelict.aistate = 0;
        if(fresh)
            ship->aistatecommand = 0;
    }
}

/*-----------------------------------------------------------------------------
    Name        : getRangeToClosestTarget
    Description : REturns the square of the range from command selection to its closest target
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
real32 getRangeToClosestTarget(CommandToDo *command)
{
    vector commandPosition;
    real32 tempSqr,minDistanceSqr = REALlyBig;
    vector distvec;
    sdword i;

    dbgAssertOrIgnore(command->selection->numShips > 0);
    dbgAssertOrIgnore(command->ordertype.order == COMMAND_ATTACK);

    commandPosition = command->selection->ShipPtr[0]->obj.posinfo.position;

    for(i=0;i<command->attack->numTargets;i++)
    {
        vecSub(distvec,commandPosition,command->attack->TargetPtr[0]->obj.posinfo.position);
        tempSqr = vecMagnitudeSquared(distvec);
        if(tempSqr < minDistanceSqr)
        {
            minDistanceSqr = tempSqr;
        }
    }
    return minDistanceSqr;
}

/*-----------------------------------------------------------------------------
    Name        : InitShipAI
    Description : Initializes ship's AI state variables
    Inputs      : if fresh is TRUE, aicommand will be set (should only be true if
                  this is called from a new command generation function
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void InitShipAI(Ship *ship,bool32 fresh)
{
    ship->shipOrDerelict.aistate = 0;
    ship->aistateattack = 0;
    if(fresh)
        ship->aistatecommand = 0;
}

/*-----------------------------------------------------------------------------
    Name        : protectShip
    Description : ship "protects" or flys nearby protectThisShip
    Inputs      : ship, protectThisShip
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
#define COLLECTOR_STEADY_DIST   3000.0f
void protectShip(Ship *ship, Ship *protectThisShip, bool32 passiveAttacked)
{
    vector diff;
    real32 dist;
    real32 desiredrange;
    real32 shipSize = ship->staticinfo->common.staticCollInfo.collspheresize;
    real32 protectThisShipSize = protectThisShip->staticinfo->common.staticCollInfo.collspheresize;
    bool32 dontrotate;
    vector protectPoint = protectThisShip->obj.posinfo.position;
    vector heading;

    if (protectThisShipSize < shipSize)
    {
        protectThisShipSize = shipSize;     // if guarding a smaller ship, give more room
    }

    // CAT: protectShip 0x004411c0 keeps the capital range for both grav-well generators and no cloak generator
    // CAT (M2-R9 oracle-rung-physics): the exe's x87 shape (0x0044121b .. 0x00441236): (range + the larger size) +
    //      the ship's size, a double rounded once at its store
    if(isCapitalShip(ship) || ship->shiptype == kGravWellGenerator || ship->shiptype == tGravWellGenerator)
    {
        desiredrange = (real32)(((double)PROTECT_GET_THIS_CLOSE_CAP + protectThisShipSize) + shipSize);
    }
    else
    {
        desiredrange = (real32)(((double)PROTECT_GET_THIS_CLOSE_STRIKE + protectThisShipSize) + shipSize);
    }

    // Let's have some ships like the ResourceController follow behind the ship so we don't accidentally block them.
    // CAT: the ships that follow behind are the processors (sProcessor 16, bProcessor 51; protectShip 0x004411c0)
    // CAT (M2-R9 oracle-rung-physics): followbehind stays a double and each component is point - heading * it, one
    //      rounding (0x00441245 .. 0x0044128b)
    if ((ship->shiptype == sProcessor) || (ship->shiptype == bProcessor))
    {
        double followbehind = (double)RESCONTROLLER_FOLLOW_BEHIND * desiredrange;      // follow behind 1.5 radii
        matGetVectFromMatrixCol3(heading,protectThisShip->rotatable.rotinfo.coordsys);
        protectPoint.x = (real32)(protectPoint.x - heading.x * followbehind);
        protectPoint.y = (real32)(protectPoint.y - heading.y * followbehind);
        protectPoint.z = (real32)(protectPoint.z - heading.z * followbehind);
    }

    vecSub(diff,ship->obj.posinfo.position,protectPoint);
    dist = getVectDistSloppy(diff);

//    ship->shipidle = FALSE;

    dontrotate = ((passiveAttacked) & (bool32)((ShipStaticInfo *)ship->staticinfo)->rotateToRetaliate);

    // CAT: no DDDFrigate (protectShip 0x004411c0 / protectShipsAvg 0x004413f0 have no DDDFrigateMakeSureItCanGuard)

    if (protectThisShip->dockvars.dockship == ship)     // if docking with me
    {
        if(MoveReachedDestinationVariable(ship,&protectThisShip->obj.posinfo.position,COLLECTOR_STEADY_DIST))
        {
            //within range of steadyness
            goto juststeady;
        }
    }

    if (dist < desiredrange)
    {
        if ((ship->shipOrDerelict.specialFlags & SPECIAL_CircularGuard) ||   // CAT: specialFlags in the exe
            (getVectDistSloppy(protectThisShip->obj.posinfo.velocity) < PROTECT_STILL_VELOCITY) )
        {
juststeady:
            if (dontrotate)
            {
                aitrackSteadyShipDriftOnly(ship);
            }
            else
            {
                aitrackSteadyShip(ship);
            }
        }
        else
        {
            // just maintain velocity
            if (dontrotate)
            {
                if (aishipFlyToPointAvoidingObjsWithVel(ship,NULL,AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured,
                                                        0.0f,&protectThisShip->obj.posinfo.velocity) & AISHIP_FLY_OBJECT_IN_WAY)
                {
                    aitrackSteadyShipDriftOnly(ship);
                }
            }
            else
            {
                if (aishipFlyToPointAvoidingObjsWithVel(ship,NULL,AISHIP_PointInDirectionFlying|AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured,
                                                        0.0f,&protectThisShip->obj.posinfo.velocity) & AISHIP_FLY_OBJECT_IN_WAY)
                {
                    aitrackSteadyShip(ship);
                }
            }
        }
    }
    else
    {
        // fly to protectThisShip
        if (dontrotate)
        {
            if (aishipFlyToPointAvoidingObjsWithVel(ship,&protectPoint,AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured,
                                                   0.0f,&protectThisShip->obj.posinfo.velocity) & AISHIP_FLY_OBJECT_IN_WAY)
            {
                aitrackSteadyShipDriftOnly(ship);
            }
        }
        else
        {
            if (aishipFlyToPointAvoidingObjsWithVel(ship,&protectPoint,AISHIP_PointInDirectionFlying|AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured,
                0.0f,&protectThisShip->obj.posinfo.velocity) & AISHIP_FLY_OBJECT_IN_WAY)
            {
                aitrackSteadyShip(ship);
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : protectShipsAvg
    Description : ship "protects" or flys nearby median position of protectships
    Inputs      : ship, protectThisShip
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void protectShipsAvg(Ship *ship,ProtectCommand *protectShips,bool32 passiveAttacked)
{
    vector diff;
    real32 dist;
    real32 desiredrange;
    double desiredranged;
    real32 shipSize = ship->staticinfo->common.staticCollInfo.collspheresize;
    bool32 dontrotate;
    sdword numShips = protectShips->numShips;
    vector temp,guardposition;
    sdword i;
    vector protectShipsAvgVel;
    real32 realtemp;
    real32 minx,maxx,miny,maxy,minz,maxz;
    bool32 anyshipdockingwithme;

    dbgAssertOrIgnore(numShips > 1);

    temp = protectShips->ShipPtr[0]->obj.posinfo.position;
    minx = maxx = temp.x;
    miny = maxy = temp.y;
    minz = maxz = temp.z;

    protectShipsAvgVel = protectShips->ShipPtr[0]->obj.posinfo.velocity;

    for (i=1;i<numShips;i++)
    {
        temp = protectShips->ShipPtr[i]->obj.posinfo.position;
        vecAddTo(protectShipsAvgVel,protectShips->ShipPtr[i]->obj.posinfo.velocity);

        if (temp.x < minx) minx = temp.x;
        else if (temp.x > maxx) maxx = temp.x;

        if (temp.y < miny) miny = temp.y;
        else if (temp.y > maxy) maxy = temp.y;

        if (temp.z < minz) minz = temp.z;
        else if (temp.z > maxz) maxz = temp.z;
    }

    guardposition.x = (minx + maxx) * 0.5f;
    guardposition.y = (miny + maxy) * 0.5f;
    guardposition.z = (minz + maxz) * 0.5f;

    // CAT (M2-R9 oracle-rung-physics): 1 / numShips is a double (fild, fdivr 1.0f) and each component is rounded once
    //      (protectShipsAvg 0x00441587 .. 0x004415a9)
    {
        double inv = 1.0 / (double)numShips;

        protectShipsAvgVel.x = (real32)(inv * protectShipsAvgVel.x);
        protectShipsAvgVel.y = (real32)(inv * protectShipsAvgVel.y);
        protectShipsAvgVel.z = (real32)(inv * protectShipsAvgVel.z);
        (void)realtemp;
    }

    // we now have the optimum guardposition and protectShipsAvgVel

    vecSub(diff,ship->obj.posinfo.position,guardposition);
    dist = getVectDistSloppy(diff);

    // CAT: protectShipsAvg 0x004413f0: capital range for both races' grav-well and cloak generators (64, 79, 63, 77)
    // CAT (M2-R9 oracle-rung-physics): range + size stays a double on the FPU and is compared unrounded with the
    //      distance (0x00441611 .. 0x00441654)
    if(isCapitalShip(ship) || ship->shiptype == kGravWellGenerator || ship->shiptype == tGravWellGenerator ||
       ship->shiptype == kCloakGenerator || ship->shiptype == tCloakGenerator)
    {
        desiredranged = (double)PROTECT_GET_THIS_CLOSE_CAP + shipSize;
    }
    else
    {
        desiredranged = (double)PROTECT_GET_THIS_CLOSE_STRIKE + shipSize;
    }
    desiredrange = (real32)desiredranged;

//    ship->shipidle = FALSE;

    dontrotate = ((passiveAttacked) & (bool32)((ShipStaticInfo *)ship->staticinfo)->rotateToRetaliate);

    // CAT: no DDDFrigate (protectShip 0x004411c0 / protectShipsAvg 0x004413f0 have no DDDFrigateMakeSureItCanGuard)

    anyshipdockingwithme = FALSE;

    // CAT: the exe checks every protected ship, with no canReceiveSomething gate (protectShipsAvg 0x004413f0)
    for (i=0;i<numShips;i++)
    {
        if (protectShips->ShipPtr[i]->dockvars.dockship == ship)
        {
            anyshipdockingwithme = TRUE;
            break;
        }
    }

    if (anyshipdockingwithme)     // if docking with me
    {
        goto juststeady;
    }

    if ((double)dist < desiredranged)
    {
        if ((ship->shipOrDerelict.specialFlags & SPECIAL_CircularGuard) ||   // CAT: specialFlags in the exe
            (getVectDistSloppy(protectShipsAvgVel) < PROTECT_STILL_VELOCITY))
        {
juststeady:
            if (dontrotate)
            {
                aitrackSteadyShipDriftOnly(ship);
            }
            else
            {
                aitrackSteadyShip(ship);
            }
        }
        else
        {
            // just maintain velocity
            if (dontrotate)
            {
                if (aishipFlyToPointAvoidingObjsWithVel(ship,NULL,AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured,
                                                        0.0f,&protectShipsAvgVel) & AISHIP_FLY_OBJECT_IN_WAY)
                {
                    aitrackSteadyShipDriftOnly(ship);
                }
            }
            else
            {
                if (aishipFlyToPointAvoidingObjsWithVel(ship,NULL,AISHIP_PointInDirectionFlying|AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured,
                                                        0.0f,&protectShipsAvgVel) & AISHIP_FLY_OBJECT_IN_WAY)
                {
                    aitrackSteadyShip(ship);
                }
            }
        }
    }
    else
    {
        // fly to protectThisShip
        if (dontrotate)
        {
            if (aishipFlyToPointAvoidingObjsWithVel(ship,&guardposition,AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured,
                                                   0.0f,&protectShipsAvgVel) & AISHIP_FLY_OBJECT_IN_WAY)
            {
                aitrackSteadyShipDriftOnly(ship);
            }
        }
        else
        {
            if (aishipFlyToPointAvoidingObjsWithVel(ship,&guardposition,AISHIP_PointInDirectionFlying|AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured,
                0.0f,&protectShipsAvgVel) & AISHIP_FLY_OBJECT_IN_WAY)
            {
                aitrackSteadyShip(ship);
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : processLaunchShipToDo
    Description : processes a launch ship command
    Inputs      :
    Outputs     :
    Return      : returns TRUE if ship has finished launching
----------------------------------------------------------------------------*/
bool32 processLaunchShipToDo(CommandToDo *launchtodo)
{
    Ship *ship = launchtodo->selection->ShipPtr[0];
    Ship *receiverShip = launchtodo->launchship.receiverShip;

    dbgAssertOrIgnore(launchtodo->selection->numShips == 1);
    dbgAssertOrIgnore(receiverShip);

    // CAT (M2-R5 dock): processLaunchShipToDo 0x00466b00: launching one of the four sMothership modules that sit under
    //      the ore-can armour ejects every sMothershipOreCanArmour clamped to the host (cat_moduleShipEject, 7 s delay,
    //      10 s); then the launch purpose's state machine; no HW1 hyperspace waits
    if ((ship->shiptype == sMothershipSpecial) || (ship->shiptype == sMothershipArmour) ||
        (ship->shiptype == sMothershipMicro) || (ship->shiptype == sMothershipWeapons))
    {
        if ((receiverShip->guidance.clampInfo != NULL) && (receiverShip->guidance.clampInfo->clampedObjs != NULL))
        {
            CatClampedList *clamped = receiverShip->guidance.clampInfo->clampedObjs;
            sdword i;

            for (i = 0; i < clamped->numShips; i++)
            {
                if (clamped->ShipPtr[i]->shiptype == sMothershipOreCanArmour)
                {
                    cat_moduleShipEject(clamped->ShipPtr[i],TRUE,7.0f,10.0f);      // 0x0087929c / 0x008792a0
                }
                clamped = receiverShip->guidance.clampInfo->clampedObjs;          // the exe re-reads the list each pass
            }
        }
    }
    if (cat_dockShipDocksAtShip(ship,receiverShip,CAT_DOCK_LAUNCH))
    {
        RemoveShipFromLaunching(ship);                      // the exe inlines it
        return TRUE;
    }
    return FALSE;
}

bool32 isThisShipThreateningMe(SpaceObjRotImpTarg *target,Ship *me)
{
    SpecificAttackVars *targetattackvars;
    Ship *targetsLeader;
    Ship *targetsWingman;

    if (target->header.objtype != OBJ_ShipType)
    {
        return FALSE;       // non-ship objects are not considered threatening
    }

    targetattackvars = &((Ship *)target)->attackvars;

    // check if target is attacking me
    if (targetattackvars->attacktarget == (SpaceObjRotImpTarg *)me)
    {
        if (targetattackvars->attacksituation == 0) return TRUE; else return FALSE;
    }

    // check if target's leader or wingman is attacking me
    if ((targetsLeader = targetattackvars->myLeaderIs) != NULL)
    {
        dbgAssertOrIgnore(targetattackvars->myWingmanIs == NULL);
        if (targetsLeader->attackvars.attacktarget == (SpaceObjRotImpTarg *)me)
        {
            if (targetattackvars->attacksituation == 0) return TRUE; else return FALSE;
        }
    }
    else if ((targetsWingman = targetattackvars->myWingmanIs) != NULL)
    {
        dbgAssertOrIgnore(targetattackvars->myLeaderIs == NULL);
        if (targetsWingman->attackvars.attacktarget == (SpaceObjRotImpTarg *)me)
        {
            if (targetattackvars->attacksituation == 0) return TRUE; else return FALSE;
        }
    }

    return FALSE;
}

SpaceObjRotImpTarg *FindNewThreateningTarget(Ship *ship,AttackCommand *attack,SpaceObjRotImpTarg *oldtarget)
{
    sdword numShipsToAttack = attack->numTargets;
    SpaceObjRotImpTarg *newtarget;
    vector dist;
    real32 distsqr;
    real32 mindist = REALlyBig;
    sdword mindistj = -1;
    sdword j;
    real32 fighterMaxThreatDist = FIGHTER_MAX_THREAT_DIST[ship->tacticstype];

    dbgAssertOrIgnore(ship->tacticstype < NUM_TACTICS_TYPES);

    // for all ships/targets we are attacking
    for (j=0;j<numShipsToAttack;j++)
    {
        newtarget = attack->TargetPtr[j];
        if (newtarget == oldtarget)
        {
            continue;
        }
        // CAT: FindNewThreateningTarget 0x004417c0 (map: IDENTICAL): not a ship clamped to this ship (a Leech on it)
        if ((newtarget->header.objtype == OBJ_ShipType) && (((Ship *)newtarget)->guidance.clampInfo != NULL) &&
            (((Ship *)newtarget)->guidance.clampInfo->host == (SpaceObjRotImpTarg *)ship))
        {
            continue;
        }
        if (isThisShipThreateningMe(newtarget,ship))
        {
            vecSub(dist,newtarget->obj.posinfo.position,ship->obj.posinfo.position);
            distsqr = vecMagnitudeSquared(dist);

            if (distsqr < fighterMaxThreatDist)
            {
                if (distsqr < mindist)
                {
                    mindist = distsqr;
                    mindistj = j;
                }
            }
        }
    }

    if (mindistj != -1)
    {
        return attack->TargetPtr[mindistj];
    }
    else
    {
        return NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : EnemyIsBehindMe
    Description : returns TRUE if enemy is behind me
    Inputs      : enemy, ship
    Outputs     :
    Return      : returns TRUE if enemy is behind me
----------------------------------------------------------------------------*/
bool32 EnemyIsBehindMe(SpaceObjRotImpTarg *enemy,Ship *me)
{
    vector myheading;
    vector enemytome;

    matGetVectFromMatrixCol3(myheading,me->rotatable.rotinfo.coordsys);
    vecSub(enemytome,me->obj.posinfo.position,enemy->obj.posinfo.position);

    if (vecDotProduct(myheading,enemytome) > 0.0f)
    {
        // enemy is behind us
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : processSpecialToDo
    Description : processes a special
    Inputs      :
    Outputs     :
    Return      : returns TRUE if special has been completed
----------------------------------------------------------------------------*/
bool32 processSpecialToDo(CommandToDo *todo)
{
    SelectCommand *selection = todo->selection;
    sdword numShips = selection->numShips;
    Ship *ship;
    ShipStaticInfo *shipstaticinfo;
    bool32 alldone = TRUE;
    bool32 shipdone;
    sdword i;

    dbgAssertOrIgnore(numShips > 0);

    if (todo->specialtargets)
    {
        // do the special target command

        for (i=0;i<numShips;i++)
        {
            ship = selection->ShipPtr[i];
            shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);
            shipdone = TRUE;

            if (shipstaticinfo->custshipheader.CustShipSpecialTarget)
            {
                shipdone = shipstaticinfo->custshipheader.CustShipSpecialTarget(ship,todo->specialtargets);
            }

            alldone &= shipdone;
        }
        return alldone;
    }
    else
    {
        // do a continuous special activate command

        for (i=0;i<numShips;i++)
        {
            ship = selection->ShipPtr[i];
            shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);
            shipdone = TRUE;

            dbgAssertOrIgnore(shipstaticinfo->specialActivateIsContinuous);

            if (shipstaticinfo->custshipheader.CustShipSpecialActivate)
            {
                shipdone = shipstaticinfo->custshipheader.CustShipSpecialActivate(ship);
            }

            alldone &= shipdone;
        }
        return alldone;
    }
}

/*-----------------------------------------------------------------------------
    Name        : processHaltToDo
    Description : returns TRUE if halt command is finished
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 processHaltToDo(CommandToDo *command,bool32 passiveAttacked)
{
    SelectCommand *selection = command->selection;
    sdword numShips = selection->numShips;
    bool32 done = TRUE;
    sdword i;
    Ship *ship;

    for (i=0;i<numShips;i++)
    {
        ship = selection->ShipPtr[i];
        if ((passiveAttacked) & (bool32)((ShipStaticInfo *)ship->staticinfo)->rotateToRetaliate)
        {
            done &= aitrackSteadyShipDriftOnly(ship);
        }
        else
        {
            done &= aitrackSteadyShip(ship);
        }
    }

    return done;
}

/*-----------------------------------------------------------------------------
    Name        : ShipsOutOfPassiveAttackRange
    Description : returns TRUE if ships being attacked are out of selection's passive attack range
    Inputs      :
    Outputs     :
    Return      : returns TRUE if ships being attacked are out of selection's passive attack range
----------------------------------------------------------------------------*/
bool32 ShipsInPassiveAttackRange(SelectCommand *selection,AttackCommand *attack)
{
    sdword numShips = selection->numShips;
    sdword numShipsToAttack = attack->numTargets;
    sdword i;
    vector avgposition = { 0.0f, 0.0f, 0.0f };
    Ship *ship;
    vector diff;
    real32 retaliateZone;
    real32 negRetaliateZone;
    bool32 aShipInRange = FALSE;
    real32 temp;

    dbgAssertOrIgnore(numShips > 0);
    dbgAssertOrIgnore(numShipsToAttack > 0);

    for (i=0;i<numShipsToAttack;i++)
    {
        vecAddTo(avgposition,attack->TargetPtr[i]->obj.posinfo.position);
    }

    vecDivideByScalar(avgposition,(real32)numShipsToAttack,temp);

    for (i=0;i<numShips;i++)
    {
        ship = selection->ShipPtr[i];
        retaliateZone = ship->staticinfo->passiveRetaliateZone;
        negRetaliateZone = -retaliateZone;

        vecSub(diff,ship->obj.posinfo.position,avgposition);

        if (isBetweenExclusive(diff.x,negRetaliateZone,retaliateZone) &&
            isBetweenExclusive(diff.y,negRetaliateZone,retaliateZone) &&
            isBetweenExclusive(diff.z,negRetaliateZone,retaliateZone) )
        {
            aShipInRange = TRUE;
        }
    }

    return aShipInRange;
}

/*-----------------------------------------------------------------------------
    Name        : processPassiveAttackToDo
    Description : processes a passive attack command (attack without moving)
    Inputs      :
    Outputs     :
    Return      : returns TRUE if passive attack has been completed
----------------------------------------------------------------------------*/
bool32 processPassiveAttackToDo(CommandToDo *attacktodo)
{
    SelectCommand *selection = attacktodo->selection;
    AttackCommand *attack = attacktodo->attack;
    sdword numShips = selection->numShips;
    sdword numShipsToAttack = attack->numTargets;
    Ship *ship;
    ShipStaticInfo *shipstaticinfo;
    SpecificAttackVars *attackvars;
    SpaceObjRotImpTarg *target;
    sdword i;
    bool32 rotate;

    dbgAssertOrIgnore(numShips > 0);

    if ((numShipsToAttack == 0) || (!ShipsInPassiveAttackRange(selection,attack))
        || ((singlePlayerGame) && (singlePlayerGameInfo.hyperspaceState != NO_HYPERSPACE ) ))
    {
#if DEBUG_PASSIVE_ATTACK
        if (numShipsToAttack != 0)
        {
            dbgMessagef("Dropping out of passive attack");
        }
#endif

        for (i=0;i<numShips;i++)
        {
            soundEventBurstStop(selection->ShipPtr[i],NULL);
        }

        return TRUE;
    }

//fix later and optimize by passing in a rotate flag
    if(attacktodo->ordertype.order == COMMAND_MOVE ||
        attacktodo->ordertype.order == COMMAND_DOCK)
        rotate = FALSE;
    else
        rotate = TRUE;

    for (i=0;i<numShips;i++)
    {
        ship = selection->ShipPtr[i];

        shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);
        attackvars = &ship->attackvars;
        target = attackvars->attacktarget;

        if (target == NULL)
        {
            soundEventBurstStop(ship,NULL);
            attackvars->attacktarget = target = AttackClosestOpponent(ship,attack);
            InitShipAI(ship,FALSE);
        }

        if (attackvars->multipleAttackTargets)
        {
            dbgAssertOrIgnore(shipstaticinfo->canTargetMultipleTargets);

            if (UNIVERSE_WOODPECKER(MULTIPLETARGETS_RETARGET_RATE, ship->shipID.shipNumber & MULTIPLETARGETS_RETARGET_RATE))
            {
                pickMultipleAttackTargets(ship,attack);
            }
        }
        // CAT: no Evasive exception (processPassiveAttackToDo 0x00441ab0: every ship passive attacks; HW1 only turned
        //      an Evasive ship towards its target)
        if (shipstaticinfo->custshipheader.CustShipAttackPassive)
        {
            //dbgAssertOrIgnore(target->header.objtype == OBJ_ShipType);     // only target SHIPs should be passive attacked
            shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)target,rotate);
        }
        attackvars->attackevents = 0;       // clear any events we received
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : processAttackToDo
    Description : processes an attack command
    Inputs      :
    Outputs     :
    Return      : returns TRUE if attack has been completed
----------------------------------------------------------------------------*/
bool32 processAttackToDo(CommandToDo *attacktodo)
{
    sdword i;
    SelectCommand *selection = attacktodo->selection;
    AttackCommand *attack = attacktodo->attack;
    sdword numShips = selection->numShips;
    sdword numShipsToAttack = attack->numTargets;
    Ship *ship;
    ShipStaticInfo *shipstaticinfo;
    SpecificAttackVars *attackvars;
    bool32 alldone;
    SpaceObjRotImpTarg *target;
    udword checkbogeyrate;
    SpaceObjRotImpTarg *newtarget;
    bool32 fighterpair;
    udword flightman;
    Ship *myLeader;
    sdword attackAndMoving;


    dbgAssertOrIgnore(numShips > 0);

    // CAT (M2-R6 nis-fow-camera): the attack needs a target its player can see (processAttackToDo 0x00441c10,
    //      0x00441c34..0x00441ce7): a crystal ship counts, else cat_fowCanPlayerSeeShip(owner, target, 1) 0x0047e810 (the
    //      loop rereads the target count). With none seen a multiplayer game halts the ships (clWrapHalt) and goes on;
    //      a single-player one treats the order as having no target left, unless the leader is SU-exempt
    //      (SPECIAL_2_CatExempt20). A recorded multiplayer game halts too (0x00a7519c && 0x00a75190)
    if (numShipsToAttack > 0)
    {
        Player *owner = selection->ShipPtr[0]->playerowner;
        bool32 seen = FALSE;

        for (i = 0; (i < attack->numTargets) && !seen; i++)
        {
            target = attack->TargetPtr[i];
            if (((target->header.objtype == OBJ_ShipType) && cat_shipIsCrystal((Ship *)target)) ||
                cat_fowCanPlayerSeeShip(owner, (Ship *)target, TRUE))
            {
                seen = TRUE;
            }
        }
        if (!seen)
        {
            if (catNetworkGame())
            {
                clWrapHalt(&universe.mainCommandLayer, selection);
            }
            else if (!bitTest(selection->ShipPtr[0]->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
            {
                numShipsToAttack = 0;
            }
        }
    }

    if (numShipsToAttack == 0)
    {                                                               // don't do holding pattern for swarmers (don't want them to go into delta formation)
        if(isSelectionExclusivlyStrikeCraft(attacktodo->selection))     // CAT: no HW1 P2 swarmers (M2-R4 ships-b)
        {
            attacktodo->holdingStartTime = universe.totaltimeelapsed;
            //use leaders heading for holding pattern
            attacktodo->turnAroundTimes = 0;
            //use leaders heading for holding pattern
            matGetVectFromMatrixCol3(attacktodo->holdingPatternVec,selection->ShipPtr[0]->rotatable.rotinfo.coordsys);
            vecScalarMultiply(attacktodo->holdingPatternVec,attacktodo->holdingPatternVec,TW_HOLDING_PATTERN_PATH_DISTANCE);
            for (i=0;i<selection->numShips;i++)
            {
                //save individual positions
                attacktodo->selection->ShipPtr[i]->holdingPatternPos = selection->ShipPtr[i]->obj.posinfo.position;
                soundEventBurstStop(selection->ShipPtr[i],NULL);
            }
            ////////////////////
            //Speech Event for end of attack
            //event num:  STAT_Strike_AttackComplete
            //use battlechatter
            if(attacktodo->selection->ShipPtr[0]->playerowner->playerIndex == universe.curPlayerIndex)
            {
                if (battleCanChatterAtThisTime(BCE_STAT_Strike_AttackComplete, attacktodo->selection->ShipPtr[0]))
                {
                    battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_STAT_Strike_AttackComplete, attacktodo->selection->ShipPtr[0], SOUND_EVENT_DEFAULT);
                }
            }
            ///////////////
            attacktodo->ordertype.attributes |= COMMAND_MASK_HOLDING_PATTERN;
            return TRUE;    //return to do the holding pattern
        }
        alldone = TRUE;

        // we've killed all the ships, so let's stop movement and wait for next command
        for (i=0;i<selection->numShips;i++)
        {
            //save individual positions
            attacktodo->selection->ShipPtr[i]->holdingPatternPos = selection->ShipPtr[i]->obj.posinfo.position;
            soundEventBurstStop(selection->ShipPtr[i],NULL);
        }
        if (attacktodo->ordertype.attributes)
        {
            // something else to do afterwards, so return TRUE without bothering to stabilize ships
            return TRUE;
        }

        for (i=0;i<selection->numShips;i++)
        {
            ship = selection->ShipPtr[i];
            if (!aitrackSteadyShip(ship))
            {
                alldone = FALSE;
            }
        }

        return alldone;

    }

    //check attack conditions as to whether or not we need to
    //go back to guarding a ship...

    if((tacticsInfo.tacticsGuardConditionCheckRate & universe.univUpdateCounter) == tacticsInfo.tacticsGuardConditionCheckFrame)
    {
        if(attacktodo->ordertype.attributes & COMMAND_MASK_PROTECTING)
        if(tacticsCheckGuardConditionsDuringAttack(attacktodo))
        {
            for (i=0;i<numShips;i++)
            {
                soundEventBurstStop(selection->ShipPtr[i],NULL);
            }
            return TRUE;
        }
    }

    attackAndMoving = FALSE;
    if(attacktodo->ordertype.attributes & COMMAND_MASK_ATTACKING_AND_MOVING)
    {
        //specialized movement + combat
        attackAndMoving = TRUE;             //set flag for future movement reference
        // CAT: each ship along the move path (processAttackToDo 0x00441c10 calls processMoveToDo per ship)
        for (i=0;i<attacktodo->selection->numShips;i++)
        {
            processMoveToDo(attacktodo,TRUE,i,FALSE);   //move the group to the new attack location
        }

    }

    for (i=0;i<numShips;i++)
    {
        ship = selection->ShipPtr[i];
        shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);
        attackvars = &ship->attackvars;
        target = attackvars->attacktarget;

        if(!isShipStaticInterceptor(shipstaticinfo))     // CAT: no HW1 DefenseFighter (M2-R4 ships-b)
        {
            if (target == NULL)
            {
                soundEventBurstStop(ship,NULL);
                attackvars->attacktarget = target = AttackClosestOpponent(ship,attack);
                InitShipAI(ship,FALSE);
            }

            if (attackvars->multipleAttackTargets)
            {
                dbgAssertOrIgnore(shipstaticinfo->canTargetMultipleTargets);

                if (UNIVERSE_WOODPECKER(MULTIPLETARGETS_RETARGET_RATE, ship->shipID.shipNumber & MULTIPLETARGETS_RETARGET_RATE))
                {
                    pickMultipleAttackTargets(ship,attack);
                }
            }

            goto doattack;
        }
        else
        {
            // if we are an interceptor then things get more complicated...

            // execute any flight maneuvers
            if (attackvars->flightmansLeft)
            {
                if (flightmanExecute(ship))
                {
                    attackvars->flightmansLeft--;
                    if (attackvars->flightmansLeft >= 1)
                    {
                        // select another evading maneuver
                        flightmanInit(ship,flightmanGetRandom(GenericInterceptorGetFlightManProb(ship,target),FLIGHTMAN_EVASIVE_PURE));
                    }
                    else
                    {
                        attackvars->attacksituation = 0;
                    }
                }
                goto skipattack;
            }
            else
            {
                dbgAssertOrIgnore(attackvars->attacksituation == 0);
            }

            // if we have a leader
            if(ship->tacticstype != Aggressive) //only do wingman stuff for evasive
            if ((myLeader = attackvars->myLeaderIs) != NULL)
            {
                SpaceObjRotImpTarg *myLeaderTarget;
                bool32 shouldrotate = TRUE;

                dbgAssertOrIgnore(attackvars->myWingmanIs == NULL);

                if (attackvars->attacktarget != NULL)
                {
                    goto doattack;
                }

                // CAT: no HW1 DefenseFighter test (M2-R4 ships-b)
                {
                    if ((myLeaderTarget = myLeader->attackvars.attacktarget) != NULL)
                    {
                        // CAT (M2-R5 layout-dock): a wingman whose leader attacks a module-carrying mothership aims at
                        //      the nearest module instead (cat_gunPickMothershipModuleTarget 0x0048b950, Gun.c)
                        myLeaderTarget = cat_gunPickMothershipModuleTarget(ship, myLeaderTarget);
                        if (shipstaticinfo->custshipheader.CustShipAttackPassive)
                        {
                            vector leadhead;
                            vector othervec;

                            matGetVectFromMatrixCol3(leadhead,myLeader->rotatable.rotinfo.coordsys);
                            vecSub(othervec,ship->obj.posinfo.position,myLeaderTarget->obj.posinfo.position);
                            if(vecDotProduct(othervec,leadhead) < 0)
                            {
                                shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)myLeaderTarget,TRUE);
                                shouldrotate = FALSE;
                            }
                        }
                    }
                    else
                    {
                        soundEventBurstStop(ship,NULL);
                    }
                }

                // track leader position
                formationWingmanTrackLeader(ship,myLeader,shouldrotate);

                goto skipattack;
            }

            if (((attackvars->myLeaderIs) || (attackvars->myWingmanIs)) && ship->tacticstype != Aggressive)
            {
                fighterpair = TRUE;
                checkbogeyrate = FIGHTER_PAIR_CHECK_RATE[ship->tacticstype];
            }
            else
            {
                fighterpair = FALSE;
                checkbogeyrate = FIGHTER_SINGLE_CHECK_RATE[ship->tacticstype];
            }

            if (target != NULL)
            {
                if (UNIVERSE_WOODPECKER(checkbogeyrate, CHECKBOGEY_FRAME & checkbogeyrate))
                {
                    if (!isThisShipThreateningMe(target,ship))
                    {
                        // we should switch targets to a target that is threatening me, if there is one
                        newtarget = FindNewThreateningTarget(ship,attack,target);
                        if (newtarget != NULL)
                        {
                            soundEventBurstStop(ship,NULL);
                            if ((fighterpair) && ((gamerand() & 255) < FLIGHTMAN_SPLIT_PROB[ship->tacticstype]))
                            {
                                Ship *wingman = attackvars->myWingmanIs;

                                dbgAssertOrIgnore(wingman);

                                wingman->attackvars.flightmansLeft = 1;
                                wingman->attackvars.attacksituation = ATTACKSITUATION_SPLITTING;
                                if (gamerand() & 1)
                                {
                                    flightmanInitWithFlags(wingman,FLIGHTMAN_IMMELMAN,IMMELMAN_INVERT);
                                }
                                else
                                {
                                    flightmanInit(wingman,FLIGHTMAN_HARDBANK);
                                }

                                attackvars->flightmansLeft = 1;
                                attackvars->attacksituation = ATTACKSITUATION_SPLITTING;
                                flightmanInitWithFlags(ship,FLIGHTMAN_IMMELMAN,0);
                            }
                            else
                            {
                                if ((gamerand() & 255) < FLIGHTMAN_EVASIVEPURE_PROB[ship->tacticstype])
                                {
                                    flightman = flightmanGetRandom(GenericInterceptorGetFlightManProb(ship,newtarget),FLIGHTMAN_EVASIVE_PURE);

                                    attackvars->flightmansLeft = FLIGHTMAN_NUM_EVASIVEPURE_MANEUVERS[ship->tacticstype];
                                    attackvars->attacksituation = ATTACKSITUATION_JUSTEVADING;
                                }
                                else
                                {
                                    // sensed threatening enemy attack
                                    if (EnemyIsBehindMe(newtarget,ship))
                                    {
                                        flightman = flightmanGetRandom(GenericInterceptorGetFlightManProb(ship,newtarget),FLIGHTMAN_EVASIVE_BEHIND);
                                    }
                                    else
                                    {
                                        flightman = flightmanGetRandom(GenericInterceptorGetFlightManProb(ship,newtarget),FLIGHTMAN_EVASIVE_FRONT);
                                    }
                                    attackvars->flightmansLeft = 1;
                                    attackvars->attacksituation = ATTACKSITUATION_EVADINGREPOS;
                                }
                                flightmanInit(ship,flightman);
                            }

                            attackvars->attacktarget = target = newtarget;
                            InitShipAI(ship,FALSE);
                            goto skipattack;
                        }
                        else
                        {
                            // no new threatening target, so keep attacking old one.
                            goto doattack;
                        }
                    }
                    else
                    {
                        // ship is threatening me, perhaps a token pure evasive maneuver if we are in Evasive tactics?
                        if ((ship->tacticstype == Evasive) && (UNIVERSE_WOODPECKER(FLIGHTMAN_TOKEN_EVASIVEPURE_CHECKRATE, FLIGHTMAN_TOKEN_EVASIVEPURE_CHECKFRAME)))
                        {
                            if ((gamerand() & 255) < FLIGHTMAN_TOKEN_EVASIVEPURE_PROB)
                            {
                                flightman = flightmanGetRandom(GenericInterceptorGetFlightManProb(ship,target),FLIGHTMAN_EVASIVE_PURE);

                                attackvars->flightmansLeft = FLIGHTMAN_NUM_EVASIVEPURE_MANEUVERS[ship->tacticstype];
                                attackvars->attacksituation = ATTACKSITUATION_JUSTEVADING;

                                flightmanInit(ship,flightman);

                                goto skipattack;
                            }
                        }

                        goto doattack;
                    }
                }
                else
                {
                    // just attack our target
                    goto doattack;
                }
            }
            else
            {
                // we don't currently have a target.  Let's pick one.
                soundEventBurstStop(ship,NULL);
                newtarget = FindNewThreateningTarget(ship,attack,target);
                if (newtarget != NULL)
                {
                    if ((gamerand() & 255) < FLIGHTMAN_EVASIVEPURE_PROB[ship->tacticstype])
                    {
                        flightman = flightmanGetRandom(GenericInterceptorGetFlightManProb(ship,newtarget),FLIGHTMAN_EVASIVE_PURE);

                        attackvars->flightmansLeft = FLIGHTMAN_NUM_EVASIVEPURE_MANEUVERS[ship->tacticstype];
                        attackvars->attacksituation = ATTACKSITUATION_JUSTEVADING;
                    }
                    else
                    {
                        // sensed threatening enemy attack
                        if (EnemyIsBehindMe(newtarget,ship))
                        {
                            flightman = flightmanGetRandom(GenericInterceptorGetFlightManProb(ship,newtarget),FLIGHTMAN_EVASIVE_BEHIND);
                        }
                        else
                        {
                            flightman = flightmanGetRandom(GenericInterceptorGetFlightManProb(ship,newtarget),FLIGHTMAN_EVASIVE_FRONT);
                        }
                        attackvars->flightmansLeft = 1;
                        attackvars->attacksituation = ATTACKSITUATION_EVADINGREPOS;
                    }

                    flightmanInit(ship,flightman);
                    attackvars->attacktarget = target = newtarget;
                    InitShipAI(ship,FALSE);

                    goto skipattack;
                }
                else
                {
                    // find a new target to pick on
                    attackvars->attacktarget = target = AttackClosestOpponent(ship,attack);
                    InitShipAI(ship,FALSE);

                    if ((fighterpair) && (target->header.objtype == OBJ_ShipType) &&
                        ((((Ship *)target)->attackvars.myWingmanIs == NULL) && (((Ship *)target)->attackvars.myLeaderIs == NULL)) &&
                        ((gamerand() & 255) < FLIGHTMAN_SANDWICH_PROB[ship->tacticstype]) )
                    {
                        Ship *wingman = attackvars->myWingmanIs;

                        dbgAssertOrIgnore(wingman);
                        wingman->attackvars.attacktarget = target;
                        InitShipAI(wingman,FALSE);

                        wingman->flightmanState2 = SANDWICH_RIGHT;
                        flightmanInitWithFlags(wingman,FLIGHTMAN_SANDWICH,(memsize)target);
                        wingman->attackvars.flightmansLeft = 1;
                        wingman->attackvars.attacksituation = ATTACKSITUATION_SANDWICH;

                        ship->flightmanState2 = SANDWICH_LEFT;
                        flightmanInitWithFlags(ship,FLIGHTMAN_SANDWICH,(memsize)target);
                        attackvars->flightmansLeft = 1;
                        attackvars->attacksituation = ATTACKSITUATION_SANDWICH;
                        goto skipattack;
                    }
                    else    // we didn't find a threatening enemy attack. Perhaps a celebratory barrel roll?
                    if (attackvars->attackevents & ATTACKEVENT_ENEMYTARGET_DIED)
                    {
                        flightmanInit(ship,FLIGHTMAN_BARREL_ROLL);
                        attackvars->flightmansLeft = 1;
//                        speechEvent(ship, BattleEnemyKilled, 0);
                        goto skipattack;
                    }

                    goto doattack;
                }

            } // end if target != NULL

        } // end if ship is Interceptor

doattack:
        if(attackAndMoving)
        {
            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else if (shipstaticinfo->custshipheader.CustShipAttackPassive)
            {
                shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)target,TRUE);
            }
        }
        else
        {
            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else if (shipstaticinfo->custshipheader.CustShipAttack)
            {
                shipstaticinfo->custshipheader.CustShipAttack(ship,target,ship->staticinfo->bulletRange[ship->tacticstype]);
            }
        }
skipattack:
        ship->shipidle = FALSE;
        attackvars->attackevents = 0;       // clear any events we received

    } // end for

    return FALSE;
}
/*-----------------------------------------------------------------------------
    Name        : fixWallFlipTurnSelection
    Description : Fixes selection when ships in WALL only formation attempt a flipturn
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

void fixWallFlipTurnSelection(SelectCommand *selection)
{
    sdword i;

    for(i=1;i<selection->numShips;i++)
    {
        selection->ShipPtr[i]->formationOffset.y = -selection->ShipPtr[i]->formationOffset.y;
    }

}
/*-----------------------------------------------------------------------------
    Name        : fixFlipTurnSelection
    Description : Fixes selection when ships in formation attempt a flipturn
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

void fixFlipTurnSelection(SelectCommand *selection,SelectCommand *global)
{
    sdword i;//,j,switcher;
    Ship *tempship;
    //vector tempvec,tempvec2; //temporary formation vector
    //j=0;
    for(i=1;i<selection->numShips;i++)
    {
        //save temporary ship
        tempship = selection->ShipPtr[i];

        /*
        tempvec = selection->ShipPtr[i]->formationOffset;

        i++;
        if(i >= selection->numShips)
        {
            */
            tempship->formationOffset.y = -tempship->formationOffset.y;
        /*    break;              //done
        }

        while(tempship != global->ShipPtr[j])
            j++;
        //while loop exits when j points to same ship as temp ship

        switcher = j;
        while(global->ShipPtr[j] != selection->ShipPtr[i])
            j++;
        //while loop exits when j points to other ship to switch

        //swap ships and continue;
        selection->ShipPtr[i-1] = selection->ShipPtr[i];
        tempvec2 = selection->ShipPtr[i]->formationOffset;
        selection->ShipPtr[i-1]->formationOffset = tempvec;

        selection->ShipPtr[i] = tempship;
        selection->ShipPtr[i]->formationOffset = tempvec2;

        global->ShipPtr[switcher] = selection->ShipPtr[i-1];
        global->ShipPtr[j] = tempship;
        //global->ShipPtr[switcher]->formationOffset = selection->ShipPtr[i-1]->formationOffset;
        //global->ShipPtr[j]->formationOffset = tempvec;
        */
    }
}
void formGroups(SelectCommand *selection,SelectCommand *selectionDefenders,SelectCommand *selectionCorvettes,SelectCommand *selectionCapitalShips,SelectCommand *selectionInterceptors)
{
    sdword a,i,j,k,l;
    selectionDefenders->numShips = 0;       //i
    selectionCorvettes->numShips = 0;       //j
    selectionCapitalShips->numShips = 0;    //k
    selectionInterceptors->numShips = 0;    //l
    i = j = k = l = 0;

    for(a=0;a<selection->numShips;a++)
    {
        if(selection->ShipPtr[a]->staticinfo->shipclass == CLASS_Fighter)     // CAT: no HW1 TargetDrone (M2-R4 ships-b)
        {
            //check if defender, else fighter
            // CAT: both races' heavy defenders (formGroups 0x004426fb: 0x43 and 0x51); no HW1 LightDefender (M2-R4 ships-b)
            if ((selection->ShipPtr[a]->staticinfo->shiptype == kHeavyDefender) ||
                (selection->ShipPtr[a]->staticinfo->shiptype == tHeavyDefender))
            {
                selectionDefenders->ShipPtr[i] = selection->ShipPtr[a];
                i++;
                selectionDefenders->timeLastStatus = selection->timeLastStatus;
                selectionDefenders->numShips++;
            }
            else
            {
                selectionInterceptors->ShipPtr[l] = selection->ShipPtr[a];
                l++;
                selectionInterceptors->timeLastStatus = selection->timeLastStatus;
                selectionInterceptors->numShips++;
            }
        }
        else if(selection->ShipPtr[a]->staticinfo->shipclass == CLASS_Corvette)
        {
            selectionCorvettes->ShipPtr[j] = selection->ShipPtr[a];
            j++;
            selectionCorvettes->timeLastStatus = selection->timeLastStatus;
            selectionCorvettes->numShips++;

        }
        else        // capital ship and miscellaneous ships
        {
            //captital ship
            selectionCapitalShips->ShipPtr[k] = selection->ShipPtr[a];
            k++;
            selectionCapitalShips->timeLastStatus = selection->timeLastStatus;
            selectionCapitalShips->numShips++;
        }
    }

}
//group types
#define DEFENDERS       10
#define INTERCEPTORS    11
#define CAPITALS        12
#define CORVETTES       13

// CAT: delegateCommand 0x004427a0: the interceptor leader's flip-turn state is kept in its command, read as
//      ship->command whether or not that has COMMAND_MASK_FORMATION (Cataclysm has no Ship.formationcommand); the
//      followers of a force-field sphere always passive attack (as a sphere's); the minelayer is tMinelayerCorvette (84)
bool32 delegateCommand(CommandToDo *attacktodo,sdword group,sdword doform, SelectCommand *globalselection)
{
    sdword i;
    SelectCommand *selection = attacktodo->selection;
    AttackCommand *attack = attacktodo->attack;
    sdword numShips = selection->numShips;
    sdword allNotAttacking;
    sdword formationoverride = FALSE;
    Ship *ship;
    sdword pee;
    Ship *tempship;
    ShipStaticInfo *shipstaticinfo;
    SpecificAttackVars *attackvars;
    SpaceObjRotImpTarg *target;
    Ship *formationLeader;
    vector leadhead,othervec;
    real32 maxdist;
    bool32 attackAndMoving;

    formationLeader = selection->ShipPtr[0];   //set leader ship

    attackAndMoving = FALSE;
    if(attacktodo->ordertype.attributes & COMMAND_MASK_ATTACKING_AND_MOVING)
    {
        //specialized movement + combat
        attackAndMoving = TRUE;             //set flag for future movement reference
        processMoveToDo(attacktodo,TRUE,0,TRUE);   //move the group to the new attack location (CAT: the leader, delegateCommand 0x004427a0)

    }

    //perform fancy pants reordering of selection if we leader does a flip turn
    if(group == INTERCEPTORS)
    {
        i=0;
        ship = selection->ShipPtr[i];
        shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);
        attackvars = &ship->attackvars;
        target = attackvars->attacktarget;

        if(doform && !attackAndMoving)
        {
            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else if (shipstaticinfo->custshipheader.CustShipAttack)
            {
                shipstaticinfo->custshipheader.CustShipAttack(ship,target,ship->staticinfo->bulletRange[ship->tacticstype]);
            }
        }
        else
        {
            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else if(shipstaticinfo->custshipheader.CustShipAttackPassive)
            {
                shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)target,TRUE);
            }
        }
//skipattack:
        if(ship->tacticsTalk != 0)
        {
            if(ship->command->formation.flipselectionfixed != TRUE)
            {
                //lock the leaders formation!
                //fix later:  this is bad because this 'formation' is
                //only a pseudo formation...it will fuck up other sub formations
                //FUCK!
                if(!bitTest(ship->tacticsTalk,TACTALK_BARRELROLL))
                {
                    //if(ship->flightman == FLIGHTMAN_IMMELMAN)
                    //{
                        //for immelman, lets not fix the formation, it should look good maybe
                    //    ship->command->formation.needFix = FALSE;
                    //    lockFormation(ship->formationcommand,0);
                    //}
                    //else
                    //{
                        ship->command->formation.needFix = TRUE;
                    //do special heading reversal in formation lock so
                    //as to begin forming the new formation rightaway
                    lockFormation(ship->command,1);
                    //}
                }
                else
                {
                    ship->command->formation.needFix = FALSE;
                    lockFormation(ship->command,0);
                }

                ship->command->formation.flipselectionfixed = TRUE;
                for(i=1;i<numShips;i++)
                {
                    flightmanInitFunc(selection->ShipPtr[i],ship->flightman,0);

#ifdef bpasechn
    #ifdef DEBUG_TACTICS
                    if(ship->flightman == FLIGHTMAN_NULL)
                        dbgNonFatal(DBG_Loc,"FLIGHTMAN_NULL Detected.");
    #endif
#endif

                    selection->ShipPtr[i]->tacticsNeedToFlightMan=TRUE;
                }
            }
        }
        else
        {
            if(ship->command->formation.flipselectionfixed == TRUE)
            {
                //only if did flipturn;
                unlockFormation(ship->command);
                if(ship->command->formation.needFix == TRUE)
                {
                    if(ship->command->formation.formationtype != WALL_FORMATION)
                    {
                        fixFlipTurnSelection(attacktodo->selection,globalselection);
                    }
                    else
                    {
                        fixWallFlipTurnSelection(attacktodo->selection);
                    }
                    ship->command->formation.needFix = FALSE;

                    //negate this bool32 value so the formation status is uptodate.
                    ship->command->formation.flipState = !ship->command->formation.flipState;
                }
            }
            ship->command->formation.flipselectionfixed = FALSE;
        }


        allNotAttacking=0;
        for (i=1;i<numShips;i++)
        {
            ship = selection->ShipPtr[i];
            shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);
            attackvars = &ship->attackvars;
            target = attackvars->attacktarget;
             attackvars->attackevents = 0;       // clear any events we received

            //perform ship flight maneuvers here
            if(ship->tacticsNeedToFlightMan)
            {
#ifdef bpasechn
    #ifdef DEBUG_TACTICS
                if(ship->flightman == FLIGHTMAN_NULL)
                    dbgNonFatal(DBG_Loc,"FLIGHTMAN_NULL Detected.");
#endif
                if(ship->flightman != FLIGHTMAN_NULL)
#endif
                {
                    if(flightmanExecute(ship))
                    {
                        ship->tacticsNeedToFlightMan = FALSE;
                    }
                    else
                    {
                        formationoverride = TRUE;
                    }
                    continue;
                }
            }
            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else if (shipstaticinfo->custshipheader.CustShipAttackPassive)
            {
                //only do the attack if in proper attack state...
                //quick fix
                // CAT: a force-field sphere too (delegateCommand 0x00442a6c)
                if ((attacktodo->formation.formationtype == SPHERE_FORMATION) ||
                    (attacktodo->formation.formationtype == FORCEFIELD_SPHERE_FORMATION))
                {
                    shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)target,TRUE);
                    bitSet(ship->dontrotateever,4);
                }
                else
                {
                    matGetVectFromMatrixCol3(leadhead,formationLeader->rotatable.rotinfo.coordsys);
                    vecSub(othervec,ship->obj.posinfo.position,target->obj.posinfo.position);
                    if(vecDotProduct(othervec,leadhead) < 0)
                    {
                        shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)target,TRUE);
                        bitSet(ship->dontrotateever,4);
                    }
                    else
                    {
                        bitClear(ship->dontrotateever,4);
                    }
                }
            }

            /*
            if (shipstaticinfo->custshipheader.CustShipAttackPassive)
            {
                //only do the attack if in proper attack state...
                if(!doform)
                {
                    shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)target,TRUE);
                    continue;
                }
                if(ship->tacticsFormationVar1 == TRUE)
                    if(selection->ShipPtr[0]->aistateattack != 4)
                    {
                        //set flag so we can try to attack because leader has finished attack pass
                        continue;
                    }
                    else
                    {
                        ship->tacticsFormationVar1 = FALSE;
                    }


                if(InterceptorInRange(ship,target))
                {
                    //set a state flag here so when it tries to passive attack again later, it won't
                    //unless the leader is again attacking...
                    ship->tacticsFormationVar1 = FALSE;
                    shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)target,TRUE);
                }
                else
                {
                    //ship is too close
                    ship->tacticsFormationVar1 = TRUE;
                    allNotAttacking++;
                }
            }
            */
        }
        if(doform)
        {
            if(!formationoverride)
            {
                processFormationToDo(attacktodo,FALSE,FALSE);
            }
            else
            {
                //do formation, but loosish...
                processFormationToDo(attacktodo,FALSE,TRUE);
            }
        }

        for(i=0;i<selection->numShips;i++)
        {
            bitClear(selection->ShipPtr[i]->dontrotateever,4);
        }
    }
    else if(group == DEFENDERS)
    {
        i=0;
        ship = selection->ShipPtr[i];
        shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);
        attackvars = &ship->attackvars;
        target = attackvars->attacktarget;


        if (target == NULL)
        {
            soundEventBurstStop(ship,NULL);
            attackvars->attacktarget = target = AttackClosestOpponent(ship,attack);
            InitShipAI(ship,FALSE);
        }

        if (attackvars->multipleAttackTargets)
        {
            dbgAssertOrIgnore(shipstaticinfo->canTargetMultipleTargets);

            if (UNIVERSE_WOODPECKER(MULTIPLETARGETS_RETARGET_RATE, ship->shipID.shipNumber & MULTIPLETARGETS_RETARGET_RATE))
            {
                pickMultipleAttackTargets(ship,attack);
            }
        }

        maxdist = tacticsMaxDistToTarget(selection,target);
        if(doform && !attackAndMoving)
        {
            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else if (shipstaticinfo->custshipheader.CustShipAttack)
            {
                shipstaticinfo->custshipheader.CustShipAttack(ship,target,maxdist);
            }
        }
        else
        {
            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else if (shipstaticinfo->custshipheader.CustShipAttackPassive)
            {
                shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)target,TRUE);
            }
        }


        allNotAttacking=0;
        for (i=1;i<numShips;i++)
        {
            ship = selection->ShipPtr[i];
            shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);
            attackvars = &ship->attackvars;
            target = attackvars->attacktarget;

            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else if (shipstaticinfo->custshipheader.CustShipAttackPassive)
            {
                //only do the attack if in proper attack state...
                shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)target,TRUE);
            }
        attackvars->attackevents = 0;       // clear any events we received
        }
        //these need to be moved up...

        if(doform)
        {
            processFormationToDo(attacktodo,FALSE,TRUE);
        }

    }
    else if(group == CORVETTES)
    {
        if(selection->ShipPtr[0]->shiptype == tMinelayerCorvette)
        {
            for(pee=1;pee<selection->numShips;pee++)
            {
                if(selection->ShipPtr[pee]->shiptype != tMinelayerCorvette)
                {
                    tempship = selection->ShipPtr[0];
                    selection->ShipPtr[0] = selection->ShipPtr[pee];
                    selection->ShipPtr[pee] = tempship;
                }
            }
        }

        i=0;
        ship = selection->ShipPtr[i];
        shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);
        attackvars = &ship->attackvars;
        target = attackvars->attacktarget;

        if(doform && !attackAndMoving)
        {
            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else if (shipstaticinfo->custshipheader.CustShipAttack)
            {
                shipstaticinfo->custshipheader.CustShipAttack(ship,target,ship->staticinfo->bulletRange[ship->tacticstype]);
            }
        }
        else
        {
            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else if (shipstaticinfo->custshipheader.CustShipAttackPassive)
            {
                shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)target,TRUE);
            }
        }

        allNotAttacking=0;
        for (i=1;i<numShips;i++)
        {
            ship = selection->ShipPtr[i];
            shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);
            attackvars = &ship->attackvars;
            target = attackvars->attacktarget;

            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else if (shipstaticinfo->custshipheader.CustShipAttackPassive)

            {
                //only do the attack if in proper attack state...
                shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)target,TRUE);
            }
            else if(shipstaticinfo->shiptype == tMinelayerCorvette)
            {
                bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_BrokenFormation);
                shipstaticinfo->custshipheader.CustShipAttack(ship,target,ship->staticinfo->bulletRange[ship->tacticstype]);
            }
            attackvars->attackevents = 0;       // clear any events we received
        }

        if(doform)
        {
            if(selection->ShipPtr[0]->aistateattack == 3 || selection->ShipPtr[0]->aistateattack == 4)
            {
            //leader of formation is turning around or reaproaching, so lets do a real formation...
                processFormationToDo(attacktodo,FALSE,FALSE);
            }
            else
            {
                processFormationToDo(attacktodo,FALSE,TRUE);
            }
        }
    }
    else if(group == CAPITALS)
    {
        i=0;
        ship = selection->ShipPtr[i];
        shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);
        attackvars = &ship->attackvars;
        target = attackvars->attacktarget;


        if (target == NULL)
        {
            soundEventBurstStop(ship,NULL);
            attackvars->attacktarget = target = AttackClosestOpponent(ship,attack);
            InitShipAI(ship,FALSE);
        }

        if (attackvars->multipleAttackTargets)
        {
            dbgAssertOrIgnore(shipstaticinfo->canTargetMultipleTargets);

            if (UNIVERSE_WOODPECKER(MULTIPLETARGETS_RETARGET_RATE, ship->shipID.shipNumber & MULTIPLETARGETS_RETARGET_RATE))
            {
                pickMultipleAttackTargets(ship,attack);
            }
        }

        maxdist = tacticsMaxDistToTarget(selection,target);
        if(doform && !attackAndMoving)
        {
            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else if (shipstaticinfo->custshipheader.CustShipAttack)
            {
                shipstaticinfo->custshipheader.CustShipAttack(ship,target,maxdist);
            }
        }
        else
        {
            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else if(shipstaticinfo->custshipheader.CustShipAttackPassive)
            {
                shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)target,TRUE);
            }
        }


        allNotAttacking=0;
        for (i=1;i<numShips;i++)
        {
            ship = selection->ShipPtr[i];
            shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);
            attackvars = &ship->attackvars;
            target = attackvars->attacktarget;

            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                doKamikazeAttack(ship,target);
            }
            else
            {
                if (shipstaticinfo->custshipheader.CustShipAttackPassive)
                {
                    //only do the attack if in proper attack state...
                    shipstaticinfo->custshipheader.CustShipAttackPassive(ship,(Ship *)target,TRUE);
                }
            }
            ship->shipidle = FALSE;
            attackvars->attackevents = 0;       // clear any events we received
        }

        if(doform)
        {
            processFormationToDo(attacktodo,FALSE,TRUE);
        }


    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : targetIsntNearShip
    Description : determines if ships target is in or out of gun range

    Inputs      :
    Outputs     :
    Return      : returns TRUE if ships target is out of gun range
----------------------------------------------------------------------------*/

bool32 targetIsntNearShip(Ship *ship,SpaceObjRotImpTarg *target)
{
    vector distvect;
    real32 dist;
    vecSub(distvect,target->obj.posinfo.position,ship->obj.posinfo.position);
    dist = vecMagnitudeSquared(distvect);
    dist = fsqrt(dist);

    return(dist > ship->staticinfo->bulletRange[ship->tacticstype]);
}

/*-----------------------------------------------------------------------------
    Name        : processAttackToDoInFormation
    Description : processes an attack command for a group of ships in
                  formation...maintains formation
    Inputs      :
    Outputs     :
    Return      : returns TRUE if attack has been completed
----------------------------------------------------------------------------*/
bool32 processAttackToDoInFormation(CommandToDo *attacktodo)
{
    sdword donness = 0;
    CommandToDo fakeCommand;
    MaxSelection selectionDefenders, selectionCorvettes,selectionCaptialShips,selectionInterceptors;
    AttackCommand *attack = attacktodo->attack;
    sdword numShipsToAttack = attack->numTargets;
    SelectCommand *selection = attacktodo->selection;
    bool32 alldone;
    ShipStaticInfo *shipstaticinfo = NULL;
    Ship *ship = NULL;
    SpaceObjRotImpTarg *target = NULL;
    SpaceObjRotImpTarg *leadertarget = NULL;
    sdword i;
    sdword should = TRUE;
    bool32 focusfire = FALSE;

    SpecificAttackVars *attackvars;

    dbgAssertOrIgnore(selection->numShips > 0);

    if (selection->ShipPtr[0]->tacticstype == Aggressive)
    {
        focusfire = TRUE;
    }

    if (numShipsToAttack == 0)
    {                                                               // don't do holding pattern for swarmers (don't want them to go into delta formation)
        if(isSelectionExclusivlyStrikeCraft(attacktodo->selection))     // CAT: no HW1 P2 swarmers (M2-R4 ships-b)
        {
            attacktodo->holdingStartTime = universe.totaltimeelapsed;
            //use leaders heading for holding pattern
            attacktodo->turnAroundTimes = 0;
            //use leaders heading for holding pattern
            matGetVectFromMatrixCol3(attacktodo->holdingPatternVec,selection->ShipPtr[0]->rotatable.rotinfo.coordsys);
            vecScalarMultiply(attacktodo->holdingPatternVec,attacktodo->holdingPatternVec,TW_HOLDING_PATTERN_PATH_DISTANCE);
            for (i=0;i<selection->numShips;i++)
            {
                //save individual positions
                attacktodo->selection->ShipPtr[i]->holdingPatternPos = selection->ShipPtr[i]->obj.posinfo.position;
                soundEventBurstStop(selection->ShipPtr[i],NULL);
            }
            ////////////////////
            //Speech Event for end of attack
            //event num:  STAT_Strike_AttackComplete
            // CAT: through the fleet, without the can-chatter test (processAttackToDoInFormation 0x004430fd)
            if(attacktodo->selection->ShipPtr[0]->playerowner->playerIndex == universe.curPlayerIndex)
            {
                battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_STAT_Strike_AttackComplete, SOUND_EVENT_DEFAULT, NULL);
            }

            ///////////////

            attacktodo->ordertype.attributes |= COMMAND_MASK_HOLDING_PATTERN;
            return TRUE;    //return to do the holding pattern
        }
        alldone = TRUE;

        // we've killed all the ships, so let's stop movement and wait for next command
        for (i=0;i<selection->numShips;i++)
        {
            // CAT: a ship without a blob ends the attack at once (processAttackToDoInFormation 0x0044313e)
            if (selection->ShipPtr[i]->obj.collMyBlob == NULL)
            {
                return TRUE;
            }
            //save individual positions
            attacktodo->selection->ShipPtr[i]->holdingPatternPos = selection->ShipPtr[i]->obj.posinfo.position;
            soundEventBurstStop(selection->ShipPtr[i],NULL);
        }
        if (attacktodo->ordertype.attributes)
        {
            // something else to do afterwards, so return TRUE without bothering to stabilize ships
            return TRUE;
        }

        for (i=0;i<selection->numShips;i++)
        {
            ship = selection->ShipPtr[i];
            if (!aitrackSteadyShip(ship))
            {
                alldone = FALSE;
            }
        }

        return alldone;

    }


    //check attack conditions as to whether or not we need to
    //go back to guarding a ship...

    if((tacticsInfo.tacticsGuardConditionCheckRate & universe.univUpdateCounter) == tacticsInfo.tacticsGuardConditionCheckFrame)
    {
        if(attacktodo->ordertype.attributes & COMMAND_MASK_PROTECTING)
        if(tacticsCheckGuardConditionsDuringAttack(attacktodo))
        {
            for (i=0;i<selection->numShips;i++)
            {
                soundEventBurstStop(selection->ShipPtr[i],NULL);
            }
            return TRUE;
        }
    }

    for (i=0;i<selection->numShips;i++)
    {
        ship = selection->ShipPtr[i];
        shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);
        attackvars = &ship->attackvars;
        ship->shipidle = FALSE;
        target = attackvars->attacktarget;
        if(i==0)
            leadertarget = target;

        if (target == NULL)
        {
            if(i == 0)
            {
                soundEventBurstStop(ship,NULL);
                attackvars->attacktarget = target = AttackClosestOpponent(ship,attack);
                InitShipAI(ship,FALSE);
                leadertarget = target;
            }
            else
            {
                soundEventBurstStop(ship,NULL);

                // CAT: the leader's target is shared when it is no ship, a ship without a command (HW1: not in a
                //      formation; processAttackToDoInFormation 0x0044329c reads Ship+0x370) or focus fire is on
                if ( leadertarget &&
                     (
                     ( ((leadertarget->header.objtype != OBJ_ShipType) || (((Ship *)leadertarget)->command == NULL)) ) ||
                     (focusfire)
                     )
                   )
                {
                    attackvars->attacktarget = leadertarget;
                }
                else
                {
                    attackvars->attacktarget = AttackClosestOpponent(ship,attack);
                }
                InitShipAI(ship,FALSE);
            }
        }

        if(i >= 1)
        {
            if ( leadertarget &&
                 ((leadertarget->header.objtype != OBJ_ShipType) || (((Ship *)leadertarget)->command == NULL)) )   // CAT: 0x004432f0
            {
                attackvars->attacktarget = leadertarget;    // override with leadertarget if we are attacking a ship that isn't in formation
            }

            // CAT: a force-field sphere too (processAttackToDoInFormation 0x00443300)
            if ((attacktodo->formation.formationtype == SPHERE_FORMATION) ||
                (attacktodo->formation.formationtype == FORCEFIELD_SPHERE_FORMATION))
            {
                //do this check infrequently
                if (UNIVERSE_WOODPECKER(SPHERE_CHECK_TARGET_MASK, SPHERE_CHECK_TARGET_FRAME))
                {
                    if(targetIsntNearShip(ship,ship->attackvars.attacktarget))
                    {
                        soundEventBurstStop(ship,NULL);
                        attackvars->attacktarget = AttackClosestOpponent(ship,attack);//leadertarget;
                        InitShipAI(ship,FALSE);
                    }
                }
            }
        }
        if (attackvars->multipleAttackTargets)
        {
            dbgAssertOrIgnore(shipstaticinfo->canTargetMultipleTargets);

            if (UNIVERSE_WOODPECKER(MULTIPLETARGETS_RETARGET_RATE, ship->shipID.shipNumber & MULTIPLETARGETS_RETARGET_RATE))
            {
                pickMultipleAttackTargets(ship,attack);
            }
        }
    }

    //ships should each be guaranteed to have their proper targets now!


    //GET TARGET HERE!

    //then, if in sphere formation, and target doesn't = center ship,
    //    make it the center ship, otherwise, center ship is target, so keep
    //    pumpleing it


    fakeCommand = *attacktodo;
    formGroups(selection,(SelectCommand *)&selectionDefenders,(SelectCommand *)&selectionCorvettes,(SelectCommand *)&selectionCaptialShips,(SelectCommand *)&selectionInterceptors);
    // CAT: a force-field sphere too (processAttackToDoInFormation 0x00443426)
    if ((attacktodo->formation.formationtype == SPHERE_FORMATION) ||
        (attacktodo->formation.formationtype == FORCEFIELD_SPHERE_FORMATION))
    {
        //if(attacktodo->formation.enders != TRUE || attacktodo->formation.endership != (Ship *)leadertarget)
        if(attacktodo->formation.enders != TRUE)
        {
            //FormationSphereInfo *sphereInfo = (FormationSphereInfo *)attacktodo->formation.formationSpecificInfo;
            //SphereVar *sphereVar;
            attacktodo->formation.enders = TRUE;
            //attacktodo->formation.endership = (Ship *)leadertarget;
            //sphere formation...lets do enders game..
            //
            //first we need to save old formation type, so we can change it back

            //attacktodo->selection->ShipPtr[attacktodo->selection->numShips] = attacktodo->selection->ShipPtr[0];
            //attacktodo->selection->numShips++;
            //attacktodo->selection->ShipPtr[0] = (Ship *)target;
            formationContentHasChanged(attacktodo);
            should = FALSE;
        }
        else
        {
            should = FALSE;
        }
        processFormationToDo(attacktodo,FALSE,TRUE);
    }
    else
    {
        should = TRUE;
    }

    // CAT: no targets left (the target list is shared with fakeCommand): done (processAttackToDoInFormation 0x0044346b)
    if (fakeCommand.attack->numTargets <= 0)
    {
        return TRUE;
    }

    if(selectionInterceptors.numShips > 0)
    {
        fakeCommand.selection = (SelectCommand *)&selectionInterceptors;
        donness |= delegateCommand(&fakeCommand,INTERCEPTORS,should,attacktodo->selection);
    }

    if(selectionDefenders.numShips > 0)
    {
        fakeCommand.selection = (SelectCommand *)&selectionDefenders;
        donness |= delegateCommand(&fakeCommand,DEFENDERS,should,attacktodo->selection);
    }
    if(selectionCorvettes.numShips > 0)
    {
        fakeCommand.selection = (SelectCommand *)&selectionCorvettes;
        donness |= delegateCommand(&fakeCommand,CORVETTES,should,attacktodo->selection);
    }

    if(selectionCaptialShips.numShips > 0)
    {
        fakeCommand.selection = (SelectCommand *)&selectionCaptialShips;
        donness |= delegateCommand(&fakeCommand,CAPITALS,should,attacktodo->selection);
        //processAttackToDo(&fakeCommand);
    }

    return(donness);

}

/*-----------------------------------------------------------------------------
    Name        : hyperspaceOutPingTimeOut
    Description : timeout cb function that determines if the ping should time out
    Inputs      : hellaPing - the ping we're evaluating
                  userID - cast to ship * for pointer to proximity sensor.
                  userData - ignore
                  bRemoveReferneces - if TRUE, remove references to pingDyingObject
    Outputs     :
    Return      : TRUE if the ping times out.
----------------------------------------------------------------------------*/
bool32 hyperspaceOutPingTimeOut(struct ping *hellaPing, SpaceObj *user, ubyte *userData, bool32 bRemoveReferences)
{
    if (bRemoveReferences)
    {
        if (user == pingDyingObject)
        {
            return(TRUE);
        }
        return(FALSE);
    }

    if(user->header.flags & SOF_Hyperspace)
    {
        return(TRUE);
    }
    return(FALSE);
}
/*-----------------------------------------------------------------------------
    Name        : hyperspaceInPingTimeOut
    Description : timeout cb function that determines if the ping should time out
    Inputs      : hellaPing - the ping we're evaluating
                  userID - cast to ship * for pointer to proximity sensor.
                  userData - ignore
                  bRemoveReferneces - if TRUE, remove references to pingDyingObject
    Outputs     :
    Return      : TRUE if the ping times out.
----------------------------------------------------------------------------*/
 bool32 hyperspaceInPingTimeOut(struct ping *hellaPing, SpaceObj *user, ubyte *userData, bool32 bRemoveReferences)
{
    if (bRemoveReferences)
    {
        if (user == pingDyingObject)
        {
            return(TRUE);
        }
        return(FALSE);
    }
    if(((Ship *)user)->shipSinglePlayerGameInfo->shipHyperspaceState == SHIPHYPERSPACE_NONE)
    {
        return(TRUE);
    }
    return(FALSE);
}

bool32 needToWaitForThisShip(Ship *ship)
{
    Node *node;
    CommandToDo *command;
    SelectCommand *selection;
    sdword i;

    // CAT (M2-R5 dock): needToWaitForThisShip 0x004435d0: for a Kushan or Taiidan support frigate, any ship docking with
    //      it (no HW1 ASF sub-state exemptions) or launching from it while outside; for a mothership-class ship, a ship
    //      launching from it while outside
    if ((ship->shiptype == kAdvanceSupportFrigate) || (ship->shiptype == tAdvanceSupportFrigate))
    {
        for (node = universe.mainCommandLayer.todolist.head; node != NULL; node = node->next)
        {
            command = (CommandToDo *)listGetStructOfNode(node);
            selection = command->selection;
            if (command->ordertype.order == COMMAND_DOCK)
            {
                for (i = 0; i < selection->numShips; i++)
                {
                    if (selection->ShipPtr[i]->dockvars.dockship == ship)
                    {
                        return TRUE;
                    }
                }
            }
            else if ((command->ordertype.order == COMMAND_LAUNCH_SHIP) && (command->launchship.receiverShip == ship) &&
                     (ship->obj.collMyBlob != NULL))
            {
                return TRUE;
            }
        }
    }
    else if (ship->staticinfo->shipclass == CLASS_Mothership)
    {
        for (node = universe.mainCommandLayer.todolist.head; node != NULL; node = node->next)
        {
            command = (CommandToDo *)listGetStructOfNode(node);
            if ((command->ordertype.order == COMMAND_LAUNCH_SHIP) && (command->launchship.receiverShip == ship) &&
                (command->selection->ShipPtr[0]->obj.collMyBlob != NULL))
            {
                return TRUE;        //need to wait for this ship!
            }
        }
    }

    //don't need to wait for anything else
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : processMpHyperspaceingToDo
    Description : processes the hyperspaceing command
    Inputs      : movetodo
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
#define HW_ETHER_WAIT       TW_MULTIPLAYER_HYPERSPACE_WAIT_IN_ETHER_TIME

bool32 processMpHyperspaceingToDo(CommandToDo *movetodo)
{
    sdword i,j,donehsflag;
    vector shipheading,othervector;
    real32 dotproduct;
    real32 timefraction;
    sdword shipslice;
    bool32 dontgoyet=FALSE;
    for(i=0;i<movetodo->selection->numShips;i++)
    {
       SET_MOVING_LINEARLY(movetodo->selection->ShipPtr[i]->obj.posinfo.isMoving);
    }
    switch(movetodo->hyperspaceState)
    {
    case COM_HYPERSPACE_START:
        //check if any ASF's in selection
        //if yes, we won't hs out until all slots on asf are full
        for(i=0;i<movetodo->selection->numShips;i++)
        {
            if(aitrackZeroRotationAnywhere(movetodo->selection->ShipPtr[i])
               && aitrackZeroVelocity(movetodo->selection->ShipPtr[i]))
            {
                movetodo->selection->ShipPtr[i]->rotatable.rotinfo.rotspeed.x=0.0f;
                movetodo->selection->ShipPtr[i]->rotatable.rotinfo.rotspeed.y=0.0f;
                movetodo->selection->ShipPtr[i]->rotatable.rotinfo.rotspeed.z=0.0f;
                movetodo->selection->ShipPtr[i]->obj.posinfo.velocity.x=0.0f;
                movetodo->selection->ShipPtr[i]->obj.posinfo.velocity.y=0.0f;
                movetodo->selection->ShipPtr[i]->obj.posinfo.velocity.z=0.0f;
                continue;
            }
            dontgoyet=TRUE;

        }
        for(i=0;i<movetodo->selection->numShips;i++)
        {
            if(needToWaitForThisShip(movetodo->selection->ShipPtr[i]))
            {
                //ship needs us to wait!
                return FALSE;
            }
        }
        if(dontgoyet)
        {
            return FALSE;
        }
        //set to hyperspace out!
        spHyperspaceSelectionOut(movetodo->selection);
        //setup timer to count down...
        movetodo->hyperSpaceingTime = universe.totaltimeelapsed;
        movetodo->hyperspaceState=COM_HYPERSPACE_OUT;

        //reset ping flag status
        for(j=0;j<movetodo->selection->numShips;j++)
        {
            movetodo->selection->ShipPtr[j]->hyperspacePing = FALSE;
        }
        return FALSE;

    case COM_HYPERSPACE_OUT:
        //check if all hyperspaced out...
        timefraction = universe.totaltimeelapsed - movetodo->hyperSpaceingTime;
        shipslice = (sdword) (timefraction/TW_PING_MAX_DISPERSAL)*movetodo->selection->numShips;
        if(shipslice-movetodo->selection->numShips == 1 ||
            shipslice > movetodo->selection->numShips )
        {
            shipslice = movetodo->selection->numShips;
        }
        else if(shipslice < 1)
        {
            shipslice = 1;
        }
        for(i=0;i<movetodo->selection->numShips;i++)
        {
            for(j=0;j<shipslice;j++)
            {
                if(!movetodo->selection->ShipPtr[j]->hyperspacePing)
                {
                    ping *newPing;

                    movetodo->selection->ShipPtr[j]->hyperspacePing=TRUE;
                    //create ping HERE!
                    newPing = pingCreate(NULL, (SpaceObj *)movetodo->selection->ShipPtr[j], (pingeval)hyperspaceOutPingTimeOut, NULL, 0, (SpaceObj *)movetodo->selection->ShipPtr[j]);
                    newPing->c = TW_HW_PING_COLOUR_OUT;
                    newPing->size = TW_HW_PING_MAX_SIZE_OUT;
                    newPing->minScreenSize = primScreenToGLScaleX(2);
                    newPing->minSize = TW_HW_PING_MIN_SIZE_OUT;
                    newPing->pingDuration = 1.0f / TW_HW_PING_RATE_OUT;
                    newPing->interPingPause = 0.25;
                    newPing->TOMask         = PTOM_Hyperspace;

                    ///////////////////////////////
                    //PUT SPEECH EVENTS HERE!
                    ///////////////////////////////
                }
            }
            if(!(movetodo->selection->ShipPtr[0]->header.flags & SOF_Hyperspace))
            {
                return(FALSE);
            }
        }

        //tell code that all ships have no pings...so we can create them for hyping in..
        for(i=0;i<movetodo->selection->numShips;i++)
        {
            movetodo->selection->ShipPtr[i]->hyperspacePing=FALSE;
        }

        movetodo->hyperSpaceingTime = universe.totaltimeelapsed+HW_ETHER_WAIT;
        movetodo->hyperspaceState=COM_HYPERSPACE_WAIT;
        break;
    case COM_HYPERSPACE_WAIT:
        //check if done in the ether
        if(movetodo->hyperSpaceingTime<= universe.totaltimeelapsed)
        {
            //hyperspaced out!
            hvector movetoplace;
            // CAT: the destination is the current point of the move path (processMpHyperspaceingToDo 0x00443970)
            movetoplace.x = movetodo->move.points[movetodo->move.curPoint].x;
            movetoplace.y = movetodo->move.points[movetodo->move.curPoint].y;
            movetoplace.z = movetodo->move.points[movetodo->move.curPoint].z;

            matGetVectFromMatrixCol3(shipheading,movetodo->selection->ShipPtr[0]->rotatable.rotinfo.coordsys);
            vecSet(othervector,1.0f,0.0f,0.0f);

            dotproduct = vecDotProduct(shipheading,othervector);
            if(dotproduct >= 1.0)
                dotproduct = 1.0f;
            else if(dotproduct <=-1.0f)
                dotproduct = -1.0f;

            movetoplace.w = RAD_TO_DEG(acos(dotproduct));

            vecSet(othervector,0.0f,1.0f,0.0f);
            dotproduct = vecDotProduct(shipheading,othervector);
            if(dotproduct < 0)
            {
                movetoplace.w = 360-movetoplace.w;
            }

            spHyperspaceSelectionIn(movetodo->selection,&movetoplace);
            movetodo->hyperspaceState=COM_HYPERSPACE_IN;
            movetodo->hyperSpaceingTime = universe.totaltimeelapsed;
        }
        break;
    case COM_HYPERSPACE_IN:
        //check if all hyperspaced in...
        donehsflag = TRUE;
        timefraction = universe.totaltimeelapsed - movetodo->hyperSpaceingTime;
        shipslice = (sdword) (timefraction/TW_PING_MAX_DISPERSAL)*movetodo->selection->numShips;
        if(shipslice-movetodo->selection->numShips == 1 ||
            shipslice > movetodo->selection->numShips )
        {
            shipslice = movetodo->selection->numShips;
        }
        else if(shipslice < 1)
        {
            shipslice = 1;
        }
        for(i=0;i<movetodo->selection->numShips;i++)
        {
            if(movetodo->selection->ShipPtr[i]->playerowner != universe.curPlayerPtr)
            {
                //ship isn't a local players ship...
                // CAT: no battleCanChatterAtThisTime test and no position (processMpHyperspaceingToDo 0x00443ac4)
                battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_COMM_F_HyperspaceDetected, SOUND_EVENT_DEFAULT, NULL);
            }


            //do hs ping for ALL ships
            for(j=0;j<shipslice;j++)
            {
                if(!movetodo->selection->ShipPtr[j]->hyperspacePing)
                {
                    ping *newPing;
                    movetodo->selection->ShipPtr[j]->hyperspacePing=TRUE;
                    //create ping HERE!
                    newPing = pingCreate(NULL, (SpaceObj *)movetodo->selection->ShipPtr[j], (pingeval)hyperspaceInPingTimeOut, NULL, 0, (SpaceObj *)movetodo->selection->ShipPtr[j]);
                    newPing->c = TW_HW_PING_COLOUR_IN;
                    newPing->size = TW_HW_PING_MAX_SIZE_IN;
                    newPing->minScreenSize = primScreenToGLScaleX(2);
                    newPing->minSize = TW_HW_PING_MIN_SIZE_IN;
                    newPing->pingDuration = 1.0f / TW_HW_PING_RATE_IN;
                    newPing->interPingPause = 0.25;
                    newPing->TOMask         = PTOM_Hyperspace;

                    ///////////////////////////////
                    //PUT SPEECH EVENTS HERE!
                    ///////////////////////////////

                }
            }
            if((movetodo->selection->ShipPtr[i]->shipSinglePlayerGameInfo->shipHyperspaceState != SHIPHYPERSPACE_NONE))
            {
                donehsflag = FALSE;
            }
            else
            {
                //individual ship is out of hyperspace...make it selectable!
                // CAT: the hyperspace disable reason 6 is cleared instead of setting SOF_Selectable (0x00443b8d)
                cat_shipSetDisableReason(movetodo->selection->ShipPtr[i],FALSE,6);
            }
        }
        if(donehsflag == FALSE)
        {
            return FALSE;
        }
        for(j=0;j<movetodo->selection->numShips;j++)
        {
            movetodo->selection->ShipPtr[j]->hyperspacePing=FALSE;
        }
        //all out..
        for(i=0;i<movetodo->selection->numShips;i++)
        {
            //make selectable again!
            cat_shipSetDisableReason(movetodo->selection->ShipPtr[i],FALSE,6);   // CAT: as above (0x00443bfc)
        }
        return(TRUE);
        break;
    }

    return(FALSE);
}

double hyperspaceCost(real32 distance,SelectCommand *selection)
{
    sdword i;
    Ship *ship;
    // CAT (M2-R8 oracle-rung-physics): the x87 shape (hyperspaceCost 0x00443c30): the distance, each ship's cost and
    //      the sum stay doubles on the FPU across the ships (HW1's cumulative "distance -= minDistance" included); each
    //      cost is (distance * slope) + min, compared as a double with the float max
    // CAT (M2-R9 oracle-rung-physics): the sum is returned unrounded on the FPU (hyperspaceCost 0x00443c30 ends with
    //      no store): its callers _ftol it (0x00407ea9 aimProcessHyperspace, 0x0044750a clMpHyperspace, 0x004e6240
    //      pieDistanceReadoutDraw, 0x00564c1c the main region's move) or add it as a double (0x00433170
    //      cat_aisTeamAttack), so the function returns the double
    double dist = distance;
    double tempcost;
    double cost = 0.0;
    for(i=0;i<selection->numShips;i++)
    {
        ship = selection->ShipPtr[i];
        if(dist < (double)TW_HyperSpaceCostStruct[ship->shiptype].minDistance)
        {
            tempcost = TW_HyperSpaceCostStruct[ship->shiptype].min;
        }
        else
        {
            dist -= TW_HyperSpaceCostStruct[ship->shiptype].minDistance;
            tempcost = dist * TW_HyperSpaceCostStruct[ship->shiptype].distanceSlope + TW_HyperSpaceCostStruct[ship->shiptype].min;
        }
        if(tempcost > (double)TW_HyperSpaceCostStruct[ship->shiptype].max)
        {
            tempcost = TW_HyperSpaceCostStruct[ship->shiptype].max;
        }
        cost += tempcost;
    }
    return(cost);
}


/*-----------------------------------------------------------------------------
    Name        : processMoveToDo
    Description : processes one ship of a move command along its waypoint path
    Inputs      : movetodo, passiveAttacked, shipIndex (the ship of the selection), inFormation (the leader of a
                  formation, which flies at the formation's travel velocity)
    Outputs     :
    Return      : TRUE when the ship has reached the end of a one-way path and is steady
----------------------------------------------------------------------------*/
// CAT: processMoveToDo 0x00443cb0 runs one ship (clProcess calls it for each ship, or for the leader of a formation;
//      HW1's processMoveLeaderToDo is gone). The ship flies to its moveTo; on arrival the path advances: a one-way path
//      (0) to the next point, a loop (1) back to the first, a patrol (2/3) turns at either end, each by
//      CalculateMoveToPoints from the previous point to the new one. The arrival tolerance is 800 between waypoints
//      and 50 at the last point of a one-way path (250 while dodging). A Sentinel in a force-field sphere never turns
//      to its direction of flight. There is no final heading (the exe's move has none): the ship only stops.
bool32 processMoveToDo(CommandToDo *movetodo,bool32 passiveAttacked,sdword shipIndex,bool32 inFormation)
{
    SelectCommand *selection = movetodo->selection;
    Ship *ship = selection->ShipPtr[shipIndex];
    ShipStaticInfo *shipstatic = (ShipStaticInfo *)ship->staticinfo;
    MoveCommand *move = &movetodo->move;
    bool32 dontrotate;
    udword aiflags;
    sdword j;
    sdword from;
    real32 MOVE_ARRIVE_TOLERANCE;

    if (movetodo->ordertype.attributes & COMMAND_MASK_ATTACKING_AND_MOVING)
    {
        dontrotate = TRUE;
    }
    else if (cat_shipIsSentinelInSphere(ship))
    {
        dontrotate = TRUE;
    }
    else
    {
        dontrotate = (passiveAttacked & (bool32)shipstatic->rotateToRetaliate);
    }

    switch (ship->aistatecommand)
    {
        case 0:
            if ((ship->isDodging) || (universe.totaltimeelapsed < ship->DodgeTime))
            {
                //ship is dodging...be a little more forgiving
                MOVE_ARRIVE_TOLERANCE = 250.0f;
            }
            else if ((move->pathType == CAT_MOVEPATH_ONCE) && (move->curPoint == move->numPoints - 1))
            {
                MOVE_ARRIVE_TOLERANCE = 50.0f;
            }
            else
            {
                MOVE_ARRIVE_TOLERANCE = 800.0f;
            }
            if (MoveReachedDestinationVariable(ship,&ship->moveTo,MOVE_ARRIVE_TOLERANCE))
            {
                switch (move->pathType)
                {
                    case CAT_MOVEPATH_ONCE:
                        move->curPoint++;
                        if (move->curPoint < move->numPoints)
                        {
                            from = move->curPoint - 1;
                            break;
                        }
                        goto arrived;
                    case CAT_MOVEPATH_LOOP:
                        move->curPoint++;
                        if (move->curPoint >= move->numPoints)
                        {
                            move->curPoint = 0;
                            from = move->numPoints - 1;
                        }
                        else
                        {
                            from = move->curPoint - 1;
                        }
                        break;
                    case CAT_MOVEPATH_PATROL_OUT:
                        move->curPoint++;
                        if (move->curPoint >= move->numPoints)
                        {
                            move->pathType = CAT_MOVEPATH_PATROL_BACK;
                            move->curPoint = move->numPoints - 2;
                            from = move->numPoints - 1;
                        }
                        else
                        {
                            from = move->curPoint - 1;
                        }
                        break;
                    case CAT_MOVEPATH_PATROL_BACK:
                        from = move->curPoint;
                        move->curPoint--;
                        if (move->curPoint < 0)
                        {
                            move->pathType = CAT_MOVEPATH_PATROL_OUT;
                            move->curPoint = 1;
                            from = 0;
                        }
                        break;
                    default:
                        return FALSE;
                }
                CalculateMoveToPoints(selection,move->points[from],move->points[move->curPoint]);
                return FALSE;
            }
            else
            {
                aiflags = (dontrotate) ? (AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured) : (AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured|AISHIP_FirstPointInDirectionFlying|AISHIP_PointInDirectionFlying|AISHIP_CarTurn);
                if (!(aishipFlyToPointAvoidingObjs(ship,&ship->moveTo,aiflags,inFormation ? movetodo->formation.travelvel : -20.0f) & AISHIP_FLY_OBJECT_IN_WAY))
                {
                    return FALSE;
                }
            }
arrived:
            ship->aistatecommand = 1;
            vecZeroVector(ship->moveTo);
            vecZeroVector(ship->moveFrom);
            if(movetodo->ordertype.attributes & COMMAND_MASK_ATTACKING_AND_MOVING)
            {
                for(j=1;j<movetodo->selection->numShips;j++)
                {
                    vecZeroVector(movetodo->selection->ShipPtr[j]->moveTo);
                    vecZeroVector(movetodo->selection->ShipPtr[j]->moveFrom);
                }
            }
            break;

        case 1:
            break;

        default:
            return FALSE;
    }

    return aitrackSteadyShipDriftOnly(ship) && (move->pathType == CAT_MOVEPATH_ONCE) && (move->curPoint >= move->numPoints - 1);
}

// CAT (M2-R4 combat): a ship taken off a special order calls its header's order-changed slot (slot 9): the exe's
//      RemoveShipsFromDoingStuff 0x00444810 calls [statinfo+0x12c] at 0x00444994 and 0x00444d15. HW1's per-type
//      switch is that slot's content now (the salvagers' cat_salvageOrderChanged, MinelayerCorvetteOrderChanged,
//      HeavyCorvetteOrderChanged, AdvanceSupportFrigateOrderChanged ...); HW1's SalCapCancelOrder and
//      SalCapOrderChangedCleanUp are gone with HW1's salvage code.
void RemoveShipFromSpecial(Ship *shiptoremove)
{
    if (shiptoremove->staticinfo->custshipheader.CustShipOrderChanged != NULL)
    {
        shiptoremove->staticinfo->custshipheader.CustShipOrderChanged(shiptoremove);
    }
}

// CAT: the link block of the linkable types (ShipLink.h): sAcolyte, sACV, sMimic, sMCV, bAcolyte, bACV, the types
//      cat_clLinkCleanup 0x00444060 and cat_clFindLinkPartner 0x0044d2e0 test; NULL for any other ship
CatLinkInfo *cat_shipGetLinkInfo(Ship *ship)
{
    switch (ship->shiptype)
    {
        case sAcolyte:
        case sACV:
        case sMimic:
        case sMCV:
        case bAcolyte:
        case bACV:
            return (CatLinkInfo *)((ubyte *)ship->ShipSpecifics + 0x18);
        default:
            return NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_clLinkCleanup
    Description : breaks the link of a linkable ship: both ships lose the partner, their no-avoidance flag and the
                  link disable reason (3)
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: new (cat_clLinkCleanup 0x00444060; also inlined in FreeLastOrder 0x00444490 and RemoveShipsFromDoingStuff)
void cat_clLinkCleanup(Ship *ship)
{
    CatLinkInfo *link = cat_shipGetLinkInfo(ship);
    Ship *partner;

    if (link == NULL)
    {
        return;
    }
    partner = link->partner;
    if (partner != NULL)
    {
        bitClear(ship->shipOrDerelict.specialFlags2,SPECIAL_2_CatNoAvoidance);
        link->partner = NULL;
        bitClear(partner->shipOrDerelict.specialFlags2,SPECIAL_2_CatNoAvoidance);
        ((CatLinkInfo *)((ubyte *)partner->ShipSpecifics + 0x18))->partner = NULL;     // the exe writes partner+0x650
        cat_shipSetDisableReason(partner,FALSE,3);
    }
    cat_shipSetDisableReason(ship,FALSE,3);
}

void SpecialCleanup(CommandToDo *command)
{
    sdword i;
    SelectCommand *selection = command->selection;
    sdword numShips = selection->numShips;

    // CAT: the special order's cleanup only calls each ship's order-changed slot and frees the targets
    //      (FreeCommandToDoContents 0x004440f0 case 7, FreeLastOrder 0x00444490 case 7): no HW1 repair-beam stop
    for (i=0;i<numShips;i++)
    {
        RemoveShipFromSpecial(selection->ShipPtr[i]);
    }

    if (command->specialtargets)
    {
        memFree(command->specialtargets);
        command->specialtargets = NULL;
    }
}

// CAT: the collect order's per-ship cleanup stops the ship's repair-style effect (stopRepairEffect 0x005c23b0 on
//      Ship+0x38C; FreeCommandToDoContents 0x004440f0 case 5, FreeLastOrder 0x00444490, RemoveShipsFromDoingStuff
//      0x00444b95), not HW1's TurnHarvestEffectOff
void RemoveShipFromResourceCollecting(Ship *shiptoremove)
{
    if (shiptoremove->rceffect != NULL)
    {
        stopRepairEffect(shiptoremove);
    }
    shiptoremove->rcstate1 = 0;
    shiptoremove->rcstate2 = 0;
}

void CollectResourceCleanup(CommandToDo *command)
{
    sdword i;
    SelectCommand *selection = command->selection;
    sdword numShips = selection->numShips;

    for (i=0;i<numShips;i++)
    {
        RemoveShipFromResourceCollecting(selection->ShipPtr[i]);
    }
}

// CAT: the link and unlink orders' cleanup (FreeCommandToDoContents 0x004440f0 and FreeLastOrder 0x00444490, cases
//      11 and 12)
static void LinkCleanup(CommandToDo *command)
{
    sdword i;
    SelectCommand *selection = command->selection;

    for (i = 0; i < selection->numShips; i++)
    {
        cat_clLinkCleanup(selection->ShipPtr[i]);
    }
}

// CAT: FreeCommandToDoContents 0x004440f0 takes a second argument, a code its callers pass (RemoveShipsFromDoingStuff
//      1-11, clProcess); the exe never reads it. No tacticsAttackCommandVoided (a nullsub in its place); the order
//      cases follow the exe: hyperspace clears the disable reason 6, link/unlink break the links, the Cataclysm orders
//      14-16 free only the selection.
void FreeCommandToDoContents(CommandToDo *command,sdword catReason)
{
    (void)catReason;

    if (command->ordertype.attributes & COMMAND_MASK_PROTECTING)
    {
        memFree(command->protect);
        bitClear(command->ordertype.attributes,COMMAND_MASK_PROTECTING);
        command->protect = NULL;
    }

    if (command->ordertype.attributes & COMMAND_MASK_FORMATION)
    {
        FormationCleanup(command);
        bitClear(command->ordertype.attributes,COMMAND_MASK_FORMATION);
    }

    if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        AttackCleanup(command);
        memFree(command->attack);
        bitClear(command->ordertype.attributes,COMMAND_MASK_PASSIVE_ATTACKING);
        command->attack = NULL;
    }

    RemoveShipsFromCommand(command);

    switch (command->ordertype.order)
    {
        case COMMAND_ATTACK:
            AttackCleanup(command);
            memFree(command->attack);
            command->attack = NULL;
            break;

        case COMMAND_MOVE:
            MoveCleanup(command);
            break;
        case COMMAND_MP_HYPERSPACING:
            MpHyperspaceCleanup(command);
            break;

        case COMMAND_COLLECT_RESOURCES:
            CollectResourceCleanup(command);
            break;

        case COMMAND_MILITARY_PARADE:
            FreeMilitaryParadeContents(command->militaryParade);
            memFree(command->militaryParade);
            command->militaryParade = NULL;
            break;

        case COMMAND_LAUNCH_SHIP:
            LaunchCleanup(command);
            break;

        case COMMAND_DOCK:
            DockCleanup(command);
            break;

        case COMMAND_SPECIAL:
            SpecialCleanup(command);
            break;

        case COMMAND_CAT_LINK:
        case COMMAND_CAT_UNLINK:
            LinkCleanup(command);
            break;

        default:
            // COMMAND_NULL, COMMAND_HALT, COMMAND_BUILDING_SHIP, COMMAND_CAT_POINT, COMMAND_CAT_MIMIC,
            // COMMAND_CAT_RESEARCH: nothing but the selection
            break;
    }

    memFree(command->selection);
    command->selection = NULL;
}

// CAT: FreeLastOrder 0x00444490 as FreeCommandToDoContents above, for the order only; every order but an attack also
//      ends the holding pattern
void FreeLastOrder(CommandToDo *command)
{
    sdword dontClearHoldingPattern = FALSE;

    // deliberately do not call
    // RemoveShipsFromCommand(command)
    // because last order is being freed in order to be changed, the actual command is not freed.

    switch (command->ordertype.order)
    {
        case COMMAND_ATTACK:
            AttackCleanup(command);
            memFree(command->attack);
            command->attack = NULL;
            dontClearHoldingPattern = TRUE;
            break;

        case COMMAND_MOVE:
            MoveCleanup(command);
            break;
        case COMMAND_MP_HYPERSPACING:
            MpHyperspaceCleanup(command);
            break;

        case COMMAND_COLLECT_RESOURCES:
            CollectResourceCleanup(command);
            break;

        case COMMAND_MILITARY_PARADE:
            FreeMilitaryParadeContents(command->militaryParade);
            memFree(command->militaryParade);
            command->militaryParade = NULL;
            break;

        case COMMAND_LAUNCH_SHIP:
            LaunchCleanup(command);
            break;

        case COMMAND_DOCK:
            DockCleanup(command);
            break;

        case COMMAND_SPECIAL:
            SpecialCleanup(command);
            break;

        case COMMAND_CAT_LINK:
        case COMMAND_CAT_UNLINK:
            LinkCleanup(command);
            break;

        default:
            break;
    }
    if(!dontClearHoldingPattern)
    {
        bitClear(command->ordertype.attributes,COMMAND_MASK_HOLDING_PATTERN);
    }
}

void ClearPassiveAttacking(CommandToDo *command)
{
    bitClear(command->ordertype.attributes,COMMAND_MASK_PASSIVE_ATTACKING);
    AttackCleanup(command);
    dbgAssertOrIgnore(command->attack);
    memFree(command->attack);
    command->attack = NULL;
}

void ClearProtecting(CommandToDo *command)
{
    bitClear(command->ordertype.attributes,COMMAND_MASK_PROTECTING);
    dbgAssertOrIgnore(command->protect);
    memFree(command->protect);
    command->protect = NULL;

    if (command->ordertype.attributes & COMMAND_MASK_FORMATION)
    {
        // CAT: ClearProtecting 0x004447b0: a sphere needs at least 2 ships (HW1: 1), and a force-field sphere around the
        //      protected ship becomes a force-field wall
        if ((command->formation.formationtype == SPHERE_FORMATION) && (command->selection->numShips >= 2))
        {
            // sphere formation was surrounding protected ship, so let's undo this
            formationContentHasChanged(command);
        }
        else if (command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION)
        {
            command->formation.formationtype = FORCEFIELD_WALL_FORMATION;
            formationContentHasChanged(command);
        }
    }
}

// CAT: the leader/wingman links of the ships that stay in todo, as the exe inlines them in RemoveShipsFromDoingStuff
//      after each RemoveShipFromAttacking (RemoveShipReferencesFromExtraAttackInfo 0x004479f0 is the same code)
static void catRemoveShipFromAttackCommand(Ship *shiptoremove,CommandToDo *todo)
{
    RemoveShipFromAttacking(shiptoremove);
    RemoveShipReferencesFromExtraAttackInfo(shiptoremove,todo);
}

/*-----------------------------------------------------------------------------
    Name        : RemoveShipsFromDoingStuff
    Description : takes the ships of selectcom out of every command of comlayer
    Inputs      : comlayer, selectcom, catReason
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: RemoveShipsFromDoingStuff 0x00444810 takes a third argument, a reason code each caller passes (2 attack-move
//      split, 3 attack-move, 4 move, 5 hyperspace, 0xb launch, 0xd parade, 0xe died, 0xf link, 0x11 mimic, 0x12
//      point, 0x20 formation ...); the exe never reads it. Per order, as the exe's jump table 0x00445124: the move and
//      hyperspace orders clear moveTo/moveFrom (hyperspace also its disable reason 6), the collect order stops the
//      ship's effect, the special order calls the order-changed slot, link/unlink break the ship's link whether or not
//      it was in that command, the point order (14) and the build order keep their ships, and an emptied command is
//      freed (FreeCommandToDoContents with the exe's codes). A formation that loses a ship drops to a plain command
//      at one ship left and is freed at none (HW1: below ABSOLUTE_MIN_SHIPS_IN_FORMATION), except a link/unlink
//      formation, which is kept; the force-field formations are re-arranged like the others.
void RemoveShipsFromDoingStuff(CommandLayer *comlayer,SelectCommand *selectcom,sdword catReason)
{
    sdword i;
    ShipPtr shiptoremove;
    Node *todonode;
    CommandToDo *todo;
    SelectCommand *todoselection;
    sdword freeCode;

    (void)catReason;

    for (i=0;i<selectcom->numShips;i++)
    {
        shiptoremove = selectcom->ShipPtr[i];
        if (shiptoremove->shipOrDerelict.specialFlags & SPECIAL_rowGettingOutOfWay)
        {
            rowRemoveShipFromGettingOutOfWay(shiptoremove);
        }
    }

    todonode = comlayer->todolist.head;

    while (todonode != NULL)
    {
        todo = (CommandToDo *)listGetStructOfNode(todonode);
        todoselection = todo->selection;

        for (i=0;i<selectcom->numShips;i++)
        {
            shiptoremove = selectcom->ShipPtr[i];

            if (todo->ordertype.attributes & COMMAND_MASK_FORMATION)
            {
                sdword removedShipFromFormation;
                if ((removedShipFromFormation = formationRemoveShipFromSelection(todo,shiptoremove)) >= 0)
                {
                    RemoveShipFromCommand(shiptoremove);
                    RemoveShipFromFormation(shiptoremove);

                    if ((todo->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING) || (todo->ordertype.order == COMMAND_ATTACK))
                    {
                        catRemoveShipFromAttackCommand(shiptoremove,todo);
                    }

                    if (todo->ordertype.order == COMMAND_DOCK)
                    {
                        RemoveShipFromDocking(shiptoremove);
                    }

                    if (todo->ordertype.order == COMMAND_MOVE)
                    {
                        RemoveShipFromMoving(shiptoremove);
                    }

                    if(todo->ordertype.order == COMMAND_SPECIAL)
                    {
                        RemoveShipFromSpecial(shiptoremove);
                    }

                    if(todo->ordertype.order == COMMAND_MP_HYPERSPACING)
                    {
                        removeShipFromMpHyperspaceing(shiptoremove);
                    }

                    if (todoselection->numShips == 0)
                    {
                        if ((todo->ordertype.order != COMMAND_CAT_LINK) && (todo->ordertype.order != COMMAND_CAT_UNLINK))
                        {
                            freeCode = 1;
                            goto freetodo;
                        }
                    }
                    else if (todoselection->numShips < 2)
                    {
                        if ((todo->ordertype.order != COMMAND_CAT_LINK) && (todo->ordertype.order != COMMAND_CAT_UNLINK))
                        {
                            // one ship left: it is no longer a formation (0x00444a18)
                            FormationCleanup(todo);
                            bitClear(todo->ordertype.attributes,COMMAND_MASK_FORMATION);
                        }
                    }
                    else
                    {
                        setFormationTravelVelocity(todo);
                        if ((todo->formation.formationtype == WALL_FORMATION) || (todo->formation.formationtype == SPHERE_FORMATION))
                        {
                            ;       // leave holes in WALL and SPHERE formations
                        }
                        else if (todo->formation.formationtype == CUSTOM_FORMATION)
                        {
                            if (removedShipFromFormation == 0)
                            {
                                formationContentHasChanged(todo);       // only update custom formation if leader died
                            }
                        }
                        else
                        {
                            formationContentHasChanged(todo);
                        }
                    }
                }
            }
            else
            {
                switch (todo->ordertype.order)
                {
                    case COMMAND_ATTACK:
                        if (clRemoveShipFromSelection(todoselection,shiptoremove))
                        {
                            RemoveShipFromCommand(shiptoremove);
                            catRemoveShipFromAttackCommand(shiptoremove,todo);
                            if (todoselection->numShips == 0)
                            {
                                freeCode = 2;
                                goto freetodo;
                            }
                        }
                        break;

                    case COMMAND_COLLECT_RESOURCES:
                        if (clRemoveShipFromSelection(todoselection,shiptoremove))
                        {
                            RemoveShipFromCommand(shiptoremove);
                            RemoveShipFromResourceCollecting(shiptoremove);

                            if (todoselection->numShips == 0)
                            {
                                freeCode = 3;
                                goto freetodo;
                            }
                        }
                        break;

                    case COMMAND_MOVE:
                        if (clRemoveShipFromSelection(todoselection,shiptoremove))
                        {
                            RemoveShipFromCommand(shiptoremove);
                            if (todo->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                            {
                                catRemoveShipFromAttackCommand(shiptoremove,todo);
                            }

                            RemoveShipFromMoving(shiptoremove);

                            if (todoselection->numShips == 0)
                            {
                                freeCode = 4;
                                goto freetodo;
                            }
                        }
                        break;

                    case COMMAND_NULL:
                    case COMMAND_HALT:
                        if (clRemoveShipFromSelection(todoselection,shiptoremove))
                        {
                            RemoveShipFromCommand(shiptoremove);

                            if (todo->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                            {
                                catRemoveShipFromAttackCommand(shiptoremove,todo);
                            }

                            if (todoselection->numShips == 0)
                            {
                                freeCode = 5;
                                goto freetodo;
                            }
                        }
                        break;

                    case COMMAND_SPECIAL:
                        if (clRemoveShipFromSelection(todoselection,shiptoremove))
                        {
                            RemoveShipFromCommand(shiptoremove);
                            RemoveShipFromSpecial(shiptoremove);

                            if (todoselection->numShips == 0)
                            {
                                freeCode = 6;
                                goto freetodo;
                            }
                        }
                        break;

                    case COMMAND_MILITARY_PARADE:
                        if (clRemoveShipFromSelection(todoselection,shiptoremove))
                        {
                            RemoveShipFromCommand(shiptoremove);

                            if (todo->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                            {
                                catRemoveShipFromAttackCommand(shiptoremove,todo);
                            }

                            RemoveShipFromMilitaryParade(shiptoremove,todo->militaryParade);

                            if (todoselection->numShips == 0)
                            {
                                freeCode = 7;
                                goto freetodo;
                            }
                        }
                        break;

                    case COMMAND_LAUNCH_SHIP:
                        if (clRemoveShipFromSelection(todoselection,shiptoremove))
                        {
                            RemoveShipFromCommand(shiptoremove);

                            if (todo->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                            {
                                catRemoveShipFromAttackCommand(shiptoremove,todo);
                            }

                            RemoveShipFromLaunching(shiptoremove);
                            if (todoselection->numShips == 0)
                            {
                                freeCode = 8;
                                goto freetodo;
                            }
                        }
                        break;

                    case COMMAND_DOCK:
                        if (clRemoveShipFromSelection(todoselection,shiptoremove))
                        {
                            RemoveShipFromCommand(shiptoremove);

                            if (todo->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                            {
                                catRemoveShipFromAttackCommand(shiptoremove,todo);
                            }

                            RemoveShipFromDocking(shiptoremove);
                            if (todoselection->numShips == 0)
                            {
                                freeCode = 9;
                                goto freetodo;
                            }
                        }
                        break;

                    case COMMAND_MP_HYPERSPACING:
                        if (clRemoveShipFromSelection(todoselection,shiptoremove))
                        {
                            RemoveShipFromCommand(shiptoremove);
                            removeShipFromMpHyperspaceing(shiptoremove);
                            if (todoselection->numShips == 0)
                            {
                                freeCode = 10;
                                goto freetodo;
                            }
                        }
                        break;

                    case COMMAND_CAT_LINK:
                    case COMMAND_CAT_UNLINK:
                        // CAT: the ship leaves the command (an emptied one stays for cat_processLinkToDo to finish);
                        //      its link is broken even when it was not in this command (0x00444f81 -> 0x00444fde)
                        if (clRemoveShipFromSelection(todoselection,shiptoremove))
                        {
                            RemoveShipFromCommand(shiptoremove);
                            if (todo->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                            {
                                catRemoveShipFromAttackCommand(shiptoremove,todo);
                            }
                        }
                        cat_clLinkCleanup(shiptoremove);
                        break;

                    case COMMAND_CAT_MIMIC:
                        if (clRemoveShipFromSelection(todoselection,shiptoremove))
                        {
                            RemoveShipFromCommand(shiptoremove);
                            if (todoselection->numShips == 0)
                            {
                                freeCode = 11;
                                goto freetodo;
                            }
                        }
                        break;

                    default:
                        // COMMAND_BUILDING_SHIP, COMMAND_CAT_POINT, COMMAND_CAT_13, COMMAND_CAT_RESEARCH and above:
                        // the exe's jump table leaves them alone
                        break;
                }

            }
        }

        todonode = todonode->next;
        continue;

freetodo:
        {
            Node *nextnode = todonode->next;
            FreeCommandToDoContents(todo,freeCode);
            listDeleteNode(todonode);
            todonode = nextnode;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : clDerelictDied
    Description : Removes derelict from command layer, such that it is
                  not the target of any ship.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clDerelictDied(CommandLayer *comlayer,DerelictPtr derelict)
{
    Node *todonode;
    CommandToDo *todo;

    todonode = comlayer->todolist.head;

    while (todonode != NULL)
    {
        todo = (CommandToDo *)listGetStructOfNode(todonode);

        if (todo->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
        {
            dbgAssertOrIgnore(todo->ordertype.order != COMMAND_ATTACK);
            if (clRemoveTargetFromSelection(todo->attack,(SpaceObjRotImpTarg *)derelict))
            {
                RemoveAttackTargetFromExtraAttackInfo((SpaceObjRotImpTarg *)derelict,todo);
            }
        }

        switch (todo->ordertype.order)
        {
            case COMMAND_NULL:
            case COMMAND_MOVE:
            case COMMAND_MP_HYPERSPACING:
            case COMMAND_DOCK:
            case COMMAND_LAUNCH_SHIP:
                break;
            case COMMAND_SPECIAL:
                if (todo->specialtargets)
                {
                    clRemoveShipFromSelection(todo->specialtargets,(SpaceObjRotImpTarg *)derelict);
                }
                break;
            case COMMAND_HALT:
            case COMMAND_BUILDING_SHIP:
            case COMMAND_COLLECT_RESOURCES:
            case COMMAND_MILITARY_PARADE:
                break;

            case COMMAND_ATTACK:
                if (clRemoveTargetFromSelection(todo->attack,(SpaceObjRotImpTarg *)derelict))
                {
                    RemoveAttackTargetFromExtraAttackInfo((SpaceObjRotImpTarg *)derelict,todo);
                }
                break;

            default:
                // CAT: no assert: the orders 11-17 come here (the exe tests only its few cases)
                break;
        }

        todonode = todonode->next;
    }
}

/*-----------------------------------------------------------------------------
    Name        : clResourceDied
    Description : Removes resource from command layer, such that it is
                  not the target of any ship.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clResourceDied(CommandLayer *comlayer,ResourcePtr resource)
{
    Node *todonode;
    CommandToDo *todo;

    todonode = comlayer->todolist.head;

    while (todonode != NULL)
    {
        todo = (CommandToDo *)listGetStructOfNode(todonode);

        if (todo->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
        {
            dbgAssertOrIgnore(todo->ordertype.order != COMMAND_ATTACK);
            if (clRemoveTargetFromSelection(todo->attack,(SpaceObjRotImpTarg *)resource))
            {
                RemoveAttackTargetFromExtraAttackInfo((SpaceObjRotImpTarg *)resource,todo);
            }
        }

        switch (todo->ordertype.order)
        {
            case COMMAND_NULL:
            case COMMAND_MOVE:
            case COMMAND_MP_HYPERSPACING:
            case COMMAND_DOCK:
            case COMMAND_LAUNCH_SHIP:
            case COMMAND_SPECIAL:
            case COMMAND_HALT:
            case COMMAND_BUILDING_SHIP:
            case COMMAND_MILITARY_PARADE:
                break;

            case COMMAND_ATTACK:
                if (clRemoveTargetFromSelection(todo->attack,(SpaceObjRotImpTarg *)resource))
                {
                    RemoveAttackTargetFromExtraAttackInfo((SpaceObjRotImpTarg *)resource,todo);
                }
                break;

            case COMMAND_COLLECT_RESOURCES:
                if (resource == todo->collect.resource)
                {
                    TurnOffAnyResourcingEffects(todo);
                    todo->collect.resource = NULL;
                }
                break;

            default:
                // CAT: no assert: the orders 11-17 come here (the exe tests only its few cases)
                break;
        }

        todonode = todonode->next;
    }
}

/*-----------------------------------------------------------------------------
    Name        : clMissileDied
    Description : Removes missile from command layer, such that it is
                  not the target of any ship.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clMissileDied(CommandLayer *comlayer,MissilePtr missile)
{
    Node *todonode;
    CommandToDo *todo;

    todonode = comlayer->todolist.head;

    while (todonode != NULL)
    {
        todo = (CommandToDo *)listGetStructOfNode(todonode);

        switch (todo->ordertype.order)
        {
            case COMMAND_NULL:
            case COMMAND_MOVE:
            case COMMAND_MP_HYPERSPACING:
            case COMMAND_DOCK:
            case COMMAND_LAUNCH_SHIP:
            case COMMAND_SPECIAL:
            case COMMAND_COLLECT_RESOURCES:
            case COMMAND_HALT:
            case COMMAND_BUILDING_SHIP:
            case COMMAND_MILITARY_PARADE:
                break;

            case COMMAND_ATTACK:
                if (clRemoveTargetFromSelection(todo->attack,(SpaceObjRotImpTarg *)missile))
                {
                    RemoveAttackTargetFromExtraAttackInfo((SpaceObjRotImpTarg *)missile,todo);
                }
                break;

            default:
                // CAT: no assert: the orders 11-17 come here (the exe tests only its few cases)
                break;
        }

        todonode = todonode->next;
    }
}

/*-----------------------------------------------------------------------------
    Name        : shipHasJustCloaked
    Description : cleans up te games such that the ship that just cloaked
                  isn';t referenced by ships n' such that it shouldn't be.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void shipHasJustCloaked(Ship *ship)
{
    RemoveShipFromBeingTargeted(&universe.mainCommandLayer,ship,REMOVE_CLOAKED,NULL);
    // CAT: "can the current player still see it" is Cataclysm's detection test (shipHasJustCloaked 0x004452f0 calls
    //      sub_53F9F0 = cat_shipDetectedByPlayer where HW1 asks proximityCanPlayerSeeShip)
    if(ship->playerowner != universe.curPlayerPtr &&
       !cat_shipDetectedByPlayer(ship,universe.curPlayerPtr->playerIndex))
    {           //if ship isn't players...remove it from camera stack
        ccRemoveShip(&universe.mainCameraCommand,ship);
    }
}

void shipHasJustDisappeared(Ship *ship)
{
    RemoveShipFromBeingTargeted(&universe.mainCommandLayer,ship,REMOVE_DISAPPEARED,NULL);
}

/*-----------------------------------------------------------------------------
    Name        : shipHasJustBeenDisabled
    Description : cleans up the game such that the ship that has just been disabled isn't
                  being attacked by ships, etc.
    Inputs      : CAT: catKeepInCamera - TRUE keeps the ship on the camera stack (the exe's two callers,
                  0x00429aa2 in the Beast weapon and cat_salvageSpecialTarget 0x004fb6f7, both pass 1)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void shipHasJustBeenDisabled(Ship *ship,bool32 catKeepInCamera)
{
    RemoveShipFromBeingTargeted(&universe.mainCommandLayer,ship,REMOVE_DISABLED,NULL);
    if (!catKeepInCamera)
    {
        ccRemoveShip(&universe.mainCameraCommand,ship);
    }
    // CAT: only the grav-well generators of both races have a disable hook (shipHasJustBeenDisabled 0x00445360: types
    //      64/79); no DDDFrigate or cloak generator case, and the three mad animations close for every ship type
    if ((ship->shiptype == kGravWellGenerator) || (ship->shiptype == tGravWellGenerator))
    {
        GravWellGeneratorJustDisabled(ship);
    }
    //if ship doesn't have the animation it won't matter
    madLinkInCloseGunsShip(ship);
    madLinkInPreDockingShip(ship);
    madLinkInCloseSpecialShip(ship);
}

// CAT: new (cat_shipHasJustBeenSalvaged 0x0044b450): the salvaged ship stops being a target of player's commands
void cat_shipHasJustBeenSalvaged(Ship *ship,struct Player *player)
{
    RemoveShipFromBeingTargeted(&universe.mainCommandLayer,ship,REMOVE_CAT_SALVAGED,player);
}

// removeShipsFromDockingWithThisShip
//
// If there are ships docking with this ship, they will be cancelled
void removeShipsFromDockingWithThisShip(Ship *shiptoremove)
{
    Node *todonode;
    CommandToDo *todo;

    if(!shiptoremove->staticinfo->canReceiveShips)
    {
        //ship can't receive ships therefore don't do check
        return;
    }

    todonode = universe.mainCommandLayer.todolist.head;
    while (todonode != NULL)
    {
        todo = (CommandToDo *)listGetStructOfNode(todonode);

        switch (todo->ordertype.order)
        {
            case COMMAND_DOCK:
                {
                    SelectCommand *selection = todo->selection;
                    sdword numShips = selection->numShips;
                    Ship *ship;
                    sdword i;
                    for (i=0;i<numShips;i++)
                    {
                        ship = selection->ShipPtr[i];
                        if (ship->dockvars.dockship == shiptoremove)      // CAT (M2-R5 dock): no HW1 busying host in the exe's dock vars
                        {
                            RemoveShipFromDocking(ship);
                        }
                    }
                }
                break;
        }
        todonode = todonode->next;
    }
}

/*-----------------------------------------------------------------------------
    Name        : RemoveShipFromBeingTargeted
    Description : removes shiptoremove from command layer, such that it is
                  not the target of any other ship
    Inputs      : shiptoremove, removeFlag which can be:
                  REMOVE_PROTECT - should remove ship from being protected
                  REMOVE_CLOAKED - ship was just cloaked.  If this flag is true, when the ship is removed,
                  it is only removed if the reference that is being considered CAN'T see the ship
                  REMOVE_DISABLED - ship was SOF_Disable'd (e.g. Salvaged)
                  REMOVE_DISAPPEARED - ship was hidden (SOF_Hide'd)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: RemoveShipFromBeingTargeted 0x004453c0 takes a fourth argument, the player for REMOVE_CAT_SALVAGED (only that
//      player's commands are touched; cat_shipHasJustBeenSalvaged 0x0044b450 passes it). Other deltas from HW1, all
//      from the exe body: the hyperspacing pass no longer ends the command's processing (the exe falls through to
//      the protect/attack/order checks); it and the dock order drop only the ships whose dockship is the ship (no
//      busyingThisShip, no AdvanceSupportFrigate latch case); the salvager checks use Cataclysm's salvagers
//      (cat_shipIsSalvager, the salvager block's target, CustShipRemoveShipReferences); "can still see it" is the
//      Cataclysm detection test cat_shipDetectedByPlayer (sub_53F9F0) for the owner of the command's first ship;
//      an attack on a crystal is kept when the crystal is disabled; ClearProtecting is inlined (same body); the
//      special order has the salvagers drop their references (cat_salvageRemoveShipReferences) instead of
//      salCapCleanUpCloakingTarget; the parade order only loses its protect/attack attributes (the inlined
//      ChangeOrderToHalt skips the halt for a parade, 0x004458f9); unknown orders are ignored (no assert).
void RemoveShipFromBeingTargeted(CommandLayer *comlayer,ShipPtr shiptoremove,udword removeFlag,struct Player *catPlayer)
{
    Node *todonode;
    Node *nextnode;
    CommandToDo *todo;
    udword REMOVE_PROTECT_FLAG = removeFlag & REMOVE_PROTECT;
    udword CLOAKED_REMOVAL_FLAG = removeFlag & REMOVE_CLOAKED;
    udword SHIP_JUST_DISABLED_FLAG = removeFlag & REMOVE_DISABLED;
    udword SHIP_DISAPPEARED_FLAG = removeFlag & REMOVE_DISAPPEARED;
    udword SHIP_HYPERSPACEING_FLAG = removeFlag & REMOVE_HYPERSPACING;
    udword SHIP_SALVAGED_FLAG = removeFlag & REMOVE_CAT_SALVAGED;
    SelectCommand *selection;
    sdword i;

// CAT: the exe's visibility test: removed unless the ship was cloaked and the command's owner still detects it
#define CL_CAN_REMOVE(todo) (SHIP_DISAPPEARED_FLAG || !CLOAKED_REMOVAL_FLAG ||                              \
        !cat_shipDetectedByPlayer(shiptoremove, (todo)->selection->ShipPtr[0]->playerowner->playerIndex))

    todonode = comlayer->todolist.head;

    while (todonode != NULL)
    {
        todo = (CommandToDo *)listGetStructOfNode(todonode);
        selection = todo->selection;

        // CAT: a salvaged ship is only dropped from the commands of the given player (0x0044540c)
        if (SHIP_SALVAGED_FLAG && (selection->numShips > 0) && (selection->ShipPtr[0]->playerowner != catPlayer))
        {
            todonode = todonode->next;
            continue;
        }

        //if ship is hyperspaceing out...via mp hp move
        if(SHIP_HYPERSPACEING_FLAG)
        {
            //prevent ships from docking with it!
            if(todo->ordertype.order == COMMAND_DOCK)
            {
                for (i=0;i<selection->numShips;i++)
                {
                    if (selection->ShipPtr[i]->dockvars.dockship == shiptoremove)
                    {
                        RemoveShipFromDocking(selection->ShipPtr[i]);
                    }
                }
            }
            else if((todo->ordertype.order == COMMAND_SPECIAL) && !SHIP_SALVAGED_FLAG)
            {
                for(i=0;i<todo->selection->numShips;i++)
                {
                    Ship *salvager = todo->selection->ShipPtr[i];
                    if (cat_shipIsSalvager(salvager) && (cat_salvageGetTarget(salvager) != NULL) &&
                        (cat_salvageGetTarget(salvager) == shiptoremove))
                    {
                        if(salvager->staticinfo->custshipheader.CustShipRemoveShipReferences != NULL)
                        {
                            salvager->staticinfo->custshipheader.CustShipRemoveShipReferences(salvager, shiptoremove);
                        }
                    }
                }
                if (CL_CAN_REMOVE(todo) && !SHIP_JUST_DISABLED_FLAG && (todo->specialtargets != NULL))
                {
                    clRemoveShipFromSelection(todo->specialtargets,shiptoremove);
                }
            }
        }

        if(REMOVE_PROTECT_FLAG)
        {
            if ((todo->ordertype.attributes & COMMAND_MASK_PROTECTING) && CL_CAN_REMOVE(todo))
            {
                clRemoveShipFromSelection((SelectCommand *)todo->protect,shiptoremove);

                if (todo->protect->numShips == 0)
                {
                    ClearProtecting(todo);

                    if ((todo->ordertype.attributes == 0) && (todo->ordertype.order == COMMAND_NULL))
                    {
                        nextnode = todonode->next;
                        FreeCommandToDoContents(todo,0xc);
                        listDeleteNode(todonode);
                        todonode = nextnode;
                        continue;
                    }
                }
            }
        }
        if ((todo->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING) && CL_CAN_REMOVE(todo))
        {
            if (clRemoveShipFromSelection((SelectCommand *)todo->attack,shiptoremove))
            {
                RemoveAttackTargetFromExtraAttackInfo((SpaceObjRotImpTarg *)shiptoremove,todo);
            }
        }

        switch (todo->ordertype.order)
        {
            case COMMAND_ATTACK:
                // CAT: a disabled crystal stays a target (cat_shipIsCrystal, 0x004456b0)
                if (CL_CAN_REMOVE(todo) && (!SHIP_JUST_DISABLED_FLAG || !cat_shipIsCrystal(shiptoremove)))
                {
                    if (clRemoveShipFromSelection((SelectCommand *)todo->attack,shiptoremove))
                    {
                        RemoveAttackTargetFromExtraAttackInfo((SpaceObjRotImpTarg *)shiptoremove,todo);
                    }
                }
                break;

            case COMMAND_SPECIAL:
                if (!SHIP_SALVAGED_FLAG && CL_CAN_REMOVE(todo) && !SHIP_JUST_DISABLED_FLAG && (todo->specialtargets != NULL))
                {
                    clRemoveShipFromSelection(todo->specialtargets,shiptoremove);
                    for(i=0;i<todo->selection->numShips;i++)
                    {
                        // CAT: the exe's trJunkYardDawg case is a compiled-out nullsubReturnFalse
                        if (cat_shipIsSalvager(todo->selection->ShipPtr[i]))
                        {
                            cat_salvageRemoveShipReferences(todo->selection->ShipPtr[i],shiptoremove);
                        }
                    }
                }
                break;

            case COMMAND_DOCK:
                if (!SHIP_DISAPPEARED_FLAG)
                {
                    for (i=0;i<selection->numShips;i++)
                    {
                        if (selection->ShipPtr[i]->dockvars.dockship == shiptoremove)
                        {
                            RemoveShipFromDocking(selection->ShipPtr[i]);
                        }
                    }
                }
                break;

            case COMMAND_LAUNCH_SHIP:
                if (!SHIP_DISAPPEARED_FLAG)
                if (shiptoremove == todo->launchship.receiverShip)
                {
                    Ship *ship = todo->selection->ShipPtr[0];

                    RemoveShipFromLaunching(ship);

                    nextnode = todonode->next;
                    FreeCommandToDoContents(todo,0xd);
                    listDeleteNode(todonode);
                    todonode = nextnode;
                    continue;
                }
                break;

            case COMMAND_BUILDING_SHIP:
                if (!SHIP_DISAPPEARED_FLAG)
                if (shiptoremove == todo->buildingship.creator)
                {
                    nextnode = todonode->next;
                    FreeCommandToDoContents(todo,0xe);
                    listDeleteNode(todonode);
                    todonode = nextnode;
                    continue;
                }
                break;

            case COMMAND_MILITARY_PARADE:
                if (!SHIP_DISAPPEARED_FLAG)
                if (shiptoremove == todo->militaryParade->aroundShip)
                {
                    // CAT: the exe's inlined ChangeOrderToHalt (0x00445898..0x00445930): the attributes are cleared, and
                    //      the halt itself is skipped because the order is a parade
                    if (todo->ordertype.attributes & COMMAND_MASK_PROTECTING)
                    {
                        ClearProtecting(todo);
                    }
                    if (todo->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                    {
                        ClearPassiveAttacking(todo);
                    }
                    bitClear(todo->ordertype.attributes,COMMAND_MASK_ATTACKING_AND_MOVING);
                }
                break;

            default:
                break;
        }
        todonode = todonode->next;
    }
#undef CL_CAN_REMOVE
}

/*=============================================================================
    Public Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : clInit
    Description : Initializes the command layer
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clInit(CommandLayer *comlayer)
{
    listInit(&comlayer->todolist);
}

// CAT: the real time of the last out-of-resources message (0x00a2db50, cat_clHarvestNearestResource); clClose and
//      clReset clear it
static real32 catHarvestMessageTime = 0.0f;

/*-----------------------------------------------------------------------------
    Name        : clClose
    Description : Closes the command layer
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clClose(CommandLayer *comlayer)
{
    Node *nextnode;
    Node *curnode = comlayer->todolist.head;
    CommandToDo *command;

    // free contents of each node of list
    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);
        nextnode = curnode->next;

        if (singlePlayerGameLoadNewLevelFlag)
        {
            // preserve parade formation throughout hyperspace
            if (command->ordertype.order == COMMAND_MILITARY_PARADE)
            {
                Ship *aroundship = command->militaryParade->aroundShip;
                if ((aroundship) && (aroundship->playerowner == universe.curPlayerPtr))
                {
                    goto dontfreeit;
                }
            }
        }

        FreeCommandToDoContents(command,0xf);
        listDeleteNode(curnode);

dontfreeit:
        curnode = nextnode;
    }
    catHarvestMessageTime = 0.0f;       // CAT: clClose 0x00445a00 / clReset 0x00445b40 (inlined copy)
}

/*-----------------------------------------------------------------------------
    Name        : clCancelAllLaunchOrdersFromPlayer
    Description : Cancels all launch orders so ships in process of launching
                  will be available to dock and to fix a fatal bug with the single player game
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

void clCancelAllLaunchOrdersFromPlayer(struct Player *player)
{
    Node *nextnode;
    Node *curnode = universe.mainCommandLayer.todolist.head;
    CommandToDo *command;
    sdword i;

    dbgAssertOrIgnore(singlePlayerGame);

    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);
        nextnode = curnode->next;


        if (command->ordertype.order == COMMAND_LAUNCH_SHIP)
        {
            for(i=0;i<command->selection->numShips;i++)
            {
                if(command->selection->ShipPtr[i]->playerowner == player)
                {
                    // CAT: clCancelAllLaunchOrdersFromPlayer 0x00445a70 (map: IDENTICAL): capital ships and modules
                    //      (CLASS_Component) stay; the slot's ship loses its command after the removal (the moved-in
                    //      ship when another took the slot, as the exe does)
                    if (!command->selection->ShipPtr[i]->staticinfo->shipIsCapital &&
                        (command->selection->ShipPtr[i]->staticinfo->shipclass != CLASS_Component))
                    {
                        RemoveShipFromLaunching(command->selection->ShipPtr[i]);
                        if(clRemoveShipFromSelection(command->selection,command->selection->ShipPtr[i]))
                        {
                            command->selection->ShipPtr[i]->command = NULL;
                            if(command->selection->numShips == 0)
                            {
                                FreeCommandToDoContents(command,0x10);
                                listDeleteNode(curnode);
                                break;
                            }
                        }
                    }
                }
            }
        }

        curnode = nextnode;
    }
}

/*-----------------------------------------------------------------------------
    Name        : clReset
    Description : Resets the command layer
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clReset(CommandLayer *comlayer)
{
    clClose(comlayer);
}

/*-----------------------------------------------------------------------------
    Name        : IsSelectionAlreadyDoingSomething
    Description : sees if the selection specified is alread in the command layer
    Inputs      :
    Outputs     :
    Return      : CommandToDo pointer if the selection specified is already
                  present, NULL otherwise
----------------------------------------------------------------------------*/
CommandToDo *IsSelectionAlreadyDoingSomething(CommandLayer *comlayer,SelectCommand *selectcom)
{
    Node *curnode = comlayer->todolist.head;
    CommandToDo *command;

    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);

        if (SelectionsAreEquivalent(selectcom,command->selection))
        {
            return command;
        }

        curnode = curnode->next;
    }

    return NULL;
}

//returns TRUE if a strike craft is in the selection <corvette or fighter>
bool32 selectionHasStrikeCraft(SelectCommand *selection)
{
    sdword i;

    for(i=0;i<selection->numShips;i++)
    {
        if(selection->ShipPtr[i]->staticinfo->shipclass == CLASS_Fighter ||
           selection->ShipPtr[i]->staticinfo->shipclass == CLASS_Corvette)
        {
            return TRUE;
        }
    }
    return FALSE;
}

bool32 isSelectionExclusivlyStrikeCraft(SelectCommand *selection)
{
    sdword i;

    for(i=0;i<selection->numShips;i++)
    {
        if(!(selection->ShipPtr[i]->staticinfo->shipclass == CLASS_Fighter ||
           selection->ShipPtr[i]->staticinfo->shipclass == CLASS_Corvette))
        {
            return FALSE;
        }
    }
    return TRUE;
}

bool32 isSelectionExclusivlyStrikeCraftorNoMoveAndAttackShips(SelectCommand *selection)
{
    sdword i;

    for(i=0;i<selection->numShips;i++)
    {
        if(!(selection->ShipPtr[i]->staticinfo->shipclass == CLASS_Fighter ||
           selection->ShipPtr[i]->staticinfo->shipclass == CLASS_Corvette ||
             selection->ShipPtr[i]->staticinfo->cantMoveAndAttack))
        {
            return FALSE;
        }
    }
    return TRUE;
}

/*
;=============================================================================
; Solution for ships moving to single point nastiness:
;=============================================================================

; If it looks like a bunch of ships in different parts of the world are moving to a "central location" then
; their relative positions will be scaled near to a single point using the factor SHRINKMIN.
; If it looks like a bunch of ships are moving to a relatively far location, they will preserve their
; relative positions (SHRINKMAX, which is typically 1.0)

; Here is the technical description:

; consider a distance d, which is the average position the ships travel.
; consider a radius r, within which all ships ordered to move are within

; Take the ratio d/r.
; if d/r is less than DISTMINRATIO, the relative scaling factor will be SHRINKMIN
; if d/r is greater than DISTMAXRATIO, the relative scaling factor will be SHRINKMAX
; in between,
; scales from a     d/r ratio of DISTMINRATIO      to      a scale factor of SHRINKMIN
;                                DISTMAXRATIO                                SHRINKMAX
*/

typedef struct
{
    vector fromtoship;
    vector totoship;
} ShipMoveCalcs;

#define NUM_STACK_MOVECALCS 100

/*-----------------------------------------------------------------------------
    Name        : CalculateMoveToPoints
    Description :
    Inputs      : selection, from, to
    Outputs     : moveTo vectors are filled in
    Return      :
----------------------------------------------------------------------------*/
void CalculateMoveToPoints(SelectCommand *selection,vector from,vector to)
{
    sdword numShips = selection->numShips;
    ShipMoveCalcs stackMoveCalcs[NUM_STACK_MOVECALCS];
    bool32 moveCalcsAllocated = FALSE;
    ShipMoveCalcs *movecalcs;
    sdword i;
    vector r;
    double dx, dy, dz, rsqrd, maxrsqrd;
    real32 dmag;
    real32 maxrsqr;
    double maxrmag;
    double scale,doverr;

    dbgAssertOrIgnore(numShips > 0);

    if (numShips == 1)
    {
        selection->ShipPtr[0]->moveTo = to;
        return;
    }

    // CAT (M2-R8 oracle-rung-physics): the x87 shape (CalculateMoveToPoints 0x00445bf0): to - from stays double, its
    //      (z*z + y*y) + x*x goes to fsqrt and the distance is stored as a float; each ship's squared distance is a double
    //      compared with the running maximum, which is kept as the float the exe stores; the maximum's square root, the
    //      ratio and the scale ((SHRINKMAX - SHRINKMIN) * (d/r - 1)) / (DISTMAXRATIO - 1) + SHRINKMIN are doubles, and
    //      each offset is scaled with one rounding per component
    dx = (double)to.x - from.x;
    dy = (double)to.y - from.y;
    dz = (double)to.z - from.z;
    dmag = (real32)catFsqrtD((dz * dz + dy * dy) + dx * dx);

    if (numShips > NUM_STACK_MOVECALCS)
    {
        moveCalcsAllocated = TRUE;
        movecalcs = memAlloc(sizeof(ShipMoveCalcs)*numShips,"movecalcs",0);
    }
    else
    {
        movecalcs = stackMoveCalcs;
    }

    maxrsqrd = -1.0;
    maxrsqr = -1.0f;

    for (i=0;i<numShips;i++)
    {
        vecSub(r,selection->ShipPtr[i]->obj.posinfo.position,from);
        movecalcs[i].fromtoship = r;
        movecalcs[i].totoship = r;

        rsqrd = ((double)r.z * r.z + (double)r.y * r.y) + (double)r.x * r.x;
        if (rsqrd > maxrsqrd)
        {
            maxrsqr = (real32)rsqrd;
            maxrsqrd = maxrsqr;
        }
    }

    dbgAssertOrIgnore(maxrsqr >= 0.0f);
    maxrmag = catFsqrtD(maxrsqr);

    // scales from a d/r ratio of DISTMINRATIO      to      SHRINKMIN
    //                            DISTMAXRATIO              SHRINKMAX

    if ((double)dmag < maxrmag)      // assumes DISTMINRATIO = 1.0
    {
        // shrink to a point:
        scale = SHRINKMIN;
    }
    else
    {
        // preserve relative movement
        doverr = (double)dmag / maxrmag;

        if (doverr >= (double)DISTMAXRATIO)
        {
            // skip scaling
            scale = SHRINKMAX;
        }
        else
        {
            // somewhere between moving to a point and relative movement

            scale = (((double)SHRINKMAX - SHRINKMIN) * (doverr - 1.0)) / ((double)DISTMAXRATIO - 1.0) + SHRINKMIN;
        }
    }

    if (scale != 1.0)
    for (i=0;i<numShips;i++)
    {
        movecalcs[i].totoship.x = (real32)(scale * movecalcs[i].totoship.x);
        movecalcs[i].totoship.y = (real32)(scale * movecalcs[i].totoship.y);
        movecalcs[i].totoship.z = (real32)(scale * movecalcs[i].totoship.z);
    }

    for (i=0;i<numShips;i++)
    {
        vecAdd(selection->ShipPtr[i]->moveTo,to,movecalcs[i].totoship);
    }

    if (moveCalcsAllocated)
    {
        memFree(movecalcs);
    }
}
/*-----------------------------------------------------------------------------
    Name        : cat_clCalculateMoveToOffsets
    Description : CalculateMoveToPoints' scaled offsets, written to outOffsets[i] for each ship instead of being added
                  to the ships' moveTo (a single ship gets offset 0)
    Inputs      : selection, from, to
    Outputs     : outOffsets (selection->numShips vectors)
    Return      :
----------------------------------------------------------------------------*/
// CAT: new (cat_clCalculateMoveToOffsets 0x00445e80): the same scaling as CalculateMoveToPoints; ChangeOrderToMove,
//      clMoveThese and clMove use it to give every ship of a group its own waypoint path
void cat_clCalculateMoveToOffsets(SelectCommand *selection,vector from,vector to,vector *outOffsets)
{
    sdword numShips = selection->numShips;
    ShipMoveCalcs stackMoveCalcs[NUM_STACK_MOVECALCS];
    bool32 moveCalcsAllocated = FALSE;
    ShipMoveCalcs *movecalcs;
    sdword i;
    vector d,r;
    real32 dsqr,rsqr,dmag;
    real32 maxrsqr,maxrmag;
    real32 scale,doverr;

    if (numShips == 1)
    {
        vecZeroVector(outOffsets[0]);
        return;
    }

    vecSub(d,to,from);
    dsqr = vecMagnitudeSquared(d);
    dmag = fsqrt(dsqr);

    if (numShips > NUM_STACK_MOVECALCS)
    {
        moveCalcsAllocated = TRUE;
        movecalcs = memAlloc(sizeof(ShipMoveCalcs)*numShips,"movecalcs",0);
    }
    else
    {
        movecalcs = stackMoveCalcs;
    }

    maxrsqr = -1.0f;

    for (i=0;i<numShips;i++)
    {
        vecSub(r,selection->ShipPtr[i]->obj.posinfo.position,from);
        movecalcs[i].fromtoship = r;
        movecalcs[i].totoship = r;

        rsqr = vecMagnitudeSquared(r);
        if (rsqr > maxrsqr)
        {
            maxrsqr = rsqr;
        }
    }

    maxrmag = fsqrt(maxrsqr);

    if (dmag < maxrmag)
    {
        scale = SHRINKMIN;
    }
    else
    {
        doverr = dmag / maxrmag;

        if (doverr >= DISTMAXRATIO)
        {
            scale = SHRINKMAX;
        }
        else
        {
            scale = ((doverr - DISTMINRATIO) * (SHRINKMAX - SHRINKMIN) / (DISTMAXRATIO - DISTMINRATIO)) + SHRINKMIN;
        }
    }

    if (scale != 1.0f)
    for (i=0;i<numShips;i++)
    {
        vecMultiplyByScalar(movecalcs[i].totoship,scale);
    }

    for (i=0;i<numShips;i++)
    {
        outOffsets[i] = movecalcs[i].totoship;
    }

    if (moveCalcsAllocated)
    {
        memFree(movecalcs);
    }
}

// CAT: the ships Cataclysm moves on their own out of an attack-move: fighters, corvettes and the cantMoveAndAttack
//      types (the inline tests of ChangeOrderToMove 0x004462df..0x0044637c)
static bool32 catShipMovesOutOfAttackMove(Ship *ship)
{
    return (ship->staticinfo->shipclass == CLASS_Fighter) || (ship->staticinfo->shipclass == CLASS_Corvette) ||
           ship->staticinfo->cantMoveAndAttack;
}

// CAT: stores a waypoint path in a move order (the inline stores of clMoveThese 0x00446bf2, ChangeOrderToMove
//      0x00446498 / 0x0044661f, ChangeOrderToHyperSpace 0x004471c5): current point 0
static void catSetMovePath(CommandToDo *command,sdword pathType,sdword numPoints,vector *points)
{
    sdword i;

    dbgAssertOrIgnore(numPoints <= CAT_MOVE_MAX_POINTS);
    command->move.curPoint = 0;
    command->move.pathType = pathType;
    command->move.numPoints = numPoints;
    for (i = 0; i < numPoints; i++)
    {
        command->move.points[i] = points[i];
    }
}

/*-----------------------------------------------------------------------------
    Name        : ChangeOrderToMove
    Description : changes an existing command to a move along a waypoint path
    Inputs      : alreadycommand, pathType (CAT_MOVEPATH_*), from, numPoints, points
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: ChangeOrderToMove 0x004460d0 takes a waypoint path (HW1: from, to). A group not in formation is split: every
//      ship but the last left in the command gets its own single-ship move (clMoveThese with the split flag) along the
//      path shifted by its cat_clCalculateMoveToOffsets offset (the exe always takes ShipPtr[0] and walks the offsets
//      in order while the removal swaps the last ship into slot 0; kept as the exe does it). An attack becomes an
//      attack-move only for capital ships: fighters, corvettes and cantMoveAndAttack ships of a mixed group get their
//      own move (and formation), a group of them only moves. No heading is kept (the exe's move has none).
void ChangeOrderToMove(CommandToDo *alreadycommand,sdword pathType,vector from,sdword numPoints,vector *points)
{
    SelectCommand *selectcom = alreadycommand->selection;
    sdword i,j;

    if (alreadycommand->ordertype.attributes & COMMAND_MASK_PROTECTING)
    {
        ClearProtecting(alreadycommand);
    }

    if (alreadycommand->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        ClearPassiveAttacking(alreadycommand);
    }

    if ((selectcom->numShips > 1) && !(alreadycommand->ordertype.attributes & COMMAND_MASK_FORMATION))
    {
        vector offsets[COMMAND_MAX_SHIPS];
        vector shifted[CAT_MOVE_MAX_POINTS];
        vector shiftedfrom;
        MaxSelection single;

        cat_clCalculateMoveToOffsets(selectcom,from,points[0],offsets);
        for (i = 0; (selectcom->numShips > 1) && (i < 99999); i++)
        {
            single.numShips = 1;
            single.ShipPtr[0] = selectcom->ShipPtr[0];
            for (j = 0; j < numPoints; j++)
            {
                vecAdd(shifted[j],points[j],offsets[i]);
            }
            vecAdd(shiftedfrom,from,offsets[i]);
            clMoveThese(&universe.mainCommandLayer,(SelectCommand *)&single,pathType,shiftedfrom,numPoints,shifted,TRUE);
        }
    }

    if (alreadycommand->ordertype.order == COMMAND_ATTACK)
    {
        bool32 anyAlone = FALSE;
        bool32 anyCapital = FALSE;

        for (i = 0; i < alreadycommand->selection->numShips; i++)
        {
            if (catShipMovesOutOfAttackMove(alreadycommand->selection->ShipPtr[i]))
            {
                anyAlone = TRUE;
            }
            else
            {
                anyCapital = TRUE;
            }
        }
        if (anyAlone && !anyCapital)
        {
            goto plainmove;     // only fighters, corvettes and cantMoveAndAttack ships: a move order (0x0044637c)
        }
        if (anyAlone)
        {
            // some capitals and some strike craft: the strike craft move on their own
            MaxSelection tempSelection;

            tempSelection.numShips = 0;
            for (i = 0; i < alreadycommand->selection->numShips; i++)
            {
                if (catShipMovesOutOfAttackMove(alreadycommand->selection->ShipPtr[i]))
                {
                    tempSelection.ShipPtr[tempSelection.numShips++] = alreadycommand->selection->ShipPtr[i];
                }
            }
            RemoveShipsFromDoingStuff(&universe.mainCommandLayer,(SelectCommand *)&tempSelection,2);
            clMoveThese(&universe.mainCommandLayer,(SelectCommand *)&tempSelection,pathType,from,numPoints,points,FALSE);
            if (alreadycommand->ordertype.attributes & COMMAND_MASK_FORMATION)
            {
                clFormation(&universe.mainCommandLayer,(SelectCommand *)&tempSelection,alreadycommand->formation.formationtype,
                            alreadycommand->formation.catForceFieldLevel);
            }
        }
        // just capital ships in the attack: attack-move
        if (!bitTest(alreadycommand->selection->ShipPtr[0]->shipOrDerelict.specialFlags,SPECIAL_ATTACKMOVECANCEL))
        {
            alreadycommand->ordertype.attributes |= COMMAND_MASK_ATTACKING_AND_MOVING;
            catSetMovePath(alreadycommand,pathType,numPoints,points);

            InitShipsForAI(selectcom,TRUE);

            CalculateMoveToPoints(selectcom,from,points[0]);

            for (i=0;i<selectcom->numShips;i++)
            {
                selectcom->ShipPtr[i]->moveFrom = selectcom->ShipPtr[i]->obj.posinfo.position;
            }
            PrepareShipsForCommand(alreadycommand,TRUE);
            return;
        }
        bitClear(alreadycommand->selection->ShipPtr[0]->shipOrDerelict.specialFlags,SPECIAL_ATTACKMOVECANCEL);
    }

plainmove:
    if (alreadycommand->ordertype.order != COMMAND_MOVE)
    {
        FreeLastOrder(alreadycommand);

        alreadycommand->ordertype.order = COMMAND_MOVE;
    }

    catSetMovePath(alreadycommand,pathType,numPoints,points);

    if (alreadycommand->ordertype.attributes & COMMAND_MASK_FORMATION)
    {
        InitShipAI(selectcom->ShipPtr[0],TRUE);
    }
    else
    {
        InitShipsForAI(selectcom,TRUE);
    }

    CalculateMoveToPoints(selectcom,from,points[0]);

    for (i=0;i<selectcom->numShips;i++)
    {
        selectcom->ShipPtr[i]->moveFrom = selectcom->ShipPtr[i]->obj.posinfo.position;
        soundEventStartEngine(selectcom->ShipPtr[i]);
    }

    PrepareShipsForCommand(alreadycommand,TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : clMoveThese
    Description : Command to move ships along a waypoint path
    Inputs      : selectcom, pathType, from, numPoints, points, catSplit
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: clMoveThese 0x004467c0 takes the waypoint path and a split flag. Without the flag a selection already doing
//      something changes its order, and the exe then walks the ships that are attacking for a single-point move: it
//      copies their targets and formation but never fills the selection it passes on, so its
//      RemoveShipsFromDoingStuff / clFormation / clAttackThese / clMoveThese calls get no ship and do nothing (the
//      walk is not kept; 0x0044688d stores 0 ships). Then every ship of a group gets its own one-ship move command
//      (the path shifted by its cat_clCalculateMoveToOffsets offset, the split flag set); a single ship gets the command.
void clMoveThese(CommandLayer *comlayer,SelectCommand *selectcom,sdword pathType,vector from,sdword numPoints,vector *points,bool32 catSplit)
{
    CommandToDo *newcommand;
    CommandToDo *alreadycommand;
    SelectCommand *selection;
    udword sizeofselection;
    sdword numShips = selectcom->numShips;
    sdword i,j;

    if (numShips == 0)
    {
        return;
    }
    if (!catSplit)
    {
        if ((alreadycommand = IsSelectionAlreadyDoingSomething(comlayer,selectcom)) != NULL)
        {
            ChangeOrderToMove(alreadycommand,pathType,from,numPoints,points);
            return;
        }
    }

    RemoveShipsFromDoingStuff(comlayer,selectcom,4);

    if (selectcom->numShips > 1)
    {
        vector offsets[COMMAND_MAX_SHIPS];
        vector shifted[CAT_MOVE_MAX_POINTS];
        vector shiftedfrom;
        MaxSelection single;

        cat_clCalculateMoveToOffsets(selectcom,from,points[0],offsets);
        for (i = 0; i < selectcom->numShips; i++)
        {
            single.numShips = 1;
            single.ShipPtr[0] = selectcom->ShipPtr[i];
            for (j = 0; j < numPoints; j++)
            {
                vecAdd(shifted[j],points[j],offsets[i]);
            }
            vecAdd(shiftedfrom,from,offsets[i]);
            clMoveThese(comlayer,(SelectCommand *)&single,pathType,shiftedfrom,numPoints,shifted,TRUE);
        }
        return;
    }

    newcommand = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
    InitCommandToDo(newcommand);

    sizeofselection = sizeofSelectCommand(numShips);
    selection = memAlloc(sizeofselection,"ToDoSelection",NonVolatile);
    memcpy(selection,selectcom,sizeofselection);

    newcommand->selection = selection;
    catSetMovePath(newcommand,pathType,numPoints,points);
    newcommand->ordertype.order = COMMAND_MOVE;
    newcommand->ordertype.attributes = 0;

    InitShipsForAI(selection,TRUE);

    CalculateMoveToPoints(selection,from,points[0]);

    for (i=0;i<numShips;i++)
    {
        selection->ShipPtr[i]->moveFrom = selection->ShipPtr[i]->obj.posinfo.position;
        soundEventStartEngine(selectcom->ShipPtr[i]);
    }

    PrepareShipsForCommand(newcommand,TRUE);

    listAddNode(&comlayer->todolist,&newcommand->todonode,newcommand);
}

/*-----------------------------------------------------------------------------
    Name        : clMove
    Description : Command to move ships along a waypoint path
    Inputs      : selectcom, pathType (CAT_MOVEPATH_*), from, numPoints, points
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: clMove 0x00446d90: the waypoint path (HW1: from, to); each ship's header can veto the move (slot 20
//      CustShipMoveCommand at statinfo+0x158: the exe passes (ship, from, numPoints, points), and every implementer
//      (ACV, Acolyte, Mothership, RammingFrigate, HiveFrigate) reads only the ship); retreating ships are left out
//      (cat_tacticsRemoveRetreatingShips); a lone ship of an existing command keeps its own offset into the path. No
//      MakeShipsMobile (the exe's clMove has no such call) and no salCapExtraSpecialOrderCleanUp.
void clMove(CommandLayer *comlayer,SelectCommand *selectcom,sdword pathType,vector from,sdword numPoints,vector *points)
{
    SelectCommand *copycom;
    Node *curnode;
    Node *endnode;
    Node *nextnode;
    CommandToDo *command;
    sdword i,j;

    if (selectcom->numShips < 1)
    {
        return;
    }

    tacticsReportMove(comlayer,selectcom);

    copycom = memAlloc(sizeofSelectCommand(selectcom->numShips),"cc2(copycom)", Pyrophoric);
    copycom->numShips = 0;
    for (i = 0; i < selectcom->numShips; i++)
    {
        Ship *ship = selectcom->ShipPtr[i];
        if ((ship->staticinfo->custshipheader.CustShipMoveCommand == NULL) ||
            ship->staticinfo->custshipheader.CustShipMoveCommand(ship))
        {
            copycom->ShipPtr[copycom->numShips++] = ship;
        }
    }
    cat_tacticsRemoveRetreatingShips(copycom);

    if (copycom->numShips > 0)
    {
        // Now see if the selected ships are already in the command layer, and if so, individually order these groups
        // to move (to preserve formations)

        curnode = comlayer->todolist.head;
        endnode = comlayer->todolist.tail;

        while (curnode != NULL)
        {
            command = (CommandToDo *)listGetStructOfNode(curnode);

            nextnode = curnode->next;

            if (TheseShipsAreInSelection(command->selection,copycom))
            {
                MakeShipsNotIncludeTheseShips(copycom,command->selection);
                if (command->selection->numShips == 1)
                {
                    // CAT: a one-ship command moves along the path shifted by that ship's offset in the whole order
                    //      (0x00446ec4)
                    vector offsets[COMMAND_MAX_SHIPS];
                    vector shifted[CAT_MOVE_MAX_POINTS];
                    vector shiftedfrom;

                    cat_clCalculateMoveToOffsets(selectcom,from,points[0],offsets);
                    for (i = 0; selectcom->ShipPtr[i] != command->selection->ShipPtr[0]; i++)
                    {
                        ;
                    }
                    for (j = 0; j < numPoints; j++)
                    {
                        vecAdd(shifted[j],points[j],offsets[i]);
                    }
                    vecAdd(shiftedfrom,from,offsets[i]);
                    ChangeOrderToMove(command,pathType,shiftedfrom,numPoints,shifted);
                }
                else
                {
                    ChangeOrderToMove(command,pathType,from,numPoints,points);
                }
            }

            if (copycom->numShips == 0)
            {
                break;
            }

            if (curnode == endnode)
            {
                break;
            }
            curnode = nextnode;
        }

        // tell rest of ships left to move too.
        if (copycom->numShips != 0)
        {
            clMoveThese(comlayer,copycom,pathType,from,numPoints,points,FALSE);
        }
    }

    memFree(copycom);
}


// CAT: the Cataclysm grav-well generators are kGravWellGenerator and tGravWellGenerator (clMpHyperspaceThese
//      0x004472a0 tests types 0x40 / 0x4f and the specifics' field-on flag)
void TurnAllGravwellsOff(SelectCommand *selectcom)
{
    sdword i;

    for(i=0;i<selectcom->numShips;i++)
    {
        if ((selectcom->ShipPtr[i]->shiptype == kGravWellGenerator) || (selectcom->ShipPtr[i]->shiptype == tGravWellGenerator))
        {
            if( ((GravWellGeneratorSpec *)(selectcom->ShipPtr[i]->ShipSpecifics))->GravFieldOn)
            {
                turnoffGravwell(selectcom->ShipPtr[i]);
            }
        }
    }
}

// CAT: ChangeOrderToHyperSpace 0x004470f0: the destination is a one-point move path (pathType 0, point 0 = to)
void ChangeOrderToHyperSpace(CommandToDo *alreadycommand,vector from,vector to)
{
    SelectCommand *selectcom = alreadycommand->selection;

    if (alreadycommand->ordertype.attributes & COMMAND_MASK_PROTECTING)
    {
        ClearProtecting(alreadycommand);
    }
    if (alreadycommand->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        ClearPassiveAttacking(alreadycommand);
    }

    if (alreadycommand->ordertype.order == COMMAND_MP_HYPERSPACING)
    {
        dbgMessagef("A little wierd:  Ships that were hyperspaceing have been given an order to hyperspace again.  Ignoreing...");
        return;
    }
    else
    {
        FreeLastOrder(alreadycommand);

        alreadycommand->ordertype.order = COMMAND_MP_HYPERSPACING;
    }

    catSetMovePath(alreadycommand,CAT_MOVEPATH_ONCE,1,&to);

    if (alreadycommand->ordertype.attributes & COMMAND_MASK_FORMATION)
    {
        InitShipAI(selectcom->ShipPtr[0],TRUE);
    }
    else
    {
        InitShipsForAI(selectcom,TRUE);
    }

    alreadycommand->hyperSpaceingTime = 0.0f;
    alreadycommand->hyperspaceState = COM_HYPERSPACE_START;
    PrepareShipsForCommand(alreadycommand,TRUE);

    return;
}

/*-----------------------------------------------------------------------------
    Name        : clMpHyperspaceThese
    Description : Command to mp hyperspace ships
    Inputs      : selectcom, from, to
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: clMpHyperspaceThese 0x004472a0: the destination is a one-point move path
void clMpHyperspaceThese(CommandLayer *comlayer,SelectCommand *selectcom,vector from,vector to)
{
    CommandToDo *newcommand;
    CommandToDo *alreadycommand;
    SelectCommand *selection;
    udword sizeofselection;
    sdword numShips = selectcom->numShips;

    if (numShips == 0)
    {
        return;
    }
    if ((alreadycommand = IsSelectionAlreadyDoingSomething(comlayer,selectcom)) != NULL)
    {
        ChangeOrderToHyperSpace(alreadycommand,from,to);
        return;
    }

    TurnAllGravwellsOff(selectcom);

    RemoveShipsFromDoingStuff(comlayer,selectcom,5);

    newcommand = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
    InitCommandToDo(newcommand);

    sizeofselection = sizeofSelectCommand(numShips);
    selection = memAlloc(sizeofselection,"ToDoSelection",NonVolatile);
    memcpy(selection,selectcom,sizeofselection);

    newcommand->selection = selection;
    catSetMovePath(newcommand,CAT_MOVEPATH_ONCE,1,&to);
    newcommand->ordertype.order = COMMAND_MP_HYPERSPACING;
    newcommand->ordertype.attributes = 0;
    newcommand->hyperSpaceingTime = 0.0f;
    newcommand->hyperspaceState = COM_HYPERSPACE_START;

    InitShipsForAI(selection,TRUE);

    PrepareShipsForCommand(newcommand,TRUE);

    listAddNode(&comlayer->todolist,&newcommand->todonode,newcommand);
}

/*-----------------------------------------------------------------------------
    Name        : getNewHyperspaceingDestination
    Description : calculates the correct destination point for a group of ships hyperspacing
    Inputs      : selection, from, to,newTo
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void getNewHyperspaceingDestination(vector *newTo,vector *from,vector *to,SelectCommand *selection)
{
    //sdword i;
//    vector groupPos;
    //real32 temp;

    //groupPos.x=0.0f;
    //groupPos.y=0.0f;
    //groupPos.z=0.0f;
    //for(i=0;i<selection->numShips;i++)
    //{
    //    vecAddTo(groupPos,selection->ShipPtr[i]->obj.posinfo.position);
    //}
    //vecDivideByScalar(groupPos,((float)selection->numShips),temp);
    //grouppos is the average posiiton of the group
    vecSub(*newTo,selection->ShipPtr[0]->obj.posinfo.position,*from);
    //add the offset to the desired desitination
    vecAddTo(*newTo,*to);
}

/*-----------------------------------------------------------------------------
    Name        : clMpHyperspace
    Description : Command to hyperspace ships in multiplayer game ships
    Inputs      : selectcom, from, to
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clMpHyperspace(CommandLayer *comlayer,SelectCommand *selectcom,vector from,vector to)
{
    SelectCommand *copycom;
    udword sizeofcopycom;
    Node *curnode;
    Node *endnode;
    Node *nextnode;
    CommandToDo *command;
    sdword cost;
    vector distsqr;
    real32 tempdist,distance;
    sdword i;
    vector newTo;
    sdword playerindex;

    makeSelectionHyperspaceCapable(selectcom);

    //should probably leave in this check...you never know
    if (selectcom->numShips < 1)
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("No ships to hyperspace");
#endif

        return;
    }

    // CAT (M2-R9 oracle-rung-physics): the exe's x87 shape (clMpHyperspace 0x004474bf .. 0x0044750a): the differences
    //      from - to stay doubles, (dz*dz + dy*dy) + dx*dx goes to fsqrt as a double, the distance is stored, and
    //      hyperspaceCost's double is truncated (_ftol)
    {
        double dx = (double)from.x - to.x;
        double dy = (double)from.y - to.y;
        double dz = (double)from.z - to.z;

        distance = (real32)catFsqrtD((dz * dz + dy * dy) + dx * dx);
        (void)distsqr;
        (void)tempdist;
    }
    cost = (sdword)hyperspaceCost(distance,selectcom);

    playerindex = selectcom->ShipPtr[0]->playerowner->playerIndex;
    if ( (playerindex >= tpGameCreated.numPlayers) ||    // computer player RU's arene't deterministic, always let it spend RUs (computer RUs can go negative)
         (universe.players[playerindex].resourceUnits >= cost) )
    {
        universe.players[playerindex].resourceUnits -= cost;
        universe.gameStats.playerStats[playerindex].totalResourceUnitsSpent += cost;
    }
    else
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("Can't afford HS - cancelling");
#endif
        return;
    }
#ifdef DEBUG_TACTICS
    if(tacticsOn)
#endif
    {
        tacticsReportMove(comlayer,selectcom);
    }

    //remove ships from being targeted...
    for(i=0;i<selectcom->numShips;i++)
    {
        RemoveShipFromBeingTargeted(comlayer,selectcom->ShipPtr[i],REMOVE_PROTECT|REMOVE_HYPERSPACING,NULL);   // CAT: fourth argument 0 (clMpHyperspace 0x004475a3)
    }
    sizeofcopycom = sizeofSelectCommand(selectcom->numShips);
    copycom = memAlloc(sizeofcopycom,"cc3(copycom)", Pyrophoric);
    memcpy(copycom,selectcom,sizeofcopycom);

    // Now see if the selected ships are already in the command layer, and if so, individually order these groups
    // to hyperspace (to preserve formations)

    curnode = comlayer->todolist.head;
    endnode = comlayer->todolist.tail;

    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);

        nextnode = curnode->next;

        if (TheseShipsAreInSelection(command->selection,copycom))
        {

            MakeShipsNotIncludeTheseShips(copycom,command->selection);
            getNewHyperspaceingDestination(&newTo,&from,&to,command->selection);
            ChangeOrderToHyperSpace(command,from,newTo);
        }

        if (copycom->numShips == 0)
        {
            break;
        }

        if (curnode == endnode)
        {
            break;
        }
        curnode = nextnode;
    }

    // tell rest of ships left to move too.
    if (copycom->numShips != 0)
    {
        getNewHyperspaceingDestination(&newTo,&from,&to,copycom);
        clMpHyperspaceThese(comlayer,copycom,from,newTo);
    }

    memFree(copycom);
}


/*-----------------------------------------------------------------------------
    Name        : AttackClosestOpponent
    Description : finds and returns nearest opponent to ship to attack
    Inputs      : ship, todo
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
SpaceObjRotImpTarg *AttackClosestOpponent(Ship *ship,AttackCommand *attack)
{
    sdword numTargetsToAttack = attack->numTargets;
    sdword i;
    vector dist;
    real32 distsqr;
    real32 mindist;
    sdword attacki;
    SpaceObjRotImpTarg *attackme;

    if (numTargetsToAttack <= 0)
    {
        return NULL;
    }

    mindist = REALlyBig;
    attacki = -1;
    for (i=0;i<numTargetsToAttack;i++)
    {
        attackme = attack->TargetPtr[i];

        vecSub(dist,ship->obj.posinfo.position,attackme->obj.posinfo.position);
        distsqr = vecMagnitudeSquared(dist);

        if (distsqr < mindist)
        {
            mindist = distsqr;
            attacki = i;
        }
    }

    dbgAssertOrIgnore(attacki != -1);

    return attack->TargetPtr[attacki];
}

/*-----------------------------------------------------------------------------
    Name        : RemoveShipFromAttacking
    Description : Removes ship from attacking
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: RemoveShipFromAttacking 0x004477c0 (map: IDENTICAL): a resource-class ship (CLASS_Resource, not HW1's
//      ResourceCollector type) leaves its resourcer attack; catAttackStarted (+0x616) is cleared with shipisattacking;
//      the formation is the ship's command when that has the formation flag (Cataclysm has no Ship.formationcommand)
void RemoveShipFromAttacking(Ship *ship)
{
    CommandToDo *formationcommand;

    if(ship->staticinfo->shipclass == CLASS_Resource)
    {
        removeResourcerFromAttacking(ship);
    }
    if (ship->attackvars.multipleAttackTargets != NULL)
    {
        memFree(ship->attackvars.multipleAttackTargets);
        ship->attackvars.multipleAttackTargets = NULL;
    }

    ship->attackvars.attacktarget = NULL;
    ship->attackvars.myLeaderIs = NULL;
    ship->attackvars.myWingmanIs = NULL;
    ship->shipisattacking = FALSE;
    ship->catAttackStarted = FALSE;
    bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze|SPECIAL_BrokenFormation|SPECIAL_AttackFromAbove);
    if (ship->flightman != FLIGHTMAN_NULL)
    {
        flightmanClose(ship);
    }
    formationcommand = ship->command;
    if ((formationcommand != NULL) && (formationcommand->ordertype.attributes & COMMAND_MASK_FORMATION))
    {
        if(formationcommand->formation.formationLocked)
            unlockFormation(formationcommand);

        formationcommand->formation.flipselectionfixed = FALSE;
        formationcommand->formation.needFix = FALSE;
    }
    //dbgMessagef("Ship %i is no longer attacking", ship->shipID.shipNumber);
}

/*-----------------------------------------------------------------------------
    Name        : AttackCleanup
    Description : cleans up anything from a attack command
    Inputs      : attacktodo
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void AttackCleanup(struct CommandToDo *attacktodo)
{
    sdword i;
    SelectCommand *selection = attacktodo->selection;
    sdword numShips = selection->numShips;
    bool32 clearmove = TRUE;

    if (bitTest(attacktodo->ordertype.attributes,COMMAND_MASK_ATTACKING_AND_MOVING))
    {
        bitClear(attacktodo->ordertype.attributes,COMMAND_MASK_ATTACKING_AND_MOVING);
    }

    if(attacktodo->ordertype.order == COMMAND_MOVE)
    {
        clearmove = FALSE;
    }


    if(attacktodo->ordertype.attributes & COMMAND_MASK_FORMATION)
    {
        // CAT: AttackCleanup 0x00447880 (map: IDENTICAL): the force-field sphere too, and only with more than one ship
        {
            if (((attacktodo->formation.formationtype == SPHERE_FORMATION) || (attacktodo->formation.formationtype == FORCEFIELD_SPHERE_FORMATION)) &&
                (attacktodo->selection->numShips > 1))
            {
                attacktodo->formation.enders = FALSE;
                //attacktodo->formation.endership = NULL;
                formationContentHasChanged(attacktodo);
            }
        }
    }

    for (i=0;i<numShips;i++)
    {
        RemoveShipFromAttacking(selection->ShipPtr[i]);
        // don't clear unless clearing a move otherwise some moves will get erroneously cancelled
        if(clearmove)
        {
            vecZeroVector(selection->ShipPtr[i]->moveTo);
            vecZeroVector(selection->ShipPtr[i]->moveFrom);
        }
        bitClear(selection->ShipPtr[i]->shipOrDerelict.specialFlags,SPECIAL_BrokenFormation);


    }

    //attack has been cancelled, lets cue up a gun closing animation
    //need a delay item here...
    madLinkInCloseGuns(attacktodo,1);
}

/*-----------------------------------------------------------------------------
    Name        : RemoveShipFromFormation
    Description : cleans up ship stuff that was in a formation
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: RemoveShipFromFormation 0x00447950 (REWRITTEN): Cataclysm has no Ship.formationcommand; a ship leaving a
//      formation loses the formation disable reason (5) and a Sentinel is unclamped from the force field
void RemoveShipFromFormation(Ship *ship)
{
    if (ship->shipOrDerelict.catDisableReason[5] == 1)
    {
        cat_shipSetDisableReason(ship,FALSE,5);
    }
    if ((ship->shiptype == sSentinel) || (ship->shiptype == bSentinel))
    {
        unClampObj((SpaceObjRotImpTargGuidance *)ship);
    }
}

/*-----------------------------------------------------------------------------
    Name        : FormationCleanup
    Description : cleans up anything from a formation
    Inputs      : formationtodo
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void FormationCleanup(struct CommandToDo *formationtodo)
{
    sdword i;
    SelectCommand *selection = formationtodo->selection;
    sdword numShips = selection->numShips;

    for (i=0;i<numShips;i++)
    {
        RemoveShipFromFormation(selection->ShipPtr[i]);
    }

    if (formationtodo->formation.formationSpecificInfo)
    {
        memFree(formationtodo->formation.formationSpecificInfo);
        formationtodo->formation.formationSpecificInfo = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : RemoveShipFromMoving
    Description : removes ship from moving
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void RemoveShipFromMoving(Ship *ship)
{
    vecZeroVector(ship->moveTo);
    vecZeroVector(ship->moveFrom);
}

/*-----------------------------------------------------------------------------
    Name        : MoveCleanup
    Description : cleans up anything from a move command
    Inputs      : movetodo
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MoveCleanup(struct CommandToDo *movetodo)
{
    sdword i;
    SelectCommand *selection = movetodo->selection;
    sdword numShips = selection->numShips;

    for (i=0;i<numShips;i++)
    {
        RemoveShipFromMoving(selection->ShipPtr[i]);
    }
}

// CAT: the hyperspace disable reason 6 is cleared instead of setting SOF_Selectable (FreeCommandToDoContents 0x004440f0
//      case 10, FreeLastOrder 0x00444490, RemoveShipsFromDoingStuff 0x00444ee5)
void removeShipFromMpHyperspaceing(Ship *ship)
{
    vecZeroVector(ship->moveTo);
    vecZeroVector(ship->moveFrom);
    cat_shipSetDisableReason(ship,FALSE,6);
}

/*-----------------------------------------------------------------------------
    Name        : MpHyperspaceCleanup
    Description : cleans up anything from a hyperspaceing move command
    Inputs      : movetodo
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MpHyperspaceCleanup(struct CommandToDo *movetodo)
{
    sdword i;
    SelectCommand *selection = movetodo->selection;
    sdword numShips = selection->numShips;

    for (i=0;i<numShips;i++)
    {
        removeShipFromMpHyperspaceing(selection->ShipPtr[i]);
    }
}

/*-----------------------------------------------------------------------------
    Name        : RemoveAttackTargetFromExtraAttackInfo
    Description : removes shiptoremove from being targeted for attack. Whatever ship was attacking it
                  will choose a new target.
    Inputs      : shiptoremove, extraattackinfo
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void RemoveAttackTargetFromExtraAttackInfo(SpaceObjRotImpTarg *targettoremove,CommandToDo *todo)
{
    sdword i;
    SelectCommand *selection = todo->selection;
    sdword numShips = selection->numShips;
    Ship *ship;
    AttackTargets *multipleAttackTargets;

    dbgAssertOrIgnore(numShips > 0);

    for (i=0;i<numShips;i++)
    {
        ship = selection->ShipPtr[i];

        if (ship->attackvars.attacktarget == targettoremove)
        {
            ship->attackvars.attacktarget = NULL;
            ship->attackvars.attackevents |= ATTACKEVENT_ENEMYTARGET_DIED;
        }

        if ((multipleAttackTargets = ship->attackvars.multipleAttackTargets) != NULL)
        {
            sdword j;
            sdword numAttackTargets = multipleAttackTargets->numAttackTargets;

            for (j=0;j<numAttackTargets;j++)
            {
                if (multipleAttackTargets->TargetPtr[j] == targettoremove)
                {
                    multipleAttackTargets->TargetPtr[j] = NULL;
                }
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : RemoveShipReferencesFromExtraAttackInfo
    Description : removes shiptoremove from being reference by extraattackinfo.  For example, if a wingman ship
                  was removed, the reference to the wingman would be removed.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void RemoveShipReferencesFromExtraAttackInfo(Ship *shiptoremove,CommandToDo *todo)
{
    SelectCommand *selection = todo->selection;
    sdword numShips = selection->numShips;
    SpecificAttackVars *attackvars;
    sdword i;
    Ship *ship;

    dbgAssertOrIgnore(shiptoremove);

    for (i=0;i<numShips;i++)
    {
        ship = selection->ShipPtr[i];
        attackvars = &ship->attackvars;

        if (ship == shiptoremove)
        {
            continue;       // don't process one that is going to be removed
        }

        if (attackvars->myLeaderIs == shiptoremove)
        {
            // this ship's leader has fallen, so we should not be his wingman anymore.
            attackvars->myLeaderIs = NULL;
            dbgAssertOrIgnore(attackvars->myWingmanIs == NULL);

            attackvars->attackevents |= ATTACKEVENT_LEADER_DIED;
        }

        if (attackvars->myWingmanIs == shiptoremove)
        {
            // lost wingman
            dbgAssertOrIgnore(attackvars->myLeaderIs == NULL);
            attackvars->myWingmanIs = NULL;

            // we lost our wingman
            attackvars->attackevents |= ATTACKEVENT_WINGMAN_DIED;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : allocateMultipleAttackTargets
    Description : allocates a AttackTargets structure based on the number of guns on ship
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
AttackTargets *allocateMultipleAttackTargets(Ship *ship)
{
    sdword numGuns = ship->gunInfo->numGuns;
    AttackTargets *attacktargets;
    sdword sizeofAttackTargets;

    dbgAssertOrIgnore(numGuns > 0);
    sizeofAttackTargets = sizeofAttackTargets(numGuns);
    attacktargets = memAlloc(sizeofAttackTargets,"AttackTargets",0);
    memset(attacktargets,0,sizeofAttackTargets);

    attacktargets->numAttackTargets = numGuns;

    return attacktargets;
}

typedef struct
{
    vector targetInShipCoordSys;
    real32 pickedMe;
} PickMultipleTargetsInfo;

/*-----------------------------------------------------------------------------
    Name        : pickMultipleAttackTargets
    Description : picks best attack target for each gun on a ship
    Inputs      : ship, attack
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void pickMultipleAttackTargets(Ship *ship,AttackCommand *attack)
{
    sdword numShipsToAttack = attack->numTargets;
    AttackTargets *attacktargets = ship->attackvars.multipleAttackTargets;
    sdword numAttackTargets = attacktargets->numAttackTargets;
    PickMultipleTargetsInfo *multipleTargetsInfo;
    PickMultipleTargetsInfo *thisTargetInfo;
    SpaceObjRotImpTarg *target;
    SpaceObjRotImpTarg *target1 = NULL;
    SpaceObjRotImpTarg *target2 = NULL;
    sdword i;
    sdword j;
    GunInfo *gunInfo = ship->gunInfo;
    Gun *gun;
    GunStatic *gunstatic;
    sdword bestIndex;
    real32 bestDist;
    vector targetInWorldCoordSys;
    vector targetInShipCoordSys;
    real32 dist;
    real32 range;
    real32 temp;
    real32 dotprod,gunRange;

    dbgAssertOrIgnore(numShipsToAttack > 0);
    dbgAssertOrIgnore(numAttackTargets > 0);
    dbgAssertOrIgnore(numAttackTargets == gunInfo->numGuns);

    multipleTargetsInfo = memAlloc(sizeof(PickMultipleTargetsInfo)*numShipsToAttack,"mTI(multTargInfo)",Pyrophoric);

    for (i=0,thisTargetInfo=multipleTargetsInfo;i<numShipsToAttack;i++,thisTargetInfo++)
    {
        target = attack->TargetPtr[i];

        aishipGetTrajectory(ship,target,&targetInWorldCoordSys);
        matMultiplyVecByMat(&thisTargetInfo->targetInShipCoordSys,&targetInWorldCoordSys,&ship->rotatable.rotinfo.coordsys);
        thisTargetInfo->pickedMe = 1.0f;
    }
    if (ship->staticinfo->shipclass == CLASS_Frigate)
    {
        target = ship->attackvars.attacktarget;
        if (target->header.objtype == OBJ_ShipType)
        {
           if (((Ship *)target)->staticinfo->shipclass == CLASS_Corvette)
           {
               target1 = target;
               for (i=0;i<numShipsToAttack;i++)
               {
                   if (attack->TargetPtr[i] != target1)
                   {
                       target2 = attack->TargetPtr[i];
                       break;
                   }
               }
           }
        }
    }

    for (i=0;i<numAttackTargets;i++)
    {
        gun = &gunInfo->guns[i];
        gunstatic = gun->gunstatic;

        gunRange = gunstatic->bulletrange;      // CAT: no tactics range bonus (pickMultipleAttackTargets 0x00447b4c)
        bestDist = REALlyBig;
        bestIndex = -1;

        for (j=0,thisTargetInfo=multipleTargetsInfo;j<numShipsToAttack;j++,thisTargetInfo++)
        {
            target = attack->TargetPtr[j];
            if (target1)
            {
                // concentrate firepower: skip this loop for other ships than target1, target2
                if ((target != target1) && (target != target2))
                {
                    continue;
                }
            }

            vecSub(targetInShipCoordSys,thisTargetInfo->targetInShipCoordSys,gunstatic->position);

            dist = fsqrt(vecMagnitudeSquared(targetInShipCoordSys));

            range = RangeToTargetGivenDist(ship,target,dist);

            if (range < gunRange)
            {
                vecDivideByScalar(targetInShipCoordSys,dist,temp);

                dotprod = vecDotProduct(targetInShipCoordSys,gunstatic->gunnormal);

                dist *= (2.0f - dotprod);
                dist *= thisTargetInfo->pickedMe;   // treat distance as if it were bigger for targets already picked
                if (dist < bestDist)
                {
                    bestDist = dist;
                    bestIndex = j;
                }
            }
        }

        if (bestIndex != -1)
        {
            attacktargets->TargetPtr[i] = attack->TargetPtr[bestIndex];
            multipleTargetsInfo[bestIndex].pickedMe *= 1.2f;
        }
        else
        {
            attacktargets->TargetPtr[i] = NULL;
        }
    }

    memFree(multipleTargetsInfo);
}

#define WINGMANSHIP_USED       1
#define WINGMANSHIP_IGNORE     2

typedef struct
{
    sdword numShips;
    sdword flags[1];
} FindWingmenInfo;

#define sizeofFindWingmenInfo(n) (sizeof(FindWingmenInfo) + (((n)-1)*sizeof(sdword)))

/*-----------------------------------------------------------------------------
    Name        : BreakupIntoLeaderWingman
    Description : breaks up the selection of ships into leader/wingman pairs
    Inputs      : selection
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void BreakupIntoLeaderWingman(SelectCommand *selection)
{
    sdword numShips = selection->numShips;
    udword sizeofFindwingmen;
    FindWingmenInfo *findwingmen;
    sdword *flags;
    vector dist;
    real32 distsqr;
    Ship *compareto;
    Ship *wingman;
    sdword wingmanj;
    sdword i,j;
    Ship *leader;
    real32 mindist;
    Ship *ship;

    sizeofFindwingmen = sizeofFindWingmenInfo(numShips);
    findwingmen = memAlloc(sizeofFindwingmen,"findwingmen",0);

    findwingmen->numShips = numShips;
    for (i=0,flags=&findwingmen->flags[0];i<numShips;i++,flags++)
    {
        ship = selection->ShipPtr[i];

        if (isShipInterceptor(ship) &&     // CAT: no HW1 DefenseFighter (M2-R4 ships-b)
            (ship->shiprace != P1) && (ship->shiprace != P2) &&
            ((gamerand() & 255) <= FIGHTER_PAIRUP_PROB)
           )
        {
            *flags = 0;
        }
        else
        {
            *flags = WINGMANSHIP_IGNORE;
        }
    }

    for (i=0;i<numShips;i++)
    {
        if (findwingmen->flags[i])
        {
            continue;
        }

        leader = selection->ShipPtr[i];

        // CAT: no HW1 DefenseFighter leader swap (M2-R4 ships-b)

        mindist = REALlyBig;
        wingmanj = 0;
        for (j=i+1;j<numShips;j++)
        {
            if (findwingmen->flags[j])
            {
                continue;
            }

            compareto = selection->ShipPtr[j];

            // CAT: no HW1 DefenseFighter autoselect (M2-R4 ships-b)

            vecSub(dist,leader->obj.posinfo.position,compareto->obj.posinfo.position);
            distsqr = vecMagnitudeSquared(dist);

            if (distsqr >= WINGMAN_JOIN_MAX_DIST)
            {
                continue;
            }

            if (distsqr < mindist)
            {
                mindist = distsqr;
                wingmanj = j;
            }
        }

        if (wingmanj != 0)
        {
            // found a wingman
            wingman = selection->ShipPtr[wingmanj];
            findwingmen->flags[wingmanj] |= WINGMANSHIP_USED;

            leader->attackvars.myLeaderIs = NULL;
            leader->attackvars.myWingmanIs = wingman;
            wingman->attackvars.myLeaderIs = leader;
            wingman->attackvars.myWingmanIs = NULL;
        }
    }

    memFree(findwingmen);
}

typedef struct
{
    sdword numFlaggedShips;
    sdword flags[1];
} AssignTargetsInfo;

#define sizeofAssignTargetsInfo(n) (sizeof(AssignTargetsInfo) + (((n)-1)*sizeof(sdword)))

/*-----------------------------------------------------------------------------
    Name        : InitExtraAttackInfo
    Description : intializes the extra attack information in the ships based on attackers (selection)
                  and ships getting attacked (attack)
    Inputs      : selection, attack
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void InitExtraAttackInfo(SelectCommand *selection,AttackCommand *attack,bool32 findwingmen)
{
    sdword numShips = selection->numShips;
    sdword numTargetsToAttack = attack->numTargets;
    sdword i,j,k;
    SpecificAttackVars *attackvars;
    sdword sizeofAssigntargets;
    AssignTargetsInfo *assigntargets;
    sdword closestshipj;
    vector dist;
    real32 distsqr;
    real32 mindist;
    SpaceObjRotImpTarg *target;
    Ship *ship;
    sdword *flags;
    vector avgposSelection = { 0.0f, 0.0f, 0.0f };
    vector avgposTargets = { 0.0f, 0.0f, 0.0f };
    vector diff;
    real32 temp;
    Ship *leader = selection->ShipPtr[0];
    bool32 focusfire = FALSE;

    dbgAssertOrIgnore(numShips > 0);
    dbgAssertOrIgnore(numTargetsToAttack > 0);

    if (leader->tacticstype == Aggressive)
    {
        focusfire = TRUE;
    }

    for (i=0;i<numShips;i++)
    {
        ship = selection->ShipPtr[i];
        //dbgAssertOrIgnore(ship->attackvars.multipleAttackTargets == NULL);
        ship->attackvars.multipleAttackTargets = NULL;
        ship->attackvars.attacktarget = NULL;
        ship->attackvars.myLeaderIs = NULL;
        ship->attackvars.myWingmanIs = NULL;
        ship->attackvars.flightmansLeft = 0;
        ship->attackvars.attacksituation = 0;
        ship->attackvars.attackevents = 0;
        bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze|SPECIAL_BrokenFormation|SPECIAL_AttackFromAbove);

        ship->tacticsNeedToFlightMan = FALSE;
        ship->tacticsTalk = 0;

        vecAddTo(avgposSelection,ship->obj.posinfo.position);
    }

    for (i=0;i<numTargetsToAttack;i++)
    {
        target = attack->TargetPtr[i];
        vecAddTo(avgposTargets,target->obj.posinfo.position);
    }

    vecDivideByScalar(avgposSelection,((real32)numShips),temp);
    vecDivideByScalar(avgposTargets,((real32)numTargetsToAttack),temp);
    vecSub(diff,avgposSelection,avgposTargets);
    diff.z = ABS(diff.z);

    if ((diff.z >= ATTACKING_FROM_ABOVE_MIN_DIST) &&
        (diff.z >= (fsqrt(diff.x*diff.x + diff.y*diff.y)*ATTACKING_FROM_ABOVE_MIN_RATIO)))
    {
        for (i=0;i<numShips;i++)
        {
            ship = selection->ShipPtr[i];
            bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_AttackFromAbove);
        }
    }

    // break fighters up into leader/wingman.
    if (findwingmen)
    {
        BreakupIntoLeaderWingman(selection);
    }

    if (numTargetsToAttack == 1)
    {
        for (i=0;i<numShips;i++)
        {
            attackvars = &selection->ShipPtr[i]->attackvars;
            if (!IAmAWingman(attackvars))
            {
                attackvars->attacktarget = attack->TargetPtr[0];
            }
        }
    }
    else
    {
        sizeofAssigntargets = sizeofAssignTargetsInfo(numTargetsToAttack);
        assigntargets = memAlloc(sizeofAssigntargets,"assigntargets",Pyrophoric);
        memset(assigntargets,0,sizeofAssigntargets);

        for (i=0;i<numShips;i++)
        {
            ship = selection->ShipPtr[i];
            attackvars = &ship->attackvars;
            if (IAmAWingman(attackvars))
            {
                continue;
            }

            closestshipj = -1;
            mindist = REALlyBig;
            for (j=0,flags=&assigntargets->flags[0];j<numTargetsToAttack;j++,flags++)
            {
                if (*flags != 0)
                {
                    continue;
                }

                target = attack->TargetPtr[j];

                vecSub(dist,ship->obj.posinfo.position,target->obj.posinfo.position);
                distsqr = vecMagnitudeSquared(dist);

                if (distsqr < mindist)
                {
                    mindist = distsqr;
                    closestshipj = j;
                }
            }

            dbgAssertOrIgnore(closestshipj != -1);

            dbgAssertOrIgnore(assigntargets->flags[closestshipj] == 0);
            assigntargets->flags[closestshipj] = 1;
            assigntargets->numFlaggedShips++;
            dbgAssertOrIgnore(assigntargets->numFlaggedShips <= numTargetsToAttack);
            if (assigntargets->numFlaggedShips == numTargetsToAttack)
            {
                for (k=0,flags=&assigntargets->flags[0];k<numTargetsToAttack;k++,flags++)
                {
                    *flags = 0;
                }
                assigntargets->numFlaggedShips = 0;
            }

            attackvars->attacktarget = attack->TargetPtr[closestshipj];

            if (focusfire && (i > 0) && leader->attackvars.attacktarget)
            {
                attackvars->attacktarget = leader->attackvars.attacktarget;     // override for focus fire
            }

            if (((ShipStaticInfo *)(ship->staticinfo))->canTargetMultipleTargets)
            {
                dbgAssertOrIgnore(attackvars->multipleAttackTargets == NULL);
                attackvars->multipleAttackTargets = allocateMultipleAttackTargets(ship);
                pickMultipleAttackTargets(ship,attack);
            }

        }

        memFree(assigntargets);
    }
}

/*-----------------------------------------------------------------------------
    Name        : ChangeOrderToAttack
    Description : changes order to attack
    Inputs      : alreadycommand, attackcom, catReason
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: ChangeOrderToAttack 0x00448460 takes a third argument, a reason code (clAttackThese 1, clAttack 2,
//      clPostProcess 3, clFormation 4) the exe never reads. The new targets get the modules attached to them
//      (cat_clAddAttachedModulesToTargets 0x004482f0); PrepareShipsForCommand and InitShipsForAI[DontResetAttack] are
//      inlined in the exe with the same effect.
void ChangeOrderToAttack(CommandToDo *alreadycommand,AttackCommand *attackcom,sdword catReason)
{
    catAttackSelfCheck("ChangeOrderToAttack", alreadycommand->selection, attackcom);   // HWI-TEST (M3-R4 nan-positions)
    AttackCommand *attack;
    udword sizeofattack;
    SelectCommand *selection = alreadycommand->selection;
    bool32 dontresetattack = FALSE;

    (void)catReason;

    if (alreadycommand->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        ClearPassiveAttacking(alreadycommand);
    }

    if (alreadycommand->ordertype.order == COMMAND_MP_HYPERSPACING)
    {
        return;     // don't want to attack under any circumstances if hyperspacing!
    }

    if (alreadycommand->ordertype.order == COMMAND_ATTACK)
    {
        if (SelectionsAreTotallyEquivalent((SelectCommand *)alreadycommand->attack,(SelectCommand *)attackcom))
        {
            if(alreadycommand->ordertype.attributes & COMMAND_MASK_ATTACKING_AND_MOVING)
            {
                bitClear(alreadycommand->ordertype.attributes,COMMAND_MASK_ATTACKING_AND_MOVING);
            }
            return;
        }
        dontresetattack = TRUE;
#if DEBUG_COMMAND_LAYER
        dbgMessage("Received Order to change target of attack");
#endif
    }
    else
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("Changing command to attack");
#endif
    }

    FreeLastOrder(alreadycommand);

    sizeofattack = sizeofAttackCommand(attackcom->numTargets);
    attack = memAlloc(sizeofattack,"ToDoAttack",0);
    memcpy(attack,attackcom,sizeofattack);
    attack = cat_clAddAttachedModulesToTargets(selection,attack);     // CAT: ChangeOrderToAttack 0x0044850c

    InitExtraAttackInfo(selection,attack,TRUE);

    alreadycommand->ordertype.order = COMMAND_ATTACK;
    alreadycommand->attack = attack;

    if (alreadycommand->ordertype.attributes & COMMAND_MASK_FORMATION)
    {
        alreadycommand->formation.doneInitialAttack = FALSE;
    }

    PrepareShipsForCommand(alreadycommand,TRUE);

    if (dontresetattack)
    {
        InitShipsForAIDontResetAttack(alreadycommand->selection,TRUE);
    }
    else
    {
        InitShipsForAI(alreadycommand->selection,TRUE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_clTargetIsLeechOnShip
    Description : TRUE if target is a Leech clamped to ship
----------------------------------------------------------------------------*/
// CAT: inlined in clAttackThese 0x00448683: a ship cannot attack a Leech that is clamped to it
static bool32 cat_clTargetIsLeechOnShip(SpaceObjRotImpTarg *target,Ship *ship)
{
    Ship *leech;

    if (target->header.objtype != OBJ_ShipType)
    {
        return FALSE;
    }
    leech = (Ship *)target;
    return (leech->shiptype == sLeech) && (leech->guidance.clampInfo != NULL) &&
           (leech->guidance.clampInfo->host == (SpaceObjRotImpTarg *)ship);
}

/*-----------------------------------------------------------------------------
    Name        : clAttackThese
    Description : Command to attack
    Inputs      : selectcom, attackcom
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: clAttackThese 0x00448620: the attackers lose every ship a targeted Leech is clamped to (swap with the last,
//      selectcom is changed in place), the target check comes before the empty-selection check, the new targets get
//      the attached modules (cat_clAddAttachedModulesToTargets 0x004482f0), RemoveShipsFromDoingStuff takes reason 6
//      and ChangeOrderToAttack reason 1
CommandToDo *clAttackThese(CommandLayer *comlayer,SelectCommand *selectcom,AttackCommand *attackcom)
{
    CommandToDo *newcommand;
    CommandToDo *alreadycommand;
    SelectCommand *selection;
    AttackCommand *attack;
    udword sizeofselection;
    udword sizeofattack;
    sdword i;
    sdword j;
    bool32 leechOnShip;

    // for safety
    if (!AreAllShipsAttackCapable(selectcom))
    {
        MakeShipsAttackCapable(selectcom,selectcom, FALSE);   // CAT: skip-disabled argument (clAttack 0x00448950 and the other game-code callers pass 0)
        dbgMessage("WARNING: ships not capable of attacking told to attack");
    }

    // CAT: clAttackThese 0x00448657: no ship attacks a Leech clamped to it
    for (i = 0; i < selectcom->numShips; )
    {
        leechOnShip = FALSE;
        for (j = 0; j < attackcom->numTargets; j++)
        {
            if (cat_clTargetIsLeechOnShip(attackcom->TargetPtr[j],selectcom->ShipPtr[i]))
            {
                leechOnShip = TRUE;
            }
        }
        if (leechOnShip)
        {
            selectcom->numShips--;
            selectcom->ShipPtr[i] = selectcom->ShipPtr[selectcom->numShips];
        }
        else
        {
            i++;
        }
    }

    if (attackcom->numTargets <= 0)
    {
        dbgMessage("WARNING: NO SHIPS TO ATTACK.");
        return NULL;
    }

    if (selectcom->numShips == 0)
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("No Ships As Attackers");
#endif
        return NULL;
    }

    if ((alreadycommand = IsSelectionAlreadyDoingSomething(comlayer,selectcom)) != NULL)
    {
        if (alreadycommand->ordertype.attributes & COMMAND_MASK_PROTECTING)
        {
            ClearProtecting(alreadycommand);
        }

        ChangeOrderToAttack(alreadycommand,attackcom,1);

        //link in animation to open guns here:
        //madLinkInOpenGuns(alreadycommand,1);

        return alreadycommand;
    }

#if DEBUG_COMMAND_LAYER
    dbgMessage("Received Order to attack");
#endif

    newcommand = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
    InitCommandToDo(newcommand);

    sizeofselection = sizeofSelectCommand(selectcom->numShips);
    selection = memAlloc(sizeofselection,"ToDoSelection",NonVolatile);
    memcpy(selection,selectcom,sizeofselection);

    sizeofattack = sizeofAttackCommand(attackcom->numTargets);
    attack = memAlloc(sizeofattack,"ToDoAttack",0);
    memcpy(attack,attackcom,sizeofattack);

    // CAT: the exe removes the ships from their commands after the copies (clAttackThese 0x004487c3)
    RemoveShipsFromDoingStuff(comlayer,selectcom,6);

    attack = cat_clAddAttachedModulesToTargets(selection,attack);     // CAT: clAttackThese 0x004487cf

    InitExtraAttackInfo(selection,attack,TRUE);

    newcommand->selection = selection;
    newcommand->ordertype.order = COMMAND_ATTACK;
    newcommand->ordertype.attributes = 0;
    newcommand->attack = attack;

    InitShipsForAI(selection,TRUE);

    PrepareShipsForCommand(newcommand,TRUE);

    listAddNode(&comlayer->todolist,&newcommand->todonode,newcommand);

    //link in animation to open guns here:
    //madLinkInOpenGuns(newcommand,1);


    return newcommand;
}

/*-----------------------------------------------------------------------------
    Name        : cat_clAttackerIsInWorldCB
    Description : TRUE if ship has a blob, is in an object list and is not hidden
----------------------------------------------------------------------------*/
// CAT: new (the unnamed constraint 0x00448920 clAttack passes to MakeShipsOnlyFollowConstraints)
static bool32 cat_clAttackerIsInWorldCB(Ship *ship)
{
    return (ship->obj.collMyBlob != NULL) && (ship->header.objlink.structptr != NULL) && !bitTest(ship->header.flags,SOF_Hide);
}

// HWI-TEST (M3-R4 nan-positions): a ship that is in both the selection and the target list of an attack attacks itself
//      (fly-to-point to its own position: a zero vector, NaN heading). Logged where attack commands are made.
#include "UnivUpdate.h"
void catAttackSelfCheck(const char *where, SelectCommand *sel, AttackCommand *att)
{
    sdword i, j;

    if (!catNaNProbeOn || (sel == NULL) || (att == NULL))
    {
        return;
    }
    for (i = 0; i < sel->numShips; i++)
    {
        for (j = 0; j < att->numTargets; j++)
        {
            if ((SpaceObjRotImpTarg *)sel->ShipPtr[i] == att->TargetPtr[j])
            {
                {
                    sdword k, own = 0, other = 0, nonship = 0;
                    for (k = 0; k < att->numTargets; k++)
                    {
                        if (att->TargetPtr[k]->header.objtype != OBJ_ShipType) nonship++;
                        else if (((Ship *)att->TargetPtr[k])->playerowner == sel->ShipPtr[i]->playerowner) own++;
                        else other++;
                    }
                    dbgMessagef("nanprobe-selfattack: tick=%u where=%s ship=%s#%u owner=%d sel=%d targets=%d (own=%d other=%d nonship=%d) targetindex=%d",
                                (unsigned)universe.univUpdateCounter, where, ShipTypeToStr(sel->ShipPtr[i]->shiptype),
                                (unsigned)sel->ShipPtr[i]->shipID.shipNumber, sel->ShipPtr[i]->playerowner ? (int)sel->ShipPtr[i]->playerowner->playerIndex : -1,
                                (int)sel->numShips, (int)att->numTargets, (int)own, (int)other, (int)nonship, (int)j);
                }
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : clAttack
    Description : Command to attack
    Inputs      : selectcom, attackcom
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: clAttack 0x00448950 copies the selection first and drops the ships retreating to dock
//      (cat_tacticsRemoveRetreatingShips 0x00529f70) and those without a blob, out of the object lists or hidden
//      (cat_clAttackerIsInWorldCB); MakeShipsAttackCapable refills the copy from selectcom; an empty copy is not an
//      error. A command interrupted while doing a waypoint move with a path type (loop or patrol) saves the move in
//      catSavedMove, which clProcess resumes when the attack ends (clAttack 0x00448aa3).
void clAttack(CommandLayer *comlayer,SelectCommand *selectcom,AttackCommand *attackcom)
{
    catAttackSelfCheck("clAttack", selectcom, attackcom);   // HWI-TEST (M3-R4 nan-positions)
    SelectCommand *copycom;
    udword sizeofcopycom;
    Node *curnode;
    Node *endnode;
    Node *nextnode;
    CommandToDo *command;

    sizeofcopycom = sizeofSelectCommand(selectcom->numShips);
    copycom = memAlloc(sizeofcopycom,"cc2(copycom)", Pyrophoric);
    memcpy(copycom,selectcom,sizeofcopycom);

    cat_tacticsRemoveRetreatingShips(copycom);
    MakeShipsOnlyFollowConstraints(copycom,cat_clAttackerIsInWorldCB);

    // for safety
    if (!AreAllShipsAttackCapable(copycom))
    {
        MakeShipsAttackCapable(copycom,selectcom, FALSE);   // CAT: skip-disabled argument (clAttack 0x00448950 and the other game-code callers pass 0)
        dbgMessage("WARNING: ships not capable of attacking told to attack");
    }

    if (attackcom->numTargets <= 0)
    {
        dbgMessage("WARNING: NO SHIPS TO ATTACK.");
        memFree(copycom);
        return;
    }
    // CAT (M2-R4 combat): no salCapExtraSpecialOrderCleanUp (HW1 salvage; clAttack has no counterpart call)

    if (copycom->numShips == 0)
    {
        memFree(copycom);
        return;
    }

    // Now see if the selected ships are already in the command layer, and if so, individually order these groups
    // to move (to preserve formations)

    curnode = comlayer->todolist.head;
    endnode = comlayer->todolist.tail;

    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);

        nextnode = curnode->next;

        if (TheseShipsAreInSelection(command->selection,copycom))
        {
            MakeShipsNotIncludeTheseShips(copycom,command->selection);

            if (command->ordertype.attributes & COMMAND_MASK_PROTECTING)
            {
                ClearProtecting(command);
            }

            // CAT: clAttack 0x00448aa3: keep a looping or patrol move to resume after the attack
            if ((command->ordertype.order == COMMAND_MOVE) && (command->move.pathType != CAT_MOVEPATH_ONCE))
            {
                command->catSavedMove = command->move;
                command->catSavedMoveValid = TRUE;
            }

            ChangeOrderToAttack(command,attackcom,2);
        }

        if (copycom->numShips == 0)
        {
            break;
        }

        if (curnode == endnode)
        {
            break;
        }
        curnode = nextnode;
    }

    // tell rest of ships left to move too.
    if (copycom->numShips != 0)
    {
        clAttackThese(comlayer,copycom,attackcom);
    }

    memFree(copycom);
}

// CAT: no canChangeOrderToPassiveAttack: its HW1 callers (tacticsDelegate*, univCheckShipState) are gone and
//      clPassiveAttack 0x00448ce0 inlines its own test (cat_clCanChangeOrderToPassiveAttack below)

/*-----------------------------------------------------------------------------
    Name        : ChangeOrderToPassiveAttack
    Description : Activates passive attack for command
    Inputs      : alreadycommand, attackcom
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ChangeOrderToPassiveAttack(CommandToDo *alreadycommand,AttackCommand *attackcom)
{
    catAttackSelfCheck("ChangeOrderToPassiveAttack", alreadycommand->selection, attackcom);   // HWI-TEST (M3-R4 nan-positions)
    AttackCommand *attack;
    udword sizeofattack;
    SelectCommand *selection = alreadycommand->selection;
    sdword count,i;

    count=0;
    for(i=0;i<alreadycommand->selection->numShips;i++)
    {
        if(alreadycommand->selection->ShipPtr[i]->passiveAttackCancelTimer > universe.totaltimeelapsed)
        {
            //count a bad ship
            count++;
        }
    }
    if( (((real32) count)/((real32) alreadycommand->selection->numShips)) > 0.5f)
    {
        //more than 50% in group bad, so don't passive attack
        return;
    }

#if DEBUG_PASSIVE_ATTACK
    dbgMessage("Changing Order to passive attack");
#endif

    // CAT: ChangeOrderToPassiveAttack 0x00448b30 frees the targets it replaces: a passive attack's (and the passive
    //      flag), or else an attack order's (the order stays COMMAND_ATTACK)
    if (alreadycommand->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        AttackCleanup(alreadycommand);
        memFree(alreadycommand->attack);
        bitClear(alreadycommand->ordertype.attributes,COMMAND_MASK_PASSIVE_ATTACKING);
        alreadycommand->attack = NULL;
    }
    else if (alreadycommand->ordertype.order == COMMAND_ATTACK)
    {
        AttackCleanup(alreadycommand);
        if (alreadycommand->attack != NULL)
        {
            memFree(alreadycommand->attack);
            alreadycommand->attack = NULL;
        }
    }

    sizeofattack = sizeofAttackCommand(attackcom->numTargets);
    attack = memAlloc(sizeofattack,"ToDoAttack",0);
    memcpy(attack,attackcom,sizeofattack);

    InitExtraAttackInfo(selection,attack,FALSE);

    alreadycommand->ordertype.attributes |= COMMAND_MASK_PASSIVE_ATTACKING;
    bitClear(alreadycommand->ordertype.attributes,COMMAND_MASK_HOLDING_PATTERN);      // don't do holding pattern if passive attacking
    alreadycommand->attack = attack;

    InitShipsForAI(alreadycommand->selection,FALSE);

    PrepareShipsForCommand(alreadycommand,FALSE);

    //setup formation properly here
    if(alreadycommand->ordertype.order == COMMAND_NULL)
    {
        if(alreadycommand->ordertype.attributes & COMMAND_MASK_FORMATION)
        {
            //NULL command and in formation
            lockFormation(alreadycommand,0);
        }
    }
}

// CAT: clPassiveAttack 0x00448ce0 inlines its own canChangeOrderToPassiveAttack: a holding pattern never, a passive
//      attack on other targets always, a force-field sphere never, and every order but COMMAND_NULL, COMMAND_MOVE,
//      COMMAND_HALT, COMMAND_MILITARY_PARADE and order 13 never (HW1: a move or dock only without strike craft)
static bool32 cat_clCanChangeOrderToPassiveAttack(CommandToDo *alreadycommand,AttackCommand *attack)
{
    if (alreadycommand->ordertype.attributes & COMMAND_MASK_HOLDING_PATTERN)
    {
        return FALSE;
    }

    if (alreadycommand->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        if ((alreadycommand->attack == NULL) || (attack == NULL))
        {
            return FALSE;
        }

        if (!SelectionsAreEquivalent((SelectCommand *)alreadycommand->attack,(SelectCommand *)attack))
        {
            return TRUE;
        }
    }

    if ((alreadycommand->ordertype.attributes & COMMAND_MASK_FORMATION) &&
        (alreadycommand->formation.formationtype == FORCEFIELD_SPHERE_FORMATION))
    {
        return FALSE;
    }

    switch (alreadycommand->ordertype.order)
    {
        case COMMAND_ATTACK:
        case COMMAND_DOCK:
        case COMMAND_LAUNCH_SHIP:
        case COMMAND_COLLECT_RESOURCES:
        case COMMAND_BUILDING_SHIP:
        case COMMAND_SPECIAL:
        case COMMAND_MP_HYPERSPACING:
        case COMMAND_CAT_LINK:
        case COMMAND_CAT_UNLINK:
        case COMMAND_CAT_POINT:
        case COMMAND_CAT_MIMIC:
        case COMMAND_CAT_RESEARCH:
        case COMMAND_CAT_17:
            return FALSE;
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : clPassiveAttack
    Description : Command to passive attack
    Inputs      : selectcom, attackcom
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: clPassiveAttack 0x00448ce0: the attackers lose every ship a targeted Leech is clamped to (as clAttackThese);
//      a changed order says so in battle chatter (the human player's ships, at most every 40 seconds per lead ship);
//      RemoveShipsFromDoingStuff takes reason 7
void clPassiveAttack(CommandLayer *comlayer,SelectCommand *selectcom,AttackCommand *attackcom)
{
    catAttackSelfCheck("clPassiveAttack", selectcom, attackcom);   // HWI-TEST (M3-R4 nan-positions)
    CommandToDo *newcommand;
    CommandToDo *alreadycommand;
    SelectCommand *selection;
    AttackCommand *attack;
    udword sizeofselection;
    udword sizeofattack;
    sdword count,i,j;
    bool32 leechOnShip;
    sdword speechEvent;
    sdword chatter;
    Ship *leader;

    // CAT: clPassiveAttack 0x00448cf5: no ship attacks a Leech clamped to it
    for (i = 0; i < selectcom->numShips; )
    {
        leechOnShip = FALSE;
        for (j = 0; j < attackcom->numTargets; j++)
        {
            if (cat_clTargetIsLeechOnShip(attackcom->TargetPtr[j],selectcom->ShipPtr[i]))
            {
                leechOnShip = TRUE;
            }
        }
        if (leechOnShip)
        {
            selectcom->numShips--;
            selectcom->ShipPtr[i] = selectcom->ShipPtr[selectcom->numShips];
        }
        else
        {
            i++;
        }
    }

    if (selectcom->numShips == 0)
    {
#if DEBUG_PASSIVE_ATTACK
        dbgMessage("No Ships For Passive attack");
#endif
        return;
    }

    if (attackcom->numTargets <= 0)
    {
#if DEBUG_PASSIVE_ATTACK
        dbgMessage("WARNING: NO SHIPS TO PASSIVE ATTACK.");
#endif
        return;
    }

    if ((alreadycommand = IsSelectionAlreadyDoingSomething(comlayer,selectcom)) != NULL)
    {
        if (cat_clCanChangeOrderToPassiveAttack(alreadycommand,attackcom))
        {
            ChangeOrderToPassiveAttack(alreadycommand,attackcom);

            // CAT: clPassiveAttack 0x00448fc8: retaliation chatter (BCE_STAT_Grp_AttackRetaliate, or
            //      BCE_STAT_AssGrp_AttackRetaliate for a hot-key group, through the fleet; then the repeat line)
            leader = selectcom->ShipPtr[0];
            if ((leader->playerowner->playerIndex == universe.curPlayerIndex) &&
                (universe.totaltimeelapsed >= leader->retaliateSpeechTime + 40.0f))
            {
                speechEvent = selAnyHotKeyTest(leader) ? BCE_STAT_AssGrp_AttackRetaliate : BCE_STAT_Grp_AttackRetaliate;
                chatter = battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, speechEvent, SOUND_EVENT_DEFAULT, NULL);
                if (chatter != SOUND_EVENT_DEFAULT)
                {
                    leader->retaliateSpeechTime = universe.totaltimeelapsed;
                    if (battleCanChatterAtThisTime(BCE_STAT_Grp_AttackRetaliate_Repeat, leader))
                    {
                        battleChatterAttempt(chatter, BCE_STAT_Grp_AttackRetaliate_Repeat, leader, leader->shiptype);
                    }
                }
            }
        }
        else
        {
#if DEBUG_PASSIVE_ATTACK
            dbgMessage("WARNING: Couldn't change order to passive attack");
#endif
        }
        return;
    }

#if DEBUG_PASSIVE_ATTACK
    dbgMessage("Received Order to passive attack");
#endif

    count=0;
    for(i=0;i<selectcom->numShips;i++)
    {
        if(selectcom->ShipPtr[i]->passiveAttackCancelTimer > universe.totaltimeelapsed)
        {
            //count a bad ship
            count++;
        }
    }
    if( (((real32) count)/((real32) selectcom->numShips)) > 0.5f)
    {
        //more than 50% in group bad, so don't passive attack
        return;
    }

    newcommand = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
    InitCommandToDo(newcommand);

    sizeofselection = sizeofSelectCommand(selectcom->numShips);
    selection = memAlloc(sizeofselection,"ToDoSelection",NonVolatile);
    memcpy(selection,selectcom,sizeofselection);

    sizeofattack = sizeofAttackCommand(attackcom->numTargets);
    attack = memAlloc(sizeofattack,"ToDoAttack",0);
    memcpy(attack,attackcom,sizeofattack);

    // CAT: the exe removes the ships from their commands after the copies (clPassiveAttack 0x00448e72)
    RemoveShipsFromDoingStuff(comlayer,selectcom,7);

    InitExtraAttackInfo(selection,attack,FALSE);

    newcommand->selection = selection;
    newcommand->ordertype.order = COMMAND_NULL;
    newcommand->ordertype.attributes = COMMAND_MASK_PASSIVE_ATTACKING;
    newcommand->attack = attack;

    InitShipsForAI(selection,FALSE);

    PrepareShipsForCommand(newcommand,FALSE);

    listAddNode(&comlayer->todolist,&newcommand->todonode,newcommand);
}

/*-----------------------------------------------------------------------------
    Name        : ChangeOrderToSpecial
    Description : changes order to special
    Inputs      : alreadycommand, targets
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ChangeOrderToSpecial(CommandToDo *alreadycommand,SpecialCommand *targets)
{
    SpecialCommand *specialtargets;
    udword sizeofspecialtargets;

    if (alreadycommand->ordertype.attributes & COMMAND_MASK_PROTECTING)
    {
        ClearProtecting(alreadycommand);
    }

    if (alreadycommand->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        ClearPassiveAttacking(alreadycommand);
    }

    FreeLastOrder(alreadycommand);

    if (targets != NULL)
    {
        sizeofspecialtargets = sizeofSelectCommand(targets->numTargets);
        specialtargets = memAlloc(sizeofspecialtargets,"ToDoSpecial",0);
        memcpy(specialtargets,targets,sizeofspecialtargets);
        alreadycommand->specialtargets = specialtargets;
    }
    else
    {
        alreadycommand->specialtargets = NULL;
    }

    alreadycommand->ordertype.order = COMMAND_SPECIAL;

    PrepareShipsForCommand(alreadycommand,TRUE);
}

// CAT: the constraint 0x00449090 clSpecial passes: kSalCapCorvette or tSalCapCorvette (0x48/0x59)
bool32 ShipsAreSalCapCorvettes(Ship *ship)
{
    if ((ship->shiptype == kSalCapCorvette) || (ship->shiptype == tSalCapCorvette))
    {
        return TRUE;
    }
    return FALSE;
}

// CAT: no clPlaySpeechEventsForSupportCraft (HW1's COMM_Support line for a carrier, resource collector or support
//      frigate given a special order): clSpecial 0x004492c0 plays no speech

/*-----------------------------------------------------------------------------
    Name        : clSpecial
    Description : Command to do special
    Inputs      : selectcom, targets
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: clSpecial 0x004492c0: an Acolyte or ACV group (sAcolyte, sACV, bAcolyte, bACV leader) given a new special
//      order keeps the formation type its leader's command had (the new command is a formation of that type with
//      force-field level 0); no support-craft speech
void clSpecial(CommandLayer *comlayer,SelectCommand *selectcom,SpecialCommand *targets)
{
    CommandToDo *newcommand;
    CommandToDo *alreadycommand;
    SelectCommand *selection;
    SpecialCommand *specialtargets;
    udword sizeofselection;
    udword sizeofspecialtargets;
    bool32 catKeepFormation = FALSE;
    TypeOfFormation catFormation = (TypeOfFormation)-1;
    Ship *leader;

    if (selectcom->numShips == 0)
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("No Ships for special");
#endif
        return;
    }

    // CAT: clSpecial 0x004492ef
    leader = selectcom->ShipPtr[0];
    switch (leader->shiptype)
    {
        case sAcolyte:
        case sACV:
        case bAcolyte:
        case bACV:
            if ((leader->command != NULL) && (leader->command->ordertype.attributes & COMMAND_MASK_FORMATION))
            {
                catFormation = leader->command->formation.formationtype;
                catKeepFormation = TRUE;
            }
            break;
        default:
            break;
    }

    // CAT (M2-R4 combat): a lone SalCap corvette already salvaging a ship of the new target list keeps its order
    //      (clSpecial 0x004492c0: kSalCapCorvette/tSalCapCorvette, ship->command, the Cataclysm salvager block's
    //      target); HW1's salCapExtraSpecialOrderCleanUp has no counterpart in the exe
    if(selectcom->numShips == 1)
    {
        Ship *salcap = selectcom->ShipPtr[0];

        if (((salcap->shiptype == kSalCapCorvette) || (salcap->shiptype == tSalCapCorvette)) &&
            (salcap->command != NULL) && (salcap->command->ordertype.order == COMMAND_SPECIAL) &&
            (cat_salvageGetTarget(salcap) != NULL) &&
            ShipInSelection((SelectCommand *)targets,cat_salvageGetTarget(salcap)))
        {
            //ship is targetting a ship in the list already...lets ignore the command
            return;
        }
    }

    sizeofselection = sizeofSelectCommand(selectcom->numShips);
    selection = memAlloc(sizeofselection,"ToDoSelection",NonVolatile);
    memcpy(selection,selectcom,sizeofselection);

    if (targets == NULL)
    {
        // this is just a special activate command, so activate it for each ship and return without modifying
        // the command layer as long as the specialActivateIsContinuous flag isn't set.
        sdword numShips = selectcom->numShips;
        sdword i;
        Ship *ship;
        ShipStaticInfo *shipstaticinfo;

        for (i=0;i<numShips;i++)
        {
            ship = selectcom->ShipPtr[i];
            shipstaticinfo = (ShipStaticInfo *)(ship->staticinfo);

            if (!shipstaticinfo->specialActivateIsContinuous)
            {
                clRemoveShipFromSelection(selection,ship);

                if (shipstaticinfo->custshipheader.CustShipSpecialActivate)
                {
                    shipstaticinfo->custshipheader.CustShipSpecialActivate(ship);
                }

            }
        }

        if (selection->numShips == 0)
        {
            // all ships have been activated, so return
            memFree(selection);
            return;
        }
    }

    if (!DoAnyShipsFollowConstraints(selection,ShipsAreSalCapCorvettes))        // Bryce later make salcap corvettes be able to salvage in formation fix later
    {
        if ((alreadycommand = IsSelectionAlreadyDoingSomething(comlayer,selection)) != NULL)
        {
            memFree(selection);
            ChangeOrderToSpecial(alreadycommand,targets);
            return;
        }
    }

#if DEBUG_COMMAND_LAYER
    dbgMessage("Received Order to do special");
#endif

    RemoveShipsFromDoingStuff(comlayer,selection,8);

    newcommand = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
    InitCommandToDo(newcommand);

    if (targets != NULL)
    {
        sizeofspecialtargets = sizeofSelectCommand(targets->numTargets);
        specialtargets = memAlloc(sizeofspecialtargets,"ToDoSpecial",0);
        memcpy(specialtargets,targets,sizeofspecialtargets);
        newcommand->specialtargets = specialtargets;
    }
    else
    {
        newcommand->specialtargets = NULL;
    }

    newcommand->selection = selection;
    newcommand->ordertype.order = COMMAND_SPECIAL;
    newcommand->ordertype.attributes = 0;

    PrepareShipsForCommand(newcommand,TRUE);

    listAddNode(&comlayer->todolist,&newcommand->todonode,newcommand);

    // CAT: clSpecial 0x00449623: the Acolyte/ACV formation is rebuilt around the special order
    if (catKeepFormation)
    {
        newcommand->ordertype.attributes |= COMMAND_MASK_FORMATION;
        newcommand->formation.formationtype = catFormation;
        newcommand->formation.catForceFieldLevel = 0;
        FillInFormationSpecifics(newcommand,catFormation,0,TRUE);
        InitShipsForAI(selection,TRUE);
        PrepareShipsForCommand(newcommand,TRUE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSetTactics
    Description : Sets tactics for a group of ships
    Inputs      : selectcom, tacticstype
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clSetTactics(CommandLayer *comlayer,SelectCommand *selectcom,TacticsType tacticstype)
{
    sdword numShips = selectcom->numShips;
    sdword i;
    Ship *ship;
    TacticsType oldtactics;

#ifdef DEBUG_TACTICS
    if(!tacticsOn) return;  //don't change tactics settings if not enabled
#endif

    dbgAssertOrIgnore(tacticstype >= 0);
    dbgAssertOrIgnore(tacticstype < NUM_TACTICS_TYPES);

    for (i=0;i<numShips;i++)
    {
        ship = selectcom->ShipPtr[i];
        //adjust ships maximum bullet range (don't forget to adjust
        //individual guns ranges on the fly
        /*
        if(ship->staticinfo->shipclass == CLASS_Fighter)
        {
            ship->bulletrage*=tacticsInfo.BulletRangeBonus[CLASS_Fighter][ship->tacticstype];
        }
        else if (ship->staticinfo->shipclass == CLASS_Corvette)
        {
            ship->bulletrage*=tacticsInfo.BulletRangeBonus[Tactics_Corvette][ship->tacticstype];
        }
        */

        oldtactics = ship->tacticstype;

        //if ship wasn't in evassive, make sure its attack target is set to
        //NULL if switching to evassive so that the wingman leader AI will work properly...

        if(tacticstype == Evasive)
            if(oldtactics != Evasive)
                if (ship->attackvars.myLeaderIs)
                {
                    ship->attackvars.attacktarget = NULL;   //set target to
                }

        ship->tacticstype = tacticstype;
    }
}

/*-----------------------------------------------------------------------------
    Name        : clRUTransfer
    Description : Transfers RU's from selectcom.numShips to selectcom.timeLastStatus
    Inputs      : selectcom,resourceUntis
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clRUTransfer(CommandLayer *comlayer, sdword toIndex, sdword fromIndex, sdword resourceUnits,ubyte flags)
{
    //Place sound event here bitch (thats right...I'm talking to you Shane)
    sdword from,to;
    from = fromIndex;
    to = toIndex;

    if (resourceUnits <= 0)     // prevents cheating client side even if they hack it
        return;

    switch(flags)
    {
    case RUTRANS_GENERATEDRUS:
    case RUTRANS_BUILTSHIP:
    case RUTRANS_BUILDCANCEL:
        // obsolete now
        break;
    default:
        if (universe.players[to].playerState == PLAYER_DEAD)
            return;

        if (universe.players[from].playerState == PLAYER_DEAD)
            return;

        // CAT: no transfer the giver cannot pay for (clRUTransfer 0x00449a12)
        if (universe.players[from].resourceUnits - resourceUnits < 0)
            return;

        universe.players[from].resourceUnits -= resourceUnits;
        universe.players[to].resourceUnits += resourceUnits;

        universe.gameStats.playerStats[from].totalResourceUnits -= resourceUnits;
        universe.gameStats.playerStats[from].totalResourceUnitsGiven += resourceUnits;

        universe.gameStats.playerStats[to].totalResourceUnits += resourceUnits;
        universe.gameStats.playerStats[to].totalResourceUnitsRecieved += resourceUnits;

        // CAT: the exe's sound and speech ids (clRUTransfer 0x00449a5c: UI sound 0x129, speech 0x4001045 / 0x4001046;
        //      Cataclysm's ids, SpeechEvent.h, M2-R5 sound). The receiver also gets a command message (sprintf of
        //      strReceivedResources with the units and the giver's playerNames entry, clCommandMessage 0x004ac820
        //      colour 0xffff0000, 0x00449ab3..0x00449add) [M2-R5 hud]
        if (from == universe.curPlayerIndex)
        {                                                   //if we've given some away
            speechEventQueue(NULL, 0x4001045, -1, -1, 0, from, -1, 5.0f, -1);
        }
        else if (to == universe.curPlayerIndex)
        {                                                   //if we just got some
            char received[256];

            soundEventPlay(NULL, 0x129, NULL);
            speechEventQueue(NULL, 0x4001046, -1, -1, 0, to, -1, 5.0f, -1);
            snprintf(received, sizeof(received), strGetString(strReceivedResources), resourceUnits, playerNames[from]);
            clCommandMessage(received, 0xffff0000);
        }
        break;
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSetKamikaze
    Description : Sets the kamikaze bit for all ships that are kamikaze capable
                  provided they are doing an attack...ideally, we do that check before we transmitt
    Inputs      : selectcom,
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clSetKamikaze(CommandLayer *comlayer,SelectCommand *selectcom)
{
     sdword numShips = selectcom->numShips;
     sdword i;
     Ship *ship;
     CommandToDo *command;

     for (i=0;i<numShips;i++)
     {
         ship = selectcom->ShipPtr[i];
         command = getShipAndItsCommand(&universe.mainCommandLayer,ship);
         if(command == NULL)
         {

         }
         else if(command->ordertype.order != COMMAND_ATTACK)
         {
            if(command->ordertype.order == COMMAND_SPECIAL)
            {
                if ((ship->shiptype == kSalCapCorvette) || (ship->shiptype == tSalCapCorvette))   // CAT: both races (clSetKamikaze 0x00449b28)
                {
                    bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze|SPECIAL_BrokenFormation);
                    continue;
                }
            }
             if(ship->playerowner->playerIndex ==
                universe.curPlayerIndex)
             {
                 //shouldn't set  the kamikazee stuff...
                /////////////////////
                //no target kamikazee speech event
                 //num: COMM_Kamikaze_NoTargets
                 //battle chatter

                 //////////////
             }
         }
         else
         {
            bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze|SPECIAL_BrokenFormation);
         }
     }
}

/*-----------------------------------------------------------------------------
    Name        : ChangeOrderToHalt
    Description : changes order to Halt
    Inputs      : alreadycommand
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ChangeOrderToHalt(CommandToDo *alreadycommand)
{
    if (alreadycommand->ordertype.attributes & COMMAND_MASK_PROTECTING)
    {
        ClearProtecting(alreadycommand);
    }
    if (alreadycommand->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        ClearPassiveAttacking(alreadycommand);
    }

    if (alreadycommand->ordertype.attributes & COMMAND_MASK_ATTACKING_AND_MOVING)
    {
        bitClear(alreadycommand->ordertype.attributes,COMMAND_MASK_ATTACKING_AND_MOVING);
    }

    // CAT: a military parade keeps its order (the inlined ChangeOrderToHalt of clProcess 0x0044bfe7 and
    //      RemoveShipFromBeingTargeted 0x004453c0 test order 9)
    if (alreadycommand->ordertype.order != COMMAND_MILITARY_PARADE)
    {
        FreeLastOrder(alreadycommand);
        alreadycommand->ordertype.order = COMMAND_HALT;

        PrepareShipsForCommand(alreadycommand,TRUE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clHaltThese
    Description : command for ships to halt
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clHaltThese(CommandLayer *comlayer,SelectCommand *selectcom)
{
    CommandToDo *command;
    CommandToDo *newcommand;
    CommandToDo *alreadycommand;
    SelectCommand *selection;
    udword sizeofselection;
    sdword numShips = selectcom->numShips;
    sdword i;

    if (numShips == 0)
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("No Ships to halt!");
#endif
        return;
    }

    for (i=0;i<selectcom->numShips;)
    {
        command = getShipAndItsCommand(&universe.mainCommandLayer,selectcom->ShipPtr[i]);
        if(command != NULL)
        {
            if(command->ordertype.order == COMMAND_DOCK ||
                command->ordertype.order == COMMAND_LAUNCH_SHIP)
            {
                //if docking ship is NULL, we can assume some stuff...
                if (selectcom->ShipPtr[i]->shipOrDerelict.dockingship != NULL)
                {
                    // CAT: no HW1 ResearchShip exception (M2-R4 ships-b)
                    {
                        //signal docking ship to cancel
                        selectcom->ShipPtr[i]->forceCancelDock=TRUE;
                        selectcom->numShips--;
                        selectcom->ShipPtr[i] = selectcom->ShipPtr[selectcom->numShips];
                        continue; // we must check same index again because we put last array entry here
                    }
                }
            }
        }
        i++;
    }

    // CAT (M2-R4 combat): no salCapExtraSpecialOrderCleanUp (HW1 salvage; no counterpart call in the exe)

    if ((alreadycommand = IsSelectionAlreadyDoingSomething(comlayer,selectcom)) != NULL)
    {
        // CAT: the exe's speech ids 0x4000802 / 0x4000806 with the ship type as variable (clHaltThese 0x00449c67)
        if (alreadycommand->ordertype.order == COMMAND_MOVE)
        {
            speechEventQueue(selectcom->ShipPtr[0], 0x4000802, selectcom->ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
        }
        else if (alreadycommand->ordertype.order == COMMAND_ATTACK)
        {
            speechEventQueue(selectcom->ShipPtr[0], 0x4000806, selectcom->ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
        }

        #ifdef HW_BUILD_FOR_DEBUGGING
              dbgAssertOrIgnore(alreadycommand->ordertype.order != COMMAND_MP_HYPERSPACING);
        #endif


        ChangeOrderToHalt(alreadycommand);
        return;
    }

#if DEBUG_COMMAND_LAYER
    dbgMessage("Received Order to halt");
#endif

    // CAT: ships in a military parade are not halted (clHaltThese 0x00449d3e); the new command holds the ships left
    for (i=0;i<selectcom->numShips;)
    {
        if ((selectcom->ShipPtr[i]->command != NULL) &&
            (selectcom->ShipPtr[i]->command->ordertype.order == COMMAND_MILITARY_PARADE))
        {
            selectcom->numShips--;
            selectcom->ShipPtr[i] = selectcom->ShipPtr[selectcom->numShips];
        }
        else
        {
            i++;
        }
    }
    numShips = selectcom->numShips;

    RemoveShipsFromDoingStuff(comlayer,selectcom,0xa);

    newcommand = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
    InitCommandToDo(newcommand);

    sizeofselection = sizeofSelectCommand(numShips);
    selection = memAlloc(sizeofselection,"ToDoSelection",NonVolatile);
    memcpy(selection,selectcom,sizeofselection);

    newcommand->selection = selection;
    newcommand->ordertype.order = COMMAND_HALT;
    newcommand->ordertype.attributes = 0;

    PrepareShipsForCommand(newcommand,TRUE);

    listAddNode(&comlayer->todolist,&newcommand->todonode,newcommand);
}

/*-----------------------------------------------------------------------------
    Name        : clHalt
    Description : command for ships to halt
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clHalt(CommandLayer *comlayer,SelectCommand *selectcom)
{
    SelectCommand *copycom;
    udword sizeofcopycom;
    Node *curnode;
    Node *endnode;
    Node *nextnode;
    CommandToDo *command;
    sdword numShips = selectcom->numShips;
    sdword i;
    Ship *ship;
    bool32 stoppassiveattacking;

    if (numShips == 0)
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("No Ships to halt!");
#endif
        return;
    }

    // CAT: clHalt 0x00449e80 says so first when the leader was moving or attacking (Cataclysm's speech ids)
    ship = selectcom->ShipPtr[0];
    if (ship->command != NULL)
    {
        if (ship->command->ordertype.order == COMMAND_MOVE)
        {
            speechEventQueue(ship, 0x4000802, ship->shiptype, -1, -1, -1, -1, 0.0f, -1);
        }
        else if (ship->command->ordertype.order == COMMAND_ATTACK)
        {
            speechEventQueue(ship, 0x4000806, ship->shiptype, -1, -1, -1, -1, 0.0f, -1);
        }
    }

    for(i=0;i<selectcom->numShips;i++)
    {
        ship = selectcom->ShipPtr[i];
        stoppassiveattacking = FALSE;

        if (ship->command == NULL)
            stoppassiveattacking = TRUE;
        else
        {
            switch (ship->command->ordertype.order)
            {
                case COMMAND_NULL:
                case COMMAND_HALT:
                case COMMAND_MILITARY_PARADE:
                    stoppassiveattacking = TRUE;
            }
        }

        if (stoppassiveattacking)
            ship->passiveAttackCancelTimer=universe.totaltimeelapsed+TW_GLOBAL_PASSIVE_ATTACK_CANCEL_MODIFIER;
        else
            ship->passiveAttackCancelTimer=0.0f;
    }

    // CAT: clHalt 0x00449f3b: a ship whose CustShipHaltCommand returns FALSE is not halted
    sizeofcopycom = sizeofSelectCommand(selectcom->numShips);
    copycom = memAlloc(sizeofcopycom,"cc2(copycom)", Pyrophoric);
    copycom->numShips = 0;
    copycom->timeLastStatus = selectcom->timeLastStatus;   // CAT-FIX: the exe leaves it uninitialised (clHalt 0x00449f55)
    for (i=0;i<selectcom->numShips;i++)
    {
        ship = selectcom->ShipPtr[i];
        if ((ship->staticinfo->custshipheader.CustShipHaltCommand == NULL) ||
            ship->staticinfo->custshipheader.CustShipHaltCommand(ship))
        {
            copycom->ShipPtr[copycom->numShips++] = ship;
        }
    }

    // Now see if the selected ships are already in the command layer, and if so, individually order these groups
    // to move (to preserve formations)

    curnode = comlayer->todolist.head;
    endnode = comlayer->todolist.tail;

    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);

        nextnode = curnode->next;

        if (TheseShipsAreInSelection(command->selection,copycom))
        {
            MakeShipsNotIncludeTheseShips(copycom,command->selection);

            ChangeOrderToHalt(command);
        }

        if (copycom->numShips == 0)
        {
            break;
        }

        if (curnode == endnode)
        {
            break;
        }
        curnode = nextnode;
    }

    // tell rest of ships left to move too.
    if (copycom->numShips != 0)
    {
        clHaltThese(comlayer,copycom);
    }

    memFree(copycom);
}

/*-----------------------------------------------------------------------------
    Name        : clScuttle
    Description : command for ship to scuttle itself
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clScuttle(CommandLayer *comlayer,SelectCommand *selectcom)
{
    sdword numShips = selectcom->numShips;
    sdword i;

    if((tutorial==TUTORIAL_ONLY) && !tutEnable.bScuttle)
        return;

    if (numShips == 0)
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("No Ships to scuttle!");
#endif
        return;
    }

    for (i=0;i<numShips;i++)
    {
        // CAT: clScuttle 0x0044a150: in the tutorial the sMothership is not scuttled. A ship with CAT_SHIPFLAG_Bit7
        //      whose infection timer (catBeastConvertTime) runs is neither scuttled nor unmimicked (0x0044a19d). A mimic
        //      (CAT_SHIPFLAG_Mimic) drops its disguise (cat_mimicUnmimic(ship, 0)) [M2-R4 ships-b]
        if (bitTest(selectcom->ShipPtr[i]->header.catFlags18, CAT_SHIPFLAG_Bit7) &&
            (selectcom->ShipPtr[i]->catBeastConvertTime != 0.0f) &&
            !((tutorial==TUTORIAL_ONLY) && (selectcom->ShipPtr[i]->shiptype == sMothership)))
        {
            continue;
        }
        if ((tutorial==TUTORIAL_ONLY) && (selectcom->ShipPtr[i]->shiptype == sMothership))
        {
            if (bitTest(selectcom->ShipPtr[i]->header.catFlags18, CAT_SHIPFLAG_Mimic))
            {
                cat_mimicUnmimic(selectcom->ShipPtr[i], FALSE);
            }
            continue;
        }
        selectcom->ShipPtr[i]->deathtime = universe.totaltimeelapsed + frandombetween(SCUTTLE_SHIPDEATHTIMEMIN,SCUTTLE_SHIPDEATHTIMEMAX);
        if (bitTest(selectcom->ShipPtr[i]->header.catFlags18, CAT_SHIPFLAG_Mimic))
        {
            cat_mimicUnmimic(selectcom->ShipPtr[i], FALSE);
        }
    }

    return;
}

/*-----------------------------------------------------------------------------
    Name        : clLaunchShip
    Description : command for a ship to launch from a ship
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clLaunchShip(CommandLayer *comlayer,SelectCommand *selectcom,ShipPtr receiverShip)
{
    CommandToDo *newcommand;
    SelectCommand *selection;
    Ship *ship = selectcom->ShipPtr[0];
    bool32 baysClosed;

    dbgAssertOrIgnore(selectcom->numShips == 1);
    dbgAssertOrIgnore(receiverShip != NULL);

    // CAT (M2-R5 dock): clLaunchShip 0x0044a220: a host whose bays are closed (attribute 0x400) launches only ships that
    //      were built externally; the type's CustShipLaunchCommand slot may refuse; the ship's dock vars are set up for
    //      the launch purpose (2) and the state machine of cat_dockShipDocksAtShip does the rest
    baysClosed = (!ship->staticinfo->externalBuild && (receiverShip->obj.attributes & CAT_ATTRIBUTES_DockBaysClosed));
    if ((ship->staticinfo->custshipheader.CustShipLaunchCommand != NULL) &&
        !ship->staticinfo->custshipheader.CustShipLaunchCommand(ship,receiverShip))     // [audit: M2-R5 layout-dock] the exe passes both
    {
        return;
    }
    if (baysClosed)
    {
        return;
    }
    if (ship->shipOrDerelict.specialFlags & SPECIAL_Launching)
    {
        return;         // already launching
    }
    if (ship->shiplink.belongto)
    {
        return;         // already outside
    }

    RemoveShipsFromDoingStuff(comlayer,selectcom,0xb);

    newcommand = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
    InitCommandToDo(newcommand);

    selection = memAlloc(sizeofSelectCommand(1),"ToDoSelection",NonVolatile);
    selection->numShips = 1;
    selection->timeLastStatus = selectcom->timeLastStatus;
    selection->ShipPtr[0] = ship;

    newcommand->selection = selection;
    newcommand->ordertype.order = COMMAND_LAUNCH_SHIP;
    newcommand->ordertype.attributes = 0;

    newcommand->launchship.receiverShip = receiverShip;

    InitializeDockVars(ship,receiverShip,CAT_DOCK_LAUNCH);

    InitShipsForAI(selection,TRUE);

    PrepareShipsForCommand(newcommand,TRUE);
    tacticsMakeShipsNotLookForOtherShips(selection);

    //insert at begining to take priority over other commands that will be occurring
    //i.e. retiring
    listAddNodeBeginning(&comlayer->todolist,&newcommand->todonode,newcommand);
}

/*-----------------------------------------------------------------------------
    Name        : cat_clHarvestNearestResource
    Description : CAT: sends the one ship of selectcom to harvest the nearest resource (cataclysm.exe 0x00449790;
                  rescollectFindNearestResource 0x004f11f0 and the harvest key mrHarvestResources 0x00563cc0 call it).
                  With no resource left: the out-of-resources message and speech for the local player (once per
                  10 s of real time, 0x00753304) or the ship's own line when the order came from the UI, and in a
                  multiplayer (or recorded) game the ship docks to deposit (purpose 3). With one: the UI's
                  acknowledgement, then for a single-ship command in a single-player game the command is turned into
                  a collect order in place, else clWrapCollectResource
    Inputs      : comlayer, selectcom (one ship), fromUI (the harvest key), firstOfSelection (sound and speech once)
    Return      : the resource when the command was changed in place, else NULL
----------------------------------------------------------------------------*/
Resource *cat_clHarvestNearestResource(CommandLayer *comlayer,SelectCommand *selectcom,bool32 fromUI,bool32 firstOfSelection)
{
    Ship *ship = selectcom->ShipPtr[0];
    CommandToDo *command;
    Resource *resource;
    ShipPtr dockwith;

    (void)comlayer;
    // CAT (M2-R6 nis-fow-camera): univFindNearestResource's fourth argument: the fog of war for a player without an AI
    //      (0x004497a0)
    resource = univFindNearestResource(ship,0.0f,NULL,(ship->playerowner->aiPlayer == NULL));
    if (resource == NULL)
    {
        if (!fromUI)
        {
            if (catHarvestMessageTime < cat_taskRealTimeElapsed)
            {
                catHarvestMessageTime = cat_taskRealTimeElapsed + 10.0f;
                // CAT (M2-R5 hud): the "no resources" command message (0x00bb4de0, red; cat_clHarvestNearestResource)
                clCommandMessage(strGetString(strNoMoreResourcesVisible), 0xff0000ff);
                // CAT: the fleet says there is nothing to harvest (cat_clHarvestNearestResource 0x00449790)
                speechEventQueue(NULL, 0x2001043, 0, -1, 0, ship->playerowner->playerIndex, -1, 5.0f, -1);
            }
        }
        else
        {
            // CAT: the order's refusal (cat_clHarvestNearestResource 0x00449790): UI sound 0x10a and the ship's line
            soundEventPlay(NULL, 0x10a, NULL);
            speechEventQueue(ship, 0x40008a2, ship->shiptype, -1, -1, -1, -1, 0.0f, -1);
        }
        if (multiPlayerGame || recordPackets || playPackets)
        {
            dockwith = FindNearestShipToDockAt(ship,CAT_DOCK_DEPOSIT_RESOURCES);
            if (dockwith != NULL)
            {
                clWrapDock(&universe.mainCommandLayer,selectcom,CAT_DOCK_DEPOSIT_RESOURCES,dockwith);
            }
        }
        return NULL;
    }
    if (fromUI && firstOfSelection)
    {
        // CAT: the order's acknowledgement (cat_clHarvestNearestResource 0x00449790): UI sound 0x109, the ship's line, its
        //      engine (ShipCmn_Engine)
        soundEventPlay(NULL, 0x109, NULL);
        speechEventQueue(ship, 0x40008a3, ship->shiptype, -1, -1, -1, -1, 0.0f, -1);
        soundEventPlay(ship, ShipCmn_Engine, NULL);
    }
    command = ship->command;
    if ((command != NULL) && (command->selection->numShips == 1) && !multiPlayerGame && !recordPackets && !playPackets)
    {
        FreeLastOrder(command);
        ChangeSingleShipToCollectResource(command,resource);
        return resource;
    }
    clWrapCollectResource(&universe.mainCommandLayer,selectcom,resource);
    return NULL;
}

/*-----------------------------------------------------------------------------
    Name        : clLaunchMultipleShips
    Description : command for multiple ships to launch from a ship
    Inputs      : selectcom, receiverShip
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clLaunchMultipleShips(CommandLayer *comlayer,SelectCommand *selectcom,ShipPtr launchFrom)
{
    sdword numShips = selectcom->numShips;
    sdword i;
    SelectCommand selectone;

    if (numShips == 0)
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("No Ships to launch");
#endif
        return;
    }

    if (launchFrom == NULL)
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("No Ship to launch from");
#endif
        return;
    }

    selectone.numShips = 1;
    for (i=0;i<numShips;i++)
    {
        if (!(selectcom->ShipPtr[i]->obj.attributes & ATTRIBUTES_Defector))      // don't let the defector launch
        {
            selectone.ShipPtr[0] = selectcom->ShipPtr[i];
            clLaunchShip(comlayer,&selectone,launchFrom);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : LaunchAllInternalShipsOfPlayerThatMustBeLaunched
    Description : launches all internal ships of given player that must be launched
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void LaunchAllInternalShipsOfPlayerThatMustBeLaunched(struct Player *player)
{
    Node *objnode = universe.ShipList.head;
    Ship *ship;

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (ship->playerowner == player)
        {
            if (ship->shipsInsideMe)     // CAT: no HW1 DDDFrigate (M2-R4 ships-b)
            {
                Node *insidenode = ship->shipsInsideMe->insideList.head;
                InsideShip *insideship;
                SelectCommand selectone;

                while (insidenode != NULL)
                {
                    insideship = (InsideShip *)listGetStructOfNode(insidenode);
                    dbgAssertOrIgnore(insideship->ship->header.objtype == OBJ_ShipType);

                        // make defector launch
                    if ((insideship->ship->obj.attributes & ATTRIBUTES_Defector) || (ShipHasToLaunch(ship,insideship->ship)))
                    {
                        selectone.numShips = 1;
                        selectone.ShipPtr[0] = insideship->ship;

                        clLaunchShip(&universe.mainCommandLayer,&selectone,ship);
                    }

                    insidenode = insidenode->next;
                }
            }
        }

        objnode = objnode->next;
    }
}

// CAT: FillInCarrierMothershipInfo 0x0044a490 (map: IDENTICAL, but modified): the mothership is the player's only when
//      it is CLASS_Mothership (HW1: a Carrier-type mothership counts as carrier 1), and three carrier slots are filled
//      from the player's first three cmCarriers (only carrierX[0..2] are cleared and written)
void FillInCarrierMothershipInfo(struct Player *player,Ship **mothership,Ship *carrierX[])
{
    sdword i;
    sdword n = 0;

    *mothership = NULL;
    carrierX[0] = NULL;
    carrierX[1] = NULL;
    carrierX[2] = NULL;

    if ((player->PlayerMothership != NULL) && (player->PlayerMothership->staticinfo->shipclass == CLASS_Mothership))
    {
        *mothership = player->PlayerMothership;
    }

    for (i = 0; i < cmNumCarriers; i++)
    {
        if ((cmCarriers[i].owner == player) && (n < 3))
        {
            carrierX[n++] = cmCarriers[i].ship;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : LaunchAllInternalShipsOfPlayer
    Description : launches all internal ships of given player
    Inputs      : carriermask: 0=momship, 1=carrier1, 2=carrier2
    Outputs     :
    Return      : the number of ships launched
----------------------------------------------------------------------------*/
// CAT: LaunchAllInternalShipsOfPlayer 0x0044a510 builds its own list (no FillInCarrierMothershipInfo): slot 0 the
//      player's mothership when it is CLASS_Mothership, slots 1-3 the player's first three cmCarriers; only mask bits
//      0-2 are tested (the mothership and carriers 1 and 2)
sdword LaunchAllInternalShipsOfPlayer(struct Player *player, udword carriermask)
{
    Node *objnode = universe.ShipList.head;
    Ship *ship;
    Ship *slots[4];
    sdword numCarriers = 0;
    sdword numShips = 0;
    sdword i;
    bool32 launchIt;

    slots[0] = slots[1] = slots[2] = slots[3] = NULL;
    if ((player->PlayerMothership != NULL) && (player->PlayerMothership->staticinfo->shipclass == CLASS_Mothership))
    {
        slots[0] = player->PlayerMothership;
    }
    for (i = 0; i < cmNumCarriers; i++)
    {
        if ((cmCarriers[i].owner == player) && (numCarriers < 3))
        {
            slots[1 + numCarriers] = cmCarriers[i].ship;
            numCarriers++;
        }
    }

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (ship->playerowner == player)
        {
            if (ship->shipsInsideMe)     // CAT: no HW1 DDDFrigate (M2-R4 ships-b)
            {
                launchIt = FALSE;
                for (i = 0; i < 3; i++)
                {
                    if ((carriermask & (1u << i)) && (ship == slots[i]))
                    {
                        launchIt = TRUE;
                        break;
                    }
                }
                if (launchIt)
                {
                    Node *insidenode = ship->shipsInsideMe->insideList.head;
                    InsideShip *insideship;
                    SelectCommand selectone;

                    while (insidenode != NULL)
                    {
                        insideship = (InsideShip *)listGetStructOfNode(insidenode);
                        dbgAssertOrIgnore(insideship->ship->header.objtype == OBJ_ShipType);

                        if (!(insideship->ship->obj.attributes & ATTRIBUTES_Defector))      // don't let the defector launch
                        {
                            selectone.numShips = 1;
                            selectone.ShipPtr[0] = insideship->ship;

                            clLaunchShip(&universe.mainCommandLayer,&selectone,ship);
                            numShips++;
                        }

                        insidenode = insidenode->next;
                    }

                }

            }
        }

        objnode = objnode->next;
    }

    return (numShips);
}

/*-----------------------------------------------------------------------------
    Name        : clAutoLaunch
    Description : command to turn the autoLaunch flag on or off for a given player
    Inputs      : OnOff - bit0=Mothership, bit1=Carrier 1, bit2=Carrier2, bit3=Carrier3, bit4=Carrier4
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: clAutoLaunch 0x0044a640 (map: IDENTICAL): every bit that is on after the change launches the ships inside
//      (not only a bit just switched on); the fleet's line is the exe's speech 0x4001068, in every mission
//      (Cataclysm's speech id, SpeechEvent.h)
void clAutoLaunch(udword OnOff,udword playerIndex)
{
    Player *player = &universe.players[playerIndex];
    bool32 speechevent = FALSE;
    sdword i;
    udword bit = BIT0;

    for (i=0;i<(4+1);i++,bit<<=1)       // 4 Carriers + 1 Mothership        BIT0..BIT4 are valid
    {
        if ((player->autoLaunch & bit) != (OnOff & bit))
        {
            if (OnOff & bit)
            {
                // changing autoLaunch from Off to On
                player->autoLaunch |= bit;
            }
            else
            {
                // changing autoLaunch from On to Off
                player->autoLaunch &= ~bit;
            }
        }
        if (player->autoLaunch & bit)
        {
            // tell all internal ships to launch:
            if (LaunchAllInternalShipsOfPlayer(player, bit) > 0)
            {
                speechevent = TRUE;
            }
        }
    }

    if (speechevent)
    {
        speechEventQueue(NULL, 0x4001068, -1, -1, 0, playerIndex, -1, 5.0f, -1);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSetAlliance
    Description : sets an alliance based on a player bitmask
    Inputs      : player bitmask
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void clSetAlliance(udword AllianceType, uword curalliance, uword newalliance)
{
    allianceSetAlliance(AllianceType, curalliance, newalliance);
}

void clSetResearch(udword type, udword playernum, udword labnum, udword tech)
{
    if (type == RESEARCH_SUBCOMMAND_START)
    {
        rmAssignPlayersLabToResearch(&universe.players[playernum], labnum, tech);
    }
    else if (type == RESEARCH_SUBCOMMAND_STOP)
    {
        rmClearResearchlab(&universe.players[playernum], labnum);
    }
}

// CAT: cat_shipIsLiveWorker 0x005c8e00 lives in Ships/Worker.c [M2-R4 ships-b]

bool32 ShipHasToLaunch(Ship *InsideShip, Ship *ship)
{
    if (singlePlayerGame)
    {
        if (singlePlayerGameInfo.hyperspaceState != NO_HYPERSPACE)
        {
            return FALSE;
        }
    }

    // CAT: ShipHasToLaunch 0x0044a780: a ship built in another player's factory always launches; swarmers
    //      (sSwarmer 9, bSwarmer 43) never do
    if (ship->playerowner->playerIndex != InsideShip->playerowner->playerIndex)
    {
        return TRUE;
    }
    if ((ship->shiptype == bSwarmer) || (ship->shiptype == sSwarmer))
    {
        return FALSE;
    }

    if (ship->shipOrDerelict.specialFlags & SPECIAL_STAY_TILL_EXPLICITLAUNCH)
    {
        return FALSE;
    }

    // CAT: micro-class ships count with the fighters (classes 6 and 11)
    if ( ((ship->staticinfo->shipclass==CLASS_Fighter) || (ship->staticinfo->shipclass==CLASS_Micro)) &&
         (InsideShip->shipsInsideMe->FightersInsideme <=
          InsideShip->staticinfo->maxDockableFighters) )
        return (FALSE);

    // CAT: live workers count with the corvettes (cat_shipIsLiveWorker)
    if ( ((ship->staticinfo->shipclass==CLASS_Corvette) || cat_shipIsLiveWorker(ship)) &&
         (InsideShip->shipsInsideMe->CorvettesInsideme <=
          InsideShip->staticinfo->maxDockableCorvettes) )
        return (FALSE);

    return (TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : clCreateShip
    Description : creates a new ship
    Inputs      : shipType, shipRace, playerIndex, creator
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
Ship *clCreateShip(CommandLayer *comlayer,ShipType shipType,ShipRace shipRace,uword playerIndex,ShipPtr creator)
{
    Ship *ship = NULL;
    SelectCommand select;


    Player *player = &universe.players[playerIndex];
    Ship *mothership, *carrier1=NULL, *carrier2=NULL;
    sword i;
    udword launchmask = BIT0;


    mothership = player->PlayerMothership;
    // CAT: a carrier-class "mothership" counts as the first carrier (clCreateShip 0x0044a840 tests shipclass 2)
    if ((mothership) && (mothership->staticinfo->shipclass == CLASS_Carrier))
    {
        carrier1 = mothership;
        mothership = NULL;
    }

    for (i = 0; i < cmNumCarriers; i++)
    {
        if (cmCarriers[i].owner == player)
        {
            if (carrier1)
            {
                carrier2 = cmCarriers[i].ship;
                break;
            }
            else
            {
                carrier1 = cmCarriers[i].ship;
            }
        }
    }

    if (creator == mothership)
    {
        launchmask = BIT0;
    }
    else if (creator == carrier1)
    {
        launchmask = BIT1;
    }
    else if (creator == carrier2)
    {
        launchmask = BIT2;
    }





    if (creator == NULL)
    {
        return NULL;         // if creator died, too bad for ships inside
    }

    if (universe.players[playerIndex].playerState != PLAYER_ALIVE)
    {
        return NULL;         // if player is not alive, don't allow ship creation
    }

    dbgAssertOrIgnore(playerIndex < universe.numPlayers);

    // create ship
    // CAT (M2-R6 managers): the creator is univCreateShip's 6th argument (clCreateShip 0x0044a840 passes it: an externally
    //      built type then starts at health 0 and is no factory yet)
    ship = univCreateShip(shipType,shipRace,&creator->obj.posinfo.position,&universe.players[playerIndex],1,creator);

    gameStatsAddShip(ship,playerIndex);

    // update unit cap variables
    unitCapCreateShip(ship,&universe.players[playerIndex]);

    // CAT (M2-R5 layout-dock): the player's built-ships-by-class count (PlayerStats+0x2A8, clCreateShip 0x0044a840):
    //      every type but the swarmers, and the sACV, sMCV and bACV twice (the exe repeats the increment for them).
    //      No research ship and no rmActivateFreeLab. The exe also passes built 5 and the creator to its 7-argument
    //      univCreateShip; the tree's 1 has the same bit 0.
    if ((shipType != sSwarmer) && (shipType != bSwarmer))
    {
        universe.gameStats.playerStats[playerIndex].catBuiltForEachClass[ship->staticinfo->shipclass]++;
        if ((shipType == sACV) || (shipType == sMCV) || (shipType == bACV))
        {
            universe.gameStats.playerStats[playerIndex].catBuiltForEachClass[ship->staticinfo->shipclass]++;
        }
    }

    // update the special internet ghost totals, because of lag.
    if ((multiPlayerGame) && (playerIndex==sigsPlayerIndex))
    {
        shiplagtotals[shipType]--;
    }

    // put ship inside creator
    dockPutShipInside(ship,creator);

    if ( (universe.players[playerIndex].autoLaunch & launchmask) ||
         (ShipHasToLaunch(creator, ship)) )
    {
        select.numShips = 1;
        select.ShipPtr[0] = ship;


        clLaunchShip(comlayer,&select,(ShipPtr)creator);
    }
    return(ship);
}

/*-----------------------------------------------------------------------------
    Name        : processBuildingShipToDo
    Description : processes ship building command
    Inputs      :
    Outputs     :
    Return      : TRUE if command is done
----------------------------------------------------------------------------*/
bool32 processBuildingShipToDo(CommandToDo *command)
{
    Player *player;
    Ship *creator;

    if (universe.univUpdateCounter > command->buildingship.frameAtWhichToCreate)
    {
        creator = command->buildingship.creator;
        dbgAssertOrIgnore(creator != NULL);
        player = creator->playerowner;

        clCreateShip(&universe.mainCommandLayer,command->buildingship.shipType,command->buildingship.shipRace,
                     player->playerIndex,creator);

        return TRUE;
    }

    return FALSE;
}

udword NumberOfOtherShipsOfSameTypeBeingBuilt(ShipType shipType,ShipPtr creator,udword playerIndex)
{
    Node *curnode = universe.mainCommandLayer.todolist.head;
    CommandToDo *command;
    udword numOtherBeingBuilt = 0;

    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);
        if ((command->ordertype.order == COMMAND_BUILDING_SHIP) && (command->buildingship.playerIndex == playerIndex))
        {
            if (command->buildingship.shipType == shipType)
            {
                // if they don't have a creator or both have the same creator
                if ( ((creator == NULL) || (command->buildingship.creator == NULL)) ||
                     (creator == command->buildingship.creator))
                {
                    numOtherBeingBuilt++;
                }
            }
        }

        curnode = curnode->next;
    }

    return numOtherBeingBuilt;
}

void clDeterministicBuild(udword command, CommandLayer* comLayer,
                          sdword numShips,
                          ShipType shipType, ShipRace shipRace,
                          uword playerIndex, ShipPtr creator)
{
    Player* player = &universe.players[playerIndex];

    dbgAssertOrIgnore(playerIndex < universe.numPlayers);

    if (creator == NULL)
    {
        //creator died, ships die too
        return;
    }

    if (player->playerState != PLAYER_ALIVE)
    {
        //!alive players can't build ships
        return;
    }

    cmDeterministicBuild(command, numShips, shipType, shipRace, playerIndex, creator);
}

/*-----------------------------------------------------------------------------
    Name        : clBuildShip
    Description : builds a new ship (including waiting time for it to be built)
    Inputs      : shipType, shipRace, playerIndex, creator
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clBuildShip(CommandLayer *comlayer,ShipType shipType,ShipRace shipRace,uword playerIndex,ShipPtr creator)
{
    Player *player = &universe.players[playerIndex];
    ShipStaticInfo *shipstaticinfo = GetShipStaticInfo(shipType,shipRace);
    CommandToDo *newcommand;

    dbgAssertOrIgnore(playerIndex < universe.numPlayers);

    if (creator == NULL)
    {
        return;         // if creator died, too bad for ships inside
    }

    if (player->playerState != PLAYER_ALIVE)
    {
        return;         // if player is not alive, don't allow ship creation
    }

    //if (shipstaticinfo->buildCost <= player->resourceUnits)
    {
        player->resourceUnits -= shipstaticinfo->buildCost;     // subtract cost right away
        universe.gameStats.playerStats[player->playerIndex].totalResourceUnitsSpent += shipstaticinfo->buildCost;
        // make new command indicating delayed build:

        newcommand = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
        InitCommandToDo(newcommand);

        newcommand->selection = memAlloc(sizeofSelectCommand(1),"NoSelection",0);
        newcommand->selection->numShips = 0;
        newcommand->ordertype.order = COMMAND_BUILDING_SHIP;
        newcommand->ordertype.attributes = 0;
        newcommand->buildingship.shipType = shipType;
        newcommand->buildingship.shipRace = shipRace;
        newcommand->buildingship.creator = creator;
        newcommand->buildingship.playerIndex = (udword)playerIndex;
        newcommand->buildingship.frameAtWhichToCreate = universe.univUpdateCounter + ((NumberOfOtherShipsOfSameTypeBeingBuilt(shipType,creator,playerIndex)+1) * shipstaticinfo->buildTime);

        PrepareShipsForCommand(newcommand,TRUE);

        listAddNode(&comlayer->todolist,&newcommand->todonode,newcommand);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_clStartExternalBuildTimer
    Description : CAT: new (0x0044aa30): marks the first ship of the selection with SPECIAL_2_CatExternalBuildTimer;
                  cat_shipUpdateExternalBuildAndAI 0x00543f10 runs and clears it
----------------------------------------------------------------------------*/
void cat_clStartExternalBuildTimer(CommandLayer *comlayer,SelectCommand *selectcom)
{
    (void)comlayer;
    if (selectcom->numShips != 0)
    {
        bitSet(selectcom->ShipPtr[0]->shipOrDerelict.specialFlags2,SPECIAL_2_CatExternalBuildTimer);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_clExternalBuildUpdate
    Description : CAT: new (0x0044dfc0): one step of an external build. The health grows with the progress
                  (health += (progress - catBuildProgress) * maxhealth, at most maxhealth); CAT_EXTBUILD_DONE also
                  ends the build (SPECIAL_2_CatBeingBuilt cleared; a Sect ship activates its research module) and
                  CAT_EXTBUILD_CANCEL deletes the ship. comlayer, playerIndex and creator are unused (the network
                  packet carries them)
----------------------------------------------------------------------------*/
static void clExternalBuildAdvance(Ship *ship,real32 progress)
{
    real32 maxhealth = ship->staticinfo->health.maxhealth;     // exe statinfo+0xA4
    double health;

    // CAT: one x87 expression with a single rounding (cat_clExternalBuildUpdate 0x0044dfc0, modes 2 and 0 alike:
    //      fld dword [progress]; fsub dword [ship+0x3e8]; fmul dword [statinfo+0xa4]; fadd dword [ship+0x170];
    //      fst dword [ship+0x170]; fcomp dword [statinfo+0xa4]). Every operand is a float (dword) load; the
    //      intermediates stay on the FPU at the 53-bit precision of the game's control word 0x027f (every tick of
    //      oracle01-rec.hwo) and are rounded to float once, at the fst; the clamp compares the UNROUNDED sum (fcomp
    //      on st0 after the fst). Each double step rounds to 53 bits as the x87 does at that precision (the values
    //      stay far from the double exponent limits), so this is the exe's result bit for bit (the oracle01 replay's
    //      tick-136 health of ship 47: 0x43160001, where float steps gave 0x43160000)
    health = ((double)progress - (double)ship->catBuildProgress) * (double)maxhealth + (double)ship->targetable.health;
    ship->catBuildProgress = progress;
    ship->targetable.health = (real32)health;
    if (health > (double)maxhealth)
    {
        ship->targetable.health = maxhealth;
    }
}

void cat_clExternalBuildUpdate(CommandLayer *comlayer,uword playerIndex,Ship *creator,Ship *ship,real32 progress,sdword mode)
{
    (void)comlayer;
    (void)playerIndex;
    (void)creator;

    if (ship == NULL)
    {
        return;     // the exe calls a compiled-out debug message here (0x0044dfc8)
    }
    switch (mode)
    {
        case CAT_EXTBUILD_DONE:
            if (ship->catBuildProgress < 1.0f)      // the exe compares with the double 1.0 at 0x0072a548
            {
                clExternalBuildAdvance(ship,progress);
            }
            bitClear(ship->shipOrDerelict.specialFlags2,SPECIAL_2_CatBeingBuilt);
            if (ship->shiprace == RACE_Sect)
            {
                // CAT: a finished Sect module opens its research lab (cat_rmResearchModuleActivated 0x004f4530)
                cat_rmResearchModuleActivated(ship->playerowner, ship->shiptype, TRUE);
            }
            cmDirtyShipInfo();
            break;

        case CAT_EXTBUILD_CANCEL:
            bitClear(ship->shipOrDerelict.specialFlags2,SPECIAL_2_CatBeingBuilt);
            univReallyDeleteThisShipRightNow(ship);
            cmDirtyShipInfo();
            break;

        case CAT_EXTBUILD_PROGRESS:
            clExternalBuildAdvance(ship,progress);
            break;

        default:
            break;
    }
}

/*-----------------------------------------------------------------------------
    Name        : FindShipsWereGuarding
    Description : finds ships we're guarding, either directly or indirectly
    Inputs      :
    Outputs     :
    Return      : returns TRUE if terminating condition was finding that shipsWereGuarding are directly or
                  indirectly guarding us - returns FALSE if terminating condition is we can't find any more ships we're guarding
----------------------------------------------------------------------------*/
bool32 FindShipsWereGuarding(CommandToDo *originalus,SelectCommand *us,GrowSelection *shipsWereGuarding,sdword iteration)
{
    sdword i,j;
    sdword usnumShips = us->numShips;
    Ship *ship;
    CommandToDo *shipcommand;
    GrowSelection newshipsWereGuarding;
    bool32 result;

    if (iteration > 10)
    {
        return FALSE;       // safety for recursive function
    }

    for (i=0;i<usnumShips;i++)
    {
        ship = us->ShipPtr[i];
        shipcommand = ship->command;
        if (shipcommand && (shipcommand->ordertype.attributes & COMMAND_MASK_PROTECTING))
        {
            for (j=0;j<shipcommand->protect->numShips;j++)
            {
                growSelectAddShipNoDuplication(shipsWereGuarding,shipcommand->protect->ShipPtr[j]);
            }
        }
    }

    if (shipsWereGuarding->selection->numShips == 0)
    {
        return FALSE;
    }

    if (AnyOfTheseShipsAreInSelection(shipsWereGuarding->selection,originalus->selection))
    {
        return TRUE;
    }

    growSelectInit(&newshipsWereGuarding);

    result = FindShipsWereGuarding(originalus,shipsWereGuarding->selection,&newshipsWereGuarding,iteration+1);

    growSelectClose(&newshipsWereGuarding);

    return result;
}

/*-----------------------------------------------------------------------------
    Name        : CheckCircularGuard
    Description : Checks for circular guard, and if it is sets the bit SPECIAL_CircularGuard for each ship otherwise clears it
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void CheckCircularGuard(CommandToDo *command)
{
    sdword i;
    GrowSelection shipsWereGuarding;

    growSelectInit(&shipsWereGuarding);

    if (FindShipsWereGuarding(command,command->selection,&shipsWereGuarding,0))
    {
        // ships we're guarding are directly or indirectly guarding us:
        SelectCommand *selection = command->selection;

        for (i=0;i<selection->numShips;i++)
        {
            bitSet(selection->ShipPtr[i]->shipOrDerelict.specialFlags,SPECIAL_CircularGuard);   // CAT: specialFlags in the exe
        }
    }
    else
    {
        // ships we're guarding are NOT directly or indirectly guarding us:
        SelectCommand *selection = command->selection;

        for (i=0;i<selection->numShips;i++)
        {
            bitClear(selection->ShipPtr[i]->shipOrDerelict.specialFlags,SPECIAL_CircularGuard);   // CAT: specialFlags in the exe
        }
    }

    growSelectClose(&shipsWereGuarding);
}

void ChangeOrderToProtect(CommandToDo *alreadycommand,ProtectCommand *protectcom)
{
    ProtectCommand *protect;
    udword sizeofprotect;

    alreadycommand->protectFlags = 0;

    if (alreadycommand->ordertype.attributes & COMMAND_MASK_PROTECTING)
    {
        if (SelectionsAreTotallyEquivalent(alreadycommand->protect,protectcom))
        {
            return;
        }
        ClearProtecting(alreadycommand);
    }

#if DEBUG_COMMAND_LAYER
    dbgMessage("Changed Order to protect");
#endif

    FreeLastOrder(alreadycommand);

    alreadycommand->ordertype.order = COMMAND_NULL;
    alreadycommand->ordertype.attributes |= COMMAND_MASK_PROTECTING;

    sizeofprotect = sizeofProtectCommand(protectcom->numShips);
    protect = memAlloc(sizeofprotect,"ToDoProtect",0);
    memcpy(protect,protectcom,sizeofprotect);

    alreadycommand->protect = protect;

    PrepareShipsForCommand(alreadycommand,TRUE);
    CheckCircularGuard(alreadycommand);

    if (alreadycommand->ordertype.attributes & COMMAND_MASK_FORMATION)
    {
        // CAT: ChangeOrderToProtect 0x0044ad50 (map: IDENTICAL): a force-field sphere too (0x0044af04)
        if ((alreadycommand->formation.formationtype == SPHERE_FORMATION) ||
            (alreadycommand->formation.formationtype == FORCEFIELD_SPHERE_FORMATION))
        {
            // we are now protecting in sphere formation, and we want to surround
            // a ship that we protect when go in sphere formation.  This is different
            // than the regular sphere formation, so call to tell formationContentHasChanged
            formationContentHasChanged(alreadycommand);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : clProtect
    Description : command to protect a ship
    Inputs      : selectcom, protectThisShip
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
CommandToDo *clProtect(CommandLayer *comlayer,SelectCommand *selectcom,ProtectCommand *protectcom)
{
    CommandToDo *newcommand;
    CommandToDo *alreadycommand;
    SelectCommand *selection;
    ProtectCommand *protect;
    udword sizeofselection;
    udword sizeofprotect;
    sdword numShips = selectcom->numShips;
    sdword numShipsToProtect = protectcom->numShips;

    if (numShips == 0)
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("No ships selected as protectors");
#endif
        return NULL;
    }
    //cleanup like halt command
    // CAT (M2-R4 combat): no salCapExtraSpecialOrderCleanUp (HW1 salvage; no counterpart call in the exe)

    if (numShipsToProtect == 0)
    {
#if DEBUG_COMMAND_LAYER
        dbgMessage("No ships to protect");
#endif
        return NULL;
    }

    if ((alreadycommand = IsSelectionAlreadyDoingSomething(comlayer,selectcom)) != NULL)
    {
        ChangeOrderToProtect(alreadycommand,protectcom);
        return alreadycommand;
    }

#if DEBUG_COMMAND_LAYER
    dbgMessage("Received Order to protect");
#endif

    RemoveShipsFromDoingStuff(comlayer,selectcom,0xc);

    newcommand = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
    InitCommandToDo(newcommand);

    sizeofselection = sizeofSelectCommand(numShips);
    selection = memAlloc(sizeofselection,"ToDoSelection",NonVolatile);
    memcpy(selection,selectcom,sizeofselection);

    sizeofprotect = sizeofProtectCommand(numShipsToProtect);
    protect = memAlloc(sizeofprotect,"ToDoProtect",0);
    memcpy(protect,protectcom,sizeofprotect);

    newcommand->selection = selection;
    newcommand->ordertype.order = COMMAND_NULL;
    newcommand->ordertype.attributes = COMMAND_MASK_PROTECTING;
    newcommand->protect = protect;
    newcommand->protectFlags = 0;

    PrepareShipsForCommand(newcommand,TRUE);

    listAddNode(&comlayer->todolist,&newcommand->todonode,newcommand);
    CheckCircularGuard(newcommand);
    return newcommand;
}

/*-----------------------------------------------------------------------------
    Name        : AddShipToGroup
    Description : Adds ship to CommandToDo group
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void AddShipToGroup(ShipPtr ship,CommandToDo *group)
{
    SelectCommand *newselection;
    sdword sizeofnewselection;
    sdword sizeofselection;
    sdword numShips;

    numShips = group->selection->numShips;
    dbgAssertOrIgnore(numShips >= 1);

    sizeofnewselection = sizeofSelectCommand(numShips+1);
    sizeofselection = sizeofSelectCommand(numShips);

    newselection = memAlloc(sizeofnewselection,"ToDoSelection",NonVolatile);
    memcpy(newselection,group->selection,sizeofselection);

    memFree(group->selection);
    group->selection = newselection;

    newselection->ShipPtr[numShips] = ship;
    newselection->numShips = numShips + 1;

    PrepareOneShipForCommand(ship,group,TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : AddShipToFormationGroup
    Description : adds a ship to group
    Inputs      : ship, group
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void AddShipToFormationGroup(ShipPtr ship,CommandToDo *group)
{
#if DEBUG_LAUNCH_SHIP
    dbgMessage("Adding ship to group");
#endif

    AddShipToGroup(ship,group);
    // CAT: no FillInShipFormationStuff (a nullsub in GroupShip 0x0044b3d6)

    dbgAssertOrIgnore(group->ordertype.attributes & COMMAND_MASK_FORMATION);
    formationContentHasChanged(group);

    InitShipAI(ship,TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : GroupShipIntoMilitaryParade
    Description : groups ship into military parade around aroundShip
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: a CLASS_Component ship (a module) joins no parade (GroupShipIntoMilitaryParade 0x0044b1c0; also inlined in
//      clSetMilitaryParade 0x0044b2b0)
void GroupShipIntoMilitaryParade(CommandLayer *comlayer,ShipPtr ship,ShipPtr aroundShip)
{
    CommandToDo *militaryGroup;

    if (ship->staticinfo->shipclass == CLASS_Component)
    {
        return;
    }
    militaryGroup = GetMilitaryGroupAroundShip(comlayer,aroundShip);
    if (militaryGroup != NULL)
    {
        AddShipToMilitaryGroup(ship,militaryGroup);
    }
    else
    {
        CreateMilitaryGroupAroundShip(comlayer,ship,aroundShip);
    }
}

/*-----------------------------------------------------------------------------
    Name        : clSetMilitaryParade
    Description : sets ships into a military parade
    Inputs      : selectcom
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clSetMilitaryParade(CommandLayer *comlayer,SelectCommand *selectcom)
{
    sdword index;
    sdword i;
    Ship *aroundShip;
    Ship *ship;
    CommandToDo *command;

    index = MothershipOrCarrierIndexInSelection(selectcom);
    if (index < 0)
    {
        dbgMessagef("Warning: No ship to form parade around");
        return;
    }

    dbgAssertOrIgnore(index < selectcom->numShips);
    aroundShip = selectcom->ShipPtr[index];

    for (i=0;i<selectcom->numShips;i++)
    {
        if (i != index)
        {
            ship = selectcom->ShipPtr[i];
            command = getShipAndItsCommand(&universe.mainCommandLayer,ship);
            if ((command != NULL) && (command->ordertype.order == COMMAND_MILITARY_PARADE) &&
                (command->militaryParade->aroundShip  == aroundShip))
            {
                // already in military Parade around aroundShip
                // do nothing
            }
            else
            {
                SelectCommand selectone;
                selectone.ShipPtr[0] = ship;
                selectone.numShips = 1;
                RemoveShipsFromDoingStuff(comlayer,&selectone,0xd);
                GroupShipIntoMilitaryParade(comlayer,selectcom->ShipPtr[i],aroundShip);
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : GroupShip
    Description : puts a ship into an appropriate group around the aroundShip
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void GroupShip(CommandLayer *comlayer,ShipPtr ship,ShipPtr aroundShip)
{
//    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    CommandToDo *shipGroup;
//    sdword groupSize;
//    vector destination;
    SelectCommand selection;


//    groupSize = shipstaticinfo->groupSize;
//    dbgAssertOrIgnore(groupSize > 0);
    // CAT: no HW1 ResearchShip docking (M2-R4 ships-b)
    shipGroup = getShipAndItsFormationCommand(comlayer,aroundShip);
    if (shipGroup != NULL)
    {
        // the aroundShip is in formation, so let's just add this ship to aroundShip's formation
        AddShipToFormationGroup(ship,shipGroup);
        return;
    }

    GroupShipIntoMilitaryParade(comlayer,ship,aroundShip);
}

/*-----------------------------------------------------------------------------
    Name        : clShipDied
    Description : Call this function to update ship command layer when ship dies
    Inputs      : deadship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clShipDied(CommandLayer *comlayer,ShipPtr deadship)
{
    SelectCommand selection;
    Node *node;
    CommandToDo *paradeAroundDeadShip = NULL;

    // CAT: clShipDied 0x0044b470: the ships of a parade around the dead ship stop first (a copy of the parade's
    //      selection goes through RemoveShipsFromDoingStuff, the exe's reason 0x65)
    for (node = comlayer->todolist.head; node != NULL; node = node->next)
    {
        CommandToDo *todo = (CommandToDo *)listGetStructOfNode(node);
        if ((todo->ordertype.order == COMMAND_MILITARY_PARADE) && (todo->militaryParade->aroundShip == deadship))
        {
            paradeAroundDeadShip = todo;
            break;
        }
    }
    if (paradeAroundDeadShip != NULL)
    {
        MaxSelection paradeShips;
        cat_selSelectionCopy(&paradeShips,(MaxSelection *)paradeAroundDeadShip->selection);
        RemoveShipsFromDoingStuff(comlayer,(SelectCommand *)&paradeShips,0x65);
    }

    selection.numShips = 1;
    selection.ShipPtr[0] = deadship;

    RemoveShipsFromDoingStuff(comlayer,&selection,0xe);

    RemoveShipFromBeingTargeted(comlayer,deadship,REMOVE_PROTECT,NULL);   // CAT: fourth argument 0 (clShipDied 0x0044b4ee)

    // Update player unit caps info
    unitCapDeleteShip(deadship,deadship->playerowner);

    // CAT: no research ship / rmDeactivateLab: a Sect ship closes the lab of its module type (cat_rmResearchModuleDied
    //      0x004f4620), a dead bMothership its lab 0 (cat_rmMothershipLabLost 0x004f4700) (clShipDied 0x0044b4ee)
    if (deadship->shiprace == RACE_Sect)
    {
        cat_rmResearchModuleDied(deadship->playerowner, deadship->shiptype);
    }
    else if (deadship->shiptype == bMothership)
    {
        cat_rmMothershipLabLost(deadship->playerowner);
    }
}

/*-----------------------------------------------------------------------------
    Name        : TargetWithinAttackRange
    Description : returns TRUE if ship within attack range of target
    Inputs      : ship,target
    Outputs     :
    Return      : returns TRUE if ship within attack range of target
----------------------------------------------------------------------------*/
bool32 TargetWithinAttackRange(Ship *ship,SpaceObjRotImpTarg *target)
{
    vector trajectory;
    real32 range;

    aishipGetTrajectory(ship,target,&trajectory);
    range = RangeToTarget(ship,target,&trajectory);

    if (range < 10000.0f)
    {
        return TRUE;        // consider within attack range sooner
    }

    if (range < ((ShipStaticInfo *)(ship->staticinfo))->bulletRange[ship->tacticstype])
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

void putFightersInSelection(MaxSelection *dst,MaxSelection *src)
{
    sdword i,k;
    k=0;
    for(i=0;i<src->numShips;i++)
    {
        if(src->ShipPtr[i]->staticinfo->shipclass == CLASS_Fighter)
        {
            dst->ShipPtr[k] = src->ShipPtr[i];
            k++;
        }
    }
    dst->numShips = k;
}
void putCorvettesInSelection(MaxSelection *dst,MaxSelection *src)
{
    sdword i,k;
    k=0;
    for(i=0;i<src->numShips;i++)
    {
        if(src->ShipPtr[i]->staticinfo->shipclass == CLASS_Corvette)
        {
            dst->ShipPtr[k] = src->ShipPtr[i];
            k++;
        }
    }
    dst->numShips = k;
}
void putCapitalsInSelection(MaxSelection *dst,MaxSelection *src)
{
    sdword i,k;
    k=0;
    for(i=0;i<src->numShips;i++)
    {
        if(src->ShipPtr[i]->staticinfo->shipclass != CLASS_Fighter
           && src->ShipPtr[i]->staticinfo->shipclass != CLASS_Corvette)
        {
            dst->ShipPtr[k] = src->ShipPtr[i];
            k++;
        }
    }
    dst->numShips = k;
}



/*-----------------------------------------------------------------------------
    Name        : clHoldingPattern
    Description : Puts command into a holding pattern by
                  breaking into small formations n' such.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
#define MAX_UNORDERED_GROUP_SIZE    11  //maximum ships in a group after
                                        //attacking {ships ungrouped previously)

void clHoldingPattern(CommandLayer *comlayer,CommandToDo *command)
{
    bool32 needToPassiveAttack = FALSE;
    bool32 needToProtect = FALSE;
    MaxSelection passiveAttackSelection;
    MaxSelection protectSelection;
    MaxSelection fighters,corvettes,capitals;
    MaxSelection working;
    MaxSelection *curselection;
    sdword i,j,k;

    if (command->ordertype.attributes & COMMAND_MASK_FORMATION)
    {
        return;         // already in formation, don't need to split them up into deltas
    }

    if(command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        if(command->attack->numTargets > 0)
        {
            //if group was passive attacking, set a flag so we remember that that was the case
            needToPassiveAttack = TRUE;
            //store the target list acordingly
            selSelectionCopy((MaxAnySelection *)&passiveAttackSelection,(MaxAnySelection *)command->attack);
        }
    }
    if(command->ordertype.attributes & COMMAND_MASK_PROTECTING)
    {
        if(command->protect->numShips > 0)
        {
            //group was protecting a ship, set the protection list and
            //set a flag accordingly
            selSelectionCopy((MaxAnySelection *)&protectSelection,(MaxAnySelection *)command->protect);
            needToProtect = TRUE;
        }
    }

    putFightersInSelection(&fighters,(MaxSelection *)command->selection);
    putCorvettesInSelection(&corvettes,(MaxSelection *)command->selection);
    putCapitalsInSelection(&capitals,(MaxSelection *)command->selection);

    // want to make sure all ships go into either fighters, corvettes, or frigates
    dbgAssertOrIgnore(fighters.numShips + corvettes.numShips + capitals.numShips == command->selection->numShips);

    //loop through all types of ships {capitals,fighters and corvetters}
    for(j = 0; j < 3; j++)
    {
        working.numShips = 0;
        if(j == 0)
            curselection = &fighters;
        else if (j==1)
            curselection = &corvettes;
        else
            curselection = &capitals;
        k =0;

        //loop through the current selection and divide type into groups
        //of MAX_UNORDERED_GROUP_SIZE
        for(i=0;i<curselection->numShips;i++)
        {
            working.ShipPtr[k] = curselection->ShipPtr[i];
            k++;
            if(k == MAX_UNORDERED_GROUP_SIZE)
            {
                //maximum # reached for this group
                //set their formation,targets,and protection flags accordingly
                working.numShips = k;
                clFormation(comlayer,(SelectCommand *)&working,DELTA_FORMATION,0);   // CAT: level 0 (clHoldingPattern 0x0044b6b4) [audit: M2-R5 command-tactics]

                //if group was previously passiveattacking or
                //protecting set them to doing the same thing again with the
                //same target list
                if(needToPassiveAttack)
                    clPassiveAttack(comlayer,(SelectCommand *)&working,(SelectAnyCommand *)&passiveAttackSelection);
                if(needToProtect)
                    clProtect(comlayer,(SelectCommand *)&working,(SelectCommand *)&protectSelection);
                working.numShips = 0;
                k = 0;
            }
        }
        //done looping through groups
        working.numShips = k;
        if(k > 0)
        {
            //set remaining ships to formation + other tasks
            clFormation(comlayer,(SelectCommand *)&working,DELTA_FORMATION,0);   // CAT: level 0 (clHoldingPattern 0x0044b718) [audit: M2-R5 command-tactics]
            //if group was previously passiveattacking or
            //protecting set them to doing the same thing again with the
            //same target list
            if(needToPassiveAttack)
                clPassiveAttack(comlayer,(SelectCommand *)&working,(SelectAnyCommand *)&passiveAttackSelection);
            if(needToProtect)
                clProtect(comlayer,(SelectCommand *)&working,(SelectCommand *)&protectSelection);
            working.numShips = 0;
            k = 0;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : salCapCorvettesInSelectionCantDoFormation
    Description : returns true if salcaps in selection can't dealwith
                  a formation at the present time
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 salCapCorvettesInSelectionCantDoFormation(SelectCommand *selection)
{
    sdword i;

    for(i=0;i<selection->numShips;i++)
    {
        // CAT: any Cataclysm salvager (cat_shipIsSalvager 0x004f8f60), as clProcess 0x0044c448 tests it inline; HW1's
        //      SalCapCorvette type is a dormant id that no ship has [audit: M2-R4 combat left this HW1 type test]
        if(cat_shipIsSalvager(selection->ShipPtr[i]))
        {
            if(selection->ShipPtr[i]->shipOrDerelict.specialFlags & SPECIAL_BrokenFormation)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}
/*-----------------------------------------------------------------------------
    Name        : processHoldingPattern
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 processHoldingPattern(CommandToDo *command,SelectCommand *selection)
{
    sdword i,count;
    vector destination,heading,right;

    if(selection->numShips == 0)
        return TRUE;

    //flight forward for 2 secs...then turn
    if((universe.totaltimeelapsed - command->holdingStartTime) > TW_HOLDING_PATTERN_PATHTIME)

    {
        //turn right
        command->holdingStartTime = universe.totaltimeelapsed;
        matGetVectFromMatrixCol2(right,selection->ShipPtr[0]->rotatable.rotinfo.coordsys);
        matGetVectFromMatrixCol3(heading,selection->ShipPtr[0]->rotatable.rotinfo.coordsys);
        vecScalarMultiply(right,right,TW_HOLDING_PATTERN_RIGHT_COEFFICIENT);
        vecScalarMultiply(heading,heading,TW_HOLDING_PATTERN_HEADING_COEFFICIENT);
        vecAdd(command->holdingPatternVec,right,heading);
        vecScalarMultiply(command->holdingPatternVec,command->holdingPatternVec,TW_HOLDING_PATTERN_PATH_DISTANCE);

        command->turnAroundTimes++;
        if(command->turnAroundTimes > TW_HOLDING_PATTERN_NUM_TURNS)
            return TRUE;
    }
    count = selection->numShips;
    if(command->ordertype.attributes & COMMAND_MASK_FORMATION)
        count = 1;  //only process leader if command is in formation!
    for(i=0;i<count;i++)
    {
        vecAdd(destination,command->holdingPatternVec,selection->ShipPtr[i]->holdingPatternPos);
        aishipFlyToPointAvoidingObjs(selection->ShipPtr[i],&destination,AISHIP_PointInDirectionFlying|AISHIP_FastAsPossible,TW_HOLDING_PATTERN_PATH_MAX_SPEED);
    }
    return FALSE;
}


/*-----------------------------------------------------------------------------
    Name        : clProcess
    Description : processes the command layer (executes all commands)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: an attack that ends: a looping or patrol move interrupted by the attack resumes (clProcess 0x0044c4f1: a move
//      to the saved path's first point from moveFrom, then the saved path itself); an attack-move goes on to its
//      destination; otherwise the attack order is freed
static void clAttackFinished(CommandToDo *command,bool32 checkAIState)
{
    Ship *ship = command->selection->ShipPtr[0];
    vector to;

    if (command->catSavedMoveValid)
    {
        to = command->catSavedMove.points[0];
        ChangeOrderToMove(command,CAT_MOVEPATH_ONCE,ship->moveFrom,1,&to);
        command->move = command->catSavedMove;
        command->catSavedMoveValid = FALSE;
        return;
    }
    if ((command->ordertype.attributes & COMMAND_MASK_ATTACKING_AND_MOVING) && (!checkAIState || (ship->aistatecommand == 0)))
    {
        to = ship->moveTo;
        bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_ATTACKMOVECANCEL);
        ChangeOrderToMove(command,CAT_MOVEPATH_ONCE,ship->moveFrom,1,&to);
        return;
    }
    // free COMMAND_ATTACK stuff
    FreeLastOrder(command);
    // attack has finished so convert it back to a formation
    command->ordertype.order = COMMAND_NULL;
}

// CAT: an attack-move still under way turns into its move when the targets are far (the woodpecker check of clProcess
//      0x0044bbb0 / 0x0044c592): the ships go on unless they have arrived, in which case a formation halts
static void clAttackMoveCheck(CommandToDo *command,bool32 haltIfArrived)
{
    Ship *ship;
    vector to;

    if ((command->ordertype.attributes & COMMAND_MASK_ATTACKING_AND_MOVING) &&
        UNIVERSE_WOODPECKER(CHECK_ATTACKMOVE_TO_MOVE_RATE, CHECK_ATTACKMOVE_TO_MOVE_FRAME) &&
        (getRangeToClosestTarget(command) >= tacticsInfo.movingAttackTurnsIntoMoveCommandDistanceSqr))
    {
        ship = command->selection->ShipPtr[0];
        if (!haltIfArrived || (ship->aistatecommand == 0))
        {
            //it is at this distance that the group of ships turns and hitails it out of there...
            to = ship->moveTo;
            bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_ATTACKMOVECANCEL);
            ChangeOrderToMove(command,CAT_MOVEPATH_ONCE,ship->moveFrom,1,&to);
        }
        else
        {
            ChangeOrderToHalt(command);
        }
    }
}

// CAT: a move has arrived: the local player's mothership reports it (speech 0x400101a, clProcess 0x0044c6c7 / 0x0044bf77,
//      where HW1 used COMM_F_MoShip_Arrived), and a retreat that ends there is reported once (battle chatter 0x2e,
//      BCE_STAT_Grp_RetreatSuccessful; Ship.catRetreating is cleared)
static void clMoveArrived(CommandToDo *command,bool32 allShips)
{
    Ship *mothership = universe.curPlayerPtr->PlayerMothership;
    bool32 said = FALSE;
    sdword i;

    if ((mothership != NULL) && (mothership->staticinfo->shipclass == CLASS_Mothership) &&
        selShipInSelection(command->selection->ShipPtr, command->selection->numShips, mothership))
    {
        speechEventFleet(0x400101a, -1, command->selection->ShipPtr[0]->playerowner->playerIndex);
    }
    for (i = 0; i < (allShips ? command->selection->numShips : 1); i++)
    {
        Ship *ship = command->selection->ShipPtr[i];
        if (!said && ship->shipOrDerelict.catRetreating)
        {
            battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_STAT_Grp_RetreatSuccessful, SOUND_EVENT_DEFAULT, NULL);
            said = TRUE;
        }
        ship->shipOrDerelict.catRetreating = FALSE;
    }
}

// CAT: clProcess 0x0044b950 follows the exe case by case. Besides the waypoint moves (processMoveToDo per ship, the
//      leader of a formation at the formation's speed), the saved looping move, the force-field sphere around a
//      protected ship, the retreat chatter and the big gun's halt exemption, it runs Cataclysm's orders: link (11) and
//      unlink (12) until done, the point order (14, the big gun) and the mimic (15) and research (16) orders once.
//      In multiplayer a dead ship (SOF_Dead) stops its command too. The finished commands are freed with the exe's
//      codes (FreeCommandToDoContents' second argument).
void clProcess(CommandLayer *comlayer)
{
    Node *curnode = comlayer->todolist.head;
    CommandToDo *command;
    Node *nextnode;
    bool32 passiveAttacked;
    bool32 done;
    udword freeCode;

    Ship *ship;
    SpaceObjRotImpTarg *attacktarget;
    sdword i;
    SelectCommand *commandselection;

#ifdef HW_BUILD_FOR_DEBUGGING
    flightmanTest();
#endif

    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);

        commandselection = command->selection;

        for (i=0;i<commandselection->numShips;i++)
        {
            ship = commandselection->ShipPtr[i];
            // CAT: multiplayer stops at Crazy or Dead ships (0x200800), single player at Crazy or hyperspace (0x2200000)
            if (ship->header.flags & (singlePlayerGame ? (SOF_Hyperspace|SOF_Crazy) : (SOF_Crazy|SOF_Dead)))
            {
                goto nextcommand;
            }
            ship->autostabilizeship = FALSE; // don't autostabilize ship if ship has a command to do
        }

        if (command->ordertype.attributes & COMMAND_MASK_FORMATION)
        {
            switch (command->ordertype.order)
            {
                case COMMAND_ATTACK:
                    ship = command->selection->ShipPtr[0];
                    attacktarget = command->attack->TargetPtr[0];
                    if ((command->formation.doneInitialAttack) || (command->attack->numTargets == 0) || (TargetWithinAttackRange(ship,attacktarget)))
                    {
                        command->formation.doneInitialAttack = TRUE;
                        if(!tacticsInfo.holdFormationDuringBattle[ship->tacticstype] || command->selection->numShips == 1)
                        {
                            done = processAttackToDo(command);   //evasive tactics, so attack as per olden times
                        }
                        else
                        {
                            done = processAttackToDoInFormation(command);  // neutral or aggressive leader tactics
                        }
                        if (done)
                        {
                            clAttackFinished(command,TRUE);
                        }
                        else
                        {
                            clAttackMoveCheck(command,TRUE);
                        }
                    }
                    else
                    {
                        aishipFlyToShipAvoidingObjs(ship,attacktarget,AISHIP_PointInDirectionFlying + AISHIP_CarTurn,
                                                    command->formation.travelvel);
                        ship->shipidle = FALSE;
                        processFormationToDo(command,FALSE,FALSE);
                    }
                    break;

                case COMMAND_DOCK:
                    // CAT (M2-R5 dock): clProcess 0x0044b950 steps the dock command at once (no HW1 fly-in-formation
                    //      pre-flight on allNearShip / DOCK_INSTANTANEOUSLY; a formation flies in through the per-ship
                    //      state machine)
                    if (processDockToDo(command))
                    {
                        if (command->selection->numShips == 0)
                        {
                            // All ships have been removed from docking.  Therefore totally wipe out command
                            freeCode = 0x1f;
                            goto freecommand;
                        }
                        FreeLastOrder(command);  // free COMMAND_DOCK
                        command->ordertype.order = COMMAND_NULL;
                    }
                    break;

                case COMMAND_HALT:
                    passiveAttacked = FALSE;
                    if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                    {
                        if (processPassiveAttackToDo(command))
                        {
                            ClearPassiveAttacking(command);
                        }
                        passiveAttacked = TRUE;
                        bitClear(command->ordertype.attributes,COMMAND_MASK_HOLDING_PATTERN);
                    }

                    ship = command->selection->ShipPtr[0];
                    if ((passiveAttacked) & (bool32)((ShipStaticInfo *)ship->staticinfo)->rotateToRetaliate)
                    {
                        done = aitrackSteadyShipDriftOnly(ship);
                    }
                    else
                    {
                        done = aitrackSteadyShip(ship);
                    }
                    processFormationToDo(command,TRUE,passiveAttacked);

                    if (done)
                    {
                        FreeLastOrder(command);
                        command->ordertype.order = COMMAND_NULL;
                    }
                    break;

                case COMMAND_NULL:
                    passiveAttacked = FALSE;
                    if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                    {
                        if (processPassiveAttackToDo(command))
                        {
                            ClearPassiveAttacking(command);
                        }
                        passiveAttacked = TRUE;
                        bitClear(command->ordertype.attributes,COMMAND_MASK_HOLDING_PATTERN);
                    }
                    if (command->ordertype.attributes & COMMAND_MASK_PROTECTING)
                    {
                        // CAT: a sphere or a force-field sphere (0x0044ba5b) of more than one ship surrounds the
                        //      protected ship
                        if (((command->formation.formationtype == SPHERE_FORMATION) || (command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION)) &&
                            (command->selection->numShips > 1))
                        {
                            processFormationToDo(command,FALSE,passiveAttacked);
                        }
                        else
                        {
                            if (command->protect->numShips > 1)
                            {
                                protectShipsAvg(command->selection->ShipPtr[0],command->protect,passiveAttacked);
                                processFormationToDo(command,FALSE,passiveAttacked);
                            }
                            else
                            {
                                protectShip(command->selection->ShipPtr[0],command->protect->ShipPtr[0],passiveAttacked);
                                processFormationToDo(command,!command->protect->ShipPtr[0]->obj.posinfo.isMoving,passiveAttacked);
                            }
                        }
                    }
                    else
                    {
                        if(command->ordertype.attributes & COMMAND_MASK_HOLDING_PATTERN)
                        {
                            if(processHoldingPattern(command,command->selection))
                            {
                                bitClear(command->ordertype.attributes,COMMAND_MASK_HOLDING_PATTERN);
                            }
                        }
                        else
                        {
                            ship = command->selection->ShipPtr[0];
                            if ((passiveAttacked) & (bool32)((ShipStaticInfo *)ship->staticinfo)->rotateToRetaliate)
                            {
                                aitrackSteadyShipDriftOnly(ship);
                            }
                            else
                            {
                                aitrackSteadyShip(ship);
                            }
                        }
                        if(passiveAttacked)
                            processFormationToDo(command,FALSE,passiveAttacked);
                        else
                            processFormationToDo(command,bitTest(command->ordertype.attributes,COMMAND_MASK_HOLDING_PATTERN) ? FALSE : TRUE,passiveAttacked);
                    }
                    break;
                case COMMAND_MP_HYPERSPACING:
                    if(processMpHyperspaceingToDo(command))
                    {
                        FreeLastOrder(command); // free COMMAND_MP_HYPERSPACING stuff
                        command->ordertype.order = COMMAND_NULL;
                    }
                    break;
                case COMMAND_MOVE:
                    if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                    {
                        if (processPassiveAttackToDo(command))
                        {
                            ClearPassiveAttacking(command);
                        }
                        bitClear(command->ordertype.attributes,COMMAND_MASK_HOLDING_PATTERN);
                        //don't set passiveAttacked: it controls whether or not a ship faces in movement direction
                    }

                    // CAT: the leader along the path at the formation's speed (processMoveToDo(command,0,0,1))
                    if (processMoveToDo(command,FALSE,0,TRUE))
                    {
                        // reached destination, so change COMMAND_MOVE | COMMAND_MASK_FORMATION to COMMAND_NULL | COMMAND_MASK_FORMATION
                        clMoveArrived(command,FALSE);
                        FreeLastOrder(command); // free COMMAND_MOVE stuff
                        command->ordertype.order = COMMAND_NULL;
                    }
                    processFormationToDo(command,FALSE,FALSE);
                    break;

                case COMMAND_SPECIAL:
                    if (processSpecialToDo(command))
                    {
                        FreeLastOrder(command);  // free COMMAND_SPECIAL
                        command->ordertype.order = COMMAND_NULL;
                    }
                    if(salCapCorvettesInSelectionCantDoFormation(command->selection))
                    {
                        processFormationToDo(command,FALSE,FALSE);
                    }
                    break;

                case COMMAND_CAT_LINK:
                    if (cat_processLinkToDo(command))
                    {
                        FreeLastOrder(command);
                        command->ordertype.order = COMMAND_NULL;
                    }
                    break;

                case COMMAND_CAT_UNLINK:
                    if (cat_processUnlinkToDo(command))
                    {
                        FreeLastOrder(command);
                        command->ordertype.order = COMMAND_NULL;
                    }
                    break;

                default:
                    // CAT: the exe's formation switch has no other case (no assertion)
                    break;
            }
        }
        else
        {
            switch (command->ordertype.order)
            {
                case COMMAND_ATTACK:
                    //open guns if in range or if we need to
                    if (processAttackToDo(command))
                    {
                        clAttackFinished(command,FALSE);
                    }
                    else
                    {
                        clAttackMoveCheck(command,FALSE);
                    }
                    break;
                case COMMAND_MP_HYPERSPACING:
                    if(processMpHyperspaceingToDo(command))
                    {
                        FreeLastOrder(command); // free COMMAND_MOVE stuff
                        command->ordertype.order = COMMAND_NULL;
                    }
                    break;

                case COMMAND_MOVE:
                    if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                    {
                        if (processPassiveAttackToDo(command))
                        {
                            ClearPassiveAttacking(command);
                        }
                        bitClear(command->ordertype.attributes,COMMAND_MASK_HOLDING_PATTERN);
                    }

                    // CAT: every ship along its path (0x0044c6a0)
                    done = TRUE;
                    for (i = 0; i < command->selection->numShips; i++)
                    {
                        done &= processMoveToDo(command,FALSE,i,FALSE);
                    }
                    if (done)
                    {
                        if (command->ordertype.attributes & (COMMAND_MASK_PROTECTING|COMMAND_MASK_PASSIVE_ATTACKING))
                        {
                            clMoveArrived(command,FALSE);
                            FreeLastOrder(command);   // free COMMAND_MOVE stuff
                            command->ordertype.order = COMMAND_NULL;
                        }
                        else
                        {
                            clMoveArrived(command,TRUE);
                            freeCode = 0x1f;
                            goto freecommand;
                        }
                    }
                    break;

                case COMMAND_DOCK:
                    if (processDockToDo(command))
                    {
                        if (command->dock.wasHarvesting)
                        {
                            // changing COMMAND_DOCK to COMMAND_COLLECT_RESOURCES
                            FreeLastOrder(command);
                            ChangeSingleShipToCollectResource(command,NULL);   // CAT: clProcess 0x0044b950 passes NULL
                        }
                        else
                        {
                            freeCode = 0x1d;        // dock command has finished
                            goto freecommand;
                        }
                    }
                    break;

                case COMMAND_SPECIAL:
                    if (processSpecialToDo(command))
                    {
                        freeCode = 0x1d;            // special command has finished
                        goto freecommand;
                    }
                    break;

                case COMMAND_COLLECT_RESOURCES:
                    if (processCollectResource(command))
                    {
                        ship = command->selection->ShipPtr[0];

                        nextnode = curnode->next;         // collect resource command has finished
                        FreeCommandToDoContents(command,0x15);
                        listDeleteNode(curnode);
                        curnode = nextnode;

                        if ((ship->playerowner) && (ship->playerowner->PlayerMothership))
                        {
                            GroupShipIntoMilitaryParade(&universe.mainCommandLayer,ship,ship->playerowner->PlayerMothership);
                        }

                        goto readyfornextnode;
                    }
                    break;

                case COMMAND_LAUNCH_SHIP:
                    if (processLaunchShipToDo(command))
                    {
                        Ship *receiverShip;

                        ship = command->selection->ShipPtr[0];
                        receiverShip = command->launchship.receiverShip;

                        nextnode = curnode->next;
                        FreeCommandToDoContents(command,0x16);
                        listDeleteNode(curnode);
                        curnode = nextnode;

                        GroupShip(comlayer,ship,receiverShip);

                        aiplayerShipLaunchedCallback(ship);

                        goto readyfornextnode;
                    }
                    break;

                case COMMAND_HALT:
                    passiveAttacked = FALSE;
                    if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                    {
                        if (processPassiveAttackToDo(command))
                        {
                            ClearPassiveAttacking(command);
                        }
                        passiveAttacked = TRUE;
                        bitClear(command->ordertype.attributes,COMMAND_MASK_HOLDING_PATTERN);
                    }

                    if (processHaltToDo(command,passiveAttacked))
                    {
                        if (command->ordertype.attributes)
                        {
                            FreeLastOrder(command);
                            command->ordertype.order = COMMAND_NULL;
                        }
                        else
                        {
                            freeCode = 0x1f;
                            goto freecommand;
                        }
                    }
                    break;

                case COMMAND_MILITARY_PARADE:
                    passiveAttacked = FALSE;
                    if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                    {
                        if (processPassiveAttackToDo(command))
                        {
                            ClearPassiveAttacking(command);
                        }
                        passiveAttacked = TRUE;
                        bitClear(command->ordertype.attributes,COMMAND_MASK_HOLDING_PATTERN);
                    }
                    processMilitaryParadeToDo(command,passiveAttacked);
                    break;

                case COMMAND_BUILDING_SHIP:
                    // don't allow COMMAND_MASK_PASSIVE_ATTACKING, COMMAND_MASK_PROTECTING, COMMAND_MASK_FORMATION
                    if (processBuildingShipToDo(command))
                    {
                        freeCode = 0x1e;
                        goto freecommand;
                    }
                    break;

                case COMMAND_NULL:
                    if (command->ordertype.attributes == 0)
                    {
                        // NULL command, and no attributes, so let's remove it from the command layer
                        freeCode = 0x1f;
                        goto freecommand;
                    }
                    if(command->ordertype.attributes & COMMAND_MASK_HOLDING_PATTERN)
                    {
                        //put ships in holding pattern...maintain all relevant info though (protecting and passiveattacking!)
                        nextnode = curnode->next;

                        //this function will result in command being turned into a formation flying a holding pattern
                        clHoldingPattern(comlayer,command);
                        curnode = nextnode;
                        goto readyfornextnode;
                    }

                    if (command->ordertype.attributes & COMMAND_MASK_PROTECTING)
                    {
                        ProtectCommand *commandprotect = command->protect;
                        SelectCommand *commandselect = command->selection;
                        sdword numShipsToProtect = commandprotect->numShips;
                        sdword numShips = commandselect->numShips;
                        Ship *protectShipPtr;
                        sdword protectThisShip;

                        passiveAttacked = FALSE;
                        if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                        {
                            if (processPassiveAttackToDo(command))
                            {
                                ClearPassiveAttacking(command);
                            }
                            passiveAttacked = TRUE;
                            bitClear(command->ordertype.attributes,COMMAND_MASK_HOLDING_PATTERN);
                        }

                        if (numShipsToProtect == 1)
                        {
                            protectShipPtr = commandprotect->ShipPtr[0];
                            for (i=0;i<numShips;i++)
                            {
                                protectShip(commandselect->ShipPtr[i],protectShipPtr,passiveAttacked);
                            }
                        }
                        else
                        {
                            if (numShips == 1)
                            {
                                // we only have 1 ship, and > 1 ships to protect, so try to stay in middle
                                protectShipsAvg(commandselect->ShipPtr[0],commandprotect,passiveAttacked);
                            }
                            else
                            {
                                protectThisShip = 0;

                                for (i=0;i<numShips;i++)
                                {
                                    protectShip(commandselect->ShipPtr[i],commandprotect->ShipPtr[protectThisShip],passiveAttacked);

                                    protectThisShip++;
                                    if (protectThisShip >= numShipsToProtect)
                                    {
                                        protectThisShip = 0;
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
                        {
                            if (processPassiveAttackToDo(command))
                            {
                                ClearPassiveAttacking(command);
                            }
                            bitClear(command->ordertype.attributes,COMMAND_MASK_HOLDING_PATTERN);

                            ship = command->selection->ShipPtr[0];
                            if ((bool32)((ShipStaticInfo *)ship->staticinfo)->rotateToRetaliate)
                            {
                                if(ship->tacticstype != Evasive)
                                    aitrackSteadyShipDriftOnly(ship);
                            }
                            else if (!cat_mothershipBigGunActive(ship))
                            {
                                // CAT: a mothership aiming its big gun is not steadied (0x0044c3a8)
                                aitrackSteadyShip(ship);
                            }
                        }
                    }
                    break;

                case COMMAND_CAT_LINK:
                    if (cat_processLinkToDo(command))
                    {
                        freeCode = 0x1d;
                        goto freecommand;
                    }
                    break;

                case COMMAND_CAT_UNLINK:
                    if (cat_processUnlinkToDo(command))
                    {
                        freeCode = 0x1d;
                        goto freecommand;
                    }
                    break;

                case COMMAND_CAT_POINT:
                    // CAT: the big gun's point order fires once (cat_bigGunProcessFireCommand 0x005bbe80)
                    cat_bigGunProcessFireCommand(command);
                    freeCode = 0x19;
                    goto freecommand;

                case COMMAND_CAT_MIMIC:
                    // CAT: every ship takes the disguise once (cat_mimicCommand 0x005b8d70)
                    for (i = 0; i < command->selection->numShips; i++)
                    {
                        cat_mimicCommand(command->selection->ShipPtr[i],(sdword)command->catMimic.mode,(sdword)command->catMimic.shipType,
                                         (sdword)command->catMimic.player,(sdword)command->catMimic.index);
                    }
                    freeCode = 0x1f;
                    goto freecommand;

                case COMMAND_CAT_RESEARCH:
                    // CAT: the research order starts or stops one research slot (cat_processResearchToDo 0x0044dc20)
                    cat_processResearchToDo(command);
                    freeCode = 0x1b;
                    goto freecommand;

                default:
                    // CAT: no other order reaches the exe's switch (no assertion)
                    break;
            }

        }
nextcommand:
        curnode = curnode->next;
readyfornextnode:
        continue;

freecommand:
        nextnode = curnode->next;
        FreeCommandToDoContents(command,(sdword)freeCode);
        listDeleteNode(curnode);
        curnode = nextnode;
    }
}

/*-----------------------------------------------------------------------------
    Name        : clPostProcess
    Description : processes all of the command layer (2nd pass), for actions such as protecting
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clPostProcess(CommandLayer *comlayer)
{
    Node *curnode = comlayer->todolist.head;
    Node *endnode = comlayer->todolist.tail;
    CommandToDo *command;
    Node *nextnode;
    AttackCommand *attack;
    ShipPtr protectMe;
    sdword i;

    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);
        nextnode = curnode->next;

        if (command->ordertype.attributes & COMMAND_MASK_PROTECTING)
        {
            dbgAssertOrIgnore(command->protect->numShips > 0);

            if (bitTest(command->protectFlags,PROTECTFLAGS_JUST_FOLLOW))
            {
                goto dontretaliate;
            }

            for (i=0;i<command->protect->numShips;i++)
            {
                protectMe = command->protect->ShipPtr[i];

                if ((protectMe->gettingrocked != NULL) && (!allianceIsShipAlly(command->selection->ShipPtr[0],protectMe->gettingrocked->playerowner)))
                {
                    if ((command->ordertype.order == COMMAND_NULL) || (command->ordertype.order == COMMAND_MOVE) || (command->ordertype.order == COMMAND_ATTACK))
                    {
                        // CAT: a force-field sphere too (clPostProcess 0x0044cfef)
                        if ((command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
                            ((command->formation.formationtype == SPHERE_FORMATION) ||
                             (command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION)))
                        {
                            if (i == 0)     // 0th ship is always protected by a surrounded sphere of ships, and these ships
                            {               // should stay nearby (not attack, just passive attack).
                                goto dontretaliate;
                            }
                        }

                       /*
                       if (AreAllShipsAttackCapable(command->selection))
                        {
                            attack = (AttackCommand *)getShipAndItsFormation(&universe.mainCommandLayer,protectMe->gettingrocked);
                            //filter gravwell and other related ships for attack during guarding
                            makeShipsNotHaveNonCombatShipsForGuardAttack(command->selection);
                            //ChangeOrderToAttack(command,attack);
                            clAttack(&universe.mainCommandLayer,command->selection,attack);
                            memFree(attack);
                        }
                        else
                        */
                          {

//                            if (AreAnyShipsAttackCapable(command->selection))
                            MaxSelection attackingships;
                            if (MakeShipsAttackCapable((SelectCommand *)&attackingships, command->selection, FALSE))   // CAT: skip-disabled argument (clAttack 0x00448950 and the other game-code callers pass 0)
                            {
                                // just tell the ships that are attack capable to attack
                                CommandToDo *newcommand;
//                                SelectCommand *attackingships = selectDupSelection(command->selection);
                                attack = (AttackCommand *)getShipAndItsFormation(&universe.mainCommandLayer,protectMe->gettingrocked);
//                                MakeShipsAttackCapable(attackingships);
                                dbgAssertOrIgnore(attackingships.numShips > 0);
                                makeShipsNotHaveNonCombatShipsForGuardAttack((SelectCommand *)&attackingships);
                                if(attackingships.numShips > 0)
                                {
                                    newcommand = clProtect(comlayer,(SelectCommand *)&attackingships,command->protect);
                                    //filter gravwell and other related ships for attack during guarding
                                    ChangeOrderToAttack(newcommand,attack,3);   // CAT: reason 3 (clPostProcess 0x0044d0ff)
                                }
                                memFree(attack);
//                                memFree(attackingships);
                            }
                        }
                        goto dontretaliate;
                    }
                }

            }
        }
dontretaliate:

        if (curnode == endnode)
        {
            break;
        }
        curnode = nextnode;
    }
}

/*-----------------------------------------------------------------------------
    Name        : getShipAndItsCommand
    Description : Returns allocated command containing the ship
    Inputs      :
    Outputs     :
    Return      : Returns allocated command containing the ship
----------------------------------------------------------------------------*/
CommandToDo *getShipAndItsCommand(CommandLayer *comlayer,ShipPtr ship)
{
    return ship->command;
}

/*-----------------------------------------------------------------------------
    Name        : getShipAndItsCommandSelection
    Description : Returns selection for allocated command containing the ship.
                  If there's no command for the ship, it'll return the formation
                  of the ship.
    Inputs      :
    Outputs     :
    Return      : Returns allocated command containing the ship
----------------------------------------------------------------------------*/
SelectCommand *getShipAndItsCommandSelection(CommandLayer *comlayer,ShipPtr ship,bool32 *parade)
{
    CommandToDo *command = getShipAndItsCommand(comlayer,ship);
    SelectCommand *returnselect;
    udword sizeofselect;

    if (command != NULL)
    {
        if (command->ordertype.order == COMMAND_MILITARY_PARADE)
        {
            *parade = TRUE;
        }
        else
        {
            *parade = FALSE;
        }
        sizeofselect = sizeofSelectCommand(command->selection->numShips);
        returnselect = memAlloc(sizeofselect,"commandselection",Pyrophoric);
        memcpy(returnselect,command->selection,sizeofselect);
    }
    else
    {
        //no command found containing this ship; return just the single ship
        returnselect = memAlloc(sizeofSelectCommand(1),"sf(shipandform)", Pyrophoric);
        returnselect->numShips = 1;
        returnselect->ShipPtr[0] = ship;
        *parade = FALSE;
    }
    return returnselect;
}

/*-----------------------------------------------------------------------------
    Name        : GetMilitaryGroupAroundShip
    Description : gets the
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
CommandToDo *GetMilitaryGroupAroundShip(CommandLayer *comlayer,Ship *aroundShip)
{
    Node *curnode = comlayer->todolist.head;
    CommandToDo *command;

    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);

        if (command->ordertype.order == COMMAND_MILITARY_PARADE)
        {
            if (command->militaryParade->aroundShip == aroundShip)
            {
                return command;
            }
        }

        curnode = curnode->next;
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
    Name        : clPresetShipsToPosition
    Description : sets ships to correct position according to command layer
    Inputs      : comlayer
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clPresetShipsToPosition(CommandLayer *comlayer)
{
    Node *curnode = comlayer->todolist.head;
    CommandToDo *command;

    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);

        if (command->ordertype.order == COMMAND_MILITARY_PARADE)
        {
            setMilitaryParade(command);
        }
        else if (command->ordertype.attributes & COMMAND_MASK_FORMATION)
        {
            setFormationToDo(command);
        }

        curnode = curnode->next;
    }
}

/*-----------------------------------------------------------------------------
    Name        : getShipAndItsFormationCommand
    Description : Returns allocated command containing the ship and its formation
    Inputs      :
    Outputs     :
    Return      : Returns allocated command containing the ship and its formation
----------------------------------------------------------------------------*/
// CAT: the ship's command when it is a formation (getShipAndItsFormation 0x0044d240 tests the command's formation flag)
CommandToDo *getShipAndItsFormationCommand(CommandLayer *comlayer,ShipPtr ship)
{
    return cat_shipFormationCommand(ship);
}

/*-----------------------------------------------------------------------------
    Name        : getShipAndItsFormation
    Description : Returns allocated selectcommand containing the ship and its formation
    Inputs      :
    Outputs     :
    Return      : Returns allocated selectcommand containing the ship and its formation
----------------------------------------------------------------------------*/
SelectCommand *getShipAndItsFormation(CommandLayer *comlayer,ShipPtr ship)
{
    CommandToDo *command = getShipAndItsFormationCommand(comlayer,ship);
    SelectCommand *returnselect;
    udword sizeofselect;

    if (command != NULL)
    {
        sizeofselect = sizeofSelectCommand(command->selection->numShips);
        returnselect = memAlloc(sizeofselect,"sf(shipandform)",Pyrophoric);
        memcpy(returnselect,command->selection,sizeofselect);
    }
    else
    {
        returnselect = memAlloc(sizeofSelectCommand(1),"sf(shipandform)", Pyrophoric);
        returnselect->numShips = 1;
        returnselect->ShipPtr[0] = ship;
    }

    return returnselect;
}

/*-----------------------------------------------------------------------------
    Name        : CommandInCommandLayer
    Description : returns TRUE if searchfor is in comlayer
    Inputs      : comlayer, searchfor
    Outputs     :
    Return      : returns TRUE if searchfor is in comlayer
----------------------------------------------------------------------------*/
bool32 CommandInCommandLayer(CommandLayer *comlayer,CommandToDo *searchfor)
{
    Node *curnode = comlayer->todolist.head;
    CommandToDo *command;

    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);
        if (command == searchfor)
        {
            return TRUE;
        }

        curnode = curnode->next;
    }

    return FALSE;
}

/*=============================================================================
    CAT (M2-R5 command-tactics): Cataclysm's own orders and command-layer helpers (cataclysm.exe 0x0044d2e0 ..
    0x0044e150, and the cat_cl* functions placed between the HW1 bodies above). The orders run in clProcess:
    COMMAND_CAT_LINK / UNLINK (cat_processLinkToDo / cat_processUnlinkToDo in Dock.c, next to the link flight),
    COMMAND_CAT_POINT (cat_bigGunProcessFireCommand), COMMAND_CAT_MIMIC (cat_mimicCommand), COMMAND_CAT_RESEARCH
    (cat_processResearchToDo below).
=============================================================================*/

// CAT: the link upgrade a partner needs (cat_clFindLinkPartner 0x0044d2e0 asks cat_shipHasUpgrade(ship, 8); name
//      chosen here)
#define CAT_UPGRADE_Link            0x00000008
// CAT: the static's type bit of the linked types (sACV, bACV, sMCV: cat_univInitShipTypeFlags 0x0053b8c0 sets 0x40);
//      only they can unlink (cat_clUnlink 0x0044d7a0; name chosen here)
#define CAT_TYPEFLAG_Linked         0x00000040

/*-----------------------------------------------------------------------------
    Name        : cat_clFindLinkPartner
    Description : the nearest ship of selection, other than ship and still unpaired, of ship's type that has the link
                  upgrade
    Inputs      : selection, ship
    Outputs     :
    Return      : the partner, or NULL
----------------------------------------------------------------------------*/
// CAT: new (cat_clFindLinkPartner 0x0044d2e0): walks universe.ShipList in its order, distance by fsqrt
Ship *cat_clFindLinkPartner(SelectCommand *selection,Ship *ship)
{
    Node *node;
    Ship *candidate;
    CatLinkInfo *link;
    Ship *best = NULL;
    real32 bestdist = 1e20f;
    real32 dist;
    vector diff;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        candidate = (Ship *)listGetStructOfNode(node);
        if (!cat_shipHasUpgrade(candidate,CAT_UPGRADE_Link) || !ShipInSelection(selection,candidate) ||
            (ship->shiptype != candidate->shiptype) || (candidate == ship))
        {
            continue;
        }
        link = (CatLinkInfo *)((ubyte *)candidate->ShipSpecifics + 0x18);     // the exe reads candidate+0x650
        if (link->partner != NULL)
        {
            continue;
        }
        vecSub(diff,candidate->obj.posinfo.position,ship->obj.posinfo.position);
        dist = fsqrt(vecMagnitudeSquared(diff));
        if (dist < bestdist)
        {
            bestdist = dist;
            best = candidate;
        }
    }
    return best;
}

// CAT: takes a ship out of a link command being built: no command, no formation disable reason, a Sentinel
//      unclamped (the inline removals of cat_clLink 0x0044d646 / 0x0044d686)
static void catLinkDropShip(SelectCommand *selection,sdword index)
{
    Ship *ship = selection->ShipPtr[index];

    selection->numShips--;
    selection->ShipPtr[index] = selection->ShipPtr[selection->numShips];
    ship->command = NULL;
    RemoveShipFromFormation(ship);
}

// CAT: the common head of the order creators that reuse a command whose selection is the same (cat_clLink 0x0044d3dd,
//      cat_clUnlink 0x0044d830): protecting, a formation and passive attacking are cleared inline, then the last order
static void catClearCommandForNewOrder(CommandToDo *command)
{
    if (command->ordertype.attributes & COMMAND_MASK_PROTECTING)
    {
        ClearProtecting(command);
    }
    if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        ClearPassiveAttacking(command);
    }
    if (command->ordertype.attributes & COMMAND_MASK_FORMATION)
    {
        FormationCleanup(command);
        bitClear(command->ordertype.attributes,COMMAND_MASK_FORMATION);
    }
    FreeLastOrder(command);
}

/*-----------------------------------------------------------------------------
    Name        : cat_clLink
    Description : the link order (COMMAND_CAT_LINK): pairs the linkable ships of selectcom, nearest first; the lower of
                  each pair leads, the other leaves the command and waits; unpaired ships leave it too
    Inputs      : comlayer, selectcom
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: new (cat_clLink 0x0044d3b0)
void cat_clLink(CommandLayer *comlayer,SelectCommand *selectcom)
{
    CommandToDo *command;
    SelectCommand *selection;
    sdword sizeofselection;
    sdword i,j;
    sdword paired;
    Ship *ship;
    Ship *partner;

    if (selectcom->numShips == 0)
    {
        return;
    }

    command = IsSelectionAlreadyDoingSomething(comlayer,selectcom);
    if (command != NULL)
    {
        catClearCommandForNewOrder(command);
    }
    else
    {
        RemoveShipsFromDoingStuff(comlayer,selectcom,0xf);
        command = memAlloc(sizeof(CommandToDo),"ToDo",0);
        InitCommandToDo(command);
        command->ordertype.attributes = 0;
    }

    sizeofselection = sizeofSelectCommand(selectcom->numShips);
    selection = memAlloc(sizeofselection,"ToDoSelection",0);
    memcpy(selection,selectcom,sizeofselection);
    if (command->selection != NULL)
    {
        memFree(command->selection);
    }
    command->selection = selection;
    command->ordertype.order = COMMAND_CAT_LINK;

    for (i = 0; i < selection->numShips; i++)
    {
        cat_clLinkCleanup(selection->ShipPtr[i]);
    }

    // pair the ships: the ship with the lower z of a pair leads (it stays in the command); its partner and every
    // ship left without one leave the selection
    paired = 0;
    i = 0;
    while (paired < selection->numShips)
    {
        ship = selection->ShipPtr[i];
        if (((CatLinkInfo *)((ubyte *)ship->ShipSpecifics + 0x18))->partner != NULL)
        {
            catLinkDropShip(selection,i);       // already a partner of an earlier ship
            continue;
        }
        partner = cat_clFindLinkPartner(selection,ship);
        if (partner == NULL)
        {
            catLinkDropShip(selection,i);
            continue;
        }
        if (partner->obj.posinfo.position.z < ship->obj.posinfo.position.z)
        {
            sdword partnerIndex = -1;

            for (j = 0; j < selection->numShips; j++)
            {
                if (selection->ShipPtr[j] == partner)
                {
                    partnerIndex = j;
                }
            }
            selection->ShipPtr[i] = partner;
            selection->ShipPtr[partnerIndex] = ship;
            cat_linkPairShips(partner,ship);
        }
        else
        {
            cat_linkPairShips(ship,partner);
        }
        paired++;
        i++;
    }

    PrepareShipsForCommand(command,TRUE);
    InitShipsForAI(command->selection,TRUE);

    if (command->todonode.belongto == NULL)
    {
        listAddNode(&comlayer->todolist,&command->todonode,command);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_clUnlink
    Description : the unlink order (COMMAND_CAT_UNLINK) for the linked ships of selectcom (sACV, bACV, sMCV)
    Inputs      : comlayer, selectcom (ships of other types are taken out of it)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: new (cat_clUnlink 0x0044d7a0)
void cat_clUnlink(CommandLayer *comlayer,SelectCommand *selectcom)
{
    CommandToDo *command;
    SelectCommand *selection;
    sdword sizeofselection;
    sdword i;

    if (selectcom->numShips == 0)
    {
        return;
    }
    for (i = 0; i < selectcom->numShips; )
    {
        if (!(selectcom->ShipPtr[i]->staticinfo->catTypeFlags & CAT_TYPEFLAG_Linked))
        {
            selectcom->numShips--;
            selectcom->ShipPtr[i] = selectcom->ShipPtr[selectcom->numShips];
        }
        else
        {
            i++;
        }
    }

    command = IsSelectionAlreadyDoingSomething(comlayer,selectcom);
    if (command != NULL)
    {
        catClearCommandForNewOrder(command);
    }
    else
    {
        RemoveShipsFromDoingStuff(comlayer,selectcom,0x10);
        command = memAlloc(sizeof(CommandToDo),"ToDo",0);
        InitCommandToDo(command);
        command->ordertype.attributes = 0;
    }

    sizeofselection = sizeofSelectCommand(selectcom->numShips);
    selection = memAlloc(sizeofselection,"ToDoSelection",0);
    memcpy(selection,selectcom,sizeofselection);
    if (command->selection != NULL)
    {
        memFree(command->selection);
    }
    command->selection = selection;
    command->ordertype.order = COMMAND_CAT_UNLINK;

    for (i = 0; i < selection->numShips; i++)
    {
        cat_unlinkPrepareShip(selection->ShipPtr[i]);
    }

    PrepareShipsForCommand(command,TRUE);
    InitShipsForAI(command->selection,TRUE);

    if (command->todonode.belongto == NULL)
    {
        listAddNode(&comlayer->todolist,&command->todonode,command);
    }
}

// CAT: the head the one-shot order creators share (cat_clPointOrder 0x0044dd70, cat_clMimic 0x0044da80): a new
//      command for a copy of selectcom, its ships taken off what they were doing first
static CommandToDo *catNewCommandForSelection(CommandLayer *comlayer,SelectCommand *selectcom,sdword reason,uword order)
{
    CommandToDo *command;
    SelectCommand *selection;
    sdword sizeofselection = sizeofSelectCommand(selectcom->numShips);

    command = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
    InitCommandToDo(command);
    selection = memAlloc(sizeofselection,"ToDoSelection",NonVolatile);
    memcpy(selection,selectcom,sizeofselection);
    RemoveShipsFromDoingStuff(comlayer,selection,reason);
    command->ordertype.order = order;
    command->ordertype.attributes = 0;
    command->selection = selection;
    return command;
}

/*-----------------------------------------------------------------------------
    Name        : cat_clPointOrder
    Description : the point order (COMMAND_CAT_POINT): the big gun of the selection's first ship fires at point
    Inputs      : comlayer, selectcom, point
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: new (cat_clPointOrder 0x0044dd70)
void cat_clPointOrder(CommandLayer *comlayer,SelectCommand *selectcom,vector *point)
{
    CommandToDo *command;

    if (selectcom->numShips == 0)
    {
        return;
    }
    command = catNewCommandForSelection(comlayer,selectcom,0x12,COMMAND_CAT_POINT);
    command->catPoint.target = *point;
    PrepareShipsForCommand(command,TRUE);
    listAddNode(&comlayer->todolist,&command->todonode,command);
}

/*-----------------------------------------------------------------------------
    Name        : cat_clMimic
    Description : the mimic order (COMMAND_CAT_MIMIC): every ship takes the disguise mode / shipType / player / index
    Inputs      : comlayer, selectcom, mode (MIMIC_COMMAND_*), shipType, player, index
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: new (cat_clMimic 0x0044da80)
void cat_clMimic(CommandLayer *comlayer,SelectCommand *selectcom,udword mode,udword shipType,udword player,udword index)
{
    CommandToDo *command;

    if (selectcom->numShips == 0)
    {
        return;
    }
    command = catNewCommandForSelection(comlayer,selectcom,0x11,COMMAND_CAT_MIMIC);
    command->catMimic.mode = mode;
    command->catMimic.shipType = shipType;
    command->catMimic.player = player;
    command->catMimic.index = index;
    PrepareShipsForCommand(command,TRUE);
    listAddNode(&comlayer->todolist,&command->todonode,command);
}

/*-----------------------------------------------------------------------------
    Name        : cat_clResearch
    Description : the research order (COMMAND_CAT_RESEARCH): starts (mode 0) or stops (mode 1) a research slot
    Inputs      : comlayer, playerIndex, lab, tech, mode
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: new (cat_clResearch 0x0044dba0): a command with an empty selection, run once by clProcess
void cat_clResearch(CommandLayer *comlayer,uword playerIndex,udword lab,udword tech,udword mode)
{
    CommandToDo *command;
    SelectCommand *selection;

    command = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
    InitCommandToDo(command);
    selection = memAlloc(sizeofSelectCommand(0),"ToDoSelection",NonVolatile);
    selection->numShips = 0;
    selection->timeLastStatus = 0.0f;
    command->ordertype.attributes = 0;
    command->selection = selection;
    command->catResearch.playerIndex = playerIndex;
    command->catResearch.lab = lab;
    command->ordertype.order = COMMAND_CAT_RESEARCH;
    command->catResearch.tech = tech;
    command->catResearch.mode = mode;
    listAddNode(&comlayer->todolist,&command->todonode,command);
}

/*-----------------------------------------------------------------------------
    Name        : cat_processResearchToDo
    Description : runs a research order: as rmAssignPlayersLabToResearch / rmClearResearchlab, inline
    Inputs      : command
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: new (cat_processResearchToDo 0x0044dc20). The start inlines rmAssignPlayersLabToResearch without its Beast
//      one-topic limit; the stop is rmClearResearchlab's body.
void cat_processResearchToDo(CommandToDo *command)
{
    Player *player = &universe.players[command->catResearch.playerIndex];
    ResearchLab *lab = &player->researchinfo.researchlabs[command->catResearch.lab];
    TechnologyType tech = (TechnologyType)command->catResearch.tech;
    ResearchTopic *topic;

    if (command->catResearch.mode == CAT_RESEARCH_START)
    {
        if ((topic = Researching(player,tech)) == NULL)
        {
            topic = (ResearchTopic *)memAlloc(sizeof(ResearchTopic),"ResearchTopic",NonVolatile);
            memset(topic,0,sizeof(ResearchTopic));
            listAddNode(&player->researchinfo.listoftopics,&topic->link,topic);
            lab->labstatus = LS_RESEARCHITEM;
            lab->topic = topic;
            topic->techresearch = tech;
            topic->timeleft = (real32)player->researchinfo.techstat->TimeToComplete[tech];
        }
        else
        {
            lab->labstatus = LS_RESEARCHITEM;
            lab->topic = topic;
        }
        if ((rmGUIActive) && (player == universe.curPlayerPtr))
        {
            rmUpdateTechList();
        }
    }
    else if (command->catResearch.mode == CAT_RESEARCH_STOP)
    {
        if (lab->labstatus == LS_RESEARCHITEM)
        {
            topic = lab->topic;
            lab->topic = NULL;
            listDeleteNode(&topic->link);
            lab->labstatus = LS_NORESEARCHITEM;
            if ((rmGUIActive) && (player == universe.curPlayerPtr))
            {
                rmUpdateTechList();
            }
        }
        else if (lab->labstatus == LS_RESEARCHITEMSOON)
        {
            lab->labstatus = LS_NORESEARCHITEM;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_clUpgradeShips
    Description : starts (or with cancel stops) the pending upgrades of the selection's ships
    Inputs      : comlayer, selectcom, cancel
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: new (cat_clUpgradeShips 0x0044de80): the ships halt and are disabled (reason 0) for the upgrade time; an
//      instant upgrade applies at once; speech 0x2000894 for the owner when any upgrade takes time
void cat_clUpgradeShips(CommandLayer *comlayer,SelectCommand *selectcom,bool32 cancel)
{
    sdword i;
    Ship *ship;
    bool32 anyTimed = FALSE;

    clHalt(comlayer,selectcom);
    for (i = 0; i < selectcom->numShips; i++)
    {
        ship = selectcom->ShipPtr[i];
        if (!cancel)
        {
            ship->catUpgrading = TRUE;
            ship->catUpgradeStart = universe.totaltimeelapsed;
            ship->catUpgradeEnd = cat_shipGetUpgradeTime(ship) + universe.totaltimeelapsed;
            cat_shipSetDisableReason(ship,TRUE,0);
            if (cat_shipGetPendingUpgradeFlags(ship) & CAT_UPGRADE_DreadnoughtCannon)
            {
                cat_dreadnoughtCreateCannon(ship);
            }
            if (ship->catUpgradeStart < ship->catUpgradeEnd)
            {
                anyTimed = TRUE;
            }
            else
            {
                // the exe formats a nice-name message here into a nullsub (0x0044df5e)
                ship->catUpgrading = FALSE;
                cat_shipSetDisableReason(ship,FALSE,0);
                cat_shipApplyUpgrades(ship);
            }
        }
        else
        {
            ship->catUpgrading = FALSE;
            cat_shipSetDisableReason(ship,FALSE,0);
        }
    }
    if (anyTimed)
    {
        speechEventQueue(NULL, 0x2000894, -1, -1, 0, selectcom->ShipPtr[0]->playerowner->playerIndex, -1, 5.0f, -1);
    }
}

// CAT: new (cat_clSpecialTargetSpeech 0x004490b0): the first worker or salvage corvette of a local selection reports
//      the special order (speech 0x4000896)
void cat_clSpecialTargetSpeech(SelectCommand *selectcom)
{
    sdword i;
    Ship *ship;

    if (selectcom->ShipPtr[0]->playerowner->playerIndex != universe.curPlayerIndex)
    {
        return;
    }
    for (i = 0; i < selectcom->numShips; i++)
    {
        ship = selectcom->ShipPtr[i];
        switch (ship->shiptype)
        {
            case sWorker:
            case bWorker:
            case kSalCapCorvette:
            case tSalCapCorvette:
                speechEvent(ship, 0x4000896, ship->shiptype);
                return;
            default:
                break;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_clSpecialTarget
    Description : the special order on targets, with the support speech and the workers' flag
    Inputs      : comlayer, selectcom, targets, flag (a worker's special mode is set to it)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: new (cat_clSpecialTarget 0x00449190): for a local selection without flag, the first carrier, worker, support
//      frigate or resource collector not exempt (SPECIAL_2_CatExempt20) says 0x2000892; then clSpecial; each live
//      worker's special mode (Ship+0x640) becomes flag != 0
void cat_clSpecialTarget(CommandLayer *comlayer,SelectCommand *selectcom,SpecialCommand *targets,bool32 flag)
{
    sdword i;
    Ship *ship;

    if ((selectcom->numShips > 0) && !flag && (selectcom->ShipPtr[0]->playerowner->playerIndex == universe.curPlayerIndex))
    {
        for (i = 0; i < selectcom->numShips; i++)
        {
            ship = selectcom->ShipPtr[i];
            if ((ship->shiptype == sCarrier) || (ship->shiptype == sWorker) || (ship->shiptype == bCarrier) ||
                (ship->shiptype == bWorker) || (ship->shiptype == kAdvanceSupportFrigate) || (ship->shiptype == kCarrier) ||
                (ship->shiptype == tAdvanceSupportFrigate) || (ship->shiptype == tCarrier) || (ship->shiptype == tResourceCollector))
            {
                if (!(ship->shipOrDerelict.specialFlags2 & SPECIAL_2_CatExempt20))
                {
                    speechEvent(ship, 0x2000892, -1);
                }
                break;
            }
        }
    }
    clSpecial(comlayer,selectcom,targets);
    for (i = 0; i < selectcom->numShips; i++)
    {
        ship = selectcom->ShipPtr[i];
        if (cat_shipIsLiveWorker(ship))
        {
            ((WorkerSpec *)ship->ShipSpecifics)->specialMode = (ubyte)(flag != 0);
        }
    }
}

// CAT: new (cat_clUnleech 0x00449770)
void cat_clUnleech(CommandLayer *comlayer,SelectCommand *selectcom)
{
    RemoveShipsFromDoingStuff(comlayer,selectcom,9);
}

// CAT: new (cat_clSetLeechAction 0x00449960)
void cat_clSetLeechAction(CommandLayer *comlayer,SelectCommand *selectcom,ubyte mode)
{
    sdword i;

    for (i = 0; i < selectcom->numShips; i++)
    {
        cat_leechSetAction(selectcom->ShipPtr[i],mode);
    }
}

// CAT: new (cat_clSetModuleSlot 0x0044e0e0): the module slot of the selection's first ship (an sMothership) whose
//      module type is moduleType gets count
void cat_clSetModuleSlot(CommandLayer *comlayer,SelectCommand *selectcom,sdword moduleType,sdword count)
{
    MothershipSpec *spec;
    sdword i;

    if (selectcom->numShips == 0)
    {
        return;
    }
    spec = (MothershipSpec *)selectcom->ShipPtr[0]->ShipSpecifics;
    for (i = 0; i < CAT_MOTHERSHIP_MODULE_SLOTS; i++)
    {
        if (spec->moduleSlots[i].moduleType == moduleType)
        {
            spec->moduleSlots[i].count = count;
        }
    }
}

// CAT: new (cat_clSetBeastSystem 0x0044e120): the bMothership's beast system state (Ship+0x650)
void cat_clSetBeastSystem(CommandLayer *comlayer,Ship *ship,sdword system)
{
    ((MothershipSpec *)ship->ShipSpecifics)->beastSystemState = system;
}

// CAT: the misc-packet bits of the module types and module slots (the exe's tables 0x0072a6d0, 7 pairs, and
//      0x0072a708, 3 pairs; cat_clModuleTypeToMiscBits 0x0044e130, cat_clModuleSlotToMiscBits 0x0044e150)
static const sdword catModuleTypeMiscBits[7][2] =
{
    { sMothershipSpecial, 0x01 }, { sMothershipArmour, 0x02 }, { sMothershipMicro, 0x04 }, { sMothershipWeapons, 0x08 },
    { sMothershipDockingBay, 0x10 }, { sMothershipSupport, 0x20 }, { sMothershipBigGun, 0x40 }
};
static const sdword catModuleSlotMiscBits[3][2] = { { 0, 0x80 }, { 1, 0x100 }, { 2, 0x200 } };

udword cat_clModuleTypeToMiscBits(sdword moduleType)
{
    udword bits = 0;
    sdword i;

    for (i = 0; i < 7; i++)
    {
        if (catModuleTypeMiscBits[i][0] == moduleType)
        {
            bits = (udword)catModuleTypeMiscBits[i][1];
        }
    }
    return bits;
}

udword cat_clModuleSlotToMiscBits(sdword slot)
{
    udword bits = 0;
    sdword i;

    for (i = 0; i < 3; i++)
    {
        if (catModuleSlotMiscBits[i][0] == slot)
        {
            bits = (udword)catModuleSlotMiscBits[i][1];
        }
    }
    return bits;
}

/*-----------------------------------------------------------------------------
    Name        : cat_clAddAttachedModulesToTargets
    Description : expands an attack's targets: an sCarrier brings its clamped modules, an sMothership or bMothership
                  is replaced by them
    Inputs      : selection (the attackers; unused), targets (freed)
    Outputs     :
    Return      : the new target list
----------------------------------------------------------------------------*/
// CAT: new (cat_clAddAttachedModulesToTargets 0x004482f0): the modules are the CLASS_Component ships of the host's
//      clamped list (ClampInfo +0x34)
// CAT: one target's entries in the expanded list (counted with out == NULL)
static sdword catAddTargetAndModules(SpaceObjRotImpTarg *target,TargetPtr *out)
{
    sdword n = 0;
    sdword j;
    Ship *ship;
    CatClampedList *list;

    if (target->header.objtype != OBJ_ShipType)
    {
        if (out != NULL) out[n] = target;
        return 1;
    }
    ship = (Ship *)target;
    if (ship->shiptype == sCarrier)
    {
        if (out != NULL) out[n] = target;
        n++;
    }
    else if ((ship->shiptype != sMothership) && (ship->shiptype != bMothership))
    {
        if (out != NULL) out[n] = target;
        return 1;
    }
    if ((ship->guidance.clampInfo != NULL) && ((list = ship->guidance.clampInfo->clampedObjs) != NULL))
    {
        for (j = 0; j < list->numShips; j++)
        {
            if (list->ShipPtr[j]->staticinfo->shipclass == CLASS_Component)
            {
                if (out != NULL) out[n] = (TargetPtr)list->ShipPtr[j];
                n++;
            }
        }
    }
    return n;
}

AttackCommand *cat_clAddAttachedModulesToTargets(SelectCommand *selection,AttackCommand *targets)
{
    sdword count = 0;
    sdword i;
    AttackCommand *newtargets;

    (void)selection;
    for (i = 0; i < targets->numTargets; i++)
    {
        count += catAddTargetAndModules(targets->TargetPtr[i],NULL);
    }
    newtargets = memAlloc(sizeofSelectCommand(count),"AttackTargets",0);
    count = 0;
    for (i = 0; i < targets->numTargets; i++)
    {
        count += catAddTargetAndModules(targets->TargetPtr[i],&newtargets->TargetPtr[count]);
    }
    newtargets->numTargets = count;
    memFree(targets);
    return newtargets;
}


#if BINNETLOG

void binwriteselection(udword header,SelectCommand *selection)
{
    binnetselectionMax sel;
    sdword i;

    sel.header = header;
    if (selection == NULL)
    {
        sel.numShips = 0;
    }
    else
    {
        sel.numShips = selection->numShips;
        for (i=0;i<sel.numShips;i++)
        {
            sel.ShipID[i] = selection->ShipPtr[i]->shipID.shipNumber;
        }
    }

    fwrite(&sel,sizeofbinnetselection(sel.numShips),1,netlogfile);
}

void binwriteship(Ship *ship)
{
    uword shipid = (uword)-1;

    if (ship != NULL)
    {
        shipid = ship->shipID.shipNumber;
    }

    fwrite(&shipid,sizeof(uword),1,netlogfile);
}

void binwriteanyselection(udword header,SelectAnyCommand *selection)
{
    binnetanyselectionMax sel;
    SpaceObjRotImpTarg *target;
    uword objtype;
    uword id;
    sdword i;

    sel.header = header;
    if (selection == NULL)
    {
        sel.numTargets = 0;
    }
    else
    {
        sel.numTargets = selection->numTargets;
        for (i=0;i<sel.numTargets;i++)
        {
            target = selection->TargetPtr[i];
            objtype = target->header.objtype;
            id = 0;

            switch (objtype)
            {
                case OBJ_ShipType:
                    id = ((Ship *)target)->shipID.shipNumber;
                    break;

                case OBJ_MissileType:
                    id = ((Missile *)target)->missileID.missileNumber;
                    break;

                case OBJ_AsteroidType:
                case OBJ_NebulaType:
                case OBJ_GasType:
                case OBJ_DustType:
                    id = ((Resource *)target)->resourceID.resourceNumber;
                    break;

                case OBJ_DerelictType:
                    id = ((Derelict *)target)->derelictID.derelictNumber;
                    break;
            }

            sel.TargetID[i] = objtype | (id << 16);
        }
    }

    fwrite(&sel,sizeofbinnetanyselection(sel.numTargets),1,netlogfile);
}

void clChecksum(void)
{
    Node *curnode = universe.mainCommandLayer.todolist.head;
    CommandToDo *command;
    SelectCommand *selection;
    binnetCmdLayerInfoMax cmd;
    udword header;
    sdword i;

    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);
        selection = command->selection;

        cmd.header = makenetcheckHeader('C','C','C','C');
        cmd.order = command->ordertype.order;
        cmd.attributes = command->ordertype.attributes;
        cmd.numShips = selection->numShips;

        for (i=0;i<cmd.numShips;i++)
        {
            cmd.ShipID[i] = selection->ShipPtr[i]->shipID.shipNumber;
        }

        fwrite(&cmd,sizeofbinnetCmdLayerInfo(cmd.numShips),1,netlogfile);

        if (cmd.attributes & COMMAND_MASK_FORMATION)
        {
            header = makenetcheckHeader('F','O','R','M');
            fwrite(&header,sizeof(header),1,netlogfile);
            fwrite(&command->formation,sizeof(TypeOfFormation)+sizeof(bool32),1,netlogfile);
            fwrite(&command->formation.tacticalState,44,1,netlogfile);
        }

        if (cmd.attributes & COMMAND_MASK_PROTECTING)
        {
            binwriteselection(makenetcheckHeader('S','E','P','T'),command->protect);
        }

        if (cmd.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
        {
            binwriteanyselection(makenetcheckHeader('S','E','P','A'),command->attack);
        }

        switch (cmd.order)
        {
            case COMMAND_MOVE:
                header = makenetcheckHeader('M','O','V','E');
                fwrite(&header,sizeof(header),1,netlogfile);
                fwrite(&command->move,sizeof(MoveCommand),1,netlogfile);
                break;

            case COMMAND_ATTACK:
                binwriteanyselection(makenetcheckHeader('S','E','A','T'),command->attack);
                break;

            case COMMAND_DOCK:
                header = makenetcheckHeader('D','O','C','K');
                fwrite(&header,sizeof(header),1,netlogfile);
                fwrite(&command->dock,sizeof(DockCommand),1,netlogfile);
                break;

            case COMMAND_LAUNCH_SHIP:
                header = makenetcheckHeader('L','A','U','N');
                fwrite(&header,sizeof(header),1,netlogfile);
                binwriteship(command->launchship.receiverShip);
                break;

            case COMMAND_COLLECT_RESOURCES:
                break;      // don't bother - ship already has this info

            case COMMAND_BUILDING_SHIP:
                header = makenetcheckHeader('B','U','L','D');
                fwrite(&header,sizeof(header),1,netlogfile);
                fwrite(&command->buildingship,sizeof(ShipType)+sizeof(ShipRace),1,netlogfile);
                break;

            case COMMAND_SPECIAL:
                binwriteanyselection(makenetcheckHeader('S','P','E','C'),command->specialtargets);
                break;

            case COMMAND_MILITARY_PARADE:
                header = makenetcheckHeader('M','I','L','P');
                fwrite(&header,sizeof(header),1,netlogfile);
                binwriteship(command->militaryParade->aroundShip);
                fwrite(&command->militaryParade->paradeType,sizeof(sdword),1,netlogfile);
                break;

            case COMMAND_MP_HYPERSPACING:
                header = makenetcheckHeader('M','P','H','P');
                fwrite(&header,sizeof(header),1,netlogfile);
                fwrite(&command->hyperspaceState,sizeof(sdword),1,netlogfile);
                break;
        }

        curnode = curnode->next;
    }
}

#endif


/*=============================================================================
    CAT (M2-R5 command-tactics): the scripted command self-test, /catCommandTest <ticks> (harness mode command;
    docs/m2-r5-command-tactics.md). Not in the exe. It runs the headless Mission01 start of /oracleMission (virtual
    clock, no animatic, quits after <ticks>) and gives the Cataclysm orders with no input:
      tick 16    a scripted fleet for the local player far from the level's ships: an sMothership, an sProcessor (the
                 sphere's protected ship), two sAcolytes and two sMimics (link pairs, with the link upgrade), one more
                 sMimic (with the mimic upgrade), four sSentinels (a force-field wall) and six (a force-field sphere
                 around the processor); then the orders: cat_clLink on the acolytes and on the mimic pair,
                 cat_clMimic (MIMIC_COMMAND_ShipType sRecon: an sMimic may pose as a fighter), cat_clResearch (start TECH_LINK in lab 0),
                 cat_clPointOrder for the mothership's big gun (when its static has the big gun, gun sound type 0x12;
                 the gun is scripted charged, the order is given once the charge reads full),
                 clFormation FORCEFIELD_WALL_FORMATION on four Sentinels, clProtect + clFormation
                 FORCEFIELD_SPHERE_FORMATION on six
      tick 17    the order end states the exe's clProcess leaves after one pass: the research lab researching TECH_LINK,
                 the mimic disguising as sRecon, the big gun aiming; then cat_clResearch stops the research
      the link   once the acolytes became an sACV (and the mimics an sMCV), cat_clUnlink splits the sACV again
      every 256  one "commandtest:" line of the states
      ticks-2    the end states: an sMCV from the mimics, two sAcolytes again from the unlinked sACV, the research lab
                 idle, every wall and sphere Sentinel in position at field level 1 (the sphere's and the wall leader's field
                 active) and the sphere's clamped to the processor; "commandtest: done ... result=PASS|FAIL", then the mission run's quit
=============================================================================*/
#define CAT_CMDTEST_ORDER_TICK      16
#define CAT_CMDTEST_MAX             16

static sdword catCommandSelfTestTicks = 0;
static bool32 catCommandSelfTestOrdered = FALSE;
static bool32 catCommandSelfTestChecked17 = FALSE;
static bool32 catCommandSelfTestDone = FALSE;
static bool32 catCommandSelfTestPass = TRUE;
static Ship *catCmdTestMothership = NULL;
static Ship *catCmdTestProcessor = NULL;
static Ship *catCmdTestMimic = NULL;
static Ship *catCmdTestWall[4];
static Ship *catCmdTestSphere[6];
static sdword catCmdTestNumWall = 0;
static sdword catCmdTestNumSphere = 0;
static Ship *catCmdTestACV = NULL;              // the link's result, unlinked again
static Ship *catCmdTestMCV = NULL;
static bool32 catCmdTestUnlinkOrdered = FALSE;
static bool32 catCmdTestPointOrdered = FALSE;
static bool32 catCmdTestPointAimed = FALSE;
static bool32 catCmdTestResearchStarted = FALSE;
static bool32 catCmdTestMimicStarted = FALSE;
static sdword catCmdTestLinkTick = -1;
static sdword catCmdTestUnlinkTick = -1;
static Ship *catCmdTestBefore[CAT_CMDTEST_MAX * 4];    // the player's link types present before the link and unlink
static sdword catCmdTestNumBefore = 0;
static Ship *catCmdTestLinkers[4];             // the two acolytes and the two mimics, for the trace
static bool32 catCmdTestPointPending = FALSE;  // the point order waits for the big gun to be charged
static sdword catCmdTestPointTick = -1;

void catCommandSelfTestSet(char *string)
{
    catCommandSelfTestTicks = (sdword)strtol(string, NULL, 10);
    if (catCommandSelfTestTicks <= 0)
    {
        catCommandSelfTestTicks = 3000;
    }
    oracleSetMission(string);                               // the headless Mission01 run with the virtual clock and the quit
}

bool32 catCommandSelfTestRequested(void)
{
    return (catCommandSelfTestTicks > 0);
}

static void catCmdTestExpect(bool32 condition, const char *what)
{
    if (!condition)
    {
        catCommandSelfTestPass = FALSE;
        dbgMessagef("commandtest: FAIL %s (tick %u)", what, (unsigned)universe.univUpdateCounter);
    }
}

static bool32 catCmdTestShipLive(Ship *ship)
{
    Node *node;

    if (ship == NULL)
    {
        return FALSE;
    }
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        if ((Ship *)listGetStructOfNode(node) == ship)
        {
            return !bitTest(ship->header.flags, SOF_Dead);
        }
    }
    return FALSE;
}

static bool32 catCmdTestWasBefore(Ship *ship)
{
    sdword i;

    for (i = 0; i < catCmdTestNumBefore; i++)
    {
        if (catCmdTestBefore[i] == ship)
        {
            return TRUE;
        }
    }
    return FALSE;
}

// the player's ships of type that were not there when the snapshot was taken
static sdword catCmdTestNewShipsOfType(ShipType type, Ship **out, sdword max)
{
    Node *node;
    Ship *ship;
    sdword n = 0;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == universe.curPlayerPtr) && (ship->shiptype == type) && !catCmdTestWasBefore(ship) &&
            !bitTest(ship->header.flags, SOF_Dead))
        {
            if ((out != NULL) && (n < max))
            {
                out[n] = ship;
            }
            n++;
        }
    }
    return n;
}

static void catCmdTestSnapshot(void)
{
    Node *node;
    Ship *ship;

    catCmdTestNumBefore = 0;
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->playerowner == universe.curPlayerPtr) &&
            ((ship->shiptype == sAcolyte) || (ship->shiptype == sACV) || (ship->shiptype == sMimic) || (ship->shiptype == sMCV)) &&
            (catCmdTestNumBefore < CAT_CMDTEST_MAX * 4))
        {
            catCmdTestBefore[catCmdTestNumBefore++] = ship;
        }
    }
}

// the exe's end state of a force field: every Sentinel in position at field level 1; field-active the sphere's (each
// keeps its own) and the wall's leader (a wall member clears its own flag on its update and the leader's update sets
// it again: cat_sentinelFormationUpdate 0x005c6830)
static bool32 catCmdTestFieldUp(Ship **ships, sdword num, Ship *clampHost)
{
    sdword i;
    SentinelSpec *spec;
    bool32 up = (num > 0);
    Ship *leader = NULL;

    if ((num > 0) && catCmdTestShipLive(ships[0]) && (ships[0]->command != NULL))
    {
        leader = ships[0]->command->selection->ShipPtr[0];
    }
    for (i = 0; i < num; i++)
    {
        if (!catCmdTestShipLive(ships[i]))
        {
            return FALSE;
        }
        spec = (SentinelSpec *)ships[i]->ShipSpecifics;
        if (!spec->inPosition || (spec->fieldLevel != 1.0f) ||
            (((clampHost != NULL) || (ships[i] == leader)) && !spec->fieldActive))
        {
            up = FALSE;
        }
        if ((clampHost != NULL) &&
            ((ships[i]->guidance.clampInfo == NULL) || (ships[i]->guidance.clampInfo->host != (SpaceObjRotImpTarg *)clampHost)))
        {
            up = FALSE;
        }
    }
    return up;
}

static void catCmdTestLogField(const char *tag, Ship **ships, sdword num)
{
    sdword i, inpos = 0, active = 0, links = 0, clamped = 0;
    SentinelSpec *spec;

    for (i = 0; i < num; i++)
    {
        if (!catCmdTestShipLive(ships[i]))
        {
            continue;
        }
        spec = (SentinelSpec *)ships[i]->ShipSpecifics;
        inpos += spec->inPosition ? 1 : 0;
        active += (spec->fieldLevel == 1.0f) ? 1 : 0;
        links += spec->numLinks;
        clamped += ((ships[i]->guidance.clampInfo != NULL) && (ships[i]->guidance.clampInfo->host != NULL)) ? 1 : 0;
    }
    dbgMessagef("commandtest: %s sentinels=%d inPosition=%d level1=%d links=%d clamped=%d formation=%d", tag, (int)num,
                (int)inpos, (int)active, (int)links, (int)clamped,
                ((num > 0) && (ships[0]->command != NULL) && (ships[0]->command->ordertype.attributes & COMMAND_MASK_FORMATION)) ?
                    (int)ships[0]->command->formation.formationtype : -1);
}

void catCommandSelfTestTick(void)
{
    sdword tick = (sdword)universe.univUpdateCounter;
    Player *player = universe.curPlayerPtr;
    Node *node;
    Ship *ship;
    sdword i;
    MaxSelection sel;

    if (catCommandSelfTestDone || (player == NULL))
    {
        return;
    }
    if (!catCommandSelfTestOrdered && (tick >= CAT_CMDTEST_ORDER_TICK))
    {
        static const struct { ShipType type; real32 dx, dy, dz; } fleet[] =
        {
            { sMothership,     0.0f,     0.0f,    0.0f },
            { sProcessor,   9000.0f,     0.0f,    0.0f },
            { sAcolyte,    -5000.0f,   600.0f,    0.0f },
            { sAcolyte,    -5000.0f,  -600.0f,  300.0f },
            { sMimic,      -5000.0f,  3000.0f,    0.0f },
            { sMimic,      -5000.0f,  3600.0f,  300.0f },
            { sMimic,      -6000.0f, -3000.0f,    0.0f },
            { sSentinel,    3000.0f,  6000.0f,    0.0f },
            { sSentinel,    3600.0f,  6000.0f,    0.0f },
            { sSentinel,    4200.0f,  6000.0f,    0.0f },
            { sSentinel,    4800.0f,  6000.0f,    0.0f },
            { sSentinel,    9000.0f,  2500.0f,    0.0f },
            { sSentinel,    9000.0f, -2500.0f,    0.0f },
            { sSentinel,    9000.0f,     0.0f, 2500.0f },
            { sSentinel,    9000.0f,     0.0f,-2500.0f },
            { sSentinel,   11500.0f,     0.0f,    0.0f },
            { sSentinel,    6500.0f,     0.0f,    0.0f },
        };
        vector base;
        sdword k, gx, gy, gz;
        real32 limit = universe.radius * 0.8f;
        real32 bestdist = -1.0f;
        Ship *acolytes[2], *mimics[2];
        sdword numAcolytes = 0, numMimics = 0;
        SelectCommand protectone;

        catCommandSelfTestOrdered = TRUE;
        // the fleet goes to the grid point (step 10,000) inside 80% of the world sphere farthest from every ship the
        // level put down, so nothing engages it (as the dock self-test)
        base.x = base.y = base.z = 0.0f;
        for (gx = -10; gx <= 10; gx++)
        {
            for (gy = -10; gy <= 10; gy++)
            {
                for (gz = -3; gz <= 3; gz++)
                {
                    vector cand, diff;
                    real32 nearest = REALlyBig;

                    cand.x = gx * 10000.0f; cand.y = gy * 10000.0f; cand.z = gz * 10000.0f;
                    if (vecMagnitudeSquared(cand) > limit * limit)
                    {
                        continue;
                    }
                    for (node = universe.ShipList.head; node != NULL; node = node->next)
                    {
                        ship = (Ship *)listGetStructOfNode(node);
                        vecSub(diff,ship->obj.posinfo.position,cand);
                        if (vecMagnitudeSquared(diff) < nearest)
                        {
                            nearest = vecMagnitudeSquared(diff);
                        }
                    }
                    if (nearest > bestdist)
                    {
                        bestdist = nearest;
                        base = cand;
                    }
                }
            }
        }
        dbgMessagef("commandtest: the scripted fleet goes to (%.0f %.0f %.0f), %.0f from the nearest level ship",
                    base.x, base.y, base.z, fsqrt(bestdist));
        for (k = 0; k < (sdword)(sizeof(fleet) / sizeof(fleet[0])); k++)
        {
            vector pos;

            pos.x = base.x + fleet[k].dx;
            pos.y = base.y + fleet[k].dy;
            pos.z = base.z + fleet[k].dz;
            ship = univAddShip(fleet[k].type,0,&pos,player,0);
            if (ship == NULL)
            {
                catCmdTestExpect(FALSE, "scripted ship created");
                continue;
            }
            ship->catUpgradeFlags |= CAT_UPGRADE_Link | CAT_UPGRADE_Mimic;     // scripted: the link and mimic research
            dbgMessagef("commandtest: created %s#%u at (%.0f %.0f %.0f)", ShipTypeToStr(ship->shiptype),
                        (unsigned)ship->shipID.shipNumber, pos.x, pos.y, pos.z);
            switch (fleet[k].type)
            {
                case sMothership: catCmdTestMothership = ship; break;
                case sProcessor:  catCmdTestProcessor = ship; break;
                case sAcolyte:    acolytes[numAcolytes++] = ship; break;
                case sMimic:
                    if (numMimics < 2) mimics[numMimics++] = ship; else catCmdTestMimic = ship;
                    break;
                case sSentinel:
                    if (catCmdTestNumWall < 4) catCmdTestWall[catCmdTestNumWall++] = ship;
                    else catCmdTestSphere[catCmdTestNumSphere++] = ship;
                    break;
                default: break;
            }
        }
        catCmdTestSnapshot();       // the link's sACV / sMCV are the new ones of their type

        // link: the acolytes, the mimic pair
        sel.numShips = numAcolytes; sel.timeLastStatus = 0.0f;
        for (i = 0; i < numAcolytes; i++) sel.ShipPtr[i] = acolytes[i];
        cat_clLink(&universe.mainCommandLayer,(SelectCommand *)&sel);
        sel.numShips = numMimics;
        for (i = 0; i < numMimics; i++) sel.ShipPtr[i] = mimics[i];
        cat_clLink(&universe.mainCommandLayer,(SelectCommand *)&sel);
        dbgMessagef("commandtest: link ordered: %d acolytes, %d mimics", (int)numAcolytes, (int)numMimics);
        for (i = 0; i < 2; i++)
        {
            catCmdTestLinkers[i] = (i < numAcolytes) ? acolytes[i] : NULL;
            catCmdTestLinkers[2 + i] = (i < numMimics) ? mimics[i] : NULL;
        }

        // mimic: the third mimic poses as a Worker of its own player
        if (catCmdTestMimic != NULL)
        {
            sel.numShips = 1; sel.ShipPtr[0] = catCmdTestMimic;
            cat_clMimic(&universe.mainCommandLayer,(SelectCommand *)&sel,MIMIC_COMMAND_ShipType,sRecon,player->playerIndex,0);
        }

        // research: start TECH_LINK in lab 0 (only when the player's research statics are loaded)
        if (player->researchinfo.techstat != NULL)
        {
            cat_clResearch(&universe.mainCommandLayer,player->playerIndex,0,TECH_LINK,CAT_RESEARCH_START);
        }
        else
        {
            catCmdTestExpect(FALSE, "research statics loaded");
        }

        // point: the mothership's big gun, once it is charged (the order does nothing before: cat_bigGunProcessFireCommand
        //      0x005bbe80 tests the reload; a mothership without the big gun's gun would fault, so it gets no order)
        catCmdTestPointPending = (catCmdTestMothership != NULL) && (cat_mothershipGetBigGun(catCmdTestMothership) != NULL);
        if (catCmdTestPointPending)
        {
            Gun *bigGun = cat_mothershipGetBigGun(catCmdTestMothership);

            bigGun->lasttimefired = universe.totaltimeelapsed - bigGun->gunstatic->firetime;   // scripted: charged
        }
        dbgMessagef("commandtest: point pending: %d (big gun %s)", (int)catCmdTestPointPending,
                    catCmdTestPointPending ? "present" : "absent: no order is given");

        // force-field wall
        sel.numShips = catCmdTestNumWall;
        for (i = 0; i < catCmdTestNumWall; i++) sel.ShipPtr[i] = catCmdTestWall[i];
        clFormation(&universe.mainCommandLayer,(SelectCommand *)&sel,FORCEFIELD_WALL_FORMATION,0);

        // force-field sphere around the processor
        sel.numShips = catCmdTestNumSphere;
        for (i = 0; i < catCmdTestNumSphere; i++) sel.ShipPtr[i] = catCmdTestSphere[i];
        if (catCmdTestProcessor != NULL)
        {
            protectone.numShips = 1; protectone.timeLastStatus = 0.0f; protectone.ShipPtr[0] = catCmdTestProcessor;
            clProtect(&universe.mainCommandLayer,(SelectCommand *)&sel,(ProtectCommand *)&protectone);
        }
        clFormation(&universe.mainCommandLayer,(SelectCommand *)&sel,FORCEFIELD_SPHERE_FORMATION,0);
        dbgMessagef("commandtest: orders tick=%u wall=%d sphere=%d ticks=%d", (unsigned)tick, (int)catCmdTestNumWall,
                    (int)catCmdTestNumSphere, (int)catCommandSelfTestTicks);
        return;
    }
    if (!catCommandSelfTestOrdered)
    {
        return;
    }
    if (!catCommandSelfTestChecked17)
    {
        ResearchLab *lab = &player->researchinfo.researchlabs[0];

        catCommandSelfTestChecked17 = TRUE;
        catCmdTestResearchStarted = (lab->labstatus == LS_RESEARCHITEM) && (lab->topic != NULL) &&
                                    (lab->topic->techresearch == TECH_LINK);
        catCmdTestExpect(catCmdTestResearchStarted, "research: lab 0 researching TECH_LINK after the start order");
        if (catCmdTestMimic != NULL)
        {
            MimicSpec *spec = (MimicSpec *)catCmdTestMimic->ShipSpecifics;

            catCmdTestMimicStarted = (spec->nextType == sRecon) || (spec->apparentType == sRecon);
            catCmdTestExpect(catCmdTestMimicStarted, "mimic: the disguise as sRecon is set");
        }
        dbgMessagef("commandtest: tick %u: research started=%d mimic set=%d", (unsigned)tick,
                    (int)catCmdTestResearchStarted, (int)catCmdTestMimicStarted);
        if (catCmdTestResearchStarted)
        {
            cat_clResearch(&universe.mainCommandLayer,player->playerIndex,0,TECH_LINK,CAT_RESEARCH_STOP);
        }
        return;
    }
    // the point order, once the big gun is charged; its end state one tick later
    if (catCmdTestPointOrdered && !catCmdTestPointAimed && (catCmdTestPointTick >= 0) && (tick == catCmdTestPointTick + 1))
    {
        catCmdTestPointAimed = (((MothershipSpec *)catCmdTestMothership->ShipSpecifics)->bigGunState != CAT_BIGGUN_Idle);
        catCmdTestExpect(catCmdTestPointAimed, "point: the big gun left its idle state");
        dbgMessagef("commandtest: point done at tick %u: big gun state %d", (unsigned)tick,
                    (int)((MothershipSpec *)catCmdTestMothership->ShipSpecifics)->bigGunState);
    }
    if (catCmdTestPointPending && catCmdTestShipLive(catCmdTestMothership) &&
        (cat_bigGunChargeFraction(catCmdTestMothership) >= 1.0f))
    {
        vector point;

        catCmdTestPointPending = FALSE;
        point.x = catCmdTestMothership->obj.posinfo.position.x;
        point.y = catCmdTestMothership->obj.posinfo.position.y + 20000.0f;
        point.z = catCmdTestMothership->obj.posinfo.position.z;
        sel.numShips = 1; sel.timeLastStatus = 0.0f; sel.ShipPtr[0] = catCmdTestMothership;
        cat_clPointOrder(&universe.mainCommandLayer,(SelectCommand *)&sel,&point);
        catCmdTestPointOrdered = TRUE;
        catCmdTestPointTick = tick;
        dbgMessagef("commandtest: point ordered at tick %u", (unsigned)tick);
    }
    // the link's results
    if ((catCmdTestACV == NULL) && (catCmdTestNewShipsOfType(sACV,&catCmdTestACV,1) > 0))
    {
        catCmdTestLinkTick = tick;
        dbgMessagef("commandtest: link done at tick %u: sACV#%u", (unsigned)tick, (unsigned)catCmdTestACV->shipID.shipNumber);
    }
    if ((catCmdTestMCV == NULL) && (catCmdTestNewShipsOfType(sMCV,&catCmdTestMCV,1) > 0))
    {
        dbgMessagef("commandtest: mimic link done at tick %u: sMCV#%u", (unsigned)tick, (unsigned)catCmdTestMCV->shipID.shipNumber);
    }
    if ((catCmdTestACV != NULL) && !catCmdTestUnlinkOrdered && (tick >= catCmdTestLinkTick + 32))
    {
        catCmdTestUnlinkOrdered = TRUE;
        catCmdTestSnapshot();       // the unlink's sAcolytes are the new ones
        sel.numShips = 1; sel.timeLastStatus = 0.0f; sel.ShipPtr[0] = catCmdTestACV;
        cat_clUnlink(&universe.mainCommandLayer,(SelectCommand *)&sel);
        dbgMessagef("commandtest: unlink ordered at tick %u", (unsigned)tick);
    }
    if (catCmdTestUnlinkOrdered && (catCmdTestUnlinkTick < 0) && (catCmdTestNewShipsOfType(sAcolyte,NULL,0) >= 2))
    {
        catCmdTestUnlinkTick = tick;
        dbgMessagef("commandtest: unlink done at tick %u", (unsigned)tick);
    }
    if ((tick % 256) == 0)
    {
        catCmdTestLogField("wall", catCmdTestWall, catCmdTestNumWall);
        catCmdTestLogField("sphere", catCmdTestSphere, catCmdTestNumSphere);
        dbgMessagef("commandtest: tick %u: link=%d mcv=%d unlink=%d lab0=%d", (unsigned)tick, (int)catCmdTestLinkTick,
                    (int)(catCmdTestMCV != NULL), (int)catCmdTestUnlinkTick, (int)player->researchinfo.researchlabs[0].labstatus);
        for (i = 0; i < 4; i++)
        {
            ship = catCmdTestLinkers[i];
            if (catCmdTestShipLive(ship))
            {
                CatLinkInfo *link = cat_shipGetLinkInfo(ship);
                real32 dist = -1.0f;

                if ((link != NULL) && (link->partner != NULL))
                {
                    vector diff;

                    vecSub(diff,ship->obj.posinfo.position,link->partner->obj.posinfo.position);
                    dist = fsqrt(vecMagnitudeSquared(diff));
                }
                dbgMessagef("commandtest:   %s#%u role=%d partner=%d dist=%.0f order=%d linkDistance=%.0f", ShipTypeToStr(ship->shiptype),
                            (unsigned)ship->shipID.shipNumber, (link != NULL) ? (int)link->role : -1,
                            ((link != NULL) && (link->partner != NULL)) ? (int)link->partner->shipID.shipNumber : -1, dist,
                            (ship->command != NULL) ? (int)ship->command->ordertype.order : -1, ship->staticinfo->linkDistance);
            }
        }
        for (i = 0; i < catCmdTestNumSphere; i++)
        {
            ship = catCmdTestSphere[i];
            if (catCmdTestShipLive(ship) && (catCmdTestProcessor != NULL))
            {
                vector offset, diff;

                matMultiplyMatByVec(&offset,&catCmdTestProcessor->rotatable.rotinfo.coordsys,&ship->formationOffset);
                diff.x = ship->obj.posinfo.position.x - catCmdTestProcessor->impactable.collInfo.collPosition.x - offset.x;
                diff.y = ship->obj.posinfo.position.y - catCmdTestProcessor->impactable.collInfo.collPosition.y - offset.y;
                diff.z = ship->obj.posinfo.position.z - catCmdTestProcessor->impactable.collInfo.collPosition.z - offset.z;
                dbgMessagef("commandtest:   sphere %s#%u err=%.0f off=(%.0f %.0f %.0f) speed=%.0f inPos=%d active=%d order=%d attr=%x",
                            ShipTypeToStr(ship->shiptype), (unsigned)ship->shipID.shipNumber, fsqrt(vecMagnitudeSquared(diff)),
                            ship->formationOffset.x, ship->formationOffset.y, ship->formationOffset.z,
                            fsqrt(vecMagnitudeSquared(ship->obj.posinfo.velocity)),
                            (int)((SentinelSpec *)ship->ShipSpecifics)->inPosition, (int)((SentinelSpec *)ship->ShipSpecifics)->fieldActive,
                            (ship->command != NULL) ? (int)ship->command->ordertype.order : -1,
                            (ship->command != NULL) ? (unsigned)ship->command->ordertype.attributes : 0u);
            }
        }
        if (catCmdTestPointPending && catCmdTestShipLive(catCmdTestMothership))
        {
            dbgMessagef("commandtest:   big gun charge %.2f", cat_bigGunChargeFraction(catCmdTestMothership));
        }
    }
    if (tick >= catCommandSelfTestTicks - 2)
    {
        catCommandSelfTestDone = TRUE;
        catCmdTestLogField("wall", catCmdTestWall, catCmdTestNumWall);
        catCmdTestLogField("sphere", catCmdTestSphere, catCmdTestNumSphere);
        catCmdTestExpect(!catCmdTestPointPending && (!catCmdTestPointOrdered || catCmdTestPointAimed),
                         "point: the big gun took the order and left its idle state");
        catCmdTestExpect(catCmdTestACV != NULL, "link: the acolytes became an sACV");
        catCmdTestExpect(catCmdTestMCV != NULL, "link: the mimics became an sMCV");
        catCmdTestExpect(catCmdTestUnlinkTick >= 0, "unlink: the sACV split into two sAcolytes");
        catCmdTestExpect(!catCmdTestShipLive(catCmdTestACV), "unlink: the sACV is gone");
        catCmdTestExpect(player->researchinfo.researchlabs[0].labstatus != LS_RESEARCHITEM, "research: lab 0 idle after the stop order");
        if (catCmdTestMimic != NULL)
        {
            catCmdTestExpect(cat_shipIsMimicking(catCmdTestMimic), "mimic: the mimic is disguised");
        }
        catCmdTestExpect(catCmdTestFieldUp(catCmdTestWall,catCmdTestNumWall,NULL), "wall: every Sentinel in position with its field up");
        catCmdTestExpect(catCmdTestFieldUp(catCmdTestSphere,catCmdTestNumSphere,catCmdTestProcessor),
                         "sphere: every Sentinel in position, field up, clamped to the processor");
        dbgMessagef("commandtest: done tick=%u link=%d mcv=%d unlink=%d research=%d/%d mimic=%d point=%d/%d result=%s",
                    (unsigned)tick, (int)catCmdTestLinkTick, (int)(catCmdTestMCV != NULL), (int)catCmdTestUnlinkTick,
                    (int)catCmdTestResearchStarted, (int)(player->researchinfo.researchlabs[0].labstatus != LS_RESEARCHITEM),
                    (int)((catCmdTestMimic != NULL) && cat_shipIsMimicking(catCmdTestMimic)),
                    (int)catCmdTestPointOrdered, (int)catCmdTestPointAimed, catCommandSelfTestPass ? "PASS" : "FAIL");
        // HWI-TEST (M2-R6 nis-fow-camera): the scripted mimics are stealth types (catTypeFlags 0x20000), so the stealth
        //      detection runs in this test; logged, not judged
        dbgMessagef("commandtest: fow grid=%s stealthUpdates=%u", (player->catFOWGrid != NULL) ? "set" : "NULL",
                    (unsigned)catStealthDetectionUpdates);
    }
}
