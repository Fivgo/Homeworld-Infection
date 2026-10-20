/*=============================================================================
    Name    : NIS.c
    Purpose : Code for animating NIS's

    Created 12/3/1997 by lmoloney
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "NIS.h"

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "Animatic.h"
#include "Attack.h"
#include "avi.h"
//#include "bink.h"
#include "BTG.h"
#include "Clamp.h"
#include "CommandLayer.h"
#include "CommandWrap.h"
#include "Debug.h"
#include "Eval.h"
#include "FastMath.h"
#include "FEFlow.h"
#include "File.h"
#include "FontReg.h"
#include "glinc.h"
#include "KAS.h"
#include "KASVar.h"
#include "Light.h"
#include "mainrgn.h"
#include "Memory.h"
#include "MEX.h"
#include "Mimic.h"
#include "mouse.h"
#include "Randy.h"
#include "Region.h"
#include "render.h"
#include "Select.h"
#include "Sensors.h"
#include "ShipDefs.h"
#include "SinglePlayer.h"
#include "Slipgate.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "StringSupport.h"
#include "Subtitle.h"
#include "Tracking.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "KASFunc.h"

#include "CatSort.h"         // CAT (M2-R9 oracle-rung-physics): the exe's qsort
#ifndef SW_Render
    #ifdef _WIN32
        #include <windows.h>
    #endif
#endif

#ifdef _MSC_VER
	#define strcasecmp _stricmp
	#define isnan _isnan
#else
    #include <strings.h>
#endif


/*=============================================================================
    Data:
=============================================================================*/
extern regionhandle ghMainRegion;
bool32 nisScreenStarted = FALSE;

bool32 nisIsRunning = FALSE;
bool32 nisCaptureCamera = FALSE;
bool32 nisEnabled = TRUE;
Camera *nisCamera = NULL;

#if NIS_TIME_CONTROLS
real32 nisPlayFactor = 1.0f;
#endif
bool32 nisPaused = FALSE;

#if NIS_PRINT_INFO
char nisInfoString[256] = "";
bool32 nisPrintInfo = FALSE;
bool32 nisNoLockout = FALSE;
#endif

//for scanning the script part of an NIS
nisevent *nisEventList = NULL;
sdword nisEventIndex;

void nisDeathAnything(nisplaying *NIS, nisevent *event);
void nisAttack(nisplaying *NIS, nisevent *event);
void nisHaltAttack(nisplaying *NIS, nisevent *event);
void nisFire(nisplaying *NIS, nisevent *event);
void nisObjectShow(nisplaying *NIS, nisevent *event);
void nisObjectHide(nisplaying *NIS, nisevent *event);
void nisInvincible(nisplaying *NIS, nisevent *event);
void nisVincible(nisplaying *NIS, nisevent *event);
void nisSoundEvent(nisplaying *NIS, nisevent *event);
void nisSpeechEvent(nisplaying *NIS, nisevent *event);
void nisFleetSpeechEvent(nisplaying *NIS, nisevent *event);
void nisAnimaticSpeechEvent(nisplaying *NIS, nisevent *event);
void nisRemainAtEnd(nisplaying *NIS, nisevent *event);
void nisCameraFOV(nisplaying *NIS, nisevent *event);
void nisCameraCut(nisplaying *NIS, nisevent *event);
void nisTextCard(nisplaying *NIS, nisevent *event);
void nisScissorOut(nisplaying *NIS, nisevent *event);
void nisFocusAtEnd(nisplaying *NIS, nisevent *event);
void nisFocus(nisplaying *NIS, nisevent *event);
void nisMusicStart(nisplaying *NIS, nisevent *event);
void nisMusicStop(nisplaying *NIS, nisevent *event);
void nisBlackFadeSet(nisplaying *NIS, nisevent *event);
void nisBlackFadeTo(nisplaying *NIS, nisevent *event);
void nisColorScheme(nisplaying *NIS, nisevent *event);
void nisMeshAnimationStart(nisplaying *NIS, nisevent *event);
void nisMeshAnimationStop(nisplaying *NIS, nisevent *event);
void nisMeshAnimationPause(nisplaying *NIS, nisevent *event);
void nisMeshAnimationSeek(nisplaying *NIS, nisevent *event);
void nisBlendCameraEndPoint(nisplaying *NIS, nisevent *event);
void nisTextScroll(nisplaying *NIS, nisevent *event);
void nisVolumeSet(nisplaying *NIS, nisevent *event);
void nisTrailMove(nisplaying *NIS, nisevent *event);
void nisTrailZero(nisplaying *NIS, nisevent *event);
void nisUniversePauseToggle(nisplaying *NIS, nisevent *event);
void nisUniverseHideToggle(nisplaying *NIS, nisevent *event);
void nisNewEnvironment(nisplaying *NIS, nisevent *event);
void nisSMPTEOn(nisplaying *NIS, nisevent *event);
void nisSMPTEOff(nisplaying *NIS, nisevent *event);
void nisStaticOn(nisplaying *NIS, nisevent *event);
void nisStaticOff(nisplaying *NIS, nisevent *event);
void nisHideDerelict(nisplaying *NIS, nisevent *event);
void nisNewNIS(nisplaying *NIS, nisevent *event);
void nisAttributes(nisplaying *NIS, nisevent *event);
void nisAttackKasSelection(nisplaying *NIS, nisevent *event);
void nisKeepMovingAtEnd(nisplaying *NIS, nisevent *event);
void nisMinimumLOD(nisplaying *NIS, nisevent *event);
void nisDamageLevel(nisplaying *NIS, nisevent *event);
void cat_nisRepair(nisplaying *NIS, nisevent *event);
void cat_nisCameraClipNear(nisplaying *NIS, nisevent *event);
void cat_nisCameraClipFar(nisplaying *NIS, nisevent *event);
void cat_nisOpenSlipgate(nisplaying *NIS, nisevent *event);
void cat_nisCloseSlipgate(nisplaying *NIS, nisevent *event);
void cat_nisLightAmbient(nisplaying *NIS, nisevent *event);
void cat_nisLightDiffuse(nisplaying *NIS, nisevent *event);
void cat_nisGlobalAmbient(nisplaying *NIS, nisevent *event);
void cat_nisNumberLights(nisplaying *NIS, nisevent *event);
void cat_nisMimic(nisplaying *NIS, nisevent *event);
void cat_nisFog(nisplaying *NIS, nisevent *event);
void cat_nisEbolts(nisplaying *NIS, nisevent *event);
void cat_nisBeastEffect(nisplaying *NIS, nisevent *event);
void cat_nisSubtitle(nisplaying *NIS, nisevent *event);
// CAT: the exe's dispatch table (nisEventDispatch 0x0088888c, 75 entries, Cataclysm's opcodes: NIS.h)
niseventdispatch nisEventDispatch[] =
{
    /*NEO_DeathDamage,          */ nisDeathAnything,
    /*NEO_DeathProjectile,      */ nisDeathAnything,
    /*NEO_DeathBeam,            */ nisDeathAnything,
    /*NEO_DeathCollision,       */ NULL,
    /*NEO_Attack,               */ nisAttack,
    /*NEO_Repair,               */ cat_nisRepair,           // CAT: 0x004cf370
    /*NEO_HaltAttack,           */ nisHaltAttack,
    /*NEO_Fire,                 */ nisFire,
    /*NEO_Show                  */ nisObjectShow,
    /*NEO_Hide                  */ nisObjectHide,
    /*NEO_Invincible,           */ nisInvincible,
    /*NEO_Vincible,             */ nisVincible,
    /*NEO_SoundEvent,           */ nisSoundEvent,
    /*NEO_SpeechEvent,          */ nisSpeechEvent,
    /*NEO_DisableDefaultSpeech  */ NULL,
    /*NEO_EnableDefaultSpeech,  */ NULL,
    /*NEO_CustomEffect,         */ NULL,
    /*NEO_CustomGunEffect,      */ NULL,
    /*NEO_GunShoot,             */ NULL,
    /*NEO_RemainAtEnd,          */ nisRemainAtEnd,
    /*NEO_FleetSpeechEvent,     */ nisFleetSpeechEvent,
    /*NEO_CameraFOV,            */ nisCameraFOV,
    /*NEO_CameraClipNear,       */ cat_nisCameraClipNear,   // CAT: 0x004d0110
    /*NEO_CameraClipFar,        */ cat_nisCameraClipFar,    // CAT: 0x004d0140
    /*NEO_CameraCut,            */ nisCameraCut,
    /*NEO_BlackScreenStart,     */ NULL,
    /*NEO_BlackScreenEnd,       */ NULL,
    /*NEO_TextCard,             */ nisTextCard,
    /*NEO_TextScroll,           */ nisTextScroll,
    /*NEO_ScissorOut,           */ nisScissorOut,
    /*NEO_Focus,                */ nisFocus,
    /*NEO_FocusAtEnd            */ nisFocusAtEnd,
    /*NEO_StartMusic            */ nisMusicStart,
    /*NEO_StopMusic             */ nisMusicStop,
    /*NEO_BlackFadeSet          */ nisBlackFadeSet,
    /*NEO_BlackFadeTo           */ nisBlackFadeTo,
    /*NEO_ColorScheme           */ nisColorScheme,
    /*NEO_MeshAnimationStart,   */ nisMeshAnimationStart,
    /*NEO_MeshAnimationStop,    */ nisMeshAnimationStop,
    /*NEO_MeshAnimationPause,   */ nisMeshAnimationPause,
    /*NEO_MeshAnimationSeek,    */ nisMeshAnimationSeek,
    /*NEO_BlendCameraEndPoint   */ nisBlendCameraEndPoint,
    /*NEO_MusicVolume,          */ nisVolumeSet,
    /*NEO_SpeechVolume,         */ nisVolumeSet,
    /*NEO_SFXVolume,            */ nisVolumeSet,
    /*NEO_TrailMove,            */ nisTrailMove,
    /*NEO_TrailZeroLength,      */ nisTrailZero,
    /*NEO_UniversePauseToggle   */ nisUniversePauseToggle,
    /*NEO_UniverseHideToggle    */ nisUniverseHideToggle,
    /*NEO_NewEnvironment,       */ nisNewEnvironment,
    /*NEO_LinkEnvironment,      */ NULL,
    /*NEO_AnimaticSpeechEvent,  */ nisAnimaticSpeechEvent,
    /*NEO_SMPTEOn,              */ nisSMPTEOn,
    /*NEO_SMPTEOff,             */ nisSMPTEOff,
    /*NEO_OpenSlipgate,         */ cat_nisOpenSlipgate,     // CAT: 0x004d00b0
    /*NEO_CloseSlipgate,        */ cat_nisCloseSlipgate,    // CAT: 0x004d00d0
    /*NEO_StaticOn,             */ nisStaticOn,
    /*NEO_StaticOff,            */ nisStaticOff,
    /*NEO_HideDerelict          */ nisHideDerelict,
    /*NEO_ShowDerelict          */ nisHideDerelict,
    /*NEO_StartNewNIS           */ nisNewNIS,
    /*NEO_Attributes            */ nisAttributes,
    /*NEO_AttackKasSelection    */ nisAttackKasSelection,
    /*NEO_KeepMovingAtEnd       */ nisKeepMovingAtEnd,
    /*NEO_DamageLevel           */ nisDamageLevel,
    /*NEO_MinimumLOD,           */ nisMinimumLOD,
    /*NEO_LightAmbient,         */ cat_nisLightAmbient,     // CAT: 0x004cf920
    /*NEO_LightDiffuse,         */ cat_nisLightDiffuse,     // CAT: 0x004cf970
    /*NEO_GlobalAmbient,        */ cat_nisGlobalAmbient,    // CAT: 0x004cf9c0
    /*NEO_NumberLights,         */ cat_nisNumberLights,     // CAT: 0x004cfa10
    /*NEO_Mimic,                */ cat_nisMimic,            // CAT: 0x004d5fa0
    /*NEO_Fog,                  */ cat_nisFog,              // CAT: 0x004d08f0
    /*NEO_Ebolts,               */ cat_nisEbolts,           // CAT: 0x004d0850
    /*NEO_BeastEffect,          */ cat_nisBeastEffect,      // CAT: 0x004d08b0
    /*NEO_Subtitle,             */ cat_nisSubtitle,         // CAT: 0x004cf740
    /*NEO_LastNEO               */ NULL,
};

//default orientation matrix for an NIS ship
matrix HPBIdentityShip = {0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f};

//global variables for parsing script files
sdword nisCurrentObject = -1;                   //current object in the script file for subsequent events
nisheader *nisCurrentHeader = NULL;              //current NIS header to go with our current script file
real32 nisCurrentTime = REALlyBig;              //current time for subsequent events

nisheader *testHeader;
nisplaying *testPlaying = 0;
taskhandle nisTaskHandle;
bool32 nisTaskToPause = FALSE;                    //task to be paused at end of loading
bool32 nisTaskToResume = FALSE;                   //task to be resumed at end of loading

nisplaying *thisNisPlaying = NULL;
nisheader *thisNisHeader = NULL;
nisDoneCB thisNisDoneCB = NULL;

// CAT: NIS.h (the hyperspace module copies 0x00a4ca38 / 0x00a4cb80, the fadeSet flag 0x00a4cb4c, 0x00a4cacc)
Ship *catNisModuleShips[CAT_NIS_MAX_MODULE_SHIPS];
sdword catNisNumModuleShips = 0;
ubyte catNisBlackFadeFlag = 0;
bool32 catNisKeepScissored = FALSE;

//information for NIS text cards
nistextcard nisTextCardList[NIS_NumberTextCards];
sdword nisTextCardIndex = 0;
color nisCurrentTextCardColor = colWhite;
sdword nisCurrentTextCardX = 0, nisCurrentTextCardY = 0;
sdword nisCurrentTextMargin = 0;
fonthandle nisCurrentTextCardFont = 0;
real32 nisCurrentDuration = 0.0f;
real32 nisCurrentTextFadeIn = 0.0f;
real32 nisCurrentTextFadeOut = 0.0f;
real32 nisTextScrollStart = 0.0f;
real32 nisTextScrollEnd = 0.0f;
real32 nisTextScrollDistance = 0.0f;

//information for zooming the camera and fading in the scissor window
real32 nisCameraCutTime = 0.0f;
real32 nisCameraCutStage2 = -1.0f;
bool32 nisCameraCutOut = FALSE;
tcb nisCameraParams[NIS_NCameraKeys] =
{                                               //b-spline parameters
    {0.8f, 0.0f, 0.0f},
    {0.8f, 0.0f, 0.0f},
    {9.9f, 9.0f, 9.9f},                         //dummy middle keyframe
    {0.8f, 0.0f, 0.0f},
    {0.8f, 0.0f, 0.0f},
};
tcb nisMiddleParam = {1.0f, 0.0f, -0.9f};
real32 nisCameraTimes[NIS_NCameraKeys] = {0.0f, 0.0f, 0.0f, 0.0f};//b-spline times
real32 nisCameraPoints[9][NIS_NCameraKeys];     //b-spline data points
splinecurve *nisCameraCurve[9];                 //actual b-splines
real32 nisScissorFadeIn = 0.0f;                 //duration of fade in/out
real32 nisScissorFadeOut = 0.0f;
real32 nisScissorFadeTime = 0.0f;               //current fade time
real32 nisScissorFade = 0.0f;                   //current fade amount (0..1)
real32 nisBlackFade = 0.0f;                     //the current fade level (0 = normal, 1 = fully faded out)
real32 nisBlackFadeDest = 0.0f;                 //what we're fading to
real32 nisBlackFadeRate = 0.0f;                 //at what speed
bool32 nisFullyScissored = FALSE;                 //is scissor fully opaque?
bool32 bCameraFocussed;                           //set to TRUE if the camera was focussed on command of the script file
bool32 bPerfectFocusComputed;                     //set to TRUE when nisBlendCameraEndPoint called
bool32 nisSeeking = FALSE;                        //set to TRUE during seeks
bool32 nisNewNISStarted = FALSE;                  //set to TRUE when a new NIS is started by an event
real32 nisPreviousCameraDistance = 10.0f;       //zoom distance of the camera when the NIS started

//music playing stuffs.
real32 nisPreviousSFXVolume, nisPreviousSpeechVolume, nisPreviousMusicVolume;
bool32 nisMusicPlaying = FALSE;
//for pausing/hiding the universe
bool8 nisUniversePause = FALSE;
bool8 nisUniverseHidden = FALSE;
real32 nisUnivStartTime;

//for changing environment zones
char *nisPreviousLighting;
char *nisPreviousBackground;
real32 nisPreviousTheta;
real32 nisPreviousPhi;

//for the SMPTE counter
nisSMPTE *nisSMPTECounter = NULL;

//for on-screen static
nisstatic *nisStatic[NIS_NumberStatics + NIS_NumberExtraStatics] = {NULL, NULL, NULL, NULL};

//for localization
char *nisLanguageSubpath[] =
{
    "",
#ifdef _WIN32
    "French\\",
    "German\\",
#else
    "French/",
    "German/",
#endif
    "",         // Spanish uses the same script settings as English as the audio isn't localized
    ""          // Italian uses the same script settings as English as the audio isn't localized
};

/*=============================================================================
    Functions:
=============================================================================*/

#if NIS_TEST
char *nisTestNIS = NULL;
char *nisTestScript = NULL;
vector nisStartingPosition;
matrix nisStartingMatrix;
sdword nisTestIndex = 0;
void nisNamesFind(sdword index, char **NIS, char **script)
{
    filehandle fp;
    char *parse;

    parse = memAlloc(PATH_MAX * 2, "Sdfgadsf", 0);
    *NIS = *script = NULL;
    if (!fileExists(nisTestNIS, 0))
    {
        return;
    }
    fp = fileOpen(nisTestNIS, FF_TextMode);
    for (; index >= 0; index--)
    {
        if (fileLineRead(fp, parse, PATH_MAX * 2) == FR_EndOfFile)
        {
            fileClose(fp);
            return;
        }
    }
    *NIS = parse;
    strtok(parse, " \n\t,");
    *script = strtok(NULL, " \n\t,");
    fileClose(fp);
    return;
}
void nisTest(vector *position, matrix *mat)
{
    char *nisFile, *nisScript;
    if (nisTestNIS == NULL)
    {
        return;
    }
    nisStartingPosition = *position;
    nisStartingMatrix = *mat;

    nisNamesFind(nisTestIndex, &nisFile, &nisScript);
    if (nisFile == NULL)
    {
        dbgMessagef("Can't find %d's NIS is file '%s'", nisTestIndex, nisFile);
        return;
    }
    dbgMessagef("nisTest: playing NIS '%s' with script '%s'.", nisFile, nisScript);
    testHeader = nisLoad(nisFile, nisScript);
    testHeader->loop = 0.0f;            //force the NIS to loop
    testPlaying = nisStart(testHeader, &nisStartingPosition, &nisStartingMatrix);
    memFree(nisFile);
    taskResume(nisTaskHandle);
}
void nisTestAnother(sdword skip)
{
    char *nisFile, *nisScript;
    nisStop(testPlaying);
    nisDelete(testHeader);
    nisTestIndex += skip;
    nisTestIndex = max(nisTestIndex, 0);
    nisNamesFind(nisTestIndex, &nisFile, &nisScript);
    if (nisFile == NULL)
    {
        dbgMessagef("Can't find %d's NIS is file '%s'", nisTestIndex, nisFile);
        return;
    }
    dbgMessagef("nisTest: playing NIS '%s' with script '%s'.", nisFile, nisScript);
    testHeader = nisLoad(nisFile, nisScript);
    testHeader->loop = 0.0f;            //force the NIS to loop
    testPlaying = nisStart(testHeader, &nisStartingPosition, &nisStartingMatrix);
    memFree(nisFile);
}
#endif

/*-----------------------------------------------------------------------------
    Name        : nisCameraFlyCompute
    Description : Compute the camera location for flying it into the starting position
    Inputs      : timeElapsed - time step for flying in
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisCameraFlyCompute(real32 timeElapsed)
{
    vector eye, look, up;

    eye.x = bsCurveUpdate(nisCameraCurve[0], timeElapsed);
    if (eye.x == REALlyBig)
    {                                           //if any curve has expired
        //...end the camera cut, start the NIS
        nisPause(FALSE);
        nisCameraCutTime = 0.0f;                //done with the camera cuts
        nisCameraCutStage2 = -1.0f;
    }
    else
    {
        eye.y = bsCurveUpdate(nisCameraCurve[1], timeElapsed);
        eye.z = bsCurveUpdate(nisCameraCurve[2], timeElapsed);
        look.x = bsCurveUpdate(nisCameraCurve[3], timeElapsed);
        look.y = bsCurveUpdate(nisCameraCurve[4], timeElapsed);
        look.z = bsCurveUpdate(nisCameraCurve[5], timeElapsed);
        up.x = bsCurveUpdate(nisCameraCurve[6], timeElapsed);
        up.y = bsCurveUpdate(nisCameraCurve[7], timeElapsed);
        up.z = bsCurveUpdate(nisCameraCurve[8], timeElapsed);
        vecNormalize(&up);
        dbgAssertOrIgnore(eye.x != REALlyBig && eye.y != REALlyBig && eye.z != REALlyBig);
        dbgAssertOrIgnore(look.x != REALlyBig && look.y != REALlyBig && look.z != REALlyBig);
        dbgAssertOrIgnore(up.x != REALlyBig && up.y != REALlyBig && up.z != REALlyBig);
        nisCamera->eyeposition = eye;
        nisCamera->lookatpoint = look;
        nisCamera->upvector = up;
    }
}

/*-----------------------------------------------------------------------------
    Name        : nisUpdateTask
    Description : Task function for updating NIS's
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
//void utyTeaserEnd(void);
extern nisplaying *utyTeaserPlaying;
DEFINE_TASK(nisUpdateTask)
{
    static real32 newTime;
    static real32 timeElapsed;

    taskBegin;

    taskYield(0);

    while (1)
    {
        //code for playing in-game NIS's
        if (nisScissorFadeOut != 0)
        {                                                   //if fading the scissor window out
            nisScissorFadeTime += (real32)UNIVERSE_UPDATE_PERIOD;
            nisFullyScissored = FALSE;                      //window no longer fully scissored
            if (nisScissorFadeTime >= nisScissorFadeOut)
            {                                               //if at the end of the fade
                nisScissorFade = nisScissorFadeOut = nisScissorFadeTime = 0.0f;//don't do any more fading
                if (thisNisPlaying == NULL)
                {
                    taskPause(nisTaskHandle);                   //NIS task all done
                }
            }
            else
            {
                nisScissorFade = 1.0f - (nisScissorFadeTime / nisScissorFadeOut);//set the fade amount
            }
        }

        //fade to/from black
        if (nisBlackFade != nisBlackFadeDest)
        {
            if (ABS(nisBlackFade - nisBlackFadeDest) <= UNIVERSE_UPDATE_PERIOD)
            {                                           //if the end of the fade
                nisBlackFade = nisBlackFadeDest;
                nisBlackFadeRate = 0.0f;
            }
            else
            {
                nisBlackFade += nisBlackFadeRate * UNIVERSE_UPDATE_PERIOD;
            }

#if NIS_VERBOSE_LEVEL >= 1
            dbgMessagef("fade level = %.2f", nisBlackFade);
#endif
        }

        //fade the scissor window in
        if (nisScissorFadeIn > 0.0f)
        {                                               //if fading window IN
//          soundEvent(NULL, UI_Letterbox);
            nisScissorFade = nisScissorFadeTime / nisScissorFadeIn;//set the fade amount
            if (nisScissorFadeTime >= nisScissorFadeIn)
            {                                           //if at the end of the fade
                nisFullyScissored = TRUE;               //window is fully scissored
                nisScissorFadeIn = nisScissorFadeTime = 0.0f;//don't do any more fading
                nisScissorFade = 0.0f;
                // CAT: a KAS letterbox that is coming in is finished here (nisUpdateTask 0x004cc600:
                //      cat_kasWideScreenActive 0x004973c0 / cat_kasWideScreenInFinish 0x004973d0, the KAS letterbox)
                if (cat_kasWideScreenActive())
                {
                    cat_kasWideScreenInFinish();
                }
            }
            nisScissorFadeTime += UNIVERSE_UPDATE_PERIOD;
            // CAT: the debug frame-step flag is cleared while the letterbox comes in (0x00a6ed74, UnivUpdate.c)
            if (catUnivFrameStep)
            {
                catUnivFrameStep = FALSE;
            }
        }

        if (thisNisPlaying)
        {
            if (nisCameraCutTime != 0.0f)
            {
                nisPause(TRUE);                             //make sure NIS is paused
            }
            //if (!nisPaused)
            {                                               //actually update NIS if NIS unpaused
                newTime = nisUpdate(thisNisPlaying, UNIVERSE_UPDATE_PERIOD);
                if (newTime == REALlyBig)
                {
                    nisStop(thisNisPlaying);
                    nisDelete(thisNisHeader);
                    if (nisScissorFadeOut == 0.0f)
                    {
                        taskPause(nisTaskHandle);
                    }
                    thisNisPlaying = NULL;
                    if (thisNisDoneCB != NULL)
                    {
                        thisNisDoneCB();
                        thisNisDoneCB = NULL;
                    }
                    // CAT: the letterbox stays when the single-player game asked for it (0x00a4cacc: the NIS
                    //      leads into a hyperspace jump or an animatic; nisUpdateTask 0x004cc600)
                    if (catNisKeepScissored)
                    {
                        catNisKeepScissored = FALSE;
                    }
                    else
                    {
                        nisFullyScissored = FALSE;
                    }
                }
            }

            //fly the camera in
            if (nisCameraCutTime != 0.0f)
            {
                timeElapsed = UNIVERSE_UPDATE_PERIOD;  //!!! cap camera velocity?
                nisCameraFlyCompute(timeElapsed);
            }

        }
        //code for playing test NIS's (similar to above but with special keyboard logic)
#if NIS_TEST
        if (testPlaying)
        {
            newTime = nisUpdate(testPlaying, UNIVERSE_UPDATE_PERIOD);
            if (newTime == REALlyBig || keyIsStuck(NUMPAD1))
            {
                keyClearSticky(NUMPAD1);
                nisStop(testPlaying);
                nisDelete(testHeader);
                taskPause(nisTaskHandle);
                testPlaying = NULL;
                nisFullyScissored = TRUE;
            }
        }
#endif
        //special-case teaser playing code (must do univupdates and whatnot)
/*
        if (utyTeaserPlaying != NULL)
        {
            if (!gameIsRunning)
            {
                univUpdate(UNIVERSE_UPDATE_PERIOD);
                soundEventUpdate();
            }
            newTime = nisUpdate(utyTeaserPlaying, UNIVERSE_UPDATE_PERIOD);

            if (newTime == REALlyBig)
            {
                if (nisScreenStarted == FALSE)
                {
                    feScreenStart(ghMainRegion, "Main_game_screen");
                    mouseCursorShow();
                }
                utyTeaserEnd();
            }
            if (keyAnyKeyHit() && nisScreenStarted == FALSE)
            {
                keyClearAll();
                feScreenStart(ghMainRegion, "Main_game_screen");
                mouseCursorShow();
                nisScreenStarted = TRUE;
            }
        }
*/
        taskYield(0);
    }
    taskEnd;
}

/*-----------------------------------------------------------------------------
    Name        : nisStartup
    Description : Start the NIS module
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisStartup(void)
{
    nisTaskHandle = taskStart(nisUpdateTask, UNIVERSE_UPDATE_PERIOD, 0);
    taskPause(nisTaskHandle);
    // CAT: back to the normal game speed (nisStartup 0x004cc8f0 ends with cat_taskTimeCompressionReset; the exe's
    //      taskStart takes the task's name "NisUpdateTask" as a fourth argument, GoK's Task.c has none)
    cat_taskTimeCompressionReset();
}

/*-----------------------------------------------------------------------------
    Name        : nisShutdown
    Description : Shut down the NIS module
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisShutdown(void)
{

}

/*-----------------------------------------------------------------------------
    Name        : nisTaskPause
    Description : Set the NIS task to pause at the end of loading.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisTaskPause(void)
{
    nisTaskToPause = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : nisTaskResume
    Description : Set the NIS task to resume at the end of loading.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisTaskResume(void)
{
    nisTaskToResume = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : nisTaskPauseOrResume
    Description : Executes a queued-up pause or resume at the end of loading.
                  This must be called from a task that will be paused by loading.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisTaskPauseOrResume(void)
{
    if (nisTaskToPause)
    {
        taskPause(nisTaskHandle);
        nisTaskToPause = FALSE;
    }
    if (nisTaskToResume)
    {
        taskResume(nisTaskHandle);
        nisTaskToResume = FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : nisRotateAboutVector
    Description : Make a rotation matrix about an arbitrary vector
    Inputs      : axis - vector to rotate about (must be a unit vector)
                  radians - angle (in radians) to rotate by
    Outputs     : m - matrix (3x3) to fill
    Return      :
----------------------------------------------------------------------------*/
void nisRotateAboutVector(real32* m, vector* axis, real32 radians)
{
    real32 s, c;
    real32 xx, yy, zz, xy, yz, zx, xs, ys, zs, one_c;
    double rads;
    real32 x = axis->x;
    real32 y = axis->y;
    real32 z = axis->z;

    rads = (double)radians;
    s = (real32)sin(rads);
    c = (real32)cos(rads);

#define M(row,col) m[((col<<1) + col)+row]

    xx = x*x;
    yy = y*y;
    zz = z*z;
    xy = x*y;
    yz = y*z;
    zx = z*x;
    xs = x*s;
    ys = y*s;
    zs = z*s;
    one_c = 1.0f - c;

    M(0,0) = (one_c * xx) + c;
    M(0,1) = (one_c * xy) - zs;
    M(0,2) = (one_c * zx) + ys;

    M(1,0) = (one_c * xy) + zs;
    M(1,1) = (one_c * yy) + c;
    M(1,2) = (one_c * yz) - xs;

    M(2,0) = (one_c * zx) - ys;
    M(2,1) = (one_c * yz) + xs;
    M(2,2) = (one_c * zz) + c;

#undef M
}
/*-----------------------------------------------------------------------------
    Name        : nisShipEulerToMatrix
    Description : Builds a coordinate system matrix from a heading,pitch,bank
                    triplicate.
    Inputs      : rotVector - vectror whose members represent the heading, pitch and bank
    Outputs     : coordsys - where to store the matrix
    Return      :
----------------------------------------------------------------------------*/
void nisShipEulerToMatrix(matrix *coordsys, vector *rotVector)
{
    matrix rotation, tempMatrix;
    vector rotateAbout = {0.0f, 0.0f, 1.0f};

    *coordsys = HPBIdentityShip;
    //rotate about up vector (heading)
    nisRotateAboutVector((real32 *)&rotation, &rotateAbout, -rotVector->x); //negative heading
    matMultiplyMatByMat(&tempMatrix, &rotation, coordsys);
    *coordsys = tempMatrix;
    //rotate about heading vector (pitch in lightwave, bank actually)
    matGetVectFromMatrixCol3(rotateAbout, *coordsys);
    nisRotateAboutVector((real32 *)&rotation, &rotateAbout, rotVector->y); //positive pitch
    matMultiplyMatByMat(&tempMatrix, &rotation, coordsys);
    *coordsys = tempMatrix;
    //rotate about heading vector (bank in lightwave, pitch actually)
    matGetVectFromMatrixCol2(rotateAbout, *coordsys);
    nisRotateAboutVector((real32 *)&rotation, &rotateAbout, -rotVector->z); //positive bank
    matMultiplyMatByMat(&tempMatrix, &rotation, coordsys);
    *coordsys = tempMatrix;
}

/*-----------------------------------------------------------------------------
    Name        : nisObjectEulerToMatrix
    Description : Builds a coordinate system matrix from a heading,pitch,bank
                    triplicate.
    Inputs      : rotVector - vectror whose members represent the heading, pitch and bank
    Outputs     : coordsys - where to store the matrix
    Return      :
----------------------------------------------------------------------------*/
void nisObjectEulerToMatrix(matrix *coordsys, vector *rotVector)
{
    matrix rotation, tempMatrix;
    vector rotateAbout = {1.0f, 0.0f, 0.0f};

    *coordsys = IdentityMatrix;
    //rotate about up vector (heading)
    nisRotateAboutVector((real32 *)&rotation, &rotateAbout, -rotVector->x); //negative heading
    matMultiplyMatByMat(&tempMatrix, &rotation, coordsys);
    *coordsys = tempMatrix;
    //rotate about heading vector (pitch in lightwave, bank actually)
    matGetVectFromMatrixCol3(rotateAbout, *coordsys);
    nisRotateAboutVector((real32 *)&rotation, &rotateAbout, rotVector->y); //positive pitch
    matMultiplyMatByMat(&tempMatrix, &rotation, coordsys);
    *coordsys = tempMatrix;
    //rotate about right vector (bank in lightwave, pitch actually)
    matGetVectFromMatrixCol2(rotateAbout, *coordsys);
    nisRotateAboutVector((real32 *)&rotation, &rotateAbout, -rotVector->z); //positive bank
    matMultiplyMatByMat(&tempMatrix, &rotation, coordsys);
    *coordsys = tempMatrix;
}

