// =============================================================================
//  Collision.c
//  - handles object collisions
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 9/25/1997 by gshaw
// =============================================================================

#include "Collision.h"
#include "BeastWeapon.h"
#include "Leech.h"
#include "Mimic.h"
#include "RKO.h"
#include "RammingFrigate.h"
#include "Sentinel.h"
#include "Salvage.h"

#include "Alliance.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "Debug.h"
#include "FastMath.h"
#include "NIS.h"
#include "prim3d.h"
#include "Ships.h"
#include "SinglePlayer.h"
#include "Tactics.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "Mesh.h"          // CAT (M2-R5 render): cat_meshLineIntersect

// CAT (M2-R5 render): a ship whose static info sets bUseRayTriangleIntersectionForHitLocation (+0x408) is hit only
//      where a bullet's or beam's line meets its mesh, the missiles' line likewise (the exe's switches 0x00752c48 and
//      0x00752c4c, both 1, and the mesh LOD levels 0x00752c54 and 0x00752c50, both 3; nothing changes them)
static bool32 catCollMeshHitBullets = TRUE;             // 0x00752c48
static bool32 catCollMeshHitMissiles = TRUE;            // 0x00752c4c
static sdword catCollMeshHitMissileLevel = 3;           // 0x00752c50
static sdword catCollMeshHitBulletLevel = 3;            // 0x00752c54

// CAT: the box hit at collideLineDist along the line from start along dir, moved on to target's mesh: FALSE if the line
//      misses the mesh, else *collideLineDist gains the distance from the box hit to the mesh hit
//      (collCheckBulletTargetColl 0x0043cd90 at 0x0043d219, collCheckBeamCollWithTargetsInBlob 0x0043c570 at 0x0043c81e)
static bool32 cat_collMeshHitDistance(Ship *target, vector *start, vector *dir, real32 *collideLineDist)
{
    vector boxHit, meshHit;
    real32 dx, dy, dz;

    boxHit.x = *collideLineDist * dir->x + start->x;
    boxHit.y = *collideLineDist * dir->y + start->y;
    boxHit.z = *collideLineDist * dir->z + start->z;
    if (!cat_meshLineIntersect(target, catCollMeshHitBulletLevel, start, &boxHit, &meshHit))
    {
        return FALSE;
    }
    dx = meshHit.x - boxHit.x;
    dy = meshHit.y - boxHit.y;
    dz = meshHit.z - boxHit.z;
    *collideLineDist = (real32)(fsqrt(dx * dx + dy * dy + dz * dz) + *collideLineDist);
    return TRUE;
}

// CAT: does target need the mesh test (a ship with bUseRayTriangleIntersectionForHitLocation, the switch on)?
static bool32 cat_collUsesMeshHit(SpaceObj *target, bool32 enabled)
{
    return (target->header.objtype == OBJ_ShipType) &&
           ((ShipStaticInfo *)target->staticinfo)->bUseRayTriangleIntersectionForHitLocation && enabled;
}


BlobProperties collBlobProperties;

#if COLLISION_CHECK_STATS

udword shipsavoidingstuff;
udword shipsavoidedwalks;
udword shipsavoidedchecks;

udword shipshipwalks;
udword shipresourcewalks;
udword shipderelictwalks;
udword shipshipchecks;
udword shipresourcechecks;
udword shipderelictchecks;

udword bulletwalks;
udword bulletchecks;

udword missilewalks;
udword missilechecks;

udword minewalks;
udword minechecks;

#endif

/*-----------------------------------------------------------------------------
    Name        : RangeToTarget
    Description : Returns range from ship to target, taking into account collision
                  information of target.
    Inputs      : ship, target, trajectory (which is target->obj.posinfo.position - ship->obj.posinfo.position)
    Outputs     :
    Return      : range (in metres) to target
----------------------------------------------------------------------------*/
real32 RangeToTarget(Ship *ship,SpaceObjRotImpTarg *target,vector *trajectory)
{
    double dist;

    // CAT (M2-R8 oracle-rung-physics): the x87 shape (RangeToTarget 0x00439c30): (x*x + y*y) + z*z into fsqrt as a
    //      double, the sphere size subtracted on the FPU, one rounding at the return
    // CAT-TODO(M3 oracle physics): the exe returns the difference UNROUNDED on the FPU (0x00439c64 fsub, 0x00439c7f ret, no store) and of its 25 exe call sites several use it unrounded (0x0041e487, 0x0044ba56, 0x0044bd97, 0x005aff45) where others store it first (0x00421ff1); this real32 return rounds it [audit: M2-R10 oracle-rung-physics]
    dist = catFsqrtD(((double)trajectory->x * trajectory->x + (double)trajectory->y * trajectory->y) +
                     (double)trajectory->z * trajectory->z);
    dist -= target->staticinfo->common.staticCollInfo.approxcollspheresize;

    if (dist > 0.0)
    {
        return (real32)dist;
    }
    else
    {
        return 0.0f;
    }
}

/*-----------------------------------------------------------------------------
    Name        : RangeToTargetGivenDist
    Description : Returns range from ship to target, taking into account collision
                  information of target.
                  Takes the additional parameter dist if it has been pre-calculated
    Inputs      : ship, target, trajectory (which is target->obj.posinfo.position - ship->obj.posinfo.position), dist
    Outputs     :
    Return      : range (in metres) to target
----------------------------------------------------------------------------*/
real32 RangeToTargetGivenDist(Ship *ship,SpaceObjRotImpTarg *target,real32 dist)
{
    // CAT-TODO(M3 oracle physics): RangeToTargetGivenDist 0x00439c80 returns dist - approxcollspheresize UNROUNDED on the FPU (0x00439c8b fsub, 0x00439ca3 ret, no store); of its 16 exe call sites GenericInterceptorAttack's WHIPSTRAFE1 compares it unrounded with bulletRange * 0.7f (0x005b2c6f) and STATE_KILL with bulletRange (0x005b3014); this real32 return rounds it [audit: M2-R10 oracle-rung-physics]
    dist -= target->staticinfo->common.staticCollInfo.approxcollspheresize;

    if (dist > 0.0f)
    {
        return dist;
    }
    else
    {
        return 0.0f;
    }
}

/*-----------------------------------------------------------------------------
    Name        : collUpdateCollRectangle
    Description : updates collision rectangle information
    Inputs      : irobj
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collUpdateCollRectangle(SpaceObjRotImp *irobj)
{
    StaticCollInfo *staticCollInfo = &irobj->obj.staticinfo->staticCollInfo;
    CollInfo *collInfo = &irobj->collInfo;
    vector upvector;
    vector rightvector;
    vector forwardvector;

    if (staticCollInfo->uplength == 0)
    {
        return;         // don't bother for all-zero rectangles
    }

    matMultiplyMatByVec(&collInfo->rectpos[0],&(irobj->rotatable.rotinfo.coordsys),&staticCollInfo->collrectoffset);

    matGetVectFromMatrixCol1(upvector,irobj->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol2(rightvector,irobj->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol3(forwardvector,irobj->rotatable.rotinfo.coordsys);

    vecMultiplyByScalar(upvector,staticCollInfo->uplength);
    vecMultiplyByScalar(rightvector,staticCollInfo->rightlength);
    vecMultiplyByScalar(forwardvector,staticCollInfo->forwardlength);

    vecAdd(collInfo->rectpos[1],collInfo->rectpos[0],rightvector);
    vecAdd(collInfo->rectpos[2],collInfo->rectpos[1],forwardvector);
    vecAdd(collInfo->rectpos[3],collInfo->rectpos[0],forwardvector);

    vecAdd(collInfo->rectpos[4],collInfo->rectpos[0],upvector);
    vecAdd(collInfo->rectpos[5],collInfo->rectpos[1],upvector);
    vecAdd(collInfo->rectpos[6],collInfo->rectpos[2],upvector);
    vecAdd(collInfo->rectpos[7],collInfo->rectpos[3],upvector);
}

#define LEFT    -1
#define MIDDLE  0
#define RIGHT   1

/*-----------------------------------------------------------------------------
    Name        : collCheckRectLine
    Description : checks for a collision between a rectangle and a line of length linelength
    Inputs      : obj1 (object to check collision with), univpoint (origin of line in universe coordinates),
                  univdir (unit vector direction of line in universe coordinates), linelength
    Outputs     : collSide
                  if hit on a side, up == 0 (TRANS_UP), down == 1, right == 2, left == 3, forward == 4, back == 5
                  otherwise collSide == -1
    Return      : -1.0 if line misses, 0.0 if line origin is inside rectangle, else +x if line collides where x is the distance
                  from the line origin to the collision point
----------------------------------------------------------------------------*/
real32 collCheckRectLine(SpaceObjRotImp *obj1,vector *univpoint,vector *univdir,real32 linelength,sdword *collSide)
{
    StaticCollInfo *staticCollInfo = &obj1->obj.staticinfo->staticCollInfo;
    vector pointInWorld;
    real32 minB[3];
    real32 maxB[3];
    real32 origin[3];
    real32 dir[3];
    sdword quadrant[3];
    real32 maxT[3];
    real32 candidatePlane[3];
    double coord;
    real32 largestMaxT;
    sdword i;
    sdword whichPlane;
    bool32 inside;

    *collSide = -1;

    // CAT (M2-R10 oracle-rung-physics): collCheckRectLine 0x00439ef0 returns -1.0 (a miss) for an object without
    //      rectangle information (0x00439f08 .. 0x00439f18; HW1 returned 0.0, a hit)
    if (staticCollInfo->uplength == 0)
    {
        return -1.0f;
    }

    // axis aligned bounding box in object co-ordinates
    minB[0] = staticCollInfo->collrectoffset.x;
    minB[1] = staticCollInfo->collrectoffset.y;
    minB[2] = staticCollInfo->collrectoffset.z;
    maxB[0] = minB[0] + staticCollInfo->uplength;
    maxB[1] = minB[1] + staticCollInfo->rightlength;
    maxB[2] = minB[2] + staticCollInfo->forwardlength;

    // convert univpoint,univdir from universe to object co-ordinates

    vecSub(pointInWorld,*univpoint,obj1->obj.posinfo.position);
    matMultiplyVecByMat((vector *)origin,&pointInWorld,&obj1->rotatable.rotinfo.coordsys);

    // now we have minB, maxB, origin, dir in object co-ordinates
    // use a ray-box intersection algorithm as found in Graphics Gems I

    inside = TRUE;

    for (i=0;i<3;i++)
    {
        if (origin[i] < minB[i])
        {
            quadrant[i] = LEFT;
            candidatePlane[i] = minB[i];
            inside = FALSE;
        }
        else if (origin[i] > maxB[i])
        {
            quadrant[i] = RIGHT;
            candidatePlane[i] = maxB[i];
            inside = FALSE;
        }
        else
        {
            quadrant[i] = MIDDLE;
        }
    }

    if (inside)
    {
        return 0.0f;
    }

    // CAT: no HW1 early miss for a line pointing away from the object's centre (pointInWorld . univdir > 0): the exe
    //      goes straight on to the planes (0x00439ffd)
    matMultiplyVecByMat((vector *)dir,univdir,&obj1->rotatable.rotinfo.coordsys);

    // calculate distances to candidate planes
    for (i=0;i<3;i++)
    {
        if ((quadrant[i] != MIDDLE) && (dir[i] != 0.0f))
        {
            // CAT: the difference stays on the FPU, one rounding at the store (0x0043a030 .. 0x0043a03c)
            maxT[i] = (real32)(((double)candidatePlane[i] - (double)origin[i]) / (double)dir[i]);
        }
        else
        {
            maxT[i] = -1.0f;
        }
    }

    // find largest maxT for final choice of intersection
    whichPlane = 0;
    for (i=1;i<3;i++)
    {
        if (maxT[i] > maxT[whichPlane])
        {
            whichPlane = i;
        }
    }

    largestMaxT = maxT[whichPlane];
    if (largestMaxT < 0.0f)
    {
        return -1.0f;
    }

    if (largestMaxT > linelength)
    {
        return -1.0f;
    }

    // Check final candidate if it is actually inside box
    for (i=0;i<3;i++)
    {
        if (whichPlane == i)
        {
            continue;
        }

        // CAT: largestMaxT * dir + origin unrounded on the FPU, compared with both bounds (0x0043a0b9 .. 0x0043a0d9)
        coord = (double)largestMaxT * (double)dir[i] + (double)origin[i];

        if ((coord <= minB[i]) || (coord >= maxB[i]))
        {
            return -1.0f;
        }
    }

    // Set collSide baesd on where the plane intersected the box:
    // up == 0 (TRANS_UP), down == 1, right == 2, left == 3, forward == 4, back == 5
    *collSide = (whichPlane<<1) + ((quadrant[whichPlane] == LEFT) ? 1 : 0);
    dbgAssertOrIgnore((*collSide >= 0) && (*collSide < NUM_TRANS_DEGOFFREEDOM));
    return largestMaxT;
}

