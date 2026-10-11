// =============================================================================
//  RammingFrigate.c
//  - Cataclysm's sRammingFrigate / bRammingFrigate: rams a target, latches
//    on and pushes it away (cataclysm.exe 0x005bf760-0x005c21e0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each).
//      cat_univPointInUniverse 0x005c1f70 sits in this block of the exe; it was ported to UnivUpdate.c by M2-R4.

#include "RammingFrigate.h"

#include "ACV.h"
#include "AIShip.h"
#include "AITrack.h"
#include "Alliance.h"
#include "Battle.h"
#include "BeastWeapon.h"
#include "Clamp.h"
#include "ClassDefs.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "CommandWrap.h"
#include "DefaultShip.h"
#include "Dissolve.h"
#include "Dock.h"
#include "ETG.h"
#include "FastMath.h"
#include "Globals.h"
#include "Leech.h"
#include "Matrix.h"
#include "MeshAnim.h"
#include "Mothership.h"
#include "Physics.h"
#include "Salvage.h"
#include "SaveGame.h"
#include "Sensors.h"
#include "Sentinel.h"
#include "ShipSelect.h"
#include "SoundEvent.h"
#include "Trails.h"
#include "UnivUpdate.h"
#include "Universe.h"

RammingFrigateStatics RammingFrigateStaticSect;     // 0x00a7a700: sRammingFrigate
RammingFrigateStatics RammingFrigateStaticBeast;    // 0x00a7a6e0: bRammingFrigate

#define RAM_CHARGE_TIME             25.0f       // 0x008c5d98
#define RAM_TRAIL_SCALE             1.3f        // 0x008c5d9c
#define RAM_TRAIL_SCALE_UPGRADED    4.0f        // 0x008c5da0
#define RAM_BLEND_NEW               0.07f       // 0x008c5da4
#define RAM_BLEND_OLD               1.0f        // 0x008c5da8
#define RAM_SPEED_CAP               400.0f      // 0x008c5dac
#define RAM_SPEED_CHANGE_CAP        0.1f        // 0x008c5db0
#define RAM_SPEED_MULT              1.2f        // 0x008c5db4
#define RAM_SPEED_MULT_UPGRADED     2.0f        // 0x008c5db8
#define RAM_RANGE_BIG               2000.0f     // 0x008c5dbc
#define RAM_RANGE_SMALL             600.0f      // 0x008c5dc0
#define RAM_RANGE_GIVE_UP           700.0f      // 0x008c5dc4
#define RAM_FACING_COS              0.5f        // 0x008c5dc8
#define RAM_KEEP_COS                0.34f       // 0x008c5dcc
#define RAM_LOS_RETRY               4.0f        // 0x008c5dd0
#define RAM_MESH_PROBE              500.0f      // 0x008c5dd8
#define RAM_PROBE_BACK              -1500.0f    // 0x008c5ddc
#define RAM_THRUST_TIME             0.8f        // 0x008c5de0
#define RAM_THRUST_DELAY            0.3f        // 0x008c5de4
#define RAM_THRUST_LODS             3           // 0x008c5dfc
#define RAM_BEAM_X                  -75.0f      // 0x008c5e14
#define RAM_BEAM_Z                  -25.0f      // 0x008c5e18
#define RAM_BEAM_LENGTH             2000.0f     // 0x008c5e1c
#define RAM_PUSH_FORCE              500000.0f   // 0x008c5e24
#define RAM_PUSH_FORCE_UPGRADED     800000.0f   // 0x008c5e28
#define RAM_PUSH_MINDOT             0.2f        // 0x008c5e2c
#define RAM_PUSH_ROT                400.0f      // 0x008c5e30
#define RAM_SPEED_MULT_RAMMING      0.6f        // 0x008c5e34
#define RAM_TURN_ACCURACY           0.99f       // 0x00878148
#define RAM_CHASE                   99999.0f    // 0x0072bfe0

// sMothership or bMothership (the exe's cat_shipIsMothership 0x005298c0, linked with the tactics code)
static void cat_rammingFrigatePushTarget(Ship *ship);
static void cat_rammingFrigateCheckCollision(Ship *rammer, Ship *other);

static bool32 cat_shipIsMothership(Ship *ship)
{
    return (ship->shiptype == sMothership) || (ship->shiptype == bMothership);
}

bool32 cat_shipImmuneToRamming(Ship *ship)                                                  // 0x005bf760
{
    return (ship->shiptype == dRKO) || (ship->shiptype == dNomadMoon) || (ship->shiptype == dNomadFieldEmitter);
}

static bool32 ramInfects(Ship *ship)
{
    return (ship->shiptype == dRKO) || (ship->shiptype == dNomadMoon) || (ship->shiptype == dNomadFieldEmitter) ||
           cat_shipHasBeastWeapon(ship);
}

// rams that hit this ship (or the ship it is a part of) get infected
bool32 cat_shipInfectsOnRam(Ship *ship)                                                     // 0x005bf790
{
    Ship *host;

    if ((ship->header.objtype == OBJ_ShipType) && ramInfects(ship))
    {
        return TRUE;
    }
    if ((ship->guidance.clampInfo != NULL) && ((host = (Ship *)ship->guidance.clampInfo->host) != NULL) &&
        (host->header.objtype == OBJ_ShipType) && ramInfects(host))
    {
        return TRUE;
    }
    return FALSE;
}

static bool32 ramIsRammer(Ship *ship)
{
    return (ship->shiptype == bRammingFrigate) || (ship->shiptype == sRammingFrigate);
}

static void cat_rammingFrigateResetCooldown(Ship *ship)                                     // 0x005bf860
{
    if (ramIsRammer(ship))
    {
        cat_timerStartGameTime(&((RammingFrigateSpec *)ship->ShipSpecifics)->cooldownTimer,
                               ((RammingFrigateStatics *)ship->staticinfo->custstatinfo)->ramWaitAfterPushTime);
    }
}

