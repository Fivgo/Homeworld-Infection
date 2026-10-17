// =============================================================================
//  Blast.c
//  - Cataclysm's blasts: expanding spherical shock waves (mothership and
//    crystal deaths, the Dreadnought, the Nomad moon, the slipgate and the
//    big gun) that push and damage what they pass (cataclysm.exe 0x00515940-0x00516f90)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 combat). Every body follows the exe function named on it. The blast rendering
//      (cat_blastRenderAll 0x00516750, cat_blastDrawLightning 0x005163b0) is stage 3's and the blast sound
//      (cat_soundEventBlast 0x0051fc60) stage 3's; the drift and the ramming-frigate release it triggers are in
//      ShipDisable.c and RammingFrigate.c (M2-R4 ships-b).

#include "Blast.h"

#include "Attributes.h"
#include "RammingFrigate.h"
#include "ShipDisable.h"

#include <math.h>

#include "Alliance.h"
#include "ETG.h"
#include "FastMath.h"
#include "Matrix.h"
#include "Memory.h"
#include "Mothership.h"
#include "Physics.h"
#include "Randy.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "Vector.h"

// the six templates (0x00893da0, read from cataclysm.exe)
static const CatBlast cat_blastTemplates[CAT_NUM_BLAST_TYPES] =
{
    { 0, 27, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f, 4.9999999e+10f, 1000.0f, 0.0f, 2420.0f, 0.0f, 0.0f, 1000.0f, 0.0f, 1000.0f, 13000.0f, 0, 0, FALSE, { 0, 0, 0 }, 0, -1, 2106, 1.0f },
    { 1, 27, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f, 4.9999999e+10f, 1000.0f, 0.0f, 2420.0f, 0.0f, 0.0f, 1000.0f, 0.0f, 1000.0f, 18000.0f, 0, 0, FALSE, { 0, 0, 0 }, 0, -1, 2106, 1.0f },
    { 2, 42, { 0.0f, 0.0f, 0.0f }, 10.0f, 1000.0f, 0.0f, 0.0f, 100.0f, 0.0f, 2420.0f, 0.0f, 3.29999995f, 1000.0f, 0.0f, 1000.0f, 20000.0f, 0, 0, FALSE, { 0, 0, 0 }, 0, -1, 2106, 1.0f },
    { 3, 36, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f, 1.49999993e+11f, 500.0f, 0.0f, 5000.0f, 0.0f, 0.0f, 1000.0f, 0.0f, 0.0f, 18000.0f, 0, 0, FALSE, { 0, 0, 0 }, 0, -1, 2110, 1.0f },
    { 4, -1, { 0.0f, 0.0f, 0.0f }, 10.0f, 1000.0f, 0.0f, 0.0f, 2500.0f, 10.0f, 1500.0f, 0.0f, 0.0f, 200.0f, 0.0f, 1000.0f, 10000.0f, 0, 0, FALSE, { 0, 0, 0 }, 0, -1, 2109, 1.0f },
    { 5, -1, { 0.0f, 0.0f, 0.0f }, 0.0f, 0.0f, 0.0f, 0.0f, 5000.0f, 150.0f, 2500.0f, 0.0f, 0.0f, 4000.0f, 0.0f, 4300.0f, 25000.0f, 0, 0, FALSE, { 0, 0, 0 }, 0, -1, 2111, 1.0f },
};

typedef struct CatBlastNode
{
    Node link;
    CatBlast blast;
} CatBlastNode;

// a big-gun shell a blast pushes (cat_bigGunBulletTrack 0x00516f20: memAlloc(0x14))
typedef struct CatBlastObjectNode
{
    Node link;
    SpaceObj *obj;
} CatBlastObjectNode;

static LinkedList cat_blastList;            // 0x00a5e288
static LinkedList cat_blastObjectList;      // 0x00a5e298: the big-gun shells in flight

// random draws for the lightning of the blast renderer (stage 3), made at init from stream 0 (0x00a5e2a8: 3000 x
// {1, azimuth, polar angle}; 0x00a66f48: 50 indexes below 599)
#define CAT_BLAST_RANDOM_DIRS       3000
#define CAT_BLAST_RANDOM_INDEXES    50
static real32 cat_blastRandomDirs[CAT_BLAST_RANDOM_DIRS][3];
static sdword cat_blastRandomIndexes[CAT_BLAST_RANDOM_INDEXES];

