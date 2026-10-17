// =============================================================================
//  Mimic.c
//  - Cataclysm's sMimic: a fighter that disguises itself as another ship or
//    as an asteroid (cataclysm.exe 0x005b8330-0x005b9c60); the mimic block is
//    shared by sMCV (MCV.c) and p1StandardCorvette
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each).
//      cat_shipIsMimicking, cat_mimicGetApparentShipType, cat_mimicGetSelectRadius, cat_mimicAppearsAlliedTo,
//      cat_mimicAppearsAsResource and cat_mimicAppearsAsDerelict moved here from ShipSelect.c (their declarations
//      stay in ShipSelect.h); cat_shipGetRamDamageScale moved here from Collision.c (it was a static stand-in).
//      Not ported here: the mimic menu tables and their save chunks (cat_mimicInitDisguiseTables 0x005b9c60 ..
//      cat_saveGameMiscChunks 0x005ba170, the disguise list of the special-order menu: stage 3 UI).

#include "Mimic.h"

#include <math.h>

#include "CameraCommand.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "Damage.h"
#include "Globals.h"
#include "Memory.h"
#include "Randy.h"
#include "SaveGame.h"
#include "ShipSelect.h"
#include "SoundEvent.h"
#include "UnivUpdate.h"
#include "Universe.h"

#define MIMIC_FADE_STEP             0.05f       // 0x0072a654: per housekeep
#define MIMIC_RADIUS_SCALE          1.2f        // 0x0087813c
#define MIMIC_ASTEROID_RADIUS_SCALE 1.1f        // 0x0072af7c
#define MIMIC_DELAY_RATE            9.313225746154785e-08f      // 0x0072a64c
#define MIMIC_DELAY_EXP             -0.004999999888241291f      // 0x0072a648
#define MIMIC_DELAY_BASE            0.0f        // 0x0072a548

// the disguises cat_mimicRandomDisguiseType picks from (0x008c4da0 ..): Sect / Beast fighters and corvettes
static const ShipType mimicDisguiseSectFighter[4] = { 0x00, 0x01, 0x03, 0x11 };            // 0x008c4da0
static const ShipType mimicDisguiseSectCorvette[5] = { 0x04, 0x0e, 0x07, 0x06, 0x08 };     // 0x008c4db0
static const ShipType mimicDisguiseBeastFighter[4] = { 0x21, 0x34, 0x23, 0x25 };           // 0x008c4dc4
static const ShipType mimicDisguiseBeastCorvette[6] = { 0x26, 0x31, 0x29, 0x2a, 0x30, 0x2d };  // 0x008c4dd4

static bool32 mimicIsMimic(Ship *ship)
{
    return (ship->header.objtype == OBJ_ShipType) && bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic);
}

// a short random delay: (0 - e^(rand * 9.3e-8 * -0.005)) + now, always at most now (the exe's x87 exp: f2xm1/fscale)
static real32 mimicDelayEnd(void)
{
    real32 r = (real32)ranRandom(RANDOM_CAT_SHIPS);

    return (MIMIC_DELAY_BASE - (real32)exp((double)(r * MIMIC_DELAY_RATE * MIMIC_DELAY_EXP))) + universe.totaltimeelapsed;
}

// free the disguise data and the fake asteroid (inlined in several of the exe's functions)
static void mimicFreeDisguise(MimicSpec *spec)
{
    if (spec->apparentBindings != NULL)
    {
        memFree(spec->apparentBindings);
        spec->apparentBindings = NULL;
    }
    if (spec->nextBindings != NULL)
    {
        memFree(spec->nextBindings);
        spec->nextBindings = NULL;
    }
    if (spec->fakeResource != NULL)
    {
        cat_univDeleteMimicResource(spec->fakeResource);
        spec->fakeResource = NULL;
    }
}

static void mimicSetDetectedAll(MimicSpec *spec, bool32 detected)
{
    sdword i;

    for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++)
    {
        spec->detected[i] = detected;
    }
}

void MimicStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)       // 0x005b8330
{
    GenericInterceptorStaticInit(directory, filename, statinfo);
    statinfo->canSpecialBandBoxFriendlies = TRUE;
    statinfo->unknownFlag212 = TRUE;
    statinfo->unknownFlag213 = TRUE;
}

void MimicInit(Ship *ship)                                                                  // 0x005b8360
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    GenericInterceptorInit(ship);
    bitSet(ship->header.catFlags18, CAT_SHIPFLAG_Mimic);
    spec->mode = MIMIC_Off;
    spec->fade = 0.0f;
    spec->apparentBindings = NULL;
    spec->apparentPlayer = -1;
    spec->apparentType = -1;
    spec->apparentRace = -1;
    spec->apparentAsteroid = -1;
    spec->nextBindings = NULL;
    spec->nextPlayer = -1;
    spec->nextType = -1;
    spec->nextRace = -1;
    spec->nextAsteroid = -1;
    spec->fakeResource = NULL;
    spec->ramDamageScale = 2.0f;                    // 0x40000000
}