// the state change, with its animations, timers, thruster sound and trail width
void cat_rammingFrigateRelease(Ship *ship, ubyte newstate)                                 // 0x005bf8a0
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;
    RammingFrigateStatics *ramstat = (RammingFrigateStatics *)ship->staticinfo->custstatinfo;
    shiptrail *trail;
    real32 scale;
    bool32 wasRamming, isRamming;

    if (spec->pushEffect != NULL)
    {
        cat_effectDeleteNow(spec->pushEffect);
        spec->pushEffect = NULL;
    }
    if ((spec->state == RAM_Push) && (newstate != RAM_Push))
    {
        unClampObj((SpaceObjRotImpTargGuidance *)ship);
        cat_madAnimationStartNumber(ship, 1);       // CAT-TODO(stage 3): the exe passes a third argument, 0
        cat_timerStartGameTime(&spec->cooldownTimer, ramstat->ramWaitAfterPushTime);
    }
    if (newstate == RAM_Idle)
    {
        ship->shipidle = TRUE;
        ship->autostabilizeship = TRUE;
        spec->target = NULL;
    }
    else if ((newstate == RAM_Ram) && (spec->state != RAM_Ram))
    {
        cat_timerStartGameTime(&spec->chargeTimer, RAM_CHARGE_TIME);
    }
    // leaving the ram for anything but the push plays animation 4 (also "leaving" it for itself, as the exe does)
    if ((spec->state == RAM_Ram) && (newstate != RAM_Push))
    {
        cat_madAnimationStartNumber(ship, 4);       // CAT-TODO(stage 3): the exe passes a third argument, 0
    }
    if ((newstate == RAM_Push) && (spec->state != RAM_Push))
    {
        if (ship->soundevent.specialHandle != -1)
        {
            soundEventStopSound(ship->soundevent.specialHandle, 0);
            ship->soundevent.specialHandle = -1;
        }
        spec->thrustActive = TRUE;
        cat_timerStartGameTime(&spec->thrustTimer, RAM_THRUST_TIME);
    }
    // the engine trail widens while ramming or pushing (x1.3, x4 upgraded) and narrows again after
    if ((trail = ship->trail[0]) != NULL)
    {
        scale = trail->ribbonadjust;
        wasRamming = (spec->state == RAM_Push) || (spec->state == RAM_Ram) || (spec->state == RAM_4);
        isRamming = (newstate == RAM_Push) || (newstate == RAM_Ram) || (newstate == RAM_4);
        if (wasRamming != isRamming)
        {
            real32 factor = cat_shipHasUpgrade(ship, CAT_UPGRADE_RamFrigate) ? RAM_TRAIL_SCALE_UPGRADED : RAM_TRAIL_SCALE;

            if (!isRamming)
            {
                factor = 1.0f / factor;
            }
            scale = factor * scale;
        }
        trail->ribbonadjust = scale;
    }
    spec->state = newstate;
}

// the impact effect (etgSpecialPurposeEffectTable[EGT_RAMMING_FRIGATE_IMPACT_EFFECT], the exe's 0x00c41d40)
static void cat_rammingFrigateHitEffect(Ship *ship)                                         // 0x005bfa60
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;
    etglod *etgLOD = etgSpecialPurposeEffectTable[EGT_RAMMING_FRIGATE_IMPACT_EFFECT];
    etgeffectstatic *stat;

    // the exe deletes the old one without clearing the pointer (RammingFrigateEffectRemoval clears it)
    cat_effectDeleteNow(spec->hitEffect);
    if ((etgLOD != NULL) && (ship->obj.currentLOD < etgLOD->nLevels) &&
        ((stat = etgLOD->level[ship->obj.currentLOD]) != NULL) && etgHitEffectsEnabled && !etgFrequencyExceeded(stat))
    {
        etgEffectCreate(stat, ship, NULL, NULL, NULL, 1.0f, EAF_Full, 0, 0);
    }
}

void RammingFrigateStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)  // 0x005bfad0
{
    RammingFrigateStatics *ramstat = (statinfo->shiprace != RACE_Sect) ? &RammingFrigateStaticBeast
                                                                         : &RammingFrigateStaticSect;

    (void)directory;
    (void)filename;
    ramstat->ramDamage = statinfo->RamDamage;
    ramstat->cuttingBeamDamagePerSecond = statinfo->cuttingBeamDamagePerSecond;
    ramstat->cuttingBeamUpgradedDamagePerSecond = statinfo->cuttingBeamUpgradedDamagePerSecond;
    ramstat->ramPushTime = statinfo->RamPushTime;
    ramstat->ramWaitAfterPushTime = statinfo->RamWaitAfterPushTime;
    statinfo->custstatinfo = ramstat;
    statinfo->canSpecialBandBoxFriendlies = TRUE;
    statinfo->unknownFlag212 = TRUE;
    statinfo->unknownFlag213 = FALSE;
}

void RammingFrigateInit(Ship *ship)                                                         // 0x005bfb40
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;

    spec->unknown10 = 300;                      // 0x00878144
    spec->state = RAM_Idle;
    spec->target = NULL;
    spec->pushEffect = NULL;
    spec->unknown40 = 0;
    spec->damageTarget = FALSE;
    cat_timerStartGameTime(&spec->chargeTimer, 0.01f);
    cat_timerStartGameTime(&spec->cooldownTimer, 0.01f);
    cat_timerStartGameTime(&spec->losTimer, 0.01f);
    spec->thrustActive = FALSE;
    cat_timerStartGameTime(&spec->thrustTimer, 0.01f);
}

// two ships are clamped together (one to the other, or both to the same host)
static bool32 cat_shipsClampedTogether(Ship *a, Ship *b)                                    // 0x005bfbd0
{
    bool32 together = FALSE;
    SpaceObjRotImpTarg *hostA;

    if (a == b)
    {
        return TRUE;
    }
    if ((a == NULL) || (b == NULL) || (a->header.objtype != OBJ_ShipType) || (b->header.objtype != OBJ_ShipType) ||
        (a->guidance.clampInfo == NULL) || (b->guidance.clampInfo == NULL))
    {
        return FALSE;
    }
    hostA = a->guidance.clampInfo->host;
    if ((hostA != NULL) && (hostA == b->guidance.clampInfo->host))
    {
        together = TRUE;
    }
    if (hostA == (SpaceObjRotImpTarg *)b)
    {
        together = TRUE;
    }
    if (b->guidance.clampInfo->host == (SpaceObjRotImpTarg *)a)
    {
        return TRUE;
    }
    return together;
}

// a new target (not a part of the old one): release, turn to it, and damage it if it is an enemy and no crystal
static void ramSetTarget(Ship *ship, Ship *target)
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;

    cat_rammingFrigateRelease(ship, RAM_Idle);
    cat_rammingFrigateRelease(ship, RAM_Turn);
    spec->target = target;
    spec->damageTarget = !allianceIsShipAlly(target, ship->playerowner);
    if (cat_shipIsCrystal(target))
    {
        spec->damageTarget = FALSE;
    }
}

void RammingFrigateAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)            // 0x005bfc30
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;

    (void)maxdist;
    if ((target->header.objtype == OBJ_ShipType) && ((Ship *)target != spec->target) &&
        !cat_shipsClampedTogether(spec->target, (Ship *)target))
    {
        ramSetTarget(ship, (Ship *)target);
    }
}

bool32 RammingFrigateSpecialActivate(Ship *ship)                                            // 0x005bfcf0
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;

    spec->target = NULL;
    cat_rammingFrigateRelease(ship, RAM_Idle);
    ship->shipidle = TRUE;
    ship->autostabilizeship = TRUE;
    return TRUE;    // the exe returns eax of the last store (1)
}

