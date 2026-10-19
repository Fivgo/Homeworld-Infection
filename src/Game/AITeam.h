// =============================================================================
//  AITeam.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 5/31/1998 by dstone
// =============================================================================

#ifndef ___AITEAM_H
#define ___AITEAM_H

#include "AIEvents.h"
#include "AIUtilities.h"
#include "AIVar.h"
#include "KAS.h"
#include "ShipDefs.h"
#include "ShipSelect.h"

//team flags
// Mask 0xff00000000 to Special Team Flags
#define AIT_SpecialTeamMask     0xff000000
#define FAST_ROVING_GUARD       0x00000001
#define SLOW_ROVING_GUARD       0x00000002
#define HARASS_TEAM             0x00000004
#define RESOURCECOLLECT_TEAM    0x00000008

#define AIT_DestroyTeam         0x00000010      //flag set to destroy the team in a safe place
#define AIT_Reissue01			0x00000020		//flag set to keep certain orders from being reissued over and over again
#define AIT_Reissue10			0x00000040		//flag set to keep certain orders from being reissued over and over again

#define TEAM_HOMOGENEOUS        0x00000100
#define TEAM_HETEROGENEOUS      0x00000200

#define TEAM_NEEDS_SUPPORT      0x00001000      //team needs a support ship
#define TEAM_GETTING_SUPPORT    0x00002000      //a support ship is near the team
#define TEAM_DOCKING            0x00004000
#define TEAM_RETREATING         0x00008000
#define TEAM_ARMADA             0x00010000
#define TEAM_Hyperspaceable     0x00020000      //the team can hyperspace (no fighters guarding or anything)
#define TEAM_TempGuard			0x00040000		//the team is an armada team temp guarding

#define TEAM_SpecialOptimal     0x00100000      //denotes whether the special team has setup with optimal targets/partners
#define TEAM_AdvancedMoveAttack 0x00200000      //the MoveAttack move will pick out targets individually
#define TEAM_SwarmTarget        0x00400000      //set when a swarm team should get it's target from the Targets structure
#define TEAM_AmIBeingWatched    0x00800000      //set when a swarm team is checking if it's being watched

#define TEAM_CloakFighters      0x01000000
#define TEAM_CloakedFighters    0x02000000
#define TEAM_CloakGenerator     0x04000000
#define TEAM_CloakedGenerator   0x08000000

//#define TEAM_GravWellGenerator  0x10000000
//#define TEAM_GravWellGenerating 0x20000000
#define TEAM_CloakCoop          0x10000000      //is cooperating with a cloak generator team

//swarmer stuff
#define MAX_NEW_SWARMERS    15

#define MAX_SPLIT           4

typedef enum {
    MOVE_DONE,                   //0
    MOVE_GUARDSHIPS,             //1
    MOVE_DEFMOSHIP,              //2
    MOVE_TEMPGUARD,              //3
    MOVE_GETSHIPS,               //4
    MOVE_FORMATION,              //5
    MOVE_MOVETEAM,               //6
    MOVE_MOVETEAMINDEX,          //7
    MOVE_MOVETEAMSPLIT,          //8
    MOVE_INTERCEPT,              //9
    MOVE_MOVETO,                 //10
    MOVE_PATROLMOVE,             //11
    MOVE_ACTIVEPATROL,           //12
    MOVE_ACTIVERECON,            //13
    MOVE_SHIPRECON,              //14
    MOVE_COUNTSHIPS,             //15
    MOVE_SPECIAL,                //16
    MOVE_ATTACK,                 //17
    MOVE_ADVANCEDATTACK,         //18
    MOVE_FLANKATTACK,            //19
    MOVE_MOVEATTACK,             //20
    MOVE_HARASSATTACK,           //21
    // CAT (M2-R7 ai-orders-moves): no P2 swarm moves (HW1's MOVE_SWARMATTACK / SWARMDEFENSE / SWARMPOD, 22-24): the
    //      exe's creators store 0x16 for fancy get-ships (aimCreateFancyGetShipsNoAdd 0x0040df90), 0x17 dock
    //      (aimCreateDock 0x0040c280), 0x2B kamikaze (0x0040d970) and 0x2D delete-team (0x0040d050)
    MOVE_FANCYGETSHIPS,          //22
    MOVE_DOCK,                   //23
    MOVE_LAUNCH,                 //24
    MOVE_REINFORCE,              //25
    MOVE_VARSET,                 //26
    MOVE_VARINC,                 //27
    MOVE_VARDEC,                 //28
    MOVE_VARWAIT,                //29
    MOVE_VARDESTROY,             //30
    MOVE_GUARDCOOPTEAM,          //31
    MOVE_SUPPORT,                //32
    MOVE_ARMADA,                 //33
    MOVE_CONTROLRESOURCES,       //34
    MOVE_RESVOLUME,              //35
    MOVE_CAPTURE,                //36
    MOVE_ACTIVECAPTURE,          //37
    MOVE_ACTIVEMINE,             //38
    MOVE_MINEVOLUME,             //39
    MOVE_SPECIALDEFENSE,         //40
    MOVE_ACTIVERES,              //41
    MOVE_MOTHERSHIP,             //42
    MOVE_KAMIKAZE,               //43
    MOVE_HYPERSPACE,             //44
    MOVE_DELETETEAM,             //45
    NUMBER_OF_MOVETYPES    // must go last
} AIMoveTypes;