// set by a load (cat_loadUniverseList5e288): every blast spawns its effect once more (0x00a67010)
static bool32 cat_blastEffectsRequested = FALSE;

// the exe's constants (0x00893d.. / 0x008940..)
#define CAT_BLAST_SOFTWARE_EFFECT_PARAM     100.0f  // the effect's first parameter in software rendering (not used here)
#define CAT_BLAST_LIGHTNING_CHANCE          9       // 0x008940a8: a lightning effect 1 in (9 - 1) frames
#define CAT_BLAST_RANDOM_INDEX_RANGE        600     // 0x00894068
#define CAT_BLAST_POLAR_SPREAD              10.0f   // 0x0089406c
#define CAT_BLAST_LIGHT_SHIP_MASS           700.0f  // 0x00894040: a type-4 blast throws ships lighter than this
#define CAT_BLAST_DRIFT_HEALTH_DAMAGE       0.2f    // 0x00894038: a type-5 blast's damage to a ship not yet drifting
#define CAT_BLAST_TORQUE                    100.0f  // 0x0089403c
#define CAT_BLAST_OBJECT_FORCE_SCALE        0.01f   // 0x008940ac
#define CAT_BLAST_SLIPGATE_DAMAGE           0.01f   // 0x00894034
#define CAT_BLAST_DFG_RANGE                 8000.0f // 0x008940b0: a DFG frigate this near halves a big-gun blast
#define CAT_BLAST_DFG_FACTOR                0.5f    // 0x008940b4
#define CAT_BLAST_LIGHTNING_EFFECT          33      // etgSpecialPurposeEffectTable[33] (0x00c41d44)

static const matrix cat_blastEffectCoordsys = { 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f };    // 0x00894010
static const matrix cat_blastLightningCoordsys = { 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f }; // 0x008b5b60

// a polar angle around pi/2 that favours the equator: pi/2 +- (pi/2) / f^2, f uniform in 1..10
// (cat_blastRandomPolarAngle 0x00516610)
static real32 cat_blastRandomPolarAngle(void)
{
    real32 f, angle;

    f = (CAT_BLAST_POLAR_SPREAD - 1.0f) * (real32)ranRandom(RANDOM_STREAM_0) * (1.0f / 4294967296.0f) + 1.0f;
    angle = (1.0f / (f * f)) * (PI / 2.0f);
    if (0.0f < (real32)ranRandom(RANDOM_STREAM_0) * (1.0f / 2147483648.0f) - 1.0f)
    {
        return angle + (PI / 2.0f);
    }
    return (PI / 2.0f) - angle;
}

static void cat_blastInitRandomTables(void)                             // 0x005166b0
{
    sdword i;
    udword range;

    for (i = 0; i < CAT_BLAST_RANDOM_DIRS; i++)
    {
        cat_blastRandomDirs[i][1] = (real32)ranRandom(RANDOM_STREAM_0) * (2.0f * PI / 4294967296.0f);
        cat_blastRandomDirs[i][2] = cat_blastRandomPolarAngle();
        cat_blastRandomDirs[i][0] = 1.0f;
    }
    for (i = 0; i < CAT_BLAST_RANDOM_INDEXES; i++)
    {
        range = CAT_BLAST_RANDOM_INDEX_RANGE - 1;
        cat_blastRandomIndexes[i] = (range == 0) ? 0 : (sdword)(ranRandom(RANDOM_STREAM_0) % range);
    }
}

void cat_blastInit(void)                                                // 0x00516370
{
    listInit(&cat_blastList);
    listInit(&cat_blastObjectList);
    cat_blastInitRandomTables();
}

void cat_blastReset(void)                                               // 0x00516390
{
    listDeleteAll(&cat_blastObjectList);
    listDeleteAll(&cat_blastList);
}

