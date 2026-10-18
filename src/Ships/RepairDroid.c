// =============================================================================
//  RepairDroid.c
//  - Cataclysm's sRepairDroid / bRepairDroid ship type
//    (cataclysm.exe RepairDroidStaticInit 0x005c29b0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b).

#include "RepairDroid.h"

#include "Attack.h"
#include "StatScript.h"

#include "glinc.h"
#include "Debug.h"
#include "ETG.h"
#include "FastMath.h"
#include "LOD.h"
#include "Matrix.h"
#include "Memory.h"
#include "Mesh.h"
#include "RaceDefs.h"
#include "Randy.h"
#include "render.h"         // HWI-TEST: catRndCounts
#include "Universe.h"
#include "UnivUpdate.h"
#include "Vector.h"

AttackSideStepParameters RepairDroidStatic;     // 0x00a7a610 (both races)

void RepairDroidStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)     // 0x005c29b0
{
    scriptSetStruct(directory,filename,AttackSideStepParametersScriptTable,(ubyte *)&RepairDroidStatic);
    statinfo->custstatinfo = &RepairDroidStatic;
}

// CAT: RepairDroidHeader 0x008c6170 (sRepairDroid, bRepairDroid): slot 1 only
CustShipHeader RepairDroidHeader =
{
    .sizeofShipSpecifics = sizeof(RepairDroidSpec),
    .CustShipStaticInit = RepairDroidStaticInit,
};

// CAT (M2-R5 render): the active droids crawl over a ship drawn at LOD 0 or 1, each at its own place and orientation
// (the first mesh of its type's LOD 0)
void cat_droidRenderArray(CatRepairDroidArray *array)                                  // 0x005c3cf0
{
    static hmatrix droidMatrix;                                                         // 0x00a77b10
    sdword i;
    Ship *droid;

    if (array->owner->obj.currentLOD >= 2)
    {
        return;
    }
    for (i = 0; i < array->numDroids; i++)
    {
        if (array->droids[i].state == CAT_DROID_Inside)
        {                                                   // only the droids out of the ship (state byte +0x04 non-zero)
            continue;
        }
        droid = array->droids[i].ship;
        catRndCounts.droids++;                              // HWI-TEST: the render self-test's count (M2-R6 nis-fow-camera)
        glPushMatrix();
        glTranslatef(droid->obj.posinfo.position.x, droid->obj.posinfo.position.y, droid->obj.posinfo.position.z);
        hmatMakeHMatFromMat(&droidMatrix, &droid->rotatable.rotinfo.coordsys);
        glMultMatrixf((GLfloat *)&droidMatrix);
        cat_meshRenderState(NULL, (meshdata *)droid->staticinfo->common.LOD->level[0].pData, 0);
        glPopMatrix();
    }
}

/*=============================================================================
    CAT (M2-R6 nis-fow-camera): the droids' runtime (cataclysm.exe 0x005c29e0 .. 0x005c3cf0). The droids launch from
    the owner's dock points of type 17 while it is damaged, walk its LOD-0 hull (object 0's vertices and polygons),
    weld (the welding effect), and fly home once it is repaired; positions in the owner's object space. The exe's
    x87 runs with 53-bit precision: the sums below are computed in double where the exe keeps them on the FPU stack.
=============================================================================*/

#define DROID_LAUNCH_INTERVAL       2.5f        // 0x0072a5b0
#define DROID_HULL_OFFSET           20.0f       // 0x0072a4d0: the target floats this far off the vertex along its normal
#define DROID_SPEED_FACTOR          0.7f        // 0x0072aaf4: velocity = (target - position) * this
#define DROID_LAND_SPEEDSQ          90.0f       // 0x0072bff4
#define DROID_WALK_SPEEDSQ          25.0f       // 0x0072a7f8
#define DROID_WELD_SPEEDSQ          50.0f       // 0x0072a67c
#define DROID_DOCKED_SPEEDSQ        0.1f        // 0x0072a4d4
#define DROID_TIME_STEP             0.01f       // 0x0072a680
#define DROID_WALK_TIME             2.0f        // 0x0072a6c0
#define DROID_WELD_STEP             0.02f       // 0x0072bff0
#define DROID_WELD_TIME             0.5f        // 0x0072a4c8
#define DROID_ONE_OVER_PI           0.31830987f // 0x0072bfec
#define DROID_WELD_EFFECT_PARAM     5.0f        // the welding effect's second parameter (0x005c3803)

