//
//  implementations of KAS functions
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ACV.h"
#include "AIFeatures.h"
#include "AIMoves.h"
#include "AIPlayer.h"
#include "AITeam.h"
#include "AITrack.h"
#include "AIVar.h"
#include "Animatic.h"
#include "BTG.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "CommandWrap.h"
#include "ConsMgr.h"
#include "Debug.h"
#include "FastMath.h"
#include "font.h"
#include "FormationDefs.h"
#include "HS.h"
#include "KAS.h"
#include "KASFunc.h"
#include "KASVar.h"
#include "mainrgn.h"
#include "Memory.h"
#include "NIS.h"
#include "Objectives.h"
#include "PiePlate.h"
#include "Ping.h"
#include "prim2d.h"
#include "Randy.h"
#include "render.h"
#include "SaveGame.h"
#include "Select.h"
#include "Sensors.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "SpeechEvent.h"
#include "StringsOnly.h"
#include "Subtitle.h"
#include "TaskBar.h"
#include "texreg.h"
#include "Timer.h"
#include "TradeMgr.h"
#include "ShipNames.h"
#include "Tutor.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "utility.h"
#include "Vector.h"
#include "Worker.h"
#include "Task.h"
#include "ShipDisable.h"
#include "Salvage.h"
#include "RKO.h"
#include "ResearchGUI.h"
#include "ResearchAPI.h"
#include "P1StandardCorvette.h"
#include "NomadMoon.h"
#include "mouse.h"
#include "Mimic.h"
#include "LaunchMgr.h"
#include "KeyBindings.h"
#include "InfoOverlay.h"
#include "HiveFrigate.h"
#include "Gun.h"
#include "Formation.h"
#include "FOW.h"
#include "Dock.h"
#include "Console.h"
#include "CameraCommand.h"
#include "Blast.h"
#include "BeastWeapon.h"

#if defined _MSC_VER
	#define strncasecmp _strnicmp
	#define snprintf _snprintf
	#define vsnprintf _vsnprintf
#else
    #include <strings.h>
#endif


extern char SinglePlayerSavedGamesPath[];
extern char TutorialSavedGamesPath[];          // CAT: the level-0 save path of cat_kasSaveLevelNow (GamePick.c)
extern bool32 catWipeShipFreeDirectly;          // CAT: 0x00a44000 (UnivUpdate.c; kasfMission13DeleteEngineeringModuleHack)
//extern char *getWord(char *dest, char *source); // Found in researchgui.c

//
//  these are for the placeholder for a real fleet intelligence message window
//
extern fonthandle selGroupFont2;
#define POPUPTEXT_TITLE_COLOR      colRGB(255, 255, 255)
#define POPUPTEXT_COLOR            colRGB(200, 200, 0)
#define POPUPTEXT_BACKGROUND       colRGB(0, 0, 0)
#define POPUPTEXT_BORDER_COLOR     colRGB(100, 100, 100)
sdword popupTextNumLines = 0;
char popupTextLines[POPUPTEXT_MAX_LINES][POPUPTEXT_MAX_LINE_LENGTH+1];
real32 popupTextEndTime;
sdword popupTextWidth;

// this can help eliminate the need to pass the current team ptr to some functions
extern AITeam *CurrentTeamP;
extern AITeam *kasUnpausedTeam;

// run-time scoping for variables, timers, etc.
extern sdword CurrentMissionScope;  // what level we're currently executing at
extern udword CurrentMissionScopeName;     // CAT: a cat_kasHashString hash (0x00c374a0)

extern LabelledVector **LabelledVectors;
extern sdword LabelledVectorsUsed;

extern bool32 ccCameraTimeoutOverride;

nisstatic stat1 = {0, 50, 25,  5,  3, 240, 173, 0.5f, 0.5f, 0.9f, 0.1f, 0.2f, 0.1f, 0.6f, 0.2f, TRUE, FALSE, 0};

//current sensors manager zoom length
extern real32 smInitialDistance;
//stores the current sensors manager zoom length
sdword kasStoreSMInitialDistance = 0;

static sdword kasfDummyEventNumber = 0;         //incremented to ensure unique event numbers (CAT: 0x00a43ff4)

extern real32 SUBMESSAGE_SAFETY_TIMEOUT;
extern real32 subMessageReturnedFalseTime;

//for fading to black at the end of the game
extern real32 nisBlackFadeDest;
extern real32 nisBlackFadeRate;

extern bool32 debugScreenshots;

void kasfMissionCompleted(void)  // KAS MissionCompleted, cataclysm.exe 0x00491c70
{
    if (tutorial == TUTORIAL_ONLY)
    {
        //put the student out of it's misery
        tutorialdone = TRUE;
    }
    else
    {
        clCommandMessage("MISSION COMPLETED", 0xffffffff);
        // play the end of Mission animatic
        // if you want to finish a level without hyperspacing, you put the stuff that happens here
        speechEventCleanup();
        singlePlayerMissionCompleteCB();

        // CAT: the animatic between this mission and the next by number (kasfMissionCompleted 0x00491c70:
        //      animBinkPlay(currentMission, currentMission + 1); GoK went through its mission sequence)
        animAviPlay(singlePlayerGameInfo.currentMission, singlePlayerGameInfo.currentMission + 1);
    }
}

void kasfMissionFailed(void)  // KAS MissionFailed, cataclysm.exe 0x00491cb0
{
    //clCommandMessage("MISSION FAILED");
    singlePlayerMissionFailedCB();
}

void kasfFadeToWhite(void)  // KAS FadeToWhite, cataclysm.exe 0x00491cc0
{
    soundEventStopSFX(3.0);
    nisBlackFadeDest = 1.0f;
    nisBlackFadeRate = 3.0f;
    nisTaskResume();
}

//
//  for scoping symbol names
//
// CAT (M2-R8 kasf library): Cataclysm's kas2c passes every timer and variable name as its cat_kasHashString hash; a
//      G_ name has bit 31 set (kas2c hashes the whole "G_..." name) and is global: scope 0 and the bit cleared; any
//      other name is scoped to the running FSM or state (the current scope hash, CurrentMissionScopeName 0x00c374a0)
//      or, at mission scope, to 0 (the inline prologue of kasfVarCreate 0x00491cf0 .. kasfTimerDestroy 0x004920e0;
//      HW1's kasfScopeName built "scope.name" strings)
static udword kasfScopeHash(udword *name)
{
    if (*name & 0x80000000)
    {
        *name &= 0x7fffffff;
        return 0;
    }
    return (CurrentMissionScope != KAS_SCOPE_MISSION) ? CurrentMissionScopeName : 0;
}

// CAT: 0x00491cf0: a variable starts at 0 (cat_kasVarFindOrCreate); a zero hash creates nothing
void kasfVarCreate(udword name)
{
    udword scope = kasfScopeHash(&name);

    if (name != 0)
    {
        cat_kasVarFindOrCreate(scope, name);
    }
}

// CAT: 0x00491d30
void kasfVarCreateSet(udword name, sdword value)
{
    udword scope = kasfScopeHash(&name);
    CatKasVar *var = cat_kasVarFindOrCreate(scope, name);

    if (var != NULL)
    {
        var->value = value;
    }
}

// CAT: 0x00491d70: only an existing variable is set
void kasfVarValueSet(udword name, sdword value)
{
    udword scope = kasfScopeHash(&name);
    CatKasVar *var = cat_kasVarFind(scope, name);

    if (var != NULL)
    {
        var->value = value;
    }
}

// CAT: 0x00491db0
void kasfVarValueInc(udword name)
{
    udword scope = kasfScopeHash(&name);
    CatKasVar *var = cat_kasVarFind(scope, name);

    if (var != NULL)
    {
        var->value++;
    }
}

// CAT: 0x00491df0
void kasfVarValueDec(udword name)
{
    udword scope = kasfScopeHash(&name);
    CatKasVar *var = cat_kasVarFind(scope, name);

    if (var != NULL)
    {
        var->value--;
    }
}

// CAT: 0x00491e30: 0 for a variable that does not exist
sdword kasfVarValueGet(udword name)
{
    udword scope = kasfScopeHash(&name);
    CatKasVar *var = cat_kasVarFind(scope, name);

    if (var != NULL)
    {
        return var->value;
    }
    return 0;
}

// CAT: 0x00491e70
void kasfVarDestroy(udword name)
{
    udword scope = kasfScopeHash(&name);

    cat_kasVarDestroy(scope, name);
}

void kasfCommandMessage(char *message)  // KAS Print, cataclysm.exe 0x00491eb0
{
    clCommandMessage(message, 0xffffffff);
}

// CAT: the timers take the scope and name hashes (kasfTimerCreate 0x00491ec0 .. kasfTimerDestroy 0x004920e0, each the
//      scope prologue of kasfScopeHash and one Timer.c call)
void kasfTimerCreate(udword name)
{
    udword scope = kasfScopeHash(&name);
    timTimerCreate(scope, name);
}

void kasfTimerSet(udword name, sdword duration)  // KAS TimerSet, cataclysm.exe 0x00491f00
{
    udword scope = kasfScopeHash(&name);
    timTimerSet(scope, name, duration);
}

void kasfTimerStart(udword name)  // KAS TimerStart, cataclysm.exe 0x00491f50
{
    udword scope = kasfScopeHash(&name);
    timTimerStart(scope, name);
}

void kasfTimerCreateSetStart(udword name, sdword duration)  // KAS TimerCreateSetStart, cataclysm.exe 0x00491f90
{
    udword scope = kasfScopeHash(&name);
    timTimerCreateSetStart(scope, name, duration);
}

// CAT: 0x00491fe0 stops the timer (HW1's kasfTimerStop called timTimerStart)
void kasfTimerStop(udword name)
{
    udword scope = kasfScopeHash(&name);
    timTimerStop(scope, name);
}

sdword kasfTimerRemaining(udword name)  // KAS TimerRemaining, cataclysm.exe 0x00492020
{
    udword scope = kasfScopeHash(&name);
    return timTimerRemaining(scope, name);
}

sdword kasfTimerExpired(udword name)  // KAS TimerExpired, cataclysm.exe 0x00492060
{
    udword scope = kasfScopeHash(&name);
    return timTimerExpired(scope, name);
}

sdword kasfTimerExpiredDestroy(udword name)  // KAS TimerExpiredDestroy, cataclysm.exe 0x004920a0
{
    udword scope = kasfScopeHash(&name);
    return timTimerExpiredDestroy(scope, name);
}

void kasfTimerDestroy(udword name)
{
    udword scope = kasfScopeHash(&name);
    timTimerDestroy(scope, name);
}

// CAT: the message is its kas2c hash (kasfMsgReceived 0x00492120)
sdword kasfMsgReceived(udword msg)
{
    // remove the need to pass a team ptr
    return aitMsgReceived(CurrentTeamP, msg);
}

void kasfAttackMothership(void)  // KAS AttackMothership, cataclysm.exe 0x00492140
{
    SelectCommand *target;
    target = (SelectCommand *)memAlloc(sizeofSelectCommand(1), "targetmoship", 0);
    // assumes human is player 0
    target->ShipPtr[0] = aiuFindEnemyMothership(&universe.players[1]);
    if (target->ShipPtr[0] != NULL)
    {
        target->numShips = 1;
        aitDeleteAllTeamMoves(CurrentTeamP);
        aimCreateAttack(CurrentTeamP, target, SAME_FORMATION, TRUE, TRUE);
    }
}

// CAT: 0x00492190: the team's ships get a plain attack order through the command layer (HW1 queued an advanced-attack
//      move); more than 256 targets are attacked from a heap copy, others from a stack MaxSelection
void kasfAttack(GrowSelection *targets)
{
    SelectCommand *selection = targets->selection;
    SelectCommand *dupeTargets;
    MaxSelection tempsel;
    sdword i;

    if (selection->numShips == 0)
    {
        return;
    }
    if (selection->numShips > 0x100)
    {
        dupeTargets = selectDupSelection(selection);
        aitDeleteAllTeamMoves(CurrentTeamP);
        clWrapAttack(&universe.mainCommandLayer, CurrentTeamP->shipList.selection, (AttackCommand *)dupeTargets);
        memFree(dupeTargets);
        return;
    }
    tempsel.numShips = 0;
    for (i = 0; i < targets->selection->numShips; i++)
    {
        selSelectionAddSingleShip(&tempsel, targets->selection->ShipPtr[i]);
    }
    aitDeleteAllTeamMoves(CurrentTeamP);
    clWrapAttack(&universe.mainCommandLayer, CurrentTeamP->shipList.selection, (AttackCommand *)&tempsel);
}

// gives a set of ships the attack command
// this allows individual ships from a team to attack a target
void kasfShipsAttack(GrowSelection *targets, GrowSelection *attackers)  // KAS ShipsAttack, cataclysm.exe 0x00492260
{
    aiuWrapAttack(attackers->selection, targets->selection);
}

void kasfAttackSpecial(GrowSelection *targets)  // KAS AttackSpecial, cataclysm.exe 0x00492280
{
    // alloc memory that the move can later free itself
    SelectCommand *dupeTargets = selectDupSelection(targets->selection);
    if (dupeTargets->numShips && dupeTargets->ShipPtr[0] != NULL)
    {
        aitDeleteAllTeamMoves(CurrentTeamP);
        aimCreateSpecial(CurrentTeamP, dupeTargets, SAME_FORMATION, CurrentTeamP->kasTactics, FALSE, TRUE);
    }
}

void kasfMoveAttack(GrowSelection *targets)  // KAS MoveAttack, cataclysm.exe 0x004922d0
{
    //alloc memory that the move can later free itself
    SelectCommand *dupeTargets = selectDupSelection(targets->selection);
    if (dupeTargets->numShips && dupeTargets->ShipPtr[0] != NULL)
    {
        aitDeleteAllTeamMoves(CurrentTeamP);
        aimCreateMoveAttack(CurrentTeamP, dupeTargets, TRUE, SAME_FORMATION, CurrentTeamP->kasTactics, TRUE, TRUE);
    }
}



/*-----------------------------------------------------------------------------
    Name        : kasfBulgeAttack
    Description : Finds ships in the current team that are near the enemy ships,
                  and only uses those to attack the enemy.  This function is an
                  optimization for Mission 14 (though it could be used anywhere)
    Inputs      : targets      - the ships to attack,
                  bulgetargets - the ships that end up being attacked,
                  attackers    - the ships in the team that split off to attack
                  radius       - the maximum distance the team members have to be in
                                 order to attack
    Outputs     : Various ships in the current team get attack orders
    Return      : the number of ships chosen to take out
----------------------------------------------------------------------------*/
sdword kasfBulgeAttack(GrowSelection *targets, GrowSelection *bulgetargets,
                       GrowSelection *attackers, sdword radius)  // KAS BulgeAttack, cataclysm.exe 0x00492320
{
    ShipPtr teamship;
    SelectCommand *teamships   = CurrentTeamP->shipList.selection,
                  *targetships = targets->selection,
                  onesel;
    MaxSelection tempsel;
    udword i, j, num_teamships = teamships->numShips,
                 num_targets   = targetships->numShips;
    real32 radiussq = (real32)radius*(real32)radius, distsq;

    // initialize variables
    bulgetargets->selection->numShips = 0;
    attackers->selection->numShips    = 0;
    tempsel.numShips = 0;
    onesel.numShips  = 1;

    //go through each individual team ship
    for (i=0; i < num_teamships;i++)
    {
        teamship = teamships->ShipPtr[i];

        // only use ships that aren't attacking already
        if (!teamship->command ||
            (teamship->command->ordertype.order != COMMAND_ATTACK))
        {
            // find target ships within the alloted radius
            for (j=0;j < num_targets;j++)
            {
                distsq = aiuFindDistanceSquared(targets->selection->ShipPtr[j]->obj.posinfo.position,
                                                teamship->obj.posinfo.position);

                if (distsq < radiussq)
                {
                    selSelectionAddSingleShip(&tempsel, targets->selection->ShipPtr[j]);
                }
            }

            // if any targets close enough were found, attack them with this ship
            if (tempsel.numShips)
            {
                onesel.ShipPtr[0] = teamship;
                aiuWrapAttack(&onesel, (SelectCommand *)&tempsel);

                // add the ship to the attackers grow selection
                growSelectAddShip(attackers, teamship);

                // add each new target to the bulgetargets grow selection
                for (j=0;j<tempsel.numShips;j++)
                {
                    growSelectAddShipNoDuplication(bulgetargets, tempsel.ShipPtr[j]);
                }
                tempsel.numShips = 0;
            }
        }
    }
    return bulgetargets->selection->numShips;
}



void kasfIntercept(GrowSelection *targets)  // KAS Intercept, cataclysm.exe 0x004924e0
{
    //alloc memory that the move can later free itself
    if (targets->selection->numShips && targets->selection->ShipPtr[0] != NULL)
    {
        aitDeleteAllTeamMoves(CurrentTeamP);
        aimCreateIntercept(CurrentTeamP, targets->selection->ShipPtr[0], 2.0, SAME_FORMATION, CurrentTeamP->kasTactics, TRUE, TRUE);
    }
}



// CAT (M2-R8 kasf library): kasfTargetDrop is gone (cataclysm.exe's function table 0x00755048 has no "TargetDrop")

void kasfAttackFlank(GrowSelection *targets)  // KAS AttackFlank, cataclysm.exe 0x00492530
{
    // alloc memory that the move can later free itself
    SelectCommand *dupeTargets = selectDupSelection(targets->selection);
    if (dupeTargets->numShips && dupeTargets->ShipPtr[0] != NULL)
    {
        aitDeleteAllTeamMoves(CurrentTeamP);
        aimCreateFlankAttack(CurrentTeamP, dupeTargets, FALSE, FALSE, TRUE);
    }
}

void kasfAttackHarass(void)  // KAS AttackHarass, cataclysm.exe 0x00492580
{
    aitDeleteAllTeamMoves(CurrentTeamP);
    aimCreateHarassAttack(CurrentTeamP, FALSE, TRUE);
}

//sets the swarmers to target mode and adds ships to the target list
//sets the swarmers to target mode and adds ships to the target list
void kasfSetSwarmerTargets(GrowSelection *targets)
{
    // CAT: 0x004925a0 frees and clears the list inline (memFree, as aiumemFree)
    aiumemFree(aiCurrentAIPlayer->Targets);
    aiCurrentAIPlayer->Targets = selectDupSelection(targets->selection);
    bitSet(aiCurrentAIPlayer->AlertStatus, ALERT_SWARMER_TARGETS);
}

