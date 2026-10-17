// =============================================================================
//  CommandLayer.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 7/2/1997 by gshaw
// =============================================================================

#ifndef ___COMMANDLAYER_H
#define ___COMMANDLAYER_H

#include "CommandDefs.h"
#include "Dock.h"
#include "Formation.h"
#include "ResCollect.h"


#ifdef HW_BUILD_FOR_DEBUGGING

#define CL_TEXTFEEDBACK     1

#else

#define CL_TEXTFEEDBACK     0

#endif
/*=============================================================================
    Types:
=============================================================================*/

//Ru Transfer types for one person
#define RUTRANS_BUILDCANCEL     101
#define RUTRANS_BUILTSHIP       102
#define RUTRANS_GENERATEDRUS    103

//defines for hyperspaceing states
#define COM_HYPERSPACE_START    0
#define COM_HYPERSPACE_OUT      1
#define COM_HYPERSPACE_WAIT     2
#define COM_HYPERSPACE_IN       3

// CAT: Cataclysm's move order is a waypoint path (CommandToDo+0x18, 0x108 bytes): HW1's heading/destination pair is
//      gone. clMoveThese 0x004467c0, ChangeOrderToMove 0x004460d0 and ChangeOrderToHyperSpace 0x004470f0 store the path
//      type at +0x18, the point count at +0x1C, the points from +0x20 and the current point at +0x11C;
//      processMoveToDo 0x00443cb0 walks them (CalculateMoveToPoints from point i-1 to point i). Path type names chosen
//      here from processMoveToDo's switch: 0 once to the last point, 1 a loop, 2/3 a patrol out and back.
#define CAT_MOVE_MAX_POINTS         21      // (0x11C - 0x20) / 12
#define CAT_MOVEPATH_ONCE           0
#define CAT_MOVEPATH_LOOP           1
#define CAT_MOVEPATH_PATROL_OUT     2
#define CAT_MOVEPATH_PATROL_BACK    3
typedef struct
{
    sdword pathType;                        // +0x00 (CommandToDo+0x18) CAT_MOVEPATH_*
    sdword numPoints;                       // +0x04 (CommandToDo+0x1C)
    vector points[CAT_MOVE_MAX_POINTS];     // +0x08 (CommandToDo+0x20)
    sdword curPoint;                        // +0x104 (CommandToDo+0x11C) the point the ships fly to now
} MoveCommand;

typedef SelectAnyCommand AttackCommand;
typedef SelectAnyCommandMax6 AttackCommandMax6;
typedef SelectAnyCommand SpecialCommand;

#define PROTECTFLAGS_JUST_FOLLOW     1
typedef SelectCommand ProtectCommand;
typedef SelectCommandMax6 ProtectCommandMax6;

#define IAmAWingman(attackvar) ((attackvar)->myLeaderIs)
#define IAmALeader(attackvar) ((attackvar)->myWingmanIs)

typedef struct
{
    ShipPtr receiverShip;
} LaunchShipCommand;

typedef struct
{
    ShipType shipType;
    ShipRace shipRace;
    ShipPtr creator;
    udword frameAtWhichToCreate;
    udword playerIndex;
} BuildingShipCommand;

typedef struct
{
    uword order;
    uword attributes;
} CommandOrder;

// CAT: the parameters of Cataclysm's orders 14-16 (CommandToDo+0x2DC..+0x307; names chosen from the creators)
typedef struct
{
    vector target;                          // +0x2DC the point the big gun fires at (cat_clPointOrder 0x0044dd70)
} CatPointCommand;
typedef struct
{
    udword mode;                            // +0x2E8 cat_clMimic 0x0044da80: cat_mimicCommand's arguments
    udword shipType;                        // +0x2EC
    udword player;                          // +0x2F0
    udword index;                           // +0x2F4
} CatMimicCommand;
typedef struct
{
    uword playerIndex;                      // +0x2F8 cat_clResearch 0x0044dba0
    uword pad;
    udword lab;                             // +0x2FC the research slot
    udword tech;                            // +0x300
    udword mode;                            // +0x304 CAT_RESEARCH_START / CAT_RESEARCH_STOP (cat_processResearchToDo)
} CatResearchCommand;
#define CAT_RESEARCH_START  0
#define CAT_RESEARCH_STOP   1

