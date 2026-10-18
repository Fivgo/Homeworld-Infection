// =============================================================================
//  HiveFrigate.c
//  - Cataclysm's sHiveFrigate / bHiveFrigate: a frigate that carries six
//    swarmers and sends them at its special targets
//    (cataclysm.exe 0x005b54b0-0x005b6820)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each).

#include "HiveFrigate.h"

#include "AIShip.h"
#include "Attack.h"
#include "Battle.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "Cloak.h"
#include "CommandWrap.h"
#include "DefaultShip.h"
#include "Dock.h"
#include "Gun.h"
#include "Memory.h"
#include "MeshAnim.h"
#include "Randy.h"
#include "SaveGame.h"
#include "SoundEvent.h"
#include "Swarmer.h"
#include "UnivUpdate.h"
#include "Universe.h"

HiveFrigateStatics HiveFrigateStaticSect;       // 0x00a7ace0: sHiveFrigate
HiveFrigateStatics HiveFrigateStaticBeast;      // 0x00a7aca0: bHiveFrigate

#define HIVE_AUTO_TARGET_RANGE      7000.0f
#define HIVE_SPEECH_RECALL          0x40008aa
#define HIVE_SPEECH_ORDER           0x40008a9
#define HIVE_CHATTER_LAUNCHED       0x2d

// CAT (M2-R5 dock): HiveFrigateHouseKeep 0x005b57d0 calls clDock with Cataclysm's dock purposes: 0 (the hive docks
//      with a ship), 1 (swarmers return into the hive and stay) and 2 (swarmers leave the hive through the dock command)
static void hiveFrigateDock(SelectCommand *selection, sdword purpose, Ship *dockwith)
{
    clDock(&universe.mainCommandLayer, selection, (DockType)purpose, dockwith);
}

void HiveFrigateStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)  // 0x005b54b0
{
    HiveFrigateStatics *hivestat = (statinfo->shiprace != RACE_Sect) ? &HiveFrigateStaticBeast : &HiveFrigateStaticSect;
    sdword i;

    (void)directory;
    (void)filename;
    hivestat->swarmerRegenTime = 15.0f;
    hivestat->swarmRange = 7000.0f;
    statinfo->custstatinfo = hivestat;
    statinfo->unknownFlag212 = TRUE;
    for (i = 0; i < NUM_TACTICS_TYPES; i++)
    {
        hivestat->gunRange[i] = statinfo->bulletRange[i] * 0.8f;            // 0x0072a5d4
        hivestat->tooCloseRange[i] = statinfo->minBulletRange[i] * 0.7f;    // 0x0072aaf4
    }
}

// a new swarmer, made inside the hive (the exe inlines this in HiveFrigateInit and twice in HiveFrigateHouseKeep)
static void hiveFrigateMakeSwarmer(Ship *ship, sdword slot)
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;
    Ship *swarmer;
    SwarmerSpec *swarmerspec;

    // CAT: the exe passes built 5 (bit 0 built, bit 2 not added to the universe) and the hive as the creator; this
    //      tree's univCreateShip never adds the ship and takes built 1 for "built"
    swarmer = univCreateShip((ship->shiprace == RACE_Beast) ? bSwarmer : sSwarmer, ship->shiprace,
                             &ship->obj.posinfo.position, ship->playerowner, 1, ship);
    gameStatsAddShip(swarmer, ship->playerowner->playerIndex);
    bitClear(swarmer->header.flags, SOF_Selectable);
    swarmerspec = (SwarmerSpec *)swarmer->ShipSpecifics;
    swarmerspec->hiveSlot = slot;
    swarmerspec->hive = ship;
    spec->numSwarmers++;
    dockPutShipInside(swarmer, ship);
    spec->swarmer[slot] = swarmer;
}

// the first empty slot gets a new swarmer
static void hiveFrigateReplaceSwarmer(Ship *ship)
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;
    sdword i;

    for (i = 0; i < HIVE_NUM_SWARMERS; i++)
    {
        if (spec->swarmer[i] == NULL)
        {
            hiveFrigateMakeSwarmer(ship, i);
            return;
        }
    }
}