sdword cat_mimicRandomDisguiseType(ShipType type, sdword race)                             // 0x005b83e0
{
    if (type == sMimic)
    {
        if (race == RACE_Sect)
        {
            return mimicDisguiseSectFighter[ranRandom(RANDOM_AI_PLAYER) & 3];
        }
        if (race == RACE_Beast)
        {
            return mimicDisguiseBeastFighter[ranRandom(RANDOM_AI_PLAYER) & 3];
        }
    }
    else if (type == sMCV)
    {
        if (race == RACE_Sect)
        {
            return mimicDisguiseSectCorvette[ranRandom(RANDOM_AI_PLAYER) % 5];
        }
        if (race == RACE_Beast)
        {
            return mimicDisguiseBeastCorvette[ranRandom(RANDOM_AI_PLAYER) % 6];
        }
    }
    return race;
}

void MimicAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)                     // 0x005b8470
{
    GenericInterceptorAttack(ship, target, maxdist);
    bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_Kamikaze | SPECIAL_BrokenFormation);
}

void MimicAttackPassive(Ship *ship,Ship *target,bool32 rotate)                              // 0x005b84a0
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;
    CommandToDo *command = ship->command;

    if ((spec->mode == MIMIC_On) && (command != NULL) && (command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
        (command->formation.formationtype == SPHERE_FORMATION))
    {
        bitSet(ship->shipOrDerelict.specialFlags, SPECIAL_Kamikaze | SPECIAL_BrokenFormation);
        return;
    }
    if (spec->mode == MIMIC_Off)
    {
        GenericInterceptorPassiveAttack(ship, target, rotate);
    }
}

void cat_mimicUnmimic(Ship *ship, bool32 animate)                                          // 0x005b84f0
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    switch (spec->mode)
    {
        case MIMIC_On:
            spec->fade = 0.0f;
            if (!animate)
            {
                spec->mode = MIMIC_FadeOut;
                break;
            }
            spec->delayEnd = mimicDelayEnd();
            // fall through
        case MIMIC_DelaySwitch:
            spec->mode = MIMIC_DelayFadeOut;
            break;
        case MIMIC_FadeIn:
        case MIMIC_Switch:
        case MIMIC_FadeOutUnlink:
            spec->mode = MIMIC_FadeOut;
            break;
        case MIMIC_Linked:
        case MIMIC_DelayFadeIn:
            spec->mode = MIMIC_Off;
            mimicFreeDisguise(spec);
            break;
        default:
            break;
    }
    mimicSetDetectedAll(spec, TRUE);
}

void cat_mimicCopyFromPartner(Ship *ship, Ship *partner)                                   // 0x005b8620
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;
    MimicSpec *pspec = (MimicSpec *)partner->ShipSpecifics;

    spec->apparentLOD = pspec->nextLOD;
    spec->apparentColor = pspec->nextColor;
    spec->apparentType = pspec->nextType;
    spec->apparentRace = pspec->nextRace;
    spec->apparentRadius = pspec->nextRadius;
    spec->asteroidRadius = pspec->nextAsteroidRadius;
    spec->asteroidCameraRadius = pspec->nextAsteroidCameraRadius;
    spec->apparentAsteroid = pspec->nextAsteroid;
    spec->apparentPlayer = pspec->nextPlayer;
    spec->asteroidResourceValue = pspec->nextAsteroidResourceValue;
    spec->fade = 1.0f;
    if ((pspec->nextType != -1) && (pspec->nextBindings != NULL))
    {
        spec->apparentBindings = cat_univMeshBindingsDupeNoGuns(GetShipStaticInfo(pspec->nextType, 0));
        spec->mode = MIMIC_Linked;
        return;
    }
    spec->nextBindings = NULL;
    spec->mode = MIMIC_Linked;
}

void cat_mimicResetState(Ship *ship)                                                        // 0x005b8700
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    spec->link.partner = NULL;
    switch (spec->mode)
    {
        case MIMIC_Off:
        case MIMIC_Linked:
            spec->fade = 1.0f;
            spec->delayEnd = mimicDelayEnd();
            spec->mode = MIMIC_DelayFadeIn;
            return;
        case MIMIC_On:
            spec->fade = 0.0f;
            spec->delayEnd = mimicDelayEnd();
            // fall through
        case MIMIC_DelayFadeOut:
            spec->mode = MIMIC_DelaySwitch;
            return;
        case MIMIC_FadeIn:
        case MIMIC_FadeOut:
            spec->mode = MIMIC_Switch;
            return;
        default:
            return;
    }
}