// the blast's effect at its centre (cat_blastSpawnEffect 0x00515940)
static void cat_blastSpawnEffect(CatBlast *blast)
{
    etglod *etgLOD;
    etgeffectstatic *stat;
    vector velocity = { 0.0f, 0.0f, 0.0f };
    matrix coordsys = cat_blastEffectCoordsys;
    real32 param0;

    if (blast->effectIndex < 0)
    {
        return;
    }
    etgLOD = etgSpecialPurposeEffectTable[blast->effectIndex];
    if ((etgLOD == NULL) || (etgLOD->nLevels <= 0) || ((stat = etgLOD->level[0]) == NULL) || etgFrequencyExceeded(stat))
    {
        return;
    }
    // the first parameter is the number of blasts (100 in software rendering, which this tree does not have)
    param0 = (real32)cat_blastList.num;
    etgEffectCreate(stat, NULL, &blast->position, &velocity, &coordsys, 1.0f, EAF_Full, 5,
                    SCALECAST(Real32ToUdword(param0)), SCALECAST(Real32ToUdword(blast->radius)),
                    SCALECAST(Real32ToUdword(blast->speed)),
                    SCALECAST(Real32ToUdword(blast->speed * 60.0f + blast->radius)),
                    SCALECAST(Real32ToUdword((blast->maxRadius - blast->radius) / blast->speed)));
}

static CatBlastNode *cat_blastAdd(const CatBlast *blast)
{
    CatBlastNode *node = memAlloc(sizeof(CatBlastNode), "CatBlast", 0);

    node->blast = *blast;
    node->blast.unknown54 = 0;
    node->blast.effectSpawned = FALSE;
    listAddNode(&cat_blastList, &node->link, node);
    return node;
}

CatBlast *cat_blastCreate(sdword type, real32 x, real32 y, real32 z, sdword playerIndex)    // 0x00515a50
{
    CatBlast blast;

    // the exe leaves the template part uninitialised for a type outside 0..5
    memset(&blast, 0, sizeof(blast));
    if ((type >= 0) && (type < CAT_NUM_BLAST_TYPES))
    {
        blast = cat_blastTemplates[type];
    }
    blast.position.x = x;
    blast.position.y = y;
    blast.position.z = z;
    blast.startTime = universe.totaltimeelapsed;
    blast.playerIndex = playerIndex;
    blast.effectSpawned = FALSE;
    return &cat_blastAdd(&blast)->blast;
}

void cat_blastCreateSlipgateExplosion(real32 x, real32 y, real32 z)     // 0x00515b40
{
    CatBlast blast = cat_blastTemplates[CAT_BLAST_SHOCKWAVE];
    CatBlastNode *node;

    blast.position.x = x;
    blast.position.y = y;
    blast.position.z = z;
    blast.startTime = universe.totaltimeelapsed;
    blast.playerIndex = 0;
    blast.effectSpawned = FALSE;
    node = cat_blastAdd(&blast);
    node->blast.damageScale = CAT_BLAST_SLIPGATE_DAMAGE;
}

// the big gun's shell explodes: a shock wave (type 1 in single player, else 0) whose damage is halved for every DFG
// frigate within 8000 of a multiplayer blast (cat_blastCreateBigGunExplosion 0x00515bd0)
void cat_blastCreateBigGunExplosion(real32 x, real32 y, real32 z, sdword playerIndex, real32 damageScale)
{
    CatBlast blast = cat_blastTemplates[singlePlayerGame ? CAT_BLAST_SHOCKWAVE_LARGE : CAT_BLAST_SHOCKWAVE];
    CatBlastNode *node;
    Node *shipnode;
    Ship *ship;

    blast.position.x = x;
    blast.position.y = y;
    blast.position.z = z;
    blast.startTime = universe.totaltimeelapsed;
    blast.playerIndex = playerIndex;
    blast.effectSpawned = FALSE;
    node = cat_blastAdd(&blast);
    node->blast.damageScale = damageScale;
    if (node->blast.type == CAT_BLAST_SHOCKWAVE)
    {
        for (shipnode = universe.ShipList.head; shipnode != NULL; shipnode = shipnode->next)
        {
            ship = (Ship *)listGetStructOfNode(shipnode);
            if ((ship->shiptype == bDFGFrigate) &&
                (cat_vecDistance(&node->blast.position, &ship->obj.posinfo.position) <= CAT_BLAST_DFG_RANGE))
            {
                node->blast.damageScale = CAT_BLAST_DFG_FACTOR * node->blast.damageScale;
            }
        }
    }
}