typedef enum
{
    TEAM_BEGINNER,
    TEAM_BEGINNER_INTERMEDIATE,
    TEAM_INTERMEDIATE,
    TEAM_INTERMEDIATE_ADVANCED,
    TEAM_ADVANCED,
    NUM_TEAMLEVEL_TYPES
} AITeamLevel;

typedef struct {
    ShipType  shipType;
    sdword    numShips;
    AIVar     *doneVar;
    sdword    priority;
} paramsGetShips;

// CAT: 20 alternatives of a dword each (HW1 12 of a byte each): AlternativeShips is 0xA8 bytes, copied as 0x2A dwords by
//      aimCreateFancyGetShipsNoAdd (inlined at 0x0040e040); aioCreateReconaissance 0x0040e3a0 fills it on the stack as
//      numNextPicks -0xC0, alternativeFlags -0xBC, ship types from -0xB8, equivalents from -0x68 (= +0x58), and
//      aimProcessFancyGetShips reads both arrays as dwords (+0x8 and +0x58 of the struct)
#define MAX_NUM_ALTERNATIVES            20      // CAT: was 12
#define ALTERNATIVE_SHIP_BASE           10      // base value for equivalence comparisons between ships
#define ALTERNATIVE_RANDOM              1       // alternativeFlags - indicates alternatives should be picked randomly

#define AIT_FAST_PATROL                 0
#define AIT_SLOW_PATROL                 1

typedef struct AlternativeShips
{
    sdword  numNextPicks;
    sdword  alternativeFlags;
    sdword  shipTypeNextPicks[MAX_NUM_ALTERNATIVES];      // CAT: dwords (HW1 sbyte), +0x08
    sdword  shipNumEquivNextPicks[MAX_NUM_ALTERNATIVES];  // CAT: dwords (HW1 sbyte), +0x58
} AlternativeShips;

// CAT: no pointers, so 0xA8 on every target
_Static_assert(sizeof(AlternativeShips) == 0xA8, "AlternativeShips is 0xA8 bytes in cataclysm.exe");
_Static_assert(offsetof(AlternativeShips, shipNumEquivNextPicks) == 0x58, "equivalents at +0x58 in cataclysm.exe");

#define SetNumAlternatives(alt,num) \
    (alt).numNextPicks = (num);   \
    (alt).alternativeFlags = 0; \
    dbgAssertOrIgnore((num) <= MAX_NUM_ALTERNATIVES);

#define SetNumAlternativesFlags(alt,num,flags) \
    (alt).numNextPicks = (num);   \
    (alt).alternativeFlags = (flags); \
    dbgAssertOrIgnore((num) <= MAX_NUM_ALTERNATIVES);

#define SetAlternative(alt,index,stype,equivnum) \
    (alt).shipTypeNextPicks[index] = (sdword)(stype);  \
    (alt).shipNumEquivNextPicks[index] = (equivnum);

typedef struct {
    char varName[AIVAR_LABEL_MAX_LENGTH+1];
    sdword value;
} paramsVarSet;

typedef struct {
    char varName[AIVAR_LABEL_MAX_LENGTH+1];
    sdword value;
} paramsVarWait;

typedef struct {
    char varName[AIVAR_LABEL_MAX_LENGTH+1];
} paramsVarInc;

