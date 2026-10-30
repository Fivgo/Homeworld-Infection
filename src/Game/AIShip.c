// =============================================================================
//  AIShip.c
//  - This AI layer controls where a ship's velocity vector should be
//  and what heading the ship should face.  It determines this based
//  on commands received (parameters passed), and on where enemy ships are, etc.
//  Note that it is the responsibility of the AItrack layer below to
//  actually cause the ship to follow the desired velocity vector and heading.
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/26/1997 by gshaw
// =============================================================================

#include "UnivUpdate.h"   // HWI-TEST (M3-R4 nan-positions): catNaNProbeOn
#include "AIShip.h"
#include "ShipLink.h"
#include "Salvage.h"

#include "AITrack.h"
#include "Alliance.h"
#include "Blobs.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "FlightManDefs.h"
#include "FormationDefs.h"
#include "FastMath.h"
#include "OracleDump.h"     // HWI-ORACLE (M2-R8 oracle-rung-physics): /oracleShipDump
#include "Physics.h"
#include "SinglePlayer.h"   // CAT: ShipSinglePlayerGameInfo.hyperspaceEffect (aishipFlyToPointAvoidingObjsFunc)
#include "ProfileTimers.h"
#include "Randy.h"
#include "Ships.h"
#include "Tactics.h"
#include "Tweak.h"
#include "Universe.h"
#include <math.h>           // HWI-TEST (M3-R3 mines): /catMineLog distances (log only)
#include "CommandWrap.h"    // HWI-TEST (M3-R3 mines): /catMineChase (clWrapMove)
#include "Select.h"         // HWI-TEST (M3-R3 mines): sizeofSelectCommand
#include <stdio.h>    // HWI-FIX(release-1.0): declared on every platform (mingw does not pull it in)


#define DEBUG_AISHIP    0

#define PAD_AROUND_SMALLEST_SHIP    1

#define VELFACTOR 0

bool32 isCapitalShipStaticOrBig(ShipStaticInfo *shipstatic);

/*=============================================================================
    AIShip Stats (optional)
=============================================================================*/

#if AISHIP_STATS
AIshipStats aishipStats = { 0,0,0,0,0,0.0f, 0, {0.0f, 0.0f, 0.0f } };
#endif

/*=============================================================================
    Data
=============================================================================*/

bool32 aishipTempDisableAvoiding = FALSE;

/*=============================================================================
    Private Function declarations:
=============================================================================*/

static void scriptAIShipPrecalculate(char *directory,char *field,void *dataToFillIn);
static void rowSetDetails(char *directory,char *field,void *dataToFillIn);

/*=============================================================================
    Tweakables:
=============================================================================*/

real32 VELOCITY_SCALE_FACTOR = 0.1f;
real32 URGENT_SCALE_FACTOR   = 1.0f;
real32 MISSILE_SCALE_FACTOR  = 1.0f;
real32 MISSILE_MIN_VELOCITY = 500.0f;
real32 MISSILE_USE_VELOCITYPRED_DISTANCE = 250.0f*250.0f;
//real32 MINE_MIN_VELOCITY = 75.0f;
real32 AVOID_OBJ_SCALE_FACTOR = (real32)1e-4;
real32 COMBAT_AVOID_OBJ_SCALE_FACTOR = (real32)1e-5;
real32 PROBE_AVOID_OBJ_SCALE_FACTOR = (real32)1e-5;
real32 AVOID_PANIC_FACTOR = 0.2f;
real32 AVOID_OBJ_PADDING_SCALE = 1.6f;
real32 AVOID_OBJ_PADDING_SCALE_BIG = 2.0f;
real32 AVOID_OBJ_ROW_PADDING_SCALE = 3.0f;
real32 AVOID_OBJ_FORMATION_PADDING_SCALE = 2.55f;

real32 AVOID_OBJ_MSHIP_MILPARADE_PADDING_SCALE = 1.55f;
real32 AVOID_OBJ_R2MSHIP_MILPARADE_PADDING_SCALE = 0.75f;

real32 AVOID_OBJ_OBSCUREDPOINT_SCALE = 1.55f;
real32 AVOID_SPREAD_OUT_DIST = 100.0f;
real32 CAR_TURN_POWER = 0.1f;
real32 FLYSHIP_HEADINGACCURACY = 0.999f;
real32 FLYSHIP_ATTACKACCURACY = 0.999f;
real32 FIGHTER_BANK = 0.99f;
real32 CORVETTE_BANK = 0.707f;
real32 FRIGATE_BANK = 0.5f;
real32 DESTROYER_BANK = 0.35f;
real32 MASSIVESHIP_BANK = 0.17f;
real32 NOSHIP_BANK = 0.01f;
real32 FRIGATE_TURNPITCH = DEG_TO_RAD(30.0f);
real32 DESTROYER_TURNPITCH = DEG_TO_RAD(25.0f);
real32 MASSIVESHIP_TURNPITCH = DEG_TO_RAD(20.0f);
real32 NOSHIP_TURNPITCH = DEG_TO_RAD(0.01f);
real32 FRIGATE_DESCENDPITCH = DEG_TO_RAD(20.0f);
real32 DESTROYER_DESCENDPITCH = DEG_TO_RAD(15.0f);
real32 MASSIVESHIP_DESCENDPITCH = DEG_TO_RAD(10.0f);
real32 NOSHIP_DESCENDPITCH = DEG_TO_RAD(0.01f);

real32 INTERCEPTORBREAK_TOLERANCE = 110.0f;
real32 INTERCEPTORBREAK_MINVELOCITY = -200.0f;
real32 INTERCEPTORKILL_MINVELOCITY = -200.0f;

real32 DESCEND_PITCH4_DIST = 0.25f;
real32 DESCEND_PITCH3_DIST = 0.50f;
real32 DESCEND_PITCH2_DIST = 0.75f;
real32 MIN_DIST_FOR_FANCY_DESCEND = 100.0f;
real32 MIN_ANGLE_FOR_FANCY_DESCEND = 0.577f;      // tan(30)

real32 MIN_DIST_TO_TRACK_HEADING = 20.0f;
real32 MAX_ANGLE_FOR_TRACK_HEADING = 2.747f;   // tan(70)

real32 ROW_GOING_MOSTLY_UPDOWN_ANGLE = 0.707f;
real32 AVOID_ROW_SHIP_BY_SCALE = 4.0f;
real32 AVOID_ROW_SHIP_BY_ADD = 200.0f;
real32 AVOID_ROW_R1MOTHERSHIP_BY = 1000.0f;
real32 ROW_GO_DOWN_INSTEAD_OF_UP_RATIO = 0.3f;

real32 PASS_RIGHT_FACTOR = 1.0f;
real32 USE_PASS_RIGHT_ANGLE = 0.97f;

real32 NEG_AVOID_MIN_VEL = -1.0f;
real32 AVOID_MIN_VEL = 1.0f;

real32 DONTUSEVELOCITYPRED_IFBELOW = 1000.0f;

bool32 DO_AVOID_OBJS = TRUE;


// pre-calculated constants:
real32 oneOverDESCEND_PITCH4_DIST;
real32 oneOverDESCEND_3_MINUS_4_DIST;
real32 oneOverDESCEND_2_MINUS_3_DIST;

scriptEntry AIShipTweaks[] =
{
    makeEntry(VELOCITY_SCALE_FACTOR,scriptSetReal32CB),
    makeEntry(URGENT_SCALE_FACTOR,scriptSetReal32CB),
    makeEntry(MISSILE_SCALE_FACTOR,scriptSetReal32CB),
    makeEntry(MISSILE_MIN_VELOCITY,scriptSetReal32CB),
    makeEntry(MISSILE_USE_VELOCITYPRED_DISTANCE,scriptSetReal32SqrCB),
    //makeEntry(MINE_MIN_VELOCITY,scriptSetReal32CB),
    makeEntry(AVOID_OBJ_SCALE_FACTOR,scriptSetReal32CB),
    makeEntry(COMBAT_AVOID_OBJ_SCALE_FACTOR,scriptSetReal32CB),
    makeEntry(PROBE_AVOID_OBJ_SCALE_FACTOR,scriptSetReal32CB),
    makeEntry(AVOID_OBJ_PADDING_SCALE,scriptSetReal32CB),
    makeEntry(AVOID_OBJ_PADDING_SCALE_BIG,scriptSetReal32CB),
    makeEntry(AVOID_OBJ_FORMATION_PADDING_SCALE,scriptSetReal32CB),
    makeEntry(AVOID_OBJ_MSHIP_MILPARADE_PADDING_SCALE,scriptSetReal32CB),
    makeEntry(AVOID_OBJ_R2MSHIP_MILPARADE_PADDING_SCALE,scriptSetReal32CB),
    makeEntry(AVOID_OBJ_OBSCUREDPOINT_SCALE,scriptSetReal32CB),
    makeEntry(AVOID_OBJ_ROW_PADDING_SCALE,scriptSetReal32CB),
    makeEntry(AVOID_PANIC_FACTOR,scriptSetReal32CB),
    makeEntry(AVOID_SPREAD_OUT_DIST,scriptSetReal32CB),
    makeEntry(CAR_TURN_POWER,scriptSetReal32CB),
    makeEntry(FLYSHIP_HEADINGACCURACY,scriptSetCosAngCB),
    makeEntry(FLYSHIP_ATTACKACCURACY,scriptSetCosAngCB),
    makeEntry(DO_AVOID_OBJS,scriptSetBool),
    makeEntry(CORVETTE_BANK,scriptSetSinAngCB),
    makeEntry(FRIGATE_BANK,scriptSetSinAngCB),
    makeEntry(DESTROYER_BANK,scriptSetSinAngCB),
    makeEntry(MASSIVESHIP_BANK,scriptSetSinAngCB),
    makeEntry(NOSHIP_BANK,scriptSetSinAngCB),
    makeEntry(FRIGATE_TURNPITCH,scriptSetAngCB),
    makeEntry(DESTROYER_TURNPITCH,scriptSetAngCB),
    makeEntry(MASSIVESHIP_TURNPITCH,scriptSetAngCB),
    makeEntry(NOSHIP_TURNPITCH,scriptSetAngCB),
    makeEntry(FRIGATE_DESCENDPITCH,scriptSetAngCB),
    makeEntry(DESTROYER_DESCENDPITCH,scriptSetAngCB),
    makeEntry(MASSIVESHIP_DESCENDPITCH,scriptSetAngCB),
    makeEntry(NOSHIP_DESCENDPITCH,scriptSetAngCB),
    makeEntry(DESCEND_PITCH4_DIST,scriptSetReal32CB),
    makeEntry(DESCEND_PITCH3_DIST,scriptSetReal32CB),
    makeEntry(DESCEND_PITCH2_DIST,scriptSetReal32CB),
    makeEntry(MIN_DIST_FOR_FANCY_DESCEND,scriptSetReal32CB),
    makeEntry(MIN_ANGLE_FOR_FANCY_DESCEND,scriptSetTanAngCB),
    makeEntry(MIN_DIST_TO_TRACK_HEADING,scriptSetReal32CB),
    makeEntry(MAX_ANGLE_FOR_TRACK_HEADING,scriptSetTanAngCB),
    makeEntry(INTERCEPTORBREAK_TOLERANCE,scriptSetReal32CB),
    makeEntry(INTERCEPTORBREAK_MINVELOCITY,scriptSetReal32CB),
    makeEntry(INTERCEPTORKILL_MINVELOCITY,scriptSetReal32CB),
    makeEntry(ROW_GOING_MOSTLY_UPDOWN_ANGLE,scriptSetCosAngCB),
    makeEntry(AVOID_ROW_SHIP_BY_SCALE,scriptSetReal32CB),
    makeEntry(AVOID_ROW_SHIP_BY_ADD,scriptSetReal32CB),
    makeEntry(AVOID_ROW_R1MOTHERSHIP_BY,scriptSetReal32CB),
    makeEntry(ROW_GO_DOWN_INSTEAD_OF_UP_RATIO,scriptSetReal32CB),
    makeEntry(PASS_RIGHT_FACTOR,scriptSetReal32CB),
    makeEntry(USE_PASS_RIGHT_ANGLE,scriptSetCosAngSqrCB),
    makeEntry(AVOID_MIN_VEL,scriptSetReal32CB),
    makeEntry(DONTUSEVELOCITYPRED_IFBELOW,scriptSetReal32CB),
    { "rowDetails", rowSetDetails, NULL },
    { "AISHIP_OTHER_CALCULATIONS", scriptAIShipPrecalculate, NULL },        // should go last

    END_SCRIPT_ENTRY
};

static void scriptAIShipPrecalculate(char *directory,char *field,void *dataToFillIn)
{
    oneOverDESCEND_PITCH4_DIST = 1.0f / DESCEND_PITCH4_DIST;
    oneOverDESCEND_3_MINUS_4_DIST = 1.0f / (DESCEND_PITCH3_DIST - DESCEND_PITCH4_DIST);
    oneOverDESCEND_2_MINUS_3_DIST = 1.0f / (DESCEND_PITCH2_DIST - DESCEND_PITCH3_DIST);
    dbgAssertOrIgnore(oneOverDESCEND_3_MINUS_4_DIST > 0.0f);
    dbgAssertOrIgnore(oneOverDESCEND_2_MINUS_3_DIST > 0.0f);
    NEG_AVOID_MIN_VEL = -AVOID_MIN_VEL;
}

#if AISHIP_STATS
void aishipStatsInitFunc(Ship *ship)
{
    if (selSelected.numShips > 0 && (ship == selSelected.ShipPtr[0]))
    {
        memset(&aishipStats,0,sizeof(aishipStats));
        aishipStats.isValid = TRUE;
        aishipStats.isDisplayable = TRUE;
    }
}

void aishipStatsCloseFunc()
{
    aishipStats.isValid = FALSE;
}

void aishipStatsPrint(sdword *y)
{
    if (aishipStats.isDisplayable)
    {
        fontPrintf(0,*y += 20,colWhite, "AIShip state:%d walks:%d considered:%d",
                   aishipStats.state, aishipStats.avoidedWalks, aishipStats.avoidedConsidered);

        *y += 20;
        if (aishipStats.passingOnRight)
        fontPrintf(0,*y,colWhite, "AIShip passingonright:(%f %f %f)",
           aishipStats.passingOnRightVec.x,aishipStats.passingOnRightVec.y,aishipStats.passingOnRightVec.z);

        *y += 20;
        if (aishipStats.repulsenum)
        fontPrintf(0,*y,colWhite, "AIShip Repulse %d:(%f %f %f)",
           aishipStats.repulsenum, aishipStats.repulse.x, aishipStats.repulse.y, aishipStats.repulse.z);

        fontPrintf(0,*y += 20,colWhite,"distToColl:%f desiredVel:(%f %f %f)",
                   aishipStats.distanceToCollision,aishipStats.desiredVel.x,aishipStats.desiredVel.y,aishipStats.desiredVel.z);

        fontPrintf(0,*y += 20,colWhite,"desiredHeading:(%f %f %f) actualHeading:(%f %f %f)",
                   aishipStats.desiredHeading.x,aishipStats.desiredHeading.y,aishipStats.desiredHeading.z,aishipStats.actualHeading.x,aishipStats.actualHeading.y,aishipStats.actualHeading.z);
    }
}