bool32 RammingFrigateSpecialTarget(Ship *ship,void *custom)                                 // 0x005bfd20
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;
    SpecialCommand *targets = (SpecialCommand *)custom;
    Ship *target = NULL;
    Ship *candidate;
    sdword i;

    if (targets->numTargets < 1)
    {
        return TRUE;
    }
    // the last ship target inside the universe box
    for (i = 0; i < targets->numTargets; i++)
    {
        candidate = (Ship *)targets->TargetPtr[i];
        if ((candidate->header.objtype == OBJ_ShipType) &&
            (-smUniverseSizeZ <= candidate->obj.posinfo.position.z) && (candidate->obj.posinfo.position.z <= smUniverseSizeZ) &&
            (-smUniverseSizeY <= candidate->obj.posinfo.position.y) && (candidate->obj.posinfo.position.y <= smUniverseSizeY) &&
            (-smUniverseSizeX <= candidate->obj.posinfo.position.x) && (candidate->obj.posinfo.position.x <= smUniverseSizeX))
        {
            target = candidate;
        }
    }
    if (target == NULL)
    {
        return TRUE;
    }
    if ((target->header.objtype == OBJ_ShipType) && (target != spec->target))
    {
        if (battleCanChatterAtThisTime(0x54, ship))
        {
            battleChatterAttempt(-1, 0x54, ship, -1);
        }
        ramSetTarget(ship, target);
    }
    return FALSE;
}

void RammingFrigateRemoveShipReferences(Ship *ship,Ship *shiptoremove)                      // 0x005bfe60
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;

    if (shiptoremove == spec->target)
    {
        if (ramIsRammer(ship) && (spec->state == RAM_Push))
        {
            cat_rammingFrigateRelease(ship, RAM_Idle);
        }
        cat_rammingFrigateRelease(ship, RAM_Idle);
        spec->target = NULL;
    }
}

void RammingFrigateEffectRemoval(Ship *ship, struct Effect *effect)                         // 0x005bfec0
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;

    if (effect == spec->hitEffect)
    {
        spec->hitEffect = NULL;
    }
    if (spec->pushEffect == effect)
    {
        spec->pushEffect = NULL;
    }
}

void cat_shipBlendHeading(Ship *ship, vector *dir, real32 wdir, real32 wcur)               // 0x005bfef0
{
    vector heading;
    real32 sum = wdir + wcur;
    matrix *coordsys = &ship->rotatable.rotinfo.coordsys;

    heading.x = (coordsys->m13 * wcur + wdir * dir->x) / sum;
    heading.y = (coordsys->m23 * wcur + wdir * dir->y) / sum;
    heading.z = (coordsys->m33 * wcur + wdir * dir->z) / sum;
    vecNormalize(&heading);
    matCreateCoordSysFromHeading(coordsys, &heading);
    collUpdateCollRectangle(&ship->impactable);
}

void cat_shipSetHeading(Ship *ship, vector *dir)                                            // 0x005bff90
{
    vector heading = *dir;

    vecNormalize(&heading);
    matCreateCoordSysFromHeading(&ship->rotatable.rotinfo.coordsys, &heading);
    collUpdateCollRectangle(&ship->impactable);
}

// the exe's cat_shipBlendHeadingDefault 0x005bffe0 (weights 0.07 / 1.0), inlined in RammingFrigateHouseKeep
static void ramBlendHeadingDefault(Ship *ship, vector *dir)
{
    cat_shipBlendHeading(ship, dir, RAM_BLEND_NEW, RAM_BLEND_OLD);
}

// in multiplayer an EMP-stunned or hyperspacing ram does not steer (inlined in RammingFrigateHouseKeep)
static bool32 ramCanSteer(Ship *ship)
{
    if (singlePlayerGame)
    {
        return TRUE;
    }
    if ((ship->command != NULL) && (ship->command->ordertype.order == COMMAND_MP_HYPERSPACING))
    {
        return FALSE;
    }
    return ship->catEmpStunned == 0;
}

// the points of the push (four thruster positions and the push direction and its opposite)
static void cat_rammingFrigatePushGeometry(Ship *ship, vector *a, vector *b, vector *c, vector *d, vector *e,
                                           vector *f)                                       // 0x005c16f0
{
    vector la = {  37.0f, -50.0f, -43.0f };     // 0x008c5df4 / 0x008c5dec / 0x008c5de8
    vector lb = { -34.0f, -50.0f, -43.0f };     // 0x008c5df8
    vector lc = {  37.0f,  50.0f, -43.0f };     // 0x008c5df0
    vector ld = { -34.0f,  50.0f, -43.0f };
    vector dir;

    cat_vecShipLocalToWorld(a, &la, ship);
    cat_vecShipLocalToWorld(b, &lb, ship);
    cat_vecShipLocalToWorld(c, &lc, ship);
    cat_vecShipLocalToWorld(d, &ld, ship);
    dir.x = c->x - a->x;
    dir.y = c->y - a->y;
    dir.z = c->z - a->z;
    vecNormalize(&dir);
    *e = dir;
    f->x = -dir.x;
    f->y = -dir.y;
    f->z = -dir.z;
}

// a thruster puff (etgSpecialPurposeEffectTable[EGT_AFTER_BURNER], the exe's 0x00c41d64) at a point, below LOD 3
static void cat_rammingFrigateThrustEffect(Ship *ship, vector *position, vector *dir)       // 0x005c1830
{
    etglod *etgLOD = etgSpecialPurposeEffectTable[EGT_AFTER_BURNER];
    etgeffectstatic *stat;
    vector velocity;
    matrix coordsys;

    if ((etgLOD == NULL) || ((stat = etgLOD->level[0]) == NULL) || !etgHitEffectsEnabled || etgFrequencyExceeded(stat))
    {
        return;
    }
    matCreateCoordSysFromHeading(&coordsys, dir);
    velocity = ship->obj.posinfo.velocity;
    if (ship->obj.currentLOD < RAM_THRUST_LODS)
    {
        etgEffectCreate(stat, NULL, position, &velocity, &coordsys, 1.0f, EAF_Full, 1, (udword)ship->obj.currentLOD);
    }
}

