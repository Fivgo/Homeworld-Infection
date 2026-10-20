//
//  SINGLE-PLAYER MISSION OBJECTIVES
//

#include <stdio.h>
#include <string.h>

#include "AIVar.h"
#include "Debug.h"
#include "font.h"
#include "FontReg.h"
#include "KASFunc.h"
#include "mainrgn.h"
#include "Memory.h"
#include "NIS.h"
#include "Objectives.h"
#include "render.h"
#include "SaveGame.h"
#include "SaveCat32.h"     // CAT (M2-R8 singleplayer-tutorial): a cataclysm.exe save's mission tail
#include "Sensors.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "StringSupport.h"
#include "Subtitle.h"
#include "InfoOverlay.h"     // CAT: the objective list lives in the info overlay (cat_ioObjective*)
#include "KASVar.h"
#include "Universe.h"

#ifdef _MSC_VER
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif


extern sdword popupTextNumLines;
extern char *getWord(char *dest, char *source); // defined in researchgui.c
extern regionhandle ghMainRegion;

// for objective status overlay
extern fonthandle selGroupFont2;
#define OBJECTIVE_TITLE_TEXTCOLOR      colRGB(255, 255, 255)
#define OBJECTIVE_TEXTCOLOR            colRGB(200, 200, 0)
#define OBJECTIVE_POPUPTEXT_COLOR      colRGB(0, 200, 200)
#define PO_FONTNAMESIZE                64
#define poPopupFont                    selGroupFont2

LinkedList poFleetIntelligence;
Objective **objectives = NULL;
sdword objectivesAllocated = 0, objectivesUsed = 0;
FleetIntelligence *poCurFleetIntelligence = NULL;

regionhandle poBaseRegion = NULL;

char poHeadingFontName[PO_FONTNAMESIZE]="blah blah blah blah blah blah";
char poTextFontName[PO_FONTNAMESIZE]="blah blah blah blah blah blah ";
fonthandle poHeadingFont = 0;
fonthandle poTextFont = 0;




// Forward reference for callback tables
void poClose(char *string, featom *atom);
void poTextDraw(featom *atom, regionhandle region);

fecallback poCallback[] =
{
    {poClose,           "PO_CloseObjectiveWindow"},
    {NULL, NULL}
};

fedrawcallback poDrawCallback[] =
{
    {poTextDraw,        "PO_TextDraw"},
    {NULL, NULL}
};


void fleetIntelligenceDestroy(FleetIntelligence *fleetIntelligence)
{
    memFree(fleetIntelligence->description);
    listDeleteNode(&fleetIntelligence->node);
}


FleetIntelligence *fleetIntelligenceCreate(char *description, bool8 showOnce)
{
    FleetIntelligence *fleetIntelligence;

    fleetIntelligence = memAlloc(sizeof(FleetIntelligence), "Fleet Intelligence", 0);
    fleetIntelligence->showOnce = showOnce;
    fleetIntelligence->showNow  = TRUE;
    fleetIntelligence->description = memStringDupe(description);
    listAddNode(&poFleetIntelligence, &fleetIntelligence->node, (void*) fleetIntelligence);

    // ASSUMING THAT THIS IS TRUE ONLY WHILE IN THE GAME ALREADY AND NOT LOADING
    if (showOnce)
    {
        poCurFleetIntelligence = fleetIntelligence;
        poPlayerObjectivesBegin(ghMainRegion);
        fleetIntelligence = NULL;
    }

    return fleetIntelligence;
}

