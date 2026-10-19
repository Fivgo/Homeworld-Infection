// =============================================================================
//  AITrack.c
//  - Given a desired velocity, heading, etc from AIShip.c, this AI layer will
//  make the ship track the desired velocity, heading, etc.
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/25/1997 by gshaw
// =============================================================================

#include "AITrack.h"

#include "Gun.h"
#include "AIShip.h"
#include "FastMath.h"
#include "Physics.h"
#include "StatScript.h"
#include "Universe.h"

#define DEBUG_AITRACK 0

/*=============================================================================
    Tweakables
=============================================================================*/

real32 ERROR_VELOCITY_SQUARED   = 0.05f;
real32 STILL_VELOCITY_LO        = -3.0f;
real32 STILL_VELOCITY_HI        = 3.0f;
real32 STILL_ROT_LO             = -0.3f;
real32 STILL_ROT_HI             = 0.3f;
real32 STABILIZE_Z_LO           = -0.1f;
real32 STABILIZE_Z_HI           = 0.1f;

real32 AITRACKRIGHT_UPACCURACY = 0.985f;

scriptEntry AITrackTweaks[] =
{
    { "ErrorVelocitySquared",   scriptSetReal32CB, &ERROR_VELOCITY_SQUARED },
    { "StillVelocityLo",        scriptSetReal32CB, &STILL_VELOCITY_LO },
    { "StillVelocityHi",        scriptSetReal32CB, &STILL_VELOCITY_HI },
    { "StillRotLo",             scriptSetReal32CB, &STILL_ROT_LO },
    { "StillRotHi",             scriptSetReal32CB, &STILL_ROT_HI },
    makeEntry(STABILIZE_Z_LO,scriptSetReal32CB),
    makeEntry(STABILIZE_Z_HI,scriptSetReal32CB),
    makeEntry(AITRACKRIGHT_UPACCURACY,scriptSetCosAngCB),
    { NULL, NULL, 0 }
};

/*-----------------------------------------------------------------------------
    Name        : aitrackIsStabilizeShip
    Description : Returns TRUE if ship is stablized
    Inputs      : ship
    Outputs     :
    Return      : Returns TRUE if ship is stablized
----------------------------------------------------------------------------*/
bool32 aitrackIsStabilizeShip(Ship *ship)
{
    vector heading,up,right;
    bool32 stabilizedpitch;
    real32 rotx = ship->rotatable.rotinfo.rotspeed.x;

    matGetVectFromMatrixCol3(heading,ship->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol1(up,ship->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol2(right,ship->rotatable.rotinfo.coordsys);

    stabilizedpitch = isBetweenExclusive(heading.z,STABILIZE_Z_LO,STABILIZE_Z_HI);
    if (stabilizedpitch)
    {
        if (isBetweenExclusive(rotx,STILL_ROT_LO,STILL_ROT_HI) &&
    //        isBetweenExclusive(roty,STILL_ROT_LO,STILL_ROT_HI) &&
    //        isBetweenExclusive(rotz,STILL_ROT_LO,STILL_ROT_HI) &&
    //          (stabilizedpitch) &&
            isBetweenExclusive(right.z,STABILIZE_Z_LO,STABILIZE_Z_HI) &&
            (up.z > 0) )
        {
            return TRUE;
        }
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : aitrackStabilizeShip
    Description : this ai will stabilize the ship (e.g. stop all pitch, roll,
                  and yaw, although it will not change the velocity of the ship)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 aitrackStabilizeGuidance(SpaceObjRotImpTargGuidance *ship)
{
    real32 rotx = ship->rotatable.rotinfo.rotspeed.x;
    real32 roty = ship->rotatable.rotinfo.rotspeed.y;
    real32 rotz = ship->rotatable.rotinfo.rotspeed.z;
    vector heading;
    vector up;
    vector right;
    real32 rotstr;
    real32 oneovertime;
    bool32 stabilizedpitch;

    StaticInfoHealthGuidance *shipstaticinfo;

    matGetVectFromMatrixCol3(heading,ship->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol1(up,ship->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol2(right,ship->rotatable.rotinfo.coordsys);

    stabilizedpitch = isBetweenExclusive(heading.z,STABILIZE_Z_LO,STABILIZE_Z_HI);
    if (stabilizedpitch)
    {
        roty = ship->rotatable.rotinfo.rotspeed.y = 0.0f;
    }

    if (isBetweenExclusive(rotx,STILL_ROT_LO,STILL_ROT_HI) &&
//        isBetweenExclusive(roty,STILL_ROT_LO,STILL_ROT_HI) &&
//        isBetweenExclusive(rotz,STILL_ROT_LO,STILL_ROT_HI) &&
          (stabilizedpitch) &&
        isBetweenExclusive(right.z,STABILIZE_Z_LO,STABILIZE_Z_HI) &&
        (up.z > 0) )
    {
        return TRUE;
    }

#if DEBUG_AITRACK
    dbgMessagef("Zeroing Rot %f %f %f %x",rotx,roty,rotz,(udword)ship);
#endif

    shipstaticinfo = ship->staticinfo;
    oneovertime = 1.0f / universe.phystimeelapsed;

    if (rotx > 0)
    {
        rotstr = rotx * shipstaticinfo->common.momentOfInertiaX * oneovertime;
        physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTXCW]),ROT_ABOUTXCW);
    }
    else
    {
        rotstr = -rotx * shipstaticinfo->common.momentOfInertiaX * oneovertime;
        physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTXCCW]),ROT_ABOUTXCCW);
    }

    if (stabilizedpitch)
    {
        if (rotz > 1.0f)
        {
            rotstr = rotz * shipstaticinfo->common.momentOfInertiaZ * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCW]),ROT_ABOUTZCW);
        }
        else if (rotz < -1.0f)
        {
            rotstr = -rotz * shipstaticinfo->common.momentOfInertiaZ * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCCW]),ROT_ABOUTZCCW);
        }
        else
        {
            if (right.z > 0)
            {
                physApplyRotToObj(&ship->rotatable,ship->nonstatvars.rotstrength[ROT_ROLLRIGHT]*right.z,ROT_ROLLRIGHT);
            }
            else
            {
                physApplyRotToObj(&ship->rotatable,ship->nonstatvars.rotstrength[ROT_ROLLLEFT]*(-right.z),ROT_ROLLLEFT);
            }
        }
    }
    else
    {
        if (rotz > 0.0f)
        {
            rotstr = rotz * shipstaticinfo->common.momentOfInertiaZ * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCW]),ROT_ABOUTZCW);
        }
        else
        {
            rotstr = -rotz * shipstaticinfo->common.momentOfInertiaZ * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTZCCW]),ROT_ABOUTZCCW);
        }

        if (roty > 1.0f)
        {
            rotstr = roty * shipstaticinfo->common.momentOfInertiaY * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTYCW]),ROT_ABOUTYCW);
        }
        else if (roty < -1.0f)
        {
            rotstr = -roty * shipstaticinfo->common.momentOfInertiaY * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->nonstatvars.rotstrength[ROT_ABOUTYCCW]),ROT_ABOUTYCCW);
        }
        else
        {
            if (heading.z < 0)
            {
                physApplyRotToObj(&ship->rotatable,ship->nonstatvars.rotstrength[ROT_PITCHUP]*(-heading.z),ROT_PITCHUP);
            }
            else
            {
                physApplyRotToObj(&ship->rotatable,ship->nonstatvars.rotstrength[ROT_PITCHDOWN]*heading.z,ROT_PITCHDOWN);
            }
        }
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : aitrackIsZeroRotation
    Description : returns TRUE if rotation is zero
    Inputs      :
    Outputs     :
    Return      : returns TRUE if rotation is zero