#endif

/*-----------------------------------------------------------------------------
    Name        : aishipPrecalcInfo
    Description : precalculates information used by the AIShip routines and puts
                  it in the ShipStaticInfo structure.
    Inputs      : shipstatinfo
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void aishipPrecalcInfo(ShipStaticInfo *shipstatinfo)
{
    shipstatinfo->shipOrDerelict.sinbank = 0.0f;
    shipstatinfo->shipOrDerelict.pitchturn = 0.0f;
    shipstatinfo->shipOrDerelict.pitchdescend = 0.0f;

    switch (shipstatinfo->shipclass)
    {
        case CLASS_Fighter:
            shipstatinfo->shipOrDerelict.sinbank = FIGHTER_BANK;
            break;

        case CLASS_Corvette:
            shipstatinfo->shipOrDerelict.sinbank = CORVETTE_BANK;
            break;

        case CLASS_Frigate:
            shipstatinfo->shipOrDerelict.sinbank = FRIGATE_BANK;
            shipstatinfo->shipOrDerelict.pitchturn = FRIGATE_TURNPITCH;
            shipstatinfo->shipOrDerelict.pitchdescend = FRIGATE_DESCENDPITCH;
            break;

        case CLASS_Destroyer:
            shipstatinfo->shipOrDerelict.sinbank = DESTROYER_BANK;
            shipstatinfo->shipOrDerelict.pitchturn = DESTROYER_TURNPITCH;
            shipstatinfo->shipOrDerelict.pitchdescend = DESTROYER_DESCENDPITCH;
            break;

        case CLASS_Carrier:
        case CLASS_HeavyCruiser:
            shipstatinfo->shipOrDerelict.sinbank = MASSIVESHIP_BANK;
            shipstatinfo->shipOrDerelict.pitchturn = MASSIVESHIP_TURNPITCH;
            shipstatinfo->shipOrDerelict.pitchdescend = MASSIVESHIP_DESCENDPITCH;
            break;

        default:
            switch (shipstatinfo->shiptype)
            {
                // CAT: no HW1 SensorArray, CryoTray, P2Mothership, P3Megaship or ResourceController cases (M2-R4 ships-b);
                //      the exe's types 0x60, 0x62, 0x65, 0x68, 0x6d, 0x82 (aishipPrecalcInfo 0x00413bd0; dRKO and
                //      dBeastMothership are Cataclysm's)
                case p1Mothership:
                case trFloatingCity:
                case trResearchStation:
                case trGhostship:
                case dRKO:
                case dBeastMothership:
                    shipstatinfo->shipOrDerelict.sinbank = MASSIVESHIP_BANK;
                    shipstatinfo->shipOrDerelict.pitchturn = MASSIVESHIP_TURNPITCH;
                    shipstatinfo->shipOrDerelict.pitchdescend = MASSIVESHIP_DESCENDPITCH;
                    break;
                default:
                    break;
            }
            break;
    }
}

/*-----------------------------------------------------------------------------
    Name        : getPitchToDescend
    Description : returns the pitch in radians the ship should be at, given the
                  normalized distance left zdistanceleft, and pitchdescend (maximum
                  pitch change)
    Inputs      : zdistanceleft, pitchdescend
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
real32 getPitchToDescend(real32 zdistanceleft,real32 pitchdescend)
{
    if (zdistanceleft < 0.0f)
    {
        zdistanceleft = -zdistanceleft;

        if (zdistanceleft < 0.0f)
        {
            return 0.0f;
        }
        else if (zdistanceleft < DESCEND_PITCH4_DIST)       // 0 to DESCEND_PITCH4_DIST
        {
            return pitchdescend * zdistanceleft * oneOverDESCEND_PITCH4_DIST;
        }
        else if (zdistanceleft < DESCEND_PITCH3_DIST)       // DESCEND_PITCH4_DIST to DESCEND_PITCH3_DIST
        {
            return pitchdescend * (DESCEND_PITCH3_DIST - zdistanceleft) * oneOverDESCEND_3_MINUS_4_DIST;
        }
        else if (zdistanceleft < DESCEND_PITCH2_DIST)       // DESCEND_PITCH3_DIST to DESCEND_PITCH2_DIST
        {
            return -pitchdescend * (zdistanceleft - DESCEND_PITCH3_DIST) * oneOverDESCEND_2_MINUS_3_DIST;
        }
        else                                                // DESCEND_PITCH2_DIST +
        {
            return -pitchdescend;
        }
    }
    else
    {
        if (zdistanceleft < 0.0f)
        {
            return 0.0f;
        }
        else if (zdistanceleft < DESCEND_PITCH4_DIST)       // 0 to DESCEND_PITCH4_DIST
        {
            return -pitchdescend * zdistanceleft * oneOverDESCEND_PITCH4_DIST;
        }
        else if (zdistanceleft < DESCEND_PITCH3_DIST)       // DESCEND_PITCH4_DIST to DESCEND_PITCH3_DIST
        {
            return -pitchdescend * (DESCEND_PITCH3_DIST - zdistanceleft) * oneOverDESCEND_3_MINUS_4_DIST;
        }
        else if (zdistanceleft < DESCEND_PITCH2_DIST)       // DESCEND_PITCH3_DIST to DESCEND_PITCH2_DIST
        {
            return pitchdescend * (zdistanceleft - DESCEND_PITCH3_DIST) * oneOverDESCEND_2_MINUS_3_DIST;
        }
        else                                                // DESCEND_PITCH2_DIST +
        {
            return pitchdescend;
        }
    }
}

void aishipGetTrajectoryWithVelPrediction(Ship *ship,SpaceObjRotImpTarg *target,real32 bulletspeed,vector *trajectory)
{
    vector distToTargetVec;
    real32 distToTargetMag;
    real32 bullettraveltime;
    vector deltatarget;

    vecSub(distToTargetVec,target->impactable.collInfo.collPosition,ship->obj.posinfo.position); // MUST be relative to ship->obj.posinfo.position, because
                                                                                  // later it is modified into the ship's coordinate system
                                                                                  // for gun firing, which is based on ship->obj.posinfo.position
    if (bulletspeed <= DONTUSEVELOCITYPRED_IFBELOW)
    {
        // don't bother with velocity prediction for 0 bulletspeed
        goto settrajectory;
    }

    // CAT (M2-R8 oracle-rung-physics): the x87 shape (aishipGetTrajectoryWithVelPrediction 0x00413e10): the double
    //      (z*z + y*y) + x*x into fsqrt, divided by the bullet speed as a double; x and y of the velocity times it are
    //      stored before they are added, z's product is added with one rounding
    {
        double traveltime = catFsqrtD(((double)distToTargetVec.z * distToTargetVec.z + (double)distToTargetVec.y * distToTargetVec.y) +
                                      (double)distToTargetVec.x * distToTargetVec.x) / bulletspeed;

        deltatarget.x = (real32)(traveltime * target->obj.posinfo.velocity.x);
        deltatarget.y = (real32)(traveltime * target->obj.posinfo.velocity.y);
        distToTargetVec.x += deltatarget.x;
        distToTargetVec.y += deltatarget.y;
        distToTargetVec.z = (real32)(traveltime * target->obj.posinfo.velocity.z + distToTargetVec.z);
        distToTargetMag = 0.0f;
        bullettraveltime = (real32)traveltime;
        (void)distToTargetMag;
        (void)bullettraveltime;
    }

settrajectory:
    *trajectory = distToTargetVec;
}

void aishipGetTrajectory(Ship *ship,SpaceObjRotImpTarg *target,vector *trajectory)
{
    vecSub(*trajectory,target->impactable.collInfo.collPosition,ship->obj.posinfo.position);
}

void aishipGetTrajectoryWithAngleCorrection(Ship *ship,SpaceObjRotImpTarg *target,vector *trajectory)
{
    vector futurepos = ship->obj.posinfo.position;
    double futurex;

    // CAT (M2-R10 oracle-rung-physics): the x87 shape (aishipGetTrajectoryWithAngleCorrection 0x00413f10): the future
    //      x stays on the FPU and the x difference is rounded once (0x00413f17 .. 0x00413f6d); y and z are stored first
    futurex = (double)ship->obj.posinfo.velocity.x * 0.25f + futurepos.x;
    futurepos.y += (ship->obj.posinfo.velocity.y * 0.25f);
    futurepos.z += (ship->obj.posinfo.velocity.z * 0.25f);

    trajectory->x = (real32)((double)target->impactable.collInfo.collPosition.x - futurex);
    trajectory->y = target->impactable.collInfo.collPosition.y - futurepos.y;
    trajectory->z = target->impactable.collInfo.collPosition.z - futurepos.z;
}

#define STATE_POINT_IN_DIRECTION    1
#define STATE_START_FLYING          2

// CAT (M2-R8 oracle-rung-physics): the x87 shape (GetCollSizeInDirection 0x00413f90): the three components are
//      doubles, (d.z*c.z + d.y*c.y) + d.x*c.x each, made positive, summed as (heading*forward + right*right) + up*up and
//      halved; the exe returns that double on the FPU, which its callers add or store as they need (catGetCollSizeD)
static double catGetCollSizeD(SpaceObjRotImp *obj, const vector *dir)
{
    const matrix *m = &obj->rotatable.rotinfo.coordsys;
    StaticCollInfo *collInfo = &obj->obj.staticinfo->staticCollInfo;
    double upcomp = ((double)dir->z * m->m31 + (double)dir->y * m->m21) + (double)dir->x * m->m11;
    double rightcomp = ((double)dir->z * m->m32 + (double)dir->y * m->m22) + (double)dir->x * m->m12;
    double headingcomp = ((double)dir->z * m->m33 + (double)dir->y * m->m23) + (double)dir->x * m->m13;

    if (upcomp < 0.0) upcomp = -upcomp;
    if (rightcomp < 0.0) rightcomp = -rightcomp;
    if (headingcomp < 0.0) headingcomp = -headingcomp;

    return ((headingcomp * collInfo->forwardlength + rightcomp * collInfo->rightlength) + upcomp * collInfo->uplength) * 0.5;
}

real32 GetCollSizeInDirection(SpaceObjRotImp *obj,vector dir)
{
    return (real32)catGetCollSizeD(obj, &dir);
}

/*-----------------------------------------------------------------------------
    Name        : rowSetDetails
    Description : sets right of way details for a ship (callback function for statscript)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
static void rowSetDetails(char *directory,char *field,void *dataToFillIn)
{
    char shipstr[50];
    sdword rowpri;
    ShipType shiptype;
    real32 rowAvoidByVal;

    RemoveCommasFromString(field);

    sscanf(field,"%s %d %f",shipstr,&rowpri,&rowAvoidByVal);

    shiptype = StrToShipType(shipstr);
    if ((shiptype < 0) || (shiptype >= TOTAL_NUM_SHIPS))
    {
        dbgFatalf(DBG_Loc,"Invalid shiptype read at line %s in aiship.script",field);
    }

    rightOfWays[shiptype] = (sbyte)rowpri;
    rowAvoidBy[shiptype] = rowAvoidByVal;
}

/*-----------------------------------------------------------------------------
    Name        : GetDirectionOfTravel
    Description : gets the direction of travel of ship
                  bases direction of travel based on where it's moving to if it's got a move command, otherwise
                  bases direction of travel on velocity of ship
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void GetDirectionOfTravel(Ship *ship,vector *dirOfTravel)
{
    if (!vecIsZero(ship->moveTo))
    {
        vecSub(*dirOfTravel,ship->moveTo,ship->moveFrom);
    }
    else
    {
        *dirOfTravel = ship->obj.posinfo.velocity;
    }
}

/*-----------------------------------------------------------------------------
    Name        : rowRemoveShipFromGettingOutOfWay
    Description : removes getting out of way status from ship
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rowRemoveShipFromGettingOutOfWay(Ship *ship)
{
    if (!(ship->shipOrDerelict.specialFlags & SPECIAL_rowGettingOutOfWay))
    {
        return;
    }

    bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_rowGettingOutOfWay);
    //bitClear(ship->dontrotateever,2);
    bitClear(ship->dontapplyforceever,2);

    ship->rowGetOutOfWay = NULL;
}

/*-----------------------------------------------------------------------------
    Name        : rowGetOutOfWayShipDiedCB
    Description : callback function notify for when rowGetOutOfWay ship dies
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rowGetOutOfWayShipDiedCB(Ship *ship)
{
    ;
}

#define ROW_CAN_GET_OUT_OF_WAY      1
#define ROW_CANNOT_GET_OUT_OF_WAY   0
#define ROW_YOU_STILL_AVOID_ME      -1

/*-----------------------------------------------------------------------------
    Name        : rowShipCanGetOutOfWayOfMe
    Description : returns TRUE if the ship can get out of the way of me
    Inputs      : ship, me
    Outputs     :
    Return      : returns TRUE if the ship can get out of the way of me
----------------------------------------------------------------------------*/
sdword rowShipCanGetOutOfWayOfMe(Ship *ship,Ship *me)
{
    CommandToDo *shipcommand = getShipAndItsCommand(&universe.mainCommandLayer,ship);
    CommandToDo *mecommand;

    // CAT: no HW1 Drone (M2-R4 ships-b)

    if (shipcommand == NULL)
    {                   // ship not doing anything, so it can get out of way
        return ROW_CAN_GET_OUT_OF_WAY;
    }

    mecommand = getShipAndItsCommand(&universe.mainCommandLayer,me);

    if (shipcommand == mecommand)
    {
        return ROW_YOU_STILL_AVOID_ME;       // if ship and me are in the same command, we shouldn't use right of way
    }

    if (shipcommand->ordertype.attributes & COMMAND_MASK_PROTECTING)
    {
        return ROW_CANNOT_GET_OUT_OF_WAY;       // ships that are protecting are avoiding stuff anyway, shouldn't use right of way
    }

    if (mecommand && (mecommand->ordertype.attributes & COMMAND_MASK_PROTECTING))
    {
        return ROW_CANNOT_GET_OUT_OF_WAY;       // ships that are protecting are avoiding stuff anyway, shouldn't use right of way
    }

    switch (shipcommand->ordertype.order)
    {
        case COMMAND_NULL:
        case COMMAND_HALT:
            return ROW_CAN_GET_OUT_OF_WAY;

        case COMMAND_MILITARY_PARADE:
            if (shipcommand->militaryParade->aroundShip == me)
            {
                return ROW_CANNOT_GET_OUT_OF_WAY;       // ships in my parade shouldn't try to avoid me!
            }
            return ROW_CAN_GET_OUT_OF_WAY;

        case COMMAND_MOVE:
        case COMMAND_DOCK:
        case COMMAND_ATTACK:
        case COMMAND_SPECIAL:
            return ROW_CANNOT_GET_OUT_OF_WAY;

        default:
            return ROW_CANNOT_GET_OUT_OF_WAY;
    }
}