void HiveFrigateInit(Ship *ship)                                                            // 0x005b5520
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;
    sdword i;

    spec->swarmState = HIVE_Idle;
    spec->nextSwarmerTime = universe.totaltimeelapsed;
    spec->target = NULL;
    spec->numTargets = 0;
    spec->moveState = 0;
    spec->doorsOpen = 0;
    spec->numSwarmers = 0;
    for (i = 0; i < HIVE_NUM_SWARMERS; i++)
    {
        hiveFrigateMakeSwarmer(ship, i);
    }
}

void HiveFrigateDied(Ship *ship)                                                            // 0x005b55e0
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;
    Ship *swarmer;
    sdword i;

    // the swarmers out in space die with it (those inside go with the ships inside it)
    for (i = 0; i < HIVE_NUM_SWARMERS; i++)
    {
        swarmer = spec->swarmer[i];
        if ((swarmer != NULL) && !bitTest(swarmer->header.flags, SOF_Dead) && (swarmer->shiplink.belongto != NULL))
        {
            swarmer->howDidIDie = ship->howDidIDie;
            swarmer->whoKilledMe = ship->whoKilledMe;
            univDeleteDeadShip(swarmer, 0);
        }
        spec->swarmer[i] = NULL;
    }
}

// clear the special targets (the exe inlines this in SpecialActivate, MoveCommand and DockCommand)
static void hiveFrigateClearTargets(HiveFrigateSpec *spec)
{
    sdword i;

    for (i = 0; i < spec->numTargets; i++)
    {
        spec->targets[i] = NULL;
    }
    spec->target = NULL;
    spec->numTargets = 0;
    spec->moveState = 0;
}

// recall the swarm unless it is idle or already docking (inlined in the same three)
static void hiveFrigateRecall(Ship *ship)
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;

    if ((spec->swarmState != HIVE_Idle) && (spec->swarmState != HIVE_Recalling))
    {
        speechEventQueue(ship, HIVE_SPEECH_RECALL, 0, -1, -1, -1, -1, 0.0f, -1);
        spec->swarmState = HIVE_Recall;
    }
}

void HiveFrigateRemoveShipReferences(Ship *ship,Ship *shiptoremove)                         // 0x005b5640
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;
    sdword i;

    if ((shiptoremove->shiptype == sSwarmer) || (shiptoremove->shiptype == bSwarmer))
    {
        for (i = 0; i < HIVE_NUM_SWARMERS; i++)
        {
            if (spec->swarmer[i] == shiptoremove)
            {
                spec->swarmer[i] = NULL;
                spec->numSwarmers--;
                break;
            }
        }
    }
    for (i = 0; i < spec->numTargets; i++)
    {
        if (spec->targets[i] == shiptoremove)
        {
            spec->targets[i] = NULL;
        }
    }
    if (spec->target == shiptoremove)
    {
        // on to the next live target, else the swarm comes home
        for (i = 0; i < spec->numTargets; i++)
        {
            if (spec->targets[i] != NULL)
            {
                spec->target = spec->targets[i];
                if (spec->swarmState == HIVE_Attacking)
                {
                    spec->swarmState = HIVE_Attack;
                }
                return;
            }
        }
        spec->target = NULL;
        spec->numTargets = 0;
        if (spec->swarmState != HIVE_Idle)
        {
            spec->swarmState = HIVE_Recall;
        }
    }
}

void HiveFrigateAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)               // 0x005b5730
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;
    HiveFrigateStatics *hivestat = (HiveFrigateStatics *)ship->staticinfo->custstatinfo;

    (void)maxdist;
    attackStraightForward(ship, target, hivestat->gunRange[ship->tacticstype], hivestat->tooCloseRange[ship->tacticstype]);
    spec->moveState = 0;
}

void HiveFrigateAttackPassive(Ship *ship,Ship *target,bool32 rotate)                       // 0x005b5770
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;

    if (rotate & (bool32)ship->staticinfo->rotateToRetaliate)
    {
        attackPassiveRotate(ship, target);
    }
    else
    {
        attackPassive(ship, target);
    }
    spec->moveState = 0;
}

bool32 HiveFrigateSpecialActivate(Ship *ship)                                               // 0x005b57c0
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;

    hiveFrigateRecall(ship);
    hiveFrigateClearTargets(spec);
    return TRUE;
}

