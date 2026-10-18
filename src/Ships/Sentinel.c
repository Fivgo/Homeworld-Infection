// =============================================================================
//  Sentinel.c
//  - Cataclysm's sSentinel / bSentinel: in a force-field formation they span
//    a shield of triangles that soaks up hits (cataclysm.exe 0x005c5af0-0x005c6fa0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each).
//      cat_shipIsSentinelFieldActive 0x005c6e50 and cat_shipIsSentinelInField 0x005c6ec0 moved here from ShipSelect.c
//      (their declarations stay in ShipSelect.h). The field's drawing (cat_sentinelFieldSegmentAdd 0x005c5b20,
//      cat_sentinelFieldsRenderBehind / RenderRest, cat_sentinelFieldQueueSegment 0x005c5ec0, cat_sentinelFieldQueue
//      0x005c67b0, cat_sentinelFieldSegmentDraw 0x005c60a0) is at the end of the file (M2-R5 render).

#include "Sentinel.h"

#include <math.h>

#include "Attack.h"
#include "Clamp.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "DefaultShip.h"
#include "Dreadnought.h"
#include "ETG.h"
#include "FastMath.h"
#include "Formation.h"
#include "FormationDefs.h"
#include "Matrix.h"
#include "MeshAnim.h"
#include "SaveGame.h"
#include "ShipSelect.h"
#include "StatScript.h"
#include "UnivUpdate.h"
#include "Universe.h"

#include "Color.h"
#include "glinc.h"
#include "Particle.h"
#include "prim3d.h"
#include "render.h"
#include "texreg.h"
#include "Randy.h"
#include "Trails.h"
extern trhandle g_glowHandle;      // Trails.c (0x00a761e0, trailStartup)

AttackSideStepParameters SentinelStaticSect;    // 0x00a7a5e0: sSentinel
AttackSideStepParameters SentinelStaticBeast;   // 0x00a7a5f0: bSentinel

static const real32 sentinelMaxCharge[3] = { 4000.0f, 6000.0f, 10000.0f };     // 0x008c6808
static const real32 sentinelChargeRate[3] = { 4.0f, 8.0f, 16.0f };             // 0x008c6814

#define SENTINEL_IN_POSITION        50.0f       // 0x008c67b8
#define SENTINEL_LEVEL_RATE         0.2f        // 0x008c67c4
#define SENTINEL_FADE_RATE          0.6f        // 0x008c67c8

static bool32 sentinelIsSentinel(Ship *ship)
{
    return (ship->shiptype == sSentinel) || (ship->shiptype == bSentinel);
}

bool32 cat_shipIsLiveSentinel(Ship *ship)                                                   // 0x005c5af0
{
    return (ship->header.objtype == OBJ_ShipType) && sentinelIsSentinel(ship);
}

void SentinelStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)     // 0x005c5d10
{
    AttackSideStepParameters *sentinelstat = (statinfo->shiprace != RACE_Sect) ? &SentinelStaticBeast : &SentinelStaticSect;

    scriptSetStruct(directory,filename,AttackSideStepParametersScriptTable,(ubyte *)sentinelstat);
    statinfo->custstatinfo = sentinelstat;
    statinfo->canSpecialBandBoxFriendlies = TRUE;
}

void SentinelInit(Ship *ship)                                                               // 0x005c5d60
{
    SentinelSpec *spec = (SentinelSpec *)ship->ShipSpecifics;

    // (the exe runs an empty 3-step loop first)
    spec->fieldActive = FALSE;
    spec->fieldLevel = 0.0f;
    spec->numLinks = 0;
    spec->unknown338 = 0;
    spec->disrupted = FALSE;
    spec->disruptEndTime = 0.0f;
}

void SentinelAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)                  // 0x005c5da0
{
    attackStraightForward(ship, target, maxdist, 1000.0f);
}