/*-----------------------------------------------------------------------------
    Name        : rowOriginalPointIsClear
    Description : returns TRUE if original point is clear
                  (right-of-way ship we're getting out way of is not near it and
                   right-of-way ship is not heading in our direction)
    Inputs      :
    Outputs     :
    Return      : returns TRUE if original point is clear
----------------------------------------------------------------------------*/
bool32 rowOriginalPointIsClear(Ship *ship)
{
    real32 avoidcollsize;
    real32 avoidcollpad;
    vector destToObj;
    real32 destToObjMag2;
    Ship *avoidme;

    vector meToShip;
    vector meDirOfTravel;

    dbgAssertOrIgnore(ship->shipOrDerelict.specialFlags & SPECIAL_rowGettingOutOfWay);

    avoidme = ship->rowGetOutOfWay;
    if (avoidme == NULL)
    {
        return TRUE;
    }

    GetDirectionOfTravel(avoidme,&meDirOfTravel);

    vecSub(meToShip,ship->impactable.collInfo.collPosition,avoidme->impactable.collInfo.collPosition);

    if (vecDotProduct(meToShip,meDirOfTravel) > 0.0f)
    {
        // right-of-way ship (avoidme) is still heading in our direction, must wait till it is not
        return FALSE;
    }

    avoidcollsize = avoidme->staticinfo->common.staticCollInfo.avoidcollspheresize;
    avoidcollpad = avoidme->staticinfo->common.staticCollInfo.avoidcollspherepad;

    vecSub(destToObj,avoidme->impactable.collInfo.collPosition,ship->rowOriginalPoint);
    destToObjMag2 = vecMagnitudeSquared(destToObj);

    if (destToObjMag2 < (avoidcollsize*avoidcollsize))
    {
        return FALSE;
    }

    // later check all ships in nearest blob too?

    return TRUE;
}

#define rowHANG_AROUND_TILL_SHIP_OUT_OF_WAY     1
#define rowFLY_BACK_TO_ORIGINAL_POINT           2

/*-----------------------------------------------------------------------------
    Name        : rowFlyShipOutOfWay
    Description : flys ship out of way
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rowFlyShipOutOfWay(Ship *ship)
{
    dbgAssertOrIgnore(ship->shipOrDerelict.specialFlags & SPECIAL_rowGettingOutOfWay);

    //bitClear(ship->dontrotateever,2);
    bitClear(ship->dontapplyforceever,2);

    switch (ship->rowState)
    {
        case 0:     // fly to rowOutOfWayPoint
            if (ship->rowGetOutOfWay == NULL)
            {
                ship->rowState = rowFLY_BACK_TO_ORIGINAL_POINT;
                break;
            }
            if (rowOriginalPointIsClear(ship))
            {
                ship->rowState = rowFLY_BACK_TO_ORIGINAL_POINT;
                break;
            }
            if (MoveReachedDestinationVariable(ship,&ship->rowOutOfWayPoint,50.0f))
            {
                ship->rowState = rowHANG_AROUND_TILL_SHIP_OUT_OF_WAY;
                break;
            }
            aishipFlyToPointAvoidingObjs(ship,&ship->rowOutOfWayPoint,0,0.0f);
            break;

        case rowHANG_AROUND_TILL_SHIP_OUT_OF_WAY:
            if (ship->rowGetOutOfWay == NULL)
            {
                ship->rowState = rowFLY_BACK_TO_ORIGINAL_POINT;
                break;
            }
            if (rowOriginalPointIsClear(ship))
            {
                ship->rowState = rowFLY_BACK_TO_ORIGINAL_POINT;
                break;
            }
            // just hover around here
            aishipFlyToPointAvoidingObjs(ship,NULL,0,0.0f);
            break;

        case rowFLY_BACK_TO_ORIGINAL_POINT:
            if (MoveReachedDestinationVariable(ship,&ship->rowOriginalPoint,50.0f))
            {
                rowRemoveShipFromGettingOutOfWay(ship);     // we're done!
                return;
            }
            if (aishipFlyToPointAvoidingObjs(ship,&ship->rowOriginalPoint,AISHIP_DontFlyToObscuredPoints|AISHIP_ReturnImmedIfPointObscured,0.0f) & AISHIP_FLY_OBJECT_IN_WAY)
            {
                rowRemoveShipFromGettingOutOfWay(ship);     // we're done!
                return;
            }
            break;

        default:
            dbgAssertOrIgnore(FALSE);
            break;
    }

    //bitSet(ship->dontrotateever,2);
    bitSet(ship->dontapplyforceever,2);
}

/*-----------------------------------------------------------------------------
    Name        : rowSignalShipToGetOutOfWayOfMe
    Description : tells ship to get out of the way of me
    Inputs      : ship, me
                  me is the right of way ship (get out of way of me)
                  ship is avoiding ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rowSignalShipToGetOutOfWayOfMe(Ship *ship,Ship *me,vector *meToShip,vector *meDirOfTravel)
{
    vector delta;

    ship->shipOrDerelict.specialFlags |= SPECIAL_rowGettingOutOfWay;
    //bitSet(ship->dontrotateever,2);
    bitSet(ship->dontapplyforceever,2);

    ship->rowState = 0;
    ship->rowGetOutOfWay = me;
    ship->rowOriginalPoint = ship->obj.posinfo.position;

    // calculate outOfWayPoint;

    // CAT: no HW1 special case for the R1 Mothership (rowSignalShipToGetOutOfWayOfMe 0x00414400 has none: every ship
    //      parts by its rowAvoidBy distance)
    vector meDirOfTravelNorm = *meDirOfTravel;
    vector meToShipNorm;
    real32 avoidby = rowAvoidBy[me->shiptype];

    if (avoidby == 0.0f) avoidby = me->staticinfo->common.staticCollInfo.avoidcollspheresize*AVOID_ROW_SHIP_BY_SCALE + AVOID_ROW_SHIP_BY_ADD;

    vecNormalize(&meDirOfTravelNorm);

    if ((meDirOfTravelNorm.z > ROW_GOING_MOSTLY_UPDOWN_ANGLE) || (meDirOfTravelNorm.z < -ROW_GOING_MOSTLY_UPDOWN_ANGLE))
    {
        // ship we are trying to avoid is going mainly up/down, so we should move on x-y plane.
        vector directionOfParting;
        vector up = { 0.0f, 0.0f, 1.0f };
        vector test;

        meToShipNorm = *meToShip;
        meDirOfTravelNorm = *meDirOfTravel;

        meToShipNorm.z = 0.0f;
        meDirOfTravelNorm.z = 0.0f;
        vecNormalize(&meToShipNorm);
        vecNormalize(&meDirOfTravelNorm);

        // find direction ships should part in (should be on x-y plane and perpendicular to direction of movement of me)
        vecCrossProduct(directionOfParting,meDirOfTravelNorm,up);

        // now find out if we should go in directionOfParting or -directionOfParting
        vecCrossProduct(test,meDirOfTravelNorm,meToShipNorm);
        if (test.z > 0.0f)
        {
            vecNegate(directionOfParting);
        }

        vecScalarMultiply(delta,directionOfParting,avoidby);
    }
    else
    {
        // move up or down to get out of way

        if ((-meToShip->z) > (avoidby*ROW_GO_DOWN_INSTEAD_OF_UP_RATIO)) // if me is above ship by this let's go down
        {
            // go down
            vecSet(delta,0.0f,0.0f,-avoidby);
        }
        else
        {
            // go up
            vecSet(delta,0.0f,0.0f,avoidby);
        }
    }

    vecAdd(ship->rowOutOfWayPoint,ship->obj.posinfo.position,delta);
}

/*-----------------------------------------------------------------------------
    Name        : rowSignalLeaderInFormationToGetOutOfWayOfMe
    Description : tells other ships in ship's formation to get out of the way of me
    Inputs      : ship, me
                  me is the right of way ship (get out of way of me)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rowSignalLeaderInFormationToGetOutOfWayOfMe(Ship *ship,Ship *me,vector *meToShip,vector *meDirOfTravel)
{
    SelectCommand *selection = cat_shipFormationCommand(ship)->selection;
    //sdword numShips;
    //sdword i;
    Ship *shipi;

    dbgAssertOrIgnore(selection);
    dbgAssertOrIgnore(selection->numShips > 0);
    //numShips = selection->numShips;

    shipi = selection->ShipPtr[0];
    if (!(shipi->shipOrDerelict.specialFlags & SPECIAL_rowGettingOutOfWay))    // not already getting out of way
    {
        rowSignalShipToGetOutOfWayOfMe(shipi,me,meToShip,meDirOfTravel);
    }
}

udword aishipFlyToPointAvoidingObjsFunc(Ship *ship,vector *destination,udword aishipflags,real32 limitvel,vector *withVel)
{
    vector desiredVel;
    vector desiredHead;
    vector targetVec;
    sdword objindex = 0;
    SpaceObjRotImp *avoidobj = NULL;
    ShipStaticInfo *shipstaticinfo = ship->staticinfo;

    blob *thisBlob = NULL;
    SelectAnyCommand *targetsAvoid = NULL;

    vector repulse;
    vector repulsenorm;
    real32 repulsedist;
    real32 repulsemag2;
#if VELFACTOR
    real32 velfactor;
#endif
    real32 collidedist;
    real32 distanceToCollision;

    real32 testExactOpposite;
    real32 shipcollsize,shipcollpad,shiprowcollpad;
    real32 avoidcollsize = 0,avoidcollpad = 0;

    real32 maxavoidcollsize,maxavoidcollpad;

    real32 maxdistconsider;
    real32 temp;

    real32 zdistanceleft;
    real32 pitchtodescend;

    udword returnflag = 0;

#ifdef HW_BUILD_FOR_DEBUGGING
    udword considered = 0;
#endif

    sdword pass;
    sdword bottom,top;
    sdword numTargets;

    real32 distcheck;
    real32 maxpossibledist;

    real32 obscureaddcollsize = 0;

    real32 destToObjDist;
    vector destToObjNorm;

    bool32 rowEnabled;

    CommandToDo *shipcommand = ship->command;

    real32 useAVOID_OBJ_SCALE_FACTOR = AVOID_OBJ_SCALE_FACTOR;

    if (ship->dontapplyforceever)
    {
        return 0;     // no point wasting our time
    }

    if (ship->staticinfo->common.immobile)
    {
        return 0;     // no point wasting our time
    }

    // CAT: in single player a ship whose hyperspace effect plays is not flown (aishipFlyToPointAvoidingObjsFunc
    //      0x00414610 at 0x0041468d: shipSinglePlayerGameInfo +0x4C4, its hyperspaceEffect +0x1C)
    if (singlePlayerGame && (ship->shipSinglePlayerGameInfo != NULL) &&
        (ship->shipSinglePlayerGameInfo->hyperspaceEffect != NULL))
    {
        return 0;
    }

    PTSTART(2);

    if (isStrikeCraft(ship->shiptype))
    {
        if ((shipcommand) && (shipcommand->ordertype.order == COMMAND_ATTACK))
        {
            useAVOID_OBJ_SCALE_FACTOR = COMBAT_AVOID_OBJ_SCALE_FACTOR;
        }
    }
    // CAT: no HW1 Probe (M2-R4 ships-b)

    aishipStatsInit(ship);

    aishipStatsState(ship->shipOrDerelict.aistate);

    switch (ship->shipOrDerelict.aistate)
    {
/*=============================================================================
    State 0
=============================================================================*/
        case 0:
            if (shipstaticinfo->shipOrDerelict.pitchdescend != 0.0f)
            {
                if (destination)
                {
                    real32 xdistanceleft = (destination->x - ship->obj.posinfo.position.x);
                    real32 ydistanceleft = (destination->y - ship->obj.posinfo.position.y);
                    real32 r;
                    zdistanceleft = ABS(destination->z - ship->obj.posinfo.position.z);
                    r = fsqrt(xdistanceleft*xdistanceleft + ydistanceleft*ydistanceleft);
                    if ((zdistanceleft < MIN_DIST_FOR_FANCY_DESCEND) ||
                        (zdistanceleft < (r*MIN_ANGLE_FOR_FANCY_DESCEND)))
                    {
                        ship->aidescend = 0.0f;                     // set to 0 to indicate no fancy descend
                    }
                    else
                    {
                        ship->aidescend = 1.0f / zdistanceleft;     // used to normalize distance left to go in descend

                        if ((r < MIN_DIST_TO_TRACK_HEADING) ||
                            (zdistanceleft > (r*MAX_ANGLE_FOR_TRACK_HEADING)))
                        {
                            ship->aidescend = -ship->aidescend;     // -ve aidescend indicates don't track heading as well
                        }
                    }
                }
                else
                {
                    ship->aidescend = 0.0f;                         // set to 0 to indicate no fancy descend
                }
            }
            ship->shipOrDerelict.aistate = STATE_POINT_IN_DIRECTION;
            // deliberately fall through to STATE_POINT_IN_DIRECTION

        case STATE_POINT_IN_DIRECTION:
            if (bitTest(aishipflags,(AISHIP_FirstPointInDirectionFlying+AISHIP_CarTurn)))
            {
        #if DEBUG_AISHIP
                 dbgMessagef("Orienting ship for flight");
        #endif
                dbgAssertOrIgnore(destination);     // must have destination for AISHIP_FirstPointInDirectionFlying or AISHIP_CarTurn to have any meaning
                vecSub(targetVec,*destination,ship->obj.posinfo.position);

                vecNormalize(&targetVec);

                if (shipstaticinfo->shipOrDerelict.pitchdescend == 0.0f)
                {
                    if (aitrackHeadingWithBankFlags(ship,&targetVec,0.96f,shipstaticinfo->shipOrDerelict.sinbank,AITRACKHEADING_IGNOREUPVEC))
                    {
                        ship->shipOrDerelict.aistate = STATE_START_FLYING;
                        aishipStatsClose();
                        PTEND(2);
                        return 0;
                    }
                }
                else
                {
                    if ((ship->aidescend < 0.0f) || (aitrackHeadingWithBankPitchFlags(ship,&targetVec,0.96f,shipstaticinfo->shipOrDerelict.sinbank,0.0f,shipstaticinfo->shipOrDerelict.pitchturn,AITRACKHEADING_IGNOREUPVEC)))
                    {
                        ship->shipOrDerelict.aistate = STATE_START_FLYING;
                        aishipStatsClose();
                        PTEND(2);
                        return 0;
                    }
                }

                if (bitTest(aishipflags,AISHIP_CarTurn) && (!isCapitalShipStaticOrBig(shipstaticinfo)))
                {
                    physApplyForceToObj(&ship->obj,ship->guidance.nonstatvars.thruststrength[TRANS_FORWARD]*CAR_TURN_POWER,TRANS_FORWARD);
                }
                else
                {
                    aitrackSteadyShipDriftOnly(ship);
                }
            }
            else
            {
                ship->shipOrDerelict.aistate = STATE_START_FLYING;
                goto startflying;
            }
            aishipStatsClose();
            PTEND(2);
            return 0;

