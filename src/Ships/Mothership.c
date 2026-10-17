// =============================================================================
//  Mothership.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "Carrier.h"
#include "Mothership.h"
#include "LaunchMgr.h"

#include "AITrack.h"
#include "Attack.h"
#include "BeastWeapon.h"
#include "CommandDefs.h"
#include "CommandWrap.h"
#include "DefaultShip.h"
#include "Debug.h"
#include "ETG.h"
#include "Gun.h"
#include "MadLinkIn.h"
#include "MadLinkInDefs.h"
#include "MaxMultiplayer.h"
#include "Memory.h"
#include "MeshAnim.h"
#include "Randy.h"
#include "RKO.h"
#include "Salvage.h"
#include "SaveGame.h"
#include "ShipSelect.h"
#include "SoundEvent.h"
#include "StringsOnly.h"
#include "Universe.h"
#include "Blast.h"
#include "UnivUpdate.h"
#include "utility.h"

// CAT: Mothership.c follows cataclysm.exe's Mothership block (0x005bb730..0x005bd730): the Sect mothership (big gun,
//      module slots, death sequence), the Beast mothership (infection weapon, "beast systems"), trGhostship, all
//      three salvage depots. HW1's door cargo, drop targets and captured-ship build queue are gone.

MothershipStatics MothershipStatic;     // CAT: the exe's one block (0x00a7a9c0)

// CAT: the death sequence (cat_mothershipStartDeathSequence 0x005bd5f0, MothershipHouseKeep 0x005bcad0): explode
//      1..2 s after the killing blow, then the ship is deleted with the explosion type of its death reason
#define CAT_MOTHERSHIP_DEATH_DELAY_LO       1.0f        // 0x008c53b0
#define CAT_MOTHERSHIP_DEATH_DELAY_HI       2.0f        // 0x008c53b4
#define CAT_MOTHERSHIP_DELETE_DELAY_LO      5.0f        // 0x008c53b8
#define CAT_MOTHERSHIP_DELETE_DELAY_HI      8.0f        // 0x008c53bc
#define CAT_RANDOM_TO_UNIT                  2.3283064365386963e-10f    // 0x0072a4e0, 2^-32

// the death explosion type per death reason (0x0087ac64, one byte each; the first 16 entries)
static const ubyte catMothershipDeathExplosion[16] = { 1, 2, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

// the player's mothership health at the last warning check, per player (0x00a77ad8)
static real32 catMothershipLastHealthFraction[MAX_MULTIPLAYER_PLAYERS];

// CAT: the bMothership "beast systems" self-damage (beastMothershipSelfDamage 0x005bc900)
#define CAT_BEASTMOSHIP_MIN_HEALTH_FRACTION 0.5f        // 0x0072a4c8
#define CAT_BEASTMOSHIP_SELF_DAMAGE         20.0f       // 0x0072a4d0 per update
#define CAT_BEASTMOSHIP_GUN_DRAIN           0.1f        // 0x0072a4d4

void MothershipStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    MothershipStatics *mothershipstat = &MothershipStatic;
    udword i;

    // CAT: one block, the two range arrays only, no key table (MothershipStaticInit 0x005bb730); bMothership loads
    //      the Beast-weapon tweaks
    statinfo->custstatinfo = mothershipstat;
    for (i = 0; i < NUM_TACTICS_TYPES; i++)
    {
        mothershipstat->mothershipGunRange[i] = statinfo->bulletRange[i];
        mothershipstat->mothershipTooCloseRange[i] = statinfo->minBulletRange[i] * 0.9f;
    }
    if (cat_staticHasBeastWeapon(statinfo))
    {
        cat_beastWeaponStaticInit();
    }
}

void MothershipInit(Ship *ship)
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    // CAT: MothershipInit 0x005bb840
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponInit(ship);
    }
    if (ship->shiptype == bMothership)
    {
        // CAT-PROVISIONAL: the exe also starts three bMothership timers in the Ship itself (Ship+0x4D0 = 1,
        //      +0x4D4 = now + 1, +0x4D8 = now + 1 + 2.5, +0x4DC = -1); their readers are the Beast mothership's
        //      launch/research code (M2-R4), so they get no field here yet
    }
    vecZeroVector(spec->bigGunTarget);
    vecZeroVector(spec->bigGunFirePosition);
    spec->bigGunBullet = NULL;
    spec->bigGunState = CAT_BIGGUN_Idle;
    spec->bigGunChargeEffectState = 0;
    spec->unknown44 = 0;
    spec->bigGunChargeEffect = NULL;
    spec->bigGunAimTimerRunning = FALSE;
    cat_timerStartGameTime(&spec->bigGunAimTimer, 0.01f);
    if (ship->shiptype == sMothership)
    {
        // the Sect mothership's module slots: one each of these module types
        spec->moduleSlots[0].moduleType = sMothershipDockingBay;    spec->moduleSlots[0].count = 1;
        spec->moduleSlots[1].moduleType = sMothershipWeapons;       spec->moduleSlots[1].count = 1;
        spec->moduleSlots[2].moduleType = sMothershipArmour;        spec->moduleSlots[2].count = 1;
        spec->moduleSlots[3].moduleType = sMothershipMicro;         spec->moduleSlots[3].count = 1;
        spec->moduleSlots[4].moduleType = sMothershipSpecial;       spec->moduleSlots[4].count = 1;
        spec->moduleSlots[5].moduleType = sMothershipSupport;       spec->moduleSlots[5].count = 1;
        spec->moduleSlots[6].moduleType = sMothershipBigGun;        spec->moduleSlots[6].count = 1;
    }
    if (ship->shiptype == bMothership)
    {
        spec->beastSystemState = 0;
        spec->beastSystemActive = FALSE;
        cat_salvageDepotInit(ship);
        return;
    }
    if (ship->shiptype == trGhostship)
    {
        // CAT-PROVISIONAL: the exe starts mesh animation 0 looping (madAnimationStart(ship, 0, 1)); this tree's
        //      madAnimationStart has no loop argument yet (MeshAnim.c, stage 3), so it plays once
        madAnimationStart(ship, 0);
    }
    cat_salvageDepotInit(ship);
}

