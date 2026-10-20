/*=============================================================================
    Tactics.c: Control Functions for Homeworld Tactics

    Created June 1998 Bryce
=============================================================================*/

#ifndef ___TACTICS_H
#define ___TACTICS_H

#include "ClassDefs.h"
#include "CommandLayer.h"
#include "Region.h"
#include "SpaceObj.h"
#include "Task.h"
#include "Vector.h"

// comment this bitch out and no more tacticsOn crap
//#define DEBUG_TACTICS

#define NO_ORDER_NO_ENEMY       0           //default if other conditions don't apply
#define NO_ORDER_ENEMY          1
#define MOVE_ORDERS_NO_ENEMY    2
#define MOVE_ORDERS_ENEMY       3
#define ATTACK_ORDERS           4
#define ATTACK_ORDERS_ENEMY     5
#define NUM_TACTICS_SITUATIONS  6

#define ORDER_TYPE_NO_ORDERS     0
#define ORDER_TYPE_MOVE_ORDERS   2
#define ORDER_TYPE_ATTACK_ORDERS 4
#define ORDER_TYPE_ENEMY_MASK    1  //can OR this two a order type to add enemy near bonuses and behavior

#define Tactics_Fighter     0
#define Tactics_Corvette    1
#define NUM_TACTICS_APPLICABLE 2

#define TightFormation  0               //default
#define LooseFormation  1               //for majority evasive,combat neutral
#define NUM_FORMATION_PADDINGS 2        //counter for blah

//dodge array references
#define DODGE_ONE_TIME      0
#define DODGE_TWO_TIME      1
#define DODGE_THRUST_MULT   2
#define DODGE_WAIT          3
#define NUM_DODGE_INFO      4

//ship object variable tacticsTalk flags
#define TACTALK_FLIPTURN    0x01
#define TACTALK_GEN_FLIGHMAN_NEED_LOCK  0x02
#define TACTALK_BARRELROLL  0x04

typedef struct
{
    Node retreatLink;
    struct Ship *retreater;         //ship that is retreating
    CommandToDo *fleeingfrom;       //attack command that ship is fleeing from
    vector retreatFrom;             //position of Attackers (average location)
    real32 distanceStartSqr;        //square of distance from attackers at time of retreat command
} RetreatAtom;

typedef struct
{
    Node attackLink;
    struct Ship *retreater;         //ship that originally retreated
    real32 expirytime;
    real32 retreatTime;             //univupdate time that retreat was registered

    //must be last!!!
    SelectCommand attackerList;    //list of ships on the lookout for retreater ship
} AttackAtom;

// CAT: Cataclysm's TacticsInfo (0x9C4 bytes at 0x00c523a4, laid out by the exe's TacticsInfotable 0x008a80a0 and
//      tacticsStartUp 0x005290b0): HW1's EnemyNearByCheckRate, MenuFeedBack_X/Y, menuPopupTime, RetreatDistanceSqr,
//      RetreatCheckRate, the tactics bonus tables (MaxVelocity, FuelBurn, Damage, BulletRange, BulletSpeed, Maneuv),
//      the burst fire / damage multipliers and the damage modifiers are gone; the class arrays have Cataclysm's 12
//      classes. Of the exe's code only EnemyNearZone, the passive retaliation fire times, formationPadding, the guard
//      check and movingAttackTurnsIntoMoveCommandDistanceSqr are read; the rest is set by Tactics.script only.
typedef struct
{
    real32 EnemyNearZone;                               // +0x000 (0x00c523a4)
    real32 PassiveRetaliationFireTimeModifierLow;       // +0x004
    real32 PassiveRetaliationFireTimeModifierHi;        // +0x008
    real32 RetreatGetAwayDistSqr[NUM_CLASSES];          // +0x00C
    real32 freeRetreatTime;                             // +0x03C
    real32 AttackMemoryTimeOut;                         // +0x040
    real32 catCapitalTransMult[NUM_TRANS_DEGOFFREEDOM]; // +0x044 CAT: HW1's capital multipliers: no script key, no reader
    real32 catCapitalRotMult[NUM_ROT_DEGOFFREEDOM];     // +0x05C
    real32 catCapitalTurnMult[NUM_TURN_TYPES];          // +0x074
    real32 InterceptorVerticalMultiplier;               // +0x080
    real32 formationPadding[NUM_FORMATION_PADDINGS];    // +0x084 (0x00c52428)
    real32 DodgeInfo[NUM_DODGE_INFO][TOTAL_NUM_SHIPS];  // +0x08C (0x00c52430, 0x244 dwords)
    real32 DodgeLowAddTime,DodgeHighAddTime;            // +0x99C
    sdword tacticsGuardConditionCheckRate;              // +0x9A4 (0x00c52d48; set with scriptSetReal32CB, as in HW1)
    sdword tacticsGuardConditionCheckFrame;             // +0x9A8
    real32 tacticsGuardReturnToGuardingDistance;        // +0x9AC
    real32 tacticsGuardReturnToGuardingDistanceSqr;     // +0x9B0
    sdword holdFormationDuringBattle[NUM_TACTICS_TYPES];// +0x9B4
    real32 movingAttackTurnsIntoMoveCommandDistanceSqr; // +0x9C0 (0x00c52d64)
} TacticsInfo;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(TacticsInfo) == 0x9C4, "TacticsInfo is 0x9C4 bytes in cataclysm.exe (0x00c523a4..0x00c52d68)");
CAT_ASSERT_OFFSET(TacticsInfo, RetreatGetAwayDistSqr, 0x00C);        // 0x00c523b0
CAT_ASSERT_OFFSET(TacticsInfo, AttackMemoryTimeOut, 0x040);          // 0x00c523e4
CAT_ASSERT_OFFSET(TacticsInfo, InterceptorVerticalMultiplier, 0x080);// 0x00c52424
CAT_ASSERT_OFFSET(TacticsInfo, formationPadding, 0x084);             // 0x00c52428
CAT_ASSERT_OFFSET(TacticsInfo, DodgeInfo, 0x08C);                    // 0x00c52430
CAT_ASSERT_OFFSET(TacticsInfo, DodgeLowAddTime, 0x99C);              // 0x00c52d40
CAT_ASSERT_OFFSET(TacticsInfo, tacticsGuardConditionCheckRate, 0x9A4);   // 0x00c52d48
CAT_ASSERT_OFFSET(TacticsInfo, tacticsGuardReturnToGuardingDistanceSqr, 0x9B0);   // 0x00c52d54
CAT_ASSERT_OFFSET(TacticsInfo, holdFormationDuringBattle, 0x9B4);    // 0x00c52d58
CAT_ASSERT_OFFSET(TacticsInfo, movingAttackTurnsIntoMoveCommandDistanceSqr, 0x9C0);   // 0x00c52d64
#endif