bool32 HiveFrigateSpecialTarget(Ship *ship,void *custom)                                    // 0x005b5850
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;
    SpecialCommand *targets = (SpecialCommand *)custom;
    sdword num = targets->numTargets;
    Ship *target;
    sdword i;

    if (num > HIVE_MAX_TARGETS)
    {
        num = HIVE_MAX_TARGETS;
    }
    // its own swarmers are no targets
    for (i = 0; i < num; i++)
    {
        target = (Ship *)targets->TargetPtr[i];
        if ((target->playerowner == ship->playerowner) &&
            ((target->shiptype == sSwarmer) || (target->shiptype == bSwarmer)))
        {
            spec->targets[i] = NULL;
        }
        else
        {
            spec->targets[i] = target;
        }
    }
    for (i = 0; i < num; i++)
    {
        if (spec->targets[i] != NULL)
        {
            spec->target = spec->targets[i];
            break;
        }
    }
    spec->numTargets = num;
    if ((spec->swarmState != HIVE_Idle) && (spec->swarmState != HIVE_Approach) && (spec->swarmState != HIVE_Launch) &&
        (spec->swarmState != HIVE_Launching))
    {
        spec->swarmState = HIVE_Attack;
    }
    spec->moveState = 0;
    return TRUE;
}

void HiveFrigateOrderChanged(Ship *ship)                                                    // 0x005b5920
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;

    spec->moveState = 0;
    speechEventQueue(ship, HIVE_SPEECH_ORDER, 0, -1, -1, -1, -1, 0.0f, -1);
}

void HiveFrigateFire(Ship *ship,SpaceObjRotImpTarg *target)                                 // 0x005b5950
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;
    GunInfo *gunInfo = ship->gunInfo;
    Gun *gun;
    sdword i;

    if (gunInfo == NULL)
    {
        return;
    }
    for (i = 0; i < gunInfo->numGuns; i++)
    {
        gun = &gunInfo->guns[i];
        if (cat_gunCanFire(ship, gun))
        {
            switch (gun->catWeaponType)
            {
                case WEAPON_PROJECTILE:
                case WEAPON_ENERGY:
                case WEAPON_ION:
                case WEAPON_BIGGUN:
                    gunShoot(ship, gun, target);
                    break;
                case WEAPON_MISSILE:
                case WEAPON_MINE:
                    if (gun->numMissiles > 0)
                    {
                        missileShoot(ship, gun, target);
                    }
                    break;
                default:
                    break;
            }
        }
    }
    spec->moveState = 0;
}

// docked swarmers heal 1 point per second
static void cat_hiveFrigateUpdateSwarmers(Ship *hive)                                      // 0x005b59f0
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)hive->ShipSpecifics;
    Ship *swarmer;
    SwarmerSpec *swarmerspec;
    real32 maxhealth;
    sdword i;

    for (i = 0; i < HIVE_NUM_SWARMERS; i++)
    {
        swarmer = spec->swarmer[i];
        if (swarmer == NULL)
        {
            continue;
        }
        swarmerspec = (SwarmerSpec *)swarmer->ShipSpecifics;
        maxhealth = swarmer->staticinfo->health.maxhealth;
        if ((cat_swarmerGetState(swarmer) == CAT_SWARMER_Docked) && (swarmer->targetable.health < maxhealth))
        {
            if (swarmerspec->lastHealTime > 0.0f)
            {
                if (universe.totaltimeelapsed - swarmerspec->lastHealTime > 1.0f)
                {
                    swarmer->targetable.health = (universe.totaltimeelapsed - swarmerspec->lastHealTime) +
                                                 swarmer->targetable.health;
                    if (swarmer->targetable.health > maxhealth)
                    {
                        swarmer->targetable.health = maxhealth;
                    }
                    swarmerspec->lastHealTime = universe.totaltimeelapsed;
                }
            }
            else
            {
                swarmerspec->lastHealTime = universe.totaltimeelapsed;
            }
        }
    }
}

bool32 cat_hiveFrigateSwarmReady(Ship *hive)                                               // 0x005b5ac0
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)hive->ShipSpecifics;

    return (spec->target != NULL) && (spec->swarmState != HIVE_Idle);
}

