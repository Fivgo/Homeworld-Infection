/*=============================================================================
    Tactics.c: Control Functions for Homeworld Tactics

    Created June 1998 Bryce
=============================================================================*/

#include "Tactics.h"
#include "FormationDefs.h"

#include <stdlib.h>

#include "ACV.h"
#include "Alliance.h"
#include "ClassDefs.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "Dock.h"
#include "FastMath.h"
#include "Formation.h"
#include "Memory.h"
#include "NomadMoon.h"
#include "RaceDefs.h"
#include "RammingFrigate.h"
#include "Select.h"
#include "Sensors.h"
#include "Sentinel.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "StatScript.h"
#include "Types.h"
#include "Universe.h"
#include "UnivUpdate.h"

/*-----------------------------------------------------------------------------
    CAT (M2-R5 command-tactics): cataclysm.exe's Tactics.c is tacticsStartUp 0x005290b0 .. tacticsGetShipsMaxVelocity
    0x00529450 (8 HW1 functions) and Cataclysm's retaliation 0x005294c0 .. cat_shipIsTargetableBy 0x0052a080. HW1's
    retreat tracking, dodging, flanking, order-status bonuses, speed burst, tactics popup and the per-ship
    tacticsUpdate / tacticsGlobalUpdate / tacticsDelegate* / tacticsAttackCommandVoided have no exe body and are gone
    with their callers: univCheckShipState 0x0054a650 retaliates through cat_tacticsShipAttacked, clPassiveAttack
    0x00448ce0 has its own passive-order test. The attack memory (universe.AttackMemory, HW1's AttackAtoms) is kept:
    the exe still walks and frees it, though no exe code adds to it.
-----------------------------------------------------------------------------*/

//global data
regionhandle    tacticsMenuRegion = NULL;      // CAT: nothing sets or clears it (no tactics popup; feCurrentScreenDelete 0x00475380 has no clear)

//special ops tweakables
real32 kamikazeDamage[TOTAL_NUM_SHIPS];
real32 kamikazeSpeedBurst;
real32 burstRange;
real32 burstChargeTime;
real32 burstCoolDownTime;
real32 burstDamageLo[NUM_RACES];
real32 burstDamageHi[NUM_RACES];
real32 burstSpeed;
real32 oneOverburstSpeed;
real32 burstRadius;

real32 ATTACKING_FROM_ABOVE_MIN_DIST = 250.0f;     // 0x008a8078
real32 ATTACKING_FROM_ABOVE_MIN_RATIO = 2.0f;      // 0x008a807c

real32 IONCANNON_TARGETS_FIGHTER_ANGLE = 0.99939f;
real32 IONCANNON_TARGETS_CORVETTE_ANGLE = 0.99756f;

real32 IONCANNON_TARGETS_FIGHTER_TRIGGERHAPPY = 0.99939f;
real32 IONCANNON_TARGETS_CORVETTE_TRIGGERHAPPY = 0.99756f;

real32 IONCANNON_TARGETS_FIGHTER_MAXANGLESPEED = 3.0f;
real32 IONCANNON_TARGETS_CORVETTE_MAXANGLESPEED = 3.0f;

real32 GLOBAL_SHIP_SPEED_MODIFIER = 1.0f;

real32 DELAY_FORMTIGHTNESS_CHANGE = 5.0f;

TacticsInfo tacticsInfo;

