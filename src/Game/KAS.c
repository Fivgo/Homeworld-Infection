//
//  support code for the KAS language and its missions, FSMs, and states
//
//  implementations of most functions are in kasfunc.c
//

#include "KAS.h"
#include "KASVar.h"

#include <stdio.h>
#include <string.h>

#include "AIPlayer.h"
#include "AITeam.h"
#include "AIVar.h"
#include "CommandWrap.h"
#include "Debug.h"
#include "File.h"
#include "font.h"
#include "glinc.h"
#include "HS.h"
#include "main.h"
#include "mainrgn.h"
#include "Memory.h"
#include "Objectives.h"
#include "prim3d.h"
#include "render.h"
#include "rglu.h"
#include "SaveGame.h"
#include "SaveCat32.h"     // CAT (M2-R8 singleplayer-tutorial): a cataclysm.exe save's mission tail
#include "Select.h"
#include "SinglePlayer.h"
#include "Timer.h"
#include "Globals.h"          // CAT: singlePlayerGame (kasSave / kasLoad)
#include "ResearchAPI.h"      // CAT: rmCanBuildShip / rmEnableShip (kasSave / kasLoad)
#include "Universe.h"
#include "utility.h"          // CAT: catSpDifficultySetting (kasSave)
#include "CatMissions.h"      // CAT: the generated scripts' function lists (kasConvertFuncPtrToOffset)
#include "OracleDump.h"       // HWI_ORACLE
#include "MissionSelfTest.h"  // HWI-TEST (M2-R8 kas-runtime-mission01): the mission self-test's flow lines

#ifdef _WIN32
    #define strcasecmp  _stricmp
    #define strncasecmp _strnicmp
#endif

#if !defined _MSC_VER
    #include <strings.h>
#endif


extern sdword objectivesUsed;
#if SP_DEBUGKEYS
//#if SP_DEBUGLEVEL2
// for debug overlay
extern fonthandle selGroupFont2;
extern AIVar **vars;
extern sdword varsUsed;
extern sdword timersUsed;
extern Timer *timers;
sdword mrKASDebugDrawStates = FALSE;
sdword mrKASDebugDrawTimers = FALSE;
sdword mrKASDebugDrawVars = FALSE;
sdword mrKASDebugDrawVolumes = FALSE;
sdword mrKASSkipFactor = 0;
#endif //SP_DEBUGLEVEL2

#define KAS_DEBUG 0

#define OBJECTIVES_COMPLETE_DELAY 10

// run-time scoping for variables, timers, etc.
sdword CurrentMissionScope;  // what level we're currently executing at
// CAT (M2-R8 kasf library): the scope is a cat_kasHashString hash (0x00c374a0: the kas2c missions' prologue stores
//      hash("missionNN") or the running team's label hash; the kasf scope their timer and variable names by it)
udword CurrentMissionScopeName;

char CurrentMissionName[KAS_MISSION_NAME_MAX_LENGTH+1];
KASWatchFunction CurrentMissionWatchFunction = NULL;
AITeam *CurrentTeamP = NULL;
AITeam *kasUnpausedTeam = NULL;

#define kasThisTeamPtr CurrentTeamP

//
//  variable space for user selections (the labelled "SHIPS" type in KAS)
//
KasSelection **Selections = NULL;
static sdword SelectionsUsed = 0;
static sdword SelectionsAllocated = 0;

//
//  table of labelled entities in mission layout file
//  (filled in at mission load time)
//

//  team labels are stored in AITeam struct
//LabelledAITeams *LabelledAITeam;
//static sdword LabelledAITeamsUsed = 0;
//static sdword LabelledAITeamsAllocated = 0;

LabelledPath **LabelledPaths = NULL;
static sdword LabelledPathsUsed = 0;
static sdword LabelledPathsAllocated = 0;

LabelledVector **LabelledVectors = NULL;
sdword LabelledVectorsUsed = 0;
sdword LabelledVectorsAllocated = 0;

LabelledVolume **LabelledVolumes = NULL;
static sdword LabelledVolumesUsed = 0;
static sdword LabelledVolumesAllocated = 0;

real32 subMessageReturnedFalseTime = 0;

// need some closing stuff for kas - global variables need to be
// made NULL etc.
void kasClose(void)
{
    kasUnpausedTeam = NULL;
    subMessageReturnedFalseTime = 0.0f;
}

void kasInit(void)
{
    subMessageReturnedFalseTime = 0.0f;
}

void kasTeamDied(struct AITeam *team)
{
    if (CurrentTeamP == team)
    {
        CurrentTeamP = NULL;
    }
}

//
//  JUMP keyword of KAS language
//
//  sets state of current team to new state (for subsequently WATCHing) and executes its
//  init function right now
//
void kasJump(udword stateName, KASInitFunction initFunction, KASWatchFunction watchFunction)
{
#ifdef HW_BUILD_FOR_DEBUGGING
    char teamName[KAS_TEAM_NAME_MAX_LENGTH+1];
    aiplayerLog((aiCurrentAIPlayer->player->playerIndex,"KAS: TEAM(\"%s\") JUMP %07x", kasAITeamName(CurrentTeamP, teamName), stateName));
#endif

    CurrentTeamP->kasStateName = stateName;     // CAT: the state's hash (kasJump 0x00490640)

    CurrentTeamP->kasStateWatchFunction = watchFunction;

    initFunction();
}

//
//  FSMCreate keyword of KAS language
//
//  "creates" an FSM (Finite State Machine) and hands control of a team to it
//
void kasFSMCreate(udword fsmName, KASInitFunction initFunction, KASWatchFunction watchFunction, AITeam *teamP)
{
	char teamName[KAS_TEAM_NAME_MAX_LENGTH+1];
    AITeam *saveP;
    sdword saveScope;
    udword SaveMissionScopeName;

    // save current team & scope
    saveP = CurrentTeamP;
    saveScope = CurrentMissionScope;
    SaveMissionScopeName = CurrentMissionScopeName;

    CurrentTeamP = teamP;

#ifdef HW_BUILD_FOR_DEBUGGING
    aiplayerLog((aiCurrentAIPlayer->player->playerIndex,"\nKAS: FSMCreate(%07x, TEAM(\"%s\")", fsmName, kasAITeamName(teamP, teamName)));
#endif

    teamP->teamType = ScriptTeam;
    teamP->kasFSMName = fsmName;          // CAT: the FSM's hash (kasFSMCreate 0x00490660)
    teamP->kasFSMWatchFunction = watchFunction;
    teamP->kasStateWatchFunction = NULL;  // just in case it doesn't get set later
    teamP->kasTactics   = Neutral;        // default to neutral tactics
    teamP->kasFormation = NO_FORMATION;   // default to no formation

    CurrentMissionScopeName = kasThisTeamPtr->kasLabel;     // CAT: the label's hash (kasFSMCreate 0x00490660 copies team +0x58)

    initFunction();

    // restore current team & scope
    CurrentTeamP = saveP;
    CurrentMissionScope = saveScope;
    CurrentMissionScopeName = SaveMissionScopeName;
}

