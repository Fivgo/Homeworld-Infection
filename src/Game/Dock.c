/*=============================================================================
    Name    : Dock.c
    Purpose : Contains code for docking, and refueling/repairing

    Created 11/25/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "Dock.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "AIShip.h"
#include "AITrack.h"
#include "AIVar.h"
#include "Alliance.h"
#include "Battle.h"
#include "Clamp.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "Debug.h"
#include "FastMath.h"
#include "LaunchMgr.h"
#include "LinkedList.h"
#include "MadLinkIn.h"
#include "MadLinkInDefs.h"
#include "Memory.h"
#include "Mothership.h"
#include "ObjTypes.h"
#include "Physics.h"
#include "prim3d.h"
#include "Randy.h"
#include "Salvage.h"
#include "Select.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "SpeechEvent.h"
#include "Tactics.h"
#include "Tutor.h"
#include "Tweak.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "Vector.h"
#include "OracleDump.h"     // CAT: the dock self-test rides on the headless mission run (/oracleMission)
// CAT (M2-R5 dock): the Cataclysm ships and systems the dock state machine calls
#include "Acolyte.h"
#include "ACV.h"
#include "Attributes.h"
#include "Carrier.h"
#include "Crystal.h"
#include "HiveFrigate.h"
#include "MeshAnim.h"
#include "Mimic.h"
#include "ModuleShip.h"
#include "RammingFrigate.h"
#include "ShipDisable.h"
#include "Slipgate.h"
#include "Worker.h"
#include "CommandWrap.h"
#include "ConsMgr.h"
#include "AIPlayer.h"


#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

//#define DEBUG_DOCKING

// CAT (M2-R5 dock): the dock self-test's trace switch (the cone helpers print their per-tick state for the local
//      player's ships while /catDockTest runs; see the self-test at the end of this file)
bool32 catDockSelfTestTrace = FALSE;

// CAT (M2-R5 dock): HW1's dock tweakables (LATCH_TOLERANCE .. SHIP_NEED_REPAIR_PERCENT, Dock.script) are gone: cataclysm.exe has
//      none of them (strings.csv), and the dock paths that read them are gone with it

/*-----------------------------------------------------------------------------
    Name        : GetDirectionVectorOfShip
    Description : gets a direction vector of ship, given a direction (0=up,1=right,2=forward,4=down,5=left,6=back)
    Inputs      : direction, ship
    Outputs     : result
    Return      :
----------------------------------------------------------------------------*/
void GetDirectionVectorOfShip(vector *result,udword direction,matrix *coordsys)
{
    // CAT: GetDirectionVectorOfShip 0x004631f0 takes a coordinate system, not a ship (cat_linkComputeTargetCoordSys
    //      passes a dock point's frame), and direction 3 leaves result as it was
    switch (direction & 3)
    {
        case 0:
            matGetVectFromMatrixCol1(*result,*coordsys);
            break;

        case 1:
            matGetVectFromMatrixCol2(*result,*coordsys);
            break;

        case 2:
            matGetVectFromMatrixCol3(*result,*coordsys);
            break;
    }

    if (direction & 4)
    {
        vecNegate(*result);
    }
}