bool32 MimicSpecialActivate(Ship *ship)                                                     // 0x005b8820
{
    cat_mimicUnmimic(ship, TRUE);
    return TRUE;
}

// the colour scheme a disguise as a ship of type statinfo posing as player shows (cat_mimicChooseApparentPlayer
//      0x005b8840): the player's own if the type has it, else the first the type has; single player by the type's race
static sdword cat_mimicChooseApparentPlayer(ShipStaticInfo *statinfo, sdword player)       // 0x005b8840
{
    sdword i;

    if (!singlePlayerGame)
    {
        if (statinfo->shipOrDerelict.teamColor[player])
        {
            return player;
        }
        for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++)
        {
            if (statinfo->shipOrDerelict.teamColor[i])
            {
                return i;
            }
        }
        return 0;
    }
    switch (cat_ShipTypeToRace(statinfo->shiptype))
    {
        case RACE_Sect:     i = 0; break;
        case RACE_Beast:    i = 1; break;
        case RACE_Kushan:   i = 4; break;
        case RACE_Taiidan:  i = 2; break;
        case RACE_P1:
        case RACE_Traders:
        case RACE_Derelict: i = 5; break;
        default:            i = -1; break;
    }
    if ((i >= 0) && statinfo->shipOrDerelict.teamColor[i])
    {
        return i;
    }
    for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++)
    {
        if (statinfo->shipOrDerelict.teamColor[i])
        {
            return i;
        }
    }
    return 0;
}

void cat_mimicBecomeShipType(Ship *ship, ShipType type, sdword player)                     // 0x005b88f0
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;
    ShipStaticInfo *statinfo = GetShipStaticInfo(type, 0);
    real32 radius;

    // what each mimic type may pose as (p1StandardCorvette: anything)
    if (ship->shiptype != p1StandardCorvette)
    {
        switch (statinfo->shipclass)
        {
            case CLASS_Mothership:
            case CLASS_HeavyCruiser:
            case CLASS_Carrier:
            case CLASS_Destroyer:
            case CLASS_MonsterShip:
            case CLASS_Component:
                return;
            case CLASS_Frigate:
                if (ship->shiptype == sMimic)
                {
                    return;
                }
                break;
            case CLASS_Fighter:
            case CLASS_Micro:
                if (ship->shiptype == sMCV)
                {
                    if (!cat_shipHasUpgrade(ship, CAT_UPGRADE_MimicSmall))
                    {
                        return;
                    }
                    spec->mode = (spec->mode != MIMIC_Off) + MIMIC_Unlink;
                }
                break;
            case CLASS_Resource:
                if (ship->shiptype == sMimic)
                {
                    return;
                }
                if (type == sProcessor)
                {
                    return;
                }
                if (type == bProcessor)
                {
                    return;
                }
                break;
            case CLASS_NonCombat:
                if ((ship->shiptype == sMimic) && (type != 0x73))
                {
                    return;
                }
                break;
            default:
                break;
        }
    }
    radius = (ship->staticinfo->common.staticCollInfo.collspheresize < statinfo->common.staticCollInfo.collspheresize)
             ? statinfo->common.staticCollInfo.collspheresize : ship->staticinfo->common.staticCollInfo.collspheresize;
    if (spec->mode == MIMIC_Off)
    {
        spec->apparentType = type;
        spec->apparentRace = universe.players[player].race;
        spec->apparentLOD = statinfo->common.LOD;
        spec->apparentRadius = radius * MIMIC_RADIUS_SCALE;
        spec->apparentBindings = cat_univMeshBindingsDupeNoGuns(statinfo);
        spec->apparentColor = cat_mimicChooseApparentPlayer(statinfo, player);
        spec->apparentPlayer = player;
        spec->apparentAsteroid = -1;
        cat_mimicResetState(ship);
        return;
    }
    spec->nextType = type;
    spec->nextRace = universe.players[player].race;
    spec->nextLOD = statinfo->common.LOD;
    spec->nextRadius = radius * MIMIC_RADIUS_SCALE;
    if (spec->nextBindings != NULL)
    {
        memFree(spec->nextBindings);
    }
    spec->nextBindings = cat_univMeshBindingsDupeNoGuns(statinfo);
    spec->nextColor = cat_mimicChooseApparentPlayer(statinfo, player);
    spec->nextPlayer = player;
    spec->nextAsteroid = -1;
    cat_mimicResetState(ship);
}