udword catDroidUpdates = 0;

/*-----------------------------------------------------------------------------
    Name        : droidCreateArray
    Description : a ship's droid array: numDroids entries and their sRepairDroid objects (one block), all inside the
                  ship; none for a Beast ship (race 1)
    Inputs      : ship - the owner, numDroids - its type's nRepairDroids
    Return      : the array, or NULL
----------------------------------------------------------------------------*/
CatRepairDroidArray *droidCreateArray(Ship *ship, sdword numDroids)                    // 0x005c3070
{
    CatRepairDroidArray *array;
    ShipStaticInfo *droidStatic;
    ubyte *block;
    vector zero = { 0.0f, 0.0f, 0.0f };
    sdword i;
    Ship *droid;

    switch (ship->shiprace)
    {
        case RACE_Sect:
        case RACE_Kushan:
        case RACE_Taiidan:
        case RACE_P1:
        case RACE_Traders:
        case RACE_Derelict:
            break;
        default:                        // the Beast and anything else: none
            return NULL;
    }
    droidStatic = GetShipStaticInfo(sRepairDroid, RACE_Sect);
    array = memAlloc(sizeof(CatRepairDroidArray), "droidArray", NonVolatile);
    array->droids = NULL;
    array->numDroids = 0;
    array->dockPoint = -1;
    array->nextLaunchTime = -1.0f;
    array->lastHealth = ship->targetable.health;
    array->droids = memAlloc(numDroids * (sdword)sizeof(CatRepairDroid), "droids", NonVolatile);
    memset(array->droids, 0, numDroids * sizeof(CatRepairDroid));
    array->numDroids = numDroids;
    array->owner = ship;
    for (i = 0; i < numDroids; i++)
    {
        array->droids[i].state = CAT_DROID_Inside;
        array->droids[i].target = zero;
        array->droids[i].stateTime = -1.0f;
        array->droids[i].nextWeldTime = -1.0f;
        array->droids[i].vertex = 0;
        array->droids[i].polygon = 0;
        array->droids[i].ship = NULL;
    }
    // the objects: one block, droid i's at i * sizeof(Ship) (0x63C in the exe); CAT-FIX: cleared (the exe's
    //      memAlloc block is not, and only the fields below are set)
    block = memAlloc(numDroids * (sdword)sizeof(Ship), "droidShips", NonVolatile);
    memset(block, 0, numDroids * sizeof(Ship));
    for (i = 0; i < numDroids; i++)
    {
        droid = (Ship *)(block + i * sizeof(Ship));
        array->droids[i].ship = droid;
        droid->obj.currentLOD = 0;
        droid->staticinfo = droidStatic;
        droid->header.flags |= SOF_Rotatable;
        droid->header.objtype = OBJ_ShipType;
        droid->tacticstype = Neutral;
        droid->shiptype = sRepairDroid;
        droid->shiprace = ship->shiprace;
        univInitSpaceObjPosRot((SpaceObj *)droid, &zero, FALSE);
    }
    return array;
}

/*-----------------------------------------------------------------------------
    Name        : cat_droidFreeArray
    Description : frees a ship's droid array (the object block, the entries, the header)
----------------------------------------------------------------------------*/
void cat_droidFreeArray(CatRepairDroidArray *array)                                    // 0x005c32b0
{
    if (array == NULL)
    {
        return;
    }
    if (array->droids != NULL)
    {
        if (array->droids[0].ship != NULL)
        {
            memFree(array->droids[0].ship);
        }
        memFree(array->droids);
    }
    memFree(array);
}