//
//  all the processing of KAS controlled stuff happens here
//
//  must be called once per AI cycle
//
void kasExecute(void)
{
    sdword i;

    // no current mission?
    if (!CurrentMissionName[0] || !CurrentMissionWatchFunction)
        return;

    if (kasUnpausedTeam == NULL)
    {                                                       //if KAS is not paused
        CurrentMissionWatchFunction();          // watch at the mission level

        for (i = 0; i < aiCurrentAIPlayer->teamsUsed; i++)
        {
            CurrentTeamP = aiCurrentAIPlayer->teams[i];
            if (CurrentTeamP->teamType == ScriptTeam)
            {
                if (CurrentTeamP->kasFSMWatchFunction)   // watch at the FSM level
                    CurrentTeamP->kasFSMWatchFunction();
                if (CurrentTeamP->kasStateWatchFunction) // watch at the state level
                    CurrentTeamP->kasStateWatchFunction();
            }
        }
    }
    else
    {                                                       //else all FSM's but one are paused
        CurrentTeamP = kasUnpausedTeam;                     //only execute this 1 unpaused team
        if (CurrentTeamP->teamType == ScriptTeam)
        {
            if (CurrentTeamP->kasFSMWatchFunction)   // watch at the FSM level
                CurrentTeamP->kasFSMWatchFunction();
            if (CurrentTeamP->kasStateWatchFunction) // watch at the state level
                CurrentTeamP->kasStateWatchFunction();
        }
    }
}

//
//  starts a mission
//
void kasMissionStart(char *name, KASInitFunction initFunction, KASWatchFunction watchFunction)
{
    aiplayerLog((0,"\nKAS: Starting %s", name));
#if HWI_ORACLE
    {                                                   // HWI-TEST: the mission self-test's flow line
        char line[64];
        snprintf(line, sizeof(line), "kas start %s", name);
        catMissionSelfTestEvent(line);
    }
#endif
    memStrncpy(CurrentMissionName, name, KAS_MISSION_NAME_MAX_LENGTH);
    CurrentMissionWatchFunction = watchFunction;

    //hyperspace init
    hsStaticInit();     // CAT: no argument (hsStaticInit 0x004901b0)

    initFunction();         // must go last - save game is called at end of this function, we want everything setup
}

//
//  return reference to labelled team
//
// CAT: 0x004907a0: by hash; no "MsgSender" label and no fatal for an unknown one (NULL)
AITeam *kasAITeamPtr(udword label)
{
    sdword i = 0;
    AITeam *aiteamp = NULL;

    while (i < aiCurrentAIPlayer->teamsUsed)
    {
        aiteamp = aiCurrentAIPlayer->teams[i];
        if (aiteamp->teamType == ScriptTeam && aiteamp->kasLabel == label)
            return aiteamp;
        ++i;
    }

    return NULL;
}

//
//  return reference to labelled team's ships (contrast with kasAITeamPtr)
//
// CAT: 0x004907e0: by hash; no "MsgSender" label and no fatal (NULL)
GrowSelection *kasAITeamShipsPtr(udword label)
{
    sdword i = 0;
    AITeam *aiteamp;

    while (i < aiCurrentAIPlayer->teamsUsed)
    {
        aiteamp = aiCurrentAIPlayer->teams[i];
        if (aiteamp->teamType == ScriptTeam && aiteamp->kasLabel == label)
            return &aiteamp->shipList;
        ++i;
    }

    return NULL;
}

//
//  return reference to labelled grow selection's vector
//
hvector *kasShipsVectorPtr(udword label)
{
    static hvector errorPos= {0, 0, 0, 1};  // safe fallback
    static hvector location;
    vector loc;
    GrowSelection *grow;
    real32 dummy;

    grow = kasGetGrowSelectionPtrIfExists(label);

    if (grow && grow->selection->numShips)
    {
        loc = selCentrePointComputeGeneral((MaxSelection *)grow->selection, &dummy);
        vecMakeHVecFromVec(location, loc);
        return &location;
    }
    else
    {
        return &errorPos;
    }

#ifdef HW_BUILD_FOR_DEBUGGING
    dbgFatalf(DBG_Loc,"\nKAS: unresolved reference to shiplist %07x", label);
#endif

    return NULL;
}

//
// return reference to labelled team's location
//
hvector *kasTeamsVectorPtr(udword label)
{
    static hvector errorPos = {0,0,0,1};
    static hvector location;
    vector loc;
    GrowSelection *grow;
    real32 dummy;

    grow = kasAITeamShipsPtr(label);

    if (grow && grow->selection->numShips)
    {
        loc = selCentrePointComputeGeneral((MaxSelection *)grow->selection, &dummy);
        vecMakeHVecFromVec(location, loc);
        return &location;
    }
    else
    {
        return &errorPos;
    }

#ifdef HW_BUILD_FOR_DEBUGGING
    dbgFatalf(DBG_Loc,"\nKAS: unresolved reference to shiplist %07x", label);
#endif

    return NULL;
}

//
// return reference to labelled volume's center
//
hvector *kasVolumeVectorPtr(udword label)
{
    static hvector returnhvec;
    vector volcenter;
    Volume *vol = kasVolumePtr(label);

    if (vol)
    {
        volcenter = volFindCenter(vol);
        vecMakeHVecFromVec(returnhvec, volcenter);
        return &returnhvec;
    }
    else
    {
        return NULL;
    }
}

//
// return reference to the team's centerpoint
//
hvector *kasThisTeamsVectorPtr(void)
{
    static hvector errorPos = {0,0,0,1};
    static hvector location;
    vector loc;
    GrowSelection *grow;
    real32 dummy;

    grow = &CurrentTeamP->shipList;

    if (grow && grow->selection->numShips)
    {
        loc = selCentrePointComputeGeneral((MaxSelection *)grow->selection, &dummy);
        vecMakeHVecFromVec(location, loc);
        return &location;
    }
    else
    {
        return &errorPos;
    }

#ifdef HW_BUILD_FOR_DEBUGGING
    dbgFatalf(DBG_Loc,"\nKAS: unresolved reference in kasThisTeamsVectorPtr");
#endif

    return NULL;

}

//
//  return reference to labelled path
//
Path *kasPathPtrNoErrorChecking(udword label)
{
    sdword i = 0;
    while (i < LabelledPathsUsed)
    {
        if (LabelledPaths[i]->label == label)
            return LabelledPaths[i]->path;
        ++i;
    }
    return NULL;
}