/*-----------------------------------------------------------------------------
    Name        : collCheckRectPoint
    Description : checks if collision between rectangle of obj1 and point (if point is inside obj1 rectangle)
    Inputs      : obj1, point
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 collCheckRectPoint(SpaceObjRotImp *obj1,vector *point)
{
    vector rect0;
    vector rect6;

    vector upvector;
    vector rightvector;
    vector forwardvector;

    vector rect0topoint;
    vector rect6topoint;

    if (obj1->obj.staticinfo->staticCollInfo.uplength == 0)
    {
        return TRUE;            // if no rectangle information present, assume collision
    }

    vecAdd(rect0,obj1->collInfo.rectpos[0],obj1->obj.posinfo.position);
    vecAdd(rect6,obj1->collInfo.rectpos[6],obj1->obj.posinfo.position);

    matGetVectFromMatrixCol1(upvector,obj1->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol2(rightvector,obj1->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol3(forwardvector,obj1->rotatable.rotinfo.coordsys);

    vecSub(rect0topoint,*point,rect0);
    vecSub(rect6topoint,*point,rect6);

    // reject if point is forward of forward face
    if (vecDotProduct(forwardvector,rect6topoint) > 0.0f)
    {
        return FALSE;
    }

    // reject if point is behind behind face
    if (vecDotProduct(forwardvector,rect0topoint) < 0.0f)
    {
        return FALSE;
    }

    // reject if point is right of right face
    if (vecDotProduct(rightvector,rect6topoint) > 0.0f)
    {
        return FALSE;
    }

    // reject if point is left of left face
    if (vecDotProduct(rightvector,rect0topoint) < 0.0f)
    {
        return FALSE;
    }

    // reject if point is above up face
    if (vecDotProduct(upvector,rect6topoint) > 0.0f)
    {
        return FALSE;
    }

    // reject if point is below bottom face
    if (vecDotProduct(upvector,rect0topoint) < 0.0f)
    {
        return FALSE;
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : collCheckRectInRect
    Description : checks if rectangle obj1 is in obj2
    Inputs      : obj1, obj2
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 collCheckRectInRect(SpaceObjRotImp *obj1,SpaceObjRotImp *obj2)
{
    vector rect0;
    vector rect6;

    vector upvector;
    vector rightvector;
    vector forwardvector;

    vector rect0topoint;
    vector rect6topoint;

    vector obj2point;

    udword i;
    bool32 pointinside = FALSE;

    matGetVectFromMatrixCol1(upvector,obj1->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol2(rightvector,obj1->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol3(forwardvector,obj1->rotatable.rotinfo.coordsys);

    vecAdd(rect0,obj1->collInfo.rectpos[0],obj1->obj.posinfo.position);
    vecAdd(rect6,obj1->collInfo.rectpos[6],obj1->obj.posinfo.position);

    for (i=0;i<8;i++)
    {
        vecAdd(obj2point,obj2->collInfo.rectpos[i],obj2->obj.posinfo.position);

        vecSub(rect0topoint,obj2point,rect0);
        vecSub(rect6topoint,obj2point,rect6);

        // reject if point is forward of forward face
        if (vecDotProduct(forwardvector,rect6topoint) > (real32)1e-6)
        {
            continue;
        }

        // reject if point is behind behind face
        if (vecDotProduct(forwardvector,rect0topoint) < (real32)-1e-6)
        {
            continue;
        }

        // reject if point is right of right face
        if (vecDotProduct(rightvector,rect6topoint) > (real32)1e-6)
        {
            continue;
        }

        // reject if point is left of left face
        if (vecDotProduct(rightvector,rect0topoint) < (real32)-1e-6)
        {
            continue;
        }

        // reject if point is above up face
        if (vecDotProduct(upvector,rect6topoint) > (real32)1e-6)
        {
            continue;
        }

        // reject if point is below bottom face
        if (vecDotProduct(upvector,rect0topoint) < (real32)-1e-6)
        {
            continue;
        }

        pointinside = TRUE;
        break;
    }

    return pointinside;
}

/*-----------------------------------------------------------------------------
    Name        : collCheckRectBumpCollision
    Description : checks if two rectangles are colliding
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 collCheckRectBumpCollision(SpaceObjRotImp *obj1,SpaceObjRotImp *obj2)
{
    sdword collSide;
    vector sourcePosition;
    vector univdir;
    real32 linelength;
    real32 temp;

    if (obj1->obj.staticinfo->staticCollInfo.uplength == 0)
    {
        return TRUE;            // if no rectangle information present, assume collision
    }

    if (obj2->obj.staticinfo->staticCollInfo.uplength == 0)
    {
        return TRUE;            // if no rectangle information present, assume collision
    }

    if (collCheckRectInRect(obj1,obj2) || collCheckRectInRect(obj2,obj1))
    {
        return TRUE;
    }

    // now check case if both rectangles are colliding, but points are not inside each other
    vecAdd(sourcePosition,obj1->collInfo.rectpos[0],obj1->obj.posinfo.position);

    vecSub(univdir,obj1->collInfo.rectpos[6],obj1->collInfo.rectpos[0]);

    linelength = obj1->obj.staticinfo->staticCollInfo.diagonallength;
    vecDivideByScalar(univdir,linelength,temp);

    if (collCheckRectLine(obj2,&sourcePosition,&univdir,linelength,&collSide) >= 0.0f)
    {
        return TRUE;
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : collZeroRectInfo
    Description : zero's out all rectangular information so it's not used
    Inputs      : irobj
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collZeroRectInfo(StaticCollInfo *staticCollInfo)
{
    staticCollInfo->collrectoffset.x = 0.0f;
    staticCollInfo->collrectoffset.y = 0.0f;
    staticCollInfo->collrectoffset.z = 0.0f;

    staticCollInfo->uplength = 0.0f;
    staticCollInfo->rightlength = 0.0f;
    staticCollInfo->forwardlength = 0.0f;
}

/*-----------------------------------------------------------------------------
    Name        : collDrawCollisionInfo
    Description : draws all collision info for debugging purposes
    Inputs      : irobj
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collDrawCollisionInfo(SpaceObjRotImp *irobj)
{
    vector position;
    vector univrectpos[8];
    udword i;

    if (irobj->obj.staticinfo->staticCollInfo.uplength == 0)
    {
        return;         // don't bother for all-zero rectangles
    }

    position = irobj->obj.posinfo.position;

    for (i=0;i<8;i++)
    {
        vecAdd(univrectpos[i],position,irobj->collInfo.rectpos[i]);
    }

    rndLightingEnable(FALSE);
    rndTextureEnable(FALSE);

    primLine3(&univrectpos[0],&univrectpos[1],colRGB(255,255,255));
    primLine3(&univrectpos[1],&univrectpos[2],colRGB(255,0,0));
    primLine3(&univrectpos[2],&univrectpos[3],colRGB(0,255,0));
    primLine3(&univrectpos[3],&univrectpos[0],colRGB(0,0,255));

    primLine3(&univrectpos[4],&univrectpos[5],colRGB(200,200,200));
    primLine3(&univrectpos[5],&univrectpos[6],colRGB(255,255,0));
    primLine3(&univrectpos[6],&univrectpos[7],colRGB(0,255,255));
    primLine3(&univrectpos[7],&univrectpos[4],colRGB(255,0,255));

    primLine3(&univrectpos[0],&univrectpos[4],colRGB(50,50,50));
    primLine3(&univrectpos[1],&univrectpos[5],colRGB(50,50,50));
    primLine3(&univrectpos[2],&univrectpos[6],colRGB(50,50,50));
    primLine3(&univrectpos[3],&univrectpos[7],colRGB(50,50,50));

    rndLightingEnable(TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : collUpdateCollBlobs
    Description : Updates collision blobs
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collUpdateCollBlobs(void)
{
    if (universe.collUpdateAllBlobs)
    {
        universe.collUpdateAllBlobs = FALSE;
        bobListDelete(&universe.collBlobList);
    }
    bobListCreate(&collBlobProperties, &universe.collBlobList, 0);
}


/*-----------------------------------------------------------------------------
    Name        : collUpdateObjsInCollBlobs
    Description : Updates objects in collision blobs
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collUpdateObjsInCollBlobs(void)
{
    Node *blobnode = universe.collBlobList.head;
    blob *thisBlob;

    while (blobnode != NULL)
    {
        thisBlob = (blob *)listGetStructOfNode(blobnode);

        bobUpdateObjsInBlobCollInfo(thisBlob);

        blobnode = blobnode->next;
    }
}

// CAT (M2-R4 combat): are a and b one and the same, or clamped to each other or to the same host?
//      (cat_shipsClampedTogether 0x005bfbd0)
static bool32 cat_shipsClampedTogether(SpaceObjRotImpTargGuidance *a, SpaceObjRotImpTargGuidance *b)
{
    bool32 together = FALSE;

    if (a == b)
    {
        return TRUE;
    }
    if ((a == NULL) || (b == NULL) || (a->header.objtype != OBJ_ShipType) || (b->header.objtype != OBJ_ShipType) ||
        (a->clampInfo == NULL) || (b->clampInfo == NULL))
    {
        return FALSE;
    }
    if ((a->clampInfo->host != NULL) && (a->clampInfo->host == b->clampInfo->host))
    {
        together = TRUE;
    }
    if ((SpaceObjRotImpTargGuidance *)a->clampInfo->host == b)
    {
        together = TRUE;
    }
    if ((SpaceObjRotImpTargGuidance *)b->clampInfo->host == a)
    {
        return TRUE;
    }
    return together;
}

// CAT (M2-R4 combat): cat_shipGetRamDamageScale 0x005b9980 lives in Ships/Mimic.c [M2-R4 ships-b]

/*-----------------------------------------------------------------------------
    Name        : cat_kamikazeCheckShip
    Description : CAT: does ship, a kamikaze, hit other? Fills in the damage to itself (selfDamage) and to the other
                  (otherDamage), and how and by whom the other dies (cat_kamikazeCheckShip 0x0043a980). A kamikaze
                  hits its attack target, or a ship clamped with it, or any enemy ship that is not a mimic. A Beast
                  cruise missile infects an infectable ship (Mission 7's prison ships take beastAttacksRequired hits
                  and a tenth of their health per hit); everything else takes the type's kamikaze damage. The kamikaze
                  itself takes 9999 x its max health and stops.
    Inputs      : ship, other
    Outputs     : selfDamage, otherDamage, otherHow, otherKillerIndex
    Return      : TRUE if ship hits other as a kamikaze
----------------------------------------------------------------------------*/
static bool32 cat_kamikazeCheckShip(Ship *ship, SpaceObjRotImpTarg *other, real32 *selfDamage, real32 *otherDamage,
                                    sdword *otherHow, sdword *otherKillerIndex)
{
    bool32 otherIsShip;

    if ((ship->header.objtype != OBJ_ShipType) || !bitTest(ship->shipOrDerelict.specialFlags, SPECIAL_Kamikaze))
    {
        return FALSE;
    }
    otherIsShip = (other->header.objtype == OBJ_ShipType);
    if (!cat_shipsClampedTogether((SpaceObjRotImpTargGuidance *)ship->attackvars.attacktarget, (SpaceObjRotImpTargGuidance *)other))
    {
        if (otherIsShip)
        {
            if (bitTest(((Ship *)other)->header.catFlags18, CAT_SHIPFLAG_Mimic))
            {
                return FALSE;
            }
            if (ship->playerowner == ((Ship *)other)->playerowner)
            {
                return FALSE;
            }
        }
    }
    // CAT: through cat_beastGetAttackStaticInfo 0x00429b70 (0x0043a9f9, 0x0043aa57)
    if (otherIsShip && (ship->shiptype == bCruiseMissile) && (cat_beastGetAttackStaticInfo((Ship *)other) != NULL) &&
        (singlePlayerGame || (cat_shipHasUpgrade(ship, 0x10000) && (ship->playerowner != ((Ship *)other)->playerowner))))
    {
        Ship *target = (Ship *)other;

        target->catKamikazeInfectionHits++;
        if (!singlePlayerGame || (target->shiptype != dPrisonShip) ||
            (cat_beastGetAttackStaticInfo(target)->attacksRequired <= target->catKamikazeInfectionHits))
        {
            cat_beastInfectShip(target, ship);
        }
        else
        {
            *otherDamage = target->targetable.health * 0.1f;
        }
    }
    else
    {
        *otherDamage = kamikazeDamage[ship->shiptype];
    }
    if (bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic))
    {
        *otherDamage = cat_shipGetRamDamageScale(ship) * *otherDamage;
    }
    *selfDamage = ship->staticinfo->health.maxhealth * 9999.0f;
    *otherHow = DEATH_Killed_By_Kamikaze;
    *otherKillerIndex = ship->playerowner->playerIndex;
    ship->obj.posinfo.velocity.x = 0.0f;
    ship->obj.posinfo.velocity.y = 0.0f;
    ship->obj.posinfo.velocity.z = 0.0f;
    return TRUE;
}

// CAT (M2-R4 combat): kamikazeCheck 0x0043ab20 asks cat_kamikazeCheckShip for both objects (one object's damage to the
//      other overwrites its own self damage) and, if either hits, damages both with GS 0 (call sites 5 and 6, each
//      the other's attacker). The four ship collision functions inline the same code.
bool32 kamikazeCheck(SpaceObjRotImpTarg *obj1, SpaceObjRotImpTarg *obj2)
{
    real32 damage1 = 0.0f, damage2 = 0.0f;
    sdword how1 = DEATH_Killed_By_Collision, how2 = DEATH_Killed_By_Collision;
    sdword killer1 = 99, killer2 = 99;
    bool32 hit1, hit2;

    hit1 = cat_kamikazeCheckShip((Ship *)obj1, obj2, &damage1, &damage2, &how2, &killer2);
    hit2 = cat_kamikazeCheckShip((Ship *)obj2, obj1, &damage2, &damage1, &how1, &killer1);
    if (!hit2 && !hit1)
    {
        return FALSE;
    }
    ApplyDamageToTarget(obj1,damage1,GS_LargeEnergyCannon,how1,killer1,(Ship *)obj2,5);
    ApplyDamageToTarget(obj2,damage2,GS_LargeEnergyCannon,how2,killer2,(Ship *)obj1,6);
    return TRUE;
}
/*-----------------------------------------------------------------------------
    Name        : cat_collLineTriangleIntersect
    Description : CAT: does the line from p0 through p1 cross the triangle a, b, c at or beyond p0? The parameter of the
                  crossing along p0->p1 goes to *t (cat_collLineTriangleIntersect 0x0043a6b0; normal may be NULL, then
                  it is computed from the corners)
    Inputs      : p0, p1, a, b, c, normal
    Outputs     : t
    Return      : TRUE if it crosses inside the triangle
----------------------------------------------------------------------------*/
bool32 cat_collLineTriangleIntersect(vector *p0, vector *p1, vector *a, vector *b, vector *c, vector *normal, real32 *t)
{
    vector planeNormal, hit, edge, edgeNormal;
    vector *corner;
    real32 denom;
    sdword i;

    if (normal == NULL)
    {
        normal = &planeNormal;
        cat_vecPlaneNormalFrom3Points(c, b, a, normal);
    }
    denom = (p0->z - p1->z) * normal->z + (p0->x - p1->x) * normal->x + (p0->y - p1->y) * normal->y;
    if ((-1e-07f < denom) && (denom < 1e-07f))
    {
        return FALSE;                                       // parallel to the plane
    }
    *t = ((p0->z - a->z) * normal->z + (p0->x - a->x) * normal->x + (p0->y - a->y) * normal->y) / denom;
    if (*t < 0.0f)
    {
        return FALSE;
    }
    hit.x = *t * (p1->x - p0->x) + p0->x;
    hit.y = *t * (p1->y - p0->y) + p0->y;
    hit.z = *t * (p1->z - p0->z) + p0->z;
    for (i = 0; i < 3; i++)
    {
        switch (i)
        {
            case 0:  corner = a; vecSub(edge, *b, *a); break;
            case 1:  corner = b; vecSub(edge, *c, *b); break;
            default: corner = c; vecSub(edge, *a, *c); break;
        }
        edgeNormal.x = edge.y * normal->z - edge.z * normal->y;
        edgeNormal.y = edge.z * normal->x - edge.x * normal->z;
        edgeNormal.z = edge.x * normal->y - edge.y * normal->x;
        vecNormalize(&edgeNormal);
        if ((corner->x - hit.x) * edgeNormal.x + (corner->y - hit.y) * edgeNormal.y + (corner->z - hit.z) * edgeNormal.z < 0.0f)
        {
            return FALSE;
        }
    }
    return TRUE;
}

// CAT: an active Sentinel's field is a fan of triangles from the Sentinel to pairs of its partners (Sentinel
//      specifics: the count at Ship+0x640, the pairs from Ship+0x644; Sentinel.c since M2-R4 ships-b)
static sdword cat_sentinelFieldTriangles(Ship *sentinel, SentinelLink **pairs)
{
    return cat_sentinelFieldLinks(sentinel, pairs);
}