static void cat_mothershipFlagOrphanedModules(Ship *ship)
{
    // the component-class ships clamped to the host get the "orphaned" flag (catFlags18 bit 2) when it dies
    // (CarrierDied 0x005af3f0, MothershipHouseKeep 0x005bcad0)
    SelectCommand *clamped;
    sdword i;

    if ((ship->guidance.clampInfo == NULL) || (ship->guidance.clampInfo->clampedObjs == NULL))
    {
        return;
    }
    clamped = (SelectCommand *)ship->guidance.clampInfo->clampedObjs;
    for (i = 0; i < clamped->numShips; i++)
    {
        if (clamped->ShipPtr[i]->staticinfo->shipclass == CLASS_Component)
        {
            bitSet(clamped->ShipPtr[i]->header.catFlags18, CAT_SHIPFLAG_Orphaned);
        }
    }
}

void MothershipAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    // CAT: an attack order aborts a big-gun shot that is aiming or charging, then attacks passively as HW1
    //      (MothershipAttack 0x005bbc50)
    if ((ship->shiptype == sMothership) && (spec->bigGunState != CAT_BIGGUN_Stabilizing) &&
        (spec->bigGunState != CAT_BIGGUN_Idle))
    {
        if (spec->bigGunState == CAT_BIGGUN_Charging)
        {
            spec->bigGunChargeEffectState = 2;
            if (spec->bigGunChargeEffect != NULL)
            {
                ((real32 *)spec->bigGunChargeEffect->variable)[0] = 1.0f;
            }
        }
        spec->bigGunState = CAT_BIGGUN_Stabilizing;
        if (aitrackStabilizeShip(ship))
        {
            spec->bigGunState = CAT_BIGGUN_Idle;
        }
        if (ship->playerowner == universe.curPlayerPtr)
        {
            // the exe passes a second argument (-1) to its clCommandMessage
            clCommandMessage(strGetString(strAbortingBigGunFiring));
        }
    }
    attackPassive(ship,(Ship *)target);  // Typecast to prevent warning, attackPassive will handle SpaceObjRotImpTarg
}

void MothershipAttackPassive(Ship *ship,Ship *target,bool32 rotate)
{
    // CAT: slot 6 is the bare attackPassive body, which the exe folded into RKOAttackPassive 0x005c59a0
    attackPassive(ship,target);
}

static bool32 cat_mothershipAbortBigGun(Ship *ship)
{
    // CAT: the shared body of MothershipMoveCommand 0x005bbbd0 and MothershipHaltCommand 0x005bce00: a move or halt
    //      order aborts a big-gun shot that is aiming or charging; the order itself goes ahead (returns TRUE)
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    if ((ship->shiptype == sMothership) && (spec->bigGunState != CAT_BIGGUN_Stabilizing) &&
        (spec->bigGunState != CAT_BIGGUN_Idle))
    {
        if (spec->bigGunState == CAT_BIGGUN_Charging)
        {
            spec->bigGunChargeEffectState = 2;
            if (spec->bigGunChargeEffect != NULL)
            {
                ((real32 *)spec->bigGunChargeEffect->variable)[0] = 1.0f;
            }
        }
        spec->bigGunState = CAT_BIGGUN_Stabilizing;
        if (aitrackStabilizeShip(ship))
        {
            spec->bigGunState = CAT_BIGGUN_Idle;
        }
        if (ship->playerowner == universe.curPlayerPtr)
        {
            clCommandMessage(strGetString(strAbortingBigGunFiring));
        }
    }
    return TRUE;
}

bool32 MothershipMoveCommand(Ship *ship)                                    // CAT: new slot 20 (0x005bbbd0)
{
    return cat_mothershipAbortBigGun(ship);
}

bool32 MothershipHaltCommand(Ship *ship)                                    // CAT: new slot 27 (0x005bce00)
{
    return cat_mothershipAbortBigGun(ship);
}

bool32 MothershipSpecialActivate(Ship *ship)                                // CAT: new slot 7 (0x005bc050)
{
    // a special-activate on a Sect mothership with a working big-gun module cancels the big gun
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    if (cat_mothershipHasBigGunModule(ship) && (spec->bigGunState != CAT_BIGGUN_Idle))
    {
        spec->bigGunState = CAT_BIGGUN_Idle;
    }
    return TRUE;
}

bool32 MothershipSpecialTarget(Ship *ship, void *custom)                    // CAT: new slot 8 (0x005bc200)
{
    // the Beast motherships' infection weapon (also HeavyCruiserHeader slot 8)
    if (cat_shipHasBeastWeapon(ship))
    {
        return cat_beastWeaponAttack(ship, custom);
    }
    return TRUE;
}

void MothershipOrderChanged(Ship *ship)                                     // CAT: new slot 9 (0x005bc230)
{
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponOrderChanged(ship);
    }
}

void MothershipEffectRemoval(Ship *ship, struct Effect *effect)             // CAT: new slot 13 (0x005bce80)
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    if ((ship->shiptype == sMothership) && (spec->bigGunChargeEffect == effect))
    {
        spec->bigGunChargeEffect = NULL;
    }
}

void MothershipRemoveShipReferences(Ship *ship,Ship *shiptoremove)
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    // CAT: MothershipRemoveShipReferences 0x005bba70: losing its own big-gun module stops the big gun (and its charge
    //      effect); a lost module of its player also leaves the launch manager's module slots
    if (ship->shiptype == sMothership)
    {
        if ((shiptoremove->shiptype == sMothershipBigGun) && (shiptoremove->playerowner == ship->playerowner))
        {
            spec->bigGunState = CAT_BIGGUN_Stabilizing;
            if (spec->bigGunChargeEffect != NULL)
            {
                etgEffectDelete(spec->bigGunChargeEffect);
                univRemoveObjFromRenderList((SpaceObj *)spec->bigGunChargeEffect);
                listDeleteNode(&spec->bigGunChargeEffect->header.objlink);
                spec->bigGunChargeEffect = NULL;
            }
        }
        // with the launch manager open, a removed component-class ship of the same player leaves its module slots
        if (lmActive && (shiptoremove->playerowner == ship->playerowner) &&
            (shiptoremove->staticinfo->shipclass == CLASS_Component))
        {
            cat_lmModuleSlotsRemoveType(shiptoremove->shiptype);
        }
    }
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponRemoveShipReferences(ship, shiptoremove);
    }
    cat_salvageDepotRemoveShipReferences(ship, shiptoremove);
}

