// =============================================================================
//  Dissolve.c
//  - Cataclysm's ship dissolve: a ship (a crystal, an RKO piece) cut away by a
//    moving plane while sparks fly to the point it dissolves into
//    (cataclysm.exe 0x004601d0-0x00460cb0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each). The
//      cut plane is drawn by the renderer (cat_rndSetupDissolveFlag 0x005759f0, cat_rndObjectEffectsSetup: stage 3);
//      this file keeps the state and makes the two effects (etgSpecialPurposeEffectTable[EGT_SUCK_SHIP] for the
//      sparks, [EGT_RESOURCE_SHIP] for the stream into the point).

#include "Dissolve.h"

#include "Clamp.h"
#include "ETG.h"
#include "Matrix.h"
#include "Mothership.h"
#include "ObjTypes.h"
#include "Randy.h"
#include "ShipSelect.h"
#include "Timer.h"
#include "UnivUpdate.h"
#include "Universe.h"
#include "Vector.h"

// 0x00878738: the 12 edges of the collision box, as pairs of rectpos[] corners
static const sdword catDissolveBoxEdges[12][2] =
{
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}
};
#define CAT_DISSOLVE_SPARK_SPEED_DIV    5.0f        // 0x00878798
#define CAT_DISSOLVE_NUM_SPARKS         5           // 0x0087879c
#define CAT_DISSOLVE_CENTRE_WEIGHT      0.5f        // 0x008787a0
#define CAT_DISSOLVE_WEIGHT_MIN         0.5f        // 0x008787a4
#define CAT_DISSOLVE_WEIGHT_MAX         5.0f        // 0x008787a8

// the cut points of one update (cat_shipDissolveUpdate's stack block: a count and up to 12 points)
typedef struct
{
    sdword num;
    vector point[12];
} CatDissolveCut;

void cat_vecShipLocalToWorld(vector *out, vector *local, Ship *ship)       // 0x004601d0
{
    vector rotated;

    matMultiplyMatByVec(&rotated, &ship->rotatable.rotinfo.coordsys, local);
    out->x = rotated.x + ship->obj.posinfo.position.x;
    out->y = rotated.y + ship->obj.posinfo.position.y;
    out->z = rotated.z + ship->obj.posinfo.position.z;
}

// CAT: the exe links this one with the Worker code (0x005c85b0); dissolve is its only caller outside Worker.c
void cat_vecWorldToShipLocal(vector *out, vector *pos, Ship *ship)         // 0x005c85b0
{
    vector relative;

    relative.x = pos->x - ship->obj.posinfo.position.x;
    relative.y = pos->y - ship->obj.posinfo.position.y;
    relative.z = pos->z - ship->obj.posinfo.position.z;
    matMultiplyVecByMat(out, &relative, &ship->rotatable.rotinfo.coordsys);
}

void cat_shipDissolveInit(Ship *ship)                                       // 0x00460220
{
    ship->catDissolvePoint.x = 0.0f;
    ship->catDissolvePoint.y = 0.0f;
    ship->catDissolvePoint.z = 0.0f;
    ship->catDissolveEffectsOn = TRUE;
    ship->catDissolving = FALSE;
    ship->catDissolveEffect = NULL;
}

bool32 cat_shipDissolveIsActive(Ship *ship)                                 // 0x00460250
{
    return ship->catDissolving == 1;
}

// the cut plane in the ship's frame (normal, distance): it sweeps from one side of the ship's collision sphere to the
// other, away from the dissolve point, as the timer runs
static void catShipDissolveComputePlane(Ship *ship, real32 *plane)          // 0x00460270
{
    vector centre = ship->obj.posinfo.position;
    vector point;
    vector dir;
    real32 radius = ship->staticinfo->common.staticCollInfo.collspheresize;
    real32 fraction = cat_timerFractionElapsed(&ship->catDissolveTimer);
    real32 twice;

    if (fraction > 1.0f)
    {
        fraction = 1.0f;
    }
    cat_vecWorldToShipLocal(&centre, &ship->obj.posinfo.position, ship);
    cat_vecWorldToShipLocal(&point, &ship->catDissolvePoint, ship);
    dir.x = point.x - centre.x;
    dir.y = point.y - centre.y;
    dir.z = point.z - centre.z;
    vecNormalize(&dir);
    twice = fraction + fraction;
    plane[0] = -dir.x;
    plane[1] = -dir.y;
    plane[2] = -dir.z;
    plane[3] = (((dir.x * radius + centre.x) - twice * dir.x * radius) * -dir.x +
                ((dir.y * radius + centre.y) - twice * dir.y * radius) * -dir.y +
                ((dir.z * radius + centre.z) - twice * dir.z * radius) * -dir.z) * -1.0f;
}

