/*=============================================================================
    Name    : avi.h
    Purpose : routines for playing AVI files

    Created 1/9/1999 by jdorie, khent
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___AVI_H
#define ___AVI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Types.h"


/*=============================================================================
    Definitions:
=============================================================================*/

#ifdef HW_BUILD_FOR_DEBUGGING
    #define AVI_VERBOSE_LEVEL  3    // print extra info
#else
    #define AVI_VERBOSE_LEVEL  0
#endif


extern bool32 utilPlayingIntro;


/*=============================================================================
    Functions:
=============================================================================*/


int aviInit(void);
bool32 aviPlay(char* filename);
void aviPictureRect(int w, int h, int physW, int physH, int rect[4]);   // HWI-FEATURE(intro-movies)
void aviMovieShotSet(char *spec);       // HWI-TEST (M3-R4 intro-movies): /catMovieShot
void aviMovieEventSet(char *spec);      // HWI-TEST (M3-R4 intro-movies): /catMovieEventAt
int aviStop(void);
int aviCleanup(void);
void aviIntroPlay(void);

int aviGetSamples(void* pBuf, long* pNumSamples, long nBufSize);

extern int aviIsPlaying;
extern bool32 aviIntroMovie;    // HWI-FEATURE(intro-movies): an intro logo is playing (the whole frame is the picture)
extern bool32 aviQuitAsked;     // HWI-FEATURE(intro-movies): the window was closed during a movie
extern sdword aviFrameCap;      // HWI-TEST (M3-R4 intro-movies): /catMovieTest's frame cap (0: the whole movie)
extern real32 aviFrameRate;     // CAT (M2-R6 nis-fow-camera): the movie's frame rate (the exe's 0x00a70f1c)



#ifdef __cplusplus
}
#endif

#endif