/*-----------------------------------------------------------------------------
    Name        : nisNewObjectCreate
    Description : Creates a new nis ship and sets it's position
    Inputs      : index - index of ship we're creating
                  header - header we're creating it from
                  path - path for this ship
    Outputs     :
    Return      : pointer to new ship
----------------------------------------------------------------------------*/
Ship *nisNewObjectCreate(nisplaying *NIS, sdword index, nisheader *header, spaceobjpath *path, nisplaying *newHeader, vector *position)
{
    vector startVector, xyzTemp, xyz;
    matrix tempCoordsys, rotation, oldCoordsys;
    Ship *newShip = NULL;
    Ship *module, *playerModule;
    CatClampedList *playerModules;
    DockStaticPoint *dockPoint;
    ShipType moduleTypes[CAT_NIS_MAX_MODULE_SHIPS];
    sdword nModules, moduleIndex = 0;

    startVector.x = path->curve[0][0];
    startVector.y = path->curve[1][0];
    startVector.z = path->curve[2][0];
    vecAdd(startVector, startVector, *position);
    switch (header->objectPath[index].race)
    {
        // CAT: the seven Cataclysm races are ship races (Sect, Beast, Kushan, Taiidan, P1, Traders, Derelict;
        //      nisNewObjectCreate 0x004cccb0 cases 0..6)
        case RACE_Sect:
        case RACE_Beast:
        case RACE_Kushan:
        case RACE_Taiidan:
        case RACE_P1:
        case RACE_Traders:
        case RACE_Derelict:
            // CAT: univCreateShip(type, race, pos, player, 0, 0, 1): created as a placed ship (HW1 passed 2, the
            //      NIS flag, which the exe no longer passes) and with the 7th argument, so an sMothership gets no
            //      module ships outside mission 1 (the NIS makes its own below)
            newShip = cat_univAddShipModules(path->type, path->race,
                 &startVector, &universe.players[((path->race == universe.players[0].race)) ? 0 : 1], 0, TRUE);
            newShip->targetable.health = REALlyBig;                   //by default, ships are invincible
            univAddObjToRenderList((SpaceObj *)newShip);
            newShip->header.flags |= SOF_DontApplyPhysics | SOF_Selectable | SOF_Targetable;
            if (path->type == sMothership)
            {
                // CAT: an NIS sMothership carries the modules the player's mothership took into hyperspace
                //      (cat_spGetHyperspaceModuleTypes 0x0051c0c0); each is the player's, clamped to it, invincible,
                //      and kept in catNisModuleShips for nisStop. The support module (sMothershipSupport) takes the
                //      dock point the player's own support module holds and is turned round when the point's parity
                //      differs (0x004cce56..0x004ccfb0); in mission 4 the docking bay is left out
                cat_spGetHyperspaceModuleTypes(&nModules, moduleTypes);
                while (nModules > 0)
                {
                    nModules--;
                    if ((singlePlayerGameInfo.currentMission == 4) && (moduleTypes[nModules] == sMothershipDockingBay))
                    {
                        continue;
                    }
                    module = univAddShip(moduleTypes[nModules], RACE_Sect, &startVector, &universe.players[0], 0);
                    catNisModuleShips[catNisNumModuleShips] = module;
                    module->targetable.health = REALlyBig;
                    univAddObjToRenderList((SpaceObj *)module);
                    module->header.flags |= SOF_Selectable;
                    module->header.flags |= SOF_Targetable;
                    if ((moduleTypes[nModules] == sMothershipSupport) && (universe.curPlayerPtr->PlayerMothership != NULL) &&
                        (universe.curPlayerPtr->PlayerMothership->guidance.clampInfo != NULL) &&
                        ((playerModules = universe.curPlayerPtr->PlayerMothership->guidance.clampInfo->clampedObjs) != NULL))
                    {
                        while (moduleIndex < playerModules->numShips)
                        {
                            playerModule = playerModules->ShipPtr[moduleIndex];
                            moduleIndex++;
                            if (playerModule->shiptype == sMothershipSupport)
                            {
                                if (playerModule->dockvars.catDockPointIndex != -1)
                                {
                                    dockPoint = &newShip->staticinfo->dockStaticInfo->dockstaticpoints[playerModule->dockvars.catDockPointIndex];
                                    dockReserveDockPoint(module, newShip, (sbyte)playerModule->dockvars.catDockPointIndex);
                                    matMultiplyMatByVec(&xyz, &newShip->rotatable.rotinfo.coordsys, &dockPoint->position);
                                    vecAdd(module->obj.posinfo.position, xyz, newShip->obj.posinfo.position);
                                    if (((ubyte)module->dockvars.catDockPointIndex -
                                         newShip->staticinfo->dockStaticInfo->firstOfType[dockPoint->type]) & 1)
                                    {
                                        matMakeRotAboutX(&rotation, (real32)cos(PI), (real32)sin(PI));
                                        oldCoordsys = module->rotatable.rotinfo.coordsys;
                                        matMultiplyMatByMat(&module->rotatable.rotinfo.coordsys, &oldCoordsys, &rotation);
                                    }
                                }
                                break;
                            }
                        }
                    }
                    clampObjToObj((SpaceObjRotImpTargGuidance *)catNisModuleShips[catNisNumModuleShips],
                                  (SpaceObjRotImpTargGuidance *)newShip);
                    catNisNumModuleShips++;
                }
                if (singlePlayerGameInfo.currentMission == 1)
                {
                    // CAT: in mission 1 also a docking bay that counts against the unit caps
                    module = univAddShip(sMothershipDockingBay, RACE_Sect, &startVector, &universe.players[0], 0);
                    unitCapEnable();
                    unitCapCreateShip(module, &universe.players[0]);
                    unitCapDisable();
                    univAddObjToRenderList((SpaceObj *)module);
                    module->header.flags |= SOF_DontApplyPhysics | SOF_Selectable | SOF_Targetable;
                    clampObjToObj((SpaceObjRotImpTargGuidance *)module, (SpaceObjRotImpTargGuidance *)newShip);
                }
            }
            break;
        case NSR_Asteroid:
            newShip = (Ship *)univAddAsteroid(path->type, &startVector);
            newShip->targetable.health = REALlyBig;
            break;
        case NSR_DustCloud:
            newShip = (Ship *)univAddDustCloud(path->type, &startVector);
            newShip->targetable.health = REALlyBig;
            break;
        case NSR_GasCloud:
            newShip = (Ship *)univAddGasCloud(path->type, &startVector);
            newShip->targetable.health = REALlyBig;
            break;
        case NSR_Nebula:
#if NIS_ERROR_CHECKING
            dbgFatal(DBG_Loc, "Can't animate nebulae");
#endif
            break;
        case NSR_Derelict:
            newShip = (Ship *)univAddDerelict(path->type, &startVector);
            newShip->targetable.health = REALlyBig;
            break;
        case NSR_Effect:
            newShip = (Ship *)memAlloc(sizeof(SpaceObjRot), "NISEffectSpaceObj", NonVolatile);
            memset(newShip, 0, sizeof(SpaceObjRot));
            newShip->header.objtype = -1;
            break;
        case NSR_Generic:
            // CAT: Cataclysm cannot create a generic NIS object any more (the case calls an empty stub and leaves
            //      the object pointer undefined, 0x004ccd0c); CAT-FIX: no object (NULL) instead of an undefined one
            newShip = NULL;
            break;
        case NSR_Missile:
            newShip = (Ship *)univAddMissile(path->type);   //type is really the race
            ((Missile *)newShip)->totallifetime = header->length;//missile should last the length of the NIS
            newShip->obj.posinfo.position = startVector;
            break;
        default:;                                            //!!! add asteroids and other stuff
#if NIS_ERROR_CHECKING
            dbgFatalf(DBG_Loc, "Undefined ship race for NIS spaceobj %d: %d.", index, path->race);
#endif
    }
    if (newShip == NULL)
    {                                                       // CAT-FIX: see NSR_Generic / NSR_Nebula (the exe faults)
        return NULL;
    }
    newShip->header.flags |= SOF_NISShip;
    if (path->nSamples == NIS_OneKeyframe)
    {                                                       //if only one keyframe in motion path
        //set the position and rotation of the object only once
        //object position:-Z, X, Y (UZI position mode 33)
        xyzTemp.x = -path->curve[2][NIS_FirstKeyFrame];
        xyzTemp.y = path->curve[0][NIS_FirstKeyFrame];
        xyzTemp.z = path->curve[1][NIS_FirstKeyFrame];
        matMultiplyMatByVec(&xyz, &NIS->nisMatrix, &xyzTemp);//multiply NIS world matrix
        newShip->obj.posinfo.position.x = position->x + xyz.x;
        newShip->obj.posinfo.position.y = position->y + xyz.y;
        newShip->obj.posinfo.position.z = position->z + xyz.z;

        startVector.x = path->curve[3][NIS_FirstKeyFrame];
        startVector.y = path->curve[4][NIS_FirstKeyFrame];
        startVector.z = path->curve[5][NIS_FirstKeyFrame];
        nisShipEulerToMatrix(&tempCoordsys, &startVector);
        matMultiplyMatByMat(&newShip->rotatable.rotinfo.coordsys, &NIS->nisMatrix, &tempCoordsys);

        univUpdateObjRotInfo((SpaceObjRot *)newShip);
        if (newShip->header.objtype == OBJ_ShipType)
        {                                                   // CAT: the ship's clamped parts follow (0x004cd23f)
            cat_clampUpdateAttachedObjects((SpaceObjRotImpTargGuidance *)newShip);
        }

        newHeader->objectsInMotion[index].curve[0] = NULL;//no motion path needed
    }
    return(newShip);
}

/*-----------------------------------------------------------------------------
    Name        : nisScissorBarsReset
    Description : Resets the scissor bars and the black fade.
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void nisScissorBarsReset(void)
{
    nisBlackFade = nisBlackFadeDest = 0.0f;                 //no fade is the default
    nisFullyScissored = FALSE;
    nisScissorFadeIn = nisScissorFadeOut = nisScissorFadeTime = nisScissorFade = 0.0f;
}

/*-----------------------------------------------------------------------------
    Name        : nisCameraPrepareForCopy
    Description : Prepares the specified NIS camera to be copied to a real camera.
    Inputs      : cam - camera to monkey with.
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisCameraPrepareForCopy(Camera *cam)
{
    vector distVect;
#if NIS_ERROR_CHECKING
    real32 magnitudeSq;

    vecSub(distVect, cam->lookatpoint, cam->eyeposition);   //make sure this camera has not already been prepared
    magnitudeSq = vecMagnitudeSquared(distVect);
    dbgAssertOrIgnore(magnitudeSq > NIS_LookScalar * NIS_LookScalar * 0.9f);
    dbgAssertOrIgnore(magnitudeSq < NIS_LookScalar * NIS_LookScalar * 1.1f);
#endif
    vecSub(distVect, cam->lookatpoint, cam->eyeposition);   //eye->look vector
    vecMultiplyByScalar(distVect, 1.0f / NIS_LookScalar);   //scale the vector back to a length of 1.0
    vecAdd(cam->lookatpoint, cam->eyeposition, distVect);   //add back to eye vector
}

/*-----------------------------------------------------------------------------
    Name        : nisStart
    Description : Start an NIS playing
    Inputs      : header - data which contains NIS information
                  position - NIS reference point
                  coordSystem - what rotation to play the NIS at
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
nisplaying *nisStart(nisheader *header, vector *position, matrix *coordSystem)
{
    nisplaying *newHeader;
    sdword size, index, j;
    spaceobjpath *path;
    camerapath *cameraPath;
    Ship *newShip;
    vector xyz, xyzTemp, rot, nisPos;
    matrix coordsys, tempCoordsys;
    cameramotion *camPath;
    real32 length;
    nisevent *event;
    splinecurve *curve;

    // CAT: the sensors manager's viewport draws again (catSmHideViewport 0x00a5b228 cleared; nisStart 0x004cd2b0)
    catSmHideViewport = FALSE;

    // disable unit cap counting during NIS sequence
    unitCapDisable();

    //clear any possibly persistant game controls
    mrNISStarting();

    // CAT: the NIS lens flare source joins the lens flares and the fog layer goes off (cat_rndLensFlareNISSourceAdd
    //      0x0057cac0, cat_rndFogLayerOff; nisStart 0x004cd2b0)
    cat_rndLensFlareNISSourceAdd();
    cat_rndFogLayerOff();

    //allocate and initialize the header
    size = sizeof(nisplaying) + header->nObjectPaths * sizeof(objectmotion) +
        header->nCameraPaths * sizeof(cameramotion);
    newHeader = memAlloc(size, "NISPlaying", NonVolatile);
    newHeader->header = header;
    newHeader->timeElapsed = 0.0f;
    newHeader->nisMatrix = *coordSystem;
    newHeader->nisPosition = *position;
    newHeader->nObjects = header->nObjectPaths;
    //create pointers to motion paths from the same block of allocation
    newHeader->objectsInMotion = (objectmotion *)((ubyte *)newHeader + sizeof(nisplaying));
    newHeader->camerasInMotion = (cameramotion *)((ubyte *)newHeader->objectsInMotion + header->nObjectPaths * sizeof(objectmotion));
    newHeader->iCurrentEvent = 0;
    //initialize all the motion paths
    newHeader->objectSplines = memAlloc(sizeof(splinecurve) * 6 * newHeader->nObjects, "objSplinePool", NonVolatile);
    curve = newHeader->objectSplines;
    for (index = 0; index < newHeader->nObjects; index++)
    {
        path = &header->objectPath[index];
        bitClear(path->race, NIS_CentreShip);
        newHeader->objectsInMotion[index].flags = 0;
        newShip = nisNewObjectCreate(newHeader, index, header, path, newHeader, position);
        newHeader->objectsInMotion[index].spaceobj = (SpaceObjRotImp *)newShip;
        if (path->nSamples == NIS_OneKeyframe)
        {                                                   //if only one keyframe in motion path
            continue;
        }
        for (j = 0; j < 6; j++, curve++)
        {                                                   //for each of x,y,z,h,p,b
            newHeader->objectsInMotion[index].curve[j] = curve;
            bsCurveStartPrealloced(curve, path->nSamples, path->curve[j],
                    path->times, path->parameters);
        }
        newHeader->objectsInMotion[index].parentIndex = path->parentIndex;
    }
    newHeader->nCameras = header->nCameraPaths;
    newHeader->cameraSplines = memAlloc(sizeof(splinecurve) * 6 * newHeader->nCameras, "camSplinePool", NonVolatile);
    curve = newHeader->cameraSplines;
    for (index = 0; index < newHeader->nCameras; index++)
    {
        cameraPath = &header->cameraPath[index];
        newHeader->camerasInMotion[index].cam = memAlloc(sizeof(Camera), "NISCamera", NonVolatile);
        cameraInit(newHeader->camerasInMotion[index].cam, 1.0f);
        newHeader->camerasInMotion[index].cam->fieldofview = NIS_DefaultFOV;
        if (cameraPath->nSamples == NIS_OneKeyframe)
        {                                                   //if only one keyframe in motion path
            newHeader->camerasInMotion[index].curve[0] = NULL;//no motion path needed
            continue;
        }
        for (j = 0; j < 6; j++, curve++)
        {                                                   //for each of x,y,z,h,p,b
            newHeader->camerasInMotion[index].curve[j] = curve;
            bsCurveStartPrealloced(curve, cameraPath->nSamples, cameraPath->curve[j],
                    cameraPath->times, cameraPath->parameters);
        }
    }
    //!!! do the same for lights
    if (newHeader->nCameras > 0)
    {
        nisCamera = newHeader->camerasInMotion[0].cam;
        nisIsRunning = TRUE;
    }
    // CAT: the exe then calls an rgl.dll hook when the RGL device is in use (0x00b3a6d8: (*0x00b3a610)()); GoK's GL
    //      backend has no such device ("Decisions" 28)
#if NIS_RENDERLIST_DISABLE
    universe.dontUpdateRenderList = TRUE;                   //don't let automatic NIS updating go on
#endif

    nisTextCardIndex = 0;

    //initialize the NIS camera
    //get starting camera parameters
    camPath = &newHeader->camerasInMotion[0];
    nisPos = newHeader->nisPosition;                    //get position from NIS matrix
    xyz.x = camPath->curve[0]->points[0];
    xyz.y = camPath->curve[1]->points[0];
    xyz.z = camPath->curve[2]->points[0];
    rot.x = camPath->curve[3]->points[0];
    rot.y = camPath->curve[4]->points[0];
    rot.z = camPath->curve[5]->points[0];
    //compute starting parameter in world space
    //camera position: -Z, X, Y (UZI mode 33)
    if (mrCamera == NULL)
    {                                                   //make sure we've got a camera pointer
        mrCamera = nisCamera;
    }
    xyzTemp.x = -xyz.z;
    xyzTemp.y = xyz.x;
    xyzTemp.z = xyz.y;
    matMultiplyMatByVec(&xyz, &newHeader->nisMatrix, &xyzTemp);//multiply NIS world matrix
    camPath->cam->eyeposition.x = nisPos.x + xyz.x;
    camPath->cam->eyeposition.y = nisPos.y + xyz.y;
    camPath->cam->eyeposition.z = nisPos.z + xyz.z;
    nisShipEulerToMatrix(&tempCoordsys, &rot);
    matMultiplyMatByMat(&coordsys, &newHeader->nisMatrix, &tempCoordsys);//multiply NIS world matrix
    matGetVectFromMatrixCol2(xyz, coordsys);
    vecSub(xyzTemp, camPath->cam->eyeposition, mrCamera->eyeposition);
    length = getVectDistSloppy(xyzTemp) * NIS_CameraLengthFactor;
    vecMultiplyByScalar(xyz, length);

    matGetVectFromMatrixCol1(camPath->cam->upvector, coordsys);
    vecAdd(camPath->cam->lookatpoint, xyz, camPath->cam->eyeposition);
    //start the camera blend, if any
    if (nisCameraCutTime != 0.0f)
    {                                                       //if a camera fade-in was specified
        sdword endFrame;

        endFrame = 2;
        nisCameraParams[2] = nisCameraParams[3];            //prepare for a 1-phase camera cut
        if (nisCameraCutStage2 != -1.0f)
        {                                                   //if it's a 2-stage cut
            vector midEye, midLook, breakPoint, midLookPoint;
            real32 breakRadius;
            GrowSelection *triggerShips;

            // CAT: the selection is looked up by cat_kasHashString("NisTriggerShips") (nisStart 0x004cd2b0)
            triggerShips = kasGetGrowSelectionPtrIfExists(cat_kasHashString("NisTriggerShips"));
            if (triggerShips != NULL)
            {
                //compute the camera's eye and look points for the mid keyframe
                //such that it is looking past the trigger ships at the start point of the NIS
                nisFocusPointCompute(&breakPoint, (MaxSelection *)triggerShips->selection);
                breakRadius = nisFocusMaxRadius(&breakPoint, (MaxSelection *)triggerShips->selection);
                if (header->iLookyObject >= 0)
                {                                           //if some ship is the designated looky ship
                    path = &header->objectPath[header->iLookyObject];
                    //object position:-Z, X, Y (UZI position mode 33)
                    midLookPoint.x = -path->curve[2][0];
                    midLookPoint.y = path->curve[0][0];
                    midLookPoint.z = path->curve[1][0];
                    matMultiplyMatByVec(&xyz, &newHeader->nisMatrix, &midLookPoint);//multiply NIS world matrix
                    midLookPoint.x = nisPos.x + xyz.x;
                    midLookPoint.y = nisPos.y + xyz.y;
                    midLookPoint.z = nisPos.z + xyz.z;
                }
                else
                {                                           //else midpoint just looks at start of camera path
                    midLookPoint = camPath->cam->eyeposition;
                }
                vecSub(midLook, midLookPoint, breakPoint);  //vector from trigger ships to look point
                length = getVectDistSloppy(midLook) * NIS_CameraLengthFactor;//remember that distance
                vecNormalize(&midLook);                     //normalize that vector
                vecScalarMultiply(midEye, midLook, breakRadius * NIS_CameraBlendPullBackFactor);//and pull the camera back far enough to see the trigger ships
                vecSub(midEye, breakPoint, midEye);         //eye position in world space
                vecMultiplyByScalar(midLook, length);       //make the camera look far enough
                vecAdd(midLook, midEye, midLook);           //lookat point relative to eye

                nisCameraPoints[0][2] = midEye.x;
                nisCameraPoints[1][2] = midEye.y;
                nisCameraPoints[2][2] = midEye.z;
                nisCameraPoints[3][2] = midLook.x;
                nisCameraPoints[4][2] = midLook.y;
                nisCameraPoints[5][2] = midLook.z;
                nisCameraPoints[6][2] = 0.0f;               //up vector
                nisCameraPoints[7][2] = 0.0f;
                nisCameraPoints[8][2] = 1.0f;
                nisCameraTimes[2] = nisCameraCutTime;       //set time for mid keyframe
                nisCameraCutTime += nisCameraCutStage2;     //make nisCameraCutTime the total cut time
                nisCameraParams[2] = nisMiddleParam;
                endFrame = 3;                               //make room for the mid keyframe
            }
        }
        //set the start/end times and points
        nisCameraTimes[endFrame + 0] = nisCameraTimes[endFrame + 1] = nisCameraCutTime;
        nisCameraPoints[0][0] = nisCameraPoints[0][1] = mrCamera->eyeposition.x;
        nisCameraPoints[1][0] = nisCameraPoints[1][1] = mrCamera->eyeposition.y;
        nisCameraPoints[2][0] = nisCameraPoints[2][1] = mrCamera->eyeposition.z;
        nisCameraPoints[3][0] = nisCameraPoints[3][1] = mrCamera->lookatpoint.x;
        nisCameraPoints[4][0] = nisCameraPoints[4][1] = mrCamera->lookatpoint.y;
        nisCameraPoints[5][0] = nisCameraPoints[5][1] = mrCamera->lookatpoint.z;
        nisCameraPoints[6][0] = nisCameraPoints[6][1] = 0.0f;//up vector
        nisCameraPoints[7][0] = nisCameraPoints[7][1] = 0.0f;
        nisCameraPoints[8][0] = nisCameraPoints[8][1] = 1.0f;
        nisCameraPoints[0][endFrame + 0] = nisCameraPoints[0][endFrame + 1] = camPath->cam->eyeposition.x;
        nisCameraPoints[1][endFrame + 0] = nisCameraPoints[1][endFrame + 1] = camPath->cam->eyeposition.y;
        nisCameraPoints[2][endFrame + 0] = nisCameraPoints[2][endFrame + 1] = camPath->cam->eyeposition.z;
        nisCameraPoints[3][endFrame + 0] = nisCameraPoints[3][endFrame + 1] = camPath->cam->lookatpoint.x;
        nisCameraPoints[4][endFrame + 0] = nisCameraPoints[4][endFrame + 1] = camPath->cam->lookatpoint.y;
        nisCameraPoints[5][endFrame + 0] = nisCameraPoints[5][endFrame + 1] = camPath->cam->lookatpoint.z;
        nisCameraPoints[6][endFrame + 0] = nisCameraPoints[6][endFrame + 1] = camPath->cam->upvector.x;
        nisCameraPoints[7][endFrame + 0] = nisCameraPoints[7][endFrame + 1] = camPath->cam->upvector.y;
        nisCameraPoints[8][endFrame + 0] = nisCameraPoints[8][endFrame + 1] = camPath->cam->upvector.z;
        //start all the curves
        for (j = 0; j < 9; j++)
        {                 //!!! possible memory leak; these splines are never freed!!!
            nisCameraCurve[j] = bsCurveStart(endFrame + 2, nisCameraPoints[j], nisCameraTimes, nisCameraParams, FALSE);
        }
        //start the flight so that the camera is positioned correctly for the next rendered frame
        nisCameraFlyCompute(0.00001f);
    }
    if (mrCamera == NULL)
    {
        mrCamera = nisCamera;
    }
    else
    {
        nisPreviousCameraDistance = mrCamera->distance;
    }
    nisCaptureCamera = TRUE;                                //now using the NIS camera
    bCameraFocussed = FALSE;
    bPerfectFocusComputed = FALSE;

    if (!nisNewNISStarted)
    {                                                       //don't monkey volumes when linking to a new NIS
        soundEventGetVolume(&nisPreviousSFXVolume, &nisPreviousSpeechVolume, &nisPreviousMusicVolume);
    }

    if (!nisNewNISStarted)
    {
        nisBlackFade = nisBlackFadeDest = 0.0f;                 //no fade is the default
        catNisBlackFadeFlag = 0;                                // CAT: 0x00a4cb4c (nisStart 0x004cd2b0)
        nisTextScrollDistance = 0.0f;
        nisPreviousBackground = nisPreviousLighting = NULL;

    }

    trkTrackValueAdd("eye.x", &nisCamera->eyeposition.x, &newHeader->timeElapsed, colRGB(180, 100, 10));
    trkTrackValueAdd("eye.y", &nisCamera->eyeposition.y, &newHeader->timeElapsed, colRGB(180, 100, 10));
    trkTrackValueAdd("eye.z", &nisCamera->eyeposition.z, &newHeader->timeElapsed, colRGB(180, 100, 10));
    trkTrackValueAdd("look.x", &nisCamera->lookatpoint.x, &newHeader->timeElapsed, colRGB(100, 180, 10));
    trkTrackValueAdd("look.y", &nisCamera->lookatpoint.y, &newHeader->timeElapsed, colRGB(100, 180, 10));
    trkTrackValueAdd("look.z", &nisCamera->lookatpoint.z, &newHeader->timeElapsed, colRGB(100, 180, 10));

    nisUnivStartTime = universe.totaltimeelapsed;

    nisSMPTECounter = NULL;
    memset(nisStatic, 0, sizeof(nisStatic));
    //nisStatic = NULL;

    //execute any events at time 0
    event = header->events;
    while (newHeader->iCurrentEvent < newHeader->header->nEvents && event->time == 0.0f)
    {
        dbgAssertOrIgnore(nisEventDispatch[event->code] != NULL);
        nisEventDispatch[event->code](newHeader, event);
        newHeader->iCurrentEvent++;
        event++;
    }

    return(newHeader);
}

/*-----------------------------------------------------------------------------
    Name        : nisStop
    Description : Stop an NIS
    Inputs      : NIS - playing NIS to stop
    Outputs     :
    Return      : void
    Note        : Does not free the actual nisplaying structure
----------------------------------------------------------------------------*/
void nisStop(nisplaying *NIS)
{
    sdword index, j;
    Ship *ship = NULL;
    Derelict *derelict = NULL;
    real32 currentSFXVolume, currentSpeechVolume, currentMusicVolume;

    dbgAssertOrIgnore(NIS != NULL);
    // CAT: the NIS lens flare source and fog layer go, the effects stop over half a second (nisStop 0x004cdd60)
    cat_rndLensFlareNISSourceRemove();
    cat_rndFogLayerTextureUnload();
    cat_rndFogLayerOff();
    cat_rndFogLayerFadeTo(0.0f, 0, 0, 0.0f, 0.0f);
    soundEventStopSFX(0.5f);
    // CAT: the NIS's module copies are deleted, the last made first (0x004cdd94)
    for (; catNisNumModuleShips > 0; catNisNumModuleShips--)
    {
        univReallyDeleteThisShipRightNow(catNisModuleShips[catNisNumModuleShips - 1]);
        catNisModuleShips[catNisNumModuleShips - 1] = NULL;
    }
    for (index = 0; index < NIS->nObjects; index++)
    {                                                       //for each ship

        if (bitTest(NIS->objectsInMotion[index].flags, OMF_ObjectDied))
        {                                                   //don't free any objects that were deleted
            continue;
        }
        if (bitTest(NIS->objectsInMotion[index].flags, OMF_RemainAtEnd))
        {
            switch (NIS->header->objectPath[index].race)
            {
                case RACE_Sect:                             // CAT: the seven Cataclysm ship races (nisStop 0x004cdd60)
                case RACE_Beast:
                case RACE_Kushan:
                case RACE_Taiidan:
                case RACE_P1:
                case RACE_Traders:
                case RACE_Derelict:
                    ship = (Ship *)NIS->objectsInMotion[index].spaceobj;

                    ship->header.flags &= ~(SOF_DontApplyPhysics);
                    if (!bitTest(NIS->objectsInMotion[index].flags, OMF_KeepOnMoving))
                    {                                       //if we should stop this ship
                        vecZeroVector(ship->obj.posinfo.velocity);
                        vecZeroVector(ship->obj.posinfo.force);
                        vecZeroVector(ship->rotatable.rotinfo.torque);
                        vecZeroVector(ship->rotatable.rotinfo.rotspeed);
                        SET_MOVING_IMMOBILE(ship->obj.posinfo.isMoving);
                    }
                    ship->targetable.health = ship->staticinfo->health.maxhealth; //make it 'vincible'

                    unitCapEnable();
                    unitCapCreateShip(ship,ship->playerowner);
                    unitCapDisable();
                    if ((ship->shiptype == sMothership) || (ship->shiptype == bMothership))
                    {
                        // CAT: a mothership's clamped modules apply physics again too (0x004cdea0). CAT-FIX: the
                        //      exe reads the clamp info and its list without testing them
                        if ((ship->guidance.clampInfo != NULL) && (ship->guidance.clampInfo->clampedObjs != NULL))
                        {
                            for (j = 0; j < ship->guidance.clampInfo->clampedObjs->numShips; j++)
                            {
                                ship->guidance.clampInfo->clampedObjs->ShipPtr[j]->header.flags &= ~SOF_DontApplyPhysics;
                            }
                        }
                    }
                    break;
                case NSR_Asteroid:
                case NSR_DustCloud:
                case NSR_GasCloud:
                    ship = (Ship *)NIS->objectsInMotion[index].spaceobj;

                    ship->header.flags &= ~(SOF_DontApplyPhysics | SOF_NISShip);
                    vecZeroVector(ship->obj.posinfo.velocity);
                    SET_MOVING_IMMOBILE(ship->obj.posinfo.isMoving);
                    ship = NULL;
                    break;
                case NSR_Derelict:
                    derelict = (Derelict *)NIS->objectsInMotion[index].spaceobj;
                    derelict->targetable.health = derelict->staticinfo->health.maxhealth;//make it 'vincible'
                    derelict->header.flags &= ~(SOF_DontApplyPhysics | SOF_NISShip);
                    vecZeroVector(derelict->obj.posinfo.velocity);
                    SET_MOVING_IMMOBILE(derelict->obj.posinfo.isMoving);
                    ship = NULL;
                    break;
                case NSR_Effect:
                    ship = (Ship *)NIS->objectsInMotion[index].spaceobj;

                    ship->header.flags &= ~(SOF_DontApplyPhysics | SOF_NISShip);
                    vecZeroVector(ship->obj.posinfo.velocity);
                    SET_MOVING_IMMOBILE(ship->obj.posinfo.isMoving);
                    ship = NULL;
                    break;
#if NIS_ERROR_CHECKING
                default:
                    dbgFatalf(DBG_Loc, "Don't know how to make race %d remainAtEnd.", NIS->header->objectPath[index].race);
#endif
            }
            if (ship != NULL)
            {
                bitClear(ship->header.flags, SOF_NISShip);
            }
        }
        else
        {
            switch (NIS->header->objectPath[index].race)
            {
                case RACE_Sect:                             // CAT: the seven Cataclysm ship races
                case RACE_Beast:
                case RACE_Kushan:
                case RACE_Taiidan:
                case RACE_P1:
                case RACE_Traders:
                case RACE_Derelict:
                    univReallyDeleteThisShipRightNow((Ship *)NIS->objectsInMotion[index].spaceobj);
                    break;
                case NSR_Asteroid:
                case NSR_DustCloud:
                case NSR_GasCloud:
                case NSR_Nebula:
                    univReallyDeleteThisResourceRightNow((Resource *)NIS->objectsInMotion[index].spaceobj);
                    break;
                case NSR_Derelict:
                    univReallyDeleteThisDerelictRightNow((Derelict *)NIS->objectsInMotion[index].spaceobj);
                    break;
                case NSR_Effect:
                    if (etgDeleteEffectsOwnedBy((Ship *)NIS->objectsInMotion[index].spaceobj) > 0)
                    {
#if NIS_VERBOSE_LEVEL > 2
                        dbgMessagef("NIS Effect not explicitly deleted.");
#endif
                    }
                    memFree(NIS->objectsInMotion[index].spaceobj);
                    NIS->objectsInMotion[index].spaceobj = NULL;    // CAT: the exe clears the pointer (0x004cdf4d)
                    break;
                case NSR_Generic:
                    univReallyDeleteThisDerelictRightNow((Derelict *)NIS->objectsInMotion[index].spaceobj);
                    break;
                case NSR_Missile:
                    ;                                       //don't bother deleting the missile because it'll just burn out and it may already have done so
                    break;
                default:;                                            //!!! add asteroids and other stuff
#if NIS_ERROR_CHECKING
                    dbgFatalf(DBG_Loc, "Undefined ship race for NIS spaceobj %d: %d.", index, NIS->header->objectPath[index].race);
#endif
            }
        }
        if (!bitTest(NIS->objectsInMotion[index].flags, OMF_CurvesDeleted))
        {
            for (j = 0; j < 6; j++)
            {                                                   //for each of x,y,z,h,p,b
                if (NIS->objectsInMotion[index].curve[j] != NULL)
                {                                               //delete all the spline curves that remain
                    //bsCurveDelete(NIS->objectsInMotion[index].curve[j]);
                }
                else
                {
                    break;
                }
            }
        }
    }
    memFree(NIS->objectSplines);                            //free the object spline pool
    if (!bCameraFocussed)
    {                                                       //if the script file didn't specify a focus point at the end
        //ccCopyCamera(&universe.mainCameraCommand,nisCamera);//focus the camera wherever it is
        nisCameraPrepareForCopy(nisCamera);
        nisCamera->oldlookatpoint = nisCamera->lookatpoint;
        nisCamera->distance = nisPreviousCameraDistance;
        cameraSetEyePosition(nisCamera);
        ccCopyCamera(&universe.mainCameraCommand,nisCamera);    //focus the camera from wherever it is

        if (universe.mainCameraCommand.currentCameraStack->focus.numShips)
        {
            ccFocusGeneral(&universe.mainCameraCommand, &universe.mainCameraCommand.currentCameraStack->focus, FALSE);
        }
        else
        {
            ccFocusOnMyMothership(&universe.mainCameraCommand);
        }
    }
    for (index = 0; index < NIS->nCameras; index++)
    {                                                       //for each camera
        memFree(NIS->camerasInMotion[index].cam);           //kill the camera
        for (j = 0; j < 6; j++)
        {                                                   //for each of x,y,z,h,p,b
            if (NIS->camerasInMotion[index].curve[j] != NULL)
            {                                               //delete all the spline curves that remain
                //bsCurveDelete(NIS->camerasInMotion[index].curve[j]);
            }
            else
            {
                break;
            }
        }
    }
    memFree(NIS->cameraSplines);

    nisIsRunning = FALSE;
    nisCaptureCamera = FALSE;
    nisCamera = NULL;

    if (nisMusicPlaying)
    {                                                       //if any music is playing
        // CAT: the NIS's music always stops at its end (nisStop 0x004cdd60 has no fast-forward test)
        soundEventStopMusic(0.0f);                          //stop it
        nisMusicPlaying = FALSE;
    }

    //!!! free any lights
#if NIS_RENDERLIST_DISABLE
    universe.dontUpdateRenderList = FALSE;                  //let automatic NIS updating go on
#endif
    memFree(NIS);

    nisTextCardIndex = 0;

    // CAT: the subtitles are updated once more and cleared (subTitlesUpdate, subReset; nisStop 0x004cdd60)
    subTitlesUpdate();
    subReset();

    //reset any possibly persistent game controls
    mrNISStopping();

    // enable unit cap counting after NIS finished
    unitCapEnable();

    if (!nisNewNISStarted)
    {
        soundEventGetVolume(&currentSFXVolume, &currentSpeechVolume, &currentMusicVolume);
        if (currentMusicVolume != nisPreviousMusicVolume)
        {
#if NIS_VERBOSE_LEVEL > 2
            dbgMessagef("Resetting Music volume from %.2f to %.2f", currentMusicVolume, nisPreviousMusicVolume);
#endif
            soundEventMusicVol(nisPreviousMusicVolume);
        }
        if (currentSpeechVolume != nisPreviousSpeechVolume)
        {
#if NIS_VERBOSE_LEVEL > 2
            dbgMessagef("Resetting Speech volume from %.2f to %.2f", currentSpeechVolume, nisPreviousSpeechVolume);
#endif
            soundEventSpeechVol(nisPreviousSpeechVolume);
        }
        if (currentSFXVolume != nisPreviousSFXVolume)
        {
#if NIS_VERBOSE_LEVEL > 2
            dbgMessagef("Resetting SFX volume from %.2f to %.2f", currentSFXVolume, nisPreviousSFXVolume);
#endif
            soundEventSFXVol(nisPreviousSFXVolume);
        }
        //restore previous lighting and backgrounds, if applicable
        if (nisPreviousLighting != NULL)
        {
            lightParseHSF(nisPreviousLighting);
            memFree(nisPreviousLighting);
        }
        if (nisPreviousBackground != NULL)
        {
            btgSetTheta(nisPreviousTheta);
            btgSetPhi(nisPreviousPhi);
            btgLoad(nisPreviousBackground);
            memFree(nisPreviousBackground);
        }

        nisBlackFade = nisBlackFadeDest = 0.0f;             //no fade is the default
        nisTextScrollDistance = 0.0f;

        if (nisUniverseHidden)
        {
            nisUniverseHideToggle(NULL, NULL);
        }
        if (nisUniversePause)
        {
            nisUniversePauseToggle(NULL, NULL);
        }
    }
    trkTrackValueRemoveAll();

    nisSMPTECounter = NULL;
    //nisStatic = NULL;
    memset(nisStatic, 0, sizeof(nisStatic));

    // CAT: the main-region camera is the default camera again (mrCamera = &defaultCamera 0x00ba1e00, nisStop
    //      0x004ce281)
    mrCamera = &defaultCamera;
}

