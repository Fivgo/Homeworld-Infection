/*=============================================================================
    Name    : Shipselect.h
    Purpose : Includes type definitions for selecting ships

    Created 10/11/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___SHIPSELECT_H
#define ___SHIPSELECT_H

#include "SpaceObj.h"
#include "ShipDisable.h"   // CAT: cat_shipSetDisableReason moved there (M2-R4 ships-b)

/*=============================================================================
    Defines:
=============================================================================*/

#define COMMAND_MAX_SHIPS 1000

/*=============================================================================
    Types:
=============================================================================*/

// select ships
typedef struct
{
    sdword numShips;
    real32 timeLastStatus;
    ShipPtr ShipPtr[1];
} SelectCommand;

typedef struct
{
    sdword numShips;
    real32 timeLastStatus;
    ShipPtr ShipPtr[6];
} SelectCommandMax6;

typedef struct
{
    sdword numShips;
    real32 timeLastStatus;
    ShipPtr ShipPtr[COMMAND_MAX_SHIPS];
} MaxSelection;

// select ships or targets
typedef struct
{
    sdword numTargets;
    real32 timeLastStatus;
    TargetPtr TargetPtr[1];
} SelectAnyCommand;

typedef struct
{
    sdword numTargets;
    real32 timeLastStatus;
    TargetPtr TargetPtr[6];
} SelectAnyCommandMax6;

typedef struct
{
    sdword numTargets;
    real32 timeLastStatus;
    TargetPtr TargetPtr[COMMAND_MAX_SHIPS];
} MaxAnySelection;

typedef struct
{
    sdword numResources;
    real32 timeLastStatus;
    ResourcePtr ResourcePtr[1];
} ResourceSelection;

typedef struct
{
    sdword numDerelicts;
    real32 timeLastStatus;
    DerelictPtr DerelictPtr[1];
} DerelictSelection;

typedef struct
{
    sdword numBullets;
    real32 timeLastStatus;
    BulletPtr BulletPtr[1];
} BulletSelection;

typedef struct
{
    sdword numMissiles;
    real32 timeLastStatus;
    MissilePtr MissilePtr[1];
} MissileSelection;

typedef struct
{
    sdword numSpaceObjs;
    real32 timeLastStatus;
    SpaceObjPtr SpaceObjPtr[1];
} SpaceObjSelection;

typedef struct
{
    sdword numShips;
    real32 timeLastStatus;
    ShipStaticInfo *ShipStaticPtr[1];
} SelectCommandStatic;

#define GROWSELECT_INITIALBATCH     16
#define GROWSELECT_ADDBATCH         8

typedef struct
{
    sdword maxNumShips;
    SelectCommand *selection;
} GrowSelection;

/*=============================================================================
    Macros:
=============================================================================*/

#define sizeofSelectCommand(n) (sizeof(SelectCommand) + sizeof(ShipPtr)*((n)-1))

/*=============================================================================
    Functions:
=============================================================================*/

// Adds a ship to a growable selection of ships.
void growSelectAddShip(GrowSelection *growSelect,Ship *ship);

// Returns TRUE if obj was removed
bool32 growSelectRemoveShip(GrowSelection *growSelect,Ship *ship);

// Removes ship at index index from growSelect
void growSelectRemoveShipIndex(GrowSelection *growSelect,sdword index);

// Initializes a growable selection of ships
void growSelectInit(GrowSelection *growSelect);

// Closes a growable selection of ships
void growSelectClose(GrowSelection *growSelect);

// Resets a growable selection (sets numShips to 0)
void growSelectReset(GrowSelection *growSelect);

// Add a ship to a growable selection without duplicating an existing ship pointer
void growSelectAddShipNoDuplication(GrowSelection *growSelect, Ship *ship);

// Adds a SpaceObj to selection before index
void AddSpaceObjToSelectionBeforeIndex(SpaceObj *obj,SpaceObjSelection *selection,sdword index);

// Adds a SpaceObj to selection after index
void AddSpaceObjToSelectionAfterIndex(SpaceObj *obj,SpaceObjSelection *selection,sdword index);

// Returns TRUE if obj was removed
bool32 RemoveSpaceObjFromSelectionPreserveOrder(SpaceObjSelection *selection,SpaceObj *obj);