typedef struct {
    char varName[AIVAR_LABEL_MAX_LENGTH+1];
} paramsVarDec;

typedef struct {
    char varName[AIVAR_LABEL_MAX_LENGTH+1];
} paramsVarDestroy;

typedef struct {
    ShipType shipType;
    sdword   numShips;
    AIVar    *doneVar;
    sdword   priority;
    AlternativeShips alternatives;
} paramsFancyGetShips;

typedef struct {
    SelectCommand *ships;
} paramsGuardShips;

typedef struct
{
    SelectCommand *targets;
} paramsDefMoship;

typedef struct
{
    //nothing yet
    udword placeholder;
} paramsTempGuard;

typedef struct {
    SelectCommand *ships;
} paramsKamikaze;

typedef struct {
    SelectCommand *ships;
} paramsAttackShips;

typedef struct
{
    SelectCommand *targets;
    Ship *target_ship;
} paramsAdvancedAttack;

typedef struct
{
    SelectCommand *targets;
    bool8          hyperspace;
} paramsFlankAttack;

typedef struct
{
    SelectCommand *targets;
    ShipPtr target_ship;
    vector  destination;
} paramsMoveAttack;

typedef struct
{
    vector destination;
    udword index;
} paramsMoveTeam;

typedef struct
{
    SelectCommand *ships;
    Path *destinations;
} paramsMoveSplit;

typedef struct
{
    ShipPtr ship;
    real32  interval;
    real32  next_int;
    vector  destination;
    bool32    moving;
} paramsIntercept;

typedef struct
{
    vector destination;
    vector source;
    real32 limiter;
    real32 timer;
    udword type;
} paramsMoveTo;

typedef struct
{
    Path *path;
    struct AITeamMove *loopMove;
    udword startIndex;
} paramsPatrolMove;

typedef struct
{
    udword patroltype;
} paramsActivePatrol;

typedef struct
{
    bool32 outwards;
    bool32 enemyrecon;
} paramsActiveRecon;

typedef struct
{
    SelectCommand *ships;
    SelectCommand *foundships;
} paramsShipRecon;

typedef struct
{
    //nothing yet
    udword placeholder;
} paramsCountShips;

typedef struct {
    TypeOfFormation formationtype;
} paramsFormation;

typedef struct
{
    ShipPtr target;
} paramsHarassAttack;

// CAT (M2-R7 ai-orders-moves): no P2 swarm move parameters (paramsSwarmAttack / SwarmDefense / SwarmPod)

#define dockmoveFlags_Normal            0
#define dockmoveFlags_AtShip            1
#define dockmoveFlags_Stay              2
#define dockmoveFlags_Instantaneously   4
typedef struct
{
    SelectCommand *shipsDocking;
    ShipPtr dockAt;
    sdword dockmoveFlags;
} paramsDock;

typedef struct
{
    struct AITeam *reinforceteam;
} paramsReinforce;

typedef struct
{
    SelectCommand *ships;
} paramsSupport;

typedef struct
{
    Volume  volume;
    ResourceSelection *volResources;
    ResourceSelection *takenResources;
    bool8 strictVolume;
} paramsResVol;


typedef struct
{
    ShipPtr ship;
} paramsCapture;

typedef struct
{
    Volume volume;
    bool32   inposition;
} paramsMineVol;

//
//  teams
//
// CAT: Cataclysm's queue holds 64 messages as their kas2c hashes (cat_kasHashString of the message name, compared as
//      words; M2-R8 kasf library: they were taken for pointers) and keeps a count rather than HW1's ring head: a received message is replaced by the last one
//      (aitMsgSend 0x00417e10, aitMsgReceived 0x00417e60, cat_aitMsgQueueRemoveAt 0x00417ec0); it lives in the team
//      (AITeam +0x7C, 0x204 bytes) and msgQueue points at it once a message arrived
#define MSG_QUEUE_MAX_MSGS 64
typedef struct {
    sdword          head;                             // CAT: the number of messages queued
    udword          msgs[MSG_QUEUE_MAX_MSGS];         // CAT: the messages' hashes
    struct AITeam   *msgSenders[MSG_QUEUE_MAX_MSGS];  // team that sent the corresponding msg above
                                                      // NOTE:  if a team dies, the message will be deleted from this queue
} MsgQueue;
#if defined(__i386__)
_Static_assert(sizeof(MsgQueue) == 0x204, "an AITeam's message queue is 0x204 bytes in cataclysm.exe");
#endif