/*-----------------------------------------------------------------------------
    Name        : nisSeek
    Description : Seek to an absolute point in an NIS.
    Inputs      : NIS - currently playing NIS to seek in.
                  seekTime - point to seek to, in seconds
    Outputs     : restarts all curves
    Return      :
----------------------------------------------------------------------------*/
#if NIS_SEEKABLE
void nisSeek(nisplaying *NIS, real32 seekTime)
{
    sdword index = 0, j = 0;
    objectmotion *path = NULL;
    cameramotion *camPath = NULL;
//    nisevent *event;
    real32 currentPos[6];
    Ship *newShip = NULL;
    vector position, startVector;
    real32 seekAmount = 0.0;
    bool32 seekRelative;
    splinecurve *curve;
/*
    vector xyz, xyzTemp, rot;
    matrix coordsys, tempCoordsys;
*/
    dbgAssertOrIgnore(seekTime >= 0);
    dbgAssertOrIgnore(seekTime < NIS->header->length);

    if (seekTime > NIS->timeElapsed)
    {
        seekRelative = TRUE;
        seekAmount = seekTime - NIS->timeElapsed;
    }
    else
    {
        seekRelative = FALSE;
    }

    position = NIS->nisPosition;                            //get position from NIS matrix
    //reset all the object motion paths
    for (index = 0; index < NIS->header->nObjectPaths; index++)
    {
        path = &NIS->objectsInMotion[index];                //get pointer to object motion path
        if (path->curve[0] != NULL)
        {                                                   //if NULL motion path or NULL object
            for (j = 0; j < 6; j++)
            {                                               //for each motion channel
                if (seekRelative)
                {
                    bsCurveUpdate(path->curve[j], seekAmount);
                }
                else
                {
                    bsCurveRestart(path->curve[j]);         //restart the curve
                    bsCurveUpdate(path->curve[j], seekTime);//update to a good point
                }
            }
        }
        if (bitTest(path->flags, OMF_ObjectDied))
        {                                                   //if object had died
            newShip = nisNewObjectCreate(NIS, index, NIS->header, &NIS->header->objectPath[index], NIS, &position);
            NIS->objectsInMotion[index].spaceobj = (SpaceObjRotImp *)newShip;
            curve = &NIS->objectSplines[6 * index];
            if (NIS->header->objectPath[index].nSamples != NIS_OneKeyframe)
            {                                               //restart the motion curves
                for (j = 0; j < 6; j++, curve++)
                {                                           //for each of x,y,z,h,p,b
                    path->curve[j] = curve;
                    bsCurveStartPrealloced(curve, NIS->header->objectPath[index].nSamples, NIS->header->objectPath[index].curve[j],
                            NIS->header->objectPath[index].times, NIS->header->objectPath[index].parameters);
                    currentPos[j] = bsCurveUpdate(path->curve[j], seekTime);//update to a good point
                }
            }

            // CAT-FIX (M2-R8 kas-runtime-mission01): HW1 and the exe (nisSeek 0x004ce2e0, local_18[6]) read currentPos[6]
            //      here (j after the loop, one past the array; unset for a one-keyframe path): ASan stack-buffer-overflow
            //      when the /debugScreenshots run's nisGoToEnd seeks the opening NIS. The first channel is tested instead
            if ((NIS->header->objectPath[index].nSamples != NIS_OneKeyframe) && (currentPos[0] != REALlyBig))
            {                                               //seek to the current position
                newShip->obj.posinfo.position.x = position.x - currentPos[2];
                newShip->obj.posinfo.position.y = position.y + currentPos[0];
                newShip->obj.posinfo.position.z = position.z + currentPos[1];

                startVector.x = currentPos[3];
                startVector.y = currentPos[4];
                startVector.z = currentPos[5];
                nisShipEulerToMatrix(&newShip->rotatable.rotinfo.coordsys, &startVector);

                univUpdateObjRotInfo((SpaceObjRot *)newShip);
            }
//            path->bObjectDied = FALSE;
            bitClear(path->flags, OMF_ObjectDied);
        }
    }
    //reset all the camera motion paths
    for (index = 0; index < NIS->header->nCameraPaths; index++)
    {
        camPath = &NIS->camerasInMotion[index];             //get pointer to camera motion path
        if (camPath->curve[0] == NULL)
        {                                                   //if NULL camera motion path
            continue;                                       //leave this camera where it was
        }
        for (j = 0; j < 6; j++)
        {                                                   //for each motion channel
            if (seekRelative)
            {
                bsCurveUpdate(camPath->curve[j], seekAmount); //update to a good point
            }
            else
            {
                bsCurveRestart(camPath->curve[j]);          //restart the curve
                bsCurveUpdate(camPath->curve[j], seekTime); //update to a good point
            }
        }
    }
    //seek to the current event
    if (!seekRelative)
    {
        NIS->iCurrentEvent = 0;
    }
    bitSet(NIS->header->flags, NIS_Seeked);
    /*
    event = &NIS->header->events[NIS->iCurrentEvent];
    nisSeeking = TRUE;
    while (NIS->iCurrentEvent < NIS->header->nEvents && event->time <= seekTime)
    {
        dbgAssertOrIgnore(nisEventDispatch[event->code] != NULL);
        if (event->code == NEO_Focus)
        {                                                   //special case for camera focus:rewind camera to the focus point
            camPath = &NIS->camerasInMotion[0];             //get pointer to camera motion path
            dbgAssertOrIgnore(camPath->curve[0] != NULL);
            for (j = 0; j < 6; j++)
            {                                               //for each motion channel
                bsCurveRestart(camPath->curve[j]);          //restart the curve
            }
            xyz.x = bsCurveUpdate(camPath->curve[0], event->time); //update the first curve
            //this chunk of code checks if the NIS loops
            dbgAssertOrIgnore(xyz.x != REALlyBig);                  //if curve ends
            dbgAssertOrIgnore(!_isnan((double)xyz.x));
            //get rest of motion path
            xyz.y = bsCurveUpdate(camPath->curve[1], event->time);
            xyz.z = bsCurveUpdate(camPath->curve[2], event->time);
            dbgAssertOrIgnore(!_isnan((double)xyz.y));
            dbgAssertOrIgnore(!_isnan((double)xyz.z));
            //camera rotation: X, Y, Z (mode 0)
            rot.x = bsCurveUpdate(camPath->curve[3], event->time);
            rot.y = bsCurveUpdate(camPath->curve[4], event->time);
            rot.z = bsCurveUpdate(camPath->curve[5], event->time);
            dbgAssertOrIgnore(xyz.y != REALlyBig && xyz.z != REALlyBig && rot.x != REALlyBig && rot.y != REALlyBig && rot.z != REALlyBig);
            //camera position: -Z, X, Y (UZI mode 33)
            xyzTemp.x = -xyz.z;
            xyzTemp.y = xyz.x;
            xyzTemp.z = xyz.y;
            dbgAssertOrIgnore(!_isnan((double)xyzTemp.x));
            dbgAssertOrIgnore(!_isnan((double)xyzTemp.y));
            dbgAssertOrIgnore(!_isnan((double)xyzTemp.z));
            matMultiplyMatByVec(&xyz, &NIS->nisMatrix, &xyzTemp);//multiply NIS world matrix
            camPath->cam->eyeposition.x = NIS->nisPosition.x + xyz.x;
            camPath->cam->eyeposition.y = NIS->nisPosition.y + xyz.y;
            camPath->cam->eyeposition.z = NIS->nisPosition.z + xyz.z;
            nisShipEulerToMatrix(&tempCoordsys, &rot);
            matMultiplyMatByMat(&coordsys, &NIS->nisMatrix, &tempCoordsys);//multiply NIS world matrix
            matGetVectFromMatrixCol2(xyz, coordsys);
            matGetVectFromMatrixCol1(camPath->cam->upvector, coordsys);
            vecAdd(camPath->cam->lookatpoint, xyz, camPath->cam->eyeposition);
        }
        nisEventDispatch[event->code](NIS, event);
        NIS->iCurrentEvent++;
        event++;
    }
    nisSeeking = FALSE;
        */
    //!!! reset all the light motion paths
    NIS->timeElapsed = seekTime;
}
#endif

/*-----------------------------------------------------------------------------
    Name        : nisGoToEnd
    Description : Causes NIS to go to end of NIS
    Inputs      : NIS
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisGoToEnd(nisplaying *NIS)
{
    nisSeek(NIS,NIS->header->length - UNIVERSE_UPDATE_PERIOD);
}

/*-----------------------------------------------------------------------------
    Name        : nisObjectHierarchyMatrixCompute
    Description : Compute the matrix for an NIS hierarchy object.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisObjectHierarchyMatrixCompute(objectmotion *paths, sdword index, matrix *mat, vector *vec)
{
    matrix temp;
    vector tempVec;
    if (paths[index].parentIndex != -1)
    {
        nisObjectHierarchyMatrixCompute(paths, paths[index].parentIndex, &temp, &tempVec);
        matMultiplyMatByMat(mat, &paths[index].spaceobj->rotatable.rotinfo.coordsys, &temp);
        vecAdd(*vec, paths[index].spaceobj->obj.posinfo.position, tempVec);
    }
    else
    {
        *mat = paths[index].spaceobj->rotatable.rotinfo.coordsys;
        *vec = paths[index].spaceobj->obj.posinfo.position;
    }
}

/*-----------------------------------------------------------------------------
    Name        : nisPause
    Description : Pause playback of an NIS
    Inputs      : bPause - TRUE if you want to pause an NIS
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisPause(bool32 bPause)
{
    nisPaused = bPause;
}

/*-----------------------------------------------------------------------------
    Name        : nisCamPathUpdate
    Description : Update the camera motion for a given camera.
    Inputs      : NIS - what NIS this camera path belongs to
                  camPath - camera motion info
                  timeElapsed - relative amount to advance motion path
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void nisCamPathUpdate(nisplaying *NIS, cameramotion *camPath, real32 timeElapsed)
{
    vector xyz, xyzTemp, rot;
    matrix coordsys, tempCoordsys;

    dbgAssertOrIgnore(camPath->curve[0] != NULL);
    xyz.x = bsCurveUpdate(camPath->curve[0], timeElapsed); //update the first curve
    //this chunk of code checks if the NIS loops
//    dbgAssertOrIgnore(xyz.x != REALlyBig);                  //if curve ends
    if (xyz.x == REALlyBig)                             //if curve ends
    {                                                   //(all curves should end at same time)
        return;
    }
    dbgAssertOrIgnore(!isnan((double)xyz.x));
    //get rest of motion path
    xyz.y = bsCurveUpdate(camPath->curve[1], timeElapsed);
    xyz.z = bsCurveUpdate(camPath->curve[2], timeElapsed);
    dbgAssertOrIgnore(!isnan((double)xyz.y));
    dbgAssertOrIgnore(!isnan((double)xyz.z));
    //camera rotation: X, Y, Z (mode 0)
    rot.x = bsCurveUpdate(camPath->curve[3], timeElapsed);
    rot.y = bsCurveUpdate(camPath->curve[4], timeElapsed);
    rot.z = bsCurveUpdate(camPath->curve[5], timeElapsed);
    dbgAssertOrIgnore(xyz.y != REALlyBig && xyz.z != REALlyBig && rot.x != REALlyBig && rot.y != REALlyBig && rot.z != REALlyBig);
    //camera position: -Z, X, Y (UZI mode 33)
    xyzTemp.x = -xyz.z;
    xyzTemp.y = xyz.x;
    xyzTemp.z = xyz.y;
    dbgAssertOrIgnore(!isnan((double)xyzTemp.x));
    dbgAssertOrIgnore(!isnan((double)xyzTemp.y));
    dbgAssertOrIgnore(!isnan((double)xyzTemp.z));
    matMultiplyMatByVec(&xyz, &NIS->nisMatrix, &xyzTemp);//multiply NIS world matrix
    camPath->cam->eyeposition.x = NIS->nisPosition.x + xyz.x;
    camPath->cam->eyeposition.y = NIS->nisPosition.y + xyz.y;
    camPath->cam->eyeposition.z = NIS->nisPosition.z + xyz.z;
    nisShipEulerToMatrix(&tempCoordsys, &rot);
    matMultiplyMatByMat(&coordsys, &NIS->nisMatrix, &tempCoordsys);//multiply NIS world matrix
    matGetVectFromMatrixCol2(xyz, coordsys);
    vecMultiplyByScalar(xyz, NIS_LookScalar);
    matGetVectFromMatrixCol1(camPath->cam->upvector, coordsys);
    vecAdd(camPath->cam->lookatpoint, xyz, camPath->cam->eyeposition);
#if NIS_VERBOSE_LEVEL >= 4
        dbgMessagef("Camera: (%.2f, %.2f, %.2f) angles (%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f).", camPath->cam->eyeposition.x, camPath->cam->eyeposition.y, camPath->cam->eyeposition.z, rot.x, rot.y, rot.z, xyz.x, xyz.y, xyz.z);
#endif//NIS_VERBOSE_LEVEL
    // CAT: the render lists follow the moved camera at once (nisCamPathUpdate 0x004ce600)
    univUpdateRenderList();
    univUpdateMinorRenderList();
}

/*=============================================================================
    HWI-FEATURE(motion-interp): render-time evaluation of an NIS (not in the
    exe; docs/features/motion-interpolation.md items 1-2, MotionInterp.c). The
    transforms nisUpdate / nisCamPathUpdate / nisCameraFlyCompute would compute
    if the NIS were advanced by dt now, computed with bsCurveEvalAtBeforeJump
    (bsCurveEvalAt, which does not advance the curves, with the jumps of the
    next tick - camera cuts, re-placed objects - left to that tick) into the
    caller's variables. Nothing of the NIS or of its objects is written:
    nisUpdate stays the only tick-time writer.
=============================================================================*/
// an NIS-driven object's position and coordinate system dt after its curves' current time (nisUpdate's arithmetic);
// FALSE for an object nisUpdate does not move
bool32 nisRenderObjectAt(nisplaying *NIS, objectmotion *path, real32 dt, vector *position, matrix *coordsys)   // HWI-FEATURE(motion-interp)
{                                                                       // HWI-FEATURE(motion-interp)
    vector xyz, xyzTemp, rot;                                           // HWI-FEATURE(motion-interp)
    matrix tempCoordsys;                                                // HWI-FEATURE(motion-interp)
                                                                        // HWI-FEATURE(motion-interp)
    if (bitTest(path->flags, OMF_CurvesDeleted) || path->curve[0] == NULL || path->spaceobj == NULL)   // HWI-FEATURE(motion-interp)
    {                                                                   // HWI-FEATURE(motion-interp)
        return FALSE;                                                   // HWI-FEATURE(motion-interp)
    }                                                                   // HWI-FEATURE(motion-interp)
    xyz.x = bsCurveEvalAtBeforeJump(path->curve[0], path->curve[0]->timeElapsed + dt);   // HWI-FEATURE(motion-interp)
    if (xyz.x == REALlyBig)                                             // HWI-FEATURE(motion-interp)
    {                                                                   // HWI-FEATURE(motion-interp)
        return FALSE;                                                   // HWI-FEATURE(motion-interp)
    }                                                                   // HWI-FEATURE(motion-interp)
    xyz.y = bsCurveEvalAtBeforeJump(path->curve[1], path->curve[1]->timeElapsed + dt);   // HWI-FEATURE(motion-interp)
    xyz.z = bsCurveEvalAtBeforeJump(path->curve[2], path->curve[2]->timeElapsed + dt);   // HWI-FEATURE(motion-interp)
    rot.x = bsCurveEvalAtBeforeJump(path->curve[3], path->curve[3]->timeElapsed + dt);   // HWI-FEATURE(motion-interp)
    rot.y = bsCurveEvalAtBeforeJump(path->curve[4], path->curve[4]->timeElapsed + dt);   // HWI-FEATURE(motion-interp)
    rot.z = bsCurveEvalAtBeforeJump(path->curve[5], path->curve[5]->timeElapsed + dt);   // HWI-FEATURE(motion-interp)
    nisShipEulerToMatrix(&tempCoordsys, &rot);                          // HWI-FEATURE(motion-interp)
    matMultiplyMatByMat(coordsys, &NIS->nisMatrix, &tempCoordsys);      // HWI-FEATURE(motion-interp)
    xyzTemp.x = -xyz.z;                                                 // HWI-FEATURE(motion-interp): UZI mode 33
    xyzTemp.y = xyz.x;                                                  // HWI-FEATURE(motion-interp)
    xyzTemp.z = xyz.y;                                                  // HWI-FEATURE(motion-interp)
    matMultiplyMatByVec(&xyz, &NIS->nisMatrix, &xyzTemp);               // HWI-FEATURE(motion-interp)
    vecAdd(*position, NIS->nisPosition, xyz);                           // HWI-FEATURE(motion-interp)
    return TRUE;                                                        // HWI-FEATURE(motion-interp)
}                                                                       // HWI-FEATURE(motion-interp)

// the NIS camera's eye, look-at point and up vector dt after its curves' current time (nisCamPathUpdate's arithmetic)
bool32 nisRenderCameraAt(nisplaying *NIS, cameramotion *camPath, real32 dt, Camera *cam)   // HWI-FEATURE(motion-interp)
{                                                                       // HWI-FEATURE(motion-interp)
    vector xyz, xyzTemp, rot;                                           // HWI-FEATURE(motion-interp)
    matrix coordsys, tempCoordsys;                                      // HWI-FEATURE(motion-interp)
                                                                        // HWI-FEATURE(motion-interp)
    if (camPath->curve[0] == NULL)                                      // HWI-FEATURE(motion-interp)
    {                                                                   // HWI-FEATURE(motion-interp)
        return FALSE;                                                   // HWI-FEATURE(motion-interp)
    }                                                                   // HWI-FEATURE(motion-interp)
    xyz.x = bsCurveEvalAtBeforeJump(camPath->curve[0], camPath->curve[0]->timeElapsed + dt);   // HWI-FEATURE(motion-interp)
    if (xyz.x == REALlyBig)                                             // HWI-FEATURE(motion-interp)
    {                                                                   // HWI-FEATURE(motion-interp)
        return FALSE;                                                   // HWI-FEATURE(motion-interp)
    }                                                                   // HWI-FEATURE(motion-interp)
    xyz.y = bsCurveEvalAtBeforeJump(camPath->curve[1], camPath->curve[1]->timeElapsed + dt);   // HWI-FEATURE(motion-interp)
    xyz.z = bsCurveEvalAtBeforeJump(camPath->curve[2], camPath->curve[2]->timeElapsed + dt);   // HWI-FEATURE(motion-interp)
    rot.x = bsCurveEvalAtBeforeJump(camPath->curve[3], camPath->curve[3]->timeElapsed + dt);   // HWI-FEATURE(motion-interp)
    rot.y = bsCurveEvalAtBeforeJump(camPath->curve[4], camPath->curve[4]->timeElapsed + dt);   // HWI-FEATURE(motion-interp)
    rot.z = bsCurveEvalAtBeforeJump(camPath->curve[5], camPath->curve[5]->timeElapsed + dt);   // HWI-FEATURE(motion-interp)
    xyzTemp.x = -xyz.z;                                                 // HWI-FEATURE(motion-interp): UZI mode 33
    xyzTemp.y = xyz.x;                                                  // HWI-FEATURE(motion-interp)
    xyzTemp.z = xyz.y;                                                  // HWI-FEATURE(motion-interp)
    matMultiplyMatByVec(&xyz, &NIS->nisMatrix, &xyzTemp);               // HWI-FEATURE(motion-interp)
    cam->eyeposition.x = NIS->nisPosition.x + xyz.x;                    // HWI-FEATURE(motion-interp)
    cam->eyeposition.y = NIS->nisPosition.y + xyz.y;                    // HWI-FEATURE(motion-interp)
    cam->eyeposition.z = NIS->nisPosition.z + xyz.z;                    // HWI-FEATURE(motion-interp)
    nisShipEulerToMatrix(&tempCoordsys, &rot);                          // HWI-FEATURE(motion-interp)
    matMultiplyMatByMat(&coordsys, &NIS->nisMatrix, &tempCoordsys);     // HWI-FEATURE(motion-interp)
    matGetVectFromMatrixCol2(xyz, coordsys);                            // HWI-FEATURE(motion-interp)
    vecMultiplyByScalar(xyz, NIS_LookScalar);                           // HWI-FEATURE(motion-interp)
    matGetVectFromMatrixCol1(cam->upvector, coordsys);                  // HWI-FEATURE(motion-interp)
    vecAdd(cam->lookatpoint, xyz, cam->eyeposition);                    // HWI-FEATURE(motion-interp)
    return TRUE;                                                        // HWI-FEATURE(motion-interp)
}                                                                       // HWI-FEATURE(motion-interp)

// the camera fly-in of an NIS that starts with a camera blend (nisCameraCutTime != 0), dt after its curves' current
// time (nisCameraFlyCompute's arithmetic)
bool32 nisRenderCameraFlyAt(real32 dt, Camera *cam)                     // HWI-FEATURE(motion-interp)
{                                                                       // HWI-FEATURE(motion-interp)
    vector eye, look, up;                                               // HWI-FEATURE(motion-interp)
    sdword j;                                                           // HWI-FEATURE(motion-interp)
    real32 v[9];                                                        // HWI-FEATURE(motion-interp)
                                                                        // HWI-FEATURE(motion-interp)
    for (j = 0; j < 9; j++)                                             // HWI-FEATURE(motion-interp)
    {                                                                   // HWI-FEATURE(motion-interp)
        if (nisCameraCurve[j] == NULL)                                  // HWI-FEATURE(motion-interp)
        {                                                               // HWI-FEATURE(motion-interp)
            return FALSE;                                               // HWI-FEATURE(motion-interp)
        }                                                               // HWI-FEATURE(motion-interp)
        v[j] = bsCurveEvalAtBeforeJump(nisCameraCurve[j], nisCameraCurve[j]->timeElapsed + dt);   // HWI-FEATURE(motion-interp)
        if (v[j] == REALlyBig)                                          // HWI-FEATURE(motion-interp)
        {                                                               // HWI-FEATURE(motion-interp)
            return FALSE;                                               // HWI-FEATURE(motion-interp)
        }                                                               // HWI-FEATURE(motion-interp)
    }                                                                   // HWI-FEATURE(motion-interp)
    eye.x = v[0]; eye.y = v[1]; eye.z = v[2];                           // HWI-FEATURE(motion-interp)
    look.x = v[3]; look.y = v[4]; look.z = v[5];                        // HWI-FEATURE(motion-interp)
    up.x = v[6]; up.y = v[7]; up.z = v[8];                              // HWI-FEATURE(motion-interp)
    vecNormalize(&up);                                                  // HWI-FEATURE(motion-interp)
    cam->eyeposition = eye;                                             // HWI-FEATURE(motion-interp)
    cam->lookatpoint = look;                                            // HWI-FEATURE(motion-interp)
    cam->upvector = up;                                                 // HWI-FEATURE(motion-interp)
    return TRUE;                                                        // HWI-FEATURE(motion-interp)
}                                                                       // HWI-FEATURE(motion-interp)