/*=============================================================================
    State START_FLYING
=============================================================================*/
startflying:
        case STATE_START_FLYING:
#if HWI_ORACLE
            oracleShipDumpFly(ship, destination, aishipflags, limitvel, withVel);   // HWI-ORACLE (M2-R9): /oracleShipDump
#endif

            if (destination)
            {
                vecSub(targetVec,*destination,ship->obj.posinfo.position);

                if ((aishipflags & AISHIP_FastAsPossible) == 0)
                {
                    vecMultiplyByScalar(targetVec,VELOCITY_SCALE_FACTOR);
                }
                // else assumes URGENT_SCALE_FACTOR is 1!
            }
            else
            {
                vecZeroVector(targetVec);
            }

            // CAT: an escorted ship flies no faster than its escort (Ship.catProtectedSpeedLimit, set by
            //      cat_univLimitProtectedShipSpeed 0x00544e00; aishipFlyToPointAvoidingObjsFunc 0x004149c6)
            if (ship->catProtectedSpeedLimit > 0.0f)
            {
                limitvel = ship->catProtectedSpeedLimit;
            }

            // CAT: no Ship.formationcommand: the formation is the command with COMMAND_MASK_FORMATION, and a docking
            //      command never sets the leader's speed (0x004149e2: order != COMMAND_DOCK); the force-field sphere
            //      (FORCEFIELD_SPHERE_FORMATION 10) is treated as the sphere (0x00414a19)
            if ((limitvel == 0.0f) && (shipcommand != NULL) && (shipcommand->ordertype.order != COMMAND_DOCK) &&
                cat_shipFormationCommand(ship) && (ship == shipcommand->selection->ShipPtr[0]))
            {
                // ship is leader of formation, it may potentially travel slower so other ships can maintain formation

                if (((shipcommand->formation.formationtype != SPHERE_FORMATION) &&
                     (shipcommand->formation.formationtype != FORCEFIELD_SPHERE_FORMATION)) ||
                    (!((shipcommand->ordertype.attributes & COMMAND_MASK_PROTECTING) || (shipcommand->formation.enders)))
                   )
                {
                    limitvel = shipcommand->formation.travelvel;
                }
            }

            if (limitvel < 0.0f)
            {
                vecCapMinVector(&targetVec,-limitvel);
            }

            if (withVel != NULL)
            {
                vecAddTo(targetVec,*withVel);
            }

            if (limitvel > 0.0f)
            {
                vecCapVector(&targetVec,limitvel);
            }
            else
            {
                // CAT: the top speed carries the speed multiplier (cat_shipGetSpeedMultiplier 0x00514940;
                //      0x00414ab2), as in physUpdateObjPosVelShip
                real32 velMult = cat_shipGetSpeedMultiplier(ship);
                velMult = (real32)(tacticsGetShipsMaxVelocity(ship) * velMult);     // CAT: one rounding (0x00414ac9)
                vecCapVector(&targetVec,velMult);
            }

        #if DEBUG_AISHIP
            dbgMessagef("Flying to target vector: (%f %f %f)",targetVec.x,targetVec.y,targetVec.z);
        #endif

            desiredVel = targetVec;
#if HWI_ORACLE
            oracleShipDumpVector(ship, "target", &targetVec);   // HWI-ORACLE (M2-R8 oracle-rung-physics): /oracleShipDump
#endif

            if (aishipTempDisableAvoiding)
            {
                goto noavoid;
            }

#ifdef HW_BUILD_FOR_DEBUGGING
            if (!DO_AVOID_OBJS)
            {
                goto noavoid;
            }
#endif

            if (ship->shipOrDerelict.dockingship != NULL)       // don't avoid any objects if docking
            {
                goto noavoid;
            }
            // CAT (M2-R5 dock): aishipFlyToPointAvoidingObjsFunc 0x00413e40 also skips avoidance for a ship flagged
            //      SPECIAL_2_CatNoAvoidance (0x40), which the dock state machine (cat_dockShipDocksAtShip 0x00465860), the
            //      link order, the Leech, the salvagers and the ramming frigate set while they fly into a hull
            if (ship->shipOrDerelict.specialFlags2 & SPECIAL_2_CatNoAvoidance)
            {
                goto noavoid;
            }

            //don't avoid our target if we are in kamikaze mode
            if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            {
                goto noavoid;
            }

#ifdef COLLISION_CHECK_STATS
            shipsavoidingstuff++;
#endif

            // CAT: a mimic avoids with its disguise's radius (cat_mimicGetAvoidRadius 0x005b9550 when CAT_SHIPFLAG_Mimic
            //      is set; 0x00414b28)
            if (ship->header.catFlags18 & CAT_SHIPFLAG_Mimic)
            {
                shipcollsize = cat_mimicGetAvoidRadius(ship);
            }
            else
            {
                shipcollsize = shipstaticinfo->common.staticCollInfo.avoidcollspheresize;
            }
            shipcollpad = shipstaticinfo->common.staticCollInfo.avoidcollspherepad;

            rowEnabled = FALSE;

            // CAT: a fighter or corvette docking at a dock point (COMMAND_DOCK with dockvars.catDockPointIndex != -1)
            //      uses no right of way (0x00414b59)
            if (((shipcommand == NULL) || (shipcommand->ordertype.order != COMMAND_DOCK) ||
                 (ship->dockvars.catDockPointIndex == -1) ||
                 ((shipstaticinfo->shipclass != CLASS_Fighter) && (shipstaticinfo->shipclass != CLASS_Corvette))) &&
                (shipstaticinfo->common.rightOfWay > 0))    // if ship has any right of way
            {
                shiprowcollpad = shipcollsize * AVOID_OBJ_ROW_PADDING_SCALE;
                rowEnabled = TRUE;
            }
            else
                shiprowcollpad = shipcollpad;

            thisBlob = ship->obj.collMyBlob;
            if (thisBlob == NULL)
            {
                dbgMessagef("Warning: flags %x ship %s %s flying with no blob",ship->header.flags,ShipRaceToStr(ship->shiprace),ShipTypeToStr(ship->shiptype));
                goto noavoid;
            }

            // CAT: a ship whose original collision sphere is under 500 skips the small-target pass and avoids only the
            //      big targets (0x00414c8a: ShipStaticInfo +0x48 against the float 500.0 at 0x0072a4ac)
            pass = (shipstaticinfo->common.staticCollInfo.originalCollSphereSize < 500.0f) ? 1 : 0;