void SentinelAttackPassive(Ship *ship,Ship *target,bool32 rotate)                          // 0x005c5dd0
{
    // a Sentinel in a sphere does not fight back
    if ((ship->command != NULL) && (ship->command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
        (ship->command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION))
    {
        return;
    }
    if (rotate & (bool32)ship->staticinfo->rotateToRetaliate)
    {
        attackPassiveRotate(ship, target);
        return;
    }
    attackPassive(ship, target);
}

// the formation leader's field update: who is in position, which triangles are up, the field level; a sphere's
// Sentinels clamp to the protected ship while their field is up
static void cat_sentinelFormationUpdate(Ship *ship)                                        // 0x005c6830
{
    CommandToDo *command = ship->command;
    SelectCommand *formation;
    Ship *centre;
    Ship *member;
    SentinelSpec *spec, *aspec, *bspec;
    vector offset;
    vector diff;
    sdword i, j;
    bool32 active;

    if ((command != NULL) && (command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
        ((command->formation.formationtype == FORCEFIELD_WALL_FORMATION) ||
         (command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION)) &&
        (command->selection->ShipPtr[0] == ship))
    {
        centre = ship;
        if (command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION)
        {
            // a sphere needs a protected ship in the universe, else it falls back to a wall
            if (!(command->ordertype.attributes & COMMAND_MASK_PROTECTING))
            {
                command->formation.formationtype = FORCEFIELD_WALL_FORMATION;
                formationContentHasChanged(ship->command);
            }
            else
            {
                centre = command->protect->ShipPtr[0];
            }
            if (centre->shiplink.belongto == NULL)
            {
                ship->command->formation.formationtype = FORCEFIELD_WALL_FORMATION;
                formationContentHasChanged(ship->command);
                centre = ship;
            }
        }
        formation = ship->command->selection;
        for (i = 0; i < formation->numShips; i++)
        {
            ((SentinelSpec *)formation->ShipPtr[i]->ShipSpecifics)->fieldActive = FALSE;
        }
        for (i = 0; i < formation->numShips; i++)
        {
            member = formation->ShipPtr[i];
            spec = (SentinelSpec *)member->ShipSpecifics;
            matMultiplyMatByVec(&offset, &centre->rotatable.rotinfo.coordsys, &member->formationOffset);
            diff.x = (member->obj.posinfo.position.x - centre->impactable.collInfo.collPosition.x) - offset.x;
            diff.y = (member->obj.posinfo.position.y - centre->impactable.collInfo.collPosition.y) - offset.y;
            diff.z = (member->obj.posinfo.position.z - centre->impactable.collInfo.collPosition.z) - offset.z;
            spec->inPosition = (fabs((double)fsqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z)) <=
                                SENTINEL_IN_POSITION);
            spec->fieldActive = FALSE;
        }
        for (i = 0; i < formation->numShips; i++)
        {
            member = formation->ShipPtr[i];
            spec = (SentinelSpec *)member->ShipSpecifics;
            active = spec->inPosition ? TRUE : FALSE;
            if (spec->inPosition)
            {
                for (j = 0; j < spec->numLinks; j++)
                {
                    if (!((SentinelSpec *)spec->links[j].a->ShipSpecifics)->inPosition ||
                        !((SentinelSpec *)spec->links[j].b->ShipSpecifics)->inPosition)
                    {
                        active = FALSE;
                    }
                }
            }
            if ((formation->numShips < 3) || spec->depleted || member->catEmpStunned || spec->disrupted || !active)
            {
                active = FALSE;
                spec->fieldLevel = 0.0f;
            }
            else
            {
                if (spec->fieldLevel == 0.0f)
                {
                    spec->fade = 1.0f;
                }
                spec->fieldLevel = universe.phystimeelapsed * SENTINEL_LEVEL_RATE + spec->fieldLevel;
                if (spec->fieldLevel > 1.0f)
                {
                    spec->fieldLevel = 1.0f;
                    if (spec->fade > 0.0f)
                    {
                        spec->fade = spec->fade - SENTINEL_FADE_RATE * universe.phystimeelapsed;
                        if (spec->fade < 0.0f)
                        {
                            spec->fade = 0.0f;
                        }
                    }
                }
            }
            spec->fieldActive = active;
            // the first partner of each triangle that is still down takes this Sentinel's state
            for (j = 0; j < spec->numLinks; j++)
            {
                aspec = (SentinelSpec *)spec->links[j].a->ShipSpecifics;
                bspec = (SentinelSpec *)spec->links[j].b->ShipSpecifics;
                if (!aspec->fieldActive)
                {
                    aspec->fieldActive = active;
                }
                else if (!bspec->fieldActive)
                {
                    bspec->fieldActive = active;
                }
            }
        }
        if (ship->command->formation.formationtype != FORCEFIELD_SPHERE_FORMATION)
        {
            return;
        }
        for (i = 0; i < formation->numShips; i++)
        {
            member = formation->ShipPtr[i];
            if (!((SentinelSpec *)member->ShipSpecifics)->fieldActive)
            {
                unClampObj((SpaceObjRotImpTargGuidance *)member);
            }
            else if ((member->guidance.clampInfo == NULL) || (member->guidance.clampInfo->host == NULL))
            {
                clampObjToObj((SpaceObjRotImpTargGuidance *)member,
                              (SpaceObjRotImpTargGuidance *)member->command->protect->ShipPtr[0]);
            }
        }
        return;
    }
    if ((command != NULL) && (command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION))
    {
        return;
    }
    ((SentinelSpec *)ship->ShipSpecifics)->fieldActive = FALSE;
    unClampObj((SpaceObjRotImpTargGuidance *)ship);
}

void SentinelHouseKeep(Ship *ship)                                                          // 0x005c5e20
{
    SentinelSpec *spec = (SentinelSpec *)ship->ShipSpecifics;
    real32 max = sentinelMaxCharge[spec->chargeLevel];
    real32 rate;

    // the charge refills (half as much in single player); a full charge ends the depletion
    if (singlePlayerGame)
    {
        max *= 0.5f;
    }
    if (spec->charge < max)
    {
        rate = sentinelChargeRate[spec->chargeLevel];
        if (singlePlayerGame)
        {
            rate *= 0.5f;
        }
        spec->charge = rate + spec->charge;
    }
    if (max <= spec->charge)
    {
        spec->depleted = FALSE;
    }
    if ((universe.univUpdateCounter & 0xc) != 0)
    {
        cat_sentinelFormationUpdate(ship);
    }
}

real32 cat_sentinelFieldChargeFraction(SentinelSpec *spec)                                 // 0x005c5ea0
{
    return spec->charge / sentinelMaxCharge[spec->chargeLevel];
}

// a bullet (or beam) hits the field: its damage against the Sentinel's armour, rounded (the exe's 1.5 * 2^52 trick),
// comes off the charge
void cat_sentinelFieldBulletHit(Ship *sentinel, void *unused, Bullet *bullet)              // 0x005c6c60
{
    SentinelSpec *spec = (SentinelSpec *)sentinel->ShipSpecifics;
    real32 damage;

    (void)unused;
    spec->lastHitTime = universe.totaltimeelapsed;
    damage = cat_weaponDamageVsArmour(((Bullet *)bullet)->soundType, sentinel->catArmourLevel) *
             ((Bullet *)bullet)->damage;
    spec->charge = spec->charge - (real32)(sdword)lrint((double)damage);
    if (spec->charge < 0.0f)
    {
        spec->depleted = TRUE;
    }
}

// a missile hits the field: its hit effect at the missile, its damage (truncated) off the charge
void cat_sentinelFieldMissileHit(Ship *sentinel, void *unused, Missile *missile)            // 0x005c6cd0
{
    SentinelSpec *spec = (SentinelSpec *)sentinel->ShipSpecifics;
    Missile *m = (Missile *)missile;
    etglod *etgLOD = m->hitEffect;
    etgeffectstatic *stat = NULL;

    (void)unused;
    spec->lastHitTime = universe.totaltimeelapsed;
    if ((etgLOD != NULL) && (sentinel->obj.currentLOD < etgLOD->nLevels))
    {
        stat = etgLOD->level[sentinel->obj.currentLOD];
    }
    if (univSpaceObjInRenderList(&sentinel->obj) && (stat != NULL) && etgHitEffectsEnabled && !etgFrequencyExceeded(stat))
    {
        etgEffectCreate(stat, NULL, &m->obj.posinfo.position, &sentinel->obj.posinfo.velocity,
                        &m->rotatable.rotinfo.coordsys, sentinel->magnitudeSquared, 0, 2, 100, 0);
    }
    spec->charge = spec->charge - (real32)(sdword)m->damage;
    if (spec->charge < 0.0f)
    {
        spec->depleted = TRUE;
    }
}

void SentinelRemoveShipReferences(Ship *ship,Ship *shiptoremove)                            // 0x005c6d90
{
    SentinelSpec *spec = (SentinelSpec *)ship->ShipSpecifics;
    sdword i;

    // a triangle with the removed ship goes; the last one takes its place (the moved one is not checked again)
    for (i = 0; i < spec->numLinks; i++)
    {
        if ((spec->links[i].a == shiptoremove) || (spec->links[i].b == shiptoremove))
        {
            if (i < spec->numLinks - 1)
            {
                // CAT: the exe copies from (count * 8) past +0x63C / +0x640, i.e. the entry one past the last
                //      (0x005c6dbb); kept
                spec->links[i] = spec->links[spec->numLinks];
            }
            spec->numLinks--;
        }
    }
}

bool32 SentinelFormationCommand(Ship *ship, sdword formation)                               // 0x005c6df0
{
    if ((formation != FORCEFIELD_WALL_FORMATION) && (formation != FORCEFIELD_SPHERE_FORMATION))
    {
        bitClear(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSentinelFormation);
        madAnimationStart(ship, 1, 2);       // CAT: the exe's third argument, 2 (M2-R6 nis-fow-camera)
        return TRUE;
    }
    bitSet(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSentinelFormation);
    madAnimationStart(ship, 0, 0);           // CAT: the exe's third argument, 0 (M2-R6 nis-fow-camera)
    return TRUE;
}

bool32 cat_shipIsSentinelFieldActive(Ship *ship)                                            // 0x005c6e50
{
    SentinelSpec *spec = (SentinelSpec *)ship->ShipSpecifics;

    return (spec->fieldActive == 1) && (spec->fieldLevel == 1.0f);
}

bool32 cat_shipIsSentinelInSphere(Ship *ship)                                               // 0x005c6e80
{
    return sentinelIsSentinel(ship) && (ship->command != NULL) &&
           (ship->command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
           (ship->command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION);
}

bool32 cat_shipIsSentinelInField(Ship *ship)                                                // 0x005c6ec0
{
    return sentinelIsSentinel(ship) && (ship->command != NULL) &&
           (ship->command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
           ((ship->command->formation.formationtype == FORCEFIELD_SPHERE_FORMATION) ||
            (ship->command->formation.formationtype == FORCEFIELD_WALL_FORMATION));
}

sdword cat_sentinelFieldLinks(Ship *sentinel, SentinelLink **links)
{
    SentinelSpec *spec = (SentinelSpec *)sentinel->ShipSpecifics;

    *links = spec->links;
    return spec->numLinks;
}

void Sentinel_PreFix(Ship *ship)                                                            // 0x005c6f00
{
    SentinelSpec *spec = (SentinelSpec *)ship->ShipSpecifics;
    sdword i;

    for (i = 0; i < spec->numLinks; i++)
    {
        spec->links[i].a = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->links[i].a);
        spec->links[i].b = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->links[i].b);
    }
}

void Sentinel_Fix(Ship *ship)                                                               // 0x005c6f50
{
    SentinelSpec *spec = (SentinelSpec *)ship->ShipSpecifics;
    sdword i;

    for (i = 0; i < spec->numLinks; i++)
    {
        spec->links[i].a = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->links[i].a);
        spec->links[i].b = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->links[i].b);
    }
}