Objective *objectiveAndFleetIntelligenceCreate(char *label, char *briefDescription, char* fullDescription, bool8 showOnce, bool32 primary)
{
    Objective *objective;
    bool32 hyperspace;

    if (objectiveFind(label))
    {
        return NULL;
    }

    // allocate the node
    objective = memAlloc(sizeof(Objective), "objective", 0);
    objective->status = FALSE;
    objective->primary = primary;
    memStrncpy(objective->label, label, MAX_OBJECTIVE_LABEL_LENGTH);
    objective->description = memStringDupe(briefDescription);

    if (!strcasecmp(label, "hyperspace"))
    {
        hyperspace = TRUE;
        singlePlayerGameInfo.playerCanHyperspace = TRUE;
        smUpdateHyperspaceStatus(TRUE);
        objective->fleetIntelligence = NULL;
    }
    else
    {
        hyperspace = FALSE;
        if (fullDescription != NULL)
        {
            objective->fleetIntelligence = fleetIntelligenceCreate(fullDescription, showOnce);
        }
        else
        {
            objective->fleetIntelligence = NULL;
        }
    }

    // keep track of all allocations
    if (!objectives)
    {
        objectives = memAlloc(sizeof(Objective*)*OBJECTIVES_ALLOC_INITIAL, "objectives", 0);
        objectivesAllocated = OBJECTIVES_ALLOC_INITIAL;
        objectivesUsed = 0;
    }
    if (objectivesUsed >= objectivesAllocated)
    {
        // allocate more if necessary
        objectives = memRealloc(objectives, sizeof(Objective*)*(objectivesAllocated + OBJECTIVES_ALLOC_INCREMENT), "objectives", 0);
        objectivesAllocated += OBJECTIVES_ALLOC_INCREMENT;
    }

    objectives[objectivesUsed++] = objective;

    // CAT: no task bar: the objective goes to the info overlay's list, the hyperspace one first
    //      (objectiveAndFleetIntelligenceCreate 0x004d6080: cat_ioObjectiveAddFirst 0x004ac600 / cat_ioObjectiveAdd
    //      0x004ac5e0)
    if (hyperspace)
    {
        cat_ioObjectiveAddFirst(objectives[objectivesUsed-1]);
    }
    else
    {
        cat_ioObjectiveAdd(objectives[objectivesUsed-1]);
    }

    return objective;
}


void fleetIntelligenceDestroyAll(void)
{
    FleetIntelligence *fleetIntelligence;
    udword numEntries;
    udword count;
    Node *node;

    numEntries = poFleetIntelligence.num;
    node = poFleetIntelligence.head;

    for (count = 0; count < numEntries; count++)
    {
        fleetIntelligence = (FleetIntelligence*) listGetStructOfNode(node);
        node = node->next;
        if (node != NULL && fleetIntelligence != NULL)
        {
            fleetIntelligenceDestroy(fleetIntelligence);
        }
        else
            break;
    }
}

void objectiveStartup(void)
{
    objectives = NULL;
    objectivesAllocated = 0;
    objectivesUsed = 0;

    feCallbackAddMultiple(poCallback); //add in the callbacks

    listInit(&poFleetIntelligence);
}

void objectiveShutdown(void)
{
    objectiveDestroyAll();
    //fleetIntelligenceDestroyAll();
}

//
//  Set status of an objective.
//  True == complete
//  False == not complete
//
void objectiveSet(char *label, sdword status)
{
    Objective *obj = objectiveFind(label);

    if (!obj)
        return;

    obj->status = status;
}

//
//  Get status of an objective.
//  True == complete
//  False == not complete
//
sdword objectiveGet(char *label)
{
    Objective *obj = objectiveFind(label);

    if (!obj)
        return FALSE;

    return obj->status;
}