passagain:
            if (pass == 0)
            {
                // CAT: a linking Acolyte or Mimic (the link order, 11) skips the small-target pass once its partner is
                //      among the blob's small targets: it flies into the partner's hull (aishipFlyToPointAvoidingObjsFunc
                //      0x00414cb9; the big-target pass still runs: the exe jumps to the pass counter, 0x00415916)
                if (((ship->shiptype == sAcolyte) || (ship->shiptype == bAcolyte) || (ship->shiptype == sMimic)) &&
                    (shipcommand != NULL) && (shipcommand->ordertype.order == COMMAND_CAT_LINK))
                {
                    Ship *partner = ((CatLinkInfo *)((ubyte *)ship->ShipSpecifics + 0x18))->partner;    // Ship+0x650
                    sdword k;

                    for (k = 0; k < thisBlob->blobSmallTargets->numTargets; k++)
                    {
                        if (thisBlob->blobSmallTargets->TargetPtr[k] == (TargetPtr)partner)
                        {
                            goto nextpass;
                        }
                    }
                }

                targetsAvoid = thisBlob->blobSmallTargets;
                dbgAssertOrIgnore(targetsAvoid);
                numTargets = targetsAvoid->numTargets;
                if (numTargets == 0)
                {
                    goto nextpass;
                }
                maxavoidcollsize = thisBlob->blobMaxSTargetCollSphereSize;
                maxavoidcollpad = maxavoidcollsize * AVOID_OBJ_PADDING_SCALE;
                // CAT (M2-R8 oracle-rung-physics): size * scale + pad on the FPU, rounded once (0x00414d28)
                maxpossibledist = (real32)((double)maxavoidcollsize * AVOID_OBJ_PADDING_SCALE + shiprowcollpad);

                // binary search targetindex such that
                // (ship->obj.collOptimizeDist - avoidobj->obj.collOptimizeDist) <= maxpossibledist
                bottom = 0;
                top = numTargets-1;
                while (top >= bottom)
                {
                    objindex = (bottom+top)>>1;
                    if ((ship->obj.collOptimizeDist - targetsAvoid->TargetPtr[objindex]->obj.collOptimizeDist) > maxpossibledist)
                    {
                        // targetindex is too small
                        bottom = objindex+1;
                    }
                    else
                    {
                        top = objindex-1;
                    }
                }

                dbgAssertOrIgnore(objindex >= 0);
                dbgAssertOrIgnore(objindex < numTargets);
            }
            else
            {
                dbgAssertOrIgnore(pass == 1);
                targetsAvoid = thisBlob->blobBigTargets;
                dbgAssertOrIgnore(targetsAvoid);
                numTargets = targetsAvoid->numTargets;
                if (numTargets == 0)
                {
                    goto nextpass;
                }
                maxavoidcollsize = thisBlob->blobMaxBTargetCollSphereSize;
                maxavoidcollpad = maxavoidcollsize * ((maxavoidcollsize > AISHIP_BIGSHIP) ? AVOID_OBJ_PADDING_SCALE_BIG : AVOID_OBJ_PADDING_SCALE);
                // CAT (M2-R8 oracle-rung-physics): size * scale + pad on the FPU, rounded once (0x00414d96)
                maxpossibledist = (real32)((double)maxavoidcollsize * ((maxavoidcollsize > AISHIP_BIGSHIP) ? AVOID_OBJ_PADDING_SCALE_BIG : AVOID_OBJ_PADDING_SCALE) + shiprowcollpad);
                objindex = 0;
            }

            while (objindex < numTargets)
            {
                avoidobj = &targetsAvoid->TargetPtr[objindex]->impactable;

#ifdef COLLISION_CHECK_STATS
                shipsavoidedwalks++;
#endif
                aishipStatsAvoidedWalks();

                if ((distcheck = (avoidobj->obj.collOptimizeDist - ship->obj.collOptimizeDist)) > maxpossibledist)
                {
                    goto nextpass;
                }

                // CAT: a ship does not avoid the ship it keeps at Ship+0x608 (catSalvageBigGun: a towed big gun's
                //      dBigGun, a Worker's repair target) nor a component clamped to that ship, and the other way round;
                //      in single player a trFloatingCity does not avoid a dSlipgateNode nor a dEcclesiam the sMothership
                //      (aishipFlyToPointAvoidingObjsFunc 0x00414e0e .. 0x00414ea0)
                if (avoidobj->header.objtype == OBJ_ShipType)
                {
                    Ship *avoidShip = (Ship *)avoidobj;

                    if ((ship->catSalvageBigGun != NULL) &&
                        ((ship->catSalvageBigGun == avoidShip) ||
                         ((avoidShip->staticinfo->shipclass == CLASS_Component) && (avoidShip->guidance.clampInfo != NULL) &&
                          ((void *)avoidShip->guidance.clampInfo->host == (void *)ship->catSalvageBigGun))))
                    {
                        goto nextnode;
                    }
                    if ((avoidShip->catSalvageBigGun != NULL) &&
                        ((avoidShip->catSalvageBigGun == ship) ||
                         ((shipstaticinfo->shipclass == CLASS_Component) && (ship->guidance.clampInfo != NULL) &&
                          ((void *)ship->guidance.clampInfo->host == (void *)avoidShip->catSalvageBigGun))))
                    {
                        goto nextnode;
                    }
                    if (singlePlayerGame &&
                        (((avoidShip->shiptype == dSlipgateNode) && (ship->shiptype == trFloatingCity)) ||
                         ((avoidShip->shiptype == sMothership) && (ship->shiptype == dEcclesiam))))
                    {
                        goto nextnode;
                    }
                }

                // use more trivial rejections?

                avoidcollpad = avoidobj->obj.staticinfo->staticCollInfo.avoidcollspherepad;

                maxdistconsider = avoidcollpad + shiprowcollpad;

                if (ABS(distcheck) > maxdistconsider)
                {
                    goto nextnode;
                }

                vecSub(repulse,ship->impactable.collInfo.collPosition,avoidobj->collInfo.collPosition);

#ifdef COLLISION_CHECK_STATS
                shipsavoidedchecks++;
#endif

                if (!rowEnabled)
                {
                    goto norowcheck;
                }

                if (!isBetweenInclusive(repulse.x,-maxdistconsider,maxdistconsider))
                {
                    goto nextnode;
                }

                if (!isBetweenInclusive(repulse.y,-maxdistconsider,maxdistconsider))
                {
                    goto nextnode;
                }

                if (!isBetweenInclusive(repulse.z,-maxdistconsider,maxdistconsider))
                {
                    goto nextnode;
                }

                if (shipstaticinfo->common.rightOfWay > avoidobj->obj.staticinfo->rightOfWay)
                {
                    // We have right of way:
                    // CAT: only an allied ship is asked to get out of the way; an enemy ship is avoided only with
                    //      SPECIAL_2_DontPlowThroughEnemyShips; any other object with a lower right of way is not avoided
                    //      (HW1's potshot at an asteroid in the way, attackSimple, and the Asteroid3 bonus are gone:
                    //      aishipFlyToPointAvoidingObjsFunc 0x00414ff0 .. 0x004151d0)
                    bool32 dontBotherAvoiding = TRUE;

                    if (avoidobj->header.flags & SOF_Hide)
                    {
                        goto nextnode;
                    }

                    if (avoidobj->header.objtype == OBJ_ShipType)
                    {
                        Ship* avoidShip = (Ship*)avoidobj;
                        // signal ship avoidobj to get out of way here (assuming it's static and our ship)
                        if (!(avoidShip->shipOrDerelict.specialFlags & SPECIAL_rowGettingOutOfWay))    // not already getting out of way
                        {
                            if (allianceIsShipAlly(avoidShip,ship->playerowner))
                            {
                                sdword canGetOutOfWay = rowShipCanGetOutOfWayOfMe(avoidShip,ship);      // ship is me
                                if (canGetOutOfWay == ROW_CAN_GET_OUT_OF_WAY)
                                {
                                    vector meToShip;
                                    vector meDirOfTravel;

                                    GetDirectionOfTravel(ship,&meDirOfTravel);

                                    vecSub(meToShip,avoidShip->impactable.collInfo.collPosition,ship->impactable.collInfo.collPosition);

                                    if (vecDotProduct(meToShip,meDirOfTravel) > 0.0f)
                                    {
                                        if (cat_shipFormationCommand(avoidShip))   // CAT: no Ship.formationcommand
                                        {
                                            rowSignalLeaderInFormationToGetOutOfWayOfMe(avoidShip,ship,&meToShip,&meDirOfTravel);
                                        }
                                        else
                                        {
                                            rowSignalShipToGetOutOfWayOfMe(avoidShip,ship,&meToShip,&meDirOfTravel);
                                        }
                                    }
                                }
                                else if (canGetOutOfWay == ROW_YOU_STILL_AVOID_ME)
                                {
                                    dontBotherAvoiding = FALSE;
                                }
                            }
                            else if (ship->shipOrDerelict.specialFlags2 & SPECIAL_2_DontPlowThroughEnemyShips)
                            {
                                dontBotherAvoiding = FALSE;
                            }
                        }
                    }

                    if (dontBotherAvoiding)
                    {
                        goto nextnode;
                    }
                }

                maxdistconsider = avoidcollpad + shipcollpad;

                if (ABS(distcheck) > maxdistconsider)
                {
                    goto nextnode;
                }
norowcheck:

                if (!isBetweenInclusive(repulse.x,-maxdistconsider,maxdistconsider))
                {
                    goto nextnode;
                }

                if (!isBetweenInclusive(repulse.y,-maxdistconsider,maxdistconsider))
                {
                    goto nextnode;
                }

                if (!isBetweenInclusive(repulse.z,-maxdistconsider,maxdistconsider))
                {
                    goto nextnode;
                }


                if ((avoidobj->header.flags & (SOF_Hide|SOF_Impactable|SOF_NotBumpable)) != (SOF_Impactable))
                {
                    goto nextnode;      // object must be impactable, bumpable
                }

                if (avoidobj == &ship->impactable)
                {
                    goto nextnode;  // cannot collide with itself
                }

                if (avoidobj->header.objtype == OBJ_ShipType)
                {
                    Ship* avoidShip = (Ship*)avoidobj;

                    // CAT: a mimic is avoided with its disguise's radius (cat_mimicGetAvoidRadius 0x005b9550; 0x004152b5)
                    if (avoidShip->header.catFlags18 & CAT_SHIPFLAG_Mimic)
                    {
                        avoidcollsize = cat_mimicGetAvoidRadius(avoidShip);
                    }
                    else
                    {
                        avoidcollsize = avoidobj->obj.staticinfo->staticCollInfo.avoidcollspheresize;
                    }

                    if (shipcommand)
                    {
                        if (avoidShip->command == shipcommand)
                        {
                            // ship and avoidobj are in the same formation, so we should effectively lower
                            // AVOID_OBJ_PADDING_SCALE to AVOID_OBJ_FORMATION_PADDING_SCALE

                            avoidcollpad = avoidcollsize * AVOID_OBJ_FORMATION_PADDING_SCALE;
                            shipcollpad = shipcollsize * AVOID_OBJ_FORMATION_PADDING_SCALE;
                            // CAT (M2-R8 oracle-rung-physics): the avoided pad stays a double, added to the stored ship
                            //      pad with one rounding (0x004152eb .. 0x00415343)
                            maxdistconsider = (real32)((double)shipcollpad + (double)AVOID_OBJ_FORMATION_PADDING_SCALE * avoidcollsize);
                            goto getmaxdistchecks;
                        }

                        if ( (shipcommand->ordertype.order == COMMAND_MILITARY_PARADE) && (shipcommand->militaryParade->aroundShip == avoidShip))
                        {
                            // CAT: the parade's host pads with the mothership scale when it is of CLASS_Mothership (no
                            //      HW1 race variant), and the ship's own pad is left as it is (0x00415305 .. 0x00415339)
                            if (avoidShip->staticinfo->shipclass == CLASS_Mothership)
                            {
                                avoidcollpad = avoidcollsize * AVOID_OBJ_MSHIP_MILPARADE_PADDING_SCALE;
                            }
                            else
                            {
                                avoidcollpad = avoidcollsize * AVOID_OBJ_FORMATION_PADDING_SCALE;
                            }
                            // CAT (M2-R8 oracle-rung-physics): the same one rounding (0x00415339 .. 0x00415343)
                            maxdistconsider = (real32)((double)shipcollpad + ((avoidShip->staticinfo->shipclass == CLASS_Mothership) ?
                                                       (double)AVOID_OBJ_MSHIP_MILPARADE_PADDING_SCALE : (double)AVOID_OBJ_FORMATION_PADDING_SCALE) * avoidcollsize);
                       getmaxdistchecks:

                            if (!isBetweenInclusive(repulse.x,-maxdistconsider,maxdistconsider))
                            {
                                goto nextnode;
                            }

                            if (!isBetweenInclusive(repulse.y,-maxdistconsider,maxdistconsider))
                            {
                                goto nextnode;
                            }

                            if (!isBetweenInclusive(repulse.z,-maxdistconsider,maxdistconsider))
                            {
                                goto nextnode;
                            }
                        }
                    }

                    if (avoidShip->shipOrDerelict.dockingship == ship)
                    {
                        goto nextnode;  // don't check if docking with ship
                    }

                }
                else
                {
                    avoidcollsize = avoidobj->obj.staticinfo->staticCollInfo.avoidcollspheresize;
                }

                // CAT (M2-R4 combat): a salvager (SPECIAL_CatSalvager) does not avoid its target once inside the
                //      target's cone (the Cataclysm salvager block's noAvoid), nor a salvager with the same target
                //      (aishipFlyToPointAvoidingObjsFunc 0x00414610 at 0x004153c5..); HW1's salvage-state exception
                //      is gone with HW1's salvage code. M2-R7: the target is compared as a pointer, as the exe does
                //      (0x004153f5 cmp ecx,esi): a salvager without a target (NULL) took the member address of NULL
                if (ship->shipOrDerelict.specialFlags & SPECIAL_CatSalvager) {
                    CatSalvagerInfo *salvager = cat_salvageGetSalvagerInfo(ship);
                    if ((void *)salvager->target == (void *)avoidobj) {
                        if (salvager->noAvoid) {
                            goto nextnode;
                        }
                    } else if (avoidobj->header.objtype == OBJ_ShipType) {
                        Ship* avoidShip = (Ship*)avoidobj;
                        if ((avoidShip->shipOrDerelict.specialFlags & SPECIAL_CatSalvager) &&
                            (cat_salvageGetSalvagerInfo(avoidShip)->target != NULL) &&
                            (cat_salvageGetSalvagerInfo(avoidShip)->target == salvager->target)) {
                            // don't avoid other salvagers of the same target
                            goto noavoid;
                        }
                    }
                }

                // CAT: the exe also tests the clamp info for NULL (0x0041545a)
                if ((avoidobj->header.flags & SOF_Clamped) &&
                    (((SpaceObjRotImpTargGuidance*)avoidobj)->clampInfo != NULL) &&
                    ((SpaceObjRotImpTargGuidance*)avoidobj)->clampInfo->host == &ship->targetable) {
                    goto nextnode;
                }

                // CAT: a Sentinel of a force-field sphere avoids neither the other Sentinels of its formation nor the
                //      ship the sphere surrounds (aishipFlyToPointAvoidingObjsFunc 0x0041546c)
                if (((ship->shiptype == sSentinel) || (ship->shiptype == bSentinel)) && (shipcommand != NULL) &&
                    (shipcommand->formation.formationtype == FORCEFIELD_SPHERE_FORMATION))
                {
                    sdword k;

                    for (k = 0; k < shipcommand->selection->numShips; k++)
                    {
                        if ((void *)shipcommand->selection->ShipPtr[k] == (void *)avoidobj)
                        {
                            goto nextnode;
                        }
                    }
                    if ((shipcommand->ordertype.attributes & COMMAND_MASK_PROTECTING) &&
                        ((void *)shipcommand->protect->ShipPtr[0] == (void *)avoidobj))
                    {
                        goto nextnode;
                    }
                }

                // CAT: a fully cloaked object is ignored unless it is a ship the player has detected
                //      (cat_shipIsCloakedFully 0x00436110, cat_shipDetectedByPlayer = sub_53F9F0 0x0053f9f0;
                //      0x004154c2); HW1 asked the proximity sensors (proximityCanPlayerSeeShip)
                if (cat_shipIsCloakedFully((Ship *)avoidobj))
                {
                    if ((avoidobj->header.objtype != OBJ_ShipType) ||
                        !cat_shipDetectedByPlayer((Ship *)avoidobj, ship->playerowner->playerIndex))
                    {
                        goto nextnode;      //don't avoid it, but ignore it
                    }
                }

                if (shipcommand && shipcommand->ordertype.order == COMMAND_COLLECT_RESOURCES &&
                    &shipcommand->collect.resource->impactable == avoidobj) {
                    goto nextnode; // don't avoid asteroids you're harvesting
                }

                // CAT: the squared distance is tested before the square root (0x00415516 .. 0x00415543)
                // CAT (M2-R8 oracle-rung-physics): the x87 shape (0x00415516 .. 0x0041563f): the squared distance
                //      (z*z + y*y) + x*x and maxdistconsider squared are doubles; the double goes to fsqrt and the
                //      distance is stored as a float; the normal is repulse * (1.0f / that float), a double reciprocal,
                //      rounded once per component; the collision distance adds the ship's size (a double) to the avoided
                //      object's size stored as a float, distance - that is a double (raised to 20), times the scale factor, and
                //      its double reciprocal scales the normal, one rounding per component
                {
                    double repulsemag2d = ((double)repulse.z * repulse.z + (double)repulse.y * repulse.y) + (double)repulse.x * repulse.x;
                    double repulsedistd;
                    double invd;
                    double dtc;
                    real32 firstsize;

                    if (repulsemag2d > (double)maxdistconsider * maxdistconsider)
                    {
                        goto nextnode;
                    }
                    repulsedistd = catFsqrtD(repulsemag2d);
                    repulsedist = (real32)repulsedistd;
                    repulsemag2 = (real32)repulsemag2d;

#ifdef HW_BUILD_FOR_DEBUGGING
                    considered++;
#endif
                    aishipStatsAvoidedConsidered();

                    // CAT (M2-R9 oracle-rung-physics): the special case compares fsqrt's unrounded double with 0.1f
                    //      (0x00415554 fst, 0x0041555b fcomp: the stored float is not the one compared)
                    if (repulsedistd < 0.1f)        // special case
                    {
                        repulse.x = 0.1f;
                        repulsedist = 0.1f;
                        repulsemag2 = 0.01f;
                    }

                    invd = 1.0 / (double)repulsedist;
                    repulsenorm.x = (real32)(invd * repulse.x);
                    repulsenorm.y = (real32)(invd * repulse.y);
                    repulsenorm.z = (real32)(invd * repulse.z);

                    // very close, so figure out collidedist based on collision cubes
                    // CAT (M2-R9 oracle-rung-physics): the exe takes the AVOIDED object's size first and stores it as a
                    //      float (0x0041558b push esi = avoidobj, 0x004155c5 fstp dword), then adds the ship's size as
                    //      the unrounded double (0x004155d7 push edi = ship, 0x004155e0 fadd); R8 had the two swapped
                    firstsize = (real32)catGetCollSizeD(avoidobj, &repulsenorm);
                    dtc = (double)repulsedist - (catGetCollSizeD(&ship->impactable, &repulsenorm) + firstsize);
#if HWI_ORACLE
                    oracleShipDumpAvoid(ship, avoidobj, repulsedist, &repulsenorm, catGetCollSizeD(avoidobj, &repulsenorm),
                                        catGetCollSizeD(&ship->impactable, &repulsenorm), dtc);  // HWI-ORACLE (M2-R9): /oracleShipDump
#endif
                    collidedist = (real32)((double)repulsedist - dtc);
#if VELFACTOR
                    velfactor = -vecDotProduct(ship->obj.posinfo.velocity,repulsenorm);
#endif
                    if (dtc < 20.0)
                    {
                        dtc = 20.0;
                    }
                    distanceToCollision = (real32)dtc;

                    aishipStatsDistanceToCollision(distanceToCollision);

                    // CAT: no HW1 R1-mothership flattening of the repulsion (the exe scales the normal at once, 0x00415600)

                    invd = 1.0 / (dtc * useAVOID_OBJ_SCALE_FACTOR);
                    repulse.x = (real32)(invd * repulsenorm.x);
                    repulse.y = (real32)(invd * repulsenorm.y);
                    repulse.z = (real32)(invd * repulsenorm.z);
                    (void)temp;
                }

#if VELFACTOR
                if (velfactor > 1)
                {
                    velfactor *= AVOID_PANIC_FACTOR;
                    vecMultiplyByScalar(repulse,velfactor);
                }
#endif
//#define DEST_OBSCURED_BY_OBJ_PADDING    50.0f

                if (bitTest(aishipflags,AISHIP_DontFlyToObscuredPoints))
                {
                    vector destToObj;
                    real32 destToObjMag2;
                    real32 obscurecollsize;

                    if (avoidobj->header.objtype == OBJ_ShipType)
                    {
                        Ship* avoidShip = (Ship*)avoidobj;
                        if (bitTest(aishipflags,AISHIP_IgnoreFormationObscuredPoints))
                        {
                            // CAT: the same formation command, as aishipFlyToPointAvoidingObjsFunc 0x00415ab2 compares the ships' commands
                            if ((cat_shipFormationCommand(ship) != NULL) && (cat_shipFormationCommand(ship) == avoidShip->command))
                            {
                                goto skipobscurecheck;
                            }
                        }

                        if (bitTest(aishipflags,AISHIP_IgnoreDockWithObscuredPoints))
                        {
                            if (ship->dockvars.dockship == avoidShip)
                            {
                                goto skipobscurecheck;
                            }
                        }
                    }

                    // CAT (M2-R8 oracle-rung-physics): the x87 shape (0x00415682 .. 0x004157b6): the obscuring size
                    //      stays a double, the squared distance is stored as a float and compared with its double square;
                    //      the distance is fsqrt of that float, stored, and the normal is a double reciprocal's product
                    double obscurecollsized = (double)AVOID_OBJ_OBSCUREDPOINT_SCALE * avoidcollsize + obscureaddcollsize; // + DEST_OBSCURED_BY_OBJ_PADDING;

                    obscurecollsize = (real32)obscurecollsized;
                    obscureaddcollsize += AVOID_SPREAD_OUT_DIST;

                    // test if trying to move to a point that is being obscured by avoidobj
                    if (destination)
                    {
                        vecSub(destToObj,avoidobj->collInfo.collPosition,*destination);
                    }
                    else
                    {
                        // no destination provided, so use current ship position
                        vecSub(destToObj,avoidobj->collInfo.collPosition,ship->obj.posinfo.position);
                    }

                    destToObjMag2 = (real32)(((double)destToObj.z * destToObj.z + (double)destToObj.y * destToObj.y) + (double)destToObj.x * destToObj.x);

                    if ((double)destToObjMag2 < (obscurecollsized*obscurecollsized))
                    {
                        if (avoidobj->obj.staticinfo->staticCollInfo.avoidcollmodifier == 0.0f)
                        {
                            goto skipobscurecollsizedirectioncheck;
                        }

                        destToObjDist = (real32)catFsqrtD(destToObjMag2);
                        {
                            double invo = 1.0 / (double)destToObjDist;
                            destToObjNorm.x = (real32)((double)destToObj.x * invo);
                            destToObjNorm.y = (real32)((double)destToObj.y * invo);
                            destToObjNorm.z = (real32)((double)destToObj.z * invo);
                        }

                        if ((double)destToObjDist < catGetCollSizeD(avoidobj,&destToObjNorm))
                        {
skipobscurecollsizedirectioncheck:
                            if (bitTest(aishipflags,AISHIP_ReturnImmedIfPointObscured))
                            {
                                aishipStatsClose();
                                PTEND(2);
                                return AISHIP_FLY_OBJECT_IN_WAY;
                            }
                            else
                            {
                                returnflag |= AISHIP_FLY_OBJECT_IN_WAY;
                            }
                        }
                    }
skipobscurecheck:;
                }

                // test if repulsion vector is opposite of desired velocity by USE_PASS_RIGHT_ANGLE
                // so that we can add pass on right to repulsion vector so objects
                // will be able to go around each other
                // CAT (M2-R8 oracle-rung-physics): the x87 shape (0x004157c6 .. 0x004158d1): the dot product
                //      (t.z*n.z + t.y*n.y) + t.x*n.x is stored as a float; the target vector's square stays a double;
                //      the pass-right vector is (t.y - 0, 0 - t.x, 0), normalised; its scale PASS_RIGHT_FACTOR *
                //      repulsedist is a double; x and y are stored, z's product is added to repulse.z with one rounding
                testExactOpposite = (real32)(((double)targetVec.z * repulsenorm.z + (double)targetVec.y * repulsenorm.y) +
                                             (double)targetVec.x * repulsenorm.x);
                if (testExactOpposite < 0)
                {
                    double targetVecMag2 = ((double)targetVec.z * targetVec.z + (double)targetVec.y * targetVec.y) +
                                           (double)targetVec.x * targetVec.x;
                    // CAT: at least 30 m away (the float 900.0 at 0x0072a4cc, 0x00415813), where HW1 had 20 m
                    if (targetVecMag2 >= (30.0*30.0))     // must be at least 30m away before we consider passing on right (avoid jitter)
                    {
                        if (((double)testExactOpposite*testExactOpposite) >= (targetVecMag2 * USE_PASS_RIGHT_ANGLE))
                        {
                            vector passright;
                            double passrightmag;

                            passright.x = targetVec.y - 0.0f;
                            passright.y = 0.0f - targetVec.x;
                            passright.z = 0.0f;
                            vecNormalize(&passright);

                            passrightmag = (double)PASS_RIGHT_FACTOR * repulsedist;
                            passright.x = (real32)(passrightmag * passright.x);
                            passright.y = (real32)(passrightmag * passright.y);

                            aishipStatsPassingOnRight(passright);

                            repulse.x += passright.x;
                            repulse.y += passright.y;
                            repulse.z = (real32)(passrightmag * passright.z + repulse.z);
                        }
                    }
                }

    #if DEBUG_AISHIP
                dbgMessagef("AVOID %x: (%f %f %f)",avoidobj,repulse.x,repulse.y,repulse.z);
    #endif
                aishipStatsRepulse(repulse);

                vecAddTo(desiredVel,repulse);
#if HWI_ORACLE
                oracleShipDumpVector(ship, "repulse", &repulse);   // HWI-ORACLE (M2-R8 oracle-rung-physics): /oracleShipDump
#endif

        nextnode:
                objindex++;
            }

        nextpass:
            pass++;
            if (pass < 2)
            {
                goto passagain;
            }