//
//  return reference to labelled path
//
// CAT: 0x00490aa0: an unknown path returns NULL (the exe's debug report is an empty stub, no fatal)
Path *kasPathPtr(udword label)
{
    sdword i = 0;
    while (i < LabelledPathsUsed)
    {
        if (LabelledPaths[i]->label == label)
            return LabelledPaths[i]->path;
        ++i;
    }
    dbgMessagef("KAS: unresolved reference to path %07x", label);
    return NULL;
}

//
//  return reference to labelled volume
//
// CAT: 0x00490af0: no fatal for an unknown volume (NULL)
Volume *kasVolumePtr(udword label)
{
    sdword i = 0;
    while (i < LabelledVolumesUsed)
    {
        if (LabelledVolumes[i]->label == label)
            return LabelledVolumes[i]->volume;
        ++i;
    }

    return NULL;
}

//
//  return reference to labelled point
//
hvector *kasVectorPtrIfExists(udword label)
{
    sdword i = 0;
    while (i < LabelledVectorsUsed)
    {
        if (LabelledVectors[i]->label == label)
            return LabelledVectors[i]->hvector;
        ++i;
    }
    return NULL;
}

// CAT: 0x00490b30 (kasVectorPtrIfExists folded into it by the linker)
hvector *kasVectorPtr(udword label)
{
    return kasVectorPtrIfExists(label);
}

GrowSelection *kasGetGrowSelectionPtrIfExists(udword label)
{
    sdword i = 0;

    while (i < SelectionsUsed)
    {
        if (Selections[i]->label == label)
            return &(Selections[i]->shipList);
        ++i;
    }

    return NULL;
}

//
//  on first reference to a new GrowSelection, create one and return its pointer
//  otherwise, return the pointer
//
GrowSelection *kasGrowSelectionPtr(udword label)
{
    sdword i = 0;

    while (i < SelectionsUsed)
    {
        if (Selections[i]->label == label)
            return &(Selections[i]->shipList);
        ++i;
    }

    if (!Selections)
    {
        Selections = memAlloc(sizeof(KasSelection *) * KASSELECTION_ALLOC_INCREMENT, "kasSelections", 0);
        SelectionsAllocated = KASSELECTION_ALLOC_INCREMENT;
        SelectionsUsed = 0;
    }
    // keep track of all allocations
    if (SelectionsUsed >= SelectionsAllocated)
    {
        // allocate more if necessary
        Selections = memRealloc(Selections, sizeof(KasSelection *) * (SelectionsAllocated + KASSELECTION_ALLOC_INCREMENT), "kasSelections", 0);
        SelectionsAllocated += KASSELECTION_ALLOC_INCREMENT;
    }
    Selections[SelectionsUsed] = memAlloc(sizeof(KasSelection), "kasGrowSelection", 0);
    Selections[SelectionsUsed]->label = label;
    growSelectInit(&Selections[SelectionsUsed]->shipList);

    return &(Selections[SelectionsUsed++]->shipList);
}

void kasGrowSelectionClear(GrowSelection *ships)
{
    if (ships && ships->selection)
    {
        ships->selection->numShips = 0;
    }
}

//
//  add new labelled path (with no points, yet), return pointer to it
//
Path *kasLabelledPathAdd(udword label, sdword numPoints, sdword closed)
{
    if (!LabelledPaths)
    {
        LabelledPaths = memAlloc(sizeof(LabelledPath *) * KAS_LABELLED_ENTITY_ALLOC_INCREMENT, "kasLabelledPaths", 0);
        LabelledPathsAllocated = KAS_LABELLED_ENTITY_ALLOC_INCREMENT;
        LabelledPathsUsed = 0;
    }
    // keep track of all allocations
    if (LabelledPathsUsed >= LabelledPathsAllocated)
    {
        // allocate more if necessary
        LabelledPaths = memRealloc(LabelledPaths, sizeof(LabelledPath *) * (LabelledPathsAllocated + KAS_LABELLED_ENTITY_ALLOC_INCREMENT), "kasLabelledPaths", 0);
        LabelledPathsAllocated += KAS_LABELLED_ENTITY_ALLOC_INCREMENT;
    }
    LabelledPaths[LabelledPathsUsed] = memAlloc(sizeof(LabelledPath), "kasLabelledPath", MBF_NonVolatile);
    LabelledPaths[LabelledPathsUsed]->path = aiuCreatePathStruct(numPoints, closed);
    LabelledPaths[LabelledPathsUsed]->label = label;

    return LabelledPaths[LabelledPathsUsed++]->path;
}

//
//  add new labelled volume (with no contents, yet), return pointer to it
//
Volume *kasLabelledVolumeAdd(udword label)
{
    if (!LabelledVolumes)
    {
        LabelledVolumes = memAlloc(sizeof(LabelledVolume *) * KAS_LABELLED_ENTITY_ALLOC_INCREMENT, "kasLabelledVolumes", 0);
        LabelledVolumesAllocated = KAS_LABELLED_ENTITY_ALLOC_INCREMENT;
        LabelledVolumesUsed = 0;
    }
    // keep track of all allocations
    if (LabelledVolumesUsed >= LabelledVolumesAllocated)
    {
        // allocate more if necessary
        LabelledVolumes = memRealloc(LabelledVolumes, sizeof(LabelledVolume *) * (LabelledVolumesAllocated + KAS_LABELLED_ENTITY_ALLOC_INCREMENT), "kasLabelledVolumes", 0);
        LabelledVolumesAllocated += KAS_LABELLED_ENTITY_ALLOC_INCREMENT;
    }
    LabelledVolumes[LabelledVolumesUsed] = memAlloc(sizeof(LabelledVolume), "kasLabelledVolume", MBF_NonVolatile);
    LabelledVolumes[LabelledVolumesUsed]->volume = memAlloc(sizeof(Volume), "kasVolume", MBF_NonVolatile);
    LabelledVolumes[LabelledVolumesUsed]->label = label;

    return LabelledVolumes[LabelledVolumesUsed++]->volume;
}

