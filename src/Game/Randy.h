// =============================================================================
//  Randy.h
//  - Generic random-number generators with multiple streams
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 11/11/1997 by lmoloney
// =============================================================================

#ifndef ___RANDY_H
#define ___RANDY_H

#include "Types.h"

// INTERFACE -------------------------------------------------------------------

#ifdef HW_BUILD_FOR_DEBUGGING
    #define RANDOM_ERROR_CHECKING      1    // general error checking
    #define RANDOM_VERBOSE_LEVEL       2    // control specific output code
    #define RANDOM_DEBUG_CALL_SEQ      1    // debug random number calling sequence
#else
    #define RANDOM_ERROR_CHECKING      0
    #define RANDOM_VERBOSE_LEVEL       0
    #define RANDOM_DEBUG_CALL_SEQ      0
#endif


// CAT: 27 streams (HW1 25): ranStartup 0x004efa30 resets 0x00C2FD80..0x00C2FFA0 (27 x 20 bytes), ranSave 0x004efb50 /
//      ranLoad 0x004efb70 save all 27. The particle draws moved from stream 0 to 25 (partFillGenericParticles,
//      partMakePosition, partColorDist ... call ranRandomFn(0x19)) and the ETG draws from stream 1 to 26 (etgCRandom,
//      etgIRandom, etgFRandom, etgFrequencyExceeded, memStartup, factor call ranRandomFn(0x1a)). Stream 0 is now drawn
//      by renderer and simulation code alike (Make3dStar, gameStart, univFindNearestResource, lgStartGame,
//      cat_leechDrainEffect ...; docs/m2-checksum-oracle.md 2.6) and stream 1 by Cataclysm ship code (cat_cloakUpdate,
//      AcolyteHouseKeep, RKOHouseKeep, cat_mimicBecomeTarget ...). Streams 2-24 keep their HW1 users (counted by
//      literal index over the exe's decompile: 4/5 clouds/nebulae, 6 AI, 7 damage, 8 battle chatter, 9 NIS static,
//      10 gamerand, 11-17 sound/trails).
#define RANDOM_STREAM_0                0    // CAT: shared stream, wall-clock seeded by ranStartup (HW1's particle stream)
#define RANDOM_CAT_SHIPS               1    // CAT: Cataclysm-only ship code (HW1's ETG stream)
#define RANDOM_SOUND                   2
#define RANDOM_TRAILS                  3
#define RANDOM_CLOUDS                  4
#define RANDOM_NEBULAE                 5
#define RANDOM_AI_PLAYER               6
#define RANDOM_DAMAGE                  7
#define RANDOM_BATTLE                  8
#define RANDOM_STATIC                  9
#define RANDOM_GAME                   10    // if you change this, change RANDOM_SOUND_GAME_THREAD too
#define RANDOM_SOUND_GAME_THREAD      11
#define RANDOM_SOUND_BOTH_THREADS     12
#define RANDOM_TRAILS_0               13
#define RANDOM_TRAILS_1               14
#define RANDOM_TRAILS_2               15
#define RANDOM_TRAILS_3               16
#define RANDOM_TRAILS_4               17
#define RANDOM_SOUND_GAME_THREAD_0    18
#define RANDOM_SOUND_GAME_THREAD_1    19
#define RANDOM_SOUND_GAME_THREAD_2    20
#define RANDOM_SOUND_GAME_THREAD_3    21
#define RANDOM_SOUND_GAME_THREAD_4    22
#define RANDOM_SOUND_GAME_THREAD_5    23
#define RANDOM_SOUND_GAME_THREAD_6    24
#define RANDOM_PARTICLE_STREAM        25    // CAT: was 0
#define RANDOM_ETG                    26    // CAT: was 1
#define RANDOM_NUMBER_STREAMS         27    // CAT: was 25


typedef struct
{
    udword x, y, z, c, n;
}
ranstream;

// CAT: 20 bytes per stream in the exe (ranRandomFn 0x004ef860 indexes ranIndex * 5 dwords)
_Static_assert(sizeof(ranstream) == 0x14, "ranstream is 0x14 bytes in cataclysm.exe");


#if RANDOM_DEBUG_CALL_SEQ
    udword ranRandomFnSimple(sdword ranIndex);
    udword ranRandomFn(sdword ranIndex, char *file, sdword line);

    #define ranRandom(i)  ranRandomFn(i, NULL, 0)
    #define gamerand()    ranRandomFn(RANDOM_GAME, __FILE__, __LINE__)
    
    extern bool32 ranCallerDebug;
#else
    #define ranRandomFnSimple  ranRandomFn
    udword  ranRandomFn(sdword ranIndex);

    #define ranRandom(i)  ranRandomFn(i)
    #define gamerand()    ranRandomFn(RANDOM_GAME)
#endif


#define randomG(n)          (gamerand() % (n))
#define frandom(n)          (gamerand() * (((real32)(n)) * (1.0f / ((real32)UDWORD_Max))))
#define randombetween(a,b)  (randomG((b)-(a)+1) + (a))
#define frandombetween(a,b) (frandom((b)-(a)  ) + (a))


void ranRandomize(sdword ranIndex);

udword ranNumberGet(sdword ranIndex);
void   ranNumberSet(sdword ranIndex, udword nn);

void ranParametersGet(sdword ranIndex, udword *xx, udword *yy, udword *zz, udword *cc, udword *nn);
void ranParametersSet(sdword ranIndex, udword  xx, udword  yy, udword  zz, udword  cc, udword  nn);
void ranParametersReset(sdword ranIndex);

void ranStartup(void);
void ranShutdown(void);
void cat_ranResetGameStreams(void);

// HWI-ORACLE: TRUE in checksum-oracle runs (/oracleDump, /oracleSelfTest): the wall-clock second that ranStartup and
// ranRandomize advance a stream by reads as 0, so two runs start from the same RNG state (docs/m2-r3-closeout-headers.md)
extern bool32 ranVirtualWallClock;

void ranLoad(void);
void ranSave(void);


#endif