// Returns TRUE if ship is in selection
bool32 ShipInSelection(SelectCommand *selection,Ship *ship);

// Returns number if ShipType "type"  in selection
udword ShiptypeInSelection(SelectCommand *selection, ShipType type);

// Returns the first instance of the specified type in the selection
ShipPtr FindFirstInstanceOfShipType(SelectCommand *selection, ShipType type);

// Does not delete entry - just removes reference to it.
// Returns TRUE if any ship was removed
bool32 clRemoveTargetFromSelection(SelectAnyCommand *selection,TargetPtr removeTargetPtr);

#define clRemoveShipFromSelection(sel,ship) clRemoveTargetFromSelection((SelectAnyCommand *)(sel),(TargetPtr)ship)

// returns true if any ships in theseShips are in selection
bool32 AnyOfTheseShipsAreInSelection(SelectCommand *theseShips,SelectCommand *selection);

// returns true if all of these ships are in selection
bool32 TheseShipsAreInSelection(SelectCommand *theseShips,SelectCommand *selection);

// returns true if selections 1 and 2 are equivalent (ignores order)
bool32 SelectionsAreEquivalent(SelectCommand *selection1,SelectCommand *selection2);

// returns true if selections 1 and 2 are equivalent (including order)
bool32 SelectionsAreTotallyEquivalent(SelectCommand *selection1,SelectCommand *selection2);

// Makes sure that ships in selection do not include theseships
void MakeShipsNotIncludeTheseShips(SelectCommand *selection,SelectCommand *theseships);

// Makes sure that the targets in selection are not missiles.
void MakeTargetsNotIncludeMissiles(SelectAnyCommand *selection);

// Returns true if ship can harvest
bool32 ShipCanHarvest(ShipStaticInfo *shipstatic);

// Makes targets all be within range of comparewith, but don't include comparewith
void MakeTargetsOnlyBeWithinRangeAndNotIncludeMe(SelectAnyCommand *selection,SpaceObjRotImpTarg *comparewith,real32 range);

// Makes sure that targets in selection are ships and enemies
void MakeTargetsOnlyEnemyShips(SelectAnyCommand *selection,struct Player *curplayer);

// Same as MakeTargetsOnlyEnemyShips except includes stuff like hyperspace gates
void MakeTargetsOnlyNonForceAttackTargets(SelectAnyCommand *selection,struct Player *curplayer);

// Makes targets only be salvageable ships/derelicts
void MakeTargetsSalvageable(SelectAnyCommand *selection,struct Player *curplayer);

typedef bool32 (*ShipConstraintsCB)(Ship *ship);
// Makes sure that all ships in selection follow shipConstraintsCB
void MakeShipsOnlyFollowConstraints(SelectCommand *selection,ShipConstraintsCB shipConstraintsCB);
// Returns TRUE if any ships in selection follow shipConstraintsCB
bool32 DoAnyShipsFollowConstraints(SelectCommand *selection,ShipConstraintsCB shipConstraintsCB);
// Returns TRUE if all ships in selection follow shipConstraintsCB
bool32 DoAllShipsFollowConstraints(SelectCommand *selection,ShipConstraintsCB shipConstraintsCB);

// Makes sure that ships in selection are friendly ships (are curplayer's ships)
void MakeShipsFriendlyShips(SelectCommand *selection,struct Player *curplayer);

// Makes sure that ships in the selection are friendly, i.e. allies and curplayer's ships
void MakeShipsFriendlyAndAlliesShips(SelectCommand *selection,struct Player *curplayer);

// Makes sure that ships are harvest capable
sdword MakeShipsHarvestCapable(SelectCommand *dest, SelectCommand *source, bool32 workersOnly);   // CAT: 3rd argument

// Makes sure that ships are attack capable
sdword MakeShipsAttackCapable(SelectCommand *dest, SelectCommand *source, bool32 skipDisabled);    // CAT: 3rd argument

// Returns TRUE if all ships are attack capable
bool32 AreAllShipsAttackCapable(SelectCommand *selection);

// returns TRUE if any ships are attack capable
bool32 AreAnyShipsAttackCapable(SelectCommand *selection);

// Returns TRUE if ship is attack capable
bool32 isShipAttackCapable(Ship *ship);