//
//  add new labelled vector, return pointer to it
//
hvector *kasLabelledVectorAdd(udword label, real32 x, real32 y, real32 z, real32 w)
{
    if (!LabelledVectors)
    {
        LabelledVectors = memAlloc(sizeof(LabelledVector *) * KAS_LABELLED_ENTITY_ALLOC_INCREMENT, "kasLabelledVectors", 0);
        LabelledVectorsAllocated = KAS_LABELLED_ENTITY_ALLOC_INCREMENT;
        LabelledVectorsUsed = 0;
    }
    // keep track of all allocations
    if (LabelledVectorsUsed >= LabelledVectorsAllocated)
    {
        // allocate more if necessary
        LabelledVectors = memRealloc(LabelledVectors, sizeof(LabelledVector *) * (LabelledVectorsAllocated + KAS_LABELLED_ENTITY_ALLOC_INCREMENT), "kasLabelledVectors", 0);
        LabelledVectorsAllocated += KAS_LABELLED_ENTITY_ALLOC_INCREMENT;
    }
    LabelledVectors[LabelledVectorsUsed] = memAlloc(sizeof(LabelledVector), "kasLabelledVector", MBF_NonVolatile);
    LabelledVectors[LabelledVectorsUsed]->hvector = memAlloc(sizeof(hvector), "kasVector", MBF_NonVolatile);
    LabelledVectors[LabelledVectorsUsed]->label = label;
    LabelledVectors[LabelledVectorsUsed]->hvector->x = x;
    LabelledVectors[LabelledVectorsUsed]->hvector->y = y;
    LabelledVectors[LabelledVectorsUsed]->hvector->z = z;
    LabelledVectors[LabelledVectorsUsed]->hvector->w = w;

    return LabelledVectors[LabelledVectorsUsed++]->hvector;
}

//
//  remove all labelled entities
//
void kasLabelledEntitiesDestroy(void)
{
    sdword i;

    CurrentTeamP = NULL;

    // selections
    for (i = 0; i < SelectionsUsed; ++i)
    {
        growSelectClose(&Selections[i]->shipList);
        memFree(Selections[i]);
    }
    if (Selections)
        memFree(Selections);
    SelectionsUsed = 0;
    SelectionsAllocated = 0;
    Selections = NULL;

    // vectors
    for (i = 0; i < LabelledVectorsUsed; ++i)
    {
        memFree(LabelledVectors[i]->hvector);
        memFree(LabelledVectors[i]);
    }
    if (LabelledVectors)
        memFree(LabelledVectors);
    LabelledVectorsUsed = 0;
    LabelledVectorsAllocated = 0;
    LabelledVectors = NULL;

    // volumes
    for (i = 0; i < LabelledVolumesUsed; ++i)
    {
        memFree(LabelledVolumes[i]->volume);
        memFree(LabelledVolumes[i]);
    }
    if (LabelledVolumes)
        memFree(LabelledVolumes);
    LabelledVolumesUsed = 0;
    LabelledVolumesAllocated = 0;
    LabelledVolumes = NULL;

    // paths
    for (i = 0; i < LabelledPathsUsed; ++i)
    {
        memFree(LabelledPaths[i]->path);
        memFree(LabelledPaths[i]);
    }
    if (LabelledPaths)
        memFree(LabelledPaths);
    LabelledPathsUsed = 0;
    LabelledPathsAllocated = 0;
    LabelledPaths = NULL;

    // vars
    aivarDestroyAll();
    cat_kasVarDestroyAll();     // CAT: and the hashed KAS variable store (kasLabelledEntitiesDestroy 0x00490f90 calls 0x00491ae0)

    // timers
    timTimerDestroyAll();
}

//
//  Find team label from pointer.
//  Returns teamName (& fills it in).
//  If not found, returns "NULL" or something like that.
//
char *kasAITeamName(AITeam *team, char *teamName)
{
    if (team)
        sprintf(teamName, "%07x", team->kasLabel);     // CAT: the label's hash
    else
        strcpy(teamName, "NULL");
    return teamName;
}

/*-----------------------------------------------------------------------------
    Name        : kasAddShipToTeam
    Description : utility function to add a ship to a team (creates team if doesn't exist)
    Inputs      : ship, str
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void kasAddShipToTeam(Ship *ship,udword label)
{
    sdword i;
    AITeam *teamp;

    // if labelled team doesn't currently exist, create it
    // otherwise, add ships to existing labelled team
    for (i = 0; i < aiCurrentAIPlayer->teamsUsed; i++)
    {
        teamp = aiCurrentAIPlayer->teams[i];
        if (teamp->teamType == ScriptTeam && teamp->kasLabel == label)      // CAT: by hash (kasAddShipToTeam 0x00491120)
            break;
    }
    if (i >= aiCurrentAIPlayer->teamsUsed)
    {
        teamp = aitCreate(ScriptTeam);
        teamp->kasLabel = label;
        teamp->kasOrigShipsCount = 0;
    }

    teamp->kasOrigShipsCount++;
    teamp->kasOrigShipsType = ship->shiptype;

    aitAddShip(teamp, ship);
    teamp->newships--;
}

//
//  remove ship reference from any ship lists
//
void kasShipDied(Ship *ship)
{
    sdword i = 0;

    while (i < SelectionsUsed)
    {
        clRemoveShipFromSelection(Selections[i]->shipList.selection, ship);
        ++i;
    }
}

//
//  CAT: cat_kasShipRemovedFromSelections 0x004911e0: a byte-identical, unfolded twin of kasShipDied with other callers:
//      the deleted resources (univDeleteDeadResources 0x00549270, univReallyDeleteThisResourceRightNow 0x005490d0 and
//      univUpdate 0x0054c3f0, HW1's DeleteDeadResource inlined three times), the mimic's fake asteroid
//      (cat_univDeleteMimicResource 0x00548f40) and the dead missiles (DeleteDeadMissile 0x00549880) leave every KAS
//      selection too. Name chosen (docs/mission-code-map.md 3).
//
void cat_kasShipRemovedFromSelections(void *obj)
{
    sdword i = 0;

    while (i < SelectionsUsed)
    {
        clRemoveShipFromSelection(Selections[i]->shipList.selection, (Ship *)obj);
        ++i;
    }
}

//
//  wipe the labelled object tables clean
//  (do this before reading in a new mission layout)
//
void kasLabelsInit(void)
{
    LabelledPathsUsed = 0;
    LabelledVectorsUsed = 0;
    LabelledVolumesUsed = 0;

    //hyperspace reset
    hsStaticReset();
}

//////////////////////////////////////////////////////////////////
//Encase all this debug stuff in this ifdef

//
//  this macro takes care of column wrapping
//  for the debug display
//
#define COLUMN_CHECK \
if (y > MAIN_WindowHeight - 2*rowHeight)    \
{                                         \
    y = rowHeight;                        \
    x += MAIN_WindowWidth/2 - rowHeight;  \
}


#if SP_DEBUGKEYS

/*-----------------------------------------------------------------------------
    Name        : kasTakeADump
    Description : Dumps all the singleplayer information into a file
    Inputs      : None
    Outputs     : A brand new file!
    Return      : void
----------------------------------------------------------------------------*/
void kasTakeADump(void)
{
    FILE *fp;
    char fileName[256] = "StatusDump.txt", *fullName;
    udword i, remaining;
    Timer *tp;

    fullName = filePathPrepend(fileName, FF_UserSettingsPath);

    if (!fileMakeDestinationDirectory(fullName))
        return;

    fp = fopen(fullName, "wt");
    if (!fp)
        return;

    fprintf(fp, "%s\n\n", CurrentMissionName);

    //printout states
    fprintf(fp, "STATES:\n");
    for (i=0;i<aiCurrentAIPlayer->teamsUsed;i++)
    {
        CurrentTeamP = aiCurrentAIPlayer->teams[i];

        if (CurrentTeamP->teamType == ScriptTeam)
        {
            fprintf(fp, "%07x {%d} : %07x %07x\n",          // CAT: the KAS names are hashes
                    CurrentTeamP->kasLabel,
                    CurrentTeamP->shipList.selection->numShips,
                    CurrentTeamP->kasFSMName,
                    CurrentTeamP->kasStateName);
        }
    }

    //printout variables
    fprintf(fp, "\nVARIABLES:\n");
    for (i = 0; i < varsUsed; ++i)
    {
        if (vars[i]->label[0] != '_' && vars[i]->label[1] != 'V')  // skip non KAS vars
        {
            fprintf(fp, "%s = %d\n", vars[i]->label, vars[i]->value);
        }
    }

    //printout timers
    fprintf(fp, "\nTIMERS:\n");
    for (i=0;i<timersUsed;i++)
    {
        tp = &timers[i];
        remaining = (sdword)(tp->duration - (universe.totaltimeelapsed - tp->startTime));
        if (remaining <= 0)
        {
            fprintf(fp, "%07x.%07x EXPIRED\n", tp->scopeHash, tp->nameHash);  // CAT: hashes
        }
        else
        {
            fprintf(fp, "%07x.%07x %s : %ds\n", tp->scopeHash, tp->nameHash, tp->enabled ? "ON" : "OFF", remaining);
        }
    }
    fclose(fp);
    clCommandMessage("Taking a Dump", 0xffffffff);
}