typedef enum {
    AttackTeam,
    DefenseTeam,
    ResourceTeam,
    ScriptTeam,
    AnyTeam,        //neuter type
    NumTeamType
} TeamType;

typedef struct AITeam {
    TeamType          teamType;
    udword            teamFlags;
    udword            catUnknown08;     // CAT: +0x08, not set by aitCreate 0x00416810 nor read by the AI code read in M2-R7
    udword            TeamFeatures;
    ubyte             teamDelay;
    udword            newships;
    struct AIPlayer   *aiplayerowner;
    GrowSelection     shipList;
    udword            teamStrength;
    udword            teamValue;
    ubyte             teamDifficultyLevel;
    LinkedList        moves;
    struct AITeamMove *curMove;
    void              *custTeamInfo;
    void (*TeamDiedCB)(struct AITeam *team);
    struct AITeam     *cooperatingTeam;
    void (*cooperatingTeamDiedCB)(struct AITeam *team);
    MsgQueue          *msgQueue;
    struct AITeam     *msgSender;

    // KAS attributes (if teamType == SCRIPT_TEAM)
    // CAT (M2-R8 kasf library): the label, FSM and state names are cat_kasHashString hashes (AITeam +0x58 / +0x5C /
    //      +0x60: kasAddShipToTeam 0x00491120, kasFSMCreate 0x00490660, kasJump 0x00490640); HW1 kept 48-byte strings
    udword              kasLabel;
    udword              kasFSMName;
    udword              kasStateName;
    KASWatchFunction    kasFSMWatchFunction;
    KASWatchFunction    kasStateWatchFunction;
    ShipType            kasOrigShipsType;  // predominant ship type of this team (in mission layout file)
    sdword              kasOrigShipsCount; // original size of team (in mission layout file)
    TacticsType         kasTactics;  // current tactics setting, passed to certain moves
    TypeOfFormation     kasFormation; // current formation setting, passed to certain moves

    // CAT: the message queue's storage (AITeam +0x7C in cataclysm.exe)
    MsgQueue            catMsgQueue;
} AITeam;
// CAT: cataclysm.exe's AITeam is 0x280 bytes with the same fields up to +0x54 (+0x08 inserted), its KAS identity as
//      three hashes (+0x58 label, +0x5C FSM, +0x60 state) and its message queue at +0x7C; since M2-R8 kasf library this
//      tree has the same layout on ILP32 (pinned below); the mode-A loader still converts a team chunk field by field
//      (aitCat32LoadTeam in AITeam.c), since the pointers are 8 bytes on LP64
#if defined(__i386__)
_Static_assert(sizeof(AITeam) == 0x280, "AITeam is 0x280 bytes in cataclysm.exe");
_Static_assert(offsetof(AITeam, kasLabel) == 0x58 && offsetof(AITeam, kasFSMWatchFunction) == 0x64 &&
               offsetof(AITeam, kasTactics) == 0x74 && offsetof(AITeam, catMsgQueue) == 0x7C,
               "AITeam's KAS fields at cataclysm.exe's offsets");
#endif

// all move processing functions look like this:
// (returning 1 when we this move has completed
//  and any required waiting is finished, 0 otherwise)
typedef sdword (*aimProcessFunction) (AITeam *team);
typedef void (*aimShipDied) (AITeam *team, struct AITeamMove *move, Ship *ship);
typedef void (*aimCloseFunction) (AITeam *team, struct AITeamMove *move);
typedef void (*aimResourceDied) (AITeam *team, struct AITeamMove *move, Resource *resource);