// CAT: SentinelHeader 0x008c6830 (sSentinel, bSentinel); slot 5 is DreadnoughtFire, slots 7 and 8 kasfSpecialOff, slot
//      34 the formation command
CustShipHeader SentinelHeader =
{
    .sizeofShipSpecifics = sizeof(SentinelSpec),
    .CustShipStaticInit = SentinelStaticInit,
    .CustShipInit = SentinelInit,
    .CustShipFire = DreadnoughtFire,
    .CustShipAttackPassive = SentinelAttackPassive,
    .CustShipSpecialActivate = kasfSpecialOffShip,
    .CustShipSpecialTarget = kasfSpecialOffTarget,
    .CustShipHousekeep = SentinelHouseKeep,
    .CustShipRemoveShipReferences = SentinelRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))SentinelRemoveShipReferences,
    .CustShip_PreFix = Sentinel_PreFix,
    .CustShip_Fix = Sentinel_Fix,
    .CustShipAttack = SentinelAttack,
    .CustShipFormationCommand = SentinelFormationCommand,
};

/*=============================================================================
    CAT (M2-R5 render): the force field's drawing. The main view queues every Sentinel formation's triangles once a
    frame (cat_sentinelFieldQueue from cat_rndRenderObjectList), sorted far to near by the distance of their centres
    from the eye; the sorted pass draws the ones behind each blended object first (cat_sentinelFieldsRenderBehind)
    and the rest at the end (cat_sentinelFieldsRenderRest).
=============================================================================*/
// [audit: M2-R5 render] the cap is 0x22 + (RGLtype != 0 ? 0x2d : 0) (0x005c5b20 reads RGLtype 0x00b3a700, not RGL
//      0x00b3a6d8): 34 only on the software renderer (SWtype 0), 79 on GLtype, which this tree is (CAT_RND_RGLTYPE)
#define CAT_SENTINEL_SEGMENTS_GL        (0x22 + 0x2d)   // the queue holds 79 segments (34 on the software renderer)
typedef struct CatSentinelSegment                   // 0x38 bytes (0x00a77b98)
{
    SentinelSpec *a, *b, *c;
    vector pa, pb, pc;
    real32 distance;
    struct CatSentinelSegment *next;
} CatSentinelSegment;
static CatSentinelSegment catSentinelSegments[CAT_SENTINEL_SEGMENTS_GL];
static CatSentinelSegment *catSentinelQueue = NULL;     // 0x00a78d18: farthest first
static sdword catSentinelSegmentCount = 0;              // 0x00a78d1c
static bool32 catSentinelDrawing = FALSE;               // 0x00a78d20 (no reader)
// the edge colours (0x00a78d28..0x00a78d3c, .bss with no writer: always 0) blended to the tables of 0x008c67e0:
// per charge level {edge, mid, fill}
static const color catSentinelEdgeBase[3][2] = { {0, 0}, {0, 0}, {0, 0} };
static const color catSentinelColors[3][3] =
{
    { 0xffff0000, 0xffff0000, 0x64c80000 },
    { 0xff00ff00, 0xff00ff00, 0x6400c800 },
    { 0xff0000ff, 0xff0000ff, 0x640000c8 },
};
#define CAT_SENTINEL_FLASH_COLOR        0x96ffffffu     // 0x008c6804
#define CAT_SENTINEL_VERTEX_COLOR       0xff0000ffu     // 0x008c67dc
#define CAT_SENTINEL_EDGE_LEVEL         0.75f   // 0x008c67cc
#define CAT_SENTINEL_MID_LEVEL          0.25f   // 0x008c67d0
#define CAT_SENTINEL_VERTEX_LEVEL       0.65f   // 0x008c67d4
#define CAT_SENTINEL_VERTEX_SIZE        4.0f    // 0x008c67d8
#define CAT_SENTINEL_HIT_TIME           10.0f   // 0x008c67c0