void cat_mimicBecomeTarget(Ship *ship, sdword asteroid)                                     // 0x005b8b00
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;
    AsteroidStaticInfo *aststat;
    real32 cameraRadius;
    real32 radius, shown;
    udword r;

    // 0: a random asteroid (an sMimic Asteroid1 or Asteroid2, the others Asteroid2 or Asteroid3)
    if (asteroid == 0)
    {
        r = ranRandom(RANDOM_CAT_SHIPS) & 1;
        if (ship->shiptype == sMimic)
        {
            asteroid = (r == 0) ? 1 : 2;
        }
        else
        {
            asteroid = (r != 0) ? 3 : 2;
        }
    }
    if (asteroid == 1)
    {
        if (ship->shiptype == sMCV)
        {
            if (!cat_shipHasUpgrade(ship, CAT_UPGRADE_MimicSmall))
            {
                return;
            }
            spec->mode = (spec->mode != MIMIC_Off) + MIMIC_Unlink;
        }
        cameraRadius = 130.0f;                      // 0x43020000
    }
    else if (asteroid == 2)
    {
        cameraRadius = 500.0f;                      // 0x43fa0000
    }
    else if (asteroid == 3)
    {
        if (ship->shiptype == sMimic)
        {
            return;
        }
        cameraRadius = 800.0f;                      // 0x44480000
    }
    else
    {
        // CAT-FIX: any other value leaves the exe's camera radius and asteroid type undefined (the type is the ship
        //      pointer, indexing asteroidStaticInfos out of range); MimicSpecialTarget only passes 1-3
        return;
    }
    aststat = &asteroidStaticInfos[asteroid];
    radius = fsqrt((aststat->common.staticCollInfo.uplength * 0.5f) * (aststat->common.staticCollInfo.uplength * 0.5f) +
                   (aststat->common.staticCollInfo.rightlength * 0.5f) * (aststat->common.staticCollInfo.rightlength * 0.5f) +
                   (aststat->common.staticCollInfo.forwardlength * 0.5f) * (aststat->common.staticCollInfo.forwardlength * 0.5f)) *
             MIMIC_ASTEROID_RADIUS_SCALE;
    shown = radius;
    if (radius <= ship->staticinfo->common.staticCollInfo.collspheresize)
    {
        shown = ship->staticinfo->common.staticCollInfo.collspheresize;
    }
    if (spec->mode != MIMIC_Off)
    {
        spec->nextType = -1;
        spec->nextRace = -1;
        spec->nextAsteroid = asteroid;
        spec->nextLOD = aststat->common.LOD;
        spec->nextRadius = shown * MIMIC_RADIUS_SCALE;
        if (spec->nextBindings != NULL)
        {
            memFree(spec->nextBindings);
        }
        spec->nextBindings = NULL;
        spec->nextColor = 0;
        spec->nextAsteroidRadius = radius;
        spec->nextAsteroidCameraRadius = cameraRadius;
        spec->nextPlayer = -1;
        spec->nextAsteroidResourceValue = aststat->resource.resourcevalue * 10;
        cat_mimicResetState(ship);
        return;
    }
    spec->apparentType = -1;
    spec->apparentRace = -1;
    spec->apparentAsteroid = asteroid;
    spec->apparentLOD = aststat->common.LOD;
    spec->apparentBindings = NULL;
    spec->apparentColor = 0;
    spec->apparentRadius = shown * MIMIC_RADIUS_SCALE;
    spec->asteroidRadius = radius;
    spec->asteroidCameraRadius = cameraRadius;
    spec->apparentPlayer = -1;
    spec->asteroidResourceValue = aststat->resource.resourcevalue * 10;
    cat_mimicResetState(ship);
}

void cat_mimicCommand(Ship *ship, sdword mode, sdword type, sdword player, sdword asteroid)  // 0x005b8d70
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    if (cat_shipHasUpgrade(ship, CAT_UPGRADE_Mimic) && (spec->link.partner == NULL))
    {
        if (mode == MIMIC_COMMAND_Unmimic)
        {
            cat_mimicUnmimic(ship, TRUE);
        }
        else if (mode == MIMIC_COMMAND_ShipType)
        {
            cat_mimicBecomeShipType(ship, type, player);
        }
        else if (mode == MIMIC_COMMAND_Target)
        {
            cat_mimicBecomeTarget(ship, asteroid);
        }
    }
}