// CAT: 0x330 bytes in cataclysm.exe (every creator allocates 0x330 and zeroes 0xCC dwords; CET "Size is now 0x330").
//      HW1's fields keep their order; Cataclysm's move path (0x108 bytes) and its saved copy, the formation's
//      force-field level (FormationCommand +0x58) and the parameters of orders 14-16 are new. Offsets from the
//      exe's users (ILP32 asserts below, tools/m2_layout32.sh).
typedef struct CommandToDo
{
    Node todonode;                          // +0x000
    SelectCommand *selection;               // +0x010
    CommandOrder ordertype;                 // +0x014
    MoveCommand move;                       // +0x018
    bool32 catSavedMoveValid;               // +0x120 CAT: a looping/patrol move interrupted by an attack (clAttack 0x00448950)
    MoveCommand catSavedMove;               // +0x124 CAT: ... resumed when the attack ends (clProcess 0x0044c53e)
    AttackCommand *attack;                  // +0x22C
    FormationCommand formation;             // +0x230
    DockCommand dock;                       // +0x290
    LaunchShipCommand launchship;           // +0x29C
    CollectResourceCommand collect;         // +0x2A0
    ProtectCommand *protect;                // +0x2B4
    udword protectFlags;                    // +0x2B8
    SpecialCommand *specialtargets;         // +0x2BC
    MilitaryParadeCommand *militaryParade;  // +0x2C0
    BuildingShipCommand buildingship;       // +0x2C4
    udword catReserved2D8;                  // +0x2D8 CAT: no exe user found
    CatPointCommand catPoint;               // +0x2DC CAT: COMMAND_CAT_POINT
    CatMimicCommand catMimic;               // +0x2E8 CAT: COMMAND_CAT_MIMIC
    CatResearchCommand catResearch;         // +0x2F8 CAT: COMMAND_CAT_RESEARCH
    real32 tacticalupdatetime;                 //needed for optimal scanning of ship list in enemy detection for tactics
    real32 holdingStartTime;                //time that a command entered a holding pattern
    real32 hyperSpaceingTime;
    sdword hyperspaceState;
    sdword turnAroundTimes;
    vector holdingPatternVec;
    real32 pingUpdateTime;                  //last time this command was processed by pings
    void *updatedPing;                      //what ping it was last updated for
} CommandToDo;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(MoveCommand) == 0x108, "MoveCommand is 0x108 bytes in cataclysm.exe");
_Static_assert(sizeof(CommandToDo) == 0x330, "CommandToDo is 0x330 bytes in cataclysm.exe");
CAT_ASSERT_OFFSET(CommandToDo, move.curPoint, 0x11C);         // processMoveToDo 0x00443cb0
CAT_ASSERT_OFFSET(CommandToDo, catSavedMoveValid, 0x120);     // clAttack 0x00448ac5
CAT_ASSERT_OFFSET(CommandToDo, attack, 0x22C);                // FreeCommandToDoContents 0x004440f0
CAT_ASSERT_OFFSET(CommandToDo, formation.formationtype, 0x230);
CAT_ASSERT_OFFSET(CommandToDo, formation.catForceFieldLevel, 0x288);
CAT_ASSERT_OFFSET(CommandToDo, formation.formationSpecificInfo, 0x28C);
CAT_ASSERT_OFFSET(CommandToDo, dock, 0x290);
CAT_ASSERT_OFFSET(CommandToDo, protect, 0x2B4);
CAT_ASSERT_OFFSET(CommandToDo, specialtargets, 0x2BC);        // FreeLastOrder 0x00444490
CAT_ASSERT_OFFSET(CommandToDo, militaryParade, 0x2C0);
CAT_ASSERT_OFFSET(CommandToDo, catPoint, 0x2DC);              // cat_clPointOrder 0x0044dd70
CAT_ASSERT_OFFSET(CommandToDo, catMimic, 0x2E8);              // cat_clMimic 0x0044da80
CAT_ASSERT_OFFSET(CommandToDo, catResearch, 0x2F8);           // cat_clResearch 0x0044dba0
CAT_ASSERT_OFFSET(CommandToDo, tacticalupdatetime, 0x308);
CAT_ASSERT_OFFSET(CommandToDo, holdingStartTime, 0x30C);      // processHoldingPattern 0x0044b790
CAT_ASSERT_OFFSET(CommandToDo, hyperSpaceingTime, 0x310);     // processMpHyperspaceingToDo 0x004436a0
CAT_ASSERT_OFFSET(CommandToDo, hyperspaceState, 0x314);
CAT_ASSERT_OFFSET(CommandToDo, holdingPatternVec, 0x31C);
#endif