#ifdef HW_BUILD_FOR_DEBUGGING
            //dbgMessagef("AVOID considered: %d",considered);
#endif

noavoid:

        #if DEBUG_AISHIP
            dbgMessagef("Final Desired Velocity: (%f %f %f)",desiredVel.x,desiredVel.y,desiredVel.z);
        #endif

            aishipStatsDesiredVel(desiredVel);

            if ((aishipflags & (AISHIP_StopIfVelocityTooLow|AISHIP_DontFlyToObscuredPoints)) &&
                isBetweenExclusive(desiredVel.x,NEG_AVOID_MIN_VEL,AVOID_MIN_VEL) &&
                isBetweenExclusive(desiredVel.y,NEG_AVOID_MIN_VEL,AVOID_MIN_VEL) &&
                isBetweenExclusive(desiredVel.z,NEG_AVOID_MIN_VEL,AVOID_MIN_VEL))
            {
                if (bitTest(aishipflags,AISHIP_ReturnImmedIfPointObscured))
                {
                    aishipStatsClose();
                    PTEND(2);
                    return AISHIP_FLY_OBJECT_IN_WAY;
                }
                else
                {
                    returnflag |= AISHIP_FLY_OBJECT_IN_WAY;
                }
                vecZeroVector(desiredVel);
            }
            else if (bitTest(aishipflags,AISHIP_PointInDirectionFlying))
            {
                if (shipstaticinfo->shipOrDerelict.pitchdescend != 0.0f)
                {
                    // calculate normalize distance left
                    if ((destination) && (ship->aidescend != 0.0f))
                    {
                        zdistanceleft = (destination->z - ship->obj.posinfo.position.z) * ABS(ship->aidescend);
                        pitchtodescend = getPitchToDescend(zdistanceleft,shipstaticinfo->shipOrDerelict.pitchdescend);
#if DEBUG_AISHIP
                        dbgMessagef("zdistanceleft: %f pitchtodescend: %f",zdistanceleft,pitchtodescend);
#endif
                    }
                    else
                    {
                        pitchtodescend = 0.0f;
                    }
                    // use desiredVel for heading, because the heading does not have to be noramlized for this function

                    if ((destination) && (ship->aidescend < 0.0f))
                    {
                        matGetVectFromMatrixCol3(desiredHead,ship->rotatable.rotinfo.coordsys);
                        aitrackHeadingWithBankPitch(ship,&desiredHead,FLYSHIP_HEADINGACCURACY,shipstaticinfo->shipOrDerelict.sinbank,pitchtodescend,shipstaticinfo->shipOrDerelict.pitchturn);
                    }
                    else
                        aitrackHeadingWithBankPitch(ship,&desiredVel,FLYSHIP_HEADINGACCURACY,shipstaticinfo->shipOrDerelict.sinbank,pitchtodescend,shipstaticinfo->shipOrDerelict.pitchturn);
                }
                else
                {
                    // HWI-TEST (M3-R4 nan-positions): the inputs of a normalise that would divide by zero
                    if (catNaNProbeOn && ((desiredVel.x * desiredVel.x + desiredVel.y * desiredVel.y + desiredVel.z * desiredVel.z) == 0.0f ||
                                          isnan(desiredVel.x) || isnan(desiredVel.y) || isnan(desiredVel.z)))
                    {
                        SpaceObjRotImpTarg *tgt = (shipcommand && shipcommand->ordertype.order == COMMAND_ATTACK && shipcommand->attack && shipcommand->attack->numTargets) ? shipcommand->attack->TargetPtr[0] : NULL;
                        dbgMessagef("nanprobe-fly: tick=%u ship=%s#%u desiredVel=(%g,%g,%g) targetVec=(%g,%g,%g) dest=%s(%g,%g,%g) withVel=%s(%g,%g,%g) limitvel=%g flags=0x%x pos=(%g,%g,%g) target=%s objtype=%d tid=%u towner=%d self=%d tpos=(%g,%g,%g) tvel=(%g,%g,%g)",
                                    (unsigned)universe.univUpdateCounter, ShipTypeToStr(ship->shiptype), (unsigned)ship->shipID.shipNumber,
                                    desiredVel.x, desiredVel.y, desiredVel.z, targetVec.x, targetVec.y, targetVec.z,
                                    destination ? "" : "NULL", destination ? destination->x : 0, destination ? destination->y : 0, destination ? destination->z : 0,
                                    withVel ? "" : "NULL", withVel ? withVel->x : 0, withVel ? withVel->y : 0, withVel ? withVel->z : 0,
                                    limitvel, (unsigned)aishipflags, ship->obj.posinfo.position.x, ship->obj.posinfo.position.y, ship->obj.posinfo.position.z,
                                    tgt ? ((tgt->header.objtype == OBJ_ShipType) ? ShipTypeToStr(((Ship *)tgt)->shiptype) : "nonship") : "none", tgt ? (int)tgt->header.objtype : -1,
                                    (tgt && tgt->header.objtype == OBJ_ShipType) ? (unsigned)((Ship *)tgt)->shipID.shipNumber : 0u, (tgt && tgt->header.objtype == OBJ_ShipType && ((Ship *)tgt)->playerowner) ? (int)((Ship *)tgt)->playerowner->playerIndex : -1, (tgt == (SpaceObjRotImpTarg *)ship) ? 1 : 0,
                                    tgt ? tgt->obj.posinfo.position.x : 0, tgt ? tgt->obj.posinfo.position.y : 0, tgt ? tgt->obj.posinfo.position.z : 0,
                                    tgt ? tgt->obj.posinfo.velocity.x : 0, tgt ? tgt->obj.posinfo.velocity.y : 0, tgt ? tgt->obj.posinfo.velocity.z : 0);
                    }
                    vecCopyAndNormalize(&desiredVel,&desiredHead);
                    aitrackHeadingWithBank(ship,&desiredHead,FLYSHIP_HEADINGACCURACY,shipstaticinfo->shipOrDerelict.sinbank);
                }
            }

            // CAT: no HW1 attackSimple on an asteroid in the way (the exe never takes it out)

            aitrackVelocityVector(ship,&desiredVel);
#if HWI_ORACLE
            oracleShipDumpVector(ship, "desired", &desiredVel);   // HWI-ORACLE (M2-R8 oracle-rung-physics): /oracleShipDump
#endif

            aishipStatsClose();
            PTEND(2);
            return returnflag;

/*=============================================================================
    Unknown State:
=============================================================================*/

        default:
            dbgAssertOrIgnore(FALSE);
            aishipStatsClose();
            PTEND(2);
            return 0;
    }
}

void aishipFlyMissileToTarget(Missile *missile,SpaceObjRotImpTarg *target)
{
    MissileStaticInfo *missilestaticinfo = (MissileStaticInfo *)missile->staticinfo;
    //vector desiredHead;
    real32 missilevel;
    real32 missiletraveltime;
    vector distToTargetVec;
    real32 distToTargetMagSqr, distToTargetMag;
    vector deltatarget;

    vecSub(distToTargetVec,target->impactable.collInfo.collPosition,missile->obj.posinfo.position);

    distToTargetMagSqr = vecMagnitudeSquared(distToTargetVec);

    if (distToTargetMagSqr > MISSILE_USE_VELOCITYPRED_DISTANCE)
    {
        goto gottargettrajectory;
    }

    missilevel = fsqrt(vecMagnitudeSquared(missile->obj.posinfo.velocity));

    if (missilevel < 200.0f)
    {
        goto gottargettrajectory;
    }

    distToTargetMag = fsqrt(distToTargetMagSqr);

    missiletraveltime = distToTargetMag / missilevel;

    vecScalarMultiply(deltatarget,target->obj.posinfo.velocity,missiletraveltime);

    vecAddTo(distToTargetVec,deltatarget);

gottargettrajectory:
    //vecMultiplyByScalar(distToTargetVec,MISSILE_SCALE_FACTOR);

    vecCapMinMaxVector(&distToTargetVec,missilestaticinfo->common.maxvelocity,missilestaticinfo->common.maxvelocity);

    //vecCopyAndNormalize(&desiredVel,&desiredHead);
    //aitrackHeadingWithFlags(missile,&desiredHead,FLYSHIP_ATTACKACCURACY,AITRACKHEADING_DONTROLL+AITRACKHEADING_IGNOREUPVEC);
    aitrackVelocityVectorGuidance((SpaceObjRotImpTargGuidance *)missile,&distToTargetVec);
}