/*-----------------------------------------------------------------------------
    Name        : nisUpdate
    Description : Update an NIS for a given time interval
    Inputs      : NIS - nis to update
                  timeElapsed - time since NIS last updated or created
    Outputs     : updates all the splines in the NIS
    Return      : Total time elapsed in the NIS, or REALlyBig if it's all done.
----------------------------------------------------------------------------*/
real32 nisUpdate(nisplaying *NIS, real32 timeElapsed)
{
    vector xyz, xyzTemp, rot, nisPos, tempVect, lastPos;
    matrix coordsys, tempCoordsys;//, temp;
    objectmotion *path;
    cameramotion *camPath;
    sdword index, j;
    nisevent *event;

    // CAT: the NIS waits while a movie plays (nisUpdate 0x004ce780 tests binkDonePlaying 0x008b6d94; GoK plays
    //      movies with its avi player, whose aviIsPlaying is the movie-on flag, as SpeechEvent.c's Bink term)
    if (aviIsPlaying)
    {
        return 0.0000001f;
    }

#if NIS_CAMERA_RELEASE
    if (keyIsStuck(NIS_CAMERA_RELEASE))
    {
        keyClearSticky(NIS_CAMERA_RELEASE);
        nisCaptureCamera ^= TRUE;
    }
#endif

#if NIS_TIME_CONTROLS
    if (keyIsStuck(NIS_Pause))
    {                                                       //pause/unpause the game
        keyClearSticky(NIS_Pause);
        nisPaused ^= TRUE;
        dbgMessagef("NIS playback %s", nisPaused ? "PAUSED" : "UNPAUSED");
    }
#if NIS_SEEKABLE
    if (nisPaused)
    {
        timeElapsed = 0.0000001f;
    }
    if (keyIsStuck(NIS_Back1Second))
    {
        keyClearSticky(NIS_Back1Second);
        nisSeek(NIS, max(0, NIS->timeElapsed - 1.0f));
        dbgMessagef("NIS seek backward 1 second.");
    }
    if (keyIsStuck(NIS_Back10Second))
    {
        keyClearSticky(NIS_Back10Second);
        nisSeek(NIS, max(0, NIS->timeElapsed - 10.0f));
        dbgMessagef("NIS seek backward 10 seconds.");
    }
#endif
    if (keyIsStuck(NIS_FastForward))
    {
        keyClearSticky(NIS_FastForward);
        if (keyIsHit(CONTROLKEY))
        {
            nisPlayFactor /= 2.0f;
        }
        else
        {
            nisPlayFactor *= 2.0f;
        }
        dbgMessagef("NIS playback speed x%.2f.", nisPlayFactor);
    }
    if (keyIsStuck(NIS_NormalSpeed))
    {
        keyClearSticky(NIS_NormalSpeed);
        nisPlayFactor = 1.0f;
        dbgMessagef("NIS playback NORMAL speed.", nisPlayFactor);
    }

    timeElapsed *= nisPlayFactor;
#endif
    nisPos = NIS->nisPosition;                              //get position from NIS matrix
    for (index = 0; index < NIS->nObjects; index++)
    {                                                       //for each object in motion NIS
        path = &NIS->objectsInMotion[index];                //get pointer to object motion path
        if (bitTest(path->flags, OMF_CurvesDeleted))
        {                                                   //if the object is already dead
            continue;
        }
        if (path->curve[0] == NULL)
        {                                                   //if NULL motion path or NULL object
            SET_MOVING_IMMOBILE(path->spaceobj->obj.posinfo.isMoving);
            continue;                                       //leave this object where it was
        }
        xyz.x = bsCurveUpdate(path->curve[0], timeElapsed); //update the first curve
        //this chunk of code checks if the NIS loops
        if (xyz.x == REALlyBig)                             //if curve ends
        {                                                   //(all curves should end at same time)
            SET_MOVING_IMMOBILE(path->spaceobj->obj.posinfo.isMoving);
            continue;                                       //don't update this one anymore
        }
        //get rest of motion path
        xyz.y = bsCurveUpdate(path->curve[1], timeElapsed);
        xyz.z = bsCurveUpdate(path->curve[2], timeElapsed);
        rot.x = bsCurveUpdate(path->curve[3], timeElapsed);
        rot.y = bsCurveUpdate(path->curve[4], timeElapsed);
        rot.z = bsCurveUpdate(path->curve[5], timeElapsed);
        //verify none of these curves expired
        dbgAssertOrIgnore(xyz.y != REALlyBig && xyz.z != REALlyBig && rot.x != REALlyBig && rot.y != REALlyBig && rot.z != REALlyBig);

        nisShipEulerToMatrix(&tempCoordsys, &rot);          //multiply NIS world matrix
        matMultiplyMatByMat(&coordsys, &NIS->nisMatrix, &tempCoordsys);
        //stuff the new coordinate system and position into the spaceobj
        lastPos = path->spaceobj->obj.posinfo.position;
        //object position:-Z, X, Y (UZI position mode 33)
        xyzTemp.x = -xyz.z;
        xyzTemp.y = xyz.x;
        xyzTemp.z = xyz.y;
        matMultiplyMatByVec(&xyz, &NIS->nisMatrix, &xyzTemp);//multiply NIS world matrix
        if (path->spaceobj->header.objtype == OBJ_ShipType)
        {                                                   // CAT: a ship moves through cat_univSetShipPosition
            vecAdd(tempVect, nisPos, xyz);
            cat_univSetShipPosition((Ship *)path->spaceobj, &tempVect);
        }
        else
        {
            path->spaceobj->obj.posinfo.position.x = nisPos.x + xyz.x;
            path->spaceobj->obj.posinfo.position.y = nisPos.y + xyz.y;
            path->spaceobj->obj.posinfo.position.z = nisPos.z + xyz.z;
        }
        path->spaceobj->rotatable.rotinfo.coordsys = coordsys;        //set new object coordinate system
        if (path->spaceobj->header.objtype == OBJ_ShipType)
        {
            hvector lightPos;
            Ship *nisShip = (Ship *)path->spaceobj;

            // CAT: its clamped parts follow, and the NIS light / lens flare source ships move the two lights and
            //      the NIS lens flare (nisUpdate 0x004ce780: cat_clampUpdateAttachedObjects, cat_lightSetPosition,
            //      cat_rndLensFlareNISSourceSetPosition with w = 0)
            cat_clampUpdateAttachedObjects((SpaceObjRotImpTargGuidance *)nisShip);
            lightPos.x = nisShip->obj.posinfo.position.x;
            lightPos.y = nisShip->obj.posinfo.position.y;
            lightPos.z = nisShip->obj.posinfo.position.z;
            lightPos.w = 0.0f;
            if (nisShip->shiptype == nisLightSource1)
            {
                cat_lightSetPosition(0, (real32 *)&lightPos);
            }
            else if (nisShip->shiptype == nisLightSource2)
            {
                cat_lightSetPosition(1, (real32 *)&lightPos);
            }
            else if (nisShip->shiptype == nisLensflareSource1)
            {
                cat_rndLensFlareNISSourceSetPosition(0, (vector *)&lightPos);
            }
        }

        //get velocity of ship
        vecSub(tempVect, path->spaceobj->obj.posinfo.position, lastPos);
        //kill the trails if the ship has jumped to new queue point
        if (path->spaceobj->header.objtype == OBJ_ShipType &&
            getVectDistSloppy(tempVect) >= NIS_JumpDistance)
        {
            Ship* ship = (Ship*)path->spaceobj;
            for (j = 0; j < MAX_NUM_TRAILS; j++)
            {
                if (ship->trail[j] != NULL)
                {
                    trailZeroLength(ship->trail[j]);
                }
            }
        }
        //cap the velocity vector of the ship
        if (ABS(tempVect.x) + ABS(tempVect.y) + ABS(tempVect.z) > 0.0001f) // only mark object as moving if it's actually moving in a "meaningful" quantity
        {
            if (NIS->header->objectPath[index].type < NUM_RACES)
            {
                vecCapVector(&tempVect, path->spaceobj->obj.staticinfo->maxvelocity);
            }
            SET_MOVING_LINEARLY(path->spaceobj->obj.posinfo.isMoving);
        }
        else
        {
            SET_MOVING_IMMOBILE(path->spaceobj->obj.posinfo.isMoving);
        }
        path->spaceobj->obj.posinfo.velocity = tempVect;
        if (path->spaceobj->header.flags & SOF_Rotatable)
        {
            univUpdateObjRotInfo(&path->spaceobj->rotatable);
        }
#if NIS_VERBOSE_LEVEL >= 4
        dbgMessagef("NIS object at 0x%x at (%.2f, %.2f, %.2f) angles (%.2f, %.2f, %.2f).", path->spaceobj, path->spaceobj->obj.posinfo.position.x, path->spaceobj->obj.posinfo.position.y, path->spaceobj->obj.posinfo.position.z, rot.x, rot.y, rot.z);
#endif
    }
    for (index = 0; index < NIS->nCameras; index++)
    {                                                       //for each camera motion NIS
        camPath = &NIS->camerasInMotion[index];             //get pointer to object motion camPath
        if (camPath->curve[0] == NULL)
        {                                                   //if NULL motion camPath
            continue;                                       //leave this object where it was
        }
        nisCamPathUpdate(NIS, camPath, timeElapsed);
/*
        xyz.x = bsCurveUpdate(camPath->curve[0], timeElapsed); //update the first curve
        //this chunk of code checks if the NIS loops
        if (xyz.x == REALlyBig)                             //if curve ends
        {                                                   //(all curves should end at same time)
            continue;
        }
        dbgAssertOrIgnore(!_isnan((double)xyz.x));
        //get rest of motion path
        xyz.y = bsCurveUpdate(camPath->curve[1], timeElapsed);
        xyz.z = bsCurveUpdate(camPath->curve[2], timeElapsed);
        dbgAssertOrIgnore(!_isnan((double)xyz.y));
        dbgAssertOrIgnore(!_isnan((double)xyz.z));
        //camera rotation: X, Y, Z (mode 0)
        rot.x = bsCurveUpdate(camPath->curve[3], timeElapsed);
        rot.y = bsCurveUpdate(camPath->curve[4], timeElapsed);
        rot.z = bsCurveUpdate(camPath->curve[5], timeElapsed);
        dbgAssertOrIgnore(xyz.y != REALlyBig && xyz.z != REALlyBig && rot.x != REALlyBig && rot.y != REALlyBig && rot.z != REALlyBig);
        //camera position: -Z, X, Y (UZI mode 33)
        xyzTemp.x = -xyz.z;
        xyzTemp.y = xyz.x;
        xyzTemp.z = xyz.y;
        dbgAssertOrIgnore(!_isnan((double)xyzTemp.x));
        dbgAssertOrIgnore(!_isnan((double)xyzTemp.y));
        dbgAssertOrIgnore(!_isnan((double)xyzTemp.z));
        matMultiplyMatByVec(&xyz, &NIS->nisMatrix, &xyzTemp);//multiply NIS world matrix
        camPath->cam->eyeposition.x = nisPos.x + xyz.x;
        camPath->cam->eyeposition.y = nisPos.y + xyz.y;
        camPath->cam->eyeposition.z = nisPos.z + xyz.z;
        nisShipEulerToMatrix(&tempCoordsys, &rot);
        matMultiplyMatByMat(&coordsys, &NIS->nisMatrix, &tempCoordsys);//multiply NIS world matrix
        matGetVectFromMatrixCol2(xyz, coordsys);
        matGetVectFromMatrixCol1(camPath->cam->upvector, coordsys);
        vecAdd(camPath->cam->lookatpoint, xyz, camPath->cam->eyeposition);
#if NIS_VERBOSE_LEVEL >= 4
        dbgMessagef("Camera: (%.2f, %.2f, %.2f) angles (%.2f, %.2f, %.2f), (%.2f, %.2f, %.2f).", camPath->cam->eyeposition.x, camPath->cam->eyeposition.y, camPath->cam->eyeposition.z, rot.x, rot.y, rot.z, xyz.x, xyz.y, xyz.z);
#endif//NIS_VERBOSE_LEVEL
*/
    }

    //process all the events pending
    event = &NIS->header->events[NIS->iCurrentEvent];
    if (bitTest(NIS->header->flags, NIS_Seeked))
    {                                                       //if we have to seek some events
        bitClear(NIS->header->flags, NIS_Seeked);
        nisSeeking = TRUE;
        while (NIS->iCurrentEvent < NIS->header->nEvents && event->time <= NIS->timeElapsed)
        {                                                   //execute all seekable events pending
            dbgAssertOrIgnore(nisEventDispatch[event->code] != NULL);
            nisEventDispatch[event->code](NIS, event);
            if (nisNewNISStarted)
            {
                nisNewNISStarted = FALSE;
                return(0.0f);
            }
            NIS->iCurrentEvent++;
            event++;
        }
        nisSeeking = FALSE;
    }
    //now process the regular events pending
    while (NIS->iCurrentEvent < NIS->header->nEvents && event->time <= NIS->timeElapsed + timeElapsed)
    {
        dbgAssertOrIgnore(nisEventDispatch[event->code] != NULL);
        nisEventDispatch[event->code](NIS, event);
        if (nisNewNISStarted)
        {
            nisNewNISStarted = FALSE;
            return(0.0f);
        }
        NIS->iCurrentEvent++;
        event++;
    }
#if NIS_PRINT_INFO
    if (keyIsStuck(NIS_PRINT_INFO))
    {                                                       //toggle time display
        keyClearSticky(NIS_PRINT_INFO);
        nisPrintInfo ^= TRUE;
        dbgMessagef("NIS info printing %s", nisPrintInfo ? "ON" : "OFF");
    }
    if (nisPrintInfo)
    {
        sprintf(nisInfoString, "NIS time = %.2f of %.2f, frame = %.0f of %.0f", NIS->timeElapsed, NIS->header->length, NIS->timeElapsed * NIS_FrameRate, NIS->header->length * NIS_FrameRate);
        if (selSelected.numShips == 1)
        {                                                   //print index of selected ship
            char string[256], *pString;

            for (index = 0; index < NIS->nObjects; index++)
            {                                               //for each object in motion NIS
                if ((Ship *)NIS->objectsInMotion[index].spaceobj == selSelected.ShipPtr[0])
                {
                    strcat(nisInfoString, " Selected: '");
                    if (NIS->objectsInMotion[index].spaceobj->header.objtype == OBJ_ShipType)
                    {                                       //if it's a ship
                        pString = NisRaceToStr(((Ship *)NIS->objectsInMotion[index].spaceobj)->shiprace);
                        if (pString != NULL)
                        {
                            strcat(nisInfoString, pString);
#ifdef _WIN32
                            strcat(nisInfoString, "\\");
#else
                            strcat(nisInfoString, "/");
#endif
                        }
                    }
                    strcat(nisInfoString, ShipTypeToStr(((Ship *)NIS->objectsInMotion[index].spaceobj)->shiptype));
                    if (NIS->header->objectPath[index].instance != 0)
                    {
                        sprintf(string, " (%d)", NIS->header->objectPath[index].instance);
                        strcat(nisInfoString, string);
                    }
                    strcat(nisInfoString, "'");
                    break;
                }
            }
        }
    }
#endif
    //update the time of the NIS and restart it if it ends
    NIS->timeElapsed += timeElapsed;
    if (NIS->timeElapsed >= NIS->header->length)
    {                                                       //if NIS times out
#if NIS_SEEKABLE
        if (NIS->header->loop >= 0)
        {                                                   //if we should loop this NIS
            nisSeek(NIS, NIS->header->loop);
        }
        else
#endif
        {
            return(REALlyBig);
        }
    }
    return(NIS->timeElapsed);
}

/*-----------------------------------------------------------------------------
    Name        : nisDeleteTheseCurves
    Description : Delete this NIS-ship and it's motion paths
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisDeleteTheseCurves(objectmotion *path)
{
    sdword j;

    for (j = 0; j < 6; j++)
    {                                                   //for each of x,y,z,h,p,b
        if (path->curve[j] != NULL)
        {                                               //delete all the spline curves that remain
            //bsCurveDelete(path->curve[j]);
            path->curve[j] = NULL;
        }
    }
    path->flags |= OMF_CurvesDeleted;
}


/*-----------------------------------------------------------------------------
    Name        : nisFindObjectByName
    Description : Find the currently named object in the specified NIS.
    Inputs      : header - NIS to look through.
                  name - name of object in the format:
                    "[<race>\]<lwoFileName>[ (<instance>)]"
    Outputs     :
    Return      : Index of object.
----------------------------------------------------------------------------*/
sdword nisFindObjectByName(nisheader *header, char *name)
{
    sdword index, instance = 0;
    udword race = RACE_Sect;
//    ShipType type;
    char *pSlash = NULL, *pType = NULL;
    char raceString[32];
    char effectName[256];
    spaceobjpath *path = NULL;

    if ((pSlash = strchr(name, '(')) != NULL)
    {                                                       //find the instance ID, if any
        sscanf(pSlash, "(%d)", &instance);
        *(pSlash - 1) = 0;                                  //get rid of the instance ID
    }
    /*if ((pSlash = strchr(name, '\\')) != NULL || (pSlash = strchr(name, '/')) != NULL)*/
    if ((pSlash = strpbrk(name, "\\/")) != NULL)
    {                                                       //find the race
        memcpy(raceString, name, pSlash - name);
        raceString[pSlash - name] = 0;
        race = StrToNisRace(raceString);
        if (bitTest(header->flags, NHF_RacesSwapped))
        {
            if (race == RACE_Sect)
            {
                race = RACE_Beast;
            }
            else if (race == RACE_Beast)
            {
                race = RACE_Sect;
            }
        }
        dbgAssertOrIgnore(race != -1);
        pSlash++;                                           //point to first character of object name
    }
    else
    {
        pSlash = name;
    }
    if (!strcasecmp(pSlash, "Missile"))
    {                                                       //if we're looking for a missile
        race = NSR_Missile;
    }
//    type = StrToShipType(pSlash);                           //get the type

    path = header->objectPath;
    for (index = 0; index < header->nObjectPaths; index++, path++)
    {                                                       //look at all object paths
        /*
        if (path->instance == instance && path->race == race && path->type == type)
        {                                                   //if matching object criteria
            return(index);
        }
        */
        if (path->instance == instance && path->race == race)
        {                                                   //if instance and race match
            switch (race)
            {
                case RACE_Sect:                             // CAT: the seven Cataclysm ship races
                case RACE_Beast:                            //      (nisFindObjectByName 0x004ced60 cases 0..6)
                case RACE_Kushan:
                case RACE_Taiidan:
                case RACE_P1:
                case RACE_Traders:
                case RACE_Derelict:
                    pType = ShipTypeToStr(path->type);
                    break;
                case NSR_Asteroid:
                    pType = AsteroidTypeToStr(path->type);
                    break;
                case NSR_DustCloud:
                    pType = DustCloudTypeToStr(path->type);
                    break;
                case NSR_GasCloud:
                    pType = GasCloudTypeToStr(path->type);
                    break;
                case NSR_Nebula:
                    pType = NebulaTypeToStr(path->type);
                    break;
                case NSR_Derelict:
                    pType = DerelictTypeToStr(path->type);
                    break;
                case NSR_Effect:
#ifdef _WIN32
                    for (pType = ((etgeffectstatic *)path->type)->name; strchr(pType, '\\'); pType = strchr(pType, '\\'))
#else
                    for (pType = ((etgeffectstatic *)path->type)->name; strpbrk(pType, "\\/"); pType = strpbrk(pType, "\\/"))
#endif
                    {                                       //find just the name
                        ;
                    }
                    strcpy(effectName, pType);              //copy over just the name
                    pType = strchr(effectName, '.');
                    if (pType != NULL)                      //remove the extension
                    {
                        *pType = 0;
                    }
                    pType = effectName;
                    break;
                case NSR_Generic:
                    pType = header->genericObject[path->type]->name;
//                    pType = (char *)path->type;             //!!!
                    break;
                case NSR_Missile:
                    pType = ShipRaceToStr(path->type);      //get a race string
                    break;
                default:;
#if NIS_ERROR_CHECKING
                    dbgFatalf(DBG_Loc, "Invalid object race: %d", path->race);
#endif
            }
            if (!strcasecmp(pType, pSlash))
            {                                               //if this is the proper object type
                return(index);
            }
        }
    }
#if NIS_ERROR_CHECKING
    dbgFatalf(DBG_Loc, "nisFindObjectByName: object '%s' not found.", name);
#endif
    return(-1);
}