void Mothership_PreFix(Ship *ship)                                          // 0x005bcdb0 (also Carrier/P1Mothership slot 16)
{
    // CAT: the depot's pre-save fix-up only (HW1 saved door cargo and drop targets)
    cat_salvageDepot_PreFix(ship);
}

void Mothership_Save(Ship *ship)                                            // CAT: new slot 17 (0x005bcd70)
{
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponSave(ship);
    }
}

void Mothership_Load(Ship *ship)                                            // CAT: new slot 18 (0x005bcd90)
{
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponLoad(ship);
    }
}

void Mothership_Fix(Ship *ship)
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    // CAT: Mothership_Fix 0x005bcdc0: the big gun's shell and charge effect are not kept across a load
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponFix(ship);
    }
    if (ship->shiptype == sMothership)
    {
        spec->bigGunBullet = NULL;
        spec->bigGunChargeEffect = NULL;
    }
    cat_salvageDepotFix(ship);
}

// CAT (M2-R4 ships-b): the Sect mothership's big gun (0x005bb7e0 .. 0x005bd4b0)
#define CAT_BIGGUN_AIM_TIME             20.0f       // 0x008c53c0 (cat_bigGunAimTime)
#define CAT_BIGGUN_SCATTER_MP           0.1f        // 0x008c53c4
#define CAT_BIGGUN_RANGE_SPREAD         0.1f        // 0x008c53c8
#define CAT_BIGGUN_RANGE_SPREAD_MIN     20000.0f    // 0x008c53cc
#define CAT_BIGGUN_AIM_DOT              0.985f      // 0x008c53d0 (cat_bigGunAimDotThreshold)
#define CAT_BIGGUN_MAX_RANGE_SP         60000.0f    // 0x008c53d4 (cat_bigGunMaxRangeSP)
#define CAT_BIGGUN_FALLOFF_START_SP     35000.0f    // 0x008c53d8
#define CAT_BIGGUN_MAX_RANGE_MP         90000.0f    // 0x008c53dc (cat_bigGunMaxRangeMP)
#define CAT_BIGGUN_FALLOFF_START_MP     30000.0f    // 0x008c53e0
#define CAT_BIGGUN_MIN_DAMAGE           0.01f       // 0x008c53e4
#define CAT_BIGGUN_CHARGE_TIME          11.0        // 0x0072bfc8 (double): the charge before the shot
#define CAT_BIGGUN_CHARGE_SPIN          3.0f        // 0x0072bfc0
#define CAT_BIGGUN_AIM_ACCURACY         0.999f
#define CAT_BIGGUN_AIM_UP_ACCURACY      0.2f

// the single-player scatter factor (0x00a77af8, .bss: nothing in the exe writes it, so single-player shots go
//      straight)
static real32 cat_bigGunScatterSP = 0.0f;

static bool32 cat_bigGunAimTimerExpired(Ship *ship)                         // 0x005bb7e0
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    return (spec->bigGunAimTimerRunning == TRUE) && cat_timerExpired(&spec->bigGunAimTimer);
}

static void cat_bigGunAimTimerStart(Ship *ship)                             // 0x005bb810
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    if (!spec->bigGunAimTimerRunning)
    {
        spec->bigGunAimTimerRunning = TRUE;
        cat_timerStartGameTime(&spec->bigGunAimTimer, CAT_BIGGUN_AIM_TIME);
    }
}

// the big gun has reloaded: more than its fire time less the charge time since it last fired
static bool32 cat_bigGunReloaded(Gun *gun)
{
    return ((real32)(gun->gunstatic->firetime - CAT_BIGGUN_CHARGE_TIME) < universe.totaltimeelapsed - gun->lasttimefired);
}

real32 cat_bigGunChargeFraction(Ship *ship)                                 // 0x005bbb30
{
    Gun *gun = cat_mothershipGetBigGun(ship);
    real32 fraction = (universe.totaltimeelapsed - gun->lasttimefired) /
                      (real32)(gun->gunstatic->firetime - CAT_BIGGUN_CHARGE_TIME);

    if (1.0f < fraction)
    {
        fraction = 1.0f;
    }
    return fraction;
}

// a random point around aim, up to the scatter factor x the distance off in each axis (0.1 in multiplayer)
static void cat_bigGunScatterTarget(Ship *ship, vector *aim, vector *out)  // 0x005bbd60
{
    real32 spread, low, span;
    real32 x, y;

    *out = *aim;
    spread = (singlePlayerGame ? cat_bigGunScatterSP : CAT_BIGGUN_SCATTER_MP) *
             cat_vecDistance(&ship->obj.posinfo.position, aim);
    if (spread <= 0.0f)
    {
        return;
    }
    low = spread * -1.0f;
    span = spread - low;
    x = (real32)ranRandom(RANDOM_CAT_SHIPS) * span * CAT_RANDOM_TO_UNIT + low;
    y = (real32)ranRandom(RANDOM_CAT_SHIPS) * span * CAT_RANDOM_TO_UNIT + low;
    out->x = x + out->x;
    out->y = y + out->y;
    out->z = (real32)ranRandom(RANDOM_CAT_SHIPS) * span * CAT_RANDOM_TO_UNIT + low + out->z;
}