/*-----------------------------------------------------------------------------
    Name        : shouldMissileLoseTarget
    Description : checks certain criteria and sees if missile should lose its target
    Inputs      : missile
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 shouldMissileLoseTarget(Missile *missile)
{
    real32 rannum;
    real32 prob;
    vector roughDist;
    ObjType objtype = missile->target->header.objtype;

    if(objtype == OBJ_ShipType)
    {
        //if captial class we can't miss unless we're stupid
        if(isCapitalShip(((Ship *)missile->target)))
            return FALSE;

        vecSub(roughDist,missile->target->obj.posinfo.position,missile->obj.posinfo.position);

        if(isBetweenInclusive(roughDist.x,-tweakMinDistBeforeTargetLoss,tweakMinDistBeforeTargetLoss) &&
            isBetweenInclusive(roughDist.y,-tweakMinDistBeforeTargetLoss,tweakMinDistBeforeTargetLoss) &&
            isBetweenInclusive(roughDist.z,-tweakMinDistBeforeTargetLoss,tweakMinDistBeforeTargetLoss))
        {
            //target is within range to be considered for target loss
            missile->haveCheckedForLoss++;   //indicate check

         prob = 0.0f;

            prob = tweakRandomNess[((Ship *)missile->target)->tacticstype];
            if( ((Ship *)missile->target)->isDodging == TRUE)
            {
                prob+= tweakBoostIfDodging;
            }
            if( ((Ship *)missile->target)->flightman != FLIGHTMAN_NULL)
            {
                prob+=tweakBoostIfFlightMan;
            }

            rannum = frandombetween(0.0,1.0f);

            if(rannum < prob)
            {
                //satisfied: Target aquisition lost!
                return TRUE;
            }
        }
    }
    else if ((objtype == OBJ_DustType) || (objtype == OBJ_NebulaType))
    {
        vecSub(roughDist,missile->target->obj.posinfo.position,missile->obj.posinfo.position);

        if(isBetweenInclusive(roughDist.x,-200.0f,200.0f) &&
            isBetweenInclusive(roughDist.y,-200.0f,200.0f) &&
            isBetweenInclusive(roughDist.z,-200.0f,200.0f))
        {
            return TRUE;
        }
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : aishipGuideMissile
    Description : flys missile
    Inputs      : missile
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 aishipGuideMissile(Missile *missile)
{
    SpaceObjRotImpTarg *target = missile->target;
    vector heading;

    if (missile->header.flags & SOF_Dead)
    {
        return FALSE;
    }

    if (target == NULL)
    {
        ;   //let missile just keep flying
    }
    else
    {
        aishipFlyMissileToTarget(missile,target);
        // CAT: a fully cloaked target is lost (cat_shipIsCloakedFully 0x00436110; aishipGuideMissile 0x00415e60), where
        //      HW1 tested SOF_Cloaked alone
        if (cat_shipIsCloakedFully((Ship *)missile->target))
        {
            missile->target = NULL;
        }
        else if(missile->haveCheckedForLoss < tweakNumTimesCheck)
        {
            if(UNIVERSE_WOODPECKER(tweakCheckMissileTargetLossRate, universe.univUpdateCounter & missile->missileID.missileNumber))
            {
                if(shouldMissileLoseTarget(missile))
                {
                    missile->target = NULL;
                }
            }
        }
    }

    heading = missile->obj.posinfo.velocity;
    if ((heading.x != 0.0f) || (heading.y != 0.0f) || (heading.z != 0.0f))
    {
        vecNormalize(&heading);
        matCreateCoordSysFromHeading(&missile->rotatable.rotinfo.coordsys,&heading);
        vecZeroVector(missile->rotatable.rotinfo.rotspeed);
        vecZeroVector(missile->rotatable.rotinfo.torque);
    }

    SET_MOVING_LINEARLY(missile->obj.posinfo.isMoving);

    missile->timelived += universe.phystimeelapsed;
    if (missile->timelived > missile->totallifetime)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    HWI-TEST (M3-R3 mines): /catMineLog <ticks>: the mines' state machine in the log (the user's play-test item 6).
    Every state change of every mine, "minelog: change tick=.. id=.. race=.. state=A->B why=.. target=.." (why: the
    attack run's exit path: the target flew out of the range test, became fully cloaked or disabled, flew away faster
    than the mine, or was cleared elsewhere: died / left / docked), and for the first CAT_MINELOG_MAX mines that start
    an attack run, <ticks> universe ticks of "minelog: tick=.. id=.. state=.. target=.. dist=.. speed=.. tspeed=.."
    lines from that tick (an attack run that starts with a target). Read only: no simulation field, no random draw.
-----------------------------------------------------------------------------*/
sdword catMineLogTicks = 0;                                                 // HWI-TEST
static const char *catMineLogWhy = "-";                                     // HWI-TEST
#define CAT_MINELOG_MAX 8                                                   // HWI-TEST
static sdword catMineLogN = 0;                                              // HWI-TEST
static uword catMineLogId[CAT_MINELOG_MAX];                                 // HWI-TEST
static udword catMineLogFrom[CAT_MINELOG_MAX];                              // HWI-TEST

void catMineLogSet(char *string)                                            // HWI-TEST
{
    catMineLogTicks = (sdword)strtol(string, NULL, 10);
}

static void catMineLogTarget(char *out, size_t n, SpaceObjRotImpTarg *t)   // HWI-TEST
{
    if (t == NULL)
    {
        snprintf(out, n, "none");
    }
    else if (t->header.objtype == OBJ_ShipType)
    {
        snprintf(out, n, "%s#%u", ShipTypeToStr(((Ship *)t)->shiptype), (unsigned)((Ship *)t)->shipID.shipNumber);
    }
    else
    {
        snprintf(out, n, "objtype%d", (int)t->header.objtype);
    }
}

static void catMineLogStep(Missile *mine, udword oldState, SpaceObjRotImpTarg *oldTarget)  // HWI-TEST
{
    char tn[64];
    sdword i, k = -1;
    udword tick = universe.univUpdateCounter;
    vector d;
    double dist = -1.0, tspeed = -1.0, speed;

    if (catMineLogTicks <= 0)
    {
        return;
    }
    speed = sqrt((double)vecMagnitudeSquared(mine->obj.posinfo.velocity));
    if (mine->target != NULL)
    {
        vecSub(d, mine->target->impactable.collInfo.collPosition, mine->obj.posinfo.position);
        dist = sqrt((double)vecMagnitudeSquared(d));
        tspeed = sqrt((double)vecMagnitudeSquared(mine->target->obj.posinfo.velocity));
    }
    if (mine->mineAIState != oldState)
    {
        catMineLogTarget(tn, sizeof(tn), (mine->target != NULL) ? mine->target : oldTarget);
        dbgMessagef("minelog: change tick=%u id=%u race=%d forced=%d state=%u->%u why=%s target=%s speed=%.1f",
                    (unsigned)tick, (unsigned)mine->missileID.missileNumber, (int)mine->race, (int)mine->FORCE_DROPPED,
                    (unsigned)oldState, (unsigned)mine->mineAIState,
                    (mine->mineAIState == MINE_ATTACK_RUN) ? ((mine->target != NULL) ? "acquired" : "start") : catMineLogWhy,
                    tn, speed);
    }
    for (i = 0; i < catMineLogN; i++)
    {
        if (catMineLogId[i] == mine->missileID.missileNumber)
        {
            k = i;
        }
    }
    if (k < 0 && mine->mineAIState == MINE_ATTACK_RUN && mine->target != NULL && catMineLogN < CAT_MINELOG_MAX)
    {
        k = catMineLogN++;
        catMineLogId[k] = mine->missileID.missileNumber;
        catMineLogFrom[k] = tick;
    }
    if (k >= 0 && tick - catMineLogFrom[k] < (udword)catMineLogTicks)
    {
        catMineLogTarget(tn, sizeof(tn), mine->target);
        dbgMessagef("minelog: tick=%u id=%u state=%u target=%s dist=%.0f speed=%.1f tspeed=%.1f pos=(%.0f %.0f %.0f)",
                    (unsigned)tick, (unsigned)mine->missileID.missileNumber, (unsigned)mine->mineAIState, tn, dist,
                    speed, tspeed, (double)mine->obj.posinfo.position.x, (double)mine->obj.posinfo.position.y,
                    (double)mine->obj.posinfo.position.z);
    }
    catMineLogWhy = "-";
}

/*-----------------------------------------------------------------------------
    HWI-TEST (M3-R3 mines): /catMineChase <tick>[:<shiptype>]: at that universe tick up to four of player 0's ships of
    the type (default sRecon) are ordered through the command layer (clWrapMove, no input) on a straight pass 2,000
    units beside the level mine (race Derelict) nearest the first of them and 20,000 units beyond it: the scene of the
    user's report (a mine field that strike craft fly past), for /catMineLog. Changes the game: a test run only
-----------------------------------------------------------------------------*/
static sdword catMineChaseAt = -1;                                          // HWI-TEST
static ShipType catMineChaseType = sRecon;                                  // HWI-TEST
static bool32 catMineChaseDone = FALSE;                                     // HWI-TEST

void catMineChaseSet(char *string)                                          // HWI-TEST
{
    char *c = strchr(string, ':');
    ShipType type;

    catMineChaseAt = (sdword)strtol(string, NULL, 10);
    if (c != NULL && (type = StrToShipType(c + 1)) != (ShipType)0xffffffff)
    {
        catMineChaseType = type;
    }
}

bool32 catMineChaseRequested(void)                                         // HWI-TEST
{
    return (catMineChaseAt >= 0) && !catMineChaseDone;
}

void catMineChaseTick(void)                                                 // HWI-TEST
{
    SelectCommand *sel;
    Node *node;
    Missile *best = NULL;
    real32 bestDist = REALlyBig, dist;
    vector d, side, dest, up = {0.0f, 0.0f, 1.0f};

    if ((sdword)universe.univUpdateCounter < catMineChaseAt || !gameIsRunning)
    {
        return;
    }
    catMineChaseDone = TRUE;
    sel = memAlloc(sizeofSelectCommand(4), "minechase", 0);
    sel->numShips = 0;
    for (node = universe.ShipList.head; node != NULL && sel->numShips < 4; node = node->next)
    {
        Ship *ship = (Ship *)listGetStructOfNode(node);

        if (ship->playerowner == &universe.players[0] && ship->shiptype == catMineChaseType &&
            !(ship->header.flags & SOF_Dead))
        {
            sel->ShipPtr[sel->numShips++] = ship;
        }
    }
    for (node = universe.MissileList.head; node != NULL && sel->numShips > 0; node = node->next)
    {
        Missile *mis = (Missile *)listGetStructOfNode(node);

        if (mis->missileType != MISSILE_Mine || mis->race != RACE_Derelict)
        {
            continue;
        }
        vecSub(d, mis->obj.posinfo.position, sel->ShipPtr[0]->obj.posinfo.position);
        dist = vecMagnitudeSquared(d);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = mis;
        }
    }
    if (best == NULL)
    {
        dbgMessagef("minechase: tick=%u ships=%d type=%s no level mine or no ship: nothing ordered",
                    (unsigned)universe.univUpdateCounter, (int)sel->numShips, ShipTypeToStr(catMineChaseType));
        memFree(sel);
        return;
    }
    vecSub(d, best->obj.posinfo.position, sel->ShipPtr[0]->obj.posinfo.position);
    vecNormalize(&d);
    vecCrossProduct(side, d, up);
    if (vecMagnitudeSquared(side) < 0.01f)
    {
        side.x = 1.0f; side.y = 0.0f; side.z = 0.0f;
    }
    vecNormalize(&side);
    dest.x = best->obj.posinfo.position.x + side.x * 2000.0f + d.x * 20000.0f;
    dest.y = best->obj.posinfo.position.y + side.y * 2000.0f + d.y * 20000.0f;
    dest.z = best->obj.posinfo.position.z + side.z * 2000.0f + d.z * 20000.0f;
    clWrapMove(&universe.mainCommandLayer, sel, 0, sel->ShipPtr[0]->obj.posinfo.position, 1, &dest);
    dbgMessagef("minechase: tick=%u ships=%d type=%s#%u.. mine id=%u at (%.0f %.0f %.0f) distance=%.0f dest=(%.0f %.0f %.0f)",
                (unsigned)universe.univUpdateCounter, (int)sel->numShips, ShipTypeToStr(catMineChaseType),
                (unsigned)sel->ShipPtr[0]->shipID.shipNumber, (unsigned)best->missileID.missileNumber,
                (double)best->obj.posinfo.position.x, (double)best->obj.posinfo.position.y,
                (double)best->obj.posinfo.position.z, sqrt((double)bestDist), (double)dest.x, (double)dest.y, (double)dest.z);
    memFree(sel);
}

bool32 aishipFlyMineToTarget(Missile *mine,SpaceObjRotImpTarg *target)
{
    MissileStaticInfo *minestaticinfo = (MissileStaticInfo *)mine->staticinfo;
    vector desiredVel;
    //vector desiredHead;
    real32 mine_rangesqr;
    real32 max_velocity;
    real32 max_velocitySQR;
    if(mine->FORCE_DROPPED)
    {
        mine_rangesqr = minestaticinfo->MINE_RANGESQR_FORCED;
        max_velocity = minestaticinfo->maxvelocity_FORCED;
        max_velocitySQR = minestaticinfo->maxvelocity_FORCED*minestaticinfo->maxvelocity_FORCED;
    }
    else
    {
        mine_rangesqr = minestaticinfo->MINE_RANGESQR;
        max_velocity = minestaticinfo->common.maxvelocity;
        max_velocitySQR =minestaticinfo->common.maxvelocity*minestaticinfo->common.maxvelocity;
    }

    vecSub(desiredVel,target->impactable.collInfo.collPosition,mine->obj.posinfo.position);
    //if(vecMagnitudeSquared(desiredVel) > mine_rangesqr)
    if(!MoveReachedDestinationVariable((Ship *)mine,&target->impactable.collInfo.collPosition,minestaticinfo->MINE_RANGESQR_FORCED))

    {
        //target flew out of range.
        mine->target = NULL;
        catMineLogWhy = "range";                                            // HWI-TEST (M3-R3 mines)
        return(FALSE);
    }
    else if(mine->target != NULL)
    {
        // CAT: a fully cloaked target is lost (cat_shipIsCloakedFully 0x00436110; aishipFlyMineToTarget 0x00415fa0)
        if (cat_shipIsCloakedFully((Ship *)mine->target) || (mine->target->header.flags & SOF_Disabled))
        {
            catMineLogWhy = (mine->target->header.flags & SOF_Disabled) ? "disabled" : "cloaked";   // HWI-TEST (M3-R3 mines)
            mine->target = NULL;
            return FALSE;
        }
    }
    //checks targets speed and only goes after it if it can
    //have a chance of reaching it
    if(vecDotProduct(desiredVel,target->obj.posinfo.velocity) > 0 )
    {
        //target is flying away from mine...check velocity now
        if(vecMagnitudeSquared(target->obj.posinfo.velocity) > max_velocitySQR)
        {
            //target is flying faster than mine can go so don't follow it!
            mine->target = NULL;
            catMineLogWhy = "faster-away";                                  // HWI-TEST (M3-R3 mines)
            return(FALSE);
        }
    }

    if ((target->header.objtype != -1) && (target->obj.collMyBlob))
    {
        // we've got a "real" target here
        if (mine->obj.collMyBlob != target->obj.collMyBlob)
        {
            if (mine->obj.collMyBlob)
            {
                // let's take it out of our blob
                bobRemoveMineFromSpecificBlob(mine->obj.collMyBlob,mine);
            }

            // put it in targets blob
            collAddSpaceObjToSpecificBlob(target->obj.collMyBlob, &mine->obj);
        }
    }

    vecNormalizeToLength(&desiredVel,max_velocity);
    //vecCapMinMaxVector(&desiredVel,MINE_MIN_VELOCITY,max_velocity);

    //vecCopyAndNormalize(&desiredVel,&desiredHead);

    // TAKE out track mines with right vector to spin them - save cpu
    //matGetVectFromMatrixCol1(desiredHead,mine->rotatable.rotinfo.coordsys);       //track mines right vector always...spins then.
    //aitrackHeadingWithFlags(mine,&desiredHead,FLYSHIP_ATTACKACCURACY,AITRACKHEADING_IGNOREUPVEC);

    aitrackVelocityVectorGuidance((SpaceObjRotImpTargGuidance *)mine,&desiredVel);
    return(TRUE);
}

