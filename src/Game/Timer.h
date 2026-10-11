//
//  Created 1997/07/01  Darren Stone
//

//
//  support for named, low-resolution timers
//  (specified in seconds, based on universe time)
//
//  the named referencing of these timers make using
//  them a little slower, but friendly for scripting, etc.
//

#ifndef ___TIMER_H
#define ___TIMER_H

#include "Types.h"

#define TIMER_NAME_MAX_LENGTH 47

#define TIMER_ALLOC_INCREMENT 16

typedef struct {
    char name[TIMER_NAME_MAX_LENGTH+1];
    real32 startTime;
    real32 duration;
    real32 pauseTime;
    bool8 enabled;
    bool8 bPaused;
} Timer;

// CAT (M2-R3 ships-a): Cataclysm's small unnamed timers (cat_timerStartGameTime 0x00528410 .. 0x00528540), 0x10
//      bytes, kept inside ship specifics (e.g. the Sect mothership's big-gun aim timer, Ship+0x68C)
typedef struct CatTimer
{
    real32 startTime;       // +0x00 universe.totaltimeelapsed, or cat_taskRealTimeElapsed for a real-time timer (cat_timerStartRealTime 0x00528440)
    real32 duration;        // +0x04
    udword magic;           // +0x08 0x499602D2 (1234567890) once started
    bool32 realTime;        // +0x0C
} CatTimer;
_Static_assert(sizeof(CatTimer) == 0x10, "CatTimer is 0x10 bytes in cataclysm.exe");

extern real32 cat_taskRealTimeElapsed;
void cat_timerStartGameTime(CatTimer *t, real32 duration);
void cat_timerStartRealTime(CatTimer *t, real32 duration);
bool32 cat_timerExpired(CatTimer *t);

void timTimerCreate(char *name);
void timTimerStart(char *name);
void timTimerStop(char *name);
sdword timTimerExpired(char *name);
sdword timTimerExpiredDestroy(char *name);
sdword timTimerRemaining(char *name);
void timTimerSet(char *name, sdword duration);
void timTimerCreateSetStart(char *name, sdword duration);
void timTimerDestroy(char *name);
void timTimerDestroyAll(void);
void timTimerPause(Timer *tim);
void timTimerPauseAllNotScoped(char *scopeName);
void timTimerUnpause(Timer *tim);
void timTimerUnpauseAll(void);

void timTimerSave(void);
void timTimerLoad(void);

#endif