bool32 MimicSpecialTarget(Ship *ship, void *custom)                                         // 0x005b8dd0
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;
    SpaceObj *target = (SpaceObj *)((SpecialCommand *)custom)->TargetPtr[0];
    sdword asteroid;

    if (spec->link.partner != NULL)
    {
        return TRUE;
    }
    if (!cat_shipHasUpgrade(ship, CAT_UPGRADE_Mimic))
    {
        return TRUE;
    }
    if (target->header.objtype == OBJ_ShipType)
    {
        cat_mimicBecomeShipType(ship, ((Ship *)target)->shiptype, ((Ship *)target)->playerowner->playerIndex);
        return TRUE;
    }
    if (target->header.objtype == OBJ_AsteroidType)
    {
        asteroid = ((Asteroid *)target)->asteroidtype;
        if (asteroid > 0)
        {
            if (asteroid == 1)
            {
                asteroid = 1;
            }
            else if (asteroid == 3)
            {
                asteroid = 3;
            }
            else
            {
                asteroid = 2;
            }
            if (((ship->shiptype == sMimic) && (asteroid == 3)) ||
                ((ship->shiptype == sMCV) && (asteroid == 1) && !cat_shipHasUpgrade(ship, CAT_UPGRADE_MimicSmall)))
            {
                asteroid = 2;
            }
            cat_mimicBecomeTarget(ship, asteroid);
        }
    }
    return TRUE;
}

void cat_mimicCleanup(Ship *ship)                                                           // 0x005b8ea0
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatStayMimicked))
    {
        spec->mode = MIMIC_Off;
        spec->started = FALSE;
        mimicFreeDisguise(spec);
        mimicSetDetectedAll(spec, TRUE);
    }
}

// the unlink order a fading MCV half gives itself (MimicHouseKeep: a 0xC-byte selection of one, cat_clUnlink 0x0044d7a0)
static void mimicUnlinkSelf(Ship *ship)
{
    SelectCommand *selectone;

    // CAT: cat_clUnlink 0x0044d7a0 on a heap selection of one, freed after (MimicHouseKeep 0x005b9320)
    selectone = memAlloc(sizeofSelectCommand(1),"MimicUnlink",0);
    selectone->numShips = 1;
    selectone->timeLastStatus = 0.0f;     // CAT-FIX: the exe leaves it uninitialised (0x005b9323)
    selectone->ShipPtr[0] = ship;
    cat_clUnlink(&universe.mainCommandLayer,selectone);
    memFree(selectone);
}