extern TacticsInfo tacticsInfo;

//special operation tweakables
// CAT: the exe keeps kamikazeDamage, kamikazeSpeedBurst and the burst values (TacticsInfotable 0x008a80a0); HW1's
//      kamikazeFuelBurnMult and speedBurst* are gone
extern real32 kamikazeDamage[TOTAL_NUM_SHIPS];
extern real32 kamikazeSpeedBurst;
extern real32 burstRange;
extern real32 burstChargeTime;
extern real32 burstCoolDownTime;
extern real32 burstDamageLo[NUM_RACES];
extern real32 burstDamageHi[NUM_RACES];
extern real32 burstSpeed;
extern real32 oneOverburstSpeed;
extern real32 burstRadius;

extern real32 ATTACKING_FROM_ABOVE_MIN_DIST;
extern real32 ATTACKING_FROM_ABOVE_MIN_RATIO;

//data

extern regionhandle    tacticsMenuRegion;
//ProtoTypes

void tacticsStartUp();
void tacticsShutDown();
void tacticsPopUpSetUp(TacticsType tacticstype);     // CAT-DORMANT: no exe body (see Tactics.c)
void tacticsReportMove(CommandLayer *comlayer,SelectCommand *selection);

void tacticsShipDied(Ship *ship);

real32 tacticsMaxDistToTarget(SelectCommand *selection,SpaceObjRotImpTarg *target);
bool32 tacticsCheckGuardConditionsDuringAttack(struct CommandToDo *command);
// CAT (M2-R10 oracle-rung-physics): tacticsGetShipsMaxVelocity 0x00529450 returns its product unrounded on the FPU
double tacticsGetShipsMaxVelocity(Ship *ship);

void tacticsMakeShipsNotLookForOtherShips(SelectCommand *selection);

// CAT: Cataclysm's retaliation (0x005294c0..0x0052a1d0): univCheckShipState 0x0054a650 calls cat_tacticsShipAttacked
//      where HW1 called tacticsDelegateAttackCommand / tacticsDelegateSingleAttack
Ship *cat_tacticsFindResponsibleShip(Ship *ship);                                          // CAT: 0x005294c0
void cat_tacticsMergeAttackTargets(AttackCommand *dest,AttackCommand *existing,AttackCommand *attackers);   // CAT: 0x005295c0
void cat_tacticsRetaliatePassive(Ship *ship,CommandToDo *command,AttackCommand *attackers);   // CAT: 0x005296f0
void cat_tacticsAveragePosition(SelectCommand *selection,vector *vec);                     // CAT: 0x00529830
void cat_tacticsRetaliateEvasive(Ship *ship,CommandToDo *command,AttackCommand *attackers);   // CAT: 0x005298e0
bool32 cat_tacticsSelectionHasFlag80Ship(Ship *ship,CommandToDo *command);                 // CAT: 0x00529b70
void cat_tacticsShipAttacked(Ship *ship,CommandToDo *command,AttackCommand *attackers);    // CAT: 0x00529c20
void cat_tacticsRetaliate(Ship *ship,CommandToDo *command,AttackCommand *attackers);       // CAT: 0x00529cb0
void cat_tacticsEvasiveRetreatToDock(Ship *ship);                         // CAT: 0x00529f90 (M2-R4 combat)
bool32 cat_tacticsNotRetreatingToDockCB(Ship *ship);                    // CAT: 0x00529f00
void cat_tacticsRemoveRetreatingShips(SelectCommand *selection);         // CAT: 0x00529f70
bool32 cat_shipIsTargetableBy(struct Player *player,Ship *ship);         // CAT: 0x0052a080

extern real32 IONCANNON_TARGETS_FIGHTER_ANGLE;
extern real32 IONCANNON_TARGETS_CORVETTE_ANGLE;

extern real32 IONCANNON_TARGETS_FIGHTER_TRIGGERHAPPY;
extern real32 IONCANNON_TARGETS_CORVETTE_TRIGGERHAPPY;

extern real32 IONCANNON_TARGETS_FIGHTER_MAXANGLESPEED;
extern real32 IONCANNON_TARGETS_CORVETTE_MAXANGLESPEED;

#endif