/*-----------------------------------------------------------------------------
    Name        : kasCountDisplayLines
    Description : Counts the number of lines the debug draw routine will be drawing
    Inputs      : none
    Outputs     : none
    Return      : Returns the number of lines to draw
----------------------------------------------------------------------------*/
udword kasCountDisplayLines(udword *states, udword *variables)
{
    udword display = 0, i=0;

    *states    = 0;
    *variables = 0;

    //count the number of states to draw
    if (mrKASDebugDrawStates)
    {
        for (i=0; i<aiCurrentAIPlayer->teamsUsed;i++)
        {
            if (aiCurrentAIPlayer->teams[i]->teamType == ScriptTeam)
            {
                display++;
                (*states)++;
            }
        }
    }
    //count the number of Variables to draw
    if (mrKASDebugDrawVars)
    {
        for (i=0; i<varsUsed;i++)
        {
            if (vars[i]->label[0] != '_' && vars[i]->label[1] != 'V')
            {
                display++;
                (*variables)++;
            }
        }
    }
    //count the number of Timers to draw
    if (mrKASDebugDrawTimers)
    {
        display += timersUsed;
    }
    return display;
}

#define KAS_NUM_SCREEN_LINES 80
void kasDebugDraw(void)
{
    sdword rowHeight, y, x, i;
//    fonthandle fhSave;
    AITeam *CurrentTeamP;
    sdword remaining;
    Timer *tp;
    char buf[256];
    vector vec0, vec1;
    Volume *volumep;
    udword numlines = 0, numstates = 0, numvars = 0, skiplines = 0;

    if (!gameIsRunning)
        return;

    if (!CurrentMissionName[0] || !CurrentMissionWatchFunction)
        return;

    // set up textual stuff
//    fhSave = fontCurrentGet();                  //save the current font
//    fontMakeCurrent(selGroupFont2);  // use a common, fairly small font

    rowHeight = fontHeight("M")-2; // used to space the text
    y = rowHeight;
    x = (sdword)rowHeight;

#if SP_DEBUGLEVEL2
    //count the number of lines to display
    numlines  = kasCountDisplayLines(&numstates, &numvars);
    skiplines = mrKASSkipFactor*KAS_NUM_SCREEN_LINES;

    // if we're displaying less than the screen can hold,
    // or if the user has paged over
    if ((numlines < KAS_NUM_SCREEN_LINES) ||
        (numlines < skiplines))
    {
        skiplines       = 0;
        mrKASSkipFactor = 0;
    }

    // states
    if (mrKASDebugDrawStates)
    {
        // mission name
        fontPrint(x-rowHeight/2, y + (rowHeight>>2), KAS_DEBUG_TEXTCOLOR, CurrentMissionName);
        y += rowHeight + 1;
        COLUMN_CHECK

        // each team's state
        for (i = 0; i < aiCurrentAIPlayer->teamsUsed; i++)
        {
            CurrentTeamP = aiCurrentAIPlayer->teams[i];
            if (CurrentTeamP->teamType == ScriptTeam)
            {
                if (skiplines)
                {
                    skiplines--;
                }
                else
                {
                    sprintf(buf, "%07x {%d} : %07x %07x",       // CAT: the KAS names are hashes
                            CurrentTeamP->kasLabel,
                            CurrentTeamP->shipList.selection->numShips,
                            CurrentTeamP->kasFSMName,
                            CurrentTeamP->kasStateName);
                    fontPrint(x, y + (rowHeight>>2), KAS_DEBUG_TEXTCOLOR, buf);
                    y += rowHeight + 1;
                    COLUMN_CHECK
                }
            }
        }
        y += rowHeight/3;
        COLUMN_CHECK
    }

    // vars
    if (mrKASDebugDrawVars)
    {
        sprintf(buf,"Enemy RU = %d",universe.players[1].resourceUnits);
        fontPrint(x, y + (rowHeight>>2), KAS_DEBUG_TEXTCOLOR, buf);
        y += rowHeight + 1;
        COLUMN_CHECK

        for (i = 0; i < varsUsed; ++i)
        {
            if (vars[i]->label[0] != '_' && vars[i]->label[1] != 'V')  // skip non KAS vars
            {
                if (skiplines)
                {
                    skiplines--;
                }
                else
                {
                    sprintf(buf, "%s = %d", vars[i]->label, vars[i]->value);
                    fontPrint(x, y + (rowHeight>>2), KAS_DEBUG_TEXTCOLOR, buf);
                    y += rowHeight + 1;
                    COLUMN_CHECK
                }
            }
        }
        y += rowHeight/3;
        COLUMN_CHECK
    }

    // timers
    if (mrKASDebugDrawTimers)
    {
        for (i = 0; i < timersUsed; ++i)
        {
            if (skiplines)
            {
                skiplines--;
            }
            else
            {
                tp = &timers[i];
                remaining = (sdword)(tp->duration - (universe.totaltimeelapsed - tp->startTime));
                if (remaining <= 0)
                    sprintf(buf, "%07x.%07x EXPIRED", tp->scopeHash, tp->nameHash);    // CAT: hashes
                else
                    sprintf(buf, "%07x.%07x %s : %ds", tp->scopeHash, tp->nameHash, tp->enabled ? "ON" : "OFF", remaining);
                fontPrint(x, y + (rowHeight>>2), KAS_DEBUG_TEXTCOLOR, buf);
                y += rowHeight + 1;
                COLUMN_CHECK
            }
        }
        y += rowHeight/3;
        COLUMN_CHECK
    }

    // volumes
    if (mrKASDebugDrawVolumes)
    {
        primModeClear2();
        rndLightingEnable(FALSE);
        rndTextureEnable(FALSE);
        glDisable(GL_DEPTH_TEST);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        rgluPerspective(mrCamera->fieldofview, rndAspectRatio, mrCamera->clipPlaneNear, mrCamera->clipPlaneFar * 15.0f);
        glMatrixMode(GL_MODELVIEW);

        #if KAS_DEBUG
        {
            Ship *m0p = aiuFindEnemyMothership(&universe.players[0]),
                 *m1p = aiuFindEnemyMothership(&universe.players[1]);
            primCircleOutline3(&m0p->obj.posinfo.position,
                               10000,
                               32, 4, KAS_DEBUG_TEXTCOLOR, X_AXIS);
            primCircleOutline3(&m0p->obj.posinfo.position,
                               10000,
                               32, 4, KAS_DEBUG_TEXTCOLOR, Y_AXIS);
            primCircleOutline3(&m0p->obj.posinfo.position,
                               10000,
                               32, 8, KAS_DEBUG_TEXTCOLOR, Z_AXIS);
            primCircleOutline3(&m1p->obj.posinfo.position,
                               10000,
                               32, 0, KAS_DEBUG_TEXTCOLOR, X_AXIS);
            primCircleOutline3(&m1p->obj.posinfo.position,
                               10000,
                               32, 0, KAS_DEBUG_TEXTCOLOR, Y_AXIS);
            primCircleOutline3(&m1p->obj.posinfo.position,
                               10000,
                               32, 8, KAS_DEBUG_TEXTCOLOR, Z_AXIS);

        }
        #endif

        for (i = 0; i < LabelledVolumesUsed; ++i)
        {
            volumep = LabelledVolumes[i]->volume;
            switch (volumep->type)
            {
                case VOLUME_AA_BOX:
                    vec0.x = volumep->attribs.aaBox.x0;  vec1.x = volumep->attribs.aaBox.x1;
                    vec0.y = volumep->attribs.aaBox.y0;  vec1.y = volumep->attribs.aaBox.y0;
                    vec0.z = volumep->attribs.aaBox.z0;  vec1.z = volumep->attribs.aaBox.z0;
                    primLine3(&vec0, &vec1, KAS_DEBUG_TEXTCOLOR);
                    vec0.x = volumep->attribs.aaBox.x1;
                    vec0.y = volumep->attribs.aaBox.y1;
                    vec0.z = volumep->attribs.aaBox.z0;
                    primLine3(&vec1, &vec0, KAS_DEBUG_TEXTCOLOR);
                    vec1.x = volumep->attribs.aaBox.x0;
                    vec1.y = volumep->attribs.aaBox.y1;
                    vec1.z = volumep->attribs.aaBox.z0;
                    primLine3(&vec0, &vec1, KAS_DEBUG_TEXTCOLOR);
                    vec0.x = volumep->attribs.aaBox.x0;
                    vec0.y = volumep->attribs.aaBox.y0;
                    vec0.z = volumep->attribs.aaBox.z0;
                    primLine3(&vec1, &vec0, KAS_DEBUG_TEXTCOLOR);
                    vec1.x = volumep->attribs.aaBox.x0;
                    vec1.y = volumep->attribs.aaBox.y0;
                    vec1.z = volumep->attribs.aaBox.z1;
                    primLine3(&vec0, &vec1, KAS_DEBUG_TEXTCOLOR);
                    vec0.x = volumep->attribs.aaBox.x1;
                    vec0.y = volumep->attribs.aaBox.y0;
                    vec0.z = volumep->attribs.aaBox.z1;
                    primLine3(&vec1, &vec0, KAS_DEBUG_TEXTCOLOR);
                    vec1.x = volumep->attribs.aaBox.x1;
                    vec1.y = volumep->attribs.aaBox.y1;
                    vec1.z = volumep->attribs.aaBox.z1;
                    primLine3(&vec0, &vec1, KAS_DEBUG_TEXTCOLOR);
                    vec0.x = volumep->attribs.aaBox.x0;
                    vec0.y = volumep->attribs.aaBox.y1;
                    vec0.z = volumep->attribs.aaBox.z1;
                    primLine3(&vec1, &vec0, KAS_DEBUG_TEXTCOLOR);
                    vec1.x = volumep->attribs.aaBox.x0;
                    vec1.y = volumep->attribs.aaBox.y0;
                    vec1.z = volumep->attribs.aaBox.z1;
                    primLine3(&vec0, &vec1, KAS_DEBUG_TEXTCOLOR);
                    vec0.x = volumep->attribs.aaBox.x0;  vec1.x = volumep->attribs.aaBox.x0;
                    vec0.y = volumep->attribs.aaBox.y1;  vec1.y = volumep->attribs.aaBox.y1;
                    vec0.z = volumep->attribs.aaBox.z0;  vec1.z = volumep->attribs.aaBox.z1;
                    primLine3(&vec0, &vec1, KAS_DEBUG_TEXTCOLOR);
                    vec0.x = volumep->attribs.aaBox.x1;  vec1.x = volumep->attribs.aaBox.x1;
                    vec0.y = volumep->attribs.aaBox.y1;  vec1.y = volumep->attribs.aaBox.y1;
                    vec0.z = volumep->attribs.aaBox.z0;  vec1.z = volumep->attribs.aaBox.z1;
                    primLine3(&vec0, &vec1, KAS_DEBUG_TEXTCOLOR);
                    vec0.x = volumep->attribs.aaBox.x1;  vec1.x = volumep->attribs.aaBox.x1;
                    vec0.y = volumep->attribs.aaBox.y0;  vec1.y = volumep->attribs.aaBox.y0;
                    vec0.z = volumep->attribs.aaBox.z0;  vec1.z = volumep->attribs.aaBox.z1;
                    primLine3(&vec0, &vec1, KAS_DEBUG_TEXTCOLOR);
                    break;

                case VOLUME_SPHERE:
                    primCircleOutline3(&volumep->attribs.sphere.center,
                                       volumep->attribs.sphere.radius,
                                       32, 4, KAS_DEBUG_TEXTCOLOR, X_AXIS);
                    primCircleOutline3(&volumep->attribs.sphere.center,
                                       volumep->attribs.sphere.radius,
                                       32, 4, KAS_DEBUG_TEXTCOLOR, Y_AXIS);
                    primCircleOutline3(&volumep->attribs.sphere.center,
                                       volumep->attribs.sphere.radius,
                                       32, 4, KAS_DEBUG_TEXTCOLOR, Z_AXIS);
                    break;
            }
        }

        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf((GLfloat*)&rndProjectionMatrix);
        glMatrixMode(GL_MODELVIEW);
        primModeSet2();
    }
#endif //SP_DEBUGLEVEL2

//    fontMakeCurrent(fhSave);
}
#endif //SP_DEBUGKEYS