void RammingFrigateHouseKeep(Ship *ship)                                                    // 0x005c0090
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;
    RammingFrigateStatics *ramstat = (RammingFrigateStatics *)ship->staticinfo->custstatinfo;
    Ship *target;
    vector a, b, c, d, e, f;
    vector tpos, spos, dir, heading, chase, newvel, velnorm, newnorm;
    real32 dist, radius, speed, mult, change;
    ubyte newstate;
    sdword shipclass;

    if (bitTest(ship->shipOrDerelict.specialFlags, SPECIAL_SinglePlayerWithinHyperSpaceRange))
    {
        cat_rammingFrigateRelease(ship, RAM_Idle);
    }
    target = spec->target;
    if (spec->thrustActive == TRUE)
    {
        if (!cat_timerExpired(&spec->thrustTimer))
        {
            if (cat_timerElapsed(&spec->thrustTimer) > RAM_THRUST_DELAY)
            {
                cat_rammingFrigatePushGeometry(ship, &a, &b, &c, &d, &e, &f);
                cat_rammingFrigateThrustEffect(ship, &a, &e);
                cat_rammingFrigateThrustEffect(ship, &b, &e);
                cat_rammingFrigateThrustEffect(ship, &c, &f);
                cat_rammingFrigateThrustEffect(ship, &d, &f);
            }
        }
        else
        {
            spec->thrustActive = FALSE;
        }
    }
    if (target == NULL)
    {
        return;
    }
    tpos = target->obj.posinfo.position;
    spos = ship->obj.posinfo.position;
    dist = cat_vecDistance(&spos, &tpos);
    dir.x = tpos.x - spos.x;
    dir.y = tpos.y - spos.y;
    dir.z = tpos.z - spos.z;
    vecNormalize(&dir);
    heading.x = ship->rotatable.rotinfo.coordsys.m13;
    heading.y = ship->rotatable.rotinfo.coordsys.m23;
    heading.z = ship->rotatable.rotinfo.coordsys.m33;
    switch (spec->state)
    {
        case RAM_Turn:
            if ((dir.x * heading.x + dir.y * heading.y + dir.z * heading.z <= RAM_FACING_COS) ||
                !cat_timerExpired(&spec->cooldownTimer))
            {
                if (ramCanSteer(ship))
                {
                    aitrackHeadingFunc(&ship->guidance, &dir, RAM_TURN_ACCURACY, RAM_TURN_ACCURACY,
                                       ship->staticinfo->shipOrDerelict.sinbank, 0);
                }
                goto checkcollision;
            }
            newstate = RAM_Approach;
            break;

        case RAM_Approach:
            radius = target->staticinfo->common.staticCollInfo.collspheresize;
            shipclass = target->staticinfo->shipclass;
            if ((dist < radius + RAM_RANGE_SMALL) ||
                (((target->header.objtype != OBJ_ShipType) || ((shipclass != CLASS_Fighter) && (shipclass != CLASS_Corvette))) &&
                 (dist < radius + RAM_RANGE_BIG)))
            {
                if (cat_timerExpired(&spec->losTimer))
                {
                    if (!collCheckLineOfSight(ship, target, &spos, &dir))
                    {
                        cat_timerStartGameTime(&spec->losTimer, RAM_LOS_RETRY);
                    }
                    else
                    {
                        spec->unknown10 = 0;
                        cat_rammingFrigateRelease(ship, RAM_Ram);
                        cat_madAnimationStartNumber(ship, 2);   // CAT-TODO(stage 3): the exe passes a third argument, 0
                    }
                }
            }
            ship->autostabilizeship = FALSE;
            if (ramCanSteer(ship))
            {
                aishipFlyToPointAvoidingObjsFunc(ship, &target->obj.posinfo.position, 6, -30.0f,
                                                 &target->obj.posinfo.velocity);
                aitrackHeadingFunc(&ship->guidance, &dir, RAM_TURN_ACCURACY, RAM_TURN_ACCURACY,
                                   ship->staticinfo->shipOrDerelict.sinbank, 0);
            }
            goto checkcollision;

        case RAM_Ram:
            chase.x = dir.x * RAM_CHASE + tpos.x;
            chase.y = dir.y * RAM_CHASE + tpos.y;
            chase.z = dir.z * RAM_CHASE + tpos.z;
            bitSet(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatNoAvoidance);
            ship->shipidle = FALSE;
            ship->autostabilizeship = FALSE;
            mult = cat_shipHasUpgrade(ship, CAT_UPGRADE_RamFrigate) ? RAM_SPEED_MULT_UPGRADED : RAM_SPEED_MULT;
            speed = fsqrt(ship->obj.posinfo.velocity.z * ship->obj.posinfo.velocity.z +
                          ship->obj.posinfo.velocity.y * ship->obj.posinfo.velocity.y +
                          ship->obj.posinfo.velocity.x * ship->obj.posinfo.velocity.x);
            newvel.x = speed * mult * dir.x;
            newvel.y = speed * mult * dir.y;
            newvel.z = speed * mult * dir.z;
            velnorm = ship->obj.posinfo.velocity;
            newnorm = newvel;
            vecNormalize(&velnorm);
            vecNormalize(&newnorm);
            if ((RAM_KEEP_COS <= dir.x * heading.x + dir.y * heading.y + dir.z * heading.z) ||
                (RAM_KEEP_COS <= newnorm.x * velnorm.x + newnorm.y * velnorm.y + newnorm.z * velnorm.z))
            {
                // an unupgraded ram above 400 turns its velocity by at most 10% per update
                if (!cat_shipHasUpgrade(ship, CAT_UPGRADE_RamFrigate) &&
                    (RAM_SPEED_CAP * RAM_SPEED_CAP < speed * speed))
                {
                    change = cat_vecDistance(&velnorm, &newnorm);
                    if (change > RAM_SPEED_CHANGE_CAP)
                    {
                        change = RAM_SPEED_CHANGE_CAP;
                    }
                    change = 1.0f - change;
                    newvel.x *= change;
                    newvel.y *= change;
                    newvel.z *= change;
                }
                ship->obj.posinfo.velocity = newvel;
                if (ramCanSteer(ship))
                {
                    ramBlendHeadingDefault(ship, &dir);
                }
            }
            else
            {
                cat_rammingFrigateRelease(ship, RAM_Turn);
            }
            // an upgraded ram gives up after 25 s
            if (cat_shipHasUpgrade(ship, CAT_UPGRADE_RamFrigate) && cat_timerExpired(&spec->chargeTimer))
            {
                cat_rammingFrigateRelease(ship, RAM_Turn);
                cat_timerStartGameTime(&spec->cooldownTimer, ramstat->ramWaitAfterPushTime);
            }
            if (ramCanSteer(ship))
            {
                aitrackHeadingFunc(&ship->guidance, &dir, 0.0f, 0.0f, 0.0f, 0);
                aitrackVelocityVectorGuidance(&ship->guidance, &chase);
            }
            // past a fighter or corvette by 700 it turns for another pass
            if ((target->header.objtype != OBJ_ShipType) ||
                (((shipclass = target->staticinfo->shipclass) != CLASS_Fighter) && (shipclass != CLASS_Corvette)) ||
                (dist <= target->staticinfo->common.staticCollInfo.collspheresize + RAM_RANGE_GIVE_UP))
            {
                goto checkcollision;
            }
            newstate = RAM_Turn;
            break;

        case RAM_Push:
            if (!cat_timerExpired(&spec->pushTimer) && cat_univPointInUniverse(&ship->obj.posinfo.position))
            {
                cat_rammingFrigatePushTarget(ship);
                goto checkcollision;
            }
            newstate = RAM_Idle;
            break;

        default:
            goto checkcollision;
    }
    cat_rammingFrigateRelease(ship, newstate);
checkcollision:
    if (spec->state != RAM_Push)
    {
        cat_rammingFrigateCheckCollision(ship, target);
    }
    // CAT-TODO(stage 3 FOW): a target the player cannot see (cat_fowCanPlayerSeeShip(owner, target, 1)) cancels the
    //      special order (clWrapSpecial with no targets); without the fog of war every target is seen
}