// queue a segment, keeping the list sorted far to near; a full queue replaces its farthest segment if this one is
// nearer
static void cat_sentinelFieldSegmentAdd(Camera *camera, SentinelSpec *a, SentinelSpec *b, SentinelSpec *c,
                                        vector *pa, vector *pb, vector *pc, real32 distance)        // 0x005c5b20
{
    CatSentinelSegment *seg, *node, *prev;

    (void)camera;
    if (catSentinelSegmentCount < CAT_SENTINEL_SEGMENTS_GL)
    {
        seg = &catSentinelSegments[catSentinelSegmentCount++];
    }
    else
    {
        if (catSentinelQueue->distance <= distance)
        {
            return;
        }
        seg = catSentinelQueue;
        catSentinelQueue = catSentinelQueue->next;
    }
    if (seg == NULL)
    {
        return;
    }
    prev = NULL;
    for (node = catSentinelQueue; node != NULL; node = node->next)
    {
        if (node->distance < distance)
        {
            break;
        }
        prev = node;
        if (node->next == NULL)
        {
            break;
        }
    }
    if (prev != NULL)
    {
        seg->next = prev->next;
        prev->next = seg;
    }
    else
    {
        seg->next = catSentinelQueue;
        catSentinelQueue = seg;
    }
    seg->a = a;
    seg->b = b;
    seg->c = c;
    seg->pa = *pa;
    seg->pb = *pb;
    seg->pc = *pc;
    seg->distance = distance;
}