/*-----------------------------------------------------------------------------
    Name        : cat_bigGunProcessFireCommand
    Description : CAT (M2-R4 ships-b): the point order of a Sect mothership (cat_bigGunProcessFireCommand 0x005bbe80):
                  once the big gun has reloaded it aims at a scattered point near the ordered one
    Inputs      : ship - the mothership (the exe takes the CommandToDo and reads its first ship), point - the
                  ordered point (the exe's CommandToDo +0x2DC, the COMMAND_CAT_POINT block that this tree's
                  CommandToDo does not have yet: the point order is an M2-R4 command follow-up)
----------------------------------------------------------------------------*/
void cat_bigGunProcessFireCommand(Ship *ship, vector *point)               // 0x005bbe80
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    if (cat_bigGunReloaded(cat_mothershipGetBigGun(ship)))
    {
        cat_bigGunScatterTarget(ship, point, &spec->bigGunTarget);
        spec->bigGunState = CAT_BIGGUN_Aiming;
        speechEventQueue(NULL, 0x40008b5, -1, -1, 0, ship->playerowner->playerIndex, -1, 5.0f, -1);
        if (spec->bigGunChargeEffect != NULL)
        {
            etgEffectDelete(spec->bigGunChargeEffect);
            univRemoveObjFromRenderList((SpaceObj *)spec->bigGunChargeEffect);
            listDeleteNode(&spec->bigGunChargeEffect->header.objlink);
            spec->bigGunChargeEffect = NULL;
        }
    }
}

bool32 cat_mothershipBigGunActive(Ship *ship)                               // 0x005bbf60
{
    return cat_mothershipHasBigGunModule(ship) &&
           (((MothershipSpec *)ship->ShipSpecifics)->bigGunState != CAT_BIGGUN_Idle);
}

static bool32 cat_bigGunIsAimed(Ship *ship)                                 // 0x005bc250
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;
    vector dir, heading;

    heading.x = ship->rotatable.rotinfo.coordsys.m13;
    heading.y = ship->rotatable.rotinfo.coordsys.m23;
    heading.z = ship->rotatable.rotinfo.coordsys.m33;
    vecSub(dir, spec->bigGunTarget, ship->obj.posinfo.position);
    vecNormalize(&dir);
    vecNormalize(&heading);
    return (CAT_BIGGUN_AIM_DOT < dir.x * heading.x + dir.y * heading.y + dir.z * heading.z);
}

// keep turning to the target until aimed and the aim time is up; TRUE when done
static bool32 cat_bigGunAimUpdate(Ship *ship)                               // 0x005bc2f0
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;
    vector dir;

    vecSub(dir, spec->bigGunTarget, ship->obj.posinfo.position);
    vecNormalize(&dir);
    ship->autostabilizeship = FALSE;
    if (cat_bigGunIsAimed(ship) && !spec->bigGunAimTimerRunning)
    {
        spec->bigGunAimTimerRunning = TRUE;
        cat_timerStartGameTime(&spec->bigGunAimTimer, CAT_BIGGUN_AIM_TIME);
    }
    if (((spec->bigGunAimTimerRunning != TRUE) || !cat_timerExpired(&spec->bigGunAimTimer)) &&
        !aitrackHeadingFunc((SpaceObjRotImpTargGuidance *)ship, &dir, CAT_BIGGUN_AIM_ACCURACY,
                            CAT_BIGGUN_AIM_UP_ACCURACY, 0.0f, 0))
    {
        return FALSE;
    }
    return TRUE;
}

// the charge effect at the big gun's muzzle, scaled like the ship
static void cat_bigGunChargeEffectStart(Ship *ship)                         // 0x005bd4b0
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;
    etglod *etgLOD = etgSpecialPurposeEffectTable[EGT_BIG_GUN_CHARGING];
    etgeffectstatic *stat;
    Gun *gun;
    matrix coordsys;
    vector pos, velocity;

    if ((etgLOD != NULL) && (etgLOD->nLevels > 0) && ((stat = etgLOD->level[0]) != NULL) && etgFireEffectsEnabled &&
        !etgFrequencyExceeded(stat))
    {
        coordsys = ship->rotatable.rotinfo.coordsys;
        gun = cat_mothershipGetBigGun(ship);
        vecZeroVector(velocity);
        matMultiplyMatByVec(&pos, &ship->rotatable.rotinfo.coordsys, &gun->gunstatic->position);
        vecAddTo(pos, ship->obj.posinfo.position);
        // (an effect still running reaches a debug hook only)
        spec->bigGunChargeEffect = etgEffectCreate(stat, NULL, &pos, &velocity, &coordsys, ship->staticinfo->scaleCap,
                                                   EAF_Full, 0);
    }
}