void MimicHouseKeep(Ship *ship)                                                             // 0x005b8f30
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;
    FocusCommand focus;

    switch (spec->mode)
    {
        case MIMIC_On:
            if (spec->apparentType != -1)
            {
                break;
            }
            // an asteroid that is idle and still leaves a real asteroid in the resource list
            if (ship->shipidle && !ship->obj.posinfo.isMoving)
            {
                if (spec->fakeResource == NULL)
                {
                    spec->fakeResource = cat_univAddMimicResource(spec->apparentAsteroid, &ship->impactable.collInfo.collPosition);
                    ((Asteroid *)spec->fakeResource)->catMimicShip = ship;
                }
                break;
            }
            if (spec->fakeResource != NULL)
            {
                cat_univDeleteMimicResource(spec->fakeResource);
                spec->fakeResource = NULL;
            }
            break;
        case MIMIC_FadeIn:
            if (1.0f <= spec->fade)
            {
                soundEventPlay(ship, 0x817, NULL);
                speechEventQueue(ship, 0x40008a5, ship->shiptype, -1, -1, -1, -1, 0.0f, -1);
            }
            spec->fade -= MIMIC_FADE_STEP;
            if (spec->fade <= 0.0f)
            {
                // the camera focused on this ship alone refocuses (its size changed)
                if ((currentFocus(&universe.mainCameraCommand)->numShips == 1) &&
                    (currentFocus(&universe.mainCameraCommand)->ShipPtr[0] == ship))
                {
                    focus.ShipPtr[0] = ship;
                    focus.numShips = 1;
                    ccFocus(&universe.mainCameraCommand, &focus);
                }
                spec->mode = MIMIC_On;
                if ((spec->apparentPlayer < 0) || (spec->apparentType < 0) || (spec->apparentType > STD_LAST_SHIP))
                {
                    mimicSetDetectedAll(spec, FALSE);
                }
                else
                {
                    spec->detected[spec->apparentPlayer] = FALSE;
                }
            }
            break;
        case MIMIC_FadeOut:
            if (spec->fade <= 0.0f)
            {
                soundEventPlay(ship, 0x818, NULL);
                speechEventQueue(ship, 0x40008a6, ship->shiptype, -1, -1, -1, -1, 0.0f, -1);
            }
            spec->fade += MIMIC_FADE_STEP;
            if (spec->fade < 1.0f)
            {
                break;
            }
            spec->mode = MIMIC_Off;
            spec->started = FALSE;
            mimicFreeDisguise(spec);
            break;
        case MIMIC_Switch:
            if (spec->fade <= 0.0f)
            {
                soundEventPlay(ship, 0x818, NULL);
            }
            spec->fade += MIMIC_FADE_STEP;
            if (1.0f <= spec->fade)
            {
                if (spec->apparentBindings != NULL)
                {
                    memFree(spec->apparentBindings);
                    spec->apparentBindings = NULL;
                }
                spec->apparentType = spec->nextType;
                spec->apparentRace = spec->nextRace;
                spec->apparentLOD = spec->nextLOD;
                spec->apparentColor = spec->nextColor;
                spec->apparentBindings = spec->nextBindings;
                spec->apparentRadius = spec->nextRadius;
                spec->apparentPlayer = spec->nextPlayer;
                spec->asteroidRadius = spec->nextAsteroidRadius;
                spec->asteroidCameraRadius = spec->nextAsteroidCameraRadius;
                spec->apparentAsteroid = spec->nextAsteroid;
                spec->asteroidResourceValue = spec->nextAsteroidResourceValue;
                spec->nextBindings = NULL;
                spec->mode = MIMIC_FadeIn;
            }
            break;
        case MIMIC_FadeOutUnlink:
            spec->fade += MIMIC_FADE_STEP;
            if (spec->fade < 1.0f)
            {
                break;
            }
            spec->mode = MIMIC_Off;
            spec->started = FALSE;
            mimicFreeDisguise(spec);
            mimicUnlinkSelf(ship);
            break;
        case MIMIC_Unlink:
            mimicUnlinkSelf(ship);
            spec->mode = MIMIC_Linked;
            break;
        case MIMIC_FadeOutRelink:
            if (spec->fade <= 0.0f)
            {
                soundEventPlay(ship, 0x818, NULL);
                speechEventQueue(ship, 0x40008a6, ship->shiptype, -1, -1, -1, -1, 0.0f, -1);
            }
            spec->fade += MIMIC_FADE_STEP;
            if (spec->fade < 1.0f)
            {
                break;
            }
            spec->started = FALSE;
            if (spec->apparentBindings != NULL)
            {
                memFree(spec->apparentBindings);
                spec->apparentBindings = NULL;
            }
            if (spec->fakeResource != NULL)
            {
                cat_univDeleteMimicResource(spec->fakeResource);
                spec->fakeResource = NULL;
            }
            mimicUnlinkSelf(ship);
            spec->mode = MIMIC_Linked;
            break;
        case MIMIC_DelayFadeIn:
            if (spec->delayEnd < universe.totaltimeelapsed)
            {
                spec->mode = MIMIC_FadeIn;
                spec->started = TRUE;
            }
            break;
        case MIMIC_DelayFadeOut:
            if (spec->delayEnd < universe.totaltimeelapsed)
            {
                spec->mode = MIMIC_FadeOut;
            }
            break;
        case MIMIC_DelaySwitch:
            if (spec->delayEnd < universe.totaltimeelapsed)
            {
                spec->mode = MIMIC_Switch;
            }
            break;
        default:
            break;
    }
    if ((spec->mode != MIMIC_On) && (spec->fakeResource != NULL))
    {
        cat_univDeleteMimicResource(spec->fakeResource);
        spec->fakeResource = NULL;
    }
}

void MimicDied(Ship *ship)                                                                  // 0x005b9460
{
    mimicFreeDisguise((MimicSpec *)ship->ShipSpecifics);
}

real32 cat_mimicGetSelectRadius(Ship *ship)                                                 // 0x005b94e0
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    if (mimicIsMimic(ship))
    {
        switch (spec->mode)
        {
            case MIMIC_On:
            case MIMIC_FadeOut:
            case MIMIC_Switch:
            case MIMIC_FadeOutUnlink:
            case MIMIC_DelayFadeOut:
            case MIMIC_DelaySwitch:
                if (spec->apparentType != -1)
                {
                    return GetShipStaticInfo(spec->apparentType, 0)->common.staticCollInfo.collspheresize;
                }
                return spec->asteroidRadius;
            default:
                break;
        }
    }
    return ship->staticinfo->common.staticCollInfo.collspheresize;
}

real32 cat_mimicGetAvoidRadius(Ship *ship)                                                  // 0x005b9550
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    if (mimicIsMimic(ship))
    {
        switch (spec->mode)
        {
            case MIMIC_On:
            case MIMIC_FadeOut:
            case MIMIC_Switch:
            case MIMIC_FadeOutUnlink:
            case MIMIC_DelayFadeOut:
            case MIMIC_DelaySwitch:
                if (spec->apparentType != -1)
                {
                    return GetShipStaticInfo(spec->apparentType, 0)->common.staticCollInfo.avoidcollspheresize;
                }
                return spec->asteroidRadius;
            default:
                break;
        }
    }
    return ship->staticinfo->common.staticCollInfo.avoidcollspheresize;
}