bool32 RammingFrigateMoveCommand(Ship *ship)                                                // 0x005c0970
{
    cat_rammingFrigateRelease(ship, RAM_Idle);
    return TRUE;
}

void RammingFrigate_Fix(Ship *ship)                                                         // 0x005c0990
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;

    spec->target = SpaceObjRegistryGetShip((sdword)(smemsize)spec->target);
    spec->pushEffect = NULL;
}

void RammingFrigate_PreFix(Ship *ship)                                                      // 0x005c09c0
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;

    spec->target = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->target);
    if (spec->pushEffect != NULL)
    {
        cat_effectDeleteNow(spec->pushEffect);
        spec->pushEffect = NULL;
    }
    spec->pushEffect = NULL;
}

void RammingFrigateDied(Ship *ship)                                                         // 0x005c0a20
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;

    if (spec->pushEffect != NULL)
    {
        cat_effectDeleteNow(spec->pushEffect);
        spec->pushEffect = NULL;
    }
    spec->target = NULL;
}

void RammingFrigateClose(Ship *ship)                                                        // 0x005c0a60
{
    ((RammingFrigateSpec *)ship->ShipSpecifics)->target = NULL;
}

bool32 cat_shipIsLiveRammingFrigate(Ship *ship)                                             // 0x005c0a70
{
    return (ship->header.objtype == OBJ_ShipType) && ramIsRammer(ship);
}

bool32 cat_shipIsRammingFrigate(Ship *ship)                                                 // 0x005c0a90
{
    return ramIsRammer(ship);
}

bool32 cat_rammingFrigateIsPushing(Ship *ship)                                              // 0x005c0ac0
{
    return ((RammingFrigateSpec *)ship->ShipSpecifics)->state == RAM_Push;
}

void cat_rammingFrigateForceRelease(Ship *ship)                                             // 0x005c0ae0
{
    if (ramIsRammer(ship) && (((RammingFrigateSpec *)ship->ShipSpecifics)->state == RAM_Push))
    {
        cat_rammingFrigateRelease(ship, RAM_Idle);
    }
}

// where a line through origin along dir crosses target's mesh, relative to origin
static bool32 cat_rammingFrigateLineHitsMesh(Ship *target, vector *dir, vector *origin, vector *hit)  // 0x005c0b20
{
    // CAT-PROVISIONAL (stage 3, the mesh hit location): the exe intersects the line origin -/+ 500 * dir with the
    //      target's mesh (cat_meshLineIntersect 0x004b5910 with cat_meshObjectLineIntersect / R and the mesh matrix
    //      stack, Mesh.c; the collision code's bullet, beam and missile tests use it too, M2-R4 combat "What is
    //      left"). Without it no line hits, so a ram finds no latch point and turns for another pass.
    (void)target;
    (void)dir;
    (void)origin;
    (void)hit;
    (void)RAM_MESH_PROBE;
    return FALSE;
}

// the nine "Snap*" dock points of the ram, each probed along the ram's heading against target's mesh; the hit nearest
// the probe origin behind the ram wins
static void cat_rammingFrigateProbeLatchPoints(Ship *ship, Ship *target, DockStaticPoint **points, bool32 *found,
                                               real32 *bestDist, vector *bestPos, vector *probeOrigin, vector *dir,
                                               Ship **hitShip)                              // 0x005c0c00
{
    vector origin, hit, candidate;
    real32 d;
    sdword i;

    for (i = 0; i < 9; i++)
    {
        cat_vecShipLocalToWorld(&origin, &points[i]->position, ship);
        if (cat_rammingFrigateLineHitsMesh(target, dir, &origin, &hit))
        {
            candidate.x = hit.x + ship->obj.posinfo.position.x;
            candidate.y = hit.y + ship->obj.posinfo.position.y;
            candidate.z = hit.z + ship->obj.posinfo.position.z;
            if (!*found)
            {
                *found = TRUE;
                *bestPos = candidate;
                *hitShip = target;
                *bestDist = cat_vecDistance(bestPos, probeOrigin);
            }
            else if ((d = cat_vecDistance(&candidate, probeOrigin)) < *bestDist)
            {
                *bestDist = d;
                *bestPos = candidate;
                *hitShip = target;
            }
        }
    }
}

// the ram moves up to the nearest latch point on the target, its host or the ships clamped to the host
static bool32 cat_rammingFrigateFindLatchPoint(Ship *ship, Ship *target, Ship **hitShip)    // 0x005c0d30
{
    static char *snapNames[9] = { "SnapCenter", "SnapStarbFT", "SnapStarbFB", "SnapPortFT", "SnapPortFB",
                                  "SnapStarbBT", "SnapStarbBB", "SnapPortBT", "SnapPortBB" };
    DockStaticPoint *points[9];
    DockStaticInfo *dockstaticinfo = ship->staticinfo->dockStaticInfo;
    vector dir, probeOrigin, bestPos;
    real32 bestDist = 0.0f;
    bool32 found = FALSE;
    Ship *host, *clamped;
    Ship *probed = target;
    sdword i;

    dir.x = ship->rotatable.rotinfo.coordsys.m13;
    dir.y = ship->rotatable.rotinfo.coordsys.m23;
    dir.z = ship->rotatable.rotinfo.coordsys.m33;
    probeOrigin.x = dir.x * RAM_PROBE_BACK + ship->obj.posinfo.position.x;
    probeOrigin.y = dir.y * RAM_PROBE_BACK + ship->obj.posinfo.position.y;
    probeOrigin.z = dir.z * RAM_PROBE_BACK + ship->obj.posinfo.position.z;
    *hitShip = NULL;
    for (i = 0; i < 9; i++)
    {
        points[i] = cat_dockFindDockStaticPoint(snapNames[i], dockstaticinfo);
    }
    cat_rammingFrigateProbeLatchPoints(ship, target, points, &found, &bestDist, &bestPos, &probeOrigin, &dir, hitShip);
    if (target->guidance.clampInfo != NULL)
    {
        host = (Ship *)target->guidance.clampInfo->host;
        if ((host != NULL) && (host != target))
        {
            probed = host;
            cat_rammingFrigateProbeLatchPoints(ship, host, points, &found, &bestDist, &bestPos, &probeOrigin, &dir,
                                               hitShip);
        }
        else if (host != NULL)
        {
            probed = host;
        }
        // CAT: the exe reads the clamped list of the host (or the target) without testing it (0x005c0e8a)
        for (i = 0; (probed->guidance.clampInfo != NULL) && (probed->guidance.clampInfo->clampedObjs != NULL) &&
                    (i < probed->guidance.clampInfo->clampedObjs->numShips); i++)
        {
            clamped = probed->guidance.clampInfo->clampedObjs->ShipPtr[i];
            if (!cat_shipIsLeech(clamped) && !cat_shipIsLiveSentinel(clamped))
            {
                cat_rammingFrigateProbeLatchPoints(ship, clamped, points, &found, &bestDist, &bestPos, &probeOrigin,
                                                   &dir, hitShip);
            }
        }
    }
    if (found)
    {
        cat_univSetShipPosition(ship, &bestPos);
    }
    // no latching onto a ship being built or another ram
    if (found && (bitTest((*hitShip)->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt) || ramIsRammer(*hitShip)))
    {
        found = FALSE;
    }
    return found;
}