typedef struct AITeamMove {
    Node listNode;
    AIMoveTypes type;

    union {
        paramsGuardShips        guardShips;
        paramsDefMoship         defmoship;
        paramsTempGuard         tempguard;
        paramsGetShips          getShips;
        paramsFormation         formation;
        paramsMoveTeam          move;
        paramsMoveSplit         movesplit;
        paramsIntercept         intercept;
        paramsMoveTo            moveTo;
        paramsPatrolMove        patrolmove;
        paramsActivePatrol      activepatrol;
        paramsActiveRecon       activerecon;
        paramsShipRecon         shiprecon;
        paramsCountShips        countShips;
        paramsAttackShips       attack;
        paramsAdvancedAttack    advatt;
        paramsFlankAttack       flankatt;
        paramsMoveAttack        moveatt;
        paramsHarassAttack      harass;
        paramsFancyGetShips     fancyGetShips;
        paramsDock              dock;
        paramsReinforce         reinforce;
        paramsSupport           support;
        paramsSupport           rescontrol;
        paramsResVol            resvolume;
        paramsCapture           capture;
        paramsMineVol           minevolume;
        paramsVarSet            varSet;
        paramsVarInc            varInc;
        paramsVarDec            varDec;
        paramsVarWait           varWait;
        paramsVarDestroy        varDestroy;
        paramsKamikaze          kamikaze;

        // ...
    } params;

    AIEvents events;

    aimProcessFunction processFunction;
    aimShipDied moveShipDiedFunction;
    aimCloseFunction moveCloseFunction;

    //only for resource oriented moves
    aimResourceDied moveResourceDiedFunction;

    bool8 processing;   // if the process function has starting working on this move yet

    bool8 wait;    // non-zero = wait for the move to finish
    bool8 remove;  // non-zero = remove this move from the move list when it has finished
    TypeOfFormation formation;
    TacticsType     tactics;

} AITeamMove;

#if defined(__i386__) || defined(_M_IX86)
// CAT (M2-R7 ai-orders-moves): the exe's move (memAllocFunction(0x170) in every aimCreate*; type +0x10, params +0x14,
//      events +0xCC, process / shipDied / close +0x154 / +0x158 / +0x15C, processing / wait / remove +0x164..+0x166,
//      formation +0x168, tactics +0x16C)
_Static_assert(sizeof(AITeamMove) == 0x170, "AITeamMove is 0x170 bytes in cataclysm.exe");
_Static_assert(offsetof(AITeamMove, type) == 0x10, "AITeamMove.type at +0x10");
_Static_assert(offsetof(AITeamMove, events) == 0xCC, "AITeamMove.events at +0xCC (aieHandlersClear 0x00403eb0)");
_Static_assert(offsetof(AITeamMove, processFunction) == 0x154, "AITeamMove.processFunction at +0x154");
_Static_assert(offsetof(AITeamMove, processing) == 0x164, "AITeamMove.processing at +0x164");
_Static_assert(offsetof(AITeamMove, tactics) == 0x16C, "AITeamMove.tactics at +0x16C");
#endif

/*=============================================================================
    Macros:
=============================================================================*/
#define InitNewMove(newMove,movetype,waitflag,removeflag,form,tactic,moveprocessfunc,moveshipdiedfunc,moveclosefunc)    \
    (newMove)->type                     = (movetype);             \
    (newMove)->processing               = FALSE;                  \
    (newMove)->wait                     = (waitflag);             \
    (newMove)->remove                   = (removeflag);           \
    (newMove)->formation                = (form);                 \
    dbgAssertOrIgnore(tactic >= 0);                                       \
    dbgAssertOrIgnore(tactic < NUM_TACTICS_TYPES);                        \
    (newMove)->tactics                  = (tactic);               \
    (newMove)->processFunction          = (moveprocessfunc);      \
    (newMove)->moveShipDiedFunction     = (moveshipdiedfunc);     \
    (newMove)->moveCloseFunction        = (moveclosefunc);        \
    aieHandlersClear(newMove)

#define FixMoveFuncPtrs(move,moveprocessfunc,moveshipdiedfunc,moveclosefunc)     \
    (move)->processFunction          = (moveprocessfunc);      \
    (move)->moveShipDiedFunction     = (moveshipdiedfunc);     \
    (move)->moveCloseFunction        = (moveclosefunc)

#define aitApproxTeamPos(team)          ((team)->shipList.selection->ShipPtr[0]->obj.posinfo.position)
#define aitNumTeamShips(team)           ((team)->shipList.selection->numShips)
#define aitTeamShipTypeIs(type, team)   (((team)->shipList.selection->numShips) && ((team)->shipList.selection->ShipPtr[0]->shiptype == (type)))
#define aitTeamShipClassIs(cls, team)   (((team)->shipList.selection->numShips) && ((team)->shipList.selection->ShipPtr[0]->staticinfo->shipclass == (cls)))

/*=============================================================================
    Function Prototypes:
=============================================================================*/
//teams initialization and closing routines
void aitInit(struct AIPlayer *aiplayer);
void aitClose(struct AIPlayer *aiplayer);