real32 cat_mimicGetCameraRadius(Ship *ship)                                                 // 0x005b95c0
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    if (mimicIsMimic(ship))
    {
        switch (spec->mode)
        {
            case MIMIC_On:
            case MIMIC_FadeOut:
            case MIMIC_Switch:
            case MIMIC_FadeOutUnlink:
            case MIMIC_DelayFadeOut:
            case MIMIC_DelaySwitch:
                if (spec->apparentType != -1)
                {
                    return GetShipStaticInfo(spec->apparentType, 0)->minimumZoomDistance;
                }
                return spec->asteroidCameraRadius;
            default:
                break;
        }
    }
    return ship->staticinfo->minimumZoomDistance;
}

lodinfo *cat_mimicGetStaticInfo(Ship *ship)                                                 // 0x005b9640
{
    if (mimicIsMimic(ship) && (((MimicSpec *)ship->ShipSpecifics)->mode == MIMIC_On))
    {
        return ((MimicSpec *)ship->ShipSpecifics)->apparentLOD;
    }
    return ship->staticinfo->common.LOD;
}

real32 cat_mimicGetApparentScale(Ship *ship)                                                // 0x005b9670
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    if (mimicIsMimic(ship))
    {
        switch (spec->mode)
        {
            case MIMIC_Off:
            case MIMIC_Unlink:
            case MIMIC_Linked:
            case MIMIC_DelayFadeIn:
                return ship->staticinfo->scaleCap;
            default:
                break;
        }
        if (spec->apparentType != -1)
        {
            return GetShipStaticInfo(spec->apparentType, 0)->scaleCap;
        }
    }
    return 0.0f;
}

bool32 cat_mimicAppearsAlliedTo(Ship *ship, struct Player *player)                          // 0x005b96f0
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    if (mimicIsMimic(ship) && (spec->mode == MIMIC_On) && (spec->apparentType != -1))
    {
        return !cat_shipDetectedByPlayer(ship, player->playerIndex) && ((udword)spec->apparentPlayer == player->playerIndex);
    }
    return FALSE;
}

udword cat_mimicGetApparentPlayerIndex(Ship *ship)                                          // 0x005b9760
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    if (!mimicIsMimic(ship))
    {
        return ship->playerowner->playerIndex;
    }
    if (singlePlayerGame)
    {
        if ((spec->mode == MIMIC_On) && (spec->apparentType != -1))
        {
            return (udword)spec->apparentColor;
        }
        return (ubyte)ship->colorScheme;
    }
    if ((spec->mode == MIMIC_On) && (spec->apparentType != -1))
    {
        return (udword)spec->apparentPlayer;
    }
    return ship->playerowner->playerIndex;
}

ShipType cat_mimicGetApparentShipType(Ship *ship)                                           // 0x005b97e0
{
    if (mimicIsMimic(ship))
    {
        return ((MimicSpec *)ship->ShipSpecifics)->apparentType;
    }
    return ship->shiptype;
}

ShipClass cat_mimicGetApparentShipClass(Ship *ship)                                         // 0x005b9810
{
    if (mimicIsMimic(ship))
    {
        return GetShipStaticInfo(((MimicSpec *)ship->ShipSpecifics)->apparentType, 0)->shipclass;
    }
    return GetShipStaticInfo(ship->shiptype, 0)->shipclass;
}

bool32 cat_mimicAppearsAsResource(Ship *ship, struct Player *player)                        // 0x005b9860
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    return mimicIsMimic(ship) && !cat_shipDetectedByPlayer(ship, player->playerIndex) && (spec->mode == MIMIC_On) &&
           (spec->apparentType == -1) &&
           (DMG_StrikeLightLimit < ship->targetable.health / ship->staticinfo->health.maxhealth);
}

bool32 cat_mimicAppearsAsDerelict(Ship *ship, struct Player *player)                        // 0x005b98d0
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    return mimicIsMimic(ship) && !cat_shipDetectedByPlayer(ship, player->playerIndex) && (spec->mode == MIMIC_On) &&
           (spec->apparentType > p1StandardCorvette) &&
           (DMG_StrikeLightLimit < ship->targetable.health / ship->staticinfo->health.maxhealth);
}

sdword cat_mimicGetApparentObject(Ship *ship)                                               // 0x005b9940
{
    if (mimicIsMimic(ship))
    {
        return ((MimicSpec *)ship->ShipSpecifics)->asteroidResourceValue;
    }
    return 0;
}