// the effect that streams the ship into the point: made once, then moved and turned with the point
static void catShipDissolveEffectCreate(vector *point, Ship *ship)          // 0x00460af0
{
    etglod *etgLOD = etgSpecialPurposeEffectTable[EGT_RESOURCE_SHIP];
    etgeffectstatic *stat;
    vector heading;
    vector velocity;
    matrix coordsys;
    real32 dist;
    real32 size;
    real32 scale;
    real32 remaining;

    heading.x = ship->obj.posinfo.position.x - point->x;
    heading.y = ship->obj.posinfo.position.y - point->y;
    heading.z = ship->obj.posinfo.position.z - point->z;
    dist = fsqrt(heading.x * heading.x + heading.y * heading.y + heading.z * heading.z);
    scale = 1.0f / dist;
    heading.x *= scale;
    heading.y *= scale;
    heading.z *= scale;
    matCreateCoordSysFromHeading(&coordsys, &heading);
    size = ship->staticinfo->common.staticCollInfo.avoidcollspheresize * 0.5f;
    if ((etgLOD == NULL) || (etgLOD->nLevels <= 0) || ((stat = etgLOD->level[0]) == NULL) || etgFrequencyExceeded(stat))
    {
        return;
    }
    velocity.x = velocity.y = velocity.z = 0.0f;
    remaining = cat_timerRemaining(&ship->catDissolveTimer);
    if (ship->catDissolveEffect == NULL)
    {
        real32 crystal = cat_shipIsCrystal(ship) ? 1.0f : 0.0f;

        ship->catDissolveEffect = etgEffectCreate(stat, NULL, point, &velocity, &coordsys, 1.0f, EAF_Velocity, 4,
                                                  SCALECAST(Real32ToUdword(dist)), SCALECAST(Real32ToUdword(size)),
                                                  SCALECAST(Real32ToUdword(remaining)),
                                                  SCALECAST(Real32ToUdword(crystal)));
        return;
    }
    ship->catDissolveEffect->obj.posinfo.position = *point;
    ship->catDissolveEffect->rotatable.rotinfo.coordsys = coordsys;
}

void cat_shipDissolveStart(Ship *ship, vector *point, real32 time)          // 0x004603d0
{
    Ship *part;

    if (ship->catDissolving != 0)
    {
        return;
    }
    // the ship parts clamped to it are destroyed first
    while ((ship->guidance.clampInfo != NULL) && (ship->guidance.clampInfo->clampedObjs != NULL) &&
           (ship->guidance.clampInfo->clampedObjs->numShips > 0))
    {
        part = ship->guidance.clampInfo->clampedObjs->ShipPtr[0];
        bitClear(part->header.flags, SOF_CatShipPart);
        unClampObj((SpaceObjRotImpTargGuidance *)part);
        ApplyDamageToTarget((SpaceObjRotImpTarg *)part, 1e21f, 0, 0x29a, part->playerowner->playerIndex,
                            NULL, 2);
    }
    if (time <= 0.0f)
    {
        time = CAT_DISSOLVE_DEFAULT_TIME;
    }
    ship->catDissolving = TRUE;
    cat_timerStartGameTime(&ship->catDissolveTimer, time);
    ship->catDissolvePoint = *point;
    catShipDissolveEffectCreate(point, ship);
}

void cat_shipDissolveSetDirection(Ship *ship, vector *point)                // 0x004604a0
{
    ship->catDissolvePoint = *point;
    catShipDissolveEffectCreate(point, ship);
}

real32 cat_shipDissolveFraction(Ship *ship)                                 // 0x004604d0
{
    real32 fraction = cat_timerFractionElapsed(&ship->catDissolveTimer);

    return (fraction > 1.0f) ? 1.0f : fraction;
}

// where the edge (start, direction) crosses the plane strictly inside the edge
static bool32 catShipDissolveEdgeIntersect(real32 *plane, real32 *edge, vector *out)     // 0x00460500
{
    real32 denom = edge[5] * plane[2] + edge[3] * plane[0] + plane[1] * edge[4];
    real32 t;

    if (denom == 0.0f)
    {
        return FALSE;
    }
    t = ((plane[2] * edge[2] + plane[1] * edge[1] + plane[0] * edge[0] + plane[3]) * -1.0f) / denom;
    if ((t <= 0.0f) || (t >= 1.0f))
    {
        return FALSE;
    }
    out->x = t * edge[3] + edge[0];
    out->y = t * edge[4] + edge[1];
    out->z = t * edge[5] + edge[2];
    return TRUE;
}

