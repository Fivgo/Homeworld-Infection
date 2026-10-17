/*=============================================================================
    Name    : SpeechEvent.c
    Purpose : Speech Event processor

    Created 10/01/1998 by salfreds
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "SpeechEvent.h"

//#include "bink.h"
#include "Battle.h"          // CAT: the chatter table (sound self-test)
#include "Blobs.h"
#include "Chatting.h"
#include "File.h"
#include "fquant.h"
#include "mainrgn.h"
#include "Leech.h"          // CAT: cat_leechIsLeeching / cat_leechGetSpecifics (the leech alert)
#include "Alliance.h"       // CAT: allianceArePlayersAllied (the stealth alerts)
#include "ShipSelect.h"     // CAT: cat_mimicAppearsAlliedTo, cat_mimicGetApparentShipType
#include "Sensors.h"        // smSensorsActive
#include "fqcodec.h"        // CAT: fqRint
#include "Memory.h"
#include "NIS.h"
#include "Randy.h"
#include "Select.h"
#include "SinglePlayer.h"
#include "soundcmn.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "SoundEventPrivate.h"
#include "SoundMusic.h"
#include "Switches.h"
#include "Types.h"
#include "UnivUpdate.h"
#include "utility.h"

#ifdef _MSC_VER
    #define strncasecmp strnicmp
#else
    #include <strings.h>
#endif


#define STATE_BREAK     3

#define MAX_BATTLES     20

#define Asteroids       0
#define Nebulae         1
#define GasClouds       2
#define DustClouds      3

#define STINGER_FADE_TIME   0.5f
#define MUSIC_FADE_FAST     0.5f
#define MUSIC_FADE_SLOW     1.0f

#define MUSIC_MIN_FADE  80

// CAT: Cataclysm's two speech EQ slots: eq_fleet.script and eq_intel.script (speechEventInit 0x00521a40 fills
//      0x00c097c0 and 0x00c097e8, 0x28 bytes apart); HW1 had five speakers with EQ, delay and effect scripts
#define SPEECH_FLEET        0
#define SPEECH_INTEL        1
#define NUM_SPEECH_EQS      2

#define SE_MAX_QUEUE        10

#define SPEECH_PAUSE        1.5f        // CAT: the exe's gap between two speech events (0x0072a7ec)

// CAT: the Relic player names of the alliance and player-death lines: 25 names of 0x12 characters (.data 0x00895d9c,
//      compared with _strnicmp by speechEventQueue 0x00522560)
#define CAT_RELIC_NAMES         25
#define CAT_RELIC_NAME_LEN      0x12

// CAT: the music (Music4Cat.wxh): 62 tracks, one stream (speechEventInit 0x00521a40, musicEventPlay 0x00523480,
//      musicEventUpdateVolume 0x00523660)
#define CAT_MUSIC_NUM_TRACKS        62
#define CAT_MUSIC_RANDOM_TRACK      0x44e   // musicEventPlay: any of the first 17 tracks
#define CAT_MUSIC_RANDOM_RANGE      0x11
#define CAT_MUSIC_TUTORIAL          0x12    // in the main screen at MUSIC_TUTORIAL_VOL
#define CAT_MUSIC_FIRST_BATTLE      0x13    // tracks below are the ambients
#define CAT_MUSIC_FIRST_FIXED       0x19    // from here the main screen plays a track at its own level
#define CAT_MUSIC_GAME_STATS        0x28    // the game-statistics screen's track (cat_gameStatsBegin plays it): pilot
                                            //   lines over it, its own level in the managers
#define CAT_MUSIC_NIS_VOLUME        0x29    // the one track set by the NIS volume (HW1: RollCredits)
#define CAT_MUSIC_TYPE_NIS          4       // track type whose stop also stops the NIS speech (musicEventStop)

#define CAT_SPEECH_ALERT_INTERVAL   60.0f   // cat_speechAlertsUpdate 0x00522ce0 (0x0072a554)
#define CAT_SPEECH_EASTEREGG_TIME   540.0   // speechQueueUpdate 0x00521eb0 (0x0072bce0, a double)
#define CAT_SPEECH_EASTEREGG_EVENT  0x20012d1

/*=============================================================================
    Structures
=============================================================================*/
typedef struct
{
    sdword      offset;
    sword       flags;
    sword       bitrate;
    ubyte       volume;
    ubyte       numchannels;
    sword       pad;
} MUSICSTREAM;

typedef struct
{
    sdword      ID;
    udword      checksum;
    sdword      numstreams;
    MUSICSTREAM mstreams[];
} MUSICHEADER;

#if 1
typedef struct
{
    sdword      handle;     // this is the handle for this event (might not be unique if linked)
    sdword      index;      // this is the index of linked events
    sdword      status;

    sdword      actornum;   // need this for linked speech events

    sdword      numlinks;


    Ship        *pShip;     // ship pointer associated with this event
    sdword      event;      // actual event, listed in speechevent.h
    sdword      variable;   // optional variable for this event
    sdword      variation;  // overriding variation to play
    uword       priority;   // priority of this event
    sword       volume;     // overriding volume

    real32      timein;     // at what time was this event put in the queue?
    real32      timeout;    // at what point is this going to timeout at?
} QUEUEEVENT;

typedef struct
{
    QUEUEEVENT  queue[SE_MAX_QUEUE];    // these are the queued events
    QUEUEEVENT  current;    // the event that is currently playing

    sdword      status;     // status of this speech channel
    sdword      nextevent;  // index into the queue of the next event

    bool32        locked;     // set when deleting a bunch of stuff so another event doesn't play

    real32      timeover;   // what time will this event be over (approx)

    sdword      numqueued;

    sdword      lastpriority;
    sdword      laststarttick;
    sdword      lastendtick;
    sdword      lastevent;
    Ship        *lastship;
} SPEECHQUEUE;

typedef struct
{
    sdword      handle;
    sdword      status;
    sdword      tracknum;
    sdword      nexttrack;
    sword       targetvol;
    sword       pad;
    sdword      cat14;      // CAT: +0x14, cleared by speechEventInit 0x00521a40 and read nowhere
} MUSICINFO;

// CAT: the per-track table speechEventInit 0x00521a40 builds (0x00c09f00, 8 bytes a track): a type and a 0.5 s fade
typedef struct
{
    sdword      type;
    real32      fadetime;
} CATMUSICTRACK;
#endif


/*=============================================================================
    Prototypes
=============================================================================*/

void speechPlayQueue(SPEECHQUEUE *pSQueue, sdword streamchannel);
sdword SEselectsentence(sdword actor, sdword event, sdword variable, sdword setvariation, sdword **pOffsets, udword *pDuration);
void speechQueueUpdate(void);
sdword SEreorderqueue(SPEECHQUEUE *pSQueue);


/*=============================================================================
    Private Data
=============================================================================*/

SPEECHQUEUE speechqueue[SE_NUM_ACTORS];
sdword  streamhandle[SE_NUM_ACTORS];
smemsize  speechfilehandle;

sdword  numSinglePlayerEvents;

bool32    MoShipMoving[MAX_MULTIPLAYER_PLAYERS] = {FALSE};

ubyte   *pspeechstream;

// CAT: three speech stream buffers (speechEventInit 0x00521a40: 0x00a6ae6c, 0x00a6ae70, 0x00a6ae74)
ubyte   *pspeechbuffer0;
ubyte   *pspeechbuffer1;
ubyte   *pspeechbuffer2;

Ship    *lastshiptospeak = NULL;
sdword  lastgrouptospeak = -1;

/* speech lookup tables */
SENTENCELUT *SentenceLUT;
PHRASELUT   *PhraseLUT;

/* special fx vars */
// CAT: only the EQ of the two speech scripts; no delay, effect or damage scripts (speechEventInit 0x00521a40)
STREAMEQ    streamEQ[NUM_SPEECH_EQS];


/* music variables */
// CAT: one music stream (musicinfo 0x00c0a100, buffer 0x00a6ae8c, header 0x00a6ae90, file 0x00c09efc)
MUSICINFO   musicinfo;
smemsize  musicfilehandle;
ubyte   *pmusicbuffer0;
MUSICHEADER *musicheader;
CATMUSICTRACK catMusicTrack[CAT_MUSIC_NUM_TRACKS];

// CAT: the letterbox flag (0x00a6ae94, cat_soundEventLetterboxVolume 0x00521770): while set, chatter and status
//      speech are not queued, queued ones play quieter and the music is turned down
bool32  catSoundLetterbox = FALSE;
static real32 catSoundLetterboxSFXVol = 10.0f;      // .data 0x00895d98: the SFX level saved by the letterbox (10 = none)

// CAT: the speech alerts for enemy stealth ships (cat_speechAlertsNoteShip 0x00522bc0, cat_speechAlertsUpdate 0x00522ce0)
bool32  catSpeechAlertCloak = FALSE;                // 0x00c09820
bool32  catSpeechAlertMimic = FALSE;                // 0x00c09824
bool32  catSpeechAlertLeech = FALSE;                // 0x00c09814
real32  catSpeechAlertTime = 0.0f;                  // 0x00c09828

// CAT: the Easter eggs (cat_speechEasterEggCheck 0x00522dc0): a player named "Eegah" gets one line after 9 minutes;
//      a Somtaaw player named "PrinceOfSpace" changes a battle-chatter line (battlePingEvaluate)
bool32  catSpeechEasterEgg = FALSE;                 // 0x00c0a0f0
bool32  catSpeechEasterEggSaid = FALSE;             // 0x00c09ee8
bool32  catSpeechPrinceOfSpace = FALSE;             // 0x00c0981c

// CAT: the pilot lines over track 0x28 (musicEventUpdateVolume 0x00523660: 0x00a6aea4 / 0x00a6aea8 / 0x00a6aeac)
static sdword catMusicRadioCount = 0;
static sdword catMusicRadioNext = 0;
static sdword catMusicRadioLast = 0;

// CAT: Cataclysm's Relic player names (.data 0x00895d9c, 25 x 0x12; HW1 had 33 others)
static char catRelicPlayerNames[CAT_RELIC_NAMES][CAT_RELIC_NAME_LEN] = {
    "RogueTurnip",
    "Ayanami",
    "Katsuragi",
    "Ikari",
    "Kusanagi",
    "RandomTask",
    "DragLine",
    "ZugZug",
    "BeastGirl",
    "Ripley",
    "Shadow",
    "DarkLord",
    "Hobbes",
    "Sarmel",
    "Jaysus",
    "BlondBoy",
    "Terraformer",
    "ChoclateChipDonky",
    "Eegah",
    "Castewar",
    "Mecron",
    "Peanuts",
    "Egon",
    "Sabitz",
    "GayShadow"
};

/* name of music header file */
#ifdef HW_GAME_DEMO
char musicHeaderName[] = "DL_Music.wxh";
#elif defined(HW_GAME_RAIDER_RETREAT)
char musicHeaderName[] = "OEM_Music.wxh";
#else
// CAT: Cataclysm's music header (speechEventInit 0x00521a40, .data 0x00895f60 "Music4Cat.wxh")
char musicHeaderName[] = "Music4Cat.wxh";
#endif

/* externs */
extern bool32 soundpaused;

/* global volume settings */
extern real32 volSpeech;
extern real32 volMusic;
extern real32 volSFX;
extern real32 volNIS;
extern bool32 bVolMusicNoFade;

/* global speech settings */
extern bool32 bActorOn[];
extern bool32 bCommandsOn;
extern bool32 bStatusOn;
extern bool32 bChatterOn;

extern sdword actorFlagsEnabled;