//
//  Get status of all objectives.
//  True == all complete
//  False == not all complete
//
//  note that with no objectives, this will return TRUE
//
sdword objectiveGetAll(void)
{
    sdword i = 0;

    while (i < objectivesUsed)
    {
        if (!objectives[i]->status)
            return FALSE;
        ++i;
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_objectiveCountActive                          0x004d6370
    Description : CAT: the number of objectives whose status is set (KAS "ObjectivesGetAny", kasfObjectiveGetAny
                  0x00494f50, jumps here)
----------------------------------------------------------------------------*/
sdword cat_objectiveCountActive(void)
{
    sdword i, count = 0;

    for (i = 0; i < objectivesUsed; i++)
    {
        if (objectives[i]->status)
        {
            count++;
        }
    }
    return count;
}

//
//  remove one objective
//
void objectiveDestroy(char *label)
{
    sdword i = 0;
    Objective *obj = NULL;

    while (i < objectivesUsed)
    {
        if (!strncmp(objectives[i]->label, label, MAX_OBJECTIVE_LABEL_LENGTH))
        {
            obj = objectives[i];
            break;
        }
        ++i;
    }
    if (!obj)
        return;

    cat_ioObjectiveRemove(obj);     // CAT: the info overlay's list (objectiveDestroy 0x004d63a0: cat_ioObjectiveRemove 0x004ac650)

    if (obj->fleetIntelligence)
    {
       if (obj->fleetIntelligence == poCurFleetIntelligence)
       {
           poCurFleetIntelligence = NULL;
       }
       fleetIntelligenceDestroy(obj->fleetIntelligence);
    }

    if (obj->description)
       memFree(obj->description);
    memFree(obj);

    --objectivesUsed;

    if (i < objectivesUsed)
        memmove(&objectives[i], &objectives[i+1], sizeof(Objective*)*(objectivesUsed-i));
}

//
//  Remove all objectives from a mission.
//
void objectiveDestroyAll(void)
{
    sdword i = 0;
    Objective *obj;

    // ASSUMING THIS GETS CALLED AT THE END OF A MISSION
    cat_ioObjectivesClear();        // CAT: the info overlay's list (objectiveDestroyAll 0x004d6480: cat_ioObjectivesClear 0x004ac6a0)
    fleetIntelligenceDestroyAll();

    while (i < objectivesUsed)
    {
        obj = objectives[i];
        memFree(obj->description);
        memFree(obj);
        ++i;
    }
    objectivesUsed = 0;

    if (objectives)
        memFree(objectives);
    objectives = NULL;
    objectivesAllocated = 0;
}

//
//  finds an objective by label
//
//  returns NULL if not found
//
Objective *objectiveFind(char *label)
{
    sdword i = 0;

    while (i < objectivesUsed)
    {
        if (!strncmp(objectives[i]->label, label, MAX_OBJECTIVE_LABEL_LENGTH))
            return objectives[i];
        ++i;
    }
    return NULL;
}

//
//  this can be removed, NOW OBSOLETE (remove reference in MAINRGN.C)
//
void objectiveDrawStatus(void)
{
}

/*-----------------------------------------------------------------------------
    Name        : poPopupTextDraw
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void poPopupTextDraw(rectangle *rect)
{
    sdword x, y, width, rectWidth;
//    udword count;
    fonthandle currentFont;
    bool8 done, justified;
    char *pos, *oldpos;
    char oldline[256], line[256];

    // CAT: also drawn in the tutorial (poPopupTextDraw 0x004d6530 tests only the game and the NIS)
    if (!gameIsRunning || nisIsRunning)
        return;

    rectWidth = (rect->x1 - rect->x0);


    poHeadingFont = frFontRegister(poHeadingFontName);
    poTextFont = frFontRegister(poTextFontName);


    //currentFont = fontMakeCurrent(poPopupFont);
    currentFont = fontMakeCurrent(poTextFont);

    if (poCurFleetIntelligence != NULL)
    {
        x = rect->x0;
        y = rect->y0;

        dbgAssertOrIgnore(poCurFleetIntelligence->description != NULL);

        pos = poCurFleetIntelligence->description;

        done = FALSE;
        while (!done)
        {
            justified = FALSE;
            line[0]=0;
            while (!justified)
            {
                strcpy(oldline, line);
                oldpos = pos;
                pos = getWord(line, pos);

                if (pos[0] == '\n')
                {
                    justified = TRUE;
                    pos++;
                    while ( pos[0] == ' ' )
                        pos++;
                }
                else
                {
                    if ( (width=fontWidth(line)) > rectWidth)
                    {
                        strcpy(line, oldline);
                        pos = oldpos;
                        while ( pos[0] == ' ' )
                            pos++;

                        justified = TRUE;
                    }
                    if (pos[0]==0)
                    {
                        justified = TRUE;
                        done      = TRUE;
                    }
                }
            }

            fontPrintf(x,y,OBJECTIVE_POPUPTEXT_COLOR,"%s",line);
            y += fontHeight(" ");
            if (y > rect->y1 + fontHeight(" "))
                done=TRUE;
        }
    }
    fontMakeCurrent(currentFont);
}


/*-----------------------------------------------------------------------------
    Name        : poPlayerObjectivesBegin
    Description : startup the mission objectives window
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void poPlayerObjectivesBegin(regionhandle region)
{
    spMainScreen(TRUE);     // make sure we're on main screen

    // disbale rendering of main screen - NOT!
    //mrRenderMainScreen = FALSE;

    // CAT: no task bar to disable (inlined in objectiveAndFleetIntelligenceCreate 0x004d6080 and poPopupFleetIntelligence
    //      0x004d66f0)

    smSensorsDisable = TRUE;
    mrDisable();

    universePause = TRUE;

    feDrawCallbackAddMultiple(poDrawCallback);

    poBaseRegion = feScreenStart(region, "Single_Player_Objective");
}


/*-----------------------------------------------------------------------------
    Name        : poPopupFleetIntelligence
    Description : Starts the fleet intel event for a given objective
    Inputs      : objective - objective clicked on
    Outputs     :
    Return      :
    Note        : Creates a KAS global variable with the name "G_CLICK_<label>"
----------------------------------------------------------------------------*/
void poPopupFleetIntelligence(Objective* objective)
{
    char labelName[48] = "G_CLICK_";
    CatKasVar *var;

    // CAT: poPopupFleetIntelligence 0x004d66f0: in the tutorial a click sets the global KAS variable "G_CLICK_<label>"
    //      (scope 0, the hash of the whole name) to 1 and, if the objective has fleet intelligence text, shows it; an
    //      objective without text clears the intel-event flag (subMessageEnded); in a mission the objective's fleet
    //      intelligence is shown when it has one (HW1 set the AIVar "CLICK_<label>" in both)
    if (tutorial == TUTORIAL_ONLY)
    {
        strncat(labelName, objective->label, sizeof(labelName) - strlen(labelName) - 1);
        var = cat_kasVarFindOrCreate(0, cat_kasHashString(labelName));
        if (var != NULL)
        {
            var->value = 1;
        }
        if ((objective->fleetIntelligence->description == NULL) || (objective->fleetIntelligence->description[0] == 0))
        {
            subMessageEnded = FALSE;
            return;
        }
    }
    else if (objective->fleetIntelligence == NULL)
    {
        return;
    }
    poCurFleetIntelligence = objective->fleetIntelligence;
    poPlayerObjectivesBegin(ghMainRegion);
}

/*-----------------------------------------------------------------------------
    Name        : poTextDraw
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void poTextDraw(featom *atom, regionhandle region)
{
    poPopupTextDraw(&region->rect);
}

/*-----------------------------------------------------------------------------
    Name        : poClose
    Description : Callback function to close the mission objectives window
    Inputs      :
    Outputs     : Deletes all regions associated with mission objectives window
    Return      :
----------------------------------------------------------------------------*/
void poClose(char *string, featom *atom)
{                                                           //close the window
#if CM_VERBOSE_LEVEL >= 1
    dbgMessagef("Close mission objectives window.");
#endif
    feScreenDeleteFlags(poBaseRegion, FE_DONT_DELETE_REGION_IF_SCREEN_NOT_FOUND);
    poBaseRegion = NULL;

    // enable rendering of main game screen - NOT DISABLED!
    //mrRenderMainScreen = TRUE;

    // CAT: no task bar to enable (poClose 0x004d6810)

    smSensorsDisable = FALSE;
    mrEnable();

    universePause = FALSE;

    if (poCurFleetIntelligence != NULL)
    {
        if (poCurFleetIntelligence->showOnce)
            fleetIntelligenceDestroy(poCurFleetIntelligence);

        poCurFleetIntelligence = NULL;
    }

    // shut up the fleet intel speech
    speechEventFleetIntelStop(1.0f);
}

/*=============================================================================
    Save Game Stuff:
=============================================================================*/

void SaveObjective(Objective *objective)
{
    Objective *sc;
    SaveChunk *chunk;

    chunk = CreateChunk(BASIC_STRUCTURE,sizeof(Objective),objective);
    sc = chunkContents(chunk);

    sc->fleetIntelligence = (FleetIntelligence *)ConvertPointerInListToNum(&poFleetIntelligence,sc->fleetIntelligence);

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(Objective));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);

    Save_String(objective->description);
}