----------------------------------------------------------------------------*/
bool32 aitrackIsZeroRotation(Ship *ship)
{
    real32 rotx = ship->rotatable.rotinfo.rotspeed.x;
    real32 roty = ship->rotatable.rotinfo.rotspeed.y;
    real32 rotz = ship->rotatable.rotinfo.rotspeed.z;

    if (isBetweenExclusive(rotx,STILL_ROT_LO,STILL_ROT_HI) &&
        isBetweenExclusive(roty,STILL_ROT_LO,STILL_ROT_HI) &&
        isBetweenExclusive(rotz,STILL_ROT_LO,STILL_ROT_HI))
    {
        return TRUE;
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : aitrackZeroRotationAnywhere
    Description : this ai will stabilize the ship (e.g. stop all pitch, roll,
                  and yaw, although it will not change the velocity of the ship)
                  It does not necessarily stop the ship oriented up
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 aitrackZeroRotationAnywhere(Ship *ship)
{
    real32 rotx = ship->rotatable.rotinfo.rotspeed.x;
    real32 roty = ship->rotatable.rotinfo.rotspeed.y;
    real32 rotz = ship->rotatable.rotinfo.rotspeed.z;
    vector heading;
    vector up;
    vector right;
    real32 rotstr;
    real32 oneovertime;

    ShipStaticInfo *shipstaticinfo;

    matGetVectFromMatrixCol3(heading,ship->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol1(up,ship->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol2(right,ship->rotatable.rotinfo.coordsys);

    if (isBetweenExclusive(rotx,STILL_ROT_LO,STILL_ROT_HI) &&
        isBetweenExclusive(roty,STILL_ROT_LO,STILL_ROT_HI) &&
        isBetweenExclusive(rotz,STILL_ROT_LO,STILL_ROT_HI))
    {
        return TRUE;
    }

    shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    oneovertime = 1.0f / universe.phystimeelapsed;

    if (rotx > 0)
    {
        rotstr = rotx * shipstaticinfo->common.momentOfInertiaX * oneovertime;
        physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTXCW]),ROT_ABOUTXCW);
    }
    else
    {
        rotstr = -rotx * shipstaticinfo->common.momentOfInertiaX * oneovertime;
        physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTXCCW]),ROT_ABOUTXCCW);
    }

    if (roty > 0)
    {
        rotstr = roty * shipstaticinfo->common.momentOfInertiaY * oneovertime;
        physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTYCW]),ROT_ABOUTYCW);
    }
    else
    {
        rotstr = -roty * shipstaticinfo->common.momentOfInertiaY * oneovertime;
        physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTYCCW]),ROT_ABOUTYCCW);
    }
    if (rotz > 0)
    {
        rotstr = rotz * shipstaticinfo->common.momentOfInertiaZ * oneovertime;
        physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTZCW]),ROT_ABOUTZCW);
    }
    else
    {
        rotstr = -rotz * shipstaticinfo->common.momentOfInertiaZ * oneovertime;
        physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTZCCW]),ROT_ABOUTZCCW);
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : aitrackHeading
    Description : given a desired heading vector, this ai will orient the
                  ship in the correct heading as well as stopping any roll.
    Inputs      : desiredHeading
    Outputs     :
    Return      : TRUE if heading has been successfully tracked.