static void cat_rammingFrigateLatchTarget(Ship *rammer, Ship *target)                       // 0x005c1010
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)rammer->ShipSpecifics;
    RammingFrigateStatics *ramstat = (RammingFrigateStatics *)rammer->staticinfo->custstatinfo;
    Ship *dest = target;
    Ship *hit = NULL;
    Ship *other;
    CatClampedList *clamped;
    SelectCommand selectone;
    bool32 canLatch = TRUE;
    bool32 blocked, alone;
    vector dir;
    sdword i, shipclass;

    if ((target->guidance.clampInfo != NULL) && (target->guidance.clampInfo->host != NULL))
    {
        dest = (Ship *)target->guidance.clampInfo->host;
    }
    if (ramIsRammer(dest))
    {
        canLatch = FALSE;
    }
    if ((dest->guidance.clampInfo != NULL) && (dest->guidance.clampInfo->host != NULL))
    {
        canLatch = FALSE;
    }
    if ((spec->target != dest) &&
        ((spec->target->guidance.clampInfo == NULL) || ((Ship *)spec->target->guidance.clampInfo->host != dest)))
    {
        canLatch = FALSE;
    }
    if (cat_rammingFrigateIsPushing(rammer) || (dest == NULL))
    {
        canLatch = FALSE;
    }
    if (!cat_univPointInUniverse(&rammer->obj.posinfo.position) || !canLatch)
    {
        goto giveup;
    }
    // a ram already pushing the same ship where this one would go lets go, and this one gives up
    if ((dest->guidance.clampInfo != NULL) && ((clamped = dest->guidance.clampInfo->clampedObjs) != NULL) &&
        (clamped->numShips > 0))
    {
        blocked = FALSE;
        for (i = 0; i < clamped->numShips; i++)
        {
            other = clamped->ShipPtr[i];
            if (ramIsRammer(other) && collCheckRectBumpCollision(&rammer->impactable, &other->impactable))
            {
                if (cat_shipIsRammingFrigate(other) && cat_rammingFrigateIsPushing(other))
                {
                    cat_rammingFrigateRelease(other, RAM_Idle);
                }
                blocked = TRUE;
            }
        }
        if (blocked)
        {
            goto giveup;
        }
    }
    if (cat_shipIsRammingFrigate(spec->target))
    {
        goto giveup;
    }
    // leeches on the ram are shaken off; any other ship clamped to it stops the latch
    alone = TRUE;
    if ((rammer->guidance.clampInfo != NULL) && ((clamped = rammer->guidance.clampInfo->clampedObjs) != NULL) &&
        (clamped->numShips >= 1))
    {
        for (i = clamped->numShips - 1; i >= 0; i--)
        {
            other = clamped->ShipPtr[i];
            if (!cat_shipIsLeech(other))
            {
                (void)cat_shipIsSalvager(other);    // the exe calls it and ignores the result
                alone = FALSE;
            }
            else
            {
                selectone.numShips = 1;
                selectone.ShipPtr[0] = other;
                RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone);  // CAT-TODO(M2-R4 command follow-up): reason 0x2d
            }
        }
        if (!alone)
        {
            goto giveup;
        }
    }
    // CAT-TODO(stage 3): in single player the exe also gives up while the wide-screen letterbox is fully in
    //      (0x00a4cb58, set by nisUpdateTask)
    dir.x = target->obj.posinfo.position.x - rammer->obj.posinfo.position.x;
    dir.y = target->obj.posinfo.position.y - rammer->obj.posinfo.position.y;
    dir.z = target->obj.posinfo.position.z - rammer->obj.posinfo.position.z;
    vecNormalize(&dir);
    cat_shipBlendHeading(rammer, &dir, 1.0f, 0.0f);
    if (cat_rammingFrigateFindLatchPoint(rammer, target, &hit) == TRUE)
    {
        if (hit != NULL)
        {
            ApplyDamageToTarget(&hit->targetable, ramstat->ramDamage, 0, 0x29a, rammer->playerowner->playerIndex, rammer,
                                0x46);
            // fighters, micro ships, ACVs, the sMCV and bHeavyCorvette that survive the hit die anyway
            shipclass = hit->staticinfo->shipclass;
            if (((shipclass == CLASS_Micro) || (shipclass == CLASS_Fighter) || cat_shipIsACV(hit) ||
                 (hit->shiptype == sMCV) || (hit->shiptype == bHeavyCorvette)) && (hit->targetable.health >= 0.0f))
            {
                ApplyDamageToTarget(&hit->targetable, hit->targetable.health + 1.0f, 0, 0x29a,
                                    rammer->playerowner->playerIndex, rammer, 0x47);
            }
        }
        if (hit->targetable.health >= 0.0f)
        {
            spec->target = dest;
            clampObjToObj((SpaceObjRotImpTargGuidance *)rammer, (SpaceObjRotImpTargGuidance *)dest);
            cat_timerStartGameTime(&spec->pushTimer, ramstat->ramPushTime);
            cat_rammingFrigateRelease(rammer, RAM_Push);
            spec->pushDirection.x = dest->rotatable.rotinfo.coordsys.m13;
            spec->pushDirection.y = dest->rotatable.rotinfo.coordsys.m23;
            spec->pushDirection.z = dest->rotatable.rotinfo.coordsys.m33;
            cat_madAnimationStartNumber(rammer, 0);     // CAT-TODO(stage 3): the exe passes a third argument, 0
            cat_rammingFrigateHitEffect(rammer);
            soundEventPlay(rammer, 0x81c, NULL);
            if (battleCanChatterAtThisTime(0x42, rammer))
            {
                battleChatterAttempt(-1, 0x42, rammer, -1);
            }
            return;
        }
    }
    else if ((hit != NULL) && ramIsRammer(hit) && cat_shipIsRammingFrigate(hit) && cat_rammingFrigateIsPushing(hit))
    {
        cat_rammingFrigateRelease(hit, RAM_Idle);
    }
    if (cat_shipIsRammingFrigate(rammer))
    {
        cat_timerStartGameTime(&spec->cooldownTimer, ramstat->ramWaitAfterPushTime);
    }
    cat_rammingFrigateRelease(rammer, RAM_Turn);
    return;
giveup:
    cat_rammingFrigateResetCooldown(rammer);
    cat_rammingFrigateRelease(rammer, RAM_Turn);
}

