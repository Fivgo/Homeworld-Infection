/*=============================================================================
    Name    : SoundEventPrivate.h
    Purpose : Private Defines for SoundEvent*.c files

    Created 9/8/98 by salfreds
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___SOUNDEVENTPRIVATE_H
#define ___SOUNDEVENTPRIVATE_H

#include "ObjTypes.h"
#include "soundcmn.h"
#include "SoundStructs.h"
#include "SpaceObj.h"
#include "Types.h"
#include "Vector.h"


/*=============================================================================
    Private Defines:
=============================================================================*/

// CAT: the rows of Cataclysm's Volume / Range / Frequency LUTs (92 rows: 12 classes' engines, 12 ambients, 20 gun
//      shots, 20 gun moves, 4 specials, 16 explosions, 8 hits): soundEventPlay 0x005209f0 uses class + 0xc,
//      gun type + 0x18 / + 0x2c, explosion event - 0xfbc (row 0x52 past 0x100f), hit event - 0x3fac (0x56 for 0x4007)
#define AMBIENT_OFFSET		12
#define GUNSHOT_OFFSET		24
#define GUNMOVE_OFFSET		44
#define SPEC_OFFSET			64
#define EXPLOSION_OFFSET	68
#define HIT_OFFSET			84
#define CAT_EXPLOSION_LAST_ROW  0x52    // explosions from 0x1010 on
#define CAT_HIT_CAT7_ROW        0x56    // Hit_Cat7 uses the ricochet row

#ifdef _WIN32
#define SOUNDFXDIR "SoundFX\\"
#else
#define SOUNDFXDIR "SoundFX/"
#endif

// CAT: three speech queues and streams plus one music stream (speechEventInit 0x00521a40: soundstreamquery(4, ...),
//      three speech buffers, one music buffer; speechQueueUpdate 0x00521eb0 walks 3 queues of 0x238 bytes)
#define SE_NUM_ACTORS		3

#define SE_NUM_STREAMS      (SE_NUM_ACTORS + 1)

#define ACTOR_FLEETCOMMAND  0

#define MAX_CHATTERS        2

#define CARDIOD_POINTS		180

#define PI_UNDER_180  57.29577f

#define SOUND_NOTINITED             -1
#define SOUND_SHIP_EXPLODED         -2


/*=============================================================================
    Tweaks
=============================================================================*/

extern real32 SFX_VOL_FACTOR;
extern sdword SFX_MAX_ENGINES;
extern sdword SFX_MIN_CAPSHIPS;
extern sdword SFX_AMBIENT_VOLUME;
extern sdword SFX_NIS_MAX_ENGINES;
extern sdword SFX_NIS_MIN_CAPSHIPS;
extern real32 SFX_MAX_ENGINE_RANGE;
extern bool32   SFX_CAPSHIPS_ALWAYS_ON;
extern sdword SFX_MAX_AMBIENT;
extern real32 SFX_FLOAT_VELOCITY;
extern real32 SFX_NIS_FLOAT_VELOCITY;
extern real32 SFX_MIN_PERCEPTABLE_VOL;

extern sword  SFX_HYPERSPACE_VOLUME;

extern real32 SFX_DAMAGERATIO_LIGHT;
extern real32 SFX_DAMAGERATIO_MEDIUM;
extern real32 SFX_DAMAGERATIO_HEAVY;
extern bool32   SFX_DAMAGERATIO_ENABLE;

extern real32 SFX_CARDIOD_FACTOR;
extern real32 SFX_CARDIOD_MIN;

extern real32 FIGHTER_VELOCITY_LOWPITCH;
extern real32 FIGHTER_VELOCITY_HIGHPITCH;
extern real32 FIGHTER_VELOCITY_SCALE;

extern real32 CORVETTE_VELOCITY_LOWPITCH;
extern real32 CORVETTE_VELOCITY_HIGHPITCH;
extern real32 CORVETTE_VELOCITY_SCALE;

extern real32 FIGHTER_DOPPLER_SCALE;
extern sdword FIGHTER_DOPPLER_LOW;
extern sdword FIGHTER_DOPPLER_HIGH;
extern bool32   FIGHTER_DOPPLER_USEVELOCITY;

extern real32 CORVETTE_DOPPLER_SCALE;
extern sdword CORVETTE_DOPPLER_LOW;
extern sdword CORVETTE_DOPPLER_HIGH;
extern bool32   CORVETTE_DOPPLER_USEVELOCITY;