/*-----------------------------------------------------------------------------
    General Utility Functions:
-----------------------------------------------------------------------------*/
void aitAddmoveBeforeAndMakeCurrent(AITeam *team, AITeamMove *newMove, AITeamMove *thisMove);
void aitAddmoveBeforeAndMakeCurrentNoSpecial(AITeam *team, AITeamMove *newMove, AITeamMove *thisMove);

/*-----------------------------------------------------------------------------
    Team State Query Functions:
-----------------------------------------------------------------------------*/
bool32 aitAnyTeamOfPlayerAttackingThisShip(struct AIPlayer *aiplayer,Ship *ship);

udword aitFindNumTeamsWithFlag(udword flag);
AITeam *aitFindNextTeamWithFlag(AITeam *team, udword flag);

sdword aitCountTeamsWaitingForShips(TeamType type);

//team status functions
bool32 aitTeamIsDone(AITeam *team);
bool32 aitTeamIsIdle(AITeam *team);
bool32 aitTeamIsGuarding(AITeam *team);
bool32 aitTeamIsAttacking(AITeam *team);
bool32 aitTeamIsntDefendingMothership(AITeam *team, SelectCommand *enemyships);
bool32 aitTeamIsFinishedMoving(AITeam *team, vector destination, real32 range);
bool32 aitTeamIsDoingSpecialOp(AITeam *team);
bool32 aitCheckIfOtherDefTeamAnsweringSignalNeedsHelp(AITeam *team, SelectCommand *ships);
bool32 aitTeamIsInMothershipRange(AITeam *team);

void aitRecallGuardTeam(AITeam *team);
bool32 aitNeedStrikeSupport(udword minstr);

//set team attributes
void aitMakeTeamSupportShips(AITeam *team, SelectCommand *ships);
void aitSetTeamHomoHetero(AITeam *team);
void aitSetTeamSpecialFlags(AITeam *team);
bool32 aitTeamHomogenous(AITeam *team);
void aitSetTeamStrengthValue(AITeam *team);
ubyte aitSetTeamDifficultyLevel(AITeam *team, udword playerDL, udword teamDL);

AITeam *aitFindMostValuable(udword valueness);

AITeam *aitFindGoodCoopTeam(ShipType type);

AITeam *aitCreate(TeamType teamType);
void aitDestroy(struct AIPlayer *aiplayer, AITeam *team, bool32 removeAllReferencesToTeam);

void aitDeleteCurrentMove(AITeam *team);
void aitDeleteMovesUntilMoveType(AITeam *team, AIMoveTypes type);
void aitDeleteAllTeamMoves(AITeam *team);

void aitAddShip(AITeam *team, ShipPtr ship);

// CAT (M2-R7 ai-orders-moves): no P2 swarm team helpers

void aitSpecialDefenseCoopTeamDiedCB(AITeam *team);
void GenericCooperatingTeamDiedCB(AITeam *team);

// returns TRUE if removed ship
bool32 aitRemoveShip(AITeam *team, ShipPtr ship);

//process current move for each team
void aitExecute(void);


void aitShipDied(struct AIPlayer *aiplayer,ShipPtr ship);
void aitResourceDied(struct AIPlayer *aiplayer, Resource *resource);

bool32 aitCheckForLeaderAndMoveToFront(AITeam *team);

bool32 aitCheckAmIBeingWatched(AITeam *team, SelectCommand *sel);
void aitSetAmIBeingWatched(AITeam *team, SelectCommand *sel);


sdword aitMsgReceived(AITeam *teamp, udword msg);                 // CAT: the message's hash
void aitMsgSend(AITeam *fromTeamp, AITeam *teamp, udword msg);
void aitMsgQueueFree(AITeam *teamp);
udword cat_aitMsgQueueRemoveAt(AITeam *teamp, udword index);     // CAT: 0x00417ec0
void cat_aitMsgQueueFlush(AITeam *team);                         // CAT: 0x00417f20

void aitSave(struct AIPlayer *aiplayer);
void aitLoad(struct AIPlayer *aiplayer);
void aitFix(struct AIPlayer *aiplayer);

//
//  reserved pointers to all allocated AIVars
//
#define AITEAM_ALLOC_INITIAL   64
#define AITEAM_ALLOC_INCREMENT 32

/*=============================================================================
    Globals:
=============================================================================*/

extern AITeam *savingThisAITeam;

#endif