/*-----------------------------------------------------------------------------
    Name        : cat_collLineSentinelFieldIntersect
    Description : CAT: the first triangle of sentinel's field the ray from start along dir crosses (its parameter in *t),
                  or -1 (cat_collLineSentinelFieldIntersect 0x0043abf0)
    Inputs      : sentinel, start, dir
    Outputs     : t
    Return      : the triangle index or -1
----------------------------------------------------------------------------*/
sdword cat_collLineSentinelFieldIntersect(Ship *sentinel, vector *start, vector *dir, real32 *t)
{
    SentinelLink *pairs;
    sdword num = cat_sentinelFieldTriangles(sentinel, &pairs);
    sdword i, nearest = -1;
    real32 nearestT = 1e21f;
    vector end;

    vecAdd(end, *start, *dir);
    for (i = 0; i < num; i++)
    {
        if (cat_collLineTriangleIntersect(start, &end, &sentinel->obj.posinfo.position, &pairs[i].a->obj.posinfo.position,
                                          &pairs[i].b->obj.posinfo.position, NULL, t) && (*t < nearestT))
        {
            nearestT = *t;
            nearest = i;
        }
    }
    if (nearest != -1)
    {
        *t = nearestT;
    }
    return nearest;
}

// CAT (M2-R4 combat): an sLeech that has a last target (cat_shipIsLeech 0x005b7fd0 and its specifics +0x48,
//      cat_leechGetSpecifics 0x005b7ff0; Leech.c since M2-R4 ships-b)
static bool32 cat_leechIsAttached(Ship *ship)
{
    return cat_shipIsLeech(ship) && (cat_leechGetSpecifics(ship)->lastTarget != NULL);
}