bool32 aishipslowminewithFriction(Missile *mine, real32 fric)
{
    if(isBetweenInclusive(mine->obj.posinfo.velocity.x,-1.0f,1.0f) &&
        isBetweenInclusive(mine->obj.posinfo.velocity.y,-1.0f,1.0f) &&
        isBetweenInclusive(mine->obj.posinfo.velocity.z,-1.0f,1.0f))
    {
        mine->obj.posinfo.velocity.x = 0.0f;
        mine->obj.posinfo.velocity.y = 0.0f;
        mine->obj.posinfo.velocity.z = 0.0f;
        mine->rotatable.rotinfo.rotspeed.x = 0.0f;
        mine->rotatable.rotinfo.rotspeed.y = 0.0f;
        mine->rotatable.rotinfo.rotspeed.z = 0.0f;
        return(TRUE);
    }
    else
    {
        mine->obj.posinfo.velocity.x *= fric;
        mine->obj.posinfo.velocity.y *= fric;
        mine->obj.posinfo.velocity.z *= fric;
        mine->rotatable.rotinfo.rotspeed.x *= fric;
        mine->rotatable.rotinfo.rotspeed.y *= fric;
        mine->rotatable.rotinfo.rotspeed.z *= fric;
    }
    return(FALSE);
}


#define blobSearchRight  1
#define blobSearchLeft  2

SpaceObjRotImpTarg *baseObj;
blob *baseBlob;
sdword blobDirection;
sdword blobIndex;
extern BlobProperties collBlobProperties;


//SEARCH Right...then
//SEARCH Left
Ship *getShipNearObjTok(SpaceObjRotImpTarg *obj, real32 range)
{

    Ship *blobShip;
    vector difvec;
    real32 dif;

    if(obj != NULL)
    {
        //starting
        if(obj->obj.collMyBlob == NULL)
            return NULL;

        blobDirection = blobSearchRight;
        baseBlob = obj->obj.collMyBlob;
        baseObj = obj;
        blobIndex = 0;
    }
    //continue or starting...doesn't matter
newBlob:
    for(;blobIndex < baseBlob->blobShips->numShips;blobIndex++)
    {
        //scan blob list
        blobShip = baseBlob->blobShips->ShipPtr[blobIndex];
        if(MoveReachedDestinationVariable(blobShip,&baseObj->impactable.collInfo.collPosition,range))
        {
            //found ship within range
            blobIndex++;
            return blobShip;
        }
    }

    //blob expired...no more ships
    if(blobDirection == blobSearchRight)
    {
needanotherblob:
        if(baseBlob->node.next == NULL)
        {
            //done going right...go left
            //first reset blob though
startleft:
            blobDirection = blobSearchLeft;
            baseBlob=baseObj->obj.collMyBlob;
            goto leftsearch;
        }

        baseBlob = (blob *)listGetStructOfNode(baseBlob->node.next);
        blobIndex = 0;
        //check if in range

        //this blob is further than normal blob
        //dif = baseBlob->sqrtSortDistance-baseObj->obj.collMyBlob->sqrtSortDistance;
        //dif = dif + baseObj->obj.collMyBlob->radius - collBlobProperties.bobBiggestRadius;
        dif = (baseBlob->sqrtSortDistance-collBlobProperties.bobBiggestRadius)-
               (baseObj->obj.collMyBlob->sqrtSortDistance+baseObj->obj.collOptimizeDist);
        if(dif > range)
        {
            //blob list exhausted to right
            goto startleft;
        }

        //blob is possible
        vecSub(difvec,baseObj->impactable.collInfo.collPosition,baseBlob->centre);
        if(!MoveReachedDestinationVariable((Ship *)baseObj,&baseBlob->centre,range+collBlobProperties.bobBiggestRadius))
        {
            //blob edge not in range...consider another
            goto needanotherblob;
        }
        //good
        goto newBlob;

    }
    else
    {
leftsearch:
        if(baseBlob->node.prev == NULL)
        {
            //done going left..return NULL
            return NULL;
        }

        //get next left blob
        baseBlob = (blob *)listGetStructOfNode(baseBlob->node.prev);

        //consider if in range
                //this blob is further than normal blob
        //dif = baseObj->obj.collMyBlob->sqrtSortDistance - baseBlob->sqrtSortDistance;
        //dif = dif + collBlobProperties.bobBiggestRadius - baseObj->obj.collMyBlob->radius;
        dif = (baseObj->obj.collOptimizeDist+baseObj->obj.collMyBlob->sqrtSortDistance)-
               (baseBlob->sqrtSortDistance+collBlobProperties.bobBiggestRadius);
        if(dif > range)
        {
            //blob list exhausted to right
            return NULL;
        }

        //blob is possible
        vecSub(difvec,baseObj->impactable.collInfo.collPosition,baseBlob->centre);
        if(!MoveReachedDestinationVariable((Ship *)baseObj,&baseBlob->centre,range+baseBlob->radius))
        {
            //blob edge not in range...consider another
            goto leftsearch;
        }
        //good
        goto newBlob;
    }
    return NULL;
}

Ship *aishipmineaquiretarget(Missile *mine)
{
    blob *MyBlob;
    SelectCommand *shipselection;
    //sdword shipindex = 0;
    Ship *ship;
    Ship *ShiptoRecieveShitPounding = NULL;
    //real32 distanceSqr;
    //vector diff;
    real32 mine_range;

    MissileStaticInfo *minestaticinfo = (MissileStaticInfo *)mine->staticinfo;

    MyBlob = mine->obj.collMyBlob;
    shipselection = MyBlob->blobShips;
    if(mine->FORCE_DROPPED)
    {
        mine_range = minestaticinfo->mineRangeForced;
    }
    else
    {
        mine_range = minestaticinfo->mineRange;
    }

    ship = getShipNearObjTok((SpaceObjRotImpTarg *)mine,mine_range);

    while (ship != NULL)
    {
        //ship = shipselection->ShipPtr[shipindex];

        //ship = (Ship *)listGetStructOfNode(shipnode);

        // CAT: outside single player a derelict-race ship is no target, nor a ship a mission marked with
        //      CAT_SHIPFLAG_KasMines (aishipmineaquiretarget 0x004162c0)
        if ((!singlePlayerGame) && (ship->shiprace == RACE_Derelict))
        {
            goto nextnode;
        }
        if ( (ship->playerowner == mine->playerowner) ||
             (allianceIsShipAlly(ship,mine->playerowner)) )
        {   //don't target are ships....
            goto nextnode;
        }
        if (ship->header.catFlags18 & CAT_SHIPFLAG_KasMines)
        {
            goto nextnode;
        }
        // CAT: a fully cloaked ship is a target only once the mine's player has detected it (and then without the
        //      tests below); a Leech only when detected, a mimic not while it appears allied to the mine's player
        //      (cat_shipIsCloakedFully 0x00436110, cat_shipDetectedByPlayer = sub_53F9F0 0x0053f9f0,
        //      cat_mimicAppearsAlliedTo; 0x00416340 .. 0x004163b6); HW1 asked the proximity sensors
        if (cat_shipIsCloakedFully(ship))
        {
            if (!cat_shipDetectedByPlayer(ship, mine->playerowner->playerIndex))
            {
                goto nextnode;
            }
        }
        else
        {
            if(ship->header.flags & SOF_Disabled)
            {
                goto nextnode;
            }
            if ((ship->shiptype == sLeech) && !cat_shipDetectedByPlayer(ship, mine->playerowner->playerIndex))
            {
                goto nextnode;
            }
            if ((ship->header.catFlags18 & CAT_SHIPFLAG_Mimic) && cat_mimicAppearsAlliedTo(ship, mine->playerowner))
            {
                goto nextnode;
            }
        }
        //here we decide not to attack allies as well :)

        //add more rejections to speed it up

        /*
        vecSub(diff,ship->obj.posinfo.position,mine->obj.posinfo.position);
        distanceSqr = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;

        if(distanceSqr < minestaticinfo->MINE_RANGESQR)
        */
        {
            if(ShiptoRecieveShitPounding == NULL)
            {
                ShiptoRecieveShitPounding = ship;
            }
            else if(ship->staticinfo->common.mass > ShiptoRecieveShitPounding->staticinfo->common.mass)
            {
                ShiptoRecieveShitPounding = ship;
            }
            else if (ship->staticinfo->common.mass == ShiptoRecieveShitPounding->staticinfo->common.mass)
            {   //if masses are the same
                if(randombetween(1,2) & 1)
                {   //randomly decide one over the other (that way, a group of fighters will
                   ShiptoRecieveShitPounding = ship;   // be attacked equally)
                }
            }
        }

    nextnode:
        //shipindex++;
        ship = getShipNearObjTok(NULL, mine_range);
    }
    return(ShiptoRecieveShitPounding);
}

// If it is time, it orders a search for a target
bool32 mine_do_search(Missile *mine)
{
    if(UNIVERSE_WOODPECKER(MINE_DO_SEARCH_MASK, mine->missileID.missileNumber & MINE_DO_SEARCH_MASK))
    {   //only do a check ever MINE_DO_SEARCH_MASKth frame...much quicker!
        if(mine->target == NULL)
        {
            mine->target = (SpaceObjRotImpTarg *)aishipmineaquiretarget(mine);
        }
        if(mine->target != NULL)
        {
            return(TRUE);
        }
    }
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : aishipGuideMine
    Description : controls mine
    Inputs      : missile of type mine
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 aishipGuideMine(Missile *mine)
{
    //Ship *ship;
    MissileStaticInfo *minestaticinfo = (MissileStaticInfo *)mine->staticinfo;
    vector dist;
    real32 distsqr;
    MinelayerCorvetteStatics *minelayercorvettestatics;
    udword catMineLogOld = mine->mineAIState;                               // HWI-TEST (M3-R3 mines)
    SpaceObjRotImpTarg *catMineLogOldTarget = mine->target;                 // HWI-TEST

    if (mine->header.flags & SOF_Dead)
    {
        return FALSE;
    }

    dbgAssertOrIgnore(mine->missileType == MISSILE_Mine);

    if (mine->target)
    {
        switch (mine->target->header.objtype)
        {
            case OBJ_MissileType:
            case OBJ_AsteroidType:
            case OBJ_NebulaType:
            case OBJ_DustType:
                mine->target = NULL;
            default:
                break;
        }
    }

    switch(mine->mineAIState)
    {
    case MINE_DROP_ATTACK:                      //Fix exit path of mines here
        mine->mineAIState = MINE_ATTACK_RUN;
        //this is the state where mines are being dropped during an attack
        break;
    case MINE_DROP_FORMATION:     //starting of wall formation
        if(mine->owner != NULL)
        {
            vecSub(dist,mine->obj.posinfo.position,mine->owner->obj.posinfo.position);
            minelayercorvettestatics = (MinelayerCorvetteStatics *)((ShipStaticInfo *)mine->owner->staticinfo)->custstatinfo;
            distsqr = vecMagnitudeSquared(dist);
            if(distsqr > minelayercorvettestatics->MineClearDistanceSQR)
            {
                mine->mineAIState = MINE_WALL_FORMATION;
            }
        }
        else
        {
            mine->mineAIState = MINE_WALL_FORMATION;
        }
        break;
    case MINE_WALL_FORMATION:
        if(mine_do_search(mine))        //always search for enemy after launching is cleared
        {
            mine->mineAIState = MINE_ATTACK_RUN;
        }
        //TODO(type safety, 2026-04-25): Remove unsafe cast from Missile* to Ship*
        if(!MoveReachedDestinationVariable((Ship *)mine,&mine->formation_position,10.0f))
        {
            SpaceObjRotImpTarg dest_faker;
            dest_faker.header.objtype = -1;
            dest_faker.impactable.collInfo.collPosition = mine->formation_position;
            vecSet(dest_faker.obj.posinfo.velocity, 0.0f,0.0f,0.0f);
            dest_faker.header.flags = 0;
            dest_faker.staticinfo = NULL;      // CAT: the exe clears it too (aishipGuideMine 0x00416410)
            aishipFlyMineToTarget(mine,&dest_faker);
        }
        else
        {
            //we've reached the destination so Slow down...
            aishipslowminewithFriction(mine, minestaticinfo->MINE_STOP_FRICTION);
        }

        break;
    case MINE_SEARCH_AND_STOP:
        aishipslowminewithFriction(mine, minestaticinfo->MINE_STOP_FRICTION);
        if(mine_do_search(mine))        //always search for enemy after launching is cleared
        {
            mine->mineAIState = MINE_ATTACK_RUN;
        }
        break;
    case MINE_ATTACK_RUN:
        if(mine->target == NULL)
        {   //Lost target...so stop, reform wall and search for new target
            catMineLogWhy = "cleared";                                      // HWI-TEST (M3-R3 mines): died / left / docked
            //check if too far from formation location
            if(mine->formationinfo == NULL)
            {
                mine->mineAIState = MINE_SEARCH_AND_STOP;
            }
            else
            {
                mine->mineAIState = MINE_WALL_FORMATION;
            }
                break;
        }
        else
        {
            if(!aishipFlyMineToTarget(mine,mine->target))
            {
                //lost target, so reform wall
                // CAT: as for no target: a mine without a formation stops and searches (aishipGuideMine 0x00416410
                //      at 0x00416783: the state is 4 with formationinfo, else 2), where HW1 always reformed the wall
                mine->mineAIState = (mine->formationinfo == NULL) ? MINE_SEARCH_AND_STOP : MINE_WALL_FORMATION;
            }
        }
        break;
    default:
        dbgAssertOrIgnore(FALSE);
        break;
    }

    SET_MOVING_LINEARLY(mine->obj.posinfo.isMoving);
    catMineLogStep(mine, catMineLogOld, catMineLogOldTarget);               // HWI-TEST (M3-R3 mines): /catMineLog

    // all mines, including force dropped ones, now expire
    mine->timelived += universe.phystimeelapsed;

    return mine->timelived > mine->totallifetime;
}

