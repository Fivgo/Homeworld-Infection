// =============================================================================
//  AIOrders.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___AIORDERS_H
#define ___AIORDERS_H

#include "AIUtilities.h"
#include "ShipSelect.h"
#include "AIPlayer.h"
#include "Globals.h"
#include "ResearchAPI.h"

/*=============================================================================
    Constant Definitions:
=============================================================================*/
#define RECON_STANDOFF_DISTANCE     15000
#define HARASS_STANDOFF_DISTANCE    25000
#define HARASS_LIGHTINT_INITNUM     15
#define HARASS_LIGHTINT_REPLACE     12
#define HARASS_HEAVYINT_EQUIVNUM    14
#define HARASS_BOMBER_EQUIVNUM      14

// CAT: a Beast computer player asks for some types only once its research has enabled them, else for a fallback type;
//      the exe reads Player.researchinfo.catShipTypeEnabled[type] inline at each site (aioCreateGuardShips 0x0040e160,
//      aihHarassNumbersLowHandler 0x00405d40, ...; cat_playerShipTypeEnabled 0x004f4780 is the same read)
#define aioCatEnabledOr(type, fallback) \
    (cat_playerShipTypeEnabled(aiCurrentAIPlayer->player, (type)) ? (type) : (fallback))

// CAT: the tactics of an attack the AI starts: a single-player script team keeps its own (AITeam +0x74, the tree's
//      kasTactics), every other team is Aggressive (aihFastDefenseDistressHandler 0x00406880, aiuRescueShip 0x0041a700,
//      the attack moves of AIMoves.c)
#define aioCatAttackTactics(team) \
    ((singlePlayerGame && ((team)->teamType == ScriptTeam)) ? (team)->kasTactics : Aggressive)

/*=============================================================================
    Structures:
=============================================================================*/
typedef enum
{
    RECON_MOTHERSHIP,
    RECON_ACTIVE_GENERAL,
    RECON_ACTIVE_ENEMY,
    NUM_RECONTYPE
} ReconType;

typedef enum
{
    SUPPORT_STRIKECRAFT,
    SUPPORT_RESOURCE,
    SUPPORT_MOTHERSHIP,
    NUM_SUPPORTTYPE
} SupportType;

void aioCreateGuardShips(struct AITeam *team, SelectCommand *ships);
void aioCreateReconaissance(struct AITeam *team, ReconType type);
void aioCreateTakeoutTarget(struct AITeam *team,Ship *target);
void aioCreateHarass(struct AITeam *team);
void aioCreateTakeoutMothershipFast(struct AITeam *team,Ship *mothership);
void aioCreateTakeoutMothershipGuard(struct AITeam *team,Ship *mothership);
void aioCreateTakeoutMothershipHuge(struct AITeam *team, Ship *mothership);
void aioCreateTakeoutMothershipBig(struct AITeam *team,Ship *mothership, bool32 ForceBig);
void aioCreateTakeoutTargetsWithCurrentTeam(struct AITeam *team,SelectCommand *targets);
void aioCreateTakeoutTargetWithCurrentTeam(struct AITeam *team,Ship *ship);
void aioCreateFancyTakeoutTarget(struct AITeam *team,Ship *target);
void aioCreateDefendMothership(struct AITeam *team);
void aioCreateFastRovingDefense(struct AITeam *team);
void aioCreateSlowRovingDefense(struct AITeam *team);
void aioCreateActiveSupport(struct AITeam *team, SelectCommand *ships, SupportType type);
void aioCreateFighterStrike(struct AITeam *team);
void aioCreateCorvetteStrike(struct AITeam *team);
void aioCreateFrigateStrike(struct AITeam *team);
void aioCreateResourcer(struct AITeam *team);
void aioCreateCapture(struct AITeam *team);
void aioCreateMine(struct AITeam *team);
void aioCreateSpecialDefense(struct AITeam *team, ShipType type);
// CAT: no aioCreatePatrol, aioCreateReconHarass, aioCreateReinforcements or the P2 orders (aioCreateSwarmAttack /
//      Defense / Support, aioCreateMultiBeamAttack, aioCreateP2MothershipAttack): cataclysm.exe has none of them and the
//      tree never called them

void aioCreateTakeoutMothershipFast(struct AITeam *team,Ship *mothership);
void aioCreateTakeoutMothershipBig(struct AITeam *team,Ship *mothership, bool32 ForceBig);
void aioCreateTakeoutMothershipHuge(struct AITeam *team,Ship *mothership);
void aioCreateTakeoutMothershipGuard(struct AITeam *team,Ship *mothership);
void aioCreateTakeoutTargetsWithCurrentTeam(struct AITeam *team,SelectCommand *targets);
void aioCreateTakeoutTargetWithCurrentTeam(struct AITeam *team,Ship *ship);
void aioCreateTakeoutTarget(struct AITeam *team,Ship *target);
void aioCreateFancyTakeoutTarget(struct AITeam *team,Ship *target);

#endif
