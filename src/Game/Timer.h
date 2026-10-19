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

#define TIMER_ALLOC_INCREMENT 16

// CAT (M2-R8 kasf library): Cataclysm's KAS timers are named by two cat_kasHashString hashes, the scope (the running
//      FSM's team label hash, 0 at mission scope or for a G_ name) and the name (timTimerCreate 0x0052c580 ..
//      timTimerLoad 0x0052c8a0); HW1 kept one 48-byte scoped name string. 0x18 bytes (timTimerSave writes count * 0x18).
typedef struct {
    udword scopeHash;           // +0x00
    udword nameHash;            // +0x04
    real32 startTime;           // +0x08
    real32 duration;            // +0x0C
    real32 pauseTime;           // +0x10
    bool8 enabled;              // +0x14
    bool8 bPaused;              // +0x15
} Timer;
_Static_assert(sizeof(Timer) == 0x18, "a KAS timer is 0x18 bytes in cataclysm.exe");

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

#include "Task.h"       // CAT: cat_taskRealTimeElapsed (the real-time timers' clock)
void cat_timerStartGameTime(CatTimer *t, real32 duration);
void cat_timerStartRealTime(CatTimer *t, real32 duration);
bool32 cat_timerExpired(CatTimer *t);
real32 cat_timerFractionElapsed(CatTimer *t);              // CAT: 0x005284c0 (M2-R4 combat)
real32 cat_timerElapsed(CatTimer *t);                      // CAT: 0x00528470 (M2-R4 ships-b)
real32 cat_timerRemaining(CatTimer *t);                    // CAT: 0x00528510 (M2-R4 ships-b)
void cat_timerRestartWithRemainder(CatTimer *t, real32 extra);  // CAT: 0x00528540 (M2-R4 ships-b)

// CAT (M2-R8 kasf library): addressed by (scope hash, name hash), cataclysm.exe 0x0052bfe0 .. 0x0052c820
void timTimerCreate(udword scopeHash, udword nameHash);
void timTimerStart(udword scopeHash, udword nameHash);
void timTimerStop(udword scopeHash, udword nameHash);
sdword timTimerExpired(udword scopeHash, udword nameHash);
sdword timTimerExpiredDestroy(udword scopeHash, udword nameHash);
sdword timTimerRemaining(udword scopeHash, udword nameHash);
void timTimerSet(udword scopeHash, udword nameHash, sdword duration);
void timTimerCreateSetStart(udword scopeHash, udword nameHash, sdword duration);
void timTimerDestroy(udword scopeHash, udword nameHash);
void timTimerDestroyAll(void);
void timTimerPause(Timer *tim);
void timTimerPauseAllNotScoped(udword scopeHash);
void timTimerUnpause(Timer *tim);
void timTimerUnpauseAll(void);

void timTimerSave(void);
void timTimerLoad(void);

#endif