// after the charge time the shot goes (state Firing, the module's firing animation); the charge effect follows the
//      muzzle and spins faster and faster
static void cat_bigGunFiringUpdate(Ship *ship)                              // 0x005bd1e0
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;
    Effect *effect;
    Ship *module = NULL;
    Node *node;
    Gun *gun;
    vector pos;
    matrix rot, coordsys;
    real32 t;

    if ((spec->bigGunState == CAT_BIGGUN_Charging) &&
        ((real32)CAT_BIGGUN_CHARGE_TIME < universe.totaltimeelapsed - spec->bigGunChargeStartTime))
    {
        speechEventQueue(NULL, 0x20008b6, -1, -1, 0, ship->playerowner->playerIndex, -1, 5.0f, -1);
        if (ship->playerowner != universe.curPlayerPtr)
        {
            // the exe passes a second argument (the colour 0xff0000ff) to its clCommandMessage
            clCommandMessage(strGetString(strWarnBigGunHasFired));
            soundEventPlay(NULL, 0x13d, NULL);
        }
        spec->bigGunState = CAT_BIGGUN_Firing;
        if (ship->playerowner->shiptotals[sMothershipBigGun] != 0)
        {
            for (node = universe.ShipList.head; node != NULL; node = node->next)
            {
                module = (Ship *)listGetStructOfNode(node);
                if ((module->staticinfo->shiptype == sMothershipBigGun) && (module->playerowner == ship->playerowner) &&
                    !bitTest(module->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
                {
                    if (spec->bigGunChargeEffectState == 1)
                    {
                        cat_madAnimationStartNumber(module, 0);  // CAT-TODO(stage 3): the exe passes a third argument, 0
                    }
                    break;
                }
            }
        }
        spec->bigGunChargeEffectState = 0;
        spec->bigGunChargeEffect = NULL;
    }
    if ((effect = spec->bigGunChargeEffect) != NULL)
    {
        gun = cat_mothershipGetBigGun(ship);
        matMultiplyMatByVec(&pos, &ship->rotatable.rotinfo.coordsys, &gun->gunstatic->position);
        effect->obj.posinfo.position.x = pos.x + ship->obj.posinfo.position.x;
        effect->obj.posinfo.position.y = pos.y + ship->obj.posinfo.position.y;
        effect->obj.posinfo.position.z = pos.z + ship->obj.posinfo.position.z;
        effect->rotatable.rotinfo.coordsys = ship->rotatable.rotinfo.coordsys;
        t = (universe.totaltimeelapsed - spec->bigGunChargeStartTime) / CAT_BIGGUN_CHARGE_SPIN;
        t = t * t;
        // the exe passes sin(t) as the cosine and cos(t) as the sine (0x005bd3f3 .. 0x005bd3ff)
        matMakeRotAboutZ(&rot, (real32)sin((double)t), (real32)cos((double)t));
        coordsys = effect->rotatable.rotinfo.coordsys;
        matMultiplyMatByMat(&effect->rotatable.rotinfo.coordsys, &coordsys, &rot);
    }
}

static void SectMothershipHouseKeep(Ship *ship)                             // 0x005bc430
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;
    vector dir;
    Gun *gun;
    Bullet *bullet;
    real32 dist, spread, low;

    if ((spec->bigGunState == CAT_BIGGUN_Idle) || (spec->bigGunState == CAT_BIGGUN_Stabilizing))
    {
        spec->bigGunAimTimerRunning = FALSE;
        cat_timerStartGameTime(&spec->bigGunAimTimer, 0.01f);
    }
    switch (spec->bigGunState)
    {
        case CAT_BIGGUN_Aiming:
            vecSub(dir, spec->bigGunTarget, ship->obj.posinfo.position);
            vecNormalize(&dir);
            ship->autostabilizeship = FALSE;
            if (cat_bigGunIsAimed(ship) && !spec->bigGunAimTimerRunning)
            {
                spec->bigGunAimTimerRunning = TRUE;
                cat_timerStartGameTime(&spec->bigGunAimTimer, CAT_BIGGUN_AIM_TIME);
            }
            if (((spec->bigGunAimTimerRunning != TRUE) || !cat_timerExpired(&spec->bigGunAimTimer)) &&
                !aitrackHeadingFunc((SpaceObjRotImpTargGuidance *)ship, &dir, CAT_BIGGUN_AIM_ACCURACY,
                                    CAT_BIGGUN_AIM_UP_ACCURACY, 0.0f, 0))
            {
                return;
            }
            spec->bigGunFirePosition = ship->obj.posinfo.position;
            break;
        case CAT_BIGGUN_Scattering:
            vecSub(dir, spec->bigGunTarget, ship->obj.posinfo.position);
            vecNormalize(&dir);
            ship->autostabilizeship = FALSE;
            if (cat_bigGunIsAimed(ship) && !spec->bigGunAimTimerRunning)
            {
                cat_bigGunAimTimerStart(ship);
            }
            if (!cat_bigGunAimTimerExpired(ship))
            {
                aitrackHeadingFunc((SpaceObjRotImpTargGuidance *)ship, &dir, CAT_BIGGUN_AIM_ACCURACY,
                                   CAT_BIGGUN_AIM_UP_ACCURACY, 0.0f, 0);
            }
            if (!cat_bigGunReloaded(cat_mothershipGetBigGun(ship)))
            {
                return;
            }
            break;
        case CAT_BIGGUN_Firing:
            gun = cat_mothershipGetBigGun(ship);
            vecSub(dir, spec->bigGunTarget, ship->obj.posinfo.position);
            vecNormalize(&dir);
            ship->autostabilizeship = FALSE;
            if (cat_bigGunIsAimed(ship) && !spec->bigGunAimTimerRunning)
            {
                spec->bigGunAimTimerRunning = TRUE;
                cat_timerStartGameTime(&spec->bigGunAimTimer, CAT_BIGGUN_AIM_TIME);
            }
            if (!cat_bigGunAimTimerExpired(ship))
            {
                aitrackHeadingFunc((SpaceObjRotImpTargGuidance *)ship, &dir, CAT_BIGGUN_AIM_ACCURACY,
                                   CAT_BIGGUN_AIM_UP_ACCURACY, 0.0f, 0);
            }
            // (the exe computes the distance moved since bigGunFirePosition here and drops it)
            bullet = gunShoot(ship, gun, NULL);
            spec->bigGunBullet = bullet;
            if (bullet != NULL)
            {
                // CAT-FIX: the exe tracks the shell before its NULL test (cat_isBigGunBullet would read a NULL bullet)
                cat_bigGunBulletTrack(bullet);
            }
            cat_rkoWarnBigGunFire(&spec->bigGunTarget);
            if (spec->bigGunBullet == NULL)
            {
                return;
            }
            dist = cat_vecDistance(&spec->bigGunTarget, &spec->bigGunBullet->obj.posinfo.position);
            if (!singlePlayerGame && (CAT_BIGGUN_RANGE_SPREAD_MIN < dist))
            {
                // the multiplayer shell flies its distance +-10%
                spread = CAT_BIGGUN_RANGE_SPREAD * dist;
                low = spread * -1.0f;
                dist = (spread - low) * (real32)ranRandom(RANDOM_CAT_SHIPS) * CAT_RANDOM_TO_UNIT + low + dist;
            }
            spec->bigGunBullet->catBigGunTravelled = 0.0f;
            spec->bigGunBullet->catBigGunRange = dist;
            spec->bigGunState = CAT_BIGGUN_Stabilizing;
            return;
        case CAT_BIGGUN_Charging:
            cat_bigGunAimUpdate(ship);
            cat_bigGunFiringUpdate(ship);
            return;
        case CAT_BIGGUN_Stabilizing:
            cat_bigGunFiringUpdate(ship);
            if (aitrackStabilizeShip(ship))
            {
                spec->bigGunState = CAT_BIGGUN_Idle;
            }
            return;
        case CAT_BIGGUN_Idle:
            cat_bigGunFiringUpdate(ship);
            return;
        default:
            spec->bigGunState = CAT_BIGGUN_Stabilizing;
            return;
    }
    // aimed (and reloaded): charge, if the big-gun module is there
    if (cat_mothershipHasBigGunModule(ship))
    {
        cat_bigGunChargeEffectStart(ship);
        spec->bigGunState = CAT_BIGGUN_Charging;
        spec->bigGunChargeEffectState = 1;
        spec->bigGunChargeStartTime = universe.totaltimeelapsed;
        ship->autostabilizeship = FALSE;
        return;
    }
    spec->bigGunState = CAT_BIGGUN_Stabilizing;
}