// the field emitter's position: the static's dock point of type 0x19 in world space
static void cat_sentinelEmitterPosition(vector *out, Ship *ship)
{
    DockStaticInfo *dock = ship->staticinfo->dockStaticInfo;
    sbyte index = (sbyte)dock->firstOfType[0x19];

    matMultiplyMatByVec(out, &ship->rotatable.rotinfo.coordsys, &dock->dockstaticpoints[index].position);
    vecAddTo(*out, ship->obj.posinfo.position);
}

static void cat_sentinelFieldQueueSegment(Camera *camera, Ship *a, Ship *b, Ship *c)          // 0x005c5ec0
{
    vector pa, pb, pc, centre;

    if (((SentinelSpec *)a->ShipSpecifics)->fieldLevel > 0.0)
    {
        cat_sentinelEmitterPosition(&pa, a);
        cat_sentinelEmitterPosition(&pb, b);
        cat_sentinelEmitterPosition(&pc, c);
        centre.x = (pb.x + pa.x + pc.x) * (1.0f / 3.0f) - camera->eyeposition.x;
        centre.y = (pb.y + pa.y + pc.y) * (1.0f / 3.0f) - camera->eyeposition.y;
        centre.z = (pb.z + pa.z + pc.z) * (1.0f / 3.0f) - camera->eyeposition.z;
        cat_sentinelFieldSegmentAdd(camera, (SentinelSpec *)a->ShipSpecifics, (SentinelSpec *)b->ShipSpecifics,
                                    (SentinelSpec *)c->ShipSpecifics, &pa, &pb, &pc,
                                    fsqrt(centre.x * centre.x + centre.y * centre.y + centre.z * centre.z));
    }
}