Objective *LoadObjective(void)
{
    SaveChunk *chunk;
    Objective *objective;

    chunk = LoadNextChunk();
    if (saveCat32Loading)
    {                                       // CAT (M2-R8 singleplayer-tutorial): the exe's 0x34-byte record (objectiveLoad 0x004d6960)
        chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(Objective));
    }
    VerifyChunk(chunk,BASIC_STRUCTURE,sizeof(Objective));

    objective = memAlloc(sizeof(Objective),"objective",0);
    memcpy(objective,chunkContents(chunk),sizeof(Objective));

    memFree(chunk);

    objective->fleetIntelligence = ConvertNumToPointerInList(&poFleetIntelligence,(sdword)objective->fleetIntelligence);

    objective->description = Load_String();

    return objective;
}

void SaveFleetIntelligence(void *stuff)
{
    SaveChunk *chunk = CreateChunk(BASIC_STRUCTURE,sizeof(FleetIntelligence),stuff);
    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(FleetIntelligence));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);
    Save_String(((FleetIntelligence *)stuff)->description);
}

void LoadFleetIntelligence(LinkedList *list)
{
    FleetIntelligence *fleetintelligence;
    SaveChunk *chunk = LoadNextChunk();

    if (saveCat32Loading)
    {                                       // CAT (M2-R8 singleplayer-tutorial): the exe's 0x18-byte record (0x004d68a0)
        chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(FleetIntelligence));
    }
    VerifyChunk(chunk, BASIC_STRUCTURE, sizeof(FleetIntelligence));
    fleetintelligence = memAlloc(sizeof(FleetIntelligence), "loadedstructure", 0);
    memcpy(fleetintelligence, chunkContents(chunk), sizeof(FleetIntelligence));
    memFree(chunk);
    fleetintelligence->description = Load_String();

    listAddNode(list,&fleetintelligence->node,fleetintelligence);
}