/*=============================================================================
    Save Game Stuff
=============================================================================*/

/*-----------------------------------------------------------------------------
    CAT (M2-R8 singleplayer-tutorial): a KAS label entry. A cataclysm.exe save holds the ILP32 struct (kasLoad
    0x00491450: 0xC-byte selections, 8-byte path / vector / volume entries), converted by the save loader's tables
    (SaveCat32Layouts.c); this tree's own saves hold the native struct
----------------------------------------------------------------------------*/
static void *kasLoadEntry(sdword size, const void *layout32)
{
    SaveChunk *chunk;
    void *entry;

    chunk = LoadNextChunk();
    if (saveCat32Loading)
    {
        chunk = saveCat32ChunkStruct(chunk, layout32);
    }
    VerifyChunk(chunk, BASIC_STRUCTURE, size);
    entry = memAlloc(size, "loadedstructure", 0);
    memcpy(entry, chunkContents(chunk), size);
    memFree(chunk);
    return entry;
}

void SaveKasSelection(KasSelection *kasselection)
{
    SaveStructureOfSize(kasselection,sizeof(KasSelection));
    SaveGrowSelection(&kasselection->shipList);
}

KasSelection *LoadKasSelection(void)
{
    KasSelection *kasselection = kasLoadEntry(sizeof(KasSelection), SAVECAT32_LAYOUT(KasSelection));
    LoadGrowSelectionAndFix(&kasselection->shipList);
    return kasselection;
}