// the shell passes a ship: it is pushed out and spun, thrown into a drift by blast types 5 (and 4 when light), and
// damaged by the distance terms (cat_blastAffectShip 0x00515d30)
static void cat_blastAffectShip(CatBlast *blast, Ship *ship)
{
    vector diff, dir, push;
    real32 distSq, dist, inner, size, pushStrength, damage = 0.0f;

    vecSub(diff, ship->obj.posinfo.position, blast->position);
    distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    inner = blast->radius - blast->thickness;
    if (!((distSq < blast->radius * blast->radius) && (inner * inner < distSq)))
    {
        return;                                             // not in the shell
    }
    dist = fsqrt(distSq);
    size = ship->staticinfo->common.staticCollInfo.collspheresize;
    dir = diff;
    vecNormalize(&dir);
    pushStrength = blast->force * blast->speed + size * size * blast->forceBySize * blast->speed;
    push.x = pushStrength * dir.x;
    push.y = pushStrength * dir.y;
    push.z = pushStrength * dir.z;
    if (cat_univPointInUniverse(&ship->obj.posinfo.position))
    {
        vecAddTo(ship->obj.posinfo.force, push);
        if ((blast->type == CAT_BLAST_TYPE5) ||
            ((blast->type == CAT_BLAST_TYPE4) && (ship->staticinfo->common.mass < CAT_BLAST_LIGHT_SHIP_MASS)))
        {
            damage = 0.0f;
            if ((blast->type == CAT_BLAST_TYPE5) && !ship->catDrifting)
            {
                damage = CAT_BLAST_DRIFT_HEALTH_DAMAGE * ship->staticinfo->health.maxhealth;
            }
            cat_shipStartDrift(ship, 1.0f, 1.0f);           // CAT: 0x005b3b40 (M2-R4 ships-b)
            ship->obj.posinfo.force = push;
            cat_physApplyForceToVelocity(ship);
        }
    }
    ship->rotatable.rotinfo.torque.x = -(CAT_BLAST_TORQUE * dir.x) + ship->rotatable.rotinfo.torque.x;
    ship->rotatable.rotinfo.torque.y = -(CAT_BLAST_TORQUE * dir.y) + ship->rotatable.rotinfo.torque.y;
    ship->rotatable.rotinfo.torque.z = -(CAT_BLAST_TORQUE * dir.z) + ship->rotatable.rotinfo.torque.z;
    if (0.0f < dist)
    {
        // [audit: the exe's order of the sums (0x00515f6e..0x00515fa5): ((damage + const) + k/d) + k/d^2, stored,
        //      then k/sqrt(d) + that; the port had summed the terms in the reverse order]
        damage = ((damage + blast->damageConst) + blast->damageOverDist / dist) + blast->damageOverDistSq / distSq;
        damage = blast->damageOverSqrtDist / fsqrt(dist) + damage;
    }
    // CAT (M2-R5 layout-dock): a large shockwave (type 1) marks the ship with the attribute dword's bit 0x20000
    //      (SpaceObj +0x38, CAT_ATTRIBUTES_BlastHit) and spares a ship carrying bit 0x10000 (CAT_ATTRIBUTES_BlastImmune);
    //      cat_blastAffectShip 0x00515d30, 0x00515fa5..0x00515fd0
    if (blast->type == CAT_BLAST_SHOCKWAVE_LARGE)
    {
        bitSet(ship->obj.attributes, CAT_ATTRIBUTES_BlastHit);
    }
    if (!bitTest(ship->header.flags, SOF_CatShipPart) &&
        ((blast->type != CAT_BLAST_SHOCKWAVE_LARGE) || !bitTest(ship->obj.attributes, CAT_ATTRIBUTES_BlastImmune)))
    {
        ApplyDamageToTarget(&ship->targetable, damage * blast->damageScale, GS_LargeEnergyCannon, DEATH_Killed_By_Player,
                            blast->playerIndex, NULL, 2);
    }
    // CAT: a ramming frigate latched to ship lets go (cat_rammingFrigateForceRelease 0x005c0ae0) [M2-R4 ships-b]
    cat_rammingFrigateForceRelease(ship);
}