static void beastMothershipSelfDamage(Ship *ship)                           // 0x005bc900
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;
    real32 minhealth;
    Gun *gun;
    sdword i;

    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponHouseKeep(ship);
    }
    if (spec->beastSystemState == 0)
    {
        spec->beastSystemActive = FALSE;
        return;
    }
    // the beast systems burn 20 health per update, down to half health, where they shut off
    minhealth = ship->staticinfo->health.maxhealth * CAT_BEASTMOSHIP_MIN_HEALTH_FRACTION;
    if (ship->targetable.health <= minhealth)
    {
        spec->beastSystemActive = FALSE;
        // CAT: the exe calls cat_lmBeastSystemsOff 0x0049fc30 here: cat_clWrapSetBeastSystem 0x004535f0 with 0, which
        //      in a single-player game is cat_clSetBeastSystem 0x0044e120 (it only stores the state, as below).
        // CAT-TODO(M2-R4 #4): the multiplayer path (clSendMisc misc command 10) needs cat_clWrapSetBeastSystem in
        //      CommandWrap.c; the launch manager's LM_BeastSystemsSet radio button reset is stage 3
        spec->beastSystemState = 0;
        return;
    }
    spec->beastSystemActive = TRUE;
    ship->targetable.health -= CAT_BEASTMOSHIP_SELF_DAMAGE;
    if (ship->targetable.health < minhealth)
    {
        ship->targetable.health = minhealth;
    }
    if ((spec->beastSystemState == 2) && (ship->gunInfo != NULL))
    {
        // state 2 also speeds up the beast guns' refire (gun static +0x04 == 0x13 in the exe; the field's
        // Cataclysm meaning is not pinned, see GunStatic.gunsoundtype)
        for (i = 0; i < ship->gunInfo->numGuns; i++)
        {
            gun = &ship->gunInfo->guns[i];
            if (((sdword)gun->gunstatic->gunsoundtype == 0x13) &&
                ((universe.totaltimeelapsed - gun->lasttimefired) < gun->gunstatic->firetime))
            {
                gun->lasttimefired -= CAT_BEASTMOSHIP_GUN_DRAIN;
                if (gun->lasttimefired < 0.0f)
                {
                    gun->lasttimefired = 0.0f;
                }
            }
        }
    }
}

void MothershipHouseKeep(Ship *ship)
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;
    DeleteShip *deleteShip;
    real32 healthFraction, *last;
    bool32 warn;

    // CAT: MothershipHouseKeep 0x005bcad0
    if (spec->deathState == 1)
    {
        // dying: explode once the delay has passed
        if (spec->deathExplodeTime < universe.totaltimeelapsed)
        {
            cat_mothershipFlagOrphanedModules(ship);
            spec->deathState = 2;
            spec->deathDeleteTime = (CAT_MOTHERSHIP_DELETE_DELAY_HI - CAT_MOTHERSHIP_DELETE_DELAY_LO) *
                                    CAT_RANDOM_TO_UNIT * (real32)ranRandom(RANDOM_GAME) +
                                    CAT_MOTHERSHIP_DELETE_DELAY_LO + universe.totaltimeelapsed;
        }
        return;
    }
    if (spec->deathState == 2)
    {
        // exploding: rock the neighbours and hand the ship to the delete list with its death explosion
        MakeExplosionRockBumpables(ship, FALSE);
        deleteShip = memAlloc(sizeof(DeleteShip), "DeleteShip", Pyrophoric);
        deleteShip->ship = ship;
        dbgAssertOrIgnore((spec->deathReason >= 0) && (spec->deathReason < 16));
        deleteShip->deathBy = catMothershipDeathExplosion[spec->deathReason & 15];
        listAddNode(&universe.DeleteShipList, &deleteShip->objlink, deleteShip);
        bitSet(ship->header.flags, SOF_Dead);
        return;
    }
    if (spec->deathState == 3)
    {
        return;
    }

    if (ship->shiptype == sMothership)
    {
        SectMothershipHouseKeep(ship);
    }
    else if (ship->shiptype == bMothership)
    {
        beastMothershipSelfDamage(ship);
    }
    cat_salvageDepotHouseKeep(ship);

    // the local player's mothership warns at 25%, 20%, 15%, 10% and 5% health (on 31 of every 32 updates)
    if (((universe.univUpdateCounter & 0x1f) != 0) && (ship->playerowner->playerIndex == universe.curPlayerIndex) &&
        (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20)))
    {
        healthFraction = ship->targetable.health / ship->staticinfo->health.maxhealth;
        last = &catMothershipLastHealthFraction[ship->playerowner->playerIndex];
        warn = (((*last > 0.25f) && (healthFraction < 0.25f)) || ((*last > 0.2f) && (healthFraction < 0.2f)) ||
                ((*last > 0.15f) && (healthFraction < 0.15f)) || ((*last > 0.1f) && (healthFraction < 0.1f)) ||
                ((*last > 0.05f) && (healthFraction < 0.05f)));
        if (warn)
        {
            // CAT-PROVISIONAL: the exe also puts strMothershipHealthLow into the info overlay in red
            //      (cat_ioMessageAdd 0x004ac940, CommandWrap.c, M2-R4 #2); the speech is Cataclysm's status
            //      event 0x02001057 (Fleet Command)
            speechEventQueue(NULL, 0x02001057, -1, -1, 0, ship->playerowner->playerIndex, -1, 5.0f, -1);
        }
        *last = healthFraction;
    }
}