/*-----------------------------------------------------------------------------
    Name        : nisObjectDied
    Description : Ship death function called from univupdate when objects die.
    Inputs      : NIS - what NIS we're removing references from
                  obj - object that is expiring
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisRemoveObjectReference(nisplaying *NIS, SpaceObj *obj)
{
    sdword index;

    dbgAssertOrIgnore(NIS != NULL);
    for (index = 0; index < NIS->nObjects; index++)
    {
        if (NIS->objectsInMotion[index].spaceobj == (SpaceObjRotImp *)obj)
        {                                                   //if this is the object that is dying
            if (!(NIS->objectsInMotion[index].flags & OMF_CurvesDeleted))
            {                                               //and it's still not totally dead
                if (NIS->header->objectPath[index].nSamples != NIS_OneKeyframe)
                {                                           //and there is an actual motion curve
                    nisDeleteTheseCurves(&NIS->objectsInMotion[index]);//stop animating it
                }
                else
                {
                    bitSet(NIS->objectsInMotion[index].flags, OMF_CurvesDeleted);
                }
            }
            NIS->objectsInMotion[index].spaceobj = NULL;
            break;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : nisObjectDied
    Description : Remove references from a particular object from any playing NIS's
    Inputs      : obj - object that is expiring
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisObjectDied(SpaceObj *obj)
{
    if (testPlaying != NULL)
    {
        nisRemoveObjectReference(testPlaying, obj);
    }
    if (thisNisPlaying != NULL)
    {
        nisRemoveObjectReference(thisNisPlaying, obj);
    }

}

/*-----------------------------------------------------------------------------
    Name        : nisRemoveMissileReferenceFromPlaying
    Description : Removed missile referenced from a particular playing NIS
    Inputs      : NIS - nis to search through
                  missile - missile to remove references of
    Outputs     :
    Return      : voiv
----------------------------------------------------------------------------*/
void nisRemoveMissileReferenceFromPlaying(nisplaying *NIS, Missile *missile)
{
    sdword index;

    for (index = 0; index < NIS->nObjects; index++)
    {                                                       //search all objects
        if ((Missile *)NIS->objectsInMotion[index].spaceobj == missile)
        {                                                   //if this is the one to delete
            dbgAssertOrIgnore(NIS->header->objectPath[index].race == NSR_Missile);//make sure it's a missile
            if (!bitTest(NIS->objectsInMotion[index].flags, OMF_ObjectDied))
            {                                               //if it hasn't died yet
                nisDeleteTheseCurves(&NIS->objectsInMotion[index]);
                bitSet(NIS->objectsInMotion[index].flags, OMF_ObjectDied);//make it die
                NIS->objectsInMotion[index].spaceobj = NULL;//kill the pointer
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : nisRemoveMissileReference
    Description : Removes referenced to deleted missile, as the NIS may be
                    animating it.
    Inputs      : missile - missile that just died
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void nisRemoveMissileReference(Missile *missile)
{
    if (testPlaying != NULL)
    {
        nisRemoveMissileReferenceFromPlaying(testPlaying, missile);
    }
    if (thisNisPlaying != NULL)
    {
        nisRemoveMissileReferenceFromPlaying(thisNisPlaying, missile);
    }
}

/*-----------------------------------------------------------------------------
    Name        : nisDeatAnything
    Description : Event execution callback
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisDeathAnything(nisplaying *NIS, nisevent *event)
{
    DeleteShip *deleteShip;
    Ship *deadShip = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;

    if (nisSeeking)
    {
        return;
    }
    if (bitTest(NIS->objectsInMotion[event->shipID].flags, OMF_ObjectDied))
    {                                                       //if ship already dead
        return;
    }

    if (deadShip->header.objtype == OBJ_ShipType)
    {
        deleteShip = memAlloc(sizeof(DeleteShip),"DeleteShip",Pyrophoric);
        deleteShip->ship = deadShip;
        deleteShip->deathBy = event->param[0];
        listAddNode(&universe.DeleteShipList,&(deleteShip->objlink),deleteShip);
    }
    else
    {
        dbgAssertOrIgnore(deadShip->header.objtype == OBJ_DerelictType);
        AddDerelictToDeleteDerelictList((Derelict *)deadShip, event->param[0]);
    }
    bitSet(NIS->objectsInMotion[event->shipID].flags, OMF_ObjectDied);
}
void nisAttack(nisplaying *NIS, nisevent *event)
{
    Ship *goodShip, *badShip;
    SelectCommand selectone;
    AttackCommand attack;

    goodShip = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;
    badShip = (Ship *)NIS->objectsInMotion[event->param[0]].spaceobj;
    selectone.numShips = 1;
    selectone.ShipPtr[0] = goodShip;
    attack.numTargets = 1;
    attack.TargetPtr[0] = (SpaceObjRotImpTarg *)badShip;
    if (goodShip->shiptype == bMothership)
    {                                                       // CAT: the Beast mothership "attacks" with its special
        clSpecial(&universe.mainCommandLayer,&selectone,(SpecialCommand *)&attack);   // (nisAttack 0x004cf2c0)
        return;
    }
    if (goodShip->playerowner == badShip->playerowner)
    {
        clAttack(&universe.mainCommandLayer,&selectone,&attack);
    }
    else
    {
        clPassiveAttack(&universe.mainCommandLayer,&selectone,&attack);
    }
}
/*-----------------------------------------------------------------------------
    Name        : cat_nisRepair
    Description : CAT (M2-R6 nis-fow-camera): NIS event: the current ship repairs the object of param[0]
                  (cat_nisRepair 0x004cf370): a live Worker through cat_clSpecialTarget, any other ship through
                  clSpecial
----------------------------------------------------------------------------*/
void cat_nisRepair(nisplaying *NIS, nisevent *event)
{
    SelectCommand selectone;
    SpecialCommand target;

    selectone.ShipPtr[0] = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;
    target.TargetPtr[0] = (SpaceObjRotImpTarg *)NIS->objectsInMotion[event->param[0]].spaceobj;
    selectone.numShips = 1;
    selectone.timeLastStatus = 0.0f;
    target.numTargets = 1;
    target.timeLastStatus = 0.0f;
    if (cat_shipIsLiveWorker(selectone.ShipPtr[0]))
    {
        cat_clSpecialTarget(&universe.mainCommandLayer, &selectone, &target, FALSE);
        return;
    }
    clSpecial(&universe.mainCommandLayer, &selectone, &target);
}
void nisHaltAttack(nisplaying *NIS, nisevent *event)
{
    Ship *goodShip;
    SelectCommand selectone;

    goodShip = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;
    selectone.numShips = 1;
    selectone.ShipPtr[0] = goodShip;
    RemoveShipsFromDoingStuff(&universe.mainCommandLayer,&selectone,0x21);   // CAT: reason 0x21 as the exe (nisHaltAttack 0x004cf417) [audit: M2-R5 command-tactics]
}
void nisFire(nisplaying *NIS, nisevent *event)
{
    Ship *goodShip;
    ShipStaticInfo *shipstatic;

    goodShip = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;
    shipstatic = (ShipStaticInfo *)goodShip->staticinfo;
    if (shipstatic->custshipheader.CustShipFire != NULL)
    {
        shipstatic->custshipheader.CustShipFire(goodShip,NULL);
    }
}
void nisObjectShow(nisplaying *NIS, nisevent *event)
{
    SpaceObj *obj = (SpaceObj *)NIS->objectsInMotion[event->shipID].spaceobj;
    ShipRace race = NIS->header->objectPath[event->shipID].race;
    etgeffectstatic *stat;
    Effect *newEffect;

    if (race == NSR_Effect)
    {                                                       //if we are showing an effect, count it as starting up the effect
        //start the effect
        if (!nisSeeking)
        {
            stat = (etgeffectstatic *)NIS->header->objectPath[event->shipID].type;
            univAddObjToRenderList(obj);                    //temp. add this object to render list
            dbgAssertOrIgnore(obj->header.objtype != OBJ_BulletType);
            newEffect = etgEffectCreate(stat, (Ship *)obj, NULL, NULL, NULL, 1.0f, EAF_AllButNLips, 0);
            univRemoveObjFromRenderList(obj);               //don't leave this dummy object in the render list
            obj->staticinfo = (StaticInfo *)newEffect;      //store reference to effect in unused member of the spaceobj structure
            bitSet(newEffect->header.flags, SOF_ForceVisible);     //NIS effects should always be on-screen so force them to be visible
        }
    }
    else
    {                                                       //else any other type of object: add to render list
        if (!univSpaceObjInRenderList(obj))
        {                                                   //if object not in render list
            univAddObjToRenderList(obj);                    //add it to render list
            // CAT: a ship's clamped parts join the render list with it (nisObjectShow 0x004cf470)
            if ((obj->header.objtype == OBJ_ShipType) && (((Ship *)obj)->guidance.clampInfo != NULL) &&
                (((Ship *)obj)->guidance.clampInfo->clampedObjs != NULL))
            {
                CatClampedList *parts = ((Ship *)obj)->guidance.clampInfo->clampedObjs;
                sdword i;

                for (i = 0; i < parts->numShips; i++)
                {
                    if (!univSpaceObjInRenderList((SpaceObj *)parts->ShipPtr[i]))
                    {
                        univAddObjToRenderList((SpaceObj *)parts->ShipPtr[i]);
                    }
                }
            }
        }
#if NIS_VERBOSE_LEVEL >= 1
        else
        {
            dbgMessagef("nisObjectShow: at time %.2f, object#%d is already visible.", event->time, event->shipID);
        }
#endif
        bitClear(obj->header.flags, SOF_Hide);                     //make sure it stays in render list
        //bitSet(obj->header.flags, SOF_ForceVisible);
        if ((obj->header.objtype == OBJ_ShipType) && (((Ship *)obj)->guidance.clampInfo != NULL) &&
            (((Ship *)obj)->guidance.clampInfo->clampedObjs != NULL))
        {                                                   // CAT: and are unhidden with it
            CatClampedList *parts = ((Ship *)obj)->guidance.clampInfo->clampedObjs;
            sdword i;

            for (i = 0; i < parts->numShips; i++)
            {
                bitClear(parts->ShipPtr[i]->header.flags, SOF_Hide);
            }
        }
        if ((obj->header.objtype == OBJ_ShipType) && (((Ship *)obj)->shiptype == nisLensflareSource1))
        {                                                   // CAT: the NIS lens flare source shows its flare
            cat_rndLensFlareNISSourceEnable(0, TRUE);
        }
    }
}
void nisObjectHide(nisplaying *NIS, nisevent *event)
{
    SpaceObj *obj = (SpaceObj *)NIS->objectsInMotion[event->shipID].spaceobj;
    ShipRace race = NIS->header->objectPath[event->shipID].race;

    if (race == NSR_Effect)
    {                                                       //if we are hiding an effect, count it as stopping the effect
        if ((SpaceObj *)obj->staticinfo != NULL)
        {
            //end the effect
            obj = (SpaceObj *)obj->staticinfo;              //this is where the effect pointer is stored.  It better not have deleted itself.
            etgEffectDelete((Effect *)obj);                 //delete the effect
            univRemoveObjFromRenderList(obj);               //remove from render list
            listDeleteNode(&obj->header.objlink);                  //remove from object list
        }
    }
    else
    {                                                       //else any other type of object: remove from render list
        if (univSpaceObjInRenderList(obj))
        {
            univRemoveObjFromRenderList(obj);
        }
#if NIS_VERBOSE_LEVEL >= 1
        else
        {
            dbgMessagef("nisObjectHide: at time %.2f, object#%d is already hidden.", event->time, event->shipID);
        }
#endif
        bitSet(obj->header.flags, SOF_Hide);                       //make sure it stays out of render list
        //bitClear(obj->header.flags, SOF_ForceVisible);
        if (obj->header.objtype == OBJ_ShipType)
        {
            // CAT: a ship's clamped parts are hidden with it (they stay in the render list), the NIS lens flare
            //      source hides its flare and the ship's sounds stop (nisObjectHide 0x004cf5d0)
            if ((((Ship *)obj)->guidance.clampInfo != NULL) && (((Ship *)obj)->guidance.clampInfo->clampedObjs != NULL))
            {
                CatClampedList *parts = ((Ship *)obj)->guidance.clampInfo->clampedObjs;
                sdword i;

                for (i = 0; i < parts->numShips; i++)
                {
                    bitSet(parts->ShipPtr[i]->header.flags, SOF_Hide);
                }
            }
            if (((Ship *)obj)->shiptype == nisLensflareSource1)
            {
                cat_rndLensFlareNISSourceEnable(0, FALSE);
            }
            soundEventShipRemove((Ship *)obj);
        }
    }
}
void nisInvincible(nisplaying *NIS, nisevent *event)
{
    ((Ship *)NIS->objectsInMotion[event->shipID].spaceobj)->targetable.health = REALlyBig;
}
void nisVincible(nisplaying *NIS, nisevent *event)
{
    Ship *ship = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;
    ship->targetable.health = ship->staticinfo->health.maxhealth;
}
void nisSoundEvent(nisplaying *NIS, nisevent *event)
{
    if (nisSeeking)
    {
        return;
    }
    soundEvent(NIS->objectsInMotion[event->shipID].spaceobj, event->param[0]);
}
/*-----------------------------------------------------------------------------
    Name        : cat_nisSubtitle
    Description : CAT (M2-R6 nis-fow-camera): NIS event: show one of the NIS subtitle strings for param[1]
                  milliseconds (cat_nisSubtitle 0x004cf740: index 0 strNIS01, 2 strNISLET1, 3..11 strNIS02,
                  strNIS04..strNIS11; index 1 (strNIS01a) and anything above 11 show nothing; the text goes to
                  subtitle region 0x70 prefixed with the "#r3#tf" colour codes)
----------------------------------------------------------------------------*/
void cat_nisSubtitle(nisplaying *NIS, nisevent *event)
{
    static const sdword subtitleString[12] = { strNIS01, -1, strNISLET1, strNIS02, strNIS04, strNIS05, strNIS06,
                                               strNIS07, strNIS08, strNIS09, strNIS10, strNIS11 };
    char text[0x7f8];
    char *string;

    // CAT-FIX: an index above 11 reads an undefined local in the exe; here it shows nothing
    if ((sdword)event->param[0] < 0 || (sdword)event->param[0] > 11 || subtitleString[event->param[0]] < 0)
    {
        return;
    }
    string = strGetString(subtitleString[event->param[0]]);
    if (string == NULL)
    {
        return;
    }
    snprintf(text, 0x7f7, "#r%x#t%x%s", 3, 0xf, string);
    subTitleAdd(0x70, 0, text, strlen(text), (real32)(sdword)event->param[1] * 0.001f);
}
void nisSpeechEvent(nisplaying *NIS, nisevent *event)
{
    sdword actor;

    if ((event->param[1] & NIS_ActorMask) == NIS_ActorMask)
    {
        actor = SOUND_EVENT_DEFAULT;
    }
    else
    {
        actor = (event->param[1] & NIS_ActorMask) >> NIS_ActorShiftBits;
    }
    if (nisSeeking)
    {
        return;
    }
    //speechEvent(NIS->objectsInMotion[event->shipID].spaceobj, event->param[0], event->param[1]);
    // CAT: the variation word is the ship's type (nisSpeechEvent 0x004cf840 passes obj+0x2B0), not the event's
    speechEventQueue(NIS->objectsInMotion[event->shipID].spaceobj,
                     event->param[0], ((Ship *)NIS->objectsInMotion[event->shipID].spaceobj)->shiptype,
                     SOUND_EVENT_DEFAULT, actor,
                     universe.curPlayerIndex, SOUND_EVENT_DEFAULT,
                     (real32)SOUND_EVENT_DEFAULT, SOUND_EVENT_DEFAULT);
}
void nisFleetSpeechEvent(nisplaying *NIS, nisevent *event)
{
    if (nisSeeking)
    {
        return;
    }
    speechEventFleet(event->param[0], event->param[1], universe.curPlayerIndex);
}

void nisAnimaticSpeechEvent(nisplaying *NIS, nisevent *event)
{
    // CAT: an animatic line has no timeout (nisAnimaticSpeechEvent 0x004cf8f0 passes 0.0, the fleet macro 5.0)
    speechEventQueue(NULL, event->param[0], event->param[1], -1, 0, universe.curPlayerIndex, -1, 0.0f, -1);
}
/*-----------------------------------------------------------------------------
    Name        : cat_nisLightAmbient / cat_nisLightDiffuse / cat_nisGlobalAmbient / cat_nisNumberLights
    Description : CAT (M2-R6 nis-fow-camera): NIS light events (0x004cf920 / 0x004cf970: light param[0]'s ambient or
                  diffuse colour from the 0..255 components param[1..3]; 0x004cf9c0: the global ambient from
                  param[0..2]; 0x004cfa10: the number of lights)
----------------------------------------------------------------------------*/
void cat_nisLightAmbient(nisplaying *NIS, nisevent *event)
{
    real32 rgb[4];

    rgb[0] = (real32)(sdword)event->param[1] * (1.0f / 255.0f);
    rgb[3] = 1.0f;
    rgb[1] = (real32)(sdword)event->param[2] * (1.0f / 255.0f);
    rgb[2] = (real32)(sdword)event->param[3] * (1.0f / 255.0f);
    cat_lightSetAmbient((sdword)event->param[0], rgb);
}
void cat_nisLightDiffuse(nisplaying *NIS, nisevent *event)
{
    real32 rgb[4];

    rgb[0] = (real32)(sdword)event->param[1] * (1.0f / 255.0f);
    rgb[3] = 1.0f;
    rgb[1] = (real32)(sdword)event->param[2] * (1.0f / 255.0f);
    rgb[2] = (real32)(sdword)event->param[3] * (1.0f / 255.0f);
    cat_lightSetDiffuse((sdword)event->param[0], rgb);
}
void cat_nisGlobalAmbient(nisplaying *NIS, nisevent *event)
{
    real32 rgb[4];

    rgb[3] = 1.0f;
    rgb[0] = (real32)(sdword)event->param[0] * (1.0f / 255.0f);
    rgb[1] = (real32)(sdword)event->param[1] * (1.0f / 255.0f);
    rgb[2] = (real32)(sdword)event->param[2] * (1.0f / 255.0f);
    cat_lightSetGlobalAmbient(rgb);
}
void cat_nisNumberLights(nisplaying *NIS, nisevent *event)
{
    lightSetNumLights((udword)event->param[0]);
}
void nisSMPTEOn(nisplaying *NIS, nisevent *event)
{
    nisSMPTECounter = (nisSMPTE *)event->param[0];
}
void nisSMPTEOff(nisplaying *NIS, nisevent *event)
{
    nisSMPTECounter = NULL;
}
/*-----------------------------------------------------------------------------
    Name        : nisStaticOnExp
    Description : Exported function to turn static on.  Can be used by other modules.
    Inputs      : newStatic - parameters for static.
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisStaticOnExp(nisstatic *newStatic)
{
    sdword count;
    real32 minHue, maxHue, minLum, maxLum, minSat, maxSat;
    real32 red, green, blue;
    real32 hue, lum;
    real32 sat;
    udword minAlpha, maxAlpha, alphaUbyte;
    color textureData[NIS_NoiseTextureWidth * NIS_NoiseTextureHeight * sizeof(color)];
    color *texturePointer = textureData;

    //create the static texture if needed
    if (newStatic->bTextured)
    {                                                       //if indeed it is textured
        minHue = max(newStatic->hue - newStatic->hueVariation, 0.0f);
        maxHue = min(newStatic->hue + newStatic->hueVariation, 1.0f);
        minSat = max(newStatic->sat - newStatic->satVariation, 0.0f);
        maxSat = min(newStatic->sat + newStatic->satVariation, 1.0f);
        minLum = max(newStatic->lum - newStatic->lumVariation, 0.0f);
        maxLum = min(newStatic->lum + newStatic->lumVariation, 1.0f);
        minAlpha = colRealToUbyte(max(newStatic->alpha - newStatic->alphaVariation, 0.0f));
        maxAlpha = colRealToUbyte(min(newStatic->alpha + newStatic->alphaVariation, 1.0f));
        for (count = NIS_NoiseTextureWidth * NIS_NoiseTextureHeight; count > 0; count--, texturePointer++)
        {                                                   //fill in the texture bitmap
            hue = frandyrandombetween(RANDOM_STATIC, minHue, maxHue);
            sat = frandyrandombetween(RANDOM_STATIC, minSat, maxSat);
            lum = frandyrandombetween(RANDOM_STATIC, minLum, maxLum);
            alphaUbyte = randyrandombetween(RANDOM_STATIC, minAlpha, maxAlpha);
            colHLSToRGB(&red, &green, &blue, hue, lum, sat);
            dbgAssertOrIgnore(red >= 0.0f && red <= 1.0f);
            dbgAssertOrIgnore(green >= 0.0f && green <= 1.0f);
            dbgAssertOrIgnore(blue >= 0.0f && blue <= 1.0f);
            *texturePointer = colRGBA(colRealToUbyte(red), colRealToUbyte(green), colRealToUbyte(blue), alphaUbyte);
        }
        newStatic->texture = trRGBTextureCreate(textureData, NIS_NoiseTextureWidth, NIS_NoiseTextureHeight, newStatic->bAlpha);
    }
    else
    {                                                       //else no texture
        newStatic->texture = TR_InvalidInternalHandle;
    }

    nisStatic[newStatic->index] = newStatic;
}

/*-----------------------------------------------------------------------------
    Name        : nisStaticOffExp
    Description : Exported function for turning off a particular static channel.
    Inputs      : index - what channel to disable
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisStaticOffExp(sdword index)
{
    dbgAssertOrIgnore(index >= 0 && index < NIS_NumberStatics + NIS_NumberExtraStatics);

    if (nisStatic[index] != NULL)
    {                                                       //if this static is on
        if (nisStatic[index]->texture != TR_InvalidInternalHandle)
        {                                                   //and there even is some static here
            trRGBTextureDelete(nisStatic[index]->texture);
            nisStatic[index]->texture = TR_InvalidInternalHandle;
        }
        nisStatic[index] = NULL;
    }
}
void nisStaticOn(nisplaying *NIS, nisevent *event)
{
    nisstatic *newStatic = (nisstatic *)event->param[0];

    nisStaticOnExp(newStatic);
}
void nisStaticOff(nisplaying *NIS, nisevent *event)
{
    nisStaticOffExp(event->param[0]);
}

void nisHideDerelict(nisplaying *NIS, nisevent *event)
{
    Node *node;
    Derelict *derelict;
    //find a derelict of the specified type
    for (node = universe.DerelictList.head; node != NULL; node = node->next)
    {
        derelict = (Derelict *)listGetStructOfNode(node);
        if (derelict->derelicttype == event->param[0])
        {                                                   //if this is the derelict we're looking for
            if (event->code == NEO_HideDerelict)
            {
                if (derelict->staticinfo->worldRender)
                {                                           //if it's a world-render type derelict
                    univRemoveFromWorldList(derelict);
                }
                else
                {
                    bitSet(derelict->header.flags, SOF_Hide);
                    if (univSpaceObjInRenderList((SpaceObj *)derelict))
                    {
                        univRemoveObjFromRenderList((SpaceObj *)derelict);
                    }
                }
            }
            else
            {                                               //else we're mot hiding it; we're showing it
                if (derelict->staticinfo->worldRender)
                {                                           //if it's a world-render type of derelict
                    univAddToWorldList(derelict);
                }
                else
                {
                    bitClear(derelict->header.flags, SOF_Hide);
                }
            }
            break;                                          //only process 1 derelict
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : nisNewNIS
    Description : Starts a new NIS
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisNewNIS(nisplaying *NIS, nisevent *event)
{
    vector tempPosition;
    matrix tempCoordSys;

    if (nisSeeking)
    {                                                       //if seeking or very close to the end of NIS
        return;
    }
    tempPosition = thisNisPlaying->nisPosition;
    tempCoordSys = thisNisPlaying->nisMatrix;
    nisNewNISStarted = TRUE;
    nisStop(NIS);
    thisNisHeader = nisLoad((char *)event->param[0], (char *)event->param[1]);
    thisNisPlaying = nisStart(thisNisHeader, &tempPosition, &tempCoordSys);
}

void nisAttributes(nisplaying *NIS, nisevent *event)
{
    SpaceObj *obj = (SpaceObj *)NIS->objectsInMotion[event->shipID].spaceobj;
    obj->attributes = (uword)event->param[0];
}

void nisAttackKasSelection(nisplaying *NIS, nisevent *event)
{
    SelectCommand thisShip;                                 //selection of length 1

    thisShip.numShips = 1;                                 //make a temp selection of just this ship
    thisShip.ShipPtr[0] = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;
    clWrapAttack(&universe.mainCommandLayer,&thisShip,(AttackCommand *)event->param[0]);
}

void nisKeepMovingAtEnd(nisplaying *NIS, nisevent *event)
{
    bitSet(NIS->objectsInMotion[event->shipID].flags, OMF_KeepOnMoving);
}

void nisMinimumLOD(nisplaying *NIS, nisevent *event)
{
    Ship *goodShip;

    goodShip = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;
    goodShip->staticinfo->common.LOD->nLevels = event->param[0];
}

void nisDamageLevel(nisplaying *NIS, nisevent *event)
{
    Ship *goodShip;
    real32 health = SdwordToReal32(event->param[0]);

    goodShip = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;
    goodShip->targetable.health = health * goodShip->staticinfo->health.maxhealth;
}

void nisRemainAtEnd(nisplaying *NIS, nisevent *event)
{
    bitSet(NIS->objectsInMotion[event->shipID].flags, OMF_RemainAtEnd);
    if (event->param[0])
    {
        char *str = (char *)event->param[0];
        Ship *ship = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;
        dbgAssertOrIgnore(strlen(str) > 0);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (singlePlayerGame)
        {
            // CAT: the exe passes the label's hash (cat_kasHashString; nisRemainAtEnd 0x004d0050)
            kasAddShipToTeam(ship,cat_kasHashString(str));
        }

        memFree((void *)event->param[0]);
        event->param[0] = 0;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_nisOpenSlipgate / cat_nisCloseSlipgate
    Description : CAT (M2-R6 nis-fow-camera): NIS events: open or close the current object, a slipgate
                  (0x004d00b0 / 0x004d00d0)
----------------------------------------------------------------------------*/
void cat_nisOpenSlipgate(nisplaying *NIS, nisevent *event)
{
    cat_slipgateOpen((Ship *)NIS->objectsInMotion[event->shipID].spaceobj);
}
void cat_nisCloseSlipgate(nisplaying *NIS, nisevent *event)
{
    cat_slipgateClose((Ship *)NIS->objectsInMotion[event->shipID].spaceobj);
}
void nisCameraFOV(nisplaying *NIS, nisevent *event)
{
    real32 FOV = SdwordToReal32(event->param[0]);

    nisCamera->fieldofview = FOV;
}
/*-----------------------------------------------------------------------------
    Name        : cat_nisCameraClipNear / cat_nisCameraClipFar
    Description : CAT (M2-R6 nis-fow-camera): NIS events: the NIS camera's near / far clip plane, 0 = the default
                  CAMERA_CLIP_NEAR / CAMERA_CLIP_FAR (0x004d0110 / 0x004d0140)
----------------------------------------------------------------------------*/
void cat_nisCameraClipNear(nisplaying *NIS, nisevent *event)
{
    real32 clip = SdwordToReal32(event->param[0]);

    nisCamera->clipPlaneNear = (clip != 0.0f) ? clip : CAMERA_CLIP_NEAR;
}
void cat_nisCameraClipFar(nisplaying *NIS, nisevent *event)
{
    real32 clip = SdwordToReal32(event->param[0]);

    nisCamera->clipPlaneFar = (clip != 0.0f) ? clip : CAMERA_CLIP_FAR;
}
void nisCameraCut(nisplaying *NIS, nisevent *event)
{
    sdword index, j;
    objectmotion *path;
    cameramotion *camPath;
    vector position;
    real32 timeElapsed = SdwordToReal32(event->param[0]);

    dbgAssertOrIgnore(timeElapsed >= 0);
    dbgAssertOrIgnore(timeElapsed < NIS->header->length);

    if (nisSeeking)
    {
        return;
    }
    position = NIS->nisPosition;                            //get position from NIS matrix
    //advance all the object motion paths
    for (index = 0; index < NIS->header->nObjectPaths; index++)
    {
        path = &NIS->objectsInMotion[index];                //get pointer to object motion path
        if (path->curve[0] != NULL)
        {                                                   //if NULL motion path or NULL object
            for (j = 0; j < 6; j++)
            {                                               //for each motion channel
                bsCurveUpdate(path->curve[j], timeElapsed);    //update to a good point
            }
        }
    }
    //advance all the camera motion paths
    for (index = 0; index < NIS->header->nCameraPaths; index++)
    {
        camPath = &NIS->camerasInMotion[index];     //get pointer to object motion path
        if (camPath->curve[0] == NULL)
        {                                           //if NULL camera motion path
            continue;                               //leave this object where it was
        }
        for (j = 0; j < 6; j++)
        {                                           //for each motion channel
            bsCurveUpdate(camPath->curve[j], timeElapsed);//update to a good point
        }
    }
    //advance the official time of the event
    NIS->timeElapsed += timeElapsed;
}
void nisTextCard(nisplaying *NIS, nisevent *event)
{
    if (nisSeeking)
    {
        return;
    }
    dbgAssertOrIgnore(nisTextCardIndex < NIS_NumberTextCards);

    nisTextCardList[nisTextCardIndex] = *((nistextcard *)event->param[0]);
    nisTextCardList[nisTextCardIndex].creationTime = NIS->timeElapsed;
    nisTextCardList[nisTextCardIndex].NIS = NIS;
    nisTextCardIndex++;
}
void nisTextScroll(nisplaying *NIS, nisevent *event)
{
    if (nisSeeking)
    {
        return;
    }
    nisTextScrollDistance = SdwordToReal32(event->param[0]);
    nisTextScrollStart = NIS->timeElapsed;
    nisTextScrollEnd = nisTextScrollStart + SdwordToReal32(event->param[1]);
}
void nisScissorOut(nisplaying *NIS, nisevent *event)
{
    soundEvent(NULL, CAT_UI_Unletterbox);           // CAT: Cataclysm's id 0x124 (nisScissorOut 0x004d02d9)
    nisScissorFadeOut = SdwordToReal32(event->param[0]);
    nisScissorFadeTime = 0.0f;
}
void nisFocusAtEnd(nisplaying *NIS, nisevent *event)
{
    bitSet(NIS->objectsInMotion[event->shipID].flags, OMF_FocusAtEnd);
}

/*-----------------------------------------------------------------------------
    Name        : nisFocusMaxRadius
    Description : Find the radius of a sphere that would completely encompass
                    the specified selection if centred about centre.
    Inputs      : centre - centre of sphere to compute radius of
                  selection - selection of ships to fit in sphere
    Outputs     :
    Return      : radius of sphere
----------------------------------------------------------------------------*/
real32 nisFocusMaxRadius(vector *centre, MaxSelection *selection)
{
    sdword index;
    vector shipCentre, distance;
    Ship *ship;
    real32 radius, maxRadius = 0.0f;

    for (index = 0; index < selection->numShips; index++)
    {
        ship = selection->ShipPtr[index];
        ccGetShipCollCenter(ship, &shipCentre, &radius);    //get location of ship
        vecSub(distance, *centre, shipCentre);              //get distance from centre
        radius = vecMagnitudeSquared(distance) + radius;    //get d^2
        if (radius > maxRadius)
        {                                                   //if new record
            maxRadius = radius;
        }
    }
    return(fsqrt(maxRadius));                               //return max distance
}

/*-----------------------------------------------------------------------------
    Name        : nisFocusPointCompute
    Description : Compute the focal point of a list of ships
    Inputs      : selection - selection to scan
    Outputs     : centre - computed centre point
    Return      :
----------------------------------------------------------------------------*/
void nisFocusPointCompute(vector *centre, MaxSelection *selection)
{
    sdword index;
    Ship *ship;
    real32 minx,maxx,miny,maxy,minz,maxz, radius;

    dbgAssertOrIgnore(selection->numShips > 0);

    ship = selection->ShipPtr[0];

    ccGetShipCollCenter(ship, centre, &radius);

    minx = maxx = centre->x;
    miny = maxy = centre->y;
    minz = maxz = centre->z;

    for (index = 1; index < selection->numShips; index++)
    {
        ship = selection->ShipPtr[index];
        ccGetShipCollCenter(ship, centre, &radius);

        if (centre->x < minx) minx = centre->x;
        else if (centre->x > maxx) maxx = centre->x;

        if (centre->y < miny) miny = centre->y;
        else if (centre->y > maxy) maxy = centre->y;

        if (centre->z < minz) minz = centre->z;
        else if (centre->z > maxz) maxz = centre->z;
    }

    centre->x = (minx + maxx) * 0.5f;
    centre->y = (miny + maxy) * 0.5f;
    centre->z = (minz + maxz) * 0.5f;
}

/*-----------------------------------------------------------------------------
    Name        : nisFocus
    Description : NIS-event callback to focus the camera on the specified ships
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisFocus(nisplaying *NIS, nisevent *event)
{
    sdword index;
    MaxSelection tempselected;
    vector focusPoint, distance, lookDirection;

    tempselected.numShips = 0;
    for (index = 0; index < NIS->nObjects; index++)
    {                                                       //find ships to focus on
        if (bitTest(NIS->objectsInMotion[index].flags, OMF_FocusAtEnd))
        {
            tempselected.ShipPtr[tempselected.numShips] = (ShipPtr)NIS->objectsInMotion[index].spaceobj;
            tempselected.numShips++;
        }
    }

    if (tempselected.numShips > 0)
    {                                                       //if there were ships to focus on
        //find the distance to the focus point.
        nisCameraPrepareForCopy(nisCamera);
        nisFocusPointCompute(&focusPoint, &tempselected);
        if (bPerfectFocusComputed)
        {
            nisCamera->lookatpoint = focusPoint;
        }
        else
        {
            vecSub(distance, focusPoint, nisCamera->eyeposition);
            vecSub(lookDirection, nisCamera->lookatpoint, nisCamera->eyeposition);
            nisCamera->distance = fsqrt(vecMagnitudeSquared(distance));
            vecMultiplyByScalar(lookDirection, nisCamera->distance);
            vecAdd(nisCamera->lookatpoint, nisCamera->eyeposition, lookDirection);
        }
        //set the azimuth and declination properly
        nisCamera->oldlookatpoint = nisCamera->lookatpoint;

        //now it's safe to copy the camera
        ccCopyCamera(&universe.mainCameraCommand,nisCamera);    //focus the camera to wherever it is
        ccFocus(&universe.mainCameraCommand,(FocusCommand *)&tempselected);
    }
    else
    {                                                       //no ships to focus on, focus on the previous scene
        nisCameraPrepareForCopy(nisCamera);
        nisCamera->oldlookatpoint = nisCamera->lookatpoint;
        ccCopyCamera(&universe.mainCameraCommand,nisCamera);    //focus the camera from wherever it is
//        ccCancelFocus(&(universe.mainCameraCommand));
        ccFocusGeneral(&universe.mainCameraCommand, &universe.mainCameraCommand.currentCameraStack->focus, FALSE);
    }
    bCameraFocussed = TRUE;
    nisCaptureCamera = FALSE;
    nisCamera = NULL;
    nisCameraCutTime = 0.0f;                                //in case player hit (escape)
    nisCameraCutStage2 = -1.0f;
}
void nisMusicStart(nisplaying *NIS, nisevent *event)
{
    if (nisMusicPlaying)
    {                                                       //if any music is playing
        soundEventStopMusic(0.0f);                          //stop it
        nisMusicPlaying = FALSE;
    }
#if NIS_VERBOSE_LEVEL >= 1
    dbgMessagef("Playing music track %d.", event->param[0]);
#endif
    soundEventPlayMusic(event->param[0]);
    nisMusicPlaying = TRUE;
}
void nisMusicStop(nisplaying *NIS, nisevent *event)
{
#if NIS_VERBOSE_LEVEL >= 1
    dbgMessagef("Stopping music over the next %.2f seconds.", SdwordToReal32(event->param[0]));
#endif
    soundEventStopMusic(SdwordToReal32(event->param[0]));
    nisMusicPlaying = FALSE;
}
void nisBlackFadeSet(nisplaying *NIS, nisevent *event)
{
#if NIS_VERBOSE_LEVEL >= 1
    dbgMessagef("Setting fade level to %.2f", SdwordToReal32(event->param[0]));
#endif
    nisBlackFade = nisBlackFadeDest = SdwordToReal32(event->param[0]);
    catNisBlackFadeFlag = (ubyte)event->param[1];           // CAT: the second parameter (nisBlackFadeSet 0x004d0770)
}
void nisBlackFadeTo(nisplaying *NIS, nisevent *event)
{
#if NIS_VERBOSE_LEVEL >= 1
    dbgMessagef("Fading to level %.2f over %.2f seconds", SdwordToReal32(event->param[0]), SdwordToReal32(event->param[1]));
#endif
    nisBlackFadeDest = SdwordToReal32(event->param[0]);
    nisBlackFadeRate = (SdwordToReal32(event->param[0]) - nisBlackFade) / SdwordToReal32(event->param[1]);
}
void nisColorScheme(nisplaying *NIS, nisevent *event)
{
    SpaceObjRotImp *goodShip = NIS->objectsInMotion[event->shipID].spaceobj;

    switch (goodShip->header.objtype)
    {
        case OBJ_ShipType:
            ((Ship *)goodShip)->colorScheme = event->param[0];
            break;
        case OBJ_MissileType:
            ((Missile *)goodShip)->colorScheme = event->param[0];
            break;
        case OBJ_DerelictType:
            ((Derelict *)goodShip)->colorScheme = event->param[0];
            break;
        default:
            dbgAssertOrIgnore(FALSE);
    }
}
void nisMeshAnimationStart(nisplaying *NIS, nisevent *event)
{
    Ship *goodShip = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;
    sdword animIndex;
    madanim *anim;

    dbgAssertOrIgnore(goodShip->madBindings != NULL);
    anim = goodShip->madBindings;
    animIndex = madAnimIndexFindByName(anim->header, (char *)event->param[0]);
    if (anim->nCurrentAnim != -1)
    {                                                       //if there is an animation playing
        //!!! store current animation info somewhere and delete it when we're done
        madAnimationStop(goodShip);                         //stop any animations currently playing
    }
    madAnimationStart(goodShip, animIndex, MAD_ModePauseAtEnd);     //start the animation playing (CAT: the exe's third argument 0, nisMeshAnimationStart 0x004d083f)
}
/*-----------------------------------------------------------------------------
    Name        : cat_nisEbolts / cat_nisBeastEffect / cat_nisFog
    Description : CAT (M2-R6 nis-fow-camera): NIS events. Ebolts (0x004d0850): param[0] set starts the Beast-stun
                  electric bolts on the current ship for 2.5 s (the stun state 1, spin -1), clear stops them.
                  BeastEffect (0x004d08b0): param[0] set starts the infection, which converts the ship in 4 s, clear
                  stops it. Fog (0x004d08f0): the fog layer on or off (param[0]) and its fade to colours param[1] /
                  param[2] with time param[3], speed param[5] and density param[4]
----------------------------------------------------------------------------*/
void cat_nisEbolts(nisplaying *NIS, nisevent *event)
{
    Ship *ship = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;

    if (event->param[0] != 0)
    {
        ship->catBeastStunStart = universe.totaltimeelapsed;
        ship->catBeastSpin = -1.0f;
        ship->catBeastStunState = 1;
        ship->catBeastStunEnd = ship->catBeastStunStart + 2.5f;
        return;
    }
    ship->catBeastStunState = 0;
}
void cat_nisBeastEffect(nisplaying *NIS, nisevent *event)
{
    Ship *ship = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;

    if (event->param[0] != 0)
    {
        ship->catBeastConvertTime = universe.totaltimeelapsed + 4.0f;
        return;
    }
    ship->catBeastConvertTime = 0.0f;
}
void cat_nisFog(nisplaying *NIS, nisevent *event)
{
    catRndFogOn = (bool8)event->param[0];
    cat_rndFogLayerFadeTo(SdwordToReal32(event->param[3]), (color)event->param[1], (color)event->param[2],
                          SdwordToReal32(event->param[5]), SdwordToReal32(event->param[4]));
}
void nisMeshAnimationStop(nisplaying *NIS, nisevent *event)
{
    Ship *goodShip = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;

    dbgAssertOrIgnore(goodShip->madBindings != NULL);
    if (goodShip->madBindings->nCurrentAnim != -1)
    {                                                       //if there is an animation playing
        madAnimationStop(goodShip);
    }
}
void nisMeshAnimationPause(nisplaying *NIS, nisevent *event)
{
    Ship *goodShip = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;

    dbgAssertOrIgnore(goodShip->madBindings != NULL);
    if (goodShip->madBindings->nCurrentAnim != -1)
    {                                                       //if there is an animation playing
        madAnimationPause(goodShip, goodShip->madBindings->bPaused ^ TRUE);
    }
}
void nisMeshAnimationSeek(nisplaying *NIS, nisevent *event)
{
    Ship *goodShip = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;
    real32 seekTime;

    dbgAssertOrIgnore(goodShip->madBindings != NULL);
    if (goodShip->madBindings->nCurrentAnim != -1)
    {                                                       //if there is an animation playing
//        goodShip->madBindings->time = SdwordToReal32(event->param[0]);
        seekTime = SdwordToReal32(event->param[0]);
        if (seekTime > goodShip->madBindings->time)
        {
            madAnimationUpdate(goodShip, seekTime - goodShip->madBindings->time);
        }
    }
}
/*-----------------------------------------------------------------------------
    Name        : nisBlendCameraEndPoint
    Description : NIS-event function that computes the focus point of all this
                    ships that will be focussed on at the end of the NIS and
                    plugs that point into the last keyframe of the NIS camera's
                    motion path.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisBlendCameraEndPoint(nisplaying *NIS, nisevent *event)
{
    sdword index;
    MaxSelection tempselected;
    vector position, difference;
    vector presumedFinalPosition, presumedFinalRotation, presumedFinalLook;
    vector xyz, xyzTemp;
    vector Vfocus;
    matrix tempCoordsys, coordsys;
    real32 **path;
    real32 currentTimeElapsed;

    tempselected.numShips = 0;
    for (index = 0; index < NIS->nObjects; index++)
    {                                                       //find ships to focus on
        if (bitTest(NIS->objectsInMotion[index].flags, OMF_FocusAtEnd))
        {
            tempselected.ShipPtr[tempselected.numShips] = (ShipPtr)NIS->objectsInMotion[index].spaceobj;
            tempselected.numShips++;
        }
    }
    //compute the centre of that selection
    nisFocusPointCompute(&position, &tempselected);
    //now plug that postion into the last and second-from-last keyframes of this NIS camera's motion path
    index = NIS->header->cameraPath[0].nSamples;
    path = NIS->header->cameraPath[0].curve;
    //camera position: -Z, X, Y (UZI mode 33)
    xyzTemp.x = -path[2][index - 1];
    xyzTemp.y = path[0][index - 1];
    xyzTemp.z = path[1][index - 1];

    matMultiplyMatByVec(&xyz, &NIS->nisMatrix, &xyzTemp);   //multiply NIS world matrix

    presumedFinalPosition.x = NIS->nisPosition.x + xyz.x;
    presumedFinalPosition.y = NIS->nisPosition.y + xyz.y;
    presumedFinalPosition.z = NIS->nisPosition.z + xyz.z;

    //camera rotation: X, Y, Z (mode 0)
    presumedFinalRotation.x = path[3][index - 1];
    presumedFinalRotation.y = path[4][index - 1];
    presumedFinalRotation.z = path[5][index - 1];

    nisShipEulerToMatrix(&tempCoordsys, &presumedFinalRotation);
    matMultiplyMatByMat(&coordsys, &NIS->nisMatrix, &tempCoordsys);//multiply NIS world matrix
    matGetVectFromMatrixCol2(presumedFinalLook, coordsys);

    //focus point and camera.eye-focus point defines a plane
    vecSub(Vfocus, presumedFinalPosition, position);
    //camera.eye and camera.eye-camera.look defines a ray
    //compute the intersection of these.
    vecNormalize(&Vfocus);       //!!! needed?
    vecLineIntersectWithPlane(&xyz, &position, &Vfocus, &presumedFinalPosition, &presumedFinalLook);
    vecSub(difference, xyz, position);
    matCopyAndTranspose(&NIS->nisMatrix, &tempCoordsys);    //get the transpose of the NIS matrix
    matMultiplyMatByVec(&xyz, &NIS->nisMatrix, &difference);//get difference in 'NIS space'

    //game back to NIS: y, z, -x (inverse of UZI mode 33)
    path[0][index - 1] -= xyz.y;                            //adjust the last keyframe
    path[1][index - 1] -= xyz.z;
    path[2][index - 1] += xyz.x;
    path[0][index - 2] -= xyz.y;                            //adjust the second-to-last keyframe
    path[1][index - 2] -= xyz.z;
    path[2][index - 2] += xyz.x;
    path[0][index - 3] -= xyz.y;                            //adjust the third-to-last keyframe
    path[1][index - 3] -= xyz.z;
    path[2][index - 3] += xyz.x;

    bPerfectFocusComputed = TRUE;

    //if we are seeking, we should update the camera at this point because the keyframes have changed
    if (nisSeeking)
    {
        currentTimeElapsed = NIS->camerasInMotion[0].curve[0]->timeElapsed;
        for (index = 0; index < 6; index++)
        {                                               //for each motion channel
            bsCurveRestart(NIS->camerasInMotion[0].curve[index]);      //restart the curve
        }
        nisCamPathUpdate(NIS, &NIS->camerasInMotion[0], currentTimeElapsed);//advance it just a tiny little bit
    }
}

/*-----------------------------------------------------------------------------
    Name        : nisVolumeSet
    Description : NIS-event callback for setting the volume of either music, speech or SFX
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisVolumeSet(nisplaying *NIS, nisevent *event)
{
    switch (event->code)
    {
        case NEO_MusicVolume:
#if NIS_VERBOSE_LEVEL > 2
            dbgMessagef("Setting Music volume to %.2f", SdwordToReal32(event->param[0]));
#endif
            soundEventMusicVol(SdwordToReal32(event->param[0]));
            break;
        case NEO_SpeechVolume:
#if NIS_VERBOSE_LEVEL > 2
            dbgMessagef("Setting Speech volume to %.2f", SdwordToReal32(event->param[0]));
#endif
            soundEventSpeechVol(SdwordToReal32(event->param[0]));
            break;
        case NEO_SFXVolume:
#if NIS_VERBOSE_LEVEL > 2
            dbgMessagef("Setting SFX volume to %.2f", SdwordToReal32(event->param[0]));
#endif
            soundEventSFXVol(SdwordToReal32(event->param[0]));
            break;
        default:
            break;
    }
}

/*-----------------------------------------------------------------------------
    Name        : nisTrailMove
    Description : NIS-event callback for moving a ship's trail by the difference
                    of two upcoming adjacent keyframes
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
#define NIS_AdjacentFrames      (1.0 / 15.0)
#define NIS_NotAnyTimeSoon      1.5f
void nisTrailMove(nisplaying *NIS, nisevent *event)
{
    Ship *ship;
    sdword pointIndex, index;
    real32 difference;
    splinecurve **curve;
    vector xyzTemp, moveAmount;

/*
    if (nisSeeking)
    {
        return;
    }
*/
    ship = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;
    dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
    curve = NIS->objectsInMotion[event->shipID].curve;
    pointIndex = max(curve[0]->currentPoint + 1, 0);
    for (; pointIndex < curve[0]->nPoints - 1; pointIndex++)
    {                                                       //look for adjacent frames
        if (curve[0]->times[pointIndex] - NIS->timeElapsed >= NIS_NotAnyTimeSoon)
        {                                                   //if no adjacent frames coming up
            break;
        }
        difference = curve[0]->times[pointIndex] - curve[0]->times[pointIndex - 1];
        if (difference > 0.0f && difference < NIS_AdjacentFrames)
        {                                                   //if these 2 frames are quite close together
            xyzTemp.x = curve[0]->points[pointIndex - 1] - curve[0]->points[pointIndex];
            xyzTemp.y = curve[1]->points[pointIndex - 1] - curve[1]->points[pointIndex];
            xyzTemp.z = curve[2]->points[pointIndex - 1] - curve[2]->points[pointIndex];
            //object position:-Z, X, Y (UZI position mode 33)
            moveAmount.x = -xyzTemp.z;
            moveAmount.y = xyzTemp.x;
            moveAmount.z = xyzTemp.y;
            for (index = 0; index < MAX_NUM_TRAILS; index++)
            {                                               //for each trail in the ship
                if (ship->trail[index] != NULL)
                {
                    trailMove(ship->trail[index], &moveAmount);//move it a bit
                }
            }
            return;
        }
    }
#if NIS_ERROR_CHECKING
    dbgWarningf(DBG_Loc, "No nearby consecutive frames at %.2f.", NIS->timeElapsed);
#endif
}

/*-----------------------------------------------------------------------------
    Name        : nisTrailZero
    Description : NIS-event callback for zeroing the length of a ship's engine trail(s)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisTrailZero(nisplaying *NIS, nisevent *event)
{
    Ship *ship;
    sdword index;

    ship = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;
    for (index = 0; index < MAX_NUM_TRAILS; index++)
    {                                                       //for every trail in the ship
        if (ship->trail[index] != NULL)
        {
            trailZeroLength(ship->trail[index]);            //zero it out
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : nisUniversePauseToggle
    Description : NIS-event callback for pausing the universe
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisUniversePauseToggle(nisplaying *NIS, nisevent *event)
{
    nisUniversePause ^= TRUE;
#if NIS_VERBOSE_LEVEL >= 1
    dbgMessagef("NIS has %sPAUSED the universe.", nisUniversePause ? "" : "UN");
#endif
}

/*-----------------------------------------------------------------------------
    Name        : nisNewEnvironment
    Description : Set the background and lighting to something new.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisNewEnvironment(nisplaying *NIS, nisevent *event)
{
    nisenvironment *env = (nisenvironment *)event->param[0];
    char path[PATH_MAX];

    if (nisSeeking)
    {
        return;
    }
    // CAT: memStringDupe (the exe has no non-volatile pool variant; nisNewEnvironment 0x004d0e30)
    if (nisPreviousBackground == NULL)
    {
        nisPreviousBackground = memStringDupe(btgLastBackground);
    }
    if (nisPreviousLighting == NULL)
    {
        nisPreviousLighting = memStringDupe(lightCurrentLighting);
    }
#ifdef _WIN32
    sprintf(path, "BTG\\%s.btg", env->name);                //load the background
#else
    sprintf(path, "BTG/%s.btg", env->name);                //load the background
#endif
    nisPreviousTheta = btgGetTheta();
    nisPreviousPhi = btgGetPhi();
    btgSetTheta(env->theta);
    btgSetPhi(env->phi);
    btgLoad(path);

#ifdef _WIN32
    sprintf(path,"hsf\\%s.hsf",env->name);
#else
    sprintf(path,"hsf/%s.hsf",env->name);
#endif
    lightParseHSF(path);                               //load the lighting
}

/*-----------------------------------------------------------------------------
    Name        : nisUniverseHideToggle
    Description : NIS-event callback for hiding all non-NIS ships in the universe.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisUniverseHideToggle(nisplaying *NIS, nisevent *event)
{
    nisUniverseHidden ^= TRUE;
#if NIS_VERBOSE_LEVEL >= 1
    dbgMessagef("NIS has %sHIDDEN the universe.", nisUniverseHidden ? "" : "UN");
#endif
    if (nisUniverseHidden)
    {                                                       //if hiding everything
        univHideJustAboutEverything();
    }
    else
    {                                                       //else everything being unhidden
        univUnhideJustAboutEverything();                    //show everything previously hidden
        univUpdateRenderList();
    }
}

/*-----------------------------------------------------------------------------
    Name        : nisNewEvent
    Description : Allocate and return a new event.
    Inputs      : opcode - opcode for the new event
    Outputs     : allocates new event, sets the current time and object
    Return      : newly allocated event.
----------------------------------------------------------------------------*/
nisevent *nisNewEvent(nisopcode opcode)
{
    dbgAssertOrIgnore(nisEventIndex >= 0);
    nisEventIndex++;
    nisEventList = memRealloc(nisEventList, sizeof(nisevent) * nisEventIndex, "nisEventBuffer", 0);
    dbgAssertOrIgnore(nisCurrentTime != REALlyBig);
    dbgAssertOrIgnore(nisCurrentObject != -1);
    dbgAssertOrIgnore(opcode >= 0 && opcode < NEO_LastNEO);
    nisEventList[nisEventIndex - 1].time = nisCurrentTime;
    nisEventList[nisEventIndex - 1].shipID = nisCurrentObject;
    nisEventList[nisEventIndex - 1].code = opcode;
    nisEventList[nisEventIndex - 1].initialID = nisEventIndex;
    return(&nisEventList[nisEventIndex - 1]);
}

nisevent* nisNewAnimaticEvent(nisopcode opcode)
{
    dbgAssertOrIgnore(nisEventIndex >= 0);
    nisEventIndex++;
    nisEventList = memRealloc(nisEventList, sizeof(nisevent) * nisEventIndex, "nisEventBuffer", 0);
    dbgAssertOrIgnore(nisCurrentTime != REALlyBig);
    dbgAssertOrIgnore(opcode >= 0 && opcode < NEO_LastNEO);
    nisEventList[nisEventIndex - 1].time = nisCurrentTime;
    nisEventList[nisEventIndex - 1].shipID = nisCurrentObject;
    nisEventList[nisEventIndex - 1].code = opcode;
    nisEventList[nisEventIndex - 1].initialID = nisEventIndex;
    return(&nisEventList[nisEventIndex - 1]);
}

/*-----------------------------------------------------------------------------
    Name        : nisNewEventNoObject
    Description : Allocate and return a new event.
    Inputs      : opcode - opcode for the new event
    Outputs     : allocates new event, sets the current time and object
    Return      : newly allocated event.
----------------------------------------------------------------------------*/
nisevent *nisNewEventNoObject(nisopcode opcode)
{
    dbgAssertOrIgnore(nisEventIndex >= 0);
    nisEventIndex++;
    nisEventList = memRealloc(nisEventList, sizeof(nisevent) * nisEventIndex, "nisEventBuffer", 0);
    dbgAssertOrIgnore(nisCurrentTime != REALlyBig);
    dbgAssertOrIgnore(opcode >= 0 && opcode < NEO_LastNEO);
    nisEventList[nisEventIndex - 1].time = nisCurrentTime;
    nisEventList[nisEventIndex - 1].shipID = 0;
    nisEventList[nisEventIndex - 1].code = opcode;
    nisEventList[nisEventIndex - 1].initialID = nisEventIndex;
    return(&nisEventList[nisEventIndex - 1]);
}

/*-----------------------------------------------------------------------------
    Name        : nisDeathDamageSet
    Description : Script parsing callback
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void nisDeathDamageSet(char *directory,char *field,void *dataToFillIn)
{
    nisevent *event = nisNewEvent(NEO_DeathDamage);
    event->param[0] = EDT_AccumDamage;
}
void nisDeathProjectileSet(char *directory,char *field,void *dataToFillIn)
{
    nisevent *event = nisNewEvent(NEO_DeathProjectile);
    event->param[0] = EDT_ProjectileHit;
}
void nisDeathBeamSet(char *directory,char *field,void *dataToFillIn)
{
    nisevent *event = nisNewEvent(NEO_DeathBeam);
    event->param[0] = EDT_BeamHit;
}
void nisAttackSet(char *directory,char *field,void *dataToFillIn)
{
    nisevent *event = nisNewEvent(NEO_Attack);
    sdword enemy = nisFindObjectByName(nisCurrentHeader, field);
    event->param[0] = enemy;
}
void cat_nisRepairSet(char *directory,char *field,void *dataToFillIn)
{                                                           // CAT: "repair <object>" (cat_nisRepairSet 0x004d11a0)
    nisevent *event = nisNewEvent(NEO_Repair);
    sdword target = nisFindObjectByName(nisCurrentHeader, field);
    event->param[0] = target;
}
void nisHaltAttackSet(char *directory,char *field,void *dataToFillIn)
{
    nisNewEvent(NEO_HaltAttack);
}
void nisFireSet(char *directory,char *field,void *dataToFillIn)
{
    nisNewEvent(NEO_Fire);
}
//set the current object for subsequent events
void nisCurrentObjectSet(char *directory,char *field,void *dataToFillIn)
{
    nisCurrentObject = nisFindObjectByName(nisCurrentHeader, field);
}
//make no object the current object
void nisCurrentObjectClear(char *directory,char *field,void *dataToFillIn)
{
    nisCurrentObject = -1;
}
//sets the time for subsequent events
void nisCurrentTimeSet(char *directory,char *field,void *dataToFillIn)
{
    unsigned int i;
    sdword nScanned;
    for (i = 0; (field[i] = toupper(field[i])); i++) { }
    if (strstr(field, "FRAME") == field)
    {
        nScanned = sscanf(field, "FRAME %f", &nisCurrentTime);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
        // CAT: no range check: the exe's nisCurrentTimeSet 0x004d1380 has none, and the install's nislet01.script
        //      keys events at frame 3209, the NIS's own length (HW1's debug check stopped the parse there)
#endif
        nisCurrentTime /= NIS_FrameRate;                    //convert from frames to seconds
    }
    else
    {
        nScanned = sscanf(field, "%f", &nisCurrentTime);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);                   // CAT: no range check (see above)
#endif
    }
}
//show an object
void nisCurrentObjectShow(char *directory,char *field,void *dataToFillIn)
{
    nisNewEvent(NEO_Show);
}
//hide an object
void nisCurrentObjectHide(char *directory,char *field,void *dataToFillIn)
{
    nisNewEvent(NEO_Hide);
}
void nisInvincibleSet(char *directory,char *field,void *dataToFillIn)
{
    nisNewEvent(NEO_Invincible);
}
void nisVincibleSet(char *directory,char *field,void *dataToFillIn)
{
    nisNewEvent(NEO_Vincible);
}
void nisSoundEventSet(char *directory,char *field,void *dataToFillIn)
{
    nisevent *event = nisNewEvent(NEO_SoundEvent);
    event->param[0] = event->param[1] = 0;
    int param = event->param[0];
    sscanf(field, "%d", &param);
    event->param[0] = param;
}
/*-----------------------------------------------------------------------------
    Name        : nisSeparateExpression
    Description : Separate an expression (as contained in parenthesis) and return a pointer to the next item in a string
    Inputs      : in - string that may contain an expression in parenthesis
    Outputs     : out - separated expression
    Return      : pointer to string as it appears after the expression.
----------------------------------------------------------------------------*/
char *nisSeparateExpression(char *in, char *out)
{
    sdword parenNumber = 0;
    static char *delimiters = ", \t";

    while (*in)
    {
        *out = 0;
        *(out + 1) = 0;
        if (*in == '(')
        {                                                   //opening parenthesis
            parenNumber++;
        }
        else if (*in == ')')
        {
            parenNumber--;
            if (parenNumber == 0)
            {                                               //end of expression due to closing parenthesis
                *out = *in;
                dbgAssertOrIgnore(strchr(delimiters, *(in + 1)));
                return(in + 1);
            }
        }
        else if (strchr(delimiters, *in) && parenNumber == 0)
        {
            return(in);
        }
        *out = *in;
        out++;
        in++;
    }
    return(in);
}
void nisSpeechEventSet(char *directory,char *field,void *dataToFillIn)
{
    char *actorString;
    char *nextString;
    char eventNumberString[256];
    double evaluatedNumber;
    ERR_TYPE exprError;
    unsigned int i;
    nisevent *event = nisNewEvent(NEO_SpeechEvent);
    sdword actor = -1;

    event->param[0] = event->param[1] = 0;
    nextString = nisSeparateExpression(field, eventNumberString);
    evaluatedNumber = evalEvaluate(eventNumberString, &exprError);
#if NIS_ERROR_CHECKING
    if (exprError != ALL_OK)
    {
        dbgFatalf(DBG_Loc, "%s in expression '%s'", evalErrorString(exprError), field);
    }
#endif
    event->param[0] = (udword)(sdword)evaluatedNumber;
    if (*nextString)
    {
        int param = event->param[1];
        sscanf(nextString + 1, "%d", &param);
        event->param[1] = param;
    }
    for (i = 0; (field[i] = toupper(field[i])); i++) { }
    if ((actorString = strstr(field, "ACTOR")) != NULL)
    {
        sscanf(actorString + strlen("ACTOR "), "%d", &actor);
        dbgAssertOrIgnore(actor > 0 && actor <= 3);
        event->param[1] |= actor << NIS_ActorShiftBits;
    }
    else
    {
        event->param[1] |= NIS_ActorMask;
    }
}
void nisFleetSpeechEventSet(char *directory,char *field,void *dataToFillIn)
{
    nisevent *event = nisNewEvent(NEO_FleetSpeechEvent);
    char *nextString;
    char eventNumberString[256];
    double evaluatedNumber;
    ERR_TYPE exprError;

    event->param[0] = event->param[1] = 0;
    nextString = nisSeparateExpression(field, eventNumberString);
    evaluatedNumber = evalEvaluate(eventNumberString, &exprError);
#if NIS_ERROR_CHECKING
    if (exprError != ALL_OK)
    {
        dbgFatalf(DBG_Loc, "%s in expression '%s'", evalErrorString(exprError), field);
    }
#endif
    event->param[0] = (udword)(sdword)evaluatedNumber;
    if (*nextString)
    {
        int param = event->param[1];
        sscanf(nextString + 1, "%d", &param);
        event->param[1] = param;
    }
}
void nisAnimaticSpeechEventSet(char* directory, char* field, void* dataToFillIn)
{
    nisevent* event = nisNewAnimaticEvent(NEO_AnimaticSpeechEvent);
    char *nextString;
    char eventNumberString[256];
    double evaluatedNumber;
    ERR_TYPE exprError;

    event->param[0] = event->param[1] = 0;
    nextString = nisSeparateExpression(field, eventNumberString);
    evaluatedNumber = evalEvaluate(eventNumberString, &exprError);
#if NIS_ERROR_CHECKING
    if (exprError != ALL_OK)
    {
        dbgFatalf(DBG_Loc, "%s in expression '%s'", evalErrorString(exprError), field);
    }
#endif
    event->param[0] = (udword)(sdword)evaluatedNumber;
    if (*nextString)
    {
        int param = event->param[1];
        sscanf(nextString + 1, "%d", &param);
        event->param[1] = param;
    }
}

void nisRemainAtEndSet(char *directory,char *field,void *dataToFillIn)
{
    char optionalTeamName[50];
    nisevent *event;

    optionalTeamName[0] = 0;
    sscanf(field, "%s", optionalTeamName);

    event = nisNewEvent(NEO_RemainAtEnd);
    if (strlen(optionalTeamName) > 0)
    {
        event->param[0] = (memsize)memStringDupe(optionalTeamName);
    }
    else
    {
        event->param[0] = 0;
    }
}

void nisCameraFOVSet(char *directory,char *field,void *dataToFillIn)
{
    // CAT: an event of no object whose parameter is the field of view as a float (nisCameraFOVSet 0x004d2290:
    //      sscanf "%f"); GoK's version read "%d" with scanf (from stdin) into an object event
    nisevent *event = nisNewEventNoObject(NEO_CameraFOV);
    real32 fov = 0.0f;
    sscanf(field, "%f", &fov);
    event->param[0] = Real32ToSdword(fov);
}
void cat_nisCameraClipNearSet(char *directory,char *field,void *dataToFillIn)
{                                                           // CAT: "cameraClipNear <f>" (0x004d2330)
    nisevent *event = nisNewEventNoObject(NEO_CameraClipNear);
    real32 clip = 0.0f;
    sscanf(field, "%f", &clip);
    event->param[0] = Real32ToSdword(clip);
}
void cat_nisCameraClipFarSet(char *directory,char *field,void *dataToFillIn)
{                                                           // CAT: "cameraClipFar <f>" (0x004d23d0)
    nisevent *event = nisNewEventNoObject(NEO_CameraClipFar);
    real32 clip = 0.0f;
    sscanf(field, "%f", &clip);
    event->param[0] = Real32ToSdword(clip);
}
void nisRaceSwap(char *directory,char *field,void *dataToFillIn)
{
    sdword index;

    // CAT: Cataclysm swaps unless the player is race 2, the Kushan (nisRaceSwap 0x004d2470); the swapped races are
    //      the Sect and the Beast
    if (universe.curPlayerPtr->race == RACE_Kushan)
    {
        return;
    }
    for (index = 0; index < nisCurrentHeader->nObjectPaths; index++)
    {                                                       //swap race of all obj paths
        if (nisCurrentHeader->objectPath[index].race == RACE_Sect)
        {
            nisCurrentHeader->objectPath[index].race = RACE_Beast;
        }
        else if(nisCurrentHeader->objectPath[index].race == RACE_Beast)
        {
            nisCurrentHeader->objectPath[index].race = RACE_Sect;
        }
    }
    bitSet(nisCurrentHeader->flags, NHF_RacesSwapped);      //and remember that we swapped it
}
void nisCameraCutSet(char *directory,char *field,void *dataToFillIn)
{
    nisevent *event = nisNewEventNoObject(NEO_CameraCut);
    sdword nScanned;
    real32 value;
    unsigned int i;
    for (i = 0; (field[i] = toupper(field[i])); i++) { }
    if (strstr(field, "FRAME"))
    {
        nScanned = sscanf(field, "%f FRAME", &value);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
        value /= NIS_FrameRate;                   //convert from frames to seconds
    }
    else
    {
        nScanned = sscanf(field, "%f", &value);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
    }
    event->param[0] = Real32ToSdword(value);
}
void nisBlackScreenSet(char *directory, char *field, void *dataToFillIn)
{
    nisNewEventNoObject(NEO_BlackScreenStart);
}
void nisEndBlackScreenSet(char *directory, char *field, void *dataToFillIn)
{
    nisNewEventNoObject(NEO_BlackScreenEnd);
}
void nisTextFontSet(char *directory, char *field, void *dataToFillIn)
{
    nisCurrentTextCardFont = frFontRegister(field);
}
void nisTextColorSet(char *directory, char *field, void *dataToFillIn)
{
    sdword nScanned, red, green, blue;

    nScanned = sscanf(field, "%d,%d,%d", &red, &green, &blue);
#if NIS_ERROR_CHECKING
    dbgAssertOrIgnore(nScanned == 3);
    dbgAssertOrIgnore(red >= 0 && red < 256 && green >= 0 && green < 256 && blue >= 0 && blue < 256);
#endif
    nisCurrentTextCardColor = colRGB(red, green, blue);
}
void nisTextPositionSet(char *directory, char *field, void *dataToFillIn)
{
    sdword nScanned, x, y;

    nScanned = sscanf(field, "%d,%d", &x, &y);
#if NIS_ERROR_CHECKING
    dbgAssertOrIgnore(nScanned == 2);
    dbgAssertOrIgnore(x >= 0 && x < MAIN_WindowWidth);
    dbgAssertOrIgnore(y >= 0 && y < MAIN_WindowHeight * 10);
#endif
    nisCurrentTextCardX = x;
    nisCurrentTextCardY = y;
}
void nisTextScrollSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEventNoObject(NEO_TextScroll);
    sdword nScanned;
    real32 duration, distance;
    unsigned int i;

    for (i = 0; (field[i] = toupper(field[i])); i++) { }
    if (strstr(field, "FRAME"))
    {
        nScanned = sscanf(field, "%f,%f FRAME", &distance, &duration);
        duration /= NIS_FrameRate;                   //convert from frames to seconds
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 2);
        dbgAssertOrIgnore(duration > 0.0f && duration < 300.0f);
#endif
    }
    else
    {
        nScanned = sscanf(field, "%f,%f", &distance, &duration);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 2);
        dbgAssertOrIgnore(duration > 0.0f && duration < 300.0f);
#endif
    }
    event->param[0] = Real32ToSdword(distance);
    event->param[1] = Real32ToSdword(duration);
}
void nisTextFadeSet(char *directory, char *field, void *dataToFillIn)
{
    sdword nScanned;
    real32 fade;
    unsigned int i;

    for (i = 0; (field[i] = toupper(field[i])); i++) { }
    if (strstr(field, "FRAME"))
    {
        nScanned = sscanf(field, "%f FRAME", &fade);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
        fade /= NIS_FrameRate;                   //convert from frames to seconds
    }
    else
    {
        nScanned = sscanf(field, "%f", &fade);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
    }
    if (dataToFillIn != NULL)
    {
        nisCurrentTextFadeOut = fade;
    }
    else
    {
        nisCurrentTextFadeIn = fade;
    }
}
void nisTextMarginSet(char *directory, char *field, void *dataToFillIn)
{
    sdword nScanned;

    nScanned = sscanf(field, "%d", &nisCurrentTextMargin);
#if NIS_ERROR_CHECKING
    dbgAssertOrIgnore(nScanned == 1);
#endif
}
void nisTextDurationSet(char *directory, char *field, void *dataToFillIn)
{
    sdword nScanned;
    real32 value;
    unsigned int i;

    for (i = 0; (field[i] = toupper(field[i])); i++) { }
    if (strstr(field, "FRAME"))
    {
        nScanned = sscanf(field, "%f FRAME", &value);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
        value /= NIS_FrameRate;                   //convert from frames to seconds
    }
    else
    {
        nScanned = sscanf(field, "%f", &value);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
    }
    nisCurrentDuration = value;
}
void nisTextCardSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEventNoObject(NEO_TextCard);
    nistextcard *card;
    char *newLine, *lineStart;
    fonthandle fhSave = fontCurrentGet();
    char *text;
    strGamesMessages message;

    //find the text we are to draw
    message = strNameToEnum(field);

    dbgAssertOrIgnore(message != NumStrings);
    text = strGetString(message);
    //clear out quotes from the text.  I know this is modifying the string but we should get rid of bracketing quotes.
    while (*text == '"')
    {
        text++;
    }
    while (strlen(text) > 1 && text[strlen(text) - 1] == '"')
    {
        text[strlen(text) - 1] = 0;
    }
    //allocate text card and string
    card = memAlloc(sizeof(nistextcard) + strlen(text) + 1, "TextCard", NonVolatile);

    card->duration = nisCurrentDuration;
    card->c = nisCurrentTextCardColor;
    card->font = nisCurrentTextCardFont;
    card->x = nisCurrentTextCardX;
    card->y = nisCurrentTextCardY;
    card->fadeIn = nisCurrentTextFadeIn;
    card->fadeOut = nisCurrentTextFadeOut;
    card->margin = nisCurrentTextMargin;
    strcpy((char *)(card + 1), text);
    card->text = (char *)(card + 1);

    fontMakeCurrent(nisCurrentTextCardFont);
    lineStart = card->text;
    newLine = strstr(lineStart, NIS_NewLine);
    while (newLine != NULL)
    {
        nisCurrentTextCardY += fontHeight(" ") + 1;
        nisCurrentTextCardX = nisCurrentTextMargin + fontWidthN(newLine + NIS_NewLineLength, newLine - lineStart);
        lineStart = newLine + NIS_NewLineLength;
        newLine = strstr(lineStart, NIS_NewLine);
    }
    nisCurrentTextCardX += fontWidth(lineStart);

    event->param[0] = (memsize)card;
    fontMakeCurrent(fhSave);
}
void nisCameraBlendInSet(char *directory, char *field, void *dataToFillIn)
{
    sdword nScanned;
    real32 time0, time1 = -1.0f;
    char *string0, *string1;
    unsigned int i;

    for (i = 0; (field[i] = toupper(field[i])); i++) { }
    string0 = strtok(field, " \n\t,");
    string1 = strtok(NULL, " \n\t,");
    //read in the first time
    dbgAssertOrIgnore(string0);
    if (string1 != NULL && strstr(string1, "FRAME"))
    {
        nScanned = sscanf(string0, "%f", &time0);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
        time0 /= NIS_FrameRate;                   //convert from frames to seconds
        string0 = strtok(NULL, " \n\t,");
        string1 = strtok(NULL, " \n\t,");
    }
    else
    {
        nScanned = sscanf(field, "%f", &time0);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
        string0 = string1;
        string1 = strtok(NULL, " \n\t,");
    }
    //read in the second time, if there was one
    if (string0)
    {
        if (string1 != NULL && strstr(string1, "FRAME"))
        {
            nScanned = sscanf(string0, "%f", &time1);
#if NIS_ERROR_CHECKING
            dbgAssertOrIgnore(nScanned == 1);
#endif
            time1 /= NIS_FrameRate;                   //convert from frames to seconds
        }
        else
        {
            nScanned = sscanf(field, "%f", &time1);
#if NIS_ERROR_CHECKING
            dbgAssertOrIgnore(nScanned == 1);
#endif
        }
    }
#if NIS_ERROR_CHECKING
    if (time0 <= 0.0f || time0 > 400.4f || (time1 != -1.0f && (time1 <= 0.0f || time1 > 400.4f)))
    {
        dbgFatalf(DBG_Loc, "Error scanning '%s'", field);
    }
#endif
    nisCameraCutTime = time0;
    nisCameraCutStage2 = time1;
}
/*
void nisCameraBlendOutSet(char *directory, char *field, void *dataToFillIn)
{
    nisCameraCutOut = TRUE;
}
*/
void nisScissorBlendSet(char *directory, char *field, void *dataToFillIn)
{
    sdword nScanned;
    real32 value;
    nisevent *event;
    unsigned int i;

    for (i = 0; (field[i] = toupper(field[i])); i++) { }
    if (strstr(field, "FRAME"))
    {
        nScanned = sscanf(field, "%f FRAME", &value);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
        value /= NIS_FrameRate;                   //convert from frames to seconds
    }
    else
    {
        nScanned = sscanf(field, "%f", &value);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
    }
#if NIS_ERROR_CHECKING
    if (nScanned != 1 || value <= 0.0f || value > 400.4f)
    {
        dbgFatalf(DBG_Loc, "Error scanning '%s'", field);
    }
#endif
    if (dataToFillIn == NULL)
    {
        nisScissorFadeIn = value;
    }
    else
    {
        event = nisNewEventNoObject(NEO_ScissorOut);
        event->param[0] = Real32ToSdword(value);
    }
}
void nisFocusAtEndSet(char *directory,char *field,void *dataToFillIn)
{
    nisNewEvent(NEO_FocusAtEnd);
}
void nisFocusSet(char *directory,char *field,void *dataToFillIn)
{
    nisNewEventNoObject(NEO_Focus);
}
void nisMusicStartSet(char *directory,char *field,void *dataToFillIn)
{
    sdword trackNumber, nScanned;
    nisevent *event = nisNewEventNoObject(NEO_StartMusic);

    nScanned = sscanf(field, "%d", &trackNumber);
#if NIS_ERROR_CHECKING
    if (nScanned != 1 || trackNumber < 0 || trackNumber > 99)
    {
        dbgFatalf(DBG_Loc, "Error scanning '%s' for a track number.", field);
    }
#endif
    event->param[0] = trackNumber;
}
void nisMusicStopSet(char *directory,char *field,void *dataToFillIn)
{
    real32 fadeOut;
    sdword nScanned;
    unsigned int i;
    nisevent *event = nisNewEventNoObject(NEO_StopMusic);

    for (i = 0; (field[i] = toupper(field[i])); i++) { }
    if (strstr(field, "FRAME"))
    {
        nScanned = sscanf(field, "%f FRAME", &fadeOut);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
        fadeOut /= NIS_FrameRate;                   //convert from frames to seconds
    }
    else
    {
        nScanned = sscanf(field, "%f", &fadeOut);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
    }
#if NIS_ERROR_CHECKING
    if (nScanned != 1 || fadeOut < 0.0f || fadeOut > 99.0f)
    {
        dbgFatalf(DBG_Loc, "Error scanning '%s' for a fade-out.", field);
    }
#endif
    event->param[0] = Real32ToSdword(fadeOut);
}
void nisCentreShipSet(char *directory,char *field,void *dataToFillIn)
{
    dbgAssertOrIgnore(nisCurrentObject >= 0);
    dbgAssertOrIgnore(nisCurrentObject < nisCurrentHeader->nObjectPaths);
    bitSet(nisCurrentHeader->objectPath[nisCurrentObject].race, NIS_CentreShip);
}
void nisFadeSetSet(char *directory,char *field,void *dataToFillIn)
{
    nisevent *event = nisNewEventNoObject(NEO_BlackFadeSet);
    real32 level;
    sdword nScanned;

    nScanned = sscanf(field, "%f", &level);
    if (strchr(field, '%'))
    {                                                   //if it's a percentage
        level /= 100.0f;
    }
#if NIS_ERROR_CHECKING
    dbgAssertOrIgnore(nScanned == 1);
    if (level < 0.0f || level > 1.0f)
    {
        dbgFatalf(DBG_Loc, "'%s' is not in the range of 0..1", field);
    }
#endif
    event->param[0] = Real32ToSdword(level);
    // CAT: an optional ", <n>" second parameter (nisFadeSetSet 0x004d2fa0; nisBlackFadeSet keeps its low byte)
    {
        char *comma = strchr(field, ',');
        sdword flag = 0;

        if (comma != NULL)
        {
            sscanf(comma + 1, "%d", &flag);
        }
        event->param[1] = flag;
    }
}
void nisFadeToSet(char *directory,char *field,void *dataToFillIn)
{
    nisevent *event = nisNewEventNoObject(NEO_BlackFadeTo);
    real32 level, duration;
    sdword nScanned;
    char *time;
    unsigned int i;

    time = strchr(field, ',');
    dbgAssertOrIgnore(time);
    *time = 0;
    time++;
    nScanned = sscanf(field, "%f", &level);
    if (strchr(field, '%'))
    {                                                   //if it's a percentage
        level /= 100.0f;
    }
#if NIS_ERROR_CHECKING
    if (level < 0.0f || level > 1.0f)
    {
        dbgFatalf(DBG_Loc, "'%s' is not in the range of 0..1", field);
    }
#endif
    for (i = 0; (time[i] = toupper(time[i])); i++) { }
    if (strstr(time, "FRAME"))
    {
        nScanned = sscanf(time, "%f FRAME", &duration);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
        duration /= NIS_FrameRate;                   //convert from frames to seconds
    }
    else
    {
        nScanned = sscanf(time, "%f", &duration);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
    }
#if NIS_ERROR_CHECKING
    if (nScanned != 1 || duration <= 0.0f || duration > 400.4f)
    {
        dbgFatalf(DBG_Loc, "Error scanning '%s'", time);
    }
#endif
    event->param[0] = Real32ToSdword(level);
    event->param[1] = Real32ToSdword(duration);
}
void nisColorSchemeSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEvent(NEO_ColorScheme);
    sdword nScanned, value;

    nScanned = sscanf(field, "%d", &value);