void objectiveSave(void)
{
    sdword i;

    SaveLinkedListOfStuff(&poFleetIntelligence,SaveFleetIntelligence);

    // Save objectives

    SaveInfoNumber(objectivesUsed);
    SaveInfoNumber(objectivesAllocated);

    for (i=0;i<objectivesUsed;i++)
    {
        SaveObjective(objectives[i]);
    }
}

void objectiveLoad(void)
{
    sdword i;
    Objective *hyperspaceObjective = NULL;

    poCurFleetIntelligence = NULL;

    LoadLinkedListOfStuff(&poFleetIntelligence,LoadFleetIntelligence);

    // Load objectives

    objectivesUsed = LoadInfoNumber();
    objectivesAllocated = LoadInfoNumber();

    if (objectivesAllocated > 0)
    {
        objectives = memAlloc(sizeof(Objective*)*objectivesAllocated, "objectives", 0);
    }
    else
    {
        objectives = NULL;
    }

    for (i=0;i<objectivesUsed;i++)
    {
        objectives[i] = LoadObjective();
        if (!strcasecmp(objectives[i]->label, "hyperspace"))
        {
            dbgAssertOrIgnore(hyperspaceObjective == NULL);
            hyperspaceObjective = objectives[i];
        }
        else
            cat_ioObjectiveAdd(objectives[i]);      // CAT: the info overlay's list (objectiveLoad 0x004d6960)
    }

    // CAT: the hyperspace objective is added last (objectiveLoad 0x004d6960: cat_ioObjectiveAdd after the loop; HW1
    //      put it at the task bar's head)
    if (hyperspaceObjective)
    {
        cat_ioObjectiveAdd(hyperspaceObjective);
    }

    feDrawCallbackAddMultiple(poDrawCallback);
}

