#ifndef ___KAS_H
#define ___KAS_H

#include "AIUtilities.h"
#include "ShipSelect.h"
#include "Vector.h"

// run-time scoping for variables, timers, etc.
enum {
    KAS_SCOPE_MISSION,
    KAS_SCOPE_FSM,
    KAS_SCOPE_STATE};

typedef void (*KASInitFunction) (void);
typedef void (*KASWatchFunction) (void);

//  these have corresponding #defines in kas2c.h
#define KAS_MISSION_NAME_MAX_LENGTH 47
#define KAS_FSM_NAME_MAX_LENGTH 47
#define KAS_STATE_NAME_MAX_LENGTH 47

// CAT (M2-R8 kasf library): every KAS label is its cat_kasHashString hash (KASVar.h): the kas2c missions pass the
//      hashes, the level loader hashes the .mission labels (scriptSetShipsCB 0x004a13f0, scriptSetAIPointCB ..
//      scriptSetAISphereCB), and the lookups compare the words (kasAITeamPtr 0x004907a0 .. kasGrowSelectionPtr
//      0x00490bb0); HW1 kept 48-byte label strings and compared them without case (the hash ignores case too)

//  user variables for SHIPS
#define KASSELECTION_ALLOC_INCREMENT 16
#define KASSELECTION_MAX_LABEL_LENGTH 47
typedef struct {
    udword label;               // CAT: the hash (0xC-byte entries in cataclysm.exe: kasSave saves 0xC then the selection)
    GrowSelection shipList;
} KasSelection;

//
//  labelled entities in mission layout file
//
#define KAS_MAX_LABEL_LENGTH 47
#define KAS_LABELLED_ENTITY_ALLOC_INCREMENT 16
//  team labels are stored in AITeam struct
typedef struct {
    udword label;               // CAT: the hash (8-byte entries in cataclysm.exe)
    Path *path;
} LabelledPath;
typedef struct {
    udword label;               // CAT: the hash
    hvector *hvector;
} LabelledVector;
typedef struct {
    udword label;               // CAT: the hash
    Volume *volume;
} LabelledVolume;
#if defined(__i386__)
_Static_assert(sizeof(KasSelection) == 0xC && sizeof(LabelledPath) == 8 && sizeof(LabelledVector) == 8 &&
               sizeof(LabelledVolume) == 8, "the KAS label entries are cataclysm.exe's sizes");
#endif


/*=============================================================================
    Functions
=============================================================================*/
//  to fill in table of labelled mission layout entities
void kasLabelsInit(void);
//void kasLabelledAITeamAdd(char *label);
Path *kasLabelledPathAdd(udword label, sdword numPoints, sdword closed);
hvector *kasLabelledVectorAdd(udword label, real32 x, real32 y, real32 z,real32 w);
Volume *kasLabelledVolumeAdd(udword label);

void kasMissionStart(char *name, KASInitFunction initFunction, KASWatchFunction watchFunction);
void kasExecute(void);

// keywords of KAS language
void kasJump(udword stateName, KASInitFunction initFunction, KASWatchFunction watchFunction);
void kasFSMCreate(udword fsmName, KASInitFunction initFunction, KASWatchFunction watchFunction, struct AITeam *team);

// labels: resolves references from script to mission layout file (CAT: by hash)
struct AITeam *kasAITeamPtr(udword label);
GrowSelection *kasAITeamShipsPtr(udword label);
hvector *kasShipsVectorPtr(udword label);
hvector *kasTeamsVectorPtr(udword label);
hvector *kasVolumeVectorPtr(udword label);
hvector *kasThisTeamsVectorPtr(void);
Path   *kasPathPtr(udword label);
Path *kasPathPtrNoErrorChecking(udword label);
Volume *kasVolumePtr(udword label);
hvector *kasVectorPtr(udword label);
hvector *kasVectorPtrIfExists(udword label);
GrowSelection *kasGrowSelectionPtr(udword label);

GrowSelection *kasGetGrowSelectionPtrIfExists(udword label);

void kasShipDied(Ship *ship);

void kasGrowSelectionClear(GrowSelection *ships);

void kasLabelledEntitiesDestroy(void);

void kasAddShipToTeam(Ship *ship,udword label);

#define KAS_TEAM_NAME_MAX_LENGTH 47
char *kasAITeamName(struct AITeam *team, char *teamName);

void kasTakeADump(void);
void kasDebugDraw(void);
#define KAS_DEBUG_TEXTCOLOR            colRGB(64, 255, 196)

void kasTeamDied(struct AITeam *team);

void kasClose(void);
void kasInit(void);

// Save Game Stuff
void kasSave(void);
void kasLoad(void);
void *kasConvertOffsetToFuncPtr(sdword offset);
sdword kasConvertFuncPtrToOffset(void *func);


#endif