void MothershipDied(Ship *ship)
{
    // CAT: MothershipDied 0x005bb9c0: a blast at the wreck, the weapon and depot close, and the ships clamped to it
    //      that are being built die a second later (Ship+0x2FC, deathtime) [M2-R4 combat: the blast, the clamp list]
    cat_blastCreate(CAT_BLAST_MOTHERSHIP, ship->obj.posinfo.position.x, ship->obj.posinfo.position.y,
                    ship->obj.posinfo.position.z, ship->playerowner->playerIndex);
    if (cat_shipHasBeastWeapon(ship))
    {
        cat_beastWeaponClose(ship);
    }
    cat_salvageDepotDied(ship);
    if ((ship->guidance.clampInfo != NULL) && (ship->guidance.clampInfo->clampedObjs != NULL))
    {
        CatClampedList *clamped = ship->guidance.clampInfo->clampedObjs;
        sdword i;

        for (i = 0; i < clamped->numShips; i++)
        {
            if (bitTest(clamped->ShipPtr[i]->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
            {
                clamped->ShipPtr[i]->deathtime = universe.totaltimeelapsed + 1.0f;
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_effectDeleteNow
    Description : CAT (M2-R4 ships-b): deletes an effect at once (cat_effectDeleteNow 0x005bbbb0): what univUpdate
                  does with an expired effect
----------------------------------------------------------------------------*/
void cat_effectDeleteNow(Effect *effect)
{
    if (effect != NULL)
    {
        etgEffectDelete(effect);
        univRemoveObjFromRenderList((SpaceObj *)effect);
        listDeleteNode(&effect->header.objlink);
    }
}

/*-----------------------------------------------------------------------------
    CAT: Cataclysm mothership queries
-----------------------------------------------------------------------------*/
Gun *cat_mothershipGetBigGun(Ship *ship)                                    // 0x005bb790
{
    // the first gun whose static +0x04 is 0x12 (see beastMothershipSelfDamage on that field)
    GunInfo *gunInfo = ship->gunInfo;
    sdword i;

    if (gunInfo == NULL)
    {
        return NULL;
    }
    for (i = 0; i < gunInfo->numGuns; i++)
    {
        if ((sdword)gunInfo->guns[i].gunstatic->gunsoundtype == 0x12)
        {
            return &gunInfo->guns[i];
        }
    }
    return NULL;
}

bool32 cat_mothershipHasBigGunModule(Ship *ship)                            // 0x005bd6e0
{
    SelectCommand *clamped;
    bool32 has = FALSE;
    sdword i;

    if ((ship->shiptype != sMothership) || (ship->guidance.clampInfo == NULL) ||
        (ship->guidance.clampInfo->clampedObjs == NULL))
    {
        return FALSE;
    }
    clamped = (SelectCommand *)ship->guidance.clampInfo->clampedObjs;
    for (i = 0; i < clamped->numShips; i++)
    {
        if ((clamped->ShipPtr[i]->shiptype == sMothershipBigGun) &&
            (!bitTest(clamped->ShipPtr[i]->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt)))
        {
            has = TRUE;
        }
    }
    return has;
}

bool32 cat_mothershipBigGunFiring(Ship *ship)                               // 0x005bbfd0
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    return (cat_mothershipHasBigGunModule(ship) && (spec->bigGunState != CAT_BIGGUN_Idle) &&
            (spec->bigGunState != CAT_BIGGUN_Stabilizing)) ? TRUE : FALSE;
}

void cat_mothershipStartDeathSequence(Ship *ship, sdword reason)            // 0x005bd5f0
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    if (spec->deathState == 0)
    {
        spec->deathState = 1;
        spec->deathExplodeTime = (CAT_MOTHERSHIP_DEATH_DELAY_HI - CAT_MOTHERSHIP_DEATH_DELAY_LO) *
                                 CAT_RANDOM_TO_UNIT * (real32)ranRandom(RANDOM_GAME) +
                                 CAT_MOTHERSHIP_DEATH_DELAY_LO + universe.totaltimeelapsed;
        spec->deathReason = reason;
        cat_shipSetDisableReason(ship, 1, 10);
        cat_salvageDepotDied(ship);
    }
}

sdword cat_mothershipModuleSlotGet(Ship *mothership, sdword moduleType)     // 0x005bd670
{
    MothershipSpec *spec = (MothershipSpec *)mothership->ShipSpecifics;
    sdword i;

    for (i = 0; i < CAT_MOTHERSHIP_MODULE_SLOTS; i++)
    {
        if (spec->moduleSlots[i].moduleType == moduleType)
        {
            return spec->moduleSlots[i].count;
        }
    }
    return -1;
}

bool32 cat_isBigGunBullet(SpaceObj *obj)                                   // 0x005bcf80
{
    // a bullet whose weapon type is WEAPON_BIGGUN (7): Bullet +0x108, the weapon word (M2-R4 combat)
    return (obj->header.objtype == OBJ_BulletType) && (((Bullet *)obj)->bulletType == WEAPON_BIGGUN);
}

// CAT (M2-R4 ships-b): the Beast mothership's systems (the launch manager's choice, beastSystemState 1-5) give one
//      bonus while active (0x005bca00 .. 0x005bcac0)
real32 cat_beastMothershipSpeedBonus(Ship *ship)                            // 0x005bca00
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    return (spec->beastSystemActive && (spec->beastSystemState == 1)) ? 0.2f : 0.0f;       // 0x0072a510
}

real32 cat_beastMothershipDamageBonus(Ship *ship)                           // 0x005bca30
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    return (spec->beastSystemActive && (spec->beastSystemState == 3)) ? 0.15f : 1.0f;      // 0x0072af78, 0x0072a4c4
}

real32 cat_beastMothershipResearchFactor(Ship *ship)                        // 0x005bca60
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    return (spec->beastSystemActive && (spec->beastSystemState == 4)) ? 2.0f : 1.0f;
}

sdword cat_beastMothershipBuildFactor(Ship *ship)                           // 0x005bca90
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;

    return (spec->beastSystemActive && (spec->beastSystemState == 5)) ? 2 : 1;
}

bool32 cat_beastMothershipSystemActive(Ship *ship)                          // 0x005bcac0
{
    return (ubyte)((MothershipSpec *)ship->ShipSpecifics)->beastSystemActive;
}