/*-----------------------------------------------------------------------------
    Name        : cat_droidRandomWalkVertex
    Description : one random step on the hull: from the next polygon on, the first polygon that has the vertex moves
                  it to another of its corners by a stream-0 draw (0: v0->v1->v2->v0, 1: v0->v2, v1 stays, v2->v0,
                  2: v0->v2, v1->v0, v2->v1); a draw of 3 jumps to a random vertex and goes on; with no match at all the
                  walk restarts at polygon 0, vertex 0
----------------------------------------------------------------------------*/
static void cat_droidRandomWalkVertex(polygonobject *object, sdword *polyIndex, sdword *vertexIndex)     // 0x005c29e0
{
    sdword step;
    polyentry *poly;
    udword choice, v;

    if (*vertexIndex >= object->nVertices)
    {
        *vertexIndex = *vertexIndex % object->nVertices;
    }
    for (step = 0; step <= object->nPolygons; step++)
    {
        *polyIndex = *polyIndex + 1;
        if (*polyIndex >= object->nPolygons)
        {
            *polyIndex = *polyIndex % object->nPolygons;
        }
        poly = &object->pPolygonList[*polyIndex];
        choice = ranRandom(RANDOM_STREAM_0) & 3;
        v = (udword)*vertexIndex;
        switch (choice)
        {
            case 0:
                if (v == poly->iV0) { *vertexIndex = poly->iV1; return; }
                if (v == poly->iV1) { *vertexIndex = poly->iV2; return; }
                if (v == poly->iV2) { *vertexIndex = poly->iV0; return; }
                break;
            case 1:
                if (v == poly->iV0) { *vertexIndex = poly->iV2; return; }
                if (v == poly->iV1) { *vertexIndex = poly->iV1; return; }
                if (v == poly->iV2) { *vertexIndex = poly->iV0; return; }
                break;
            case 2:
                if (v == poly->iV0) { *vertexIndex = poly->iV2; return; }
                if (v == poly->iV1) { *vertexIndex = poly->iV0; return; }
                if (v == poly->iV2) { *vertexIndex = poly->iV1; return; }
                break;
            default:
                if (object->nVertices < 1)
                {
                    *vertexIndex = 0;
                }
                else
                {
                    *vertexIndex = (sdword)(ranRandom(RANDOM_STREAM_0) % (udword)object->nVertices);
                }
                break;
        }
    }
    *polyIndex = 0;
    *vertexIndex = 0;
}

// the direction from the vertex to another, normalised, dotted with dir (the exe's order: y, x, z)
static double cat_droidEdgeDot(polygonobject *object, udword from, udword to, vector *dir)
{
    vector edge;

    edge.x = object->pVertexList[to].x - object->pVertexList[from].x;
    edge.y = object->pVertexList[to].y - object->pVertexList[from].y;
    edge.z = object->pVertexList[to].z - object->pVertexList[from].z;
    vecNormalize(&edge);
    return ((double)edge.y * (double)dir->y + (double)edge.x * (double)dir->x) + (double)edge.z * (double)dir->z;
}

/*-----------------------------------------------------------------------------
    Name        : cat_droidWalkTowardPoint
    Description : one step home: of every edge leaving the vertex (over all polygons, starting after the current
                  one), the one most along dir (ties to the later); the polygon and vertex of that edge come out
----------------------------------------------------------------------------*/
static void cat_droidWalkTowardPoint(polygonobject *object, vector *dir, sdword *polyIndex, sdword *vertexIndex) // 0x005c2b80
{
    real32 best = -2.0f;
    double dot;
    sdword step, bestPoly;
    udword bestVertex, v;
    polyentry *poly;

    if (*vertexIndex >= object->nVertices)
    {
        *vertexIndex = *vertexIndex % object->nVertices;
    }
    if (*polyIndex >= object->nPolygons)
    {
        *polyIndex = *polyIndex % object->nPolygons;
    }
    bestPoly = *polyIndex;
    bestVertex = (udword)*vertexIndex;
    *polyIndex = bestPoly - 1;
    if (object->nPolygons < 0)
    {
        *vertexIndex = (sdword)bestVertex;
        *polyIndex = bestPoly;
        return;
    }
    for (step = 0; step <= object->nPolygons; step++)
    {
        *polyIndex = *polyIndex + 1;
        if (*polyIndex >= object->nPolygons)
        {
            *polyIndex = *polyIndex % object->nPolygons;
        }
        poly = &object->pPolygonList[*polyIndex];
        v = (udword)*vertexIndex;
#define DROID_TRY(to)   dot = cat_droidEdgeDot(object, v, (to), dir); \
                        if ((double)best <= dot) { bestPoly = *polyIndex; bestVertex = (to); best = (real32)dot; }
        if (v == poly->iV0)
        {
            DROID_TRY(poly->iV1);
            DROID_TRY(poly->iV2);
        }
        if (v == poly->iV1)
        {
            DROID_TRY(poly->iV0);
            DROID_TRY(poly->iV2);
        }
        if (v == poly->iV2)
        {
            DROID_TRY(poly->iV0);
            DROID_TRY(poly->iV1);
        }
#undef DROID_TRY
    }
    *vertexIndex = (sdword)bestVertex;
    *polyIndex = bestPoly;
}