// Returns TRUE if all ships are attack capable
bool32 AreAllShipsPassiveAttackCapable(SelectCommand *selection);

// returns TRUE if any ships are attack capable
bool32 AreAnyShipsPassiveAttackCapable(SelectCommand *selection);

// Returns TRUE if ship is attack capable
bool32 isShipPassiveAttackCapable(Ship *ship);

// Makes sure that ships are guard capable
void MakeShipsGuardCapable(SelectCommand *selection, bool32 skipDisabled);      // CAT: 2nd argument

// Makes sure the ships have a special activate function
void MakeShipsSpecialActivateCapable(SelectCommand *selection, bool32 skipDisabled);    // CAT: 2nd argument

// Makes sure the ships have special targeting capability
void MakeShipsSpecialTargetCapable(SelectCommand *selection, bool32 bFriendlies, bool32 skipDisabled);   // CAT: 3rd argument

// Makes sure that ships are mobile
void MakeShipsMobile(SelectCommand *selection, bool32 skipDisabled);    // CAT: 2nd argument
bool32 AreShipsMobile(SelectCommand *selection);

// Makes sure ships aren't capital ships
void MakeShipsNonCapital(SelectCommand *selection);

// Makes sure ships are only capital ships
void MakeShipsOnlyCapital(SelectCommand *selection);

// Makes sure selection don't have cloaked enemy ships
void MakeSelectionNotHaveCloakedEnemyShips(SelectCommand *selection, struct Player *curplayer);

// Makes sure selection doesn't have non-visible cloaked enemy ships
// Note: takes into account proximity sensors detecting cloaked ships
void MakeSelectionNotHaveNonVisibleEnemyShips(SelectCommand *selection, struct Player *curplayer);

// Adds slaves to a selection list if the master is in it

// Removes Research Ships from Selection
void MakeSelectionNonResearchShips(SelectCommand *selection);

// removes ships that can't dock from the selection
void makeShipsDockCapable(SelectCommand *selection);
void makeShipsRetireable(SelectCommand *selection);

// rmakes list contain no docking ships <ships in docking cones>
void makeShipsControllable(SelectCommand *selection,sdword newCommand);

// makes list contains ships that can gointo formation..probe is removed
void makeShipsFormationCapable(SelectCommand *selection);

// removes the player's mothership from selection
void makeShipsNotIncludeSinglePlayerMotherships(SelectCommand *selection);

//Removes non Kamikazeable ships
bool32 MakeSelectionKamikazeCapable(SelectCommand *selection);

//remove any ships not allowed to hyperspace
void makeSelectionHyperspaceCapable(SelectCommand *selection);

//removes certain ships from an attack selection so that guarding ships
//isn't wacky
void makeShipsNotHaveNonCombatShipsForGuardAttack(SelectCommand *selection);

// convert a LinkedList of ships into the equivalent SelectCommand
SelectCommand * shipLinkedListAsSelectCommand(LinkedList *list, char *label);

// Selects all of the current player's non-hyperspacing ships
SelectCommand *selectAllCurrentPlayersNonHyperspacingShips(void);

// Selects all of the current player's hyperspacing ships
SelectCommand *selectAllCurrentPlayersHyperspacingShips(void);

// Selects all of a player's ships
SelectCommand *selectAllPlayersShips(struct Player *player);

// returns a new selection consisting of the ships in both selection 1 and selection 2
// Note: the new selection created with a memAlloc and therefore must be freed later
SelectCommand *selectSelectionIntersection(SelectCommand *selection1, SelectCommand *selection2);

//select all ships with the canSingleClickSpecialActivate flag set
sdword MakeShipsSingleClickSpecialCapable(SelectCommand *dest, SelectCommand *source);

//select all visible ships
sdword MakeShipsNotHidden(SelectCommand *dest, SelectCommand *source);

sdword MothershipOrCarrierIndexInSelection(SelectCommand *selection);

// Merge two selections
#define NO_DEALLOC      0   //no selections are deallocated
#define DEALLOC1        1   //the first selection is deallocated
#define DEALLOC2        2   //the second selection is deallocated
#define DEALLOC_BOTH    3   //both selections are deallocated
SelectCommand *selectMergeTwoSelections(SelectCommand *selection1, SelectCommand *selection2, udword dealloc);