void cat_sentinelFieldQueue(Camera *camera, struct CommandToDo *formation)                    // 0x005c67b0
{
    SelectCommand *selection = formation->selection;
    SentinelSpec *spec;
    sdword i, j;

    for (i = 0; i < selection->numShips; i++)
    {
        spec = (SentinelSpec *)selection->ShipPtr[i]->ShipSpecifics;
        for (j = 0; j < spec->numLinks; j++)
        {
            cat_sentinelFieldQueueSegment(camera, selection->ShipPtr[i], spec->links[j].a, spec->links[j].b);
        }
    }
}

// the midpoint as the exe's x87 code rounds it: x in extended precision, y and z through float temporaries
static void cat_sentinelMidpoint(vector *m, vector *a, vector *b)
{
    real32 dy = b->y - a->y, dz = b->z - a->z;

    m->x = (real32)(((real64)b->x - (real64)a->x) * 0.5 + (real64)a->x);
    dy = (real32)((real64)dy * 0.5);
    dz = (real32)((real64)dz * 0.5);
    m->y = dy + a->y;
    m->z = dz + a->z;
}

// one segment: its two edges from the first emitter (bright at the emitters, fading toward the middle as the field
// comes up), the filled triangle once the field is up (flickering while it comes up, flashing when hit) and a glow
// on each emitter
static void cat_sentinelFieldSegmentDraw(CatSentinelSegment *seg)                               // 0x005c60a0
{
    SentinelSpec *spec = seg->a;
    real32 sinceHit = universe.totaltimeelapsed - spec->lastHitTime;
    real32 f, g, x, size;
    color edge = 0, mid = 0, fill = 0, c;
    vector m, zero = {0.0f, 0.0f, 0.0f};
    bool32 depth;
    sdword level = spec->chargeLevel;

    rndAdditiveBlends(TRUE);
    glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    rndLightingEnable(FALSE);
    rndTextureEnable(FALSE);
    glBegin(GL_LINES);
    f = spec->fieldLevel / CAT_SENTINEL_EDGE_LEVEL;
    if (!(f < 1.0f))
    {
        f = 1.0f;
    }
    if (level >= 0 && level <= 2)
    {
        edge = cat_colBlendRGBA(catSentinelEdgeBase[level][0], catSentinelColors[level][0], 1.0f - f);
    }
    g = (spec->fieldLevel - CAT_SENTINEL_EDGE_LEVEL) / CAT_SENTINEL_MID_LEVEL;
    if (!(g < 1.0f))
    {
        g = 1.0f;
    }
    else if (!(g > 0.0f))
    {
        g = 0.0f;
    }
    if (level >= 0 && level <= 2)
    {
        mid = cat_colBlendRGBA(catSentinelEdgeBase[level][1], catSentinelColors[level][1], 1.0f - g);
    }
    cat_sentinelMidpoint(&m, &seg->pa, &seg->pb);
    glColor4ub(colRed(edge), colGreen(edge), colBlue(edge), colAlpha(edge));
    glVertex3f(seg->pa.x, seg->pa.y, seg->pa.z);
    glColor4ub(colRed(mid), colGreen(mid), colBlue(mid), colAlpha(mid));
    glVertex3f(m.x, m.y, m.z);
    glVertex3f(m.x, m.y, m.z);
    glColor4ub(colRed(edge), colGreen(edge), colBlue(edge), colAlpha(edge));
    glVertex3f(seg->pb.x, seg->pb.y, seg->pb.z);
    cat_sentinelMidpoint(&m, &seg->pa, &seg->pc);
    glColor4ub(colRed(edge), colGreen(edge), colBlue(edge), colAlpha(edge));
    glVertex3f(seg->pa.x, seg->pa.y, seg->pa.z);
    glColor4ub(colRed(mid), colGreen(mid), colBlue(mid), colAlpha(mid));
    glVertex3f(m.x, m.y, m.z);
    glVertex3f(m.x, m.y, m.z);
    glColor4ub(colRed(edge), colGreen(edge), colBlue(edge), colAlpha(edge));
    glVertex3f(seg->pc.x, seg->pc.y, seg->pc.z);
    glEnd();

    if (spec->fieldLevel == 1.0)
    {
        fill = (level >= 0 && level <= 2) ? catSentinelColors[level][2] : mid;
        x = 1.0f - spec->fade;
        if (x < 0.5f)
        {
            c = cat_colBlendRGBA(CAT_SENTINEL_FLASH_COLOR, fill,
                                 (real32)((real64)ranRandom(RANDOM_STREAM_0) * (1.0 / 4294967296.0)));
        }
        else if (x < 0.8f)
        {
            c = CAT_SENTINEL_FLASH_COLOR;
        }
        else
        {
            c = cat_colBlendRGBA(CAT_SENTINEL_FLASH_COLOR, fill, (1.0f - x) * 5.0f);   // 0x0072c010
        }
        rndAdditiveBlends(TRUE);
        glEnable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        rndLightingEnable(FALSE);
        rndTextureEnable(FALSE);
        if (sinceHit <= CAT_SENTINEL_HIT_TIME)
        {
            real32 v = (CAT_SENTINEL_HIT_TIME - sinceHit) / CAT_SENTINEL_HIT_TIME;
            if (v > 0.0f && level >= 0 && level <= 2)
            {
                c = cat_colBlendRGBA(CAT_SENTINEL_FLASH_COLOR, catSentinelColors[level][2], v);
            }
        }
        glColor4ub(colRed(c), colGreen(c), colBlue(c), colAlpha(c));
        glBegin(GL_TRIANGLES);
        glVertex3f(seg->pa.x, seg->pa.y, seg->pa.z);
        glVertex3f(seg->pb.x, seg->pb.y, seg->pb.z);
        glVertex3f(seg->pc.x, seg->pc.y, seg->pc.z);
        glEnd();

        g = spec->fieldLevel / CAT_SENTINEL_VERTEX_LEVEL;
        if (!(g <= 1.0f))
        {
            g = 1.0f;
        }
        size = CAT_SENTINEL_VERTEX_SIZE * g;
        depth = cat_rndDepthTestEnable(FALSE);
        rndTextureEnvironment(RTE_Modulate);
        if (g_glowHandle != TR_InvalidHandle)
        {
            trMakeCurrent(g_glowHandle);
            partFilter(TRUE);
            rndBillboardEnable(&seg->pa);
            primSolidTexture3(&zero, size, CAT_SENTINEL_VERTEX_COLOR, g_glowHandle);
            rndBillboardDisable();
            rndBillboardEnable(&seg->pb);
            primSolidTexture3(&zero, size, CAT_SENTINEL_VERTEX_COLOR, g_glowHandle);
            rndBillboardDisable();
            rndBillboardEnable(&seg->pc);
            primSolidTexture3(&zero, size, CAT_SENTINEL_VERTEX_COLOR, g_glowHandle);
        }
        else
        {
            rndBillboardEnable(&seg->pa);
            primCircleSolid3(&zero, size, 16, CAT_SENTINEL_VERTEX_COLOR);
            rndBillboardDisable();
            rndBillboardEnable(&seg->pb);
            primCircleSolid3(&zero, size, 16, CAT_SENTINEL_VERTEX_COLOR);
            rndBillboardDisable();
            rndBillboardEnable(&seg->pc);
            primCircleSolid3(&zero, size, 16, CAT_SENTINEL_VERTEX_COLOR);
        }
        rndBillboardDisable();
        cat_rndDepthTestEnable(depth);
    }
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    rndLightingEnable(TRUE);
    catRndCounts.fieldSegments++;
}

void cat_sentinelFieldsRenderBehind(Camera *camera, SpaceObj *obj)                            // 0x005c5c20
{
    real32 distance = 0.0f;

    catSentinelDrawing = TRUE;
    if (obj != NULL)
    {
        vector d;
        vecSub(d, obj->posinfo.position, camera->eyeposition);
        distance = fsqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    }
    for (; catSentinelQueue != NULL && distance < catSentinelQueue->distance; catSentinelQueue = catSentinelQueue->next)
    {
        cat_sentinelFieldSegmentDraw(catSentinelQueue);
    }
}

void cat_sentinelFieldsRenderRest(void)                                                        // 0x005c5cb0
{
    catSentinelDrawing = TRUE;
    for (; catSentinelQueue != NULL && 0.0f < catSentinelQueue->distance; catSentinelQueue = catSentinelQueue->next)
    {
        cat_sentinelFieldSegmentDraw(catSentinelQueue);
    }
    catSentinelDrawing = FALSE;
    catSentinelSegmentCount = 0;
    catSentinelQueue = NULL;
}
