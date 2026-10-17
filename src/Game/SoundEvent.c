/*=============================================================================
    Name    : SoundEvent.c
    Purpose : Sound Event processor

    Created 7/24/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "SoundEvent.h"
#include "Salvage.h"

#include <math.h>

#include "CommandDefs.h"
#include "FastMath.h"
#include "File.h"
#include "Gun.h"
#include "mainrgn.h"
#include "Memory.h"
#include "Matrix.h"
#include "Randy.h"
#include "render.h"
#include "Select.h"
#include "Ships.h"
#include "SinglePlayer.h"
#include "SoundEventPrivate.h"
#include "soundcmn.h"
#include "soundlow.h"
#include "Switches.h"
#include "VolTweakDefs.h"
#include "Universe.h"
#include "Vector.h"
#include "Battle.h"
#include "Blast.h"
#include "Sensors.h"
#include "NIS.h"
#include "fqcodec.h"
#include "ShipSelect.h"
#include "NomadMoon.h"
#include "Crystal.h"
#include "Slipgate.h"
#include "RKO.h"
#include "GravWellGenerator.h"
#include "Mothership.h"
#include "Worker.h"
#include "RammingFrigate.h"
#include "Mimic.h"
#include "Leech.h"

#define DIRECTSOUND     1
#define SE_MAX_STRIKECRAFT  10
#define SE_MAX_CAPITALSHIPS 6
#define SE_MAX_MOSHIPS		3

#define SFX_MAX_STRIKEENGINES   5
#define SFX_MAX_CAPENGINES      3

real32	SPEECH_MOSHIP_WARNING_TIME;
real32	SPEECH_WARNING_TIME;

real32 SFX_VOL_FACTOR;
sdword SFX_MAX_ENGINES;
sdword SFX_MIN_CAPSHIPS;
sdword SFX_AMBIENT_VOLUME;
sdword SFX_NIS_MAX_ENGINES;
sdword SFX_NIS_MIN_CAPSHIPS;
real32 SFX_MAX_ENGINE_RANGE;
bool32   SFX_CAPSHIPS_ALWAYS_ON;
sdword SFX_MAX_AMBIENT;
real32 SFX_FLOAT_VELOCITY;
real32 SFX_NIS_FLOAT_VELOCITY;
real32 SFX_MIN_PERCEPTABLE_VOL;

sword  SFX_HYPERSPACE_VOLUME;

real32 SFX_DAMAGERATIO_LIGHT;
real32 SFX_DAMAGERATIO_MEDIUM;
real32 SFX_DAMAGERATIO_HEAVY;
bool32   SFX_DAMAGERATIO_ENABLE;

real32 SFX_CARDIOD_FACTOR;
real32 SFX_CARDIOD_MIN;

real32 FIGHTER_VELOCITY_LOWPITCH;
real32 FIGHTER_VELOCITY_HIGHPITCH;
real32 FIGHTER_VELOCITY_SCALE;

real32 CORVETTE_VELOCITY_LOWPITCH;
real32 CORVETTE_VELOCITY_HIGHPITCH;
real32 CORVETTE_VELOCITY_SCALE;

real32 FIGHTER_DOPPLER_SCALE;
sdword FIGHTER_DOPPLER_LOW;
sdword FIGHTER_DOPPLER_HIGH;
bool32   FIGHTER_DOPPLER_USEVELOCITY;

real32 CORVETTE_DOPPLER_SCALE;
sdword CORVETTE_DOPPLER_LOW;
sdword CORVETTE_DOPPLER_HIGH;
bool32   CORVETTE_DOPPLER_USEVELOCITY;

real32 SPEECH_VOL_FACTOR;
sword  SPEECH_VOL_LOW;
sword  SPEECH_VOL_MAX;
real32 SPEECH_NOISE_FACTOR;
real32 SPEECH_NOISE_LOW;
real32 SPEECH_NOISE_HIGH;
udword SPEECH_FILTER_LOW;
udword SPEECH_FILTER_HIGH;
real32 SPEECH_BREAK_THRESHOLD;
real32 SPEECH_BREAK_RATE_FACTOR;
udword SPEECH_BREAK_RATE_LOW;
udword SPEECH_BREAK_RATE_HIGH;
real32 SPEECH_BREAK_LENGTH_FACTOR;
udword SPEECH_BREAK_LENGTH_LOW;
udword SPEECH_BREAK_LENGTH_HIGH;
real32 SPEECH_CAPSHIP_CHATTER_RANGE;
sdword SPEECH_CAPSHIP_CHATTER_TIME;
real32 SPEECH_COMBAT_CHATTER_RANGE;
sdword SPEECH_COMBAT_CHATTER_TIME;
real32 SPEECH_DISOBEY_FORCEDATTACK;

real32 SPEECH_MIN_PERCEPTABLE_VOL;
real32 SPEECH_AMBIENT_LEVEL;
bool32   SPEECH_AMBIENT_ENABLE;

real32 SPEECH_STIKEDAMAGE_MULT;
real32 SPEECH_CAPDAMAGE_MULT;

real32 SPEECH_SINGLEPLAYER_RATIO;
real32 SPEECH_STATUS_RATIO;
real32 SPEECH_CHATTER_RATIO;

real32 MUSIC_DISTANCE_SILENT;
real32 MUSIC_DISTANCE_MAX;
real32 MUSIC_MAXGAME_VOL;
real32 MUSIC_MINACTIVE_VOL;
real32 MUSIC_MININACTIVE_VOL;
real32 MUSIC_SENSORS_VOL;
real32 MUSIC_MANAGERS_VOL;
real32 MUSIC_TUTORIAL_VOL;
real32 MUSIC_FADE_TIME;
real32 MUSIC_MAXBATTLE_VOL;
real32 MUSIC_MINBATTLE_VOL;

real32 RANDOM_AMBIENCE_MINFREQ;
sdword RANDOM_AMBIENCE_ADDRANDOM;


/*=============================================================================
    Tweaks
=============================================================================*/

scriptEntry SoundeventTweaks[] =
{
    makeEntry(SFX_VOL_FACTOR,scriptSetReal32CB),
    makeEntry(SFX_MAX_ENGINES,scriptSetSdwordCB),
    makeEntry(SFX_MIN_CAPSHIPS,scriptSetSdwordCB),
    makeEntry(SFX_AMBIENT_VOLUME,scriptSetSdwordCB),
    makeEntry(SFX_NIS_MAX_ENGINES,scriptSetSdwordCB),
    makeEntry(SFX_NIS_MIN_CAPSHIPS,scriptSetSdwordCB),
    makeEntry(SFX_MAX_ENGINE_RANGE,scriptSetReal32CB),
    makeEntry(SFX_CAPSHIPS_ALWAYS_ON,scriptSetBool),
    makeEntry(SFX_MAX_AMBIENT,scriptSetSdwordCB),
    makeEntry(SFX_FLOAT_VELOCITY,scriptSetReal32CB),
    makeEntry(SFX_NIS_FLOAT_VELOCITY,scriptSetReal32CB),
	makeEntry(SFX_MIN_PERCEPTABLE_VOL, scriptSetReal32CB),
	makeEntry(SFX_HYPERSPACE_VOLUME,scriptSetSwordCB),

    makeEntry(SFX_DAMAGERATIO_LIGHT,scriptSetReal32CB),
    makeEntry(SFX_DAMAGERATIO_MEDIUM,scriptSetReal32CB),
    makeEntry(SFX_DAMAGERATIO_HEAVY,scriptSetReal32CB),
    makeEntry(SFX_DAMAGERATIO_ENABLE,scriptSetBool),

    makeEntry(SFX_CARDIOD_FACTOR,scriptSetReal32CB),
    makeEntry(SFX_CARDIOD_MIN,scriptSetReal32CB),

    makeEntry(FIGHTER_VELOCITY_LOWPITCH,scriptSetReal32CB),
    makeEntry(FIGHTER_VELOCITY_HIGHPITCH,scriptSetReal32CB),
    makeEntry(FIGHTER_VELOCITY_SCALE,scriptSetReal32CB),

    makeEntry(CORVETTE_VELOCITY_LOWPITCH,scriptSetReal32CB),
    makeEntry(CORVETTE_VELOCITY_HIGHPITCH,scriptSetReal32CB),
    makeEntry(CORVETTE_VELOCITY_SCALE,scriptSetReal32CB),

    makeEntry(FIGHTER_DOPPLER_SCALE,scriptSetReal32CB),
    makeEntry(FIGHTER_DOPPLER_LOW,scriptSetSdwordCB),
    makeEntry(FIGHTER_DOPPLER_HIGH,scriptSetSwordCB),
    makeEntry(FIGHTER_DOPPLER_USEVELOCITY,scriptSetBool),

    makeEntry(CORVETTE_DOPPLER_SCALE,scriptSetReal32CB),
    makeEntry(CORVETTE_DOPPLER_LOW,scriptSetSwordCB),
    makeEntry(CORVETTE_DOPPLER_HIGH,scriptSetSwordCB),
    makeEntry(CORVETTE_DOPPLER_USEVELOCITY,scriptSetBool),

    makeEntry(SPEECH_VOL_FACTOR,scriptSetReal32CB),
    makeEntry(SPEECH_VOL_LOW,scriptSetSwordCB),
    makeEntry(SPEECH_VOL_MAX,scriptSetSwordCB),
    makeEntry(SPEECH_NOISE_FACTOR,scriptSetReal32CB),
    makeEntry(SPEECH_NOISE_LOW,scriptSetReal32CB),
    makeEntry(SPEECH_NOISE_HIGH,scriptSetReal32CB),
    makeEntry(SPEECH_FILTER_LOW,scriptSetUdwordCB),
    makeEntry(SPEECH_FILTER_HIGH,scriptSetUdwordCB),
    makeEntry(SPEECH_BREAK_THRESHOLD,scriptSetReal32CB),
    makeEntry(SPEECH_BREAK_RATE_FACTOR,scriptSetReal32CB),
    makeEntry(SPEECH_BREAK_RATE_LOW,scriptSetUdwordCB),
    makeEntry(SPEECH_BREAK_RATE_HIGH,scriptSetUdwordCB),
    makeEntry(SPEECH_BREAK_LENGTH_FACTOR,scriptSetReal32CB),
    makeEntry(SPEECH_BREAK_LENGTH_LOW,scriptSetUdwordCB),
    makeEntry(SPEECH_BREAK_LENGTH_HIGH,scriptSetUdwordCB),
    makeEntry(SPEECH_CAPSHIP_CHATTER_RANGE,scriptSetReal32CB),
    makeEntry(SPEECH_CAPSHIP_CHATTER_TIME,scriptSetSdwordCB),
    makeEntry(SPEECH_COMBAT_CHATTER_RANGE,scriptSetReal32CB),
    makeEntry(SPEECH_COMBAT_CHATTER_TIME,scriptSetSdwordCB),
    makeEntry(SPEECH_MOSHIP_WARNING_TIME,scriptSetReal32CB),
    makeEntry(SPEECH_WARNING_TIME,scriptSetReal32CB),
    makeEntry(SPEECH_DISOBEY_FORCEDATTACK,scriptSetReal32CB),
    makeEntry(SPEECH_MIN_PERCEPTABLE_VOL,scriptSetReal32CB),
    makeEntry(SPEECH_AMBIENT_LEVEL,scriptSetReal32CB),
    makeEntry(SPEECH_AMBIENT_ENABLE,scriptSetBool),
	makeEntry(SPEECH_STIKEDAMAGE_MULT,scriptSetReal32CB),
	makeEntry(SPEECH_CAPDAMAGE_MULT,scriptSetReal32CB),
	makeEntry(SPEECH_SINGLEPLAYER_RATIO, scriptSetReal32CB),
	makeEntry(SPEECH_STATUS_RATIO, scriptSetReal32CB),
	makeEntry(SPEECH_CHATTER_RATIO, scriptSetReal32CB),
	makeEntry(MUSIC_DISTANCE_SILENT,scriptSetReal32CB),
    makeEntry(MUSIC_DISTANCE_MAX,scriptSetReal32CB),
    makeEntry(MUSIC_MAXGAME_VOL,scriptSetReal32CB),
    makeEntry(MUSIC_MINACTIVE_VOL,scriptSetReal32CB),
    makeEntry(MUSIC_MININACTIVE_VOL,scriptSetReal32CB),
    makeEntry(MUSIC_SENSORS_VOL,scriptSetReal32CB),
    makeEntry(MUSIC_MANAGERS_VOL,scriptSetReal32CB),
    makeEntry(MUSIC_TUTORIAL_VOL,scriptSetReal32CB),
    makeEntry(MUSIC_FADE_TIME,scriptSetReal32CB),
	makeEntry(MUSIC_MAXBATTLE_VOL,scriptSetReal32CB),
	makeEntry(MUSIC_MINBATTLE_VOL,scriptSetReal32CB),

	makeEntry(RANDOM_AMBIENCE_MINFREQ,scriptSetReal32CB),
    makeEntry(RANDOM_AMBIENCE_ADDRANDOM,scriptSetSdwordCB),
    
    END_SCRIPT_ENTRY
};


/*=============================================================================
    Private Data
=============================================================================*/

sword   soundeventinited = FALSE;
ubyte   *bankLoadAddress;
sdword  ambienthandle = SOUND_DEFAULT;
bool32    soundpaused = FALSE;

/* new banks */
BANK   *GunBank;
BANK   *ShipBank;
BANK   *SpecialEffectBank;
BANK   *UIBank;

/* SFX lookup tables */
SFXLUT  *GunEventsLUT;
SFXLUT  *GunSpecEventsLUT;      // CAT: the special gun sounds (SEloadbank 0x005200c0, .data 0x00a6ae2c)
SFXLUT  *ShipCmnEventsLUT;
SFXLUT  *ShipEventsLUT;
SFXLUT  *DerelictEventsLUT;
SFXLUT  *SpecEffectEventsLUT;
SFXLUT  *SpecExpEventsLUT;
SFXLUT  *SpecHitEventsLUT;
SFXLUT  *UIEventsLUT;

TABLELUT        *VolumeLUT;
real32			*VolumeFloatLUT;
TABLELUT        *RangeLUT;
real32			*RangeFloatLUT;
FREQUENCYLUT    *FrequencyLUT;
real32		    *FreqLUT;

real32  DefaultEQ[SOUND_EQ_SIZE];
real32  MasterEQ[SOUND_EQ_SIZE] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

extern bool32 nisIsRunning;

#ifdef USING_SDL1
SDL_CD* gCD = NULL;  /* CD device used for CD playback. */
#endif

real32 cardiod[CARDIOD_POINTS];

real32 volSpeech = 1.0f;
real32 volMusic = 1.0f;
real32 volSFX = 1.0f;
real32 volNIS = 1.0f;
bool32 bVolMusicNoFade = FALSE;

bool32    bActorOn[4] = {TRUE, TRUE, TRUE, TRUE};
bool32    bCommandsOn = TRUE;
bool32    bStatusOn = TRUE;
bool32    bChatterOn = TRUE;

sdword  actorFlagsEnabled = (
                             SPEECH_ACTOR_PILOT               |
                             SPEECH_ACTOR_FLEET_COMMAND       |
                             SPEECH_ACTOR_FLEET_INTELLIGENCE  |
                             SPEECH_ACTOR_TRADERS             |
                             SPEECH_ACTOR_P2_KADESHI          |
                             SPEECH_ACTOR_ALL_ENEMY_SHIPS     |
                             SPEECH_ACTOR_AMBASSADOR          |
                             SPEECH_ACTOR_NARRATOR            |
                             SPEECH_ACTOR_DEFECTOR            |
                             SPEECH_ACTOR_GENERAL             |
                             SPEECH_ACTOR_EMPEROR             |
                             SPEECH_ACTOR_KHAR_SELIM
                            );