// Duplicate a selection
SelectCommand *selectMemDupSelection(SelectCommand *selection, char *str, udword memflag);
#define selectDupSelection(sel)     selectMemDupSelection(sel, "selDup", 0)

// CAT: HW1 also excluded its cryo-tray and probe types, which Cataclysm does not have. The exe's single-player hyperspace test is
//      cat_spShipCanHyperspace 0x00517650; the macro's users (SinglePlayer.c) are stage 6.
#define ShipCanHyperspace(ship) (isCapitalShip(ship))

/*=============================================================================
    CAT: Cataclysm-only selection functions (exe ShipSelect.c block 0x00511870-0x00514210)
=============================================================================*/
void cat_selSelectionAddShip(MaxSelection *dest, Ship *ship);
void cat_selSelectionCopy(MaxSelection *dest, MaxSelection *src);
void cat_growSelectNullShip(GrowSelection *grow, Ship *ship);
void cat_selMakeShipsScuttleable(SelectCommand *selection, bool32 check);
void cat_selMakeShipsNotLinkedACVs(SelectCommand *selection);
void cat_selMakeShipsSameOwnerAsFirst(SelectCommand *selection);
void cat_selMakeShipsHaltable(SelectCommand *selection, bool32 check);
void cat_selMakeShipsProtectCapable(SelectCommand *selection, bool32 check);
void cat_selMakeShipsMobileNotBusy(SelectCommand *selection);
void cat_selMakeShipsHaveAbilityUpgrade(SelectCommand *selection);
void cat_selMakeShipsLeechCapable(SelectCommand *selection, bool32 check);
void cat_selMakeShipsRammingFrigates(SelectCommand *selection, bool32 check);
void cat_selMakeShipsRepairCapable(SelectCommand *selection);
void cat_selMakeShipsHaveUpgrade(SelectCommand *selection, udword flags);
void cat_selMakeShipsAcolyteLinkFire(SelectCommand *selection);
void cat_selMakeShipsNotBigShips(SelectCommand *selection);
void cat_selMakeShipsNotBusy(SelectCommand *selection);
void cat_selMakeShipsLeeching(SelectCommand *selection);
void cat_selMakeTargetsAttackable(SelectCommand *targets, bool32 force);
void cat_selMakeTargetsBeastAttackable(SelectCommand *targets);
void cat_selMakeShipsDockCapableFor(SelectCommand *selection, ShipType dockTargetType, bool32 check);
bool32 cat_shipClassCanUseSlipgate(ShipClass shipclass, ShipType slipgateType);
bool32 cat_shipCanMove(Ship *ship, bool32 check);
bool32 cat_shipCanDock(Ship *ship, bool32 check);
bool32 cat_shipCanBeHalted(Ship *ship, bool32 check);
bool32 cat_shipCanSetTactics(Ship *ship, bool32 check);
real32 cat_shipGetFOWRadius(Ship *ship, sdword researchSource);      // CAT: 0x00514a30 (M2-R5 command-tactics)
bool32 cat_shipCanCollectResources(Ship *ship, bool32 check);
bool32 cat_shipCanLeech(Ship *ship, bool32 checkAttached);
bool32 cat_shipCanUnleech(Ship *ship);
bool32 cat_shipCanRepair(Ship *ship);
bool32 cat_shipCanSalvage(Ship *ship);
real32 cat_shipGetSpeedMultiplier(Ship *ship);                      // CAT: 0x00514940 (M2-R4 combat)
bool32 cat_salvageTargetAllowed(Ship *target);                      // CAT: 0x00513920 (M2-R4 combat)
bool32 cat_shipHasUpgrade(Ship *ship, udword flag);
real32 cat_weaponDamageVsArmour(sdword damageType, sdword armourLevel);   // CAT: 0x00514460 (M2-R4 combat)
real32 cat_gunStaticBaseDamage(GunStatic *gunstatic);                     // CAT: 0x00514600 (M2-R4 combat)
real32 cat_shipGetDamageMultiplier(Ship *ship);                           // CAT: 0x005148f0 (M2-R4 combat)
void cat_shipUpdateExperienceLevel(Ship *ship);                           // CAT: 0x005147d0 (M2-R5 layout-dock)
extern sdword catOpExperienceMessages;                                    // CAT: the OP_ExperienceMessages option 0x00a5e248
void cat_shipUpgradesInit(Ship *ship);                                    // CAT: 0x005140a0 (M2-R4 combat)
void cat_shipApplyUpgrades(Ship *ship);                                   // CAT: 0x00514490 (M2-R4 combat)
void cat_playerUpgradesReset(struct Player *player);                             // CAT: 0x00514100 (M2-R4 rest-oracle)
void cat_playerUpdateUpgradesFromResearch(struct Player *player);                // CAT: 0x00514140 (M2-R4 rest-oracle)
void cat_shipTypeGetUpgradeTechs(ShipType type, sdword *techs);           // CAT: 0x005146f0 (M2-R4 rest-oracle): a CatTechSet
void cat_shipInitResearchUpgrades(Ship *ship);                             // CAT: 0x00514af0 (M2-R4 rest-oracle)
real32 cat_upgradeFlagsBaseTime(udword flags);
// CAT (M2-R5 layout-dock): the upgrade queries of the upgrade UI, cat_clUpgradeShips and cat_rmAcquireShipTechs
bool32 cat_shipCanUpgradeArmour(Ship *ship);                               // CAT: 0x00514280
udword cat_shipGetPendingUpgradeFlags(Ship *ship);                         // CAT: 0x005142b0
bool32 cat_shipGetPendingWeaponUpgrades(Ship *ship, bool32 *out);          // CAT: 0x005142e0 (out: NUM_WEAPON_TYPES flags)
bool32 cat_shipIsUpgradable(Ship *ship);                                   // CAT: 0x005143c0
udword cat_shipTypeUpgradeMask(ShipType type);                             // CAT: 0x00514480
real32 cat_shipGetUpgradeTime(Ship *ship);                                 // CAT: 0x00514540
void cat_shipGetUpgradeTechs(Ship *ship, sdword *techs);                   // CAT: 0x00514620 (techs: a CatTechSet)
real32 cat_shipUpgradeFlagsTime(Ship *ship, udword flags);