----------------------------------------------------------------------------*/
bool32 aitrackHeadingAndUp(Ship *ship,vector *desiredHeading,vector *desiredUp,real32 accuracy)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;

    vector curheading;
    vector curup;
    real32 dotProdCheckHeading;
    real32 dotProdCheckUp;

    real32 desiredrotspeed;
    real32 rotspeed;

    vector desiredHeadingInShipCoordSys;

    vector desiredUpInShipCoordSys;

    real32 rotstr;
    real32 oneovertime;

    if(ship->header.objtype == OBJ_ShipType)
    {
        if(ship->dontrotateever)
        {
            return TRUE;
        }
    }
    matGetVectFromMatrixCol3(curheading,ship->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol1(curup,ship->rotatable.rotinfo.coordsys);

    dotProdCheckHeading = vecDotProduct(*desiredHeading,curheading);
    dotProdCheckUp = vecDotProduct(*desiredUp,curup);

#if DEBUG_AITRACK
    dbgMessagef("Heading accuracy: %f",dotProdCheckHeading);
#endif

    if (dotProdCheckHeading <= -1.0f)   // use <= in case inaccuraces cause number slightly less than 1.0
    {
        // Exactly opposite direction, so arbitrarily chose a direction and rotate that way
        physApplyRotToObj(&ship->rotatable,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTXCW],ROT_ABOUTXCW);
        return FALSE;
    }

    if (dotProdCheckUp <= -1.0f)   // use <= in case inaccuraces cause number slightly less than 1.0
    {
        // Exactly opposite direction, so arbitrarily chose a direction and rotate that way
        physApplyRotToObj(&ship->rotatable,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTZCW],ROT_ABOUTZCW);
        return FALSE;
    }

    if ((dotProdCheckHeading >= accuracy) && (dotProdCheckUp >= accuracy))
    {
#if DEBUG_AITRACK
        dbgMessage("On correct heading...");
#endif
        aitrackForceShipZeroRotation(ship);
        return TRUE;     // heading close enough to desired heading, so return
    }

    // Our heading is not close to the desired heading, so we have
    // to do more work...

    // transform desiredHeading into desiredHeading in Ship co-ordinate system

    matMultiplyVecByMat(&desiredHeadingInShipCoordSys,desiredHeading,&ship->rotatable.rotinfo.coordsys);

    // transform desiredUp into desiredUp in Ship co-ordinate system

    matMultiplyVecByMat(&desiredUpInShipCoordSys,desiredUp,&ship->rotatable.rotinfo.coordsys);

    oneovertime = 1.0f / universe.phystimeelapsed;

    // Check if we need to rotate about x axis (yaw)
    if (desiredHeadingInShipCoordSys.y != 0)
    {
        if (desiredHeadingInShipCoordSys.z < 0.0f)
        {
            if (desiredHeadingInShipCoordSys.y < 0.0f)
            {
                desiredrotspeed = ship->guidance.nonstatvars.turnspeed[TURN_ABOUTX];
            }
            else
            {
                desiredrotspeed = -ship->guidance.nonstatvars.turnspeed[TURN_ABOUTX];
            }
        }
        else
        {
            desiredrotspeed = -desiredHeadingInShipCoordSys.y * ship->guidance.nonstatvars.turnspeed[TURN_ABOUTX];
        }

        rotspeed = ship->rotatable.rotinfo.rotspeed.x;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->common.momentOfInertiaX * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTXCCW]),ROT_ABOUTXCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->common.momentOfInertiaX * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTXCW]),ROT_ABOUTXCW);
        }
    }

    // Check if we need to rotate about y axis (pitch)
    if (desiredHeadingInShipCoordSys.x != 0)
    {
        desiredrotspeed = desiredHeadingInShipCoordSys.x * ship->guidance.nonstatvars.turnspeed[TURN_ABOUTY];
        rotspeed = ship->rotatable.rotinfo.rotspeed.y;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->common.momentOfInertiaY * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTYCCW]),ROT_ABOUTYCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->common.momentOfInertiaY * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTYCW]),ROT_ABOUTYCW);
        }
    }

    // Check if we need to rotate about z axis (roll) to make up-vector of ship
    // align with world z axis.

    if (desiredUpInShipCoordSys.y != 0.0f)
    {
        desiredrotspeed = desiredUpInShipCoordSys.y * ship->guidance.nonstatvars.turnspeed[TURN_ABOUTZ];
        rotspeed = ship->rotatable.rotinfo.rotspeed.z;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->common.momentOfInertiaZ * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTZCCW]),ROT_ABOUTZCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->common.momentOfInertiaZ * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTZCW]),ROT_ABOUTZCW);
        }
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : aitrackHeading
    Description : given a desired heading vector, this ai will orient the
                  ship in the correct heading as well as stopping any roll.
    Inputs      : desiredHeading
    Outputs     :
    Return      : TRUE if heading has been successfully tracked.
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        : catAitrackTurn
    Description : CAT (M2-R8 oracle-rung-physics): one axis of aitrackHeadingFunc 0x00418de0 at the exe's x87 shape:
                  the float rotation speed compared with the double desired speed; the torque
                  ((desired - rotspeed) * moment of inertia) * oneovertime, or (rotspeed - desired) * ..., is a double
                  compared with the float strength of that direction (test ah,0x41: above it, the strength is used)
                  and otherwise rounded once to float, then applied
    Inputs      : ship, desired (double), rotspeed, moment of inertia, 1/t, the two directions (ccw first)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
static void catAitrackTurn(SpaceObjRotImpTargGuidance *ship, double desired, real32 rotspeed, real32 moi,
                           real32 oneovertime, uword ccw, uword cw)
{
    double rotstr;
    uword dir;

    if ((double)rotspeed < desired)
    {
        rotstr = ((desired - (double)rotspeed) * moi) * oneovertime;
        dir = ccw;
    }
    else
    {
        rotstr = (((double)rotspeed - desired) * moi) * oneovertime;
        dir = cw;
    }
    if (rotstr > (double)ship->nonstatvars.rotstrength[dir])
    {
        physApplyRotToObj(&ship->rotatable, ship->nonstatvars.rotstrength[dir], dir);
    }
    else
    {
        physApplyRotToObj(&ship->rotatable, (real32)rotstr, dir);
    }
}

