
/*=============================================================================
    Name    : options.h
    Purpose : Definitons for the options

    Created  11/05/1998 by yo
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/


#ifndef ___OPTIONS_H
#define ___OPTIONS_H

#include "Key.h"
#include "Region.h"
#include "Types.h"

/*=============================================================================
    Definitions:
=============================================================================*/
#define OP_KEYMAPSIZE 80
    //4 chars needed per key

#define OP_TroubleShootPage "http://www.sierrastudios.com/games/homeworld/noflash/c-support.html"

#define OP_ResolutionFont "Arial_12.hff"
#define OP_RenderFont "Arial_12.hff"

#define OP_ResolutionSelectedColor  colRGB(255,255,105)
#define OP_ResolutionColor          colRGB(240,240,240)

#define OP_RenderSelectedColor      colRGB(255,255,105)
#define OP_RenderColor              colRGB(240,240,240)

#define OP_DimRenderColor     colRGB(192,192,192)
#define OP_RenderColorOutline colRGB(120,70,0)

extern uword opKeyDetour;
extern char opKeymap[OP_KEYMAPSIZE];

extern bool32 opReloading;
extern sdword opNoPalMB;
extern sdword opMusicVol;
extern sdword opSFXVol;
extern sdword opSpeechVol;
extern sdword opSpeakerSe;
extern sdword opEqualizer;
extern sdword opSpeakerSetting;
extern sdword opVoice0On;
extern sdword opVoice1On;
extern sdword opVoice2On;
extern sdword opVoiceComm;
extern sdword opVoiceStat;
extern sdword opVoiceChat;
extern sdword opMouseSens;
extern sdword opInfoOverlayVar;
extern sdword opBattleChatter;
extern sdword opEffectsVal;
extern sdword opBrightnessVal;
extern udword opDetailThresholdVal;
extern bool32 opCustomEffectsToggled;
extern sdword opNoLODVal;
extern sdword opNumChannels;
extern sdword opAutoChannel;
extern sdword opSoundQuality;

extern bool32   opTimerActive;
extern real32 opTimerStart;
extern real32 opTimerLength;

// previously in LilOptions.h
extern udword opDeviceCRC;
extern sdword opDeviceIndex;
extern sdword opCPUDiff;
extern sdword opCPUAttack;

// Cataclysm.cfg configurable values (CAT: the exe's options file; HW1 Homeworld.cfg)

// CAT (M2-R6 frontend): Cataclysm's own option words and callbacks (Options.c)
struct tagfeatom;
extern sdword catOpTextureDetail;                   // 0x0088be74
extern sdword catOpGameFramerate;                   // 0x0088be98
extern sdword catOpHardwareTL;                      // 0x00a4cc18
extern sdword catOpTextureRequirementsSet;          // 0x00a76260
void cat_opLODDetail(char *name, struct tagfeatom *atom);
void cat_opSmallFonts(char *name, struct tagfeatom *atom);
void cat_opHWTaskbar(char *name, struct tagfeatom *atom);
void cat_opTaskbarButtons(char *name, struct tagfeatom *atom);
void cat_opTaskbarBackdrop(char *name, struct tagfeatom *atom);
void cat_opExperienceMessages(char *name, struct tagfeatom *atom);
void cat_opHomeworldCSA(char *name, struct tagfeatom *atom);
void cat_opRightClickCSA(char *name, struct tagfeatom *atom);
void cat_opAutoFocusOnWaypoints(char *name, struct tagfeatom *atom);
void cat_opAreaExplored(char *name, struct tagfeatom *atom);
void cat_opSensorsSphere(char *name, struct tagfeatom *atom);
void cat_opCompass(char *name, struct tagfeatom *atom);
void cat_opSensorsWireframe(char *name, struct tagfeatom *atom);
void cat_opResources(char *name, struct tagfeatom *atom);
void cat_opDust(char *name, struct tagfeatom *atom);
void cat_opBackground(char *name, struct tagfeatom *atom);
void cat_opObjectivePointers(char *name, struct tagfeatom *atom);
void cat_opSlipgatePaths(char *name, struct tagfeatom *atom);
void cat_opCapitalShipOverlay(char *name, struct tagfeatom *atom);
void cat_opHomeworldClassic(char *name, struct tagfeatom *atom);
void cat_opCataclysmDefault(char *name, struct tagfeatom *atom);
void cat_opTextureDetail(char *name, struct tagfeatom *atom);
void cat_opSpeechTest(char *name, struct tagfeatom *atom);
void cat_opSoundFXTest(char *name, struct tagfeatom *atom);
void cat_opSetAllDefaults(sdword lowEnd);
void cat_opCataclysmFramerate(char *name, struct tagfeatom *atom);
void catOptionsRunSelfTest(void);                  // HWI: port diagnostic, /catFeScreensTest's options round trip

extern udword opPauseOrders;
extern udword opShipRecoil;

void opOptionsSaveCustomEffectsSettings(void);


#define NUM_SMOOTHIES 32


struct SmoothieX;

typedef void (*smoothieFunc)(real32 data, struct SmoothieX *smoo);    //callback function

typedef struct SmoothieX
{
    real32 feedback;
    real32 buffer;
    real32 *source;
    real32 *dest;
    sdword *destint;
    real32 threshold;
    sdword *user;
    smoothieFunc callback;
}
Smoothie;



/*=============================================================================
    Function Prototypes:
=============================================================================*/

void opUpdateSettings(void);
char *opKeyToNiceString(keyindex key);
keyindex opKeyTranslate(keyindex key);
void opKeyAssign(uword func, keyindex key);
bool32 opValidKey(keyindex key);
void opDefineThisKey(keyindex key);
void opKeyboardLoad(void);
udword opMusicVolumeProcess(regionhandle reg, sdword ID, udword event, udword data);
udword opSFXVolumeProcess(regionhandle reg, sdword ID, udword event, udword data);
udword opSpeechVolumeProcess(regionhandle reg, sdword ID, udword event, udword data);
udword opNumChannelsProcess(regionhandle reg, sdword ID, udword event, udword data);

void opTimerExpired(void);

void InitSmoothie(Smoothie *smooothie,real32 feedback, real32 *source, real32 *dest,
                  sdword *destint, smoothieFunc callback, real32 threshold);
void AddSmoothie(Smoothie *smoo);
void RemoveSmoothie(Smoothie *smoo);

char ConvertSliderToAIPlayerDifficulty(sdword sliderval);

#endif