/*-----------------------------------------------------------------------------
    Name        : dockFindDockIndex
    Description : finds a dock point with given name
    Inputs      : name, dockstaticinfo
    Outputs     :
    Return      : returns an index to the found dockpoint
----------------------------------------------------------------------------*/
sdword dockFindDockIndex(char *name,DockStaticInfo *dockstaticinfo)
{
    sdword numDockPoints = dockstaticinfo->numDockPoints;
    DockStaticPoint *curdockpoint;
    sdword i;

    for (i=0,curdockpoint=&dockstaticinfo->dockstaticpoints[0];i<numDockPoints;i++,curdockpoint++)
    {
        if (strcasecmp(name,curdockpoint->name) == 0)
        {
            return i;
        }
    }
    dbgFatalf(DBG_Loc,"Dock name %s not found",name);
    return -1;
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockFindDockStaticPoint
    Description : CAT: finds a dock point by name, case-insensitive (cat_dockFindDockStaticPoint 0x00463d00, the exe's
                  _stricmp 0x00676c90). Unlike dockFindDockIndex it returns the point and is not fatal: a missing
                  point gives NULL (the exe's warning for a missing "center" goes to a stripped debug print).
    Inputs      : name, dockstaticinfo
    Outputs     :
    Return      : the dock point, or NULL
----------------------------------------------------------------------------*/
DockStaticPoint *cat_dockFindDockStaticPoint(char *name,DockStaticInfo *dockstaticinfo)
{
    sdword numDockPoints = dockstaticinfo->numDockPoints;
    DockStaticPoint *curdockpoint;
    sdword i;

    for (i=0,curdockpoint=&dockstaticinfo->dockstaticpoints[0];i<numDockPoints;i++,curdockpoint++)
    {
        if (strcasecmp(name,curdockpoint->name) == 0)
        {
            return curdockpoint;
        }
    }
    return NULL;
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockDepositResourceUnits
    Description : CAT: gives a player resource units from a depot (cat_dockDepositResourceUnits 0x00463270: the
                  crystal processing of the salvage depots)
    Inputs      : player, resourceUnits
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void cat_dockDepositResourceUnits(Player *player,sdword resourceUnits)
{
    if (resourceUnits > 0)
    {
        player->resourceUnits += resourceUnits;
        // CAT-TODO(stage 4): the exe also adds them to the player's two AI resource counters (universe.aiplayerEnemy
        //      + playerIndex * 0x7F8, +0x4E and +0x52; the Cataclysm AIPlayer layout is the AI stage's)
        universe.gameStats.totalResourceUnitsCollected += resourceUnits;
        if ((singlePlayerGame) && (player->playerIndex == 0))
        {
            singlePlayerGameInfo.resourceUnitsCollected += resourceUnits;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_salvageOpenAnimIndex / cat_salvageCloseAnimIndex
    Description : CAT: the mesh animation a salvager plays to open / close its claws: the Worker's 3 / 2, none for
                  any other type (0x004638f0 / 0x004639b0)
    Inputs      : type
    Outputs     :
    Return      : the animation number, or -1
----------------------------------------------------------------------------*/
sdword cat_salvageOpenAnimIndex(ShipType type)
{
    switch (type)
    {
        case sWorker:
        case bWorker:
            return 3;
        default:
            return -1;
    }
}

sdword cat_salvageCloseAnimIndex(ShipType type)
{
    switch (type)
    {
        case sWorker:
        case bWorker:
            return 2;
        default:
            return -1;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_matCoordSysAligned
    Description : CAT: TRUE when the up, forward and right axes of two frames are each within cosAngle of each
                  other (cat_matCoordSysAligned 0x00463840, tested in that order)
    Inputs      : a, b, cosAngle
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 cat_matCoordSysAligned(matrix *a,matrix *b,real32 cosAngle)
{
    if (b->m11 * a->m11 + b->m21 * a->m21 + b->m31 * a->m31 < cosAngle)
    {
        return FALSE;
    }
    if (b->m13 * a->m13 + b->m23 * a->m23 + b->m33 * a->m33 < cosAngle)
    {
        return FALSE;
    }
    if (b->m12 * a->m12 + b->m22 * a->m22 + b->m32 * a->m32 < cosAngle)
    {
        return FALSE;
    }
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_linkComputeTargetCoordSys
    Description : CAT: the frame ship must have so that its link point faces the host's dock point
                  (cat_linkComputeTargetCoordSys 0x00463680)
    Inputs      : ship, linkpoint (ship's point), host, hostpoint
    Outputs     : out
    Return      :
----------------------------------------------------------------------------*/
void cat_linkComputeTargetCoordSys(Ship *ship,DockStaticPoint *linkpoint,Ship *host,DockStaticPoint *hostpoint,matrix *out)
{
    matrix pointframe, tmpmat;
    vector heading, up, right;
    vector backward, side, third;

    (void)ship;

    // the host point's frame: up / right / heading from its cone normal and its heading and up directions
    matCreateCoordSysFromHeading(&pointframe, &hostpoint->conenormal);
    GetDirectionVectorOfShip(&heading, hostpoint->headingdirection, &pointframe);
    GetDirectionVectorOfShip(&up, hostpoint->updirection, &pointframe);
    right.x = up.z * heading.y - up.y * heading.z;
    right.y = up.x * heading.z - heading.x * up.z;
    right.z = heading.x * up.y - up.x * heading.y;
    vecNormalize(&right);
    matCreateMatFromVecs(&tmpmat, &up, &right, &heading);
    matMultiplyMatByMat(out, &host->rotatable.rotinfo.coordsys, &tmpmat);

    // undo the link point's own frame (its cone normal reversed)
    backward.x = -linkpoint->conenormal.x;
    backward.y = -linkpoint->conenormal.y;
    backward.z = -linkpoint->conenormal.z;
    side.x = 0.0f - backward.y;
    side.z = 0.0f;
    side.y = backward.x - 0.0f;
    vecNormalize(&side);
    third.x = side.z * backward.y - side.y * backward.z;
    third.y = side.x * backward.z - side.z * backward.x;
    third.z = side.y * backward.x - side.x * backward.y;
    vecNormalize(&third);
    matCreateMatFromVecs(&tmpmat, &backward, &side, &third);
    matTranspose(&tmpmat);
    matMultiplyMatByMat(out, out, &tmpmat);
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockFlyToConeOriginLatch
    Description : CAT: flies ship down the cone of the dock point it holds on dockwith (dockvars.catDockPointIndex)
                  to the point itself, keeping it on the cone axis (cat_dockFlyToConeOriginLatch 0x00464b30)
    Inputs      : ship, dockwith, radius (arrival distance)
    Outputs     :
    Return      : TRUE when it has arrived
----------------------------------------------------------------------------*/
bool32 cat_dockFlyToConeOriginLatch(Ship *ship,Ship *dockwith,real32 radius)
{
    DockStaticPoint *dockpoint;
    vector pointpos, conenormal, heading, up, diff, newpos, dest;
    real32 dist, oneOverDist, along, upDot, headingDot, proj, speed, arrival;
    udword aishipflags;
    bool32 aligned;
    ShipClass shipclass = ship->staticinfo->shipclass;

    dockpoint = &dockwith->staticinfo->dockStaticInfo->dockstaticpoints[ship->dockvars.catDockPointIndex];

    if (cat_shipIsUntargetable(dockwith) &&
        ((shipclass == CLASS_Fighter) || (shipclass == CLASS_Corvette) || (shipclass == CLASS_Micro)))
    {
        matMultiplyMatByVec(&pointpos, &dockwith->rotatable.rotinfo.coordsys, &dockpoint->position);
        vecAddTo(pointpos, dockwith->obj.posinfo.position);
        if (MoveReachedDestinationVariable(ship, &pointpos, ship->staticinfo->common.staticCollInfo.collspheresize))
        {
            return TRUE;
        }
        aishipFlyToPointAvoidingObjsFunc(ship, &pointpos, AISHIP_FirstPointInDirectionFlying|AISHIP_PointInDirectionFlying|AISHIP_FastAsPossible, -100.0f, &dockwith->obj.posinfo.velocity);
        return FALSE;
    }

    matMultiplyMatByVec(&pointpos, &dockwith->rotatable.rotinfo.coordsys, &dockpoint->position);
    vecAddTo(pointpos, dockwith->obj.posinfo.position);
    if (MoveReachedDestinationVariable(ship, &pointpos, radius))
    {
        return TRUE;
    }

    matMultiplyMatByVec(&conenormal, &dockwith->rotatable.rotinfo.coordsys, &dockpoint->conenormal);
    GetDirectionVectorOfShip(&up, dockpoint->updirection, &dockwith->rotatable.rotinfo.coordsys);
    if (ship->shiptype == dEcclesiam)
    {
        real32 cx, cy, cz;

        heading.x = -conenormal.x;
        heading.y = -conenormal.y;
        heading.z = -conenormal.z;
        cx = up.y * heading.z - up.z * heading.y;
        cy = up.z * heading.x - up.x * heading.z;
        cz = up.x * heading.y - up.y * heading.x;
        up.x = cz * heading.y - cy * heading.z;
        up.y = cx * heading.z - cz * heading.x;
        up.z = cy * heading.x - cx * heading.y;
    }
    else
    {
        GetDirectionVectorOfShip(&heading, dockpoint->headingdirection, &dockwith->rotatable.rotinfo.coordsys);
    }

    vecSub(diff, ship->obj.posinfo.position, pointpos);
    dist = fsqrt(vecMagnitudeSquared(diff));
    oneOverDist = 1.0f / dist;
    along = conenormal.x * (oneOverDist * diff.x) + conenormal.y * oneOverDist * diff.y + conenormal.z * oneOverDist * diff.z;

    // keep the ship on the cone axis as the axis turns with the host
    newpos.x = (conenormal.x - ship->catDockLatchHeading.x) * dist + ship->obj.posinfo.position.x;
    newpos.y = (conenormal.y - ship->catDockLatchHeading.y) * dist + ship->obj.posinfo.position.y;
    newpos.z = (conenormal.z - ship->catDockLatchHeading.z) * dist + ship->obj.posinfo.position.z;
    cat_univSetShipPosition(ship, &newpos);

    if (ship->shiptype == dEcclesiam)
    {
        matrix *coordsys = &ship->rotatable.rotinfo.coordsys;

        headingDot = coordsys->m13 * heading.x + coordsys->m23 * heading.y + coordsys->m33 * heading.z;
        upDot = coordsys->m11 * up.x + coordsys->m31 * up.z + coordsys->m21 * up.y;
        aitrackHeadingAndUp(ship, &heading, &up, 0.995f);
        aligned = !((headingDot < 0.995f) || (upDot < 0.78f));
    }
    else
    {
        aligned = aitrackHeadingAndUp(ship, &heading, &up, 0.995f);
    }

    if (along < 0.0f)
    {
        return TRUE;
    }
    if ((along < 0.995f) && (100.0f < dist))
    {
        proj = conenormal.x * diff.x + conenormal.y * diff.y + conenormal.z * diff.z;
        dest.x = proj * conenormal.x + pointpos.x;
        dest.y = conenormal.y * proj + pointpos.y;
        dest.z = proj * conenormal.z + pointpos.z;
        aishipFlyToPointAvoidingObjsFunc(ship, &dest, 0, -20.0f, &dockwith->obj.posinfo.velocity);
    }
    if (aligned)
    {
        if (cat_shipIsUntargetable(dockwith))
        {
            aishipflags = AISHIP_FastAsPossible;
            speed = 100.0f;
        }
        else
        {
            aishipflags = 0;
            speed = 120.0f;
        }
        aishipFlyToPointAvoidingObjsFunc(ship, &pointpos, aishipflags, -speed, &dockwith->obj.posinfo.velocity);
        if (cat_shipIsUntargetable(dockwith))
        {
            if (ship->staticinfo->common.staticCollInfo.collspheresize <= 1000.0f)
            {
                arrival = 1000.0f;
            }
            else
            {
                arrival = ship->staticinfo->common.staticCollInfo.collspheresize;
            }
            if (MoveReachedDestinationVariable(ship, &pointpos, arrival))
            {
                return TRUE;
            }
        }
    }
    if (catDockSelfTestTrace && (ship->playerowner == universe.curPlayerPtr) && ((universe.univUpdateCounter % 32) == 0))
    {
        dbgMessagef("docktest: latch tick=%u %s#%u dist=%.1f along=%.3f aligned=%d speed=%.1f pos=(%.0f %.0f %.0f) point=(%.0f %.0f %.0f)",
                    (unsigned)universe.univUpdateCounter, ShipTypeToStr(ship->shiptype), (unsigned)ship->shipID.shipNumber, dist, along, (int)aligned,
                    fsqrt(vecMagnitudeSquared(ship->obj.posinfo.velocity)), ship->obj.posinfo.position.x, ship->obj.posinfo.position.y,
                    ship->obj.posinfo.position.z, pointpos.x, pointpos.y, pointpos.z);
    }
    ship->catDockLatchHeading = conenormal;
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockFlyToConeInside
    Description : CAT: flies ship into the cone of a dock point of dockwith (cat_dockFlyToConeInside 0x004671d0)
    Inputs      : ship, dockwith, dockpoint
    Outputs     :
    Return      : TRUE when it is inside the cone
----------------------------------------------------------------------------*/
bool32 cat_dockFlyToConeInside(Ship *ship,Ship *dockwith,DockStaticPoint *dockpoint)
{
    vector conepos, diff, conenormal, dest;
    real32 dist, clampdist;
    ShipClass shipclass;

    matMultiplyMatByVec(&conepos, &dockwith->rotatable.rotinfo.coordsys, &dockpoint->position);
    vecAddTo(conepos, dockwith->obj.posinfo.position);
    vecSub(diff, ship->obj.posinfo.position, conepos);
    dist = fsqrt(vecMagnitudeSquared(diff));
    if (dist < dockpoint->mindist)
    {
        clampdist = dockpoint->mindist;
    }
    else if (dockpoint->maxdist < dist)
    {
        clampdist = dockpoint->maxdist;
    }
    else
    {
        clampdist = dist;
    }

    matMultiplyMatByVec(&conenormal, &dockwith->rotatable.rotinfo.coordsys, &dockpoint->conenormal);
    dest.x = dockpoint->mindist * conenormal.x + conepos.x;
    dest.y = dockpoint->mindist * conenormal.y + conepos.y;
    dest.z = dockpoint->mindist * conenormal.z + conepos.z;

    if (dist * dockpoint->coneangle < conenormal.x * diff.x + conenormal.y * diff.y + conenormal.z * diff.z + 5.0f)
    {
        shipclass = ship->staticinfo->shipclass;
        if ((dockwith->header.objtype != OBJ_ShipType) || !cat_shipIsUntargetable(dockwith) ||
            ((shipclass != CLASS_Fighter) && (shipclass != CLASS_Corvette) && (shipclass != CLASS_Micro)))
        {
            aitrackForceSteadyShip(ship);
        }
        return TRUE;
    }

    if (dist <= clampdist + 500.0f)
    {
        aishipFlyToPointAvoidingObjsFunc(ship, &dest, AISHIP_FirstPointInDirectionFlying|AISHIP_PointInDirectionFlying, -10.0f, &dockwith->obj.posinfo.velocity);
        return FALSE;
    }
    aishipFlyToPointAvoidingObjsFunc(ship, &dest, AISHIP_FirstPointInDirectionFlying|AISHIP_PointInDirectionFlying|AISHIP_FastAsPossible, -80.0f, &dockwith->obj.posinfo.velocity);
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockFlyToConeOrigin
    Description : CAT: flies ship along the cone of a dock point of dockwith towards its origin
                  (cat_dockFlyToConeOrigin 0x004673c0)
    Inputs      : ship, dockwith, dockpoint
    Outputs     :
    Return      : TRUE when it has arrived
----------------------------------------------------------------------------*/
bool32 cat_dockFlyToConeOrigin(Ship *ship,Ship *dockwith,DockStaticPoint *dockpoint)
{
    vector conepos, diff, conenormal, dest;
    real32 dist, tolerance, speed;
    udword aishipflags;
    ShipClass shipclass = ship->staticinfo->shipclass;
    bool32 untargetableHost = (dockwith->header.objtype == OBJ_ShipType) && cat_shipIsUntargetable(dockwith);

    if (untargetableHost && ((shipclass == CLASS_Fighter) || (shipclass == CLASS_Corvette) || (shipclass == CLASS_Micro)))
    {
        return TRUE;
    }

    matMultiplyMatByVec(&conepos, &dockwith->rotatable.rotinfo.coordsys, &dockpoint->position);
    vecAddTo(conepos, dockwith->obj.posinfo.position);
    vecSub(diff, ship->obj.posinfo.position, conepos);
    dist = fsqrt(vecMagnitudeSquared(diff));
    if (dist < dockpoint->mindist)
    {
        dist = dockpoint->mindist;
    }
    else if (dockpoint->maxdist < dist)
    {
        dist = dockpoint->maxdist;
    }

    matMultiplyMatByVec(&conenormal, &dockwith->rotatable.rotinfo.coordsys, &dockpoint->conenormal);
    dest.x = conenormal.x * dist + conepos.x;
    dest.y = conenormal.y * dist + conepos.y;
    dest.z = conenormal.z * dist + conepos.z;

    if (untargetableHost)
    {
        tolerance = ship->staticinfo->common.staticCollInfo.collspheresize * 0.125f + 25.0f;
    }
    else
    {
        tolerance = 10.0f;
    }
    if (MoveReachedDestinationVariable(ship, &dest, tolerance))
    {
        return TRUE;
    }

    aishipflags = AISHIP_FirstPointInDirectionFlying|AISHIP_PointInDirectionFlying|AISHIP_DontFlyToObscuredPoints|
                  AISHIP_ReturnImmedIfPointObscured|AISHIP_IgnoreDockWithObscuredPoints;    // 0xB3
    if (untargetableHost)
    {
        aishipflags |= AISHIP_FastAsPossible;                                               // 0xB7
        speed = 100.0f;
    }
    else
    {
        speed = 80.0f;
    }
    aishipFlyToPointAvoidingObjsFunc(ship, &dest, aishipflags, -speed, &dockwith->obj.posinfo.velocity);
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_linkFlyIntoPosition
    Description : CAT: lines ship's link point up with host's dock point and snaps it there when position,
                  orientation and both ships' mesh animations are ready (cat_linkFlyIntoPosition 0x004675c0)
    Inputs      : ship, linkpoint (ship's point), host, hostpoint
    Outputs     :
    Return      : TRUE when it is in position
----------------------------------------------------------------------------*/
bool32 cat_linkFlyIntoPosition(Ship *ship,DockStaticPoint *linkpoint,Ship *host,DockStaticPoint *hostpoint)
{
    real32 nearRadiusSq, collsphere, tolerance, alignCos, distSq;
    matrix target;
    vector targetpos, offset, heading, up;
    bool32 positioned, aligned, animsReady, hostAnimReady;
    madanim *mad, *hostmad;
    bool32 untargetableHost = (host->header.objtype == OBJ_ShipType) && cat_shipIsUntargetable(host);

    nearRadiusSq = ship->staticinfo->common.staticCollInfo.avoidcollspheresize;
    nearRadiusSq = nearRadiusSq * nearRadiusSq;
    collsphere = ship->staticinfo->common.staticCollInfo.collspheresize;

    cat_linkComputeTargetCoordSys(ship, linkpoint, host, hostpoint, &target);
    matMultiplyMatByVec(&targetpos, &host->rotatable.rotinfo.coordsys, &hostpoint->position);
    vecAddTo(targetpos, host->obj.posinfo.position);
    matMultiplyMatByVec(&offset, &ship->rotatable.rotinfo.coordsys, &linkpoint->position);
    vecSubFrom(targetpos, offset);

    if (untargetableHost)
    {
        tolerance = ship->staticinfo->common.staticCollInfo.collspheresize * 0.25f;
        alignCos = 0.985f;
    }
    else
    {
        tolerance = 5.0f;
        alignCos = 0.996f;
    }
    positioned = !((tolerance <= ABS(ship->obj.posinfo.position.x - targetpos.x)) ||
                   (tolerance <= ABS(ship->obj.posinfo.position.y - targetpos.y)) ||
                   (tolerance <= ABS(ship->obj.posinfo.position.z - targetpos.z)));

    aligned = cat_matCoordSysAligned(&ship->rotatable.rotinfo.coordsys, &target, alignCos);
    distSq = (targetpos.x - ship->obj.posinfo.position.x) * (targetpos.x - ship->obj.posinfo.position.x) +
             (targetpos.y - ship->obj.posinfo.position.y) * (targetpos.y - ship->obj.posinfo.position.y) +
             (targetpos.z - ship->obj.posinfo.position.z) * (targetpos.z - ship->obj.posinfo.position.z);

    // CAT: the exe's madanim +0x08 is the mode madAnimationStart 0x004b6b90 stores (its third argument, stage 3);
    //      this tree keeps it in madanim.flags
    mad = ship->madBindings;
    hostmad = (host->header.objtype == OBJ_ShipType) ? host->madBindings : NULL;
    animsReady = TRUE;
    hostAnimReady = TRUE;
    if ((mad != NULL) && (mad->nCurrentAnim != -1) && (mad->flags == 0) && ((nearRadiusSq <= distSq) || !mad->bPaused))
    {
        animsReady = FALSE;
    }
    if ((hostmad != NULL) && (hostmad->nCurrentAnim != -1) && (hostmad->flags == 0))
    {
        hostAnimReady = (distSq < nearRadiusSq) && hostmad->bPaused;
    }
    animsReady = animsReady && hostAnimReady;

    if (aligned && positioned && animsReady)
    {
        ship->rotatable.rotinfo.coordsys = target;
        ship->obj.posinfo.velocity = host->obj.posinfo.velocity;
        cat_univSetShipPosition(ship, &targetpos);
        return TRUE;
    }

    if (catDockSelfTestTrace && (ship->playerowner == universe.curPlayerPtr) && ((universe.univUpdateCounter % 32) == 0))
    {
        dbgMessagef("docktest: link tick=%u %s#%u dist=%.1f positioned=%d aligned=%d anims=%d/%d mad=%d/%d/%d hostmad=%d/%d/%d",
                    (unsigned)universe.univUpdateCounter, ShipTypeToStr(ship->shiptype), (unsigned)ship->shipID.shipNumber, fsqrt(distSq),
                    (int)positioned, (int)aligned, (int)animsReady, (int)hostAnimReady,
                    (mad != NULL) ? (int)mad->nCurrentAnim : -2, (mad != NULL) ? (int)mad->flags : -2, (mad != NULL) ? (int)mad->bPaused : -2,
                    (hostmad != NULL) ? (int)hostmad->nCurrentAnim : -2, (hostmad != NULL) ? (int)hostmad->flags : -2, (hostmad != NULL) ? (int)hostmad->bPaused : -2);
    }
    if (distSq < nearRadiusSq)
    {
        if ((mad != NULL) && (mad->nCurrentAnim != -1) && (mad->flags == 0))
        {
            madAnimationPause(ship, FALSE);
        }
        if ((hostmad != NULL) && (hostmad->nCurrentAnim != -1) && (hostmad->flags == 0))
        {
            madAnimationPause(host, FALSE);
        }
    }

    if (!aligned)
    {
        GetDirectionVectorOfShip(&heading, 2, &target);
        GetDirectionVectorOfShip(&up, 0, &target);
        aitrackHeadingAndUp(ship, &heading, &up, FLYSHIP_HEADINGACCURACY);
        if (positioned)
        {
            return FALSE;
        }
    }
    else
    {
        if (positioned)
        {
            return FALSE;
        }
        if (animsReady)
        {
            goto flyto;
        }
    }

    if (distSq <= collsphere * collsphere)
    {
        aitrackVelocityVectorGuidance((SpaceObjRotImpTargGuidance *)ship, &host->obj.posinfo.velocity);
        return FALSE;
    }
flyto:
    aishipFlyToPointAvoidingObjsFunc(ship, &targetpos, 0, -120.0f, &host->obj.posinfo.velocity);
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : ShipWithinDockRange
    Description : returns TRUE if ship is within docking range of target
    Inputs      : ship, target
    Outputs     :
    Return      : returns TRUE if ship is within docking range of target
----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
    Name        : ClampToWithin
    Description : returns number clamped to within min and max
    Inputs      : number, min, max
    Outputs     :
    Return      : returns number clamped to within min and max
----------------------------------------------------------------------------*/
real32 ClampToWithin(real32 number,real32 min,real32 max)
{
    if (number < min)
    {
        return min;
    }
    else if (number > max)
    {
        return max;
    }
    else
    {
        return number;
    }
}

/*-----------------------------------------------------------------------------
    Name        : dockFindNearestDockPoint
    Description : out of a selection of dock points given by dockpointindices, finds the nearest docking point for
                  ship to dockwith.  Also outputs the intermediate variables in calculation.
    Inputs      : dockpointindices, ship, dockwith
    Outputs     : outconepositionInWorldCoordSys, outconeToShip, outconeToShipMagSqr (only valid if docking point was found)
    Return      : index of nearest dock point, or -1 if not found
----------------------------------------------------------------------------*/
void dockPutShipInside(Ship *ship,Ship *dockwith)
{
    ShipsInsideMe *shipsInsideMe = dockwith->shipsInsideMe;
    InsideShip *insideShip;
    Node *node;
    ShipClass shipclass;

    // CAT: dockPutShipInside 0x004652c0: a ship already inside is not added twice; micro ships count as fighters and
    //      resource ships as corvettes; no HW1 SalCap target hand-over (HW1 salvage, gone)
    for (node = shipsInsideMe->insideList.head; node != NULL; node = node->next)
    {
        if (((InsideShip *)listGetStructOfNode(node))->ship == ship)
        {
            return;
        }
    }

    insideShip = memAlloc(sizeof(InsideShip),"InsideShip",0);
    insideShip->ship = ship;
    listAddNode(&shipsInsideMe->insideList,&insideShip->node,insideShip);
    lmUpdateShipsInside();
    shipclass = ship->staticinfo->shipclass;
    if ((shipclass == CLASS_Fighter) || (shipclass == CLASS_Micro))
        dockwith->shipsInsideMe->FightersInsideme++;
    else if ((shipclass == CLASS_Corvette) || (shipclass == CLASS_Resource))
        dockwith->shipsInsideMe->CorvettesInsideme++;
}

void dockRemoveShipFromInside(Ship *ship,Ship *dockwith)
{
    Node *node = dockwith->shipsInsideMe->insideList.head;
    InsideShip *insideShip;

    while (node != NULL)
    {
        insideShip = (InsideShip *)listGetStructOfNode(node);

        if (insideShip->ship == ship)
        {
            listDeleteNode(node);
            lmUpdateShipsInside();
            // CAT: dockRemoveShipFromInside 0x004632e0 counts as dockPutShipInside does: micro ships as fighters,
            //      resource ships as corvettes [audit: M2-R4 combat changed only the increment]
            if ((ship->staticinfo->shipclass==CLASS_Fighter) || (ship->staticinfo->shipclass==CLASS_Micro))
                dockwith->shipsInsideMe->FightersInsideme--;
            else if ((ship->staticinfo->shipclass==CLASS_Corvette) || (ship->staticinfo->shipclass==CLASS_Resource))
                dockwith->shipsInsideMe->CorvettesInsideme--;

            return;
        }

        node = node->next;
    }
    //dbgAssertOrIgnore(FALSE);
    dbgMessagef("Warning: ship %d not inside ship %d but expected",ship->shipID.shipNumber,dockwith->shipID.shipNumber);
}

/*=============================================================================
    CAT (M2-R5 dock): the Cataclysm dock system. HW1's per-ship-type dock paths (ShipDocksAtCarrierMother,
    ShipDocksAtASF, ShipDocksAtResourceCollector, ShipDocksAtJunkYardHQ, the research ships, the DDDF drones, the
    StaticDockInit / launch-point tables, dockNeedRotationOveride, the busyness bookkeeping, the slave trade: 95 of
    Dock.c's 123 functions) have no counterpart in cataclysm.exe and are gone. What replaces them:
      - a dock PURPOSE 0..10 (Dock.h) in place of the DOCK_* bitmask;
      - cat_dockGetDockClass 0x00463d70: purpose + ship class/type/flags -> the dock point type (DPT_*) used on the host;
      - ThisShipCanDockWith 0x00464290 with the exe's fourth argument (test the per-type busy counts);
      - one cone-based state machine for every ship pair, cat_dockShipDocksAtShip 0x00465860, with
        cat_dockFindNearestDockPoint 0x00464660, cat_dockFindLaunchPoint 0x004649b0, cat_dockFlyOutOfCone 0x004650d0,
        cat_dockFlyInFrontOfHost 0x00465630, cat_dockCanDockWithBusyHost 0x00463a70 and the M2-R4 cone helpers above;
      - the REWRITTEN dockReserveDockPoint 0x00464610, RemoveShipFromDocking 0x00465360, RemoveShipFromLaunching
        0x00465590, and clDock 0x00466c70, processDockToDo 0x00466950, cat_dockFinishedDocking 0x004667f0,
        dockPutShipOutside 0x00463350, InitializeDockVars 0x00463c40, dockChangeSingleShipToDock 0x004644c0,
        FindNearestShipToDockAt 0x00465750 at the exe's bodies.
    Constants read from the exe's .rdata: 0x0072a530 4.0 and 0x0072a7f0 6.0 (dock-time bonuses), 0x0072a7ec 1.5 (the
    slipgate transit), 0x0072a464 100.0 (fly-in-front threshold), 0x00879298 2.0 (the mothership's front factor),
    0x0072a7d8 pi (a double), 0x0072a7cc / 0x0072a7d0 +-0.1221730 rad (7 degrees).
=============================================================================*/

#define CAT_DOCK_TIME_BONUS_RESOURCES   4.0f        // 0x0072a530
#define CAT_DOCK_TIME_BONUS_DAMAGED     6.0f        // 0x0072a7f0
#define CAT_DOCK_SLIPGATE_TRANSIT_TIME  1.5f        // 0x0072a7ec
#define CAT_DOCK_IN_FRONT_DISTANCE      100.0f      // 0x0072a464
#define CAT_DOCK_MOTHERSHIP_FRONT_SCALE 2.0f        // 0x00879298
#define CAT_DOCK_SUPPORT_ROT_X          3.1415927f  // 0x0072a7d8 (double pi in the exe)
#define CAT_DOCK_SUPPORT_ROT_Z          0.12217305f // 0x0072a7cc (7 degrees)
#define CAT_DOCK_LATCH_RADIUS           4.0f        // cat_dockShipDocksAtShip state 5
#define CAT_DOCK_IN_FRONT_LIMITVEL      (-80.0f)
#define CAT_DOCK_PURPOSE_RESOURCE_DEPOSIT_SOUND 0x804
#define CAT_DOCK_LATCHED_SOUND          0x807

// the dock point (static) a ship holds on its host
#define catDockPointOf(ship, dockwith)  (&(dockwith)->staticinfo->dockStaticInfo->dockstaticpoints[(ship)->dockvars.catDockPointIndex])
// the host is a slipgate (dSlipgateSmall / dSlipgateMedium / dSlipgateLarge)
#define catDockHostIsSlipgate(host)     ((host)->shiptype >= dSlipgateSmall && (host)->shiptype <= dSlipgateLarge)
// the exe's "pod" set for the hive-swarm reset
#define catDockHostIsHive(host)         ((host)->shiptype == sHiveFrigate || (host)->shiptype == bHiveFrigate)

/*-----------------------------------------------------------------------------
    Name        : cat_dockFreePoint
    Description : CAT: frees the dock point ship holds on dockwith (the inline of RemoveShipFromDocking 0x00465360,
                  RemoveShipFromLaunching, LaunchCleanup, processLaunchShipToDo and cat_dockShipDocksAtShip): the
                  point's busy word and the host's count for the point type
    Inputs      : ship, dockwith
----------------------------------------------------------------------------*/
static void cat_dockFreePoint(Ship *ship,Ship *dockwith)
{
    sdword index = ship->dockvars.catDockPointIndex;

    if (index != -1)
    {
        DockStaticPoint *point = &dockwith->staticinfo->dockStaticInfo->dockstaticpoints[index];

        dockwith->dockInfo->thisDockBusy[index] = 0;
        dockwith->dockInfo->catNumBusyOfType[point->type]--;
        ship->dockvars.catDockPointIndex = -1;
    }
}

/*-----------------------------------------------------------------------------
    Name        : dockReserveDockPoint
    Description : reserves dock point dockpointindex of dockwith for ship (cataclysm.exe 0x00464610, REWRITTEN:
                  the index goes into the ship's dock vars, the host's busy word and per-type count go up; HW1's
                  door animations and reserveddocking are gone)
    Inputs      : ship, dockwith, dockpointindex (the exe reads it as a signed byte)
----------------------------------------------------------------------------*/
void dockReserveDockPoint(Ship *ship,Ship *dockwith,sdword dockpointindex)
{
    sdword index = (sbyte)dockpointindex;
    DockStaticPoint *point = &dockwith->staticinfo->dockStaticInfo->dockstaticpoints[index];

    ship->dockvars.catDockPointIndex = index;
    dockwith->dockInfo->thisDockBusy[index] = 1;
    dockwith->dockInfo->catNumBusyOfType[point->type]++;
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockInitShipDockInfo
    Description : CAT: allocates and clears a ship's dock info (cataclysm.exe 0x00463bb0, called by univCreateShip
                  0x00542320): numDockPoints x 4 + 0x24 bytes, or NULL for a ship without dock points
    Inputs      : ship
----------------------------------------------------------------------------*/
void cat_dockInitShipDockInfo(Ship *ship)
{
    DockStaticInfo *dockstaticinfo = ship->staticinfo->dockStaticInfo;

    if (dockstaticinfo == NULL)
    {
        ship->dockInfo = NULL;
    }
    else
    {
        sdword size = sizeofDockInfo(dockstaticinfo->numDockPoints);

        ship->dockInfo = memAlloc(size,"dockInfo",NonVolatile);
        memset(ship->dockInfo,0,size);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockGetDockClass
    Description : CAT: the dock point type (DPT_*) a ship of the given class, type and flags uses on dockwith for a
                  dock purpose (cataclysm.exe 0x00463d70), or -1 when it cannot dock there for that purpose
    Inputs      : shipclass, shiptype, externalBuild (the ship static's), typeFlags (the static's catTypeFlags), purpose,
                  dockwith
    Return      : the DPT_* index, -1 none
----------------------------------------------------------------------------*/
sdword cat_dockGetDockClass(ShipClass shipclass,ShipType shiptype,bool32 externalBuild,udword typeFlags,DockType purpose,Ship *dockwith)
{
    ShipStaticInfo *hoststatic = dockwith->staticinfo;
    DockStaticInfo *dockstaticinfo = hoststatic->dockStaticInfo;

    if (dockstaticinfo == NULL)
    {
        return -1;
    }
    // Mission 16: a Worker boards the infected Ecclesiam
    if (singlePlayerGame && (singlePlayerGameInfo.currentMission == 16) && (shiptype == sWorker) &&
        (dockwith->shiptype == dEcclesiamInfected))
    {
        return DPT_CleesanBoarding;
    }
    switch (purpose)
    {
        case CAT_DOCK_SUPPORT:
        case CAT_DOCK_SUPPORT_INSTANT:
            switch (shipclass)
            {
                case CLASS_Mothership:
                case CLASS_HeavyCruiser:
                case CLASS_Carrier:
                case CLASS_Destroyer:
                    return DPT_Large_In;
                case CLASS_Frigate:
                    return DPT_Med_In;
                case CLASS_Corvette:
                    return (dockstaticinfo->firstOfType[DPT_Small_In] == 0xFF) ? DPT_Corvette_Latch : DPT_Small_In;
                case CLASS_Fighter:
                    return (dockstaticinfo->firstOfType[DPT_Small_In] == 0xFF) ? DPT_Fighter_Latch : DPT_Small_In;
                case CLASS_Resource:
                    switch (shiptype)
                    {
                        case sWorker:
                            if ((dockwith->shiptype == dEcclesiamInfected) && singlePlayerGame &&
                                (singlePlayerGameInfo.currentMission == 16))
                            {
                                return DPT_Tow;
                            }
                            return DPT_Med_In;
                        case bWorker:
                            return DPT_Med_In;
                        case tResourceCollector:
                            return DPT_Resourcer_RU_Transfer;
                        default:
                            return DPT_Large_In;
                    }
                case CLASS_NonCombat:
                    if ((shiptype == dCrystal0) || (shiptype == dCrystal1))
                    {
                        return DPT_Crystal_Process;
                    }
                    return -1;
                case CLASS_Micro:
                    if (shiptype == sLeech)
                    {
                        return DPT_Small_In;
                    }
                    break;                                  // the swarmer tail below
                default:
                    return -1;
            }
            break;

        case CAT_DOCK_STAY:
        case CAT_DOCK_STAY_INSTANT:
            if (shipclass != CLASS_Micro)
            {
                return (shipclass == CLASS_NonCombat) ? DPT_Med_In : DPT_Small_In;
            }
            break;                                          // the swarmer tail below

        case CAT_DOCK_LAUNCH:
            if (hoststatic->catTypeFlags & 0x4000)
            {                                               // a slipgate host
                switch (shipclass)
                {
                    case CLASS_Mothership:
                        return DPT_Mothership_Out;
                    case CLASS_Frigate:
                        return DPT_Med_Out;
                    case CLASS_Corvette:
                    case CLASS_Fighter:
                    case CLASS_NonCombat:
                    case CLASS_Component:
                    case CLASS_Micro:
                        return DPT_Small_Out;
                    case CLASS_Resource:
                        if ((shiptype != sProcessor) && (shiptype != bProcessor))
                        {
                            return DPT_Med_Out;
                        }
                        return DPT_Large_Out;
                    default:
                        return DPT_Large_Out;
                }
            }
            if (externalBuild)
            {
                switch (shiptype)
                {
                    case sDreadnought:
                    case sCarrier:
                    case sProcessor:
                    case sDestroyer:
                    case bHeavyCruiser:
                    case bCarrier:
                    case bProcessor:
                        return DPT_Destroyer_EB;
                    case sCarrierSupport:
                    case sMothershipSupport:
                    case bMothershipSupport:
                        return DPT_Support_EB;
                    case sMothershipSpecial:
                    case sMothershipEngineering:
                    case sMothershipArmour:
                    case sMothershipMicro:
                    case sMothershipWeapons:
                    case sMothershipDockingBay:
                    case sMothershipBigGun:
                        return DPT_Module_EB;
                    default:
                        break;
                }
            }
            switch (shipclass)
            {
                case CLASS_Mothership:
                case CLASS_HeavyCruiser:
                case CLASS_Carrier:
                case CLASS_Destroyer:
                    return DPT_Large_Out;
                case CLASS_Frigate:
                case CLASS_NonCombat:
                    return DPT_Med_Out;
                case CLASS_Corvette:
                case CLASS_Fighter:
                    return DPT_Small_Out;
                case CLASS_Resource:
                    if ((shiptype == sProcessor) || (shiptype == bProcessor))
                    {
                        return DPT_Large_Out;
                    }
                    return DPT_Med_Out;
                case CLASS_Micro:
                    if ((shiptype == sSwarmer) || (shiptype == bSwarmer))
                    {
                        return DPT_Swarmer_Out;
                    }
                    return DPT_Small_Out;
                default:
                    // CAT-FIX: the exe's switch has no default and runs on into the purpose-3 code for a monster or
                    //      component class; no such ship launches (clLaunchShip's slot refuses), so -1 here
                    return -1;
            }

        case CAT_DOCK_DEPOSIT_RESOURCES:
            if (shiptype == tResourceCollector)
            {
                return DPT_Resourcer_RU_Transfer;
            }
            if (typeFlags & 0x1)
            {
                return DPT_Worker_RU_Transfer;
            }
            return (typeFlags & 0x200) ? DPT_Leech_RU_Transfer : -1;

        case CAT_DOCK_LEECH:
            return DPT_Leech;

        case CAT_DOCK_TOW:
            return DPT_Tow;

        case CAT_DOCK_RETIRE:
            switch (shiptype)
            {
                case dBigGunUnit:
                    if ((dockwith->shiptype != sMothership) && (dockwith->shiptype != bMothership))
                    {
                        return DPT_SalvageProcess;
                    }
                    return DPT_Destroyer_EB;
                case dMessagePod:
                    return DPT_Med_In;
                case dCrystal0:
                case dCrystal1:
                    return DPT_Crystal_Process;
                default:
                    if (shipclass > CLASS_HeavyCruiser)
                    {
                        if (shipclass < CLASS_Frigate)
                        {
                            return DPT_Large_In;
                        }
                        if ((shipclass == CLASS_Resource) && ((shiptype == sProcessor) || (shiptype == bProcessor)))
                        {
                            return DPT_Large_In;
                        }
                    }
                    return DPT_Med_In;
            }

        case CAT_DOCK_SLIPGATE:
            if (hoststatic->catTypeFlags & 0x4000)
            {
                switch (shipclass)
                {
                    case CLASS_Mothership:
                        return DPT_Mothership_In;
                    case CLASS_Frigate:
                        return DPT_Med_In;
                    case CLASS_Corvette:
                    case CLASS_Fighter:
                    case CLASS_NonCombat:
                    case CLASS_Component:
                    case CLASS_Micro:
                        // [audit: M2-R5 layout-dock] the exe's purpose-10 table sends these five classes to "return 0"
                        //      (0x463e6d), not to the swarmer tail: a swarmer transits a slipgate through DPT_Small_In
                        return DPT_Small_In;
                    case CLASS_Resource:
                        if ((shiptype == sProcessor) || (shiptype == bProcessor))
                        {
                            return DPT_Large_In;
                        }
                        return DPT_Med_In;
                    default:
                        return DPT_Large_In;
                }
            }
            return -1;

        default:
            return -1;
    }
    // the tail: swarmers use their own points, everything else the small bay
    if ((shiptype == sSwarmer) || (shiptype == bSwarmer))
    {
        return DPT_Swarmer_In;
    }
    return DPT_Small_In;
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockCanDockWithBusyHost
    Description : CAT: whether ship may dock for purpose at a host whose bays are closed (attribute
                  CAT_ATTRIBUTES_DockBaysClosed), i.e. latch outside it (cataclysm.exe 0x00463a70)
    Inputs      : ship, purpose
    Return      : TRUE for the Mission 16 Worker at the infected Ecclesiam, a ship already holding a latch point,
                  resourcers and crystals docking for support, and the latch purposes 3..5
----------------------------------------------------------------------------*/
bool32 cat_dockCanDockWithBusyHost(Ship *ship,DockType purpose)
{
    Ship *dockship = ship->dockvars.dockship;

    if (singlePlayerGame && (singlePlayerGameInfo.currentMission == 16) && (ship->shiptype == sWorker) &&
        (dockship != NULL) && (dockship->shiptype == dEcclesiamInfected))
    {
        return TRUE;
    }
    if (dockship != NULL)
    {
        // CAT-FIX: the exe reads the point of catDockPointIndex without testing it for -1 (it holds one whenever a
        //      state after 0 asks); the test is kept explicit here
        if (ship->dockvars.catDockPointIndex != -1)
        {
            DockPointType type = catDockPointOf(ship,dockship)->type;
            if ((type == DPT_Fighter_Latch) || (type == DPT_Corvette_Latch))
            {
                return TRUE;
            }
        }
    }
    if (purpose == CAT_DOCK_SUPPORT)
    {
        if ((ship->shiptype == tResourceCollector) || (ship->shiptype == dCrystal0) || (ship->shiptype == dCrystal1))
        {
            return TRUE;
        }
    }
    else if ((purpose >= CAT_DOCK_DEPOSIT_RESOURCES) && (purpose <= CAT_DOCK_TOW))
    {
        return TRUE;
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockHasFreeSlotForShip
    Description : CAT: whether host has a free launch point for a ship of the given class and type (cataclysm.exe
                  0x00464910; the construction manager's cmCanBuildShipType / cmDeterministicBuildProcess /
                  cat_cmFactoryHasFreeSlot ask, stage 3 R6-4). The sMothership without its docking bay module (and
                  the Ghostship) use the engineering points instead
    Inputs      : host, shipclass, shiptype, externalBuild, typeFlags
    Return      : TRUE when the point type has a free point
----------------------------------------------------------------------------*/
bool32 cat_dockHasFreeSlotForShip(Ship *host,ShipClass shipclass,ShipType shiptype,bool32 externalBuild,udword typeFlags)
{
    DockStaticInfo *dockstaticinfo = host->staticinfo->dockStaticInfo;
    DockInfo *dockInfo = host->dockInfo;
    sdword dockclass = cat_dockGetDockClass(shipclass,shiptype,externalBuild,typeFlags,CAT_DOCK_LAUNCH,host);

    if (((host->shiptype == sMothership) && !cat_playerHasCompletedShip(sMothershipDockingBay,host->playerowner)) ||
        (host->shiptype == trGhostship))
    {
        if (dockclass == DPT_Small_Out)
        {
            dockclass = DPT_Small_Out_Engineering;
        }
        else if (dockclass == DPT_Med_Out)
        {
            dockclass = DPT_Med_Out_Engineering;
            if (dockInfo->catNumBusyOfType[DPT_Med_In] != 0)
            {
                return FALSE;
            }
        }
    }
    // CAT-FIX: the exe indexes the counts with a -1 class for a type that cannot launch; FALSE here
    if (dockclass < 0)
    {
        return FALSE;
    }
    return (dockInfo->catNumBusyOfType[dockclass] != dockstaticinfo->numOfType[dockclass]);
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockFindNearestDockPoint
    Description : CAT: picks the free dock point of ship's class on dockwith nearest to ship and reserves it
                  (cataclysm.exe 0x00464660): the sMothership without its docking bay (and the Ghostship) fall back
                  to the engineering points; an sCarrier uses its medium bay for both medium types; the clamped
                  distance to the point goes into dockvars.catDockDistance
    Inputs      : ship, dockwith, purpose
    Return      : TRUE when a point is held (also when the sCarrier ship already had one)
----------------------------------------------------------------------------*/
bool32 cat_dockFindNearestDockPoint(Ship *ship,Ship *dockwith,DockType purpose)
{
    ShipStaticInfo *shipstatic = ship->staticinfo;
    DockStaticInfo *dockstaticinfo = dockwith->staticinfo->dockStaticInfo;
    DockInfo *dockInfo = dockwith->dockInfo;
    sdword dockclass;
    sdword index, best = -1;
    real32 bestdistsqr = REALlyBig;
    vector pointpos, diff;
    DockStaticPoint *point;

    dockclass = cat_dockGetDockClass(shipstatic->shipclass,shipstatic->shiptype,FALSE,shipstatic->catTypeFlags,purpose,dockwith);
    // CAT-FIX: the exe indexes with the class before testing it; ThisShipCanDockWith refused a -1 class before this
    //      is reached, so the test only guards a direct caller
    if (dockclass < 0)
    {
        return FALSE;
    }
    if (((dockwith->shiptype == sMothership) || (dockwith->shiptype == trGhostship)) &&
        (!cat_playerHasCompletedShip(sMothershipDockingBay,dockwith->playerowner) ||
         (dockInfo->catNumBusyOfType[dockclass] == dockstaticinfo->numOfType[dockclass])))
    {
        switch (dockclass)
        {
            case DPT_Small_In:
                dockclass = DPT_Small_In_Engineering;
                break;
            case DPT_Small_Out:
                dockclass = DPT_Small_Out_Engineering;
                break;
            case DPT_Med_In:
                if (dockInfo->catNumBusyOfType[DPT_Med_Out] != 0)
                {
                    return FALSE;
                }
                dockclass = DPT_Med_In_Engineering;
                break;
            case DPT_Med_Out:
                if (dockInfo->catNumBusyOfType[DPT_Med_In] != 0)
                {
                    return FALSE;
                }
                dockclass = DPT_Med_Out_Engineering;
                break;
            default:
                break;
        }
    }
    if ((dockwith->shiptype == sCarrier) && ((dockclass == DPT_Med_Out) || (dockclass == DPT_Med_In)))
    {
        dockclass = DPT_Med_In;
        if (ship->dockvars.catDockPointIndex != -1)
        {
            return TRUE;
        }
    }
    if (dockInfo->catNumBusyOfType[dockclass] == dockstaticinfo->numOfType[dockclass])
    {
        return FALSE;
    }
    for (index = dockstaticinfo->firstOfType[dockclass]; index < dockstaticinfo->numDockPoints; index++)
    {
        point = &dockstaticinfo->dockstaticpoints[index];
        if (point->type != (DockPointType)dockclass)
        {
            break;
        }
        if (dockInfo->thisDockBusy[index] == 0)
        {
            real32 distsqr;

            matMultiplyMatByVec(&pointpos,&dockwith->rotatable.rotinfo.coordsys,&point->position);
            vecAddTo(pointpos,dockwith->obj.posinfo.position);
            vecSub(diff,ship->obj.posinfo.position,pointpos);
            distsqr = vecMagnitudeSquared(diff);
            if (distsqr < bestdistsqr)
            {
                best = index;
                bestdistsqr = distsqr;
            }
        }
    }
    if (best == -1)
    {
        return FALSE;
    }
    point = &dockstaticinfo->dockstaticpoints[best];
    ship->dockvars.catDockDistance = ClampToWithin(fsqrt(bestdistsqr),point->mindist,point->maxdist);
    dockReserveDockPoint(ship,dockwith,best);
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockFindLaunchPoint
    Description : CAT: reserves the first free launch point of ship's class on dockwith (cataclysm.exe 0x004649b0),
                  with the same engineering and sCarrier fall-backs as the dock point; the point's mindist goes into
                  dockvars.catDockDistance
    Inputs      : ship, dockwith
    Return      : TRUE when a point is held
----------------------------------------------------------------------------*/
bool32 cat_dockFindLaunchPoint(Ship *ship,Ship *dockwith)
{
    ShipStaticInfo *shipstatic = ship->staticinfo;
    DockStaticInfo *dockstaticinfo = dockwith->staticinfo->dockStaticInfo;
    DockInfo *dockInfo = dockwith->dockInfo;
    sdword dockclass;
    sdword index;
    DockStaticPoint *point;

    dockclass = cat_dockGetDockClass(shipstatic->shipclass,shipstatic->shiptype,shipstatic->externalBuild,shipstatic->catTypeFlags,CAT_DOCK_LAUNCH,dockwith);
    if (dockclass < 0)
    {
        return FALSE;                                       // CAT-FIX: the exe indexes with -1 (a class that cannot launch)
    }
    if (((dockwith->shiptype == sMothership) && !cat_playerHasCompletedShip(sMothershipDockingBay,dockwith->playerowner)) ||
        (dockwith->shiptype == trGhostship))
    {
        switch (dockclass)
        {
            case DPT_Small_In:  dockclass = DPT_Small_In_Engineering;  break;
            case DPT_Small_Out: dockclass = DPT_Small_Out_Engineering; break;
            case DPT_Med_In:    dockclass = DPT_Med_In_Engineering;    break;
            case DPT_Med_Out:   dockclass = DPT_Med_Out_Engineering;   break;
            default: break;
        }
    }
    if ((dockwith->shiptype == sCarrier) && ((dockclass == DPT_Med_Out) || (dockclass == DPT_Med_In)))
    {
        dockclass = DPT_Med_In;
        if (ship->dockvars.catDockPointIndex != -1)
        {
            return TRUE;
        }
    }
    for (index = (sbyte)dockstaticinfo->firstOfType[dockclass]; (index >= 0) && (index < dockstaticinfo->numDockPoints); index++)
    {
        point = &dockstaticinfo->dockstaticpoints[index];
        if (point->type != (DockPointType)dockclass)
        {
            return FALSE;
        }
        if (dockInfo->thisDockBusy[index] == 0)
        {
            ship->dockvars.catDockDistance = point->mindist;
            dockReserveDockPoint(ship,dockwith,index);
            return TRUE;
        }
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockFlyOutOfCone
    Description : CAT: flies ship out along the axis of the dock point it holds on dockwith (cataclysm.exe
                  0x004650d0): the ship is snapped onto the cone axis, then flies to flyawaydist x 4 along the cone
                  normal until it is flyawaydist from the origin; an sCarrier's door must have opened first
    Inputs      : ship, dockwith
    Return      : TRUE once the ship is out (SPECIAL_KasCheckDoneLaunching is cleared then)
----------------------------------------------------------------------------*/
bool32 cat_dockFlyOutOfCone(Ship *ship,Ship *dockwith)
{
    DockStaticPoint *point;
    vector normal, origin, target, diff, onaxis;
    real32 flyawaydist, t;

    // CAT-FIX: the exe reads the point through dockwith before its NULL test; same order of effects without the read
    if (dockwith != NULL)
    {
        point = catDockPointOf(ship,dockwith);
        flyawaydist = point->flyawaydist;
        matMultiplyMatByVec(&normal,&dockwith->rotatable.rotinfo.coordsys,&point->conenormal);
        matMultiplyMatByVec(&origin,&dockwith->rotatable.rotinfo.coordsys,&point->position);
        vecAddTo(origin,dockwith->obj.posinfo.position);
        vecScalarMultiply(target,normal,flyawaydist * CAT_DOCK_TIME_BONUS_RESOURCES);   // x 4.0 (0x0072a530)
        vecAddTo(target,origin);
        vecSub(diff,ship->obj.posinfo.position,origin);
        t = vecDotProduct(diff,normal) / vecDotProduct(normal,normal);
        vecScalarMultiply(onaxis,normal,t);
        vecAddTo(onaxis,origin);
        cat_univSetShipPosition(ship,&onaxis);
        if ((dockwith->shiptype == sCarrier) && !cat_carrierDoorAnimUpdate(dockwith,point->type))
        {
            return FALSE;
        }
        if (vecMagnitudeSquared(diff) < flyawaydist * flyawaydist)
        {
            aishipFlyToPointAvoidingObjsFunc(ship,&target,0,0.0f,&dockwith->obj.posinfo.velocity);
            return FALSE;
        }
    }
    bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_KasCheckDoneLaunching);
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockFlyInFrontOfHost
    Description : CAT: flies ship to a point one collision sphere (two for the sMothership and the Ghostship) in
                  front of dockwith (cataclysm.exe 0x00465630; dock state 1)
    Inputs      : ship, dockwith
    Return      : TRUE within 100 units of it
----------------------------------------------------------------------------*/
bool32 cat_dockFlyInFrontOfHost(Ship *ship,Ship *dockwith)
{
    vector unit = { 1.0f, 0.0f, 0.0f };
    vector point, diff;
    real32 scale = dockwith->staticinfo->common.staticCollInfo.collspheresize;

    matMultiplyMatByVec(&point,&dockwith->rotatable.rotinfo.coordsys,&unit);
    if ((dockwith->shiptype == sMothership) || (dockwith->shiptype == trGhostship))
    {
        scale *= CAT_DOCK_MOTHERSHIP_FRONT_SCALE;
    }
    vecMultiplyByScalar(point,scale);
    vecAddTo(point,dockwith->obj.posinfo.position);
    vecSub(diff,point,ship->obj.posinfo.position);
    if (fsqrt(vecMagnitudeSquared(diff)) >= CAT_DOCK_IN_FRONT_DISTANCE)
    {
        // [audit: M2-R5 layout-dock] the exe passes 6 = AISHIP_PointInDirectionFlying | AISHIP_FastAsPossible (AIShip.h: CarTurn is 8)
        aishipFlyToPointAvoidingObjsFunc(ship,&point,AISHIP_PointInDirectionFlying | AISHIP_FastAsPossible,CAT_DOCK_IN_FRONT_LIMITVEL,&dockwith->obj.posinfo.velocity);
        return FALSE;
    }
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockShipIsInsideHost
    Description : CAT: whether ship is in host's inside list (cataclysm.exe 0x00468f80; kasfGetDockHost asks)
    Inputs      : ship, host
----------------------------------------------------------------------------*/
bool32 cat_dockShipIsInsideHost(Ship *ship,Ship *host)
{
    Node *node;

    if (host->shipsInsideMe != NULL)
    {
        for (node = host->shipsInsideMe->insideList.head; node != NULL; node = node->next)
        {
            if (((InsideShip *)listGetStructOfNode(node))->ship == ship)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : ThisShipCanDockWith
    Description : whether ship can dock with dockwith for purpose (cataclysm.exe 0x00464290, with the exe's fourth
                  argument: checkCounts also requires a free point of the ship's dock class)
    Inputs      : ship, dockwith, purpose, checkCounts
    Return      : TRUE if it can
----------------------------------------------------------------------------*/
bool32 ThisShipCanDockWith(Ship *ship,Ship *dockwith,DockType purpose,bool32 checkCounts)
{
    ShipStaticInfo *shipstatic = ship->staticinfo;
    DockStaticInfo *dockstaticinfo = dockwith->staticinfo->dockStaticInfo;
    sdword dockclass;
    bool32 untargetableTested = FALSE;

    dockclass = cat_dockGetDockClass(shipstatic->shipclass,shipstatic->shiptype,FALSE,shipstatic->catTypeFlags,purpose,dockwith);
    if ((dockwith->shiptype == dRKO) && (ship->shiptype != sMimic) && (ship->shiptype != sMCV) && (ship->shiptype != bCruiseMissile))
    {
        return FALSE;
    }
    if ((dockwith->shiptype == dBeastMothership) && (ship->playerowner->playerIndex == 0))
    {
        return FALSE;
    }
    if (dockstaticinfo == NULL)
    {
        return FALSE;
    }
    if (dockclass == -1)
    {
        return FALSE;
    }
    if (dockstaticinfo->firstOfType[dockclass] == 0xFF)
    {
        return FALSE;
    }
    if (ship->shiptype == dockwith->shiptype)
    {
        return FALSE;
    }
    if ((dockwith->header.flags & SOF_Dead) || (ship->header.flags & SOF_Dead))
    {
        return FALSE;
    }
    if ((dockwith->obj.attributes & CAT_ATTRIBUTES_DockBaysClosed) && !cat_dockCanDockWithBusyHost(ship,purpose))
    {
        return FALSE;
    }
    if ((!singlePlayerGame || ((ship->playerowner->playerIndex != 1) && (ship->shipOrDerelict.catDisableReason[1] == 0))) &&
        (purpose != CAT_DOCK_SLIPGATE))
    {
        if (cat_shipIsUntargetable(dockwith))
        {
            return FALSE;
        }
        if (singlePlayerGame)
        {
            if (dockwith->shipOrDerelict.catDisabled)
            {
                return FALSE;
            }
            untargetableTested = TRUE;
        }
    }
    if (singlePlayerGame && ((singlePlayerGameInfo.currentMission == 7) || (singlePlayerGameInfo.currentMission == 15) ||
                             (singlePlayerGameInfo.currentMission == 6)) && (ship->playerowner->playerIndex == 0))
    {
        if (ship->shipOrDerelict.specialFlags2 & SPECIAL_2_CatExempt20)
        {
            goto allyTest;
        }
        if (!untargetableTested && cat_shipIsUntargetable(dockwith))
        {
            return FALSE;
        }
        (void)untargetableTested;
    }
    if (!(ship->shipOrDerelict.specialFlags2 & SPECIAL_2_CatExempt20) && (dockwith->shipOrDerelict.specialFlags2 & SPECIAL_2_CatExempt20))
    {
        return FALSE;
    }
allyTest:
    if (!allianceIsShipAlly(ship,dockwith->playerowner) && !(ship->obj.attributes & CAT_ATTRIBUTES_DockWithEnemies) &&
        !(dockwith->shipOrDerelict.specialFlags2 & SPECIAL_2_CatSlipgate))
    {
        if (!cat_shipIsCrystal(ship))
        {
            return FALSE;
        }
        if ((ship->catSalvageDepot == NULL) && (cat_salvageAttachedIterFirst(ship) == NULL))
        {
            return FALSE;
        }
    }
    if (!(dockwith->shipOrDerelict.specialFlags2 & SPECIAL_2_CatBeingBuilt) &&
        (((dockclass != DPT_Fighter_Latch) && (dockclass != DPT_Corvette_Latch)) ||
         ((shipstatic->dockStaticInfo != NULL) && (shipstatic->dockStaticInfo->centerDockPoint != NULL))) &&
        (!checkCounts || (dockwith->dockInfo->catNumBusyOfType[dockclass] < dockstaticinfo->numOfType[dockclass])))
    {
        return TRUE;
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : FindNearestShipToDockAt
    Description : finds the nearest ship that ship can dock with for purpose (cataclysm.exe 0x00465750); for the
                  launch purpose, the ship it is inside
    Inputs      : ship, purpose
    Return      : the host, or NULL
----------------------------------------------------------------------------*/
Ship *FindNearestShipToDockAt(Ship *ship,DockType purpose)
{
    Node *objnode;
    Ship *dockat;

    if (purpose != CAT_DOCK_LAUNCH)
    {
        real32 mindist = (real32)1e20;
        Ship *closestDockat = NULL;
        vector diff;
        real32 dist;

        for (objnode = universe.ShipList.head; objnode != NULL; objnode = objnode->next)
        {
            dockat = (Ship *)listGetStructOfNode(objnode);
            if (ThisShipCanDockWith(ship,dockat,purpose,FALSE))
            {
                vecSub(diff,dockat->obj.posinfo.position,ship->obj.posinfo.position);
                dist = fsqrt(vecMagnitudeSquared(diff));
                if (dist < mindist)
                {
                    mindist = dist;
                    closestDockat = dockat;
                }
            }
        }
        return closestDockat;
    }
    for (objnode = universe.ShipList.head; objnode != NULL; objnode = objnode->next)
    {
        dockat = (Ship *)listGetStructOfNode(objnode);
        if (cat_dockShipIsInsideHost(ship,dockat))
        {
            return dockat;
        }
    }
    dbgMessagef("DOCKING: Ship %08x (%s) has no ship to launch from",(udword)ship->shipID.shipNumber,ShipTypeToStr(ship->shiptype));
    return NULL;
}

/*-----------------------------------------------------------------------------
    Name        : InitializeDockVars
    Description : initialises ship's dock vars for docking with dockship for purpose (cataclysm.exe 0x00463c40, with
                  the exe's third argument): the stay flag, the hive swarm reset, the launch state, the closed guns
                  of the ion array frigates
    Inputs      : ship, dockship, purpose
----------------------------------------------------------------------------*/
void InitializeDockVars(Ship *ship,Ship *dockship,DockType purpose)
{
    if ((purpose == CAT_DOCK_STAY) || (purpose == CAT_DOCK_STAY_INSTANT))
    {
        bitSet(ship->shipOrDerelict.specialFlags,SPECIAL_STAY_TILL_EXPLICITLAUNCH);
        // CAT-FIX: the exe reads dockship->shiptype here without a NULL test (a stay order with no host would fault)
        if ((purpose == CAT_DOCK_STAY_INSTANT) && (dockship != NULL) && catDockHostIsHive(dockship))
        {
            cat_hiveFrigateResetSwarm(dockship);
        }
    }
    ship->dockvars.dockship = dockship;
    if (purpose == CAT_DOCK_LAUNCH)
    {
        bitSet(ship->header.catFlags18,CAT_SHIPFLAG_Launching);
        ship->dockvars.dockstate = CAT_DOCKSTATE_LAUNCH;
        ship->dockvars.catDockPointIndex = -1;
        return;
    }
    if (((ship->shiptype == bIonArrayFrigate) || (ship->shiptype == p1IonArrayFrigate)) &&
        ((ship->madGunStatus == MAD_STATUS_GUNS_OPEN) || (ship->madGunStatus == MAD_STATUS_GUNS_OPENING)))
    {
        madLinkInCloseGunsShip(ship);
    }
    ship->dockvars.dockstate = CAT_DOCKSTATE_APPROACH;
    ship->dockvars.catDockPointIndex = -1;
}

/*-----------------------------------------------------------------------------
    Name        : clDock
    Description : Command to dock (cataclysm.exe 0x00466c70): the selection is filtered by ThisShipCanDockWith for a
                  given host and by each type's CustShipDockCommand slot; a selection already under command gets its
                  order changed in place unless it is harvesting (then a new dock command is made, and the ships
                  resume harvesting after docking for purposes 0, 3 and 10)
    Inputs      : comlayer, selectcom, purpose, dockwith (NULL: the nearest host per ship)
    Return      : the number of ships that are actually docking
----------------------------------------------------------------------------*/
sdword clDock(CommandLayer *comlayer,SelectCommand *selectcom,DockType purpose,ShipPtr dockwith)
{
    CommandToDo *newcommand;
    CommandToDo *alreadycommand;
    SelectCommand *selection;
    sdword numShips = selectcom->numShips;
    sdword i;
    Ship *ship;
    Ship *host;
    bool8 wasHarvesting = FALSE;

    if (numShips == 0)
    {
        return 0;
    }
    if (dockwith != NULL)
    {
        for (i = 0; i < selectcom->numShips; )
        {
            if (!ThisShipCanDockWith(selectcom->ShipPtr[i],dockwith,purpose,FALSE))
            {
                selectcom->numShips--;
                selectcom->ShipPtr[i] = selectcom->ShipPtr[selectcom->numShips];
                continue;
            }
            i++;
        }
        numShips = selectcom->numShips;
        if (numShips == 0)
        {
            return 0;
        }
    }
    tacticsReportMove(comlayer,selectcom);

    selection = memAlloc(sizeofSelectCommand(numShips),"ToDoSelection",0);
    selection->numShips = 0;
    for (i = 0; i < numShips; i++)
    {
        ship = selectcom->ShipPtr[i];
        if ((ship->staticinfo->custshipheader.CustShipDockCommand == NULL) ||
            ship->staticinfo->custshipheader.CustShipDockCommand(ship,(sdword)purpose,dockwith))
        {
            selection->ShipPtr[selection->numShips++] = ship;
        }
    }
    if (selection->numShips == 0)
    {
        memFree(selection);
        return 0;
    }

    alreadycommand = IsSelectionAlreadyDoingSomething(comlayer,selection);
    if (alreadycommand != NULL)
    {
        if (alreadycommand->ordertype.attributes & COMMAND_MASK_PROTECTING)
        {
            ClearProtecting(alreadycommand);
        }
        if (alreadycommand->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
        {
            ClearPassiveAttacking(alreadycommand);
        }
        if (alreadycommand->ordertype.order != COMMAND_COLLECT_RESOURCES)
        {
            FreeLastOrder(alreadycommand);
            alreadycommand->ordertype.order = COMMAND_DOCK;
            alreadycommand->dock.wasHarvesting = FALSE;
            alreadycommand->dock.formationToGoIntoAfterDockingID = 0;
            alreadycommand->dock.dockType = purpose;
            alreadycommand->dock.allNearShip = FALSE;
            alreadycommand->dock.haveFlyedFarEnoughAway = FALSE;
            for (i = 0; i < selection->numShips; i++)
            {
                ship = selection->ShipPtr[i];
                host = (dockwith != NULL) ? dockwith : FindNearestShipToDockAt(ship,purpose);
                InitializeDockVars(ship,host,purpose);              // the exe inlines it
            }
            alreadycommand->dock.allDockingWithSameShip = TRUE;
            for (i = 1; i < selection->numShips; i++)
            {
                if (selection->ShipPtr[i]->dockvars.dockship != selection->ShipPtr[0]->dockvars.dockship)
                {
                    alreadycommand->dock.allDockingWithSameShip = FALSE;
                    break;
                }
            }
            InitShipsForAI(alreadycommand->selection,TRUE);
            PrepareShipsForCommand(alreadycommand,TRUE);
            numShips = selection->numShips;
            memFree(selection);
            return numShips;
        }
        if ((purpose == CAT_DOCK_SUPPORT) || (purpose == CAT_DOCK_DEPOSIT_RESOURCES) || (purpose == CAT_DOCK_SLIPGATE))
        {
            wasHarvesting = TRUE;
        }
    }

    // CAT-TODO(M2-R5 command follow-up): the exe passes the reason code 0x1F (RemoveShipsFromDoingStuff's third argument)
    RemoveShipsFromDoingStuff(comlayer,selection);

    newcommand = memAlloc(sizeof(CommandToDo),"ToDo",0);
    InitCommandToDo(newcommand);
    newcommand->selection = selection;
    newcommand->ordertype.order = COMMAND_DOCK;
    newcommand->dock.wasHarvesting = wasHarvesting;
    newcommand->dock.formationToGoIntoAfterDockingID = 0;
    newcommand->dock.dockType = purpose;
    newcommand->dock.allNearShip = FALSE;
    newcommand->dock.haveFlyedFarEnoughAway = FALSE;
    for (i = 0; i < selection->numShips; i++)
    {
        ship = selection->ShipPtr[i];
        host = (dockwith != NULL) ? dockwith : FindNearestShipToDockAt(ship,purpose);
        InitializeDockVars(ship,host,purpose);                      // the exe inlines it
    }
    newcommand->dock.allDockingWithSameShip = TRUE;
    for (i = 1; i < selection->numShips; i++)
    {
        if (selection->ShipPtr[i]->dockvars.dockship != selection->ShipPtr[0]->dockvars.dockship)
        {
            newcommand->dock.allDockingWithSameShip = FALSE;
            break;
        }
    }
    InitShipsForAI(selection,TRUE);
    PrepareShipsForCommand(newcommand,TRUE);
    listAddNode(&comlayer->todolist,&newcommand->todonode,newcommand);
    return selection->numShips;
}

/*-----------------------------------------------------------------------------
    Name        : dockChangeSingleShipToDock
    Description : turns a one-ship command into a dock command (cataclysm.exe 0x004644c0; processCollectResource's
                  trip home with purpose 3)
    Inputs      : command, ship, dockship, wasHarvesting, purpose
----------------------------------------------------------------------------*/
void dockChangeSingleShipToDock(struct CommandToDo *command,Ship *ship,Ship *dockship,bool32 wasHarvesting,DockType purpose)
{
    if (command->ordertype.attributes & COMMAND_MASK_PROTECTING)
    {
        ClearProtecting(command);
    }
    if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        ClearPassiveAttacking(command);
    }
    command->ordertype.order = COMMAND_DOCK;
    command->dock.wasHarvesting = (bool8)wasHarvesting;
    command->dock.formationToGoIntoAfterDockingID = 0;
    command->dock.dockType = purpose;
    command->dock.allDockingWithSameShip = TRUE;
    command->dock.allNearShip = FALSE;
    command->dock.haveFlyedFarEnoughAway = FALSE;
    InitializeDockVars(ship,dockship,purpose);                      // the exe inlines it
    PrepareShipsForCommand(command,TRUE);
    InitShipsForAI(command->selection,TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : RemoveShipFromDocking
    Description : takes ship out of the dock process (cataclysm.exe 0x00465360, REWRITTEN): frees its dock point,
                  clears the stay flag, SOF_CatDocking and SPECIAL_2_CatNoAvoidance, unclamps it from its host, and
                  closes a Worker's claws
    Inputs      : ship
----------------------------------------------------------------------------*/
void RemoveShipFromDocking(Ship *ship)
{
    if (ship->dockvars.catDockPointIndex != -1)
    {
        cat_dockFreePoint(ship,ship->dockvars.dockship);
    }
    bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_STAY_TILL_EXPLICITLAUNCH);
    bitClear(ship->header.flags,SOF_CatDocking);
    bitClear(ship->shipOrDerelict.specialFlags2,SPECIAL_2_CatNoAvoidance);
    if ((ship->guidance.clampInfo != NULL) && (ship->guidance.clampInfo->host != NULL))
    {
        if ((Ship *)ship->guidance.clampInfo->host != ship->dockvars.dockship)
        {
            dbgMessagef("\nDOCKING: %s unclamped from a ship it is not docking with",ShipTypeToStr(ship->shiptype));
        }
        unClampObj((SpaceObjRotImpTargGuidance *)ship);
    }
    ship->dockvars.dockship = NULL;
    if ((ship->shiptype == sWorker) || (ship->shiptype == bWorker))
    {
        if ((ship->madBindings != NULL) && (ship->madBindings->nCurrentAnim == cat_salvageOpenAnimIndex(ship->shiptype)))
        {
            cat_madAnimationStartNumber(ship,cat_salvageCloseAnimIndex(ship->shiptype));   // CAT-TODO(stage 3): the exe passes a third argument, 2
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : RemoveShipFromLaunching
    Description : takes ship out of the launch process (cataclysm.exe 0x00465590, REWRITTEN): frees its launch
                  point unless it is a component, clears the stay flag, the launching bit, SOF_CatDocking and
                  SPECIAL_2_CatNoAvoidance (no welcome chatter, no mesh animation)
    Inputs      : ship
----------------------------------------------------------------------------*/
void RemoveShipFromLaunching(Ship *ship)
{
    if ((ship->dockvars.catDockPointIndex != -1) && (ship->staticinfo->shipclass != CLASS_Component))
    {
        cat_dockFreePoint(ship,ship->dockvars.dockship);
        ship->dockvars.dockship = NULL;
    }
    bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_STAY_TILL_EXPLICITLAUNCH);
    bitClear(ship->header.catFlags18,CAT_SHIPFLAG_Launching);
    bitClear(ship->header.flags,SOF_CatDocking);
    bitClear(ship->shipOrDerelict.specialFlags2,SPECIAL_2_CatNoAvoidance);
}

/*-----------------------------------------------------------------------------
    Name        : LaunchCleanup
    Description : the launch command's clean-up (cataclysm.exe 0x004679b0: RemoveShipFromLaunching inlined for the
                  one ship of the command)
    Inputs      : launchtodo
----------------------------------------------------------------------------*/
void LaunchCleanup(struct CommandToDo *launchtodo)
{
    if (launchtodo->selection->numShips != 0)
    {
        RemoveShipFromLaunching(launchtodo->selection->ShipPtr[0]);
    }
}

/*-----------------------------------------------------------------------------
    Name        : DockCleanup
    Description : the dock command's clean-up (cataclysm.exe 0x00467970)
    Inputs      : docktodo
----------------------------------------------------------------------------*/
void DockCleanup(struct CommandToDo *docktodo)
{
    sdword i;
    SelectCommand *selection = docktodo->selection;
    sdword numShips = selection->numShips;

    for (i=0;i<numShips;i++)
    {
        RemoveShipFromDocking(selection->ShipPtr[i]);
    }
    madLinkInPostDocking(docktodo,1);
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockDepositShipResources
    Description : CAT: the resource deposit of cat_dockShipDocksAtShip 0x00465860 (dock states 5 and 9): a Leech,
                  Worker or Taiidan resource collector docking for a purpose that reaches a bay or a transfer point
                  empties its hold into its player's RUs (sound 0x804; the player's stats and the single-player total)
    Inputs      : ship, purpose
----------------------------------------------------------------------------*/
static void cat_dockDepositShipResources(Ship *ship,DockType purpose)
{
    switch (purpose)
    {
        case CAT_DOCK_SUPPORT:
        case CAT_DOCK_STAY:
        case CAT_DOCK_LAUNCH:
        case CAT_DOCK_DEPOSIT_RESOURCES:
        case CAT_DOCK_RETIRE:
        case CAT_DOCK_SUPPORT_INSTANT:
        case CAT_DOCK_STAY_INSTANT:
            switch (ship->shiptype)
            {
                case sLeech:
                case sWorker:
                case bWorker:
                case tResourceCollector:
                {
                    sdword resources = ship->resources;

                    ship->resources = 0;
                    soundEventPlay(ship,CAT_DOCK_PURPOSE_RESOURCE_DEPOSIT_SOUND,NULL);
                    cat_dockDepositResourceUnits(ship->playerowner,resources);
                    break;
                }
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockSetDockTime
    Description : CAT: the time a docked or latched ship stays (cat_dockShipDocksAtShip states 5 and 7): now, plus
                  4 s when it carries resources, plus 6 s when it is damaged
    Inputs      : ship
----------------------------------------------------------------------------*/
static void cat_dockSetDockTime(Ship *ship)
{
    ship->dockvars.catDockTime = universe.totaltimeelapsed;
    if ((real32)ship->resources > 0.0f)
    {
        ship->dockvars.catDockTime += CAT_DOCK_TIME_BONUS_RESOURCES;
    }
    if (ship->targetable.health < ship->staticinfo->health.maxhealth)
    {
        ship->dockvars.catDockTime += CAT_DOCK_TIME_BONUS_DAMAGED;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockChatter
    Description : CAT: the "docking" battle chatter of cat_dockShipDocksAtShip (event 0x4A, or 0x53 for a ship with
                  resources, with the ship's type as the variable)
    Inputs      : ship
----------------------------------------------------------------------------*/
static void cat_dockChatter(Ship *ship)
{
    if (battleCanChatterAtThisTime(0x4A,ship))
    {
        battleChatterAttempt(-1,((real32)ship->resources > 0.0f) ? 0x53 : 0x4A,ship,ship->staticinfo->shiptype);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockShipDocksAtShip
    Description : CAT: the dock state machine of cataclysm.exe 0x00465860, one step for ship docking at dockwith for
                  purpose (HW1's ShipDocksAtShip role for every ship type)
    Inputs      : ship, dockwith, purpose
    Return      : 1 when the ship is done with the dock command (docked and staying, retired, or back outside)
----------------------------------------------------------------------------*/
udword cat_dockShipDocksAtShip(Ship *ship,Ship *dockwith,DockType purpose)
{
    DockStaticPoint *point;
    vector trajectory, position;
    real32 range, dockrange;

    switch (ship->dockvars.dockstate)
    {
        case CAT_DOCKSTATE_APPROACH:
            if ((purpose == CAT_DOCK_SUPPORT_INSTANT) || (purpose == CAT_DOCK_STAY_INSTANT))
            {
                univRemoveShipFromOutside(ship);
                dockPutShipInside(ship,dockwith);
                if ((dockwith->shiptype != dRKO) && (ship->header.catFlags18 & CAT_SHIPFLAG_Mimic))
                {
                    cat_mimicCleanup(ship);
                }
                return 1;
            }
            dockrange = dockwith->staticinfo->dockShipRange + dockwith->staticinfo->common.staticCollInfo.avoidcollspheresize;
            aishipGetTrajectory(ship,(SpaceObjRotImpTarg *)dockwith,&trajectory);
            range = RangeToTarget(ship,(SpaceObjRotImpTarg *)dockwith,&trajectory);
            if ((range >= dockrange) || !cat_dockFindNearestDockPoint(ship,dockwith,purpose))
            {
                aishipGetTrajectory(ship,(SpaceObjRotImpTarg *)dockwith,&trajectory);
                range = RangeToTarget(ship,(SpaceObjRotImpTarg *)dockwith,&trajectory);
                if (range < dockrange)
                {
                    aitrackForceSteadyShip(ship);
                    return 0;
                }
                aishipFlyToPointAvoidingObjsFunc(ship,&dockwith->obj.posinfo.position,
                                                 AISHIP_PointInDirectionFlying + AISHIP_CarTurn + AISHIP_DontFlyToObscuredPoints + AISHIP_ReturnImmedIfPointObscured,
                                                 0.0f,NULL);     // 0x3A
                return 0;
            }
            if (cat_shipIsUntargetable(dockwith) && (purpose == CAT_DOCK_SLIPGATE))
            {
                cat_slipgateAddShip(dockwith,ship);
            }
            ship->dockvars.dockstate = CAT_DOCKSTATE_FLY_TO_CONE;
            break;

        case CAT_DOCKSTATE_FLY_IN_FRONT:
            if (cat_dockFlyInFrontOfHost(ship,dockwith))
            {
                ship->dockvars.dockstate = CAT_DOCKSTATE_FLY_TO_CONE;
                return 0;
            }
            break;

        case CAT_DOCKSTATE_FLY_TO_CONE:
            point = catDockPointOf(ship,dockwith);
            if (catDockHostIsSlipgate(dockwith))
            {
                cat_slipgateOpen(dockwith);
            }
            if (cat_dockFlyToConeInside(ship,dockwith,point))
            {
                bitSet(ship->shipOrDerelict.specialFlags2,SPECIAL_2_CatNoAvoidance);
                bitSet(ship->header.flags,SOF_CatDocking);
                if (cat_dockCanDockWithBusyHost(ship,purpose))
                {
                    ship->dockvars.dockstate = CAT_DOCKSTATE_CONE_ORIGIN;
                    return 0;
                }
                ship->dockvars.dockstate = CAT_DOCKSTATE_LATCH;
                matMultiplyMatByVec(&ship->catDockLatchHeading,&dockwith->rotatable.rotinfo.coordsys,&point->conenormal);
                return 0;
            }
            break;

        case CAT_DOCKSTATE_LATCH:
            if (catDockHostIsSlipgate(dockwith))
            {
                cat_slipgateOpen(dockwith);
            }
            if (cat_dockFlyToConeOriginLatch(ship,dockwith,CAT_DOCK_LATCH_RADIUS))
            {
                if (catDockHostIsSlipgate(dockwith))
                {
                    if (cat_slipgateOpen(dockwith))
                    {
                        ship->dockvars.dockstate = CAT_DOCKSTATE_SLIPGATE_WAIT_IN;
                        ship->catDockStateTime = universe.totaltimeelapsed;
                        cat_shipReleaseAttachedRammers(ship);
                        cat_univRemoveObjReferencesFromShips(&ship->obj);
                        cat_dockFreePoint(ship,dockwith);
                        return 0;
                    }
                    aitrackSteadyShip(ship);
                    return 0;
                }
                {
                    sdword index = ship->dockvars.catDockPointIndex;
                    DockStaticInfo *dockstaticinfo = dockwith->staticinfo->dockStaticInfo;

                    univRemoveShipFromOutside(ship);
                    if (purpose == CAT_DOCK_RETIRE)
                    {
                        speechEventQueue(NULL,0x2001040,ship->shiptype,-1,0,ship->playerowner->playerIndex,-1,5.0f,-1);
                        if (ship->shiprace == 1)
                        {
                            // CAT-TODO(M2-R6 research GUI): cat_rmAcquireShipTechs(ship) 0x004f3650: a retired Beast ship's
                            //      technologies go to its player
                        }
                    }
                    else
                    {
                        cat_dockChatter(ship);
                        dockPutShipInside(ship,dockwith);
                        ship->dockvars.dockstate = CAT_DOCKSTATE_INSIDE;
                        // [audit: M2-R5 layout-dock] the exe (0x00465860 state 5) empties the hold BEFORE it tests
                        //      "resources > 0" for the 4 s bonus, so a depositing Worker gets no bonus; the deposit
                        //      goes first here too (the order was reversed)
                        cat_dockDepositShipResources(ship,purpose);
                        cat_dockSetDockTime(ship);
                        if ((dockwith->shiptype != dRKO) && (ship->header.catFlags18 & CAT_SHIPFLAG_Mimic))
                        {
                            cat_mimicCleanup(ship);
                        }
                    }
                    // an sCarrier keeps a medium point held while the ship is inside (its door); every other point is freed
                    if ((dockwith->shiptype != sCarrier) ||
                        ((index != -1) && (dockstaticinfo->dockstaticpoints[index].type != DPT_Med_In) &&
                         (dockstaticinfo->dockstaticpoints[index].type != DPT_Med_Out)))
                    {
                        cat_dockFreePoint(ship,dockwith);
                    }
                    if (purpose == CAT_DOCK_RETIRE)
                    {
                        ship->command->dock.wasHarvesting = FALSE;
                        return 1;
                    }
                }
            }
            break;

        case CAT_DOCKSTATE_CONE_ORIGIN:
            point = catDockPointOf(ship,dockwith);
            if (cat_dockFlyToConeOrigin(ship,dockwith,point))
            {
                if ((ship->shiptype == sWorker) || (ship->shiptype == bWorker))
                {
                    if ((ship->madBindings != NULL) && (ship->madBindings->nCurrentAnim != cat_salvageOpenAnimIndex(ship->shiptype)))
                    {
                        cat_madAnimationStartNumber(ship,cat_salvageOpenAnimIndex(ship->shiptype));   // CAT-TODO(stage 3): the exe passes a third argument, 0
                        madAnimationPause(ship,TRUE);
                    }
                }
                ship->dockvars.dockstate = CAT_DOCKSTATE_LINK_IN;
                return 0;
            }
            break;

        case CAT_DOCKSTATE_LINK_IN:
        {
            DockStaticPoint *linkpoint = (ship->staticinfo->dockStaticInfo != NULL) ? ship->staticinfo->dockStaticInfo->centerDockPoint : NULL;

            point = catDockPointOf(ship,dockwith);
            if (cat_linkFlyIntoPosition(ship,linkpoint,dockwith,point))
            {
                cat_dockSetDockTime(ship);
                clampObjToObj((SpaceObjRotImpTargGuidance *)ship,(SpaceObjRotImpTargGuidance *)dockwith);
                ship->dockvars.dockstate = CAT_DOCKSTATE_LATCHED;
                cat_dockChatter(ship);
                if ((ship->shiptype == bWorker) || (ship->shiptype == sWorker))
                {
                    soundEventPlay(ship,CAT_DOCK_LATCHED_SOUND,NULL);
                    return 0;
                }
            }
            break;
        }

        case CAT_DOCKSTATE_INSIDE:
            if (ship->dockvars.catDockTime < universe.totaltimeelapsed)
            {
                if ((cat_lmShipAutoLaunchOn(dockwith) || ShipHasToLaunch(dockwith,ship)) &&
                    !(ship->shipOrDerelict.specialFlags & SPECIAL_STAY_TILL_EXPLICITLAUNCH))
                {
                    if (purpose != CAT_DOCK_DEPOSIT_RESOURCES)
                    {
                        ship->command->dock.wasHarvesting = FALSE;
                    }
                    ship->dockvars.dockstate = CAT_DOCKSTATE_LAUNCH;
                    return 0;
                }
                ship->command->dock.wasHarvesting = FALSE;
                return 1;
            }
            break;

        case CAT_DOCKSTATE_LATCHED:
            if ((ship->dockvars.catDockTime < universe.totaltimeelapsed) &&
                !(ship->shipOrDerelict.specialFlags & SPECIAL_StopForResearchDocking))
            {
                cat_dockDepositShipResources(ship,purpose);
                unClampObj((SpaceObjRotImpTargGuidance *)ship);
                ship->dockvars.dockstate = CAT_DOCKSTATE_FLY_OUT;
                return 0;
            }
            break;

        case CAT_DOCKSTATE_LAUNCH:
            if (singlePlayerGame && (singlePlayerGameInfo.currentMission == 3) && (ship->shiptype == dEcclesiam))
            {
                return 1;
            }
            if (!(dockwith->shipOrDerelict.specialFlags & SPECIAL_Hyperspacing) && cat_dockFindLaunchPoint(ship,dockwith))
            {
                point = catDockPointOf(ship,dockwith);
                bitSet(ship->shipOrDerelict.specialFlags2,SPECIAL_2_CatNoAvoidance);
                bitSet(ship->header.flags,SOF_CatDocking);
                matMultiplyMatByVec(&position,&dockwith->rotatable.rotinfo.coordsys,&point->position);
                vecAddTo(position,dockwith->obj.posinfo.position);
                dockRemoveShipFromInside(ship,dockwith);
                dockPutShipOutside(ship,dockwith,&position,point->headingdirection,point->updirection);
                if (catDockHostIsSlipgate(dockwith))
                {
                    ship->dockvars.dockstate = CAT_DOCKSTATE_SLIPGATE_WAIT_OUT;
                    ship->catDockStateTime = universe.totaltimeelapsed;
                    return 0;
                }
                if (!(ship->shipOrDerelict.specialFlags2 & SPECIAL_2_CatBeingBuilt))
                {
                    ship->dockvars.dockstate = CAT_DOCKSTATE_FLY_OUT;
                    if (ship->staticinfo->UpgradeByDocking || cat_shipIsLiveWorker(ship))
                    {
                        cat_shipApplyUpgrades(ship);
                    }
                    cat_acolyteLinkReset(ship);
                    cat_acvLinkReset(ship);
                    ship->targetable.health = ship->staticinfo->health.maxhealth;
                    bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerWithinHyperSpaceRange);
                    return 0;
                }
                ship->dockvars.dockstate = CAT_DOCKSTATE_BUILD_ATTACHED;
                clampObjToObj((SpaceObjRotImpTargGuidance *)ship,(SpaceObjRotImpTargGuidance *)ship->catCreator);
                cat_shipSetDisableReason(ship,1,4);
                if (ship->playerowner->aiPlayer != NULL)
                {
                    SelectCommand selectone;

                    selectone.numShips = 1;
                    selectone.ShipPtr[0] = ship;
                    cat_clWrapStartExternalBuildTimer(&universe.mainCommandLayer,&selectone);
                    return 0;
                }
                // CAT-TODO(M2-R6 ConsMgr, CAT_EXTERNAL_BUILD_DRIVER): cat_cmExternalBuildAttach(ship) 0x00459be0 (the
                //      construction manager takes over the externally built ship); unreachable while the driver is off
                return 0;
            }
            break;

        case CAT_DOCKSTATE_BUILD_ATTACHED:
            if (!(ship->shipOrDerelict.specialFlags2 & SPECIAL_2_CatBeingBuilt))
            {
                // CAT-TODO(M2-R6 ConsMgr): cat_suShipProvidesSU(ship) 0x00459b00 (the support units)
                if (ship->staticinfo->shipclass == CLASS_Component)
                {
                    return 1;
                }
                unClampObj((SpaceObjRotImpTargGuidance *)ship);
                cat_shipSetDisableReason(ship,0,4);
                // CAT-TODO(M2-R6 ConsMgr): cat_cmShipDockedReclampCannon(ship) 0x00456f60 (a Dreadnought's cannon)
                ship->dockvars.dockstate = CAT_DOCKSTATE_FLY_OUT;
                if (ship->staticinfo->catTypeFlags & 6)
                {
                    cmAddFactory(ship,(ship->staticinfo->catTypeFlags >> 2) & 1);
                    return 0;
                }
            }
            break;

        case CAT_DOCKSTATE_FLY_OUT:
            if (cat_dockFlyOutOfCone(ship,dockwith))
            {
                if (cat_dockCanDockWithBusyHost(ship,purpose))
                {
                    if ((ship->shiptype == sWorker) || (ship->shiptype == bWorker))
                    {
                        if ((ship->madBindings != NULL) && (ship->madBindings->nCurrentAnim != cat_salvageCloseAnimIndex(ship->shiptype)))
                        {
                            cat_madAnimationStartNumber(ship,cat_salvageCloseAnimIndex(ship->shiptype));   // CAT-TODO(stage 3): the exe passes a third argument, 2
                        }
                    }
                }
                return 1;
            }
            break;

        case CAT_DOCKSTATE_SLIPGATE_ENTER:
            aitrackSteadyShipDriftOnly(ship);
            cat_slipgateOpen(dockwith);
            if (ship->dockvars.catDockTime < universe.totaltimeelapsed)
            {
                SlipgateSpec *gate = (SlipgateSpec *)dockwith->ShipSpecifics;
                Ship *partner = gate->partner;

                univRemoveShipFromOutside(ship);
                dockPutShipInside(ship,partner);
                ship->dockvars.dockship = partner;
                ship->dockvars.dockstate = CAT_DOCKSTATE_LAUNCH;
                ((ubyte *)&gate->unknown1C)[ship->playerowner->playerIndex] = 1;
                ((ubyte *)&((SlipgateSpec *)partner->ShipSpecifics)->unknown1C)[ship->playerowner->playerIndex] = 1;
                return 0;
            }
            break;

        case CAT_DOCKSTATE_SLIPGATE_EXIT:
            aitrackSteadyShipDriftOnly(ship);
            cat_slipgateOpen(dockwith);
            if (ship->dockvars.catDockTime < universe.totaltimeelapsed)
            {
                ship->dockvars.dockstate = CAT_DOCKSTATE_FLY_OUT;
                return 0;
            }
            break;

        case CAT_DOCKSTATE_SLIPGATE_WAIT_IN:
        case CAT_DOCKSTATE_SLIPGATE_WAIT_OUT:
            aitrackSteadyShipDriftOnly(ship);
            cat_slipgateOpen(dockwith);
            if (ship->catDockStateTime + CAT_DOCK_SLIPGATE_TRANSIT_TIME < universe.totaltimeelapsed)
            {
                ship->dockvars.dockstate = (ship->dockvars.dockstate == CAT_DOCKSTATE_SLIPGATE_WAIT_IN) ? CAT_DOCKSTATE_SLIPGATE_ENTER : CAT_DOCKSTATE_SLIPGATE_EXIT;
                return 0;
            }
            break;

        default:
            break;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : cat_dockFinishedDocking
    Description : CAT: a ship is done with its dock command (cataclysm.exe 0x004667f0): out of the dock process and
                  the command; a retiring ship refunds its build cost (and its hold, and a crystal's value), marks
                  Mission 9's KAS variable and is deleted; a ship that flew out is grouped around its host
    Inputs      : ship, dockwith, command (the dock command)
----------------------------------------------------------------------------*/
void cat_dockFinishedDocking(Ship *ship,Ship *dockwith,struct CommandToDo *command)
{
    SelectCommand *selection = command->selection;

    RemoveShipFromDocking(ship);
    clRemoveShipFromSelection(selection,ship);
    ship->command = NULL;
    if (command->ordertype.attributes & COMMAND_MASK_FORMATION)
    {
        RemoveShipFromFormation(ship);
        if (selection->numShips > 0)
        {
            formationContentHasChanged(command);
        }
    }
    if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        RemoveShipFromAttacking(ship);
        RemoveShipReferencesFromExtraAttackInfo(ship,command);
    }
    if (command->dock.dockType == CAT_DOCK_RETIRE)
    {
        sdword playerIndex = ship->playerowner->playerIndex;
        // the refund: the decompile shows the __ftol of a float the exe computed before, as HW1's
        //      TW_RETIRE_MONEYBACK_RATIO x buildCost (medium confidence), plus the ship's hold
        sdword refund = (sdword)(TW_RETIRE_MONEYBACK_RATIO * (real32)ship->staticinfo->buildCost) + ship->resources;

        if (cat_shipIsCrystal(ship))
        {
            refund += ((CrystalSpec *)ship->ShipSpecifics)->resourceValue;
        }
        ship->playerowner->resourceUnits += refund;
        if (singlePlayerGame && (singlePlayerGameInfo.currentMission == 9) && (ship->shiptype == tResourceCollector))
        {
            // CAT-TODO(stage 6 KAS): cat_kasVarFindOrCreate(0, cat_kasHashString("G_ResourcersRetired"))->value = 1
        }
        universe.gameStats.playerStats[playerIndex].totalRegeneratedResourceUnits += refund;
        universe.gameStats.playerStats[playerIndex].totalResourceUnits += refund;
        ship->howDidIDie = DEATH_BY_RETIREMENT;
        aiplayerShipDied(ship);
        univDeleteWipeInsideShipOutOfExistence(ship);
        return;
    }
    if ((ship->dockvars.dockstate == CAT_DOCKSTATE_FLY_OUT) && !command->dock.wasHarvesting &&
        (ship->shiptype != sSwarmer) && (ship->shiptype != bSwarmer))
    {
        GroupShip(&universe.mainCommandLayer,ship,dockwith);
    }
}

/*-----------------------------------------------------------------------------
    Name        : processDockToDo
    Description : processes a dock command (cataclysm.exe 0x00466950): a ship without a host gets the nearest one;
                  the others step through cat_dockShipDocksAtShip; a ship that is done leaves the command unless it
                  was harvesting (then the command ends and the ship goes back to harvesting)
    Inputs      : docktodo
    Return      : TRUE when every ship is done
----------------------------------------------------------------------------*/
bool32 processDockToDo(CommandToDo *docktodo)
{
    SelectCommand *selection = docktodo->selection;
    DockType purpose = docktodo->dock.dockType;
    Ship *ship;
    Ship *dockwith;
    bool32 alldone = TRUE;
    sdword i = 0;

    if (selection->numShips < 1)
    {
        return TRUE;
    }
    do
    {
        ship = selection->ShipPtr[i];
        dockwith = ship->dockvars.dockship;
        if (dockwith == NULL)
        {
            dockwith = FindNearestShipToDockAt(ship,purpose);
            if (dockwith != NULL)
            {
                InitializeDockVars(ship,dockwith,purpose);          // the exe inlines it
                InitShipAI(ship,TRUE);
                alldone = FALSE;
            }
            i++;
        }
        else if (cat_dockShipDocksAtShip(ship,dockwith,purpose) == 0)
        {
            alldone = FALSE;
            i++;
        }
        else
        {
            if (docktodo->ordertype.attributes & COMMAND_MASK_FORMATION)
            {
                RemoveShipFromFormation(ship);
                if (selection->numShips > 0)
                {
                    formationContentHasChanged(docktodo);
                }
            }
            if (docktodo->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
            {
                RemoveShipFromAttacking(ship);
                RemoveShipReferencesFromExtraAttackInfo(ship,docktodo);
            }
            if (docktodo->dock.wasHarvesting && (purpose != CAT_DOCK_RETIRE))
            {
                i++;                                        // stays in the command, which clProcess turns back to harvesting
            }
            else
            {
                cat_dockFinishedDocking(ship,dockwith,docktodo);  // removes the ship from the selection: same index again
            }
        }
    } while (i < selection->numShips);
    return alldone;
}

/*-----------------------------------------------------------------------------
    Name        : dockPutShipOutside
    Description : puts a ship that was inside creator back into the universe at createat, heading and up as the
                  launch point says (cataclysm.exe 0x00463350): into the lists and blobs with its modules, unhidden,
                  with the creator's velocity; a support module is turned about its axis by the parity of its launch
                  point (bMothershipSupport also 7 degrees about the host's right); the fog of war is told
    Inputs      : ship, creator, createat, headingdirection, updirection
----------------------------------------------------------------------------*/
void dockPutShipOutside(Ship *ship,Ship *creator,vector *createat,udword headingdirection,udword updirection)
{
    vector heading;
    vector up;
    vector right;
    sdword i;
    Ship *module;

    dbgAssertOrIgnore(creator);
    if (creator->obj.collMyBlob != NULL)
    {
        collAddSpaceObjToSpecificBlob(creator->obj.collMyBlob,(SpaceObj *)ship);
    }
    else
    {
        collAddSpaceObjToCollBlobs((SpaceObj *)ship);
    }
    listAddNode(&universe.SpaceObjList,&(ship->header.objlink),ship);
    listAddNode(&universe.ShipList,&(ship->shiplink),ship);
    listAddNode(&universe.ImpactableList,&(ship->impactable.impactablelink),ship);

    bitClear(ship->header.flags,SOF_Hide);                  // CAT: the exe unhides the ship whatever the creator's state
    univAddObjToRenderListIf((SpaceObj *)ship,(SpaceObj *)creator);     // add to render list if parent ship is in render list
    for (module = cat_moduleIterFirst(ship); module != NULL; module = cat_moduleIterNext())
    {
        univAddObjToRenderListIf((SpaceObj *)module,(SpaceObj *)creator);
        bitClear(module->header.flags,SOF_Hide);
    }
    ship->obj.posinfo.position = *createat;
    ship->obj.posinfo.velocity = creator->obj.posinfo.velocity;

    GetDirectionVectorOfShip(&heading,headingdirection,&creator->rotatable.rotinfo.coordsys);
    GetDirectionVectorOfShip(&up,updirection,&creator->rotatable.rotinfo.coordsys);
    vecCrossProduct(right,heading,up);   // should not have to normalize because heading, up are unit vectors and angle between them is 90 degrees.
    matCreateMatFromVecs(&ship->rotatable.rotinfo.coordsys,&up,&right,&heading);

    // CAT-TODO(M2-R6 FOW): cat_univShipFOWActivate(ship) 0x00547c40 (the ship's fog-of-war cube comes back)

    // the support modules: turned about the launch point's axis by the parity of the point's index within its type
    {
        ShipType type = ship->staticinfo->shiptype;
        sdword parity = 0;
        matrix copy, rot;

        if ((type == sMothershipSupport) || (type == sCarrierSupport) || (type == bMothershipSupport))
        {
            DockStaticInfo *dockstaticinfo = creator->staticinfo->dockStaticInfo;
            sdword index = ship->dockvars.catDockPointIndex;

            if (index != -1)
            {
                parity = (sbyte)index - (sbyte)dockstaticinfo->firstOfType[dockstaticinfo->dockstaticpoints[index].type];
            }
        }
        if ((type == sMothershipSupport) || (type == sCarrierSupport))
        {
            if (parity & 1)
            {
                matMakeRotAboutX(&rot,(real32)cos(CAT_DOCK_SUPPORT_ROT_X),(real32)sin(CAT_DOCK_SUPPORT_ROT_X));
                copy = ship->rotatable.rotinfo.coordsys;
                matMultiplyMatByMat(&ship->rotatable.rotinfo.coordsys,&copy,&rot);
            }
        }
        else if (type == bMothershipSupport)
        {
            if (parity & 2)
            {
                real32 angle = (parity & 1) ? -CAT_DOCK_SUPPORT_ROT_Z : CAT_DOCK_SUPPORT_ROT_Z;

                matMakeRotAboutY(&rot,(real32)cos(CAT_DOCK_SUPPORT_ROT_X),(real32)sin(CAT_DOCK_SUPPORT_ROT_X));
                copy = ship->rotatable.rotinfo.coordsys;
                matMultiplyMatByMat(&ship->rotatable.rotinfo.coordsys,&copy,&rot);
                matMakeRotAboutZ(&rot,(real32)cos(angle),(real32)sin(angle));
                copy = ship->rotatable.rotinfo.coordsys;
                matMultiplyMatByMat(&ship->rotatable.rotinfo.coordsys,&copy,&rot);
            }
            if (parity & 1)
            {
                matMakeRotAboutX(&rot,(real32)cos(CAT_DOCK_SUPPORT_ROT_X),(real32)sin(CAT_DOCK_SUPPORT_ROT_X));
                copy = ship->rotatable.rotinfo.coordsys;
                matMultiplyMatByMat(&ship->rotatable.rotinfo.coordsys,&copy,&rot);
            }
        }
    }

    univUpdateObjRotInfo(&ship->rotatable);

    for (i = 0; i < MAX_NUM_TRAILS; i++)
    {
        if (ship->trail[i])
        {
            trailZeroLength(ship->trail[i]);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : ShipIsRefuelingAtCarrierMother
    Description : whether ship is inside a carrier for support (cataclysm.exe 0x00467a90: the host is an sCarrier,
                  bCarrier, tCarrier or kCarrier and the dock state is 8 or 9)
    Inputs      : ship
----------------------------------------------------------------------------*/
bool32 ShipIsRefuelingAtCarrierMother(struct Ship *ship)
{
    Ship *dockship = ship->dockvars.dockship;

    if (dockship == NULL)
    {
        return FALSE;
    }
    if ((dockship->shiptype != sCarrier) && (dockship->shiptype != bCarrier) && (dockship->shiptype != tCarrier) &&
        (dockship->shiptype != kCarrier))
    {
        return FALSE;
    }
    return ((ship->dockvars.dockstate == CAT_DOCKSTATE_INSIDE) || (ship->dockvars.dockstate == CAT_DOCKSTATE_LATCHED));
}

/*-----------------------------------------------------------------------------
    Name        : ShipIsWaitingForSoftLaunch
    Description : whether ship is docked and waiting (cataclysm.exe 0x00467a60: a host and dock state 8 or 9)
    Inputs      : ship
----------------------------------------------------------------------------*/
bool32 ShipIsWaitingForSoftLaunch(struct Ship *ship)
{
    if ((ship->dockvars.dockship != NULL) &&
        ((ship->dockvars.dockstate == CAT_DOCKSTATE_INSIDE) || (ship->dockvars.dockstate == CAT_DOCKSTATE_LATCHED)))
    {
        return TRUE;
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : dockDrawDockInfo
    Description : draws docking info for debugging
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
#if RND_VISUALIZATION
void dockDrawDockInfo(Ship *ship)
{
    sdword i;
    DockStaticInfo *dockStaticInfo = ship->staticinfo->dockStaticInfo;
    sdword numDockPoints;
    DockStaticPoint *dockstaticpoint;
    vector to;

    if (dockStaticInfo == NULL)
    {
        return;
    }

    numDockPoints = dockStaticInfo->numDockPoints;

    rndLightingEnable(FALSE);
    rndTextureEnable(FALSE);

    for (i=0,dockstaticpoint=&dockStaticInfo->dockstaticpoints[0];i<numDockPoints;i++,dockstaticpoint++)
    {
        vecScalarMultiply(to,dockstaticpoint->conenormal,1000.0f);
        vecAddTo(to,dockstaticpoint->position);
        primLine3(&dockstaticpoint->position,&to,colWhite);
    }

    rndLightingEnable(TRUE);
}
#endif

/*=============================================================================
    CAT (M2-R5 dock): the scripted dock self-test, /catDockTest <ticks> (harness mode dock; docs/m2-r5-layout-dock.md).
    Not in the exe. It runs the headless Mission01 start of /oracleMission (virtual clock, no animatic, quits after
    <ticks>) and drives the local player's ships through the dock purposes with no input:
      tick 16    the scripted fleet is created for the local player far from the level's ships (an sMothership, made the
                 player's mothership as the mission script does, an sProcessor, three Workers, four Acolytes, two Recons,
                 two ACVs), the player's auto-launch bits are cleared (so docked ships stay inside until told), the
                 acolytes, recons and ACVs are ordered to dock for support (purpose 0, nearest host), one Worker is
                 given 250 RUs and ordered to deposit them (purpose 3), another Worker is sent to harvest
                 (cat_clHarvestNearestResource). The PASS verdict covers the fighters (acolytes, recons) and the
                 depositing Worker; the two corvette-class ACVs are ordered, logged and launched if they got in, but
                 not judged: one of them orbits its dock point for minutes before latching (docs/m2-r5-layout-dock.md,
                 open questions), and the harvesting Worker is only traced
      every 256  one "docktest:" line per scripted ship (dock state, host, inside/outside, hold, position)
      ticks/4..  checkpoint A, once every fighter is inside (at the latest at 80% of the ticks): a docked fighter is in a
                 host's inside list, off the ShipList, in dock state 8, with no command and no host left in its dock vars
                 (the exe's RemoveShipFromDocking clears it when the command finishes); the depositing Worker is back
                 outside with an empty hold and the player's RUs are up by 250; then every docked fighter is launched
                 (clLaunchMultipleShips -> clLaunchShip, purpose 2)
      ticks-2    checkpoint B: the fighters outside again with no host and no dock command
      "docktest: done ... result=PASS|FAIL", then the mission run's own quit
=============================================================================*/
#define CAT_DOCKTEST_ORDER_TICK     16
#define CAT_DOCKTEST_DEPOSIT_RUS    250
#define CAT_DOCKTEST_MAX_SHIPS      64

static sdword catDockSelfTestTicks = 0;
static bool32 catDockSelfTestOrdered = FALSE;
static bool32 catDockSelfTestLaunched = FALSE;
static bool32 catDockSelfTestDone = FALSE;
static bool32 catDockSelfTestPass = TRUE;
static sdword catDockSelfTestRUBefore = 0;
static Ship *catDockSelfTestFighters[CAT_DOCKTEST_MAX_SHIPS];
static sdword catDockSelfTestNumFighters = 0;
static Ship *catDockSelfTestCorvettes[CAT_DOCKTEST_MAX_SHIPS];   // the ACVs: ordered and logged, not judged (see the header)
static sdword catDockSelfTestNumCorvettes = 0;
static Ship *catDockSelfTestDepositor = NULL;
static Ship *catDockSelfTestHarvester = NULL;

void catDockSelfTestSet(char *string)
{
    catDockSelfTestTicks = (sdword)strtol(string, NULL, 10);
    if (catDockSelfTestTicks <= 0)
    {
        catDockSelfTestTicks = 5000;
    }
    oracleSetMission(string);                               // the headless Mission01 run with the virtual clock and the quit
}

bool32 catDockSelfTestRequested(void)
{
    return (catDockSelfTestTicks > 0);
}

static Ship *catDockSelfTestInsideHostOf(Ship *ship)
{
    Node *node;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        Ship *host = (Ship *)listGetStructOfNode(node);

        if (cat_dockShipIsInsideHost(ship,host))
        {
            return host;
        }
    }
    return NULL;
}

static bool32 catDockSelfTestShipAlive(Ship *ship)
{
    Node *node;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        if ((Ship *)listGetStructOfNode(node) == ship)
        {
            return TRUE;
        }
    }
    // inside a host: still alive but off the ShipList
    return (catDockSelfTestInsideHostOf(ship) != NULL);
}

static void catDockSelfTestLogShip(const char *tag, Ship *ship)
{
    Ship *host = ship->dockvars.dockship;
    bool32 inside = (catDockSelfTestInsideHostOf(ship) != NULL);

    if ((ship->command != NULL) && (ship->command->ordertype.order == COMMAND_COLLECT_RESOURCES))
    {
        Resource *resource = ship->command->collect.resource;

        dbgMessagef("docktest: %s tick=%u %s#%u harvesting resource=%s value=%d rcstate=%d/%d effect=%d", tag, (unsigned)universe.univUpdateCounter,
                    ShipTypeToStr(ship->shiptype), (unsigned)ship->shipID.shipNumber,
                    (resource == NULL) ? "none" : ((resource->header.objtype == OBJ_AsteroidType) ? "asteroid" : "other"),
                    (resource != NULL) ? (int)resource->resourcevalue : -1, (int)ship->rcstate1, (int)ship->rcstate2, (int)(ship->rceffect != NULL));
    }
    dbgMessagef("docktest: %s tick=%u %s#%u state=%d host=%s inside=%d outside=%d order=%d hold=%d pos=(%.0f %.0f %.0f) hostpos=(%.0f %.0f %.0f)",
                tag, (unsigned)universe.univUpdateCounter, ShipTypeToStr(ship->shiptype), (unsigned)ship->shipID.shipNumber,
                (int)ship->dockvars.dockstate, (host != NULL) ? ShipTypeToStr(host->shiptype) : "none", (int)inside,
                (int)(ship->shiplink.belongto != NULL), (ship->command != NULL) ? (int)ship->command->ordertype.order : -1,
                (int)ship->resources, ship->obj.posinfo.position.x, ship->obj.posinfo.position.y, ship->obj.posinfo.position.z,
                (host != NULL) ? host->obj.posinfo.position.x : 0.0f, (host != NULL) ? host->obj.posinfo.position.y : 0.0f,
                (host != NULL) ? host->obj.posinfo.position.z : 0.0f);
}

static void catDockSelfTestExpect(bool32 condition, const char *what, Ship *ship)
{
    if (!condition)
    {
        catDockSelfTestPass = FALSE;
        dbgMessagef("docktest: FAIL %s (%s#%u)", what, (ship != NULL) ? ShipTypeToStr(ship->shiptype) : "-",
                    (ship != NULL) ? (unsigned)ship->shipID.shipNumber : 0u);
    }
}

void catDockSelfTestTick(void)
{
    sdword tick = (sdword)universe.univUpdateCounter;
    Player *player = universe.curPlayerPtr;
    Node *node;
    Ship *ship;
    sdword i;
    SelectCommand *selection;

    if (catDockSelfTestDone || (player == NULL))
    {
        return;
    }
    if (!catDockSelfTestOrdered && (tick >= CAT_DOCKTEST_ORDER_TICK))
    {
        sdword numWorkers = 0;

        catDockSelfTestOrdered = TRUE;
        catDockSelfTestTrace = TRUE;
        player->autoLaunch = 0;                             // scripted: docked ships stay inside until launched
        // a census of what the level put in the universe (the mission file's fleet is player 1's; the player's own
        // Somtaaw ships come from the kas2c script, which is off under CAT_HW1_MISSION_SCRIPTS 0)
        {
            sdword census[MAX_MULTIPLAYER_PLAYERS + 1];

            memset(census,0,sizeof(census));
            for (node = universe.ShipList.head; node != NULL; node = node->next)
            {
                ship = (Ship *)listGetStructOfNode(node);
                census[(ship->playerowner != NULL) ? ship->playerowner->playerIndex : MAX_MULTIPLAYER_PLAYERS]++;
            }
            dbgMessagef("docktest: census before the scripted fleet: player0=%d player1=%d others=%d", (int)census[0], (int)census[1],
                        (int)(universe.ShipList.num - census[0] - census[1]));
        }
        // the scripted fleet for the local player, far from the level's ships: a carrier (the sMothership, 75 dock
        // points), a processor, three Workers, four Acolytes, two Recons and two ACVs
        {
            static const struct { ShipType type; real32 dx, dy, dz; } fleet[] =
            {
                { sMothership,     0.0f,     0.0f,    0.0f },
                { sProcessor,   6000.0f,     0.0f,    0.0f },
                { sWorker,      3000.0f,  3000.0f,    0.0f },
                { sWorker,      3000.0f, -3000.0f,    0.0f },
                { sWorker,      4500.0f,     0.0f, 2500.0f },
                { sAcolyte,    -5000.0f,  1500.0f,    0.0f },
                { sAcolyte,    -5000.0f, -1500.0f,    0.0f },
                { sAcolyte,    -5000.0f,     0.0f, 1500.0f },
                { sAcolyte,    -5000.0f,     0.0f,-1500.0f },
                { sRecon,      -6000.0f,  3000.0f,    0.0f },
                { sRecon,      -6000.0f, -3000.0f,    0.0f },
                { sACV,        -4000.0f,  3500.0f,    0.0f },
                { sACV,        -4000.0f, -3500.0f,    0.0f },
            };
            vector base;
            sdword k;
            real32 limit = universe.radius * 0.8f;
            real32 bestdist = -1.0f;
            sdword gx, gy, gz;

            // the fleet goes to the grid point (step 10,000) inside 80% of the world sphere that is farthest from
            // every ship the level put down, so nothing engages it
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
            dbgMessagef("docktest: world radius %.0f; the scripted fleet goes to (%.0f %.0f %.0f), %.0f from the nearest level ship",
                        universe.radius, base.x, base.y, base.z, fsqrt(bestdist));

            for (k = 0; k < (sdword)(sizeof(fleet) / sizeof(fleet[0])); k++)
            {
                vector pos;

                pos.x = base.x + fleet[k].dx;
                pos.y = base.y + fleet[k].dy;
                pos.z = base.z + fleet[k].dz;
                ship = univAddShip(fleet[k].type,0,&pos,player,0);
                if (ship == NULL)
                {
                    catDockSelfTestExpect(FALSE, "scripted ship created", NULL);
                    continue;
                }
                dbgMessagef("docktest: created %s#%u at (%.0f %.0f %.0f) docks=%d", ShipTypeToStr(ship->shiptype),
                            (unsigned)ship->shipID.shipNumber, pos.x, pos.y, pos.z,
                            (ship->staticinfo->dockStaticInfo != NULL) ? (int)ship->staticinfo->dockStaticInfo->numDockPoints : 0);
                if (fleet[k].type == sMothership)
                {
                    // the mission's kas2c script makes the Kuun-Lan the player's mothership; the auto-launch bits of
                    // cat_lmShipAutoLaunchOn apply to it only then
                    player->PlayerMothership = ship;
                }
            }
        }
        selection = memAlloc(sizeofSelectCommand(CAT_DOCKTEST_MAX_SHIPS),"docktest",0);
        selection->numShips = 0;
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if (ship->playerowner != player)
            {
                continue;
            }
            if (((ship->shiptype == sAcolyte) || (ship->shiptype == sRecon) || (ship->shiptype == sACV)) &&
                (selection->numShips < CAT_DOCKTEST_MAX_SHIPS))
            {
                selection->ShipPtr[selection->numShips++] = ship;
                if (ship->shiptype == sACV)
                {
                    catDockSelfTestCorvettes[catDockSelfTestNumCorvettes++] = ship;
                }
                else
                {
                    catDockSelfTestFighters[catDockSelfTestNumFighters++] = ship;
                }
            }
            else if (ship->shiptype == sWorker)
            {
                if (numWorkers == 0)
                {
                    catDockSelfTestDepositor = ship;
                }
                else if (numWorkers == 1)
                {
                    catDockSelfTestHarvester = ship;
                }
                numWorkers++;
            }
        }
        catDockSelfTestRUBefore = player->resourceUnits;
        dbgMessagef("docktest: orders tick=%u fighters=%d corvettes=%d workers=%d RUs=%d ticks=%d", (unsigned)tick,
                    (int)catDockSelfTestNumFighters, (int)catDockSelfTestNumCorvettes, (int)numWorkers, (int)catDockSelfTestRUBefore,
                    (int)catDockSelfTestTicks);
        if (selection->numShips > 0)
        {
            sdword docking = clDock(&universe.mainCommandLayer,selection,CAT_DOCK_SUPPORT,NULL);
            dbgMessagef("docktest: clDock purpose 0: %d of %d fighters and corvettes docking", (int)docking, (int)selection->numShips);
            for (i = 0; i < catDockSelfTestNumFighters; i++)
            {
                catDockSelfTestLogShip("ordered", catDockSelfTestFighters[i]);
            }
            for (i = 0; i < catDockSelfTestNumCorvettes; i++)
            {
                catDockSelfTestLogShip("ordered", catDockSelfTestCorvettes[i]);
            }
        }
        if (catDockSelfTestDepositor != NULL)
        {
            SelectCommand selectone;

            catDockSelfTestDepositor->resources = CAT_DOCKTEST_DEPOSIT_RUS;    // scripted: a hold to deposit
            selectone.numShips = 1;
            selectone.ShipPtr[0] = catDockSelfTestDepositor;
            dbgMessagef("docktest: clDock purpose 3: %d worker(s) depositing %d RUs",
                        (int)clDock(&universe.mainCommandLayer,&selectone,CAT_DOCK_DEPOSIT_RESOURCES,NULL), CAT_DOCKTEST_DEPOSIT_RUS);
            catDockSelfTestLogShip("ordered", catDockSelfTestDepositor);
        }
        if (catDockSelfTestHarvester != NULL)
        {
            SelectCommand selectone;

            selectone.numShips = 1;
            selectone.ShipPtr[0] = catDockSelfTestHarvester;
            cat_clHarvestNearestResource(&universe.mainCommandLayer,&selectone,FALSE,FALSE);
            catDockSelfTestLogShip("harvest", catDockSelfTestHarvester);
        }
        memFree(selection);
        return;
    }
    if (!catDockSelfTestOrdered)
    {
        return;
    }
    if ((tick % 256) == 0)
    {
        for (i = 0; i < catDockSelfTestNumFighters; i++)
        {
            if (catDockSelfTestShipAlive(catDockSelfTestFighters[i]))
            {
                catDockSelfTestLogShip("tick", catDockSelfTestFighters[i]);
            }
        }
        for (i = 0; i < catDockSelfTestNumCorvettes; i++)
        {
            if (catDockSelfTestShipAlive(catDockSelfTestCorvettes[i]))
            {
                catDockSelfTestLogShip("tick", catDockSelfTestCorvettes[i]);
            }
        }
        if ((catDockSelfTestDepositor != NULL) && catDockSelfTestShipAlive(catDockSelfTestDepositor))
        {
            catDockSelfTestLogShip("tick", catDockSelfTestDepositor);
        }
        if ((catDockSelfTestHarvester != NULL) && catDockSelfTestShipAlive(catDockSelfTestHarvester))
        {
            catDockSelfTestLogShip("tick", catDockSelfTestHarvester);
        }
    }
    if (!catDockSelfTestLaunched && (tick >= catDockSelfTestTicks / 4))
    {
        sdword insideCount = 0;

        for (i = 0; i < catDockSelfTestNumFighters; i++)
        {
            if (catDockSelfTestInsideHostOf(catDockSelfTestFighters[i]) != NULL)
            {
                insideCount++;
            }
        }
        if ((insideCount < catDockSelfTestNumFighters) && (tick < (catDockSelfTestTicks * 4) / 5))
        {
            return;                                         // not every fighter is in yet: poll again next tick
        }
        catDockSelfTestLaunched = TRUE;
        insideCount = 0;
        dbgMessagef("docktest: checkpoint A tick=%u", (unsigned)tick);
        for (i = 0; i < catDockSelfTestNumFighters; i++)
        {
            Ship *host;

            ship = catDockSelfTestFighters[i];
            catDockSelfTestLogShip("A", ship);
            host = catDockSelfTestInsideHostOf(ship);
            // the exe's end state of a support dock that stays: dock state 8, inside a host, off the ShipList, the dock
            // command finished (RemoveShipFromDocking cleared dockvars.dockship)
            catDockSelfTestExpect(host != NULL, "fighter in a host's inside list", ship);
            catDockSelfTestExpect(ship->dockvars.dockstate == CAT_DOCKSTATE_INSIDE, "fighter in dock state 8 (inside)", ship);
            catDockSelfTestExpect(ship->shiplink.belongto == NULL, "fighter off the ShipList while inside", ship);
            catDockSelfTestExpect(ship->dockvars.dockship == NULL, "fighter's dock command finished (no host left in its dock vars)", ship);
            catDockSelfTestExpect(ship->command == NULL, "fighter has no command while docked", ship);
            if (host != NULL)
            {
                insideCount++;
            }
        }
        if (catDockSelfTestDepositor != NULL)
        {
            ship = catDockSelfTestDepositor;
            catDockSelfTestLogShip("A", ship);
            catDockSelfTestExpect(ship->resources == 0, "depositor's hold emptied", ship);
            catDockSelfTestExpect(ship->shiplink.belongto != NULL, "depositor back outside", ship);
            catDockSelfTestExpect(ship->dockvars.dockship == NULL, "depositor has no host", ship);
            catDockSelfTestExpect(player->resourceUnits == catDockSelfTestRUBefore + CAT_DOCKTEST_DEPOSIT_RUS, "player RUs up by the deposit", ship);
        }
        if (catDockSelfTestHarvester != NULL)
        {
            ship = catDockSelfTestHarvester;
            catDockSelfTestLogShip("A", ship);
            catDockSelfTestExpect((ship->command != NULL) &&
                                  ((ship->command->ordertype.order == COMMAND_COLLECT_RESOURCES) || (ship->command->ordertype.order == COMMAND_DOCK)),
                                  "harvester harvesting or bringing its hold home", ship);
        }
        for (i = 0; i < catDockSelfTestNumCorvettes; i++)
        {
            catDockSelfTestLogShip("A corvette", catDockSelfTestCorvettes[i]);
        }
        dbgMessagef("docktest: A inside=%d of %d fighters; RUs %d -> %d", (int)insideCount, (int)catDockSelfTestNumFighters,
                    (int)catDockSelfTestRUBefore, (int)player->resourceUnits);
        // launch every docked fighter from its host, host by host
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            Ship *host = (Ship *)listGetStructOfNode(node);
            SelectCommand *launch;

            if ((host->playerowner != player) || (host->shipsInsideMe == NULL))
            {
                continue;
            }
            launch = memAlloc(sizeofSelectCommand(CAT_DOCKTEST_MAX_SHIPS),"docktest",0);
            launch->numShips = 0;
            for (i = 0; i < catDockSelfTestNumFighters; i++)
            {
                if (cat_dockShipIsInsideHost(catDockSelfTestFighters[i],host) && (launch->numShips < CAT_DOCKTEST_MAX_SHIPS))
                {
                    launch->ShipPtr[launch->numShips++] = catDockSelfTestFighters[i];
                }
            }
            for (i = 0; i < catDockSelfTestNumCorvettes; i++)
            {
                if (cat_dockShipIsInsideHost(catDockSelfTestCorvettes[i],host) && (launch->numShips < CAT_DOCKTEST_MAX_SHIPS))
                {
                    launch->ShipPtr[launch->numShips++] = catDockSelfTestCorvettes[i];
                }
            }
            if (launch->numShips > 0)
            {
                dbgMessagef("docktest: clLaunchShip: %d fighters from %s#%u", (int)launch->numShips, ShipTypeToStr(host->shiptype),
                            (unsigned)host->shipID.shipNumber);
                clLaunchMultipleShips(&universe.mainCommandLayer,launch,host);
            }
            memFree(launch);
        }
        return;
    }
    if (tick >= catDockSelfTestTicks - 2)
    {
        sdword outsideCount = 0;

        catDockSelfTestDone = TRUE;
        dbgMessagef("docktest: checkpoint B tick=%u", (unsigned)tick);
        for (i = 0; i < catDockSelfTestNumFighters; i++)
        {
            ship = catDockSelfTestFighters[i];
            if (!catDockSelfTestShipAlive(ship))
            {
                catDockSelfTestExpect(FALSE, "fighter still exists", NULL);
                continue;
            }
            catDockSelfTestLogShip("B", ship);
            catDockSelfTestExpect(ship->shiplink.belongto != NULL, "fighter outside again", ship);
            catDockSelfTestExpect(ship->dockvars.dockship == NULL, "fighter has no host after launch", ship);
            catDockSelfTestExpect((ship->command == NULL) ||
                                  ((ship->command->ordertype.order != COMMAND_DOCK) && (ship->command->ordertype.order != COMMAND_LAUNCH_SHIP)),
                                  "fighter has no dock or launch command left", ship);
            if (ship->shiplink.belongto != NULL)
            {
                outsideCount++;
            }
        }
        for (i = 0; i < catDockSelfTestNumCorvettes; i++)
        {
            if (catDockSelfTestShipAlive(catDockSelfTestCorvettes[i]))
            {
                catDockSelfTestLogShip("B corvette", catDockSelfTestCorvettes[i]);
            }
        }
        if ((catDockSelfTestDepositor != NULL) && catDockSelfTestShipAlive(catDockSelfTestDepositor))
        {
            catDockSelfTestLogShip("B", catDockSelfTestDepositor);
        }
        if ((catDockSelfTestHarvester != NULL) && catDockSelfTestShipAlive(catDockSelfTestHarvester))
        {
            catDockSelfTestLogShip("B", catDockSelfTestHarvester);
        }
        dbgMessagef("docktest: done fighters=%d outside=%d corvettes=%d RUs=%d result=%s", (int)catDockSelfTestNumFighters, (int)outsideCount,
                    (int)catDockSelfTestNumCorvettes, (int)player->resourceUnits, catDockSelfTestPass ? "PASS" : "FAIL");
    }
}