void cat_hiveFrigateResetSwarm(Ship *hive)                                                 // 0x005b5af0
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)hive->ShipSpecifics;
    sdword i;

    spec->swarmState = HIVE_Idle;
    for (i = 0; i < HIVE_NUM_SWARMERS; i++)
    {
        if (spec->swarmer[i] != NULL)
        {
            cat_swarmerSetState(spec->swarmer[i], CAT_SWARMER_Docked);
        }
    }
}

// the selection of one ship, for an order the hive gives itself
static void hiveFrigateSelectSelf(SelectCommand *selectone, Ship *ship)
{
    selectone->numShips = 1;
    selectone->timeLastStatus = 0.0f;
    selectone->ShipPtr[0] = ship;
}

// the enemies within 7000 as a target list (memAlloc'd; NULL if none)
static SelectAnyCommand *hiveFrigateNearbyEnemies(Ship *ship)
{
    SelectCommand *enemies = getEnemiesWithinProximity(ship, HIVE_AUTO_TARGET_RANGE);
    SelectAnyCommand *targets;
    sdword i;

    if ((enemies == NULL) || (enemies->numShips <= 0))
    {
        return NULL;
    }
    targets = memAlloc(sizeofSelectCommand(enemies->numShips), "HiveTargets", 0);
    for (i = 0; i < enemies->numShips; i++)
    {
        targets->TargetPtr[i] = (TargetPtr)enemies->ShipPtr[i];
    }
    targets->numTargets = enemies->numShips;
    return targets;
}

// fly toward the target while out of swarm range (inlined three times in HiveFrigateHouseKeep); TRUE when in range
static bool32 hiveFrigateApproachTarget(Ship *ship, HiveFrigateSpec *spec, HiveFrigateStatics *hivestat)
{
    SpaceObjRotImpTarg *target = (SpaceObjRotImpTarg *)spec->target;
    vector trajectory;

    aishipGetTrajectory(ship, target, &trajectory);
    if (RangeToTarget(ship, target, &trajectory) < hivestat->swarmRange)
    {
        return TRUE;
    }
    spec->moveState = 1;
    if (spec->swarmState == HIVE_Idle)
    {
        spec->swarmState = HIVE_Approach;
    }
    aishipFlyToPointAvoidingObjsFunc(ship, &target->obj.posinfo.position, 7, -400.0f, &target->obj.posinfo.velocity);
    ship->shipidle = FALSE;
    ship->autostabilizeship = FALSE;
    return FALSE;
}

