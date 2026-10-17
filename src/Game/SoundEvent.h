/*=============================================================================
    Name    : SoundEvent.h
    Purpose : Defines for SoundEvent.c

    Created 7/24/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___SOUNDEVENT_H
#define ___SOUNDEVENT_H

#include "SoundEventDefs.h"
#include "SoundMusic.h"
#include "SoundStructs.h"
#include "SpaceObj.h"
#include "SpeechEvent.h"
#include "Types.h"

/*=============================================================================
    Switches:
=============================================================================*/

#ifdef HW_BUILD_FOR_DEBUGGING

#define SE_ERROR_CHECKING          1           //general error checking
#define SE_VERBOSE_LEVEL           1           //control specific output code
#define SE_DEBUG                   1           //switch for command line options

#else

#define SE_ERROR_CHECKING          0           //general error checking
#define SE_VERBOSE_LEVEL           0           //control specific output code
#define SE_DEBUG                   0           //switch for command line options

#endif


#define SOUND_NOTINITED             -1

#define SOUND_EVENT_DEFAULT			-1

// CAT: Cataclysm's front-end track is 0x11 (soundEventRestart 0x0051caf0, the utyLoad* functions: musicEventPlay(0x11))
#define SOUND_FRONTEND_TRACK		0x11

#define SPEECH_HYPERSPACE_DELAY		2.5f

/*=============================================================================
    Tweaks
=============================================================================*/

extern real32	SPEECH_MOSHIP_WARNING_TIME;
extern real32	SPEECH_WARNING_TIME;

extern Ship *lastshiptospeak;
extern sdword lastgrouptospeak;


/* generic functions */
void soundEventInit(sdword race);		/* Initializes audio mixer, stream and DirectSound/Waveout; CAT: takes the player race for the speech set (soundEventInit 0x0051c6e0) */
void soundEventClose(void);				/* Shuts down the mixer and frees resources, any soundevent calls will fail */
#ifndef HW_GAME_DEMO
sdword cat_speechEventLoadRaceVoice(sdword race);  /* CAT: per-race speech LUTs + voice file (0x005217f0) */
sdword cat_speechEventInitForRace(sdword race);    /* CAT: speech start-up for a race (0x00521d50) */
#endif
void soundEventReset(void);				/* Shuts down the mixer but does not free resources, soundevent calls will continue to work */
void soundEventUpdate(void);			/* Update function that does panning, volume, etc for objects in the universe */
void soundEventInitStruct(SOUNDEVENT *pseStruct);	/* initializes a SOUNDEVENT structure for use, all ships have a SOUNDEVENT structure */
void soundEventPause(bool32 bPause);		/* Shuts down or restore the mixer */
void soundEventReloadVolumes(void);
void soundEventShutdown(void);
void soundEventRestart(void);

/* sound FX functions */
sdword soundEventPlay(void *object, sdword event, Gun *gun);
#define soundEvent(a,b)					soundEventPlay(a, b, NULL)
#define soundEventMisc(a)				soundEventPlay(NULL, a, NULL)
#define soundEventStartEngine(a)		soundEventPlay(a, ShipCmn_Engine, NULL)
#define soundEventBurstFire(a,b)		soundEventPlay(a, Gun_WeaponFireLooped, b)

// CAT: the ETG callable for playSound takes one argument array: args[0] the effect, args[1] the event (soundEffect
//      0x005216b0 = soundEventPlay(args[0], args[1], NULL)); ETG.h etgcallable
memsize soundEffect(memsize *args);
sdword soundEffectType(Effect *effect, sdword event, sdword objecttype);

sdword soundEventStopSound(sdword soundhandle, sdword numblocks);
#define soundEventStop(a)				soundEventStopSound(a, 0)

void soundEventBurstStop(Ship *ship, Gun *gun);

void soundEventShipDied(Ship *deadship);
void soundEventShipRemove(Ship *ship);
void soundEventDerelictRemove(Derelict *pDerelict);

void soundEventStopSFX(real32 fadetime);


/* speech functions */
void speechEventUpdate(void);			/* Update function so that speech continues to stream */
//sdword speechEventPlay(void *object, sdword event, sdword var, sdword playernum, sdword variation);
//#define speechEventFleet(a,b,c)			speechEventPlay(NULL, a, b, c, SOUND_NOTINITED)
//#define speechEvent(a,b,c)				speechEventPlay(a, b, c, SOUND_NOTINITED, SOUND_NOTINITED)
//#define speechEventVar(a, b, c, d)		speechEventPlay(a, b, c, SOUND_NOTINITED, d)

bool32 speechEventAttack(void);
void speechEventUnderAttack(Ship *target);