// CAT: ship-state queries the selection code calls; their exe homes are other subsystems (see ShipSelect.c)
bool32 cat_shipIsCrystal(Ship *ship);
bool32 cat_shipIsCloakedFully(Ship *ship);
bool32 cat_shipIsUntargetable(Ship *ship);
bool32 cat_shipIsSentinelInField(Ship *ship);
bool32 cat_shipIsSentinelFieldActive(Ship *ship);
bool32 cat_leechIsLeeching(Ship *ship);
sdword cat_acvLinkState(Ship *ship);
bool32 cat_acolyteCanFireLinkedWeapon(Ship *ship);
bool32 cat_shipIsAcolyteOrDerelict(Ship *ship);                      // CAT: 0x005ad360 (M2-R4 combat)
bool32 cat_salvageDepotHoldsCrystal(Ship *ship);
bool32 cat_mothershipBigGunActive(Ship *ship);
bool32 cat_shipDetectedByPlayer(Ship *ship, sdword playerIndex);
sdword cat_shipStealthTargetState(Ship *ship, struct Player *player);   // CAT: 0x0053ff30 (M2-R4 combat)
bool32 cat_shipIsAcolyte(Ship *ship);
void cat_shipStealthDetectedSet(Ship *ship, sdword playerIndex, bool32 detected, bool32 silent);
void cat_shipStealthInit(Ship *ship);                                // CAT: 0x0053fce0 (M2-R4 ships-b)
void cat_shipStealthDetectionUpdate(Ship *ship);                    // CAT: 0x0053fd90 (M2-R6 nis-fow-camera)
bool32 cat_shipIsMimicking(Ship *ship);
ShipType cat_mimicGetApparentShipType(Ship *ship);
real32 cat_mimicGetSelectRadius(Ship *ship);
bool32 cat_mimicAppearsAlliedTo(Ship *ship, struct Player *player);
bool32 cat_mimicAppearsAsResource(Ship *ship, struct Player *player);
bool32 cat_mimicAppearsAsDerelict(Ship *ship, struct Player *player);
bool32 cat_spShipCanHyperspace(Ship *ship);