#if NIS_ERROR_CHECKING
    dbgAssertOrIgnore(nScanned == 1);
    dbgAssertOrIgnore(value >= 0 && value < MAX_MULTIPLAYER_PLAYERS);
#endif
    event->param[0] = value;
}
void nisMeshAnimationStartSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEvent(NEO_MeshAnimationStart);
    event->param[0] = (memsize)memStringDupe(field);
}
void nisMeshAnimationStopSet(char *directory, char *field, void *dataToFillIn)
{
    nisNewEvent(NEO_MeshAnimationStop);
}
void nisMeshAnimationPauseSet(char *directory, char *field, void *dataToFillIn)
{
    nisNewEvent(NEO_MeshAnimationPause);
}
void nisMeshAnimationSeekSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEvent(NEO_MeshAnimationSeek);
    real32 seekTime;
    sdword nScanned;
    unsigned int i;

    for (i = 0; (field[i] = toupper(field[i])); i++) { }
    if (strstr(field, "FRAME"))
    {
        nScanned = sscanf(field, "FRAME %f", &seekTime);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
        seekTime /= NIS_FrameRate;                   //convert from frames to seconds
    }
    else
    {
        nScanned = sscanf(field, "%f", &seekTime);
#if NIS_ERROR_CHECKING
        dbgAssertOrIgnore(nScanned == 1);
#endif
    }
#if NIS_ERROR_CHECKING
    if (nScanned != 1 || seekTime < 0.0f || seekTime > 300.0f)
    {
        dbgFatalf(DBG_Loc, "Error scanning '%s' for a seek time.", field);
    }
#endif
    event->param[0] = Real32ToSdword(seekTime);
}
void nisBlendCameraToFocusSet(char *directory, char *field, void *dataToFillIn)
{
    nisNewEventNoObject(NEO_BlendCameraEndPoint);
}
void nisVolumesSet(char *directory, char *field, void *dataToFillIn)
{
    sdword nScanned;
    float level;
    nisevent *event = nisNewEventNoObject((memsize)dataToFillIn);

    nScanned = sscanf(field, "%f", &level);

    dbgAssertOrIgnore(nScanned == 1);
    dbgAssertOrIgnore(level >= 0.0f && level <= 1.0f);
    event->param[0] = Real32ToSdword(level);
}
void nisTrailZeroSet(char *directory, char *field, void *dataToFillIn)
{
    nisNewEvent(NEO_TrailZeroLength);
}
void nisTrailMoveSet(char *directory, char *field, void *dataToFillIn)
{
    nisNewEvent(NEO_TrailMove);
}
void nisUniversePauseSet(char *directory, char *field, void *dataToFillIn)
{
    nisNewEventNoObject(NEO_UniversePauseToggle);
}
void nisUniverseHideSet(char *directory, char *field, void *dataToFillIn)
{
    nisNewEventNoObject(NEO_UniverseHideToggle);
}
void nisNewEnvironmentSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEventNoObject(NEO_NewEnvironment);
    char *name, *scan;
    nisenvironment *env = memAlloc(sizeof(nisenvironment), "NISEnv", NonVolatile);

    name = strtok(field, " \n\t,");
    dbgAssertOrIgnore(name);
    dbgAssertOrIgnore(strlen(name) < NIS_EnvNameLength);
    strcpy(env->name, name);
    scan = strtok(NULL, " \n\t,");
    dbgAssertOrIgnore(scan);
    sscanf(scan, "%f", &env->theta);
    scan = strtok(NULL, " \n\t,");
    dbgAssertOrIgnore(scan);
    sscanf(scan, "%f", &env->phi);
    event->param[0] = (memsize)env;
}
void nisLookyObjectSet(char *directory, char *field, void *dataToFillIn)
{
    dbgAssertOrIgnore(nisCurrentObject != -1);

    nisCurrentHeader->iLookyObject = nisCurrentObject;
}
//scan in formatted like this:
//<hh>:<mm>:<ss>:<ff> <x> <y> [<bRightJustified>] [<red>] [<green>] [<blue>] [<font>] [<format>]
//defaults are:                 ^TRUE^             ^255^    ^255^     ^255^  ^default^  ^"%s"^
void nisSMPTEOnSet(char *directory, char *field, void *dataToFillIn)
{
    char timeString[80];
    char formatString[80] = "%s";
    char fontString[80] = "default.hff";
    char justifyString[80] = "TRUE";
    ubyte red = UBYTE_Max, green = UBYTE_Max, blue = UBYTE_Max;
    sdword x, y;
    sdword nScanned;
    real32 hours, minutes, seconds, frames;
    nisSMPTE *newSMPTE;
    nisevent *event = nisNewEventNoObject(NEO_SMPTEOn);

    //scan in all the parameters
    RemoveCommasFromString(field);
    /*nScanned = sscanf(field, "%s %d %d %s %d %d %d %s %s", timeString, &x, &y, &red, &green, &blue, fontString, formatString);*/
    nScanned = sscanf(field, "%s %d %d %s %hhu %hhu %hhu %s %s", timeString, &x, &y, justifyString, &red, &green, &blue, fontString, formatString);
    dbgAssertOrIgnore(nScanned >= 3);
    dbgAssertOrIgnore(strlen(formatString) > 1);

    //interpret the SMPTE timecode
    nScanned = sscanf(timeString, "%f:%f:%f:%f", &hours, &minutes, &seconds, &frames);
    dbgAssertOrIgnore(nScanned == 4);
    dbgAssertOrIgnore(hours >= 0 && hours < 24);
    dbgAssertOrIgnore(minutes >= 0 && minutes < 60);
    dbgAssertOrIgnore(seconds >= 0 && seconds < 60);
    dbgAssertOrIgnore(frames >= 0 && frames < NIS_FrameRate);

    //allocate parameter block and format string
    newSMPTE = memAlloc(sizeof(nisSMPTE) + strlen(formatString) + 1, "nisSMPTE", NonVolatile);
    strcpy((char *)(newSMPTE + 1), formatString);
    newSMPTE->format = (char *)(newSMPTE + 1);              //point to end of structure, where the format string is stored
    newSMPTE->x = x * MAIN_WindowWidth / 640;
    newSMPTE->y = y * MAIN_WindowHeight / 480;
    newSMPTE->bRightJustified = scriptStringToBool(justifyString);
    newSMPTE->startTime = nisCurrentTime - (hours * 3600 + minutes * 60 + seconds + frames / NIS_FrameRate);
    newSMPTE->font = frFontRegister(fontString);
    newSMPTE->c = colRGB(red, green, blue);
    event->param[0] = (memsize)newSMPTE;
}
void nisSMPTEOffSet(char *directory, char *field, void *dataToFillIn)
{
    nisNewEventNoObject(NEO_SMPTEOff);
}
//scan a string formatted like this:
//nLines, nLinesVar, width, widthVar, y, yVar[, hue, hueVar[, lum, lumVar[, sat, satVar[, alpha, alphaVar[, bTextured]]]]
//defaults are:                                 ^0^   ^0^     ^1^   ^0^     ^0^   ^0^      ^1^     ^0^       ^FALSE^
void nisStaticOnSet(char *directory, char *field, void *dataToFillIn)
{
    sdword nScanned;
    nisstatic *newStatic;
    sdword nLines, nLinesVariation, index;
    real32 width, widthVariation, y, yVariation;
    real32 hue = 0.0f, hueVariation = 0.0f, lum = 1.0f, lumVariation = 0.0f;
    real32 sat = 0.0f, satVariation = 0.0f, alpha = 1.0f, alphaVariation = 0.0f;
    nisevent *event = nisNewEventNoObject(NEO_StaticOn);
    char bTextureString[80] = "TRUE";

    RemoveCommasFromString(field);
    nScanned = sscanf(field, "%d %d %d %f %f %f %f %f %f %f %f %f %f %f %f %s", &index,
        &nLines, &nLinesVariation, &width, &widthVariation, &y, &yVariation,
        &hue, &hueVariation, &lum, &lumVariation, &sat, &satVariation, &alpha, &alphaVariation,
        bTextureString);
    dbgAssertOrIgnore(nScanned >= 7);
    dbgAssertOrIgnore(index >= 0 && index < NIS_NumberStatics);
    dbgAssertOrIgnore(hue >= 0.0f && hue <= 1.0f);
    dbgAssertOrIgnore(lum >= 0.0f && lum <= 1.0f);
    dbgAssertOrIgnore(sat >= 0.0f && sat <= 1.0f);
    dbgAssertOrIgnore(alpha >= 0.0f && alpha <= 1.0f);

    //scale some parameters by screen resolution
    width = primScreenToGLScaleX(width * MAIN_WindowWidth / 640.0f);
    widthVariation = primScreenToGLScaleX(widthVariation * MAIN_WindowWidth / 640.0f);
    y = primScreenToGLY(y * MAIN_WindowHeight / 480.0f);
    yVariation = primScreenToGLScaleY(yVariation * MAIN_WindowHeight / 480.0f);
    nLines = nLines * MAIN_WindowHeight / 480;              //more lines on a taller screen
    nLinesVariation = nLinesVariation * MAIN_WindowHeight / 480;
    newStatic = memAlloc(sizeof(nisstatic), "nisstatic", NonVolatile);
    newStatic->index = index;
    newStatic->nLines = nLines;
    newStatic->nLinesVariation = nLinesVariation;
    newStatic->width = width;
    newStatic->widthVariation = widthVariation;
    newStatic->y = y;
    newStatic->yVariation = yVariation;
    newStatic->hue = hue;
    newStatic->hueVariation = hueVariation;
    newStatic->lum = lum;
    newStatic->lumVariation = lumVariation;
    newStatic->sat = sat;
    newStatic->satVariation = satVariation;
    newStatic->alpha = alpha;
    newStatic->alphaVariation = alphaVariation;
    newStatic->bAlpha = (alpha != 1.0f || alphaVariation != 0.0f);
    newStatic->bTextured = scriptStringToBool(bTextureString);
    event->param[0] = (memsize)newStatic;
}
void nisStaticOffSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEventNoObject(NEO_StaticOff);
    sdword nScanned;

    int param = event->param[0];
    nScanned = sscanf(field, "%d", &param);
    event->param[0] = param;
    dbgAssertOrIgnore(nScanned == 1);
    dbgAssertOrIgnore(event->param[0] >= 0 && event->param[0] < NIS_NumberStatics);
}
void nisHideDerelictSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEventNoObject(dataToFillIn == NULL ? NEO_HideDerelict : NEO_ShowDerelict);
    DerelictType type = StrToDerelictType(field);

    dbgAssertOrIgnore(type != -1);
    event->param[0] = type;
}
void nisNewNISSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEventNoObject(NEO_StartNewNIS);
    char nisName[80], scriptName[80];
    char fileName[80];
    sdword nScanned;

    event->time -= UNIVERSE_UPDATE_PERIOD;                  //this is to make sure that hitting ESCAPE will seek over this event
    nScanned = sscanf(field, "%s %s", nisName, scriptName);
    dbgAssertOrIgnore(nScanned == 2);
    dbgAssertOrIgnore(strlen(nisName) > 0);
    dbgAssertOrIgnore(strlen(scriptName) > 0);