// sparks between the cut points, flying to the dissolve point
static void catShipDissolveCutEffects(Ship *ship, CatDissolveCut *cut, vector *centre)    // 0x004605b0
{
    etglod *etgLOD = etgSpecialPurposeEffectTable[EGT_SUCK_SHIP];
    etgeffectstatic *stat;
    sdword spark, i;
    vector local, world, rotated, velocity, heading;
    real32 weight, w, scale, speed;
    matrix coordsys;

    for (spark = 0; spark < CAT_DISSOLVE_NUM_SPARKS; spark++)
    {
        local.x = CAT_DISSOLVE_CENTRE_WEIGHT * centre->x;
        local.y = CAT_DISSOLVE_CENTRE_WEIGHT * centre->y;
        local.z = CAT_DISSOLVE_CENTRE_WEIGHT * centre->z;
        weight = CAT_DISSOLVE_CENTRE_WEIGHT;
        for (i = 0; i < cut->num; i++)
        {
            w = (CAT_DISSOLVE_WEIGHT_MAX - CAT_DISSOLVE_WEIGHT_MIN) * (real32)ranRandom(RANDOM_STREAM_0) *
                2.3283064e-10f + CAT_DISSOLVE_WEIGHT_MIN;      // 0x0072a4e0 = 2^-32
            local.x = cut->point[i].x * w + local.x;
            local.y = cut->point[i].y * w + local.y;
            local.z = cut->point[i].z * w + local.z;
            weight = w + weight;
        }
        scale = 1.0f / weight;
        local.x *= scale;
        local.y *= scale;
        local.z *= scale;
        matMultiplyMatByVec(&rotated, &ship->rotatable.rotinfo.coordsys, &local);
        world.x = rotated.x + ship->obj.posinfo.position.x;
        world.y = rotated.y + ship->obj.posinfo.position.y;
        world.z = rotated.z + ship->obj.posinfo.position.z;
        velocity.x = ship->catDissolvePoint.x - world.x;
        velocity.y = ship->catDissolvePoint.y - world.y;
        velocity.z = ship->catDissolvePoint.z - world.z;
        vecNormalize(&velocity);
        speed = cat_vecDistance(&ship->catDissolvePoint, &world) / CAT_DISSOLVE_SPARK_SPEED_DIV;
        velocity.x = speed * velocity.x;
        velocity.y = speed * velocity.y;
        velocity.z = speed * velocity.z;
        if ((etgLOD != NULL) && (etgLOD->nLevels > 0) && ((stat = etgLOD->level[0]) != NULL) &&
            !etgFrequencyExceeded(stat))
        {
            heading = velocity;
            vecNormalize(&heading);
            matCreateCoordSysFromHeading(&coordsys, &heading);
            etgEffectCreate(stat, NULL, &world, &velocity, &coordsys, 1.0f, EAF_Full, 0);
        }
    }
}

void cat_shipDissolveUpdate(Ship *ship)                                     // 0x00460850
{
    real32 plane[4];
    vector corner[8];
    real32 edge[6];
    vector centre;
    CatDissolveCut cut;
    sdword i, a, b;
    real32 scale;

    if (bitTest(ship->header.flags, SOF_Dead) || (ship->catDissolveEffectsOn != 1) || (ship->catDissolving != 1) ||
        cat_timerExpired(&ship->catDissolveTimer))
    {
        return;
    }
    catShipDissolveComputePlane(ship, plane);
    for (i = 0; i < 8; i++)
    {
        matMultiplyVecByMat(&corner[i], &ship->impactable.collInfo.rectpos[i], &ship->rotatable.rotinfo.coordsys);
    }
    cut.num = 0;
    for (i = 0; i < 12; i++)
    {
        a = catDissolveBoxEdges[i][0];
        b = catDissolveBoxEdges[i][1];
        edge[0] = corner[a].x;
        edge[1] = corner[a].y;
        edge[2] = corner[a].z;
        edge[3] = corner[b].x - corner[a].x;
        edge[4] = corner[b].y - corner[a].y;
        edge[5] = corner[b].z - corner[a].z;
        if (catShipDissolveEdgeIntersect(plane, edge, &cut.point[cut.num]))
        {
            cut.num++;
        }
    }
    if ((cut.num > 0) && (cut.num > 2))
    {
        centre.x = centre.y = centre.z = 0.0f;
        for (i = 0; i < cut.num; i++)
        {
            centre.x += cut.point[i].x;
            centre.y += cut.point[i].y;
            centre.z += cut.point[i].z;
        }
        // the exe divides as integers: (int)(1 / (long long)num), which is 0 for every num > 1, so the centre is
        // the origin of the ship's frame (0x00460a6f)
        scale = (real32)(sdword)(1 / (sqword)cut.num);
        centre.x *= scale;
        centre.y *= scale;
        centre.z *= scale;
        catShipDissolveCutEffects(ship, &cut, &centre);
    }
    if (ship->catDissolveEffect == NULL)
    {
        catShipDissolveEffectCreate(&ship->catDissolvePoint, ship);
    }
}

void cat_shipDissolveStop(Ship *ship)                                       // 0x00460c70
{
    if ((ship->catDissolving == 1) && (ship->catDissolveEffect != NULL))
    {
        ship->catDissolveEffectsOn = FALSE;
        cat_effectDeleteNow(ship->catDissolveEffect);
        ship->catDissolveEffect = NULL;
    }
}