// used for calculating the music volume
real32 nearestShipDistance = 0.0f;
real32 musicVolFactor = 0.0f;
real32 battleMusicVolFactor = 0.0f;
bool32 nearestShipMoving = FALSE;
real32 musicInactiveFactor = 0.0f;

// used for figuring out which engines to play
Ship *strike[SE_MAX_STRIKECRAFT];
Ship *capships[SE_MAX_CAPITALSHIPS];
Ship *moships[SE_MAX_MOSHIPS];
sdword numMoships;


extern sdword  streamhandle[];
extern smemsize  speechfilehandle;
extern SENTENCELUT *SentenceLUT;

sdword SEselectsentence(sdword actor, sdword event, sdword variable, sdword setvariation, sdword **pOffsets, udword *pDuration);
void SEinitHandles(void);


/*=============================================================================
    Functions
=============================================================================*/


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
-----------------------------------------------------------------------------*/
sdword GetPatch(SFXLUT *lut, sdword object, sdword event)
{
    sdword numvars;
    sdword index;

    if (lut == NULL)
    {
        return (SOUND_ERR);
    }

    if (object > lut->numobjects)
    {
        return (SOUND_ERR);
    }

    index = (object * lut->numevents * (lut->numvariations + 1)) + ((event & SFX_Event_Mask) * (lut->numvariations + 1));
    numvars = lut->lookup[index];

    if (numvars <= 1)
    {
        return (index + 1);
    }

    return (index + 1 + (ranRandom(RANDOM_SOUND_GAME_THREAD) % numvars));
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
-----------------------------------------------------------------------------*/
void SEinitcardiod(void)
{
    sdword i;

    for (i = 0; i < CARDIOD_POINTS; i++)
    {
        cardiod[i] = (1.0f + (real32)cos((CARDIOD_POINTS - i) / PI_UNDER_180)) * SFX_CARDIOD_FACTOR;
        if (cardiod[i] < SFX_CARDIOD_MIN)
        {
            cardiod[i] = SFX_CARDIOD_MIN;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : SEupdateNISvol
    Description : updates the NIS master volume scalar to the maximum of
				  SFX, Speech or Music volumes.
    Inputs      : none
    Outputs     : none
    Return      : void
-----------------------------------------------------------------------------*/
void SEupdateNISvol(void)
{
	volNIS = 1.0f;
#if 0
	if (volSpeech > volMusic)
	{
		if (volSpeech > volSFX)
		{
			volNIS = volSpeech;
		}
		else
		{
			volNIS = volSFX;
		}
	}
	else if (volMusic > volSFX)
	{
        volNIS = volMusic;
	}
	else
	{
		volNIS = volSFX;
	}

	if (volNIS < 0.5f)
	{
		volNIS = 0.5f;
	}
#endif
}

/*-----------------------------------------------------------------------------
    Name        : SEsetVol
    Description : ensures that the volume scalar is between 0.0 and 1.0
    Inputs      : level: this is the level to attempt to set the volume to
    Outputs     :
    Return      : a volume between 0.0 and 1.0
-----------------------------------------------------------------------------*/
real32 SEsetVol(real32 level)
{
	real32 vol = 0.0;

    if ((level >= 0.0f) && (level <= 1.0f))
    {
        vol = level;
    }
	else if (level < 0.0f)
	{
		vol = 0.0f;
	}
	else if (level > 1.0f)
	{
		vol = 1.0f;
	}

	return (vol);
}

/*-----------------------------------------------------------------------------
    Name        : soundEventGetVolume
    Description : returns the current volume settings
    Inputs      : 3 pointers to hold the current volume settings
    Outputs     : sfxvol: current volume for sound effects
				  speechvol: current volume for the speech
				  musicvol: current volume for the music
    Return      : void
-----------------------------------------------------------------------------*/
void soundEventGetVolume(real32 *sfxvol, real32 *speechvol, real32 *musicvol)
{
    *sfxvol = volSFX;
    *speechvol = volSpeech;
    *musicvol = volMusic;
}


/*-----------------------------------------------------------------------------
    Name        : soundEventSFXVol
    Description : sets the current sound effects volume, this function should
				  only be called in the NIS or animatic code since it scales
				  the input level by the NIS volume scalar.
    Inputs      : level: volume to attempt to set
    Outputs     : none
    Return      : void
-----------------------------------------------------------------------------*/
void soundEventSFXVol(real32 level)
{
    volSFX = SEsetVol(level) * volNIS;
}

/*-----------------------------------------------------------------------------
    Name        : soundEventSFXMasterVol
    Description : sets the current sound effects master volume, also resets the
				  NIS volume scalar.  This should be called by the front end.
    Inputs      : level: volume to attempt to set
    Outputs     : none
    Return      : void
-----------------------------------------------------------------------------*/
void soundEventSFXMasterVol(real32 level)
{
    volSFX = SEsetVol(level);
	SEupdateNISvol();
}

/*-----------------------------------------------------------------------------
    Name        : soundEventSpeechVol
    Description : sets the current speech volume, this function should
				  only be called in the NIS or animatic code since it scales
				  the input level by the NIS volume scalar.
    Inputs      : level: volume to attempt to set
    Outputs     : none
    Return      : void
-----------------------------------------------------------------------------*/
void soundEventSpeechVol(real32 level)
{
	volSpeech = SEsetVol(level) * volNIS;
}

/*-----------------------------------------------------------------------------
    Name        : soundEventSpeechMasterVol
    Description : sets the current speech master volume, also resets the
				  NIS volume scalar.  This should be called by the front end.
    Inputs      : level: volume to attempt to set
    Outputs     : none
    Return      : void
-----------------------------------------------------------------------------*/
void soundEventSpeechMasterVol(real32 level)
{
    volSpeech = SEsetVol(level);
	SEupdateNISvol();
}

/*-----------------------------------------------------------------------------
    Name        : soundEventMusicVol
    Description : sets the current music volume, this function should
				  only be called in the NIS or animatic code since it scales
				  the input level by the NIS volume scalar.
    Inputs      : level: volume to attempt to set
    Outputs     : none
    Return      : void
-----------------------------------------------------------------------------*/
void soundEventMusicVol(real32 level)
{
	volMusic = SEsetVol(level * volNIS);
}

/*-----------------------------------------------------------------------------
    Name        : soundEventMusicMasterVol
    Description : sets the current music master volume, also resets the
				  NIS volume scalar.  This should be called by the front end.
    Inputs      : level: volume to attempt to set
    Outputs     : none
    Return      : void
-----------------------------------------------------------------------------*/
void soundEventMusicMasterVol(real32 level)
{
	volMusic = SEsetVol(level);
	SEupdateNISvol();
}

/*-----------------------------------------------------------------------------
    Name        : soundEventMusicVolNOW
    Description : sets the current music volume and ignores the volume
				  fading code so the volume change happens immediatly.
				  This function should only be called in the NIS or animatic code
				  since it scales the input level by the NIS volume scalar.
    Inputs      : level: volume to attempt to set
    Outputs     : none
    Return      : void
-----------------------------------------------------------------------------*/
void soundEventMusicVolNOW(real32 level)
{
	soundEventMusicVol(level);
	bVolMusicNoFade = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : soundEventMusicMasterVolNOW
    Description : sets the current music master volume, also resets the NIS volume
				  scalar.  This ignores the volume fading code and sets the volume
				  immediatley. This should be called by the front end.
    Inputs      : level: volume to attempt to set
    Outputs     : none
    Return      : void
-----------------------------------------------------------------------------*/
void soundEventMusicMasterVolNOW(real32 level)
{
	volMusic = SEsetVol(level);
	bVolMusicNoFade = TRUE;
	SEupdateNISvol();
}

/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
-----------------------------------------------------------------------------*/
void soundEventMasterEQ(real32 *pmasterEQ)
{
    sdword i;

    for (i = 0; i < SOUND_EQ_SIZE; i++)
    {
        MasterEQ[i] = *pmasterEQ++;
    }
}

/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
-----------------------------------------------------------------------------*/
void soundEventSetActor(sdword actornum, bool32 bOn)
{
    if ((actornum > 0) && (actornum < 4))
    {
        bActorOn[actornum] = bOn;
    }
}

/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
-----------------------------------------------------------------------------*/
void soundEventVocalSettings(bool32 bCommands, bool32 bStatus, bool32 bChatter)
{
    bCommandsOn = bCommands;
    bStatusOn = bStatus;
    bChatterOn = bChatter;
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
-----------------------------------------------------------------------------*/
void soundEventHearActor(sdword actornum)
{
    // CAT: Cataclysm has one voice per race LUT (numactors 1) and three speech streams, and the exe has no pilot
    //      voice preview: HW1's preview of pilot voice 1..3 (Options.c's voice buttons) would read past the LUT
    //      and the stream handles, so it plays nothing. CAT-TODO(M2-R6 front end: Options.c): the exe's options
    //      have no voice buttons; drop the callers with the options port.
    (void)actornum;
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
-----------------------------------------------------------------------------*/
void soundEventSetActorFlag(sdword actorflag, bool32 bOn)
{
    if (bOn)
    {
        bitSet(actorFlagsEnabled, actorflag);
    }
    else
    {
        bitClear(actorFlagsEnabled, actorflag);
    }
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
-----------------------------------------------------------------------------*/
void SEprecalcVolTables(void)
{
    sdword i, j, index;

#if FIX_ENDIAN	
        for (j = 0; j< (RangeLUT->rows * RangeLUT->columns); j++ )
            RangeLUT->lookup[j] = FIX_ENDIAN_INT_32( RangeLUT->lookup[j] );
        for (j = 0; j< (VolumeLUT->rows * VolumeLUT->columns); j++ )
            VolumeLUT->lookup[j] = FIX_ENDIAN_INT_32( VolumeLUT->lookup[j] );
        for (j=0; j < (FrequencyLUT->columns * FrequencyLUT->rows); j++ )
            FrequencyLUT->lookup[j] = FIX_ENDIAN_FLOAT_32( FrequencyLUT->lookup[j] );
#endif

	/* do some precalculations on the volume and range tables, saves a divide, compare, 2 [] and 2 subtractions for every volume update */
	for (j = 0; j < (VolumeLUT->rows * VolumeLUT->columns); j += VolumeLUT->columns)
	{
		RangeFloatLUT[j] = 1.0f / (real32)RangeLUT->lookup[j];
		VolumeFloatLUT[j] = (real32)(SOUND_VOL_MAX - VolumeLUT->lookup[j]);

		for (i = 1; i < RangeLUT->columns; i++)
		{
			VolumeFloatLUT[j+i] = (real32)(VolumeLUT->lookup[j+i-1] - VolumeLUT->lookup[j+i]);
			RangeFloatLUT[j+i] = 1.0f / (real32)(RangeLUT->lookup[j+i] - RangeLUT->lookup[j+i-1]);
		}
	}
	
        /* do some precalcs on the Frequency table, saves a compare, [], addition and 2 subtractions for every SEequalize */
	for (j = 0; j < FrequencyLUT->columns; j++)
	{
		FreqLUT[j] = DefaultEQ[j] - FrequencyLUT->lookup[j];

		for (i = 1; i < FrequencyLUT->rows; i++)
		{
			index = i * FrequencyLUT->columns + j;
			FreqLUT[index] = FrequencyLUT->lookup[(i-1) * FrequencyLUT->columns + j] - FrequencyLUT->lookup[index];
		}
	}
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
-----------------------------------------------------------------------------*/
void soundEventInit(sdword race)  // CAT: race selects the speech set (soundEventInit 0x0051c6e0)
{
#if SOUND

#if DIRECTSOUND
    char loadfile[100];
	sdword i, size;

    if (!enableSFX && !enableSpeech)
    {
        return;
    }

    if (soundinit(useWaveout) == SOUND_ERR)
    {
        enableSFX = FALSE;
        enableSpeech = FALSE;
    }

    for (i = 0; i < SOUND_EQ_SIZE; i++)
    {
        DefaultEQ[i] = 1.0f;
    }

    // used for calculating music volume in the game
    musicVolFactor = (MUSIC_MAXGAME_VOL - MUSIC_MINACTIVE_VOL) / (MUSIC_DISTANCE_MAX - MUSIC_DISTANCE_SILENT);
    musicInactiveFactor = (MUSIC_MAXGAME_VOL - MUSIC_MININACTIVE_VOL) / (MUSIC_DISTANCE_MAX - MUSIC_DISTANCE_SILENT);
	battleMusicVolFactor = (MUSIC_MAXBATTLE_VOL - MUSIC_MINBATTLE_VOL) / (MUSIC_DISTANCE_MAX - MUSIC_DISTANCE_SILENT);

    /* LOAD volume curves */
    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "Volume.lut");
    size = fileLoadAlloc(loadfile, (void**)&VolumeLUT, NonVolatile);
    VolumeFloatLUT = memAlloc(size, "Volume Table", NonVolatile);

#if FIX_ENDIAN
    VolumeLUT->ID      = FIX_ENDIAN_INT_32( VolumeLUT->ID );
    VolumeLUT->columns = FIX_ENDIAN_INT_16( VolumeLUT->columns );
    VolumeLUT->rows    = FIX_ENDIAN_INT_16( VolumeLUT->rows );
#endif

    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "Range.lut");
    size = fileLoadAlloc(loadfile, (void**)&RangeLUT, NonVolatile);
	RangeFloatLUT = memAlloc(size, "Range Table", NonVolatile);

#if FIX_ENDIAN
    RangeLUT->ID      = FIX_ENDIAN_INT_32( RangeLUT->ID );
    RangeLUT->columns = FIX_ENDIAN_INT_16( RangeLUT->columns );
    RangeLUT->rows    = FIX_ENDIAN_INT_16( RangeLUT->rows );
#endif
    
    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "Frequency.lut");
    size = fileLoadAlloc(loadfile, (void**)&FrequencyLUT, NonVolatile);
	FreqLUT = memAlloc(size, "Frequency Table", NonVolatile);

#if FIX_ENDIAN
    FrequencyLUT->ID      = FIX_ENDIAN_INT_32( FrequencyLUT->ID );
    FrequencyLUT->columns = FIX_ENDIAN_INT_16( FrequencyLUT->columns );
    FrequencyLUT->rows    = FIX_ENDIAN_INT_16( FrequencyLUT->rows );
#endif
    
    /* do some pre-calculations on the volume, range and frequency tables */
	SEprecalcVolTables();

    /* initialze the cardiod lookup table */
    SEinitcardiod();

	SEinitHandles();

    if (enableSFX)
    {
        SEloadbank();
    }

#if SPEECH
    if (enableSpeech)
    {
#ifdef HW_GAME_DEMO
        if (speechEventInit() != SOUND_OK)
#else
        // CAT: per-race tables and voice file, then the streams (cat_speechEventInitForRace 0x00521d50)
        if (cat_speechEventInitForRace(race) != SOUND_OK)
#endif
        {
            enableSpeech = FALSE;
        }
    }
#endif // SPEECH

    soundeventinited = TRUE;

    // CAT: the mesh animations' sounds (soundEventInit 0x0051c6e0 ends with cat_madAnimSoundTableInit)
    cat_madAnimSoundTableInit();
    
#endif // DIRECTSOUND
#endif // SOUND
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void soundEventClose(void)
{
#if SOUND
#if DIRECTSOUND

    if (!enableSFX && !enableSpeech)
    {
        return;
    }

    soundrestore();

    if (enableSFX || enableSpeech)
    {
        memFree(VolumeLUT);
        memFree(RangeLUT);
        memFree(FrequencyLUT);
		memFree(VolumeFloatLUT);
		memFree(RangeFloatLUT);
		memFree(FreqLUT);
	}

    if (enableSFX)
    {
        memFree(GunEventsLUT);
        memFree(GunSpecEventsLUT);      // CAT (soundEventClose 0x0051c9a0)
        memFree(ShipCmnEventsLUT);
        memFree(ShipEventsLUT);
        memFree(DerelictEventsLUT);
//        memFree(SpecEffectEventsLUT);
        memFree(SpecExpEventsLUT);
        memFree(SpecHitEventsLUT);
        memFree(UIEventsLUT);
        memFree(GunBank);
        memFree(ShipBank);
        memFree(SpecialEffectBank);
        memFree(UIBank);
    }

    if (enableSpeech)
    {
        speechEventClose();
    }
#endif
#endif
}


void soundEventShutdown(void)
{
    if (enableSFX || enableSpeech)
		soundclose();
//        sounddeactivate(TRUE);
//	soundrestore();
}


void soundEventRestart(void)
{
	if (enableSFX || enableSpeech)
	{
		soundpause(FALSE);
		soundEventPlayMusic(SOUND_FRONTEND_TRACK);
	}

//        sounddeactivate(FALSE);
//    if (soundinit(ghMainWindow, useWaveout) == SOUND_ERR)
//    {
//        enableSFX = FALSE;
//        enableSpeech = FALSE;
//    }
//	soundpause(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void soundEventReset(void)
{
    soundstopall(SOUND_FADE_STOPALL);
}


void soundEventStopSFX(real32 fadetime)
{
    Ship *ship;
    Derelict *pDerelict;
    Node *objnode = universe.RenderList.tail;
	
	if (enableSFX)
	{
		soundstopallSFX(fadetime, FALSE);
	
		while (objnode != NULL)
		{
			ship = (Ship *)listGetStructOfNode(objnode);
	
			if (ship->header.objtype == OBJ_DerelictType)
			{
				pDerelict = (Derelict *)ship;
                soundEventDerelictRemove(pDerelict);
			}
			else if (ship->header.objtype == OBJ_ShipType)
			{
				soundEventShipRemove(ship);
			}
			objnode = objnode->prev;
		}
	}
}


void soundEventPause(bool32 bPause)
{
    if (!soundeventinited)
    {
        return;
    }

    if (bPause != soundpaused)
    {
        soundpaused = bPause;
        soundpause(bPause);

        if (bPause)
        {
            /* clean up the speech queue */
            speechEventCleanup();
        }
    }
}


void soundEventReloadVolumes(void)
{
    void *tempLUT;
    char loadfile[100];

    if (!enableSFX)
    {
        return;
    }

	/* LOAD volume curves */
    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "Volume.lut");
    fileLoadAlloc(loadfile, &tempLUT, NonVolatile);
    memFree(VolumeLUT);
    VolumeLUT = (TABLELUT *)tempLUT;

    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "Range.lut");
    fileLoadAlloc(loadfile, &tempLUT, NonVolatile);
    memFree(RangeLUT);
    RangeLUT = (TABLELUT *)tempLUT;

    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "Frequency.lut");
    fileLoadAlloc(loadfile, &tempLUT, NonVolatile);
    memFree(FrequencyLUT);
    FrequencyLUT = (FREQUENCYLUT *)tempLUT;

    /* do some pre-calculations on the volume, range and frequency tables */
	SEprecalcVolTables();
}


#if 0
void soundEventUpdateNew(void)
{
    if (!soundeventinited || (objnode == NULL))
    {
        return;
    }

//  if (nisIsRunning)
//  {
//      return;
//  }

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);

        if (ship->header.objtype != OBJ_ShipType)
        {
            goto nextship;
        }

        shipclass = ship->staticinfo->shipclass;




nextship:
        objnode = objnode->prev;
    }

}
#endif


#if 1
/*-----------------------------------------------------------------------------
    Name        : soundEventUpdate
    Description : CAT: the exe's per-frame update of the object sounds (soundEventUpdate 0x0051cd10). Walks the
                  render list from the tail as HW1, with Cataclysm's rules:
                  - derelicts only keep their playing ambients up to date (they are not started here);
                  - the engine limits (5 strike craft, 3 capital ships) spare the mothership class only, and the
                    Sentinels keep their field sound; engines stop with a 5 s fade when idle;
                  - the engine loop is the upgraded one (ShipCmn event 0x404) for a ship with upgrade 9, 11 or 13;
                    every ship, not only strike craft, gets the heading filter;
                  - a special loop per type: mimics (the ambient of the type they show), the Leech's drain
                    (0x824 / 0x825), the ramming frigates (0x813 / 0x81d), the Workers (0x802 / 0x803 / 0x816 /
                    0x819), the Sentinel field (at most 3), the sMothership big gun (0x83c), the cloakers' hum
                    (0x83b), the gravwell (0x80d), the SalCap beam (0x816), the RKO (0x80b), the slipgates, the
                    crystals (0x805), the Nomad moon (0x806);
                  - the damage loop is the EMP stun (0x832) or the Beast infection (0x838, at most 3);
                  - the ambient by health (0x401 / 0x402 / 0x407 / 0x408), being built (0x809) or converting
                    (0x80a); the random ambient by health;
                  - hyperspace in and out (0x801 / 0x814) for the local player's ships in single player (at most 3);
                  - the gimbal and burst loops by the gun the burst remembers (at most 4 bursts).
                  A leech draining a ship below 20 % health raises the leech speech alert.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: the Cataclysm rows the update uses outside the class / gun ranges (SoundEventPrivate.h has the layout)
#define CAT_ROW_SENTINEL_FIELD  0x17    // the ambient row of class 11
#define CAT_ROW_SLIPGATE        0x40    // the first special row
#define CAT_ROW_GRAVWELL        0x43    // the last special row
#define CAT_ROW_NOMAD_MOON      0x52
#define CAT_ROW_DERELICT        0x5b
#define CAT_SFX_MAX_STRIKE      5
#define CAT_SFX_MAX_CAP         3
#define CAT_SFX_MAX_SENTINELS   3
#define CAT_SFX_MAX_INFECTED    3
#define CAT_SFX_MAX_HYPERSPACE  3
#define CAT_SFX_MAX_BURSTS      4

// CAT: the patch number of a LUT entry (GetPatch inlined by the exe: a random variation on RAN_SoundGameThread)
#define catShipPatch(lut, obj, ev)  ((lut)->lookup[GetPatch((lut), (obj), (ev))])

// CAT: cat_ccIsPilotingShip 0x0042c2e0 / cat_ccIsPilotingThisShip 0x0042c310 are in CameraCommand.c (M2-R5 hud: the
//      camera command's CCMODE_PILOT_SHIP with a one-ship focus)

static void catSEupdateHandle(sdword handle, sword vol, sword pan, real32 *eq)
{
    soundvolumeF(handle, vol, 0.0f);
    soundpanF(handle, pan, 0.0f);
    soundequalize(handle, eq);
}

void soundEventUpdate(void)
{
#if SOUND
    Node *objnode = universe.RenderList.tail;
    Ship *ship = NULL;
    ShipClass shipclass;
    sdword objrow;
    sword vol = 0;
    sword pan = 0;
    real32 dist     = 0.0;
    real32 velocity = 0.0;
    real32 velratio = 1.0f;
    real32 threshold, healthratio;
    sdword  numships = 0,
            numcapships = 0,
            numsentinels = 0,
            numinfected = 0,
            numhyperspace = 0,
            numbursts = 0;
    sword shipangle = 0;
    static real32 tempEQ[SOUND_EQ_SIZE];                    // .bss 0x00a6add0
    GunInfo *gunInfo;
    sdword i, row, gunIndex, event, patch;
    Gun *gun = NULL;
    bool32 ambienton;
    CommandToDo *command = NULL;
    Derelict *pDerelict = NULL;
    ShipType apparent;
    ubyte state;
    SFXLUT *lut;
    GunSoundType soundtype;

    if (!soundeventinited || soundpaused)
    {
        return;
    }

    // update the music volume
    musicEventUpdateVolume();

	if (smSensorsActive || !mrRenderMainScreen)
	{
		return;
	}

    nearestShipDistance = REALlyBig;
    nearestShipMoving = FALSE;

	while ((objnode != NULL) && enableSFX)
    {
        ship = (Ship *)listGetStructOfNode(objnode);

		// deal with derelicts first: only an ambience that plays is kept up to date
        if (ship->header.objtype == OBJ_DerelictType)
        {
            pDerelict = (Derelict *)ship;

            dist = fsqrt(pDerelict->obj.cameraDistanceSquared);
            if (dist < nearestShipDistance)
            {
                nearestShipDistance = dist;
            }

            if (!SEinrange(CAT_ROW_DERELICT, dist))
            {
                SEstopsoundhandle(&(pDerelict->ambientSoundHandle), SOUND_FADE_STOPNOW);
                SEstopsoundhandle(&(pDerelict->randomSoundHandle), SOUND_FADE_STOPNOW);
                goto nextship;
            }

            vol = SEequalize(CAT_ROW_DERELICT, dist, tempEQ);
            pan = getPanAngle(pDerelict->obj.posinfo.position, pDerelict->staticinfo->common.staticCollInfo.approxcollspheresize, dist);
            if ((pDerelict->ambientSoundHandle != SOUND_NOTINITED) && !soundover(pDerelict->ambientSoundHandle))
            {
                catSEupdateHandle(pDerelict->ambientSoundHandle, vol, pan, tempEQ);
            }
            if ((pDerelict->randomSoundHandle == SOUND_NOTINITED) || soundover(pDerelict->randomSoundHandle))
            {
                if ((pDerelict->nextRandomTime < universe.totaltimeelapsed) && (pDerelict->randomSoundHandle == SOUND_NOTINITED))
                {
                    pDerelict->nextRandomTime = RANDOM_AMBIENCE_MINFREQ + universe.totaltimeelapsed;
                }
            }
            else if (!soundover(pDerelict->randomSoundHandle))
            {
                catSEupdateHandle(pDerelict->randomSoundHandle, vol, pan, tempEQ);
            }
            else
            {
                pDerelict->randomSoundHandle = SOUND_NOTINITED;
                pDerelict->nextRandomTime = (real32)(ranRandom(RANDOM_SOUND_GAME_THREAD) % RANDOM_AMBIENCE_ADDRANDOM) * 0.01f +
                                            RANDOM_AMBIENCE_MINFREQ + universe.totaltimeelapsed;
            }
            goto nextship;
        }
        else if (ship->header.objtype != OBJ_ShipType)
        {
			// if this wasn't a Derelict or a Ship then skip it
            goto nextship;
        }

		// ok, now we're into the SHIP stuff
        shipclass = ship->staticinfo->shipclass;
        dist = fsqrt(ship->obj.cameraDistanceSquared);

		// a little hack here so that these ships have reasonable volume curves
		if (ship->shiptype == trMiningBase)
		{
			shipclass = CLASS_Mothership;
            ship->soundevent.coverage = rndComputeOverlap(ship, 0.4f);
		}
		else if (ship->shiptype == trResearchStation)
		{
			shipclass = CLASS_Carrier;
            ship->soundevent.coverage = rndComputeOverlap(ship, 0.4f);
		}
        else
        {
            // a leech draining a ship below 20 % health raises the leech alert for that ship's player
            if ((ship->shiptype == sLeech) && cat_leechIsLeeching(ship))
            {
                Ship *target = cat_leechGetSpecifics(ship)->target;

                if ((target != NULL) && (target->targetable.health / target->staticinfo->health.maxhealth < 0.2f))
                {
                    cat_speechAlertsNoteShip(ship, target->playerowner->playerIndex);
                }
            }
            if ((shipclass == CLASS_Fighter) || (shipclass == CLASS_Micro))
            {
                ship->soundevent.coverage = rndComputeOverlap(ship, 5.0f);
            }
            else if (shipclass == CLASS_Corvette)
            {
                ship->soundevent.coverage = rndComputeOverlap(ship, 2.2f);
            }
            else
            {
                ship->soundevent.coverage = rndComputeOverlap(ship, 0.4f);
            }
        }

        // have we reached the maximum number of sounds for these types of ships? (the mothership class always plays)
        if ((((numships >= CAT_SFX_MAX_STRIKE) &&
              ((shipclass == CLASS_Fighter) || (shipclass == CLASS_Corvette) || (shipclass == CLASS_Micro))) ||
             (numcapships >= CAT_SFX_MAX_CAP)) && (shipclass != CLASS_Mothership))
        {
            SEstopengine(ship, TRUE);
            SEstopsoundhandle(&(ship->soundevent.specialHandle), SOUND_FADE_STOPNOW);
            SEstopsoundhandle(&(ship->soundevent.ambientHandle), SOUND_FADE_STOPNOW);
            SEstopsoundhandle(&(ship->soundevent.damageHandle), SOUND_FADE_STOPNOW);
            SEstopsoundhandle(&(ship->soundevent.randomHandle), SOUND_FADE_STOPNOW);
            SEstopsoundhandle(&(ship->soundevent.gunHandle), SOUND_FADE_STOPNOW);
            SEstopsoundhandle(&(ship->soundevent.hyperspaceHandle), SOUND_FADE_STOPNOW);
            if ((ship->shiptype == bSentinel) || (ship->shiptype == sSentinel))
            {
                goto othersounds;   // the Sentinels keep their field sound
            }
            goto playloopingguns;	// this is only the gun firing sounds
        }

		// has this ship just exploded?
		if (ship->soundevent.engineHandle == SOUND_SHIP_EXPLODED)
		{
			soundEventShipRemove(ship);
			goto nextship;
		}

		// is this ship even in range to be heard?
		if (!SEinrange(shipclass, dist))
		{
			// then shut the engine down if its playing
			SEstopengine(ship, TRUE);
			goto othersounds;
		}

		// need to keep track of the number of ships
		if (shipclass != CLASS_Mothership)
		{
			numships++;
			if (ship->staticinfo->shipIsCapital)
			{
				numcapships++;
			}
		}

		// figure out all the variables
		pan = getPanAngle(ship->enginePosition, ship->staticinfo->common.staticCollInfo.approxcollspheresize, dist);
		vol = SEequalize(shipclass, dist, tempEQ);
		shipangle = SEgetAngleToCamera(ship);
		velocity = fsqrt(vecMagnitudeSquared(ship->obj.posinfo.velocity));
        if (nisIsRunning)
        {                                                   // in an NIS a ship moves or not
            velocity = (velocity <= 0.01f) ? 0.0f : 500.0f;
        }

		// need this for setting the music volume
		if (dist < nearestShipDistance)
		{
			nearestShipDistance = dist;
			if (velocity > SFX_FLOAT_VELOCITY)
			{
				nearestShipMoving = TRUE;
			}
		}

		// the coverage scales the strike craft's engines; they get a velocity ratio
		if ((shipclass == CLASS_Fighter) || (shipclass == CLASS_Micro))
		{
			velratio = FIGHTER_VELOCITY_LOWPITCH;
			vol = (sword)fqRint((1.0f - ship->soundevent.coverage) * (real32)vol);
            // the exe also calls cat_shipGetSpeedMultiplier here and ignores the result
			if (velratio > FIGHTER_VELOCITY_HIGHPITCH)
			{
				velratio = FIGHTER_VELOCITY_HIGHPITCH;
			}
		}
		else if (shipclass == CLASS_Corvette)
		{
			velratio = CORVETTE_VELOCITY_LOWPITCH;
			vol = (sword)fqRint((1.0f - ship->soundevent.coverage) * (real32)vol);
			if (velratio > CORVETTE_VELOCITY_HIGHPITCH)
			{
				velratio = CORVETTE_VELOCITY_HIGHPITCH;
			}
		}
		else
		{
			ship->soundevent.coverage = rndComputeOverlap(ship, 0.4f);
		}

		// need to figure out if this ship is actually doing something and not just drifting
		if (ship->soundevent.engineState != SOUND_STARTING)
		{
            threshold = 0.0f;                               // capital ships play while they move at all
			if ((shipclass == CLASS_Fighter) || (shipclass == CLASS_Corvette) || (shipclass == CLASS_Micro))
			{
				command = getShipAndItsCommand(&universe.mainCommandLayer, ship);
				if (command == NULL)
				{
                    if (!nisIsRunning)
                    {
                        goto engineoff;
                    }
                    threshold = SFX_NIS_FLOAT_VELOCITY;
				}
                else if ((command->ordertype.order == COMMAND_NULL) || (command->ordertype.order == COMMAND_MP_HYPERSPACING))
                {
                    threshold = 100.0f;
                }
                else if ((command->ordertype.order == COMMAND_MILITARY_PARADE) && (velocity <= 2.0f * SFX_FLOAT_VELOCITY))
                {
                    goto engineoff;
                }
                else
                {
                    threshold = SFX_FLOAT_VELOCITY;
                    if (nisIsRunning && (velocity <= SFX_NIS_FLOAT_VELOCITY))
                    {
                        goto engineoff;
                    }
                }
			}
            if (velocity <= threshold)
            {
                goto engineoff;
            }
		}

		// ENGINE SOUNDS
		if (vol <= SOUND_VOL_MIN)
		{
            goto engineoff;
		}
        if ((ship->soundevent.engineHandle == SOUND_NOTINITED) || soundover(ship->soundevent.engineHandle))
        {
            // start the engine sound: the upgraded engines have their own loop (ShipCmn event 4)
            event = (cat_shipHasUpgrade(ship, 0xd) + cat_shipHasUpgrade(ship, 0xb) + cat_shipHasUpgrade(ship, 9) != 0) ?
                    (ShipCmn_Flag + 4) : ShipCmn_Engine;
            {                                               // (a type past the table plays the patch GetPatch gives, as in the exe)
                if (velratio <= 1.0f)
                {
                    ship->soundevent.engineHandle = splayFPRVL(ShipBank, catShipPatch(ShipCmnEventsLUT, ship->shiptype, event),
                                                               tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_MAX, vol, TRUE, TRUE, FALSE);
                }
                else if (shipclass == CLASS_Corvette)
                {
                    ship->soundevent.engineHandle = splayFPRVL(ShipBank, catShipPatch(ShipCmnEventsLUT, ship->shiptype, event),
                                                               tempEQ, velratio, pan, SOUND_PRIORITY_MAX - 1, vol, TRUE, TRUE, FALSE);
                }
                else if ((shipclass == CLASS_Fighter) || (shipclass == CLASS_Micro))
                {
                    ship->soundevent.engineHandle = splayFPRVL(ShipBank, catShipPatch(ShipCmnEventsLUT, ship->shiptype, event),
                                                               tempEQ, velratio, pan, SOUND_PRIORITY_MAX - 2, vol, TRUE, TRUE, FALSE);
                }
            }
        }
        else
        {
            if ((ship->soundevent.engineState == SOUND_STARTING) && (velocity > SFX_FLOAT_VELOCITY))
            {
                ship->soundevent.engineState = SOUND_PLAYING;
            }

            soundvolumeF(ship->soundevent.engineHandle, vol, 0.0f);
            soundpanF(ship->soundevent.engineHandle, pan, 0.0f);
            if (velratio > 1.0f)
            {
                soundfrequency(ship->soundevent.engineHandle, velratio);
            }
            soundequalize(ship->soundevent.engineHandle, tempEQ);
        }

        // the heading filter: the strike craft's own settings, the corvettes' for every other ship
        if ((shipclass == CLASS_Fighter) || (shipclass == CLASS_Micro))
        {
            soundshipheading(ship->soundevent.engineHandle, shipangle, FIGHTER_DOPPLER_HIGH, FIGHTER_DOPPLER_LOW,
                             FIGHTER_DOPPLER_USEVELOCITY ? velratio : 1.0f, FIGHTER_DOPPLER_SCALE);
        }
        else if (shipclass == CLASS_Corvette)
        {
            soundshipheading(ship->soundevent.engineHandle, shipangle, CORVETTE_DOPPLER_HIGH, CORVETTE_DOPPLER_LOW,
                             CORVETTE_DOPPLER_USEVELOCITY ? velratio : 1.0f, CORVETTE_DOPPLER_SCALE);
        }
        else
        {
            soundshipheading(ship->soundevent.engineHandle, shipangle, CORVETTE_DOPPLER_HIGH, CORVETTE_DOPPLER_LOW, 1.0f, 1.0f);
        }
        goto othersounds;

engineoff:
        // an idle or silent engine fades out over 5 s
        if (ship->soundevent.engineHandle > SOUND_NOTINITED)
        {
            SEstopengine(ship, FALSE);
        }

othersounds:
		// now we deal with the special sounds, ambients, damage ambients and random ambients
        objrow = shipclass + AMBIENT_OFFSET;
		if (!SEinrange(objrow, dist) && !((ship->shiptype == dNomadMoon) && SEinrange(CAT_ROW_NOMAD_MOON, dist)))
		{
			SEstopsoundhandle(&(ship->soundevent.ambientHandle), SOUND_FADE_STOPNOW);
			SEstopsoundhandle(&(ship->soundevent.damageHandle), SOUND_FADE_STOPNOW);
			SEstopsoundhandle(&(ship->soundevent.randomHandle), SOUND_FADE_STOPNOW);
			SEstopsoundhandle(&(ship->soundevent.hyperspaceHandle), SOUND_FADE_STOPNOW);
			SEstopsoundhandle(&(ship->soundevent.specialHandle), SOUND_FADE_STOPNOW);
			goto playgimbles;
		}
		vol = SEequalize(objrow, dist, tempEQ);
		vol = (sword)fqRint((1.0f - ship->soundevent.coverage) * (real32)vol);
		ambienton = TRUE;

        // the special loops: each case either keeps a special sound (goto specialon) or stops it (break)
		switch (ship->shiptype)
		{
			case sMimic:
			case sMCV:
			case p1StandardCorvette:
                // a mimicking ship sounds like the ship it shows
                if (!SEinrange(objrow, dist) || (((MimicSpec *)ship->ShipSpecifics)->mode != 1))
                {
                    break;
                }
                apparent = cat_mimicGetApparentShipType(ship);
                if (apparent == ShipType_Uninitialized)
                {
                    break;
                }
                if (ship->soundevent.specialHandle == SOUND_NOTINITED)
                {
                    vol = SEequalize(objrow, dist, tempEQ);
                    ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipCmnEventsLUT, apparent, ShipCmn_Ambient),
                                                                tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
                }
                vol = SEequalize(objrow, dist, tempEQ);
                ambienton = FALSE;
                goto specialon;

			case sLeech:
                // the leech's drain (states 7 and 8)
                state = ((LeechSpec *)ship->ShipSpecifics)->state;
                if (!SEinrange(objrow, dist) || ((state != 7) && (state != 8)))
                {
                    break;
                }
                if (ship->soundevent.specialHandle == SOUND_NOTINITED)
                {
                    vol = SEequalize(shipclass, dist, tempEQ);
                    ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, (state == 7) ? 0x824 : 0x825),
                                                                tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
                }
                vol = SEequalize(shipclass, dist, tempEQ);
                ambienton = FALSE;
                goto specialon;

			case sRammingFrigate:
			case bRammingFrigate:
                // the ram: charging at an enemy (state 5 with a target to damage) or latched (3, or 5 when upgraded)
                if (!SEinrange(shipclass, dist))
                {
                    break;
                }
                state = ((RammingFrigateSpec *)ship->ShipSpecifics)->state;
                if ((state == 5) && ((RammingFrigateSpec *)ship->ShipSpecifics)->damageTarget)
                {
                    if (ship->soundevent.specialHandle == SOUND_NOTINITED)
                    {
                        vol = SEequalize(shipclass, dist, tempEQ);
                        ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, 0x813),
                                                                    tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
                    }
                    ambienton = FALSE;
                    goto specialon;
                }
                if ((state != 3) && ((state != 5) || !cat_shipHasRamUpgrade(ship)))
                {
                    break;
                }
                if (ship->soundevent.specialHandle == SOUND_NOTINITED)
                {
                    ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, 0x81d),
                                                                tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
                }
                ambienton = FALSE;
                goto specialon;

			case sWorker:
			case bWorker:
                // harvesting (rcstate1 4), unloading (5), the salvage beam, the repair beam
                if (!SEinrange(objrow, dist))
                {
                    break;
                }
                if ((ship->soundevent.specialHandle == SOUND_NOTINITED) || soundover(ship->soundevent.specialHandle))
                {
                    if (ship->rcstate1 == 4)
                    {
                        event = 0x802;
                    }
                    else if (ship->rcstate1 == 5)
                    {
                        event = 0x803;
                    }
                    else if (((WorkerSpec *)ship->ShipSpecifics)->salvager.tractorBeam)
                    {
                        event = 0x816;
                    }
                    else if (((WorkerSpec *)ship->ShipSpecifics)->repairEffectOn)
                    {
                        event = 0x819;
                    }
                    else
                    {
                        break;
                    }
                    vol = SEequalize(shipclass, dist, tempEQ);
                    ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, event),
                                                                tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
                    ambienton = FALSE;
                    goto specialon;
                }
                if ((ship->rcstate1 == 4) || (ship->rcstate1 == 5) || ((WorkerSpec *)ship->ShipSpecifics)->salvager.tractorBeam ||
                    ((WorkerSpec *)ship->ShipSpecifics)->repairEffectOn)
                {
                    vol = SEequalize(shipclass, dist, tempEQ);
                    ambienton = FALSE;
                    goto specialon;
                }
                break;

			case sSentinel:
			case bSentinel:
                // the force field, for at most 3 Sentinels
                if (!cat_shipIsSentinelFieldActive(ship))
                {
                    break;
                }
                if (numsentinels >= CAT_SFX_MAX_SENTINELS)
                {
                    ambienton = FALSE;
                    break;
                }
                if (!SEinrange(CAT_ROW_SENTINEL_FIELD, dist))
                {
                    break;                                  // the ambient stays on
                }
                ambienton = FALSE;
                if (ship->soundevent.specialHandle == SOUND_NOTINITED)
                {
                    vol = SEequalize(CAT_ROW_SENTINEL_FIELD, dist, tempEQ);
                    ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, 0x800),
                                                                tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
                }
                vol = SEequalize(CAT_ROW_SENTINEL_FIELD, dist, tempEQ);
                numsentinels++;
                goto specialon;

			case sMothership:
                // the big gun charging (state 4)
                if (!SEinrange(objrow, dist) || (((MothershipSpec *)ship->ShipSpecifics)->bigGunState != 4))
                {
                    break;
                }
                if (ship->soundevent.specialHandle == SOUND_NOTINITED)
                {
                    vol = SEequalize(objrow, dist, tempEQ);
                    ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, 0x83c),
                                                                tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
                }
                vol = SEequalize(objrow, dist, tempEQ);
                ambienton = FALSE;
                goto specialon;

			case bCloakedFighter:
			case bIonArrayFrigate:
			case p1IonArrayFrigate:
                // the local player's cloakers hum while visible: the exe tests the float at specifics +0x2C (Ship+0x664)
                // for all three, the bCloakedFighter's cloak fade (its CatCloakInfo is at +0x18); for the ion array
                // frigates that word is a detected[] flag
                if ((universe.curPlayerIndex != ship->playerowner->playerIndex) ||
                    (*(real32 *)((ubyte *)ship->ShipSpecifics + 0x2C) > 0.0f))
                {
                    break;
                }
                if (ship->soundevent.specialHandle == SOUND_NOTINITED)
                {
                    vol = SEequalize(shipclass, dist, tempEQ);
                    ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, 0x83b),
                                                                tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
                }
                vol = SEequalize(shipclass, dist, tempEQ);
                ambienton = FALSE;
                goto specialon;

			case kAdvanceSupportFrigate:
			case tAdvanceSupportFrigate:
                // the repair effect: the exe starts no loop here, it only keeps one up to date
                if (ship->rceffect == NULL)
                {
                    break;
                }
                ambienton = FALSE;
                goto specialon;

			case kGravWellGenerator:
			case tGravWellGenerator:
                if (!((GravWellGeneratorSpec *)ship->ShipSpecifics)->GravFieldOn || !SEinrange(CAT_ROW_GRAVWELL, dist))
                {
                    break;
                }
                ambienton = FALSE;
                vol = SEequalize(CAT_ROW_GRAVWELL, dist, tempEQ);
                if ((ship->soundevent.specialHandle == SOUND_NOTINITED) || soundover(ship->soundevent.specialHandle))
                {
                    ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, 0x80d),
                                                                tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_MAX, vol, FALSE, TRUE, FALSE);
                }
                goto specialon;

			case kSalCapCorvette:
			case tSalCapCorvette:
				// is the tractor beam on?
				if (!cat_salvageGetSalvagerInfo(ship)->tractorBeam || !SEinrange(shipclass, dist))
				{
                    break;
                }
                if (ship->soundevent.specialHandle == SOUND_NOTINITED)
                {
                    vol = SEequalize(shipclass, dist, tempEQ);
                    ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, 0x816),
                                                                tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
                }
                vol = SEequalize(shipclass, dist, tempEQ);
                ambienton = FALSE;
                goto specialon;

            case dRKO:
                // no ambient during the first two minutes of mission 14; the RKO's own loop in its state 0x10
                if (singlePlayerGame && (singlePlayerGameInfo.currentMission == 14) && (universe.totaltimeelapsed < 120.0f))
                {
                    ambienton = FALSE;
                }
                if (!SEinrange(shipclass, dist) || (((RKOSpec *)ship->ShipSpecifics)->state != 0x10))
                {
                    break;
                }
                if (ship->soundevent.specialHandle == SOUND_NOTINITED)
                {
                    vol = SEequalize(shipclass, dist, tempEQ);
                    ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, 0x80b),
                                                                tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
                }
                vol = SEequalize(shipclass, dist, tempEQ);
                ambienton = TRUE;                           // the RKO keeps its ambient with the loop
                goto specialon;

            case dSlipgateSmall:
            case dSlipgateMedium:
            case dSlipgateLarge:
                // one-shot sounds when the gate opens (state 1) or closes (3), its loop while not idle; the last
                // state is kept in soundevent.actorNum
                if (!SEinrange(CAT_ROW_SLIPGATE, dist))
                {
                    break;
                }
                state = ((SlipgateSpec *)ship->ShipSpecifics)->state;
                if ((udword)ship->soundevent.actorNum != (udword)state)
                {
                    if ((state == 1) || (state == 3))
                    {
                        vol = SEequalize(CAT_ROW_SLIPGATE, dist, tempEQ);
                        splayFPRVL(ShipBank, catShipPatch(ShipCmnEventsLUT, ship->shiptype, ShipCmn_Flag + ((state == 1) ? 5 : 6)),
                                   tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
                    }
                    ship->soundevent.actorNum = state;
                }
                if (state == 0)
                {
                    break;
                }
                if (ship->soundevent.specialHandle == SOUND_NOTINITED)
                {
                    vol = SEequalize(CAT_ROW_SLIPGATE, dist, tempEQ);
                    ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipCmnEventsLUT, ship->shiptype, ShipCmn_Flag + 2),
                                                                tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
                }
                vol = SEequalize(CAT_ROW_SLIPGATE, dist, tempEQ);
                ambienton = FALSE;
                goto specialon;

            case dCrystal0:
            case dCrystal1:
                // a crystal a depot dissolves
                if (!((CrystalSpec *)ship->ShipSpecifics)->processing)
                {
                    break;
                }
                if ((ship->soundevent.specialHandle == SOUND_NOTINITED) || soundover(ship->soundevent.specialHandle))
                {
                    ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, 0x805),
                                                                tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_MAX, vol, TRUE, TRUE, FALSE);
                }
                ambienton = FALSE;
                goto specialon;

            case dNomadMoon:
                // the moon's field charging (state 1), louder in an NIS
                if (!SEinrange(CAT_ROW_NOMAD_MOON, dist))
                {
                    break;
                }
                vol = SEequalize(CAT_ROW_NOMAD_MOON, dist, tempEQ);
                if (((NomadMoonSpec *)ship->ShipSpecifics)->fieldState != 1)
                {
                    break;
                }
                if (nisIsRunning)
                {
                    vol = SEequalize(CAT_ROW_NOMAD_MOON, dist * 0.25f, tempEQ);
                }
                if (ship->soundevent.specialHandle == SOUND_NOTINITED)
                {
                    ship->soundevent.specialHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, 0x806),
                                                                tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
                }
                ambienton = FALSE;
                goto specialon;

            default:
                break;
		}

        // no special sound: stop the one that plays
        SEstopsoundhandle(&(ship->soundevent.specialHandle), SOUND_FADE_STOPTIME);
        goto damagesounds;

specialon:
        if (ship->soundevent.specialHandle == SOUND_NOTINITED)
        {
            SEstopsoundhandle(&(ship->soundevent.specialHandle), SOUND_FADE_STOPTIME);
        }
        else
        {
            catSEupdateHandle(ship->soundevent.specialHandle, vol, pan, tempEQ);
        }

damagesounds:
		// DAMAGE: the EMP stun loop, or the Beast infection loop (at most 3, not on Beast ships or crystals)
        if (ship->catBeastStunState == 0)
        {
            if (ship->catEmpStunned != 1)
            {
                SEstopsoundhandle(&(ship->soundevent.damageHandle), SOUND_FADE_STOPNOW);
            }
            else if ((ship->soundevent.damageHandle == SOUND_NOTINITED) || soundover(ship->soundevent.damageHandle))
            {
                ship->soundevent.damageHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, 0x832),
                                                           tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_LOW, vol, FALSE, TRUE, FALSE);
            }
            else
            {
                catSEupdateHandle(ship->soundevent.damageHandle, vol, pan, tempEQ);
            }
        }
        else if (numinfected < CAT_SFX_MAX_INFECTED)
        {
            if (((ship->shiptype < bRecon) || (ship->shiptype > bMissileCorvette)) &&
                (ship->shiptype != dCrystal0) && (ship->shiptype != dCrystal1))
            {
                if (!cat_ccIsPilotingShip())
                {
                    vol = SEequalize(objrow, dist, tempEQ);
                }
                else if (cat_ccIsPilotingThisShip(ship))
                {
                    vol = 0x78;
                }
                if ((ship->soundevent.damageHandle == SOUND_NOTINITED) || soundover(ship->soundevent.damageHandle))
                {
                    ship->soundevent.damageHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, 0x838),
                                                               tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_LOW, vol, FALSE, TRUE, FALSE);
                }
                else
                {
                    catSEupdateHandle(ship->soundevent.damageHandle, vol, pan, tempEQ);
                }
                numinfected++;
            }
        }
        else
        {
            SEstopsoundhandle(&(ship->soundevent.damageHandle), SOUND_FADE_STOPNOW);
        }

		// AMBIENT: by health, or being built / converting; the random ambient by health
		if (ambienton)
		{
            healthratio = ship->targetable.health / ship->staticinfo->health.maxhealth;
            if (bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
            {
                event = 0x809;
            }
            else if (ship->catSalvageConverting)
            {
                event = 0x80a;
            }
            else if (healthratio > SFX_DAMAGERATIO_LIGHT)
            {
                event = ShipCmn_Ambient;
            }
            else if (healthratio > SFX_DAMAGERATIO_MEDIUM)
            {
                event = ShipCmn_Flag + 2;
            }
            else if (healthratio > SFX_DAMAGERATIO_HEAVY)
            {
                event = ShipCmn_Flag + 7;
            }
            else
            {
                event = ShipCmn_Flag + 8;
            }

            if (ship->soundevent.lastAmbient != event)
            {
                SEstopsoundhandle(&(ship->soundevent.ambientHandle), SOUND_FADE_STOPNOW);
                ship->soundevent.lastAmbient = event;
            }

			if ((ship->soundevent.ambientHandle == SOUND_NOTINITED) || soundover(ship->soundevent.ambientHandle))
			{
                lut = (event & Ship_Flag) ? ShipEventsLUT : ShipCmnEventsLUT;
				ship->soundevent.ambientHandle = splayFPRVL(ShipBank, catShipPatch(lut, (event & Ship_Flag) ? 0 : ship->shiptype, event),
														    tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_LOW, vol, FALSE, TRUE, FALSE);
			}
			else
			{
                catSEupdateHandle(ship->soundevent.ambientHandle, vol, pan, tempEQ);
			}

			// now we have the random ambience
			if (ship->soundevent.randomHandle == SOUND_NOTINITED)
			{
				if (ship->soundevent.nextRandom < universe.totaltimeelapsed)
				{
                    if (healthratio > SFX_DAMAGERATIO_MEDIUM)
                    {
                        lut = ShipCmnEventsLUT;
                        patch = catShipPatch(lut, ship->shiptype, ShipCmn_Flag + 3);
                    }
                    else if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
                    {
                        lut = ShipCmnEventsLUT;
                        patch = catShipPatch(lut, ship->shiptype, ShipCmn_Flag + 9);
                    }
                    else
                    {
                        lut = ShipEventsLUT;
                        patch = catShipPatch(lut, 0, 0x820);
                    }
					ship->soundevent.randomHandle = splayFPRVL(ShipBank, patch, tempEQ, (real32)SOUND_DEFAULT, pan,
                                                               SOUND_PRIORITY_LOW, vol, FALSE, TRUE, FALSE);
					if (ship->soundevent.randomHandle == SOUND_NOTINITED)
					{
						ship->soundevent.nextRandom = RANDOM_AMBIENCE_MINFREQ + universe.totaltimeelapsed;
					}
				}
			}
			else if (!soundover(ship->soundevent.randomHandle))
			{
                catSEupdateHandle(ship->soundevent.randomHandle, vol, pan, tempEQ);
			}
			else
			{
				// this puppy is done, get ready for the next (a ship being built sooner)
				ship->soundevent.randomHandle = SOUND_NOTINITED;
                if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
                {
                    ship->soundevent.nextRandom = (real32)(ranRandom(RANDOM_SOUND_GAME_THREAD) % RANDOM_AMBIENCE_ADDRANDOM) * 0.01f +
                                                  RANDOM_AMBIENCE_MINFREQ + universe.totaltimeelapsed;
                }
                else
                {
                    ship->soundevent.nextRandom = RANDOM_AMBIENCE_MINFREQ * 0.25f +
                                                  (real32)(ranRandom(RANDOM_SOUND_GAME_THREAD) % RANDOM_AMBIENCE_ADDRANDOM) * 0.0025f +
                                                  universe.totaltimeelapsed;
                }
			}
		}
		else
		{
			SEstopsoundhandle(&(ship->soundevent.ambientHandle), SOUND_FADE_STOPNOW);
			SEstopsoundhandle(&(ship->soundevent.randomHandle), SOUND_FADE_STOPNOW);
			SEstopsoundhandle(&(ship->soundevent.damageHandle), SOUND_FADE_STOPNOW);
            ship->soundevent.lastAmbient = SOUND_NOTINITED;
		}

		// HYPERSPACING: the local player's ships in single player (all ships otherwise), at most 3
		if ((numhyperspace < CAT_SFX_MAX_HYPERSPACE) &&
            (!singlePlayerGame || ((universe.curPlayerIndex == ship->playerowner->playerIndex) &&
                                   !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))))
		{
            udword hsState = ship->shipSinglePlayerGameInfo->hsState;

			if ((hsState == HS_SLICING_INTO) || (hsState == HS_SLICING_OUTOF))
			{
                numhyperspace++;
				if ((ship->soundevent.hyperspaceHandle == SOUND_NOTINITED) || soundover(ship->soundevent.hyperspaceHandle))
				{
					ship->soundevent.hyperspaceHandle = splayFPRVL(ShipBank, catShipPatch(ShipEventsLUT, 0, (hsState == HS_SLICING_INTO) ? 0x801 : 0x814),
										   tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_MAX, vol, TRUE, TRUE, FALSE);
				}
				else
				{
                    catSEupdateHandle(ship->soundevent.hyperspaceHandle, vol, pan, tempEQ);
				}
			}
		}

playgimbles:
		// the gun turrets (the first gun's move row)
		if (ship->gunInfo != NULL)
		{
			gunInfo = ship->gunInfo;
            row = gunInfo->guns[0].gunstatic->gunsoundtype + GUNMOVE_OFFSET;

			if (SEinrange(row, dist))
			{
				vol = (sword)fqRint((1.0f - ship->soundevent.coverage) * (real32)SEequalize(row, dist, tempEQ));
				for (i = 0; i < gunInfo->numGuns; i++)
				{
					gun = &gunInfo->guns[i];
					if ((gun->gimblehandle != SOUND_NOTINITED) && !soundover(gun->gimblehandle) && (vol > SOUND_VOL_MIN))
					{
                        catSEupdateHandle(gun->gimblehandle, vol, pan, tempEQ);
					}
				}
			}
			else
			{
				// out of range so see if we need to shut off the gimbles
				for (i = 0; i < gunInfo->numGuns; i++)
				{
					gun = &gunInfo->guns[i];
                    SEstopsoundhandle(&(gun->gimblehandle), SOUND_FADE_STOPNOW);
				}
			}
		}

playloopingguns:
		// the burst fire loop of the gun the burst remembers, at most 4
		if (ship->soundevent.burstfiring)
		{
            numbursts++;
            gunIndex = (sdword)(memsize)ship->soundevent.gun;
			if ((numbursts <= CAT_SFX_MAX_BURSTS) &&
                SEinrange(ship->gunInfo->guns[gunIndex].gunstatic->gunsoundtype + GUNSHOT_OFFSET, dist))
			{
				if (ship->aistateattack == STATE_BREAK)
				{
					soundEventBurstStop(ship, &ship->gunInfo->guns[gunIndex]);
				}
				else
				{
                    pan = getPanAngle(ship->obj.posinfo.position, ship->staticinfo->common.staticCollInfo.approxcollspheresize, dist);
					vol = (sword)fqRint((real32)SEequalize(ship->gunInfo->guns[gunIndex].gunstatic->gunsoundtype + GUNSHOT_OFFSET, dist, tempEQ) *
                                        (1.0f - ship->soundevent.coverage * 0.5f));

					if (((ship->soundevent.burstHandle == SOUND_NOTINITED) || soundover(ship->soundevent.burstHandle)) &&
						(vol > SOUND_VOL_MIN))
					{
                        if ((ship->shiptype == sMultiBeamFrigate) || (ship->shiptype == bMultiBeamFrigate))
                        {
                            patch = catShipPatch(GunSpecEventsLUT, 0, 0x8001);
                        }
                        else if (ship->shiptype == dAcolyte)
                        {
                            patch = catShipPatch(GunSpecEventsLUT, 0, 0x8000);
                        }
                        else
                        {
                            soundtype = cat_gunGetSoundType(&ship->gunInfo->guns[gunIndex]);
                            patch = catShipPatch(GunEventsLUT, soundtype, Gun_WeaponFireLooped);
                        }
						ship->soundevent.burstHandle = splayFPRVL(GunBank, patch, tempEQ, (real32)SOUND_DEFAULT, pan,
                                                                  SOUND_PRIORITY_MAX, vol, TRUE, TRUE, FALSE);
					}
					else
					{
                        catSEupdateHandle(ship->soundevent.burstHandle, vol, pan, tempEQ);
					}
				}
			}
			else
			{
				SEstopsoundhandle(&(ship->soundevent.burstHandle), SOUND_FADE_STOPNOW);
			}
		}

nextship:
		objnode = objnode->prev;
    }

#endif
}
#else

void soundEventUpdate(void)
{
	Ship *ship;

    if (!soundeventinited || soundpaused)
    {
        return;
    }

    // update the music volume
    musicEventUpdateVolume();

    nearestShipDistance = REALlyBig;
    nearestShipMoving = FALSE;

	/* update ship ranges */
	SEupdateShipRange();

	/* update engine sounds */
	for (i = 0; i < numMoships; i++)
	{
		ship = moships[i];

		/* EQ this puppy */
		SEeq(ship->staticinfo->shipclass, ship->soundevent.distance, tempEQ);

		if (ship->soundevent.engineHandle == SOUND_NOTINITED)
		{
			/* start the engine sound */
			ship->soundevent.engineHandle = splayEPRVL(ShipBank, ShipCmnEventsLUT->lookup[GetPatch(ShipCmnEventsLUT, ship->shiptype, ShipCmn_Engine)],
								   tempEQ, ship->soundevent.pan, SOUND_PRIORITY_MAX, ship->soundevent.engineVol, TRUE);
		}
		else
		{
			if (ship->soundevent.engineState == SOUND_STARTING)
			{
				if (vecMagnitudeSquared(ship->obj.posinfo.velocity) > SFX_FLOAT_VELOCITYSQR)
				{
					ship->soundevent.engineState = SOUND_PLAYING;
				}
			}
	
			/* set volume */
			soundvolume(ship->soundevent.engineHandle, ship->soundevent.engineVol);
	
			/* set pan */
			soundpan(ship->soundevent.engineHandle, ship->soundevent.pan);
	
			/* set equalization */
			soundequalize(ship->soundevent.engineHandle, tempEQ);
		}
	}
	
	for (i = 0; i < SFX_MAX_CAPENGINES; i++)
	{
		ship = capships[i];
		if (ship != NULL)
		{
			/* EQ this puppy */
			SEeq(ship->staticinfo->shipclass, ship->soundevent.distance, tempEQ);
	
			if (ship->soundevent.engineHandle == SOUND_NOTINITED)
			{
				/* start the engine sound */
				ship->soundevent.engineHandle = splayEPRVL(ShipBank, ShipCmnEventsLUT->lookup[GetPatch(ShipCmnEventsLUT, ship->shiptype, ShipCmn_Engine)],
									   tempEQ, ship->soundevent.pan, SOUND_PRIORITY_MAX, ship->soundevent.engineVol, TRUE);
			}
			else
			{
				if (ship->soundevent.engineState == SOUND_STARTING)
				{
					if (vecMagnitudeSquared(ship->obj.posinfo.velocity) > SFX_FLOAT_VELOCITYSQR)
					{
						ship->soundevent.engineState = SOUND_PLAYING;
					}
				}
		
				/* set volume */
				soundvolume(ship->soundevent.engineHandle, ship->soundevent.engineVol);
		
				/* set pan */
				soundpan(ship->soundevent.engineHandle, ship->soundevent.pan);
		
				/* set equalization */
				soundequalize(ship->soundevent.engineHandle, tempEQ);
			}
		}
	}


	for (i = 0; i < SFX_MAX_STRIKEENGINES; i++)
	{
		ship = strike[i];
		if (ship != NULL)
		{
			/* EQ this puppy */
			SEeq(ship->staticinfo->shipclass, ship->soundevent.distance, tempEQ);
	
			if (ship->soundevent.engineHandle == SOUND_NOTINITED)
			{
				/* start the engine sound */
				ship->soundevent.engineHandle = splayEPRVL(ShipBank, ShipCmnEventsLUT->lookup[GetPatch(ShipCmnEventsLUT, ship->shiptype, ShipCmn_Engine)],
									   tempEQ, ship->soundevent.pan, SOUND_PRIORITY_MAX, ship->soundevent.engineVol, TRUE);
			}
			else
			{
				if (ship->soundevent.engineState == SOUND_STARTING)
				{
					if (vecMagnitudeSquared(ship->obj.posinfo.velocity) > SFX_FLOAT_VELOCITYSQR)
					{
						ship->soundevent.engineState = SOUND_PLAYING;
					}
				}
				soundvolume(ship->soundevent.engineHandle, ship->soundevent.engineVol);
				soundpan(ship->soundevent.engineHandle, ship->soundevent.pan);
				soundequalize(ship->soundevent.engineHandle, tempEQ);
			}
		}
	}



	if (ship->soundevent.engineHandle == SOUND_SHIP_EXPLODED)
	{
		soundEventShipRemove(ship);
		goto nextship;
	}


	/* figure out the volume */
	vol = SEequalize(shipclass, dist, tempEQ);

	if (vol > SOUND_VOL_MAX)
	{
		vol = SOUND_VOL_MAX;
	}

	shipangle = SEgetAngleToCamera(ship);

	velocity = fsqrt(vecMagnitudeSquared(ship->obj.posinfo.velocity));

	if (ship->staticinfo->maxfuel > 0.0)
	{
		/* this is a fighter */
		if (shipclass == CLASS_Fighter)
		{
			velratio = FIGHTER_VELOCITY_LOWPITCH; + (velocity / ship->staticinfo->common.maxvelocity * FIGHTER_VELOCITY_SCALE);
			if (velratio > FIGHTER_VELOCITY_HIGHPITCH)
			{
				velratio = FIGHTER_VELOCITY_HIGHPITCH;
			}
		}
#if 0
		/* this is a corvette */
		else if (shipclass == CLASS_Corvette)
		{
			velratio = CORVETTE_VELOCITY_LOWPITCH; + (velocity / ship->staticinfo->common.maxvelocity * CORVETTE_VELOCITY_SCALE);
			if (velratio > CORVETTE_VELOCITY_HIGHPITCH)
			{
				velratio = CORVETTE_VELOCITY_HIGHPITCH;
			}
		}
#endif
	}

	if (nisIsRunning)
	{
		if ((velocity <= SFX_NIS_FLOAT_VELOCITY) && (ship->soundevent.engineState != SOUND_STARTING))
		{
			vol = 0;
		}
	}
	else
	{
		if ((velocity <= SFX_FLOAT_VELOCITY)  && (ship->soundevent.engineState != SOUND_STARTING))
		{
			vol = 0;
		}
	}

	/* need this for setting the music volume */
	if (dist < nearestShipDistance)
	{
		nearestShipDistance = dist;
		if (velocity > SFX_FLOAT_VELOCITY)
		{
			nearestShipMoving = TRUE;
		}
	}

	if (ship->soundevent.engineHandle == SOUND_NOTINITED)
	{
		/* start the engine sound */
			if (velratio > 1.0f)
			{
				ship->soundevent.engineHandle = splayFPRVL(ShipBank, ShipCmnEventsLUT->lookup[GetPatch(ShipCmnEventsLUT, ship->shiptype, ShipCmn_Engine)],
									   tempEQ, velratio, pan, SOUND_PRIORITY_MAX, vol, TRUE, TRUE, FALSE);
			}
			else
			{
				ship->soundevent.engineHandle = splayEPRVL(ShipBank, ShipCmnEventsLUT->lookup[GetPatch(ShipCmnEventsLUT, ship->shiptype, ShipCmn_Engine)],
									   tempEQ, pan, SOUND_PRIORITY_MAX, vol, TRUE);
			}
	}
	else
	{
		if ((ship->soundevent.engineState == SOUND_STARTING) && (velocity > SFX_FLOAT_VELOCITY))
		{
			ship->soundevent.engineState = SOUND_PLAYING;
		}

		/* set volume */
		soundvolume(ship->soundevent.engineHandle, vol);

		/* set pan */
		soundpan(ship->soundevent.engineHandle, pan);

		/* set frequency */
		if (velratio > 1.0f)
		{
			soundfrequency(ship->soundevent.engineHandle, velratio);
		}

		/* set equalization */
		soundequalize(ship->soundevent.engineHandle, tempEQ);
	}

	if (shipclass == CLASS_Fighter)
	{
		if (FIGHTER_DOPPLER_USEVELOCITY)
		{
			soundshipheading(ship->soundevent.engineHandle, shipangle, FIGHTER_DOPPLER_HIGH, FIGHTER_DOPPLER_LOW, velratio, FIGHTER_DOPPLER_SCALE);
		}
		else
		{
			soundshipheading(ship->soundevent.engineHandle, shipangle, FIGHTER_DOPPLER_HIGH, FIGHTER_DOPPLER_LOW, 1.0f, FIGHTER_DOPPLER_SCALE);
		}
	}
#if 1
	if (shipclass == CLASS_Corvette)
	{
		if (CORVETTE_DOPPLER_USEVELOCITY)
		{
			soundshipheading(ship->soundevent.engineHandle, shipangle, CORVETTE_DOPPLER_HIGH, CORVETTE_DOPPLER_LOW, velratio, CORVETTE_DOPPLER_SCALE);
		}
		else
		{
			soundshipheading(ship->soundevent.engineHandle, shipangle, CORVETTE_DOPPLER_HIGH, CORVETTE_DOPPLER_LOW, 1.0f, CORVETTE_DOPPLER_SCALE);
		}
	}
#endif

	/* ambients */
	/* special sounds */
	/* gimbles */

}
#endif

#if 0
void SEupdateShipRange(void)
{
    Node *objnode = universe.RenderList.tail;
    Ship *ship;
	Ship *tempship;
	Ship *temp;
    ShipClass shipclass;
	sdword i;

	
	numMoships = 0;

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);

        if (ship->header.objtype != OBJ_ShipType)
        {
			goto next;
        }

        shipclass = ship->staticinfo->shipclass;

        if (SEinrangeSqr(shipclass, ship->obj.cameraDistanceSquared))
		{
			ship->soundevent.distance = fsqrt(ship->obj.cameraDistanceSquared);
			
			ship->soundevent.coverage = rndComputeOverlap(ship, 1.0);	// make 1.0 tweakable in a script
	
			ship->soundevent.pan = getPanAngle(ship->enginePosition);

			ship->soundevent.engineVol = SEcalcvol(shipclass, ship->soundevent.distance);	// want to modify the volume by the coverage and a tweak to modify the effect of coverage

			if (ship->soundevent.engineVol > SOUND_VOL_MAX)
			{
				ship->soundevent.engineVol = SOUND_VOL_MAX;
			}
		}
		else
		{
			ship->soundevent.engineVol = 0;
		}
		
		if (ship->soundevent.engineVol > 0)
		{
			tempship = ship;

			if ((shipclass == CLASS_Fighter) || (shipclass == CLASS_Corvette))
			{
				for (i = 0; i < SFX_MAX_STRIKEENGINES; i++)
				{
					if (strike[i] != NULL)
					{
						if ((tempship->soundevent.engineVol > strike[i]->soundevent.engineVol) ||
							((tempship->soundevent.engineVol == strike[i]->soundevent.engineVol) && (tempship->soundevent.coverage < strike[i]->soundevent.coverage)))
						{
							temp = strike[i];
							strike[i] = tempship;
							tempship = temp;
						}
					}
					else
					{
						strike[i] = tempship;
						break;
					}
				}
			}
			else if (shipclass == CLASS_Mothership)
			{
				if (numMoships < SE_MAX_MOSHIPS)
				{
					moships[numMoships++] = tempship;
				}
			}
			else
			{
				for (i = 0; i < SFX_MAX_CAPENGINES; i++)
				{
					if (capships[i] != NULL)
					{
						if ((tempship->soundevent.engineVol > capships[i]->soundevent.engineVol) ||
							((tempship->soundevent.engineVol == capships[i]->soundevent.engineVol) && (tempship->soundevent.coverage < capships[i]->soundevent.coverage)))
						{
							temp = capships[i];
							capships[i] = tempship;
							tempship = temp;
						}
					}
					else
					{
						capships[i] = tempship;
						break;
					}
				}
			}
		}
next:
        objnode = objnode->prev;
    }
}
#endif