typedef struct CommandLayer
{
    LinkedList todolist;
} CommandLayer;

// CAT: Cataclysm has no Ship.formationcommand (no exe code reads or writes Ship+0x36C): a ship's formation is its
//      command when that carries COMMAND_MASK_FORMATION (getShipAndItsFormation 0x0044d240, RemoveShipFromAttacking
//      0x004477c0, aishipFlyToPointAvoidingObjsFunc 0x00414610 ...)
static inline struct CommandToDo *cat_shipFormationCommand(struct Ship *ship)
{
    struct CommandToDo *command = ship->command;
    return ((command != NULL) && (command->ordertype.attributes & COMMAND_MASK_FORMATION)) ? command : NULL;
}

/*=============================================================================
    Macros:
=============================================================================*/

#define sizeofAttackCommand sizeofSelectCommand
#define sizeofProtectCommand sizeofSelectCommand

// any variables you want to reset in CommandToDo upon initialization, put here
#define InitCommandToDo(command) memset(command,0,sizeof(CommandToDo))

/*=============================================================================
    Data:
=============================================================================*/
#if CL_TEXTFEEDBACK
extern bool32 enableTextFeedback;
#endif

/*=============================================================================
    Functions:
=============================================================================*/

// Initializing, Closing, Processing
void clInit(CommandLayer *comlayer);
void clClose(CommandLayer *comlayer);
void clReset(CommandLayer *comlayer);
void clProcess(CommandLayer *comlayer);
void clPostProcess(CommandLayer *comlayer);

void clChecksum(void);

// Ship Command Layer
// CAT: clMove 0x00446d90 takes a waypoint path: pathType (CAT_MOVEPATH_*), the start point, numPoints points
void clMove(CommandLayer *comlayer,SelectCommand *selectcom,sdword pathType,vector from,sdword numPoints,vector *points);
// CAT: new (0x00445e80): the per-ship offsets CalculateMoveToPoints would add
void cat_clCalculateMoveToOffsets(SelectCommand *selection,vector from,vector to,vector *outOffsets);
CommandToDo *clAttackThese(CommandLayer *comlayer,SelectCommand *selectcom,AttackCommand *attackcom);
void clAttack(CommandLayer *comlayer,SelectCommand *selectcom,AttackCommand *attackcom);
void clPassiveAttack(CommandLayer *comlayer,SelectCommand *selectcom,AttackCommand *attackcom);
// CAT: clFormation 0x00484c80 takes the force-field level as a fourth argument
void clFormation(CommandLayer *comlayer,SelectCommand *selectcom,TypeOfFormation formation,sdword catForceFieldLevel);
sdword clDock(CommandLayer *comlayer,SelectCommand *selectcom,DockType dockType,ShipPtr dockwith);
Ship *clCreateShip(CommandLayer *comlayer,ShipType shipType,ShipRace shipRace,uword playerIndex,ShipPtr creator);
void clBuildShip(CommandLayer *comlayer,ShipType shipType,ShipRace shipRace,uword playerIndex,ShipPtr creator);