// the shell pushes a big-gun shell (cat_blastAffectObject 0x00516da0)
static void cat_blastAffectObject(CatBlast *blast, SpaceObj *obj)
{
    vector diff, dir, force;
    real32 distSq, inner, strength;

    vecSub(diff, obj->posinfo.position, blast->position);
    distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    inner = blast->radius - blast->thickness;
    if (!((distSq < blast->radius * blast->radius) && (inner * inner < distSq)))
    {
        return;
    }
    dir = diff;
    vecNormalize(&dir);
    strength = blast->speed * blast->force;
    force.x = (strength * dir.x + obj->posinfo.force.x) * CAT_BLAST_OBJECT_FORCE_SCALE;
    force.y = (strength * dir.y + obj->posinfo.force.y) * CAT_BLAST_OBJECT_FORCE_SCALE;
    force.z = (strength * dir.z + obj->posinfo.force.z) * CAT_BLAST_OBJECT_FORCE_SCALE;
    vecZeroVector(obj->posinfo.force);
    obj->posinfo.velocity.x = force.x * universe.phystimeelapsed + obj->posinfo.velocity.x;
    obj->posinfo.velocity.y = force.y * universe.phystimeelapsed + obj->posinfo.velocity.y;
    obj->posinfo.velocity.z = force.z * universe.phystimeelapsed + obj->posinfo.velocity.z;
}

// one frame of a blast; FALSE once it has reached its maximum radius (cat_blastUpdate 0x00516020)
static bool32 cat_blastUpdate(CatBlast *blast)
{
    real32 t, azimuth, polar, sinPolar;
    Node *node;
    Ship *ship;
    bool32 affect;
    udword chance;
    vector position, velocity;
    etgeffectstatic *stat;
    matrix coordsys;

    if (!(blast->radius < blast->maxRadius))
    {
        if (blast->soundHandle != -1)
        {
            soundEventStopSound(blast->soundHandle, 0);
            blast->soundHandle = -1;
        }
        return FALSE;
    }
    t = (universe.totaltimeelapsed - blast->delay) - blast->startTime;
    if (t <= 0.0f)
    {
        return TRUE;
    }
    if (!blast->effectSpawned)
    {
        blast->effectSpawned = TRUE;
        blast->radius = blast->startRadius;
        cat_blastSpawnEffect(blast);
    }
    if (cat_blastEffectsRequested)
    {
        cat_blastSpawnEffect(blast);
    }
    blast->radius = t * blast->speed + blast->startRadius;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        affect = TRUE;
        if ((blast->type == CAT_BLAST_TYPE5) &&
            ((ship->shiptype == dNomadMoon) || (ship->shiptype == dNomadFieldEmitter) || (ship->shiptype == dRKO) ||
             (ship->shiprace == RACE_Beast) || (singlePlayerGame && (ship->playerowner->playerIndex == 1))))
        {
            affect = FALSE;
        }
        if ((blast->type == CAT_BLAST_TYPE4) &&
            (allianceIsShipAlly(ship, &universe.players[blast->playerIndex]) || (ship->shiptype == dNomadMoon) ||
             (ship->shiptype == dNomadFieldEmitter) || (ship->shiptype == dRKO)))
        {
            affect = FALSE;
        }
        if (!cat_shipIsUntargetable(ship) && affect)
        {
            cat_blastAffectShip(blast, ship);
        }
    }
    for (node = cat_blastObjectList.head; node != NULL; )
    {
        CatBlastObjectNode *objnode = (CatBlastObjectNode *)listGetStructOfNode(node);

        node = node->next;
        cat_blastAffectObject(blast, objnode->obj);
    }

    // types 4 and 5 throw lightning from the shell now and then
    if ((blast->type == CAT_BLAST_TYPE4) || (blast->type == CAT_BLAST_TYPE5))
    {
        chance = CAT_BLAST_LIGHTNING_CHANCE - 1;
        if ((chance == 0) || ((sdword)chance < 1) || ((ranRandom(RANDOM_STREAM_0) % chance) == 0))
        {
            azimuth = (real32)ranRandom(RANDOM_STREAM_0) * (2.0f * PI / 4294967296.0f);
            polar = cat_blastRandomPolarAngle();
            sinPolar = (real32)sin(polar);
            position.x = blast->radius * ((real32)cos(azimuth) * sinPolar) + blast->position.x;
            position.y = (real32)sin(azimuth) * sinPolar * blast->radius + blast->position.y;
            position.z = blast->radius * (real32)cos(polar) + blast->position.z;
            velocity.x = ((real32)cos(azimuth) * sinPolar) * blast->speed;
            velocity.y = ((real32)sin(azimuth) * sinPolar) * blast->speed;
            velocity.z = (real32)cos(polar) * blast->speed;
            stat = NULL;
            if ((etgSpecialPurposeEffectTable[CAT_BLAST_LIGHTNING_EFFECT] != NULL) &&
                (etgSpecialPurposeEffectTable[CAT_BLAST_LIGHTNING_EFFECT]->nLevels > 0))
            {
                stat = etgSpecialPurposeEffectTable[CAT_BLAST_LIGHTNING_EFFECT]->level[0];
            }
            if (!universePause && (stat != NULL) && !etgFrequencyExceeded(stat))
            {
                coordsys = cat_blastLightningCoordsys;
                etgEffectCreate(stat, NULL, &position, &velocity, &coordsys, 1.0f, EAF_Full, 0);
            }
        }
    }
    // CAT-TODO(stage 3): cat_soundEventBlast 0x0051fc60 (the blast's sound; soundHandle, soundEvent)
    return TRUE;
}