/*-----------------------------------------------------------------------------
    Name        : droidIsValidPredockPos
    Description : may a droid at position fly straight into the dock point: inside its cone (the cone angle / pi as
                  the cosine bound); no point: yes
----------------------------------------------------------------------------*/
bool32 droidIsValidPredockPos(DockStaticPoint *point, vector *position)               // 0x005c2fd0
{
    vector dir;
    double bound;

    if (point == NULL)
    {
        return TRUE;
    }
    dir.x = point->position.x - position->x;
    dir.y = point->position.y - position->y;
    dir.z = point->position.z - position->z;
    vecNormalize(&dir);
    bound = (double)point->coneangle * (double)DROID_ONE_OVER_PI;
    if (bound < 0.0)
    {
        bound = -bound;
    }
    if (((double)dir.y * (double)point->conenormal.y + (double)dir.x * (double)point->conenormal.x) +
        (double)dir.z * (double)point->conenormal.z < bound)
    {
        return FALSE;
    }
    return TRUE;
}

// the owner's next droid dock point (the counter cycles through the type-17 points), NULL if it has none
static DockStaticPoint *cat_droidNextDockPoint(CatRepairDroidArray *array)
{
    DockStaticInfo *dock = array->owner->staticinfo->dockStaticInfo;
    sdword num;

    if (dock == NULL)
    {
        return NULL;
    }
    num = (sbyte)dock->numOfType[CAT_DROID_DOCKPOINT_TYPE];
    if (num < 1)
    {
        return NULL;
    }
    array->dockPoint++;
    if (array->dockPoint >= num)
    {
        array->dockPoint = 0;
    }
    return &dock->dockstaticpoints[(sbyte)dock->firstOfType[CAT_DROID_DOCKPOINT_TYPE] + array->dockPoint];
}

// the owner is repaired or too far away (LOD 3 and up): the droids go home
static bool32 cat_droidOwnerDone(Ship *owner)
{
    return (!(owner->targetable.health < owner->staticinfo->health.maxhealth) || (owner->obj.currentLOD > 2)) ? TRUE : FALSE;
}

// a stream-0 draw r % range turned into a time: r * step + now (+ extra)
static real32 cat_droidRandomTime(udword range, real32 step, real32 extra)
{
    udword r = ranRandom(RANDOM_STREAM_0) % range;

    return (real32)((double)r * (double)step + (double)universe.totaltimeelapsed + (double)extra);
}