// CAT: external build (Cataclysm's big ships, statinfo->externalBuild). univCreateShip 0x00542320 creates such a ship
//      with health 0, catBuildProgress 0 and SPECIAL_2_CatBeingBuilt when its built argument has bit 0 set; the
//      construction manager then drives it through cat_clExternalBuildUpdate (cmDeterministicBuildProcess 0x004592a0,
//      cmDeterministicBuildCancel 0x00459040). CAT-PROVISIONAL: that driver is ConsMgr.c's (M2-R6 ConsMgr) and
//      not ported, and the tree's ConsMgr still builds every type inside the factory and creates it at the end
//      (clCreateShip, built 1), so the univCreateShip branch stays off until the driver lands: with it on, an
//      externally built type would be created at health 0 and never finish.
#define CAT_EXTERNAL_BUILD_DRIVER   0
#define CAT_EXTBUILD_DONE           0   // the build finished (cat_clExternalBuildUpdate mode 0)
#define CAT_EXTBUILD_CANCEL         1   // the build was cancelled: the ship is deleted (mode 1)
#define CAT_EXTBUILD_PROGRESS       2   // progress update (mode 2)
void cat_clStartExternalBuildTimer(CommandLayer *comlayer,SelectCommand *selectcom);
void cat_clExternalBuildUpdate(CommandLayer *comlayer,uword playerIndex,Ship *creator,Ship *ship,real32 progress,sdword mode);
bool32 cat_shipIsLiveWorker(Ship *ship);   // CAT: 0x005c8e00 (Ships/Worker.c)
void clCollectResource(CommandLayer *comlayer,SelectCommand *selectcom,ResourcePtr resource);
Resource *cat_clHarvestNearestResource(CommandLayer *comlayer,SelectCommand *selectcom,bool32 fromUI,bool32 firstOfSelection);   // CAT: 0x00449790 (M2-R5 dock)
CommandToDo *clProtect(CommandLayer *comlayer,SelectCommand *selectcom,ProtectCommand *protectcom);
void clSpecial(CommandLayer *comlayer,SelectCommand *selectcom,SpecialCommand *targets);
void clHalt(CommandLayer *comlayer,SelectCommand *selectcom);
void clHaltThese(CommandLayer *comlayer,SelectCommand *selectcom);
void clScuttle(CommandLayer *comlayer,SelectCommand *selectcom);
void clAutoLaunch(udword OnOff,udword playerIndex);
void clSetAlliance(udword AllianceType, uword curalliance, uword newalliance);
void clLaunchMultipleShips(CommandLayer *comlayer,SelectCommand *selectcom,ShipPtr launchFrom);
void clSetTactics(CommandLayer *comlayer,SelectCommand *selectcom,TacticsType tacticstype);
void clSetKamikaze(CommandLayer *comlayer,SelectCommand *selectcom);
void clSetMilitaryParade(CommandLayer *comlayer,SelectCommand *selectcom);
void clRUTransfer(CommandLayer *comlayer, sdword toIndex, sdword fromIndex, sdword resourceUnits,ubyte flags);
void clMpHyperspace(CommandLayer *comlayer,SelectCommand *selectcom,vector from,vector to);
void clSetResearch(udword type, udword playernum, udword labnum, udword tech);
void clDeterministicBuild(udword command, CommandLayer* comlayer, sdword numShips, ShipType shipType, ShipRace shipRace, uword playerIndex, ShipPtr creator);

// CAT: the link orders (ShipLink.h): the link block of a linkable ship (NULL for other types) and the link cleanup
struct CatLinkInfo *cat_shipGetLinkInfo(Ship *ship);
void cat_clLinkCleanup(Ship *ship);                                     // CAT: 0x00444060

// CAT (M2-R5 command-tactics): Cataclysm's orders and command-layer helpers (CommandLayer.c, 0x00444060 .. 0x0044e150)
struct Ship *cat_clFindLinkPartner(SelectCommand *selection,struct Ship *ship);                                 // 0x0044d2e0
void cat_clLink(CommandLayer *comlayer,SelectCommand *selectcom);                                               // 0x0044d3b0
void cat_clUnlink(CommandLayer *comlayer,SelectCommand *selectcom);                                             // 0x0044d7a0
void cat_clMimic(CommandLayer *comlayer,SelectCommand *selectcom,udword mode,udword shipType,udword player,udword index);   // 0x0044da80
void cat_clResearch(CommandLayer *comlayer,uword playerIndex,udword lab,udword tech,udword mode);              // 0x0044dba0
void cat_processResearchToDo(struct CommandToDo *command);                                                      // 0x0044dc20
void cat_clPointOrder(CommandLayer *comlayer,SelectCommand *selectcom,vector *point);                          // 0x0044dd70
void cat_clUpgradeShips(CommandLayer *comlayer,SelectCommand *selectcom,bool32 cancel);                        // 0x0044de80
void cat_clSpecialTargetSpeech(SelectCommand *selectcom);                                                       // 0x004490b0
void cat_clSpecialTarget(CommandLayer *comlayer,SelectCommand *selectcom,SpecialCommand *targets,bool32 flag); // 0x00449190
void cat_clUnleech(CommandLayer *comlayer,SelectCommand *selectcom);                                            // 0x00449770
void cat_clSetLeechAction(CommandLayer *comlayer,SelectCommand *selectcom,ubyte mode);                         // 0x00449960
void cat_clSetModuleSlot(CommandLayer *comlayer,SelectCommand *selectcom,sdword moduleType,sdword count);      // 0x0044e0e0
void cat_clSetBeastSystem(CommandLayer *comlayer,struct Ship *ship,sdword system);                             // 0x0044e120
udword cat_clModuleTypeToMiscBits(sdword moduleType);                                                           // 0x0044e130
udword cat_clModuleSlotToMiscBits(sdword slot);                                                                 // 0x0044e150
AttackCommand *cat_clAddAttachedModulesToTargets(SelectCommand *selection,AttackCommand *targets);              // 0x004482f0