void cat_blastUpdateAll(void)                                           // 0x00516320
{
    Node *node, *next;

    for (node = cat_blastList.head; node != NULL; node = next)
    {
        next = node->next;
        if (!cat_blastUpdate(&((CatBlastNode *)listGetStructOfNode(node))->blast))
        {
            // CAT: the exe's listRemoveNode leaves the blast allocated; it is freed here
            listDeleteNode(node);
        }
    }
    cat_blastEffectsRequested = FALSE;
}

// after a load every blast spawns its effect again (cat_loadUniverseList5e288 sets 0x00a67010)
void cat_blastRequestEffects(void)
{
    cat_blastEffectsRequested = TRUE;
}

void cat_bigGunBulletTrack(Bullet *bullet)                              // 0x00516f20
{
    CatBlastObjectNode *node;

    if (cat_isBigGunBullet((SpaceObj *)bullet))
    {
        node = memAlloc(sizeof(CatBlastObjectNode), "CatBlastObj", 0);
        node->obj = (SpaceObj *)bullet;
        listAddNode(&cat_blastObjectList, &node->link, node);
    }
}

void cat_bigGunBulletUntrack(Bullet *bullet)                            // 0x00516f50
{
    Node *node, *next;

    if (cat_isBigGunBullet((SpaceObj *)bullet))
    {
        for (node = cat_blastObjectList.head; node != NULL; node = next)
        {
            next = node->next;
            if (((CatBlastObjectNode *)listGetStructOfNode(node))->obj == (SpaceObj *)bullet)
            {
                // CAT: the exe's listRemoveNode leaves the entry allocated; it is freed here
                listDeleteNode(node);
            }
        }
    }
}

void cat_bigGunBulletRebuildList(void)                                  // 0x00516d60
{
    Node *node;
    SpaceObj *obj;
    CatBlastObjectNode *objnode;

    for (node = universe.BulletList.head; node != NULL; node = node->next)
    {
        obj = (SpaceObj *)listGetStructOfNode(node);
        if (cat_isBigGunBullet(obj))
        {
            objnode = memAlloc(sizeof(CatBlastObjectNode), "CatBlastObj", 0);
            objnode->obj = obj;
            listAddNode(&cat_blastObjectList, &objnode->link, objnode);
        }
    }
}

bool32 cat_bigGunBulletsInFlight(void)                                  // 0x00516d40
{
    return cat_blastObjectList.num != 0;
}