#ifdef _WIN32
    strcpy(fileName, "nis\\");
#else
    strcpy(fileName, "nis/");
#endif
    strcat(fileName, nisName);
    dbgAssertOrIgnore(fileExists(fileName, 0));
    event->param[0] = (memsize)memStringDupe(fileName);    // CAT: memStringDupe (nisNewNISSet 0x004d4380)
#ifdef _WIN32
    strcpy(fileName, "nis\\");
#else
    strcpy(fileName, "nis/");
#endif
    strcat(fileName, scriptName);
    dbgAssertOrIgnore(fileExists(fileName, 0));
    event->param[1] = (memsize)memStringDupe(fileName);
}
void nisAttributesSet(char *directory, char *field, void *dataToFillIn)
{
    sdword nScanned, attributes;
    nisevent *event = nisNewEvent(NEO_Attributes);

    nScanned = sscanf(field, "%d", &attributes);
    dbgAssertOrIgnore(nScanned == 1);
    event->param[0] = attributes;
}
void nisAttackKasSelectionSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEvent(NEO_AttackKasSelection);
    GrowSelection *selection;

    // CAT: the exe looks the selection up by the label's hash (cat_kasHashString; nisAttackKasSelectionSet
    //      0x004d45a0)
    selection = kasGetGrowSelectionPtrIfExists(cat_kasHashString(field));      //get a named selection
    dbgAssertOrIgnore(selection != NULL);
    dbgAssertOrIgnore(selection->selection->numShips > 0);          //verify there are ships in it
    event->param[0] = (memsize)(selection->selection);
}
void nisKeepMovingAtEndSet(char *directory, char *field, void *dataToFillIn)
{
    nisNewEvent(NEO_KeepMovingAtEnd);
}

void nisMinimumLODSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEvent(NEO_MinimumLOD);
    sdword nScanned;

    int param = event->param[0];
    nScanned = sscanf(field, "%d", &param);
    event->param[0] = param;
    dbgAssertOrIgnore(nScanned == 1);
    dbgAssertOrIgnore(event->param[0] >= 0 && event->param[0] < 10);
}

void nisDamageLevelSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEvent(NEO_DamageLevel);
    sdword nScanned;
    real32 percentage;

    nScanned = sscanf(field, "%f", &percentage);
    dbgAssertOrIgnore(nScanned == 1);
    if (strchr(field, '%'))
    {
        percentage /= 100.0f;
    }
    dbgAssertOrIgnore(percentage > 0.0f);
    event->param[0] = Real32ToSdword(percentage);
}

void cat_nisOpenSlipgateSet(char *directory, char *field, void *dataToFillIn)
{                                                           // CAT: "openSlipgate" (0x004d2190)
    nisNewEvent(NEO_OpenSlipgate);
}
void cat_nisCloseSlipgateSet(char *directory, char *field, void *dataToFillIn)
{                                                           // CAT: "closeSlipgate" (0x004d2210)
    nisNewEvent(NEO_CloseSlipgate);
}
/*-----------------------------------------------------------------------------
    Name        : cat_nisLightAmbientSet / cat_nisLightDiffuseSet
    Description : CAT (M2-R6 nis-fow-camera): "lightAmbient r,g,b" / "lightDiffuse r,g,b" of the current object,
                  an nisLightSource1 (light 0) or nisLightSource2 (light 1) ship (0x004d1690 / 0x004d1790); for any
                  other object the light index stays unset, as in the exe
----------------------------------------------------------------------------*/
static void catNisLightColourSet(nisopcode opcode, char *field)
{
    nisevent *event = nisNewEvent(opcode);
    sdword red = 0, green = 0, blue = 0;
    ShipType type;

    sscanf(field, "%d,%d,%d", &red, &green, &blue);
    type = (ShipType)nisCurrentHeader->objectPath[nisCurrentObject].type;
    if (type == nisLightSource1)
    {
        event->param[0] = 0;
    }
    else if (type == nisLightSource2)
    {
        event->param[0] = 1;
    }
    event->param[1] = red;
    event->param[2] = green;
    event->param[3] = blue;
}
void cat_nisLightAmbientSet(char *directory, char *field, void *dataToFillIn)
{
    catNisLightColourSet(NEO_LightAmbient, field);
}
void cat_nisLightDiffuseSet(char *directory, char *field, void *dataToFillIn)
{
    catNisLightColourSet(NEO_LightDiffuse, field);
}
void cat_nisGlobalAmbientSet(char *directory, char *field, void *dataToFillIn)
{                                                           // CAT: "globalAmbient r,g,b" (0x004d1890)
    nisevent *event = nisNewEventNoObject(NEO_GlobalAmbient);
    sdword red = 0, green = 0, blue = 0;

    sscanf(field, "%d,%d,%d", &red, &green, &blue);
    event->param[0] = red;
    event->param[1] = green;
    event->param[2] = blue;
}
void cat_nisNumberLightsSet(char *directory, char *field, void *dataToFillIn)
{                                                           // CAT: "numberLights <n>" (0x004d1960)
    nisevent *event = nisNewEventNoObject(NEO_NumberLights);
    sdword number = 0;

    sscanf(field, "%d", &number);
    event->param[0] = number;
}
void cat_nisMimicSet(char *directory, char *field, void *dataToFillIn)
{                                                           // CAT: "mimic <ship type>" (0x004d1a10)
    nisevent *event = nisNewEvent(NEO_Mimic);
    char typeName[256];
    ShipType type;

    typeName[0] = 0;
    sscanf(field, "%s", typeName);
    type = StrToShipType(typeName);
    if ((udword)type > (udword)nisLensflareSource1)
    {
        dbgFatalf(DBG_Loc, "Invalid ship type for NIS mimicing! (%s=%d)", typeName, type);
    }
    event->param[0] = type;
}
/*-----------------------------------------------------------------------------
    Name        : cat_nisFogSet
    Description : CAT (M2-R6 nis-fow-camera): "fog <on> <time> <r1> <g1> <b1> <r2> <g2> <b2> <speed> <density>"
                  (cat_nisFogSet 0x004d3490: sscanf "%d %f %d %d %d %d %d %d %f %f"; the two colours opaque, the
                  last value times 10,000,000)
----------------------------------------------------------------------------*/
void cat_nisFogSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEventNoObject(NEO_Fog);
    sdword on = 0, r1 = 0, g1 = 0, b1 = 0, r2 = 0, g2 = 0, b2 = 0;
    real32 time = 0.0f, speed = 0.0f, density = 0.0f;

    sscanf(field, "%d %f %d %d %d %d %d %d %f %f", &on, &time, &r1, &g1, &b1, &r2, &g2, &b2, &speed, &density);
    event->param[1] = colRGB(r1, g1, b1);
    event->param[2] = colRGB(r2, g2, b2);
    event->param[0] = on;
    event->param[5] = Real32ToSdword(speed);
    event->param[4] = Real32ToSdword((real32)((double)density * 10000000.0));
    event->param[3] = Real32ToSdword(time);
}
void cat_nisEboltsSet(char *directory, char *field, void *dataToFillIn)
{                                                           // CAT: "ebolts <0|1>" (0x004d3330)
    nisevent *event = nisNewEvent(NEO_Ebolts);
    sdword on = 0;

    sscanf(field, "%d", &on);
    event->param[0] = on;
}
void cat_nisBeastEffectSet(char *directory, char *field, void *dataToFillIn)
{                                                           // CAT: "beastEffect <0|1>" (0x004d33e0)
    nisevent *event = nisNewEvent(NEO_BeastEffect);
    sdword on = 0;

    sscanf(field, "%d", &on);
    event->param[0] = on;
}
/*-----------------------------------------------------------------------------
    Name        : cat_nisSubtitleSet
    Description : CAT (M2-R6 nis-fow-camera): "subtitle <index expression> <milliseconds>" (cat_nisSubtitleSet
                  0x004d1cc0: the index is evaluated like a speech event's number)
----------------------------------------------------------------------------*/
void cat_nisSubtitleSet(char *directory, char *field, void *dataToFillIn)
{
    nisevent *event = nisNewEventNoObject(NEO_Subtitle);
    char *nextString;
    char indexString[256];
    ERR_TYPE exprError;
    sdword duration = 0;

    event->param[0] = event->param[1] = 0;
    nextString = nisSeparateExpression(field, indexString);
    event->param[0] = (udword)(sdword)evalEvaluate(indexString, &exprError);
    if (*nextString)
    {
        sscanf(nextString + 1, "%d", &duration);
        event->param[1] = duration;
    }
}

// CAT: the exe's 81-row table (0x00888a60), HW1's rows in their places plus the Cataclysm commands
scriptEntry nisScriptTable[] =
{
    { "with",           nisCurrentObjectSet, NULL},
    { "at",             nisCurrentTimeSet, NULL},
    { "}",              nisCurrentObjectClear, NULL},
    { "deathDamage",    nisDeathDamageSet, NULL},
    { "deathProjectile",nisDeathProjectileSet, NULL},
    { "deathBeam",      nisDeathBeamSet, NULL},
    { "attack",         nisAttackSet, NULL},
    { "repair",         cat_nisRepairSet, NULL},                        // CAT
    { "haltAttack",     nisHaltAttackSet, NULL},
    { "fire",           nisFireSet, NULL},
    { "hide",           nisCurrentObjectHide, NULL},
    { "show",           nisCurrentObjectShow, NULL},
    { "invincible",     nisInvincibleSet, NULL},
    { "vincible",       nisVincibleSet, NULL},
    { "soundEvent",     nisSoundEventSet, NULL},
    { "speechEvent",    nisSpeechEventSet, NULL},
    { "fleetSpeechEvent", nisFleetSpeechEventSet, NULL},
    { "animaticSpeechEvent", nisAnimaticSpeechEventSet, NULL},
    { "startMusic",     nisMusicStartSet, NULL},
    { "stopMusic",      nisMusicStopSet, NULL},
    { "remainAtEnd",    nisRemainAtEndSet, NULL},
    { "cameraFOV",      nisCameraFOVSet, NULL},
    { "cameraClipNear", cat_nisCameraClipNearSet, NULL},                // CAT
    { "cameraClipFar",  cat_nisCameraClipFarSet, NULL},                 // CAT
    { "swapRaces",      nisRaceSwap, NULL},
    { "cameraCut",      nisCameraCutSet, NULL},
//    { "startBlackScreen",nisBlackScreenSet, NULL},
//    { "endBlackScreen", nisEndBlackScreenSet, NULL},
    { "fadeSet",        nisFadeSetSet, NULL},
    { "fadeTo",         nisFadeToSet, NULL},
    { "colorScheme",    nisColorSchemeSet, NULL},
    { "textFont",       nisTextFontSet, NULL},
    { "textColor",      nisTextColorSet, NULL},
    { "textPosition",   nisTextPositionSet, NULL},
    { "textMargin",     nisTextMarginSet, NULL},
    { "textDuration",   nisTextDurationSet, NULL},
    { "textScroll",     nisTextScrollSet, NULL},
    { "textFadeIn",     nisTextFadeSet, NULL},
    { "textFadeOut",    nisTextFadeSet, (void *)TRUE},
    { "textCard",       nisTextCardSet, NULL},
    { "cameraBlendIn",  nisCameraBlendInSet, NULL},
//    { "cameraBlendOut", nisCameraBlendOutSet, NULL},
    { "wideScreenIn",   nisScissorBlendSet, NULL},
    { "wideScreenOut",  nisScissorBlendSet, (void *)TRUE},
    { "focusAtEnd",     nisFocusAtEndSet, (void *)TRUE},
    { "focus",          nisFocusSet, (void *)TRUE},
    { "centreShip",     nisCentreShipSet, NULL},
    { "meshAnimStart",  nisMeshAnimationStartSet, NULL},
    { "meshAnimStop",   nisMeshAnimationStopSet, NULL},
    { "meshAnimPause",  nisMeshAnimationPauseSet, NULL},
    { "meshAnimSeek",   nisMeshAnimationSeekSet, NULL},
    { "blendCameraToFocus",   nisBlendCameraToFocusSet, NULL},
    { "musicVolume",    nisVolumesSet, (void *)NEO_MusicVolume},
    { "speechVolume",   nisVolumesSet, (void *)NEO_SpeechVolume},
    { "SFXVolume",      nisVolumesSet, (void *)NEO_SFXVolume},
    { "trailMove",      nisTrailMoveSet, NULL},
    { "trailZero",      nisTrailZeroSet, NULL},
    { "pauseUniverseToggle",  nisUniversePauseSet, NULL},
    { "hideUniverseToggle",  nisUniverseHideSet, NULL},
    { "environment",    nisNewEnvironmentSet, NULL},
    { "midPointLookAt", nisLookyObjectSet, NULL},
    { "SMPTEOn",        nisSMPTEOnSet, NULL},
    { "SMPTEOff",       nisSMPTEOffSet, NULL},
    { "openSlipgate",   cat_nisOpenSlipgateSet, NULL},                  // CAT
    { "closeSlipgate",  cat_nisCloseSlipgateSet, NULL},                 // CAT
    { "staticOn",       nisStaticOnSet, NULL},
    { "staticOff",      nisStaticOffSet, NULL},
    { "hideDerelict",   nisHideDerelictSet, NULL},
    { "showDerelict",   nisHideDerelictSet, (void *)TRUE},
    { "newNIS",         nisNewNISSet, NULL},
    { "attributes",     nisAttributesSet, NULL},
    { "attackKasSelection",nisAttackKasSelectionSet, NULL},
    { "keepMovingAtEnd",nisKeepMovingAtEndSet, NULL},
    { "numberLODs",     nisMinimumLODSet, NULL},
    { "health",         nisDamageLevelSet, NULL},
    { "lightAmbient",   cat_nisLightAmbientSet, NULL},                  // CAT
    { "lightDiffuse",   cat_nisLightDiffuseSet, NULL},                  // CAT
    { "globalAmbient",  cat_nisGlobalAmbientSet, NULL},                 // CAT
    { "numberLights",   cat_nisNumberLightsSet, NULL},                  // CAT
    { "mimic",          cat_nisMimicSet, NULL},                         // CAT
    { "fog",            cat_nisFogSet, NULL},                           // CAT
    { "ebolts",         cat_nisEboltsSet, NULL},                        // CAT
    { "beastEffect",    cat_nisBeastEffectSet, NULL},                   // CAT
    { "subtitle",       cat_nisSubtitleSet, NULL},                      // CAT
    { NULL, NULL, 0 }
};

/*-----------------------------------------------------------------------------
    Name        : nisCompareFunc
    Description : For sorting the object lists by parent name
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
int nisCompareFunc(const void *p1, const void *p2)
{
    return((udword)(((spaceobjpath *)p1)->parentIndex) - (udword)(((spaceobjpath *)p2)->parentIndex));
}

/*-----------------------------------------------------------------------------
    Name        : nisEventSortCB
    Description : For sorting the event lists by time, subsorted by origional order
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
int nisEventSortCB(const void *p1, const void *p2)
{
    if (((nisevent *)p1)->time < ((nisevent *)p2)->time)
    {
        return(-1);
    }
    else if (((nisevent *)p1)->time == ((nisevent *)p2)->time)
    {
        if (((nisevent *)p1)->initialID < ((nisevent *)p2)->initialID)
        {
            return(-1);
        }
        else
        {
            dbgAssertOrIgnore(((nisevent *)p1)->initialID != ((nisevent *)p2)->initialID);
            return(1);
        }
    }
    else
    {
        return(1);
    }
}

/*-----------------------------------------------------------------------------
    Name        : nisGenericObjectRegister
    Description : Register and load, if needed, generic object as a derelict
                    into this NIS's local generic object registry.
    Inputs      : header - what NIS header to load object into
                  name - name of object to load
    Outputs     :
    Return      : index, in this NIS's registry of the newly loaded generic object.
----------------------------------------------------------------------------*/
sdword nisGenericObjectRegister(nisheader *header, char *name)
{
    sdword index;
    nisgenericobject *generic;
    char shipFile[256];

    for (index = 0; index < header->nGenericObjects; index++)
    {
        if (!strcmp(header->genericObject[index]->name, name))
        {
            return(index);
        }
    }
    //if it gets here, no match was found.  Load in a new object.
    header->genericObject = memRealloc(header->genericObject,//reallocate the list of generic objects
        sizeof(nisgenericobject **) * (header->nGenericObjects + 1), "NISGenericList", 0);
    generic = memAlloc(nisGenericObjectSize(strlen(name)), "NISGenericObject", 0);
    header->genericObject[header->nGenericObjects] = generic;
    header->nGenericObjects++;
    strcpy(generic->name, name);
    generic->staticInfo = memAlloc(sizeof(DerelictStaticInfo), "NISGenericStat", 0);
    strcpy(shipFile, name);
    strcat(shipFile, ".shp");
#ifdef _WIN32
    InitStatDerelictInfoByPath(generic->staticInfo, NUM_DERELICTTYPES + index, "Misc\\", shipFile);
#else
    InitStatDerelictInfoByPath(generic->staticInfo, NUM_DERELICTTYPES + index, "Misc/", shipFile);
#endif
    return(header->nGenericObjects - 1);
}


#ifdef HW_PTR_64
void nisLoadAndConvertTo64Bit(char *fileName, void** loadAddress)
{
    int i;
    int j;

    int loopvar;
    int loopcount;

    char oldFile[80];
    char newFile[80];

    nisheader_disk *oldHeader = NULL;
    nisheader *newHeader = NULL;

    spaceobjpath_disk *oldObjPath = NULL;
    spaceobjpath *newObjPath = NULL;

    camerapath_disk *oldCamPath = NULL;
    camerapath *newCamPath = NULL;

    void *offset = 0;
    memsize loopsize = 0;

    void *oldptr = NULL;
    void *newptr = NULL;
    void *oldbase = NULL;
    void *newbase = NULL;

    int oldLength;
    int newLength;

    oldLength = fileLoadAlloc(fileName, (void **)&oldbase, NonVolatile);
    //oldbase = fileloadalloc(oldFile, &oldLength, 0);

    oldHeader = oldptr = oldbase;

    newLength = 2 * oldLength;

    newbase = memAlloc(2* oldLength, "nis 64bit conversion", 0);
    //newbase = malloc(2 * oldLength);
    newHeader = newbase;
    newptr = (void *)newbase;

    memset(newbase, 0, 2 * oldLength);

    offset = 0;

    strncpy(newHeader->identifier, oldHeader->identifier, 12);
    newHeader->version = oldHeader->version;
    newHeader->flags = oldHeader->flags;
    newHeader->stringBlockLength = oldHeader->stringBlockLength;
    newHeader->length = oldHeader->length;
    newHeader->loop = oldHeader->loop;
    newHeader->nObjectPaths = oldHeader->nObjectPaths;
    newHeader->nCameraPaths = oldHeader->nCameraPaths;
    newHeader->nLightPaths = oldHeader->nLightPaths;
    newHeader->nEvents = oldHeader->nEvents;

    offset += sizeof(nisheader);
    newptr += sizeof(nisheader);

    newHeader->objectPath = offset;

    oldObjPath = (spaceobjpath_disk *)((void *)oldHeader + sizeof(nisheader_disk));
    newObjPath = (spaceobjpath *)newptr;

    offset += sizeof(spaceobjpath) * newHeader->nObjectPaths;

    newHeader->cameraPath = offset;

    offset += sizeof(camerapath) * newHeader->nCameraPaths;
    offset += sizeof(lightpath) * newHeader->nLightPaths;
    offset += sizeof(nisevent) * newHeader->nEvents;

    newptr = (void *)((memsize)newbase + offset);

    //printf("offest of objects: %x -- %d:%d:%d:%d\n", offset, newHeader->nObjectPaths, newHeader->nCameraPaths,
    //        newHeader->nLightPaths, newHeader->nEvents);

    loopcount = newHeader->nObjectPaths;
    for (loopvar = 0; loopvar < loopcount; loopvar++) {
        newObjPath[loopvar].instance = (sdword)offset;
        newObjPath[loopvar].type = oldObjPath[loopvar].type;
        newObjPath[loopvar].parentIndex = oldObjPath[loopvar].parentIndex;
        newObjPath[loopvar].race = oldObjPath[loopvar].race;
        newObjPath[loopvar].nSamples = oldObjPath[loopvar].nSamples;
        memcpy(newptr, oldptr + oldObjPath[loopvar].instance, 4 * oldObjPath[loopvar].nSamples);
        offset += 4 * newObjPath[loopvar].nSamples;
        newptr += 4 * newObjPath[loopvar].nSamples;
        newObjPath[loopvar].timeOffset = oldObjPath[loopvar].timeOffset;

        newObjPath[loopvar].times = offset;
        memcpy(newptr, oldptr + oldObjPath[loopvar].times, 4 * oldObjPath[loopvar].nSamples);
        offset += 4 * newObjPath[loopvar].nSamples;
        newptr += 4 * newObjPath[loopvar].nSamples;

        newObjPath[loopvar].parameters = offset;
        memcpy(newptr, oldptr + oldObjPath[loopvar].parameters, sizeof(tcb) * oldObjPath[loopvar].nSamples);
        offset += sizeof(tcb) * newObjPath[loopvar].nSamples;
        newptr += sizeof(tcb) * newObjPath[loopvar].nSamples;

        for (j = 0; j < 6; j++) {
            newObjPath[loopvar].curve[j] = offset;
            memcpy(newptr, oldptr + oldObjPath[loopvar].curve[j], sizeof(real32) * oldObjPath[loopvar].nSamples);
            offset += sizeof(real32) * newObjPath[loopvar].nSamples;
            newptr += sizeof(real32) * newObjPath[loopvar].nSamples;
        }
    }

    //    newHeader->cameraPath = offset;

    oldCamPath = (camerapath_disk *)&oldObjPath[loopvar]; // seqential objects!
    newCamPath = (camerapath *)&newObjPath[loopvar];      // seqential objects!

    loopcount = newHeader->nCameraPaths;
    for (loopvar = 0; loopvar < loopcount; loopvar++) {
        newCamPath[loopvar].oLength = (udword)offset;
        memcpy(newptr, oldptr + oldCamPath[loopvar].oLength, 4 * oldCamPath[loopvar].nSamples);
        offset += 4 * newCamPath[loopvar].nSamples;
        newptr += 4 * newCamPath[loopvar].nSamples;

        newCamPath[loopvar].nSamples = oldCamPath[loopvar].nSamples;

        memcpy(newptr, oldptr + oldCamPath[loopvar].oLength, 4 * oldCamPath[loopvar].nSamples);
        offset += 4 * newCamPath[loopvar].nSamples;
        newptr += 4 * newCamPath[loopvar].nSamples;

        newCamPath[loopvar].times = offset;
        memcpy(newptr, oldptr + oldCamPath[loopvar].times, 4 * oldCamPath[loopvar].nSamples);
        offset += 4 * newCamPath[loopvar].nSamples;
        newptr += 4 * newCamPath[loopvar].nSamples;

        newCamPath[loopvar].parameters = offset;
        memcpy(newptr, oldptr + oldCamPath[loopvar].parameters, sizeof(tcb) * oldCamPath[loopvar].nSamples);
        offset += sizeof(tcb) * newCamPath[loopvar].nSamples;
        newptr += sizeof(tcb) * newCamPath[loopvar].nSamples;

        for (j = 0; j < 6; j++) {
            newCamPath[loopvar].curve[j] = offset;
            memcpy(newptr, oldptr + oldCamPath[loopvar].curve[j], sizeof(real32) * oldCamPath[loopvar].nSamples);
            offset += sizeof(real32) * newCamPath[loopvar].nSamples;
            newptr += sizeof(real32) * newCamPath[loopvar].nSamples;
        }
    }

    memcpy(newptr, oldptr + (udword)oldHeader->stringBlock, oldHeader->stringBlockLength);
    newHeader->stringBlock = offset;

    offset += oldHeader->stringBlockLength;
    
    memFree(oldbase);

    *loadAddress = newbase;
}
#endif


/*-----------------------------------------------------------------------------
    Name        : nisLoad
    Description : Load in an NIS from file
    Inputs      : fileName - name of NIS file.
                  scriptName - name of file to read events from.
    Outputs     : Allocates memory for and returns the loaded NIS
    Return      :                           ^^^^^^^^^^^^^^^^^^^^
----------------------------------------------------------------------------*/
nisheader *nisLoad(char *fileName, char *scriptName)
{
    nisheader header, *newHeader;
    filehandle f;
    sdword index, j;
    spaceobjpath *objPath;
    camerapath *camPath;
    ShipType type = ShipType_Uninitialized;
    char *instancePtr;
    char string[256];
    char effectPath[256];
    char *pString;
    char localisedPath[256];
    sdword instanceID;
    ubyte* loadAddress;

#ifdef HW_PTR_64
    nisLoadAndConvertTo64Bit(fileName, (void **)&loadAddress);
#else
    fileLoadAlloc(fileName, (void **)&loadAddress, NonVolatile);
#endif
    
    memcpy(&header, loadAddress, sizeof(nisheader));

#if FIX_ENDIAN
	header.version           = FIX_ENDIAN_INT_32( header.version );
	header.flags             = FIX_ENDIAN_INT_32( header.flags );
	header.stringBlock       = ( char *)FIX_ENDIAN_INT_32( ( udword )header.stringBlock );
	header.stringBlockLength = FIX_ENDIAN_INT_32( header.stringBlockLength );
	header.length            = FIX_ENDIAN_FLOAT_32( header.length );
	header.loop              = FIX_ENDIAN_FLOAT_32( header.loop );
	header.nObjectPaths      = FIX_ENDIAN_INT_32( header.nObjectPaths );
	header.objectPath        = ( spaceobjpath *)FIX_ENDIAN_INT_32( ( udword )header.objectPath );
	header.nCameraPaths      = FIX_ENDIAN_INT_32( header.nCameraPaths );
	header.cameraPath        = ( camerapath *)FIX_ENDIAN_INT_32( ( udword )header.cameraPath );
	header.nLightPaths       = FIX_ENDIAN_INT_32( header.nLightPaths );
	header.lightPath         = ( lightpath *)FIX_ENDIAN_INT_32( ( udword )header.lightPath );
	header.nEvents           = FIX_ENDIAN_INT_32( header.nEvents );
	header.events            = ( nisevent *)FIX_ENDIAN_INT_32( ( udword )header.events );
	header.nGenericObjects   = FIX_ENDIAN_INT_32( header.nGenericObjects );
	header.genericObject     = ( nisgenericobject **)FIX_ENDIAN_INT_32( ( udword )header.genericObject );
	header.iLookyObject      = FIX_ENDIAN_INT_32( header.iLookyObject );
#endif

#if NIS_ERROR_CHECKING
    if (strcmp(header.identifier, NIS_Identifier) != 0)
    {
        dbgFatalf(DBG_Loc, "Invalid NIS file: %s", fileName);
    }
    if (header.version != NIS_VersionNumber)
    {
        dbgFatalf(DBG_Loc, "Invalid version number in %s.  Expected 0x%x found 0x%x", fileName, NIS_VersionNumber, header.version);
    }
#endif                                                      //allocate the new header/NIS data minus strings
    newHeader = memAlloc((memsize)header.stringBlock, "NISHeader", NonVolatile);
    memset(newHeader, 0, sizeof(nisheader));
    memcpy(newHeader, &header, sizeof(nisheader));         //make allocated copy of header
    memcpy((ubyte *)newHeader + sizeof(nisheader), loadAddress + sizeof(nisheader), //read in the main body of NIS
           (sdword)(memsize)header.stringBlock - sizeof(nisheader));

    //separately allocate and load in the string block
    newHeader->stringBlock = memAlloc(newHeader->stringBlockLength, "NISStrings", NonVolatile);
    memcpy(newHeader->stringBlock, loadAddress + (sdword)(memsize)header.stringBlock, //read in the main body of NIS
           newHeader->stringBlockLength);
    memFree(loadAddress); //done with the file

    newHeader->flags = 0;
//    newHeader->name += (udword)newHeader->stringBlock;      //fixup file name

    newHeader->objectPath = (spaceobjpath *)((memsize)newHeader + (ubyte *)newHeader->objectPath);//fixup the object motion path array

#if FIX_ENDIAN
	for (index = 0; index < newHeader->nObjectPaths; index++)
    {                                                       //for each object motion path
        objPath = &newHeader->objectPath[index];            //get pointer to this motion path
		objPath->instance = FIX_ENDIAN_INT_32( objPath->instance );
		objPath->type = FIX_ENDIAN_INT_32( objPath->type );
		objPath->parentIndex = FIX_ENDIAN_INT_32( objPath->parentIndex );
		objPath->race = FIX_ENDIAN_INT_32( objPath->race );
		objPath->nSamples = FIX_ENDIAN_INT_32( objPath->nSamples );
		objPath->timeOffset = FIX_ENDIAN_FLOAT_32( objPath->timeOffset );
		objPath->times = ( real32 *)FIX_ENDIAN_INT_32( ( udword )objPath->times );
		objPath->parameters = ( tcb *)FIX_ENDIAN_INT_32( ( udword )objPath->parameters );
		objPath->curve[0] = ( real32 *)FIX_ENDIAN_INT_32( ( udword )objPath->curve[0] );
		objPath->curve[1] = ( real32 *)FIX_ENDIAN_INT_32( ( udword )objPath->curve[1] );
		objPath->curve[2] = ( real32 *)FIX_ENDIAN_INT_32( ( udword )objPath->curve[2] );
		objPath->curve[3] = ( real32 *)FIX_ENDIAN_INT_32( ( udword )objPath->curve[3] );
		objPath->curve[4] = ( real32 *)FIX_ENDIAN_INT_32( ( udword )objPath->curve[4] );
		objPath->curve[5] = ( real32 *)FIX_ENDIAN_INT_32( ( udword )objPath->curve[5] );
	}
#endif

    //sort the object headers based upon their parentage
    // CAT (M2-R9 oracle-rung-physics): the exe's qsort 0x00671060 (CatSort.c; nisLoad 0x004d4a00 at 0x004d4ab1): ties sort as in the exe
    catQsort(newHeader->objectPath, newHeader->nObjectPaths, sizeof(spaceobjpath), nisCompareFunc);

    for (index = 0; index < newHeader->nObjectPaths; index++)
    {                                                       //for each object motion path
        objPath = &newHeader->objectPath[index];            //get pointer to this motion path
        objPath->parameters = (tcb *)((memsize)newHeader + (ubyte *)objPath->parameters);  //fixup the various arrays
        objPath->times = (real32 *)((memsize)newHeader + (ubyte *)objPath->times);
        for (j = 0; j < 6; j++)
        {
            objPath->curve[j] = (real32 *)((memsize)newHeader + (ubyte *)objPath->curve[j]);
        }

#if FIX_ENDIAN
		for( j = 0; j < objPath->nSamples; j++ )
		{
			int k;

			objPath->times[j]                 = FIX_ENDIAN_FLOAT_32( objPath->times[j] );
			objPath->parameters[j].tension    = FIX_ENDIAN_FLOAT_32( objPath->parameters[j].tension );
			objPath->parameters[j].continuity = FIX_ENDIAN_FLOAT_32( objPath->parameters[j].continuity );
			objPath->parameters[j].bias       = FIX_ENDIAN_FLOAT_32( objPath->parameters[j].bias );
			
			for( k = 0; k < 6; k++ )
			{
				objPath->curve[k][j] = FIX_ENDIAN_FLOAT_32( objPath->curve[k][j] );
			}
		}
#endif

#if NIS_NORMALIZE_ANGLES

        for (j = 3; j < 6; j++)
        {                                                   //for heading, pitch, bank
            sdword k;
            for (k = 0; k < newHeader->objectPath[index].nSamples; k++)
            {
                newHeader->objectPath[index].curve[j][k] = DEG_TO_RAD(newHeader->objectPath[index].curve[j][k]);
            }
        }

#endif
        //convert ship name to type
        strcpy(string, (char *)objPath->type + (memsize)newHeader->stringBlock);
        if ((instancePtr = strchr(string, '(')) != NULL)
        {
            sscanf(instancePtr + 1, "%d", &instanceID);
            dbgAssertOrIgnore(instanceID > 0 && instanceID < SWORD_Max);
            *instancePtr = 0;
        }
        else
        {
            instanceID = 0;
        }
        objPath->instance = instanceID;
        switch (objPath->race)
        {
            case RACE_Sect:                                     // CAT: the seven Cataclysm ship races
            case RACE_Beast:                                    //      (nisLoad 0x004d4a00 cases 0..6)
            case RACE_Kushan:
            case RACE_Taiidan:
            case RACE_P1:
            case RACE_Traders:
            case RACE_Derelict:
                if (!strcasecmp(string, "Missile"))
                {                                               //if they're creating a missile
                    type = objPath->race;
                    objPath->race = NSR_Missile;
                }
                else
                {
                    type = StrToShipType(string);
                }
                break;
            case NSR_Asteroid:
                type = StrToAsteroidType(string);
                break;
            case NSR_DustCloud:
                type = StrToDustCloudType(string);
                break;
            case NSR_GasCloud:
                type = StrToGasCloudType(string);
                break;
            case NSR_Nebula:
                type = StrToNebulaType(string);
                break;
            case NSR_Derelict:
                type = StrToDerelictType(string);
                break;
            case NSR_Effect:
#ifdef _WIN32
                sprintf(effectPath, "ETG\\%s.ebg", string);
#else
                sprintf(effectPath, "ETG/%s.ebg", string);
#endif
                etgErrorRecoverable = FALSE;
                type = (ShipType)etgEffectCodeLoad(effectPath);
                break;
            case NSR_Generic:
                type = nisGenericObjectRegister(newHeader, string);
                break;
            default:;
#if NIS_ERROR_CHECKING
                dbgFatalf(DBG_Loc, "Invalid object race: %d", objPath->race);
#endif                                                      //allocate the new header/NIS data minus strings
        }
#if NIS_ERROR_CHECKING
        if (type == ShipType_Uninitialized)
        {
            dbgFatalf(DBG_Loc, "Invalid object type '%s' of race %d", string, objPath->race);
        }
#endif                                                      //allocate the new header/NIS data minus strings
        objPath->type = type;                               //set new scanned ship type
        if (objPath->parentIndex != 0)
        {
            strcpy(string, (char *)objPath->parentIndex + (memsize)newHeader->stringBlock);
            if ((instancePtr = strchr(string, '(')) != NULL)//if there is an instance
            {
                sscanf(instancePtr + 1, "%d", &instanceID);
                *instancePtr = 0;
            }
            else
            {
                instanceID = 0;
            }
            type = StrToShipType(string);
#if NIS_ERROR_CHECKING
            if (type == ShipType_Uninitialized)
            {
                dbgFatalf(DBG_Loc, "Invalid parentIndex ship type '%s'", string);
            }
#endif                                                      //allocate the new header/NIS data minus strings
            for (j = 0; j < index; j++)
            {                                               //search for a parent
                if (newHeader->objectPath[j].type == type &&
                    newHeader->objectPath[j].instance == instanceID)
                {
                    objPath->parentIndex = j;
                    goto foundOne;
                }
            }
#if NIS_ERROR_CHECKING
            dbgFatalf(DBG_Loc, "Parent '%s' not found for object %d", string, index);
#endif
foundOne:;
        }
        else
        {
            objPath->parentIndex = -1;
        }
    }

    newHeader->cameraPath = (camerapath * )((memsize)newHeader + (ubyte *)newHeader->cameraPath);    //fixup the object motion path array
    for (index = 0; index < newHeader->nCameraPaths; index++)
    {                                                       //for each object motion path
        camPath = &newHeader->cameraPath[index];            //get pointer to this motion path

#if FIX_ENDIAN
		camPath->oLength = FIX_ENDIAN_FLOAT_32( camPath->oLength );
		camPath->nSamples = FIX_ENDIAN_INT_32( camPath->nSamples );
		camPath->timeOffset = FIX_ENDIAN_FLOAT_32( camPath->timeOffset );
		camPath->times = ( real32 *)FIX_ENDIAN_INT_32( ( udword )camPath->times );
		camPath->parameters = ( tcb *)FIX_ENDIAN_INT_32( ( udword )camPath->parameters );
#endif

        camPath->parameters = (tcb *)((memsize)newHeader + (ubyte *)camPath->parameters);  //fixup the various arrays
        camPath->times = (real32 *)((memsize)newHeader + (ubyte *)camPath->times);
        for (j = 0; j < 6; j++)
        {
#if FIX_ENDIAN
			camPath->curve[j] = ( real32 *)FIX_ENDIAN_INT_32( ( udword )camPath->curve[j] );
#endif
            camPath->curve[j] = (real32 *)((memsize)newHeader + (ubyte *)camPath->curve[j]);
        }

#if FIX_ENDIAN
		for( j = 0; j < camPath->nSamples; j++ )
		{
			int k;

			camPath->times[j]                 = FIX_ENDIAN_FLOAT_32( camPath->times[j] );
			camPath->parameters[j].tension    = FIX_ENDIAN_FLOAT_32( camPath->parameters[j].tension );
			camPath->parameters[j].continuity = FIX_ENDIAN_FLOAT_32( camPath->parameters[j].continuity );
			camPath->parameters[j].bias       = FIX_ENDIAN_FLOAT_32( camPath->parameters[j].bias );
			
			for( k = 0; k < 6; k++ )
			{
				camPath->curve[k][j] = FIX_ENDIAN_FLOAT_32( camPath->curve[k][j] );
			}
		}
#endif

#if NIS_NORMALIZE_ANGLES
        for (j = 3; j < 6; j++)
        {                                                   //for heading, pitch, bank
            sdword k;
            for (k = 0; k < newHeader->cameraPath[index].nSamples; k++)
            {
                newHeader->cameraPath[index].curve[j][k] = DEG_TO_RAD(newHeader->cameraPath[index].curve[j][k]);
            }
        }
#endif
    }
    //!!! should also do the light lists

    newHeader->iLookyObject = -1;
    //load in the script file
    if (scriptName != NULL)
    {
        strcpy(localisedPath, scriptName);
        if (strCurLanguage >= 1)
        {
            for (pString = localisedPath + strlen(localisedPath); pString > localisedPath; pString--)
            {                                               //find the end of the path
#ifdef _WIN32
                if (*pString == '\\')
#else
                if (*pString == '\\' || *pString == '/')
#endif
                {
                    strcpy(string, pString + 1);            //save the file name
                    strcpy(pString + 1, nisLanguageSubpath[strCurLanguage]);//add the language sub-path
                    strcat(pString, string);                //put the filename back on
                    break;
                }
            }
        }
        nisEventIndex = 0;                                  //create temporary event list
        nisCurrentHeader = newHeader;
        scriptSet(NULL, localisedPath, nisScriptTable);     //load in the script file
        nisCurrentHeader = NULL;
        newHeader->nEvents = nisEventIndex;                 //create final event list in correct size
        if (newHeader->nEvents != 0)
        {
            newHeader->events = nisEventList;               //copy reference to event list
            // CAT (M2-R9 oracle-rung-physics): the exe's qsort 0x00671060 (CatSort.c; nisLoad 0x004d4a00 at 0x004d4f62)
            catQsort(newHeader->events, newHeader->nEvents, sizeof(nisevent), nisEventSortCB);//qsort this list by time
            nisEventList = NULL;                            //forget about this list
        }
        else
        {
            newHeader->events = NULL;
        }
    }
    else
    {
        newHeader->nEvents = 0;
        newHeader->events = NULL;
    }
    return(newHeader);
}