void HiveFrigateHouseKeep(Ship *ship)                                                       // 0x005b5b30
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;
    HiveFrigateStatics *hivestat = (HiveFrigateStatics *)ship->staticinfo->custstatinfo;
    SelectCommandMax6 selection;
    SelectCommandMax6 selectone;
    AttackCommandMax6 attackone;
    SelectAnyCommand *targets;
    Ship *swarmer;
    SwarmerSpec *swarmerspec;
    sdword state;
    sdword i, num;
    bool32 allDone;
    double delay;
    vector trajectory;

    if (bitTest(ship->shipOrDerelict.specialFlags, SPECIAL_SinglePlayerWithinHyperSpaceRange))
    {
        return;
    }
    // CAT-TODO(stage 3 FOW): a target the player cannot see (cat_fowCanPlayerSeeShip(owner, target, 1) 0x0047f480)
    //      cancels the special order (clWrapSpecial with no targets); without the fog of war every target is seen

    switch (spec->swarmState)
    {
        case HIVE_Idle:
            // an aggressive idle hive picks the enemies near it as its targets (every 16th update)
            if ((ship->tacticstype == Aggressive) && (spec->target == NULL) &&
                ((universe.univUpdateCounter & 0xf) == 0xe) &&                  // 0x008ac584 / 0x008ac588
                ((targets = hiveFrigateNearbyEnemies(ship)) != NULL))
            {
                hiveFrigateSelectSelf((SelectCommand *)&selectone, ship);
                clSpecial(&universe.mainCommandLayer, (SelectCommand *)&selectone, targets);
                memFree(targets);
            }
            if (spec->doorsOpen)
            {
                cat_madAnimationStartNumber(ship, 1);       // CAT-TODO(stage 3): the exe passes a third argument, 0
                spec->doorsOpen = 0;
            }
            // fall through
        case HIVE_Approach:
            if (spec->moveState == 2)
            {
                hiveFrigateSelectSelf((SelectCommand *)&selectone, ship);
                hiveFrigateDock((SelectCommand *)&selectone, 0, spec->dockwith);
                spec->moveState = 0;
            }
            if (spec->nextSwarmerTime < universe.totaltimeelapsed)
            {
                spec->nextSwarmerTime = universe.totaltimeelapsed + hivestat->swarmerRegenTime;
                hiveFrigateReplaceSwarmer(ship);
            }
            cat_hiveFrigateUpdateSwarmers(ship);
            if (spec->target != NULL)
            {
                if (hiveFrigateApproachTarget(ship, spec, hivestat))
                {
                    spec->swarmState = HIVE_Launch;
                    spec->moveState = 0;
                }
                return;
            }
            break;

        case HIVE_Launch:
            // each docked swarmer gets a random launch delay of 0 .. 1.7 s
            num = 0;
            for (i = 0; i < HIVE_NUM_SWARMERS; i++)
            {
                swarmer = spec->swarmer[i];
                if ((swarmer != NULL) && (cat_swarmerGetState(swarmer) == CAT_SWARMER_Docked))
                {
                    num++;
                    delay = cat_randomExpDelay();
                    delay = delay + delay;
                    swarmerspec = (SwarmerSpec *)swarmer->ShipSpecifics;
                    swarmerspec->moveDelay = (real32)delay;
                    swarmerspec->moveTime = (real32)(delay + (double)universe.totaltimeelapsed);
                }
            }
            if (num < 1)
            {
                spec->swarmState = HIVE_Idle;
                return;
            }
            // an aggressive hive also attacks the enemies near it
            if ((ship->tacticstype == Aggressive) && ((targets = hiveFrigateNearbyEnemies(ship)) != NULL))
            {
                hiveFrigateSelectSelf((SelectCommand *)&selectone, ship);
                clAttack(&universe.mainCommandLayer, (SelectCommand *)&selectone, (AttackCommand *)targets);
                memFree(targets);
            }
            spec->swarmState = HIVE_Launching;
            if (!spec->doorsOpen)
            {
                cat_madAnimationStartNumber(ship, 0);       // CAT-TODO(stage 3): the exe passes a third argument, 0
                spec->doorsOpen = 1;
                return;
            }
            break;

        case HIVE_Launching:
            allDone = TRUE;
            num = 0;
            for (i = 0; i < HIVE_NUM_SWARMERS; i++)
            {
                swarmer = spec->swarmer[i];
                if (swarmer == NULL)
                {
                    continue;
                }
                state = cat_swarmerGetState(swarmer);
                if (state == CAT_SWARMER_Docked)
                {
                    if (((SwarmerSpec *)swarmer->ShipSpecifics)->moveTime < universe.totaltimeelapsed)
                    {
                        cat_swarmerSetState(swarmer, CAT_SWARMER_Launching);
                        selection.ShipPtr[num++] = swarmer;
                    }
                }
                else if (state != CAT_SWARMER_Launching)
                {
                    continue;
                }
                allDone = FALSE;
            }
            if (num > 0)
            {
                selection.numShips = num;
                hiveFrigateDock((SelectCommand *)&selection, 2, ship);
            }
            if (allDone)
            {
                spec->swarmState = HIVE_Attack;
                if (battleCanChatterAtThisTime(HIVE_CHATTER_LAUNCHED, ship))
                {
                    battleChatterAttempt(-1, HIVE_CHATTER_LAUNCHED, ship, -1);
                    return;
                }
            }
            break;

        case HIVE_Attack:
            num = 0;
            for (i = 0; i < HIVE_NUM_SWARMERS; i++)
            {
                swarmer = spec->swarmer[i];
                if ((swarmer != NULL) && ((state = cat_swarmerGetState(swarmer)) != CAT_SWARMER_Docked) &&
                    (state != CAT_SWARMER_Launching))
                {
                    cat_swarmerSetState(swarmer, CAT_SWARMER_Attacking);
                    selection.ShipPtr[num++] = swarmer;
                }
            }
            if ((num > 0) && (spec->target != NULL))
            {
                selection.numShips = num;
                attackone.numTargets = 1;
                attackone.TargetPtr[0] = (TargetPtr)spec->target;
                clAttack(&universe.mainCommandLayer, (SelectCommand *)&selection, (AttackCommand *)&attackone);
            }
            spec->swarmState = HIVE_Attacking;
            if ((spec->target != NULL) && !hiveFrigateApproachTarget(ship, spec, hivestat))
            {
                return;
            }
            break;

        case HIVE_Attacking:
            if ((spec->moveState == 1) && (spec->target != NULL))
            {
                if (hiveFrigateApproachTarget(ship, spec, hivestat))
                {
                    spec->moveState = 0;
                }
            }
            // the swarmers that came free go at the target again
            num = 0;
            for (i = 0; i < HIVE_NUM_SWARMERS; i++)
            {
                swarmer = spec->swarmer[i];
                if ((swarmer != NULL) && (cat_swarmerGetState(swarmer) == CAT_SWARMER_Free))
                {
                    cat_swarmerSetState(swarmer, CAT_SWARMER_Attacking);
                    selection.ShipPtr[num++] = swarmer;
                }
            }
            if ((num > 0) && (spec->target != NULL))
            {
                selection.numShips = num;
                attackone.numTargets = 1;
                attackone.TargetPtr[0] = (TargetPtr)spec->target;
                clAttack(&universe.mainCommandLayer, (SelectCommand *)&selection, (AttackCommand *)&attackone);
            }
            // in range, the docked ones launch too
            if (spec->target != NULL)
            {
                aishipGetTrajectory(ship, (SpaceObjRotImpTarg *)spec->target, &trajectory);
                if (RangeToTarget(ship, (SpaceObjRotImpTarg *)spec->target, &trajectory) < hivestat->swarmRange)
                {
                    num = 0;
                    for (i = 0; i < HIVE_NUM_SWARMERS; i++)
                    {
                        swarmer = spec->swarmer[i];
                        if ((swarmer != NULL) && (cat_swarmerGetState(swarmer) == CAT_SWARMER_Docked))
                        {
                            cat_swarmerSetState(swarmer, CAT_SWARMER_Launching);
                            selection.ShipPtr[num++] = swarmer;
                        }
                    }
                    if (num > 0)
                    {
                        selection.numShips = num;
                        hiveFrigateDock((SelectCommand *)&selection, 2, ship);
                    }
                }
            }
            if (spec->nextSwarmerTime < universe.totaltimeelapsed)
            {
                spec->nextSwarmerTime = universe.totaltimeelapsed + hivestat->swarmerRegenTime;
                hiveFrigateReplaceSwarmer(ship);
                return;
            }
            break;

        case HIVE_Recall:
            num = 0;
            for (i = 0; i < HIVE_NUM_SWARMERS; i++)
            {
                swarmer = spec->swarmer[i];
                if ((swarmer != NULL) && ((state = cat_swarmerGetState(swarmer)) != CAT_SWARMER_Docked) &&
                    (state != CAT_SWARMER_Returning))
                {
                    num++;
                    swarmerspec = (SwarmerSpec *)swarmer->ShipSpecifics;
                    swarmerspec->moveTime = universe.totaltimeelapsed + swarmerspec->moveDelay;
                }
            }
            if (num > 0)
            {
                spec->swarmState = HIVE_Recalling;
                return;
            }
            spec->swarmState = HIVE_Idle;
            if (spec->doorsOpen)
            {
                cat_madAnimationStartNumber(ship, 1);       // CAT-TODO(stage 3): the exe passes a third argument, 0
                spec->doorsOpen = 0;
            }
            break;

        case HIVE_Recalling:
            allDone = TRUE;
            num = 0;
            for (i = 0; i < HIVE_NUM_SWARMERS; i++)
            {
                swarmer = spec->swarmer[i];
                if (swarmer == NULL)
                {
                    continue;
                }
                state = cat_swarmerGetState(swarmer);
                if ((state == CAT_SWARMER_Free) || (state == CAT_SWARMER_Attacking))
                {
                    if (((SwarmerSpec *)swarmer->ShipSpecifics)->moveTime < universe.totaltimeelapsed)
                    {
                        cat_swarmerSetState(swarmer, CAT_SWARMER_Returning);
                        selection.ShipPtr[num++] = swarmer;
                    }
                }
                else if (state == CAT_SWARMER_Docked)
                {
                    continue;
                }
                allDone = FALSE;
            }
            if (num > 0)
            {
                selection.numShips = num;
                hiveFrigateDock((SelectCommand *)&selection, 1, ship);
            }
            if (!allDone)
            {
                return;
            }
            spec->swarmState = HIVE_Idle;
            if (spec->doorsOpen)
            {
                cat_madAnimationStartNumber(ship, 1);       // CAT-TODO(stage 3): the exe passes a third argument, 0
                spec->doorsOpen = 0;
            }
            break;

        default:
            break;
    }
}