extern real32 nearestShipDistance;
extern real32 musicVolFactor;
extern real32 battleMusicVolFactor;
extern real32 musicInactiveFactor;
extern bool32 nearestShipMoving;

extern bool32 FalkosFuckedUpTutorialFlag;

/*=============================================================================
    Scripts
=============================================================================*/

scriptStructEntry streamEQScriptTable[] =
{
    { "flags", scriptSetSdwordCB, &streamEQ[0].flags, &streamEQ[0] },
    { "eq0", scriptSetReal32CB,   &streamEQ[0].eq[0], &streamEQ[0] },
    { "eq1", scriptSetReal32CB,   &streamEQ[0].eq[1], &streamEQ[0] },
    { "eq2", scriptSetReal32CB,   &streamEQ[0].eq[2], &streamEQ[0] },
    { "eq3", scriptSetReal32CB,   &streamEQ[0].eq[3], &streamEQ[0] },
    { "eq4", scriptSetReal32CB,   &streamEQ[0].eq[4], &streamEQ[0] },
    { "eq5", scriptSetReal32CB,   &streamEQ[0].eq[5], &streamEQ[0] },
    { "eq6", scriptSetReal32CB,   &streamEQ[0].eq[6], &streamEQ[0] },
    { "eq7", scriptSetReal32CB,   &streamEQ[0].eq[7], &streamEQ[0] },

    END_SCRIPT_STRUCT_ENTRY
};


/*=============================================================================
    Functions
=============================================================================*/
void SEloadscripts(void)
{
#ifdef _WIN32
#define SFX_SCRIPT_PATH "SoundFX\\Scripts\\"
#else
#define SFX_SCRIPT_PATH "SoundFX/Scripts/"
#endif

    // CAT: Cataclysm loads only the fleet and intel EQ scripts (the only speech scripts it ships) into its two EQ
    //      slots and copies band 7 into the unused band 8 (speechEventInit 0x00521a40, SEloadscripts inlined:
    //      scriptSetStruct of "eq_fleet.script" / "eq_intel.script" with the table at 0x00895f70 into 0x00c097c0 /
    //      0x00c097e8, then eq[8] = eq[7] for both). No pilot EQ, delay or effect scripts.
    scriptSetStruct(SFX_SCRIPT_PATH, "eq_fleet.script", streamEQScriptTable, (ubyte *)&streamEQ[SPEECH_FLEET]);
    scriptSetStruct(SFX_SCRIPT_PATH, "eq_intel.script", streamEQScriptTable, (ubyte *)&streamEQ[SPEECH_INTEL]);
    streamEQ[SPEECH_FLEET].eq[8] = streamEQ[SPEECH_FLEET].eq[7];
    streamEQ[SPEECH_INTEL].eq[8] = streamEQ[SPEECH_INTEL].eq[7];

#undef SFX_SCRIPT_PATH
}


void soundEventDebugPrint(char *pszInformation)
{
#if SE_VERBOSE_LEVEL >= 1
    dbgMessagef("%s", pszInformation);
#endif
}


/*-----------------------------------------------------------------------------
    Name        : speechEventInit
    Description : CAT: the exe's start-up of the speech and music streams (speechEventInit 0x00521a40). The race's
                  tables and voice file are loaded before (cat_speechEventLoadRaceVoice 0x005217f0); this creates
                  the 4 streams (3 speech streams at the LUT's bitrate, 1 music stream at 0x200 bits a block), loads
                  Music4Cat.wxh and opens Music4Cat.wxd, builds the music track table, loads the two EQ scripts,
                  resets the 3 speech queues and starts the stream thread. HW1's 5 speech + 2 music streams, the
                  effect scripts and the demo-build LUT load are gone.
    Inputs      :
    Outputs     :
    Return      : SOUND_OK, or SOUND_ERR when the streamer has no buffers
----------------------------------------------------------------------------*/
sdword speechEventInit(void)
{
    sdword buffersize;
    sdword streamersize;
    char loadfile[100];
    sdword i, j;

    soundstreamquery(SE_NUM_STREAMS, &buffersize, &streamersize);

    if (buffersize == 0)
    {
        /* soundstreamquery failed */
        return (SOUND_ERR);
    }

    pspeechstream = memAlloc(streamersize, "StreamStructures", NonVolatile);

    soundstreaminit(pspeechstream, streamersize, SE_NUM_STREAMS, soundEventDebugPrint);

    // every speech stream uses the LUT's single bitrate at +0x0E
    pspeechbuffer0 = memAlloc(buffersize, "SpeechBuffer0", NonVolatile);
        streamhandle[0] = soundstreamcreatebuffer(pspeechbuffer0, buffersize, SentenceLUT->compbitrate[0]);
    pspeechbuffer1 = memAlloc(buffersize, "SpeechBuffer1", NonVolatile);
        streamhandle[1] = soundstreamcreatebuffer(pspeechbuffer1, buffersize, SentenceLUT->compbitrate[0]);
    pspeechbuffer2 = memAlloc(buffersize, "SpeechBuffer2", NonVolatile);
        streamhandle[2] = soundstreamcreatebuffer(pspeechbuffer2, buffersize, SentenceLUT->compbitrate[0]);

    /* music stuff */
    /* load the music header file */
    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile,musicHeaderName);
    fileLoadAlloc(loadfile, (void**)&musicheader, NonVolatile);

#if FIX_ENDIAN
	musicheader->ID = FIX_ENDIAN_INT_32( musicheader->ID );
	musicheader->checksum = FIX_ENDIAN_INT_32( musicheader->checksum );
	musicheader->numstreams = FIX_ENDIAN_INT_32( musicheader->numstreams );
#endif

    /* open music file and compare checksums */
    strcpy(loadfile,utyMusicFilename);
    if (soundstreamopenfile(loadfile, &musicfilehandle) != musicheader->checksum)
    {
        dbgMessage("Music data file does not match lookup table.  Not from same generate.");
        dbgAssertOrIgnore(FALSE);
    }

    // CAT: one music stream at 0x200 bits a block (FQ_BR44)
    pmusicbuffer0 = memAlloc(buffersize, "MusicBuffer", NonVolatile);
        musicinfo.handle = soundstreamcreatebuffer(pmusicbuffer0, buffersize, (uword)FQ_BR44);

    musicinfo.status = SOUND_FREE;
    musicinfo.tracknum = SOUND_NOTINITED;
    musicinfo.nexttrack = SOUND_NOTINITED;
    musicinfo.targetvol = SOUND_DEFAULT;
    musicinfo.cat14 = 0;

    // CAT: the track types: 0..0x10 and 0x12 type 5 (ambients), 0x11 type 2, 0x13..0x18 type 6 (battle),
    //      0x19..0x29 type 4 (NIS: stopping one stops the NIS speech), 0x2a..0x3d type 3; every fade 0.5 s
    for (i = 0; i < CAT_MUSIC_NUM_TRACKS; i++)
    {
        if (i < 0x11 || i == 0x12)
        {
            catMusicTrack[i].type = 5;
        }
        else if (i == 0x11)
        {
            catMusicTrack[i].type = 2;
        }
        else if (i < 0x19)
        {
            catMusicTrack[i].type = 6;
        }
        else if (i < 0x2a)
        {
            catMusicTrack[i].type = 4;
        }
        else
        {
            catMusicTrack[i].type = 3;
        }
        catMusicTrack[i].fadetime = 0.5f;
    }

    SEloadscripts();

    for (i = 0; i < SE_NUM_ACTORS; i++)
    {
        memset(&speechqueue[i], 0, sizeof(SPEECHQUEUE));
        speechqueue[i].current.handle = SOUND_NOTINITED;
        speechqueue[i].status = SOUND_STOPPED;
        speechqueue[i].nextevent = SOUND_NOTINITED;
        speechqueue[i].locked = FALSE;
        for (j = 0; j < SE_MAX_QUEUE; j++)
        {
            speechqueue[i].queue[j].handle = SOUND_NOTINITED;
            speechqueue[i].queue[j].variation = SOUND_NOTINITED;
        }
    }

    numSinglePlayerEvents = 0;

    // CAT: the stream thread starts here, once the streams exist (speechEventInit 0x00521a40 ends with
    //      streamStartThread; HW1's soundstreaminit started it)
    streamStartThread();

    return (SOUND_OK);
}


#ifndef HW_GAME_DEMO
// CAT: TRUE once speechEventInit has created the streams (.data 0x00a6ae98, cat_speechEventInitForRace 0x00521d50)
static bool32 catSpeechStreamsInitialised = FALSE;

/*-----------------------------------------------------------------------------
    Name        : cat_speechEventLoadRaceVoice
    Description : CAT: load the speech lookup tables of one race and open its voice file
                  (cat_speechEventLoadRaceVoice 0x005217f0). Race 0 (Somtaaw/Sect) uses
                  SoundFX\Sectsentence.lut, SoundFX\Sectphrase.lut and SectSpeech.vce; any other race uses the Beast
                  set (SoundFX\Beastsentence.lut, SoundFX\Beastphrase.lut, BeastSpeech.vce), as the exe does.
    Inputs      : race - player race
    Outputs     : SentenceLUT, PhraseLUT, speechfilehandle
    Return      : 0
----------------------------------------------------------------------------*/
sdword cat_speechEventLoadRaceVoice(sdword race)
{
    char loadfile[100];
    char *raceName = (race == 0) ? "Sect" : "Beast";        // .data 0x00896144 "Sect", 0x0089613c "Beast"
#if FIX_ENDIAN
    sdword p, z;
#endif

    if (SentenceLUT != NULL)
    {
        memFree(SentenceLUT);
        SentenceLUT = NULL;
    }
    if (PhraseLUT != NULL)
    {
        memFree(PhraseLUT);
        PhraseLUT = NULL;
    }
    if (speechfilehandle != 0)
    {
        fileClose(speechfilehandle);
        speechfilehandle = 0;
    }

    /* load the speech lookup tables */
    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, raceName);
    strcat(loadfile, "sentence.lut");                       // .data 0x0089612c
    fileLoadAlloc(loadfile, (void**)&SentenceLUT, NonVolatile);

#if FIX_ENDIAN
	SentenceLUT->ID         = FIX_ENDIAN_INT_32( SentenceLUT->ID );
	SentenceLUT->checksum   = FIX_ENDIAN_INT_32( SentenceLUT->checksum );
	SentenceLUT->numcolumns = FIX_ENDIAN_INT_16( SentenceLUT->numcolumns );
	SentenceLUT->numevents  = FIX_ENDIAN_INT_16( SentenceLUT->numevents );
	SentenceLUT->numactors  = FIX_ENDIAN_INT_16( SentenceLUT->numactors );
	SentenceLUT->compbitrate[0] = FIX_ENDIAN_INT_16( SentenceLUT->compbitrate[0] );
	for ( z=0; z< (SentenceLUT->numcolumns*(SentenceLUT->numactors*SentenceLUT->numevents)); z++){
		SentenceLUT->lookup[z] = FIX_ENDIAN_INT_16( SentenceLUT->lookup[z] );
	}
#endif // FIX_ENDIAN

    strcpy(loadfile, SOUNDFXDIR);
    strcat(loadfile, raceName);
    strcat(loadfile, "phrase.lut");                         // .data 0x008960fc
    fileLoadAlloc(loadfile, (void**)&PhraseLUT, NonVolatile);