// CAT (M2-R6 mainrgn-popup): what a selection can do and what it is as a target, 0x29 dwords (cat_selComputeTargetActions
//      0x00513a00 fills it for the selection and for the clicked targets; the popup menu, the right-click order
//      cat_mrReleaseRight 0x0056b0b0 and the cursor mouseSetCursorSetting 0x0056f3b0 read it). Names chosen here from
//      the tests that set each word.
typedef enum
{
    CAT_TA_Harvest = 0,             // a Worker that may harvest (class 7, harvestAmount > 0, not disabled)
    CAT_TA_Salvage,                 // upgrade 0x80000 (CAT_UPGRADE_Salvage)
    CAT_TA_Repair,                  // upgrade 0x40000 (CAT_UPGRADE_Repair), not a Processor
    CAT_TA_CanAttack,               // has an attack function, not a Sentinel in a field, not disabled
    CAT_TA_Enemy,                   // a target: an enemy ship (or a mine) that may be attacked
    CAT_TA_Ally,                    // a target: an allied ship (or a mimic that looks allied)
    CAT_TA_Resource,                // a target: a resource (or a mimic posing as one)
    CAT_TA_Salvageable,             // a target: may be salvaged (cat_salvageCheckTargetReasons)
    CAT_TA_SlipgateSmall,           // a target: dSlipgateSmall / Medium / Large
    CAT_TA_SlipgateMedium,
    CAT_TA_SlipgateLarge,
    CAT_TA_Hive,                    // upgrade 0x800 (the hive frigate's swarm)
    CAT_TA_HiveReady,               //  and the swarm is ready
    CAT_TA_EMP,                     // upgrade 0x200 (CAT_UPGRADE_ACVEMP)
    CAT_TA_Leech,                   // upgrade 0x40 (CAT_UPGRADE_Leech)
    CAT_TA_Leeching,                //  and leeching now
    CAT_TA_BigGun,                  // a mothership with the big-gun module
    CAT_TA_BigGunFiring,            //  and firing it
    CAT_TA_Link,                    // upgrade 0x8 (CAT_UPGRADE_Link)
    CAT_TA_Unlink,                  // else upgrade 0x10
    CAT_TA_BeastWeapon,             // cat_shipCanUseBeastWeapon
    CAT_TA_Ram,                     // a ramming frigate (sRammingFrigate / bRammingFrigate)
    CAT_TA_Mimic,                   // upgrade 0x20 (CAT_UPGRADE_Mimic)
    CAT_TA_MCV,                     //  and an sMCV
    CAT_TA_Mimicking,               //  and mimicking now
    CAT_TA_DreadnoughtCannon,       // upgrade 0x400
    CAT_TA_ForceField,              // upgrade 0x1000 / 0x2000 / 0x4000 (the protect formation)
    CAT_TA_InForceField,            //  and in a force-field formation now (the command's formation 9 or 10)
    CAT_TA_Cloak,                   // upgrade 0x20000, not cloaked
    CAT_TA_Decloak,                 // upgrade 0x20000, cloaked
    CAT_TA_Unused30,
    CAT_TA_AcolyteLinkFire,         // cat_acolyteCanFireLinkedWeapon
    CAT_TA_AllLeech,                // every ship of the selection can leech (32..39 start TRUE when there are ships)
    CAT_TA_AllEMP,
    CAT_TA_AllMimic,
    CAT_TA_AllHive,
    CAT_TA_AllRam,
    CAT_TA_AllBeastWeapon,
    CAT_TA_AllAcolyteLinkFire,
    CAT_TA_AllForceField,
    CAT_TA_NumberFlags              // 40; the leech action follows
} CatTargetActionIndex;

typedef struct CatTargetActions
{
    udword flag[CAT_TA_NumberFlags];    // +0x00
    ubyte leechAction;                  // +0xA0 the leech action all leeches share (cat_leechGetAction), else 0
    ubyte pad[3];
} CatTargetActions;

#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatTargetActions) == 0x29 * 4, "cat_selComputeTargetActions copies 0x29 dwords");
#endif

CatTargetActions *cat_selComputeTargetActions(CatTargetActions *out, MaxSelection *targets);   // CAT: 0x00513a00

#endif