real32 cat_bigGunMaxRange(void)                                             // 0x005bceb0
{
    return singlePlayerGame ? CAT_BIGGUN_MAX_RANGE_SP : CAT_BIGGUN_MAX_RANGE_MP;
}

// full damage up to the falloff start, then down linearly to 0 at the maximum range, never below 0.01
real32 cat_bigGunDamageFalloff(real32 distance)                             // 0x005bced0
{
    real32 range = singlePlayerGame ? CAT_BIGGUN_MAX_RANGE_SP : CAT_BIGGUN_MAX_RANGE_MP;
    real32 start = singlePlayerGame ? CAT_BIGGUN_FALLOFF_START_SP : CAT_BIGGUN_FALLOFF_START_MP;
    real32 scale = 1.0f;

    if (start < distance)
    {
        distance = distance - start;
        range = range - start;
        if (range < distance)
        {
            distance = range;
        }
        if (0.0f < range)
        {
            scale = 1.0f - distance / range;
            if (scale < 0.0f)
            {
                scale = 0.0f;
            }
        }
    }
    if (scale < CAT_BIGGUN_MIN_DAMAGE)
    {
        scale = CAT_BIGGUN_MIN_DAMAGE;
    }
    return scale;
}

/*-----------------------------------------------------------------------------
    Name        : cat_bigGunBulletExplodeAt
    Description : CAT (M2-R4 ships-b): a big-gun shell explodes at pos: the RKO's warning ends, the shell leaves the
                  blast list, a big-gun blast with the damage of the distance it flew, the explosion sound
                  (cat_bigGunBulletExplodeAt 0x005bd0c0; its callers pass the shell's position among the arguments of
                  an effect call it shares; cat_bigGunBulletExplode 0x005bcfa0 is the same at the shell's position)
----------------------------------------------------------------------------*/
void cat_bigGunBulletExplodeAt(Bullet *bullet, vector *pos)                // 0x005bd0c0
{
    cat_rkoNotifyBigGunExplosion(pos);
    cat_bigGunBulletUntrack(bullet);
    cat_blastCreateBigGunExplosion(pos->x, pos->y, pos->z, bullet->playerowner->playerIndex,
                                   cat_bigGunDamageFalloff(bullet->catBigGunTravelled));
    soundEventPlay(pos, 0x1010, NULL);
}

void cat_bigGunBulletExplode(Bullet *bullet)                                // 0x005bcfa0
{
    cat_bigGunBulletExplodeAt(bullet, &bullet->obj.posinfo.position);
}

/*-----------------------------------------------------------------------------
    HW1 door-cargo helpers. CAT: cataclysm.exe has none of them (no door cargo, no special-door offsets); their HW1
    callers (Dock.c, SinglePlayer.c, MadLinkIn.c, SalCapCorvette.c) are not ported yet, so they stay as inert entry
    points: nothing is ever put on the door.
-----------------------------------------------------------------------------*/
void MothershipAttachObjectToDoor(Ship *ship,ShipOrDerelict *object)
{
    (void)ship;
    (void)object;
}

void MothershipDettachObjectFromDoor(Ship *ship)
{
    (void)ship;
}

void MothershipDoorUpKeep(Ship *ship)
{
    (void)ship;
}

void mothershipCleanDoorForHSInstant(Ship *ship)
{
    // the door-animation half of HW1's body (the door state is the Ship's, not the specifics')
    if (ship->madDoorStatus != MAD_STATUS_DOOR_CLOSED)
    {
        if (ship->madBindings->bPaused)
        {
            madAnimationPause(ship,!ship->madBindings->bPaused);
        }
        madAnimationStop(ship);

        ship->cuedAnimationIndex = ship->staticinfo->madStatic->DoorOpenIndexes[0];
        ship->cuedAnimationType = MAD_ANIMATION_NOTHING;
        ship->madDoorStatus = MAD_STATUS_DOOR_CLOSED;
        ship->madAnimationFlags=0;
        ship->nextAnim = 0;
    }
}

void mothershipGetCargoPosition(Ship *ship,ShipOrDerelict *cargo, vector *position, matrix *coordsys, vector *heading, vector *up, vector *right)
{
    // HW1's "no position attainable" defaults
    (void)cargo;
    *position = ship->obj.posinfo.position;
    *coordsys = ship->rotatable.rotinfo.coordsys;
    matGetVectFromMatrixCol3(*heading,ship->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol1(*up,ship->rotatable.rotinfo.coordsys);
    vecCrossProduct(*right,*heading,*up);
}

// CAT: the exe's header (MothershipHeader 0x008c53e8: sMothership, bMothership, trGhostship). Slot 14 is
//      CarrierBulletRemoval (Carrier.c), the infection-bullet removal the three Beast-weapon headers share.
CustShipHeader MothershipHeader =
{
    .sizeofShipSpecifics = sizeof(MothershipSpec),
    .CustShipStaticInit = MothershipStaticInit,
    .CustShipInit = MothershipInit,
    .CustShipFire = DefaultShipFire,
    .CustShipAttackPassive = MothershipAttackPassive,
    .CustShipSpecialActivate = MothershipSpecialActivate,                   // CAT: new
    .CustShipSpecialTarget = MothershipSpecialTarget,                       // CAT: new
    .CustShipOrderChanged = MothershipOrderChanged,                         // CAT: new slot 9
    .CustShipHousekeep = MothershipHouseKeep,
    .CustShipRemoveShipReferences = MothershipRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))MothershipRemoveShipReferences,   // CAT: new slot 12
    .CustShipEffectRemoval = MothershipEffectRemoval,                       // CAT: new slot 13
    .CustShipBulletRemoval = CarrierBulletRemoval,                          // CAT: new slot 14
    .CustShipDied = MothershipDied,
    .CustShip_PreFix = Mothership_PreFix,
    .CustShip_Save = Mothership_Save,                                       // CAT: new
    .CustShip_Load = Mothership_Load,                                       // CAT: new
    .CustShip_Fix = Mothership_Fix,
    .CustShipMoveCommand = MothershipMoveCommand,                           // CAT: new slot 20
    .CustShipAttack = MothershipAttack,
    .CustShipHaltCommand = MothershipHaltCommand,                           // CAT: new slot 27
};