// does ship have to launch
bool32 ShipHasToLaunch(Ship *InsideShip, Ship *ship);

// launch functions
void LaunchAllInternalShipsOfPlayerThatMustBeLaunched(struct Player *player);
sdword LaunchAllInternalShipsOfPlayer(struct Player *player, udword carriermask);

// checks if can ChangeOrderToPassiveAttack
bool32 ShipsInPassiveAttackRange(SelectCommand *selection,AttackCommand *attack);

// changes order to passive attack
// CAT: the callers test the order first (clPassiveAttack 0x00448ce0); ChangeOrderToPassiveAttack frees what it replaces
void ChangeOrderToPassiveAttack(CommandToDo *alreadycommand,AttackCommand *attackcom);

// changes order to attack
void ChangeOrderToAttack(CommandToDo *alreadycommand,AttackCommand *attackcom,sdword catReason);   // CAT: reason code (0x00448460)

// changes order to move
void ChangeOrderToMove(CommandToDo *alreadycommand,sdword pathType,vector from,sdword numPoints,vector *points);   // CAT: waypoint path (0x004460d0)

// Returns allocated command containing the ship
CommandToDo *getShipAndItsCommand(CommandLayer *comlayer,ShipPtr ship);
SelectCommand *getShipAndItsCommandSelection(CommandLayer *comlayer,ShipPtr ship,bool32 *parade);

// Returns allocated command containing the ship and its formation
CommandToDo *getShipAndItsFormationCommand(CommandLayer *comlayer,ShipPtr ship);

// Returns allocated selectcommand containing the ship and its formation
SelectCommand *getShipAndItsFormation(CommandLayer *comlayer,ShipPtr ship);

// returns TRUE if searchfor is in comlayer
bool32 CommandInCommandLayer(CommandLayer *comlayer,CommandToDo *searchfor);

// Call this function to see if this selection is already in formation
TypeOfFormation clSelectionAlreadyInFormation(CommandLayer *comlayer,SelectCommand *selectcom);

// Call this function to update ship command layer when ship dies
void clShipDied(CommandLayer *comlayer,ShipPtr deadship);

// Call this function to update ship command layer when resource dies
void clResourceDied(CommandLayer *comlayer,ResourcePtr resource);

// Call this function to update ship command layer when derelict dies
void clDerelictDied(CommandLayer *comlayer,DerelictPtr derelict);

// Call this function to update ship command layer when missile dies
void clMissileDied(CommandLayer *comlayer,MissilePtr missile);

// Tells if selectcom is already doing something in the command layer
CommandToDo *IsSelectionAlreadyDoingSomething(CommandLayer *comlayer,SelectCommand *selectcom);

// Removes ships in selectcom from doing stuff in the command layer
// CAT: third argument, a reason code the exe passes but never reads (0x00444810)
void RemoveShipsFromDoingStuff(CommandLayer *comlayer,SelectCommand *selectcom,sdword catReason);
// CAT: second argument, a code the exe passes but never reads (0x004440f0)
void FreeCommandToDoContents(struct CommandToDo *command,sdword catReason);

// values for removeFlag for RemoveShipFromBeingTargeted
#define REMOVE_PROTECT          1
#define REMOVE_CLOAKED          2
#define REMOVE_DISABLED         4
#define REMOVE_DISAPPEARED      8
#define REMOVE_HYPERSPACING     16
// CAT: new: a salvaged ship, removed only from the commands of the player given as the fourth argument
//      (cat_shipHasJustBeenSalvaged 0x0044b450 passes 0x20; RemoveShipFromBeingTargeted 0x004453c0)
#define REMOVE_CAT_SALVAGED     32