struct Resource *cat_mimicGetFakeResource(Ship *ship)                                       // 0x005b9960
{
    if (mimicIsMimic(ship))
    {
        return ((MimicSpec *)ship->ShipSpecifics)->fakeResource;
    }
    return NULL;
}

real32 cat_shipGetRamDamageScale(Ship *ship)                                                // 0x005b9980
{
    if (cat_shipHasUpgrade(ship, CAT_UPGRADE_MimicRam))
    {
        return ((MimicSpec *)ship->ShipSpecifics)->ramDamageScale;
    }
    return 1.0f;
}

bool32 cat_shipIsMimicking(Ship *ship)                                                      // 0x005b99b0
{
    if (bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic))
    {
        return (((MimicSpec *)ship->ShipSpecifics)->mode != MIMIC_Off);
    }
    return FALSE;
}

bool32 MimicHaltCommand(Ship *ship)                                                         // 0x005b99e0
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    if (spec->mode == MIMIC_Linked)
    {
        spec->mode = MIMIC_Off;
        mimicFreeDisguise(spec);
    }
    return TRUE;
}

// the mesh and bindings of a disguise are rebuilt after a load (Mimic_Fix 0x005b9a50)
static void mimicFixDisguise(sdword type, sdword asteroid, lodinfo **lod, shipbindings **bindings)
{
    ShipStaticInfo *statinfo;

    if (type == -1)
    {
        *lod = asteroidStaticInfos[asteroid].common.LOD;
        *bindings = NULL;
    }
    else
    {
        statinfo = GetShipStaticInfo(type, 0);
        *lod = statinfo->common.LOD;
        *bindings = cat_univMeshBindingsDupeNoGuns(statinfo);
    }
}

void Mimic_Fix(Ship *ship)                                                                  // 0x005b9a50
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    spec->link.partner = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->link.partner);
    if (spec->mode != MIMIC_Off)
    {
        switch (spec->mode)
        {
            case MIMIC_On:
            case MIMIC_FadeIn:
            case MIMIC_FadeOut:
            case MIMIC_FadeOutUnlink:
            case MIMIC_FadeOutRelink:
            case MIMIC_DelayFadeIn:
            case MIMIC_DelayFadeOut:
            case MIMIC_DelaySwitch:
                mimicFixDisguise(spec->apparentType, spec->apparentAsteroid, &spec->apparentLOD, &spec->apparentBindings);
                break;
            default:
                break;
        }
        switch (spec->mode)
        {
            case MIMIC_Switch:
            case MIMIC_Unlink:
            case MIMIC_FadeOutRelink:
            case MIMIC_Linked:
            case MIMIC_DelaySwitch:
                mimicFixDisguise(spec->nextType, spec->nextAsteroid, &spec->nextLOD, &spec->nextBindings);
                break;
            default:
                break;
        }
    }
    spec->fakeResource = (struct Resource *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->fakeResource);
}

void Mimic_PreFix(Ship *ship)                                                               // 0x005b9b90
{
    MimicSpec *spec = (MimicSpec *)ship->ShipSpecifics;

    spec->link.partner = (Ship *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->link.partner);
    if (spec->link.effect != NULL)
    {
        spec->link.effect = NULL;
    }
    spec->fakeResource = (struct Resource *)(smemsize)SpaceObjRegistryGetID((SpaceObj *)spec->fakeResource);
}

void MimicRemoveShipReferences(Ship *ship, Ship *shiptoremove)                              // 0x005b9bf0
{
    if (ship == shiptoremove)
    {
        mimicFreeDisguise((MimicSpec *)shiptoremove->ShipSpecifics);
    }
}

// CAT: MimicHeader 0x008c4df0 (sMimic). Slot 29 is MimicSlot74Command 0x005b94c0 (getShipAndItsCommand, return 1):
//      no caller of slot 29 is known (SpaceObj.h), so it stays NULL here
CustShipHeader MimicHeader =
{
    .sizeofShipSpecifics = sizeof(MimicSpec),
    .CustShipStaticInit = MimicStaticInit,
    .CustShipInit = MimicInit,
    .CustShipFire = GenericInterceptorFire,
    .CustShipAttackPassive = MimicAttackPassive,
    .CustShipSpecialActivate = MimicSpecialActivate,
    .CustShipSpecialTarget = MimicSpecialTarget,
    .CustShipHousekeep = MimicHouseKeep,
    .CustShipRemoveShipReferences = MimicRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))MimicRemoveShipReferences,
    .CustShipDied = MimicDied,
    .CustShip_PreFix = Mimic_PreFix,
    .CustShip_Fix = Mimic_Fix,
    .CustShipAttack = MimicAttack,
    .CustShipHaltCommand = MimicHaltCommand,
};