// two ships touched: two rams bounce off each other; a ram in its ram state infects itself on a Beast ship or latches
void cat_rammingFrigateCollide(Ship *a, Ship *b)                                            // 0x005c14a0
{
    Ship *ram = NULL, *other = NULL;
    bool32 immune;

    if ((a->header.objtype != OBJ_ShipType) || (b->header.objtype != OBJ_ShipType))
    {
        return;
    }
    if (ramIsRammer(a) && ramIsRammer(b))
    {
        if (((RammingFrigateSpec *)a->ShipSpecifics)->state == RAM_Push)
        {
            cat_rammingFrigateRelease(a, RAM_Idle);
        }
        if (((RammingFrigateSpec *)b->ShipSpecifics)->state == RAM_Push)
        {
            cat_rammingFrigateRelease(b, RAM_Idle);
        }
        cat_timerStartGameTime(&((RammingFrigateSpec *)a->ShipSpecifics)->cooldownTimer,
                               ((RammingFrigateStatics *)a->staticinfo->custstatinfo)->ramWaitAfterPushTime);
        cat_timerStartGameTime(&((RammingFrigateSpec *)b->ShipSpecifics)->cooldownTimer,
                               ((RammingFrigateStatics *)b->staticinfo->custstatinfo)->ramWaitAfterPushTime);
        cat_rammingFrigateRelease(a, RAM_Turn);
        cat_rammingFrigateRelease(b, RAM_Turn);
        return;
    }
    if (ramIsRammer(a))
    {
        ram = a;
        other = b;
    }
    if (ramIsRammer(b))
    {
        ram = b;
        other = a;
    }
    if ((ram == NULL) || (((RammingFrigateSpec *)ram->ShipSpecifics)->state != RAM_Ram))
    {
        return;
    }
    // a Beast ram (or a ram that is an infector) just latches; a Sect ram hitting a Beast ship or an infector without
    // the anti-infection upgrade gets infected
    if ((ram->header.objtype == OBJ_ShipType) &&
        (((ram->shiptype > SECT_LAST_SHIP) && (ram->shiptype <= BEAST_LAST_SHIP)) || cat_shipImmuneToRamming(ram)))
    {
        cat_rammingFrigateLatchTarget(ram, other);
        return;
    }
    if (other->header.objtype == OBJ_ShipType)
    {
        immune = cat_shipImmuneToRamming(other);
        if ((((other->shiptype > SECT_LAST_SHIP) && (other->shiptype <= BEAST_LAST_SHIP)) || immune) &&
            (ram->playerowner != other->playerowner) && !cat_shipHasUpgrade(ram, CAT_UPGRADE_RamInfectionImmunity) &&
            cat_shipInfectsOnRam(other))
        {
            cat_beastInfectShip(ram, other);
            cat_rammingFrigateRelease(ram, RAM_Idle);
            return;
        }
    }
    cat_rammingFrigateLatchTarget(ram, other);
}

// the cutting beam (etgSpecialPurposeEffectTable[EGT_BULLET_CUTTING_BEAM], the exe's 0x00c41d54) from the ram's nose to
// where it meets the target's hull
static void cat_rammingFrigatePushEffectCreate(Ship *ship)                                  // 0x005c1900
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;
    etglod *etgLOD = etgSpecialPurposeEffectTable[EGT_BULLET_CUTTING_BEAM];
    etgeffectstatic *stat;
    vector nose = { RAM_BEAM_X, 0.0f, RAM_BEAM_Z };     // y: the exe reads the zeroed global 0x00a77b08
    vector position, start, velocity;
    matrix coordsys;

    if ((etgLOD == NULL) || (etgLOD->nLevels <= 0) || ((stat = etgLOD->level[0]) == NULL) || !etgFireEffectsEnabled ||
        etgFrequencyExceeded(stat))
    {
        return;
    }
    velocity.x = velocity.y = velocity.z = 0.0f;
    coordsys = ship->rotatable.rotinfo.coordsys;
    cat_vecShipLocalToWorld(&position, &nose, ship);
    cat_vecShipLocalToWorld(&start, &nose, ship);
    // CAT-PROVISIONAL (stage 3, the mesh hit location): the beam's length is where the line start .. start + heading *
    //      2000 meets the target's mesh (cat_meshLineIntersect 0x004b5910); without that no beam is made, as the exe
    //      does when the line misses (the cutting damage still applies: cat_rammingFrigateDamageTarget)
    (void)start;
    (void)position;
    (void)velocity;
    (void)coordsys;
    (void)RAM_BEAM_LENGTH;
    spec->pushEffect = NULL;
}

// while pushing, an enemy target takes the cutting beam's damage
static void cat_rammingFrigateDamageTarget(Ship *ship)                                      // 0x005c1b20
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;
    RammingFrigateStatics *ramstat = (RammingFrigateStatics *)ship->staticinfo->custstatinfo;
    vector nose = { RAM_BEAM_X, 0.0f, RAM_BEAM_Z };
    real32 dps;

    if (spec->damageTarget != TRUE)
    {
        return;
    }
    if (spec->pushEffect == NULL)
    {
        cat_rammingFrigatePushEffectCreate(ship);
    }
    else
    {
        cat_vecShipLocalToWorld(&spec->pushEffect->obj.posinfo.position, &nose, ship);
        spec->pushEffect->rotatable.rotinfo.coordsys = ship->rotatable.rotinfo.coordsys;
    }
    dps = cat_shipHasUpgrade(ship, CAT_UPGRADE_RamCuttingBeam) ? ramstat->cuttingBeamUpgradedDamagePerSecond
                                                               : ramstat->cuttingBeamDamagePerSecond;
    ApplyDamageToTarget((SpaceObjRotImpTarg *)spec->target, universe.phystimeelapsed * dps, 0, 0x29a,
                        ship->playerowner->playerIndex, ship, 0x48);
}

// turn the pushed ship toward the push direction (TRUE when aligned)
static bool32 cat_rammingFrigateApplyPush(Ship *ship, vector *dir, real32 mindot, real32 rot)      // 0x005c1d20
{
    ShipStaticInfo *statinfo = ship->staticinfo;
    vector local;
    real32 dot, oneOverTime, ang, rotspeed, torque;
    uword rotdir;

    if ((ship->header.objtype == OBJ_ShipType) && (ship->dontrotateever != 0))
    {
        return TRUE;
    }
    if (cat_shipIsLiveWorker(ship))
    {
        return FALSE;
    }
    dot = ship->rotatable.rotinfo.coordsys.m13 * dir->x + ship->rotatable.rotinfo.coordsys.m23 * dir->y +
          ship->rotatable.rotinfo.coordsys.m33 * dir->z;
    if (dot <= -1.0f)
    {
        physApplyRotToObj(&ship->rotatable, rot, 1);
        return FALSE;
    }
    if (mindot <= dot)
    {
        aitrackForceGuidanceZeroRotation(&ship->guidance);
        return TRUE;
    }
    matMultiplyVecByMat(&local, dir, &ship->rotatable.rotinfo.coordsys);
    oneOverTime = 1.0f / universe.phystimeelapsed;
    if (local.y != 0.0f)
    {
        if (local.z >= 0.0f)
        {
            ang = -(local.y * ship->guidance.nonstatvars.turnspeed[TURN_YAW]);
        }
        else
        {
            ang = ship->guidance.nonstatvars.turnspeed[TURN_YAW];
            if (local.y >= 0.0f)
            {
                ang = -ang;
            }
        }
        rotspeed = ship->rotatable.rotinfo.rotspeed.x;
        if (ang <= rotspeed)
        {
            torque = (rotspeed - ang) * statinfo->common.momentOfInertiaX * oneOverTime;
            if (rot < torque)
            {
                torque = rot;
            }
            rotdir = 1;
        }
        else
        {
            torque = (ang - rotspeed) * statinfo->common.momentOfInertiaX * oneOverTime;
            if (rot < torque)
            {
                torque = rot;
            }
            rotdir = 0;
        }
        physApplyRotToObj(&ship->rotatable, torque, rotdir);
    }
    if (local.x != 0.0f)
    {
        ang = local.x * ship->guidance.nonstatvars.turnspeed[TURN_PITCH];
        rotspeed = ship->rotatable.rotinfo.rotspeed.y;
        if (rotspeed < ang)
        {
            torque = (ang - rotspeed) * statinfo->common.momentOfInertiaY * oneOverTime;
            physApplyRotToObj(&ship->rotatable, (rot < torque) ? rot : torque, 2);
            return FALSE;
        }
        torque = (rotspeed - ang) * statinfo->common.momentOfInertiaY * oneOverTime;
        if (rot < torque)
        {
            torque = rot;
        }
        physApplyRotToObj(&ship->rotatable, torque, 3);
    }
    return FALSE;
}