void SaveLabelledPath(LabelledPath *labelledpath)
{
    SaveStructureOfSize(labelledpath,sizeof(LabelledPath));
    SavePath(labelledpath->path);
}

LabelledPath *LoadLabelledPath(void)
{
    LabelledPath *labelledpath = kasLoadEntry(sizeof(LabelledPath), SAVECAT32_LAYOUT(LabelledPath));
    labelledpath->path = LoadPath();
    return labelledpath;
}

void SaveLabelledVector(LabelledVector *labelledvector)
{
    SaveStructureOfSize(labelledvector,sizeof(LabelledVector));
    SaveStructureOfSize(labelledvector->hvector,sizeof(hvector));
}

LabelledVector *LoadLabelledVector(void)
{
    LabelledVector *labelledvector = kasLoadEntry(sizeof(LabelledVector), SAVECAT32_LAYOUT(LabelledVector));
    labelledvector->hvector = LoadStructureOfSize(sizeof(hvector));
    return labelledvector;
}

void SaveLabelledVolume(LabelledVolume *labelledvolume)
{
    SaveStructureOfSize(labelledvolume,sizeof(LabelledVolume));
    SaveStructureOfSize(labelledvolume->volume,sizeof(Volume));
}

LabelledVolume *LoadLabelledVolume(void)
{
    LabelledVolume *labelledvolume = kasLoadEntry(sizeof(LabelledVolume), SAVECAT32_LAYOUT(LabelledVolume));
    labelledvolume->volume = LoadStructureOfSize(sizeof(Volume));
    return labelledvolume;
}

// CAT: the ship types whose buildability a single-player save keeps (kasSave 0x00491220 / kasLoad 0x00491450, in order)
#define CAT_KAS_SAVED_BUILDABLE_COUNT 6
static const ShipType catKasSavedBuildable[CAT_KAS_SAVED_BUILDABLE_COUNT] =
{
    sMothershipArmour, sMothershipDockingBay, sMothershipMicro, sMothershipSpecial, sMothershipWeapons, dAcolyte
};

void kasSave(void)
{
    sdword i;

    // hyperspace save
    SaveHyperspaceGates();

    // Save Global Variables

    SaveInfoNumber(CurrentMissionScope);

    SaveInfoNumber(CurrentMissionScopeName);                   // CAT: the hash (kasSave 0x00491220)
    SaveStructureOfSize(CurrentMissionName,sizeof(CurrentMissionName));

    SaveInfoNumber(WatchFunctionToMissionEnum(CurrentMissionWatchFunction));

    //this is a fix for a weird bug where after starting the game over and over
    //Current
    SaveInfoNumber(AITeamToTeamIndex(CurrentTeamP));

    // CAT: the single-player difficulty, where HW1 saved the skill level (kasSave 0x00491220 writes 0x008941a0)
    SaveInfoNumber(catSpDifficultySetting);

    // Save Selections

    SaveInfoNumber(SelectionsUsed);
    SaveInfoNumber(SelectionsAllocated);

    for (i=0;i<SelectionsUsed;i++)
    {
        SaveKasSelection(Selections[i]);
    }

    // Save LabelledPaths

    SaveInfoNumber(LabelledPathsUsed);
    SaveInfoNumber(LabelledPathsAllocated);

    for (i=0;i<LabelledPathsUsed;i++)
    {
        SaveLabelledPath(LabelledPaths[i]);
    }

    // Save LabelledVectors

    SaveInfoNumber(LabelledVectorsUsed);
    SaveInfoNumber(LabelledVectorsAllocated);

    for (i=0;i<LabelledVectorsUsed;i++)
    {
        SaveLabelledVector(LabelledVectors[i]);
    }

    // Save LabelledVolumes

    SaveInfoNumber(LabelledVolumesUsed);
    SaveInfoNumber(LabelledVolumesAllocated);

    for (i=0;i<LabelledVolumesUsed;i++)
    {
        SaveLabelledVolume(LabelledVolumes[i]);
    }

    // Save KAS Timers

    timTimerSave();

    // CAT: a single-player save ends with whether the player can build the five mothership modules and the dAcolyte
    //      (kasSave 0x00491220: rmCanBuildShip(player 0, type, FALSE) for 0x15, 0x18, 0x16, 0x13, 0x17, 0x85)
    if (singlePlayerGame)
    {
        for (i = 0; i < CAT_KAS_SAVED_BUILDABLE_COUNT; i++)
        {
            SaveInfoNumber(rmCanBuildShip(&universe.players[0], catKasSavedBuildable[i], FALSE));
        }
    }
}