void soundEventInitStruct(SOUNDEVENT *pseStruct)
{
    pseStruct->engineHandle = SOUND_NOTINITED;
    pseStruct->engineState = SOUND_NOTINITED;
    pseStruct->ambientHandle = SOUND_NOTINITED;
	pseStruct->damageHandle = SOUND_NOTINITED;
    pseStruct->specialHandle = SOUND_NOTINITED;
	pseStruct->hyperspaceHandle = SOUND_NOTINITED;
    pseStruct->actorNum = SOUND_NOTINITED;

	pseStruct->coverage = 0.0f;
	pseStruct->distance = REALlyBig;
	pseStruct->pan = SOUND_PAN_CENTER;
	pseStruct->engineVol = SOUND_NOTINITED;

	pseStruct->randomHandle = SOUND_NOTINITED;
    pseStruct->nextRandom = universe.totaltimeelapsed;

    pseStruct->lastAmbient = SOUND_NOTINITED;      // CAT: none yet (soundEventInitStruct 0x0051f900; HW1 ShipCmn_Ambient)

    pseStruct->timeLastStatus = universe.totaltimeelapsed;

    pseStruct->burstHandle = SOUND_NOTINITED;
    pseStruct->burstfiring = FALSE;

    pseStruct->gunHandle = SOUND_NOTINITED;
    pseStruct->gun = NULL;
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void soundEventShipDied(Ship *deadship)
{
	soundEventShipRemove(deadship);
	
	if (deadship != NULL)
	{
		deadship->soundevent.engineHandle = SOUND_SHIP_EXPLODED;
	}

    /* might need some other stuff here */
    if (enableSpeech)
    {
        if (deadship != NULL)
        {
            /* remove all the speech events for this ship */
            speechEventRemoveShip(deadship);

            if (deadship->playerowner == universe.curPlayerPtr)
            {
                /* need some global warnings */
                if (isCapitalShip(deadship))
                {
                    // CAT: no CryoTray exemption, Cataclysm has no such type (soundEventShipDied 0x0051f960)
                    speechEventFleet(STAT_F_Cap_Dies, deadship->staticinfo->shipclass, deadship->playerowner->playerIndex);
                }
                else if (deadship->shiptype == ResourceCollector)
                {
                    speechEventFleet(STAT_F_ResCol_Dies, 0, deadship->playerowner->playerIndex);
                }
            }
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void soundEventShipRemove(Ship *ship)
{
    if (enableSFX)
    {
        SEstopengine(ship, TRUE);
        soundEventBurstStop(ship, NULL);
        SEstopsoundhandle(&(ship->soundevent.specialHandle), SOUND_FADE_STOPNOW);
        SEstopsoundhandle(&(ship->soundevent.ambientHandle), SOUND_FADE_STOPNOW);
        SEstopsoundhandle(&(ship->soundevent.damageHandle), SOUND_FADE_STOPNOW);
        SEstopsoundhandle(&(ship->soundevent.randomHandle), SOUND_FADE_STOPNOW);
		SEstopsoundhandle(&(ship->soundevent.gunHandle), SOUND_FADE_STOPNOW);
		SEstopsoundhandle(&(ship->soundevent.hyperspaceHandle), SOUND_FADE_STOPNOW);
		SEstopsoundhandle(&(ship->soundevent.burstHandle), SOUND_FADE_STOPNOW);

        if (ship != NULL)
        {
            ship->soundevent.engineHandle = SOUND_NOTINITED;
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : cat_madAnimSoundTableInit
    Description : CAT: the sound event each mesh animation of the first 60 ship types plays when it starts
                  (cat_madAnimSoundTableInit 0x004b64c0, from soundEventInit; table .bss 0x00c32660, 60 x 6, -1 none):
                  the doors and jacks of the Acolytes, Mimic, Leech, Ramming frigate, Hives, sCarrier, Workers and
                  Sentinels. Its reader is madAnimationUpdate 0x004b6ce0 (cat_madAnimSoundEvent below), which the
                  mesh-animation port brings (MeshAnim.c, CAT-TODO(M2-R6 mesh animation)).
----------------------------------------------------------------------------*/
#define CAT_MAD_SOUND_TYPES     60
#define CAT_MAD_SOUND_ANIMS     6
static sdword catMadAnimSound[CAT_MAD_SOUND_TYPES][CAT_MAD_SOUND_ANIMS];

void cat_madAnimSoundTableInit(void)
{
    sdword i, j;

    for (i = 0; i < CAT_MAD_SOUND_TYPES; i++)
    {
        for (j = 0; j < CAT_MAD_SOUND_ANIMS; j++)
        {
            catMadAnimSound[i][j] = SOUND_NOTINITED;
        }
    }
    catMadAnimSound[sAcolyte][0] = 0x81a;
    catMadAnimSound[sAcolyte][1] = 0x81b;
    catMadAnimSound[sAcolyte][2] = 0x405;
    catMadAnimSound[sAcolyte][3] = 0x406;
    catMadAnimSound[sMimic][0] = 0x405;
    catMadAnimSound[sMimic][1] = 0x406;
    catMadAnimSound[sLeech][0] = 0x822;
    catMadAnimSound[sLeech][1] = 0x823;
    catMadAnimSound[sRammingFrigate][0] = 0x81c;
    catMadAnimSound[sHiveFrigate][0] = 0x405;
    catMadAnimSound[sHiveFrigate][1] = 0x406;
    catMadAnimSound[sCarrier][0] = 0x405;
    catMadAnimSound[sCarrier][1] = 0x406;
    catMadAnimSound[sWorker][0] = 0x81e;
    catMadAnimSound[sWorker][1] = 0x81f;
    catMadAnimSound[sWorker][2] = 0x406;
    catMadAnimSound[sWorker][3] = 0x405;
    catMadAnimSound[sSentinel][0] = 0x405;
    catMadAnimSound[sSentinel][1] = 0x406;
    catMadAnimSound[bAcolyte][0] = 0x81a;
    catMadAnimSound[bAcolyte][1] = 0x81b;
    catMadAnimSound[bAcolyte][2] = 0x405;
    catMadAnimSound[bAcolyte][3] = 0x406;
    catMadAnimSound[bHiveFrigate][0] = 0x405;
    catMadAnimSound[bHiveFrigate][1] = 0x406;
    catMadAnimSound[bWorker][0] = 0x81e;
    catMadAnimSound[bWorker][1] = 0x81f;
    catMadAnimSound[bWorker][2] = 0x406;
    catMadAnimSound[bWorker][3] = 0x405;
    catMadAnimSound[bSentinel][0] = 0x405;
    catMadAnimSound[bSentinel][1] = 0x406;
}

// CAT: the event of animation number anim of a ship type, or SOUND_NOTINITED (madAnimationUpdate 0x004b6ce0 plays it
//      through soundEventPlay at the animation's first frame)
sdword cat_madAnimSoundEvent(ShipType shiptype, sdword anim)
{
    if (((udword)shiptype >= CAT_MAD_SOUND_TYPES) || ((udword)anim >= CAT_MAD_SOUND_ANIMS))
    {
        return SOUND_NOTINITED;
    }
    return catMadAnimSound[shiptype][anim];
}


/*-----------------------------------------------------------------------------
    Name        : cat_soundEventBlast
    Description : CAT: a blast's sound (cat_soundEventBlast 0x0051fc60, from cat_blastUpdate): heard from the point
                  of the shell nearest the camera (the NIS camera during an NIS, four times nearer), on the big gun's
                  shot row (0x2a); the blast's event of ShipEvents.lut loops while it is in range.
    Inputs      : blast - the blast
----------------------------------------------------------------------------*/
void cat_soundEventBlast(CatBlast *blast)
{
    vector eye, dir, nearest;
    real32 dist;
    sword vol, pan;
    real32 tempEQ[SOUND_EQ_SIZE];

    // CAT-FIX: the exe has no SFX test here and reads the ship LUT, which is NULL when the sound effects are off
    if (!enableSFX)
    {
        return;
    }
    if (nisIsRunning)
    {
        eye = nisCamera->eyeposition;
    }
    else
    {
        eye = universe.mainCameraCommand.actualcamera.eyeposition;
    }
    vecSub(dir, eye, blast->position);
    vecNormalize(&dir);
    vecMultiplyByScalar(dir, blast->radius);
    vecAdd(nearest, dir, blast->position);
    dist = cat_vecDistance(&eye, &nearest);
    if (nisIsRunning)
    {
        dist *= 0.25f;
    }

    if (!SEinrange(GUNSHOT_OFFSET + GS_BigGun, dist))
    {
        soundEventStopSound(blast->soundHandle, 0);
        blast->soundHandle = SOUND_NOTINITED;
        return;
    }

    pan = getPanAngle(nearest, 350.0f, dist);
    vol = SEequalize(GUNSHOT_OFFSET + GS_BigGun, dist, tempEQ);
    if ((blast->soundHandle == SOUND_NOTINITED) || soundover(blast->soundHandle))
    {
        blast->soundHandle = splayFPRVL(ShipBank, ShipEventsLUT->lookup[GetPatch(ShipEventsLUT, 0, blast->soundEvent)],
                                        tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, TRUE, TRUE, FALSE);
    }
    soundvolumeF(blast->soundHandle, vol, 0.0f);
    soundpanF(blast->soundHandle, pan, 0.0f);
    soundequalize(blast->soundHandle, tempEQ);
}


/*-----------------------------------------------------------------------------
    Name        : cat_soundEventAsteroidBreak
    Description : CAT: the sound of an asteroid breaking up (cat_soundEventAsteroidBreak 0x005215d0, from
                  BreakAsteroidUp): ship event 0x80c on the first explosion row, in the main screen only
    Inputs      : asteroid - the asteroid
----------------------------------------------------------------------------*/
void cat_soundEventAsteroidBreak(SpaceObj *asteroid)
{
    real32 dist;
    sword vol, pan;
    real32 tempEQ[SOUND_EQ_SIZE];

    // CAT-FIX: !enableSFX: the exe reads the ship LUT without that test (NULL when the sound effects are off)
    if (!enableSFX || smSensorsActive || !mrRenderMainScreen)
    {
        return;
    }
    if (SEinrangeSqr(EXPLOSION_OFFSET, asteroid->cameraDistanceSquared))
    {
        dist = fsqrt(asteroid->cameraDistanceSquared);
        vol = SEequalize(EXPLOSION_OFFSET, dist, tempEQ);
        pan = getPanAngle(asteroid->posinfo.position, 500.0f, dist);
        splayFPRVL(ShipBank, ShipEventsLUT->lookup[GetPatch(ShipEventsLUT, 0, 0x80c)], tempEQ, (real32)SOUND_DEFAULT,
                   pan, SOUND_PRIORITY_NORMAL, vol, FALSE, TRUE, FALSE);
    }
}


/*-----------------------------------------------------------------------------
    Name        : cat_soundLevelReset
    Description : CAT: the sound's part of a level start (cat_soundLevelReset 0x0051c980, from levelPreInit): the
                  speech alerts, the battle chatter's memory, the letterbox off, the player-name Easter eggs
----------------------------------------------------------------------------*/
void cat_soundLevelReset(void)
{
    cat_speechAlertsReset();
    cat_battleChatterReset();
    cat_soundEventLetterboxVolume(FALSE);
    cat_speechEasterEggCheck();
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void soundEventDerelictRemove(Derelict *pDerelict)
{
    if (enableSFX)
    {
		SEstopsoundhandle(&(pDerelict->ambientSoundHandle), SOUND_FADE_STOPNOW);
		SEstopsoundhandle(&(pDerelict->randomSoundHandle), SOUND_FADE_STOPNOW);
    }
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 SEinrange(sdword shipclass, real32 distance)
{
    if (RangeLUT != NULL)
    {
        if (shipclass < RangeLUT->rows)
        {
            if (distance <= (real32)RangeLUT->lookup[shipclass * RangeLUT->columns + 2])
            {
                return (TRUE);
            }
        }
    }

    return (FALSE);
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 SEinrangeSqr(sdword shipclass, real32 distancesqr)
{
    if (RangeLUT != NULL)
    {
        if (shipclass < RangeLUT->rows)
        {
            real32 rangelut = (real32)RangeLUT->lookup[shipclass * RangeLUT->columns + 2];
            if (distancesqr <= (rangelut*rangelut))
            {
                return (TRUE);
            }
        }
    }

    return (FALSE);
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sword SEcalcvol(sdword objtype, real32 distance)
{
    sdword i, pos, index = SOUND_ERR;
    real32 mult, vol = 0.0f;

    if ((RangeLUT != NULL) && (VolumeLUT != NULL))
    {
        if ((objtype < RangeLUT->rows) && (objtype < VolumeLUT->rows))
        {
            pos = objtype * RangeLUT->columns;

            for (i = 0; i < RangeLUT->columns; i++)
            {
                if (distance <= (real32)RangeLUT->lookup[pos + i])
                {
                    index = pos + i;
                    break;
                }
            }

            if (index < SOUND_OK)
            {
                return (0);
            }

			mult = ((real32)RangeLUT->lookup[index] - distance) * RangeFloatLUT[index];

			vol = volSFX * ((real32)VolumeLUT->lookup[index] + (mult * VolumeFloatLUT[index]));
        }
    }

    return ((sword)vol);
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sword SEequalize(sdword objtype, real32 distance, real32 *eq)
{
    sdword i, pos, index = SOUND_ERR, Findex = SOUND_ERR;
    real32 mult, vol = 0.0f;

    if ((RangeLUT != NULL) && (FrequencyLUT != NULL) && (VolumeLUT != NULL))
    {
        if ((objtype < RangeLUT->rows) && (objtype < VolumeLUT->rows))
        {
            pos = objtype * RangeLUT->columns;

            for (i = 0; i < RangeLUT->columns; i++)
            {
                if (distance <= (real32)RangeLUT->lookup[pos + i])
                {
                    Findex = i * FrequencyLUT->columns;
                    index = pos + i;
                    break;
                }
            }

            if (index < SOUND_OK)
            {
                for (i = 0; i < FrequencyLUT->columns; i++)
                {
                    eq[i] = DefaultEQ[i];
                }

                return (0);
            }

			mult = ((real32)RangeLUT->lookup[index] - distance) * RangeFloatLUT[index];
			vol = volSFX * ((real32)VolumeLUT->lookup[index] + (mult * VolumeFloatLUT[index]));
			
			if (vol < SFX_MIN_PERCEPTABLE_VOL)
			{
				vol = 0;
			}
			else
			{
				if (vol > SOUND_VOL_MAX)
				{
					vol = SOUND_VOL_MAX;
				}

				for (i = 0; i < FrequencyLUT->columns; i++)
				{
					eq[i] = FrequencyLUT->lookup[Findex + i] + (mult * FreqLUT[Findex + i]);
				}
			}
        }
    }

    // CAT: the volume is rounded with Relic's rint (SEequalize 0x0051ff50; HW1 truncated)
    return ((sword)fqRint(vol));
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SEeq(sdword objtype, real32 distance, real32 *eq)
{
    sdword i, pos, index = SOUND_ERR, Findex = SOUND_ERR;
    real32 mult;

    if ((RangeLUT != NULL) && (FrequencyLUT != NULL) && (VolumeLUT != NULL))
    {
        if ((objtype < RangeLUT->rows) && (objtype < VolumeLUT->rows))
        {
            pos = objtype * RangeLUT->columns;

            for (i = 0; i < RangeLUT->columns; i++)
            {
                if (distance <= (real32)RangeLUT->lookup[pos + i])
                {
                    Findex = i * FrequencyLUT->columns;
                    index = pos + i;
                    break;
                }
            }

            if (index < SOUND_OK)
            {
                for (i = 0; i < FrequencyLUT->columns; i++)
                {
                    eq[i] = DefaultEQ[i];
                }

                return;
            }

			mult = ((real32)RangeLUT->lookup[index] - distance) * RangeFloatLUT[index];

			for (i = 0; i < FrequencyLUT->columns; i++)
			{
				eq[i] = FrequencyLUT->lookup[Findex + i] + (mult * FreqLUT[Findex + i]);
			}
        }
    }

    return;
}


sword SEcalcenginevol(sdword shipclass, real32 distance)
{
    sdword i, index = SOUND_ERR;
    real32 mult;
    sword vol;
    sword pos;

    pos = shipclass * RangeLUT->columns;


    for (i = 1; i < 3; i++)
    {
        if (distance <= (real32)RangeLUT->lookup[pos + i])
        {
            index = pos + i - 1;
            break;
        }
    }

    if (index < SOUND_OK)
    {
        return (0);
    }

    mult = ((real32)RangeLUT->lookup[index+1] - distance) / (real32)(RangeLUT->lookup[index+1] - RangeLUT->lookup[index]);

    vol = (sword)(VolumeLUT->lookup[index+1] + (mult * (VolumeLUT->lookup[index] - VolumeLUT->lookup[index+1])));

    return ((sword)(vol * SFX_VOL_FACTOR));
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SEstopengine(Ship *ship, bool32 stopnow)
{
    if (ship == NULL)
        return;

    ship->soundevent.engineState = SOUND_NOTINITED;

    if (ship->soundevent.engineHandle > SOUND_NOTINITED)
    {
        if (stopnow)
        {
            soundstop(ship->soundevent.engineHandle, SOUND_FADE_STOPNOW);
        }
        else
        {
            soundstop(ship->soundevent.engineHandle, 5.0f);
        }
        ship->soundevent.engineHandle = SOUND_NOTINITED;
    }
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SEstopsoundhandle(sdword *shandle, real32 fadetime)
{
    if (*shandle > SOUND_NOTINITED)
    {
        soundstop(*shandle, fadetime);
        *shandle = SOUND_NOTINITED;
    }
}


#ifdef HW_PTR_64
void loadbankAndConvertTo64Bit(char *fileName, void** loadAddress)
{
	int i;

	int loopvar;
	int loopcount;

	BANK *oldHeader = NULL;
	BANK *newHeader = NULL;

	PATCH_DISK *oldPatch = NULL;
	PATCH *newPatch = NULL;

	memsize offset=0;
	memsize loopsize=0;

	void * oldptr = NULL;
	void * newptr = NULL;
	void * oldbase = NULL;
	void * newbase = NULL;
	void * tmpptr = NULL;

	int oldLength;
	int newLength;

	udword *oldShare = NULL;
	udword *newShare = NULL;

	oldLength = fileLoadAlloc(fileName, (void **)&oldbase, NonVolatile);
    oldHeader = oldptr = oldbase;

    //printf("%s size is %d\n",oldFile, oldLength);

    newLength = 2*oldLength;

	newbase = memAlloc(2* oldLength, "bank 64bit conversion", 0); 
    newHeader = newbase;
    newptr =  (void*) newbase;

    memset (newbase, 0 ,2*oldLength);
  

    newHeader->id = oldHeader->id;
    newHeader->checksum = oldHeader->checksum;
    newHeader->numpatches = oldHeader->numpatches;

    oldPatch = (PATCH_DISK*)&oldHeader->firstpatch;
    // HWI LP64: the 8-byte aligned patch table SNDbankpatches reads (soundcmn.h); the data moves by the larger patches
    // and by the padding before the table
    newPatch = SNDbankpatches(newHeader);


    offset=newHeader->numpatches * (sizeof(PATCH) - sizeof(PATCH_DISK))
           + (SND_BANK_PATCHES_OFFSET - offsetof(BANK, firstpatch));
    //printf("New data offset =  +%d\n",offset);

    loopsize=newHeader->numpatches;

    for (i=0;i<loopsize;i++){
        //printf("%3d: %d: dataoffset:%ld datasize: %ld loopstart: %ld loopend: %ld\n",i, oldPatch->id , oldPatch->dataoffset, oldPatch->datasize, oldPatch->loopstart, oldPatch -> loopend); 
        newPatch->id = oldPatch->id;
        newPatch->priority = oldPatch->priority;
        newPatch->pitch = oldPatch->pitch;
        newPatch->dataoffset = oldPatch->dataoffset + offset;
        newPatch->datasize = oldPatch->datasize;
        newPatch->loopstart = oldPatch->loopstart;
        newPatch->loopend = oldPatch->loopend;
        newPatch->bitrate = oldPatch->bitrate;
        newPatch->flags = oldPatch->flags;
        newPatch->volume = oldPatch->volume;
        newPatch->pan = oldPatch->pan;
        newPatch->waveformat.format = oldPatch->waveformat.format;
        newPatch->waveformat.channels = oldPatch->waveformat.channels;
        newPatch->waveformat.frequency = oldPatch->waveformat.frequency;
        newPatch->waveformat.avgBytesPerSecond = oldPatch->waveformat.avgBytesPerSecond;
        newPatch->waveformat.blockAlign = oldPatch->waveformat.blockAlign;
        newPatch->wavepad = oldPatch->wavepad;
        //printf(" becomes dataoffset:%ld \n", newPatch->dataoffset); 

    oldPatch++;
    newPatch++;
    }

    newLength = oldLength + offset;
    
    offset = (memsize)oldptr + oldLength - (memsize) oldPatch;

    memcpy (newPatch, oldPatch, offset);

	memFree(oldbase);
	*loadAddress = newbase;
}
#endif


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SEloadbank(void)
{
    char loadfile[100];
    
#if FIX_ENDIAN
	int  i;
#endif

    /* new stuff */
    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "GunEvents.lut");
    fileLoadAlloc(loadfile, (void**)&GunEventsLUT, NonVolatile);

#if FIX_ENDIAN	
	GunEventsLUT->ID            = FIX_ENDIAN_INT_32( GunEventsLUT->ID );
	GunEventsLUT->checksum      = FIX_ENDIAN_INT_32( GunEventsLUT->checksum );
	GunEventsLUT->numvariations = FIX_ENDIAN_INT_16( GunEventsLUT->numvariations );
	GunEventsLUT->numevents     = FIX_ENDIAN_INT_16( GunEventsLUT->numevents );
	GunEventsLUT->numobjects    = FIX_ENDIAN_INT_16( GunEventsLUT->numobjects );
	int lt = GunEventsLUT->numvariations * GunEventsLUT->numevents * GunEventsLUT->numobjects;
	for ( i=0; i<lt; i++ )
		GunEventsLUT->lookup[i] = FIX_ENDIAN_INT_16( GunEventsLUT->lookup[i] );
#endif

    // CAT: the special gun sounds of the multibeam frigates, dAcolyte, the Dreadnought and the destroyers
    //      (SEloadbank 0x005200c0 loads "GunSpecEvents.lut" after GunEvents.lut; read by soundEventPlay)
    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "GunSpecEvents.lut");
    fileLoadAlloc(loadfile, (void**)&GunSpecEventsLUT, NonVolatile);

    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "ShipCmnEvents.lut");
    fileLoadAlloc(loadfile, (void**)&ShipCmnEventsLUT, NonVolatile);

#if FIX_ENDIAN
	ShipCmnEventsLUT->ID            = FIX_ENDIAN_INT_32( ShipCmnEventsLUT->ID );
	ShipCmnEventsLUT->checksum      = FIX_ENDIAN_INT_32( ShipCmnEventsLUT->checksum );
	ShipCmnEventsLUT->numvariations = FIX_ENDIAN_INT_16( ShipCmnEventsLUT->numvariations );
	ShipCmnEventsLUT->numevents     = FIX_ENDIAN_INT_16( ShipCmnEventsLUT->numevents );
	ShipCmnEventsLUT->numobjects    = FIX_ENDIAN_INT_16( ShipCmnEventsLUT->numobjects );
	lt = ShipCmnEventsLUT->numvariations * ShipCmnEventsLUT->numevents * ShipCmnEventsLUT->numobjects;
	for ( i=0; i<lt; i++ )
		ShipCmnEventsLUT->lookup[i] = FIX_ENDIAN_INT_16( ShipCmnEventsLUT->lookup[i] );
#endif
		
    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "ShipEvents.lut");
    fileLoadAlloc(loadfile, (void**)&ShipEventsLUT, NonVolatile);

#if FIX_ENDIAN
	ShipEventsLUT->ID            = FIX_ENDIAN_INT_32( ShipEventsLUT->ID );
	ShipEventsLUT->checksum      = FIX_ENDIAN_INT_32( ShipEventsLUT->checksum );
	ShipEventsLUT->numvariations = FIX_ENDIAN_INT_16( ShipEventsLUT->numvariations );
	ShipEventsLUT->numevents     = FIX_ENDIAN_INT_16( ShipEventsLUT->numevents );
	ShipEventsLUT->numobjects    = FIX_ENDIAN_INT_16( ShipEventsLUT->numobjects );
	lt = ShipEventsLUT->numvariations * ShipEventsLUT->numevents * ShipEventsLUT->numobjects;
	for ( i=0; i<lt; i++ )
		ShipEventsLUT->lookup[i] = FIX_ENDIAN_INT_16( ShipEventsLUT->lookup[i] );
#endif
		
	if (ShipEventsLUT->checksum != ShipCmnEventsLUT->checksum)
	{
		dbgMessage("Lookup tables do not match.  Not from same generate.");
		dbgAssertOrIgnore(FALSE);
	}

    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "DerelictEvents.lut");
    fileLoadAlloc(loadfile, (void**)&DerelictEventsLUT, NonVolatile);

#if FIX_ENDIAN
	DerelictEventsLUT->ID            = FIX_ENDIAN_INT_32( DerelictEventsLUT->ID );
	DerelictEventsLUT->checksum      = FIX_ENDIAN_INT_32( DerelictEventsLUT->checksum );
	DerelictEventsLUT->numvariations = FIX_ENDIAN_INT_16( DerelictEventsLUT->numvariations );
	DerelictEventsLUT->numevents     = FIX_ENDIAN_INT_16( DerelictEventsLUT->numevents );
	DerelictEventsLUT->numobjects    = FIX_ENDIAN_INT_16( DerelictEventsLUT->numobjects );
	lt = DerelictEventsLUT->numvariations * DerelictEventsLUT->numevents * DerelictEventsLUT->numobjects;
	for ( i=0; i<lt; i++ )
		DerelictEventsLUT->lookup[i] = FIX_ENDIAN_INT_16( DerelictEventsLUT->lookup[i] );
#endif
		
	if (DerelictEventsLUT->checksum != ShipCmnEventsLUT->checksum)
	{
		dbgMessage("Lookup tables do not match.  Not from same generate.");
		dbgAssertOrIgnore(FALSE);
	}

//    strcpy(loadfile, SOUNDFXDIR);
//    strcat(loadfile, "SpecEffectEvents.lut");
//    fileLoadAlloc(loadfile, &SpecEffectEventsLUT, NonVolatile);

    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "SpecExpEvents.lut");
    fileLoadAlloc(loadfile, (void**)&SpecExpEventsLUT, NonVolatile);

#if FIX_ENDIAN
	SpecExpEventsLUT->ID            = FIX_ENDIAN_INT_32( SpecExpEventsLUT->ID );
	SpecExpEventsLUT->checksum      = FIX_ENDIAN_INT_32( SpecExpEventsLUT->checksum );
	SpecExpEventsLUT->numvariations = FIX_ENDIAN_INT_16( SpecExpEventsLUT->numvariations );
	SpecExpEventsLUT->numevents     = FIX_ENDIAN_INT_16( SpecExpEventsLUT->numevents );
	SpecExpEventsLUT->numobjects    = FIX_ENDIAN_INT_16( SpecExpEventsLUT->numobjects );
	lt = SpecExpEventsLUT->numvariations * SpecExpEventsLUT->numevents * SpecExpEventsLUT->numobjects;
	for ( i=0; i<lt; i++ )
		SpecExpEventsLUT->lookup[i] = FIX_ENDIAN_INT_16( SpecExpEventsLUT->lookup[i] );
#endif

//	if (SpecExpEventsLUT->checksum != SpecEffectEventsLUT->checksum)
//	{
//		dbgMessage("Lookup tables do not match.  Not from same generate.");
//		dbgAssertOrIgnore(FALSE);
//	}

    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "SpecHitEvents.lut");
    fileLoadAlloc(loadfile, (void**)&SpecHitEventsLUT, NonVolatile);

#if FIX_ENDIAN
	SpecHitEventsLUT->ID            = FIX_ENDIAN_INT_32( SpecHitEventsLUT->ID );
	SpecHitEventsLUT->checksum      = FIX_ENDIAN_INT_32( SpecHitEventsLUT->checksum );
	SpecHitEventsLUT->numvariations = FIX_ENDIAN_INT_16( SpecHitEventsLUT->numvariations );
	SpecHitEventsLUT->numevents     = FIX_ENDIAN_INT_16( SpecHitEventsLUT->numevents );
	SpecHitEventsLUT->numobjects    = FIX_ENDIAN_INT_16( SpecHitEventsLUT->numobjects );
	lt = SpecHitEventsLUT->numvariations * SpecHitEventsLUT->numevents * SpecHitEventsLUT->numobjects;
	for ( i=0; i<lt; i++ )
		SpecHitEventsLUT->lookup[i] = FIX_ENDIAN_INT_16( SpecHitEventsLUT->lookup[i] );
#endif
		
//	if (SpecHitEventsLUT->checksum != SpecEffectEventsLUT->checksum)
	if (SpecHitEventsLUT->checksum != SpecExpEventsLUT->checksum)
	{
		dbgMessage("Lookup tables do not match.  Not from same generate.");
		dbgAssertOrIgnore(FALSE);
	}

    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, "UIEvents.lut");
    fileLoadAlloc(loadfile, (void**)&UIEventsLUT, NonVolatile);

#if FIX_ENDIAN
	UIEventsLUT->ID = FIX_ENDIAN_INT_32( UIEventsLUT->ID );
	UIEventsLUT->checksum = FIX_ENDIAN_INT_32( UIEventsLUT->checksum );
	UIEventsLUT->numvariations = FIX_ENDIAN_INT_16( UIEventsLUT->numvariations );
	UIEventsLUT->numevents = FIX_ENDIAN_INT_16( UIEventsLUT->numevents );
	UIEventsLUT->numobjects = FIX_ENDIAN_INT_16( UIEventsLUT->numobjects );
	lt = UIEventsLUT->numvariations * UIEventsLUT->numevents * UIEventsLUT->numobjects;
	for ( i=0; i<lt; i++ )
		UIEventsLUT->lookup[i] = FIX_ENDIAN_INT_16( UIEventsLUT->lookup[i] );
#endif

    strcpy(loadfile, SOUNDFXDIR);
	strcat(loadfile, "Guns.bnk");
#ifdef HW_PTR_64
    loadbankAndConvertTo64Bit(loadfile, (void**)&GunBank);
#else
    fileLoadAlloc(loadfile, (void**)&GunBank, NonVolatile);
#endif
	if (soundbankadd(GunBank) != GunEventsLUT->checksum)
	{
		dbgMessage("Lookup tables do not match.  Not from same generate.");
		dbgAssertOrIgnore(FALSE);
	}

    strcpy(loadfile, SOUNDFXDIR);
	strcat(loadfile, "Ships.bnk");
#ifdef HW_PTR_64
    loadbankAndConvertTo64Bit(loadfile, (void**)&ShipBank);
#else
    fileLoadAlloc(loadfile, (void**)&ShipBank, NonVolatile);
#endif
	if (soundbankadd(ShipBank) != ShipCmnEventsLUT->checksum)
	{
		dbgMessage("Ship bank file does not match Lookup tables.  Not from same generate.");
		dbgAssertOrIgnore(FALSE);
	}

    strcpy(loadfile, SOUNDFXDIR);
	strcat(loadfile, "SpecialEffects.bnk");
#ifdef HW_PTR_64
    loadbankAndConvertTo64Bit(loadfile, (void**)&SpecialEffectBank);
#else
    fileLoadAlloc(loadfile, (void**)&SpecialEffectBank, NonVolatile);
#endif
//	if (soundbankadd(SpecialEffectBank) != SpecEffectEventsLUT->checksum)
	if (soundbankadd(SpecialEffectBank) != SpecExpEventsLUT->checksum)
	{
		dbgMessage("Lookup tables do not match.  Not from same generate.");
		dbgAssertOrIgnore(FALSE);
	}

    strcpy(loadfile, SOUNDFXDIR);
	strcat(loadfile, "UI.bnk");
#ifdef HW_PTR_64
    loadbankAndConvertTo64Bit(loadfile, (void**)&UIBank);
#else
    fileLoadAlloc(loadfile, (void**)&UIBank, NonVolatile);
#endif
	if (soundbankadd(UIBank) != UIEventsLUT->checksum)
	{
		dbgMessage("Lookup tables do not match.  Not from same generate.");
		dbgAssertOrIgnore(FALSE);
	}
}


#ifdef USING_SDL1
/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void soundEventPlayCD(uword tracknum)
{
    Uint32 flags, i;

    /* Initialize CD playback if necessary. */
    flags = SDL_WasInit(SDL_INIT_CDROM);
    if (!flags)
    {
        if (SDL_Init(SDL_INIT_CDROM) == -1)
            return;
    }
    if (!(flags & SDL_INIT_CDROM))
    {
        if (SDL_InitSubSystem(SDL_INIT_CDROM) == -1)
            return;
    }

    /* Check the current CD status. */
    if (gCD)
    {
        if (!CD_INDRIVE(SDL_CDStatus(gCD)))
        {
            SDL_CDClose(gCD);
            gCD = NULL;
        }
        else if (gCD->status != CD_STOPPED)
        {
            SDL_CDStop(gCD);
        }
    }

    if (!gCD)
    {
        /* Cycle through each drive. */
        flags = SDL_CDNumDrives();
        for (i = 0; i < flags; i++)
        {
            if (!(gCD = SDL_CDOpen(i)))
                continue;
            if (!CD_INDRIVE(SDL_CDStatus(gCD)))
            {
                SDL_CDClose(gCD);
                gCD = NULL;
                continue;
            }
            break;
        }
        if (i >= flags)
            return;
    }

    /* Play the track. */
    if (tracknum >= gCD->numtracks)
        return;
    if (SDL_CDPlayTracks(gCD, tracknum, 0, 1, 0) == -1)
    {
        SDL_CDClose(gCD);
        gCD = NULL;
    }
}


void soundEventStopCD(void)
{
    CDstatus status;

    if (!gCD)
        return;
    status = SDL_CDStatus(gCD);
    if (status == CD_PLAYING || status == CD_PAUSED)
        SDL_CDStop(gCD);
    SDL_CDClose(gCD);
    gCD = NULL;
}
#endif /* USING_SDL1 */


void soundEventPlayMusic(sdword tracknum)
{
#ifndef _MACOSX_FIX_SOUND
    musicEventPlay(tracknum);
#endif

    return;
}


void soundEventStopMusic(real32 fadetime)
{
#ifndef _MACOSX_FIX_SOUND
    musicEventStop(-1, fadetime);
#endif
    return;
}


void soundEventStopTrack(sdword tracknum, real32 fadetime)
{
#ifndef _MACOSX_FIX_SOUND
    musicEventStop(tracknum, fadetime);
#endif
    return;
}


////**********  Panning Angle code  ************/////

/*-----------------------------------------------------------------------------
    Name        : getPanAngle
    Description : calculates the angle between the passed vector and the cameras
                lookat vector (projected onto the cameras 2D horizontal plane).

                            0|
                             |
                             |
                -90 --------------------90
                             |
                             |
                             |
                         -179/179

    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

sword getPanAngle(vector WorldVector, real32 objsize, real32 distance)
{
    vector CameraToPoint;
    vector CameraRightVec;
    vector CameraLookAt;
    vector ProjOnRight;
    vector ProjOnLookAt;
    vector Projection;
    real32 Prright;
    real32 Prlook;
    real32 dotprod;
    real32 angle;

    if (distance < (objsize * 2.0f))
	{
		return ((sword)SOUND_PAN_CENTER);
	}
	
	vecSub(CameraToPoint, WorldVector, mrCamera->eyeposition);
    vecSub(CameraLookAt, mrCamera->lookatpoint, mrCamera->eyeposition);

    //if NIS isn't playing...we know upvector is 0,0,1 so we can simplify cross product a grreat
    // deal
    vecCrossProduct(CameraRightVec,CameraLookAt,mrCamera->upvector);

    vecNormalize(&CameraLookAt);
    Prlook = vecDotProduct(CameraLookAt,CameraToPoint);
    vecScalarMultiply(ProjOnLookAt,CameraLookAt,Prlook);

    Prright = vecDotProduct(CameraRightVec,CameraToPoint)/vecMagnitudeSquared(CameraRightVec);
    vecScalarMultiply(ProjOnRight,CameraRightVec,Prright);

    vecAdd(Projection,ProjOnLookAt,ProjOnRight);

    vecNormalize(&Projection);
    dotprod = vecDotProduct(Projection,CameraLookAt);
    if(dotprod <-1.0f)
        dotprod = -1.0f;
    else if(dotprod > 1.0f)
        dotprod = 1.0f;
    angle = (float) acos(dotprod);

    if(Prright < 0.0f)
       angle = -angle;

    // CAT: rounded with Relic's rint (getPanAngle 0x005206f0; HW1 truncated)
    return((sword)fqRint(angle*PI_UNDER_180));
}


sword SEgetAngleToCamera(Ship *ship)
{
    vector ShipToCamera;
    vector ShipDest;
    vector heading;
    vector head;
    vector up;
    real32 dotprod;
    real32 angle;

    matGetVectFromMatrixCol3(head,ship->rotatable.rotinfo.coordsys);
    matGetVectFromMatrixCol1(up,ship->rotatable.rotinfo.coordsys);

    vecAdd(heading,ship->obj.posinfo.position,head);

    vecSub(ShipToCamera, mrCamera->eyeposition, ship->obj.posinfo.position);
    vecSub(ShipDest, heading, ship->obj.posinfo.position);

    vecNormalize(&ShipToCamera);
    vecNormalize(&ShipDest);

    dotprod = vecDotProduct(ShipToCamera,ShipDest);

    angle = (float) acos(dotprod);

    // CAT: rounded with Relic's rint (SEgetAngleToCamera 0x005208e0; HW1 truncated)
    return((sword)fqRint(angle*PI_UNDER_180));
}