// push the latched target along the ram's heading (not a mothership or a part of one) and turn it
static void cat_rammingFrigatePushTarget(Ship *ship)                                        // 0x005c1c20
{
    RammingFrigateSpec *spec = (RammingFrigateSpec *)ship->ShipSpecifics;
    real32 force = cat_shipHasUpgrade(ship, CAT_UPGRADE_RamFrigate) ? RAM_PUSH_FORCE_UPGRADED : RAM_PUSH_FORCE;
    Ship *target = spec->target;
    Ship *host;

    if (!cat_shipIsMothership(target) &&
        ((target->guidance.clampInfo == NULL) || ((host = (Ship *)target->guidance.clampInfo->host) == NULL) ||
         !cat_shipIsMothership(host)))
    {
        target->obj.posinfo.force.x = ship->rotatable.rotinfo.coordsys.m13 * force + target->obj.posinfo.force.x;
        target->obj.posinfo.force.y = ship->rotatable.rotinfo.coordsys.m23 * force + target->obj.posinfo.force.y;
        target->obj.posinfo.force.z = ship->rotatable.rotinfo.coordsys.m33 * force + target->obj.posinfo.force.z;
    }
    cat_rammingFrigateApplyPush(target, &spec->pushDirection, RAM_PUSH_MINDOT, RAM_PUSH_ROT);
    cat_rammingFrigateDamageTarget(ship);
}

bool32 cat_rammingFrigateStateActive(Ship *ship)                                            // 0x005c1ff0
{
    return ramIsRammer(ship) && (((RammingFrigateSpec *)ship->ShipSpecifics)->state != RAM_Idle);
}

// the ram touches its target (collision spheres, then the boxes)
static void cat_rammingFrigateCheckCollision(Ship *rammer, Ship *other)                     // 0x005c2020
{
    real32 colldist = rammer->staticinfo->common.staticCollInfo.collspheresize +
                      other->staticinfo->common.staticCollInfo.collspheresize;
    vector diff;
    real32 distsquared, dist;

    diff.x = other->impactable.collInfo.collPosition.x - rammer->impactable.collInfo.collPosition.x;
    diff.y = other->impactable.collInfo.collPosition.y - rammer->impactable.collInfo.collPosition.y;
    diff.z = other->impactable.collInfo.collPosition.z - rammer->impactable.collInfo.collPosition.z;
    distsquared = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    if (distsquared < colldist * colldist)
    {
        dist = cat_vecDistance(&rammer->impactable.collInfo.collPosition, &other->impactable.collInfo.collPosition);
        if (collCheckRectBumpCollision(&rammer->impactable, &other->impactable))
        {
            cat_rammingFrigateCollide(rammer, other);
            if (ramIsRammer(other))
            {
                ObjectsCollided(&rammer->targetable, &other->targetable, colldist, &diff, dist, distsquared);
            }
            return;
        }
    }
    // CAT: the exe also clears a global here (0x00a77b0c), which nothing else in the ship code reads
}

real32 cat_rammingFrigateSpeedMultiplier(Ship *ship)                                        // 0x005c2120
{
    if (ramIsRammer(ship) && (((RammingFrigateSpec *)ship->ShipSpecifics)->state == RAM_Ram) &&
        cat_shipHasUpgrade(ship, CAT_UPGRADE_RamFrigate))
    {
        return RAM_SPEED_MULT_RAMMING;
    }
    return 0.0f;
}

// is the trail drawn: every trail but the main one of a ram, and the main one while ramming or pushing upgraded
bool32 cat_rammingFrigateTrailActive(void *vtrail)                                          // 0x005c2180
{
    shiptrail *trail = (shiptrail *)vtrail;
    Ship *ship = (Ship *)trail->vship;
    ubyte state;

    if (ramIsRammer(ship) && (ship->trail[0] != trail) &&
        ((((state = ((RammingFrigateSpec *)ship->ShipSpecifics)->state) != RAM_Push) && (state != RAM_Ram) &&
          (state != RAM_4)) || !cat_shipHasUpgrade(ship, CAT_UPGRADE_RamFrigate)))
    {
        return FALSE;
    }
    return TRUE;
}

// CAT: RammingFrigateHeader 0x008c5e38 (sRammingFrigate, bRammingFrigate); slot 6 is the exe's nullsub; slots 20, 22
//      and 27 are RammingFrigateMoveCommand
CustShipHeader RammingFrigateHeader =
{
    .sizeofShipSpecifics = sizeof(RammingFrigateSpec),
    .CustShipStaticInit = RammingFrigateStaticInit,
    .CustShipInit = RammingFrigateInit,
    .CustShipClose = RammingFrigateClose,
    .CustShipAttackPassive = nullsubAttackPassive,
    .CustShipSpecialActivate = RammingFrigateSpecialActivate,
    .CustShipSpecialTarget = RammingFrigateSpecialTarget,
    .CustShipHousekeep = RammingFrigateHouseKeep,
    .CustShipRemoveShipReferences = RammingFrigateRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))RammingFrigateRemoveShipReferences,
    .CustShipEffectRemoval = RammingFrigateEffectRemoval,
    .CustShipDied = RammingFrigateDied,
    .CustShip_PreFix = RammingFrigate_PreFix,
    .CustShip_Fix = RammingFrigate_Fix,
    .CustShipMoveCommand = RammingFrigateMoveCommand,
    .CustShipAttack = RammingFrigateAttack,
    .CustShipDockCommand = (bool32 (*)(struct Ship *, sdword, struct Ship *))RammingFrigateMoveCommand,
    .CustShipHaltCommand = RammingFrigateMoveCommand,
};