// CAT: the exe's TacticsInfotable 0x008a80a0, row for row (HW1's MenuFeedBack, menuPopupTime, RetreatDistance,
//      bonus, speed-burst, burst-modifier, damage-modifier and OUT_OF_FUEL_VELOCITY_SLOWDOWN rows are gone; the
//      class rows have Cataclysm's 12 classes, the dodge and kamikaze rows its ship types, the burst damage rows its
//      four playable races)
scriptEntry TacticsInfotable[] =
{
    { "EnemyNearZone", scriptSetReal32CB, &tacticsInfo.EnemyNearZone},
    { "PassiveRetaliationFireTimeModifierLow", scriptSetReal32CB, &tacticsInfo.PassiveRetaliationFireTimeModifierLow},
    { "PassiveRetaliationFireTimeModifierHi", scriptSetReal32CB, &tacticsInfo.PassiveRetaliationFireTimeModifierHi},
    makeEntry(ATTACKING_FROM_ABOVE_MIN_DIST,scriptSetReal32CB),
    makeEntry(ATTACKING_FROM_ABOVE_MIN_RATIO,scriptSetReal32CB),
    { "RetreatGetAwayDistance[CLASS_Mothership]", scriptSetReal32SqrCB, &tacticsInfo.RetreatGetAwayDistSqr[CLASS_Mothership]},
    { "RetreatGetAwayDistance[CLASS_HeavyCruiser]", scriptSetReal32SqrCB, &tacticsInfo.RetreatGetAwayDistSqr[CLASS_HeavyCruiser]},
    { "RetreatGetAwayDistance[CLASS_Carrier]", scriptSetReal32SqrCB, &tacticsInfo.RetreatGetAwayDistSqr[CLASS_Carrier]},
    { "RetreatGetAwayDistance[CLASS_Destroyer]", scriptSetReal32SqrCB, &tacticsInfo.RetreatGetAwayDistSqr[CLASS_Destroyer]},
    { "RetreatGetAwayDistance[CLASS_Frigate]", scriptSetReal32SqrCB, &tacticsInfo.RetreatGetAwayDistSqr[CLASS_Frigate]},
    { "RetreatGetAwayDistance[CLASS_Corvette]", scriptSetReal32SqrCB, &tacticsInfo.RetreatGetAwayDistSqr[CLASS_Corvette]},
    { "RetreatGetAwayDistance[CLASS_Fighter]", scriptSetReal32SqrCB, &tacticsInfo.RetreatGetAwayDistSqr[CLASS_Fighter]},
    { "RetreatGetAwayDistance[CLASS_Resource]", scriptSetReal32SqrCB, &tacticsInfo.RetreatGetAwayDistSqr[CLASS_Resource]},
    { "RetreatGetAwayDistance[CLASS_NonCombat]", scriptSetReal32SqrCB, &tacticsInfo.RetreatGetAwayDistSqr[CLASS_NonCombat]},
    { "RetreatGetAwayDistance[CLASS_MonsterShip]", scriptSetReal32SqrCB, &tacticsInfo.RetreatGetAwayDistSqr[CLASS_Corvette]},     // CAT: the exe's row points at [CLASS_Corvette] (0x00c523c4)
    { "RetreatGetAwayDistance[CLASS_Component]", scriptSetReal32SqrCB, &tacticsInfo.RetreatGetAwayDistSqr[CLASS_Component]},
    { "RetreatGetAwayDistance[CLASS_Micro]", scriptSetReal32SqrCB, &tacticsInfo.RetreatGetAwayDistSqr[CLASS_Micro]},
    { "freeRetreatTime", scriptSetReal32CB, &tacticsInfo.freeRetreatTime},
    { "AttackMemoryTimeOut", scriptSetReal32CB, &tacticsInfo.AttackMemoryTimeOut},
    { "formationPadding[LooseFormation]", scriptSetReal32CB, &tacticsInfo.formationPadding[LooseFormation]},
    { "formationPadding[TightFormation]", scriptSetReal32CB, &tacticsInfo.formationPadding[TightFormation]},
    { "InterceptorVerticalMultiplier", scriptSetReal32CB, &tacticsInfo.InterceptorVerticalMultiplier},
    { "DodgeLowAddTime", scriptSetReal32CB, &tacticsInfo.DodgeLowAddTime},
    { "DodgeHighAddTime", scriptSetReal32CB, &tacticsInfo.DodgeHighAddTime},
    { "DodgeInfo[DODGE_ONE_TIME][sRecon]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][sRecon]},
    { "DodgeInfo[DODGE_TWO_TIME][sRecon]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][sRecon]},
    { "DodgeInfo[DODGE_THRUST_MULT][sRecon]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][sRecon]},
    { "DodgeInfo[DODGE_WAIT][sRecon]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][sRecon]},
    { "DodgeInfo[DODGE_ONE_TIME][sAcolyte]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][sAcolyte]},
    { "DodgeInfo[DODGE_TWO_TIME][sAcolyte]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][sAcolyte]},
    { "DodgeInfo[DODGE_THRUST_MULT][sAcolyte]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][sAcolyte]},
    { "DodgeInfo[DODGE_WAIT][sAcolyte]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][sAcolyte]},
    { "DodgeInfo[DODGE_ONE_TIME][sMimic]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][sMimic]},
    { "DodgeInfo[DODGE_TWO_TIME][sMimic]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][sMimic]},
    { "DodgeInfo[DODGE_THRUST_MULT][sMimic]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][sMimic]},
    { "DodgeInfo[DODGE_WAIT][sMimic]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][sMimic]},
    { "DodgeInfo[DODGE_ONE_TIME][sLeech]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][sLeech]},
    { "DodgeInfo[DODGE_TWO_TIME][sLeech]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][sLeech]},
    { "DodgeInfo[DODGE_THRUST_MULT][sLeech]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][sLeech]},
    { "DodgeInfo[DODGE_WAIT][sLeech]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][sLeech]},
    { "DodgeInfo[DODGE_ONE_TIME][sACV]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][sACV]},
    { "DodgeInfo[DODGE_TWO_TIME][sACV]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][sACV]},
    { "DodgeInfo[DODGE_THRUST_MULT][sACV]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][sACV]},
    { "DodgeInfo[DODGE_WAIT][sACV]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][sACV]},
    { "DodgeInfo[DODGE_ONE_TIME][sMCV]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][sMCV]},
    { "DodgeInfo[DODGE_TWO_TIME][sMCV]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][sMCV]},
    { "DodgeInfo[DODGE_THRUST_MULT][sMCV]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][sMCV]},
    { "DodgeInfo[DODGE_WAIT][sMCV]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][sMCV]},
    { "DodgeInfo[DODGE_ONE_TIME][bRecon]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][bRecon]},
    { "DodgeInfo[DODGE_TWO_TIME][bRecon]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][bRecon]},
    { "DodgeInfo[DODGE_THRUST_MULT][bRecon]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][bRecon]},
    { "DodgeInfo[DODGE_WAIT][bRecon]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][bRecon]},
    { "DodgeInfo[DODGE_ONE_TIME][bAcolyte]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][bAcolyte]},
    { "DodgeInfo[DODGE_TWO_TIME][bAcolyte]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][bAcolyte]},
    { "DodgeInfo[DODGE_THRUST_MULT][bAcolyte]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][bAcolyte]},
    { "DodgeInfo[DODGE_WAIT][bAcolyte]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][bAcolyte]},
    { "DodgeInfo[DODGE_ONE_TIME][bACV]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][bACV]},
    { "DodgeInfo[DODGE_TWO_TIME][bACV]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][bACV]},
    { "DodgeInfo[DODGE_THRUST_MULT][bACV]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][bACV]},
    { "DodgeInfo[DODGE_WAIT][bACV]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][bACV]},
    { "DodgeInfo[DODGE_ONE_TIME][bAttackBomber]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][bAttackBomber]},
    { "DodgeInfo[DODGE_TWO_TIME][bAttackBomber]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][bAttackBomber]},
    { "DodgeInfo[DODGE_THRUST_MULT][bAttackBomber]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][bAttackBomber]},
    { "DodgeInfo[DODGE_WAIT][bAttackBomber]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][bAttackBomber]},
    { "DodgeInfo[DODGE_ONE_TIME][bCloakedFighter]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][bCloakedFighter]},
    { "DodgeInfo[DODGE_TWO_TIME][bCloakedFighter]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][bCloakedFighter]},
    { "DodgeInfo[DODGE_THRUST_MULT][bCloakedFighter]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][bCloakedFighter]},
    { "DodgeInfo[DODGE_WAIT][bCloakedFighter]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][bCloakedFighter]},
    { "DodgeInfo[DODGE_ONE_TIME][bHeavyCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][bHeavyCorvette]},
    { "DodgeInfo[DODGE_TWO_TIME][bHeavyCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][bHeavyCorvette]},
    { "DodgeInfo[DODGE_THRUST_MULT][bHeavyCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][bHeavyCorvette]},
    { "DodgeInfo[DODGE_WAIT][bHeavyCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][bHeavyCorvette]},
    { "DodgeInfo[DODGE_ONE_TIME][bInterceptor]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][bInterceptor]},
    { "DodgeInfo[DODGE_TWO_TIME][bInterceptor]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][bInterceptor]},
    { "DodgeInfo[DODGE_THRUST_MULT][bInterceptor]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][bInterceptor]},
    { "DodgeInfo[DODGE_WAIT][bInterceptor]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][bInterceptor]},
    { "DodgeInfo[DODGE_ONE_TIME][bMultiGunCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][bMultiGunCorvette]},
    { "DodgeInfo[DODGE_TWO_TIME][bMultiGunCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][bMultiGunCorvette]},
    { "DodgeInfo[DODGE_THRUST_MULT][bMultiGunCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][bMultiGunCorvette]},
    { "DodgeInfo[DODGE_WAIT][bMultiGunCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][bMultiGunCorvette]},
    { "DodgeInfo[DODGE_ONE_TIME][kAttackBomber]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][kAttackBomber]},
    { "DodgeInfo[DODGE_TWO_TIME][kAttackBomber]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][kAttackBomber]},
    { "DodgeInfo[DODGE_THRUST_MULT][kAttackBomber]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][kAttackBomber]},
    { "DodgeInfo[DODGE_WAIT][kAttackBomber]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][kAttackBomber]},
    { "DodgeInfo[DODGE_ONE_TIME][tAttackBomber]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][tAttackBomber]},
    { "DodgeInfo[DODGE_TWO_TIME][tAttackBomber]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][tAttackBomber]},
    { "DodgeInfo[DODGE_THRUST_MULT][tAttackBomber]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][tAttackBomber]},
    { "DodgeInfo[DODGE_WAIT][tAttackBomber]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][tAttackBomber]},
    { "DodgeInfo[DODGE_ONE_TIME][kHeavyCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][kHeavyCorvette]},
    { "DodgeInfo[DODGE_TWO_TIME][kHeavyCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][kHeavyCorvette]},
    { "DodgeInfo[DODGE_THRUST_MULT][kHeavyCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][kHeavyCorvette]},
    { "DodgeInfo[DODGE_WAIT][kHeavyCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][kHeavyCorvette]},
    { "DodgeInfo[DODGE_ONE_TIME][kHeavyDefender]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][kHeavyDefender]},
    { "DodgeInfo[DODGE_TWO_TIME][kHeavyDefender]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][kHeavyDefender]},
    { "DodgeInfo[DODGE_THRUST_MULT][kHeavyDefender]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][kHeavyDefender]},
    { "DodgeInfo[DODGE_WAIT][kHeavyDefender]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][kHeavyDefender]},
    { "DodgeInfo[DODGE_ONE_TIME][tHeavyDefender]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][tHeavyDefender]},
    { "DodgeInfo[DODGE_TWO_TIME][tHeavyDefender]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][tHeavyDefender]},
    { "DodgeInfo[DODGE_THRUST_MULT][tHeavyDefender]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][tHeavyDefender]},
    { "DodgeInfo[DODGE_WAIT][tHeavyDefender]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][tHeavyDefender]},
    { "DodgeInfo[DODGE_ONE_TIME][kHeavyInterceptor]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][kHeavyInterceptor]},
    { "DodgeInfo[DODGE_TWO_TIME][kHeavyInterceptor]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][kHeavyInterceptor]},
    { "DodgeInfo[DODGE_THRUST_MULT][kHeavyInterceptor]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][kHeavyInterceptor]},
    { "DodgeInfo[DODGE_WAIT][kHeavyInterceptor]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][kHeavyInterceptor]},
    { "DodgeInfo[DODGE_ONE_TIME][tHeavyInterceptor]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][tHeavyInterceptor]},
    { "DodgeInfo[DODGE_TWO_TIME][tHeavyInterceptor]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][tHeavyInterceptor]},
    { "DodgeInfo[DODGE_THRUST_MULT][tHeavyInterceptor]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][tHeavyInterceptor]},
    { "DodgeInfo[DODGE_WAIT][tHeavyInterceptor]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][tHeavyInterceptor]},
    { "DodgeInfo[DODGE_ONE_TIME][tMinelayerCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][tMinelayerCorvette]},
    { "DodgeInfo[DODGE_TWO_TIME][tMinelayerCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][tMinelayerCorvette]},
    { "DodgeInfo[DODGE_THRUST_MULT][tMinelayerCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][tMinelayerCorvette]},
    { "DodgeInfo[DODGE_WAIT][tMinelayerCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][tMinelayerCorvette]},
    { "DodgeInfo[DODGE_ONE_TIME][kMultiGunCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][kMultiGunCorvette]},
    { "DodgeInfo[DODGE_TWO_TIME][kMultiGunCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][kMultiGunCorvette]},
    { "DodgeInfo[DODGE_THRUST_MULT][kMultiGunCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][kMultiGunCorvette]},
    { "DodgeInfo[DODGE_WAIT][kMultiGunCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][kMultiGunCorvette]},
    { "DodgeInfo[DODGE_ONE_TIME][tMultiGunCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][tMultiGunCorvette]},
    { "DodgeInfo[DODGE_TWO_TIME][tMultiGunCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][tMultiGunCorvette]},
    { "DodgeInfo[DODGE_THRUST_MULT][tMultiGunCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][tMultiGunCorvette]},
    { "DodgeInfo[DODGE_WAIT][tMultiGunCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][tMultiGunCorvette]},
    { "DodgeInfo[DODGE_ONE_TIME][kSalCapCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][kSalCapCorvette]},
    { "DodgeInfo[DODGE_TWO_TIME][kSalCapCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][kSalCapCorvette]},
    { "DodgeInfo[DODGE_THRUST_MULT][kSalCapCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][kSalCapCorvette]},
    { "DodgeInfo[DODGE_WAIT][kSalCapCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][kSalCapCorvette]},
    { "DodgeInfo[DODGE_ONE_TIME][tSalCapCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][tSalCapCorvette]},
    { "DodgeInfo[DODGE_TWO_TIME][tSalCapCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][tSalCapCorvette]},
    { "DodgeInfo[DODGE_THRUST_MULT][tSalCapCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][tSalCapCorvette]},
    { "DodgeInfo[DODGE_WAIT][tSalCapCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][tSalCapCorvette]},
    { "DodgeInfo[DODGE_ONE_TIME][p1Fighter]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][p1Fighter]},
    { "DodgeInfo[DODGE_TWO_TIME][p1Fighter]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][p1Fighter]},
    { "DodgeInfo[DODGE_THRUST_MULT][p1Fighter]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][p1Fighter]},
    { "DodgeInfo[DODGE_WAIT][p1Fighter]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][p1Fighter]},
    { "DodgeInfo[DODGE_ONE_TIME][p1MissileCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][p1MissileCorvette]},
    { "DodgeInfo[DODGE_TWO_TIME][p1MissileCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][p1MissileCorvette]},
    { "DodgeInfo[DODGE_THRUST_MULT][p1MissileCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][p1MissileCorvette]},
    { "DodgeInfo[DODGE_WAIT][p1MissileCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][p1MissileCorvette]},
    { "DodgeInfo[DODGE_ONE_TIME][p1StandardCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_ONE_TIME][p1StandardCorvette]},
    { "DodgeInfo[DODGE_TWO_TIME][p1StandardCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_TWO_TIME][p1StandardCorvette]},
    { "DodgeInfo[DODGE_THRUST_MULT][p1StandardCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_THRUST_MULT][p1StandardCorvette]},
    { "DodgeInfo[DODGE_WAIT][p1StandardCorvette]", scriptSetReal32CB, &tacticsInfo.DodgeInfo[DODGE_WAIT][p1StandardCorvette]},
    { "tacticsGuardReturnToGuardingDistance", scriptSetReal32CB, &tacticsInfo.tacticsGuardReturnToGuardingDistance},
    { "tacticsGuardConditionCheckRate", scriptSetReal32CB, &tacticsInfo.tacticsGuardConditionCheckRate},
    { "tacticsGuardConditionCheckFrame", scriptSetReal32CB, &tacticsInfo.tacticsGuardConditionCheckFrame},
    { "kamikazeDamage[sRecon]", scriptSetReal32CB, &kamikazeDamage[sRecon]},
    { "kamikazeDamage[sAcolyte]", scriptSetReal32CB, &kamikazeDamage[sAcolyte]},
    { "kamikazeDamage[sMimic]", scriptSetReal32CB, &kamikazeDamage[sMimic]},
    { "kamikazeDamage[sLeech]", scriptSetReal32CB, &kamikazeDamage[sLeech]},
    { "kamikazeDamage[sACV]", scriptSetReal32CB, &kamikazeDamage[sACV]},
    { "kamikazeDamage[sMCV]", scriptSetReal32CB, &kamikazeDamage[sMCV]},
    { "kamikazeDamage[bRecon]", scriptSetReal32CB, &kamikazeDamage[bRecon]},
    { "kamikazeDamage[bAcolyte]", scriptSetReal32CB, &kamikazeDamage[bAcolyte]},
    { "kamikazeDamage[bInterceptor]", scriptSetReal32CB, &kamikazeDamage[bInterceptor]},
    { "kamikazeDamage[bACV]", scriptSetReal32CB, &kamikazeDamage[bACV]},
    { "kamikazeDamage[bAttackBomber]", scriptSetReal32CB, &kamikazeDamage[bAttackBomber]},
    { "kamikazeDamage[bCloakedFighter]", scriptSetReal32CB, &kamikazeDamage[bCloakedFighter]},
    { "kamikazeDamage[bHeavyCorvette]", scriptSetReal32CB, &kamikazeDamage[bHeavyCorvette]},
    { "kamikazeDamage[bMultiGunCorvette]", scriptSetReal32CB, &kamikazeDamage[bMultiGunCorvette]},
    { "kamikazeDamage[bCruiseMissile]", scriptSetReal32CB, &kamikazeDamage[bCruiseMissile]},
    { "kamikazeDamage[kAttackBomber]", scriptSetReal32CB, &kamikazeDamage[kAttackBomber]},
    { "kamikazeDamage[tAttackBomber]", scriptSetReal32CB, &kamikazeDamage[tAttackBomber]},
    { "kamikazeDamage[kHeavyCorvette]", scriptSetReal32CB, &kamikazeDamage[kHeavyCorvette]},
    { "kamikazeDamage[kHeavyDefender]", scriptSetReal32CB, &kamikazeDamage[kHeavyDefender]},
    { "kamikazeDamage[kHeavyInterceptor]", scriptSetReal32CB, &kamikazeDamage[kHeavyInterceptor]},
    { "kamikazeDamage[kMultiGunCorvette]", scriptSetReal32CB, &kamikazeDamage[kMultiGunCorvette]},
    { "kamikazeDamage[kSalCapCorvette]", scriptSetReal32CB, &kamikazeDamage[kSalCapCorvette]},
    { "kamikazeDamage[tHeavyDefender]", scriptSetReal32CB, &kamikazeDamage[tHeavyDefender]},
    { "kamikazeDamage[tHeavyInterceptor]", scriptSetReal32CB, &kamikazeDamage[tHeavyInterceptor]},
    { "kamikazeDamage[tMinelayerCorvette]", scriptSetReal32CB, &kamikazeDamage[tMinelayerCorvette]},
    { "kamikazeDamage[tMultiGunCorvette]", scriptSetReal32CB, &kamikazeDamage[tMultiGunCorvette]},
    { "kamikazeDamage[tResourceCollector]", scriptSetReal32CB, &kamikazeDamage[tResourceCollector]},
    { "kamikazeDamage[tSalCapCorvette]", scriptSetReal32CB, &kamikazeDamage[tSalCapCorvette]},
    { "kamikazeDamage[p1Fighter]", scriptSetReal32CB, &kamikazeDamage[p1Fighter]},
    makeEntry(kamikazeSpeedBurst,scriptSetReal32CB),
    makeEntry(burstRange,scriptSetReal32CB),
    makeEntry(burstChargeTime,scriptSetReal32CB),
    makeEntry(burstCoolDownTime,scriptSetReal32CB),
    { "burstDamageLo[RACE_SECT]", scriptSetReal32CB, &burstDamageLo[RACE_Sect]},
    { "burstDamageHi[RACE_SECT]", scriptSetReal32CB, &burstDamageHi[RACE_Sect]},
    { "burstDamageLo[RACE_BEAST]", scriptSetReal32CB, &burstDamageLo[RACE_Beast]},
    { "burstDamageHi[RACE_BEAST]", scriptSetReal32CB, &burstDamageHi[RACE_Beast]},
    { "burstDamageLo[RACE_KUSHAN]", scriptSetReal32CB, &burstDamageLo[RACE_Kushan]},
    { "burstDamageHi[RACE_KUSHAN]", scriptSetReal32CB, &burstDamageHi[RACE_Kushan]},
    { "burstDamageLo[RACE_TAIDAN]", scriptSetReal32CB, &burstDamageLo[RACE_Taiidan]},
    { "burstDamageHi[RACE_TAIDAN]", scriptSetReal32CB, &burstDamageHi[RACE_Taiidan]},
    makeEntry(burstSpeed,scriptSetReal32CB),
    makeEntry(burstRadius,scriptSetReal32CB),
    { "holdFormationDuringBattle[Evasive]", scriptSetUdwordCB, &tacticsInfo.holdFormationDuringBattle[Evasive]},
    { "holdFormationDuringBattle[Neutral]", scriptSetUdwordCB, &tacticsInfo.holdFormationDuringBattle[Neutral]},
    { "holdFormationDuringBattle[Aggressive]", scriptSetUdwordCB, &tacticsInfo.holdFormationDuringBattle[Aggressive]},
    { "movingAttackTurnsIntoMoveCommandDistance", scriptSetReal32SqrCB, &tacticsInfo.movingAttackTurnsIntoMoveCommandDistanceSqr},
    makeEntry(IONCANNON_TARGETS_FIGHTER_ANGLE,scriptSetCosAngCB),
    makeEntry(IONCANNON_TARGETS_CORVETTE_ANGLE,scriptSetCosAngCB),
    makeEntry(IONCANNON_TARGETS_FIGHTER_TRIGGERHAPPY,scriptSetCosAngCB),
    makeEntry(IONCANNON_TARGETS_CORVETTE_TRIGGERHAPPY,scriptSetCosAngCB),
    makeEntry(IONCANNON_TARGETS_FIGHTER_MAXANGLESPEED,scriptSetAngCB),
    makeEntry(IONCANNON_TARGETS_CORVETTE_MAXANGLESPEED,scriptSetAngCB),
    makeEntry(GLOBAL_SHIP_SPEED_MODIFIER,scriptSetReal32CB),
    makeEntry(DELAY_FORMTIGHTNESS_CHANGE,scriptSetReal32CB),

    END_SCRIPT_ENTRY
};

// CAT: tacticsStartUp 0x005290b0: EnemyNearZone and the dodge table are zeroed and the two formation paddings set to
//      1 before Tactics.script is read (HW1's bonus tables, menu and retreat list are gone)
void tacticsStartUp()
{
    sdword x,y;

    tacticsInfo.EnemyNearZone = 0;
    for(x=0;x< NUM_DODGE_INFO;x++)
        for(y=0;y< TOTAL_NUM_SHIPS;y++)
           tacticsInfo.DodgeInfo[x][y] = 0.0f;

    tacticsInfo.formationPadding[TightFormation] = 1.0f;
    tacticsInfo.formationPadding[LooseFormation] = 1.0f;

    scriptSet(NULL,"Tactics.script",TacticsInfotable);

    //precalculate certain variables
    tacticsInfo.tacticsGuardReturnToGuardingDistanceSqr = tacticsInfo.tacticsGuardReturnToGuardingDistance*tacticsInfo.tacticsGuardReturnToGuardingDistance;
    oneOverburstSpeed = 1.0f/burstSpeed;
    listInit(&universe.AttackMemory);
}

void tacticsShutDown()
{
    // CAT (tacticsShutDown 0x00529120): the attack memory is deleted, nothing else (no menu baby, no menu region)
    listDeleteAll(&universe.AttackMemory);
}

// CAT-DORMANT: the exe has no tacticsPopUpSetUp (HW1's "Group_Tactics_submenu" flash). Its callers, the tactics keys
//      of mainrgn.c, are stage 3's (the exe's cat_mrSetTactics 0x00565c30 / cat_mrTacticsCycle 0x00566860); until
//      then the keys set the tactics without a popup, as no exe path shows that menu.
void tacticsPopUpSetUp(TacticsType tacticstype)
{
    (void)tacticstype;
}

//Name: tacticsReportMove
//
//Description:  Function scans todo list and deals with any commands with attackinfo
//              dealing with the ships in selection.

void tacticsReportMove(CommandLayer *comlayer,SelectCommand *selection)
{
    Node *curnode;

    // CAT (tacticsReportMove 0x00529130): the walk of the command list is all that is left of HW1's retreat
    //      registration (17 bytes: the loop body was compiled away)
    (void)selection;
    for (curnode = comlayer->todolist.head; curnode != NULL; curnode = curnode->next)
    {
    }
}

//Name: tacticsShipDied
//
//Desc: A ship has died, so we need to remove certain references
//      from our tactics data structures
void tacticsShipDied(Ship *ship)
{
    sdword i;
    Node *link,*templink;
    AttackAtom *a;

    // CAT (tacticsShipDied 0x00529150): the attack memory only; HW1's retreat list is gone
    link = universe.AttackMemory.head;
    while(link != NULL)
    {
        a = (AttackAtom *) listGetStructOfNode(link);
        if(a->retreater == ship)
        {
            goto deleteNode;
        }
        for(i=0;i<a->attackerList.numShips;i++)
        {
            if(ship == a->attackerList.ShipPtr[i])
            {
                a->attackerList.numShips--;
                if(a->attackerList.numShips == 0)
                    goto deleteNode;
                a->attackerList.ShipPtr[i] = a->attackerList.ShipPtr[a->attackerList.numShips];
                return;
            }
        }

        link = link->next;
        continue;

deleteNode:
        templink = link->next;

        listDeleteNode(link);

        link = templink;
        return;

    }
}
//This function will make all ships in selection stop being on the
//lookout for otherships <if selection is given new order>
void tacticsMakeShipsNotLookForOtherShips(SelectCommand *selection)
{
    AttackAtom  *attackatom;
    Node *node,*tempnode;
    sdword i,j;

    node = universe.AttackMemory.head;

    while(node != NULL)
    {
        attackatom = (AttackAtom *)listGetStructOfNode(node);

        for(j=0;j<selection->numShips;j++)
        {
            Ship *ship = selection->ShipPtr[j];
            for(i=0;i<attackatom->attackerList.numShips;i++)
            {
                if(ship == attackatom->attackerList.ShipPtr[i])
                {
                    //ship is looking for something
                    //remove it from list
                    attackatom->attackerList.numShips--;
                    attackatom->attackerList.ShipPtr[i] = attackatom->attackerList.ShipPtr[attackatom->attackerList.numShips];
                    if(attackatom->attackerList.numShips == 0)
                    {
                        tempnode = node->next;
                        listDeleteNode(node);   //delete node+memory
                        node = tempnode;
                        goto nextnode;
                    }
                    break;
                }

            }
        }
        node = node->next;
nextnode:
    continue;
    }
}

//name : tacticsMaxDistToTarget
//
//desc: returns the distance from the furthest ship, to the target
//      Used to determine flying distance for defenders

real32 tacticsMaxDistToTarget(SelectCommand *selection,SpaceObjRotImpTarg *target)
{
    sdword i;
    real32 maxdistsqr,newdistsqr;
    vector tempvec;
    //fix later : could do optimally by analysing formation type, and determining the
    //furthest ship and looking at just it..but for now, we brute force it

    //could use camera distacne squared maybe?

    vecSub(tempvec,selection->ShipPtr[0]->obj.posinfo.position,target->obj.posinfo.position);
    maxdistsqr = vecMagnitudeSquared(tempvec);


    for(i=1;i<selection->numShips;i++)
    {
        vecSub(tempvec,selection->ShipPtr[i]->obj.posinfo.position,target->obj.posinfo.position);
        newdistsqr = vecMagnitudeSquared(tempvec);
        if(newdistsqr > maxdistsqr)
        {
            maxdistsqr = newdistsqr;
        }
    }
    return(fsqrt(maxdistsqr));
}

/*-----------------------------------------------------------------------------
    Name        : tacticsCheckGuardConditionsDuringAttack
    Description : returns TRUE if command needs to cancel its attack and return to guarding ship
    Inputs      : command in question
    Outputs     :
    Return      : as above
----------------------------------------------------------------------------*/
bool32 tacticsCheckGuardConditionsDuringAttack(CommandToDo *command)
{
    sdword i;
    real32 distSqr,tempreal;
    vector distVec,protectAv,defendAv;

    dbgAssertOrIgnore(command->ordertype.attributes * COMMAND_MASK_PROTECTING);

    //get average position of defenders
    vecSet(defendAv,0.0f,0.0f,0.0f);
    for(i=0;i<command->selection->numShips;i++)
    {
        vecAddTo(defendAv,command->selection->ShipPtr[i]->obj.posinfo.position);
    }
    tempreal = 1.0f/((real32)i);
    vecScalarMultiply(defendAv,defendAv,tempreal);

    //get average position of ships needing protection...
    vecSet(protectAv,0.0f,0.0f,0.0f);
    for(i=0;i<command->protect->numShips;i++)
    {
        vecAddTo(protectAv,command->protect->ShipPtr[i]->obj.posinfo.position);
    }
    tempreal = 1.0f/((real32)i);
    vecScalarMultiply(protectAv,protectAv,tempreal);


    vecSub(distVec, protectAv, defendAv);
    distSqr = vecMagnitudeSquared(distVec);

    if(distSqr > tacticsInfo.tacticsGuardReturnToGuardingDistanceSqr)
    {
        //guarding ship has
#ifdef DEBUG_TACTICS
        dbgMessagef("Ships guarding a ship cancelling attack and returning to guard duty.");
#endif
        return TRUE;
    }
    return FALSE;
}

real32 tacticsGetShipsMaxVelocity(Ship *ship)
{
    real32 speedmult = 1.0f;
    CommandToDo *command;

    if(ship->header.objtype != OBJ_ShipType)
    {
        return(ship->staticinfo->common.maxvelocity);
    }

    // CAT (tacticsGetShipsMaxVelocity 0x00529450): a sphere formation moves at 0.7 (0x0072bd10); the kamikaze burst
    //      needs both kamikaze flags; the single-player limiter wins; no speed burst, no fuel, no tactics velocity
    //      bonus table and no probe dispatch speed
    command = ship->command;
    if ((command != NULL) && bitTest(command->ordertype.attributes, COMMAND_MASK_FORMATION) &&
        (command->formation.formationtype == SPHERE_FORMATION))
    {
        speedmult = 0.699999988079071f;
    }
    if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_Kamikaze) &&
       bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_KamikazeCrazyFast))
    {
        speedmult = kamikazeSpeedBurst;
    }
    if (bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerLimitSpeed))
    {
        return ship->singlePlayerSpeedLimiter;      // deliberately override other considerations - we want this to be the final word
    }
    return(speedmult * ship->staticinfo->common.maxvelocity);
}


// CAT: new (cat_tacticsNotRetreatingToDockCB 0x00529f00): FALSE for an evasive ship that is docking because it is
//      below a third of its health (the docking cat_tacticsEvasiveRetreatToDock starts), TRUE for any other ship
bool32 cat_tacticsNotRetreatingToDockCB(Ship *ship)
{
    if ((ship->command != NULL) && (ship->command->ordertype.order == COMMAND_DOCK) && (ship->tacticstype == Evasive) &&
        cat_shipCanDock(ship, TRUE) && (ship->staticinfo->health.oneOverMaxHealth * ship->targetable.health < 0.333f))
    {
        return FALSE;
    }
    return TRUE;
}

// CAT: new (cat_tacticsRemoveRetreatingShips 0x00529f70): the move and attack orders leave such ships alone
void cat_tacticsRemoveRetreatingShips(SelectCommand *selection)
{
    MakeShipsOnlyFollowConstraints(selection, cat_tacticsNotRetreatingToDockCB);
}

/*-----------------------------------------------------------------------------
    Name        : cat_tacticsEvasiveRetreatToDock
    Description : CAT: an evasive ship that may dock and has fallen below a third of its health goes home to dock,
                  unless it is already docking (cat_tacticsEvasiveRetreatToDock 0x00529f90; ApplyDamageToTarget calls
                  it on every hit)
    Inputs      : ship (any targetable object; only ships act)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void cat_tacticsEvasiveRetreatToDock(Ship *ship)
{
    // 0x008a89f4 (0.333): the health fraction below which an evasive ship retreats (a constant of the exe's data)
    const real32 retreatHealthFraction = 0.333f;
    SelectCommand selectone;

    if ((ship->header.objtype != OBJ_ShipType) || !cat_shipCanSetTactics(ship, TRUE) || (ship->tacticstype != Evasive) ||
        !cat_shipCanDock(ship, TRUE) ||
        !(ship->staticinfo->health.oneOverMaxHealth * ship->targetable.health < retreatHealthFraction))
    {
        return;
    }
    if ((ship->command != NULL) && (ship->command->ordertype.order == COMMAND_DOCK))
    {
        return;                                             // already docking
    }
    selectone.numShips = 1;
    selectone.ShipPtr[0] = ship;
    clDock(&universe.mainCommandLayer, &selectone, CAT_DOCK_SUPPORT, NULL);     // the exe's purpose 0 (M2-R5 dock)
}

/*-----------------------------------------------------------------------------
    Cataclysm's retaliation (0x005294c0 .. 0x0052a1d0)
-----------------------------------------------------------------------------*/

// CAT: inlined twice in cat_tacticsFindResponsibleShip 0x005294c0 and once in cat_tacticsShipAttacked 0x00529c20:
//      a ship answers for itself unless it is a follower of a formation or a component clamped to a host
static bool32 cat_tacticsShipIsOwnLeader(Ship *ship)
{
    CommandToDo *command = ship->command;
    bool32 own = FALSE;

    if ((command == NULL) || !(command->ordertype.attributes & COMMAND_MASK_FORMATION) || (command->selection == NULL) ||
        (command->selection->ShipPtr[0] == ship))
    {
        own = TRUE;
    }
    if ((ship->staticinfo->shipclass == CLASS_Component) && (ship->guidance.clampInfo != NULL) &&
        (ship->guidance.clampInfo->host != NULL))
    {
        own = FALSE;
    }
    return own;
}

/*-----------------------------------------------------------------------------
    Name        : cat_tacticsFindResponsibleShip
    Description : the ship that retaliates for ship: the Nomad moon for its field emitters, a component's host, a
                  formation's leader
----------------------------------------------------------------------------*/
// CAT: new (cat_tacticsFindResponsibleShip 0x005294c0)
Ship *cat_tacticsFindResponsibleShip(Ship *ship)
{
    Ship *responsible;
    Ship *host;

    if ((ship->shiptype != dNomadFieldEmitter) && cat_tacticsShipIsOwnLeader(ship))
    {
        return ship;
    }
    if ((ship->shiptype == dNomadFieldEmitter) && ((responsible = cat_nomadMoonFind()) != NULL))
    {
        return responsible;
    }
    responsible = ship;
    if ((ship->staticinfo->shipclass == CLASS_Component) && (ship->guidance.clampInfo != NULL) &&
        ((host = (Ship *)ship->guidance.clampInfo->host) != NULL))
    {
        responsible = host;
        if ((host->shiptype != dNomadFieldEmitter) && cat_tacticsShipIsOwnLeader(host))
        {
            return host;
        }
    }
    if (responsible->command != NULL)
    {
        return responsible->command->selection->ShipPtr[0];
    }
    return responsible;
}

/*-----------------------------------------------------------------------------
    Name        : cat_tacticsMergeAttackTargets
    Description : dest = the existing targets followed by the attackers not among them; empty when every attacker is
                  already a target, the attackers alone when there are no existing targets
----------------------------------------------------------------------------*/
// CAT: new (cat_tacticsMergeAttackTargets 0x005295c0); dest holds COMMAND_MAX_SHIPS targets (the exe's caller has a
//      1000-entry buffer)
void cat_tacticsMergeAttackTargets(AttackCommand *dest,AttackCommand *existing,AttackCommand *attackers)
{
    TargetPtr added[COMMAND_MAX_SHIPS];
    sdword numAdded = 0;
    sdword i,j;
    bool32 found;

    dest->numTargets = 0;
    if ((attackers == NULL) || (attackers->numTargets <= 0))
    {
        return;
    }
    if ((existing == NULL) || (existing->numTargets < 1))
    {
        for (i = 0; i < attackers->numTargets; i++)
        {
            dest->TargetPtr[i] = attackers->TargetPtr[i];
        }
        dest->numTargets = attackers->numTargets;
        return;
    }
    for (i = 0; i < attackers->numTargets; i++)
    {
        found = FALSE;
        for (j = 0; j < existing->numTargets; j++)
        {
            if (existing->TargetPtr[j] == attackers->TargetPtr[i])
            {
                found = TRUE;
            }
        }
        if (!found)
        {
            added[numAdded++] = attackers->TargetPtr[i];
        }
    }
    if (numAdded > 0)
    {
        for (i = 0; i < existing->numTargets; i++)
        {
            dest->TargetPtr[i] = existing->TargetPtr[i];
        }
        for (j = 0; j < numAdded; j++)
        {
            dest->TargetPtr[existing->numTargets + j] = added[j];
        }
        dest->numTargets = existing->numTargets + numAdded;
    }
}

// CAT: the attackers that ship's owner can target, in place (swap with the last; inlined in cat_tacticsRetaliate,
//      cat_tacticsRetaliatePassive and cat_tacticsRetaliateEvasive)
static void cat_tacticsKeepTargetable(Ship *ship,AttackCommand *attackers)
{
    Player *player = ship->playerowner;
    sdword i = 0;

    while (i < attackers->numTargets)
    {
        if (!cat_shipIsTargetableBy(player,(Ship *)attackers->TargetPtr[i]))
        {
            attackers->numTargets--;
            attackers->TargetPtr[i] = attackers->TargetPtr[attackers->numTargets];
        }
        else
        {
            i++;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_tacticsRetaliatePassive
    Description : the command (or the lone ship) passive attacks the attackers it can target, merged into its
                  existing targets, when they are in passive attack range
----------------------------------------------------------------------------*/
// CAT: new (cat_tacticsRetaliatePassive 0x005296f0); the exe pushes a fourth value (7 with a command, 8 for a lone
//      ship) clPassiveAttack never reads
void cat_tacticsRetaliatePassive(Ship *ship,CommandToDo *command,AttackCommand *attackers)
{
    MaxAnySelection merged;
    MaxSelection selection;
    SelectCommand selectone;
    AttackCommand *existing = NULL;

    cat_tacticsKeepTargetable(ship,attackers);
    if ((attackers->numTargets <= 0) || !isShipPassiveAttackCapable(ship))
    {
        return;
    }
    if ((command != NULL) && (command->attack != NULL))
    {
        existing = command->attack;
    }
    cat_tacticsMergeAttackTargets((AttackCommand *)&merged,existing,attackers);
    if (merged.numTargets <= 0)
    {
        return;
    }
    if (command != NULL)
    {
        cat_selSelectionCopy(&selection,(MaxSelection *)command->selection);
        if (ShipsInPassiveAttackRange((SelectCommand *)&selection,(AttackCommand *)&merged))
        {
            clPassiveAttack(&universe.mainCommandLayer,(SelectCommand *)&selection,(AttackCommand *)&merged);
        }
    }
    else
    {
        selectone.numShips = 1;
        selectone.timeLastStatus = 0.0f;     // CAT-FIX: the exe leaves it uninitialised (0x005297f0)
        selectone.ShipPtr[0] = ship;
        if (ShipsInPassiveAttackRange(&selectone,(AttackCommand *)&merged))
        {
            clPassiveAttack(&universe.mainCommandLayer,&selectone,(AttackCommand *)&merged);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_tacticsAveragePosition
    Description : the average position of the selection (the first ship's for fewer than two)
----------------------------------------------------------------------------*/
// CAT: new (cat_tacticsAveragePosition 0x00529830)
void cat_tacticsAveragePosition(SelectCommand *selection,vector *vec)
{
    sdword i;
    real32 oneOverNum;

    if (selection->numShips < 2)
    {
        *vec = selection->ShipPtr[0]->obj.posinfo.position;
        return;
    }
    vec->x = 0.0f;
    vec->y = 0.0f;
    vec->z = 0.0f;
    for (i = 0; i < selection->numShips; i++)
    {
        vec->x = selection->ShipPtr[i]->obj.posinfo.position.x + vec->x;
        vec->y = selection->ShipPtr[i]->obj.posinfo.position.y + vec->y;
        vec->z = selection->ShipPtr[i]->obj.posinfo.position.z + vec->z;
    }
    oneOverNum = 1.0f / (real32)selection->numShips;
    vec->x = oneOverNum * vec->x;
    vec->y = oneOverNum * vec->y;
    vec->z = oneOverNum * vec->z;
}

// CAT: the unnamed constraint 0x0052a1e0: no sMothership or bMothership
static bool32 cat_tacticsNotMothershipCB(Ship *ship)
{
    return !((ship->shiptype == sMothership) || (ship->shiptype == bMothership));
}

// CAT: the unnamed constraint 0x0052a210: no ship that is ramming, a Sentinel in a sphere, a fully linked ACV, a
//      mothership firing its big gun, being built, an externally built type launching, or within hyperspace range
static bool32 cat_tacticsCanRetaliateCB(Ship *ship)
{
    bool32 busy = FALSE;

    if (cat_rammingFrigateStateActive(ship))
    {
        busy = TRUE;
    }
    if (cat_shipIsSentinelInSphere(ship))
    {
        busy = TRUE;
    }
    if (cat_acvIsFullyLinked(ship))
    {
        busy = TRUE;
    }
    if (cat_mothershipBigGunActive(ship))
    {
        busy = TRUE;
    }
    if (ship->shipOrDerelict.specialFlags2 & SPECIAL_2_CatBeingBuilt)
    {
        busy = TRUE;
    }
    else if (ship->staticinfo->externalBuild && (ship->command != NULL) &&
             (ship->command->ordertype.order == COMMAND_LAUNCH_SHIP))
    {
        busy = TRUE;
    }
    if (ship->shipOrDerelict.specialFlags & SPECIAL_SinglePlayerWithinHyperSpaceRange)
    {
        busy = TRUE;
    }
    return !busy;
}

/*-----------------------------------------------------------------------------
    Name        : cat_tacticsRetaliateEvasive
    Description : an evasive ship's group (not while docking) moves away from the attackers' average position by the
                  ship's fog-of-war radius (inside the universe), then retaliates passively
----------------------------------------------------------------------------*/
// CAT: new (cat_tacticsRetaliateEvasive 0x005298e0)
void cat_tacticsRetaliateEvasive(Ship *ship,CommandToDo *command,AttackCommand *attackers)
{
    MaxSelection movers;
    bool32 flee = TRUE;
    vector avg;
    vector dir;
    vector dest;
    real32 radius;

    movers.numShips = 0;
    if ((command == NULL) || (command->ordertype.order != COMMAND_DOCK))
    {
        cat_tacticsKeepTargetable(ship,attackers);
    }
    else
    {
        flee = FALSE;
    }
    if (attackers->numTargets < 1)
    {
        flee = FALSE;
    }
    else if (flee)
    {
        if (command == NULL)
        {
            cat_selSelectionAddShip(&movers,ship);
        }
        else
        {
            selSelectionCopy((MaxAnySelection *)&movers,(MaxAnySelection *)command->selection);
        }
        MakeShipsOnlyFollowConstraints((SelectCommand *)&movers,cat_tacticsNotMothershipCB);
        MakeShipsOnlyFollowConstraints((SelectCommand *)&movers,cat_tacticsCanRetaliateCB);
        cat_selMakeShipsSameOwnerAsFirst((SelectCommand *)&movers);
    }
    if ((movers.numShips > 0) && flee)
    {
        cat_tacticsAveragePosition((SelectCommand *)attackers,&avg);
        radius = cat_shipGetFOWRadius(ship,0);
        dir.x = ship->obj.posinfo.position.x - avg.x;
        dir.y = ship->obj.posinfo.position.y - avg.y;
        dir.z = ship->obj.posinfo.position.z - avg.z;
        vecNormalize(&dir);
        dest.x = dir.x * radius + ship->obj.posinfo.position.x;
        dest.y = dir.y * radius + ship->obj.posinfo.position.y;
        dest.z = dir.z * radius + ship->obj.posinfo.position.z;
        if (dest.x > smUniverseSizeX) dest.x = smUniverseSizeX;
        if (dest.x < -smUniverseSizeX) dest.x = -smUniverseSizeX;
        if (dest.y > smUniverseSizeY) dest.y = smUniverseSizeY;
        if (dest.y < -smUniverseSizeY) dest.y = -smUniverseSizeY;
        if (dest.z > smUniverseSizeZ) dest.z = smUniverseSizeZ;
        if (dest.z < -smUniverseSizeZ) dest.z = -smUniverseSizeZ;
        clMove(&universe.mainCommandLayer,(SelectCommand *)&movers,CAT_MOVEPATH_ONCE,ship->obj.posinfo.position,1,&dest);
    }
    cat_tacticsRetaliatePassive(ship,command,attackers);
}

/*-----------------------------------------------------------------------------
    Name        : cat_tacticsSelectionHasFlag80Ship
    Description : TRUE if ship, a ship of its formation or of command is within hyperspace range
                  (SPECIAL_SinglePlayerWithinHyperSpaceRange)
----------------------------------------------------------------------------*/
// CAT: new (cat_tacticsSelectionHasFlag80Ship 0x00529b70): selections of one ship are not searched
bool32 cat_tacticsSelectionHasFlag80Ship(Ship *ship,CommandToDo *command)
{
    CommandToDo *own;
    SelectCommand *selection;
    bool32 found = FALSE;
    sdword i;

    if (ship->shipOrDerelict.specialFlags & SPECIAL_SinglePlayerWithinHyperSpaceRange)
    {
        return TRUE;
    }
    own = ship->command;
    if ((own != NULL) && (own->ordertype.attributes & COMMAND_MASK_FORMATION) && ((selection = own->selection) != NULL) &&
        (selection->numShips > 1))
    {
        for (i = 0; i < selection->numShips; i++)
        {
            if (selection->ShipPtr[i]->shipOrDerelict.specialFlags & SPECIAL_SinglePlayerWithinHyperSpaceRange)
            {
                found = TRUE;
            }
        }
        if (found)
        {
            return TRUE;
        }
    }
    if ((command != NULL) && ((selection = command->selection) != NULL) && (selection->numShips > 1))
    {
        for (i = 0; i < selection->numShips; i++)
        {
            if (selection->ShipPtr[i]->shipOrDerelict.specialFlags & SPECIAL_SinglePlayerWithinHyperSpaceRange)
            {
                found = TRUE;
            }
        }
    }
    return found;
}

/*-----------------------------------------------------------------------------
    Name        : cat_tacticsShipAttacked
    Description : ship (with its command) was attacked by attackers: the responsible ship retaliates
----------------------------------------------------------------------------*/
// CAT: new (cat_tacticsShipAttacked 0x00529c20)
void cat_tacticsShipAttacked(Ship *ship,CommandToDo *command,AttackCommand *attackers)
{
    if ((ship->shiptype == dNomadFieldEmitter) || !cat_tacticsShipIsOwnLeader(ship))
    {
        ship = cat_tacticsFindResponsibleShip(ship);
        command = getShipAndItsCommand(&universe.mainCommandLayer,ship);
    }
    cat_tacticsRetaliate(ship,command,attackers);
}

/*-----------------------------------------------------------------------------
    Name        : cat_tacticsRetaliate
    Description : ship and its command answer the attackers by their tactics: evasive flees, neutral (and an
                  aggressive escort) passive attacks, aggressive attacks with the ships able to
----------------------------------------------------------------------------*/
// CAT: new (cat_tacticsRetaliate 0x00529cb0): nothing for a hidden ship, one within hyperspace range or a command
//      that is attacking, docking, launching, collecting, doing a special, halting, hyperspacing, linking,
//      unlinking, mimicking or doing order 17 [audit: M2-R5 command-tactics: no "building", order 6 is not in the
//      exe's case list 0x00529d1d]; a linked Acolyte or Mimic does not retaliate. The attackers are copied
//      (the caller's list is left alone).
void cat_tacticsRetaliate(Ship *ship,CommandToDo *command,AttackCommand *attackers)
{
    bool32 protecting = FALSE;
    bool32 linked = FALSE;
    AttackCommand *copy;
    udword size;
    MaxSelection attackersOwn;
    SelectCommand selectone;
    SelectCommand *source;
    CatLinkInfo *link;

    if ((attackers == NULL) || cat_tacticsSelectionHasFlag80Ship(ship,command) || bitTest(ship->header.flags,SOF_Hide))
    {
        return;
    }
    if (command != NULL)
    {
        switch (command->ordertype.order)
        {
            case COMMAND_ATTACK:
            case COMMAND_DOCK:
            case COMMAND_LAUNCH_SHIP:
            case COMMAND_COLLECT_RESOURCES:
            case COMMAND_SPECIAL:
            case COMMAND_HALT:
            case COMMAND_MP_HYPERSPACING:
            case COMMAND_CAT_LINK:
            case COMMAND_CAT_UNLINK:
            case COMMAND_CAT_MIMIC:
            case COMMAND_CAT_17:
                return;
            default:
                if (command->ordertype.attributes & COMMAND_MASK_PROTECTING)
                {
                    protecting = TRUE;
                }
                break;
        }
    }
    link = cat_shipGetLinkInfo(ship);
    if (cat_shipIsAcolyte(ship) && (link != NULL) && (link->partner != NULL))
    {
        linked = TRUE;
    }
    if (((ship->shiptype == sMimic) && (link != NULL) && (link->partner != NULL)) || linked)
    {
        return;
    }

    size = sizeofAttackCommand(attackers->numTargets);
    copy = memAlloc(size,"RetaliateTargets",Pyrophoric);
    memcpy(copy,attackers,size);

    switch (ship->tacticstype)
    {
        case Evasive:
            cat_tacticsRetaliateEvasive(ship,command,copy);
            break;
        case Neutral:
            cat_tacticsRetaliatePassive(ship,command,copy);
            break;
        case Aggressive:
            if (protecting)
            {
                cat_tacticsRetaliatePassive(ship,command,copy);
                break;
            }
            attackersOwn.numShips = 0;
            cat_tacticsKeepTargetable(ship,copy);
            if (command == NULL)
            {
                selectone.numShips = 1;
                selectone.timeLastStatus = 0.0f;     // CAT-FIX: the exe leaves it uninitialised (0x00529e2c)
                selectone.ShipPtr[0] = ship;
                source = &selectone;
            }
            else
            {
                source = command->selection;
            }
            MakeShipsAttackCapable((SelectCommand *)&attackersOwn,source,FALSE);
            MakeShipsOnlyFollowConstraints((SelectCommand *)&attackersOwn,cat_tacticsCanRetaliateCB);
            cat_selMakeShipsSameOwnerAsFirst((SelectCommand *)&attackersOwn);
            if ((attackersOwn.numShips > 0) && ((command == NULL) || (command->ordertype.order != COMMAND_ATTACK)))
            {
                clAttack(&universe.mainCommandLayer,(SelectCommand *)&attackersOwn,copy);
            }
            break;
        default:
            break;
    }
    memFree(copy);
}


/*-----------------------------------------------------------------------------
    Name        : cat_shipIsTargetableBy
    Description : TRUE if player (NULL: anyone) can target ship
----------------------------------------------------------------------------*/
// CAT: new (cat_shipIsTargetableBy 0x0052a080): not a ship the player has not detected, a crystal, a type with
//      catTypeFlags 0x4000, a ship being delivered to a salvage depot, an infected ship turning Beast, a salvage
//      target, a non-ship, the big gun and its unit, the crates, junk, message pod and buoy, the infected Ecclesiam,
//      or a ship outside the universe box
bool32 cat_shipIsTargetableBy(Player *player,Ship *ship)
{
    bool32 targetable = TRUE;
    ShipType type;

    if ((player != NULL) && !cat_shipDetectedByPlayer(ship,player->playerIndex))
    {
        targetable = FALSE;
    }
    type = ship->shiptype;
    if (cat_shipIsCrystal(ship) || (ship->staticinfo->catTypeFlags & 0x4000) || (ship->catSalvageDepot != NULL) ||
        (ship->catBeastConvertTime != 0.0f) || (ship->shipOrDerelict.specialFlags2 & SPECIAL_2_CatSalvageTargeted) ||
        (ship->header.objtype != OBJ_ShipType) ||
        (type == dBigGun) || (type == dBigGunUnit) || ((type >= dCrate) && (type <= dBuoy)) ||
        (type == dEcclesiamInfected) ||
        (ship->obj.posinfo.position.x < -smUniverseSizeX) || (smUniverseSizeX < ship->obj.posinfo.position.x) ||
        (ship->obj.posinfo.position.y < -smUniverseSizeY) || (smUniverseSizeY < ship->obj.posinfo.position.y) ||
        (ship->obj.posinfo.position.z < -smUniverseSizeZ) || (smUniverseSizeZ < ship->obj.posinfo.position.z))
    {
        targetable = FALSE;
    }
    return targetable;
}