/*-----------------------------------------------------------------------------
    Name        : cat_collClampExempt
    Description : CAT: the clamp test collCheckShipShipColl 0x0043ade0 and collCheckBigShipSmallShipColl 0x0043b460
                  share (inlined in both): TRUE when obj1 and obj2 must not collide because of what they are
                  clamped to. A clamp info with no host is a pure host (Clamp.c) and counts as not clamped.
    Inputs      : obj1, obj2
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
static bool32 cat_collClampExempt(Ship *obj1, Ship *obj2)
{
    Ship *host1 = (obj1->guidance.clampInfo != NULL) ? (Ship *)obj1->guidance.clampInfo->host : NULL;
    Ship *host2 = (obj2->guidance.clampInfo != NULL) ? (Ship *)obj2->guidance.clampInfo->host : NULL;

    if (host1 != NULL)
    {
        // a ship clamped to a salvager does not collide with the depot the salvager tows it to
        if ((host1->header.objtype == OBJ_ShipType) && cat_shipIsSalvager(host1) &&
            (cat_salvageGetSalvagerInfo(host1)->dockwith == obj2))
        {
            return TRUE;
        }
        if (host2 == NULL)
        {
            if ((host1->shipOrDerelict.dockingship != NULL) &&
                ((host1->shipOrDerelict.dockingship == obj2) ||
                 (host1->shipOrDerelict.dockingship == obj2->shipOrDerelict.dockingship)))
            {
                return TRUE;
            }
        }
        else if (((host2->shipOrDerelict.dockingship != NULL) &&
                  (host2->shipOrDerelict.dockingship == obj1->shipOrDerelict.dockingship)) ||
                 (host2 == host1))
        {
            // docking with the same ship, or the same host
            return TRUE;
        }
    }
    if ((host2 != NULL) && (host2->header.objtype == OBJ_ShipType))
    {
        if (cat_shipIsSalvager(host2) && (cat_salvageGetSalvagerInfo(host2)->dockwith == obj1))
        {
            return TRUE;
        }
        if (host1 == NULL)
        {
            if ((host2->shipOrDerelict.dockingship != NULL) &&
                ((host2->shipOrDerelict.dockingship == obj1) ||
                 (host2->shipOrDerelict.dockingship == obj1->shipOrDerelict.dockingship)))
            {
                return TRUE;
            }
        }
        else if ((host1->shipOrDerelict.dockingship != NULL) &&
                 (host1->shipOrDerelict.dockingship == obj2->shipOrDerelict.dockingship))
        {
            return TRUE;
        }
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_collSentinelFieldExempt
    Description : CAT (M2-R5 layout-dock): cat_collSentinelFieldExempt 0x0043acc0. An sSentinel / bSentinel with its
                  field up does not collide with a ship that is not on a kamikaze run (SPECIAL_Kamikaze); a Sentinel
                  whose field is down, when the other ship is no Sentinel, does not collide with the first ship of
                  the protect order it is on (its command's COMMAND_MASK_PROTECTING selection).
    Inputs      : the two ships
    Return      : TRUE when the pair is exempt from collision
----------------------------------------------------------------------------*/
static bool32 cat_collSentinelFieldExempt(Ship *obj1, Ship *obj2)
{
    Ship *sentinel1 = NULL, *active1 = NULL;
    Ship *sentinel2 = NULL, *active2 = NULL;
    CommandToDo *command;

    if ((obj1->shiptype == sSentinel) || (obj1->shiptype == bSentinel))
    {
        sentinel1 = obj1;
        if (cat_shipIsSentinelFieldActive(obj1))
        {
            active1 = obj1;
        }
    }
    if ((obj2->shiptype == sSentinel) || (obj2->shiptype == bSentinel))
    {
        sentinel2 = obj2;
        if (cat_shipIsSentinelFieldActive(obj2))
        {
            active2 = obj2;
        }
    }
    if ((active1 != NULL) && !bitTest(obj2->shipOrDerelict.specialFlags, SPECIAL_Kamikaze))
    {
        return TRUE;
    }
    if ((active2 != NULL) && !bitTest(obj1->shipOrDerelict.specialFlags, SPECIAL_Kamikaze))
    {
        return TRUE;
    }
    if (sentinel1 == NULL)
    {
        if (sentinel2 == NULL)
        {
            return FALSE;
        }
        command = sentinel2->command;
        return ((command != NULL) && (command->ordertype.attributes & COMMAND_MASK_PROTECTING) &&
                (command->protect->numShips > 0) && (command->protect->ShipPtr[0] == obj1));
    }
    if (sentinel2 == NULL)
    {
        command = sentinel1->command;
        return ((command != NULL) && (command->ordertype.attributes & COMMAND_MASK_PROTECTING) &&
                (command->protect->numShips > 0) && (command->protect->ShipPtr[0] == obj2));
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : collCheckShipShipColl
    Description : checks collisions between ships and ships, and impacts a force
                  on each one (equal and opposite forces)
    Inputs      : thisBlob
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    CAT (M2-R6 managers, found by the dock self-test's processor leg): a ship with SOF_CatDocking (docking, attached
    or launching; Dock.c sets it once the ship is in its host's cone) takes part in no ship-ship, big-small or
    ship-resource collision, unless it is an externally built ship being built (SPECIAL_2_CatBeingBuilt): the exe's
    collCheckShipShipColl 0x0043ade0, collCheckBigShipSmallShipColl 0x0043b460 and collCheckShipResourceColl
    0x0043bac0 test both words of either object before anything else (a resource only the flag). Without it a docking
    Worker pushed the light sProcessor away from its own dock point for good.
----------------------------------------------------------------------------*/
static bool32 catCollDockingExempt(Ship *ship)
{
    return bitTest(ship->header.flags, SOF_CatDocking) && !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt);
}

void collCheckShipShipColl(blob *thisBlob,bool32 checkSmallShips)
{
    sdword obj1index = 0;
    Ship *obj1;

    sdword obj2index;
    Ship *obj2;

    real32 dist;
    real32 distsquared;
    real32 colldist;
    vector distvector;

    real32 obj1collspheresize;
    real32 maxdistCollPossible;

    real32 checkdist;

    SelectCommand *selection;
    sdword numShips;

    real32 maxShipCollSphereSize;

    if (checkSmallShips)
    {
        selection = thisBlob->blobSmallShips;
        maxShipCollSphereSize = thisBlob->blobMaxSmallShipCollSphereSize;
    }
    else
    {
        selection = thisBlob->blobBigShips;
        maxShipCollSphereSize = thisBlob->blobMaxBigShipCollSphereSize;
    }
    numShips = selection->numShips;

    while (obj1index < numShips)
    {
        obj1 = selection->ShipPtr[obj1index];
        if (catCollDockingExempt(obj1))
        {
            goto nextobj1;                                  // CAT: 0x0043ae38
        }
        obj1collspheresize = obj1->staticinfo->common.staticCollInfo.collspheresize;
        maxdistCollPossible = obj1collspheresize + maxShipCollSphereSize;

        obj2index = obj1index + 1;
        while (obj2index < numShips)
        {
            obj2 = selection->ShipPtr[obj2index];

#if COLLISION_CHECK_STATS
            shipshipwalks++;
#endif

            checkdist = obj2->obj.collOptimizeDist - obj1->obj.collOptimizeDist;

            if (checkdist > maxdistCollPossible)
            {
                goto nextobj1;
            }

            if (catCollDockingExempt(obj2))
            {
                goto nocollision;                           // CAT: 0x0043ae9d
            }

            if ((obj1->obj.posinfo.isMoving | obj2->obj.posinfo.isMoving) == FALSE)     // assume objects not moving can't collide
            {
                goto nocollision;
            }

            // CAT (M2-R8 oracle-rung-physics): a ship clamped to the other one does not collide with it
            //      (collCheckShipShipColl 0x0043aec4 .. 0x0043aef3: SOF_Clamped, the clamp info and its host, either way
            //      round), before the sphere distance and the Sentinel test
            if (bitTest(obj1->header.flags, SOF_Clamped) && (obj1->guidance.clampInfo != NULL) &&
                ((void *)obj1->guidance.clampInfo->host == (void *)obj2))
            {
                goto nocollision;
            }
            if (bitTest(obj2->header.flags, SOF_Clamped) && (obj2->guidance.clampInfo != NULL) &&
                ((void *)obj2->guidance.clampInfo->host == (void *)obj1))
            {
                goto nocollision;
            }

            colldist = obj1collspheresize + obj2->staticinfo->common.staticCollInfo.collspheresize;

            if (checkdist > colldist)
            {
                goto nocollision;
            }

            //old docking rejection code

            if ((obj1->shipOrDerelict.dockingship == obj2) || (obj2->shipOrDerelict.dockingship == obj1))
            {
                goto nocollision;       // docking ships shouldn't collide
            }

            if (obj1->shipOrDerelict.dockingship != NULL && obj2->shipOrDerelict.dockingship != NULL)
            {   //special for slaved ships in DOCKING process...silly I know
                goto nocollision;
            }

            // CAT (M2-R5 layout-dock): a Sentinel's field (cat_collSentinelFieldExempt 0x0043acc0; collCheckShipShipColl
            //      0x0043ade0 calls it here, before the distance test; collCheckBigShipSmallShipColl does not)
            if (cat_collSentinelFieldExempt(obj1, obj2))
            {
                goto nocollision;
            }

            // Check for collisions between obj1 and obj2 here:

#if COLLISION_CHECK_STATS
            shipshipchecks++;
#endif

            vecSub(distvector,obj2->impactable.collInfo.collPosition,obj1->impactable.collInfo.collPosition);

            if (!isBetweenInclusive(distvector.x,-colldist,colldist))
            {
                goto nocollision;
            }

            if (!isBetweenInclusive(distvector.y,-colldist,colldist))
            {
                goto nocollision;
            }

            if (!isBetweenInclusive(distvector.z,-colldist,colldist))
            {
                goto nocollision;
            }

            // CAT (M2-R4 combat): a salvager inside its target's cone (the Cataclysm salvager block's noAvoid) does not
            //      collide with its target (SPECIAL_CatSalvager; collCheckShipShipColl 0x0043ade0,
            //      collCheckBigShipSmallShipColl 0x0043b460)
            if ((obj1->shipOrDerelict.specialFlags & SPECIAL_CatSalvager) &&
                (cat_salvageGetSalvagerInfo(obj1)->target == obj2) && cat_salvageGetSalvagerInfo(obj1)->noAvoid) {
                goto nocollision;
            }
            if ((obj2->shipOrDerelict.specialFlags & SPECIAL_CatSalvager) &&
                (cat_salvageGetSalvagerInfo(obj2)->target == obj1) && cat_salvageGetSalvagerInfo(obj2)->noAvoid) {
                goto nocollision;
            }

            if ((obj1->header.flags|obj2->header.flags) & SOF_Hide)
            {
                goto nocollision;
            }

            // CAT (M2-R4 combat): the exe's clamp test (both functions have the same code): clamped objects whose
            //      hosts are docking with, or towing to, the other object do not collide
            if (cat_collClampExempt(obj1, obj2))
            {
                goto nocollision;
            }
            // CAT (M2-R8 oracle-rung-physics): the x87 shape (collCheckShipShipColl 0x0043b1ba .. 0x0043b252): the double
            //      (z*z + y*y) + x*x is stored as the float distsquared and compared, unrounded, with colldist squared
            //      before any square root; the distance is fsqrt of the stored float, taken after the too-close special
            //      case and the bump tests
            {
                double distsquaredd = ((double)distvector.z * distvector.z + (double)distvector.y * distvector.y) +
                                      (double)distvector.x * distvector.x;

                distsquared = (real32)distsquaredd;
                if (!(distsquaredd < (double)colldist * colldist))
                {
                    goto nocollision;
                }
            }

            {
                if (collCheckRectBumpCollision(&obj1->impactable,&obj2->impactable))
                {
                    if (distsquared < 1.0f)
                    {
                        distvector.x = 10.0f;   // special case, if objects are too close together
                        distsquared = 100.0f;
                    }

                    if (!bitTest(obj1->header.flags, SOF_NotBumpable) && !bitTest(obj2->header.flags, SOF_NotBumpable))
                    {
                        dist = (real32)catFsqrtD(distsquared);
                        if(!kamikazeCheck(&obj1->targetable, &obj2->targetable))    //return true if kamikaze collision is true
                        {
                            // CAT: a ramming frigate acts first (cat_rammingFrigateCollide 0x005c14a0: latch, release,
                            //      infect) [M2-R4 ships-b]
                            cat_rammingFrigateCollide(obj1, obj2);
                            if (obj1->playerowner == obj2->playerowner)
                            {
                                goto dontapplydamage;      // don't apply bonk damage for own ships
                            }

                            if ((obj1->header.flags & SOF_Disabled) || (obj2->header.flags & SOF_Disabled))
                            {
                                goto dontapplydamage;
                            }

                            // CAT (M2-R4 combat): a salvager does no bump damage to the ship it just let go of (the
                            //      Cataclysm salvager block's noDamageTarget; collCheckShipShipColl 0x0043ade0 tests
                            //      obj2 only when obj1 is no salvager)
                            if (cat_shipIsSalvager(obj1))
                            {
                                if (cat_salvageGetSalvagerInfo(obj1)->noDamageTarget == obj2)
                                {
                                    goto dontapplydamage;
                                }
                            }
                            else if (cat_shipIsSalvager(obj2) && (cat_salvageGetSalvagerInfo(obj2)->noDamageTarget == obj1))
                            {
                                goto dontapplydamage;
                            }

                            // CAT: a Leech that is attached does no bump damage (Leech specifics +0x48), and nothing
                            //      bumps an RKO (collCheckShipShipColl 0x0043ade0: cat_shipIsLeech, cat_leechGetSpecifics,
                            //      cat_shipIsRKO)
                            if (cat_leechIsAttached(obj1) || cat_leechIsAttached(obj2))
                            {
                                goto dontapplydamage;
                            }
                            if (cat_shipIsRKO(obj1) || cat_shipIsRKO(obj2))
                            {
                                goto dontapplydamage;
                            }

                            ApplyDamageToCollidingObjects(&obj1->targetable,&obj2->targetable,&distvector,dist);
dontapplydamage:
                            ObjectsCollided(&obj1->targetable,&obj2->targetable,colldist,&distvector,dist,distsquared);
                        }
                        else
                        {
                            //fix collided ships velocity....
                            ObjectsCollided(&obj1->targetable,&obj2->targetable,colldist,&distvector,dist,distsquared);
                        }

                    }

                }
            }
nocollision:
            obj2index++;
        }
nextobj1:
        obj1index++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : collCheckBigShipSmallShipColl
    Description : checks collisions between bigships and smallships, and impacts a force
                  on each one (equal and opposite forces)
    Inputs      : thisBlob
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collCheckBigShipSmallShipColl(blob *thisBlob)
{
    sdword obj1index = 0;
    Ship *obj1;

    sdword obj2index = 0;
    Ship *obj2;

    real32 dist;
    real32 distsquared;
    real32 colldist;
    vector distvector;

    real32 obj1collspheresize;
    real32 maxdistCollPossible;

    real32 checkdist;

    SelectCommand *selection1 = thisBlob->blobBigShips;
    sdword numShips1 = selection1->numShips;

    SelectCommand *selection2 = thisBlob->blobSmallShips;
    sdword numShips2 = selection2->numShips;

    real32 maxShipCollSphereSize = thisBlob->blobMaxSmallShipCollSphereSize;

    sdword bottom,top;

    if (numShips2 == 0)
    {
        return;
    }

    while (obj1index < numShips1)
    {
        obj1 = selection1->ShipPtr[obj1index];
        if (catCollDockingExempt(obj1))
        {
            goto nextobj1;                                  // CAT: collCheckBigShipSmallShipColl 0x0043b460
        }
        obj1collspheresize = obj1->staticinfo->common.staticCollInfo.collspheresize;
        maxdistCollPossible = obj1collspheresize + maxShipCollSphereSize;

        // binary search targetindex such that
        // (obj1->obj.collOptimizeDist - obj2->obj.collOptimizeDist) <= maxdistCollPossible
        bottom = 0;
        top = numShips2-1;
        while (top >= bottom)
        {
            obj2index = (bottom+top)>>1;
            if ((obj1->obj.collOptimizeDist - selection2->ShipPtr[obj2index]->obj.collOptimizeDist) > maxdistCollPossible)
            {
                // targetindex is too small
                bottom = obj2index+1;
            }
            else
            {
                top = obj2index-1;
            }
        }

        dbgAssertOrIgnore(obj2index >= 0);
        dbgAssertOrIgnore(obj2index < numShips2);

        while (obj2index < numShips2)
        {
            obj2 = selection2->ShipPtr[obj2index];

#if COLLISION_CHECK_STATS
            shipshipwalks++;
#endif

            checkdist = obj2->obj.collOptimizeDist - obj1->obj.collOptimizeDist;

            if (checkdist > maxdistCollPossible)
            {
                goto nextobj1;
            }

            if (catCollDockingExempt(obj2))
            {
                goto nocollision;
            }

            if ((obj1->obj.posinfo.isMoving | obj2->obj.posinfo.isMoving) == FALSE)     // assume objects not moving can't collide
            {
                goto nocollision;
            }

            // CAT (M2-R9 oracle-rung-physics): a ship clamped to the other one does not collide with it
            //      (collCheckBigShipSmallShipColl 0x0043b578 .. 0x0043b5a7: SOF_Clamped, the clamp info and its host,
            //      either way round, as collCheckShipShipColl 0x0043aec4): a mothership and the module it is building
            if (bitTest(obj1->header.flags, SOF_Clamped) && (obj1->guidance.clampInfo != NULL) &&
                ((void *)obj1->guidance.clampInfo->host == (void *)obj2))
            {
                goto nocollision;
            }
            if (bitTest(obj2->header.flags, SOF_Clamped) && (obj2->guidance.clampInfo != NULL) &&
                ((void *)obj2->guidance.clampInfo->host == (void *)obj1))
            {
                goto nocollision;
            }

            colldist = obj1collspheresize + obj2->staticinfo->common.staticCollInfo.collspheresize;

            if (checkdist > colldist)
            {
                goto nocollision;
            }

            //old docking rejection code

            if ((obj1->shipOrDerelict.dockingship == obj2) || (obj2->shipOrDerelict.dockingship == obj1))
            {
                goto nocollision;       // docking ships shouldn't collide
            }

            if (obj1->shipOrDerelict.dockingship != NULL && obj2->shipOrDerelict.dockingship != NULL)
            {   //special for slaved ships in DOCKING process...silly I know
                goto nocollision;
            }

            // Check for collisions between obj1 and obj2 here:

#if COLLISION_CHECK_STATS
            shipshipchecks++;
#endif

            vecSub(distvector,obj2->impactable.collInfo.collPosition,obj1->impactable.collInfo.collPosition);

            if (!isBetweenInclusive(distvector.x,-colldist,colldist))
            {
                goto nocollision;
            }

            if (!isBetweenInclusive(distvector.y,-colldist,colldist))
            {
                goto nocollision;
            }

            if (!isBetweenInclusive(distvector.z,-colldist,colldist))
            {
                goto nocollision;
            }

            // CAT (M2-R4 combat): a salvager inside its target's cone (the Cataclysm salvager block's noAvoid) does not
            //      collide with its target (SPECIAL_CatSalvager; collCheckShipShipColl 0x0043ade0,
            //      collCheckBigShipSmallShipColl 0x0043b460)
            if ((obj1->shipOrDerelict.specialFlags & SPECIAL_CatSalvager) &&
                (cat_salvageGetSalvagerInfo(obj1)->target == obj2) && cat_salvageGetSalvagerInfo(obj1)->noAvoid) {
                goto nocollision;
            }
            if ((obj2->shipOrDerelict.specialFlags & SPECIAL_CatSalvager) &&
                (cat_salvageGetSalvagerInfo(obj2)->target == obj1) && cat_salvageGetSalvagerInfo(obj2)->noAvoid) {
                goto nocollision;
            }

            if ((obj1->header.flags|obj2->header.flags) & SOF_Hide)
            {
                goto nocollision;
            }

            // CAT (M2-R4 combat): the exe's clamp test (both functions have the same code): clamped objects whose
            //      hosts are docking with, or towing to, the other object do not collide
            if (cat_collClampExempt(obj1, obj2))
            {
                goto nocollision;
            }
            // CAT (M2-R9 oracle-rung-physics): the exe's distance test (collCheckBigShipSmallShipColl 0x0043b83f ..
            //      0x0043b8d7, as collCheckShipShipColl's): (z*z + y*y) + x*x is stored as the float distsquared and
            //      compared, unrounded, with colldist squared before any root; the distance is fsqrt of the stored
            //      float, taken after the too-close special case and the bump tests
            {
                double distsquaredd = ((double)distvector.z * distvector.z + (double)distvector.y * distvector.y) +
                                      (double)distvector.x * distvector.x;

                distsquared = (real32)distsquaredd;
                if (!(distsquaredd < (double)colldist * colldist))
                {
                    goto nocollision;
                }
            }

            {
                if (collCheckRectBumpCollision(&obj1->impactable,&obj2->impactable))
                {
                    if (distsquared < 1.0f)
                    {
                        distvector.x = 10.0f;   // special case, if objects are too close together
                        distsquared = 100.0f;
                    }

                    if (!bitTest(obj1->header.flags, SOF_NotBumpable) && !bitTest(obj2->header.flags, SOF_NotBumpable))
                    {
                        dist = (real32)catFsqrtD(distsquared);
                        if(!kamikazeCheck(&obj1->targetable,&obj2->targetable))    //return true if kamikaze collision is true
                        {
                            // CAT: a ramming frigate acts first (cat_rammingFrigateCollide 0x005c14a0: latch, release,
                            //      infect) [M2-R4 ships-b]
                            cat_rammingFrigateCollide(obj1, obj2);
                            if (obj1->playerowner == obj2->playerowner)
                            {
                                goto dontapplydamage;
                            }

                            if ((obj1->header.flags & SOF_Disabled) || (obj2->header.flags & SOF_Disabled))
                            {
                                goto dontapplydamage;
                            }

                            // CAT (M2-R4 combat): a salvager does no bump damage to the ship it just let go of (the
                            //      Cataclysm salvager block's noDamageTarget; collCheckBigShipSmallShipColl 0x0043b460
                            //      tests both ships)
                            if ((cat_shipIsSalvager(obj1) && (cat_salvageGetSalvagerInfo(obj1)->noDamageTarget == obj2)) ||
                                (cat_shipIsSalvager(obj2) && (cat_salvageGetSalvagerInfo(obj2)->noDamageTarget == obj1)))
                            {
                                goto dontapplydamage;
                            }

                            // CAT: a Leech that is attached does no bump damage (collCheckBigShipSmallShipColl 0x0043b460)
                            if (cat_leechIsAttached(obj1) || cat_leechIsAttached(obj2))
                            {
                                goto dontapplydamage;
                            }

                            ApplyDamageToCollidingObjects(&obj1->targetable,&obj2->targetable,&distvector,dist);
dontapplydamage:
                            ObjectsCollided(&obj1->targetable,&obj2->targetable,colldist,&distvector,dist,distsquared);
                        }
                        else
                        {
                            //fix collided ships velocity....
                            ObjectsCollided(&obj1->targetable,&obj2->targetable,colldist,&distvector,dist,distsquared);
                        }
                    }

                }
            }
nocollision:
            obj2index++;
        }
nextobj1:
        obj1index++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : collCheckShipResourceColl
    Description : checks collisions between ships and resources
    Inputs      : thisBlob
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collCheckShipResourceColl(blob *thisBlob)
{
    sdword obj1index = 0;
    Ship *obj1;

    sdword obj2index = 0;
    Resource *obj2;

    real32 dist;
    real32 distsquared;
    real32 colldist;
    vector distvector;

    real32 obj1collspheresize;
    real32 maxdistCollPossible;

    real32 distcheck;
    real32 absdistcheck;

    SelectCommand *selection = thisBlob->blobShips;
    sdword numShips = selection->numShips;

    ResourceSelection *resselection = thisBlob->blobResources;
    sdword numResources = resselection->numResources;

    real32 maxResourceCollSphereSize;

    sdword bottom,top;

    if (numResources == 0)
    {
        return;
    }

    maxResourceCollSphereSize = thisBlob->blobMaxResourceCollSphereSize;

    while (obj1index < numShips)
    {
        obj1 = selection->ShipPtr[obj1index];
        if (catCollDockingExempt(obj1))
        {
            goto nextobj1;                                  // CAT: collCheckShipResourceColl 0x0043bac0
        }
        obj1collspheresize = obj1->staticinfo->common.staticCollInfo.collspheresize;
        maxdistCollPossible = obj1collspheresize + maxResourceCollSphereSize;

        // binary search targetindex such that
        // (obj1->obj.collOptimizeDist - obj2->obj.collOptimizeDist) <= maxdistCollPossible
        bottom = 0;
        top = numResources-1;
        while (top >= bottom)
        {
            obj2index = (bottom+top)>>1;
            if ((obj1->obj.collOptimizeDist - resselection->ResourcePtr[obj2index]->obj.collOptimizeDist) > maxdistCollPossible)
            {
                // targetindex is too small
                bottom = obj2index+1;
            }
            else
            {
                top = obj2index-1;
            }
        }

        dbgAssertOrIgnore(obj2index >= 0);
        dbgAssertOrIgnore(obj2index < numResources);

        while (obj2index < numResources)
        {
            obj2 = resselection->ResourcePtr[obj2index];

#if COLLISION_CHECK_STATS
            shipresourcewalks++;
#endif
            if ((distcheck = obj2->obj.collOptimizeDist - obj1->obj.collOptimizeDist) > maxdistCollPossible)
            {
                goto nextobj1;
            }

            if (bitTest(obj2->header.flags, SOF_CatDocking))
            {
                goto nocollision;                           // CAT: the resource's flag alone
            }

            if ((absdistcheck = ABS(distcheck)) > maxdistCollPossible)
            {
                goto nocollision;
            }

            if ((obj1->obj.posinfo.isMoving | obj2->obj.posinfo.isMoving) == FALSE)     // assume objects not moving can't collide
            {
                goto nocollision;
            }

            colldist = obj1collspheresize + obj2->staticinfo->common.staticCollInfo.collspheresize;

            if (absdistcheck > colldist)
            {
                goto nocollision;
            }

            // Check for collisions between obj1 and obj2 here:

#if COLLISION_CHECK_STATS
            shipresourcechecks++;
#endif

            vecSub(distvector,obj2->impactable.collInfo.collPosition,obj1->impactable.collInfo.collPosition);

            if (!isBetweenInclusive(distvector.x,-colldist,colldist))
            {
                goto nocollision;
            }

            if (!isBetweenInclusive(distvector.y,-colldist,colldist))
            {
                goto nocollision;
            }

            if (!isBetweenInclusive(distvector.z,-colldist,colldist))
            {
                goto nocollision;
            }

            distsquared = vecMagnitudeSquared(distvector);
            dist = (real32)fsqrt(distsquared);

            if (dist < colldist)
            {
                if (collCheckRectBumpCollision(&obj1->impactable,&obj2->impactable))
                {
                    if (distsquared < 1.0f)
                    {
                        distvector.x = 10.0f;   // special case, if objects are too close together
                        distsquared = 100.0f;
                        dist = 10.0f;
                    }

                    if (!bitTest(obj1->header.flags, SOF_NotBumpable) && !bitTest(obj2->header.flags, SOF_NotBumpable))
                    {
                        if(!kamikazeCheck(&obj1->targetable,&obj2->targetable))    //return true if kamikaze collision is true
                        {
                            // CAT: collCheckShipResourceColl 0x0043bac0: every resource-class ship (not HW1's
                            //      ResourceCollector type) has the special armour, except against a killer resource;
                            //      a collision with a killer resource does not bounce
                            if ((obj1->staticinfo->shipclass != CLASS_Resource) ||
                                ((obj2->obj.attributes | obj1->obj.attributes) & CAT_ATTRIBUTES_KillerCollision))
                            {
                                ApplyDamageToCollidingObjects(&obj1->targetable,&obj2->targetable,&distvector,dist);
                            }

                            if (!((obj2->obj.attributes | obj1->obj.attributes) & CAT_ATTRIBUTES_KillerCollision))
                            {
                                ObjectsCollided(&obj1->targetable,&obj2->targetable,colldist,&distvector,dist,distsquared);
                            }
                        }
                        else
                        {
                             //fix collided ships velocity....
                            ObjectsCollided(&obj1->targetable,&obj2->targetable,colldist,&distvector,dist,distsquared);
                        }
                    }
                }
            }
nocollision:
            obj2index++;
        }
nextobj1:
        obj1index++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : collCheckShipDerelictColl
    Description : checks collisions between ships and derelicts
    Inputs      : thisBlob
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collCheckShipDerelictColl(blob *thisBlob)
{
    sdword obj1index = 0;
    Ship *obj1       = NULL;

    sdword obj2index = 0;
    Derelict *obj2   = NULL;

    real32 dist                = 0.0;
    real32 distsquared         = 0.0;
    real32 colldist            = 0.0;
    real32 obj1collspheresize  = 0.0;
    real32 maxdistCollPossible = 0.0;
    real32 distcheck           = 0.0;
    real32 absdistcheck        = 0.0;

    vector distvector;


    SelectCommand *selection = thisBlob->blobShips;
    sdword numShips = selection->numShips;

    DerelictSelection *derselection = thisBlob->blobDerelicts;
    sdword numDerelicts = derselection->numDerelicts;

    real32 maxDerelictCollSphereSize;

    sdword bottom,top;

    if (numDerelicts == 0)
    {
        return;
    }

    maxDerelictCollSphereSize = thisBlob->blobMaxDerelictCollSphereSize;

    while (obj1index < numShips)
    {
        obj1 = selection->ShipPtr[obj1index];
        obj1collspheresize = obj1->staticinfo->common.staticCollInfo.collspheresize;
        maxdistCollPossible = obj1collspheresize + maxDerelictCollSphereSize;

        // binary search targetindex such that
        // (obj1->obj.collOptimizeDist - obj2->obj.collOptimizeDist) <= maxdistCollPossible
        bottom = 0;
        top = numDerelicts-1;
        while (top >= bottom)
        {
            obj2index = (bottom+top)>>1;
            if ((obj1->obj.collOptimizeDist - derselection->DerelictPtr[obj2index]->obj.collOptimizeDist) > maxdistCollPossible)
            {
                // targetindex is too small
                bottom = obj2index+1;
            }
            else
            {
                top = obj2index-1;
            }
        }

        dbgAssertOrIgnore(obj2index >= 0);
        dbgAssertOrIgnore(obj2index < numDerelicts);

        while (obj2index < numDerelicts)
        {
            obj2 = derselection->DerelictPtr[obj2index];

#if COLLISION_CHECK_STATS
            shipderelictwalks++;
#endif
            if ((distcheck = obj2->obj.collOptimizeDist - obj1->obj.collOptimizeDist) > maxdistCollPossible)
            {
                goto nextobj1;
            }

            if ((absdistcheck = ABS(distcheck)) > maxdistCollPossible)
            {
                goto nocollision;
            }

            if ((obj1->obj.posinfo.isMoving | obj2->obj.posinfo.isMoving) == FALSE)     // assume objects not moving can't collide
            {
                goto nocollision;
            }

            // CAT (M2-R10 tests-leftovers review): collCheckShipDerelictColl 0x0043bed0 skips a ship clamped to the
            //      derelict and a derelict clamped to the ship (0x0043bfcc .. 0x0043bff3: SOF_Clamped and clampInfo's host);
            //      CAT-FIX: the clampInfo != NULL tests are the tree's guard, the exe reads clampInfo->host unguarded
            //      (0x0043bfd8 / 0x0043bfeb) [audit: M2-R10 tests-leftovers: the guard was unmarked]
            if (bitTest(obj1->header.flags, SOF_Clamped) && (((SpaceObjRotImpTargGuidance *)obj1)->clampInfo != NULL) &&
                ((Derelict *)((SpaceObjRotImpTargGuidance *)obj1)->clampInfo->host == obj2))
            {
                goto nocollision;
            }
            if (bitTest(obj2->header.flags, SOF_Clamped) && (((SpaceObjRotImpTargGuidance *)obj2)->clampInfo != NULL) &&
                ((Ship *)((SpaceObjRotImpTargGuidance *)obj2)->clampInfo->host == obj1))
            {
                goto nocollision;
            }

            colldist = obj1collspheresize + obj2->staticinfo->common.staticCollInfo.collspheresize;

            if (absdistcheck > colldist)
            {
                goto nocollision;
            }

            if(obj2->shipOrDerelict.dockingship == obj1)
            {    //derelict is 'docking' with obj1 so don't do collision detection
                goto nocollision;
            }
            // Check for collisions between obj1 and obj2 here:

#if COLLISION_CHECK_STATS
            shipderelictchecks++;
#endif

            vecSub(distvector,obj2->impactable.collInfo.collPosition,obj1->impactable.collInfo.collPosition);

            if (!isBetweenInclusive(distvector.x,-colldist,colldist))
            {
                goto nocollision;
            }

            if (!isBetweenInclusive(distvector.y,-colldist,colldist))
            {
                goto nocollision;
            }

            if (!isBetweenInclusive(distvector.z,-colldist,colldist))
            {
                goto nocollision;
            }

            // CAT (M2-R4 combat): Cataclysm salvages no derelicts: a salvager whose target is this derelict only gets a
            //      debug print ("SALVAGING: Salvager target derelict.  This shouldn't be happening.") and collides
            //      (collCheckShipDerelictColl 0x0043bed0)

            if ((obj1->header.flags|obj2->header.flags) & SOF_Hide)
            {
                goto nocollision;
            }

            // CAT (M2-R10 tests-leftovers review): the squared distance (dz*dz + dy*dy) + dx*dx on the FPU, compared
            //      unrounded with colldist * colldist (0x0043c114 .. 0x0043c145) and kept as a float; the square root
            //      only for the collision itself (fsqrt of the double, 0x0043c197) (HW1: fsqrt first, dist < colldist)
            {
                double distsquaredD = ((double)distvector.z * (double)distvector.z + (double)distvector.y * (double)distvector.y) +
                                      (double)distvector.x * (double)distvector.x;

                distsquared = (real32)distsquaredD;
                if (!(distsquaredD < (double)colldist * (double)colldist))
                {
                    goto nocollision;
                }
            }

            {
                if (collCheckRectBumpCollision(&obj1->impactable,&obj2->impactable))
                {
                    if (distsquared < 1.0f)
                    {
                        distvector.x = 10.0f;   // special case, if objects are too close together
                        distsquared = 100.0f;
                    }

                    if (!bitTest(obj1->header.flags, SOF_NotBumpable) && !bitTest(obj2->header.flags, SOF_NotBumpable))
                    {
                        dist = (real32)catFsqrtD(distsquared);
                        if(!kamikazeCheck(&obj1->targetable,&obj2->targetable))    //return true if kamikaze collision is true
                        {
                            // deliberately do not apply damage for ship-derelict collisions
                            ObjectsCollided(&obj1->targetable,&obj2->targetable,colldist,&distvector,dist,distsquared);
                        }
                        else
                        {
                             //fix collided ships velocity....
                            ObjectsCollided(&obj1->targetable,&obj2->targetable,colldist,&distvector,dist,distsquared);
                        }
                    }
                }
            }
nocollision:
            obj2index++;
        }
nextobj1:
        obj1index++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : collCheckLineOfSight
    Description :
    Inputs      : source - ship that's firing
                  target - potential destination (target)
                  sourcePosition - origin of the los test in worldspace
                  direction - worldspace unit direction
    Outputs     :
    Return      : TRUE if path is clear, FALSE if obscured
    todo: must check if target is closer than a "hit"
----------------------------------------------------------------------------*/
// CAT (M2-R10 oracle-rung-physics): collCheckLineOfSight 0x0043c2e0. In the source's own blob (the target in it, or no
//      target) the exe also skips every ship clamped to the source or to the target (a mothership's modules, which
//      share its position and hull: 0x0043c333 .. 0x0043c367) and, when the target is itself a ship clamped to a host,
//      every ship clamped to that host and the host itself; the other blobs' loop has no such skip. The x87 shape: the
//      length is fsqrt of (dz*dz + dy*dy) + dx*dx over the double differences, stored as a float (0x0043c3dc ..
//      0x0043c412); a blob is crossed when v = (dz*dir.z + dx*dir.x) + dy*dir.y and r*r - (((dz*dz + dy*dy) + dx*dx) -
//      v*v) are both above 0, all on the FPU (0x0043c43d .. 0x0043c4af). Without the skip a sMothership's gimbal guns
//      saw its own modules in the way and never fired (10-18 tick 1721: guns 3 and 6 at the bRecon 41)
static bool32 cat_collLosSkipClamped(Ship *ship, Ship *source, Ship *target)
{
    SpaceObjRotImpTarg *host, *targethost;

    if ((ship->header.objtype != OBJ_ShipType) || (ship->guidance.clampInfo == NULL))
    {
        return FALSE;
    }
    host = ship->guidance.clampInfo->host;
    if ((host == (SpaceObjRotImpTarg *)source) || (host == (SpaceObjRotImpTarg *)target))
    {
        return TRUE;
    }
    // CAT-FIX: the exe reads the target unguarded here (0x0043c34e) and would fault on a NULL one; gunShoot, missileShoot
    //      and the ramming frigate pass one [audit: M2-R10 oracle-rung-physics: marked CAT-FIX]
    if ((target != NULL) && (target->header.objtype == OBJ_ShipType) && (target->guidance.clampInfo != NULL))
    {
        targethost = target->guidance.clampInfo->host;
        if ((host == targethost) || ((SpaceObjRotImpTarg *)ship == targethost))
        {
            return TRUE;
        }
    }
    return FALSE;
}

bool32 collCheckLineOfSight(Ship* source, Ship* target, vector* sourcePosition, vector* direction)
{
    sdword i, collSide;
    real32 length;
    blob* thisBlob;
    Ship* ship;

    if (target == NULL)
    {
        length = REALlyBig;
    }
    else
    {
        double dx = (double)target->obj.posinfo.position.x - (double)sourcePosition->x;
        double dy = (double)target->obj.posinfo.position.y - (double)sourcePosition->y;
        double dz = (double)target->obj.posinfo.position.z - (double)sourcePosition->z;

        length = (real32)catFsqrtD((dz * dz + dy * dy) + dx * dx);
    }

    if (target == NULL || target->obj.collMyBlob == source->obj.collMyBlob)
    {
        //los in current blob
        thisBlob = source->obj.collMyBlob;
        for (i = 0; i < thisBlob->blobShips->numShips; i++)
        {
            ship = thisBlob->blobShips->ShipPtr[i];
            if (ship == source || ship == target)
            {
                //ignore source or target
                continue;
            }
            if (cat_collLosSkipClamped(ship, source, target))
            {
                continue;
            }

            if (!allianceIsShipAlly(ship, source->playerowner))
            {
                //only test with allied ships, self included
                continue;
            }

            if (collCheckRectLine((SpaceObjRotImp*)ship, sourcePosition, direction, length, &collSide) != -1.0f)
            {
                //hit something of ours
                return FALSE;
            }
        }

        //path is clear
        return TRUE;
    }
    else
    {
        //los in universe
        Node* blobnode = universe.collBlobList.head;
        double dx, dy, dz, v, r;

        while (blobnode != NULL)
        {
            thisBlob = (blob*)listGetStructOfNode(blobnode);
            blobnode = blobnode->next;

            dx = (double)thisBlob->centre.x - (double)sourcePosition->x;
            dy = (double)thisBlob->centre.y - (double)sourcePosition->y;
            dz = (double)thisBlob->centre.z - (double)sourcePosition->z;
            v = (dz * direction->z + dx * direction->x) + dy * direction->y;
            r = thisBlob->radius;
            if ((v > 0.0) && ((r * r - (((dz * dz + dy * dy) + dx * dx) - v * v)) > 0.0))
            {
                //line intersects blob, check all ships
                for (i = 0; i < thisBlob->blobShips->numShips; i++)
                {
                    ship = thisBlob->blobShips->ShipPtr[i];
                    if (ship == source || ship == target)
                    {
                        //ignore source or target
                        continue;
                    }

                    if (!allianceIsShipAlly(ship, source->playerowner))
                    {
                        //only test with allied ships, self included
                        continue;
                    }

                    if (collCheckRectLine((SpaceObjRotImp*)ship, sourcePosition, direction, length, &collSide) != -1.0f)
                    {
                        //hit something of ours
                        return FALSE;
                    }
                }
            }
        }

        //path is clear
        return TRUE;
    }
}

// CAT (M2-R6 nis-fow-camera): the exe's sixth argument (collCheckBeamCollWithTargetsInBlob 0x0043c570): the triangle of a
//      Sentinel field the beam stops at (-1: none). A beam of another player that crosses an sSentinel's / bSentinel's
//      active field (cat_collLineSentinelFieldIntersect 0x0043abf0) nearer than the nearest hit so far stops there
//      (side 0, the triangle index out) and the Sentinel is not tested further; every other hit sets the index to -1.
//      The exe's seventh argument is the beam length, its caller's third (collCheckBeamColl 0x0043c900 is passed
//      bullet->traveldist, Bullet+0x100, by collCheckBulletTargetColl 0x0043d2e5), so the tree reads bullet->traveldist.
//      The length test is the exe's v*v - dsqr < length*length (0x0043c6eb; HW1 tested v - sqrt(dsqr) < length).
void collCheckBeamCollWithTargetsInBlob(Bullet *bullet,blob *thisBlob,real32 *minbeamCollideLineDist,SpaceObjRotImpTarg **minbeamTarget,sdword *minbeamCollSide,sdword *minbeamFieldTriangle)
{
    sdword fieldTriangle;
    real32 fieldDist;
    SelectAnyCommand *targetselection;
    sdword numTargets;

    SpaceObjRotImpTarg *target;
    StaticInfo *targetstaticheader;

    real32 distsquared;
    vector distvector;
    real32 v,d,dsqr;
    sdword collSide;

    real32 collideLineDist;

    sdword targetindex;

    sdword pass = 0;

passagain:

    if (pass == 0)
    {
        targetselection = thisBlob->blobSmallTargets;
    }
    else
    {
        dbgAssertOrIgnore(pass == 1);
        targetselection = thisBlob->blobBigTargets;
    }

    targetindex = 0;
    numTargets = targetselection->numTargets;

    while (targetindex < numTargets)
    {
        target = targetselection->TargetPtr[targetindex];
        targetstaticheader = &target->staticinfo->common;

#if COLLISION_CHECK_STATS
bulletwalks++;
#endif

        if (bullet->owner == (Ship *)target)
        {
            goto beamnexttarget;
        }

        // CAT: an active Sentinel field of another player (0x0043c5c7..0x0043c642)
        if ((target->header.objtype == OBJ_ShipType) &&
            ((((Ship *)target)->shiptype == sSentinel) || (((Ship *)target)->shiptype == bSentinel)) &&
            cat_shipIsSentinelFieldActive((Ship *)target) && (bullet->playerowner != ((Ship *)target)->playerowner) &&
            ((fieldTriangle = cat_collLineSentinelFieldIntersect((Ship *)target, &bullet->obj.posinfo.position,
                                                                 &bullet->bulletheading, &fieldDist)) != -1) &&
            (fieldDist < *minbeamCollideLineDist))
        {
            *minbeamCollideLineDist = fieldDist;
            *minbeamTarget = target;
            *minbeamCollSide = 0;
            *minbeamFieldTriangle = fieldTriangle;
            goto beamnexttarget;
        }

        // Check for collisions between bullet and target here:

#if COLLISION_CHECK_STATS
bulletchecks++;
#endif

        vecSub(distvector,target->impactable.collInfo.collPosition,bullet->obj.posinfo.position);
        distsquared = vecMagnitudeSquared(distvector);
        v = vecDotProduct(distvector,bullet->bulletheading);
		if (v > 0)
        {
			dsqr = targetstaticheader->staticCollInfo.collspheresizeSqr - (distsquared - (v*v));
			if (dsqr > 0)
			{
				if ((double)v * (double)v - (double)dsqr < (double)bullet->traveldist * (double)bullet->traveldist)    // CAT: the exe's x87 test (HW1: v - sqrt(dsqr) < traveldist)
				{
					if ((collideLineDist = collCheckRectLine((SpaceObjRotImp *)target,&bullet->obj.posinfo.position,&bullet->bulletheading,bullet->traveldist,&collSide)) >= 0.0f)
					{
						d = fsqrt(dsqr);
						if (collideLineDist == 0.0f)
						{
							collideLineDist = v-d;
							if (collideLineDist < 0.0f)
							{
								collideLineDist = 0.0f;
							}
						}
						if (collideLineDist < *minbeamCollideLineDist)
						{
							// CAT (M2-R5 render): a ship with bUseRayTriangleIntersectionForHitLocation stops the beam only
							//      where the line meets its mesh, and only if that is still the nearest (0x0043c7a7..0x0043c81e)
							if (cat_collUsesMeshHit((SpaceObj *)target, catCollMeshHitBullets) && (collideLineDist > 0.0f))
							{
								if (cat_collMeshHitDistance((Ship *)target, &bullet->obj.posinfo.position, &bullet->bulletheading,
															&collideLineDist) &&
									(collideLineDist < *minbeamCollideLineDist))
								{
									*minbeamCollideLineDist = collideLineDist;
									*minbeamTarget = target;
									*minbeamCollSide = collSide;
									*minbeamFieldTriangle = -1;
								}
							}
							else
							{
								*minbeamCollideLineDist = collideLineDist;
								*minbeamTarget = target;
								*minbeamCollSide = collSide;
								*minbeamFieldTriangle = -1;
							}
						}
					}
				}
			}
		}

beamnexttarget:
        targetindex++;
    }

//nextpass:
    pass++;
    if (pass < 2)
    {
        goto passagain;
    }
}

void collCheckBeamColl(blob *thisBlob,Bullet *bullet)
{
    real32 minbeamCollideLineDist = REALlyBig;
    SpaceObjRotImpTarg *minbeamTarget = NULL;
    sdword minbeamCollSide = -1;
    sdword minbeamFieldTriangle = -1;       // CAT: the Sentinel-field triangle the beam stops at (M2-R6 nis-fow-camera)
    Node *blobnode = universe.collBlobList.head;
    blob *checkBlob;

    real32 collideLineDist;
    real32 distsquared;
    vector distvector;
    real32 v,d,dsqr;
    sdword collSide;

    if(bullet->target != NULL)
    {
        switch(bullet->target->header.objtype)
        {
        case OBJ_MissileType:
            //do bullet/missle target collision with its target only!
            vecSub(distvector,bullet->target->impactable.collInfo.collPosition,bullet->obj.posinfo.position);
            distsquared = vecMagnitudeSquared(distvector);
            v = vecDotProduct(distvector,bullet->bulletheading);
			//if (v > 0)
			{
				dsqr = bullet->target->staticinfo->common.staticCollInfo.collspheresizeSqr - (distsquared - (v*v));
				if (dsqr > 0)
				{
					// CAT: the exe's length test (collCheckBeamColl 0x0043c900 at 0x0043c9e4; HW1: v - sqrt(dsqr) < traveldist)
					if ((double)v * (double)v - (double)dsqr < (double)bullet->traveldist * (double)bullet->traveldist)
					{
						if ((collideLineDist = collCheckRectLine((SpaceObjRotImp *)bullet->target,&bullet->obj.posinfo.position,&bullet->bulletheading,bullet->traveldist,&collSide)) >= 0.0f)
						{
							d = fsqrt(dsqr);
							if (collideLineDist == 0.0f)
							{
								collideLineDist = v-d;
								if (collideLineDist < 0.0f)
								{
									collideLineDist = 0.0f;
								}
							}
							if (collideLineDist < minbeamCollideLineDist)
							{
								minbeamCollideLineDist = collideLineDist;
								minbeamTarget = bullet->target;
								minbeamCollSide = collSide;
							}
						}
					}
				}
			}
            break;
        default:
            break;
        }
    }

    collCheckBeamCollWithTargetsInBlob(bullet,thisBlob,&minbeamCollideLineDist,&minbeamTarget,&minbeamCollSide,&minbeamFieldTriangle);

    // do line-blob collision test for all other blobs in universe:
    while (blobnode != NULL)
    {
        checkBlob = (blob *)listGetStructOfNode(blobnode);
        blobnode = blobnode->next;

        if (checkBlob == thisBlob)
        {
            continue;
        }

        vecSub(distvector,checkBlob->centre,bullet->obj.posinfo.position);
        distsquared = vecMagnitudeSquared(distvector);
        v = vecDotProduct(distvector,bullet->bulletheading);
		if (v > 0)
		{
			dsqr = (checkBlob->radius*checkBlob->radius) - (distsquared - (v*v));
			if (dsqr > 0)
			{
				// CAT: the exe's length test (0x0043cb4a; it computes and drops the square root)
				if ((double)v * (double)v - (double)dsqr < (double)bullet->traveldist * (double)bullet->traveldist)
				{
					collCheckBeamCollWithTargetsInBlob(bullet,checkBlob,&minbeamCollideLineDist,&minbeamTarget,&minbeamCollSide,&minbeamFieldTriangle);
				}
			}
		}
    }

    if (minbeamTarget != NULL)        // beam only collides with closest target
    {
        bool32 applyDamage = TRUE;

        dbgAssertOrIgnore(minbeamCollideLineDist >= 0.0f);

        // CAT: collCheckBeamColl 0x0043c900: a beam that hits an sSentinel / bSentinel with its field up only draws
        //      the hit (univBulletCollidedWithTarget's first argument 0). The exe also pushes a seventh value (1, 2, 3
        //      per call) that univBulletCollidedWithTarget 0x00546a20 never reads.
        // CAT (M2-R6 nis-fow-camera): a beam stopped at a field triangle hits the field with the triangle as the
        //      (unused) second argument (0x0043cbb5)
        if (minbeamFieldTriangle != -1)
        {
            cat_sentinelFieldBulletHit((Ship *)minbeamTarget, (void *)(smemsize)minbeamFieldTriangle, bullet);
            applyDamage = FALSE;
        }
        else if ((minbeamTarget->header.objtype == OBJ_ShipType) &&
            ((((Ship *)minbeamTarget)->shiptype == sSentinel) || (((Ship *)minbeamTarget)->shiptype == bSentinel)) &&
            cat_shipIsSentinelFieldActive((Ship *)minbeamTarget))
        {
            cat_sentinelFieldBulletHit((Ship *)minbeamTarget, NULL, bullet);     // CAT: 0x005c6c60 (M2-R4 ships-b)
            applyDamage = FALSE;
        }

        // don't apply beam damage every tick, correct with update rate factor
        if ((universe.univUpdateCounter % (UNIVERSE_UPDATE_RATE_FACTOR)) == 0) {
            univBulletCollidedWithTarget(applyDamage, minbeamTarget,&minbeamTarget->staticinfo->common,bullet,minbeamCollideLineDist,minbeamCollSide);
        }
        bullet->beamtraveldist = minbeamCollideLineDist;
    }
    else
    {
        bullet->beamtraveldist = bullet->lengthmag;

        // CAT: collCheckBeamColl 0x0043c900: a Beast beam (GS_BeastWeapon) that hits nothing else still shows its hit
        //      on its own target when its line crosses the target's box: no damage (first argument 0), the beam keeps
        //      its full length
        if ((bullet->soundType == GS_BeastWeapon) && (bullet->target != NULL))
        {
            if ((collideLineDist = collCheckRectLine((SpaceObjRotImp *)bullet->target,&bullet->obj.posinfo.position,&bullet->bulletheading,bullet->traveldist,&collSide)) >= 0.0f)
            {
                univBulletCollidedWithTarget(FALSE, bullet->target,&bullet->target->staticinfo->common,bullet,collideLineDist,collSide);
            }
        }
    }
    //adjust length of effect associated with bullet, if applicable
    if (bullet->effect != NULL)
    {
        ((real32 *)bullet->effect->variable)[SCALESHIFT(ETG_LengthVariable)] = bullet->beamtraveldist;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_collBulletDFGFieldCheck
    Description : CAT: a bDFGFrigate / tDFGFrigate target whose field radius holds the bullet acts on it
                  (cat_collBulletDFGFieldCheck 0x0043cd10, univDFGFieldEffect)
    Inputs      : bullet, target
    Return      : TRUE when the field destroyed the bullet (univDFGFieldEffect 0x005b0940 returns 1 only after it
                  detonated a big-gun shell)
----------------------------------------------------------------------------*/
static bool32 cat_collBulletDFGFieldCheck(Bullet *bullet, SpaceObjRotImpTarg *target)
{
    vector diff;
    Ship *ship;

    if (target->header.objtype != OBJ_ShipType)
    {
        return FALSE;
    }
    ship = (Ship *)target;
    if ((ship->shiptype != bDFGFrigate) && (ship->shiptype != tDFGFrigate))
    {
        return FALSE;
    }
    vecSub(diff, target->impactable.collInfo.collPosition, bullet->obj.posinfo.position);
    if (vecMagnitudeSquared(diff) < ((DFGFrigateStatics *)ship->staticinfo->custstatinfo)->DFGFrigateFieldRadiusSqr)
    {
        return univDFGFieldEffect(ship, bullet, universe.totaltimeelapsed);
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : collCheckBulletTargetColl
    Description : checks collisions between bullets and targets.  When a bullet
                  collides with a target, it should disappear and do damage to
                  the enemy ship.
    Inputs      :
    Outputs     :
    Return      :
    Notes       : This routine uses line-sphere intersection for collision detection
----------------------------------------------------------------------------*/
void collCheckBulletTargetColl(blob *thisBlob)
{
    sdword bulletindex = 0;
    Bullet *bullet;

    sdword targetindex = 0;
    SpaceObjRotImpTarg *target;
    StaticInfo *targetstaticheader;

    sdword collSide;

    real32 distcheck;
    double distcheckD, dx, dy, dz, vD, dsqrD;
    real32 vsq;

    bool32 deletebulletflag;

    real32 collideLineDist;

    real32 testcollsizesqr;

    BulletSelection *bulletselection = thisBlob->blobBullets;

    SelectAnyCommand *targetselection;
    sdword numTargets;

    sdword bottom,top;

    real32 maxTargetCollSphereSizePlusBulletTravelDist;

    sdword pass;

#ifdef HW_BUILD_FOR_DEBUGGING
//    thisBlob->debugFlag = 1;
#endif

    while (bulletindex < bulletselection->numBullets)
    {
        bullet = bulletselection->BulletPtr[bulletindex];
        dbgAssertOrIgnore(bullet->header.objtype == OBJ_BulletType);

        deletebulletflag = FALSE;

        // CAT: by weapon (collCheckBulletTargetColl 0x0043cd90 switches on Bullet +0x108): the ion beam and the Beast
        //      weapon collide as beams, the laser does not collide at all
        switch (bullet->bulletType)
        {
            case WEAPON_ION:
            case WEAPON_BEASTINFECT:
                collCheckBeamColl(thisBlob,bullet);
                break;
            case WEAPON_LASER:
                break;              //don't do collision detection for lasers
            default:
                pass = 0;


                if(bullet->target != NULL)
                {
                    switch(bullet->target->header.objtype)
                    {
                    case OBJ_MissileType:
                        //do bullet/missle target collision with its target only!
                        // CAT (M2-R10 oracle-rung-physics): the x87 shape (collCheckBulletTargetColl 0x0043ce04 ..
                        //      0x0043ceca): the |difference| and the sphere size plus the travel distance stay on the FPU;
                        //      v = (dz*hz + dy*hy) + dx*hx over the double differences, v*v stored as a float, dsqr =
                        //      size^2 - (((dz*dz + dy*dy) + dx*dx) - vsq) on the FPU, and the line is tested when vsq -
                        //      dsqr < traveldist^2 (no square root: HW1 tested v - sqrt(dsqr) < traveldist)
                        distcheckD = (double)bullet->target->obj.collOptimizeDist - (double)bullet->obj.collOptimizeDist;

                        if (ABS(distcheckD) > ((double)bullet->target->staticinfo->common.staticCollInfo.collspheresize+(double)bullet->traveldist))
                        {
                            //trivially reject...
                            goto nominecollision;
                        }
                        dx = (double)bullet->target->impactable.collInfo.collPosition.x - (double)bullet->obj.posinfo.position.x;
                        dy = (double)bullet->target->impactable.collInfo.collPosition.y - (double)bullet->obj.posinfo.position.y;
                        dz = (double)bullet->target->impactable.collInfo.collPosition.z - (double)bullet->obj.posinfo.position.z;
                        vD = (dz * bullet->bulletheading.z + dy * bullet->bulletheading.y) + dx * bullet->bulletheading.x;
                        vsq = (real32)(vD * vD);
						//if (v > 0)
						{

							testcollsizesqr = bullet->target->staticinfo->common.staticCollInfo.collspheresizeSqr;

							dsqrD = testcollsizesqr - ((((dz * dz + dy * dy) + dx * dx)) - vsq);
							if (dsqrD > 0.0)
							{
								if (((double)vsq - dsqrD) < (double)bullet->traveldist * bullet->traveldist)
								{
									if ((collideLineDist = collCheckRectLine((SpaceObjRotImp *)bullet->target,&bullet->obj.posinfo.position,&bullet->bulletheading,bullet->traveldist,&collSide)) >= 0.0f)
									{
										univBulletCollidedWithTarget(TRUE, bullet->target,&bullet->target->staticinfo->common,bullet,collideLineDist,collSide);   // CAT: 1 (collCheckBulletTargetColl 0x0043cd90)
										deletebulletflag = TRUE;
										goto nextbullet;
									}
								}
							}
						}

                        break;
                    default:
                        break;
                    }
                }
nominecollision:
passagain:
                if (pass == 0)
                {
                    targetselection = thisBlob->blobSmallTargets;
                    numTargets = targetselection->numTargets;

                    if (numTargets == 0)
                    {
                        goto nextpass;         // go directly to next pass
                    }

                    maxTargetCollSphereSizePlusBulletTravelDist = thisBlob->blobMaxSTargetCollSphereSize + bullet->traveldist;

                    // binary search targetindex such that
                    // (bullet->obj.collOptimizeDist - target->obj.collOptimizeDist) <= maxTargetCollSphereSize
                    // CAT: the difference compared unrounded (0x0043cf6f .. 0x0043cf82)
                    bottom = 0;
                    top = numTargets-1;
                    while (top >= bottom)
                    {
                        targetindex = (bottom+top)>>1;
                        if (((double)bullet->obj.collOptimizeDist - (double)targetselection->TargetPtr[targetindex]->obj.collOptimizeDist) > maxTargetCollSphereSizePlusBulletTravelDist)
                        {
                            // targetindex is too small
                            bottom = targetindex+1;
                        }
                        else
                        {
                            top = targetindex-1;
                        }
                    }

                    dbgAssertOrIgnore(targetindex >= 0);
                    dbgAssertOrIgnore(targetindex < numTargets);
                }
                else
                {
                    dbgAssertOrIgnore(pass == 1);
                    targetselection = thisBlob->blobBigTargets;
                    numTargets = targetselection->numTargets;
                    targetindex = 0;
                    maxTargetCollSphereSizePlusBulletTravelDist = thisBlob->blobMaxBTargetCollSphereSize + bullet->traveldist;
                }

                while (targetindex < numTargets)
                {
                    target = targetselection->TargetPtr[targetindex];
                    targetstaticheader = &target->staticinfo->common;

#if COLLISION_CHECK_STATS
            bulletwalks++;
#endif

                    // CAT: the difference is stored as a float and compared unrounded (0x0043cfd7 .. 0x0043cff1)
                    distcheckD = (double)target->obj.collOptimizeDist - (double)bullet->obj.collOptimizeDist;
                    distcheck = (real32)distcheckD;

                    if (distcheckD > maxTargetCollSphereSizePlusBulletTravelDist)
                    {
                        goto nextpass;
                    }

                    // CAT: an enemy bullet whose line crosses an active Sentinel field within this frame's travel hits the
                    //      field (collCheckBulletTargetColl 0x0043cd90): only the hit is drawn (first argument 0) and the
                    //      bullet is deleted (cat_shipIsSentinelFieldActive, Sentinel.c).
                    if ((target->header.objtype == OBJ_ShipType) &&
                        ((((Ship *)target)->shiptype == sSentinel) || (((Ship *)target)->shiptype == bSentinel)) &&
                        cat_shipIsSentinelFieldActive((Ship *)target) &&
                        (bullet->playerowner != ((Ship *)target)->playerowner))
                    {
                        real32 fieldDist;

                        if ((cat_collLineSentinelFieldIntersect((Ship *)target, &bullet->obj.posinfo.position, &bullet->bulletheading, &fieldDist) != -1) &&
                            (fieldDist < bullet->traveldist))
                        {
                            // CAT: cat_sentinelFieldBulletHit 0x005c6c60 first (M2-R4 ships-b); the exe passes the
                            //      triangle as its unused second argument. univBulletCollidedWithTarget gets the side
                            //      its last collCheckRectLine left behind; -1 here.
                            cat_sentinelFieldBulletHit((Ship *)target, NULL, bullet);
                            univBulletCollidedWithTarget(FALSE, target, targetstaticheader, bullet, fieldDist, -1);
                            deletebulletflag = TRUE;
                            goto nextbullet;
                        }
                    }

                    // CAT: no collision-sphere exemption for any type (HW1 exempted the DFG frigate and the defense
                    //      fighter): collCheckBulletTargetColl 0x0043cd90 tests every target against its sphere first
                    // CAT: the stored |difference| against the sphere size plus the travel distance on the FPU (0x0043d05e ..
                    //      0x0043d08a)
                    if (ABS(distcheck) > ((double)targetstaticheader->staticCollInfo.collspheresize+(double)bullet->traveldist))
                    {
                        goto nexttarget;
                    }

                    if (bullet->owner == (Ship *)target)
                    {
                        goto nexttarget;
                    }


                    if ((target->header.objtype == OBJ_DustType) || (target->header.objtype == OBJ_NebulaType))
                    {
                        goto nexttarget;
                    }

                    // CAT: a DFG frigate's field acts on the bullet here (cat_collBulletDFGFieldCheck 0x0043cd10); a bullet
                    //      the field destroyed is done with for this frame
                    if (cat_collBulletDFGFieldCheck(bullet, target))
                    {
                        goto nextbullet;
                    }

                    // Check for collisions between bullet and target here:

#if COLLISION_CHECK_STATS
            bulletchecks++;
#endif

                    // CAT (M2-R10 oracle-rung-physics): the x87 shape (collCheckBulletTargetColl 0x0043d0c0 .. 0x0043d15b):
                    //      v = (dx*hx + dy*hy) + dz*hz over the double differences, v*v stored as a float, dsqr = size^2 -
                    //      (((dz*dz + dy*dy) + dx*dx) - vsq) on the FPU, and the line is tested when vsq - dsqr <
                    //      traveldist^2: no square root (HW1 tested v - sqrt(dsqr) < traveldist, which let a bullet hit a
                    //      tick early: 10-17 tick 2043, the bRecon 42's bullet at the sWorker 28)
                    dx = (double)target->impactable.collInfo.collPosition.x - (double)bullet->obj.posinfo.position.x;
                    dy = (double)target->impactable.collInfo.collPosition.y - (double)bullet->obj.posinfo.position.y;
                    dz = (double)target->impactable.collInfo.collPosition.z - (double)bullet->obj.posinfo.position.z;
                    vD = (dx * bullet->bulletheading.x + dy * bullet->bulletheading.y) + dz * bullet->bulletheading.z;
                    vsq = (real32)(vD * vD);

                    // CAT: HW1's DFGFrigate test is cat_collBulletDFGFieldCheck above; HW1's DefenseFighter bullet report
                    //      is gone with the type (collCheckBulletTargetColl 0x0043cd90)

					//if (v > 0)
					{

						testcollsizesqr = targetstaticheader->staticCollInfo.collspheresizeSqr;

						dsqrD = testcollsizesqr - ((((dz * dz + dy * dy) + dx * dx)) - vsq);
						if (dsqrD > 0.0)
						{
							if (((double)vsq - dsqrD) < (double)bullet->traveldist * bullet->traveldist)
							{
								if ((collideLineDist = collCheckRectLine((SpaceObjRotImp *)target,&bullet->obj.posinfo.position,&bullet->bulletheading,bullet->traveldist,&collSide)) >= 0.0f)
								{
									// CAT (M2-R5 render): a ship with bUseRayTriangleIntersectionForHitLocation is hit only where the
									//      line meets its mesh, and the hit moves there (0x0043d1a5..0x0043d219)
									if (cat_collUsesMeshHit((SpaceObj *)target, catCollMeshHitBullets) && (collideLineDist > 0.0f) &&
										!cat_collMeshHitDistance((Ship *)target, &bullet->obj.posinfo.position, &bullet->bulletheading,
																 &collideLineDist))
									{
										goto nexttarget;
									}
									univBulletCollidedWithTarget(TRUE, target,targetstaticheader,bullet,collideLineDist,collSide);   // CAT: 1 (collCheckBulletTargetColl 0x0043cd90)
									deletebulletflag = TRUE;
									goto nextbullet;
								}
							}
						}
					}

        nexttarget:
                    targetindex++;
                }

nextpass:
                pass++;
                if (pass < 2)
                {
                    goto passagain;
                }
                break;
        }

nextbullet:
        if (deletebulletflag)
        {
            // we are deleting this bullet's index, so don't update bulletindex
            // CAT: no DefenseFighterBulletRemoval (SpecialEffectFlag 0x2), Cataclysm has no defense fighter
            //      (M2-R4 ships-b retire)

            bobObjectDied((SpaceObj *)bullet,&universe.collBlobList);
            listRemoveNode(&bullet->bulletlink);
            univRemoveObjFromRenderList((SpaceObj *)bullet);
            listDeleteNode(&bullet->header.objlink);
        }
        else
        {
            bulletindex++;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : collCheckMissileShipColl
    Description : checks collisions between missiles and ships
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collCheckMissileShipColl(blob *thisBlob,MissileType missileType)
{
    sdword missileindex = 0;
    Missile *missile;

    sdword targetindex = 0;
    Ship *target;
    StaticInfo *targetstaticheader;

    real32 distsquared;
    vector distvector;
    real32 v,d,dsqr;
    sdword collSide;

    real32 distcheck;

    real32 collideLineDist;

    vector missileheading;

    MissileSelection *missileselection;
    sdword numMissiles;

    SelectCommand *targetselection;
    sdword numTargets;

    sdword bottom,top;

    real32 maxCollSphereSizePlusMissileTravelDist;

    sdword pass;

    if (missileType == MISSILE_Mine)
    {
        missileselection = thisBlob->blobMissileMines;
    }
    else
    {
        missileselection = thisBlob->blobMissileMissiles;
    }
    numMissiles = missileselection->numMissiles;

#ifdef HW_BUILD_FOR_DEBUGGING
//    thisBlob->debugFlag = 1;
#endif

    while (missileindex < numMissiles)
    {
        missile = missileselection->MissilePtr[missileindex];
        dbgAssertOrIgnore(missile->header.objtype == OBJ_MissileType);

        if (missile->header.flags & SOF_Dead)
        {
            goto nextmissile;
        }

        pass = 0;

passagain:

        if (pass == 0)
        {
            targetselection = thisBlob->blobSmallShips;
            numTargets = targetselection->numShips;

            if (numTargets == 0)
            {
                goto nextpass;
            }

            maxCollSphereSizePlusMissileTravelDist = thisBlob->blobMaxSmallShipCollSphereSize+missile->maxtraveldist;

            // binary search targetindex such that
            // (missile->obj.collOptimizeDist - target->obj.collOptimizeDist) <= maxCollSphereSize
            bottom = 0;
            top = numTargets-1;
            while (top >= bottom)
            {
                targetindex = (bottom+top)>>1;
                if ((missile->obj.collOptimizeDist - targetselection->ShipPtr[targetindex]->obj.collOptimizeDist) > maxCollSphereSizePlusMissileTravelDist)
                {
                    // targetindex is too small
                    bottom = targetindex+1;
                }
                else
                {
                    top = targetindex-1;
                }
            }

            dbgAssertOrIgnore(targetindex >= 0);
            dbgAssertOrIgnore(targetindex < numTargets);
        }
        else
        {
            dbgAssertOrIgnore(pass == 1);
            targetselection = thisBlob->blobBigShips;
            numTargets = targetselection->numShips;
            maxCollSphereSizePlusMissileTravelDist = thisBlob->blobMaxBigShipCollSphereSize+missile->maxtraveldist;
            targetindex = 0;
        }

        while (targetindex < numTargets)
        {
            target = targetselection->ShipPtr[targetindex];

#if COLLISION_CHECK_STATS
    missilewalks++;
#endif

            distcheck = target->obj.collOptimizeDist - missile->obj.collOptimizeDist;

            if (distcheck > maxCollSphereSizePlusMissileTravelDist)
            {
                goto nextpass;
            }

            targetstaticheader = &target->staticinfo->common;

            if (ABS(distcheck) > (targetstaticheader->staticCollInfo.collspheresize+missile->maxtraveldist))
            {
                goto nexttarget;
            }

            if (missile->owner == (Ship *)target)
            {
                goto nexttarget;
            }

            // CAT (M2-R4 ships-b): an enemy missile whose path (its position along its velocity) crosses an active
            //      Sentinel field hits the field and is deleted (collCheckMissileShipColl 0x0043d330:
            //      cat_collLineTriangleIntersect per triangle, then cat_sentinelFieldMissileHit)
            if (((target->shiptype == sSentinel) || (target->shiptype == bSentinel)) && cat_shipIsSentinelFieldActive(target) &&
                (missile->playerowner != target->playerowner))
            {
                SentinelLink *pairs;
                sdword numPairs = cat_sentinelFieldLinks(target, &pairs);
                sdword pair;
                vector pathEnd;
                real32 pathT;

                vecAdd(pathEnd, missile->obj.posinfo.position, missile->obj.posinfo.velocity);
                for (pair = 0; pair < numPairs; pair++)
                {
                    if (cat_collLineTriangleIntersect(&missile->obj.posinfo.position, &pathEnd, &target->obj.posinfo.position,
                                                      &pairs[pair].a->obj.posinfo.position,
                                                      &pairs[pair].b->obj.posinfo.position, NULL, &pathT))
                    {
                        cat_sentinelFieldMissileHit(target, (void *)(smemsize)pair, missile);
                        AddMissileToDeleteMissileList(missile, -1);
                        goto nextmissile;
                    }
                }
            }

            // Check for collisions between missile and target here:

#if COLLISION_CHECK_STATS
    missilechecks++;
#endif
            vecSub(distvector,target->impactable.collInfo.collPosition,missile->obj.posinfo.position);
            distsquared = vecMagnitudeSquared(distvector);
            matGetVectFromMatrixCol3(missileheading,missile->rotatable.rotinfo.coordsys);
            v = vecDotProduct(distvector,missileheading);
			//if (v > 0)
			{
				dsqr = targetstaticheader->staticCollInfo.collspheresizeSqr - (distsquared - (v*v));
				if (dsqr > 0)
				{
					d = fsqrt(dsqr);
					if ((v-d) < missile->maxtraveldist)
					{
						if ((collideLineDist = collCheckRectLine((SpaceObjRotImp *)target,&missile->obj.posinfo.position,&missileheading,missile->maxtraveldist,&collSide)) >= 0.0f)
						{
							// CAT (M2-R5 render): a ship with bUseRayTriangleIntersectionForHitLocation is hit only if the line
							//      from the missile along its velocity meets its mesh (0x0043d604..0x0043d685)
							if (cat_collUsesMeshHit((SpaceObj *)target, catCollMeshHitMissiles))
							{
								vector pathEnd, meshHit;

								vecAdd(pathEnd, missile->obj.posinfo.position, missile->obj.posinfo.velocity);
								if (!cat_meshLineIntersect((Ship *)target, catCollMeshHitMissileLevel,
														   &missile->obj.posinfo.position, &pathEnd, &meshHit))
								{
									goto nexttarget;
								}
							}
							univMissileCollidedWithTarget(TRUE, (SpaceObjRotImpTarg *)target,targetstaticheader,missile,collideLineDist,collSide);   // CAT: 1 (all four exe callers)
							AddMissileToDeleteMissileList(missile, -1);
							goto nextmissile;
						}
					}
				}
			}

nexttarget:
            targetindex++;
        }

nextpass:
        pass++;
        if (pass < 2)
        {
            goto passagain;
        }

nextmissile:
        missileindex++;
    }

#ifdef HW_BUILD_FOR_DEBUGGING
//    thisBlob->debugFlag = 0;
#endif

}

/*-----------------------------------------------------------------------------
    Name        : collCheckMissileResourceColl
    Description : checks collisions between missiles and resources
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collCheckMissileResourceColl(blob *thisBlob,MissileType missileType)
{
    sdword missileindex = 0;
    Missile *missile;

    sdword targetindex = 0;
    Resource *target;
    StaticInfo *targetstaticheader;

    real32 distsquared;
    vector distvector;
    real32 v,d,dsqr;
    sdword collSide;

    real32 distcheck;

    real32 collideLineDist;

    vector missileheading;

    MissileSelection *missileselection;
    sdword numMissiles;

    ResourceSelection *targetselection = thisBlob->blobResources;
    sdword numTargets = targetselection->numResources;

    sdword bottom,top;

    real32 maxCollSphereSize = thisBlob->blobMaxResourceCollSphereSize;
    real32 maxCollSphereSizePlusMissileTravelDist;

    if (numTargets == 0)
    {
        return;
    }

    if (missileType == MISSILE_Mine)
    {
        missileselection = thisBlob->blobMissileMines;
    }
    else
    {
        missileselection = thisBlob->blobMissileMissiles;
    }
    numMissiles = missileselection->numMissiles;

#ifdef HW_BUILD_FOR_DEBUGGING
//    thisBlob->debugFlag = 1;
#endif

    while (missileindex < numMissiles)
    {
        missile = missileselection->MissilePtr[missileindex];
        dbgAssertOrIgnore(missile->header.objtype == OBJ_MissileType);

        if (missile->header.flags & SOF_Dead)
        {
            goto nextmissile;
        }

        maxCollSphereSizePlusMissileTravelDist = maxCollSphereSize + missile->maxtraveldist;

        // binary search targetindex such that
        // (missile->obj.collOptimizeDist - target->obj.collOptimizeDist) <= maxCollSphereSizePlusMissileTravelDist
        bottom = 0;
        top = numTargets-1;
        while (top >= bottom)
        {
            targetindex = (bottom+top)>>1;
            if ((missile->obj.collOptimizeDist - targetselection->ResourcePtr[targetindex]->obj.collOptimizeDist) > maxCollSphereSizePlusMissileTravelDist)
            {
                // targetindex is too small
                bottom = targetindex+1;
            }
            else
            {
                top = targetindex-1;
            }
        }

        dbgAssertOrIgnore(targetindex >= 0);
        dbgAssertOrIgnore(targetindex < numTargets);

        while (targetindex < numTargets)
        {
            target = targetselection->ResourcePtr[targetindex];

#if COLLISION_CHECK_STATS
    missilewalks++;
#endif

            distcheck = target->obj.collOptimizeDist - missile->obj.collOptimizeDist;

            if (distcheck > maxCollSphereSizePlusMissileTravelDist)
            {
                goto nextmissile;
            }

            targetstaticheader = &target->staticinfo->common;

            if (ABS(distcheck) > (targetstaticheader->staticCollInfo.collspheresize+missile->maxtraveldist))
            {
                goto nexttarget;
            }

            if (target->header.flags & SOF_NotBumpable)
            {
                goto nexttarget;
            }

            // Check for collisions between missile and target here:

#if COLLISION_CHECK_STATS
    missilechecks++;
#endif
            vecSub(distvector,target->impactable.collInfo.collPosition,missile->obj.posinfo.position);
            distsquared = vecMagnitudeSquared(distvector);
            matGetVectFromMatrixCol3(missileheading,missile->rotatable.rotinfo.coordsys);
            v = vecDotProduct(distvector,missileheading);
			//if (v > 0)
			{
				dsqr = targetstaticheader->staticCollInfo.collspheresizeSqr - (distsquared - (v*v));
				if (dsqr > 0)
				{
					d = fsqrt(dsqr);
					if ((v-d) < missile->maxtraveldist)
					{
						if ((collideLineDist = collCheckRectLine((SpaceObjRotImp *)target,&missile->obj.posinfo.position,&missileheading,missile->maxtraveldist,&collSide)) >= 0.0f)
						{
							univMissileCollidedWithTarget(TRUE, (SpaceObjRotImpTarg *)target,targetstaticheader,missile,collideLineDist,collSide);   // CAT: 1 (all four exe callers)
							AddMissileToDeleteMissileList(missile, -1);
							goto nextmissile;
						}
					}
				}
			}

nexttarget:
            targetindex++;
        }

nextmissile:
        missileindex++;
    }

#ifdef HW_BUILD_FOR_DEBUGGING
//    thisBlob->debugFlag = 0;
#endif

}


/*-----------------------------------------------------------------------------
    Name        : collCheckMissileDerelictColl
    Description : checks collisions between missiles and derelicts
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collCheckMissileDerelictColl(blob *thisBlob,MissileType missileType)
{
    sdword missileindex = 0;
    Missile *missile;

    sdword targetindex = 0;
    Derelict *target;
    StaticInfo *targetstaticheader;

    real32 distsquared;
    vector distvector;
    real32 v,d,dsqr;
    sdword collSide;

    real32 distcheck;

    real32 collideLineDist;

    vector missileheading;

    MissileSelection *missileselection;
    sdword numMissiles;

    DerelictSelection *targetselection = thisBlob->blobDerelicts;
    sdword numTargets = targetselection->numDerelicts;

    real32 maxCollSphereSize = thisBlob->blobMaxDerelictCollSphereSize;
    real32 maxCollSphereSizePlusMissileTravelDist;

    sdword bottom,top;

    if (numTargets == 0)
    {
        return;
    }

    if (missileType == MISSILE_Mine)
    {
        missileselection = thisBlob->blobMissileMines;
    }
    else
    {
        missileselection = thisBlob->blobMissileMissiles;
    }
    numMissiles = missileselection->numMissiles;

#ifdef HW_BUILD_FOR_DEBUGGING
//    thisBlob->debugFlag = 1;
#endif

    while (missileindex < numMissiles)
    {
        missile = missileselection->MissilePtr[missileindex];
        dbgAssertOrIgnore(missile->header.objtype == OBJ_MissileType);

        if (missile->header.flags & SOF_Dead)
        {
            goto nextmissile;
        }

        maxCollSphereSizePlusMissileTravelDist = maxCollSphereSize + missile->maxtraveldist;

        // binary search targetindex such that
        // (missile->obj.collOptimizeDist - target->obj.collOptimizeDist) <= maxCollSphereSizePlusMissileTravelDist
        bottom = 0;
        top = numTargets-1;
        while (top >= bottom)
        {
            targetindex = (bottom+top)>>1;
            if ((missile->obj.collOptimizeDist - targetselection->DerelictPtr[targetindex]->obj.collOptimizeDist) > maxCollSphereSizePlusMissileTravelDist)
            {
                // targetindex is too small
                bottom = targetindex+1;
            }
            else
            {
                top = targetindex-1;
            }
        }

        dbgAssertOrIgnore(targetindex >= 0);
        dbgAssertOrIgnore(targetindex < numTargets);

        while (targetindex < numTargets)
        {
            target = targetselection->DerelictPtr[targetindex];

#if COLLISION_CHECK_STATS
    missilewalks++;
#endif

            distcheck = target->obj.collOptimizeDist - missile->obj.collOptimizeDist;

            if (distcheck > maxCollSphereSizePlusMissileTravelDist)
            {
                goto nextmissile;
            }

            targetstaticheader = &target->staticinfo->common;

            if (ABS(distcheck) > (targetstaticheader->staticCollInfo.collspheresize+missile->maxtraveldist))
            {
                goto nexttarget;
            }

            // Check for collisions between missile and target here:

#if COLLISION_CHECK_STATS
    missilechecks++;
#endif
            vecSub(distvector,target->impactable.collInfo.collPosition,missile->obj.posinfo.position);
            distsquared = vecMagnitudeSquared(distvector);
            matGetVectFromMatrixCol3(missileheading,missile->rotatable.rotinfo.coordsys);
            v = vecDotProduct(distvector,missileheading);
			//if (v > 0)
			{
				dsqr = targetstaticheader->staticCollInfo.collspheresizeSqr - (distsquared - (v*v));
				if (dsqr > 0)
				{
					d = fsqrt(dsqr);
					if ((v-d) < missile->maxtraveldist)
					{
						if ((collideLineDist = collCheckRectLine((SpaceObjRotImp *)target,&missile->obj.posinfo.position,&missileheading,missile->maxtraveldist,&collSide)) >= 0.0f)
						{
							univMissileCollidedWithTarget(TRUE, (SpaceObjRotImpTarg *)target,targetstaticheader,missile,collideLineDist,collSide);   // CAT: 1 (all four exe callers)
							AddMissileToDeleteMissileList(missile, -1);
							goto nextmissile;
						}
					}
				}
			}

nexttarget:
            targetindex++;
        }

nextmissile:
        missileindex++;
    }

#ifdef HW_BUILD_FOR_DEBUGGING
//    thisBlob->debugFlag = 0;
#endif

}

/*-----------------------------------------------------------------------------
    Name        : collCheckMissileMineColl
    Description : checks collisions between missiles and mines
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collCheckMissileMineColl(blob *thisBlob)
{
    sdword missileindex = 0;
    Missile *missile;

    sdword targetindex = 0;
    Missile *target;
    StaticInfo *targetstaticheader;

    real32 distsquared;
    vector distvector;
    real32 v,d,dsqr;
    sdword collSide;

    real32 distcheck;

    real32 collideLineDist;

    vector missileheading;

    MissileSelection *missileselection = thisBlob->blobMissileMissiles;
    sdword numMissiles = missileselection->numMissiles;

    MissileSelection *targetselection = thisBlob->blobMissileMines;
    sdword numTargets = targetselection->numMissiles;

    sdword bottom,top;

    real32 maxMineCollSphereSizePlusMissileTravelDist;

    if (numTargets == 0)
    {
        return;
    }

#ifdef HW_BUILD_FOR_DEBUGGING
//    thisBlob->debugFlag = 1;
#endif

    while (missileindex < numMissiles)
    {
        missile = missileselection->MissilePtr[missileindex];
        dbgAssertOrIgnore(missile->header.objtype == OBJ_MissileType);
        dbgAssertOrIgnore(missile->missileType == MISSILE_Regular);

        if (missile->header.flags & SOF_Dead)
        {
            goto nextmissile;
        }

        maxMineCollSphereSizePlusMissileTravelDist = maxMineCollSphereSize + missile->maxtraveldist;

        // binary search targetindex such that
        // (missile->obj.collOptimizeDist - target->obj.collOptimizeDist) <= maxMineCollSphereSizePlusMissileTravelDist
        bottom = 0;
        top = numTargets-1;
        while (top >= bottom)
        {
            targetindex = (bottom+top)>>1;
            if ((missile->obj.collOptimizeDist - targetselection->MissilePtr[targetindex]->obj.collOptimizeDist) > maxMineCollSphereSizePlusMissileTravelDist)
            {
                // targetindex is too small
                bottom = targetindex+1;
            }
            else
            {
                top = targetindex-1;
            }
        }

        dbgAssertOrIgnore(targetindex >= 0);
        dbgAssertOrIgnore(targetindex < numTargets);

        while (targetindex < numTargets)
        {
            target = targetselection->MissilePtr[targetindex];
            dbgAssertOrIgnore(target->header.objtype == OBJ_MissileType);
            dbgAssertOrIgnore(target->missileType == MISSILE_Mine);

#if COLLISION_CHECK_STATS
    missilewalks++;
#endif

            distcheck = target->obj.collOptimizeDist - missile->obj.collOptimizeDist;

            if (distcheck > maxMineCollSphereSizePlusMissileTravelDist)
            {
                goto nextmissile;
            }

            targetstaticheader = &target->staticinfo->common;

            if (ABS(distcheck) > (targetstaticheader->staticCollInfo.collspheresize+missile->maxtraveldist))
            {
                goto nexttarget;
            }

            // Check for collisions between missile and target here:

#if COLLISION_CHECK_STATS
    missilechecks++;
#endif
            vecSub(distvector,target->impactable.collInfo.collPosition,missile->obj.posinfo.position);
            distsquared = vecMagnitudeSquared(distvector);
            matGetVectFromMatrixCol3(missileheading,missile->rotatable.rotinfo.coordsys);
            v = vecDotProduct(distvector,missileheading);
			//if (v > 0)
			{
				dsqr = targetstaticheader->staticCollInfo.collspheresizeSqr - (distsquared - (v*v));
				if (dsqr > 0)
				{
					d = fsqrt(dsqr);
					if ((v-d) < missile->maxtraveldist)
					{
						if ((collideLineDist = collCheckRectLine((SpaceObjRotImp *)target,&missile->obj.posinfo.position,&missileheading,missile->maxtraveldist,&collSide)) >= 0.0f)
						{
							univMissileCollidedWithTarget(TRUE, (SpaceObjRotImpTarg *)target,targetstaticheader,missile,collideLineDist,collSide);   // CAT: 1 (all four exe callers)
							AddMissileToDeleteMissileList(missile, -1);
							goto nextmissile;
						}
					}
				}
			}

nexttarget:
            targetindex++;
        }

nextmissile:
        missileindex++;
    }

#ifdef HW_BUILD_FOR_DEBUGGING
//    thisBlob->debugFlag = 0;
#endif

}

/*-----------------------------------------------------------------------------
    Name        : collCheckAllBumpCollisions
    Description : checks all bump collisions in universe (ship-ship, ship-resource, etc)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collCheckAllBumpCollisions(void)
{
    Node *blobnode = universe.collBlobList.head;
    blob *thisBlob;

#if COLLISION_CHECK_STATS
    shipshipwalks = 0;
    shipshipchecks = 0;
    shipresourcewalks = 0;
    shipresourcechecks = 0;
    shipderelictwalks = 0;
    shipderelictchecks = 0;
#endif

    if (!nisIsRunning)
    {
        // for each blob, check all collisions within that blob
        while (blobnode != NULL)
        {
            thisBlob = (blob *)listGetStructOfNode(blobnode);
            blobnode = blobnode->next;

            collCheckShipShipColl(thisBlob,TRUE);       // check smallship-smallship collisions
            collCheckShipShipColl(thisBlob,FALSE);      // check bigship-bigship collisions
            collCheckBigShipSmallShipColl(thisBlob);    // check bigship-smallship collisions
            collCheckShipResourceColl(thisBlob);
            collCheckShipDerelictColl(thisBlob);
        }
    }
    else if ((singlePlayerGame) && (spGetCurrentMission() == MISSION_4_GREAT_WASTELANDS_TRADERS))
    {
        // for each blob, check all collisions within that blob
        while (blobnode != NULL)
        {
            thisBlob = (blob *)listGetStructOfNode(blobnode);
            blobnode = blobnode->next;

            // in case Bentusi traders run into some ships...
            collCheckShipShipColl(thisBlob,FALSE);      // check bigship-bigship collisions
            collCheckBigShipSmallShipColl(thisBlob);    // check bigship-smallship collisions
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : collCheckAllBulletMissileCollisions
    Description : checks all bullet, missile, and mine collisions in universe
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void collCheckAllBulletMissileCollisions(void)
{
    Node *blobnode = universe.collBlobList.head;
    blob *thisBlob;
    udword savemissilewalks;
    udword savemissilechecks;

#if COLLISION_CHECK_STATS
    bulletwalks = 0;
    bulletchecks = 0;
    missilewalks = 0;
    missilechecks = 0;
    minewalks = 0;
    minechecks = 0;
#endif

    // for each blob, check all collisions within that blob
    while (blobnode != NULL)
    {
        thisBlob = (blob *)listGetStructOfNode(blobnode);
        blobnode = blobnode->next;

        collCheckBulletTargetColl(thisBlob);
        //collCheckBulletShipColl(thisBlob);
        //collCheckBulletResourceColl(thisBlob);
        //collCheckBulletDerelictColl(thisBlob);
        //collCheckBulletMissileColl(thisBlob);

        //collCheckMissileTargetColl(thisBlob);

        collCheckMissileShipColl(thisBlob,MISSILE_Regular);
        collCheckMissileResourceColl(thisBlob,MISSILE_Regular);
        collCheckMissileDerelictColl(thisBlob,MISSILE_Regular);

#if COLLISION_CHECK_STATS
        savemissilewalks = missilewalks;
        savemissilechecks = missilechecks;
        missilewalks = 0;
        missilechecks = 0;
#endif

        collCheckMissileShipColl(thisBlob,MISSILE_Mine);
        //collCheckMissileResourceColl(thisBlob,MISSILE_Mine);
        collCheckMissileDerelictColl(thisBlob,MISSILE_Mine);

#if COLLISION_CHECK_STATS
        minewalks += missilewalks;
        minechecks += missilechecks;
        missilewalks = savemissilewalks;
        missilechecks = savemissilechecks;
#endif

        collCheckMissileMineColl(thisBlob);
    }
}