extern real32 SPEECH_VOL_FACTOR;
extern sword  SPEECH_VOL_LOW;
extern sword  SPEECH_VOL_MAX;
extern real32 SPEECH_NOISE_FACTOR;
extern real32 SPEECH_NOISE_LOW;
extern real32 SPEECH_NOISE_HIGH;
extern udword SPEECH_FILTER_LOW;
extern udword SPEECH_FILTER_HIGH;
extern real32 SPEECH_BREAK_THRESHOLD;
extern real32 SPEECH_BREAK_RATE_FACTOR;
extern udword SPEECH_BREAK_RATE_LOW;
extern udword SPEECH_BREAK_RATE_HIGH;
extern real32 SPEECH_BREAK_LENGTH_FACTOR;
extern udword SPEECH_BREAK_LENGTH_LOW;
extern udword SPEECH_BREAK_LENGTH_HIGH;
extern real32 SPEECH_CAPSHIP_CHATTER_RANGE;
extern sdword SPEECH_CAPSHIP_CHATTER_TIME;
extern real32 SPEECH_COMBAT_CHATTER_RANGE;
extern sdword SPEECH_COMBAT_CHATTER_TIME;
extern real32 SPEECH_DISOBEY_FORCEDATTACK;

extern real32 SPEECH_MIN_PERCEPTABLE_VOL;
extern real32 SPEECH_AMBIENT_LEVEL;
extern bool32   SPEECH_AMBIENT_ENABLE;

extern real32 SPEECH_STIKEDAMAGE_MULT;
extern real32 SPEECH_CAPDAMAGE_MULT;

extern real32 SPEECH_SINGLEPLAYER_RATIO;
extern real32 SPEECH_STATUS_RATIO;
extern real32 SPEECH_CHATTER_RATIO;

extern real32 MUSIC_DISTANCE_SILENT;
extern real32 MUSIC_DISTANCE_MAX;
extern real32 MUSIC_MAXGAME_VOL;
extern real32 MUSIC_MINACTIVE_VOL;
extern real32 MUSIC_MININACTIVE_VOL;
extern real32 MUSIC_SENSORS_VOL;
extern real32 MUSIC_MANAGERS_VOL;
extern real32 MUSIC_TUTORIAL_VOL;
extern real32 MUSIC_FADE_TIME;
extern real32 MUSIC_MAXBATTLE_VOL;
extern real32 MUSIC_MINBATTLE_VOL;

extern real32 RANDOM_AMBIENCE_MINFREQ;
extern sdword RANDOM_AMBIENCE_ADDRANDOM;


/*=============================================================================
    Functions
=============================================================================*/
void SEupdateShipRange(void);

sword SEcalcenginevol(sdword shipclass, real32 distance);
sword SEcalcvol(sdword shipclass, real32 distance);
bool32 SEinrange(sdword shipclass, real32 distance);
bool32 SEinrangeSqr(sdword shipclass, real32 distancesqr);
sword SEequalize(sdword objtype, real32 distance, real32 *eq);
void SEeq(sdword objtype, real32 distance, real32 *eq);
void SEstopengine(Ship *ship, bool32 stopnow);
void SEloadbank(void);
sdword SEselectactor(void);
sdword SEspeechevent(sdword stream, sdword actor, sdword event, sdword var, sword vol, sword pan, double dist, float damageratio, sdword setVariation, bool32 bookend, ShipClass shipclass);
sword SEgetAngleToCamera(Ship *ship);
sdword GetPatch(SFXLUT *lut, sdword object, sdword event);
real32 SEsilence(void);

sword getPanAngle(vector WorldVector, real32 objsize, real32 distance);	/* Returns an angle to the object from the camera: 0 ahead, 90 right, -90 left, 179/-179 behind */

void SEstopsoundhandle(sdword *shandle, real32 fadetime);

sdword speechEventInit(void);
void speechEventClose(void);
sdword speechEventCleanup(void);

sdword musicEventPlay(sdword tracknum);
sdword musicEventUpdateVolume(void);
sdword musicEventStop(sdword tracknum, real32 fadetime);


/* SFX lookup tables */
extern SFXLUT  *MiscLUT;
extern SFXLUT  *R1GunsLUT;
extern SFXLUT  *R1ShipsLUT;
extern SFXLUT  *R2GunsLUT;
extern SFXLUT  *R2ShipsLUT;
extern SFXLUT  *SpecialLUT;
/* new */
extern SFXLUT	*GunEventsLUT;
extern SFXLUT	*GunSpecEventsLUT;     // CAT: GunSpecEvents.lut (SEloadbank 0x005200c0, 0x00a6ae2c)
extern SFXLUT	*ShipCmnEventsLUT;
extern SFXLUT	*ShipEventsLUT;
extern SFXLUT	*SpecEffectEventsLUT;
extern SFXLUT	*SpecExpEventsLUT;
extern SFXLUT	*SpecHitEventsLUT;
extern SFXLUT	*UIEventsLUT;

/* new banks */
extern BANK *GunBank;
extern BANK *ShipBank;
extern BANK *SpecialEffectBank;
extern BANK *UIBank;

extern sdword  ambienthandle;

#endif