bool32 aitrackHeadingFunc(SpaceObjRotImpTargGuidance *ship,vector *desiredHeading,real32 accuracy,real32 upaccuracy,real32 sinbank,udword flags)
{
    StaticInfoHealthGuidance *shipstaticinfo = ship->staticinfo;

    vector curheading;
    vector curup;
    double dotProdCheckHeading;     // CAT: a double on the exe's FPU (M2-R8 oracle-rung-physics)
    real32 dotProdCheckUp;

    vector desiredHeadingInShipCoordSys;

    vector desiredUp = { 0.0f,0.0f,1.0f };
    vector desiredUpInShipCoordSys;

    real32 rotstr;
    real32 oneovertime;

    real32 yawcloseness = 0.0f;

    if(ship->header.objtype == OBJ_ShipType)
    {
        if(((Ship *)ship)->dontrotateever)    //if this flag is set we don't want to rotate the ship at all
        {
            return TRUE;
        }
    }
    // CAT: a zero desired heading counts as reached (aitrackHeadingFunc 0x00418e34: each component compared with the
    //      double 0.0 at 0x0072a4e8)
    if ((desiredHeading->x == 0.0f) && (desiredHeading->y == 0.0f) && (desiredHeading->z == 0.0f))
    {
        return TRUE;
    }
    if (ship->nonstatvars.rotstrength[ROT_PITCHUP] == 0)
    {
        desiredHeading->z = 0.0f;
        vecNormalize(desiredHeading);       // renormalize
    }

    matGetVectFromMatrixCol3(curheading,ship->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol1(curup,ship->rotatable.rotinfo.coordsys);

    aishipStatsDesiredHeading(*desiredHeading);
    aishipStatsActualHeading(curheading);

    // CAT (M2-R8 oracle-rung-physics): the x87 shape (aitrackHeadingFunc 0x00418e8d): the dot product stays a double on
    //      the FPU, (cur.z*des.z + cur.y*des.y) + cur.x*des.x, compared as such with -1.0f and the float accuracy
    dotProdCheckHeading = ((double)curheading.z * desiredHeading->z + (double)curheading.y * desiredHeading->y) +
                          (double)curheading.x * desiredHeading->x;
    dotProdCheckUp = curup.z; //vecDotProduct(desiredUp,curup);

#if DEBUG_AITRACK
    dbgMessagef("Heading accuracy: %f",dotProdCheckHeading);
#endif

    if (dotProdCheckHeading <= -1.0)
    {
        // Exactly opposite direction, so arbitrarily chose a direction and rotate that way
        physApplyRotToObj(&ship->rotatable,ship->nonstatvars.rotstrength[ROT_ABOUTXCW],ROT_ABOUTXCW);
        return FALSE;
    }

    if ((dotProdCheckHeading >= (double)accuracy) &&
        ((flags & (AITRACKHEADING_IGNOREUPVEC+AITRACKHEADING_DONTROLL)) || (dotProdCheckUp >= upaccuracy)) )
    {
#if DEBUG_AITRACK
        dbgMessage("On correct heading...");
#endif
		if(!(flags & AITRACK_DONT_ZERO_ME))
		{
			aitrackForceGuidanceZeroRotation(ship);
		}
        return TRUE;     // heading close enough to desired heading, so return
    }

    // Our heading is not close to the desired heading, so we have
    // to do more work...

    // transform desiredHeading into desiredHeading in Ship co-ordinate system

    matMultiplyVecByMat(&desiredHeadingInShipCoordSys,desiredHeading,&ship->rotatable.rotinfo.coordsys);

    // transform desiredUp into desiredUp in Ship co-ordinate system

    matMultiplyVecByMat(&desiredUpInShipCoordSys,&desiredUp,&ship->rotatable.rotinfo.coordsys);

    oneovertime = 1.0f / universe.phystimeelapsed;

    // CAT (M2-R8 oracle-rung-physics): the x87 shape (aitrackHeadingFunc 0x00418f56..0x004191f3): each desired
    //      rotation speed stays a double (the yaw's -y * turnspeed, the pitch's x * turnspeed, the roll's
    //      (up.y - yawcloseness) * turnspeed), compared with the float rotation speed as a double, and the torque
    //      ((desired - rotspeed) * moment of inertia) * (1 / t) is one double rounded once where it is stored below its
    //      cap (catAitrackTurn); yawcloseness is stored as a float, as in the exe

    // Check if we need to rotate about x axis (yaw)
    if (desiredHeadingInShipCoordSys.y != 0)
    {
        double desiredrotspeedx;

        if (desiredHeadingInShipCoordSys.z < 0.0f)
        {
            if (desiredHeadingInShipCoordSys.y < 0.0f)
            {
                desiredrotspeedx = ship->nonstatvars.turnspeed[TURN_ABOUTX];
            }
            else
            {
                desiredrotspeedx = -(double)ship->nonstatvars.turnspeed[TURN_ABOUTX];
            }
        }
        else
        {
            desiredrotspeedx = -((double)desiredHeadingInShipCoordSys.y * ship->nonstatvars.turnspeed[TURN_ABOUTX]);
        }

        if (sinbank != 0.0f)
        {
            if (desiredHeadingInShipCoordSys.z < 0.0f)
            {
                if (desiredHeadingInShipCoordSys.y < 0.0f)
                {
                    yawcloseness = sinbank;
                }
                else
                {
                    yawcloseness = -sinbank;
                }
            }
            else
            {
                yawcloseness = (real32)(-((double)desiredHeadingInShipCoordSys.y * sinbank));
            }
        }
        catAitrackTurn(ship, desiredrotspeedx, ship->rotatable.rotinfo.rotspeed.x, shipstaticinfo->common.momentOfInertiaX,
                       oneovertime, ROT_ABOUTXCCW, ROT_ABOUTXCW);
    }

    // Check if we need to rotate about y axis (pitch)
    if (desiredHeadingInShipCoordSys.x != 0)
    {
        catAitrackTurn(ship, (double)desiredHeadingInShipCoordSys.x * ship->nonstatvars.turnspeed[TURN_ABOUTY],
                       ship->rotatable.rotinfo.rotspeed.y, shipstaticinfo->common.momentOfInertiaY, oneovertime,
                       ROT_ABOUTYCCW, ROT_ABOUTYCW);
    }

    if (flags & AITRACKHEADING_DONTROLL)
    {
        return FALSE;
    }

    // Check if we need to rotate about z axis (roll) to make up-vector of ship
    // align with world z axis.

    if (desiredUpInShipCoordSys.y != yawcloseness)
    {
        catAitrackTurn(ship, ((double)desiredUpInShipCoordSys.y - yawcloseness) * ship->nonstatvars.turnspeed[TURN_ABOUTZ],
                       ship->rotatable.rotinfo.rotspeed.z, shipstaticinfo->common.momentOfInertiaZ, oneovertime,
                       ROT_ABOUTZCCW, ROT_ABOUTZCW);
    }

    return FALSE;
}

bool32 aitrackHeadingWithBankPitchFunc(Ship *ship,vector *desiredHeading,real32 accuracy,real32 sinbank,real32 pitchdescend,real32 pitchturn,udword flags)
{
    vector curheading;
    vector desheading;
    real32 desheadingsqr;
    double curheadingsqr;
    real32 temp1;
    double temp2;
    double desheadingy;

    vector curup;

    double dotprodheading;
    real32 dotProdCheckUp;

    double actualpitchturn;
    double finalpitch;
    long double tanpitch;
    double tanhi;
    double norm;

    if(ship->header.objtype == OBJ_ShipType)
    {
        if(ship->dontrotateever)
        {
            //don't rotate this ship
            return TRUE;
        }
    }
    matGetVectFromMatrixCol3(curheading,ship->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol1(curup,ship->rotatable.rotinfo.coordsys);

    // I will treat curheading.z as = 0, but will not set it.

    desheading.x = desiredHeading->x;
    desheading.y = desiredHeading->y;
    // I will treat desheading.z as = 0, but will not set it.

    // CAT (M2-R8 oracle-rung-physics): the x87 shape (aitrackHeadingWithBankPitchFunc 0x00419200): the desired
    //      heading's square (y*y + x*x) is stored as a float, the current heading's stays a double; each goes to
    //      fsqrt as a double; 1 / |cur| is stored as a float, 1 / |des| stays a double; the scaled des.y is stored as
    //      a float but the dot product uses its unrounded double ((des.y * cur.y) + des.x * cur.x, a double); the
    //      pitch is a double, its fptan is the x87's extended result (here tanl: long double is the x87 format on
    //      the x86 builds), stored once as the float des.z and multiplied UNROUNDED by that float, + 1.0f, into
    //      fsqrt; the double reciprocal scales the three stored components
    desheadingsqr = (real32)((double)desheading.y * desheading.y + (double)desheading.x * desheading.x);
    curheadingsqr = (double)curheading.y * curheading.y + (double)curheading.x * curheading.x;
    if (desheadingsqr == 0.0f)       // extreme case where heading directly up
    {
        desheading.x = 1.0f;
        desheadingsqr = 1.0f;
    }
    if (curheadingsqr == 0.0)      // extreme case where heading directly up
    {
        curheading.x = 1.0f;
        curheadingsqr = 1.0;
    }

    temp1 = (real32)(1.0 / catFsqrtD(curheadingsqr));
    temp2 = 1.0 / catFsqrtD(desheadingsqr);

    curheading.x = (real32)((double)temp1 * curheading.x);
    curheading.y = (real32)((double)temp1 * curheading.y);
    desheading.x = (real32)(temp2 * desheading.x);
    desheadingy = temp2 * desheading.y;
    desheading.y = (real32)desheadingy;

    dotprodheading = desheadingy * curheading.y + (double)desheading.x * curheading.x; // no need to mulitply z components because it is 0

    dotProdCheckUp = curup.z;   //vecDotProduct({0,0,1},curup);

    if ((dotprodheading >= (double)accuracy) && (pitchdescend == 0.0f) &&
        isBetweenExclusive(curheading.z,STABILIZE_Z_LO,STABILIZE_Z_HI) &&
        ((flags & AITRACKHEADING_IGNOREUPVEC) || (dotProdCheckUp >= accuracy)) )
    {
#if DEBUG_AITRACK
        dbgMessage("Capital ship on correct heading...");
#endif
        ship->rotatable.rotinfo.rotspeed.x = 0.0f;
        ship->rotatable.rotinfo.rotspeed.y = 0.0f;
        ship->rotatable.rotinfo.rotspeed.z = 0.0f;
        return TRUE;     // heading close enough to desired heading, so return
    }

    actualpitchturn = (1.0 - ((dotprodheading < 0.0) ? -dotprodheading : dotprodheading)) * pitchturn;

    if (pitchdescend < 0.0f)
    {
        actualpitchturn = -actualpitchturn;
        if ((double)pitchdescend < actualpitchturn)
        {
            finalpitch = pitchdescend;
        }
        else
        {
            finalpitch = actualpitchturn;
        }
    }
    else
    {
        if ((double)pitchdescend > actualpitchturn)
        {
            finalpitch = pitchdescend;
        }
        else
        {
            finalpitch = actualpitchturn;
        }
    }

    // we now have the final desired pitch, finalpitch.

    tanpitch = tanl((long double)finalpitch);
    desheading.z = (real32)tanpitch;     // z = tan(theta) * 1 (because sqrt(desheading.x^2 + desheading.y^2) == 1)

    // the x87 rounds the extended tan times the float once, to 53 bits: fma over the exact split tan = hi + lo
    tanhi = (double)tanpitch;
    norm = 1.0 / catFsqrtD(fma(tanhi, (double)desheading.z, (double)(tanpitch - (long double)tanhi) * desheading.z) + 1.0);

    desheading.x = (real32)(norm * desheading.x);       // normalize vector
    desheading.y = (real32)(norm * desheading.y);
    desheading.z = (real32)((double)desheading.z * norm);

    return aitrackHeadingFunc((SpaceObjRotImpTargGuidance *)ship,&desheading,accuracy,accuracy, sinbank,flags);
}

/*-----------------------------------------------------------------------------
    Name        : aitrackRightVector
    Description : given a desired right vector, this ai will orient the
                  ship in the correct heading as well as stopping any roll.
    Inputs      : desiredRightVector
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 aitrackRightVector(Ship *ship,vector *desiredRightVector,real32 accuracy)
{
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;

    vector curright;
    vector curup;
    real32 dotProdCheckRight;
    real32 dotProdCheckUp;

    vector desiredRightInShipCoordSys;

    vector desiredUp = { 0.0f,0.0f,1.0f };
    vector desiredUpInShipCoordSys;

    real32 desiredrotspeed;
    real32 rotspeed;

    real32 rotstr;
    real32 oneovertime;

    if(ship->header.objtype == OBJ_ShipType)
    {
        if(ship->dontrotateever)
        {
            //don't rotate this ship
            return TRUE;
        }
    }
    matGetVectFromMatrixCol2(curright,ship->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol1(curup,ship->rotatable.rotinfo.coordsys);

    dotProdCheckRight = vecDotProduct(*desiredRightVector,curright);
    dotProdCheckUp = curup.z; //vecDotProduct(desiredUp,curup)

#if DEBUG_AITRACK
    dbgMessagef("Heading accuracy: %f",dotProdCheckRight);
#endif

    if (dotProdCheckRight <= -1.0f)
    {
        // Exactly opposite direction, so arbitrarily chose a direction and rotate that way
        physApplyRotToObj(&ship->rotatable,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTXCW],ROT_ABOUTXCW);
        return FALSE;
    }

    if ((dotProdCheckRight >= accuracy) && (dotProdCheckUp >= AITRACKRIGHT_UPACCURACY))
    {
#if DEBUG_AITRACK
        dbgMessage("On correct heading...");
#endif
        aitrackForceShipZeroRotation(ship);
        return TRUE;     // heading close enough to desired heading, so return
    }

    // Our heading is not close to the desired heading, so we have
    // to do more work...

    // transform desiredRightVector into desiredRightVector in Ship co-ordinate system

    matMultiplyVecByMat(&desiredRightInShipCoordSys,desiredRightVector,&ship->rotatable.rotinfo.coordsys);

    // transform desiredUp into desiredUp in Ship co-ordinate system

    matMultiplyVecByMat(&desiredUpInShipCoordSys,&desiredUp,&ship->rotatable.rotinfo.coordsys);

    oneovertime = 1.0f / universe.phystimeelapsed;

    // Check if we need to rotate about x axis (yaw)
    if (desiredRightInShipCoordSys.z != 0)
    {
        if (desiredRightInShipCoordSys.y < 0.0f)
        {
            if (desiredRightInShipCoordSys.z > 0.0f)
            {
                desiredrotspeed = ship->guidance.nonstatvars.turnspeed[TURN_ABOUTX];
            }
            else
            {
                desiredrotspeed = -ship->guidance.nonstatvars.turnspeed[TURN_ABOUTX];
            }
        }
        else
        {
            desiredrotspeed = desiredRightInShipCoordSys.z * ship->guidance.nonstatvars.turnspeed[TURN_ABOUTX];
        }

        rotspeed = ship->rotatable.rotinfo.rotspeed.x;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->common.momentOfInertiaX * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTXCCW]),ROT_ABOUTXCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->common.momentOfInertiaX * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTXCW]),ROT_ABOUTXCW);
        }
    }

    // Check if we need to rotate about y axis (pitch) to make up-vector of ship
    // align with world z axis.

    if (desiredUpInShipCoordSys.z != 0)
    {
        if (desiredUpInShipCoordSys.x < 0.0f)
        {
            if (desiredUpInShipCoordSys.z < 0.0f)
            {
                desiredrotspeed = ship->guidance.nonstatvars.turnspeed[TURN_ABOUTY];
            }
            else
            {
                desiredrotspeed = -ship->guidance.nonstatvars.turnspeed[TURN_ABOUTY];
            }
        }
        else
        {
            desiredrotspeed = -desiredUpInShipCoordSys.z * ship->guidance.nonstatvars.turnspeed[TURN_ABOUTY];
        }

        rotspeed = ship->rotatable.rotinfo.rotspeed.y;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->common.momentOfInertiaY * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTYCCW]),ROT_ABOUTYCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->common.momentOfInertiaY * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTYCW]),ROT_ABOUTYCW);
        }
    }

    // Check if we need to rotate about z axis (roll)
    if (desiredRightInShipCoordSys.x != 0)
    {
        if (desiredRightInShipCoordSys.y < 0.0f)
        {
            if (desiredRightInShipCoordSys.x < 0.0f)
            {
                desiredrotspeed = ship->guidance.nonstatvars.turnspeed[TURN_ABOUTZ];
            }
            else
            {
                desiredrotspeed = -ship->guidance.nonstatvars.turnspeed[TURN_ABOUTZ];
            }
        }
        else
        {
            desiredrotspeed = -desiredRightInShipCoordSys.x * ship->guidance.nonstatvars.turnspeed[TURN_ABOUTZ];
        }

        rotspeed = ship->rotatable.rotinfo.rotspeed.z;

        if (rotspeed < desiredrotspeed)
        {
            rotstr = (desiredrotspeed - rotspeed) * shipstaticinfo->common.momentOfInertiaZ * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTZCCW]),ROT_ABOUTZCCW);
        }
        else
        {
            rotstr = (rotspeed - desiredrotspeed) * shipstaticinfo->common.momentOfInertiaZ * oneovertime;
            physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTZCW]),ROT_ABOUTZCW);
        }
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : aitrackVelocityVector
    Description : given a velocity vector, makes the ship track it.  Note that
                  it does not change the heading, only the velocity of the ship.
    Inputs      : desiredVelocity
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        : catAitrackThrust
    Description : CAT (M2-R8 oracle-rung-physics): one axis of aitrackVelocityVectorGuidance 0x00419460 at the exe's
                  x87 shape: the difference of the two float components times the float mass / t is a double,
                  compared with the float thrust strength (above it, the strength is applied) and otherwise rounded
                  once to float
----------------------------------------------------------------------------*/
static void catAitrackThrust(SpaceObjRotImpTargGuidance *ship, real32 shipv, real32 desv, real32 massOverTime,
                             uword up, uword down)
{
    double thruststr;
    uword dir;

    if (shipv < desv)
    {
        thruststr = ((double)desv - shipv) * massOverTime;
        dir = up;
    }
    else if (shipv > desv)
    {
        thruststr = ((double)shipv - desv) * massOverTime;
        dir = down;
    }
    else
    {
        return;
    }
    if (thruststr > (double)ship->nonstatvars.thruststrength[dir])
    {
        physApplyForceToObj(&ship->obj, ship->nonstatvars.thruststrength[dir], dir);
    }
    else
    {
        physApplyForceToObj(&ship->obj, (real32)thruststr, dir);
    }
}

void aitrackVelocityVectorGuidance(SpaceObjRotImpTargGuidance *ship,vector *desiredVelocity)
{
    StaticInfoHealthGuidance *shipstaticinfo = ship->staticinfo;

    double ex, ey, ez;
    double errorMagSquared;
    real32 massOverTime;

    vector desiredVelocityInShipCoordSys;
    vector shipVelocityInShipCoordSys;

    // check if desiredVelocity is approximately the same as current velocity

    // CAT (M2-R8 oracle-rung-physics): the x87 shape (aitrackVelocityVectorGuidance 0x00419460): the error vector and
    //      its square (ez*ez + ey*ey) + ex*ex stay doubles, compared as such with ERROR_VELOCITY_SQUARED
    ex = (double)desiredVelocity->x - ship->obj.posinfo.velocity.x;
    ey = (double)desiredVelocity->y - ship->obj.posinfo.velocity.y;
    ez = (double)desiredVelocity->z - ship->obj.posinfo.velocity.z;
    errorMagSquared = (ez * ez + ey * ey) + ex * ex;

#if DEBUG_AITRACK
    dbgMessagef("Velocity Error = %f",errorMagSquared);
#endif

    if (errorMagSquared < (double)ERROR_VELOCITY_SQUARED)
    {
#if DEBUG_AITRACK
        dbgMessage("Velocity tracking locked...");
#endif
        return;     // ship velocity close enought to desired velocity, so return
    }

    // current velocity is not close enought to desiredVelocity, so
    // we have to do more work.

    // transfrom desiredVelocity into desiredVelocity in ship co-ordinate system
    // transfrom shipVelocity into shipVelocity in ship co-ordinate system

    matMultiplyVecByMat(&desiredVelocityInShipCoordSys,desiredVelocity,&ship->rotatable.rotinfo.coordsys);
    matMultiplyVecByMat(&shipVelocityInShipCoordSys,&ship->obj.posinfo.velocity,&ship->rotatable.rotinfo.coordsys);

    massOverTime = shipstaticinfo->common.mass / universe.phystimeelapsed;

    catAitrackThrust(ship, shipVelocityInShipCoordSys.x, desiredVelocityInShipCoordSys.x, massOverTime, TRANS_UP, TRANS_DOWN);
    catAitrackThrust(ship, shipVelocityInShipCoordSys.y, desiredVelocityInShipCoordSys.y, massOverTime, TRANS_RIGHT, TRANS_LEFT);
    catAitrackThrust(ship, shipVelocityInShipCoordSys.z, desiredVelocityInShipCoordSys.z, massOverTime, TRANS_FORWARD, TRANS_BACKWARD);
}

/*-----------------------------------------------------------------------------
    Name        : aitrackIsZeroVelocity
    Description : returns TRUE if ship has stopped drifting
    Inputs      :
    Outputs     :
    Return      : returns TRUE if ship has stopped drifting
----------------------------------------------------------------------------*/
bool32 aitrackIsZeroVelocity(Ship *ship)
{
    vector shipVelocity;

    shipVelocity = ship->obj.posinfo.velocity;

    if (isBetweenExclusive(shipVelocity.x,STILL_VELOCITY_LO,STILL_VELOCITY_HI) &&
        isBetweenExclusive(shipVelocity.y,STILL_VELOCITY_LO,STILL_VELOCITY_HI) &&
        isBetweenExclusive(shipVelocity.z,STILL_VELOCITY_LO,STILL_VELOCITY_HI) )
    {
        return TRUE;     // ship velocity close enought to desired velocity, so return
    }
    else
    {
        return FALSE;
    }
}

bool32 aitrackZeroForwardVelocity(Ship *ship)
{
    ShipStaticInfo *shipstaticinfo = ship->staticinfo;
    real32 thruststr;
    real32 massOverTime;
    real32 forwardvelocity;
    vector heading;

    matGetVectFromMatrixCol3(heading,ship->rotatable.rotinfo.coordsys);
    forwardvelocity = vecDotProduct(heading,ship->obj.posinfo.velocity);

    if (isBetweenExclusive(forwardvelocity,STILL_VELOCITY_LO,STILL_VELOCITY_HI))
    {
        return TRUE;    // ship velocity close enought to desired velocity, so return
    }

    massOverTime = shipstaticinfo->common.mass / universe.phystimeelapsed;

    if (forwardvelocity < 0)
    {
        thruststr = -forwardvelocity * massOverTime;
        physApplyForceToObj(&ship->obj,capNumber(thruststr,ship->guidance.nonstatvars.thruststrength[TRANS_FORWARD]),TRANS_FORWARD);
    }
    else
    {
        thruststr = forwardvelocity * massOverTime;
        physApplyForceToObj(&ship->obj,capNumber(thruststr,ship->guidance.nonstatvars.thruststrength[TRANS_BACKWARD]),TRANS_BACKWARD);
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : aitrackZeroVelocity
    Description : stops ship from drifting
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 aitrackZeroVelocity(Ship *ship)
{
    vector shipVelocity;
    vector shipVelocityInShipCoordSys;
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;
    real32 thruststr;
    real32 massOverTime;

    shipVelocity = ship->obj.posinfo.velocity;

    if (isBetweenExclusive(shipVelocity.x,STILL_VELOCITY_LO,STILL_VELOCITY_HI) &&
        isBetweenExclusive(shipVelocity.y,STILL_VELOCITY_LO,STILL_VELOCITY_HI) &&
        isBetweenExclusive(shipVelocity.z,STILL_VELOCITY_LO,STILL_VELOCITY_HI) )
    {
        return TRUE;     // ship velocity close enought to desired velocity, so return
    }

#if DEBUG_AITRACK
    dbgMessagef("Zeroing velocity %f %f %f %x",hshipVelocity.x,hshipVelocity.y,hshipVelocity.z,(udword)ship);
#endif

    // transfrom shipVelocity into shipVelocity in ship co-ordinate system
    matMultiplyVecByMat(&shipVelocityInShipCoordSys,&shipVelocity,&ship->rotatable.rotinfo.coordsys);

    massOverTime = shipstaticinfo->common.mass / universe.phystimeelapsed;
    if (shipVelocityInShipCoordSys.x < 0)
    {
        thruststr = -shipVelocityInShipCoordSys.x * massOverTime;
        physApplyForceToObj(&ship->obj,capNumber(thruststr,ship->guidance.nonstatvars.thruststrength[TRANS_UP]),TRANS_UP);
    }
    else
    {
        thruststr = shipVelocityInShipCoordSys.x * massOverTime;
        physApplyForceToObj(&ship->obj,capNumber(thruststr,ship->guidance.nonstatvars.thruststrength[TRANS_DOWN]),TRANS_DOWN);
    }

    if (shipVelocityInShipCoordSys.y < 0)
    {
        thruststr = -shipVelocityInShipCoordSys.y * massOverTime;
        physApplyForceToObj(&ship->obj,capNumber(thruststr,ship->guidance.nonstatvars.thruststrength[TRANS_RIGHT]),TRANS_RIGHT);
    }
    else
    {
        thruststr = shipVelocityInShipCoordSys.y * massOverTime;
        physApplyForceToObj(&ship->obj,capNumber(thruststr,ship->guidance.nonstatvars.thruststrength[TRANS_LEFT]),TRANS_LEFT);
    }

    if (shipVelocityInShipCoordSys.z < 0)
    {
        thruststr = -shipVelocityInShipCoordSys.z * massOverTime;
        physApplyForceToObj(&ship->obj,capNumber(thruststr,ship->guidance.nonstatvars.thruststrength[TRANS_FORWARD]),TRANS_FORWARD);
    }
    else
    {
        thruststr = shipVelocityInShipCoordSys.z * massOverTime;
        physApplyForceToObj(&ship->obj,capNumber(thruststr,ship->guidance.nonstatvars.thruststrength[TRANS_BACKWARD]),TRANS_BACKWARD);
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : aitrackSteadyShipDriftOnly
    Description : stops ship from drifting only
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 aitrackSteadyShipDriftOnly(Ship *ship)
{
    // CAT: no gun-gimbal reset (GoK's, off with /vanilla): cataclysm.exe's aitrackSteadyShipDriftOnly 0x00419a00 zeroes the
    //      velocity once aitrackZeroVelocity says so and returns TRUE
    if (aitrackZeroVelocity(ship))
    {
        aitrackForceZeroVelocity(ship);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : aitrackIsShipSteadyAnywhere
    Description : returns TRUE if ship is steady anywhere (not necessarily oriented up)
    Inputs      : ship
    Outputs     :
    Return      : returns TRUE if ship is steady anywhere (not necessarily oriented up)
----------------------------------------------------------------------------*/
bool32 aitrackIsShipSteadyAnywhere(Ship *ship)
{
    return (aitrackIsZeroRotation(ship) & aitrackIsZeroVelocity(ship));
}

/*-----------------------------------------------------------------------------
    Name        : aitrackSteadyShipAnywhere
    Description : stops ship from drifting, and stops ship from rotating (not necessarily oriented up)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 aitrackSteadyShipAnywhere(Ship *ship)
{
    bool32 stabilized = aitrackZeroRotationAnywhere(ship);
    bool32 zerovel = aitrackZeroVelocity(ship);

    // CAT: no gun-gimbal reset (GoK's, off with /vanilla; aitrackSteadyShipAnywhere 0x00419b60)
    if (stabilized && zerovel)
    {
        aitrackForceSteadyShip(ship);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : aitrackIsShipSteady
    Description : returns TRUE if ship is steady
    Inputs      : ship
    Outputs     :
    Return      : returns TRUE if ship is steady
----------------------------------------------------------------------------*/
bool32 aitrackIsShipSteady(Ship *ship)
{
    return (aitrackIsStabilizeShip(ship) & aitrackIsZeroVelocity(ship));
}

/*-----------------------------------------------------------------------------
    Name        : aitrackSteadyShip
    Description : stops ship from drifting, and stops ship from rotating
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 aitrackSteadyShip(Ship *ship)
{
    bool32 stabilized = aitrackStabilizeShip(ship);
    bool32 zerovel = aitrackZeroVelocity(ship);

    // CAT: no gun-gimbal reset (GoK's, off with /vanilla; aitrackSteadyShip 0x00419c70)
    if (stabilized && zerovel)
    {
        aitrackForceSteadyShip(ship);
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : aitrackForceSteadyShip
    Description : stops ship from drifting/rotating, perfectly.
                  Use as an approximation to aitrackSteadyShip, if ship is
                  moving very slow.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void aitrackForceSteadyShip(Ship *ship)
{
    if (ship->header.flags & SOF_Crazy)
    {
        return;     // ship is crazy, so return
    }

    ship->obj.posinfo.velocity.x = 0.0f;
    ship->obj.posinfo.velocity.y = 0.0f;
    ship->obj.posinfo.velocity.z = 0.0f;

    ship->rotatable.rotinfo.rotspeed.x = 0.0f;
    ship->rotatable.rotinfo.rotspeed.y = 0.0f;
    ship->rotatable.rotinfo.rotspeed.z = 0.0f;
}

/*-----------------------------------------------------------------------------
    Name        : aitrackForceShipZeroRotation
    Description : stops ships from rotating, perfectly.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void aitrackForceGuidanceZeroRotation(SpaceObjRotImpTargGuidance *ship)
{
    if (ship->header.flags & SOF_Crazy)
    {
        return;     // ship is crazy, so return
    }

    ship->rotatable.rotinfo.rotspeed.x = 0.0f;
    ship->rotatable.rotinfo.rotspeed.y = 0.0f;
    ship->rotatable.rotinfo.rotspeed.z = 0.0f;
}

/*-----------------------------------------------------------------------------
    Name        : aitrackForceZeroVelocity
    Description : stops ship from moving, perfectly
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void aitrackForceZeroVelocity(Ship *ship)
{
    if (ship->header.flags & SOF_Crazy)
    {
        return;     // ship is crazy, so return
    }

    ship->obj.posinfo.velocity.x = 0.0f;
    ship->obj.posinfo.velocity.y = 0.0f;
    ship->obj.posinfo.velocity.z = 0.0f;
}

/*-----------------------------------------------------------------------------
    Name        : aitrackForceHeading
    Description : points ship in a particular direction
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void aitrackForceHeading(Ship *ship, vector *heading, vector *upvector)
{
    vector rightvec;
    if (ship->header.flags & SOF_Crazy)
    {
        return;     // ship is crazy, so return
    }

    vecCrossProduct(rightvec,*heading,*upvector);
    // set up vector
    ship->rotatable.rotinfo.coordsys.m11 = upvector->x;
    ship->rotatable.rotinfo.coordsys.m21 = upvector->y;
    ship->rotatable.rotinfo.coordsys.m31 = upvector->z;
    // set right vector
    ship->rotatable.rotinfo.coordsys.m12 = rightvec.x;
    ship->rotatable.rotinfo.coordsys.m22 = rightvec.y;
    ship->rotatable.rotinfo.coordsys.m32 = rightvec.z;
    // set heading vector
    ship->rotatable.rotinfo.coordsys.m13 = heading->x;
    ship->rotatable.rotinfo.coordsys.m23 = heading->y;
    ship->rotatable.rotinfo.coordsys.m33 = heading->z;
}


/*-----------------------------------------------------------------------------
    Name        : MoveReachedDestinationVariable
    Description : returns TRUE if ship has reached destination (or is close enough)
    Inputs      : ship,destination,bounds
    Outputs     :
    Return      : TRUE if ship has reached destination (or is close enough)
----------------------------------------------------------------------------*/
bool32 MoveReachedDestinationVariable(Ship *ship,vector *destination,real32 bounds)
{
    vector lefttogo;
    real32 negbounds = -bounds;

    vecSub(lefttogo,*destination,ship->obj.posinfo.position);

    if ((isBetweenExclusive(lefttogo.x,negbounds,bounds)) &&
        (isBetweenExclusive(lefttogo.y,negbounds,bounds)) &&
        (isBetweenExclusive(lefttogo.z,negbounds,bounds)))
    {
#if DEBUG_AITRACK
        dbgMessagef("Ship %x reached destination!",(udword)ship);
#endif
        return TRUE;
    }
    else
    {
#if DEBUG_AITRACK
        dbgMessagef("Left to go: %f %f %f",lefttogo.x,lefttogo.y,lefttogo.z);
#endif
        return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_MoveReachedDestinationRadius
    Description : CAT: TRUE when ship is within radius of destination (a sphere, not
                  MoveReachedDestinationVariable's box; cat_MoveReachedDestinationRadius 0x00419e80)
    Inputs      : ship, destination, radius
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 cat_MoveReachedDestinationRadius(Ship *ship,vector *destination,real32 radius)
{
    real32 dx = ship->obj.posinfo.position.x - destination->x;
    real32 dy = ship->obj.posinfo.position.y - destination->y;
    real32 dz = ship->obj.posinfo.position.z - destination->z;

    return (dx * dx + dy * dy + dz * dz < radius * radius);
}

/*-----------------------------------------------------------------------------
    Name        : MoveLeftToGo
    Description : returns the maximum distance along a axis which the ship has left to go
    Inputs      : ship, destination
    Outputs     :
    Return      : returns the maximum distance along a axis which the ship has left to go
----------------------------------------------------------------------------*/
real32 MoveLeftToGo(Ship *ship,vector *destination)
{
    real32 absx = ABS(destination->x - ship->obj.posinfo.position.x);
    real32 absy = ABS(destination->y - ship->obj.posinfo.position.y);
    real32 absz = ABS(destination->z - ship->obj.posinfo.position.z);

    return max(max(absx,absy),absz);
}


/*-----------------------------------------------------------------------------
    Name        : aitrackRotationSpeed
    Description : make's a ship track a desired speed
    Inputs      : ship, rotation speed, track
    Outputs     : none
    Return      : void
----------------------------------------------------------------------------*/
bool32 aitrackRotationSpeed(Ship *ship, real32 desiredrotspeed, uword track)
{
    real32 rotstr      = 0.0;
    real32 oneovertime = 0.0;
    real32 rotspeed    = 0.0;

    oneovertime = 1.0f / universe.phystimeelapsed;

    switch (track)
    {
        case ROT_ABOUTXCCW :
        case ROT_ABOUTXCW  :
            if (isBetweenExclusive(ship->rotatable.rotinfo.rotspeed.x,
                desiredrotspeed+STILL_ROT_LO,
                desiredrotspeed+STILL_ROT_HI))
                return(TRUE);
            rotspeed = ship->rotatable.rotinfo.rotspeed.x;
            rotstr = (desiredrotspeed - rotspeed) * ship->staticinfo->common.momentOfInertiaX * oneovertime;
        break;
        case ROT_ABOUTYCCW :
        case ROT_ABOUTYCW  :
            if (isBetweenExclusive(ship->rotatable.rotinfo.rotspeed.y,
                desiredrotspeed+STILL_ROT_LO,
                desiredrotspeed+STILL_ROT_HI))
                return(TRUE);
            rotspeed = ship->rotatable.rotinfo.rotspeed.y;
            rotstr = (desiredrotspeed - rotspeed) * ship->staticinfo->common.momentOfInertiaY * oneovertime;
        break;
        case ROT_ABOUTZCCW :
        case ROT_ABOUTZCW  :
            if (isBetweenExclusive(ship->rotatable.rotinfo.rotspeed.z,
                desiredrotspeed+STILL_ROT_LO,
                desiredrotspeed+STILL_ROT_HI))
                return(TRUE);
            rotspeed = ship->rotatable.rotinfo.rotspeed.z;
            rotstr = (desiredrotspeed - rotspeed) * ship->staticinfo->common.momentOfInertiaZ * oneovertime;
        break;
    }

    if (rotspeed < desiredrotspeed)
    {
        physApplyRotToObj(&ship->rotatable,capNumber(rotstr,ship->guidance.nonstatvars.rotstrength[track]),track);
        return(FALSE);
    }
    else
    {
        rotstr = -capNumber(-rotstr,ship->guidance.nonstatvars.rotstrength[ROT_ABOUTZCW]);
        physApplyRotToObj(&ship->rotatable,rotstr,track);
        return(FALSE);
    }
}