void kasLoad(void)
{
    sdword i;

    // hyperspace load
    LoadHyperspaceGates();

    // Load Global Variables

    // CAT: the scope, its hash, the name and the watch function's mission index are read back as saved (kasLoad
    //      0x00491450; GoK threw the scope away and took the watch function from the current mission)
    CurrentMissionScope = LoadInfoNumber();

    CurrentMissionScopeName = (udword)LoadInfoNumber();         // CAT: the hash (kasLoad 0x00491450)
    LoadStructureOfSizeToAddress(CurrentMissionName,      sizeof(CurrentMissionName)     );

    CurrentMissionWatchFunction = MissionEnumToWatchFunction((MissionEnum)LoadInfoNumber());


    dbgAssertOrIgnore(universe.players[1].aiPlayer);
    CurrentTeamP = AITeamIndexToTeam(universe.players[1].aiPlayer,LoadInfoNumber());

    // CAT: the saved difficulty (kasLoad 0x00491450): the modifiers follow the current difficulty and a loaded
    //      Somtaaw ship's health is scaled by the current health factor over the saved one's (0x008941a4, FixShip)
    {
        real32 savedHealthFactor = 1.0f;    // CAT-FIX: the exe divides by an uninitialised local for a bad difficulty

        cat_spGetDifficultyModifiers(&savedHealthFactor, NULL, NULL, LoadInfoNumber());
        cat_spGetDifficultyModifiers(&spDifficultyHealthModifier, &catCmSinglePlayerBuildFactor,
                                     &catCmSinglePlayerCostFactor, catSpDifficultySetting);
        spDifficultyHealthLoadFactor = spDifficultyHealthModifier / savedHealthFactor;
    }

    // Load Selections

    SelectionsUsed = LoadInfoNumber();
    SelectionsAllocated = LoadInfoNumber();

    if (SelectionsAllocated > 0)
    {
        Selections = memAlloc(sizeof(KasSelection *)*SelectionsAllocated, "kasSelections", 0);
    }
    else
    {
        Selections = NULL;
    }

    for (i=0;i<SelectionsUsed;i++)
    {
        Selections[i] = LoadKasSelection();
    }

    // Load LabelledPaths

    LabelledPathsUsed = LoadInfoNumber();
    LabelledPathsAllocated = LoadInfoNumber();

    if (LabelledPathsAllocated > 0)
    {
        LabelledPaths = memAlloc(sizeof(LabelledPath *)*LabelledPathsAllocated, "kasLabelledPaths", 0);
    }
    else
    {
        LabelledPaths = NULL;
    }

    for (i=0;i<LabelledPathsUsed;i++)
    {
        LabelledPaths[i] = LoadLabelledPath();
    }

    // Load LabelledVectors

    LabelledVectorsUsed = LoadInfoNumber();
    LabelledVectorsAllocated = LoadInfoNumber();

    if (LabelledVectorsAllocated > 0)
    {
        LabelledVectors = memAlloc(sizeof(LabelledVector *)*LabelledVectorsAllocated, "kasLabelledVectors", 0);
    }
    else
    {
        LabelledVectors = NULL;
    }

    for (i=0;i<LabelledVectorsUsed;i++)
    {
        LabelledVectors[i] = LoadLabelledVector();
    }

    // Load LabelledVolumes

    LabelledVolumesUsed = LoadInfoNumber();
    LabelledVolumesAllocated = LoadInfoNumber();

    if (LabelledVolumesAllocated > 0)
    {
        LabelledVolumes = memAlloc(sizeof(LabelledVolume *)*LabelledVolumesAllocated, "kasLabelledVolumes", 0);
    }
    else
    {
        LabelledVolumes = NULL;
    }

    for (i=0;i<LabelledVolumesUsed;i++)
    {
        LabelledVolumes[i] = LoadLabelledVolume();
    }

    // Load KAS Timers

    timTimerLoad();

    // CAT: the scripts learn that the game was loaded: G_RecentlyLoaded = 1 (kasLoad 0x00491450 hashes the literal
    //      0x008820a8 inline and finds or appends the global variable)
    cat_kasVarFindOrCreate(0, cat_kasHashString("G_RecentlyLoaded"))->value = 1;

    // CAT: and the player's buildable modules (kasSave above; rmEnableShip(race 0, type, saved))
    if (singlePlayerGame)
    {
        for (i = 0; i < CAT_KAS_SAVED_BUILDABLE_COUNT; i++)
        {
            rmEnableShip((ShipRace)0, catKasSavedBuildable[i], LoadInfoNumber());
        }
    }
}

/*-----------------------------------------------------------------------------
    CAT (M2-R8 kas-runtime-mission01): kasConvertFuncPtrToOffset 0x00491800 / kasConvertOffsetToFuncPtr 0x00491830: a
        saved KAS function is its cataclysm.exe address minus the current mission's Watch function's
        (MissionEnumToWatchFunction(currentMission - 1)), -1 for none (docs/mission-code-map.md 4.10). The generated
        scripts carry each function's exe address (src/Missions, CatMissions.h), so this tree writes the exe's numbers
        and reads the original game's saves' KAS functions (an ICF-shared body resolves to any of its contexts)
-----------------------------------------------------------------------------*/
sdword kasConvertFuncPtrToOffset(void *func)
{
    const CatKasFunction *list;
    udword count, base, watchExe, i;
    sdword script;

    if (func == NULL)
    {
        return -1;
    }
    // [audit: M2-R8 kas-runtime-mission01] the base is the current mission's Watch (the exe's), the function is looked
    //      up in every generated script's list: the exe subtracts any function pointer, so a Tutorial1 function saved
    //      during the tutorial (currentMission 1, singlePlayerInit 0x00518cb1; Tutorial1's own kasfSaveLevel at
    //      0x0066a16d) is an offset from Watch_Mission01; looking only in Mission01's list wrote -1 for every one
    (void)cat_spKasScriptFunctions((sdword)singlePlayerGameInfo.currentMission - 1, &count, &base);
    for (script = 0; script < CAT_SP_KAS_SCRIPTS; script++)
    {
        list = cat_spKasScriptFunctions(script, &count, &watchExe);
        for (i = 0; i < count; i++)
        {
            if (list[i].function == func)
            {
                return (sdword)(list[i].exeAddress - base);
            }
        }
    }
    return -1;      // CAT-FIX: the exe subtracts any pointer; a function of no generated script has no offset
}

void *kasConvertOffsetToFuncPtr(sdword offset)
{
    const CatKasFunction *list;
    udword count, base, watchExe, i;
    sdword script;

    if (offset == -1)
    {
        return NULL;
    }
    (void)cat_spKasScriptFunctions((sdword)singlePlayerGameInfo.currentMission - 1, &count, &base);
    for (script = 0; script < CAT_SP_KAS_SCRIPTS; script++)
    {
        list = cat_spKasScriptFunctions(script, &count, &watchExe);
        for (i = 0; i < count; i++)
        {
            if (list[i].exeAddress == base + (udword)offset)
            {
                return list[i].function;
            }
        }
    }
    return NULL;
}