#if FIX_ENDIAN
	PhraseLUT->ID = FIX_ENDIAN_INT_32( PhraseLUT->ID );
	PhraseLUT->checksum = FIX_ENDIAN_INT_32( PhraseLUT->checksum );
	PhraseLUT->numcolumns = FIX_ENDIAN_INT_16( PhraseLUT->numcolumns );
	PhraseLUT->numsentences = FIX_ENDIAN_INT_16( PhraseLUT->numsentences );

	for ( p=0; p< (PhraseLUT->numcolumns*PhraseLUT->numsentences); p++){
		PhraseLUT->lookupsy[p] = FIX_ENDIAN_INT_16( PhraseLUT->lookupsy[p] );
	}
#endif

    /* verify sentence and phrase luts */
    if (SentenceLUT->checksum != PhraseLUT->checksum)
    {
        dbgMessage("Sentence and Phrase lookup tables do not match.  Not from same generate.");
        dbgAssertOrIgnore(FALSE);
    }

    /* open the race's speech file and compare checksums */
    strcpy(loadfile, (race == 0) ? utyVoiceFilename : cat_utyBeastVoiceFilename);
    if (soundstreamopenfile(loadfile, &speechfilehandle) != SentenceLUT->checksum)
    {
        dbgMessage("Voice file does not match lookup tables.  Not from same generate.");
        dbgAssertOrIgnore(FALSE);
    }

    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : cat_speechEventInitForRace
    Description : CAT: speech start-up for one race (cat_speechEventInitForRace 0x00521d50, called by
                  soundEventInit 0x0051c6e0): load the race's tables and voice file, then create the streams the
                  first time only.
    Inputs      : race - player race (soundEventInit passes whichRaceSelected)
    Outputs     :
    Return      : SOUND_OK, or SOUND_ERR with the tables freed
----------------------------------------------------------------------------*/
sdword cat_speechEventInitForRace(sdword race)
{
    sdword result;

    result = cat_speechEventLoadRaceVoice(race);            // frees the previous race's tables and file itself
    if (result == 0)
    {
        if (catSpeechStreamsInitialised)
        {
            return SOUND_OK;
        }
        result = speechEventInit();
    }
    if (result != SOUND_ERR)
    {
        catSpeechStreamsInitialised = TRUE;
        return SOUND_OK;
    }

    if (SentenceLUT != NULL)
    {
        memFree(SentenceLUT);
        SentenceLUT = NULL;
    }
    if (PhraseLUT != NULL)
    {
        memFree(PhraseLUT);
        PhraseLUT = NULL;
    }
    if (speechfilehandle != 0)
    {
        fileClose(speechfilehandle);
        speechfilehandle = 0;
    }
    return SOUND_ERR;
}
#endif // HW_GAME_DEMO


/*-----------------------------------------------------------------------------
    Name        : speechEventClose
    Description : CAT: frees the 8 buffers of the exe (speechEventClose 0x00521e30): the music buffer and header,
                  the stream structures, the 3 speech buffers and the two LUTs
----------------------------------------------------------------------------*/
void speechEventClose(void)
{
    memFree(pmusicbuffer0);
    memFree(musicheader);

    memFree(pspeechstream);
    memFree(pspeechbuffer0);
    memFree(pspeechbuffer1);
    memFree(pspeechbuffer2);
    memFree(SentenceLUT);
    memFree(PhraseLUT);
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void speechEventUpdate(void)
{
    /* must call this so the streamer gets updated */
    speechQueueUpdate();

    // CAT: then the stealth-ship alerts (speechEventUpdate 0x00521ea0)
    cat_speechAlertsUpdate();
}


/*-----------------------------------------------------------------------------
    Name        :
    Description :   this function updates the queues, starts playing new events,
                    reorders and cleans up the queue
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
#if 1
void speechQueueUpdate(void)
{
    sdword  i;
    SPEECHQUEUE *pSQueue;

    for (i = 0; i < SE_NUM_ACTORS; i++)
    {
        pSQueue = &speechqueue[i];

        /* is this channel playing something? */
        if ((pSQueue->status >= SOUND_PLAYING) && soundstreamover(streamhandle[i]))
        {
            /* was it a single player event? */
            if (pSQueue->current.event & SPEECH_TYPE_SINGLE_PLAYER)
            {
                numSinglePlayerEvents--;
            }
            if (pSQueue->nextevent >= SOUND_OK)
            {
                if ((pSQueue->current.event == pSQueue->queue[pSQueue->nextevent].event) &&
                    (pSQueue->current.variable == pSQueue->queue[pSQueue->nextevent].variable))
                {
                    memset(&pSQueue->queue[pSQueue->nextevent], 0, sizeof(QUEUEEVENT));
                    pSQueue->queue[pSQueue->nextevent].handle = SOUND_NOTINITED;
                    pSQueue->queue[pSQueue->nextevent].timeout = -1.0f;
                    pSQueue->queue[pSQueue->nextevent].event = SOUND_DEFAULT;     // CAT
                }
            }
            pSQueue->current.event = SOUND_DEFAULT;
            pSQueue->status = SOUND_STOPPED;
            // CAT: the gap is not counted from the end of the line (speechQueueUpdate 0x00521eb0 clears timeover;
            //      it is set to the start time plus the event's timeout below)
            pSQueue->timeover = 0.0f;
        }

        /* are we ready to play something? */
        // CAT: the exe's test also starts a line at once while a Bink movie plays (speechQueueUpdate 0x00521eb0:
        //      || !binkDonePlaying, 0x008b6d94; the tutorial flag is 0x00a6c2c0). This build has no Bink player
        //      (Win32/bink.c is not built; binkDonePlaying would stay TRUE), so that term is always FALSE here.
        //      CAT-TODO(M2-R6 front end: bink.c): add it back with the movie player.
        if ((pSQueue->status == SOUND_STOPPED) && (pSQueue->nextevent >= SOUND_OK) && (!pSQueue->locked) &&
            (((universe.totaltimeelapsed - pSQueue->timeover) >= SPEECH_PAUSE) || !gameIsRunning || FalkosFuckedUpTutorialFlag))
        {
            // CAT: timeover = now + the event's timeout
            pSQueue->timeover = universe.totaltimeelapsed + pSQueue->queue[pSQueue->nextevent].timeout;

            /* remove it from the queue */
            memcpy(&pSQueue->current, &pSQueue->queue[pSQueue->nextevent], sizeof(QUEUEEVENT));

            memset(&pSQueue->queue[pSQueue->nextevent], 0, sizeof(QUEUEEVENT));
            pSQueue->queue[pSQueue->nextevent].handle = SOUND_NOTINITED;
            pSQueue->queue[pSQueue->nextevent].timeout = -1.0f;
            pSQueue->queue[pSQueue->nextevent].event = SOUND_DEFAULT;             // CAT

            /* play this one */
            speechPlayQueue(pSQueue, i);
            if (pSQueue->current.event & SPEECH_TYPE_SINGLE_PLAYER)
            {
                numSinglePlayerEvents++;
            }
            SEreorderqueue(pSQueue);

            pSQueue->status = SOUND_PLAYING;
        }
    }

    // CAT: the "Eegah" Easter egg: one line 9 minutes into the game (speechQueueUpdate 0x00521eb0)
    if (catSpeechEasterEgg && (universe.totaltimeelapsed > (real32)CAT_SPEECH_EASTEREGG_TIME) && !catSpeechEasterEggSaid)
    {
        catSpeechEasterEggSaid = TRUE;
        speechEventQueue(NULL, CAT_SPEECH_EASTEREGG_EVENT, -1, -1, 0, -1, -1, 5.0f, -1);
    }
}