void kasfSwarmMoveTo(GrowSelection *targets)
{
    MaxSelection swarmers, Pod;
    udword i;
    real32 temp;
    vector destination;
    ShipPtr ship;

    swarmers.numShips = 0;
    Pod.numShips = 0;

    // CAT: no P2AdvanceSwarmer / fuel pod split: every ship of the team is a "swarmer" and the pod selection stays
    //      empty (kasfSwarmMoveTo 0x00492610; the HW1 type is gone)
    for (i=0;i<CurrentTeamP->shipList.selection->numShips;i++)
    {
        ship = CurrentTeamP->shipList.selection->ShipPtr[i];
        selSelectionAddSingleShip(&swarmers, ship);
    }
    destination = selCentrePointComputeGeneral((MaxSelection *)targets->selection, &temp);

    if (Pod.numShips && swarmers.numShips)
    {
        aiuWrapProtect((SelectCommand *)&swarmers, (SelectCommand *)&Pod);
        aiuWrapMove((SelectCommand *)&Pod, destination);
    }
    else if (Pod.numShips)
    {
        aiuWrapMove((SelectCommand *)&Pod, destination);
    }
    else if (swarmers.numShips)
    {
        aiuWrapMove((SelectCommand *)&swarmers, destination);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSetFormation                             0x00492740
    Description : CAT: KAS "ShipsSetFormation": the ships get a formation order (formation 8, SAME_FORMATION's value,
                  means the military parade: clWrapSetMilitaryParade; any other goes through clWrapFormation with no
                  force-field level)
----------------------------------------------------------------------------*/
void kasfShipsSetFormation(GrowSelection *Ships, sdword Formation)
{
    if (Formation == SAME_FORMATION)
    {
        clWrapSetMilitaryParade(&universe.mainCommandLayer, Ships->selection);
        return;
    }
    clWrapFormation(&universe.mainCommandLayer, Ships->selection, (TypeOfFormation)Formation, 0);
}

void kasfFormationDelta(void)  // KAS FormationDelta, cataclysm.exe 0x00492780
{
    aiuKasWrapFormation(CurrentTeamP->shipList.selection, DELTA_FORMATION);
    CurrentTeamP->kasFormation = DELTA_FORMATION;
}
void kasfFormationBroad(void)  // KAS FormationBroad, cataclysm.exe 0x004927b0
{
    aiuKasWrapFormation(CurrentTeamP->shipList.selection, BROAD_FORMATION);
    CurrentTeamP->kasFormation = BROAD_FORMATION;
}
void kasfFormationDelta3D(void)  // KAS FormationDelta3D, cataclysm.exe 0x004927e0
{
    aiuKasWrapFormation(CurrentTeamP->shipList.selection, DELTA3D_FORMATION);
    CurrentTeamP->kasFormation = DELTA3D_FORMATION;
}
void kasfFormationClaw(void)  // KAS FormationClaw, cataclysm.exe 0x00492810
{
    aiuKasWrapFormation(CurrentTeamP->shipList.selection, CLAW_FORMATION);
    CurrentTeamP->kasFormation = CLAW_FORMATION;
}
void kasfFormationWall(void)  // KAS FormationWall, cataclysm.exe 0x00492840
{
    aiuKasWrapFormation(CurrentTeamP->shipList.selection, WALL_FORMATION);
    CurrentTeamP->kasFormation = WALL_FORMATION;
}
void kasfFormationSphere(void)  // KAS FormationSphere, cataclysm.exe 0x00492870
{
    aiuKasWrapFormation(CurrentTeamP->shipList.selection, SPHERE_FORMATION);
    CurrentTeamP->kasFormation = SPHERE_FORMATION;
}
/*-----------------------------------------------------------------------------
    Name        : kasfFormationParade                               0x004928a0
    Description : CAT: KAS "FormationParade": each ship joins the military parade around player 0's mothership, then the
                  parade ships are put at their positions at once (clPresetShipsToPosition)
----------------------------------------------------------------------------*/
void kasfFormationParade(GrowSelection *pShips)
{
    sdword i;

    for (i = 0; i < pShips->selection->numShips; i++)
    {
        GroupShipIntoMilitaryParade(&universe.mainCommandLayer, pShips->selection->ShipPtr[i],
                                    universe.players[0].PlayerMothership);
    }
    clPresetShipsToPosition(&universe.mainCommandLayer);
}

void kasfFormationCustom(GrowSelection *ships)  // KAS FormationCustom, cataclysm.exe 0x004928f0
{
    aiuKasWrapFormation(ships->selection, CUSTOM_FORMATION);
}

sdword kasfRandom(sdword lowestNum, sdword highestNum)  // KAS Random, cataclysm.exe 0x00492910
{
    if (lowestNum > highestNum)
    {
        sdword temp = lowestNum;
        lowestNum = highestNum;
        highestNum = temp;
    }
    return lowestNum + (ranRandom(RANDOM_AI_PLAYER) % (highestNum - lowestNum + 1));
}

void kasfGuardMothership(void)  // KAS GuardMothership, cataclysm.exe 0x00492940
{
    aitDeleteAllTeamMoves(CurrentTeamP);
    aimCreateDefendMothership(CurrentTeamP, FALSE, TRUE);
}

// CAT: 0x00492960: the team's ships get a guard (protect) order through the command layer (HW1 queued a guard-ships
//      move on a copy)
void kasfGuardShips(GrowSelection *ships)
{
    if (ships->selection->numShips != 0)
    {
        aitDeleteAllTeamMoves(CurrentTeamP);
        clWrapProtect(&universe.mainCommandLayer, CurrentTeamP->shipList.selection, (ProtectCommand *)ships->selection);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsGuardShips                               0x004929a0
    Description : CAT: KAS "ShipsGuardShips": the guards protect the ships (both selections non-empty)
----------------------------------------------------------------------------*/
void kasfShipsGuardShips(GrowSelection *Guard, GrowSelection *Ships)
{
    if ((Ships->selection->numShips != 0) && (Guard->selection->numShips != 0))
    {
        clWrapProtect(&universe.mainCommandLayer, Guard->selection, (ProtectCommand *)Ships->selection);
    }
}

void kasfPatrolPath(Path *path)  // KAS PatrolPath, cataclysm.exe 0x004929d0
{
    Path *dupePath = aiuPathDupe(path);
    aitDeleteAllTeamMoves(CurrentTeamP);
    aimCreatePatrolMove(CurrentTeamP, dupePath, 0, SAME_FORMATION, CurrentTeamP->kasTactics, FALSE, TRUE);
}

void kasfPatrolActive(void)  // KAS Patrol, cataclysm.exe 0x00492a10
{
    // an intelligent patrol that chooses its own path
    aitDeleteAllTeamMoves(CurrentTeamP);
    aimCreateActivePatrol(CurrentTeamP, AIT_SLOW_PATROL, FALSE, TRUE);
}

// CAT: kasfLog is folded into the empty stub 0x0049ae30 (its debug log is compiled out)
void kasfLog(char *string)
{
    (void)string;
}

void kasfLogInteger(char *string, sdword integer)  // KAS LogInteger, cataclysm.exe 0x00492a40
{
    char tempstr[256];
    char tempstr2[256];

    // ensure not too long and that there is an integer field
    if (strlen(string) > 200 || !strstr(string, "%d"))
        return;

    sprintf(tempstr, "KAS: %s", string);
    sprintf(tempstr2, tempstr, integer);
    aiplayerLog((0, tempstr2));
}

sdword kasfShipsOrder(GrowSelection *ships)  // KAS ShipsOrder, cataclysm.exe 0x00492ab0
{
    Ship *ship;

    if (ships->selection->numShips > 0)
    {
        ship = ships->selection->ShipPtr[0];
        if (ship->command)
        {
            return ship->command->ordertype.order;
        }
    }

    return 0;
}

// CAT: 0x00492ae0: the attributes of every ship's order, or-ed (HW1 read the first ship's)
sdword kasfShipsOrderAttributes(GrowSelection *ships)
{
    sdword i;
    udword attributes = 0;

    for (i = 0; i < ships->selection->numShips; i++)
    {
        if (ships->selection->ShipPtr[i]->command)
        {
            attributes |= ships->selection->ShipPtr[i]->command->ordertype.attributes;
        }
    }

    return (sdword)attributes;
}

//  returns percentage
// CAT: 0x00492b20: the sums stay on the x87 stack (53-bit) and the percentage is rounded to the nearest (the 1.5 * 2^52
//      constant 0x0072a458; HW1 truncated)
sdword kasfTeamHealthAverage(void)
{
    sdword i, numShips;
    real64 maxHealth = 0.0, actualHealth = 0.0;

    if (!CurrentTeamP)
        return 0;
    numShips = CurrentTeamP->shipList.selection->numShips;
    if (!numShips)
        return 0;

    for (i = 0; i < numShips; ++i)
    {
        maxHealth += CurrentTeamP->shipList.selection->ShipPtr[i]->staticinfo->health.maxhealth;
        actualHealth += CurrentTeamP->shipList.selection->ShipPtr[i]->targetable.health;
    }
    return (sdword)lrint(actualHealth * 100.0 / maxHealth);
}

//  returns percentage
// CAT: 0x00492b90: each percentage rounded to the nearest (HW1 truncated)
sdword kasfTeamHealthLowest(void)
{
    sdword i, numShips;
    sdword thisOne = 0, lowestOne = 110;

    if (!CurrentTeamP)
        return 0;
    numShips = CurrentTeamP->shipList.selection->numShips;
    if (!numShips)
        return 0;

    for (i = 0; i < numShips; ++i)
    {
        thisOne = (sdword)lrint((real64)CurrentTeamP->shipList.selection->ShipPtr[i]->targetable.health * 100.0 / (real64)CurrentTeamP->shipList.selection->ShipPtr[i]->staticinfo->health.maxhealth);
        if (thisOne < lowestOne)
            lowestOne = thisOne;
    }
    return lowestOne;
}

//  returns percentage
// CAT: 0x00492c00: as kasfTeamHealthAverage (53-bit sums, rounded)
sdword kasfTeamFuelAverage(void)
{
    sdword i, numShips;
    real64 maxFuel = 0.0, actualFuel = 0.0;

    if (!CurrentTeamP)
        return 0;
    numShips = CurrentTeamP->shipList.selection->numShips;
    if (!numShips)
        return 0;

    for (i = 0; i < numShips; ++i)
    {
            maxFuel += CurrentTeamP->shipList.selection->ShipPtr[i]->staticinfo->maxfuel;
            actualFuel += CurrentTeamP->shipList.selection->ShipPtr[i]->fuel;
    }
    return (sdword)lrint(actualFuel * 100.0 / maxFuel);
}

//  returns percentage
// CAT: 0x00492c70: each percentage rounded to the nearest (HW1 truncated)
sdword kasfTeamFuelLowest(void)
{
    sdword i, numShips;
    sdword thisOne = 0, lowestOne = 110;

    if (!CurrentTeamP)
        return 0;
    numShips = CurrentTeamP->shipList.selection->numShips;
    if (!numShips)
        return 0;

    for (i = 0; i < numShips; ++i)
    {
        thisOne = (sdword)lrint((real64)CurrentTeamP->shipList.selection->ShipPtr[i]->fuel * 100.0 / (real64)CurrentTeamP->shipList.selection->ShipPtr[i]->staticinfo->maxfuel);
        if (thisOne < lowestOne)
            lowestOne = thisOne;
    }
    return lowestOne;
}

sdword kasfTeamCount(void)  // KAS TeamCount, cataclysm.exe 0x00492ce0
{
    return CurrentTeamP->shipList.selection->numShips;
}

sdword kasfNewShipsAdded(void)  // KAS NewShipsAdded, cataclysm.exe 0x00492cf0
{
    sdword return_value = (sdword)CurrentTeamP->newships;

    CurrentTeamP->newships = 0;

    return return_value;
}

void kasfGrowSelectionClear(GrowSelection *ships)  // KAS ShipsClear, cataclysm.exe 0x00492d10
{
    kasGrowSelectionClear(ships);
}

sdword kasfShipsCount(GrowSelection *ships)  // KAS ShipsCount, cataclysm.exe 0x00492d20
{
    if (ships == NULL)
        return 0;

    return ships->selection->numShips;
}

sdword kasfPointInside(Volume *volume, hvector *location)  // KAS PointInside, cataclysm.exe 0x00492d30
{
    vector position;

    vecGrabVecFromHVec(position, *location);

    return volPointInside(volume, &position);
}

/*-----------------------------------------------------------------------------
    Name        : kasfFindMeteors                                   0x00492d60
    Description : CAT: KAS "FindMeteors": every asteroid of the object list with the killer-collision attribute
                  (Cataclysm's meteors)
----------------------------------------------------------------------------*/
void kasfFindMeteors(GrowSelection *meteors)
{
    Node *node;
    SpaceObj *obj;

    kasGrowSelectionClear(meteors);
    for (node = universe.SpaceObjList.head; node != NULL; node = node->next)
    {
        obj = (SpaceObj *)listGetStructOfNode(node);
        if ((obj->header.objtype == OBJ_AsteroidType) && bitTest(obj->attributes, CAT_ATTRIBUTES_KillerCollision))
        {
            growSelectAddShip(meteors, (Ship *)obj);
        }
    }
}

//
//  given a volume, find all ships inside it.  make this the selection.
//  return the number of ships in this selection.
//  [audit: M2-R8 kasf-library: this HW1 comment was left above kasfFindMeteors; KAS FindShipsInside, cataclysm.exe 0x00492db0]
//
sdword kasfFindShipsInside(Volume *volume, GrowSelection *ships)
{
    Ship *ship;
    Node *node;

    if (ships == NULL)
        return 0;

    //  wipe the selection clean
    kasGrowSelectionClear(ships);       // CAT: kasFindShipsInside 0x00492db0 clears through the KAS helper

    //  check all ships in the damn universe
    node = universe.ShipList.head;
    while (node != NULL)
    {
        ship = (Ship *)listGetStructOfNode(node);

        if ((ship->header.flags & (SOF_Dead|SOF_Hide)) == 0)
        {
            if (volPointInside(volume, &(ship->obj.posinfo.position)))
                // add it
                growSelectAddShip(ships, ship);
        }

        node = node->next;
    }

    return ships->selection->numShips;
}

//
//  given a volume, find all enemy ships inside it.  make this the selection.
//  return the number of ships in this selection.
//
sdword kasfFindEnemyShipsInside(Volume *volume, GrowSelection *ships)
{
    Ship *ship;
    Node *node;

    if (ships == NULL)
        return 0;

    //  wipe the selection clean
    ships->selection->numShips = 0;

    //  check all ships in the damn universe
    node = universe.ShipList.head;
    while (node != NULL)
    {
        ship = (Ship *)listGetStructOfNode(node);

        if ((ship->header.flags & (SOF_Dead|SOF_Hide)) == 0)
        {
            if (!ship->playerowner->playerIndex && (!bitTest(ship->header.flags,SOF_Crazy|SOF_Hide|SOF_Hyperspace|SOF_Disabled)) && volPointInside(volume, &(ship->obj.posinfo.position)))
                // add it
                growSelectAddShip(ships, ship);
        }

        node = node->next;
    }

    return ships->selection->numShips;
}

//
//  given a selection of ships, find all enemies within a radius
//  of those ships.  make this the new selection.  return the number
//  of ships in the new selection.
//
sdword kasfFindEnemiesNearby(GrowSelection *ships, sdword radius)  // KAS FindEnemiesNearby, cataclysm.exe 0x00492e90
{
    Ship *medianShip, *ship;
    real32 rangeSquared = (real32)radius*(real32)radius;
    Node *node;
    sdword i;

    if (ships == NULL || ships->selection == NULL || !(ships->selection->numShips))
        return 0;

    //  we should find the median ship, but since we're dealing with a selection
    //  and we probably don't intend the radius to exclude any existing ships,
    //  we'll assume the radius is large enough to consume the whole group from
    //  any (enemy) ship within the group (and then some, since the radius is supposed
    //  to add to the group if anything).  this is heavy-duty cheating, but it
    //  saves mucho CPU time if this routine were to be in many regular watch
    //  sections of script code.
    i = 0;
    do
    {
        medianShip = ships->selection->ShipPtr[i];
        ++i;
    } while (medianShip->playerowner->playerIndex && i < ships->selection->numShips); // not enemy yet

    //  wipe the selection clean
    ships->selection->numShips = 0;

    //  check all ships in the damn universe
    node = universe.ShipList.head;
    while (node != NULL)
    {
        ship = (Ship *)listGetStructOfNode(node);

        if ((ship->header.flags & (SOF_Dead|SOF_Hide)) == 0)
        {
            // is it an enemy?  (simplifying assumption: "is it the human"?)
            if ((!ship->playerowner->playerIndex) && (!bitTest(ship->header.flags,SOF_Crazy|SOF_Hide|SOF_Hyperspace|SOF_Disabled)))
            {
                // within the radius?
                if (medianShip == ship || aiuFindDistanceSquared(medianShip->obj.posinfo.position, ship->obj.posinfo.position) <= rangeSquared)
                {
                    // add it
                    growSelectAddShip(ships, ship);
                }
            }
        }

        node = node->next;
    }

    return ships->selection->numShips;
}

//
//  like FindEnemiesNearby, only check the radius relative to the current team,
//  ignoring what might be in the initial ships list
//
sdword kasfFindEnemiesNearTeam(GrowSelection *ships, sdword radius)  // KAS FindEnemiesNearTeam, cataclysm.exe 0x00492f80
{
    Ship *medianShip, *ship;
    real32 rangeSquared = (real32)radius*(real32)radius;
    Node *node;

    if (ships == NULL || ships->selection == NULL)
        return 0;

    //  wipe the selection clean
    ships->selection->numShips = 0;

    if (!CurrentTeamP->shipList.selection->numShips)
        return 0;

    //
    //  TBD: could try to select a better median ship among the team
    //
    medianShip = CurrentTeamP->shipList.selection->ShipPtr[0];

    //  check all ships in the damn universe
    node = universe.ShipList.head;
    while (node != NULL)
    {
        ship = (Ship *)listGetStructOfNode(node);

        if ((ship->header.flags & (SOF_Dead|SOF_Hide)) == 0)
        {
            // is it an enemy?  (simplifying assumption: "is it the human"?)
            if (!ship->playerowner->playerIndex && (!bitTest(ship->header.flags,SOF_Crazy|SOF_Hide|SOF_Hyperspace|SOF_Disabled)))
            {
                // within the radius?
                if (medianShip == ship || aiuFindDistanceSquared(medianShip->obj.posinfo.position, ship->obj.posinfo.position) <= rangeSquared)
                {
                    // add it
                    growSelectAddShip(ships, ship);
                }
            }
        }

        node = node->next;
    }

    return ships->selection->numShips;
}

//
//  Find all the ships near a point
//
/*-----------------------------------------------------------------------------
    Name        : kasfFindMinesNearTeam                             0x00493060
    Description : CAT: KAS "FindMinesNearTeam": the mines (missiles of type MISSILE_Mine) within radius of the current
                  team's first ship; 0 (and the selection untouched) when the team is empty
----------------------------------------------------------------------------*/
sdword kasfFindMinesNearTeam(GrowSelection *ships, sdword radius)
{
    Ship *medianShip;
    Missile *mine;
    Node *node;

    if ((ships == NULL) || (ships->selection == NULL) || (CurrentTeamP->shipList.selection->numShips == 0))
        return 0;

    ships->selection->numShips = 0;
    medianShip = CurrentTeamP->shipList.selection->ShipPtr[0];
    for (node = universe.MissileList.head; node != NULL; node = node->next)
    {
        mine = (Missile *)listGetStructOfNode(node);
        if ((mine->header.objtype == OBJ_MissileType) && (mine->missileType == MISSILE_Mine) &&
            (aiuFindDistanceSquared(medianShip->obj.posinfo.position, mine->obj.posinfo.position) <=
             (real32)radius * (real32)radius))
        {
            growSelectAddShip(ships, (Ship *)mine);
        }
    }
    return ships->selection->numShips;
}

/*-----------------------------------------------------------------------------
    Name        : kasfFindMinesInside                               0x00493130
    Description : CAT: KAS "FindMinesInside": the mines inside the volume
----------------------------------------------------------------------------*/
sdword kasfFindMinesInside(Volume *volume, GrowSelection *ships)
{
    Missile *mine;
    Node *node;

    if (ships == NULL)
        return 0;

    kasGrowSelectionClear(ships);
    for (node = universe.MissileList.head; node != NULL; node = node->next)
    {
        mine = (Missile *)listGetStructOfNode(node);
        if ((mine->header.objtype == OBJ_MissileType) && (mine->missileType == MISSILE_Mine) &&
            volPointInside(volume, &mine->obj.posinfo.position))
        {
            growSelectAddShip(ships, (Ship *)mine);
        }
    }
    return ships->selection->numShips;
}

sdword kasfFindShipsNearPoint(GrowSelection *ships, hvector *location, sdword radius)  // KAS FindShipsNearPoint, cataclysm.exe 0x004931a0
{
    Ship *ship;
    real32 rangeSquared = (real32)radius*(real32)radius;
    vector position;
    Node *node;

    if (ships == NULL || ships->selection == NULL || location == NULL)
        return 0;

    vecGrabVecFromHVec(position, *location);

    //  wipe the selection clean
    ships->selection->numShips = 0;

    //  check all ships in the damn universe
    node = universe.ShipList.head;
    while (node != NULL)
    {
        ship = (Ship *)listGetStructOfNode(node);

        if ((ship->header.flags & (SOF_Dead|SOF_Hide)) == 0)
        {
            // is it an ship of some kind?
            if (!bitTest(ship->header.flags,SOF_Crazy|SOF_Hide|SOF_Hyperspace|SOF_Disabled))
            {
                // within the radius?
                if (aiuFindDistanceSquared(position, ship->obj.posinfo.position) <= rangeSquared)
                {
                    // add it
                    growSelectAddShip(ships, ship);
                }
            }
        }

        node = node->next;
    }

    return ships->selection->numShips;
}

//
//  combine FindShipsInside and FindEnemiesNearby because they commonly go together
//
sdword kasfFindEnemiesInside(Volume *volume, GrowSelection *ships, sdword neighborRadius)  // KAS FindEnemiesInside, cataclysm.exe 0x00493280
{
    return kasfFindEnemyShipsInside(volume, ships);
    //    return kasfFindEnemiesNearby(ships, neighborRadius);
    //else
    //    return 0;
}

// CAT: folded into the empty stub 0x0049ae30
void kasfTeamSkillSet(sdword skillLevel)
{
    (void)skillLevel;
}

// CAT: folded into the stub 0x006fe9a0 (returns 0)
sdword kasfTeamSkillGet(void)
{
    return 0;
}

void kasfShipsAttributesBitSet(GrowSelection *ships,sdword attributes)  // KAS ShipsAttributesBitSet, cataclysm.exe 0x004932a0
{
    SelectCommand *selection = ships->selection;
    sdword numShips = selection->numShips;
    sdword i;

    for (i=0;i<numShips;i++)
    {
        bitSet(selection->ShipPtr[i]->obj.attributes,attributes);
    }
}

// CAT: 0x004932d0: clearing the closed-bays bit (CAT_ATTRIBUTES_DockBaysClosed) of a host that had it launches every
//      ship inside it whose launch is due (the host's auto-launch is on or the ship must launch, and the host is not held
//      until an explicit launch)
void kasfShipsAttributesBitClear(GrowSelection *ships,sdword attributes)
{
    SelectCommand *selection = ships->selection;
    sdword numShips = selection->numShips;
    sdword i;
    Ship *host, *inside;
    bool32 reopened;
    Node *node;
    MaxSelection launch;

    for (i=0;i<numShips;i++)
    {
        host = selection->ShipPtr[i];
        reopened = bitTest(attributes, CAT_ATTRIBUTES_DockBaysClosed) && bitTest(host->obj.attributes, CAT_ATTRIBUTES_DockBaysClosed);
        bitClear(host->obj.attributes,attributes);
        if (reopened)
        {
            launch.numShips = 0;
            if (host->shipsInsideMe != NULL)
            {
                for (node = host->shipsInsideMe->insideList.head; node != NULL; node = node->next)
                {
                    inside = ((InsideShip *)listGetStructOfNode(node))->ship;
                    if ((cat_lmShipAutoLaunchOn(host) || ShipHasToLaunch(host, inside)) &&
                        !bitTest(host->shipOrDerelict.specialFlags, SPECIAL_STAY_TILL_EXPLICITLAUNCH))
                    {
                        launch.ShipPtr[launch.numShips++] = inside;
                    }
                }
                if (launch.numShips > 0)
                {
                    clLaunchMultipleShips(&universe.mainCommandLayer, (SelectCommand *)&launch, host);
                }
            }
        }
    }
}

void kasfShipsAttributesSet(GrowSelection *ships,sdword attributes)  // KAS ShipsAttributesSet, cataclysm.exe 0x004933d0
{
    SelectCommand *selection = ships->selection;
    sdword numShips = selection->numShips;
    sdword i;

    for (i=0;i<numShips;i++)
    {
        selection->ShipPtr[i]->obj.attributes = attributes;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipUnderConstruction                         0x00493400
    Description : CAT: KAS "ShipUnderConstruction": the first ship's being-built bit (no test for an empty selection)
----------------------------------------------------------------------------*/
sdword kasfShipUnderConstruction(GrowSelection *ships)
{
    return bitTest(ships->selection->ShipPtr[0]->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt) ? 1 : 0;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsParalyze                                 0x00493420
    Description : CAT: KAS "ShipsParalyze": sets or clears each ship's paralysed bit (bit 0 of paralyze)
----------------------------------------------------------------------------*/
void kasfShipsParalyze(GrowSelection *ships, sdword paralyze)
{
    sdword i;

    for (i = 0; i < ships->selection->numShips; i++)
    {
        Ship *ship = ships->selection->ShipPtr[i];
        ship->shipOrDerelict.specialFlags2 = (ship->shipOrDerelict.specialFlags2 & ~SPECIAL_2_CatParalyzed) |
                                             ((udword)(paralyze & 1) << 14);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfTimesHitByEMP                                 0x00493460
    Description : CAT: KAS "TimesHitByEMP": the sum of the ships' EMP shock counts
----------------------------------------------------------------------------*/
sdword kasfTimesHitByEMP(GrowSelection *ships)
{
    sdword i, count = 0;

    for (i = 0; i < ships->selection->numShips; i++)
    {
        count += ships->selection->ShipPtr[i]->catEmpShockCount;
    }
    return count;
}

void kasfTeamAttributesBitSet(sdword attributes)  // KAS TeamAttributesBitSet, cataclysm.exe 0x00493490
{
    sdword i, numShips;
    Ship *shipP;

    if (!CurrentTeamP)
        return;

    numShips = CurrentTeamP->shipList.selection->numShips;

    for (i = 0; i < numShips; ++i)
    {
        shipP = CurrentTeamP->shipList.selection->ShipPtr[i];
        bitSet(shipP->obj.attributes,attributes);
    }
}

void kasfTeamAttributesBitClear(sdword attributes)  // KAS TeamAttributesBitClear, cataclysm.exe 0x004934d0
{
    sdword i, numShips;
    Ship *shipP;

    if (!CurrentTeamP)
        return;

    numShips = CurrentTeamP->shipList.selection->numShips;

    for (i = 0; i < numShips; ++i)
    {
        shipP = CurrentTeamP->shipList.selection->ShipPtr[i];
        bitClear(shipP->obj.attributes,attributes);
    }
}

void kasfTeamAttributesSet(sdword attributes)  // KAS TeamAttributesSet, cataclysm.exe 0x00493510
{
    sdword i, numShips;
    Ship *shipP;

    if (!CurrentTeamP)
        return;

    numShips = CurrentTeamP->shipList.selection->numShips;

    for (i = 0; i < numShips; ++i)
    {
        shipP = CurrentTeamP->shipList.selection->ShipPtr[i];
        shipP->obj.attributes = attributes;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfTeamAttributesBitTest                         0x00493550
    Description : CAT: KAS "TeamAttributesBitTest": the given attribute bits that any ship of the current team has
----------------------------------------------------------------------------*/
sdword kasfTeamAttributesBitTest(sdword attributes)
{
    sdword i;
    udword bits = 0;

    if (!CurrentTeamP)
        return 0;

    for (i = 0; i < CurrentTeamP->shipList.selection->numShips; i++)
    {
        bits |= CurrentTeamP->shipList.selection->ShipPtr[i]->obj.attributes & (udword)attributes;
    }
    return (sdword)bits;
}

void kasfTeamMakeCrazy(sdword makecrazy)  // KAS TeamMakeCrazy, cataclysm.exe 0x00493580
{
    sdword i, numShips;
    Ship *shipP;

    if (!CurrentTeamP)
        return;

    numShips = CurrentTeamP->shipList.selection->numShips;

    for (i = 0; i < numShips; ++i)
    {
        shipP = CurrentTeamP->shipList.selection->ShipPtr[i];
        if (makecrazy)
        {
            ApplyCareenRotationDirectly(shipP);
            bitSet(shipP->header.flags,SOF_Crazy);
        }
        else
        {
            bitClear(shipP->header.flags,SOF_Crazy);
        }
    }
}

void kasfDisablePlayerHyperspace(void)  // KAS DisablePlayerHyperspace, cataclysm.exe 0x004935e0
{
    singlePlayerGameInfo.hyperspaceFails = TRUE;
}

void kasfHoldHyperspaceWindow(bool32 hold)  // KAS HoldHyperspaceWindow, cataclysm.exe 0x004935f0
{
    spHoldHyperspaceWindow = hold;
}

void kasfTeamHyperspaceIn(hvector *destination)  // KAS TeamHyperspaceIn, cataclysm.exe 0x00493600
{
    if (!CurrentTeamP)
        return;

    spHyperspaceSelectionIn(CurrentTeamP->shipList.selection,destination);
    spHyperspaceDelay = 0.0f;
}

// CAT: 0x00493630: the standoff point is kept inside the universe (each coordinate clamped to +-half the universe size,
//      smUniverseSize*)
void kasfTeamHyperspaceInNear(hvector *destination, sdword distance)
{
    vector origin = {0.0,0.0,0.0};
    vector dest, hypepoint;
    hvector finalHyperspacePoint;

    if (!CurrentTeamP)
        return;

    vecGrabVecFromHVec(dest, *destination);
    hypepoint = aiuGenerateRandomStandoffPoint(dest, (real32)distance, origin, RSP_NORMAL);
    hypepoint.x = (hypepoint.x > smUniverseSizeX * 0.5f) ? smUniverseSizeX * 0.5f : ((hypepoint.x < smUniverseSizeX * -0.5f) ? smUniverseSizeX * -0.5f : hypepoint.x);
    hypepoint.y = (hypepoint.y > smUniverseSizeY * 0.5f) ? smUniverseSizeY * 0.5f : ((hypepoint.y < smUniverseSizeY * -0.5f) ? smUniverseSizeY * -0.5f : hypepoint.y);
    hypepoint.z = (hypepoint.z > smUniverseSizeZ * 0.5f) ? smUniverseSizeZ * 0.5f : ((hypepoint.z < smUniverseSizeZ * -0.5f) ? smUniverseSizeZ * -0.5f : hypepoint.z);
    vecMakeHVecFromVec(finalHyperspacePoint, hypepoint);

    spHyperspaceSelectionIn(CurrentTeamP->shipList.selection, &finalHyperspacePoint);
    spHyperspaceDelay = 0.0f;
}

// CAT: 0x004937e0: milliseconds times 0.001f (0x0072a650) on the x87 stack, where HW1 divided by 1000
void kasfHyperspaceDelay(sdword milliseconds)
{
    spHyperspaceDelay = (real32)((real64)milliseconds * (real64)0.001f);
}

void kasfTeamHyperspaceOut(void)  // KAS TeamHyperspaceOut, cataclysm.exe 0x00493800
{
    MaxSelection filtered;

    if (!CurrentTeamP)
        return;

    filtered.numShips = 0;

    aiuFilterDisabledShips(CurrentTeamP->shipList.selection, &filtered);

    RemoveShipsFromDoingStuff(&universe.mainCommandLayer,(SelectCommand *)&filtered,0x13);   // CAT: reason 0x13 as the exe (kasfTeamHyperspaceOut 0x00493825) [audit: M2-R5 command-tactics]
    spHyperspaceSelectionOut((SelectCommand *)&filtered);
    spHyperspaceDelay = 0.0f;
}

void kasfTeamSetMaxVelocity(sdword maxVelocity)  // KAS TeamVelocityMaxSet, cataclysm.exe 0x00493860
{
    sdword i, numShips;
    Ship *shipP;

    if (!CurrentTeamP)
        return;
    numShips = CurrentTeamP->shipList.selection->numShips;

    for (i = 0; i < numShips; ++i)
    {
        shipP = CurrentTeamP->shipList.selection->ShipPtr[i];
        shipP->singlePlayerSpeedLimiter = (real32)maxVelocity;
        bitSet(shipP->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerLimitSpeed);
    }
}

void kasfTeamClearMaxVelocity(void)  // KAS TeamVelocityMaxClear, cataclysm.exe 0x004938b0
{
    sdword i, numShips;
    Ship *shipP;

    if (!CurrentTeamP)
        return;
    numShips = CurrentTeamP->shipList.selection->numShips;

    for (i = 0; i < numShips; ++i)
    {
        shipP = CurrentTeamP->shipList.selection->ShipPtr[i];
        bitClear(shipP->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerLimitSpeed);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSetMaxVelocity
    Description : Same as TeamSetMaxVelocity, except that it works on arbitrary
                    ship selections.
    Inputs      : ships - ships to cap velocity of, be they friendly or enemy.
                  maxVelocity - velocity to cap them at.
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void kasfShipsSetMaxVelocity(GrowSelection *ships, sdword maxVelocity)  // KAS ShipsVelocityMaxSet, cataclysm.exe 0x004938f0
{
    sdword i, numShips;
    Ship *shipP;

    if (!ships)
        return;
    numShips = ships->selection->numShips;

    for (i = 0; i < numShips; ++i)
    {
        shipP = ships->selection->ShipPtr[i];
        shipP->singlePlayerSpeedLimiter = (real32)maxVelocity;
        bitSet(shipP->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerLimitSpeed);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsClearMaxVelocity
    Description : Similar to TeamClearMaxVelocity, except that it works on an
                    arbitrary selection of ships.
    Inputs      : ships - ships to cap velocity of, be they friendly or enemy.
                  maxVelocity - velocity to cap them at.
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
// CAT: one argument, the ships (the function table's "ShipsVelocityMaxClear"; HW1's prototype had an unused second)
void kasfShipsClearMaxVelocity(GrowSelection *ships)  // KAS ShipsVelocityMaxClear, cataclysm.exe 0x00493940
{
    sdword i, numShips;
    Ship *shipP;

    if (!ships)
        return;
    numShips = ships->selection->numShips;

    for (i = 0; i < numShips; ++i)
    {
        shipP = ships->selection->ShipPtr[i];
        bitClear(shipP->shipOrDerelict.specialFlags,SPECIAL_SinglePlayerLimitSpeed);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSetDamageFactor
    Description : Sets a modifier in the ship structure to
                    modify the ammount of damage a ship does.
    Inputs      : ships - ships to modify damage of.
                  damagePercent (0 to 100) - divided by 100 then multiplied by ships damage
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
// CAT: 0x00493980: the factor is percent times 0.01f (0x0072a5b8, computed once on the x87 stack) and goes to the
//      ship's damage factor (+0x3E0, which gunShoot / missileShoot read; HW1's damageModifier +0x3DC is the KAS repair
//      factor in Cataclysm); no ships with an empty selection
void kasfShipsSetDamageFactor(GrowSelection *ships, sdword damagePercent)
{
    sdword i, numShips;
    Ship *shipP;
    real32 factor;

    if (!ships)
        return;
    numShips = ships->selection->numShips;
    factor = (real32)((real64)damagePercent * (real64)0.01f);

    for (i = 0; i < numShips; ++i)
    {
        shipP = ships->selection->ShipPtr[i];
        shipP->catDamageFactor = factor;
        bitSet(shipP->shipOrDerelict.specialFlags,SPECIAL_KasSpecialDamageModifier);
    }
}
/*-----------------------------------------------------------------------------
    Name        : kasfShipsClearDamageFactor
    Description : Sets a modifier in the ship structure to
                    modify the ammount of damage a ship does.
    Inputs      : ships - ships to modify damage of.
                  damagePercent (0 to 100) - divided by 100 then multiplied by ships damage
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void kasfShipsClearDamageFactor(GrowSelection *ships)  // KAS ShipsDamageModifierClear, cataclysm.exe 0x004939d0
{
    sdword i, numShips;
    Ship *shipP;

    if (!ships)
        return;
    numShips = ships->selection->numShips;

    for (i = 0; i < numShips; ++i)
    {
        shipP = ships->selection->ShipPtr[i];
        bitClear(shipP->shipOrDerelict.specialFlags,SPECIAL_KasSpecialDamageModifier);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSetRepairFactor                          0x00493a10
    Description : CAT: KAS "ShipsRepairModifierSet": the ships' repair factor (+0x3DC, HW1's damageModifier field, which
                  the repair code multiplies by) is percent * 0.01f and its special flag is set
----------------------------------------------------------------------------*/
void kasfShipsSetRepairFactor(GrowSelection *ships, sdword repairModifier)
{
    sdword i;
    real32 factor;

    if (!ships)
        return;
    factor = (real32)((real64)repairModifier * (real64)0.01f);
    for (i = 0; i < ships->selection->numShips; i++)
    {
        ships->selection->ShipPtr[i]->damageModifier = factor;
        bitSet(ships->selection->ShipPtr[i]->shipOrDerelict.specialFlags, SPECIAL_CatKasSpecialRepairModifier);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsClearRepairFactor                        0x00493a60
    Description : CAT: KAS "ShipsRepairModifierClear": clears the flag (the factor stays)
----------------------------------------------------------------------------*/
void kasfShipsClearRepairFactor(GrowSelection *ships)
{
    sdword i;

    if (!ships)
        return;
    for (i = 0; i < ships->selection->numShips; i++)
    {
        bitClear(ships->selection->ShipPtr[i]->shipOrDerelict.specialFlags, SPECIAL_CatKasSpecialRepairModifier);
    }
}


// this function is a bit of a misnomer: it actually sets the percentage of health
// therefore 0 "percentDamaged", actually means the health is at 0 and the ship is dead
// CAT: 0x00493aa0: health = percent * maxhealth * 0.01f on the x87 stack (HW1: maxhealth * percent / 100)
void kasfTeamSetPercentDamaged(sdword percentDamaged)
{
    sdword i, numShips;

    if (!CurrentTeamP)
        return;
    numShips = CurrentTeamP->shipList.selection->numShips;

    for (i = 0; i < numShips; ++i)
        CurrentTeamP->shipList.selection->ShipPtr[i]->targetable.health =
            (real32)((real64)percentDamaged * (real64)CurrentTeamP->shipList.selection->ShipPtr[i]->staticinfo->health.maxhealth * (real64)0.01f);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSetPercentDamaged                        0x00493af0
    Description : CAT: KAS "ShipsHealthSet": each ship's health is percent of its maximum (as kasfTeamSetPercentDamaged)
----------------------------------------------------------------------------*/
void kasfShipsSetPercentDamaged(GrowSelection *ships, sdword percentHealth)
{
    sdword i;

    for (i = 0; i < ships->selection->numShips; i++)
    {
        ships->selection->ShipPtr[i]->targetable.health =
            (real32)((real64)percentHealth * (real64)ships->selection->ShipPtr[i]->staticinfo->health.maxhealth * (real64)0.01f);
    }
}

// CAT: 0x00493b30: fuel = percent * maxfuel * 0.01f on the x87 stack (HW1: maxfuel * percent / 100)
void kasfTeamSetPercentFueled(sdword percentFueled)
{
    sdword i, numShips;

    if (!CurrentTeamP)
        return;
    numShips = CurrentTeamP->shipList.selection->numShips;

    for (i = 0; i < numShips; ++i)
        CurrentTeamP->shipList.selection->ShipPtr[i]->fuel =
            (real32)((real64)percentFueled * (real64)CurrentTeamP->shipList.selection->ShipPtr[i]->staticinfo->maxfuel * (real64)0.01f);
}

// CAT (M2-R8 singleplayer-tutorial): the KAS skill level is the single-player difficulty word 0x008941a0
//      (catSpDifficultySetting); HW1 kept a separate CurrentMissionSkillLevel (0 here, which made
//      cat_shipGetDamageMultiplier double every single-player ship's damage)
void kasfMissionSkillSet(sdword skillLevel)  // KAS MissionSkillSet, cataclysm.exe 0x00493b80
{
    catSpDifficultySetting = (udword)skillLevel;
}

sdword kasfMissionSkillGet(void)  // KAS MissionSkillGet, cataclysm.exe 0x00493b90
{
    return (sdword)catSpDifficultySetting;
}

// CAT: 0x00493ba0: the temporary team's label is the current team's label hash + 1 (HW1: "R" + the label)
void kasfRequestShips(char *shipType, sdword numShips)
{
    AITeam *teamp;
    ShipType st = StrToShipType(shipType);

    // make a temp team and get them to reinforce the first team
    teamp = aitCreate(ScriptTeam);
    teamp->kasLabel = CurrentTeamP->kasLabel + 1;
    teamp->kasOrigShipsCount = 0;
    aimCreateGetShips(teamp, st, (sbyte)numShips, (sdword)REQUESTSHIPS_HIPRI, TRUE, TRUE);
    aimCreateReinforce(teamp, CurrentTeamP, SAME_FORMATION, CurrentTeamP->kasTactics, TRUE, TRUE);
    aimCreateDeleteTeam(teamp);
}

// CAT: 0x00493c00: count * percent (exact on the x87 stack) * 0.01f + 0.5f, truncated to a byte; the temporary team's
//      label hash is the current team's + 1 (HW1: "R" + the label)
void kasfRequestShipsOriginal(sdword percentOriginal)
{
    AITeam *teamp;
    sbyte numShips = (sbyte)(sdword)((real64)CurrentTeamP->kasOrigShipsCount * (real64)percentOriginal * (real64)0.01f + (real64)0.5f);
    if (numShips <= 0)
    {
        numShips = 1;
    }

    // make a temp team and get them to reinforce the first team
    teamp = aitCreate(ScriptTeam);
    teamp->kasLabel = CurrentTeamP->kasLabel + 1;
    teamp->kasOrigShipsCount = 0;
    aimCreateGetShips(teamp, CurrentTeamP->kasOrigShipsType, numShips, (sdword)REQUESTSHIPS_HIPRI, TRUE, TRUE);
    aimCreateReinforce(teamp, CurrentTeamP, SAME_FORMATION, CurrentTeamP->kasTactics, TRUE, TRUE);
    aimCreateDeleteTeam(teamp);
}

/*-----------------------------------------------------------------------------
    Name        : kasfReinforceTeamWithNewShips                     0x00493c90
    Description : CAT: KAS "ReinforceTeamWithNewShips": as kasfReinforceTeamWithShips, but the team's moves are deleted
                  first (so the current move it then reads is normally NULL)
----------------------------------------------------------------------------*/
void kasfReinforceTeamWithNewShips(struct AITeam *teamtoreinforce, GrowSelection *shipstoadd)
{
    sdword i;
    sdword numShips = shipstoadd->selection->numShips;
    AITeamMove *reinforceMove;

    aitDeleteAllTeamMoves(teamtoreinforce);
    reinforceMove = teamtoreinforce->curMove;
    for (i = 0; i < numShips; i++)
    {
        growSelectAddShip(&teamtoreinforce->shipList, shipstoadd->selection->ShipPtr[i]);
    }

    if (reinforceMove)
    {
        reinforceMove->processing = FALSE;
    }
}

void kasfReinforceTeamWithShips(struct AITeam *teamtoreinforce,GrowSelection *shipstoadd)  // KAS ReinforceTeamWithShips, cataclysm.exe 0x00493cf0
{
    sdword i;
    sdword numShips = shipstoadd->selection->numShips;
    AITeamMove *reinforceMove = teamtoreinforce->curMove;

    for (i=0;i<numShips;i++)
    {
        growSelectAddShip(&teamtoreinforce->shipList, shipstoadd->selection->ShipPtr[i]);
    }

    if (reinforceMove)
    {
        reinforceMove->processing = FALSE;
    }
}

void kasfReinforce(AITeam *team)  // KAS Reinforce, cataclysm.exe 0x00493d40
{
    aitDeleteAllTeamMoves(CurrentTeamP);
    aimCreateReinforce(CurrentTeamP, team, SAME_FORMATION, CurrentTeamP->kasTactics, TRUE, TRUE);
}

void kasfForceCombatStatus(GrowSelection *ships, sdword on)  // KAS ForceCombatStatus, cataclysm.exe 0x00493d70
{
    udword i;

    if (on)
    {
        for (i=0;i<ships->selection->numShips;i++)
        {
            bitSet(ships->selection->ShipPtr[i]->shipOrDerelict.specialFlags, SPECIAL_ForcedAttackStatus);
        }
    }
    else
    {
        for (i=0;i<ships->selection->numShips;i++)
        {
            bitClear(ships->selection->ShipPtr[i]->shipOrDerelict.specialFlags, SPECIAL_ForcedAttackStatus);
        }
    }
}

sdword kasfThisTeamIs(AITeam *team)  // KAS ThisTeamIs, cataclysm.exe 0x00493de0
{
    return (team == CurrentTeamP);
}

sdword kasfTeamCountOriginal(void)  // KAS TeamCountOriginal, cataclysm.exe 0x00493e00
{
    return CurrentTeamP->kasOrigShipsCount;
}

// CAT: 0x00493e10: a ship of class Mothership or Carrier becomes the AI's creator (HW1 tested the HW1 ship types)
void kasfTeamGiveToAI(void)
{
    Ship *ship;
    udword i;

    if (!CurrentTeamP)
        return;

    for (i=0;i<CurrentTeamP->shipList.selection->numShips;)
    {
        ship = CurrentTeamP->shipList.selection->ShipPtr[i];
        aitRemoveShip(CurrentTeamP, ship);
        growSelectAddShip(&aiCurrentAIPlayer->newships, ship);

        if (ship->staticinfo->shipclass == CLASS_Mothership || ship->staticinfo->shipclass == CLASS_Carrier)
        {
            aiCurrentAIPlayer->AICreator = ship;
        }
    }
}

void kasfDisableAIFeature(sdword feature, sdword type)  // KAS DisableAIFeature, cataclysm.exe 0x00493e70
{
    AIPlayer *aiplayer = aiCurrentAIPlayer;

    switch (type)
    {
        case ResourceFeature:
            aiuDisableResourceFeature(feature);
            break;
        case DefenseFeature:
            aiuDisableDefenseFeature(feature);
            break;
        case AttackFeature:
            aiuDisableAttackFeature(feature);
            break;
        case TeamFeature:
            aiuDisableTeamFeature(CurrentTeamP,feature);
            break;
        default:
            aiplayerLog((0, "Unknown Feature type being enabled %i, %i", feature, type));
            break;
    }
}
void kasfEnableAIFeature(sdword feature, sdword type)  // KAS EnableAIFeature, cataclysm.exe 0x00493ee0
{
    AIPlayer *aiplayer = aiCurrentAIPlayer;

    switch (type)
    {
        case ResourceFeature:
            aiuEnableResourceFeature(feature);
            break;
        case DefenseFeature:
            aiuEnableDefenseFeature(feature);
            break;
        case AttackFeature:
            aiuEnableAttackFeature(feature);
            break;
        case TeamFeature:
            aiuEnableTeamFeature(CurrentTeamP,feature);
            break;
        default:
            aiplayerLog((0, "Unknown Feature type being enabled %i, %i", feature, type));
            break;
    }
}

void kasfDisableAllAIFeatures(void)  // KAS DisableAllAIFeatures, cataclysm.exe 0x00493f40
{
    aiCurrentAIPlayer->ResourceFeatures = 0;
    aiCurrentAIPlayer->DefenseFeatures = 0;
    aiCurrentAIPlayer->AttackFeatures = 0;
//    aiCurrentAIPlayer->TeamFeatures = 0;
}
void kasfEnableAllAIFeatures(void)  // KAS EnableAllAIFeatures, cataclysm.exe 0x00493f60
{
    aiCurrentAIPlayer->ResourceFeatures = 0xFFFFFFFF;
    aiCurrentAIPlayer->DefenseFeatures = 0xFFFFFFFF;
    aiCurrentAIPlayer->AttackFeatures = 0xFFFFFFFF;
//    aiCurrentAIPlayer->TeamFeatures = 0xFFFFFFFF;
}

// CAT: 0x00493f80, with Cataclysm's second argument keepSU: the ships clamped to a ship switch with it (recursively);
//      a ship going to the computer player leaves the human's FOW, selection, hot keys, unit caps, names and support
//      units; a ship coming to the human player is disabled and made SU exempt (keepSU FALSE) or leaves the AI
//      (keepSU TRUE: it also becomes a factory if it can build, player 0's mothership if it is a Sect mothership and he
//      has none, and counts for his support units); its FOW cube, unit caps and name follow; the collision blobs are
//      rebuilt afterwards
void SelectionSwitchPlayerOwner(SelectCommand *selection, bool32 keepSU)
{
    sdword i;
    sdword numShips = selection->numShips;
    Ship *ship;

    RemoveShipsFromDoingStuff(&universe.mainCommandLayer,selection,0x14);   // CAT: reason 0x14 as the exe (SelectionSwitchPlayerOwner 0x00493f89) [audit: M2-R5 command-tactics]

    for (i=0;i<numShips;i++)
    {
        ship = selection->ShipPtr[i];
        if ((ship->guidance.clampInfo != NULL) && (ship->guidance.clampInfo->clampedObjs != NULL))
        {
            SelectionSwitchPlayerOwner((SelectCommand *)ship->guidance.clampInfo->clampedObjs, keepSU);
        }
        RemoveShipFromBeingTargeted(&universe.mainCommandLayer,ship,REMOVE_PROTECT,NULL);      // remove ships from being targeted when switching sides (CAT: fourth argument 0, SelectionSwitchPlayerOwner 0x00493fe1)
        if (ship->playerowner == &universe.players[0])
        {
            // going from human's ships to kas ships
            cat_univShipFOWDeactivate(ship);

            // Remove ships that are destroyed from any current selection
            clRemoveShipFromSelection((SelectCommand *)&selSelected,ship);

            // Remove ships that are destroyed from any hot key groups
            univRemoveShipFromHotkeyGroup(ship,FALSE);

            ship->obj.attributes |= ATTRIBUTES_Defector;
            ship->playerowner = &universe.players[1];
            bitClear(ship->header.flags,SOF_Selectable);
            unitCapCreateShip(ship, &universe.players[1]);
            unitCapDeleteShip(ship, &universe.players[0]);
            // CAT (M2-R5 hud): the ship gives its name back (cat_shipNamesRelease 0x0050f660, call at 0x00494042); the
            //      exe calls it after the owner is already player 1, so the owner test makes it a no-op (kept)
            cat_shipNamesRelease(ship);
            // CAT (M2-R6 managers): the support units (0x00494050..0x00494068): the ship stops counting for the human
            //      player unless it is SU exempt, and loses the exemption
            if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
            {
                cat_suShipRemoved(ship);
            }
            cat_shipSetSUExempt(ship, FALSE);
            // CAT: no DDDFrigateSwitchSides, Cataclysm has no drone frigate (SelectionSwitchPlayerOwner 0x00493f80)
        }
        else
        {
            // going from kas ships to human ships
            if (keepSU)
            {
                aiplayerShipDied(ship);
            }
            else
            {
                cat_shipSetDisableReason(ship, 1, 1);
                cat_shipSetSUExempt(ship, TRUE);
            }
            bitClear(ship->obj.attributes,ATTRIBUTES_Defector);
            ship->playerowner = &universe.players[0];
            bitClear(ship->header.flags,SOF_Disabled);
            bitSet(ship->header.flags,SOF_Selectable);
            if (ship->shiplink.belongto != NULL)
            {
                cat_univShipFOWActivate(ship);
            }
            unitCapCreateShip(ship, &universe.players[0]);
            unitCapDeleteShip(ship, &universe.players[1]);
            // CAT (M2-R5 hud): a named type taken over gets a free name (cat_shipNamesAssign 0x0050f320, call at
            //      0x004940dd after the unit caps)
            cat_shipNamesAssign(ship);
            if (keepSU)
            {
                if (ship->staticinfo->canBuildShips)
                {
                    cmAddFactory(ship, (bool32)ship->staticinfo->canBuildBigShips);
                }
                if ((ship->shiptype == sMothership) && (universe.curPlayerPtr->PlayerMothership == NULL))
                {
                    universe.curPlayerPtr->PlayerMothership = ship;
                }
                // CAT (M2-R6 managers): the ship counts for the human player's support units (cat_suShipAdded
                //      0x00459a90, call at 0x00494151)
                cat_suShipAdded(ship, &universe.players[0]);
            }
            // CAT: no DDDFrigateSwitchSides (the type is gone; SelectionSwitchPlayerOwner 0x00493f80)
        }
    }
    universe.collUpdateAllBlobs = TRUE;
}

// CAT: 0x00494160: Cataclysm's argument says whether the ships keep their support units (SelectionSwitchPlayerOwner)
void kasfTeamSwitchPlayerOwner(sdword bMakeControllable)
{
    if (!CurrentTeamP)
        return;

    SelectionSwitchPlayerOwner(CurrentTeamP->shipList.selection, bMakeControllable);
}

// CAT: 0x00494180 (the same second argument)
void kasfShipsSwitchPlayerOwner(GrowSelection *ships, sdword bMakeControllable)
{
    SelectionSwitchPlayerOwner(ships->selection, bMakeControllable);
}

void kasfMoveTo(hvector *destination)  // KAS MoveTo, cataclysm.exe 0x004941a0
{
    vector dest;
    vecGrabVecFromHVec(dest,*destination);
    aitDeleteAllTeamMoves(CurrentTeamP);
    aimCreateMoveTeam(CurrentTeamP, dest, SAME_FORMATION, FALSE, TRUE);
}

void kasfShipsMoveTo(GrowSelection *ships, hvector *destination)  // KAS ShipsMoveTo, cataclysm.exe 0x00494200
{
    vector dest;
    vecGrabVecFromHVec(dest,*destination);
    aiuWrapMove(ships->selection, dest);
}

/*-----------------------------------------------------------------------------
    Name        : kasfSetTactics                                    0x00494250
    Description : CAT: KAS "SetTactics": the current team's tactics (stored and given to its ships)
----------------------------------------------------------------------------*/
void kasfSetTactics(sdword Tactics)
{
    CurrentTeamP->kasTactics = (TacticsType)Tactics;
    aiuWrapSetTactics(CurrentTeamP->shipList.selection, (TacticsType)Tactics);
}

void kasfTacticsAggressive(void)  // KAS TacticsAggressive, cataclysm.exe 0x00494280
{
    CurrentTeamP->kasTactics = Aggressive;
    aiuWrapSetTactics(CurrentTeamP->shipList.selection, Aggressive);
}

void kasfTacticsNeutral(void)  // KAS TacticsNeutral, cataclysm.exe 0x004942b0
{
    CurrentTeamP->kasTactics = Neutral;
    aiuWrapSetTactics(CurrentTeamP->shipList.selection, Neutral);
}

void kasfTacticsEvasive(void)  // KAS TacticsEvasive, cataclysm.exe 0x004942e0
{
    CurrentTeamP->kasTactics = Evasive;
    aiuWrapSetTactics(CurrentTeamP->shipList.selection, Evasive);
}

//
//  returns True if any ship in the team is a given distance from the given location
//
sdword kasfNearby(hvector *location, sdword distance)  // KAS Nearby, cataclysm.exe 0x00494310
{
    real32 rangeSquared = (real32)distance*(real32)distance;
    sdword i, numShips;
    vector loc;
    vecGrabVecFromHVec(loc,*location);

    numShips = CurrentTeamP->shipList.selection->numShips;
    for (i = 0; i < numShips; ++i)
    {
        if (aiuFindDistanceSquared(CurrentTeamP->shipList.selection->ShipPtr[i]->obj.posinfo.position, loc) <= rangeSquared)
            return TRUE;
    }
    return FALSE;
}

//
// returns the distance between two points
//
sdword kasfFindDistance(hvector *location1, hvector *location2)  // KAS FindDistance, cataclysm.exe 0x004943d0
{
    vector loc1, loc2;
    real32 distsq;

    vecGrabVecFromHVec(loc1, *location1);
    vecGrabVecFromHVec(loc2, *location2);

    distsq = aiuFindDistanceSquared(loc1, loc2);

    return (sdword)fsqrt(distsq);
}


// CAT: 0x00494450: every ship of an attack command (an attack order, or an order with the passive-attacking /
//      attacking-and-moving attributes) whose targets include one of the team's ships is an attacker; no test of a
//      visible team member or of the attacker's distance (HW1: MoveReachedDestinationVariable within 10,000 of the
//      first visible member)
sdword kasfUnderAttackElsewhere(struct AITeam *otherTeam, GrowSelection *attackers)
{
    sdword i;
    SelectCommand *teamSelection = otherTeam->shipList.selection;
    Node *curnode = universe.mainCommandLayer.todolist.head;
    CommandToDo *command;

    if (attackers == NULL)
        return 0;

    //  wipe the selection clean
    attackers->selection->numShips = 0;

    if (teamSelection->numShips == 0)
        return 0;

    // lets walk the command layer and see who's attacking us:
    while (curnode != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);

        if ((command->ordertype.order == COMMAND_ATTACK) ||
            (command->ordertype.attributes & (COMMAND_MASK_PASSIVE_ATTACKING|COMMAND_MASK_ATTACKING_AND_MOVING)))
        {
            if (command->attack)
            {
                if (AnyOfTheseShipsAreInSelection(teamSelection,(SelectCommand *)command->attack))
                {
                    SelectCommand *attackedby = command->selection;
                    for (i = 0;i < attackedby->numShips; i++)
                    {
                        growSelectAddShipNoDuplication(attackers,attackedby->ShipPtr[i]);
                    }
                }
            }
        }

        curnode = curnode->next;
    }

    return attackers->selection->numShips;
}

sdword kasfUnderAttack(GrowSelection *attackers)  // KAS UnderAttack, cataclysm.exe 0x00494510
{
    return kasfUnderAttackElsewhere(CurrentTeamP, attackers);
}

sdword kasfShipsCountType(GrowSelection *ships, char *shipType)  // KAS ShipsCountType, cataclysm.exe 0x00494530
{
    sdword i, count = 0, numShips = ships->selection->numShips;
    ShipType st = StrToShipType(shipType);
//    if (st < 0)
//        return 0;
    for (i = 0; i < numShips; ++i)
        if (ships->selection->ShipPtr[i]->shiptype == st)
            ++count;
    return count;
}

void kasfDock(struct AITeam *withTeam)
{
    aitDeleteAllTeamMoves(CurrentTeamP);
    aimCreateDock(CurrentTeamP, dockmoveFlags_Normal, NULL, FALSE, TRUE);
}

// CAT: folded with kasfDock into 0x00494570 (identical bodies)
void kasfDockSupport(void)
{
    aitDeleteAllTeamMoves(CurrentTeamP);
    aimCreateDock(CurrentTeamP, dockmoveFlags_Normal, NULL, FALSE, TRUE);
}

void DockSupportWith(struct AITeam * withTeam,sdword flags)
{
    Ship *dockwith = NULL;
    if (withTeam->shipList.selection->numShips >= 1)
    {
        dockwith = withTeam->shipList.selection->ShipPtr[0];
        dbgAssertOrIgnore(dockwith);
        if (aiuShipIsntSelectable(dockwith))
        {
            dockwith = NULL;
        }
        else
        {
            flags |= dockmoveFlags_AtShip;
        }
    }

    aitDeleteAllTeamMoves(CurrentTeamP);
    aimCreateDock(CurrentTeamP, flags, dockwith, FALSE, TRUE);
}

void kasfDockSupportWith(struct AITeam * withTeam)  // KAS DockSupportWith, cataclysm.exe 0x004945a0
{
    DockSupportWith(withTeam,0);
}

void kasfShipsDockSupportWith(GrowSelection *ships, GrowSelection *withShips)
{
    aiuWrapDock(ships->selection, CAT_DOCK_SUPPORT, withShips->selection->ShipPtr[0]);  // CAT: purpose 0 (kasfShipsDockSupportWith 0x00494610)
}

void kasfDockStay(struct AITeam * withTeam)  // KAS DockStay, cataclysm.exe 0x00494630
{
    DockSupportWith(withTeam,dockmoveFlags_Stay);
}

void kasfShipsDockStay(GrowSelection *ships, GrowSelection *withShips)
{
    // CAT: kasfShipsDockStay 0x004946a0: Mission 9's Taiidan resource collectors retire (purpose 7) instead of staying
    SelectCommand *selection = ships->selection;

    if ((selection->numShips > 0) && (selection->ShipPtr[0]->shiptype == tResourceCollector) &&
        (singlePlayerGameInfo.currentMission == 9))
    {
        aiuWrapDock(selection, CAT_DOCK_RETIRE, withShips->selection->ShipPtr[0]);
        return;
    }
    aiuWrapDock(selection, CAT_DOCK_STAY, withShips->selection->ShipPtr[0]);
}

void kasfDockStayMothership(void)  // KAS DockStayMothership, cataclysm.exe 0x004946f0
{
    Ship *dockwith;

    if (CurrentTeamP->shipList.selection->numShips >= 1)
    {
        dockwith = CurrentTeamP->shipList.selection->ShipPtr[0]->playerowner->PlayerMothership;
        if ((dockwith) && (!aiuShipIsntSelectable(dockwith)))
        {
            aitDeleteAllTeamMoves(CurrentTeamP);
            aimCreateDock(CurrentTeamP, dockmoveFlags_AtShip|dockmoveFlags_Stay, dockwith, FALSE, TRUE);
        }
    }
}

void kasfDockInstant(struct AITeam * withTeam)  // KAS DockInstant, cataclysm.exe 0x00494750
{
    //put in print command
    udword i;

    for (i=0;i<CurrentTeamP->shipList.selection->numShips;i++)
    {
        aiplayerLog((0, "Docking Instant Ship Type %i", CurrentTeamP->shipList.selection->ShipPtr[i]->shiptype));
    }
    DockSupportWith(withTeam,dockmoveFlags_Stay|dockmoveFlags_Instantaneously);
}

void kasfLaunch(void)  // KAS Launch, cataclysm.exe 0x004947c0
{
    udword i;

    for (i=0;i<CurrentTeamP->shipList.selection->numShips;i++)
    {
        bitSet(CurrentTeamP->shipList.selection->ShipPtr[i]->shipOrDerelict.specialFlags, SPECIAL_LaunchedFromKas|SPECIAL_KasCheckDoneLaunching);
    }

    aitDeleteAllTeamMoves(CurrentTeamP);
    aimCreateLaunch(CurrentTeamP, FALSE, TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsLaunch                                   0x00494830
    Description : CAT: KAS "ShipsLaunch": the ships are marked launched by KAS (as kasfLaunch) and launch from the host's
                  first ship (dock purpose 2)
----------------------------------------------------------------------------*/
void kasfShipsLaunch(GrowSelection *ships, GrowSelection *host)
{
    sdword i;
    Ship *dockwith = host->selection->ShipPtr[0];

    for (i = 0; i < ships->selection->numShips; i++)
    {
        bitSet(ships->selection->ShipPtr[i]->shipOrDerelict.specialFlags, SPECIAL_LaunchedFromKas);
        bitSet(ships->selection->ShipPtr[i]->shipOrDerelict.specialFlags, SPECIAL_KasCheckDoneLaunching);
    }
    aiuWrapDock(ships->selection, CAT_DOCK_LAUNCH, dockwith);
}

sdword kasfTeamDocking(void)  // KAS TeamDocking, cataclysm.exe 0x00494890
{
    //return bitTest(CurrentTeamP->teamFlags, TEAM_DOCKING);
    return ((CurrentTeamP->teamFlags & TEAM_DOCKING) != 0);  // ensure 0/1 return value
}

bool32 ShipReadyForLaunch(Ship *ship)
{
    if ((ship->header.flags & (SOF_Hide | SOF_Hyperspace)) == SOF_Hide)
    {
        if (ShipIsRefuelingAtCarrierMother(ship))
            return FALSE;       // wait, refueling, we're not done yet!
        else
            return TRUE;        // ship definitely ready for launch, it is inside another ship
    }
    else if ((ship->shipOrDerelict.specialFlags & SPECIAL_STAY_TILL_EXPLICITLAUNCH) && (ShipIsWaitingForSoftLaunch(ship)))
    {
        return TRUE;
    }
    return FALSE;
}

// CAT: inlined in kasfTeamFinishedLaunching 0x004948a0 / kasfShipsFinishedLaunching 0x004948f0: a ship has finished
//      launching once KAS's launch check is clear and it is back in the universe's ship list (shiplink.belongto)
bool32 ShipFinishedLaunching(Ship *ship)
{
    if (ship->shipOrDerelict.specialFlags & SPECIAL_KasCheckDoneLaunching)
    {
        return FALSE;
    }

    if (ship->shiplink.belongto == NULL)
    {
        return FALSE;
    }

    return TRUE;
}

sdword kasfTeamFinishedLaunching(void)
{
    SelectCommand *selection = CurrentTeamP->shipList.selection;
    sdword i;

    if (selection->numShips == 0)
    {
        return FALSE;
    }

    for (i=0;i<selection->numShips;i++)
    {
        if (!ShipFinishedLaunching(selection->ShipPtr[i]))
        {
            return FALSE;
        }
    }
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsFinishedLaunching                        0x004948f0
    Description : CAT: KAS "ShipsFinishedLaunching": TRUE when every ship has finished launching (also for no ships)
----------------------------------------------------------------------------*/
sdword kasfShipsFinishedLaunching(GrowSelection *Ships)
{
    sdword i;

    for (i = 0; i < Ships->selection->numShips; i++)
    {
        if (!ShipFinishedLaunching(Ships->selection->ShipPtr[i]))
        {
            return FALSE;
        }
    }
    return TRUE;
}

sdword kasfTeamDockedReadyForLaunch(void)  // KAS TeamDockedReadyForLaunch, cataclysm.exe 0x00494940
{
    SelectCommand *selection = CurrentTeamP->shipList.selection;
    sdword i;

    if (selection->numShips == 0)
    {
        return FALSE;
    }

    for (i=0;i<selection->numShips;i++)
    {
        if (!ShipReadyForLaunch(selection->ShipPtr[i]))
        {
            return FALSE;
        }
    }
    return TRUE;
}

//
//  send to all script teams
//
// CAT: 0x004949b0: the message is its hash
void kasfMsgSendAll(udword msg)
{
    AITeam *teamp;
    sdword i;

    for (i = 0; i < aiCurrentAIPlayer->teamsUsed; i++)
    {
        teamp = aiCurrentAIPlayer->teams[i];
          //not really sure we need this...  CPUTeams deserve to get messages too.
//        if ((teamp->teamType == ScriptTeam) || (teamp->teamType == AnyTeam))
            aitMsgSend(CurrentTeamP, teamp, msg);
    }
}

// CAT: 0x00494a00: the message is its hash
void kasfMsgSend(struct AITeam *team, udword msg)
{
    aitMsgSend(CurrentTeamP, team, msg);
}

/*-----------------------------------------------------------------------------
    Name        : kasfMsgQueueFlush                                 0x00494a20
    Description : CAT: KAS "MsgQueueFlush": the team forgets its message queue (cat_aitMsgQueueFlush)
----------------------------------------------------------------------------*/
void kasfMsgQueueFlush(struct AITeam *team)
{
    cat_aitMsgQueueFlush(team);
}

sdword kasfRUsEnemyCollected(void)  // KAS RUsEnemyCollected, cataclysm.exe 0x00494a30
{
    return singlePlayerGameInfo.resourceUnitsCollected;
}

//  serves the "friendly" and "enemy" functions below
static sdword kasfFindPlayersShipsOfType(GrowSelection *ships, char *shipType, sdword playerIndex)
{
    Node *node;
    ShipType st = StrToShipType(shipType);
    Ship *ship;

//    if (st < 0)
//        return 0;

    if (ships == NULL || ships->selection == NULL)
        return 0;

    //  wipe the selection clean
    ships->selection->numShips = 0;

    //  check all ships in the damn universe
    node = universe.ShipList.head;
    while (node != NULL)
    {
        ship = (Ship *)listGetStructOfNode(node);

        if ((ship->header.flags & (SOF_Dead|SOF_Hide)) == 0)
        {
            //  1. is it an enemy?  (simplifying assumption: "is it the human"?)
            //  2. is the right type?
            // CAT: a ship still being built is not counted (kasfFindEnemyShipsOfType 0x00494a40,
            //      kasfFindFriendlyShipsOfType 0x00494ac0: specialFlags2 & 8)
            if (ship->playerowner->playerIndex == playerIndex && ship->shiptype == st &&
                !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
                // add it
                growSelectAddShip(ships, ship);
        }
        node = node->next;
    }

    return ships->selection->numShips;
}

//  (enemy == human)
sdword kasfFindEnemyShipsOfType(GrowSelection *ships, char *shipType)
{
    return kasfFindPlayersShipsOfType(ships, shipType, 0);
}

//  (friendly == not human)
sdword kasfFindFriendlyShipsOfType(GrowSelection *ships, char *shipType)
{
    return kasfFindPlayersShipsOfType(ships, shipType, 1);
}

//  serves the "friendly" and "enemy" functions below
static sdword kasfFindPlayersShipsOfClass(GrowSelection *ships, char *shipClass, sdword playerIndex)
{
    Node *node;
    ShipClass sc = StrToShipClass(shipClass);
    Ship *ship;

//    if (sc < 0)
//        return 0;

    if (ships == NULL || ships->selection == NULL)
        return 0;

    //  wipe the selection clean
    ships->selection->numShips = 0;

    //  check all ships in the damn universe
    node = universe.ShipList.head;
    while (node != NULL)
    {
        ship = (Ship *)listGetStructOfNode(node);

        if ((ship->header.flags & (SOF_Dead|SOF_Hide)) == 0)
        {
            //  1. is it an enemy?  (simplifying assumption: "is it the human"?)
            //  2. is the right class?
            if (ship->playerowner->playerIndex == playerIndex && ship->staticinfo->shipclass == sc)
                // add it
                growSelectAddShip(ships, ship);
        }
        node = node->next;
    }

    return ships->selection->numShips;
}

//  (enemy == human)
sdword kasfFindEnemyShipsOfClass(GrowSelection *ships, char *shipClass)  // KAS FindEnemyShipsOfClass, cataclysm.exe 0x00494b40
{
    return kasfFindPlayersShipsOfClass(ships, shipClass, 0);
}

//  (friendly == not human)
sdword kasfFindFriendlyShipsOfClass(GrowSelection *ships, char *shipClass)  // KAS FindFriendlyShipsOfClass, cataclysm.exe 0x00494bc0
{
    return kasfFindPlayersShipsOfClass(ships, shipClass, 1);
}

sdword kasfPathNextPoint(void)  // KAS PathNextPoint, cataclysm.exe 0x00494c40
{
    AITeamMove *thisMove = CurrentTeamP->curMove;

    if (CurrentTeamP->shipList.selection->numShips <= 0)
        return -1;

    if (!thisMove)
    {
        if (CurrentTeamP->moves.num > 0)
            thisMove = (AITeamMove *)listGetStructOfNode(CurrentTeamP->moves.tail);
        if (!thisMove)
            return -1;
    }

    if (thisMove->type == MOVE_PATROLMOVE)
    {
        if (thisMove->processing)
            // end of path or loop
            return 0;
        else
            return 1;  // first point
    }
    if (thisMove->type != MOVE_MOVETEAMINDEX)
        return -1;  // not patrolling a path

    return thisMove->params.move.index + 1;
}

//
//  PETER: I think we can nuke this now, right?
//

//
//  this is a placeholder for a real fleet intelligence message window
//
void kasfPopupTextDraw(void)
{
    sdword rowHeight, x, y, i, popupTextBorder;
    rectangle rect;
    fonthandle fhSave;
    //char buf[256];

    if (!gameIsRunning || !popupTextNumLines)
        return;

    if (nisIsRunning)
        popupTextNumLines = 0;

    // set up textual stuff
    fhSave = fontCurrentGet();                  //save the current font
    fontMakeCurrent(selGroupFont2);  // use a common, fairly small font

    // to center it:
    x = 320 - popupTextWidth/2;
    //x = 640 - popupTextWidth - rowHeight*2;

    rowHeight = fontHeight("M") + 3; // used to space the text
    // to center it:
    y = (480 - (popupTextNumLines+2) * rowHeight) / 2;
    //y = rowHeight*2;
    popupTextBorder = rowHeight + 6;

    // border
    //primLineLoopStart2(1, POPUPTEXT_COLOR);
    //primLineLoopPoint3F(x - popupTextBorder, y - popupTextBorder);
    //primLineLoopPoint3F(x + popupTextWidth + popupTextBorder, y - popupTextBorder);
    //primLineLoopPoint3F(x + popupTextWidth + popupTextBorder, y + rowHeight * (popupTextNumLines+2) + popupTextBorder);
    //primLineLoopPoint3F(x - popupTextBorder, y + rowHeight * (popupTextNumLines+2) + popupTextBorder);
    //primLineLoopEnd2();

    rect.x0 = x - popupTextBorder;
    rect.x1 = x + popupTextWidth + popupTextBorder;
    rect.y0 = y - popupTextBorder;
    rect.y1 = y + rowHeight * (popupTextNumLines+2) + popupTextBorder;
    primBeveledRectSolid(&rect, POPUPTEXT_BACKGROUND, 4, 4);
    primSeriesOfRoundRects(&rect, 1, POPUPTEXT_COLOR, POPUPTEXT_BORDER_COLOR, 5, 4, 4);

    // title
    fontPrint(x, y, POPUPTEXT_TITLE_COLOR, "FLEET INTELLIGENCE");
    y += rowHeight*2;

    // each line of text
    for (i = 0; i < popupTextNumLines; i++)
    {
        fontPrint(x, y, POPUPTEXT_COLOR, popupTextLines[i]);
        y += rowHeight;
    }

    // stop displaying when expired
    // CAT: on the task clock (kasfPopupTextDraw 0x00494ca0: cat_taskRealTimeElapsed; HW1 universe time)
    if (cat_taskRealTimeElapsed > popupTextEndTime)
        popupTextNumLines = 0;

    fontMakeCurrent(fhSave);
}


//
//  PETER: I think we can nuke this now, right?
//
// CAT (M2-R8 kasf library): kasfPopupText and kasfPopupTextClear are gone (not in cataclysm.exe, whose
//      kasfPopupTextDraw 0x00494ca0 draws lines nothing adds)

//
//  PETER: I think we can nuke this now, right?
//

//
//  new fleet-intelligence style popup
//
void kasfPopup(char *text)  // KAS Popup, cataclysm.exe 0x00494e00
{
    fleetIntelligenceCreate(text, 1);
}

void kasfPopupInteger(char *text, sdword integer)  // KAS PopupInteger, cataclysm.exe 0x00494e10
{
    char tempstr[256];

    // ensure not too long and that there is an integer field
    if (strlen(text) > 200 || !strstr(text, "%d"))
        return;

    sprintf(tempstr, text, integer);
    fleetIntelligenceCreate(tempstr, 1);
}

//
//  new fleet-intelligence style objectives & popup
//
// CAT: 0x00494e70: in the tutorial an objective without full text gets an empty fleet intelligence text (elsewhere the
//      brief text, as HW1)
void kasfObjectiveCreate(char *label, char *briefText, char *fullText)
{
    if ((fullText != NULL) && (fullText[0] != 0))
    {
        objectiveAndFleetIntelligenceCreate(label, briefText, fullText, 0, TRUE);
    }
    else if (tutorial == TUTORIAL_ONLY)
    {
        objectiveAndFleetIntelligenceCreate(label, briefText, "", 0, TRUE);
    }
    else
    {
        objectiveAndFleetIntelligenceCreate(label, briefText, briefText, 0, TRUE);
    }
}

void kasfObjectiveCreateSecondary(char* label, char* briefText, char* fullText)  // KAS ObjectiveCreateSecondary, cataclysm.exe 0x00494ed0
{
    if ((fullText == NULL) || (fullText[0] == 0))
    {
    objectiveAndFleetIntelligenceCreate(label, briefText, briefText, 0, FALSE);
    }
    else
    {
    objectiveAndFleetIntelligenceCreate(label, briefText, fullText, 0, FALSE);
    }
}

void kasfObjectiveSet(char *label, sdword status)  // KAS ObjectiveSet, cataclysm.exe 0x00494f10
{
    objectiveSet(label, status);
}

//void kasfObjectivePopupAll(void)
//{
//    objectivePopupAll();
//}

sdword kasfObjectiveGet(char *label)  // KAS ObjectiveGet, cataclysm.exe 0x00494f30
{
    return objectiveGet(label);
}

sdword kasfObjectiveGetAll(void)  // KAS ObjectivesGetAll, cataclysm.exe 0x00494f40
{
    return objectiveGetAll();
}

/*-----------------------------------------------------------------------------
    Name        : kasfObjectiveGetAny                               0x00494f50
    Description : CAT: KAS "ObjectivesGetAny": the number of objectives whose status is set (a jump to
                  cat_objectiveCountActive 0x004d6370)
----------------------------------------------------------------------------*/
sdword kasfObjectiveGetAny(void)
{
    return cat_objectiveCountActive();
}

void kasfObjectiveDestroy(char *label)  // KAS ObjectiveDestroy, cataclysm.exe 0x00494f60
{
    objectiveDestroy(label);
}

void kasfObjectiveDestroyAll(void)  // KAS ObjectivesDestroyAll, cataclysm.exe 0x00494f70
{
    objectiveDestroyAll();
}


sdword kasfNISRunning(void)  // KAS NISRunning, cataclysm.exe 0x00494f80
{
    return nisIsRunning;
}

/*-----------------------------------------------------------------------------
    CAT (M2-R8 kasf library): Cataclysm's ship selections all go through one helper with a predicate
    (cat_kasShipsSelectByPredicate 0x00494f90); the predicates are small functions without names in the exe (their
    addresses are on each). HW1 repeated the loop in every kasfShipsSelect*.
-----------------------------------------------------------------------------*/
typedef bool32 (*kasfShipPredicate)(Ship *ship, memsize param1, memsize param2);

/*-----------------------------------------------------------------------------
    Name        : cat_kasShipsSelectByPredicate                     0x00494f90
    Description : CAT: the ships of oldShips the predicate accepts: into newShips (cleared first), or, when both are
                  the same selection, the rejected ships are removed from it in place (the loop then does not
                  advance); returns newShips' count. CAT-FIX: a NULL newShips returns 0 (the exe reads address 4).
----------------------------------------------------------------------------*/
static sdword cat_kasShipsSelectByPredicate(GrowSelection *newShips, GrowSelection *oldShips,
                                            kasfShipPredicate predicate, memsize param1, memsize param2)
{
    sdword i;
    bool32 inPlace;
    Ship *ship;

    if (newShips == NULL)
    {
        return 0;
    }
    inPlace = (newShips == oldShips);
    if (!inPlace)
    {
        kasGrowSelectionClear(newShips);
    }
    i = 0;
    while (i < oldShips->selection->numShips)
    {
        ship = oldShips->selection->ShipPtr[i];
        if (predicate(ship, param1, param2))
        {
            if (!inPlace)
            {
                growSelectAddShip(newShips, ship);
            }
            i++;
        }
        else if (inPlace)
        {
            clRemoveShipFromSelection(oldShips->selection, ship);
        }
        else
        {
            i++;
        }
    }
    return newShips->selection->numShips;
}

// 0x00495060: the ship's order protects one of the guarded ships
static bool32 kasfPredGuarding(Ship *ship, memsize guarding, memsize unused)
{
    SelectCommand *guarded = ((GrowSelection *)guarding)->selection;
    sdword i, j;

    (void)unused;
    for (i = 0; i < guarded->numShips; i++)
    {
        if ((ship->command != NULL) && (ship->command->protect != NULL))
        {
            for (j = 0; j < ship->command->protect->numShips; j++)
            {
                if (ship->command->protect->ShipPtr[j] == guarded->ShipPtr[i])
                {
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

// 0x004950f0: no order, or the null order
static bool32 kasfPredIdle(Ship *ship, memsize unused1, memsize unused2)
{
    (void)unused1; (void)unused2;
    return ((ship->command == NULL) || (ship->command->ordertype.order == COMMAND_NULL));
}

// 0x00495110
static bool32 kasfPredNotIdle(Ship *ship, memsize unused1, memsize unused2)
{
    return !kasfPredIdle(ship, unused1, unused2);
}

// 0x004951d0: the ship is attached to the target (cat_salvageAttachedIterFirst / Next)
static bool32 kasfPredClamped(Ship *ship, memsize target, memsize unused)
{
    Ship *attached;

    (void)unused;
    for (attached = cat_salvageAttachedIterFirst((Ship *)target); attached != NULL; attached = cat_salvageAttachedIterNext())
    {
        if (attached == ship)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static udword kasfSelectNumberCount = 0;     // 0x00a43fe0 (name chosen here)

// 0x00495290: the first Number ships (a counter kasfShipsSelectNumber resets)
static bool32 kasfPredNumber(Ship *ship, memsize number, memsize unused)
{
    (void)ship; (void)unused;
    return (kasfSelectNumberCount++ < (udword)number);
}

// 0x004952d0: a ship that counts for its player's support units (not SU exempt)
static bool32 kasfPredPlayer(Ship *ship, memsize unused1, memsize unused2)
{
    (void)unused1; (void)unused2;
    return !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20);
}

// 0x00495310: the ship's special order has a target its player may salvage that no other player's salvagers hold.
//      CAT-FIX: a ship without an order is not salvaging (the exe reads the order of the NULL getShipAndItsCommand returns)
static bool32 kasfPredSalvaging(Ship *ship, memsize unused1, memsize unused2)
{
    CommandToDo *command = getShipAndItsCommand(&universe.mainCommandLayer, ship);
    SelectCommand *targets;
    sdword i;

    (void)unused1; (void)unused2;
    if ((command == NULL) || (command->ordertype.order != COMMAND_SPECIAL))
    {
        return FALSE;
    }
    targets = (SelectCommand *)command->specialtargets;
    for (i = 0; i < targets->numShips; i++)
    {
        if (cat_salvageCanSalvage(ship->playerowner, targets->ShipPtr[i]) &&
            ((targets->ShipPtr[i]->catSalvagePlayer == -1) ||
             (targets->ShipPtr[i]->catSalvagePlayer == ship->playerowner->playerIndex)))
        {
            return TRUE;
        }
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_kasShipIsSalvagableBy                         0x004953b0
    Description : CAT: the predicate of kasfShipsSelectSalvagable: the player of one of the ships may salvage the ship
----------------------------------------------------------------------------*/
static bool32 cat_kasShipIsSalvagableBy(Ship *ship, memsize salvagers, memsize unused)
{
    SelectCommand *selection = ((GrowSelection *)salvagers)->selection;
    sdword i;

    (void)unused;
    for (i = 0; i < selection->numShips; i++)
    {
        if (cat_salvageCanSalvage(selection->ShipPtr[i]->playerowner, ship))
        {
            return TRUE;
        }
    }
    return FALSE;
}

// 0x00495430: the ship is in the other selection
static bool32 kasfPredShips(Ship *ship, memsize compare, memsize unused)
{
    SelectCommand *selection = ((GrowSelection *)compare)->selection;
    sdword i;

    (void)unused;
    for (i = 0; i < selection->numShips; i++)
    {
        if (selection->ShipPtr[i] == ship)
        {
            return TRUE;
        }
    }
    return FALSE;
}

// 0x00495480: the ship is inside the volume
static bool32 kasfPredInside(Ship *ship, memsize volume, memsize unused)
{
    (void)unused;
    return volPointInside((Volume *)volume, &ship->obj.posinfo.position);
}

// 0x004954c0
static bool32 kasfPredUpgradable(Ship *ship, memsize unused1, memsize unused2)
{
    (void)unused1; (void)unused2;
    return cat_shipIsUpgradable(ship);
}

// 0x004954f0: friendly (param1): a computer player's ship that is not a swarmer or a crystal; enemy (0, the only use):
//      player 0's ship not targeted by salvagers and not a swarmer
static bool32 kasfPredEnemyNoStealthy(Ship *ship, memsize friendly, memsize unused)
{
    bool32 selected;

    (void)unused;
    if (friendly)
    {
        selected = (ship->playerowner->playerIndex != 0) && (ship->shiptype != sSwarmer);
        if ((ship->shiptype == dCrystal0) || (ship->shiptype == dCrystal1))
        {
            selected = FALSE;
        }
        return selected;
    }
    return (ship->playerowner->playerIndex == 0) &&
           !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageTargeted) && (ship->shiptype != sSwarmer);
}

// 0x00495580: friendly (param1): a computer player's ship not targeted by salvagers, not a swarmer or a crystal; enemy:
//      player 0's ship that player 1 has detected (cat_shipDetectedByPlayer) and that does not pass for one of player
//      1's (cat_mimicAppearsAlliedTo), not targeted by salvagers, not a swarmer
static bool32 kasfPredEnemy(Ship *ship, memsize friendly, memsize unused)
{
    (void)unused;
    if (friendly)
    {
        return (ship->playerowner->playerIndex != 0) &&
               !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageTargeted) &&
               (ship->shiptype != sSwarmer) && (ship->shiptype != dCrystal0) && (ship->shiptype != dCrystal1);
    }
    return (ship->playerowner->playerIndex == 0) && cat_shipDetectedByPlayer(ship, 1) &&
           !cat_mimicAppearsAlliedTo(ship, &universe.players[1]) &&
           !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvageTargeted) && (ship->shiptype != sSwarmer);
}

// 0x00495670
static bool32 kasfPredClass(Ship *ship, memsize shipClass, memsize unused)
{
    (void)unused;
    return (ship->staticinfo->shipclass == (ShipClass)shipClass);
}

// 0x004956c0
static bool32 kasfPredType(Ship *ship, memsize shipType, memsize unused)
{
    (void)unused;
    return (ship->shiptype == (ShipType)shipType);
}

// 0x00495700: the static's capital-ship word (param1 TRUE), or its negation
static bool32 kasfPredCapital(Ship *ship, memsize capital, memsize unused)
{
    (void)unused;
    if (capital)
    {
        return ship->staticinfo->shipIsCapital;
    }
    return !ship->staticinfo->shipIsCapital;
}

// 0x00495770: hidden or in hyperspace
static bool32 kasfPredDocked(Ship *ship, memsize unused1, memsize unused2)
{
    (void)unused1; (void)unused2;
    return bitTest(ship->header.flags, SOF_Hide | SOF_Hyperspace) ? TRUE : FALSE;
}

// 0x004957b0: not hidden-and-out-of-hyperspace
static bool32 kasfPredNotDocked(Ship *ship, memsize unused1, memsize unused2)
{
    (void)unused1; (void)unused2;
    return !(bitTest(ship->header.flags, SOF_Hide) && !bitTest(ship->header.flags, SOF_Hyperspace));
}

// 0x004957f0: health at most percent * maxhealth * 0.01f (the percent zero-extended to 64 bits, on the x87 stack)
static bool32 kasfPredDamaged(Ship *ship, memsize percent, memsize unused)
{
    (void)unused;
    return !(((real64)(udword)percent * (real64)ship->staticinfo->health.maxhealth * (real64)0.01f) < (real64)ship->targetable.health);
}

// 0x00495860: health at least percent * maxhealth * 0.01f
static bool32 kasfPredMoreThanDamaged(Ship *ship, memsize percent, memsize unused)
{
    (void)unused;
    return (((real64)(udword)percent * (real64)ship->staticinfo->health.maxhealth * (real64)0.01f) <= (real64)ship->targetable.health);
}

// 0x004958d0: one velocity component above 2 (0x0072a6c0) in magnitude
static bool32 kasfPredMoving(Ship *ship, memsize unused1, memsize unused2)
{
    (void)unused1; (void)unused2;
    return (fabsf(ship->obj.posinfo.velocity.x) > 2.0f) || (fabsf(ship->obj.posinfo.velocity.y) > 2.0f) ||
           (fabsf(ship->obj.posinfo.velocity.z) > 2.0f);
}

// 0x004959f0: farther than the distance (param2 holds the squared distance's bits)
static bool32 kasfPredFarther(Ship *ship, memsize point, memsize distanceSquared)
{
    udword bits = (udword)distanceSquared;
    real32 rangeSquared;

    memcpy(&rangeSquared, &bits, sizeof(rangeSquared));
    return (aiuFindDistanceSquared(*(vector *)point, ship->obj.posinfo.position) > rangeSquared);
}

// 0x00495aa0: nearer than the distance
static bool32 kasfPredNearby(Ship *ship, memsize point, memsize distanceSquared)
{
    udword bits = (udword)distanceSquared;
    real32 rangeSquared;

    memcpy(&rangeSquared, &bits, sizeof(rangeSquared));
    return (aiuFindDistanceSquared(*(vector *)point, ship->obj.posinfo.position) < rangeSquared);
}

// the squared distance as the exe passes it (fild, fmul, fstp dword: one float rounding), carried in a memsize
static memsize kasfDistanceSquaredBits(sdword distance)
{
    real32 rangeSquared = (real32)((real64)distance * (real64)distance);
    udword bits;

    memcpy(&bits, &rangeSquared, sizeof(bits));
    return (memsize)bits;
}

//local function
/*-----------------------------------------------------------------------------
    Name        : kasfShipMeetsCriteria                             0x00495b20 (the predicate of kasfShipsSelectSpecial)
    Description : Returns TRUE if the ship meets the criteria determined by the CriteriaFlag
                  CAT: no ship returns technology in Cataclysm (flag 1 is always FALSE); an armed frigate is any but the
                  two cloak generators, an armed corvette any but the two salvage corvettes; a mothership is not armed
----------------------------------------------------------------------------*/
static bool32 kasfShipMeetsCriteria(Ship *ship, memsize CriteriaFlag, memsize unused)
{
    (void)unused;
    switch (CriteriaFlag)
    {
        case kasSpecial_ShipsDisabled:
            return bitTest(ship->header.flags, SOF_Disabled) ? TRUE : FALSE;
        case kasSpecial_ShipsAreArmed:
            switch (ship->staticinfo->shipclass)
            {
                case CLASS_HeavyCruiser:
                case CLASS_Carrier:
                case CLASS_Destroyer:
                case CLASS_Fighter:
                    return TRUE;
                case CLASS_Frigate:
                    return (ship->shiptype != kCloakGenerator) && (ship->shiptype != tCloakGenerator);
                case CLASS_Corvette:
                    return (ship->shiptype != kSalCapCorvette) && (ship->shiptype != tSalCapCorvette);
                default:
                    return FALSE;
            }
        case kasSpecial_ShipsDefected:
            return bitTest(ship->obj.attributes, ATTRIBUTES_Defector) ? TRUE : FALSE;
        case kasSpecial_ReturningTechnology:
        default:
            return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectGuarding                           0x004950d0
    Description : CAT: KAS "ShipsSelectGuarding": the ships whose order guards one of Guarding's ships
----------------------------------------------------------------------------*/
sdword kasfShipsSelectGuarding(GrowSelection *NewShips, GrowSelection *OldShips, GrowSelection *Guarding)
{
    return cat_kasShipsSelectByPredicate(NewShips, OldShips, kasfPredGuarding, (memsize)Guarding, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectIdle / kasfShipsSelectNotIdle      0x00495140 / 0x00495160
    Description : CAT: KAS "ShipsSelectIdle" / "ShipsSelectNotIdle": the ships without an order (with one)
----------------------------------------------------------------------------*/
sdword kasfShipsSelectIdle(GrowSelection *NewShips, GrowSelection *OldShips)
{
    return cat_kasShipsSelectByPredicate(NewShips, OldShips, kasfPredIdle, 0, 0);
}

sdword kasfShipsSelectNotIdle(GrowSelection *NewShips, GrowSelection *OldShips)
{
    return cat_kasShipsSelectByPredicate(NewShips, OldShips, kasfPredNotIdle, 0, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectClamped                            0x00495180
    Description : CAT: KAS "ShipsSelectClamped": the ships attached to Target's first ship (none, and NewShips cleared,
                  when nothing is attached to it)
----------------------------------------------------------------------------*/
sdword kasfShipsSelectClamped(GrowSelection *NewShips, GrowSelection *OldShips, GrowSelection *Target)
{
    if ((Target->selection->numShips > 0) && (cat_salvageAttachedIterFirst(Target->selection->ShipPtr[0]) != NULL))
    {
        return cat_kasShipsSelectByPredicate(NewShips, OldShips, kasfPredClamped, (memsize)Target->selection->ShipPtr[0], 0);
    }
    kasGrowSelectionClear(NewShips);
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsGetClamped                               0x00495200
    Description : CAT: KAS "ShipsGetClamped": every ship attached to pTarget's first ship
----------------------------------------------------------------------------*/
sdword kasfShipsGetClamped(GrowSelection *pShips, GrowSelection *pTarget)
{
    Ship *ship;

    kasGrowSelectionClear(pShips);
    if (pTarget->selection->numShips < 1)
    {
        return pShips->selection->numShips;
    }
    for (ship = cat_salvageAttachedIterFirst(pTarget->selection->ShipPtr[0]); ship != NULL; ship = cat_salvageAttachedIterNext())
    {
        growSelectAddShip(pShips, ship);
    }
    return pShips->selection->numShips;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectNumber                             0x00495260
    Description : CAT: KAS "ShipsSelectNumber": the first Number ships
----------------------------------------------------------------------------*/
sdword kasfShipsSelectNumber(GrowSelection *NewShips, GrowSelection *OldShips, sdword Number)
{
    kasfSelectNumberCount = 0;
    return cat_kasShipsSelectByPredicate(NewShips, OldShips, kasfPredNumber, (memsize)Number, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectPlayer                             0x004952b0
    Description : CAT: KAS "ShipsSelectPlayer": the ships that count for their player's support units
----------------------------------------------------------------------------*/
sdword kasfShipsSelectPlayer(GrowSelection *NewShips, GrowSelection *OldShips)
{
    return cat_kasShipsSelectByPredicate(NewShips, OldShips, kasfPredPlayer, 0, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectSalvaging                          0x004952f0
    Description : CAT: KAS "ShipsSelectSalvaging" (no return value in the function table)
----------------------------------------------------------------------------*/
void kasfShipsSelectSalvaging(GrowSelection *NewShips, GrowSelection *OldShips)
{
    cat_kasShipsSelectByPredicate(NewShips, OldShips, kasfPredSalvaging, 0, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectSalvagable                         0x00495390
    Description : CAT: KAS "ShipsSelectSalvagable": the ships the Targets' players may salvage
----------------------------------------------------------------------------*/
sdword kasfShipsSelectSalvagable(GrowSelection *NewShips, GrowSelection *OldShips, GrowSelection *Targets)
{
    return cat_kasShipsSelectByPredicate(NewShips, OldShips, cat_kasShipIsSalvagableBy, (memsize)Targets, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectShips                              0x00495410
    Description : CAT: KAS "ShipsSelectShips": the ships that are also in compareships
----------------------------------------------------------------------------*/
sdword kasfShipsSelectShips(GrowSelection *newships, GrowSelection *oldships, GrowSelection *compareships)
{
    return cat_kasShipsSelectByPredicate(newships, oldships, kasfPredShips, (memsize)compareships, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectInside                             0x00495460
    Description : CAT: KAS "ShipsSelectInside": the ships inside the volume
----------------------------------------------------------------------------*/
sdword kasfShipsSelectInside(GrowSelection *newships, GrowSelection *oldships, Volume *volume)
{
    return cat_kasShipsSelectByPredicate(newships, oldships, kasfPredInside, (memsize)volume, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectUpgradable                         0x004954a0
    Description : CAT: KAS "ShipsSelectUpgradable" (cat_shipIsUpgradable)
----------------------------------------------------------------------------*/
sdword kasfShipsSelectUpgradable(GrowSelection *newShips, GrowSelection *originalShips)
{
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredUpgradable, 0, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectEnemyNoStealthy                    0x004954d0
    Description : CAT: KAS "ShipsSelectEnemyNoStealthy": player 0's ships (no stealth test)
----------------------------------------------------------------------------*/
sdword kasfShipsSelectEnemyNoStealthy(GrowSelection *NewShips, GrowSelection *OriginalShips)
{
    return cat_kasShipsSelectByPredicate(NewShips, OriginalShips, kasfPredEnemyNoStealthy, 0, 0);
}

// CAT: 0x00495560 (the predicate 0x00495580: player 0's ships that player 1 has detected and that do not pass for
//      his, not swarmers or targeted by salvagers; HW1: player 0's)
sdword kasfShipsSelectEnemy(GrowSelection *newShips, GrowSelection *originalShips)
{
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredEnemy, FALSE, 0);
}

// CAT: 0x00495620 (the same predicate: the computer players' ships, no swarmers, crystals or salvage targets)
sdword kasfShipsSelectFriendly(GrowSelection *newShips, GrowSelection *originalShips)
{
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredEnemy, TRUE, 0);
}

// CAT: 0x00495640
sdword kasfShipsSelectClass(GrowSelection *newShips, GrowSelection *originalShips, char *shipClass)
{
    ShipClass sc = StrToShipClass(shipClass);
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredClass, (memsize)sc, 0);
}

// CAT: 0x00495690
sdword kasfShipsSelectType(GrowSelection *newShips, GrowSelection *originalShips, char *shipType)
{
    ShipType st = StrToShipType(shipType);
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredType, (memsize)st, 0);
}

// CAT: 0x004956e0 (the static's capital-ship word; HW1 compared the class with CLASS_Frigate)
sdword kasfShipsSelectCapital(GrowSelection *newShips, GrowSelection *originalShips)
{
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredCapital, TRUE, 0);
}

// CAT: 0x00495730
sdword kasfShipsSelectNonCapital(GrowSelection *newShips, GrowSelection *originalShips)
{
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredCapital, FALSE, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectDocked                             0x00495750
    Description : CAT: KAS "ShipsSelectDocked": the ships hidden or in hyperspace
----------------------------------------------------------------------------*/
sdword kasfShipsSelectDocked(GrowSelection *NewShips, GrowSelection *OldShips)
{
    return cat_kasShipsSelectByPredicate(NewShips, OldShips, kasfPredDocked, 0, 0);
}

// CAT: 0x00495790
sdword kasfShipsSelectNotDocked(GrowSelection *newShips, GrowSelection *originalShips)
{
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredNotDocked, 0, 0);
}

// CAT: 0x004957d0 (health <= percent * maxhealth * 0.01f on the x87 stack; HW1: maxhealth * percent / 100)
sdword kasfShipsSelectDamaged(GrowSelection *newShips, GrowSelection *originalShips, sdword maxHealthPercent)
{
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredDamaged, (memsize)(udword)maxHealthPercent, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectMoreThanDamaged                    0x00495840
    Description : CAT: KAS "ShipsSelectMoreThanDamaged": health >= percent of the maximum
----------------------------------------------------------------------------*/
sdword kasfShipsSelectMoreThanDamaged(GrowSelection *newShips, GrowSelection *originalShips, sdword maxHealthPercent)
{
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredMoreThanDamaged, (memsize)(udword)maxHealthPercent, 0);
}

// CAT: 0x004958b0 (a velocity component above 2 in magnitude; HW1 tested the isMoving flags)
sdword kasfShipsSelectMoving(GrowSelection *newShips, GrowSelection *originalShips)
{
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredMoving, 0, 0);
}

// CAT: 0x00495950: NewShips is only cleared (and set to the one ship) when the index is valid
sdword kasfShipsSelectIndex(GrowSelection *newShips, GrowSelection *originalShips, sdword Index)
{
    Ship *ship;

    if (newShips == NULL || newShips->selection == NULL ||
        originalShips == NULL || originalShips->selection == NULL)
        return 0;

    // if index is larger than the numships, or if numships is zero
    // return 0
    if ((originalShips->selection->numShips <= Index) ||
        (!originalShips->selection->numShips))
    {
        return 0;
    }

    ship = originalShips->selection->ShipPtr[Index];
    kasGrowSelectionClear(newShips);
    growSelectAddShip(newShips, ship);

    return newShips->selection->numShips;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectFarther                            0x004959a0
    Description : CAT: KAS "ShipsSelectFarther": the ships farther than distance from the point (no return value in
                  the function table)
----------------------------------------------------------------------------*/
void kasfShipsSelectFarther(GrowSelection *newships, GrowSelection *oldships, hvector *point, sdword distance)
{
    vector loc;

    vecGrabVecFromHVec(loc, *point);
    cat_kasShipsSelectByPredicate(newships, oldships, kasfPredFarther, (memsize)&loc, kasfDistanceSquaredBits(distance));
}

// selects the ships in originalShips that are within distance of location
// CAT: 0x00495a50: through the predicate helper (in place when both selections are the same)
sdword kasfShipsSelectNearby(GrowSelection *newShips, GrowSelection *originalShips, hvector *location, sdword distance)
{
    vector loc;

    vecGrabVecFromHVec(loc, *location);
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredNearby, (memsize)&loc, kasfDistanceSquaredBits(distance));
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectSpecial
    Description : Selects the ships using different criteria depending on the
                  SpecialFlag setting
                  CAT: 0x00495b00, through the predicate helper
    Inputs      : newShips - pointer to growselection (pass by reference)
                  originalShips - the ships to filter
                  SpecialFlag - indicates the criteria
    Outputs     :
    Return      : The number of ships selected
----------------------------------------------------------------------------*/
sdword kasfShipsSelectSpecial(GrowSelection *newShips, GrowSelection *originalShips, sdword SpecialFlag)
{
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfShipMeetsCriteria, (memsize)SpecialFlag, 0);
}


//
//  A = A + B
//
sdword kasfShipsAdd(GrowSelection *shipsA, GrowSelection *shipsB)  // KAS ShipsAdd, cataclysm.exe 0x00495bd0
{
    sdword i;

    if (shipsA == NULL || shipsA->selection == NULL ||
        shipsB == NULL || shipsB->selection == NULL)
        return 0;

    if (shipsA == shipsB)
        return shipsA->selection->numShips;

    for (i = 0; i < shipsB->selection->numShips; ++i)
        growSelectAddShipNoDuplication(shipsA, shipsB->selection->ShipPtr[i]);

    return shipsA->selection->numShips;
}

//
//  A = A - B
//
sdword kasfShipsRemove(GrowSelection *shipsA, GrowSelection *shipsB)  // KAS ShipsRemove, cataclysm.exe 0x00495c40
{
    sdword i;

    if (shipsA == NULL || shipsA->selection == NULL ||
        shipsB == NULL || shipsB->selection == NULL)
        return 0;

    if (shipsA == shipsB)
    {
        while (shipsA->selection->numShips)
            clRemoveShipFromSelection(shipsA->selection, shipsA->selection->ShipPtr[0]);
        return 0;
    }

    for (i = 0; i < shipsB->selection->numShips; ++i)
        growSelectRemoveShip(shipsA, shipsB->selection->ShipPtr[i]);
    return shipsA->selection->numShips;
}


//returns TRUE if all the selected ships are in the specified formation
sdword kasfSelectedShipsInFormation(sdword formation)  // KAS SelectedShipsInFormation, cataclysm.exe 0x00495cc0
{
    udword i;

    if (!selSelected.numShips)
    {
        return FALSE;
    }

    for (i=0;i<selSelected.numShips;i++)
    {
        if (!cat_shipFormationCommand(selSelected.ShipPtr[i]) ||
            (cat_shipFormationCommand(selSelected.ShipPtr[i])->formation.formationtype != (TypeOfFormation)formation))
        {
            return FALSE;
        }
    }

    return TRUE;
}

//returns TRUE if any of the ships are in the specific formation
// CAT: 0x00495d10: for a given formation the exe tests the current selection's ships (selSelected), as many as the
//      ships passed (HW1 tested the ships passed). CAT-FIX: only the selection's own entries are read (the exe reads
//      stale entries past selSelected.numShips when ships has more)
sdword kasfShipsInFormation(GrowSelection *ships, sdword formation)
{
    udword i;

    if (formation == -1)
    {
        for (i=0;i<ships->selection->numShips;i++)
        {
            if (cat_shipFormationCommand(ships->selection->ShipPtr[i]))
            {
                return TRUE;
            }
        }
    }
    else
    {
        for (i=0;(i<ships->selection->numShips) && (i<selSelected.numShips);i++)
        {
            if (cat_shipFormationCommand(selSelected.ShipPtr[i]) &&
                (cat_shipFormationCommand(selSelected.ShipPtr[i])->formation.formationtype == (TypeOfFormation)formation))
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

//
// sets the ships so that the player's ships don't autoretaliate against them
//
void kasfShipsSetNonRetaliation(GrowSelection *ships)  // KAS ShipsSetNonRetaliation, cataclysm.exe 0x00495da0
{
    udword i;

    for (i=0;i<ships->selection->numShips;i++)
    {
        bitSet(ships->selection->ShipPtr[i]->shipOrDerelict.specialFlags, SPECIAL_FriendlyStatus);
    }
}

//
// sets the ships so that the player's ships don't autoretaliate against them
//
void kasfShipsSetRetaliation(GrowSelection *ships)  // KAS ShipsSetRetaliation, cataclysm.exe 0x00495de0
{
    udword i;

    for (i=0;i<ships->selection->numShips;i++)
    {
        bitClear(ships->selection->ShipPtr[i]->shipOrDerelict.specialFlags, SPECIAL_FriendlyStatus);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfPingAddSingleShip
    Description : Add a ping for a single ship, the first in the selection
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void kasfPingAddSingleShip(GrowSelection *ships, char *label)  // KAS PingAddSingleShip, cataclysm.exe 0x00495e20
{
    if (ships == NULL || ships->selection == NULL || ships->selection->ShipPtr[0] == NULL)
        return;

    pingAnomalyObjectPingAdd(label, (SpaceObj *)(ships->selection->ShipPtr[0]));
}

/*-----------------------------------------------------------------------------
    Name        : kasfPingAddShips
    Description : Add a ping for a group of ships.  The ping will be centred
                    about the entire group of ships.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void kasfPingAddShips(GrowSelection *ships, char *label)  // KAS PingAddShips, cataclysm.exe 0x00495e50
{
    if (ships == NULL || ships->selection == NULL || ships->selection->ShipPtr[0] == NULL)
        return;

    pingAnomalySelectionPingAdd(label, ships->selection);
}

void kasfPingAddPoint(hvector *point, char *label)  // KAS PingAddPoint, cataclysm.exe 0x00495e80
{
    pingAnomalyPositionPingAdd(label, (vector *)point);
}

void kasfPingRemove(char *label)  // KAS PingRemove, cataclysm.exe 0x00495ea0
{
    pingAnomalyPingRemove(label);
}

void kasfStop(void)  // KAS Stop, cataclysm.exe 0x00495eb0
{
    aitDeleteAllTeamMoves(CurrentTeamP);
    clWrapHalt(&universe.mainCommandLayer, CurrentTeamP->shipList.selection);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsStop                                     0x00495ee0
    Description : CAT: KAS "ShipsStop": the ships halt (clWrapHalt)
----------------------------------------------------------------------------*/
void kasfShipsStop(GrowSelection *ship)
{
    clWrapHalt(&universe.mainCommandLayer, ship->selection);
}

// CAT: 0x00495f00: the harvest-capable ships (MakeShipsHarvestCapable with workers-only 0) collect when there are any
void kasfHarvest(void)
{
    MaxSelection tempSelection;
    SelectCommand *selection = CurrentTeamP->shipList.selection;
    //aitDeleteAllTeamMoves(CurrentTeamP);

    if (selection->numShips > 0)
    {
        MakeShipsHarvestCapable((SelectCommand *)&tempSelection, selection, FALSE);
        if (tempSelection.numShips > 0)
        {
            clWrapCollectResource(&universe.mainCommandLayer,(SelectCommand *)&tempSelection,NULL);
        }
    }
}

void kasfBuildControl(sdword on)  // KAS BuildControl, cataclysm.exe 0x00495f50
{
    singlePlayerGameInfo.giveComputerFleetControl = on;
}

// sets the team as a building team for the KAS script
void kasfBuildingTeam(struct AITeam *builder)  // KAS BuildingTeam, cataclysm.exe 0x00495f60
{
    //for now, just set the first ship on the team (assume it only has one ship)
    aiCurrentAIPlayer->ScriptCreator = builder->shipList.selection->ShipPtr[0];
}

void kasfKamikaze(GrowSelection *targets)  // KAS Kamikaze, cataclysm.exe 0x00495f80
{
    // alloc memory that the move can later free itself
    SelectCommand *dupeTargets = selectDupSelection(targets->selection);
    if (dupeTargets->numShips && dupeTargets->ShipPtr[0] != NULL)
    {
        aitDeleteAllTeamMoves(CurrentTeamP);
        aimCreateKamikaze(CurrentTeamP, dupeTargets, SAME_FORMATION, FALSE, TRUE);
    }
}

void kasfKamikazeEveryone(GrowSelection *targets)  // KAS KamikazeEveryone, cataclysm.exe 0x00495fd0
{
    AITeam *teamp;
    udword i;

    if (targets->selection->numShips && targets->selection->ShipPtr[0] != NULL)
    {
        for (i = 0; i < aiCurrentAIPlayer->teamsUsed; i++)
        {
            teamp = aiCurrentAIPlayer->teams[i];

            if (teamp->shipList.selection->numShips)
            {
                aitDeleteAllTeamMoves(teamp);
                aimCreateKamikaze(teamp, selectDupSelection(targets->selection), SAME_FORMATION, FALSE, TRUE);
            }
        }
    }

}

void kasfSpecialToggle(void)  // KAS SpecialToggle, cataclysm.exe 0x00496050
{
    if (!CurrentTeamP || !CurrentTeamP->shipList.selection->numShips)
        return;
    clWrapSpecial(&universe.mainCommandLayer, CurrentTeamP->shipList.selection, NULL);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSpecialToggle                            0x00496080
    Description : CAT: KAS "ShipsSpecialToggle": the ships' special ability (clWrapSpecial with no targets)
----------------------------------------------------------------------------*/
void kasfShipsSpecialToggle(GrowSelection *ships)
{
    if ((ships != NULL) && (ships->selection->numShips != 0))
    {
        clWrapSpecial(&universe.mainCommandLayer, ships->selection, NULL);
    }
}

void kasfShipsDamage(GrowSelection *ships, sdword points)  // KAS ShipsDamage, cataclysm.exe 0x004960b0
{
    sdword i;

    if (ships == NULL || ships->selection == NULL)
        return;

    for (i = 0; i < ships->selection->numShips; ++i)
        ApplyDamageToTarget((struct SpaceObjRotImpTarg *)ships->selection->ShipPtr[i], (real32)points, 0, 0, 0, NULL, 0x1e);   // CAT: 7 arguments (kasfShipsDamage 0x00496080)
}


// CAT (M2-R8 kasf library): kasfForceTaskbar is gone with the task bar (cataclysm.exe's function table 0x00755048 has
//      no "ForceTaskbar")



/*
----------------------------------------------------------------------------
 The following functions have been added to facilitate the tutorial mission.
----------------------------------------------------------------------------
*/

void kasfBuilderRestrictShipTypes(char *shipTypes)  // KAS BuilderRestrictShipTypes, cataclysm.exe 0x00496110
{
    tutBuilderSetRestrictions(shipTypes, 1);
}

void kasfBuilderUnrestrictShipTypes(char *shipTypes)  // KAS BuilderUnrestrictShipTypes, cataclysm.exe 0x00496120
{
    tutBuilderSetRestrictions(shipTypes, 0);
}

void kasfBuilderRestrictAll(void)  // KAS BuilderRestrictAll, cataclysm.exe 0x00496130
{
    tutBuilderRestrictAll();
}

void kasfBuilderRestrictNone(void)  // KAS BuilderRestrictNone, cataclysm.exe 0x00496140
{
    tutBuilderRestrictNone();
}

void kasfBuilderCloseIfOpen(void)  // KAS BuilderCloseIfOpen, cataclysm.exe 0x00496150
{
    cmCloseIfOpen();
}

/*-----------------------------------------------------------------------------
    Name        : kasfResearchCloseIfOpen                           0x00496160
    Description : CAT: KAS "ResearchCloseIfOpen": a jump to rmCloseIfOpen 0x004f4e30
----------------------------------------------------------------------------*/
void kasfResearchCloseIfOpen(void)
{
    rmCloseIfOpen();
}

// figure out how to do this someday....
void kasfForceBuildShipType(char *shipType)  // KAS ForceBuildShipType, cataclysm.exe 0x00496170
{
    ShipType st;

    st = StrToShipType(shipType);
    cmForceBuildShipType(st);
//    clWrapBuildShip(&universe.mainCommandLayer,st,universe.curPlayerPtr->race,0,universe.curPlayerPtr->PlayerMothership);
}

// Range from 0 to 359 as universe moves from right to left
// CAT: 0x00496190: the angle times 57.2957795f (0x0072a6a0) on the x87 stack, truncated (__ftol)
sdword kasfCameraGetAngleDeg(void)
{
    return (sdword)((real64)currentCameraStackEntry(&universe.mainCameraCommand)->remembercam.angle * (real64)57.2957795f);
}

/*-----------------------------------------------------------------------------
    Name        : kasfCameraSetAngleDeg                             0x004961b0
    Description : CAT: KAS "CameraSetAngleDeg": the camera's angle (degrees * 0.0174533f, 0x0072a4f8) with its current
                  declination, then the eye is placed
----------------------------------------------------------------------------*/
void kasfCameraSetAngleDeg(sdword angle)
{
    ccChangeAngleDeclination(&universe.mainCameraCommand, (real32)((real64)angle * (real64)0.0174533f),
                             currentCameraStackEntry(&universe.mainCameraCommand)->remembercam.declination);
    cameraSetEyePosition(&universe.mainCameraCommand.actualcamera);
}

// Range from -90 (Above) to +90 (Below)
// CAT: 0x004961f0 (as kasfCameraGetAngleDeg)
sdword kasfCameraGetDeclinationDeg(void)
{
    return (sdword)((real64)currentCameraStackEntry(&universe.mainCameraCommand)->remembercam.declination * (real64)57.2957795f);
}

/*-----------------------------------------------------------------------------
    Name        : kasfCameraSetDeclinationDeg                       0x00496210
    Description : CAT: KAS "CameraSetDeclinationDeg": the camera's declination with its current angle
----------------------------------------------------------------------------*/
void kasfCameraSetDeclinationDeg(sdword declination)
{
    ccChangeAngleDeclination(&universe.mainCameraCommand,
                             currentCameraStackEntry(&universe.mainCameraCommand)->remembercam.angle,
                             (real32)((real64)declination * (real64)0.0174533f));
    cameraSetEyePosition(&universe.mainCameraCommand.actualcamera);
}

sdword kasfCameraGetDistance(void)  // KAS CameraGetDistance, cataclysm.exe 0x00496250
{
    return (sdword)currentCameraStackEntry(&universe.mainCameraCommand)->remembercam.distance;
}

/*-----------------------------------------------------------------------------
    Name        : kasfCameraSetDistance                             0x00496260
    Description : CAT: KAS "CameraSetDistance" (cat_ccSetDistance, then the eye is placed)
----------------------------------------------------------------------------*/
void kasfCameraSetDistance(sdword distance)
{
    cat_ccSetDistance(&universe.mainCameraCommand, (real32)distance);
    cameraSetEyePosition(&universe.mainCameraCommand.actualcamera);
}

sdword kasfSelectNumSelected(void)  // KAS SelectNumSelected, cataclysm.exe 0x00496280
{
    return selSelected.numShips;
}

sdword kasfSelectIsSelectionShipType(sdword Index, char *shipType)  // KAS SelectIsSelectionShipType, cataclysm.exe 0x00496290
{
ShipType    st;

    st = StrToShipType(shipType);
    return ((selSelected.numShips > Index) && selSelected.ShipPtr[Index]->shiptype == st);
}

sdword kasfSelectContainsShipTypes(char *shipTypes)  // KAS SelectContainsShipTypes, cataclysm.exe 0x004962d0
{
    return tutSelectedContainsShipTypes(shipTypes);
}

void kasfTutSetPointerTargetXY(char *name, sdword x, sdword y)  // KAS TutSetPointerTargetXY, cataclysm.exe 0x004962e0
{
    tutSetPointerTargetXY(name, x,y);
}

void kasfTutSetPointerTargetXYRight(char *name, sdword x, sdword y)  // KAS TutSetPointerTargetXYRight, cataclysm.exe 0x00496300
{
    tutSetPointerTargetXYRight(name, x,y);
}

void kasfTutSetPointerTargetXYBottomRight(char *name, sdword x, sdword y)  // KAS TutSetPointerTargetXYBottomRight, cataclysm.exe 0x00496320
{
    tutSetPointerTargetXYBottomRight(name, x,y);
}

void kasfTutSetPointerTargetXYTaskbar(char *name, sdword x, sdword y)  // KAS TutSetPointerTargetXYTaskbar, cataclysm.exe 0x00496340
{
    tutSetPointerTargetXYTaskbar(name, x,y);
}

void kasfTutSetPointerTargetXYFE(char *name, sdword x, sdword y)  // KAS TutSetPointerTargetXYFE, cataclysm.exe 0x00496360
{
    tutSetPointerTargetXYFE(name, x,y);
}

// CAT: 0x00496380: Cataclysm's pointer setters take a third argument, FALSE for an ordinary pointer, TRUE for one of
//      the objective list's (the TutSetObjPointer* kasf)
void kasfTutSetPointerTargetShip(char *name, GrowSelection *ships)
{
    if(ships->selection->numShips > 0)
    {
        tutSetPointerTargetShip(name, ships->selection->ShipPtr[0], FALSE);
    }
}

// CAT: 0x004963a0 (third argument FALSE)
void kasfTutSetPointerTargetShipSelection(char *name, GrowSelection *ships)
{
    if(ships->selection->numShips > 0)
    {
        tutSetPointerTargetShipSelection(name, ships->selection, FALSE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfTutSetObjPointerTargetShip / kasfTutSetObjPointerTargetShipSelection   0x004963c0 / 0x004963e0
    Description : CAT: KAS "TutSetObjPointerTargetShip" / "...ShipSelection": as the plain ones, as objective pointers
----------------------------------------------------------------------------*/
void kasfTutSetObjPointerTargetShip(char *name, GrowSelection *pShipList)
{
    if (pShipList->selection->numShips > 0)
    {
        tutSetPointerTargetShip(name, pShipList->selection->ShipPtr[0], TRUE);
    }
}

void kasfTutSetObjPointerTargetShipSelection(char *name, GrowSelection *pShipList)
{
    if (pShipList->selection->numShips > 0)
    {
        tutSetPointerTargetShipSelection(name, pShipList->selection, TRUE);
    }
}

void kasfTutSetPointerTargetShipHealth(char *name, GrowSelection *ships)  // KAS TutSetPointerTargetShipHealth, cataclysm.exe 0x00496400
{
    if(ships->selection->numShips > 0)
        tutSetPointerTargetShipHealth(name, ships->selection->ShipPtr[0]);
}

void kasfTutSetPointerTargetShipGroup(char *name, GrowSelection *ships)  // KAS TutSetPointerTargetShipGroup, cataclysm.exe 0x00496420
{
    if(ships->selection->numShips > 0)
    {
        tutSetPointerTargetShipGroup(name, ships->selection->ShipPtr[0]);
    }
}

void kasfTutSetPointerTargetFERegion(char *name, char *pAtomName)  // KAS TutSetPointerTargetFERegion, cataclysm.exe 0x00496440
{
    tutSetPointerTargetFERegion(name, pAtomName);
}

void kasfTutSetPointerTargetRect(char *name, sdword x0, sdword y0, sdword x1, sdword y1)  // KAS TutSetPointerTargetRect, cataclysm.exe 0x00496460
{
    tutSetPointerTargetRect(name, x0, y0, x1, y1);
}

// CAT: 0x00496490 (third argument FALSE)
void kasfTutSetPointerTargetAIVolume(char *name, Volume *volume)
{
    tutSetPointerTargetAIVolume(name, volume, FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : kasfTutSetObjPointerTargetAIVolume                0x004964b0
    Description : CAT: KAS "TutSetObjPointerTargetAIVolume": an objective pointer at the volume
----------------------------------------------------------------------------*/
void kasfTutSetObjPointerTargetAIVolume(char *name, Volume *volume)
{
    tutSetPointerTargetAIVolume(name, volume, TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : kasfTutSetPointerTargetObj                        0x004964d0
    Description : CAT: KAS "TutSetPointerTargetObj": a pointer at the objective list (cat_tutSetPointerTargetObj)
----------------------------------------------------------------------------*/
void kasfTutSetPointerTargetObj(char *name)
{
    cat_tutSetPointerTargetObj(name);
}

// CAT: 0x004964e0: on the software renderer (RGLtype 0x00b3a700 == 0) the exe also clears the screen while the
//      construction or research screen is open; this tree draws with GL only (decision 28), so that branch is not taken
void kasfTutRemovePointer(char *name)
{
    tutRemovePointerByName(name);
}

// CAT: 0x00496530 (the software-renderer branch as kasfTutRemovePointer: not taken, decision 28)
void kasfTutRemoveAllPointers(void)
{
    tutRemoveAllPointers();
}

void kasfTutSetTextDisplayBoxGame(sdword x, sdword y, sdword width, sdword height)  // KAS TutSetTextDisplayBoxGame, cataclysm.exe 0x00496570
{
    tutSetTextDisplayBox(x, y, width, height, TRUE);
}

void kasfTutSetTextDisplayBoxToSubtitleRegion(void)  // KAS TutSetTextDisplayBoxToSubtitleRegion, cataclysm.exe 0x00496590
{
    tutSetTextDisplayBoxToSubtitleRegion();
}
void kasfTutSetTextDisplayBoxFE(sdword x, sdword y, sdword width, sdword height)  // KAS TutSetTextDisplayBoxFE, cataclysm.exe 0x004965a0
{
    tutSetTextDisplayBox(x, y, width, height, FALSE);
}

void kasfTutShowText(char *szText)  // KAS TutShowText, cataclysm.exe 0x004965c0
{
    tutShowText(szText);
}

void kasfTutHideText(void)  // KAS TutHideText, cataclysm.exe 0x004965d0
{
    tutHideText();
}

void kasfTutShowNextButton(void)  // KAS TutShowNextButton, cataclysm.exe 0x004965e0
{
    tutShowNextButton();
}

void kasfTutHideNextButton(void)  // KAS TutHideNextButton, cataclysm.exe 0x004965f0
{
    tutHideNextButton();
}

sdword kasfTutNextButtonClicked(void)  // KAS TutNextButtonClicked, cataclysm.exe 0x00496600
{
    return tutNextButtonClicked();
}

/*-----------------------------------------------------------------------------
    Name        : kasfTutShowNextTextButton .. kasfTutQuitButtonClicked   0x00496610 .. 0x00496690
    Description : CAT: the Cataclysm tutorial's extra buttons (next-text, previous, quit), each a jump to its Tutor.c
                  function (cat_tutShowNextTextButton 0x00532190 .. cat_tutQuitButtonClicked 0x00532ab0)
----------------------------------------------------------------------------*/
void kasfTutShowNextTextButton(void)
{
    cat_tutShowNextTextButton();
}

void kasfTutHideNextTextButton(void)  // KAS TutHideNextTextButton, cataclysm.exe 0x00496620
{
    cat_tutHideNextTextButton();
}

sdword kasfTutNextTextButtonClicked(void)  // KAS TutNextTextButtonClicked, cataclysm.exe 0x00496630
{
    return cat_tutNextTextButtonClicked();
}

void kasfTutShowBackButton(void)  // KAS TutShowBackButton, cataclysm.exe 0x004966a0
{
    tutShowBackButton();
}

void kasfTutHideBackButton(void)  // KAS TutHideBackButton, cataclysm.exe 0x004966b0
{
    tutHideBackButton();
}

void kasfTutShowPrevButton(void)  // KAS TutShowPrevButton, cataclysm.exe 0x00496640
{
    tutShowPrevButton();
}

void kasfTutHidePrevButton(void)  // KAS TutHidePrevButton, cataclysm.exe 0x00496650
{
    cat_tutHidePrevButton();
}

sdword kasfTutPrevButtonClicked(void)  // KAS TutPrevButtonClicked, cataclysm.exe 0x00496660
{
    return cat_tutPrevButtonClicked();
}

void kasfTutShowQuitButton(void)  // KAS TutShowQuitButton, cataclysm.exe 0x00496670
{
    cat_tutShowQuitButton();
}

void kasfTutHideQuitButton(void)  // KAS TutHideQuitButton, cataclysm.exe 0x00496680
{
    cat_tutHideQuitButton();
}

sdword kasfTutQuitButtonClicked(void)  // KAS TutQuitButtonClicked, cataclysm.exe 0x00496690
{
    return cat_tutQuitButtonClicked();
}

void kasfTutSaveLesson(sdword Num, char *pName)  // KAS TutSaveLesson, cataclysm.exe 0x004966c0
{
    tutSaveLesson(Num, pName);
}

void kasfTutShowImages(char *szImages)  // KAS TutShowImages, cataclysm.exe 0x004966e0
{
    tutShowImages(szImages);
}

void kasfTutHideImages(void)  // KAS TutHideImages, cataclysm.exe 0x004966f0
{
    tutHideImages();
}

void kasfTutEnableEverything(void)  // KAS TutEnableEverything, cataclysm.exe 0x00496700
{
    tutEnableEverything();
}

void kasfTutDisableEverything(void)  // KAS TutDisableEverything, cataclysm.exe 0x00496710
{
    tutDisableEverything();
}

void kasfTutEnableFlags(char *pFlags)  // KAS TutEnableFlags, cataclysm.exe 0x00496720
{
    tutSetEnableFlags(pFlags, 1);
}

void kasfTutDisableFlags(char *pFlags)  // KAS TutDisableFlags, cataclysm.exe 0x00496730
{
    tutSetEnableFlags(pFlags, 0);
}

void kasfTutForceUnpaused(void)  // KAS TutForceUnpaused, cataclysm.exe 0x00496740
{
    universePause = FALSE;
}

sdword kasfTutGameSentMessage(char *commandNames)  // KAS TutGameSentMessage, cataclysm.exe 0x00496750
{
    return tutGameSentMessage(commandNames);
}

void kasfTutResetGameMessageQueue(void)  // KAS TutResetGameMessageQueue, cataclysm.exe 0x00496760
{
    tutResetGameMessageQueue();
}

sdword kasfTutContextMenuDisplayedForShipType(char *shipType)  // KAS TutContextMenuDisplayedForShipType, cataclysm.exe 0x00496770
{
    return tutContextMenuDisplayedForShipType(shipType);
}

void kasfTutResetContextMenuShipTypeTest(void)  // KAS TutResetContextMenuShipTypeTest, cataclysm.exe 0x00496780
{
    tutResetContextMenuShipTypeTest();
}

void kasfTutRedrawEverything(void)  // KAS TutRedrawEverything, cataclysm.exe 0x00496790
{
    bitSet(regRootRegion.status, RSF_ReallyDirty);
}

sdword kasfBuildManagerShipTypeInBatchQueue(char *shipType)  // KAS BuildManagerShipTypeInBatchQueue, cataclysm.exe 0x004967a0
{
    return tutBuildManagerShipTypeInBatchQueue(shipType);
}

sdword kasfBuildManagerShipTypeInBuildQueue(char *shipType)  // KAS BuildManagerShipTypeInBuildQueue, cataclysm.exe 0x004967b0
{
    return tutBuildManagerShipTypeInBuildQueue(shipType);
}

sdword kasfBuildManagerShipTypeSelected(char *shipType)  // KAS BuildManagerShipTypeSelected, cataclysm.exe 0x004967c0
{
    return tutBuildManagerShipTypeSelected(shipType);
}

sdword kasfTutCameraFocusedOnShipType(char *shipTypes)  // KAS TutCameraFocusedOnShipType, cataclysm.exe 0x004967d0
{
    return tutCameraFocusedOnShipType(shipTypes);
}

/*-----------------------------------------------------------------------------
    Name        : kasfCameraFocus                                   0x00496870
    Description : CAT: KAS "CameraFocus": the scripted focus (cat_ccFocusScripted) on the ships unless the camera
                  already focuses on the same selection (then nothing); with no ships, on the current selection
----------------------------------------------------------------------------*/
void kasfCameraFocus(GrowSelection *ships)
{
    MaxSelection tempselected;

    if (ships->selection->numShips < 1)
    {
        tempselected = selSelected;
        cat_ccFocusScripted(&universe.mainCameraCommand, (FocusCommand *)&tempselected);
        return;
    }
    if (!SelectionsAreEquivalent((SelectCommand *)&currentCameraStackEntry(&universe.mainCameraCommand)->focus,
                                 ships->selection))
    {
        ccCameraTimeoutOverride = TRUE;
        if (piePointSpecMode != PSM_Idle)
        {
            piePointModeOnOff(PIE_SPEC_None, NULL);
        }
        spMainScreen(TRUE);
        cat_ccFocusScripted(&universe.mainCameraCommand, (FocusCommand *)ships->selection);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfCameraZoom / kasfCameraDistance               0x00496910 / 0x00496930
    Description : CAT: KAS "CameraZoom" (cat_ccZoomScripted, zoom * 0.01f) and "CameraDistance"
                  (cat_ccSetDistanceScripted)
----------------------------------------------------------------------------*/
void kasfCameraZoom(sdword zoom)
{
    cat_ccZoomScripted(&universe.mainCameraCommand, (real32)((real64)zoom * (real64)0.01f));
}

void kasfCameraDistance(sdword distance)
{
    cat_ccSetDistanceScripted(&universe.mainCameraCommand, (real32)distance);
}

/*-----------------------------------------------------------------------------
    Name        : kasfCameraFocusClose                              0x00496950
    Description : CAT: KAS "CameraFocusClose": the scripted close focus (cat_ccFocusCloseScripted) on the ships, or
                  on the current selection when there are none
----------------------------------------------------------------------------*/
void kasfCameraFocusClose(GrowSelection *ships)
{
    MaxSelection tempselected;

    if (ships->selection->numShips > 0)
    {
        ccCameraTimeoutOverride = TRUE;
        if (piePointSpecMode != PSM_Idle)
        {
            piePointModeOnOff(PIE_SPEC_None, NULL);
        }
        spMainScreen(TRUE);
        cat_ccFocusCloseScripted(&universe.mainCameraCommand, (FocusCommand *)ships->selection);
        return;
    }
    tempselected = selSelected;
    cat_ccFocusCloseScripted(&universe.mainCameraCommand, (FocusCommand *)&tempselected);
}

// CAT: 0x004967e0: with no ships the current selection is focused without sending the tutorial's "KB_Focus" message
void kasfTutCameraFocus(GrowSelection *ships)
{
    if(ships->selection->numShips > 0)
    {
        ccCameraTimeoutOverride = TRUE;

        // go to a clean main screen
        if (piePointSpecMode != PSM_Idle)
        {
            piePointModeOnOff(PIE_SPEC_None, NULL);   // CAT: off, mode 0 as the exe
        }
        spMainScreen(TRUE);
        ccFocus(&universe.mainCameraCommand, (FocusCommand *)ships->selection);
    }
    else
    {
        //if ships has no ships, focus on selselected
        MaxSelection tempselected;

        tempselected = selSelected;     //copy structure
        ccFocus(&universe.mainCameraCommand,(FocusCommand *)&tempselected);
    }

}

void kasfTutCameraFocusDerelictType(char *derelictType)  // KAS TutCameraFocusDerelictType, cataclysm.exe 0x004969e0
{
    Derelict *derelict;
    Node *node;
    DerelictSelection focus;
    DerelictType dt = StrToDerelictType(derelictType);

    focus.numDerelicts = 1;

    // go through the entire list of derelicts
    node = universe.DerelictList.head;
    while (node != NULL)
    {
        derelict = (Derelict *)listGetStructOfNode(node);
        if (derelict->derelicttype == dt)
        {
            ccCameraTimeoutOverride = TRUE;

            // go to a clean main screen
            if (piePointSpecMode != PSM_Idle)
            {
                piePointModeOnOff(PIE_SPEC_None, NULL);   // CAT: off, mode 0 as the exe
            }
            spMainScreen(TRUE);

            focus.DerelictPtr[0] = derelict;
            ccFocus(&universe.mainCameraCommand, (FocusCommand *)&focus);
            return;
        }
        node = node->next;
    }
}

// CAT: 0x00496a60: with no ships the current selection is focused without the "KB_Focus" message
void kasfTutCameraFocusFar(GrowSelection *ships)
{
    if (ships->selection->numShips > 0)
    {
        spMainScreen(TRUE);
        ccCameraTimeoutOverride = TRUE;
        ccFocusFar(&universe.mainCameraCommand, (FocusCommand *)ships->selection, &universe.mainCameraCommand.actualcamera);
    }
    else
    {
        //if ships has no ships, focus on selselected
        MaxSelection tempselected;

        tempselected = selSelected;     //copy structure
        ccFocusFar(&universe.mainCameraCommand,(FocusCommand *)&tempselected, &universe.mainCameraCommand.actualcamera);
    }

}


void kasfTutCameraFocusCancel(void)  // KAS TutCameraFocusCancel, cataclysm.exe 0x00496ae0
{
    ccCameraTimeoutOverride = FALSE;
    ccCancelFocus(&universe.mainCameraCommand);
}


/*-----------------------------------------------------------------------------
    Name        : kasfDisablePlayer
    Description : Disables player commands
    Inputs      : toggle - if TRUE the player's commands gets disabled,
                           if FALSE, the player's commands get enabled
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void kasfDisablePlayer(bool32 toggle)  // KAS DisablePlayer, cataclysm.exe 0x00496b00
{
    if (toggle)
    {
        mrDisable();
    }
    else
    {
        mrEnable();
    }
}


sdword kasfTutShipsInView(GrowSelection *ships)  // KAS TutShipsInView, cataclysm.exe 0x00496b20
{
    udword i, inview = 0;

    for (i=0;i<ships->selection->numShips;i++)
    {
        if (rndShipVisible((SpaceObj *)ships->selection->ShipPtr[i], mrCamera))
        {
            inview++;
        }
    }

    return inview;
}

sdword kasfTutShipsTactics(GrowSelection *ships)  // KAS TutShipsTactics, cataclysm.exe 0x00496b70
{
    if (ships->selection->numShips)
        return ships->selection->ShipPtr[0]->tacticstype;
    else
        return -1;
}

sdword kasfTutPieDistance(void)  // KAS TutPieDistance, cataclysm.exe 0x00496b90
{
    extern real32 pieDistance;

    return (sdword)pieDistance;
}

sdword kasfTutPieHeight(void)  // KAS TutPieHeight, cataclysm.exe 0x00496ba0
{
    extern real32 pieHeight;

    return (sdword)pieHeight;
}

void kasfForceFISensors(void)  // KAS ForceFISensors, cataclysm.exe 0x00496bb0
{
    smFleetIntel = TRUE;
}

void kasfOpenSensors(sdword flag)  // KAS OpenSensors, cataclysm.exe 0x00496bc0
{
    universeTurbo = FALSE;

    //if flag isn't a TRUE/FALSE value
    //note: opening the sensors manager at a given distance only
    //      requires the "flag" variable to be larger than 1000 or
    //      so.  If need be, values above 1 can be used to flag
    //      different cases
    if (flag > 1)
    {
        //store the sensors manager zoom length
        kasStoreSMInitialDistance = (sdword)smInitialDistance;
        //set the sensors manager zoom length
        smInitialDistance         = (real32)flag;
        smFleetIntel              = TRUE;
    }
    else
    {
        smFleetIntel = flag;
    }

    spMainScreen(FALSE);                    // CAT: the sensors manager is not closed (kasfOpenSensors pushes 0)
    catSmForceOpen = TRUE;                  // CAT (M2-R6 managers, sensors): opened past the disable flag (0x00a5b22c,
    smSensorsBegin(NULL, NULL);             //      kasfOpenSensors 0x00496bc0)
    catSmForceOpen = FALSE;
}

// CAT: 0x00496c30: the fleet-intelligence sensors flag is cleared also when the sensors manager was not open
void kasfCloseSensors(sdword flag)
{
    if ((smSensorsActive) && (!smZoomingIn) && (!smZoomingOut))
    {
        if (kasStoreSMInitialDistance)
        {
            //restore the sensors manager zoom length
            smInitialDistance    = (real32)kasStoreSMInitialDistance;
            kasStoreSMInitialDistance = 0;
        }

        //flags don't do anything yet
        smSensorsClose(NULL, NULL);
    }
    smFleetIntel = FALSE;
}

sdword kasfSensorsIsOpen(sdword flag)  // KAS SensorsIsOpen, cataclysm.exe 0x00496c80
{
    if (flag)
    {
        //returns true if the fleet intel window is open
        return smFleetIntel;
    }
    else
    {
        //returns true if the player is in the sensors manager but not fleet intel
        return ((smSensorsActive || smZoomingIn || smZoomingOut) && !smFleetIntel);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfResearchIsOpen / kasfBuildIsOpen              0x00496cc0 / 0x00496cd0
    Description : CAT: KAS "ResearchIsOpen" (the research screen's active word rmGUIActive; flags not read) and
                  "BuildIsOpen" (a jump to cat_cmIsOpen with the flags)
----------------------------------------------------------------------------*/
sdword kasfResearchIsOpen(sdword flags)
{
    (void)flags;
    return rmGUIActive;
}

sdword kasfBuildIsOpen(sdword flags)
{
    return cat_cmIsOpen(flags);
}

void kasfSensorsWeirdness(sdword value)  // KAS SensorsWeirdness, cataclysm.exe 0x00496ce0
{
//    static nisstatic stat2;
//    static nisstatic stat3;

    smSensorWeirdness = value;

/*    if (flag)
    {
        stat1.index = 0;

        nisStaticOnExp(&stat1);
//        nisStaticOnExp(&stat2);
//        nisStaticOnExp(&stat3);
        smSensorWeirdnessInit();
    }
    else
    {
        nisStaticOffExp(0);
//        nisStaticOffExp(1);
//        nisStaticOffExp(2);
    }
*/
}

void kasfAllowPlayerToResearch(char *name)  // KAS TechSetResearch, cataclysm.exe 0x00496cf0
{
    AllowPlayerToResearch(name);
}

void kasfAllowPlayerToPurchase(char *name)  // KAS TechSetPurchase, cataclysm.exe 0x00496d00
{
    AllowPlayerToPurchase(name);
}

void kasfPlayerAcquiredTechnology(char *name)  // KAS TechSet, cataclysm.exe 0x00496d10
{
    PlayerAcquiredTechnology(name);
}

sdword kasfCanPlayerResearch(char *name)  // KAS TechGetResearch, cataclysm.exe 0x00496d20
{
    return CanPlayerResearch(name);
}

sdword kasfCanPlayerPurchase(char *name)  // KAS TechGetPurchase, cataclysm.exe 0x00496d30
{
    return CanPlayerPurchase(name);
}

sdword kasfDoesPlayerHave(char *name)  // KAS TechGet, cataclysm.exe 0x00496d40
{
    return DoesPlayerHave(name);
}

void kasfSetBaseTechnologyCost(char *name, sdword cost)  // KAS TechSetCost, cataclysm.exe 0x00496d50
{
    SetBaseTechnologyCost(name, cost);
}

sdword kasfGetBaseTechnologyCost(char *name)  // KAS TechGetCost, cataclysm.exe 0x00496d70
{
    return GetBaseTechnologyCost(name);
}

sdword kasfTechIsResearching(void)  // KAS TechIsResearching, cataclysm.exe 0x00496d80
{
    return rmResearchingAnything(&universe.players[0]);
}

void kasfEnableTraderGUI(void)  // KAS TraderGUIDisplay, cataclysm.exe 0x00496d90
{
    tmEnableTraderGUI();
}

sdword kasfTraderGUIActive(void)  // KAS TraderGUIIsDisplayed, cataclysm.exe 0x00496da0
{
    return tmTraderGUIActive();
}

void kasfSetTraderDialog(sdword dialogNum, char *text)  // KAS TraderGUIDialogSet, cataclysm.exe 0x00496db0
{
    tmSetDialog(dialogNum, text);
}

void kasfSetTraderPriceScale(sdword percent)  // KAS TraderGUIPriceScaleSet, cataclysm.exe 0x00496dd0
{
    tmSetPriceScale(percent);
}

void kasfSetTraderDisabled(sdword disable)  // KAS TraderGUIDisable, cataclysm.exe 0x00496de0
{
    tmSetTradeDisabled(disable);
}

sdword kasfGetTraderPriceScale(void)  // KAS TraderGUIPriceScaleGet, cataclysm.exe 0x00496df0
{
    return tmGetPriceScale();
}

sdword kasfRUsGet(sdword player)  // KAS RUsGet, cataclysm.exe 0x00496e00
{
    return universe.players[player].resourceUnits;
}

void kasfRUsSet(sdword player, sdword RUs)  // KAS RUsSet, cataclysm.exe 0x00496e20
{
    universe.players[player].resourceUnits = RUs;
}

sdword kasfGetWorldResources(void)  // KAS GetWorldResources, cataclysm.exe 0x00496e40
{
    sdword total, numHarvestables, numAsteroid0s;

    univGetResourceStatistics(&total, &numHarvestables, &numAsteroid0s);

    return total;
}

void kasfSoundEvent(sdword event)  // KAS SoundEvent, cataclysm.exe 0x00496e60
{
    soundEventMisc(event);
}

void kasfSoundEventShips(GrowSelection *ships, sdword event)  // KAS SoundEventShips, cataclysm.exe 0x00496e80
{
    if (ships == NULL || ships->selection == NULL || ships->selection->numShips == 0)
        return;

    soundEvent(ships->selection->ShipPtr[0], event);
}

// CAT: 0x00496eb0: queued with no actor and no timeout (speechEventQueue(NULL, event, variable, -1, -1, player, -1,
//      -1.0f, -1); HW1's speechEventFleet used actor 0 and 5 s) and remembered as the link of kasfSpeechEventLink; the
//      fast-forward is not switched off (HW1 cleared universeTurbo)
static sdword kasfSpeechLink = -1;          // 0x00a43fe4 (name chosen here)

void kasfSpeechEvent(sdword event, sdword variable)
{
    subMessageEnded = 0;
    kasfSpeechLink = -1;
    kasfSpeechLink = speechEventQueue(NULL, event, variable, -1, -1, universe.curPlayerIndex, -1, -1.0f, -1);
}

/*-----------------------------------------------------------------------------
    Name        : kasfSpeechEventLink                               0x00496f00
    Description : CAT: KAS "SpeechEventLink": the event plays after the previous linked one (bFirstLink starts a new
                  chain); if it cannot be linked it is queued alone
----------------------------------------------------------------------------*/
void kasfSpeechEventLink(sdword event, sdword variable, sdword bFirstLink)
{
    sdword handle;

    subMessageEnded = 0;
    if (bFirstLink)
    {
        kasfSpeechLink = -1;
    }
    handle = speechEventQueue(NULL, event, variable, -1, -1, universe.curPlayerIndex, kasfSpeechLink, -1.0f, -1);
    if (!bFirstLink && (handle == -1))
    {
        handle = speechEventQueue(NULL, event, variable, -1, -1, universe.curPlayerIndex, -1, -1.0f, -1);
    }
    kasfSpeechLink = handle;
}

/*-----------------------------------------------------------------------------
    Name        : kasfSpeechEventNoLink                             0x00496f80
    Description : CAT: KAS "SpeechEventNoLink": as kasfSpeechEvent without remembering the link
----------------------------------------------------------------------------*/
void kasfSpeechEventNoLink(sdword event, sdword variable)
{
    subMessageEnded = 0;
    speechEventQueue(NULL, event, variable, -1, -1, universe.curPlayerIndex, -1, -1.0f, -1);
}

void kasfSpeechEventShips(GrowSelection *ships, sdword event, sdword variable)  // KAS SpeechEventShips, cataclysm.exe 0x00496fc0
{
    if (ships == NULL || ships->selection == NULL || ships->selection->numShips == 0)
        return;

    speechEvent(ships->selection->ShipPtr[0], event, variable);
}

// CAT: folded into the empty stub 0x0049ae30
void kasfToggleActor(sdword Actor, sdword on)
{
    (void)Actor;
    (void)on;
}

void kasfMusicPlay(sdword trackNum)  // KAS MusicPlay, cataclysm.exe 0x00497000
{
    soundEventPlayMusic((udword)trackNum);
}

void kasfMusicStop(sdword fadeTime)  // KAS MusicStop, cataclysm.exe 0x00497010
{
    soundEventStopMusic((real32)fadeTime);
}

// CAT: 0x00497020: the exe's LOD mask has bits 1 .. min(LOD, 7) (a loop from 8 down to 1 that shifts after each
//      test, so bit 0 is never set; GoK's mask had bits 0 .. LOD)
sdword kasfRenderedShips(GrowSelection *ships, sdword LOD) {
    sdword i;
    ubyte LODmask = 0;

    for (i = 8; i > 0; i--)
    {
        if (i <= LOD)
        {
            LODmask |= 1;
        }
        LODmask = (ubyte)(LODmask << 1);
    }

    if (ships == NULL || ships->selection == NULL)
        return 0;

    for (i = 0; i < ships->selection->numShips; ++i) // see if ship rendered at the specified LOD or lower
    {
        if (bitTest(ships->selection->ShipPtr[i]->obj.renderedLODs, LODmask)) {
            return 1;
        }
    }

    return 0;
}

void kasfResetShipRenderFlags(GrowSelection *ships)  // KAS ResetShipRenderFlags, cataclysm.exe 0x00497080
{
    sdword i;

    if (ships == NULL || ships->selection == NULL)
        return;

    for (i=0; i < ships->selection->numShips; i++)
        ships->selection->ShipPtr[i]->obj.renderedLODs = 0;
}

sdword kasfRenderedDerelicts(char *derelictType, sdword LOD)  // KAS RenderedDerelictType, cataclysm.exe 0x004970c0
{
    Derelict *derelict;
    Node *node;
    DerelictType dt = StrToDerelictType(derelictType);

    // go through the entire list of derelicts
    node = universe.DerelictList.head;
    while (node != NULL)
    {
        derelict = (Derelict *)listGetStructOfNode(node);
        if ((derelict->derelicttype == dt) &&
            bitTest(derelict->obj.renderedLODs, 1 << LOD))
            return 1;
        node = node->next;
    }
    return 0;
}

void kasfResetDerelictRenderFlags(char *derelictType)  // KAS ResetDerelictRenderFlags, cataclysm.exe 0x00497110
{
    Derelict *derelict;
    Node *node;
    DerelictType dt = StrToDerelictType(derelictType);

    // go through the entire list of derelicts
    node = universe.DerelictList.head;
    while (node != NULL)
    {
        derelict = (Derelict *)listGetStructOfNode(node);
        if (derelict->derelicttype == dt)
        {
            derelict->obj.renderedLODs = 0;
            break;
        }
        node = node->next;
    }
}

// CAT: folded into the empty stub 0x0049ae30 (cataclysm.exe has no gate search)
void kasfGateMoveToNearest(void)
{
}

void kasfGateDestroy(hvector *gatePoint)  // KAS GateDestroy, cataclysm.exe 0x00497140
{
    //hyperspace
    hsStaticDestroy(gatePoint);
}

void kasfGateShipsIn(GrowSelection *ships, hvector *gatePoint)  // KAS GateShipsIn, cataclysm.exe 0x00497150
{
    //hyperspace
    spHyperspaceSelectionInStatic(ships->selection, gatePoint);
}

void kasfGateShipsOut(GrowSelection *ships, hvector *gatePoint)  // KAS GateShipsOut, cataclysm.exe 0x00497170
{
    //hyperspace
    RemoveShipsFromDoingStuff(&universe.mainCommandLayer,ships->selection,0x15);   // CAT: reason 0x15 as the exe (kasfGateShipsOut 0x00497178) [audit: M2-R5 command-tactics]
    spHyperspaceSelectionOutStatic(ships->selection);
}


// CAT: folded into the stub 0x006fe9a0 (returns 0)
sdword kasfGateShipsOutNearest(GrowSelection *ships)
{
    (void)ships;
    return 0;
}

//creates a list of the player's currently selected ships
//added for the tutorial
sdword kasfFindSelectedShips(GrowSelection *ships)       // CAT: the table's number (HW1 udword); KAS FindSelectedShips, cataclysm.exe 0x004971a0
{
    udword i;

    if (ships == NULL)
        return 0;

    while (ships->selection->numShips)
        clRemoveShipFromSelection(ships->selection, ships->selection->ShipPtr[0]);

    for (i=0;i<selSelected.numShips;i++)
        growSelectAddShip(ships, selSelected.ShipPtr[i]);

    return selSelected.numShips;
}

//
//  returns how many ships in the selection are disabled
//
sdword kasfShipsDisabled(GrowSelection *ships)  // KAS ShipsDisabled, cataclysm.exe 0x00497200
{
    udword i;
    sdword count = 0;

    if (ships == NULL)
        return 0;

    for (i = 0; i < ships->selection->numShips; ++i)
    {
        if (bitTest(ships->selection->ShipPtr[i]->header.flags, SOF_Disabled))
            ++count;
    }

    return count;
}

//saves a single player level
/*-----------------------------------------------------------------------------
    Name        : cat_kasAutoSaveOnLoad                             0x00497230
    Description : CAT: at the end of a single-player load (LoadGame 0x004fe0e0): a game the script saved by itself
                  (the global KAS variable G_AutoSave exists) counts the load in G_TimesGameLoaded, is saved again
                  under the loaded name and loses G_AutoSave. CAT-FIX: a missing G_TimesGameLoaded is not counted
                  (the exe writes through the NULL)
----------------------------------------------------------------------------*/
void cat_kasAutoSaveOnLoad(char *filename)
{
    CatKasVar *autoSave = cat_kasVarFind(0, cat_kasHashString("G_AutoSave"));
    CatKasVar *timesLoaded;

    if (autoSave != NULL)
    {
        timesLoaded = cat_kasVarFind(0, cat_kasHashString("G_TimesGameLoaded"));
        if (timesLoaded != NULL)
        {
            timesLoaded->value++;
        }
        SaveGame(filename);
        cat_kasVarDestroyEntry(autoSave);
    }
}

// CAT: kasfSaveLevel 0x00497290 only asks for the save: the level number and name are kept and univUpdate carries the
//      save out last in the frame (0x0054ce03: cat_kasSaveLevelNow); HW1 saved here at once
static sdword kasfSaveLevelNumber = 0;                       // 0x00a43fe8 (names chosen here)
bool32 cat_kasSaveLevelRequested = FALSE;                    // 0x00a43fec
static char kasfSaveLevelName[128];                          // 0x00a43f74 (the exe copies the name without a bound)

void kasfSaveLevel(sdword LevelNum, char *LevelName)
{
    kasfSaveLevelNumber = LevelNum;
    cat_kasSaveLevelRequested = TRUE;
    memStrncpy(kasfSaveLevelName, LevelName, sizeof(kasfSaveLevelName) - 1);   // HWI: bounded
}

/*-----------------------------------------------------------------------------
    Name        : cat_kasSaveLevelNow                               0x004972d0
    Description : CAT: the save kasfSaveLevel asked for: the collision blobs are brought up to date, the global KAS
                  variables G_TimesGameLoaded (0) and G_AutoSave (1) are set, the game is saved as
                  "<Training path><name>" for level 0, else "<SinglePlayer path><NN> - <name>", the result is shown
                  and G_AutoSave is removed again
----------------------------------------------------------------------------*/
void cat_kasSaveLevelNow(void)
{
    CatKasVar *autoSave;

    cat_kasSaveLevelRequested = FALSE;
    // Disable autosaving when taking debug screenshots
    if (debugScreenshots) return;                           // GoK: the /debugScreenshots run saves nothing (kept)

    collUpdateCollBlobs();
    collUpdateObjsInCollBlobs();

    cat_kasVarFindOrCreate(0, cat_kasHashString("G_TimesGameLoaded"))->value = 0;
    autoSave = cat_kasVarFindOrCreate(0, cat_kasHashString("G_AutoSave"));
    autoSave->value = 1;

    if (kasfSaveLevelNumber == 0)
    {
        sprintf(CurrentLevelName, "%s%s", TutorialSavedGamesPath, kasfSaveLevelName);
    }
    else
    {
        sprintf(CurrentLevelName, "%s%02d - %s", SinglePlayerSavedGamesPath, kasfSaveLevelNumber, kasfSaveLevelName);
    }
    if (SaveGame(CurrentLevelName))
        clCommandMessage(strGetString(strSavedGame), 0xffffffff);
    else
        clCommandMessage(strGetString(strPatchUnableWriteFile), 0xffffffff);
    cat_kasVarDestroyEntry(autoSave);
}

void kasfClearScreen(void)  // KAS ClearScreen, cataclysm.exe 0x004973b0
{
    rndClear();
}


//starts the NIS widescreen thing under KAS control
/*-----------------------------------------------------------------------------
    CAT (M2-R8 kasf library): the KAS letterbox. A KAS widescreen fades the NIS scissor bars in (nisScissorFadeIn
    0x00a4cb38, which the time and pause keys test) and marks itself active (0x00a43ff0); when the bars are fully in,
    nisUpdateTask finishes it (cat_kasWideScreenInFinish 0x004973d0: the main screen, the lockout, the cursor hidden).
-----------------------------------------------------------------------------*/
static bool32 kasfWideScreenIsActive = FALSE;               // 0x00a43ff0 (name chosen here)
static bool32 kasfWideScreenCloseSensors = TRUE;            // 0x00882104 (.data 1; name chosen here): the finish closes
                                                            //      the sensors manager (every widescreen-in clears it)

// 0x004973c0
bool32 cat_kasWideScreenActive(void)
{
    return kasfWideScreenIsActive;
}

// 0x004973d0
void cat_kasWideScreenInFinish(void)
{
    if (piePointSpecMode != PSM_Idle)
    {
        piePointModeOnOff(PIE_SPEC_None, NULL);
    }
    if (kasfWideScreenCloseSensors)
    {
        spMainScreenAndLockout(SPLOCKOUT_MR);
    }
    else
    {
        cat_spMainScreenAndLockoutKeepSensors(SPLOCKOUT_MR);
    }
    kasfWideScreenIsActive = FALSE;
    catUnivFrameStep = FALSE;
    mouseCursorHide();
}

// the body the exe has three times (kasfWideScreenIn 0x00497420, cat_kasWideScreenInHyperspace 0x004974d0,
//      kasfWideScreenInDontCloseSensors 0x00497580: identical but for their addresses): the fast-forward off, the
//      time compression back to normal, a fade-out cut short, the letterbox sound (0x123) and the fade-in (frames *
//      1/30 as a float, 0x3d088889 at 0x0072ab58 [audit: was the literal 0.0333333f, 0x3d088880]) unless the bars are fully in already, the closing flag cleared, the letterbox active, the
//      camera's pilot mode bit cleared and the letterbox volume on
static void kasfWideScreenInBody(sdword frames)
{
    universeTurbo = FALSE;
    cat_taskTimeCompressionIndexSet(cat_taskTimeCompressionDefaultIndex());
    cat_taskTimeCompressionApply();
    cat_taskTimeCompressionPrint();
    if (!nisFullyScissored)
    {
        if (nisScissorFadeOut != 0.0f)
        {
            nisScissorFade = 0.0f;
            nisScissorFadeOut = 0.0f;
            nisScissorFadeTime = 0.0f;
        }
        soundEvent(NULL, CAT_UI_Letterbox);         // CAT: Cataclysm's id 0x123 (0x00497481)
        nisScissorFadeIn = (real32)((real64)frames * (real64)(1.0f / 30.0f));
        nisTaskResume();
    }
    kasfWideScreenCloseSensors = FALSE;
    kasfWideScreenIsActive = TRUE;
    if (bitTest(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP))
    {
        bitClear(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP);
    }
    cat_soundEventLetterboxVolume(TRUE);
}

void kasfWideScreenIn(sdword frames)
{
    kasfWideScreenInBody(frames);
}

// 0x004974d0 (no function-table row: the single-player hyperspace code's)
void cat_kasWideScreenInHyperspace(sdword frames)
{
    kasfWideScreenInBody(frames);
}

/*-----------------------------------------------------------------------------
    Name        : kasfWideScreenInDontCloseSensors                  0x00497580
    Description : CAT: KAS "wideScreenInDontCloseSensors" (the same body as kasfWideScreenIn in the exe)
----------------------------------------------------------------------------*/
void kasfWideScreenInDontCloseSensors(sdword frames)
{
    kasfWideScreenInBody(frames);
}

// CAT: 0x00497630: a fade-in is cut short, the unletterbox sound (0x124), the fade-out, the lockout lifted, the
//      letterbox volume off and the cursor shown
void kasfWideScreenOut(sdword frames)
{
    if (nisScissorFadeIn != 0.0f)
    {
        nisScissorFade = 0.0f;
        nisScissorFadeIn = 0.0f;
        nisScissorFadeTime = 0.0f;
    }
    soundEvent(NULL, CAT_UI_Unletterbox);           // CAT: Cataclysm's id 0x124 (0x0049766a)
    nisScissorFadeOut = (real32)((real64)frames * (real64)(1.0f / 30.0f));
    nisTaskResume();
    spUnlockout();
    cat_soundEventLetterboxVolume(FALSE);
    mouseCursorShow();
}

// CAT: 0x004976a0: milliseconds * 0.001f (0x0072a650) on the x87 stack
void kasfSubtitleSimulate(sdword actor, sdword milliseconds, char *speech)
{
    subTitleAdd(actor, kasfDummyEventNumber, speech, strlen(speech), (real32)((real64)milliseconds * (real64)0.001f));
    subMessageEnded = 0;
    kasfDummyEventNumber++;
}

/*-----------------------------------------------------------------------------
    Name        : kasfSubtitleReset                                 0x004976f0
    Description : CAT: KAS "SubtitleReset": the subtitles are cleared, the intel event counts as ended and every
                  speaking actor (mask 0x3ff800) stops at once
----------------------------------------------------------------------------*/
void kasfSubtitleReset(void)
{
    subReset();
    subMessageEnded = 1;
    speechEventActorStop(0x3ff800, 0.0f);
}

//display a location card
// CAT: 0x00497710: milliseconds * 0.001f on the x87 stack, and the intel-event flag cleared (as kasfSubtitleSimulate)
void kasfLocationCard(sdword milliseconds, char *location)
{
    char buffer[0x7f8];

    snprintf(buffer, 0x7f7, "#r%x#t%x%s", STR_LocationCard, STT_LocationCard, location);
    subTitleAdd(STA_LocationCard, kasfDummyEventNumber, buffer, strlen(buffer), (real32)((real64)milliseconds * (real64)0.001f));
    subMessageEnded = 0;
    kasfDummyEventNumber++;
}

sdword kasfRaceOfHuman(void)  // KAS RaceOfHuman, cataclysm.exe 0x00497790
{
    return universe.players[0].race;
}

//hides some ships and sets the SOF_HideSave bit where appropriate so the NIS can unhide them
void kasfHideShips(GrowSelection *ships)  // KAS HideShips, cataclysm.exe 0x004977a0
{
    sdword index;

    for (index = 0; index < ships->selection->numShips; index++)
    {
        if (bitTest(ships->selection->ShipPtr[index]->header.flags, SOF_Hide))
        {                                                   //if ship already hidden
            bitSet(ships->selection->ShipPtr[index]->header.flags, SOF_HideSave);
        }
        else
        {
            univRemoveObjFromRenderList((SpaceObj *)ships->selection->ShipPtr[index]);
            bitSet(ships->selection->ShipPtr[index]->header.flags, SOF_Hide);

        }
    }
}

//unhides some ships and sets the SOF_HideSave bit where appropriate so the NIS can unhide them
void kasfUnhideShips(GrowSelection *ships)  // KAS UnhideShips, cataclysm.exe 0x00497800
{
    sdword index;

    for (index = 0; index < ships->selection->numShips; index++)
    {
        if (bitTest(ships->selection->ShipPtr[index]->header.flags, SOF_HideSave))
        {                                                   //if ship was hidden to begin with
            bitClear(ships->selection->ShipPtr[index]->header.flags, SOF_HideSave);
        }
        else
        {
            bitClear(ships->selection->ShipPtr[index]->header.flags, SOF_Hide);

        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : kasfDeleteShips
    Description : Deletes a select of ships from the game
    Inputs      : ships - the ships to delete
    Outputs     : wipes the ships from the game
    Return      : void
----------------------------------------------------------------------------*/
// CAT: 0x00497850: a mimicking ship is cleaned up first; the wipe is univWipeShipOutOfExistence's inline (never its
//      free-directly branch), then the camera, the two selections, the ship totals, the sensors manager and every
//      player's mothership pointer forget the ship
// CAT: 0x00497850: a mimicking ship is cleaned up first; then univWipeShipOutOfExistence's steps inline (never its
//      free-directly branch); then the camera, the two selections, the ship totals, the sensors manager and every
//      player's mothership pointer forget the ship
void kasfDeleteShips(GrowSelection *ships)
{
    Ship *ship;
    sdword i;

    while (ships->selection->numShips)
    {
        ship = ships->selection->ShipPtr[0];
        if (cat_shipIsMimicking(ship))
        {
            cat_mimicCleanup(ship);
        }
        cat_univShipFOWDeactivate(ship);
        if (ship->catBeastBindings != NULL)
        {
            memFree(ship->catBeastBindings);
            ship->catBeastBindings = NULL;
        }
        if (ship->catSalvageConvertBindings != NULL)
        {
            memFree(ship->catSalvageConvertBindings);
            ship->catSalvageConvertBindings = NULL;
        }
        if (ship->shiplink.belongto != NULL)
        {
            listRemoveNode(&ship->shiplink);
        }
        if (ship->impactable.impactablelink.belongto != NULL)
        {
            listRemoveNode(&ship->impactable.impactablelink);
        }
        nisObjectDied((SpaceObj *)ship);
        etgShipDied(ship);
        univRemoveObjFromRenderList((SpaceObj *)ship);
        univFreeShipContents(ship);
        if (ship->header.objlink.belongto != NULL)
        {
            listDeleteNode(&ship->header.objlink);
        }
        ccRemoveShip(&universe.mainCameraCommand, ship);
        clRemoveShipFromSelection((SelectCommand *)&selSelected, ship);
        clRemoveShipFromSelection((SelectCommand *)&selSelecting, ship);
        ioUpdateShipTotals();
        smObjectDied(ship);
        for (i = 0; i < universe.numPlayers; i++)
        {
            if (universe.players[i].PlayerMothership == ship)
            {
                universe.players[i].PlayerMothership = NULL;
            }
        }
    }
}



/*-----------------------------------------------------------------------------
    Name        : kasfRotateDerelictType
    Description : Rotates a type of derelict
    Inputs      : derelictType - string of derelict type
                  rot_x - rotation speed around x axis  (*1000)
                  rot_y - rotation speed around y axis  (*1000)
                  rot_z - rotation speed around z axis  (*1000)
                  variation - amount of random rotation variation
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
// CAT: 0x00497980: each value times 0.001f (0x0072a650) rounded to float; the random spin is
//      ranRandom * ((variation - -variation) * 2^-32) + -variation, then the rotation added, on the x87 stack
//      (HW1: value / 1000 and frandyrandombetween)
void kasfRotateDerelictType(char *derelictType, sdword rot_x, sdword rot_y, sdword rot_z, sdword variation)
{
    Derelict *derelict;
    Node *node;
    DerelictType dt = StrToDerelictType(derelictType);
    real32 realrot_x     = (real32)((real64)rot_x * (real64)0.001f),
           realrot_y     = (real32)((real64)rot_y * (real64)0.001f),
           realrot_z     = (real32)((real64)rot_z * (real64)0.001f),
           realvariation = (real32)((real64)variation * (real64)0.001f);
    real32 low, range;

    //go through the entire list of derelicts and
    //give the appropriate ones a rotation
    node = universe.DerelictList.head;

    while (node != NULL)
    {
        derelict = (Derelict *)listGetStructOfNode(node);
        if (derelict->derelicttype == dt)
        {
            low = -realvariation;
            range = (real32)(((real64)realvariation - (real64)low) * (real64)2.3283064e-10f);
            derelict->rotatable.rotinfo.rotspeed.x = (real32)((real64)(udword)ranRandom(RANDOM_AI_PLAYER) * (real64)range + (real64)low + (real64)realrot_x);
            derelict->rotatable.rotinfo.rotspeed.y = (real32)((real64)(udword)ranRandom(RANDOM_AI_PLAYER) * (real64)range + (real64)low + (real64)realrot_y);
            derelict->rotatable.rotinfo.rotspeed.z = (real32)((real64)(udword)ranRandom(RANDOM_AI_PLAYER) * (real64)range + (real64)low + (real64)realrot_z);
        }
        node = node->next;
    }
}



/*-----------------------------------------------------------------------------
    Name        : kasfUniversePause
    Description : Pause all ships in the universe
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void kasfUniversePause(void)  // KAS PauseUniverse, cataclysm.exe 0x00497a90
{
    nisUniversePause = TRUE;
#if NIS_VERBOSE_LEVEL >= 1
    dbgMessage("KAS has PAUSED the universe.");
#endif
    //!!! pause KAS timers and FSM's
}

/*-----------------------------------------------------------------------------
    Name        : kasfUniverseUnpause
    Description : Pause all ships in the universe
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void kasfUniverseUnpause(void)  // KAS UnpauseUniverse, cataclysm.exe 0x00497aa0
{
    nisUniversePause = FALSE;
#if NIS_VERBOSE_LEVEL >= 1
    dbgMessage("KAS has UNPAUSED the universe.");
#endif
    //!!! unpause KAS timers and FSM's
}

// pause the build

void kasfPauseBuildShips(void)  // KAS PauseShipBuilding, cataclysm.exe 0x00497ab0
{
    cmPauseAllJbos();
}

void kasfUnpauseBuildShips(void)  // KAS UnpauseShipBuilding, cataclysm.exe 0x00497ac0
{
    cmUnPauseAllJobs();
}


/*-----------------------------------------------------------------------------
    Name        : kasfOtherKASPause/kasfOtherKASUnpause
    Description : Pause/unPause global KAS watches and timers not associated with
                    this FSM.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: 0x00497ad0: the other scopes' timers are those whose scope hash differs
void kasfOtherKASPause(void)
{
    kasUnpausedTeam = CurrentTeamP;
    timTimerPauseAllNotScoped(CurrentMissionScopeName);     //pause all the other timers
}
void kasfOtherKASUnpause(void)  // KAS UnpauseOtherKAS, cataclysm.exe 0x00497af0
{
    kasUnpausedTeam = NULL;
    timTimerUnpauseAll();                                   //unpause all paused timers
}

/*-----------------------------------------------------------------------------
    Name        : kasfIntelEventEnded
    Description : Returns TRUE only once if a fleet intel/command subtitled
                    speech event ends or the player hits SPACE to short-cut
                    the event.
    Inputs      :
    Outputs     :
    Return      : 1 if the current fleet intel subtitle expired, 2 if the user
                    hit space in the sensors manager.
----------------------------------------------------------------------------*/
sdword kasfIntelEventEnded(void)  // KAS IntelEventEnded, cataclysm.exe 0x00497b00
{
    sdword returnValue = subMessageEnded;
    subMessageEnded = 0;                                //only return TRUE once for each event

    // safety to make sure eventualy intel event ends
    if (!returnValue)
    {
        if (subMessageReturnedFalseTime == 0.0f)
        {
            subMessageReturnedFalseTime = universe.totaltimeelapsed;
        }
        else
        {
            if ( ((universe.totaltimeelapsed - subMessageReturnedFalseTime) > SUBMESSAGE_SAFETY_TIMEOUT) &&
                 (!subAnyCardsOnScreen()) )
            {
                subMessageReturnedFalseTime = 0.0f;
                return 1;           // subtitle finished by timeout
            }
        }
    }
    else
    {
        subMessageReturnedFalseTime = 0.0f;
    }

    return(returnValue);
}

/*-----------------------------------------------------------------------------
    Name        : kasfIntelEventNotEnded
    Description : Clears the subMessageEnded flag.  Should be called at the
                    init of any FSM that will use "IntelEventEnded"
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void kasfIntelEventNotEnded(void)  // KAS IntelEventNotEnded, cataclysm.exe 0x00497b80
{
    subMessageEnded = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : kasfForceIntelEventEnded
    Description : Clears the subMessageEnded flag.  Should be called at the
                    init of any FSM that will use "IntelEventEnded"
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void kasfForceIntelEventEnded(void)  // KAS ForceIntelEventEnded, cataclysm.exe 0x00497b90
{
    subMessageEnded = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : kasfSensorsStaticOn
    Description : Turns on sensors manager static
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void kasfSensorsStaticOn(void)  // KAS SensorsStaticOn, cataclysm.exe 0x00497ba0
{
    static nisstatic newStatic;                             //needs to be persistant

    newStatic.index = NIS_SMStaticIndex;
    //!!! make these values tweakable (see nisStaticOnSet for an example)
    newStatic.nLines = 50;
    newStatic.nLinesVariation = 5;
    newStatic.width = primScreenToGLScaleX(10);
    newStatic.widthVariation = primScreenToGLScaleX(1);
    // CAT: half a pixel lower: ((H / 2) + 0.325f) / H (kasfSensorsStaticOn 0x00497ba0, 0x0072a858)
    newStatic.y = 1.0f - 2.0f * (((real32)(MAIN_WindowHeight / 2) + 0.325f) / (real32)MAIN_WindowHeight);
    newStatic.yVariation = primScreenToGLScaleY(MAIN_WindowHeight / 2);
    newStatic.hue = 0.5f;
    newStatic.hueVariation = 0.5f;
    newStatic.lum = 0.9f;
    newStatic.lumVariation = 0.1f;
    newStatic.sat = 0.5f;
    newStatic.satVariation = 0.1f;
    newStatic.alpha = 0.6f;
    newStatic.alphaVariation = 0.1f;
    newStatic.bAlpha = TRUE;
    newStatic.bTextured = FALSE;
    newStatic.texture = TR_InvalidInternalHandle;

    nisStaticOnExp(&newStatic);
}

/*-----------------------------------------------------------------------------
    Name        : kasfSensorsStaticOff
    Description : Turns off static in the sensors manager.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void kasfSensorsStaticOff(void)  // KAS SensorsStaticOff, cataclysm.exe 0x00497ca0
{
    nisStaticOffExp(NIS_SMStaticIndex);
}

/*-----------------------------------------------------------------------------
    Name        : kasfGameEnd
    Description : Quit the game and play the exit plugscreens.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: folded into the empty stub 0x0049ae30
void kasfGameEnd(void)
{
}

/*-----------------------------------------------------------------------------
    Name        : kasfSpawnEffect
    Description : Spawns an effect with a single float parameter
    Inputs      : ships - ship(s) to spawn the effect on.
                  effectName - name of the effect
                  parameter - parameter to pass in (to be converted to a float)
    Outputs     :
    Return      :
    Note        : the first parameter to the effect will be the size of the ship.
----------------------------------------------------------------------------*/
void kasfSpawnEffect(GrowSelection *ships, char *effectName, sdword parameter)  // KAS SpawnEffect, cataclysm.exe 0x00497cb0
{
    real32 floatParameter;
    udword intParameter, intSize, index, count;
    static char *lastName = NULL;
    static etgeffectstatic *lastEffect;
    Ship **shipPtr;

    //retain name so this lookup does not have ot happen every single time
    if (effectName != lastName)
    {
        lastName = effectName;
        lastEffect = etgEffectStaticFind(effectName, FALSE);
#if ETG_ERROR_CHECKING
        if (lastEffect == NULL)
        {
            dbgFatalf(DBG_Loc, "Effect '%s' not found for KAS execution", effectName);
        }
#endif
    }
    // CAT (M2-R7 r6-leftovers): kasfSpawnEffect 0x00497cd5 converts the user parameter to real32 (fild / fstp) and
    //      passes that value's word, as HW1's commented-out lines did (GoK passed the integer)
    floatParameter = (real32)parameter;
    intParameter = Real32ToUdword(floatParameter);      //get the user parameter
    count = ships->selection->numShips;
    shipPtr = ships->selection->ShipPtr;
    for (index = 0; index < count; index++, shipPtr++)
    {
        floatParameter = (*shipPtr)->staticinfo->common.staticCollInfo.collspheresize;
        intSize = Real32ToUdword(floatParameter);            //get the size

        dbgAssertOrIgnore((*shipPtr)->header.objtype != OBJ_BulletType);
        etgEffectCreate(lastEffect, *shipPtr, NULL, NULL, NULL, 0.0f, EAF_Full, 2, SCALECAST(intSize), SCALECAST(intParameter));
    }
}


/*=============================================================================
    CAT (M2-R8 kasf library): Cataclysm's trailing block of KAS functions (cataclysm.exe 0x00497d30 ..
    0x0049ad80, after the HW1 kasf; each at the exe's body, its address on each). The KAS names are those of the
    function table (KASFuncTable.c); a C name differs from the KAS name where the table says so.
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : kasfSpecialOn                                     0x00497d30
    Description : CAT: KAS "SpecialOn": the current team's special ability against the ships (clWrapSpecial with a
                  heap copy of the targets); the exe returns nothing defined (0 here)
----------------------------------------------------------------------------*/
sdword kasfSpecialOn(GrowSelection *ships)
{
    SpecialCommand *targets;
    sdword i, numTargets;

    if ((CurrentTeamP != NULL) && (CurrentTeamP->shipList.selection->numShips != 0) && (ships != NULL) &&
        (ships->selection != NULL))
    {
        numTargets = ships->selection->numShips;
        targets = memAlloc(numTargets * sizeof(SpaceObjRotImpTarg *) + 8, "kasSpecialTargets", 0);
        for (i = 0; i < numTargets; i++)
        {
            targets->TargetPtr[i] = (SpaceObjRotImpTarg *)ships->selection->ShipPtr[i];
        }
        targets->numTargets = numTargets;
        clWrapSpecial(&universe.mainCommandLayer, CurrentTeamP->shipList.selection, targets);
        memFree(targets);
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : kasfSpecialOff                                    0x00497db0
    Description : CAT: KAS "SpecialOff": returns 0 and does nothing (the exe's body is xor eax, eax / ret)
----------------------------------------------------------------------------*/
sdword kasfSpecialOff(void)
{
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSpecialOn / kasfShipsSpecialOff          0x00497dc0 / 0x00497e40
    Description : CAT: KAS "ShipsSpecialOn": srcShips' special ability against destShips; "ShipsSpecialOff":
                  clWrapSpecial with no targets (the exe returns nothing defined: 0 here)
----------------------------------------------------------------------------*/
sdword kasfShipsSpecialOn(GrowSelection *srcShips, GrowSelection *destShips)
{
    SpecialCommand *targets;
    sdword i, numTargets;

    if ((srcShips != NULL) && (srcShips->selection != NULL) && (destShips != NULL) && (destShips->selection != NULL))
    {
        numTargets = destShips->selection->numShips;
        targets = memAlloc(numTargets * sizeof(SpaceObjRotImpTarg *) + 8, "kasSpecialTargets", 0);
        for (i = 0; i < numTargets; i++)
        {
            targets->TargetPtr[i] = (SpaceObjRotImpTarg *)destShips->selection->ShipPtr[i];
        }
        targets->numTargets = numTargets;
        clWrapSpecial(&universe.mainCommandLayer, srcShips->selection, targets);
        memFree(targets);
    }
    return 0;
}

sdword kasfShipsSpecialOff(GrowSelection *ships)
{
    if ((ships != NULL) && (ships->selection != NULL))
    {
        clWrapSpecial(&universe.mainCommandLayer, ships->selection, NULL);
    }
    return 0;
}

// 0x00497ea0: fuel / maxfuel below the fraction (param1 holds the fraction's bits)
static bool32 kasfPredLowFuel(Ship *ship, memsize fractionBits, memsize unused)
{
    udword bits = (udword)fractionBits;
    real32 fraction;

    (void)unused;
    memcpy(&fraction, &bits, sizeof(fraction));
    return (((real64)ship->fuel / (real64)ship->staticinfo->maxfuel) < (real64)fraction);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectLowFuel                            0x00497e70
    Description : CAT: KAS "ShipsSelectLowFuel": the ships below percentFuel * 0.01f of their fuel
----------------------------------------------------------------------------*/
sdword kasfShipsSelectLowFuel(GrowSelection *newShips, GrowSelection *originalShips, sdword percentFuel)
{
    real32 fraction = (real32)((real64)percentFuel * (real64)0.01f);
    udword bits;

    memcpy(&bits, &fraction, sizeof(bits));
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredLowFuel, (memsize)bits, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfDeleteShipsWhenMissionEnds                    0x00497ed0
    Description : CAT: KAS "DeleteShipsWhenMissionEnds": each ship is flagged (cat_spFlagDeleteWhenMissionEnds)
----------------------------------------------------------------------------*/
void kasfDeleteShipsWhenMissionEnds(GrowSelection *shipstodelete)
{
    sdword i;

    for (i = 0; i < shipstodelete->selection->numShips; i++)
    {
        cat_spFlagDeleteWhenMissionEnds(shipstodelete->selection->ShipPtr[i]);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfSetCurrentMimicItem / kasfSetMCVItemIndex     0x00497f10 / 0x00497f70
    Description : CAT: KAS "SetCurrentMimicItem" adds the ship type to a mimic disguise list, "SetMCVItemIndex" sets
                  a list's base row; item type 2 is the objects list (row 8), any other its own number
----------------------------------------------------------------------------*/
void kasfSetCurrentMimicItem(sdword itemType, char *shipName)
{
    ShipType type = StrToShipType(shipName);

    cat_mimicItemAdd((itemType == 2) ? CAT_MIMIC_PLAYER_OBJECTS : (ubyte)itemType, type);
}

void kasfSetMCVItemIndex(sdword itemType, sdword index)
{
    cat_spSetMCVItemIndex((itemType == 2) ? CAT_MIMIC_PLAYER_OBJECTS : (ubyte)itemType, index);
}

/*-----------------------------------------------------------------------------
    Name        : kasfMimic                                         0x00497fd0
    Description : CAT: KAS "Mimic": the ships disguise as the ship type (cat_mimicCommand mode 1), or, for a name
                  "A<digit>", as the asteroid of that size (mode 2)
----------------------------------------------------------------------------*/
void kasfMimic(GrowSelection *ships, char *shipName, sdword playerIndex)
{
    bool32 asteroid;
    sdword value, i;

    if ((shipName[0] == 'A') || (shipName[0] == 'a'))
    {
        asteroid = TRUE;
        value = (sdword)shipName[1] - '0';
    }
    else
    {
        asteroid = FALSE;
        value = StrToShipType(shipName);
    }
    if ((ships == NULL) || (ships->selection == NULL))
    {
        return;
    }
    for (i = 0; i < ships->selection->numShips; i++)
    {
        if (asteroid)
        {
            cat_mimicCommand(ships->selection->ShipPtr[i], 2, 0, playerIndex, value);
        }
        else
        {
            cat_mimicCommand(ships->selection->ShipPtr[i], 1, value, playerIndex, 0);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfUnmimic                                       0x00498080
    Description : CAT: KAS "Unmimic": every ship drops its disguise (animated)
----------------------------------------------------------------------------*/
void kasfUnmimic(GrowSelection *ships)
{
    sdword i;

    if ((ships == NULL) || (ships->selection == NULL))
    {
        return;
    }
    for (i = 0; i < ships->selection->numShips; i++)
    {
        cat_mimicUnmimic(ships->selection->ShipPtr[i], TRUE);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfEnableShipConstruction                        0x004980c0
    Description : CAT: KAS "EnableShipConstruction": the ship type of the race may (not) be built (rmEnableShip); an
                  open construction screen updates its list
----------------------------------------------------------------------------*/
void kasfEnableShipConstruction(char *shipType, char *shipRace, sdword bEnabled)
{
    ShipType type = StrToShipType(shipType);
    ShipRace race = StrToShipRace(shipRace);

    rmEnableShip(race, type, bEnabled);
    if (cmActive)
    {
        cmUpdateShipsAvailable();
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsAttackCapable                            0x00498100
    Description : CAT: KAS "ShipsAttackCapable": specialFlags2 bit 4 = bCapable's bit 0
----------------------------------------------------------------------------*/
void kasfShipsAttackCapable(GrowSelection *pShip, sdword bCapable)
{
    sdword i;

    for (i = 0; i < pShip->selection->numShips; i++)
    {
        Ship *ship = pShip->selection->ShipPtr[i];
        ship->shipOrDerelict.specialFlags2 = (ship->shipOrDerelict.specialFlags2 & ~SPECIAL_2_CatKasAttackCapable) |
                                             ((udword)(bCapable & 1) << 4);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfGetShipsClampedTo                             0x00498150
    Description : CAT: KAS "GetShipsClampedTo": every ship clamped to one of the hosts (their clamp lists); 0 for a
                  missing selection
----------------------------------------------------------------------------*/
sdword kasfGetShipsClampedTo(GrowSelection *pShipsAttached, GrowSelection *pHostShips)
{
    sdword i, j;
    Ship *host;
    CatClampedList *clamped;

    if ((pShipsAttached == NULL) || (pShipsAttached->selection == NULL) || (pHostShips == NULL) ||
        (pHostShips->selection == NULL))
    {
        return 0;
    }
    pShipsAttached->selection->numShips = 0;
    for (i = 0; i < pHostShips->selection->numShips; i++)
    {
        host = pHostShips->selection->ShipPtr[i];
        if ((host->guidance.clampInfo != NULL) && ((clamped = host->guidance.clampInfo->clampedObjs) != NULL))
        {
            for (j = 0; j < clamped->numShips; j++)
            {
                growSelectAddShip(pShipsAttached, clamped->ShipPtr[j]);
            }
        }
    }
    return pShipsAttached->selection->numShips;
}

/*-----------------------------------------------------------------------------
    Name        : kasfStartLevelByHyperspacingShips                 0x00498230
    Description : CAT: KAS "StartLevelByHyperspacingShips": the fleet of a newly loaded level starts arriving (the
                  single-player hyperspace state 9, HYPERSPACE_CATSHIPSHYPERSPACINGIN; M2-R8 kas-runtime-mission01
                  named it: the kasf library wrote HYPERSPACE_FAILED | HYPERSPACE_PREWAIT, the same value)
----------------------------------------------------------------------------*/
void kasfStartLevelByHyperspacingShips(void)
{
    singlePlayerGameInfo.hyperspaceState = HYPERSPACE_CATSHIPSHYPERSPACINGIN;
}

/*-----------------------------------------------------------------------------
    Name        : kasfLockoutMainRegion                             0x00498250
    Description : CAT: KAS "LockoutMainRegion": 1 locks everything out (spMainScreenAndLockout 7), anything else lifts
                  the lockout
----------------------------------------------------------------------------*/
void kasfLockoutMainRegion(sdword lock)
{
    if (lock == 1)
    {
        spMainScreenAndLockout(SPLOCKOUT_EVERYTHING);
        return;
    }
    spUnlockout();
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsScuttle                                  0x00498270
    Description : CAT: KAS "ShipsScuttle" (clScuttle)
----------------------------------------------------------------------------*/
void kasfShipsScuttle(GrowSelection *ships)
{
    if ((ships != NULL) && (ships->selection->numShips != 0))
    {
        clScuttle(&universe.mainCommandLayer, ships->selection);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfMakeShipCamera                                0x00498290
    Description : CAT: KAS "MakeShipCamera": the first ship becomes a camera prop: not bumpable, flagged, not
                  impactable and out of the impactable list, invincible, no tactics, no avoidance
----------------------------------------------------------------------------*/
void kasfMakeShipCamera(GrowSelection *ship)
{
    Ship *camera = ship->selection->ShipPtr[0];

    bitSet(camera->header.flags, SOF_NotBumpable);
    bitSet(camera->header.flags, SOF_CatDocking);           // bit 31 (the exe sets 0x80000000)
    bitClear(camera->header.flags, SOF_Impactable);
    listRemoveNode(&camera->impactable.impactablelink);
    bitSet(camera->header.catFlags18, CAT_SHIPFLAG_Invincible);
    bitSet(camera->header.catFlags18, CAT_SHIPFLAG_NoTactics);
    bitSet(camera->shipOrDerelict.specialFlags2, SPECIAL_2_CatNoAvoidance);
}

/*-----------------------------------------------------------------------------
    Name        : kasfSetShipPositionXYZ / kasfSetShipPosition      0x00498300 / 0x00498360
    Description : CAT: KAS "SetShipPositionXYZ" / "SetShipPosition": every ship is put at the point
----------------------------------------------------------------------------*/
void kasfSetShipPositionXYZ(GrowSelection *ship, sdword x, sdword y, sdword z)
{
    sdword i;

    for (i = 0; i < ship->selection->numShips; i++)
    {
        ship->selection->ShipPtr[i]->obj.posinfo.position.x = (real32)x;
        ship->selection->ShipPtr[i]->obj.posinfo.position.y = (real32)y;
        ship->selection->ShipPtr[i]->obj.posinfo.position.z = (real32)z;
    }
}

void kasfSetShipPosition(GrowSelection *ship, hvector *destination)
{
    sdword i;

    for (i = 0; i < ship->selection->numShips; i++)
    {
        ship->selection->ShipPtr[i]->obj.posinfo.position.x = destination->x;
        ship->selection->ShipPtr[i]->obj.posinfo.position.y = destination->y;
        ship->selection->ShipPtr[i]->obj.posinfo.position.z = destination->z;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfSetShipPositionNear                           0x004983b0
    Description : CAT: KAS "SetShipPositionNear": every ship at one random standoff point at the distance
----------------------------------------------------------------------------*/
void kasfSetShipPositionNear(GrowSelection *ship, hvector *detinstation, sdword distance)
{
    vector origin = {0.0f, 0.0f, 0.0f};
    vector dest, point;
    sdword i;

    vecGrabVecFromHVec(dest, *detinstation);
    point = aiuGenerateRandomStandoffPoint(dest, (real32)distance, origin, RSP_NORMAL);
    for (i = 0; i < ship->selection->numShips; i++)
    {
        ship->selection->ShipPtr[i]->obj.posinfo.position = point;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShowMinesInSensors / kasfChaseMode / kasfToggleChaseMode   0x00498490 / 0x004984a0 / 0x004984b0
    Description : CAT: the sensors manager's mine display; the camera mode word (its pilot bit is the chase mode);
                  the chase-mode key command (0x47)
----------------------------------------------------------------------------*/
void kasfShowMinesInSensors(sdword showMines)
{
    catSmShowMines = showMines;
}

sdword kasfChaseMode(void)
{
    return (sdword)universe.mainCameraCommand.ccMode;
}

void kasfToggleChaseMode(void)
{
    cat_kbCommandExecute(0, 0x47);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSetInvincible / kasfShipsSetDerelict     0x004984c0 / 0x00498510
    Description : CAT: KAS "ShipsSetInvincible" (catFlags18 bit 2) and "ShipsSetDerelict" (bit 4)
----------------------------------------------------------------------------*/
void kasfShipsSetInvincible(GrowSelection *ships, sdword invincible)
{
    sdword i;

    for (i = 0; i < ships->selection->numShips; i++)
    {
        Ship *ship = ships->selection->ShipPtr[i];
        ship->header.catFlags18 = (ship->header.catFlags18 & ~CAT_SHIPFLAG_Invincible) | ((udword)(invincible & 1) << 2);
    }
}

void kasfShipsSetDerelict(GrowSelection *ships, sdword bDerelict)
{
    sdword i;

    for (i = 0; i < ships->selection->numShips; i++)
    {
        Ship *ship = ships->selection->ShipPtr[i];
        ship->header.catFlags18 = (ship->header.catFlags18 & ~CAT_SHIPFLAG_KasDerelict) | ((udword)(bDerelict & 1) << 4);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfSetMotionCues                                 0x00498550
    Description : CAT: KAS "SetMotionCues": with the motion cues on, a non-zero cue restarts them (mode 1 and a reset)
----------------------------------------------------------------------------*/
void kasfSetMotionCues(sdword cue)
{
    if (catMotionCues && (cue != 0))
    {
        cat_mcSetMode(&catMotionCueState, 1);
        cat_mcResetRequest(&catMotionCueState);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfHaltAllPlayerShips                            0x00498590
    Description : CAT: KAS "HaltAllPlayerShips": every ship that may be halted (cat_shipCanBeHalted) halts alone
----------------------------------------------------------------------------*/
void kasfHaltAllPlayerShips(void)
{
    Node *node;
    Ship *ship;
    SelectCommand one;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (cat_shipCanBeHalted(ship, TRUE))
        {
            one.numShips = 1;
            one.ShipPtr[0] = ship;
            clWrapHalt(&universe.mainCommandLayer, &one);
        }
    }
}

// 0x00498610: the ship's race (by its type) is the parameter
static bool32 kasfPredRace(Ship *ship, memsize race, memsize unused)
{
    (void)unused;
    return (cat_ShipTypeToRace(ship->shiptype) == (sdword)race);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectRace                               0x004985f0
    Description : CAT: KAS "ShipsSelectRace": the ships of the race (the table calls the argument shipType)
----------------------------------------------------------------------------*/
sdword kasfShipsSelectRace(GrowSelection *newShips, GrowSelection *originalShips, sdword shipType)
{
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredRace, (memsize)shipType, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsUnderAttackElsewhere                     0x00498630
    Description : CAT: KAS "ShipsUnderAttackElsewhere": as kasfUnderAttackElsewhere for a selection (attackers is
                  cleared through the KAS helper)
----------------------------------------------------------------------------*/
sdword kasfShipsUnderAttackElsewhere(GrowSelection *ships, GrowSelection *attackers)
{
    sdword i;
    SelectCommand *these = ships->selection;
    Node *curnode = universe.mainCommandLayer.todolist.head;
    CommandToDo *command;

    if (attackers == NULL)
        return 0;

    kasGrowSelectionClear(attackers);
    if (these->numShips == 0)
        return 0;

    for (; curnode != NULL; curnode = curnode->next)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);
        if (((command->ordertype.order == COMMAND_ATTACK) ||
             (command->ordertype.attributes & (COMMAND_MASK_PASSIVE_ATTACKING|COMMAND_MASK_ATTACKING_AND_MOVING))) &&
            (command->attack != NULL) && AnyOfTheseShipsAreInSelection(these, (SelectCommand *)command->attack))
        {
            for (i = 0; i < command->selection->numShips; i++)
            {
                growSelectAddShipNoDuplication(attackers, command->selection->ShipPtr[i]);
            }
        }
    }
    return attackers->selection->numShips;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsBeastify                                 0x004986f0
    Description : CAT: KAS "ShipsBeastify": every ship the Beast can infect is infected by originator's first ship;
                  the others are scuttled
----------------------------------------------------------------------------*/
void kasfShipsBeastify(GrowSelection *ships, GrowSelection *originator)
{
    GrowSelection scuttle;
    sdword i;

    growSelectInit(&scuttle);
    for (i = 0; i < ships->selection->numShips; i++)
    {
        if (cat_beastGetAttackStaticInfo(ships->selection->ShipPtr[i]) == NULL)
        {
            growSelectAddShip(&scuttle, ships->selection->ShipPtr[i]);
        }
        else
        {
            cat_beastInfectShip(ships->selection->ShipPtr[i], originator->selection->ShipPtr[0]);
        }
    }
    clScuttle(&universe.mainCommandLayer, scuttle.selection);
    growSelectClose(&scuttle);
}

/*-----------------------------------------------------------------------------
    Name        : kasfGetPlayerMothership                           0x00498790
    Description : CAT: KAS "GetPlayerMothership": player 0's mothership (aiuFindEnemyMothership of player 1)
----------------------------------------------------------------------------*/
void kasfGetPlayerMothership(GrowSelection *ships)
{
    while (ships->selection->numShips)
    {
        clRemoveShipFromSelection(ships->selection, ships->selection->ShipPtr[0]);
    }
    if (aiuFindEnemyMothership(&universe.players[1]) != NULL)
    {
        growSelectAddShip(ships, aiuFindEnemyMothership(&universe.players[1]));
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfFindEnemyShipsInsideWithoutStealthy / kasfFindEnemiesNearbyWithoutStealthy /
                  kasfFindEnemiesNearTeamWithoutStealthy            0x004987e0 / 0x00498870 / 0x00498970
    Description : CAT: as kasfFindEnemyShipsInside / kasfFindEnemiesNearby / kasfFindEnemiesNearTeam, but only player 0's
                  ships that player 1 has detected (cat_shipDetectedByPlayer)
----------------------------------------------------------------------------*/
sdword kasfFindEnemyShipsInsideWithoutStealthy(Volume *volume, GrowSelection *ships)
{
    Node *node;
    Ship *ship;

    if ((ships == NULL) || (ships->selection == NULL) || (volume == NULL))
        return 0;

    ships->selection->numShips = 0;
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship != NULL) && !bitTest(ship->header.flags, SOF_Dead|SOF_Hide) && (ship->playerowner->playerIndex == 0) &&
            !bitTest(ship->header.flags, SOF_Crazy|SOF_Disabled|SOF_Hyperspace) && cat_shipDetectedByPlayer(ship, 1) &&
            volPointInside(volume, &ship->obj.posinfo.position))
        {
            growSelectAddShip(ships, ship);
        }
    }
    return ships->selection->numShips;
}

sdword kasfFindEnemiesNearbyWithoutStealthy(GrowSelection *ships, sdword radius)
{
    Ship *medianShip, *ship;
    real32 rangeSquared = (real32)radius*(real32)radius;
    Node *node;
    sdword i;

    if (ships == NULL || ships->selection == NULL || !(ships->selection->numShips))
        return 0;

    i = 0;
    do
    {
        medianShip = ships->selection->ShipPtr[i];
        ++i;
    } while (medianShip->playerowner->playerIndex && i < ships->selection->numShips); // not enemy yet

    ships->selection->numShips = 0;
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (!bitTest(ship->header.flags, SOF_Dead|SOF_Hide) && (ship->playerowner->playerIndex == 0) &&
            !bitTest(ship->header.flags, SOF_Crazy|SOF_Disabled|SOF_Hyperspace) && cat_shipDetectedByPlayer(ship, 1) &&
            ((medianShip == ship) ||
             (aiuFindDistanceSquared(medianShip->obj.posinfo.position, ship->obj.posinfo.position) <= rangeSquared)))
        {
            growSelectAddShip(ships, ship);
        }
    }
    return ships->selection->numShips;
}

sdword kasfFindEnemiesNearTeamWithoutStealthy(GrowSelection *ships, sdword radius)  // KAS FindEnemiesNearTeamWithoutStealthy, cataclysm.exe 0x00498970
{
    Ship *medianShip, *ship;
    real32 rangeSquared = (real32)radius*(real32)radius;
    Node *node;

    if (ships == NULL || ships->selection == NULL)
        return 0;

    ships->selection->numShips = 0;
    if (!CurrentTeamP->shipList.selection->numShips)
        return 0;

    medianShip = CurrentTeamP->shipList.selection->ShipPtr[0];
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (!bitTest(ship->header.flags, SOF_Dead|SOF_Hide) && (ship->playerowner->playerIndex == 0) &&
            !bitTest(ship->header.flags, SOF_Crazy|SOF_Disabled|SOF_Hyperspace) && cat_shipDetectedByPlayer(ship, 1) &&
            ((medianShip == ship) ||
             (aiuFindDistanceSquared(medianShip->obj.posinfo.position, ship->obj.posinfo.position) <= rangeSquared)))
        {
            growSelectAddShip(ships, ship);
        }
    }
    return ships->selection->numShips;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSetBeastInfectionUnlimited               0x00498a60
    Description : CAT: KAS "ShipsSetBeastInfectionUnlimited": specialFlags2 bit 15 = enable's bit 0; every Beast weapon
                  of the ship gets 0.01 in its gun word +0x70 (lasttimefired in this tree's Gun)
----------------------------------------------------------------------------*/
void kasfShipsSetBeastInfectionUnlimited(GrowSelection *ships, sdword enable)
{
    sdword i, g;
    Ship *ship;

    for (i = 0; i < ships->selection->numShips; i++)
    {
        ship = ships->selection->ShipPtr[i];
        ship->shipOrDerelict.specialFlags2 = (ship->shipOrDerelict.specialFlags2 & ~SPECIAL_2_CatBeastInfectionUnlimited) |
                                             ((udword)(enable & 1) << 15);
        if (ship->gunInfo != NULL)
        {
            for (g = 0; g < ship->gunInfo->numGuns; g++)
            {
                if (ship->gunInfo->guns[g].gunstatic->gunsoundtype == GS_BeastWeapon)
                {
                    ship->gunInfo->guns[g].lasttimefired = 0.01f;
                }
            }
        }
    }
}

// 0x00498b00 / 0x00498b50: the beastified / salvaged mark (specialFlags2 0x10000 / 0x20000), cleared once read
static bool32 kasfPredBeastified(Ship *ship, memsize unused1, memsize unused2)
{
    (void)unused1; (void)unused2;
    if (bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeastified))
    {
        bitClear(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeastified);
        return TRUE;
    }
    return FALSE;
}

static bool32 kasfPredSalvaged(Ship *ship, memsize unused1, memsize unused2)
{
    (void)unused1; (void)unused2;
    if (bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvaged))
    {
        bitClear(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatSalvaged);
        return TRUE;
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSelectBeastified / kasfShipsSelectSalvaged   0x00498ae0 / 0x00498b30
    Description : CAT: KAS "ShipsSelectBeastified" / "ShipsSelectSalvaged": the ships marked since the last call
----------------------------------------------------------------------------*/
sdword kasfShipsSelectBeastified(GrowSelection *newShips, GrowSelection *originalShips)
{
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredBeastified, 0, 0);
}

sdword kasfShipsSelectSalvaged(GrowSelection *newShips, GrowSelection *originalShips)
{
    return cat_kasShipsSelectByPredicate(newShips, originalShips, kasfPredSalvaged, 0, 0);
}

/*-----------------------------------------------------------------------------
    Name        : kasfPingMultipleAddShips                          0x00498b80
    Description : CAT: KAS "PingMultipleAddShips": one ping per ship, labelled "<label><index>"
----------------------------------------------------------------------------*/
void kasfPingMultipleAddShips(GrowSelection *ships, char *label)
{
    char name[256];
    SelectCommand one;
    sdword i;

    one.numShips = 1;
    if ((ships == NULL) || (ships->selection == NULL) || (ships->selection->ShipPtr[0] == NULL))
    {
        return;
    }
    for (i = 0; i < ships->selection->numShips; i++)
    {
        snprintf(name, sizeof(name), "%s%d", label, i);     // HWI: bounded (the exe's sprintf)
        one.ShipPtr[0] = ships->selection->ShipPtr[i];
        pingAnomalySelectionPingAdd(name, &one);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfGiveTechToAI                                  0x00498c10
    Description : CAT: KAS "GiveTechToAI": an empty name clears player 1's technology set; a known technology is
                  added and his upgrades follow
----------------------------------------------------------------------------*/
void kasfGiveTechToAI(char *szTechName)
{
    TechnologyType tech;

    if (szTechName[0] == 0)
    {
        cat_rmTechSetClear((sdword *)&universe.players[1].researchinfo);
    }
    tech = StrToTechType(szTechName);
    if (tech != (TechnologyType)-1)
    {
        cat_rmTechSetAdd((sdword *)&universe.players[1].researchinfo, tech);
        cat_playerUpdateUpgradesFromResearch(&universe.players[1]);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsUpgrade                                  0x00498c70
    Description : CAT: KAS "UpgradeShips": every ship takes its player's upgrades (cat_shipApplyUpgrades)
----------------------------------------------------------------------------*/
void kasfShipsUpgrade(GrowSelection *ships)
{
    sdword i;

    for (i = 0; i < ships->selection->numShips; i++)
    {
        cat_shipApplyUpgrades(ships->selection->ShipPtr[i]);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfGetShipCountSalvaged / kasfGetShipCountBeastified   0x00498cb0 / 0x00498cd0
    Description : CAT: the universe's counts of the type salvaged / beastified for the player
----------------------------------------------------------------------------*/
sdword kasfGetShipCountSalvaged(char *shipType, sdword player)
{
    return cat_univGetShipCountSalvaged(StrToShipType(shipType), player);
}

sdword kasfGetShipCountBeastified(char *shipType, sdword player)
{
    return cat_univGetShipCountBeastified(StrToShipType(shipType), player);
}

/*-----------------------------------------------------------------------------
    Name        : kasfMakeShipsControllable                         0x00498cf0
    Description : CAT: KAS "MakeShipsControllable": an uncontrollable ship is SU exempt and disabled (reason 1)
----------------------------------------------------------------------------*/
void kasfMakeShipsControllable(GrowSelection *ships, sdword controllable)
{
    sdword i;

    for (i = 0; i < ships->selection->numShips; i++)
    {
        cat_shipSetSUExempt(ships->selection->ShipPtr[i], controllable == 0);
        cat_shipSetDisableReason(ships->selection->ShipPtr[i], controllable == 0, 1);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfGetNumberShipsPlayerKilled / kasfGetNumberShipsPlayerLost   0x00498d50 / 0x00498d70
    Description : CAT: the universe's kill / loss counts of the player for the class
----------------------------------------------------------------------------*/
sdword kasfGetNumberShipsPlayerKilled(sdword playerIndex, sdword shipClass)
{
    return cat_univGetNumberShipsPlayerKilled(playerIndex, shipClass);
}

sdword kasfGetNumberShipsPlayerLost(sdword playerIndex, sdword shipClass)
{
    return cat_univGetNumberShipsPlayerLost(playerIndex, shipClass);
}

/*-----------------------------------------------------------------------------
    Name        : kasfFirstShip                                     0x00498d90
    Description : CAT: KAS "FirstShip": pDst becomes pSrc's first ship (no test for an empty pSrc)
----------------------------------------------------------------------------*/
void kasfFirstShip(GrowSelection *pDst, GrowSelection *pSrc)
{
    Ship *ship = pSrc->selection->ShipPtr[0];

    while (pDst->selection->numShips)
    {
        clRemoveShipFromSelection(pDst->selection, pDst->selection->ShipPtr[0]);
    }
    growSelectAddShip(pDst, ship);
}

// the universe-size clamp of the KAS point functions (each coordinate to +-smUniverseSize*)
static void kasfVectorClampToUniverse(hvector *v)
{
    if (v->x > smUniverseSizeX) v->x = smUniverseSizeX; else if (v->x < -smUniverseSizeX) v->x = -smUniverseSizeX;
    if (v->y > smUniverseSizeY) v->y = smUniverseSizeY; else if (v->y < -smUniverseSizeY) v->y = -smUniverseSizeY;
    if (v->z > smUniverseSizeZ) v->z = smUniverseSizeZ; else if (v->z < -smUniverseSizeZ) v->z = -smUniverseSizeZ;
}

/*-----------------------------------------------------------------------------
    Name        : kasfVectorAdd / kasfVectorCopy                    0x00498dd0 / 0x00498e30
    Description : CAT: KAS "PointAdd" / "PointCopy": an existing labelled point is overwritten, and a new labelled
                  point is added in any case (the exe adds one each call)
----------------------------------------------------------------------------*/
void kasfVectorAdd(udword label, sdword x, sdword y, sdword z)
{
    hvector *point = kasVectorPtr(label);

    if (point != NULL)
    {
        point->w = 1.0f;
        point->x = (real32)x;
        point->y = (real32)y;
        point->z = (real32)z;
    }
    kasLabelledVectorAdd(label, (real32)x, (real32)y, (real32)z, 1.0f);
}

void kasfVectorCopy(udword label, hvector *vec)
{
    hvector *point = kasVectorPtr(label);

    if (point != NULL)
    {
        *point = *vec;
    }
    kasLabelledVectorAdd(label, vec->x, vec->y, vec->z, vec->w);
}

/*-----------------------------------------------------------------------------
    Name        : kasfVectorCrossProduct                            0x00498e80
    Description : CAT: KAS "PointCrossProduct": Result = normalised One x Two (w 1)
----------------------------------------------------------------------------*/
void kasfVectorCrossProduct(hvector *Result, hvector *One, hvector *Two)
{
    vector cross;

    cross.x = Two->z * One->y - Two->y * One->z;
    cross.y = Two->x * One->z - Two->z * One->x;
    cross.z = Two->y * One->x - Two->x * One->y;
    vecNormalize(&cross);
    Result->x = cross.x;
    Result->y = cross.y;
    Result->z = cross.z;
    Result->w = 1.0f;
}

/*-----------------------------------------------------------------------------
    Name        : kasfVectorDotProduct                              0x00498ef0
    Description : CAT: KAS "PointDotProduct": floor(dot * 100 + 0.5) on the x87 stack (z, then y, then x summed)
----------------------------------------------------------------------------*/
sdword kasfVectorDotProduct(hvector *One, hvector *Two)
{
    real64 dot = ((real64)One->z * (real64)Two->z + (real64)One->y * (real64)Two->y) + (real64)One->x * (real64)Two->x;

    return (sdword)floor(dot * (real64)100.0f + 0.5);
}

/*-----------------------------------------------------------------------------
    Name        : kasfVectorParametric                              0x00498f30
    Description : CAT: KAS "PointParametric": result = start * (maxt - t) / maxt + end * t / maxt, kept inside the
                  universe
----------------------------------------------------------------------------*/
void kasfVectorParametric(hvector *result, hvector *start, hvector *end, sdword t, sdword maxt)
{
    real32 f = (real32)(maxt - t) / (real32)maxt;

    result->x = f * start->x;
    result->y = f * start->y;
    result->z = f * start->z;
    f = (real32)t / (real32)maxt;
    result->x = f * end->x + result->x;
    result->y = f * end->y + result->y;
    result->z = f * end->z + result->z;
    kasfVectorClampToUniverse(result);
}

/*-----------------------------------------------------------------------------
    Name        : kasfVectorAddTo / kasfVectorSubFrom / kasfVectorScalarMult   0x00499020 / 0x004990e0 / 0x004991a0
    Description : CAT: KAS "PointAddTo" / "PointSubFrom" (kept inside the universe) / "PointScalarMult"
----------------------------------------------------------------------------*/
void kasfVectorAddTo(hvector *result, hvector *add)
{
    result->x = result->x + add->x;
    result->y = add->y + result->y;
    result->z = add->z + result->z;
    kasfVectorClampToUniverse(result);
}

void kasfVectorSubFrom(hvector *result, hvector *sub)
{
    result->x = result->x - sub->x;
    result->y = result->y - sub->y;
    result->z = result->z - sub->z;
    kasfVectorClampToUniverse(result);
}

void kasfVectorScalarMult(hvector *result, sdword numerator, sdword denominator)
{
    real32 f = (real32)numerator / (real32)denominator;

    result->x = f * result->x;
    result->y = f * result->y;
    result->z = f * result->z;
}

/*-----------------------------------------------------------------------------
    Name        : kasfVectorNear                                    0x004991d0
    Description : CAT: KAS "PointNear": a random standoff point at the distance from p2 (w 1), kept inside the universe
----------------------------------------------------------------------------*/
void kasfVectorNear(hvector *result, hvector *p2, sdword distance)
{
    vector origin = {0.0f, 0.0f, 0.0f};
    vector center, point;

    vecGrabVecFromHVec(center, *p2);
    point = aiuGenerateRandomStandoffPoint(center, (real32)distance, origin, RSP_NORMAL);
    result->x = point.x;
    result->z = point.z;
    result->y = point.y;
    result->w = 1.0f;
    kasfVectorClampToUniverse(result);
}

/*-----------------------------------------------------------------------------
    Name        : kasfSalvage                                       0x00499330
    Description : CAT: KAS "Salvage": the current team's salvage-capable ships salvage the ships (clWrapSpecial);
                  a Worker is put in salvage mode, a P1 standard corvette marks its salvage target
----------------------------------------------------------------------------*/
void kasfSalvage(GrowSelection *ships)
{
    MaxSelection salvagers;
    MaxAnySelection targets;
    sdword i;
    Ship *ship;

    memset(&salvagers, 0, sizeof(salvagers));
    memset(&targets, 0, sizeof(targets));
    if ((CurrentTeamP == NULL) || (CurrentTeamP->shipList.selection->numShips <= 0))
    {
        return;
    }
    for (i = 0; i < CurrentTeamP->shipList.selection->numShips; i++)
    {
        ship = CurrentTeamP->shipList.selection->ShipPtr[i];
        if (cat_shipCanSalvage(ship))
        {
            salvagers.ShipPtr[salvagers.numShips++] = ship;
        }
    }
    for (i = 0; i < ships->selection->numShips; i++)
    {
        targets.TargetPtr[targets.numTargets++] = (SpaceObjRotImpTarg *)ships->selection->ShipPtr[i];
    }
    clWrapSpecial(&universe.mainCommandLayer, (SelectCommand *)&salvagers, (SpecialCommand *)&targets);
    for (i = 0; i < salvagers.numShips; i++)
    {
        ship = salvagers.ShipPtr[i];
        if ((ship->shiptype == sWorker) || (ship->shiptype == bWorker))
        {
            ((WorkerSpec *)ship->ShipSpecifics)->specialMode = WORKER_MODE_Salvage;
        }
        else if (ship->shiptype == p1StandardCorvette)
        {
            ((P1StandardCorvetteSpec *)ship->ShipSpecifics)->specialMode = 2;   // salvage target
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfRepair                                        0x00499460
    Description : CAT: KAS "Repair": the current team's live repair-capable Workers repair the ships
                  (cat_clSpecialTarget)
----------------------------------------------------------------------------*/
void kasfRepair(GrowSelection *ships)
{
    MaxSelection repairers;
    MaxAnySelection targets;
    sdword i;
    Ship *ship;

    memset(&repairers, 0, sizeof(repairers));
    memset(&targets, 0, sizeof(targets));
    if ((CurrentTeamP == NULL) || (CurrentTeamP->shipList.selection->numShips <= 0))
    {
        return;
    }
    for (i = 0; i < CurrentTeamP->shipList.selection->numShips; i++)
    {
        ship = CurrentTeamP->shipList.selection->ShipPtr[i];
        if (cat_shipCanRepair(ship) && cat_shipIsLiveWorker(ship))
        {
            repairers.ShipPtr[repairers.numShips++] = ship;
        }
    }
    for (i = 0; i < ships->selection->numShips; i++)
    {
        targets.TargetPtr[targets.numTargets++] = (SpaceObjRotImpTarg *)ships->selection->ShipPtr[i];
    }
    cat_clSpecialTarget(&universe.mainCommandLayer, (SelectCommand *)&repairers, (SpecialCommand *)&targets, FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : kasfHasShipBeenDiscovered                         0x00499570
    Description : CAT: KAS "HasShipBeenDiscovered": one of the ships is in player 0's explored fog of war and not
                  hidden
----------------------------------------------------------------------------*/
sdword kasfHasShipBeenDiscovered(GrowSelection *ship)
{
    sdword i;
    Ship *s;

    for (i = 0; i < ship->selection->numShips; i++)
    {
        s = ship->selection->ShipPtr[i];
        if (universe.players[0].catFOWGrid != NULL)
        {
            fowSelect(universe.players[0].catFOWGrid);
            if ((fowGetUsageCountW(s->obj.posinfo.position) > 0) && !bitTest(s->header.flags, SOF_Hide))
            {
                return 1;
            }
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : kasfFindSlipgate                                  0x004995e0
    Description : CAT: KAS "FindSlipgate": the ships inside the volume, then all but the untargetable ones (the
                  slipgates) dropped
----------------------------------------------------------------------------*/
void kasfFindSlipgate(Volume *volume, GrowSelection *slipgates)
{
    Node *node;
    Ship *ship;
    sdword i;

    if (slipgates != NULL)
    {
        kasGrowSelectionClear(slipgates);
        for (node = universe.ShipList.head; node != NULL; node = node->next)
        {
            ship = (Ship *)listGetStructOfNode(node);
            if (!bitTest(ship->header.flags, SOF_Dead|SOF_Hide) && volPointInside(volume, &ship->obj.posinfo.position))
            {
                growSelectAddShip(slipgates, ship);
            }
        }
    }
    for (i = 0; i < slipgates->selection->numShips; i++)
    {
        if (!cat_shipIsUntargetable(slipgates->selection->ShipPtr[i]))
        {
            growSelectRemoveShipIndex(slipgates, i);
            i--;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsDockInstant                              0x00499670
    Description : CAT: KAS "ShipsDockInstant": the ships are put inside hostSel's first ship at once; a hive frigate
                  resets its swarm
----------------------------------------------------------------------------*/
void kasfShipsDockInstant(GrowSelection *shipSel, GrowSelection *hostSel)
{
    Ship *dockwith = hostSel->selection->ShipPtr[0];
    sdword i;

    for (i = 0; i < shipSel->selection->numShips; i++)
    {
        univRemoveShipFromOutside(shipSel->selection->ShipPtr[i], TRUE);
        dockPutShipInside(shipSel->selection->ShipPtr[i], dockwith);
    }
    if ((dockwith->shiptype == sHiveFrigate) || (dockwith->shiptype == bHiveFrigate))
    {
        cat_hiveFrigateResetSwarm(dockwith);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfSlipgateDockInstant / kasfSlipgateDock        0x004996e0 / 0x00499760
    Description : CAT: the current team's ships enter the slipgate at once / dock with it (dock purpose 10)
----------------------------------------------------------------------------*/
void kasfSlipgateDockInstant(GrowSelection *slipgate)
{
    Ship *dockwith;
    sdword i;

    if ((CurrentTeamP == NULL) || (CurrentTeamP->shipList.selection->numShips == 0))
    {
        return;
    }
    dockwith = slipgate->selection->ShipPtr[0];
    for (i = 0; i < CurrentTeamP->shipList.selection->numShips; i++)
    {
        univRemoveShipFromOutside(CurrentTeamP->shipList.selection->ShipPtr[i], TRUE);
        dockPutShipInside(CurrentTeamP->shipList.selection->ShipPtr[i], dockwith);
    }
    if ((dockwith->shiptype == sHiveFrigate) || (dockwith->shiptype == bHiveFrigate))
    {
        cat_hiveFrigateResetSwarm(dockwith);
    }
}

void kasfSlipgateDock(GrowSelection *slipgate)
{
    if ((CurrentTeamP != NULL) && (CurrentTeamP->shipList.selection->numShips != 0))
    {
        aiuWrapDock(CurrentTeamP->shipList.selection, CAT_DOCK_SLIPGATE, slipgate->selection->ShipPtr[0]);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfStopInstant                                   0x00499790
    Description : CAT: KAS "StopInstant": the current team halts and its ships lose all velocity, force and spin
----------------------------------------------------------------------------*/
void kasfStopInstant(void)
{
    sdword i;
    Ship *ship;

    aitDeleteAllTeamMoves(CurrentTeamP);
    clWrapHalt(&universe.mainCommandLayer, CurrentTeamP->shipList.selection);
    for (i = 0; i < CurrentTeamP->shipList.selection->numShips; i++)
    {
        ship = CurrentTeamP->shipList.selection->ShipPtr[i];
        vecZeroVector(ship->obj.posinfo.velocity);
        vecZeroVector(ship->obj.posinfo.force);
        vecZeroVector(ship->rotatable.rotinfo.rotspeed);
        vecZeroVector(ship->rotatable.rotinfo.torque);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfEnteringSlipgate                              0x00499820
    Description : CAT: KAS "EnteringSlipgate": a ship of the current team has a dock order of purpose 10 (slipgate)
----------------------------------------------------------------------------*/
sdword kasfEnteringSlipgate(void)
{
    sdword i;
    CommandToDo *command;

    for (i = 0; i < CurrentTeamP->shipList.selection->numShips; i++)
    {
        command = CurrentTeamP->shipList.selection->ShipPtr[i]->command;
        if ((command != NULL) && (command->ordertype.order == COMMAND_DOCK) && (command->dock.dockType == CAT_DOCK_SLIPGATE))
        {
            return 1;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : kasfIsIdle / kasfIsTeamIdle / kasfThisTeamIdle    0x00499870 / 0x004998b0 / 0x004998c0
    Description : CAT: KAS "IsIdle": one of the ships has no order (or the null order); "IsTeamIdle" /
                  "ThisTeamIdle": aitTeamIsIdle
----------------------------------------------------------------------------*/
sdword kasfIsIdle(GrowSelection *ships)
{
    sdword i;
    CommandToDo *command;

    for (i = 0; i < ships->selection->numShips; i++)
    {
        command = ships->selection->ShipPtr[i]->command;
        if ((command == NULL) || (command->ordertype.order == COMMAND_NULL))
        {
            return 1;
        }
    }
    return 0;
}

sdword kasfIsTeamIdle(struct AITeam *team)
{
    return aitTeamIsIdle(team);
}

sdword kasfThisTeamIdle(void)
{
    return aitTeamIsIdle(CurrentTeamP);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsInVolume                                 0x004998d0
    Description : CAT: KAS "ShipsInVolume": TRUE unless, with more than 5 ships to find or more than 10 ships in the
                  volume, one of shipsToFind is not inside it (the exe does not check fewer ships)
----------------------------------------------------------------------------*/
sdword kasfShipsInVolume(Volume *volume, GrowSelection *shipsToFind)
{
    GrowSelection inside;
    Node *node;
    Ship *ship;
    sdword result = 1, i, j;

    inside.maxNumShips = 0;
    inside.selection = NULL;
    growSelectInit(&inside);
    kasGrowSelectionClear(&inside);
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (!bitTest(ship->header.flags, SOF_Dead|SOF_Hide) && volPointInside(volume, &ship->obj.posinfo.position))
        {
            growSelectAddShip(&inside, ship);
        }
    }
    if ((inside.selection != NULL) &&
        ((shipsToFind->selection->numShips > 5) || (inside.selection->numShips > 10)))
    {
        for (i = 0; (i < shipsToFind->selection->numShips) && result; i++)
        {
            for (j = 0; j < inside.selection->numShips; j++)
            {
                if (shipsToFind->selection->ShipPtr[i] == inside.selection->ShipPtr[j])
                {
                    break;
                }
            }
            if (j >= inside.selection->numShips)
            {
                result = 0;
            }
        }
    }
    growSelectClose(&inside);
    return result;
}

/*-----------------------------------------------------------------------------
    Name        : kasfInfectionMissileAttack                        0x004999c0
    Description : CAT: KAS "InfectionMissileAttack": the current team's cruise missiles (bCruiseMissile, type 0x3A) are shared out
                  among the targets: each target gets as many missiles as it still needs infection hits for
                  (staticinfo beastAttacksRequired - its hits so far; -1 = cannot be infected, skipped), handed out in
                  team order; targets that got missiles leave the list; any missiles left (and the team's other ships)
                  attack the remaining targets together
----------------------------------------------------------------------------*/
void kasfInfectionMissileAttack(GrowSelection *targetSel)
{
    MaxSelection targets, attackers;
    SelectCommand oneTarget;
    sdword i, j, used = 0, needed;
    Ship *target, *ship;

    targets.numShips = 0;
    memset(targets.ShipPtr, 0, sizeof(targets.ShipPtr));
    for (i = 0; i < targetSel->selection->numShips; i++)
    {
        targets.ShipPtr[targets.numShips++] = targetSel->selection->ShipPtr[i];
    }
    aitDeleteAllTeamMoves(CurrentTeamP);
    i = 0;
    while (i < targets.numShips)
    {
        attackers.numShips = 0;
        memset(attackers.ShipPtr, 0, sizeof(attackers.ShipPtr));
        target = targets.ShipPtr[i];
        oneTarget.numShips = 1;
        oneTarget.timeLastStatus = 0.0f;
        oneTarget.ShipPtr[0] = target;
        if (target->staticinfo->beastAttacksRequired == -1)
        {
            i++;
            continue;
        }
        needed = target->staticinfo->beastAttacksRequired - target->catBeastHits;
        for (j = 0; j < needed; j++)
        {
            if (used < CurrentTeamP->shipList.selection->numShips)
            {
                ship = CurrentTeamP->shipList.selection->ShipPtr[used];
                if (ship->shiptype == bCruiseMissile)
                {
                    attackers.ShipPtr[attackers.numShips++] = ship;
                }
                used++;
            }
        }
        if (attackers.numShips < 1)
        {
            i++;
            continue;
        }
        aiuWrapAttack((SelectCommand *)&attackers, &oneTarget);
        targets.numShips--;
        targets.ShipPtr[i] = targets.ShipPtr[targets.numShips];
    }
    if (targets.numShips > 0)
    {
        attackers.numShips = 0;
        memset(attackers.ShipPtr, 0, sizeof(attackers.ShipPtr));
        for (i = 0; i < CurrentTeamP->shipList.selection->numShips; i++)
        {
            ship = CurrentTeamP->shipList.selection->ShipPtr[i];
            if ((ship->shiptype != bCruiseMissile) || (used <= i))
            {
                attackers.ShipPtr[attackers.numShips++] = ship;
            }
        }
        aiuWrapAttack((SelectCommand *)&attackers, (SelectCommand *)&targets);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfGetBuildTime / kasfGetBuildCost               0x00499b80 / 0x00499bb0
    Description : CAT: KAS "GetBuildTime" (the static's build time * 0.0625f, truncated) and "GetBuildCost"
----------------------------------------------------------------------------*/
sdword kasfGetBuildTime(char *shipTypeName)
{
    ShipType type = StrToShipType(shipTypeName);

    return (sdword)((real64)GetShipStaticInfo(type, (ShipRace)cat_ShipTypeToRace(type))->buildTime * (real64)0.0625f);
}

sdword kasfGetBuildCost(char *shipTypeName)
{
    ShipType type = StrToShipType(shipTypeName);

    return GetShipStaticInfo(type, (ShipRace)cat_ShipTypeToRace(type))->buildCost;
}

// the ships kasfShipsCreate / Launch / Hyperspace make: count ships of the type and its race at the origin for the
//      owner (univCreateShip), into createdShips (cleared first); an unknown type makes none
static void kasfShipsCreateSome(GrowSelection *createdShips, char *shiptype, sdword count, sdword owner)
{
    vector origin = {0.0f, 0.0f, 0.0f};
    ShipType type = StrToShipType(shiptype);
    ShipRace race = (ShipRace)cat_ShipTypeToRace(type);
    Ship *ship;

    kasGrowSelectionClear(createdShips);
    if ((type == (ShipType)-1) || (count <= 0))
    {
        return;
    }
    while (count != 0)
    {
        ship = univCreateShip(type, race, &origin, &universe.players[owner], 0, NULL);
        if (ship != NULL)
        {
            growSelectAddShip(createdShips, ship);
        }
        count--;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsCreate                                   0x00499bd0
    Description : CAT: KAS "ShipsCreate": count new ships of the type for the owner, at the origin
----------------------------------------------------------------------------*/
void kasfShipsCreate(GrowSelection *createdShips, char *shiptype, sdword count, sdword owner)
{
    kasfShipsCreateSome(createdShips, shiptype, count, owner);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsCreateLaunch                             0x00499c70
    Description : CAT: KAS "ShipsCreateLaunch": new ships made inside host's first ship and launched from it; ships
                  of player 0 made in an SU-exempt host are SU exempt and disabled too
----------------------------------------------------------------------------*/
void kasfShipsCreateLaunch(GrowSelection *createdShips, char *shiptype, sdword count, GrowSelection *host, sdword owner)
{
    Ship *dockwith;
    bool32 exempt;
    sdword i;

    if (host->selection->numShips <= 0)
    {
        return;
    }
    kasfShipsCreateSome(createdShips, shiptype, count, owner);
    if (owner == 0)
    {
        exempt = bitTest(host->selection->ShipPtr[0]->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20) ? TRUE : FALSE;
        for (i = 0; i < createdShips->selection->numShips; i++)
        {
            cat_shipSetSUExempt(createdShips->selection->ShipPtr[i], exempt);
            cat_shipSetDisableReason(createdShips->selection->ShipPtr[i], exempt, 1);
        }
    }
    dockwith = host->selection->ShipPtr[0];
    for (i = 0; i < createdShips->selection->numShips; i++)
    {
        univRemoveShipFromOutside(createdShips->selection->ShipPtr[i], TRUE);
        dockPutShipInside(createdShips->selection->ShipPtr[i], dockwith);
    }
    if ((dockwith->shiptype == sHiveFrigate) || (dockwith->shiptype == bHiveFrigate))
    {
        cat_hiveFrigateResetSwarm(dockwith);
    }
    dockwith = host->selection->ShipPtr[0];
    for (i = 0; i < createdShips->selection->numShips; i++)
    {
        bitSet(createdShips->selection->ShipPtr[i]->shipOrDerelict.specialFlags, SPECIAL_LaunchedFromKas);
        bitSet(createdShips->selection->ShipPtr[i]->shipOrDerelict.specialFlags, SPECIAL_KasCheckDoneLaunching);
    }
    aiuWrapDock(createdShips->selection, CAT_DOCK_LAUNCH, dockwith);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsCreateHyperspace                         0x00499e40
    Description : CAT: KAS "ShipsCreateHyperspace": new ships that start in hyperspace (attribute 0x80 while they go
                  out and come in at dest)
----------------------------------------------------------------------------*/
void kasfShipsCreateHyperspace(GrowSelection *createdShips, char *shiptype, sdword count, hvector *dest, sdword owner)
{
    MaxSelection filtered;
    sdword i;

    kasfShipsCreateSome(createdShips, shiptype, count, owner);
    for (i = 0; i < createdShips->selection->numShips; i++)
    {
        bitSet(createdShips->selection->ShipPtr[i]->obj.attributes, ATTRIBUTES_StartInHS);
    }
    if (createdShips != NULL)
    {
        if (createdShips->selection->numShips != 0)
        {
            filtered.numShips = 0;
            aiuFilterDisabledShips(createdShips->selection, &filtered);
            RemoveShipsFromDoingStuff(&universe.mainCommandLayer, (SelectCommand *)&filtered, 0x17);
            spHyperspaceSelectionOut((SelectCommand *)&filtered);
            spHyperspaceDelay = 0.0f;
        }
        if (createdShips->selection->numShips != 0)
        {
            spHyperspaceSelectionIn(createdShips->selection, dest);
            spHyperspaceDelay = 0.0f;
        }
    }
    kasfShipsAttributesBitClear(createdShips, ATTRIBUTES_StartInHS);
}

/*-----------------------------------------------------------------------------
    Name        : kasfPlayerNearShips                               0x00499f90
    Description : CAT: KAS "PlayerNearShips": a ship of player 0 is nearer than distance to one of the ships (the
                  squared distances on the x87 stack: z, then y, then x)
----------------------------------------------------------------------------*/
sdword kasfPlayerNearShips(GrowSelection *ships, sdword distance)
{
    real32 rangeSquared = (real32)((real64)distance * (real64)distance);
    Node *node;
    Ship *player, *ship;
    real64 dx, dy, dz;
    sdword i;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        player = (Ship *)listGetStructOfNode(node);
        if (player->playerowner->playerIndex != 0)
        {
            continue;
        }
        for (i = 0; i < ships->selection->numShips; i++)
        {
            ship = ships->selection->ShipPtr[i];
            dx = (real64)player->obj.posinfo.position.x - (real64)ship->obj.posinfo.position.x;
            dy = (real64)player->obj.posinfo.position.y - (real64)ship->obj.posinfo.position.y;
            dz = (real64)player->obj.posinfo.position.z - (real64)ship->obj.posinfo.position.z;
            if (((dz * dz + dy * dy) + dx * dx) < (real64)rangeSquared)
            {
                return 1;
            }
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSetBulimia / kasfShipsEffectedByMines    0x0049a030 / 0x0049a080
    Description : CAT: specialFlags2 bit 21 (bulimia) and catFlags18 bit 5 (mines)
----------------------------------------------------------------------------*/
void kasfShipsSetBulimia(GrowSelection *ships, sdword bulimia)
{
    sdword i;

    for (i = 0; i < ships->selection->numShips; i++)
    {
        Ship *ship = ships->selection->ShipPtr[i];
        ship->shipOrDerelict.specialFlags2 = (ship->shipOrDerelict.specialFlags2 & ~SPECIAL_2_CatKasBulimia) |
                                             ((udword)(bulimia & 1) << 21);
    }
}

void kasfShipsEffectedByMines(GrowSelection *ships, sdword effectedByMines)
{
    sdword i;

    for (i = 0; i < ships->selection->numShips; i++)
    {
        Ship *ship = ships->selection->ShipPtr[i];
        ship->header.catFlags18 = (ship->header.catFlags18 & ~CAT_SHIPFLAG_KasMines) | ((udword)(effectedByMines & 1) << 5);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipInfectionTerminal                         0x0049a0c0
    Description : CAT: KAS "ShipInfectionTerminal": the first ship's infection timer runs
----------------------------------------------------------------------------*/
sdword kasfShipInfectionTerminal(GrowSelection *ships)
{
    if ((ships != NULL) && (ships->selection != NULL) && (ships->selection->numShips > 0) &&
        (ships->selection->ShipPtr[0]->catBeastConvertTime != 0.0f))
    {
        return 1;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSort                                     0x0049a100
    Description : CAT: KAS "ShipsSort": the ships ordered by distance from origin (an exchange sort: each ship is
                  compared with every one before it and swapped when nearer)
----------------------------------------------------------------------------*/
void kasfShipsSort(GrowSelection *ships, hvector *origin)
{
    sdword i, j;
    Ship *a, *b;
    real64 ax, ay, az, bx, by;
    real32 bz;

    if (ships->selection->numShips == 0)
    {
        return;
    }
    for (i = 1; i < ships->selection->numShips; i++)
    {
        for (j = 0; j < i; j++)
        {
            a = ships->selection->ShipPtr[i];
            b = ships->selection->ShipPtr[j];
            // CAT [audit: M2-R8 kasf-library]: the exe's x87 shape at the 53-bit control word (0x0049a147 .. 0x0049a1bf):
            //      five differences stay on the stack (double), only bz is stored as a float (0x0049a197), and each
            //      sum is (z*z + y*y) + x*x
            ax = (real64)origin->x - (real64)a->obj.posinfo.position.x;
            ay = (real64)origin->y - (real64)a->obj.posinfo.position.y;
            az = (real64)origin->z - (real64)a->obj.posinfo.position.z;
            bx = (real64)origin->x - (real64)b->obj.posinfo.position.x;
            by = (real64)origin->y - (real64)b->obj.posinfo.position.y;
            bz = (real32)((real64)origin->z - (real64)b->obj.posinfo.position.z);
            if (((az * az + ay * ay) + ax * ax) < (((real64)bz * bz + by * by) + bx * bx))
            {
                ships->selection->ShipPtr[j] = a;
                ships->selection->ShipPtr[i] = b;
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfEnableNaggarok                                0x0049a220
    Description : CAT: KAS "EnableNaggarok": specialFlags2 bit 22 set when bEnable is 0
----------------------------------------------------------------------------*/
void kasfEnableNaggarok(GrowSelection *pNaggarok, sdword bEnable)
{
    Ship *ship = pNaggarok->selection->ShipPtr[0];

    ship->shipOrDerelict.specialFlags2 = ((udword)(bEnable == 0) << 22) | (ship->shipOrDerelict.specialFlags2 & ~SPECIAL_2_CatKasNaggarok);
}

/*-----------------------------------------------------------------------------
    Name        : kasfForceCataclysmDefaultMode                     0x0049a250
    Description : CAT: KAS "ForceCataclysmDefaultMode": the sensors manager's view options back to their defaults
                  (all on but the occupied areas and the compass)
----------------------------------------------------------------------------*/
void kasfForceCataclysmDefaultMode(void)
{
    catSmViewDecorativeAsteroids = TRUE;
    catSmViewResources = TRUE;
    catSmViewFOWRanges = TRUE;
    catSmViewBackground = TRUE;
    catSmViewAreasVisited = TRUE;
    catSmViewAreasOccupied = FALSE;
    catSmViewIcons = TRUE;
    catSmViewShipMeshes = TRUE;
    catSmViewPieplateLines = TRUE;
    catSmViewMeteors = TRUE;
    catSmViewCompass = FALSE;
    catSmViewObjectivePointers = TRUE;
    catSmViewSlipgatePaths = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : kasfSetSensorsLookAtPoint                         0x0049a2a0
    Description : CAT: KAS "SetSensorsLookAtPoint": with the sensors manager open, its camera looks at the point
----------------------------------------------------------------------------*/
void kasfSetSensorsLookAtPoint(hvector *point)
{
    if (smSensorsActive)
    {
        smCameraLookatPoint.x = point->x;
        smCameraLookatPoint.y = point->y;
        smCameraLookatPoint.z = point->z;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSetHelpless                              0x0049a2d0
    Description : CAT: KAS "ShipsSetHelpless": helpless "forever" (1e21 s) or not
----------------------------------------------------------------------------*/
void kasfShipsSetHelpless(GrowSelection *ships, sdword helpless)
{
    sdword i;

    for (i = 0; i < ships->selection->numShips; i++)
    {
        if (helpless == 0)
        {
            cat_shipClearHelpless(ships->selection->ShipPtr[i]);
        }
        else
        {
            cat_shipSetHelpless(ships->selection->ShipPtr[i], 1e21f);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfTutSelectShips                                0x0049a350
    Description : CAT: KAS "TutSelectShips": the ships become the current selection (cat_tutSelectShips)
----------------------------------------------------------------------------*/
void kasfTutSelectShips(GrowSelection *pShips)
{
    cat_tutSelectShips((MaxSelection *)pShips->selection);
}

/*-----------------------------------------------------------------------------
    Name        : kasfNomadMoon* / kasf*NaggarokMissiles            0x0049a360 .. 0x0049a3e0
    Description : CAT: the first ship's Nomad moon emitters, can-fire and fire; the Naggarok's missile count
----------------------------------------------------------------------------*/
sdword kasfNomadMoonEmitterCount(GrowSelection *NomadMoon)
{
    return cat_nomadMoonEmitterCount(NomadMoon->selection->ShipPtr[0]);
}

sdword kasfNomadMoonCanFire(GrowSelection *NomadMoon)  // KAS NomadMoonCanFire, cataclysm.exe 0x0049a380
{
    return cat_nomadMoonCanFire(NomadMoon->selection->ShipPtr[0]);
}

sdword kasfNomadMoonFire(GrowSelection *NomadMoon)  // KAS NomadMoonFire, cataclysm.exe 0x0049a3a0
{
    return cat_nomadMoonFire(NomadMoon->selection->ShipPtr[0]);
}

sdword kasfGetNaggarokMissiles(GrowSelection *Naggarok)  // KAS GetNaggarokMissiles, cataclysm.exe 0x0049a3c0
{
    return cat_rkoGetNaggarokMissiles(Naggarok->selection->ShipPtr[0]);
}

void kasfDecNaggarokMissiles(GrowSelection *Naggarok)
{
    cat_rkoDecNaggarokMissiles(Naggarok->selection->ShipPtr[0]);
}

/*-----------------------------------------------------------------------------
    Name        : kasfEnableBeastWeaponAutoAttack                   0x0049a400
    Description : CAT: KAS "EnableBeastWeaponAutoAttack": specialFlags2 bit 23 set when bEnable is 0
----------------------------------------------------------------------------*/
void kasfEnableBeastWeaponAutoAttack(GrowSelection *pBeastShips, sdword bEnable)
{
    sdword i;

    for (i = 0; i < pBeastShips->selection->numShips; i++)
    {
        Ship *ship = pBeastShips->selection->ShipPtr[i];
        ship->shipOrDerelict.specialFlags2 = (ship->shipOrDerelict.specialFlags2 & ~SPECIAL_2_CatBeastWeaponAutoAttack) |
                                             ((udword)(bEnable == 0) << 23);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfSetAIDepotShip / kasfMission13DeleteEngineeringModuleHack   0x0049a460 / 0x0049a480
    Description : CAT: the AI's salvage depot; the next wiped ship is freed directly (catWipeShipFreeDirectly)
----------------------------------------------------------------------------*/
void kasfSetAIDepotShip(GrowSelection *pShip)
{
    cat_salvageSetAIDepotShip(pShip->selection->ShipPtr[0]);
}

void kasfMission13DeleteEngineeringModuleHack(void)
{
    catWipeShipFreeDirectly = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : kasfSlipgateExplode                               0x0049a490
    Description : CAT: KAS "SlipgateExplode" (cat_blastCreateSlipgateExplosion at the point)
----------------------------------------------------------------------------*/
void kasfSlipgateExplode(hvector *center)
{
    cat_blastCreateSlipgateExplosion(center->x, center->y, center->z);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsHyperspaceOut / kasfShipsHyperspaceIn / kasfShipsHyperspaceInDefault   0x0049a4d0 .. 0x0049a560
    Description : CAT: the ships (the enabled ones) hyperspace out; hyperspace in at the point; each hyperspaces in
                  at its own position
----------------------------------------------------------------------------*/
void kasfShipsHyperspaceOut(GrowSelection *ships)
{
    MaxSelection filtered;

    if ((ships != NULL) && (ships->selection->numShips != 0))
    {
        filtered.numShips = 0;
        aiuFilterDisabledShips(ships->selection, &filtered);
        RemoveShipsFromDoingStuff(&universe.mainCommandLayer, (SelectCommand *)&filtered, 0x17);
        spHyperspaceSelectionOut((SelectCommand *)&filtered);
        spHyperspaceDelay = 0.0f;
    }
}

void kasfShipsHyperspaceIn(GrowSelection *ships, hvector *point)  // KAS ShipsHyperspaceIn, cataclysm.exe 0x0049a530
{
    if ((ships != NULL) && (ships->selection->numShips != 0))
    {
        spHyperspaceSelectionIn(ships->selection, point);
        spHyperspaceDelay = 0.0f;
    }
}

void kasfShipsHyperspaceInDefault(GrowSelection *ships)
{
    SelectCommand one;
    hvector here;
    sdword i;

    if ((ships == NULL) || (ships->selection->numShips == 0))
    {
        return;
    }
    for (i = 0; i < ships->selection->numShips; i++)
    {
        one.ShipPtr[0] = ships->selection->ShipPtr[i];
        one.numShips = 1;
        here.x = ships->selection->ShipPtr[i]->obj.posinfo.position.x;
        here.y = ships->selection->ShipPtr[i]->obj.posinfo.position.y;
        here.z = ships->selection->ShipPtr[i]->obj.posinfo.position.z;
        here.w = 1.0f;
        spHyperspaceSelectionIn(&one, &here);
        spHyperspaceDelay = 0.0f;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfForceParadeFormation                          0x0049a5f0
    Description : CAT: KAS "ForceParadeFormation": every fleet ship stops what it does (reason 0x19), steadies and
                  joins the parade around the mothership, then the parade is set
----------------------------------------------------------------------------*/
void kasfForceParadeFormation(GrowSelection *mothership, GrowSelection *fleet)
{
    SelectCommand one;
    Ship *aroundShip, *ship;
    sdword i;

    if ((mothership == NULL) || (mothership->selection == NULL) || (mothership->selection->numShips == 0) ||
        (fleet == NULL) || (fleet->selection == NULL) || (fleet->selection->numShips == 0))
    {
        return;
    }
    aroundShip = mothership->selection->ShipPtr[0];
    for (i = 0; i < fleet->selection->numShips; i++)
    {
        ship = fleet->selection->ShipPtr[i];
        one.numShips = 1;
        one.ShipPtr[0] = ship;
        RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &one, 0x19);
        aitrackForceSteadyShip(ship);
        GroupShipIntoMilitaryParade(&universe.mainCommandLayer, ship, aroundShip);
    }
    setMilitaryParade(GetMilitaryGroupAroundShip(&universe.mainCommandLayer, aroundShip));
}

/*-----------------------------------------------------------------------------
    Name        : kasfIsForceFieldActive / kasfIsEMPReady / kasfIsBeastReady / kasfIsSwarmReady   0x0049a6a0 ..
    Description : CAT: one of the ships has its Sentinel field up / a full EMP charge / a full Beast weapon charge;
                  IsSwarmReady: one of the hives is not swarm-ready (cat_hiveFrigateSwarmReady FALSE)
----------------------------------------------------------------------------*/
sdword kasfIsForceFieldActive(GrowSelection *Ships)
{
    sdword i;

    for (i = 0; i < Ships->selection->numShips; i++)
    {
        if (cat_shipIsSentinelFieldActive(Ships->selection->ShipPtr[i]))
        {
            return 1;
        }
    }
    return 0;
}

sdword kasfIsEMPReady(GrowSelection *Ships)  // KAS IsEMPReady, cataclysm.exe 0x0049a6f0
{
    sdword i;

    for (i = 0; i < Ships->selection->numShips; i++)
    {
        if (!(cat_acvEMPChargeFraction(Ships->selection->ShipPtr[i]) < 1.0f))
        {
            return 1;
        }
    }
    return 0;
}

sdword kasfIsBeastReady(GrowSelection *Ships)  // KAS IsBeastReady, cataclysm.exe 0x0049a740
{
    sdword i;

    for (i = 0; i < Ships->selection->numShips; i++)
    {
        if (!(cat_beastWeaponChargeFraction(Ships->selection->ShipPtr[i]) < 1.0f))
        {
            return 1;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : kasfIsGuarding                                    0x0049a790
    Description : CAT: KAS "IsGuarding": the ships the guard orders of Ships protect (Guarded cleared first)
----------------------------------------------------------------------------*/
sdword kasfIsGuarding(GrowSelection *Ships, GrowSelection *Guarded)
{
    SelectCommand *these = Ships->selection;
    Node *curnode;
    CommandToDo *command;
    sdword i;

    kasGrowSelectionClear(Guarded);
    if (Guarded == NULL)
    {
        return 0;
    }
    if (these->numShips == 0)
    {
        return 0;
    }
    for (curnode = universe.mainCommandLayer.todolist.head; curnode != NULL; curnode = curnode->next)
    {
        command = (CommandToDo *)listGetStructOfNode(curnode);
        if ((command->ordertype.attributes & COMMAND_MASK_PROTECTING) && (command->protect != NULL) &&
            AnyOfTheseShipsAreInSelection(these, command->selection))
        {
            for (i = 0; i < command->protect->numShips; i++)
            {
                growSelectAddShipNoDuplication(Guarded, command->protect->ShipPtr[i]);
            }
        }
    }
    return Guarded->selection->numShips;
}

/*-----------------------------------------------------------------------------
    Name        : kasfSetStayMimicked                               0x0049a850
    Description : CAT: KAS "SetStayMimicked": specialFlags2 bit 27, and the mimic's player-1 detected flag cleared
----------------------------------------------------------------------------*/
void kasfSetStayMimicked(GrowSelection *pShips, sdword bStayMimicked)
{
    sdword i;

    for (i = 0; i < pShips->selection->numShips; i++)
    {
        Ship *ship = pShips->selection->ShipPtr[i];
        ship->shipOrDerelict.specialFlags2 = (ship->shipOrDerelict.specialFlags2 & ~SPECIAL_2_CatStayMimicked) |
                                             ((udword)(bStayMimicked & 1) << 27);
        ((MimicSpec *)ship->ShipSpecifics)->detected[1] = FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfGetDockHost                                   0x0049a8b0
    Description : CAT: KAS "GetDockHost": the possible hosts one of the ships is inside
----------------------------------------------------------------------------*/
void kasfGetDockHost(GrowSelection *Ships, GrowSelection *Host, GrowSelection *PossibleHost)
{
    sdword i, j;

    kasGrowSelectionClear(Host);
    for (i = 0; i < PossibleHost->selection->numShips; i++)
    {
        for (j = 0; j < Ships->selection->numShips; j++)
        {
            if (cat_dockShipIsInsideHost(Ships->selection->ShipPtr[j], PossibleHost->selection->ShipPtr[i]))
            {
                growSelectAddShipNoDuplication(Host, PossibleHost->selection->ShipPtr[i]);
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfGetTargets                                    0x0049a950
    Description : CAT: KAS "GetTargets": the targets of the ships' special orders
----------------------------------------------------------------------------*/
sdword kasfGetTargets(GrowSelection *Targets, GrowSelection *Ships)
{
    sdword i, j;
    CommandToDo *command;

    kasGrowSelectionClear(Targets);
    for (i = 0; i < Ships->selection->numShips; i++)
    {
        command = Ships->selection->ShipPtr[i]->command;
        if ((command != NULL) && (command->ordertype.order == COMMAND_SPECIAL))
        {
            for (j = 0; j < command->specialtargets->numTargets; j++)
            {
                growSelectAddShipNoDuplication(Targets, (Ship *)command->specialtargets->TargetPtr[j]);
            }
        }
    }
    return Targets->selection->numShips;
}

/*-----------------------------------------------------------------------------
    Name        : kasfRKO*                                          0x0049aa00 .. 0x0049aac0
    Description : CAT: the RKO's big-gun position, its safety, an avoid location and its warning (each 0 without a
                  ship)
----------------------------------------------------------------------------*/
void kasfRKOBigGunPos(GrowSelection *RKO, hvector *Bullet)
{
    vector pos;

    pos.y = Bullet->y;
    pos.x = Bullet->x;
    pos.z = Bullet->z;
    cat_rkoSetBigGunPos(RKO->selection->ShipPtr[0], &pos);
}

sdword kasfRKOIsSafe(GrowSelection *RKO)  // KAS RKOIsSafe, cataclysm.exe 0x0049aa40
{
    if (RKO->selection->numShips >= 1)
    {
        return cat_rkoIsSafe(RKO->selection->ShipPtr[0]);
    }
    return 0;
}

sdword kasfRKOAvoidLocation(GrowSelection *RKO, hvector *Avoid, hvector *Safe)  // KAS RKOAvoidLocation, cataclysm.exe 0x0049aa60
{
    vector avoid, safe;

    avoid.x = Avoid->x;
    avoid.y = Avoid->y;
    avoid.z = Avoid->z;
    safe.x = Safe->x;
    safe.y = Safe->y;
    safe.z = Safe->z;
    if (RKO->selection->numShips >= 1)
    {
        return cat_rkoGetAvoidLocation(RKO->selection->ShipPtr[0], &avoid, &safe);
    }
    return 0;
}

sdword kasfRKOBeware(GrowSelection *RKO)
{
    if (RKO->selection->numShips >= 1)
    {
        return cat_rkoBeware(RKO->selection->ShipPtr[0]);
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSetColor                                 0x0049aae0
    Description : CAT: KAS "ShipsSetColor" / "ShipsSetColour": each ship's colour scheme byte
----------------------------------------------------------------------------*/
void kasfShipsSetColor(GrowSelection *Ships, sdword Color)
{
    sdword i;

    for (i = 0; i < Ships->selection->numShips; i++)
    {
        Ships->selection->ShipPtr[i]->colorScheme = (sbyte)Color;
    }
}

sdword kasfIsSwarmReady(GrowSelection *Ships)  // KAS IsSwarmReady, cataclysm.exe 0x0049ab20
{
    sdword i;

    for (i = 0; i < Ships->selection->numShips; i++)
    {
        if (!cat_hiveFrigateSwarmReady(Ships->selection->ShipPtr[i]))
        {
            return 1;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsHealthAverage                            0x0049ab70
    Description : CAT: KAS "ShipsHealthAverage": the health sum * 100 / the maximum sum, truncated (sums on the x87
                  stack); 0 without ships
----------------------------------------------------------------------------*/
sdword kasfShipsHealthAverage(GrowSelection *Ships)
{
    sdword i;
    real64 maxHealth = 0.0, actualHealth = 0.0;

    if (Ships->selection->numShips == 0)
    {
        return 0;
    }
    for (i = 0; i < Ships->selection->numShips; i++)
    {
        maxHealth += Ships->selection->ShipPtr[i]->staticinfo->health.maxhealth;
        actualHealth += Ships->selection->ShipPtr[i]->targetable.health;
    }
    return (sdword)(actualHealth * (real64)100.0f / maxHealth);
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsSetInvincibleInWidescreen                0x0049abd0
    Description : CAT: specialFlags2 bit 24 set when the argument is 0 (the bit makes a ship vulnerable during a
                  widescreen: ApplyDamageToTarget)
----------------------------------------------------------------------------*/
void kasfShipsSetInvincibleInWidescreen(GrowSelection *pShips, sdword bInvincibleInWidescreen)
{
    sdword i;

    for (i = 0; i < pShips->selection->numShips; i++)
    {
        Ship *ship = pShips->selection->ShipPtr[i];
        ship->shipOrDerelict.specialFlags2 = (ship->shipOrDerelict.specialFlags2 & ~SPECIAL_2_CatInvincibleInWidescreen) |
                                             ((udword)(bInvincibleInWidescreen == 0) << 24);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfDisableBigGunFiring                           0x0049ac30
    Description : CAT: KAS "DisableBigGun": the mothership's big gun may not fire (catSpBigGunAllowed)
----------------------------------------------------------------------------*/
void kasfDisableBigGunFiring(void)
{
    catSpBigGunAllowed = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsShuffle                                  0x0049ac40
    Description : CAT: KAS "ShipsShuffle": each ship swapped with a random one (random stream 6)
----------------------------------------------------------------------------*/
void kasfShipsShuffle(GrowSelection *pShips)
{
    sdword i;
    udword pick;
    Ship *ship;

    for (i = 0; i < pShips->selection->numShips; i++)
    {
        pick = (udword)ranRandom(RANDOM_AI_PLAYER) % (udword)pShips->selection->numShips;
        ship = pShips->selection->ShipPtr[i];
        pShips->selection->ShipPtr[i] = pShips->selection->ShipPtr[pick];
        pShips->selection->ShipPtr[pick] = ship;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsStopForResearchDocking                   0x0049ac90
    Description : CAT: specialFlags bit 0 = bStop's bit 0
----------------------------------------------------------------------------*/
void kasfShipsStopForResearchDocking(GrowSelection *pShips, sdword bStop)
{
    sdword i;

    for (i = 0; i < pShips->selection->numShips; i++)
    {
        Ship *ship = pShips->selection->ShipPtr[i];
        ship->shipOrDerelict.specialFlags = ((ship->shipOrDerelict.specialFlags ^ (udword)bStop) & SPECIAL_StopForResearchDocking) ^
                                            ship->shipOrDerelict.specialFlags;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfGetVectorX .. kasfGetVectorW                  0x0049ace0 .. 0x0049ad40
    Description : CAT: KAS "GetPointX" .. "GetPointW": floor(coordinate + 0.5) (rounded half up)
----------------------------------------------------------------------------*/
sdword kasfGetVectorX(hvector *vector)
{
    return (sdword)floor((real64)vector->x + (real64)0.5f);
}

sdword kasfGetVectorY(hvector *vector)  // KAS GetPointY, cataclysm.exe 0x0049ad00
{
    return (sdword)floor((real64)vector->y + (real64)0.5f);
}

sdword kasfGetVectorZ(hvector *vector)  // KAS GetPointZ, cataclysm.exe 0x0049ad20
{
    return (sdword)floor((real64)vector->z + (real64)0.5f);
}

sdword kasfGetVectorW(hvector *vector)
{
    return (sdword)floor((real64)vector->w + (real64)0.5f);
}

/*-----------------------------------------------------------------------------
    Name        : kasfCameraSetChaseMode                            0x0049ad60
    Description : CAT: KAS "CameraSetChaseMode": the camera mode's pilot bit on or off
----------------------------------------------------------------------------*/
void kasfCameraSetChaseMode(sdword chaseMode)
{
    if (chaseMode != 0)
    {
        bitSet(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP);
    }
    else
    {
        bitClear(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP);
    }
}

/*-----------------------------------------------------------------------------
    Name        : kasfShipsInsideMe                                 0x0049ad80
    Description : CAT: KAS "ShipsInsideMe": every ship inside one of the ships
----------------------------------------------------------------------------*/
sdword kasfShipsInsideMe(GrowSelection *InsideMe, GrowSelection *Me)
{
    sdword i;
    Node *node;

    kasGrowSelectionClear(InsideMe);
    for (i = 0; i < Me->selection->numShips; i++)
    {
        if (Me->selection->ShipPtr[i]->shipsInsideMe != NULL)
        {
            for (node = Me->selection->ShipPtr[i]->shipsInsideMe->insideList.head; node != NULL; node = node->next)
            {
                growSelectAddShipNoDuplication(InsideMe, ((InsideShip *)listGetStructOfNode(node))->ship);
            }
        }
    }
    return InsideMe->selection->numShips;
}

/*-----------------------------------------------------------------------------
    Name        : kasfSetPlayerResourceCap / kasfAssert             0x0049ae30
    Description : CAT: Cataclysm-only table rows folded into the empty stub 0x0049ae30 (they do nothing in retail)
----------------------------------------------------------------------------*/
void kasfSetPlayerResourceCap(sdword maxResources)
{
    (void)maxResources;
}

void kasfAssert(sdword value)
{
    (void)value;
}