sdword speechEventQueue(void *object, sdword event, sdword var, sdword variation, sdword actornum, sdword playernum, sdword linkto, real32 timeout, sword volume);
#define speechEventFleet(a,b,c)		speechEventQueue(NULL, a, b, -1, 0, c, -1, 5.0f, -1)
#define speechEventFleetSpec(a,b,c,d)	speechEventQueue(a, b, c, -1, 0, d, -1, 5.0f, -1)
#define speechEventFleetVar(a,b,c,d)	speechEventQueue(NULL, a, b, c, 0, d, -1, 5.0f, -1)
#define speechEvent(a,b,c)			speechEventQueue(a, b, c, -1, -1, -1, -1, 0.0f, -1)
#define speechEventPlay(a, b, c, d, e)	speechEventQueue(a, b, c, e, -1, e, -1, 0.0f, -1)
#define speechEventVar(a, b, c, d)	speechEventQueue(a, b, c, d, -1, -1, -1, 0.0f, -1)

sdword speechEventRemoveShip(Ship *pShip);
sdword speechEventStop(sdword handle, real32 fadetime, bool32 addstatic);
sdword speechEventStopAllSpecific(real32 fadetime, sdword speechType);
#define speechEventStopAll(ft)		speechEventStopAllSpecific(ft, SPEECH_TYPE_SINGLE_PLAYER)
sdword speechEventActorStop(udword actorMask, real32 fadetime);
// CAT: the stops take Cataclysm's actor bits (tutHideText 0x00530ef0: 0x1000; smSensorsSkip / kasfSubtitleReset
//      0x004976f0: all actors, 0x3ff800)
#define speechEventFleetIntelStop(ft)       speechEventActorStop(CAT_SPEECH_ACTOR_FLEET_INTEL, (ft))
#define speechEventFleetCommandStop(ft)     speechEventActorStop(CAT_SPEECH_ACTOR_FLEET_COMMAND, (ft))
#define ACTOR_ALL_ACTORS    CAT_SPEECH_ACTOR_MASK
sdword speechEventCleanup(void);

// CAT: Cataclysm's speech extras (SpeechEvent.c): the stealth-ship alerts, the player-name Easter eggs, the letterbox
void cat_speechAlertsReset(void);                                   // 0x00522ba0
void cat_speechAlertsNoteShip(Ship *ship, sdword playerIndex);      // 0x00522bc0
void cat_speechAlertsUpdate(void);                                  // 0x00522ce0
void cat_speechEasterEggCheck(void);                                // 0x00522dc0
void cat_soundEventLetterboxVolume(bool32 on);                      // 0x00521770
extern bool32 catSoundLetterbox;
// CAT: Cataclysm's sound events (SoundEvent.c)
struct CatBlast;
void cat_soundEventBlast(struct CatBlast *blast);                   // 0x0051fc60
void cat_soundEventAsteroidBreak(SpaceObj *asteroid);               // 0x005215d0
void cat_soundLevelReset(void);                                     // 0x0051c980
void cat_madAnimSoundTableInit(void);                               // 0x004b64c0
sdword cat_madAnimSoundEvent(ShipType shiptype, sdword anim);       // the table's reader (madAnimationUpdate 0x004b6ce0)
// CAT: port diagnostic, not in the exe: the sound self-test (/catSoundTest, harness mode sound; SpeechEvent.c)
extern bool32 catSoundSelfTest;
void catSoundRunSelfTest(void);
extern bool32 catSpeechPrinceOfSpace;


/* Redbook audio functions */
void soundEventPlayCD(uword tracknum);
void soundEventStopCD(void);

/* Streamed Music functions */
void soundEventPlayMusic(sdword tracknum);
void soundEventStopMusic(real32 fadetime);
void soundEventStopTrack(sdword tracknum, real32 fadetime);
void musicEventNextTrack(void);
void musicEventPrevTrack(void);
sdword musicEventCurrentTrack(void);

/* Setup and Control functions */
void soundEventSFXVol(real32);
void soundEventSFXMasterVol(real32);
void soundEventSpeechVol(real32);
void soundEventSpeechMasterVol(real32);
void soundEventMusicVol(real32);
void soundEventMusicMasterVol(real32);
void soundEventMusicVolNOW(real32 level);
void soundEventMusicMasterVolNOW(real32 level);
void soundEventMasterEQ(real32 *pmasterEQ);
void soundEventSetActor(sdword actornum, bool32 bOn);
void soundEventVocalSettings(bool32 bCommands, bool32 bStatus, bool32 bChatter);
void soundEventGetVolume(real32 *sfxvol, real32 *speechvol, real32 *musicvol);
void soundEventHearActor(sdword actornum);
void soundEventSetActorFlag(sdword actorflag, bool32 bOn);

#endif