/*-----------------------------------------------------------------------------
    Name        : speechPlayQueue
    Description : CAT: the exe's speech player (speechPlayQueue 0x005220e0): the sentence of the current event goes
                  to the stream at the speech volume, with the intel EQ, no effects, no radio beeps and no
                  background patch. HW1's pilot effects (damage noise, breaks, tones), delays and per-actor EQ are
                  gone.
    Inputs      : pSQueue - the queue whose current event plays, streamchannel - its stream
----------------------------------------------------------------------------*/
void speechPlayQueue(SPEECHQUEUE *pSQueue, sdword streamchannel)
{
    sdword i;
    sdword numoffsets;
    udword duration;
    sdword *phraseoffsets = NULL;
    real32 vol;
    sword level;
    sword bitrate;
    sdword event;

    event = pSQueue->current.event;
    if (event == SOUND_DEFAULT)
    {
        return;
    }

    // the LUT has a single bitrate (*(sword *)(SentenceLUT + 0xe))
    bitrate = SentenceLUT->compbitrate[0];

    /* get the sentence */
    numoffsets = SEselectsentence(pSQueue->current.actornum, (event & CAT_SPEECH_EVENT_MASK),
                                  pSQueue->current.variable, pSQueue->current.variation, &phraseoffsets, &duration);

    /* scale by the front end volume settings */
    vol = volSpeech * (real32)SOUND_VOL_MAX;

    /* if this is not a single player event and there is a single player event playing then turn it down */
    if ((numSinglePlayerEvents > 0) && !(event & SPEECH_TYPE_SINGLE_PLAYER))
    {
        vol *= SPEECH_SINGLEPLAYER_RATIO;
    }
    else if (event & CAT_SPEECH_ACTOR_PILOT)
    {
        vol *= 0.75f;                                       // CAT: the pilots a little quieter (0x0072a670)
    }

    // CAT: during the letterbox, chatter and status lines play quieter still
    if (catSoundLetterbox && (event & (SPEECH_TYPE_CHATTER | SPEECH_TYPE_STATUS)))
    {
        vol *= SPEECH_SINGLEPLAYER_RATIO;
    }

    /* scale the background ambience thats mixed into the speech */
    level = (sword)fqRint(vol * SPEECH_AMBIENT_LEVEL);

    /* queue the offsets */
    if ((numoffsets > 0) && (vol >= SPEECH_MIN_PERCEPTABLE_VOL))
    {
        // CAT: every phrase but the last waits for the next one; the volume of those is truncated (__ftol), the
        //      last one's rounded (Relic's rint), as in the exe
        for (i = 0; i < numoffsets - 1; i++)
        {
            soundstreamqueuePatch(streamhandle[streamchannel], speechfilehandle, *phraseoffsets++, SOUND_FLAGS_QUEUESTREAM, (sword)vol, SOUND_PAN_CENTER,
                                  SOUND_MONO, bitrate, NULL, &streamEQ[SPEECH_INTEL], NULL, NULL, level, 0.0f, 0.0f, pSQueue->current.actornum, event, TRUE);
        }

        soundstreamqueuePatch(streamhandle[streamchannel], speechfilehandle, *phraseoffsets++, SOUND_FLAGS_QUEUESTREAM, (sword)fqRint(vol), SOUND_PAN_CENTER,
                              SOUND_MONO, bitrate, NULL, &streamEQ[SPEECH_INTEL], NULL, NULL, level, 0.0f, 0.0f, pSQueue->current.actornum, event, FALSE);

        /* figure out the last ship to speak var */
        if (pSQueue->current.pShip != NULL)
        {
            /* straight forward, this was just a ship */
            lastshiptospeak = pSQueue->current.pShip;
        }
        else if (event & SPEECH_TYPE_GROUP)
        {
            lastshiptospeak = (Ship *)-1;
            lastgrouptospeak = pSQueue->current.variable;
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : SERandomSequenceCreate
    Description : Creates a number which is a random BCD sequence numVariations
                    in length containing all numbers between 0 and numVarations
                    once and only once.
    Inputs      : numVariations - number of variations to encode.
                  lookupsy - the lookup table which contains probabilities
                  width - width of the lookup table
    Outputs     :
    Return      : BCD-encoded random series
----------------------------------------------------------------------------*/
udword SERandomSequenceCreate(sdword numVariations, sdword *lookupsy, sdword width)
{
    sdword index, whichIndex;
    udword sequence[16];
    ubyte probs[16], accumProb = 0;
    udword finalSequence;

    dbgAssertOrIgnore(numVariations <= 16);                         //can only fit 16 BCD's in a DWORD
    //make an order 0, 1, 2, 3, ... and a list of matching probabilities
    for (index = 0; index < numVariations; index++)
    {
        sequence[index] = index;
        accumProb += probs[index] = ((lookupinterp *)(lookupsy))->probability - accumProb;
        lookupsy += width;
    }
    //partially bubble sort this list (this will favor the most common variations first)
    for (index = 0; index < numVariations - 1; index++)     //!!! might want to bubble sort a different number of times
    {
        whichIndex = ranRandom(RANDOM_SOUND_BOTH_THREADS) % (numVariations - 1);
        if (probs[whichIndex] <= probs[whichIndex + 1])
        {
            swap(sequence[whichIndex], sequence[whichIndex + 1], finalSequence);
            swap(probs[whichIndex], probs[whichIndex + 1], accumProb);
        }
    }
    //create the final sequence DWORD
    finalSequence = 0;
    for (index = numVariations - 1; index >= 0; index--)
    {
        finalSequence <<= 4;                                //free up a new BCD digit
        finalSequence = (finalSequence & 0xfffffff0) | (sequence[index] + 1);//stick in the new digit
    }
    return(finalSequence);                                  //done
}

/*-----------------------------------------------------------------------------
    Name        : SENextVariationInSeries
    Description : Select a variation of a speech event based on a random sequence
    Inputs      : numVariations - how many variations of this event
                  lookupsy - the lookup table
                  width - width of the lookup table
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sdword SENextVariationInSeries(sdword numVariations, sdword *lookupsy, sdword width)
{
    lookupinterp *interp = (lookupinterp *)(lookupsy);
    udword sequence;
    udword iVariation;

#if defined(HW_GAME_DEMO)
    if (numVariations > 0xf)
    {                                                       //could be the case if this actor is disabled
        return(0);
    }
#endif
    dbgAssertOrIgnore(numVariations < 0xf);
    //get the random sequence into a dword
    sequence = (interp->high[1] << 24) + (interp->high[0] << 16) + (interp->low[1] << 8) + (interp->low[0]);

    if (sequence == 0)
    {                                                       //if no numbers left in sequence (tried all variations)
        sequence = SERandomSequenceCreate(numVariations, lookupsy, width);//start a new sequence
    }
    iVariation = (sequence & 0xf) - 1;                      //get a new variation
    sequence >>= 4;                                         //shift off those 4 bits
    dbgAssertOrIgnore(iVariation < numVariations);
    interp->high[1] = sequence >> 24;                       //put the sequence back in the high words
    interp->high[0] = (sequence >> 16) & 0xff;
    interp->low[1] = (sequence >> 8) & 0xff;
    interp->low[0] = (sequence) & 0xff;

#if SE_VERBOSE_LEVEL >= 1
    dbgMessagef("Selected variation %d of %d for event at 0x%x of width %d", iVariation, numVariations, lookupsy, width);
#endif

    return(iVariation);                                     //return the variation we've found
}

/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :   actor - which actor?
                    event - which event?
                    variable - value of the variable for this event
                    setvariation - select a particular variation, -1 is random
    Outputs     :   pOffsets - sets a pointer to the first offset for the sentence, NULL if there is an error
                    pDuration - sets the approximate duration of this sentence
    Return      :   the number of offsets in this sentence, -1 if there is an error
----------------------------------------------------------------------------*/
sdword SEselectsentence(sdword actor, sdword event, sdword variable, sdword setvariation, sdword **pOffsets, udword *pDuration)
{
    uword numvariations;
    uword maxvariable;
    uword index;
    udword numoffsets;
    udword i;
    udword phrase;
    sword *pSentence;
    sdword *pPhrase;
    udword variation;

    pSentence = &SentenceLUT->lookup[(actor * SentenceLUT->numevents + event) * SentenceLUT->numcolumns + 1];   // + 1 is to get past the priority

    numvariations = *pSentence++;

    if (numvariations == 0)
    {
        pOffsets = NULL;
        return (SOUND_ERR);
    }

    maxvariable = *pSentence++;
    index = *pSentence;

    if (variable < SOUND_OK)
    {                                                       //no variable specified
        variable = ranRandom(RANDOM_SOUND_BOTH_THREADS) % maxvariable;//!!! no heuristic on this one
    }
    else if (variable >= maxvariable)
    {                                                       //variable out of range
        pOffsets = NULL;
        return (SOUND_ERR);
    }

    if ((setvariation != SOUND_NOTINITED) && (setvariation < numvariations))
    {                                                       //if variation specified
        variation = setvariation;

        phrase = index + (variation * maxvariable) + variable;
    }
    else if (numvariations == 1)
    {                                                       //not spec'd but only 1 choice
        phrase = index + variable;
    }
    else
    {                                                       //no variation spec'd, choose one intelligently
        // CAT: always the random series (SEselectsentence 0x005223c0; HW1's mid- and background-chatter
        //      rotations of N01_Chatter_Mid / N01_Chatter_BG are gone with those events)
        i = SENextVariationInSeries(numvariations, &PhraseLUT->lookupsy[(index + variable) * PhraseLUT->numcolumns], maxvariable * PhraseLUT->numcolumns);

        phrase = index + (i * maxvariable) + variable;
    }

    pPhrase = &PhraseLUT->lookupsy[phrase * PhraseLUT->numcolumns];

    *pDuration = *pPhrase++;
    pPhrase++;  // skip  the probability
    numoffsets = (*pPhrase++) & 0xff;

    *pOffsets = pPhrase;

    if (numoffsets > 0)
    {
        for (i = 0; i < numoffsets; i++)
        {
            if (*(pPhrase + i) == SOUND_NOTINITED)
            {
                // bad, this won't work
                pOffsets = NULL;
                numoffsets = 0;
                break;
            }
        }
    }

    return (numoffsets);
}


/*-----------------------------------------------------------------------------
    Name        :   speechEventQueue
    Description :   Adds speech events to the queue if there is space for them.  Does trivial rejection on
                    low priority speech events.
                    CAT: the exe's queue (speechEventQueue 0x00522560): Cataclysm's ids (index 0x7ff up to 0x35e,
                    actor bits 0x3ff800), three queues chosen by the actor bits, the letterbox rejecting chatter
                    and status, a mimic disguised as an allied ship answering the enemy's selection with its
                    apparent type, the 25 Relic names; every event is actor 0 of the race's LUT.
    Inputs      :   object - pointer to a ship object that this speech event is attached to (can be NULL for Fleet Command, etc)
                    event - event # to play (Cataclysm's raw id)
                    var - variable that may be attached to an event (ie a build ship event would pass the ship variable #)
                    variation - only used if you want a specific variation of an event to play
                    actornum - not used by Cataclysm (every event is actor 0 of the LUT)
                    playernum - the playerindex in the array player in the universe structure, only want to hear your own speech events
                    linkto - handle of the speech event that you want to tie this one to, will play this event directly after that event
                    timeout - amount of time this event can wait in the queue, 0 means play now, < 0 means wait indefinatly and don't drop this from the queue
                    volume - volume at which to play this event, if you want to override the code that figures that out
    Outputs     :
    Return      :   handle to this speech event, can be used to stop the event or link another event to this one
                    or SOUND_ERR (-1) if there is an error or the event can't be queued
----------------------------------------------------------------------------*/
sdword speechEventQueue(void *object, sdword event, sdword var, sdword variation, sdword actornum, sdword playernum, sdword linkto, real32 timeout, sword volume)
{
    sdword i, j;
    Ship *pShip = NULL;
    sdword handle;
    SPEECHQUEUE *pSQueue;
    QUEUEEVENT *pQEvent;
    SpaceObj *spaceobject;
    sdword channel;
    sdword actorbits;
    sdword ally;

    (void)actornum;

    if (!enableSpeech || soundpaused)
    {
        return (SOUND_ERR);
    }

    if ((event & CAT_SPEECH_EVENT_MASK) > CAT_SPEECH_LAST_EVENT)
    {
        return (SOUND_ERR);
    }

    if (SentenceLUT->numactors == 0)
    {
        return (SOUND_ERR);
    }

    /* if an NIS is playing, only want to hear NIS events */
    if (nisIsRunning && !(event & (SPEECH_TYPE_NIS + SPEECH_TYPE_ANIMATIC)))
    {
        return (SOUND_ERR);
    }

    /* if single player events are happening, don't want any chatter */
    if ((numSinglePlayerEvents > 0) && (event & SPEECH_TYPE_CHATTER))
    {
        return (SOUND_ERR);
    }

    // CAT: no chatter or status speech during the letterbox
    if (catSoundLetterbox && (event & (SPEECH_TYPE_CHATTER | SPEECH_TYPE_STATUS)))
    {
        return (SOUND_ERR);
    }

    /* toss out events the user doesn't want to hear */
    if ((event & SPEECH_TYPE_CHATTER) && !bChatterOn)
    {
        return (SOUND_ERR);
    }
    if ((event & SPEECH_TYPE_STATUS) && !bStatusOn)
    {
        return (SOUND_ERR);
    }
    if ((event & SPEECH_TYPE_COMMAND) && !bCommandsOn)
    {
        return (SOUND_ERR);
    }

    if (!((event & CAT_SPEECH_ACTOR_MASK) & actorFlagsEnabled))
    {
        return (SOUND_ERR);
    }

    if (object != NULL)
    {
        spaceobject = (SpaceObj *)object;

        if (!(event & CAT_SPEECH_ACTOR_FLEET_COMMAND))
        {
            /* this is probably a ship, so this is one of the ALL SHIPS voices */
            if (!univSpaceObjInRenderList(spaceobject) && ((event & SPEECH_TYPE_STATUS) || (event & SPEECH_TYPE_CHATTER)))
            {
                if (!(event & SPEECH_TYPE_ALWAYS_PLAY))
                {
                    return (SOUND_ERR);        // only do sound events for objects in current mission sphere we are viewing
                }
            }

            if (spaceobject->header.objtype != OBJ_ShipType)
            {
                return (SOUND_ERR);
            }

            pShip = (Ship *)object;

            if ((universe.curPlayerIndex == pShip->playerowner->playerIndex) || nisIsRunning)
            {
                if (event != CAT_SPEECH_EVENT_SELECTED)
                {
                    goto queueit;
                }
            }
            else
            {
                // CAT: an enemy ship speaks only to answer its selection, and only a mimic that looks allied,
                //      with the type it pretends to be (the +0x18 word's CAT_SHIPFLAG_Mimic bit)
                if (event != CAT_SPEECH_EVENT_SELECTED)
                {
                    return (SOUND_ERR);
                }
                if (!bitTest(pShip->header.catFlags18, CAT_SHIPFLAG_Mimic))
                {
                    return (SOUND_ERR);
                }
                if (!cat_mimicAppearsAlliedTo(pShip, universe.curPlayerPtr))
                {
                    return (SOUND_ERR);
                }
                var = cat_mimicGetApparentShipType(pShip);
            }

            // CAT: a selected ship that is support-unit exempt or still being built says nothing (specialFlags2
            //      +0x1CC: 0x100000, 0x8)
            if (bitTest(pShip->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20) || bitTest(pShip->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
            {
                return (SOUND_ERR);
            }
            goto queueit;
        }

        if (spaceobject->header.objtype == OBJ_ShipType)
        {
            pShip = (Ship *)object;
        }
    }

    if (((udword)playernum != universe.curPlayerIndex) && (!nisIsRunning) && (playernum != SOUND_EVENT_DEFAULT))
    {
        return (SOUND_ERR);
    }

queueit:
    /* figure out which speech channel to use */
    // CAT: stream 1 for actor bits 0x1000 and 0x2000, stream 2 for chatter and actor 0x200000, stream 0 otherwise;
    //      a linked event goes to the stream of its handle
    actorbits = event & CAT_SPEECH_ACTOR_MASK;
    if ((actorbits == CAT_SPEECH_ACTOR_FLEET_COMMAND) || (actorbits == CAT_SPEECH_ACTOR_FLEET_INTEL))
    {
        channel = 1;
    }
    else if ((event & SPEECH_TYPE_CHATTER) || (actorbits == CAT_SPEECH_ACTOR_200000))
    {
        channel = 2;
    }
    else
    {
        channel = 0;
    }
    if (linkto >= SOUND_OK)
    {
        channel = linkto & 0xf;
    }
    pSQueue = &speechqueue[channel];

    /* is the queue full? */
    if (pSQueue->numqueued == SE_MAX_QUEUE)
    {
        return (SOUND_ERR);
    }

    /* is this going to time out before it'll play? (the relic-player death line is always queued) */
    if ((timeout > 0.0f) && (pSQueue->timeover > universe.totaltimeelapsed + timeout) && (event != 0x200102d))
    {
        return (SOUND_ERR);
    }

    if (var == SOUND_DEFAULT)
    {
        var = 0;
    }

    if ((pSQueue->current.event == event) && (pSQueue->current.variable == var))
    {
        return (SOUND_ERR);
    }

    /* special relic users events */
    // CAT: alliance formed / broken (0x400102b / 0x400102c) with a Relic name become 0x200102e / 0x200102f with the
    //      name's index; the Relic player death (0x200102d) needs a Relic name
    if ((event == 0x400102b) || (event == 0x400102c))
    {
        ally = var;
        var = 0;

        for (i = 0; i < CAT_RELIC_NAMES; i++)
        {
            if (strncasecmp(playerNames[ally], catRelicPlayerNames[i], CAT_RELIC_NAME_LEN) == 0)
            {
                event = (event != 0x400102b) + 0x200102e;
                var = i;
                break;
            }
        }
    }
    else if (event == 0x200102d)
    {
        for (i = 0; i < CAT_RELIC_NAMES; i++)
        {
            if (strncasecmp(playerNames[var], catRelicPlayerNames[i], CAT_RELIC_NAME_LEN) == 0)
            {
                break;
            }
        }
        if (i == CAT_RELIC_NAMES)
        {
            return (SOUND_ERR);
        }
        var = i;
    }

    for (i = 0; i < SE_MAX_QUEUE; i++)
    {
        if (pSQueue->queue[i].handle == SOUND_NOTINITED)
        {
            break;
        }
    }
    if (i == SE_MAX_QUEUE)
    {
        return (SOUND_ERR);
    }
    pQEvent = &pSQueue->queue[i];

    /* put it in this space */
    if (linkto < SOUND_OK)
    {
        handle = SNDcreatehandle(channel);
        pQEvent->index = 0;
    }
    else
    {
        handle = linkto;
        for (j = 0; j < SE_MAX_QUEUE; j++)
        {
            if ((pSQueue->queue[j].handle == linkto) && (pSQueue->queue[j].index == 0))
            {
                break;
            }
        }
        if (j < SE_MAX_QUEUE)
        {
            pQEvent->index = pSQueue->queue[j].numlinks++;
        }
        else if (pSQueue->current.handle == linkto)
        {
            // CAT: the event it links to may be playing already (speechEventQueue 0x00522560)
            pQEvent->index = pSQueue->current.numlinks++;
        }
        else
        {
            /* the one you wanted to link to is gone */
            return (SOUND_ERR);
        }
    }

    pQEvent->status = SOUND_OK;
    pQEvent->actornum = 0;                                  // CAT: one actor per race LUT

    if (event & SPEECH_TYPE_GROUP)
    {
        pQEvent->pShip = NULL;
    }
    else
    {
        pQEvent->pShip = pShip;
    }
    pQEvent->event = event;
    pQEvent->variable = (var < 0x3c) ? var : 0x3c;          // CAT: the variable is capped at 60
    pQEvent->variation = variation;
    pQEvent->priority = SentenceLUT->lookup[(event & CAT_SPEECH_EVENT_MASK) * SentenceLUT->numcolumns];
    pQEvent->volume = volume;

    pQEvent->timein = universe.totaltimeelapsed;
    if (timeout < 0.0f)
    {
        pQEvent->timeout = timeout;
    }
    else
    {
        pQEvent->timeout = universe.totaltimeelapsed + timeout;
    }

    pQEvent->handle = handle;

    SEreorderqueue(pSQueue);

    return (handle);
}


/*-----------------------------------------------------------------------------
    Name        :   speechEventStop
    Description :   stops a speech event that is currently playing and deletes any
                    linked speech events from the queue.
    Inputs      :   handle - the handle for this event that was return by speechEventQueue
                    fadetime - the amount of time to fade out this speech event (in seconds)
                    addstatic - add static noise ass the speech fades out
    Outputs     :
    Return      :   SOUND_OK (0) if successful, SOUND_ERR (-1) if not
----------------------------------------------------------------------------*/
sdword speechEventStop(sdword handle, real32 fadetime, bool32 addstatic)
{
    /* is this event still playing? */
    /* are there any events linked to this one in the queue? */
    return (SOUND_OK);
}


/*-----------------------------------------------------------------------------
    Name        : speechEventActorStop
    Description : Stops all speech events from a particular actor.  Queued
                    events are discarded; playing events faded out.
                  CAT: in all three queues (speechEventActorStop 0x00522a00; HW1 only the first)
    Inputs      : actorMask - bit fields for what actors' speech events to
                    delete.  Same as bit fields added to speech events.
                  fadeTime - amount of time to fade out over.
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sdword speechEventActorStop(udword actorMask, real32 fadetime)
{
    sdword i, j;

    for (i = 0; i < SE_NUM_ACTORS; i++)
    {
        for (j = 0; j < SE_MAX_QUEUE; j++)
        {
            if (speechqueue[i].queue[j].event & actorMask)
            {
                memset(&speechqueue[i].queue[j], 0, sizeof(QUEUEEVENT));
                speechqueue[i].queue[j].handle = SOUND_NOTINITED;
                speechqueue[i].queue[j].timeout = -1.0f;
                speechqueue[i].queue[j].event = SOUND_DEFAULT;
            }
        }

        if (speechqueue[i].current.event & actorMask)
        {
            soundstreamvolume(speechqueue[i].current.handle, -1, fadetime);
        }
    }

    return (SOUND_OK);
}


sdword speechEventStopAllSpecific(real32 fadetime, sdword speechType)
{
    sdword i, j;

    for (i = 0; i < SE_NUM_ACTORS; i++)
    {
        speechqueue[i].locked = TRUE;
        speechqueue[i].timeover = 0.0f;

        for (j = 0; j < SE_MAX_QUEUE; j++)
        {
            if (speechqueue[i].queue[j].event & speechType)
            {
                memset(&speechqueue[i].queue[j], 0, sizeof(QUEUEEVENT));
                speechqueue[i].queue[j].handle = SOUND_NOTINITED;
                speechqueue[i].queue[j].timeout = -1.0f;
                speechqueue[i].queue[j].event = SOUND_DEFAULT;    // CAT (speechEventStopAllSpecific 0x00522a70)
            }
        }

        /* need to stop the currently playing one */
        if (speechqueue[i].current.event & speechType)
        {
            soundstreamvolume(speechqueue[i].current.handle, -1, fadetime);
        }

        speechqueue[i].locked = FALSE;
    }

    return (SOUND_OK);
}


sdword speechEventRemoveShip(Ship *pShip)
{
    sdword i, j;

    if (pShip == lastshiptospeak)
    {
        lastshiptospeak = NULL;
    }

    for (i = 0; i < SE_NUM_ACTORS; i++)
    {
        speechqueue[i].locked = TRUE;

        for (j = 0; j < SE_MAX_QUEUE; j++)
        {
            if (speechqueue[i].queue[j].pShip == pShip)
            {
                memset(&speechqueue[i].queue[j], 0, sizeof(QUEUEEVENT));
                speechqueue[i].queue[j].handle = SOUND_NOTINITED;
                speechqueue[i].queue[j].timeout = -1.0f;
                speechqueue[i].queue[j].pShip = NULL;
                speechqueue[i].queue[j].event = SOUND_DEFAULT;    // CAT (speechEventRemoveShip 0x00522b00)
            }
        }

        /* need to stop the currently playing one */
        if (speechqueue[i].current.pShip == pShip)
        {
            // CAT: the exe passes a fade time of -1.0 (HW1: 100)
            soundstreamvolume(speechqueue[i].current.handle, -1, -1.0f);
            speechqueue[i].current.pShip = NULL;
        }

        speechqueue[i].locked = FALSE;
    }

    return (SOUND_OK);
}


/*-----------------------------------------------------------------------------
    Name        : SEreorderqueue
    Description : CAT: drops the timed-out events, then picks the next event (SEreorderqueue 0x00522e60, with HW1's
                  SEcleanqueue inlined). Cataclysm orders and keeps events by the top four type bits (0xf0000000):
                  a timed-out tutorial event is dropped too (HW1 kept tutorial, single-player, NIS and animatic).
----------------------------------------------------------------------------*/
sdword SEreorderqueue(SPEECHQUEUE *pSQueue)
{
    sdword i;
    sdword highpriority = 0;
    bool32 bSomethingQueued = FALSE;
    udword ti, th;

    for (i = 0; i < SE_MAX_QUEUE; i++)
    {
        if ((pSQueue->queue[i].timeout >= 0.0f) && (pSQueue->queue[i].timeout < universe.totaltimeelapsed) &&
            (((udword)pSQueue->queue[i].event & CAT_SPEECH_TYPE_ORDER_MASK) < SPEECH_TYPE_TUTORIAL))
        {
            memset(&pSQueue->queue[i], 0, sizeof(QUEUEEVENT));
            pSQueue->queue[i].handle = SOUND_NOTINITED;
            pSQueue->queue[i].timeout = -1.0f;
            pSQueue->queue[i].event = SOUND_DEFAULT;
        }
    }

    for (i = 0; i < SE_MAX_QUEUE; i++)
    {
        if (pSQueue->queue[i].handle >= SOUND_OK)
        {
            bSomethingQueued = TRUE;
            ti = (udword)pSQueue->queue[i].event & CAT_SPEECH_TYPE_ORDER_MASK;
            th = (udword)pSQueue->queue[highpriority].event & CAT_SPEECH_TYPE_ORDER_MASK;
            if (pSQueue->queue[highpriority].handle < SOUND_OK)
            {
                highpriority = i;
            }
            else if (ti > th)
            {
                highpriority = i;
            }
            else if (pSQueue->queue[i].priority > pSQueue->queue[highpriority].priority)
            {
                highpriority = i;
            }
            else if (pSQueue->queue[i].priority == pSQueue->queue[highpriority].priority)
            {
                if (pSQueue->queue[i].timein < pSQueue->queue[highpriority].timein)
                {
                    highpriority = i;
                }
            }
        }
    }

    if (bSomethingQueued)
    {
        pSQueue->nextevent = highpriority;
    }
    else
    {
        pSQueue->nextevent = SOUND_NOTINITED;
    }

    return (SOUND_OK);
}


sdword speechEventCleanup(void)
{
    sdword i, j;

    numSinglePlayerEvents = 0;
    for (i = 0; i < SE_NUM_ACTORS; i++)
    {
        for (j = 0; j < SE_MAX_QUEUE; j++)
        {
            memset(&speechqueue[i].queue[j], 0, sizeof(QUEUEEVENT));
            speechqueue[i].queue[j].handle = SOUND_NOTINITED;
            speechqueue[i].queue[j].timeout = -1.0f;
        }

        if (speechqueue[i].current.handle > SOUND_NOTINITED)
        {
            soundstreamvolume(speechqueue[i].current.handle, -1, 0.5f);
        }

        memset(&speechqueue[i].current, 0, sizeof(QUEUEEVENT));
        speechqueue[i].current.handle = SOUND_NOTINITED;
        speechqueue[i].status = SOUND_STOPPED;
        speechqueue[i].nextevent = SOUND_NOTINITED;
        speechqueue[i].timeover = 0.0f;
        speechqueue[i].numqueued = 0;
    }
    return (SOUND_OK);
}

#endif


/*-----------------------------------------------------------------------------
    Name        : speechEventAttack
    Description : CAT: the attack order's line (speechEventAttack 0x00523040): 0x4000807 when a forced attack
                  includes own ships, 0x4000808 for resources only, 0x4000805 otherwise, with the leader's type;
                  HW1's random refusal of a forced attack on own ships and its per-resource lines are gone.
    Return      : TRUE
----------------------------------------------------------------------------*/
bool32 speechEventAttack(void)
{
    sdword objtype,
            i;
    Ship *ship;
    sdword  numFriendlies = 0,
            numEnemies = 0,
            numResources = 0;
    sdword event = 0x4000805;
    static bool32 lastreturn;

    if (keyIsHit(SHIFTKEY))
    {
        if (selSelecting.numTargets < 1)
        {
            lastreturn = TRUE;
            return (lastreturn);
        }

        for (i = 0; i < selSelecting.numTargets; i++)
        {
            objtype = selSelecting.TargetPtr[i]->header.objtype;
            if (objtype == OBJ_ShipType)
            {
                ship = (Ship *)selSelecting.TargetPtr[i];
                if (ship->playerowner == universe.curPlayerPtr)
                {
                    numFriendlies++;
                }
                else
                {
                    numEnemies++;
                }
            }
            else if ((objtype >= OBJ_AsteroidType) && (objtype <= OBJ_DustType))
            {
                numResources++;
            }
        }

        if (numFriendlies > 0)
        {
            event = 0x4000807;
        }
        else if (numEnemies < 1)
        {
            if (numResources < 1)
            {
                lastreturn = TRUE;
                return (lastreturn);
            }
            event = 0x4000808;
        }
    }

    speechEventQueue(selSelected.ShipPtr[0], event, selSelected.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);

    lastreturn = TRUE;
    return (lastreturn);
}


/*-----------------------------------------------------------------------------
    Name        : speechEventUnderAttack
    Description : CAT: the exe's damage warnings (speechEventUnderAttack 0x00523190). The mothership class and the
                  component class warn through 0x2001058 (not the Dreadnought cannon or the carrier support
                  module); off screen, the carrier class and the heavy cruisers through 0x2001066 and a hot-key
                  group through 0x200105b; on screen, the carrier class, the Workers and the heavy cruisers below
                  half health say 0x100080b themselves. Support-unit exempt ships say nothing.
    Inputs      : target - the ship hit
----------------------------------------------------------------------------*/
void speechEventUnderAttack(Ship *target)
{
    sdword shipclass, shiptype, group;

    if ((target->playerowner != universe.curPlayerPtr) || bitTest(target->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
    {
        return;
    }

    shipclass = target->staticinfo->shipclass;
    shiptype = target->shiptype;

    if ((shipclass == CLASS_Mothership) || (shipclass == CLASS_Component))
    {
        if ((shiptype != sDreadnoughtCannon) && (shiptype != sCarrierSupport) &&
            (universe.totaltimeelapsed > (universe.curPlayerPtr->timeMoShipAttacked + SPEECH_MOSHIP_WARNING_TIME)))
        {
            speechEventQueue(target, 0x2001058, 0, -1, 0, target->playerowner->playerIndex, -1, 5.0f, -1);
            universe.curPlayerPtr->timeMoShipAttacked = universe.totaltimeelapsed;
        }
        return;
    }

    if (universe.totaltimeelapsed <= (target->soundevent.timeLastStatus + SPEECH_WARNING_TIME))
    {
        return;
    }

    if (!univSpaceObjInRenderList((SpaceObj *)target))
    {
        /* global warnings */
        if ((shipclass == CLASS_Carrier) || (shiptype == kHeavyCruiser) || (shiptype == tHeavyCruiser) ||
            (shiptype == bHeavyCruiser))
        {
            speechEventQueue(NULL, 0x2001066, shiptype, -1, 0, target->playerowner->playerIndex, -1, 5.0f, -1);
            target->soundevent.timeLastStatus = universe.totaltimeelapsed;
            return;
        }
        if (selAnyHotKeyTest(target))
        {
            group = selHotKeyGroupNumberTest(target);
            if (universe.totaltimeelapsed > (selHotKeyGroup[group].timeLastStatus + SPEECH_WARNING_TIME))
            {
                speechEventQueue(NULL, 0x200105b, group, -1, 0, target->playerowner->playerIndex, -1, 5.0f, -1);
                selHotKeyGroup[group].timeLastStatus = universe.totaltimeelapsed;
            }
        }
    }
    else if ((shipclass == CLASS_Carrier) || (shiptype == sWorker) || (shiptype == bWorker) ||
             (shiptype == kHeavyCruiser) || (shiptype == tHeavyCruiser) || (shiptype == bHeavyCruiser))
    {
        /* some local warnings: below half health */
        if (target->targetable.health < target->staticinfo->health.maxhealth * 0.5f)
        {
            speechEventQueue(target, 0x100080b, shiptype, -1, -1, -1, -1, 0.0f, -1);
            target->soundevent.timeLastStatus = universe.totaltimeelapsed;
        }
    }
    else if (selAnyHotKeyTest(target))
    {
        group = selHotKeyGroupNumberTest(target);
        if (universe.totaltimeelapsed > (selHotKeyGroup[group].timeLastStatus + SPEECH_WARNING_TIME))
        {
            // the exe passes no player here (-1)
            speechEventQueue(NULL, 0x200105b, group, -1, 0, -1, -1, 5.0f, -1);
            selHotKeyGroup[group].timeLastStatus = universe.totaltimeelapsed;
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : cat_musicQueueTrack
    Description : CAT: start a track on the music stream at its level (the start part of musicEventPlay 0x00523480
                  and of musicEventUpdateVolume's restart, 0x00523660): track 0x29 is set by the NIS volume, the
                  others by the music volume; the stream status becomes SOUND_STARTING.
----------------------------------------------------------------------------*/
static void cat_musicQueueTrack(sdword tracknum)
{
    MUSICSTREAM *pmstream = &musicheader->mstreams[tracknum];

    if (tracknum == CAT_MUSIC_NIS_VOLUME)
    {
        musicinfo.targetvol = (sword)fqRint((real32)pmstream->volume * volNIS);
    }
    else
    {
        musicinfo.targetvol = (sword)fqRint((real32)pmstream->volume * volMusic);
    }

    soundstreamqueuePatch(musicinfo.handle, musicfilehandle, pmstream->offset, pmstream->flags | SOUND_FLAGS_QUEUESTREAM,
                          musicinfo.targetvol, SOUND_PAN_CENTER, (sword)pmstream->numchannels, (sword)pmstream->bitrate,
                          NULL, NULL, NULL, NULL, 0, 0.0f, 0.0f, -1, -1, FALSE);

    musicinfo.status = SOUND_STARTING;
}


/*-----------------------------------------------------------------------------
    Name        : musicEventPlay
    Description : CAT: Cataclysm's single music stream (musicEventPlay 0x00523480): the track number indexes
                  Music4Cat.wxh directly; 0x44e picks one of the first 17 tracks at random; a playing track fades
                  out over 0.5 s first and the new one starts from musicEventUpdateVolume.
    Inputs      : tracknum - track
    Return      : SOUND_OK, or SOUND_ERR without speech or for a negative track
----------------------------------------------------------------------------*/
sdword musicEventPlay(sdword tracknum)
{
    if (!enableSpeech || (tracknum < 0))
    {
        return (SOUND_ERR);
    }

    if (tracknum == CAT_MUSIC_RANDOM_TRACK)
    {
        tracknum = ranRandom(RANDOM_STREAM_0) % CAT_MUSIC_RANDOM_RANGE;
    }

    if (!soundstreamover(musicinfo.handle))
    {
        // stop this track and start the next one
        musicinfo.status = SOUND_RESTART;
        musicinfo.targetvol = SOUND_VOL_AUTOSTOP;
        musicinfo.nexttrack = tracknum;
        soundstreamvolume(musicinfo.handle, SOUND_VOL_AUTOSTOP, MUSIC_FADE_FAST);
        return (SOUND_OK);
    }

    musicinfo.nexttrack = SOUND_NOTINITED;
    musicinfo.tracknum = tracknum;
    cat_musicQueueTrack(tracknum);

    return (SOUND_OK);
}


/*-----------------------------------------------------------------------------
    Name        : musicEventStop
    Description : CAT: stops the music stream's track (musicEventStop 0x005235c0); stopping an NIS-type track
                  also stops the NIS speech. HW1's NIS / ambient stream pair and the return to the level's track
                  are gone.
    Inputs      : tracknum - the track, or SOUND_DEFAULT for the current one; fadetime - fade
    Return      : SOUND_OK, or SOUND_ERR for a track that is not the current one
----------------------------------------------------------------------------*/
sdword musicEventStop(sdword tracknum, real32 fadetime)
{
    if (enableSpeech)
    {
        if ((tracknum == SOUND_DEFAULT) && ((musicinfo.status == SOUND_PLAYING) || (musicinfo.status == SOUND_STARTING)))
        {
            tracknum = musicinfo.tracknum;
        }

        if (tracknum != musicinfo.tracknum)
        {
            return (SOUND_ERR);
        }

        if ((musicinfo.status == SOUND_PLAYING) || (musicinfo.status == SOUND_STARTING))
        {
            if (catMusicTrack[musicinfo.tracknum].type == CAT_MUSIC_TYPE_NIS)
            {
                speechEventStopAllSpecific(1.0f, SPEECH_TYPE_NIS);
            }
            musicinfo.targetvol = SOUND_VOL_AUTOSTOP;
            soundstreamvolume(musicinfo.handle, SOUND_VOL_AUTOSTOP, fadetime);
            musicinfo.status = SOUND_STOPPING;
            musicinfo.tracknum = SOUND_NOTINITED;
        }
    }

    return (SOUND_OK);
}


/*-----------------------------------------------------------------------------
    Name        : musicEventUpdateVolume
    Description : CAT: the music stream's state machine (musicEventUpdateVolume 0x00523660): starting, playing at
                  a level set by the nearest ship's distance and the screen (main screen, sensors manager, other
                  managers), stopping, restarting with the next track; the letterbox turns the main-screen level
                  down to 70%; over track 0x28 a random pilot line now and then.
    Return      : SOUND_OK
----------------------------------------------------------------------------*/
sdword musicEventUpdateVolume(void)
{
    sword volume;
    sword base;
    real32 f;
    MUSICSTREAM *pmstream;
    sdword track, event, var;

    if (!enableSpeech)
    {
        return (SOUND_OK);
    }

    if (musicinfo.status == SOUND_STARTING)
    {
        if (soundstreamgetvol(musicinfo.handle) == musicinfo.targetvol)
        {
            musicinfo.status = SOUND_PLAYING;
        }
        else if ((!soundstreamfading(musicinfo.handle)) && (!soundstreamover(musicinfo.handle)))
        {
            soundstreamvolume(musicinfo.handle, musicinfo.targetvol, MUSIC_FADE_FAST);
        }
    }
    else if (musicinfo.status == SOUND_PLAYING)
    {
        if (soundstreamover(musicinfo.handle))
        {
            musicinfo.status = SOUND_FREE;
            musicEventPlay(musicinfo.tracknum);
            // CAT-FIX: the exe then sets the stream's volume to an uninitialised value (the word of its own return
            //      address at [esp+0xc], 0x0052371a) with the music fade; the restarted track keeps its start level here
        }
        else
        {
            track = musicinfo.tracknum;
            pmstream = &musicheader->mstreams[track];

            if (mrRenderMainScreen)
            {
                if (track == CAT_MUSIC_TUTORIAL)
                {
                    /* we're in the tutorial, play the music at a constant volume */
                    volume = (sword)fqRint((real32)pmstream->volume * volMusic * MUSIC_TUTORIAL_VOL);
                }
                else if (track >= CAT_MUSIC_FIRST_FIXED)
                {
                    volume = (sword)fqRint((real32)pmstream->volume * volMusic);
                }
                else
                {
                    /* we're in the game, volume depends on the distance from the nearst ship */
                    base = (sword)fqRint((real32)pmstream->volume * volMusic);

                    if (nearestShipDistance <= MUSIC_DISTANCE_SILENT)
                    {
                        if (track < CAT_MUSIC_FIRST_BATTLE)
                        {
                            f = (real32)base * (nearestShipMoving ? MUSIC_MINACTIVE_VOL : MUSIC_MININACTIVE_VOL);
                        }
                        else
                        {
                            f = (real32)base * MUSIC_MINBATTLE_VOL;
                        }
                    }
                    else if (nearestShipDistance <= MUSIC_DISTANCE_MAX)
                    {
                        if (track < CAT_MUSIC_FIRST_BATTLE)
                        {
                            if (nearestShipMoving)
                            {
                                f = ((nearestShipDistance - MUSIC_DISTANCE_SILENT) * musicVolFactor + MUSIC_MINACTIVE_VOL) * (real32)base;
                            }
                            else
                            {
                                f = ((nearestShipDistance - MUSIC_DISTANCE_SILENT) * musicInactiveFactor + MUSIC_MININACTIVE_VOL) * (real32)base;
                            }
                        }
                        else
                        {
                            f = ((nearestShipDistance - MUSIC_DISTANCE_SILENT) * battleMusicVolFactor + MUSIC_MINBATTLE_VOL) * (real32)base;
                        }
                    }
                    else
                    {
                        f = (real32)base * ((track < CAT_MUSIC_FIRST_BATTLE) ? MUSIC_MAXGAME_VOL : MUSIC_MAXBATTLE_VOL);
                    }

                    volume = (sword)fqRint(f);
                    if (volume > (sword)pmstream->volume)
                    {
                        volume = pmstream->volume;
                    }
                    else if (volume < 0)
                    {
                        volume = 0;
                    }
                }

                // CAT: the letterbox turns the music down (0x0072a628: 0.7)
                if (catSoundLetterbox)
                {
                    volume = (sword)fqRint((double)volume * 0.7);
                }
            }
            else if (smSensorsActive)
            {
                /* we're in the sensors manager, play the music at the volume defined in the script */
                if (track < CAT_MUSIC_FIRST_BATTLE)
                {
                    volume = (sword)((real32)pmstream->volume * MUSIC_SENSORS_VOL * volMusic);
                }
                else
                {
                    volume = (sword)((real32)pmstream->volume * MUSIC_MINBATTLE_VOL * volMusic);
                }
            }
            else
            {
                /* we're in one of the other manager screens */
                if (track < CAT_MUSIC_FIRST_BATTLE)
                {
                    volume = (sword)((real32)pmstream->volume * MUSIC_MANAGERS_VOL * volMusic);
                }
                else if (track == CAT_MUSIC_GAME_STATS)
                {
                    volume = (sword)fqRint((real32)pmstream->volume * volMusic);
                }
                else
                {
                    volume = (sword)((real32)pmstream->volume * MUSIC_MINBATTLE_VOL * volMusic);
                }
            }

            if (!bVolMusicNoFade)
            {
                soundstreamvolume(musicinfo.handle, volume, MUSIC_FADE_TIME);
            }
            else
            {
                soundstreamvolume(musicinfo.handle, volume, 0.0f);
                bVolMusicNoFade = FALSE;
            }
        }
    }
    else if ((musicinfo.status == SOUND_STOPPING) || (musicinfo.status == SOUND_RESTART))
    {
        if (soundstreamover(musicinfo.handle))
        {
            if (musicinfo.status == SOUND_STOPPING)
            {
                musicinfo.status = SOUND_FREE;
            }
            else
            {
                musicinfo.tracknum = musicinfo.nexttrack;
                musicinfo.nexttrack = SOUND_NOTINITED;
                cat_musicQueueTrack(musicinfo.tracknum);
            }
        }
        else if ((musicinfo.targetvol != SOUND_VOL_AUTOSTOP) ||
            (!soundstreamfading(musicinfo.handle) &&
            (soundstreamgetvol(musicinfo.handle) > SOUND_VOL_MIN)))
        {
            musicinfo.targetvol = SOUND_VOL_AUTOSTOP;
            soundstreamvolume(musicinfo.handle, SOUND_VOL_AUTOSTOP, SOUND_FADE_STOPTIME);
        }
    }

    // CAT: now and then a random pilot line over the game-statistics track 0x28 (one of 26 events, 0xa1 for the 17th;
    //      one of 19 variables, the Beast's 33..53), at 60% of the music volume
    if (musicinfo.tracknum == CAT_MUSIC_GAME_STATS)
    {
        catMusicRadioCount++;
        if (catMusicRadioCount > catMusicRadioNext + catMusicRadioLast)
        {
            catMusicRadioLast = catMusicRadioCount;
            catMusicRadioNext = ranRandom(RANDOM_STREAM_0) % 0x32 + 0x14;
            if (whichRaceSelected == 1)
            {
                var = ranRandom(RANDOM_STREAM_0) % 0x15 + 0x21;
            }
            else
            {
                var = ranRandom(RANDOM_STREAM_0) % 0x13;
            }
            event = ranRandom(RANDOM_STREAM_0) % 0x1a;
            if (event == 0x10)
            {
                event = 0xa1;
            }
            speechEventQueue(NULL, event | CAT_SPEECH_EVENT_SELECTED, var, -1, -1, -1, -1, -1.0f,
                             (sword)fqRint(volMusic * 0.6));
        }
    }

    return (SOUND_OK);
}


#if defined(HW_GAME_DEMO) || defined(HW_GAME_RAIDER_RETREAT)

void musicEventNextTrack(void)
{
    return;
}

void musicEventPrevTrack(void)
{
    return;
}
#else

// CAT: the next / previous music track key commands 0x60 / 0x61 (0x00550510 / 0x00550520) call these
//      (0x00523c50 / 0x00523ca0): HW1's bodies with Cataclysm's 0x19 non-fixed tracks
void musicEventNextTrack(void)
{
    if (!singlePlayerGame && enableSpeech)
    {
        musicinfo.nexttrack = musicinfo.tracknum + 1;
        if (musicinfo.nexttrack >= CAT_MUSIC_FIRST_FIXED)
        {
            musicinfo.nexttrack = 0;
        }
        musicinfo.status = SOUND_RESTART;
        soundstreamvolume(musicinfo.handle, SOUND_VOL_AUTOSTOP, MUSIC_FADE_FAST);
   }
}


void musicEventPrevTrack(void)
{
    if (!singlePlayerGame && enableSpeech)
    {
        musicinfo.nexttrack = musicinfo.tracknum - 1;
        if (musicinfo.nexttrack < 0)
        {
            musicinfo.nexttrack = CAT_MUSIC_FIRST_FIXED - 1;
        }
        musicinfo.status = SOUND_RESTART;
        soundstreamvolume(musicinfo.handle, SOUND_VOL_AUTOSTOP, MUSIC_FADE_FAST);
   }
}
#endif

/*-----------------------------------------------------------------------------
    Name        : cat_musicRadioReset
    Description : CAT: the pilot lines over track 0x28 start over (cat_gameStatsBegin 0x00489e90 writes the three
                  words inline after playing the track)
----------------------------------------------------------------------------*/
void cat_musicRadioReset(void)
{
    catMusicRadioCount = 0;
    catMusicRadioNext = 0x1e;
    catMusicRadioLast = 0;
}

sdword musicEventCurrentTrack(void)
{
    return (musicinfo.tracknum);
}


/*-----------------------------------------------------------------------------
    Name        : cat_speechAlertsReset
    Description : CAT: clears the three stealth-ship alerts and their timer (cat_speechAlertsReset 0x00522ba0)
----------------------------------------------------------------------------*/
void cat_speechAlertsReset(void)
{
    catSpeechAlertLeech = FALSE;
    catSpeechAlertCloak = FALSE;
    catSpeechAlertMimic = FALSE;
    catSpeechAlertTime = 0.0f;
}


/*-----------------------------------------------------------------------------
    Name        : cat_speechAlertsNoteShip
    Description : CAT: an enemy stealth ship was detected by the local player (cat_speechAlertsNoteShip 0x00522bc0,
                  from cat_shipStealthDetectedSet): the mimic types raise the mimic alert, a draining leech the
                  leech alert, the cloaking types the cloak alert.
    Inputs      : ship - the detected ship; playerIndex - the detecting player
----------------------------------------------------------------------------*/
void cat_speechAlertsNoteShip(Ship *ship, sdword playerIndex)
{

    if ((playerIndex != universe.curPlayerIndex) ||
        allianceArePlayersAllied(ship->playerowner, &universe.players[playerIndex]))
    {
        return;
    }

    switch (ship->shiptype)
    {
        case sMimic:
        case sMCV:
        case p1StandardCorvette:
            catSpeechAlertMimic = TRUE;
            break;
        case sLeech:
            if (cat_leechIsLeeching(ship) && (cat_leechGetSpecifics(ship)->target != NULL))   // the specifics' +0x04
            {
                catSpeechAlertLeech = TRUE;
            }
            break;
        case bCloakedFighter:
        case bIonArrayFrigate:
        case p1IonArrayFrigate:
            catSpeechAlertCloak = TRUE;
            break;
        default:
            break;
    }
}


/*-----------------------------------------------------------------------------
    Name        : cat_speechAlertsUpdate
    Description : CAT: says the raised alerts, at most once a minute (cat_speechAlertsUpdate 0x00522ce0, from
                  speechEventUpdate): cloaked ship 0x200104c, mimic 0x20010ba, leech 0x20010bb.
----------------------------------------------------------------------------*/
void cat_speechAlertsUpdate(void)
{
    if (catSpeechAlertTime + CAT_SPEECH_ALERT_INTERVAL < universe.totaltimeelapsed)
    {
        if (catSpeechAlertCloak)
        {
            speechEventQueue(NULL, 0x200104c, -1, -1, 0, -1, -1, 5.0f, -1);
            catSpeechAlertCloak = FALSE;
            catSpeechAlertTime = universe.totaltimeelapsed;
        }
        if (catSpeechAlertMimic)
        {
            speechEventQueue(NULL, 0x20010ba, -1, -1, 0, -1, -1, 5.0f, -1);
            catSpeechAlertMimic = FALSE;
            catSpeechAlertTime = universe.totaltimeelapsed;
        }
        if (catSpeechAlertLeech)
        {
            speechEventQueue(NULL, 0x20010bb, -1, -1, 0, -1, -1, 5.0f, -1);
            catSpeechAlertLeech = FALSE;
            catSpeechAlertTime = universe.totaltimeelapsed;
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : cat_speechEasterEggCheck
    Description : CAT: the player-name Easter eggs (cat_speechEasterEggCheck 0x00522dc0, from cat_soundLevelReset):
                  any player named "Eegah" (.data 0x008961a4) arms the 9-minute line of speechQueueUpdate; a local
                  player named "PrinceOfSpace" (0x00896194) playing the Somtaaw sets the battle-chatter variant.
----------------------------------------------------------------------------*/
void cat_speechEasterEggCheck(void)
{
    sdword i;

    catSpeechEasterEgg = FALSE;
    catSpeechEasterEggSaid = FALSE;
    catSpeechPrinceOfSpace = FALSE;

    for (i = 0; i < universe.numPlayers; i++)
    {
        if (strncasecmp(playerNames[i], "Eegah", CAT_RELIC_NAME_LEN) == 0)
        {
            catSpeechEasterEgg = TRUE;
        }
    }

    if ((strncasecmp(playerNames[universe.curPlayerIndex], "PrinceOfSpace", CAT_RELIC_NAME_LEN) == 0) &&
        (whichRaceSelected == 0))
    {
        catSpeechPrinceOfSpace = TRUE;
    }
}


/*-----------------------------------------------------------------------------
    Name        : cat_soundEventLetterboxVolume
    Description : CAT: the letterbox (the wide-screen KAS calls and the single-player update;
                  cat_soundEventLetterboxVolume 0x00521770): sets the flag the speech and music read and, in single
                  player, lowers the SFX volume to 60% (30% in mission 5) and restores it after
    Inputs      : on - TRUE when the letterbox comes in
----------------------------------------------------------------------------*/
void cat_soundEventLetterboxVolume(bool32 on)
{
    catSoundLetterbox = on;
    if (singlePlayerGame)
    {
        if (on)
        {
            catSoundLetterboxSFXVol = volSFX;
            // [audit: the exe multiplies by the doubles 0.3 / 0.6 (fmul qword 0x0072a790 / 0x0072a780, bytes dc 0d),
            //  then stores a float; a float 0.3f / 0.6f product differs by an ulp for about half the levels]
            if (singlePlayerGameInfo.currentMission == 5)
            {
                volSFX = (real32)((double)volSFX * 0.3);
            }
            else
            {
                volSFX = (real32)((double)volSFX * 0.6);
            }
            return;
        }
        if (catSoundLetterboxSFXVol < (real32)9.0)
        {
            volSFX = catSoundLetterboxSFXVol;
            catSoundLetterboxSFXVol = 10.0f;
        }
    }
}


/*=============================================================================
    CAT: port diagnostic, not in the exe: the sound self-test (/catSoundTest, harness mode sound; M2-R5 sound,
    docs/m2-r5-sound.md). After the start-up has opened the three audio banks (Music4Cat.wxd, SectSpeech.vce and
    BeastSpeech.vce), one stream of each is queued on the stream code as the game queues it and decoded by the mixer
    (fqDequantBlock / fqDecBlock / fqWriteTBlock) into the audio device, which the harness forces to SDL's dummy
    driver: nothing reaches the speakers. The 88 battle-chatter events are then looked up in the Somtaaw tables.
    Log lines "soundtest: ...", the last one "soundtest: done ... result=PASS|FAIL".
=============================================================================*/
bool32 catSoundSelfTest = FALSE;

extern volatile udword catSoundDecodedBlocks[];
extern volatile udword catSoundAudibleBlocks;
extern udword mixerticks;
extern battleevent battleChatterEvent[];

/*-----------------------------------------------------------------------------
    Name        : catSoundTestWaitStream
    Description : wait (on the wall clock: the mixer runs in SDL's audio thread) until the stream has decoded
                  minBlocks more blocks and finished, or timeoutMs passes
    Return      : blocks the mixer decoded on that stream while waiting
----------------------------------------------------------------------------*/
static udword catSoundTestWaitStream(sdword handle, udword minBlocks, udword timeoutMs)
{
    sdword channel = SNDchannel(handle);
    udword start, waited = 0;

    if ((channel < 0) || (channel >= SOUND_MAX_STREAM_BUFFERS))
    {
        return 0;
    }
    start = catSoundDecodedBlocks[channel];
    while (waited < timeoutMs)
    {
        SDL_Delay(10);
        waited += 10;
        if (((catSoundDecodedBlocks[channel] - start) >= minBlocks) && soundstreamover(handle))
        {
            break;
        }
    }
    return catSoundDecodedBlocks[channel] - start;
}

/*-----------------------------------------------------------------------------
    Name        : catSoundTestFirstSentence
    Description : the first event of the race's tables with a sentence (variation 0, variable 0: no random draw)
    Outputs     : *pEvent, *pOffsets
    Return      : number of phrase offsets, 0 if none
----------------------------------------------------------------------------*/
static sdword catSoundTestFirstSentence(sdword *pEvent, sdword **pOffsets)
{
    sdword event, numoffsets;
    udword duration;

    for (event = 0; event < SentenceLUT->numevents; event++)
    {
        if (SentenceLUT->lookup[event * SentenceLUT->numcolumns + 1] == 0)
        {
            continue;                                       // no variations
        }
        numoffsets = SEselectsentence(0, event, 0, 0, pOffsets, &duration);
        if (numoffsets > 0)
        {
            *pEvent = event;
            return numoffsets;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : catSoundTestSpeech
    Description : queue the first sentence of the loaded race on speech stream 0 and let the mixer decode it
    Return      : blocks decoded
----------------------------------------------------------------------------*/
static udword catSoundTestSpeech(char *bank)
{
    sdword event = -1, numoffsets, i;
    sdword *offsets = NULL;
    udword blocks;

    numoffsets = catSoundTestFirstSentence(&event, &offsets);
    if (numoffsets <= 0)
    {
        dbgMessagef("soundtest: %s: no sentence in the tables (%d events)", bank, SentenceLUT->numevents);
        return 0;
    }
    for (i = 0; i < numoffsets; i++)
    {
        soundstreamqueuePatch(streamhandle[0], speechfilehandle, offsets[i], SOUND_FLAGS_QUEUESTREAM,
                              (sword)SOUND_VOL_MAX, SOUND_PAN_CENTER, SOUND_MONO, SentenceLUT->compbitrate[0], NULL,
                              &streamEQ[SPEECH_INTEL], NULL, NULL, 0, 0.0f, 0.0f, 0, event, (i < numoffsets - 1));
    }
    blocks = catSoundTestWaitStream(streamhandle[0], 8, 15000);
    dbgMessagef("soundtest: %s: event %d (%d phrases, first at %d, bitrate %d) decoded %u blocks", bank, event,
                numoffsets, offsets[0], SentenceLUT->compbitrate[0], blocks);
    return blocks;
}

/*-----------------------------------------------------------------------------
    Name        : catSoundRunSelfTest
    Description : CAT: port diagnostic (see above); called by utyGameSystemsInit after the start-up
----------------------------------------------------------------------------*/
void catSoundRunSelfTest(void)
{
    udword musicBlocks = 0, sectBlocks = 0, beastBlocks = 0, audibleStart;
    sdword track, i, index, named = 0, numbered = 0, sect = 0, beast = 0, dormant = 0;
    sdword startRace = 0;
    bool32 pass;
    const char *driver = SDL_GetCurrentAudioDriver();

    dbgMessagef("soundtest: start: audio driver %s, SFX %d, speech %d, mixer ticks %u", (driver != NULL) ? driver : "none",
                enableSFX, enableSpeech, mixerticks);
    audibleStart = catSoundAudibleBlocks;

    if (enableSpeech && (musicheader != NULL) && (SentenceLUT != NULL))
    {
        // the music bank: the first track with data (Music4Cat.wxh lists the streams of Music4Cat.wxd)
        for (track = 0; track < musicheader->numstreams; track++)
        {
            if ((musicheader->mstreams[track].offset > 0) && (musicheader->mstreams[track].bitrate > 0))
            {
                break;
            }
        }
        if (track < musicheader->numstreams)
        {
            musicEventPlay(track);
            musicBlocks = catSoundTestWaitStream(musicinfo.handle, 64, 3000);
            musicEventStop(track, 0.0f);
            dbgMessagef("soundtest: Music4Cat.wxd: %d streams, track %d (offset %d, bitrate %d, %d channels) decoded %u blocks",
                        musicheader->numstreams, track, musicheader->mstreams[track].offset,
                        musicheader->mstreams[track].bitrate, musicheader->mstreams[track].numchannels, musicBlocks);
            catSoundTestWaitStream(musicinfo.handle, 0, 2000);
        }

        // the two voice banks: the Somtaaw tables are loaded at start-up, the Beast ones by the race switch
        startRace = whichRaceSelected;
        cat_speechEventLoadRaceVoice(0);
        sectBlocks = catSoundTestSpeech("SectSpeech.vce");
        cat_speechEventLoadRaceVoice(1);
        beastBlocks = catSoundTestSpeech("BeastSpeech.vce");
        cat_speechEventLoadRaceVoice(0);

        // the 88 battle-chatter events in the Somtaaw and the Beast tables (the event index's row of actor 0)
        for (i = 0; i < BCE_LastBCE; i++)
        {
            if (battleChatterEvent[i].eventName != NULL)
            {
                named++;
            }
            if (battleChatterEvent[i].eventNumber == 0)
            {
                dormant++;                                  // the exe's table has 0 there too
                continue;
            }
            numbered++;
            index = battleChatterEvent[i].eventNumber & CAT_SPEECH_EVENT_MASK;
            if ((index < SentenceLUT->numevents) && (SentenceLUT->lookup[index * SentenceLUT->numcolumns + 1] != 0))
            {
                sect++;
            }
        }
        cat_speechEventLoadRaceVoice(1);
        for (i = 0; i < BCE_LastBCE; i++)
        {
            index = battleChatterEvent[i].eventNumber & CAT_SPEECH_EVENT_MASK;
            if ((battleChatterEvent[i].eventNumber != 0) && (index < SentenceLUT->numevents) &&
                (SentenceLUT->lookup[index * SentenceLUT->numcolumns + 1] != 0))
            {
                beast++;
            }
        }
        cat_speechEventLoadRaceVoice(startRace);
        dbgMessagef("soundtest: chatter: %d events, %d named, %d with an id (%d have 0, as in the exe), %d with a Somtaaw sentence, %d with a Beast sentence",
                    BCE_LastBCE, named, numbered, dormant, sect, beast);
    }

    pass = enableSpeech && (musicBlocks > 0) && (sectBlocks > 0) && (beastBlocks > 0) && (BCE_LastBCE == 88) &&
           (named == BCE_LastBCE) && (catSoundAudibleBlocks > audibleStart);
    dbgMessagef("soundtest: done: music %u, sect %u, beast %u blocks decoded; %u audible output blocks; mixer ticks %u; result=%s",
                musicBlocks, sectBlocks, beastBlocks, catSoundAudibleBlocks - audibleStart, mixerticks, pass ? "PASS" : "FAIL");
}