bool32 HiveFrigateMoveCommand(Ship *ship)                                                   // 0x005b65c0
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;

    // a move ends an approach (the recall inside never fires: the state is idle or approaching)
    if ((spec->swarmState == HIVE_Idle) || (spec->swarmState == HIVE_Approach))
    {
        hiveFrigateRecall(ship);
        hiveFrigateClearTargets(spec);
    }
    spec->moveState = 0;
    return TRUE;
}

bool32 HiveFrigateHaltCommand(Ship *ship)                                                   // 0x005b6650
{
    ((HiveFrigateSpec *)ship->ShipSpecifics)->moveState = 0;
    return TRUE;
}

bool32 HiveFrigateDockCommand(Ship *ship, sdword dockType, Ship *dockwith)                  // 0x005b6670
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;

    (void)dockType;
    if (spec->swarmState == HIVE_Idle)
    {
        return TRUE;
    }
    // a hive with its swarm out recalls it first and docks later (HouseKeep issues the waiting dock order)
    hiveFrigateRecall(ship);
    hiveFrigateClearTargets(spec);
    spec->dockwith = dockwith;
    spec->moveState = 2;
    return FALSE;
}

void HiveFrigate_PreFix(Ship *ship)                                                         // 0x005b6720
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;
    sdword i;

    for (i = 0; i < HIVE_NUM_SWARMERS; i++)
    {
        spec->swarmer[i] = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->swarmer[i]);
    }
    for (i = 0; i < spec->numTargets; i++)
    {
        spec->targets[i] = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->targets[i]);
    }
    spec->target = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->target);
    spec->dockwith = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->dockwith);
}

