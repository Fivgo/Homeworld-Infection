// =============================================================================
//  AISkirmish.h
//  - Cataclysm's skirmish AI: the computer player of a multiplayer game sorts
//    every ship into 34 teams, builds, researches, harvests and attacks from
//    them (cataclysm.exe 0x0042fa80-0x00434135)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___AISKIRMISH_H
#define ___AISKIRMISH_H

#include "MaxMultiplayer.h"
#include "ShipDefs.h"
#include "ShipSelect.h"
#include "SpaceObj.h"
#include "Types.h"
#include "Vector.h"

/*=============================================================================
    Teams (CET AISkirmish.h; the names of the teams CET left unnamed are chosen
    here from the ships cat_aisUpdateTeams 0x0042fd50 puts into them)
=============================================================================*/

#define AIS_NUM_TEAMS               34
#define AIS_MAX_TEAM_SELECTIONS     10

typedef enum
{
    AIS_TEAM_Scout              = 0,    // s/bRecon
    AIS_TEAM_ScoutEnemy         = 1,
    AIS_TEAM_Fighter            = 2,    // sAcolyte, sACV, bAcolyte, bInterceptor, bCloakedFighter, bAttackBomber, bACV
    AIS_TEAM_FighterEnemy       = 3,
    AIS_TEAM_AntiFighter        = 4,    // sMultiBeamFrigate, bMultiGunCorvette, bMultiBeamFrigate
    AIS_TEAM_AntiFighterEnemy   = 5,
    AIS_TEAM_Sentinel           = 6,    // s/bSentinel (one selection)
    AIS_TEAM_SentinelEnemy      = 7,
    AIS_TEAM_MediumCombat       = 8,    // the frigates, destroyers, dreadnought, cruisers, missile ships
    AIS_TEAM_MediumCombatEnemy  = 9,
    AIS_TEAM_BeingBuilt         = 10,   // own ships under external construction (one selection; CET "Main")
    AIS_TEAM_MothershipModules  = 11,   // the sMothership modules (one selection)
    AIS_TEAM_CarrierSupport1    = 12,   // sCarrierSupport on the first carrier (one selection)
    AIS_TEAM_CarrierSupport2    = 13,   // sCarrierSupport on the second carrier (one selection)
    AIS_TEAM_MothershipSupport  = 14,   // s/bMothershipSupport (one selection)
    AIS_TEAM_BigGunCharged      = 15,   // an sMothershipBigGun on a charged mothership (one selection)
    AIS_TEAM_ModulesEnemy       = 16,   // enemy modules and support modules
    AIS_TEAM_Carrier            = 17,   // s/bCarrier
    AIS_TEAM_CarrierEnemy       = 18,
    AIS_TEAM_Mothership         = 19,   // s/bMothership
    AIS_TEAM_MothershipEnemy    = 20,
    AIS_TEAM_Processor          = 21,   // s/bProcessor
    AIS_TEAM_ProcessorEnemy     = 22,
    AIS_TEAM_Mimic              = 23,   // sMimic, sMCV
    AIS_TEAM_MimicEnemy         = 24,   // enemy mimics this player has not detected (cat_shipDetectedByPlayer 0)
    AIS_TEAM_MimicEnemyDetected = 25,
    AIS_TEAM_CruiseMissile      = 26,   // bCruiseMissile
    AIS_TEAM_Leech              = 27,   // sLeech
    AIS_TEAM_LeechEnemy         = 28,
    AIS_TEAM_DFGFrigate         = 29,   // bDFGFrigate
    AIS_TEAM_DFGFrigateEnemy    = 30,
    AIS_TEAM_Worker             = 31,   // s/bWorker
    AIS_TEAM_WorkerEnemy        = 32,
    AIS_TEAM_Crystal            = 33    // dCrystal0 / dCrystal1, whoever owns them
} AISTeamIndex;