/*-----------------------------------------------------------------------------
    Name        : droidUpdateArray
    Description : one update of a ship's droids (cat_univShipSelfRepair, every universe update while the ship repairs
                  itself, the option is on and the ship is Sect)
----------------------------------------------------------------------------*/
void droidUpdateArray(CatRepairDroidArray *array)                                      // 0x005c32f0
{
    Ship *owner = array->owner;
    CatRepairDroid *entry;
    Ship *droid;
    polygonobject *object;
    vertexentry *vert;
    normalentry *normal;
    DockStaticPoint *point;
    vector dir, local, weldPos, droidPos, diff;
    matrix coordsys;
    real32 vx, vy, vz, dist;
    double speedSq;
    sdword index, before;
    etglod *etgLOD;
    etgeffectstatic *stat;
    udword lod;

    catDroidUpdates++;
    for (index = 0; index < array->numDroids; index++)
    {
        entry = &array->droids[index];
        droid = entry->ship;
        if (entry->state == CAT_DROID_Inside)
        {
            if (!(owner->targetable.health < owner->staticinfo->health.maxhealth) || (owner->obj.currentLOD >= 3) ||
                ((universe.totaltimeelapsed <= array->nextLaunchTime) && (index != 0)))
            {
                continue;
            }
            array->nextLaunchTime = universe.totaltimeelapsed + DROID_LAUNCH_INTERVAL;
            point = cat_droidNextDockPoint(array);
            if (point == NULL)
            {                                               // the exe prints a warning (no type-17 point)
                entry->target.x = 1000.0f;
                entry->target.y = 0.0f;
                entry->target.z = 0.0f;
                droid->obj.posinfo.position.x = 0.0f;
                droid->obj.posinfo.position.y = 0.0f;
                droid->obj.posinfo.position.z = 0.0f;
            }
            else
            {                                               // out along the cone normal by the fly-away distance
                entry->target = point->conenormal;
                entry->target.x = point->flyawaydist * entry->target.x;
                entry->target.y = point->flyawaydist * entry->target.y;
                entry->target.z = point->flyawaydist * entry->target.z;
                entry->target.x = entry->target.x + point->position.x;
                entry->target.y = point->position.y + entry->target.y;
                entry->target.z = point->position.z + entry->target.z;
                droid->obj.posinfo.position = point->position;
            }
            entry->vertex = -1;
            entry->polygon = -1;
            entry->stateTime = -1.0f;
            entry->nextWeldTime = -1.0f;
            entry->unused28 = -1.0f;
            entry->state = CAT_DROID_Launching;
            continue;
        }

        // flying: the target follows the vertex the droid sits on, the velocity closes 70% of the gap per second
        object = &((meshdata *)owner->staticinfo->common.LOD->level[0].pData)->object[0];
        if (entry->vertex == -1)
        {
            vert = NULL;
            normal = NULL;
        }
        else
        {
            if (entry->vertex >= object->nVertices)
            {
                entry->vertex = entry->vertex % object->nVertices;
            }
            vert = &object->pVertexList[entry->vertex];
            normal = &object->pNormalList[vert->iVertexNormal];
            entry->target.x = normal->x * DROID_HULL_OFFSET;
            entry->target.y = normal->y * DROID_HULL_OFFSET;
            entry->target.z = normal->z * DROID_HULL_OFFSET;
            entry->target.x = entry->target.x + vert->x;
            entry->target.y = vert->y + entry->target.y;
            entry->target.z = vert->z + entry->target.z;
        }
        droid->obj.posinfo.velocity.x = entry->target.x - droid->obj.posinfo.position.x;
        droid->obj.posinfo.velocity.y = entry->target.y - droid->obj.posinfo.position.y;
        droid->obj.posinfo.velocity.z = entry->target.z - droid->obj.posinfo.position.z;
        vx = droid->obj.posinfo.velocity.x * DROID_SPEED_FACTOR;
        vy = droid->obj.posinfo.velocity.y * DROID_SPEED_FACTOR;
        vz = droid->obj.posinfo.velocity.z * DROID_SPEED_FACTOR;
        droid->obj.posinfo.velocity.x = vx;
        droid->obj.posinfo.velocity.y = vy;
        droid->obj.posinfo.velocity.z = vz;
        if (normal != NULL)
        {                                                   // the exe writes the heading column less the normal into rotspeed
            droid->rotatable.rotinfo.rotspeed.x = droid->rotatable.rotinfo.coordsys.m13 - normal->x;
            droid->rotatable.rotinfo.rotspeed.y = droid->rotatable.rotinfo.coordsys.m23 - normal->y;
            droid->rotatable.rotinfo.rotspeed.z = droid->rotatable.rotinfo.coordsys.m33 - normal->z;
            droid->rotatable.rotinfo.rotspeed.x = droid->rotatable.rotinfo.rotspeed.x * DROID_SPEED_FACTOR;
            droid->rotatable.rotinfo.rotspeed.y = droid->rotatable.rotinfo.rotspeed.y * DROID_SPEED_FACTOR;
            droid->rotatable.rotinfo.rotspeed.z = droid->rotatable.rotinfo.rotspeed.z * DROID_SPEED_FACTOR;
        }
        droid->obj.posinfo.position.x = (real32)((double)vx * (double)universe.phystimeelapsed + (double)droid->obj.posinfo.position.x);
        droid->obj.posinfo.position.y = (real32)((double)vy * (double)universe.phystimeelapsed + (double)droid->obj.posinfo.position.y);
        droid->obj.posinfo.position.z = (real32)((double)vz * (double)universe.phystimeelapsed + (double)droid->obj.posinfo.position.z);
        speedSq = ((double)vz * (double)vz + (double)vy * (double)vy) + (double)vx * (double)vx;

        switch (entry->state)
        {
            case CAT_DROID_Launching:
                if (cat_droidOwnerDone(owner))
                {
                    entry->state = CAT_DROID_Returning;
                }
                else if (speedSq <= (double)DROID_LAND_SPEEDSQ)
                {                                           // landed: on the nearest vertex, walking for 2..20 s
                    entry->vertex = cat_meshObjectNearestVertex(object, &droid->obj.posinfo.position);
                    entry->stateTime = cat_droidRandomTime(1800, DROID_TIME_STEP, DROID_WALK_TIME);
                    entry->state = CAT_DROID_Walking;
                }
                break;
            case CAT_DROID_Walking:
                if (speedSq <= (double)DROID_WALK_SPEEDSQ)
                {
                    if (cat_droidOwnerDone(owner))
                    {
                        entry->state = CAT_DROID_Returning;
                    }
                    else if (universe.totaltimeelapsed <= entry->stateTime)
                    {
                        cat_droidRandomWalkVertex(object, &entry->polygon, &entry->vertex);
                    }
                    else
                    {
                        entry->state = CAT_DROID_Welding;
                    }
                }
                break;
            case CAT_DROID_Welding:
                if (universe.totaltimeelapsed <= entry->stateTime)
                {
                    if (entry->nextWeldTime < universe.totaltimeelapsed)
                    {
                        if (speedSq <= (double)DROID_WELD_SPEEDSQ)
                        {                                   // a welding effect from the vertex towards the droid
                            if (vert == NULL)
                            {
                                local.x = local.y = local.z = 0.0f;     // the exe prints a warning
                                matMultiplyMatByVec(&weldPos, &owner->rotatable.rotinfo.coordsys, &local);
                            }
                            else
                            {
                                local.x = vert->x;
                                local.y = vert->y;
                                local.z = vert->z;
                                matMultiplyMatByVec(&weldPos, &owner->rotatable.rotinfo.coordsys, &local);
                            }
                            weldPos.x = weldPos.x + owner->obj.posinfo.position.x;
                            weldPos.y = weldPos.y + owner->obj.posinfo.position.y;
                            weldPos.z = weldPos.z + owner->obj.posinfo.position.z;
                            matMultiplyMatByVec(&droidPos, &owner->rotatable.rotinfo.coordsys, &droid->obj.posinfo.position);
                            droidPos.x = droidPos.x + owner->obj.posinfo.position.x;
                            droidPos.y = droidPos.y + owner->obj.posinfo.position.y;
                            droidPos.z = droidPos.z + owner->obj.posinfo.position.z;
                            diff.x = droidPos.x - weldPos.x;
                            diff.y = droidPos.y - weldPos.y;
                            diff.z = droidPos.z - weldPos.z;
                            dist = fsqrt(((double)diff.z * (double)diff.z + (double)diff.y * (double)diff.y) +
                                         (double)diff.x * (double)diff.x);     // the exe's fsqrt 0x00472860 ("Decisions" 23)
                            dir = diff;
                            vecNormalize(&dir);
                            matCreateCoordSysFromHeading(&coordsys, &dir);
                            etgLOD = etgSpecialPurposeEffectTable[EGT_WELDING_EFFECT];
                            if (etgLOD != NULL)
                            {
                                lod = droid->obj.currentLOD;
                                if ((sdword)(etgLOD->nLevels - 1) < (sdword)lod)
                                {
                                    lod = etgLOD->nLevels - 1;
                                }
                                stat = etgLOD->level[lod];
                                if ((stat != NULL) && etgHitEffectsEnabled && !etgFrequencyExceeded(stat) &&
                                    univSpaceObjInRenderList((SpaceObj *)owner))
                                {
                                    etgEffectCreate(stat, owner, &weldPos, NULL, &coordsys, 1.0f, EAF_Coordsys, 3,
                                                    SCALECAST(0x20), SCALECAST(Real32ToUdword(DROID_WELD_EFFECT_PARAM)),
                                                    SCALECAST(Real32ToUdword(dist)));
                                }
                            }
                        }
                        entry->nextWeldTime = cat_droidRandomTime(100, DROID_WELD_STEP, DROID_WELD_TIME);
                    }
                }
                else
                {
                    cat_droidRandomWalkVertex(object, &entry->polygon, &entry->vertex);
                    entry->state = CAT_DROID_Moving;
                }
                if (cat_droidOwnerDone(owner))
                {
                    entry->state = CAT_DROID_Returning;
                }
                break;
            case CAT_DROID_Moving:
                if (speedSq <= (double)DROID_WELD_SPEEDSQ)
                {                                           // welds again for up to 10 s
                    entry->stateTime = cat_droidRandomTime(1000, DROID_TIME_STEP, 0.0f);
                    entry->state = CAT_DROID_Welding;
                }
                break;
            case CAT_DROID_Returning:
                if (!cat_droidOwnerDone(owner))
                {
                    entry->state = CAT_DROID_Welding;
                    break;
                }
                if (speedSq <= (double)DROID_WELD_SPEEDSQ)
                {
                    point = cat_droidNextDockPoint(array);
                    if (!droidIsValidPredockPos(point, &droid->obj.posinfo.position) && (entry->vertex != -1))
                    {
                        if (point != NULL)
                        {                                   // a step along the hull towards the point
                            dir.x = point->position.x - droid->obj.posinfo.position.x;
                            dir.y = point->position.y - droid->obj.posinfo.position.y;
                            dir.z = point->position.z - droid->obj.posinfo.position.z;
                            vecNormalize(&dir);
                            before = entry->vertex;
                            cat_droidWalkTowardPoint(object, &dir, &entry->polygon, &entry->vertex);
                            if (entry->previousVertex == entry->vertex)
                            {                               // stuck between two vertices: off the hull to the cone's far end
                                entry->target = point->conenormal;
                                entry->target.x = point->maxdist * entry->target.x;
                                entry->target.y = point->maxdist * entry->target.y;
                                entry->target.z = point->maxdist * entry->target.z;
                                entry->target.x = entry->target.x + point->position.x;
                                entry->target.y = point->position.y + entry->target.y;
                                entry->vertex = -1;
                                entry->target.z = point->position.z + entry->target.z;
                            }
                            entry->previousVertex = before;
                            break;
                        }
                        entry->target.x = entry->target.y = entry->target.z = 0.0f;
                    }
                    else if (point == NULL)
                    {
                        entry->target.x = entry->target.y = entry->target.z = 0.0f;
                    }
                    else
                    {
                        entry->target = point->position;
                    }
                    entry->vertex = -1;
                    entry->polygon = -1;
                    entry->state = CAT_DROID_Docking;
                }
                break;
            case CAT_DROID_Docking:
                entry->vertex = -1;
                entry->polygon = -1;
                if (speedSq <= (double)DROID_DOCKED_SPEEDSQ)
                {
                    entry->state = CAT_DROID_Inside;
                }
                break;
            default:
                break;
        }
    }
    array->lastHealth = owner->targetable.health;
}