void HiveFrigate_Fix(Ship *ship)                                                            // 0x005b67a0
{
    HiveFrigateSpec *spec = (HiveFrigateSpec *)ship->ShipSpecifics;
    sdword i;

    for (i = 0; i < HIVE_NUM_SWARMERS; i++)
    {
        spec->swarmer[i] = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->swarmer[i]);
    }
    for (i = 0; i < spec->numTargets; i++)
    {
        spec->targets[i] = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->targets[i]);
    }
    spec->target = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->target);
    spec->dockwith = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->dockwith);
}

// CAT: HiveFrigateHeader 0x008c4b40 (sHiveFrigate, bHiveFrigate)
CustShipHeader HiveFrigateHeader =
{
    .sizeofShipSpecifics = sizeof(HiveFrigateSpec),
    .CustShipStaticInit = HiveFrigateStaticInit,
    .CustShipInit = HiveFrigateInit,
    .CustShipFire = HiveFrigateFire,
    .CustShipAttackPassive = HiveFrigateAttackPassive,
    .CustShipSpecialActivate = HiveFrigateSpecialActivate,
    .CustShipSpecialTarget = HiveFrigateSpecialTarget,
    .CustShipOrderChanged = HiveFrigateOrderChanged,
    .CustShipHousekeep = HiveFrigateHouseKeep,
    .CustShipRemoveShipReferences = HiveFrigateRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))HiveFrigateRemoveShipReferences,
    .CustShipDied = HiveFrigateDied,
    .CustShip_PreFix = HiveFrigate_PreFix,
    .CustShip_Fix = HiveFrigate_Fix,
    .CustShipMoveCommand = HiveFrigateMoveCommand,
    .CustShipAttack = HiveFrigateAttack,
    .CustShipDockCommand = HiveFrigateDockCommand,
    .CustShipHaltCommand = HiveFrigateHaltCommand,
};