/*-----------------------------------------------------------------------------
    Name        : nisDelete
    Description : Delete an NIS header and all it's data.
    Inputs      : header - header to delete
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void nisDelete(nisheader *header)
{
    sdword index;

    dbgAssertOrIgnore(header != NULL);
    if (header->stringBlock != NULL)
    {                                                       //free the string blockl
        memFree(header->stringBlock);
    }
    if (header->events)
    {                                                       //free all the events
        for (index = 0; index < header->nEvents; index++)
        {
            switch (header->events[index].code)
            {                                               //any special-case freeing code goes here
                case NEO_StartNewNIS:
                    memFree((char *)header->events[index].param[1]);//free the text strings
                    // fall through
                case NEO_TextCard:
                case NEO_MeshAnimationStart:
                case NEO_NewEnvironment:
                case NEO_SMPTEOn:
                case NEO_StaticOn:
                    memFree((char *)header->events[index].param[0]);//free the text strings
                    break;
                default:
                    break;
            }
        }
        memFree(header->events);
    }
    if (header->nGenericObjects != 0)
    {                                                       //if this NIS loaded generic objects
        for (index = 0; index < header->nGenericObjects; index++)
        {
            CloseStatDerelictInfo(header->genericObject[index]->staticInfo);//free the stat info and all meshes
            memFree(header->genericObject[index]->staticInfo);//free the statinfo memory
            memFree(header->genericObject[index]);          //free the name and stat info pointer
        }
        memFree(header->genericObject);                     //memfree the list
    }
    memFree(header);
}

/*-----------------------------------------------------------------------------
    Name        : nisShipStartMatrix
    Description : Find the starting vector for a ship in an NIS
    Inputs      : header - header to look in
                  race - race of ship
                  type - type of ship
                  instance - instance ID of that ship race/type
    Outputs     : destVector - starting position vector of ship to find or
                        0,0,0 if not found
                  destVector - starting matrix of ship to find or identity
                        if not found
    Return      : TRUE if found, FALSE otherwise
----------------------------------------------------------------------------*/
bool32 nisShipStartPosition(vector *destVector, matrix *destMatrix, nisheader *header, ShipRace race, ShipType type, sdword instance)
{
    spaceobjpath *path;
    sdword index, firstIndex = -1;
    vector rot;

    path = header->objectPath;
    for (index = 0; index < header->nObjectPaths; index++, path++)
    {                                                       //look at all object paths
        if (bitTest(path->race, NIS_CentreShip))
        {                                                   //if this ship explicitly specified as centre ship
            firstIndex = index;                             //use it over anything else
            break;
        }
        if (((instance == -1) || (path->instance == instance)) &&
            ((race == -1) || ((path->race & (~NIS_CentreShip)) == race)) &&
            ((type == -1) || (path->type == type)) )
        {                                                   //if matching object criteria
            if (firstIndex == -1)
            {
                firstIndex = index;
            }
        }
    }
    if (firstIndex != -1)
    {
        path = &header->objectPath[firstIndex];
        destVector->x = -path->curve[2][0];
        destVector->y = path->curve[0][0];
        destVector->z = path->curve[1][0];
        rot.x = path->curve[3][0];
        rot.y = path->curve[4][0];
        rot.z = path->curve[5][0];
        nisShipEulerToMatrix(destMatrix, &rot);
        return(TRUE);
    }
    //nothing found, return identity matrix and 0,0,0 vector
    destVector->x = destVector->y = destVector->z = 0.0f;
    *destMatrix = IdentityMatrix;
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : nisTextCardListDraw
    Description : Draws the current list of text cards and expires old ones.
    Inputs      :
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void nisTextCardListDraw(void)
{
    sdword index;
    fonthandle fhSave = fontCurrentGet();
    char *lineStart, *newLine;
    sdword x, y;
    nistextcard *card;
    color c;
    real32 fadeValue;
    udword multiplier;
    sdword scroll;
    real32 scrollFloat;
    sdword screenCentreX = (MAIN_WindowWidth - 640) / 2;
    sdword screenCentreY = (MAIN_WindowHeight - 480) / 2;

    //scroll the text, if applicable
    if (nisTextScrollDistance != 0.0f && nisTextCardList[0].NIS->timeElapsed < nisTextScrollEnd)
    {
        scrollFloat = nisTextScrollDistance * (nisTextCardList[0].NIS->timeElapsed - nisTextScrollStart) / (nisTextScrollEnd - nisTextScrollStart);
        scroll = (sdword)scrollFloat;
    }
    else
    {
        nisTextScrollDistance = 0.0f;
        scroll = 0;
    }
    for (index = 0, card = nisTextCardList; index < nisTextCardIndex; index++, card++)
    {
        if (card->NIS->timeElapsed - card->creationTime > card->duration)
        {                                                   //if card elapses
//            dbgMessagef("XXXX%10s", nisTextCardList[index].text);
            if (nisTextCardIndex > 1)
            {
                nisTextCardList[index] = nisTextCardList[nisTextCardIndex - 1];
            }
            nisTextCardIndex--;
            index--;
            card = &nisTextCardList[index];
            continue;
        }
        fontMakeCurrent(card->font);

        lineStart = card->text;
        newLine = strstr(lineStart, NIS_NewLine);
        x = card->x;
        y = card->y + scroll;
        //scroll the text, if applicable
/*
        if (card->scroll != 0.0f)
        {                                                   //if the text is scrolling
            y += (sdword)(card->scroll *                    //move to scrolled-to position
                (card->NIS->timeElapsed - card->creationTime) /
                card->duration);
        }
*/
        c = card->c;
        //fade the text in, if applicable
        if (card->fadeIn != 0.0f)
        {
            fadeValue = (card->NIS->timeElapsed - card->creationTime) / card->fadeIn;
            if (fadeValue >= 1.0f)
            {
                card->fadeIn = 0.0f;
                fadeValue = 1.0f;
            }
            multiplier = (udword)(fadeValue * 256.0f);
            c = colRGB(colRed(c) * multiplier / 256, colGreen(c) * multiplier / 256, colBlue(c) * multiplier / 256);
        }
        //fade the the text out, if applicable
        if (card->fadeOut != 0.0f)
        {                                                   //if there's a fadeout
            if (card->NIS->timeElapsed >= card->creationTime + card->duration - card->fadeOut)
            {                                               //if it in the fading part
                fadeValue = (card->NIS->timeElapsed - (card->creationTime + card->duration - card->fadeOut)) / card->fadeOut;
                fadeValue = max(0.0, 1.0f - fadeValue);
                multiplier = (udword)(fadeValue * 256.0f);
                c = colRGB(colRed(c) * multiplier / 256, colGreen(c) * multiplier / 256, colBlue(c) * multiplier / 256);
//                dbgMessagef("<0x%2x>", multiplier);
            }
        }
        //draw the text
        while (newLine != NULL)
        {
            fontPrintN(x, y, card->c, FALSE, lineStart, newLine - lineStart);
            y += fontHeight(" ") + 1;
            x = card->margin;
            newLine += NIS_NewLineLength;
            lineStart = newLine;
            newLine = strstr(newLine, NIS_NewLine);
        }
        fontPrint(x + screenCentreX, y + screenCentreY, c, lineStart);
    }
    fontMakeCurrent(fhSave);
}

/*-----------------------------------------------------------------------------
    Name        : nisSMPTECounterDraw
    Description : Draw the specified SMPTE counter
    Inputs      : NIS - what NIS this counter belongs to, and where it will get
                    it's time counter.
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
char *nisIStrings[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
void nisSMPTECounterDraw(nisplaying *NIS, nisSMPTE *SMPTE)
{
    real32 time;
    sdword integer;
    sdword hours, minutes, seconds, frames;
    char string[80] = "";
    char finalString[80];
    fonthandle oldFont;
    sdword x;

    time = NIS->timeElapsed - SMPTE->startTime;
    dbgAssertOrIgnore(time >= 0.0f);

    integer = (sdword)time;                                 //isolate the integer and fractional parts
    time -= (real32)integer;
    frames = (sdword)(time * NIS_FrameRate);
    dbgAssertOrIgnore(frames >= 0 && frames < (sdword)NIS_FrameRate);

    seconds = integer % 60;
    integer /= 60;
    dbgAssertOrIgnore(seconds >= 0 && seconds < 60);

    minutes = integer % 60;
    integer /= 60;
    dbgAssertOrIgnore(minutes >= 0 && minutes < 60);

    hours = integer;
    dbgAssertOrIgnore(hours >= 0 && hours < 60);

    //build the time string
    if (hours < 10)
    {
        strcat(string, "0");
        strcat(string, nisIStrings[hours]);
    }
    else
    {
        strcat(string, nisIStrings[hours / 10]);
        strcat(string, nisIStrings[hours % 10]);
    }
    strcat(string, ":");
    if (minutes < 10)
    {
        strcat(string, "0");
        strcat(string, nisIStrings[minutes]);
    }
    else
    {
        strcat(string, nisIStrings[minutes / 10]);
        strcat(string, nisIStrings[minutes % 10]);
    }
    strcat(string, ":");
    if (seconds < 10)
    {
        strcat(string, "0");
        strcat(string, nisIStrings[seconds]);
    }
    else
    {
        strcat(string, nisIStrings[seconds / 10]);
        strcat(string, nisIStrings[seconds % 10]);
    }
    strcat(string, ":");
    if (frames < 10)
    {
        strcat(string, "0");
        strcat(string, nisIStrings[frames]);
    }
    else
    {
        strcat(string, nisIStrings[frames / 10]);
        strcat(string, nisIStrings[frames % 10]);
    }
    sprintf(finalString, SMPTE->format, string);

    oldFont = fontCurrentGet();
    fontMakeCurrent(SMPTE->font);
    if (SMPTE->bRightJustified)
    {
        x = SMPTE->x - fontWidth(finalString);
    }
    else
    {
        x = SMPTE->x;
    }
    fontPrint(x, SMPTE->y, SMPTE->c, finalString);
    fontMakeCurrent(oldFont);
}

/*-----------------------------------------------------------------------------
    Name        : nisStaticDraw
    Description : Render some on-screen static
    Inputs      : snow - static parameters
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void nisStaticDraw(nisstatic *snow)
{
    sdword nLines;
    real32 y0, y1, x0, x1, width, s0, s1, t0;
    real32 top, bottom, shortest, longest;
    real32 one = primScreenToGLScaleY(1);
    real32 red, green, blue, alpha, hue, lum, sat;
    real32 minHue, maxHue, minLum, maxLum, minSat, maxSat, minAlpha, maxAlpha;

    shortest = (snow->width - snow->widthVariation) / 2.0f;
    longest = (snow->width + snow->widthVariation) / 2.0f;
    nLines = randyrandombetween(RANDOM_STATIC, snow->nLines - snow->nLinesVariation, snow->nLines + snow->nLinesVariation);
    top = snow->y - snow->yVariation;
    bottom = snow->y + snow->yVariation;
    if (snow->texture != TR_InvalidInternalHandle)
    {
        rndTextureEnable(TRUE);
        trRGBTextureMakeCurrent(snow->texture);
        if (snow->bAlpha)
        {                                                   //if it uses an alpha texture
            glEnable(GL_BLEND);
        }
        rndGLStateLog("Textured Static");
        glBegin(GL_TRIANGLES);
        while (nLines)
        {
            y0 = frandyrandombetween(RANDOM_STATIC, top, bottom);
            y1 = y0 + one;
            width = frandyrandombetween(RANDOM_STATIC, shortest, longest);
            x0 = x1 = frandyrandombetween(RANDOM_STATIC, -1.0f, 1.0f);
            x0 -= width;
            x1 += width;
            nLines--;
            s0 = frandyrandom(RANDOM_STATIC, 1.0f);
            s1 = s0 + width * 2 * MAIN_WindowWidth / NIS_NoiseTextureWidth;
            t0 = frandyrandom(RANDOM_STATIC, 1.0f);

            glTexCoord2f(s0, t0);
            glVertex2f(x0, y1);

            glTexCoord2f(s0, t0);
            glVertex2f(x0, y0);

            glTexCoord2f(s1, t0);
            glVertex2f(x1, y0);


            glTexCoord2f(s0, t0);
            glVertex2f(x0, y1);

            glTexCoord2f(s1, t0);
            glVertex2f(x1, y0);

            glTexCoord2f(s1, t0);
            glVertex2f(x1, y1);

        }
    }
    else
    {                                                       //non-textured
        minHue = max(snow->hue - snow->hueVariation, 0.0f);
        maxHue = min(snow->hue + snow->hueVariation, 1.0f);
        minSat = max(snow->sat - snow->satVariation, 0.0f);
        maxSat = min(snow->sat + snow->satVariation, 1.0f);
        minLum = max(snow->lum - snow->lumVariation, 0.0f);
        maxLum = min(snow->lum + snow->lumVariation, 1.0f);
        minAlpha = max(snow->alpha - snow->alphaVariation, 0.0f);
        maxAlpha = min(snow->alpha + snow->alphaVariation, 1.0f);
        if (snow->bAlpha)
        {                                                   //if it uses an alpha texture
            glEnable(GL_BLEND);
        }
        glBegin(GL_LINES);
        while (nLines)
        {
            hue = frandyrandombetween(RANDOM_STATIC, minHue, maxHue);
            sat = frandyrandombetween(RANDOM_STATIC, minSat, maxSat);
            lum = frandyrandombetween(RANDOM_STATIC, minLum, maxLum);
            alpha = frandyrandombetween(RANDOM_STATIC, minAlpha, maxAlpha);
            colHLSToRGB(&red, &green, &blue, hue, lum, sat);
            y0 = frandyrandombetween(RANDOM_STATIC, top, bottom);
            width = frandyrandombetween(RANDOM_STATIC, shortest, longest);
            x0 = x1 = frandyrandombetween(RANDOM_STATIC, -1.0f, 1.0f);
            x0 -= width;
            x1 += width;
            nLines--;
            glColor4f(red, green, blue, alpha);
            glVertex2f(x0, y0);
            glColor4f(red, green, blue, alpha);
            glVertex2f(x1, y0);
        }
    }
    glEnd();
    glDisable(GL_BLEND);
    rndTextureEnable(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : cat_nisMimic
    Description : CAT (M2-R6 nis-fow-camera): NIS event: the current ship, a Mimic, takes the guise of ship type
                  param[0], or drops it for sMimic (cat_nisMimic 0x004d5fa0; the exe also looks the type's static
                  info up and discards it)
----------------------------------------------------------------------------*/
void cat_nisMimic(nisplaying *NIS, nisevent *event)
{
    ShipType type = (ShipType)event->param[0];
    Ship *ship = (Ship *)NIS->objectsInMotion[event->shipID].spaceobj;

    if (type == sMimic)
    {
        cat_mimicUnmimic(ship, FALSE);
        return;
    }
    cat_mimicBecomeShipType(ship, type, 1);
}

/*=============================================================================
    HWI-TEST (M2-R6 nis-fow-camera), port diagnostic, not in the exe: /catNisParseTest <listfile> (harness mode
    nisparse). listfile names every file of the install's nis\ and movies\ directories that the NIS code reads (the
    cataclysm.big TOC: 14 .nis, their 14 .script files, 18 animatic scripts). Once the start-up is done the test
      1. loads each .nis with the .script of the same name through nisLoad (the 64-bit loader, the Cataclysm script
         table, the Cataclysm event layout) and checks the header (identifier, version 0x101), that it has objects and
         a camera, that every object's type resolved, that every event has an opcode below NEO_LastNEO with a
         dispatch function, that every object event names an object and that the object parameters of attack /
         repair resolve; it counts the events per opcode and the script keywords the exe's table does not know (the
         exe ignores them, e.g. motionCueScale); then nisDelete;
      2. loads each animatic script (a .script with no .nis) through animLoadNISScript and checks its events alike;
      3. logs "nisparsetest:" lines and "nisparsetest: done ... result=PASS|FAIL", and the engine quits.
=============================================================================*/
char catNisParseTestList[PATH_MAX] = "";

#define CAT_NIS_TEST_MAX_FILES      128
#define CAT_NIS_TEST_MAX_UNKNOWN    32

static sdword catNisTestOpcodeCount[NEO_LastNEO];
static char catNisTestUnknown[CAT_NIS_TEST_MAX_UNKNOWN][32];
static sdword catNisTestUnknownCount[CAT_NIS_TEST_MAX_UNKNOWN];
static sdword catNisTestNumUnknown = 0;

static void catNisTestNoteUnknownKeyword(char *word)
{
    sdword index;

    for (index = 0; index < catNisTestNumUnknown; index++)
    {
        if (strcmp(catNisTestUnknown[index], word) == 0)
        {
            catNisTestUnknownCount[index]++;
            return;
        }
    }
    if (catNisTestNumUnknown < CAT_NIS_TEST_MAX_UNKNOWN)
    {
        strncpy(catNisTestUnknown[catNisTestNumUnknown], word, 31);
        catNisTestUnknown[catNisTestNumUnknown][31] = 0;
        catNisTestUnknownCount[catNisTestNumUnknown] = 1;
        catNisTestNumUnknown++;
    }
}

//the first word of every script line the exe's parser hands to the table (StatScript.c parseLine: the keyword runs
//to the first white space or '='); returns the number of lines whose keyword the table has
static sdword catNisTestScanKeywords(char *scriptName, sdword *nUnknown)
{
    filehandle fh;
    char line[512], word[64];
    char *c;
    sdword length, nKnown = 0;
    scriptEntry *entry;

    *nUnknown = 0;
    fh = fileOpen(scriptName, FF_TextMode);
    while (fileLineRead(fh, line, sizeof(line)) != FR_EndOfFile)
    {
        for (c = line; *c == ' ' || *c == '\t'; c++)
        {
            ;
        }
        if (*c == 0 || *c == ';' || *c == '#' || (c[0] == '/' && c[1] == '/'))
        {
            continue;
        }
        for (length = 0; c[length] != 0 && c[length] != ' ' && c[length] != '\t' && c[length] != '=' && length < 63; length++)
        {
            word[length] = c[length];
        }
        word[length] = 0;
        for (entry = nisScriptTable; entry->name != NULL; entry++)
        {
            if (strcmp(entry->name, word) == 0)
            {
                break;
            }
        }
        if (entry->name != NULL)
        {
            nKnown++;
        }
        else if (strcmp(word, "{") != 0)
        {
            (*nUnknown)++;
            catNisTestNoteUnknownKeyword(word);
        }
    }
    fileClose(fh);
    return nKnown;
}

//checks the events of a loaded header; returns the number of faults
static sdword catNisTestCheckEvents(char *name, nisheader *header, bool32 bHasObjects, sdword *nCatEvents)
{
    sdword index, nFaults = 0;
    nisevent *event;

    *nCatEvents = 0;
    for (index = 0, event = header->events; index < header->nEvents; index++, event++)
    {
        if (event->code < 0 || event->code >= NEO_LastNEO || nisEventDispatch[event->code] == NULL)
        {
            dbgMessagef("nisparsetest: FAIL %s event %d at %.2f: opcode %d has no dispatch", name, index, event->time, event->code);
            nFaults++;
            continue;
        }
        catNisTestOpcodeCount[event->code]++;
        if (event->code == NEO_Repair || event->code == NEO_CameraClipNear || event->code == NEO_CameraClipFar ||
            event->code == NEO_OpenSlipgate || event->code == NEO_CloseSlipgate || event->code >= NEO_LightAmbient)
        {
            (*nCatEvents)++;
        }
        //an animatic has no objects: its events carry the current object of the parse, -1 (the exe's
        //nisAnimaticSpeechEventSet 0x004d1f60 stores it too) and are dispatched with no NIS
        if (bHasObjects && (event->shipID < 0 || event->shipID >= header->nObjectPaths))
        {
            dbgMessagef("nisparsetest: FAIL %s event %d at %.2f (opcode %d): object %d", name, index, event->time, event->code, event->shipID);
            nFaults++;
        }
        if ((event->code == NEO_Attack || event->code == NEO_Repair) &&
            ((sdword)event->param[0] < 0 || (sdword)event->param[0] >= header->nObjectPaths))
        {
            dbgMessagef("nisparsetest: FAIL %s event %d at %.2f (opcode %d): target %d", name, index, event->time, event->code, (sdword)event->param[0]);
            nFaults++;
        }
    }
    return nFaults;
}

void catNisRunParseSelfTest(void)
{
    FILE *list;
    char line[PATH_MAX];
    char *names[CAT_NIS_TEST_MAX_FILES];
    bool32 used[CAT_NIS_TEST_MAX_FILES];
    char scriptName[PATH_MAX];
    sdword nNames = 0, index, j, length, nKnown, nUnknown, nCatEvents, nFaults;
    sdword nNIS = 0, nNISScripts = 0, nAnimatics = 0, nObjects = 0, nEvents = 0, nCatTotal = 0, nUnknownTotal = 0;
    sdword nFailed = 0, nTypeFaults;
    nisheader *header;
    char *end;

    memset(catNisTestOpcodeCount, 0, sizeof(catNisTestOpcodeCount));
    list = fopen(catNisParseTestList, "rt");
    if (list == NULL)
    {
        dbgMessagef("nisparsetest: cannot open list file %s", catNisParseTestList);
        dbgMessagef("nisparsetest: done nis=0 scripts=0 animatics=0 result=FAIL");
        return;
    }
    while (fgets(line, sizeof(line), list) != NULL && nNames < CAT_NIS_TEST_MAX_FILES)
    {
        for (end = line + strlen(line); end > line && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' '); end--)
        {
            end[-1] = 0;
        }
        if (line[0] == 0)
        {
            continue;
        }
        names[nNames] = memStringDupe(line);
        used[nNames] = FALSE;
        nNames++;
    }
    fclose(list);
    dbgMessagef("nisparsetest: start, %d files listed in %s", nNames, catNisParseTestList);

    //1. every NIS with its script
    for (index = 0; index < nNames; index++)
    {
        length = strlen(names[index]);
        if (length < 5 || strcasecmp(names[index] + length - 4, ".nis") != 0)
        {
            continue;
        }
        used[index] = TRUE;
        strcpy(scriptName, names[index]);
        strcpy(scriptName + length - 4, ".script");
        for (j = 0; j < nNames; j++)
        {
            if (strcasecmp(names[j], scriptName) == 0)
            {
                used[j] = TRUE;
                strcpy(scriptName, names[j]);
                break;
            }
        }
        nNIS++;
        nFaults = 0;
        if (j == nNames)
        {
            dbgMessagef("nisparsetest: FAIL %s has no script", names[index]);
            nFailed++;
            continue;
        }
        nNISScripts++;
        nKnown = catNisTestScanKeywords(scriptName, &nUnknown);
        header = nisLoad(names[index], scriptName);
        if (header == NULL || strcmp(header->identifier, NIS_Identifier) != 0 || header->version != NIS_VersionNumber)
        {
            dbgMessagef("nisparsetest: FAIL %s: bad header", names[index]);
            nFailed++;
            continue;
        }
        if (header->nObjectPaths <= 0 || header->nCameraPaths <= 0)
        {
            dbgMessagef("nisparsetest: FAIL %s: %d objects, %d cameras", names[index], header->nObjectPaths, header->nCameraPaths);
            nFaults++;
        }
        for (j = 0, nTypeFaults = 0; j < header->nObjectPaths; j++)
        {
            if (header->objectPath[j].type == ShipType_Uninitialized)
            {
                dbgMessagef("nisparsetest: FAIL %s object %d (race %d, instance %d): type not resolved", names[index], j, header->objectPath[j].race, header->objectPath[j].instance);
                nTypeFaults++;
            }
        }
        nFaults += nTypeFaults;
        nFaults += catNisTestCheckEvents(names[index], header, TRUE, &nCatEvents);
        dbgMessagef("nisparsetest: nis %s script %s length=%.2f objects=%d cameras=%d events=%d cataclysmEvents=%d scriptLines=%d unknownKeywordLines=%d faults=%d",
                    names[index], scriptName, header->length, header->nObjectPaths, header->nCameraPaths,
                    header->nEvents, nCatEvents, nKnown, nUnknown, nFaults);
        nObjects += header->nObjectPaths;
        nEvents += header->nEvents;
        nCatTotal += nCatEvents;
        nUnknownTotal += nUnknown;
        if (nFaults > 0)
        {
            nFailed++;
        }
        //the team names of remainAtEnd are freed when the event plays; nothing plays here
        for (j = 0; j < header->nEvents; j++)
        {
            if (header->events[j].code == NEO_RemainAtEnd && header->events[j].param[0] != 0)
            {
                memFree((void *)header->events[j].param[0]);
                header->events[j].param[0] = 0;
            }
        }
        nisDelete(header);
    }

    //2. every other script: the animatic scripts
    for (index = 0; index < nNames; index++)
    {
        length = strlen(names[index]);
        if (used[index] || length < 8 || strcasecmp(names[index] + length - 7, ".script") != 0)
        {
            continue;
        }
        used[index] = TRUE;
        nAnimatics++;
        nKnown = catNisTestScanKeywords(names[index], &nUnknown);
        header = (nisheader *)animLoadNISScriptForTest(names[index]);
        if (header == NULL)
        {
            dbgMessagef("nisparsetest: FAIL %s: not loaded", names[index]);
            nFailed++;
            continue;
        }
        nFaults = catNisTestCheckEvents(names[index], header, FALSE, &nCatEvents);
        if (header->nEvents == 0)
        {
            dbgMessagef("nisparsetest: FAIL %s: no events", names[index]);
            nFaults++;
        }
        dbgMessagef("nisparsetest: animatic %s events=%d cataclysmEvents=%d scriptLines=%d unknownKeywordLines=%d faults=%d",
                    names[index], header->nEvents, nCatEvents, nKnown, nUnknown, nFaults);
        nEvents += header->nEvents;
        nCatTotal += nCatEvents;
        nUnknownTotal += nUnknown;
        if (nFaults > 0)
        {
            nFailed++;
        }
        if (header->events != NULL)
        {
            memFree(header->events);
        }
        memFree(header);
    }

    for (index = 0; index < NEO_LastNEO; index++)
    {
        if (catNisTestOpcodeCount[index] > 0)
        {
            dbgMessagef("nisparsetest: opcode 0x%02x events=%d", index, catNisTestOpcodeCount[index]);
        }
    }
    for (index = 0; index < catNisTestNumUnknown; index++)
    {
        dbgMessagef("nisparsetest: keyword not in the exe's table (ignored, as by the exe): '%s' x%d", catNisTestUnknown[index], catNisTestUnknownCount[index]);
    }
    for (index = 0; index < nNames; index++)
    {
        if (!used[index])
        {
            dbgMessagef("nisparsetest: listed but not an NIS file or script: %s", names[index]);
        }
        memFree(names[index]);
    }
    dbgMessagef("nisparsetest: done nis=%d nisScripts=%d animatics=%d objects=%d events=%d cataclysmEvents=%d unknownKeywordLines=%d failed=%d result=%s",
                nNIS, nNISScripts, nAnimatics, nObjects, nEvents, nCatTotal, nUnknownTotal, nFailed,
                (nFailed == 0 && nNIS > 0 && nAnimatics > 0) ? "PASS" : "FAIL");
}