// Removes ship from being targeted in the command layer
// CAT: fourth argument catPlayer, used with REMOVE_CAT_SALVAGED; every other caller passes NULL (0x004453c0)
void RemoveShipFromBeingTargeted(CommandLayer *comlayer,ShipPtr shiptoremove,udword removeFlag,struct Player *catPlayer);

// CAT: a ship was salvaged: drop it from the commands of player (cat_shipHasJustBeenSalvaged 0x0044b450)
void cat_shipHasJustBeenSalvaged(Ship *ship,struct Player *player);

// cleans up game when a ship goes out of view..
void shipHasJustCloaked(Ship *ship);

// cleans up game when a ship becomes disabled
void shipHasJustBeenDisabled(Ship *ship,bool32 catKeepInCamera);   // CAT: second argument (0x00445360)

// when ship just disappeared (SOF_Hide) e.g. univRemoveShipFromOutside
void shipHasJustDisappeared(Ship *ship);

// removes target info from command regarding shiptoremove
void RemoveShipReferencesFromExtraAttackInfo(Ship *shiptoremove,CommandToDo *todo);
void RemoveAttackTargetFromExtraAttackInfo(SpaceObjRotImpTarg *targettoremove,CommandToDo *todo);
void removeShipsFromDockingWithThisShip(Ship *ship);

void RemoveShipFromAttacking(Ship *ship);

// Initializes ship's AI state variables
void InitShipAI(Ship *ship,bool32 fresh);

// Initializes selection ships AI state variables
void InitShipsForAI(SelectCommand *selection,bool32 fresh);

// clears any protecting in command
void ClearProtecting(CommandToDo *command);

// clears any passive attack command
void ClearPassiveAttacking(CommandToDo *command);

// frees last order of command, so new order can be put in
void FreeLastOrder(CommandToDo *command);

// adds a ship to a formation group
void AddShipToFormationGroup(ShipPtr ship,CommandToDo *group);

// adds a ship to a command group
void AddShipToGroup(ShipPtr ship,CommandToDo *group);

// Adds a ship to the proper group around aroundShip
void GroupShip(CommandLayer *comlayer,ShipPtr ship,ShipPtr aroundShip);

// groups ship into military parade around aroundShip
void GroupShipIntoMilitaryParade(CommandLayer *comlayer,ShipPtr ship,ShipPtr aroundShip);

// sets ships to correct position according to command layer
void clPresetShipsToPosition(CommandLayer *comlayer);

// cleans up formation ship stuff
void RemoveShipFromFormation(Ship *ship);

void fixFlipTurnSelection(SelectCommand *selection,SelectCommand *global);

CommandToDo *GetMilitaryGroupAroundShip(CommandLayer *comlayer,Ship *aroundShip);

// call whenever creating a new command in command layer or ships changing their command
void PrepareShipsForCommand(CommandToDo *command,bool32 rowClear);
void PrepareOneShipForCommand(Ship *ship,CommandToDo *command,bool32 rowClear);

// call whenever removing command
void RemoveShipsFromCommand(CommandToDo *command);
// call whenever removing a ship from command (e.g. removing ship from command->selection)
void RemoveShipFromCommand(Ship *ship);

//returns cost of selSelected to hyperspace to distance
real32 hyperspaceCost(real32 distance,SelectCommand *selection);

void FillInCarrierMothershipInfo(struct Player *player,Ship **mothership,Ship *carrierX[]);

//cancels all current players launching ship(single player game only)
void clCancelAllLaunchOrdersFromPlayer(struct Player *player);

// tells ship to stay nearby protectThisShip - actual logic for attack if protectThisShip being attacked not in this routine
void protectShip(Ship *ship,Ship *protectThisShip,bool32 passiveAttacked);

/*=============================================================================
    Data:
=============================================================================*/

// CAT (M2-R5 command-tactics): the scripted command self-test (/catCommandTest <ticks>, harness mode command)
void catCommandSelfTestSet(char *string);
bool32 catCommandSelfTestRequested(void);
void catCommandSelfTestTick(void);

#endif