// a group of a team's ships and its centre (0xFB4 bytes on ILP32)
typedef struct AISTeamSelection
{
    MaxSelection sel;
    vector center;
} AISTeamSelection;

// one team: up to ten groups (0x9D10 bytes on ILP32; the 34 teams fill 0x008dfd60..0x00a2d980)
typedef struct AISTeamEntry
{
    AISTeamSelection selection[AIS_MAX_TEAM_SELECTIONS];
    udword numselections;
    sdword totalteamsize;
} AISTeamEntry;

#if defined(__i386__)
_Static_assert(sizeof(AISTeamSelection) == 0xFB4, "AISTeamSelection is 0xFB4 bytes in cataclysm.exe");
_Static_assert(sizeof(AISTeamEntry) == 0x9D10, "AISTeamEntry is 0x9D10 bytes in cataclysm.exe");
_Static_assert(__builtin_offsetof(AISTeamEntry, numselections) == 0x9D08, "AISTeamEntry.numselections at +0x9D08");
#endif

/*=============================================================================
    Data (the exe keeps it outside the AIPlayer, in .bss)
=============================================================================*/

extern AISTeamEntry aisTeams[AIS_NUM_TEAMS];                // 0x008dfd60
extern real32 aisLastPlayTime[MAX_MULTIPLAYER_PLAYERS];     // 0x008dfc44: the 8 play timers, one per player index
extern sdword aisAlliancesSet;                              // 0x00a2d9b8
extern udword aiBuildingShip[TOTAL_STD_SHIPS];              // 0x008dfc64: ships of each type in the build queue
extern sdword aiSupportUnitsPending;                        // 0x00a2d9b4: their support units
extern Ship *aiCarrier1Ship;                                // 0x008dfc38
extern Ship *aiCarrier2Ship;                                // 0x008dfc3c
extern bool32 aiHasExternalConstruction;                    // 0x00a2d984
extern bool32 aiHasCarrier1ModuleQueued;                    // 0x00a2d98c
extern bool32 aiHasSupportModuleQueued;                     // 0x00a2d994
extern bool32 aiHasCarrier2ModuleQueued;                    // 0x00a2d9a8

/*=============================================================================
    Functions
=============================================================================*/

void cat_aisTeamAddShip(AISTeamEntry *team, Ship *ship);                    // 0x0042fa80
void cat_aisUpdateTeams(void);                                              // 0x0042fd50
void cat_aisSetupAlliances(void);                                           // 0x004310a0
void cat_aisResearchUpdate(void);                                           // 0x00431290
Ship *cat_aisTeamGetShip(sdword index, AISTeamEntry *team);                 // 0x00431490
void aisRequestShip(struct Player *player, ShipType type, sdword buildCost); // 0x00431510
void aisFleetUpdate(void);                                                  // 0x00431750
bool32 cat_aisShipIsBusy(Ship *ship);                                       // 0x004324a0
void cat_aisWorkersUpdate(void);                                            // 0x004324d0
void cat_aisProcessorUpdate(void);                                          // 0x00432630
void cat_aisScoutsUpdate(void);                                             // 0x00432760
AISTeamSelection *cat_aisTeamFindNearestSelection(sdword teamIndex, vector *pos);  // 0x00432d20
void cat_aisTeamAttack(AISTeamEntry *team, sdword minSize, sdword groupSize, bool32 setFormation, bool32 setStance,
                       bool32 allowHyperspace, bool32 allowKamikaze, sdword numTargetTeams,
                       const sdword *targetTeams);                          // 0x00432df0
void cat_aisMothershipUpdate(void);                                         // 0x00433310
void cat_aisCarrierUpdate(void);                                            // 0x00433620
void cat_aisSentinelsUpdate(void);                                          // 0x00433760
void cat_aisLeechesUpdate(void);                                            // 0x004338e0
void cat_aisReset(void);                                                    // 0x00433d30
void cat_aisPlay(void);                                                     // 0x00433d50

#endif
