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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "Memory.h"
#include "SaveGame.h"
#include "Timer.h"
#include "Universe.h"


sdword timersAllocated = 0;
sdword timersUsed = 0;
Timer *timers = NULL;

// CAT (M2-R8 kasf library): the KAS timers are found by (scope hash, name hash) (the search loop every exe timer
//      function starts with, 0x0052bfe0 .. 0x0052c660); NULL when absent
static Timer *timTimerFind(udword scopeHash, udword nameHash)
{
    sdword i;

    for (i = 0; i < timersUsed; ++i)
        if ((timers[i].scopeHash == scopeHash) && (timers[i].nameHash == nameHash))
            break;

    if (i >= timersUsed)
        return NULL;  // doesn't exist
    else
        return timers+i;
}

// CAT: the time left, as the exe computes it inline in timTimerRemaining 0x0052c3d0, timTimerExpired 0x0052c190,
//      timTimerExpiredDestroy 0x0052c290 and timTimerStop 0x0052c060: rounded to the nearest second (the 1.5 * 2^52
//      constant 0x0072a458, where HW1 truncated); a stopped timer returns its rounded duration (HW1: 0); a running one
//      at 0 or less is stopped and its duration zeroed (HW1 kept the duration)
static sdword timTimerLeft(Timer *tp)
{
    sdword rem;

    if (!tp->enabled)
    {
        return (sdword)lrint((double)tp->duration);
    }
    rem = (sdword)lrint((double)tp->duration - ((double)universe.totaltimeelapsed - (double)tp->startTime));
    if (rem <= 0)
    {
        tp->enabled = 0;
        tp->duration = 0.0f;
        rem = 0;
    }
    return rem;
}

// CAT: 0x0052bfe0: a timer that runs already keeps its start time (HW1 restarted it)
void timTimerStart(udword scopeHash, udword nameHash)
{
    Timer *tp;

    tp = timTimerFind(scopeHash, nameHash);
    if (!tp)
        return;

    if (tp->duration > 0)
    {
        if (!tp->enabled)
        {
            tp->startTime = universe.totaltimeelapsed;
            tp->enabled = 1;
            tp->bPaused = FALSE;
        }
    }
    else
        tp->enabled = 0;
}

// CAT: 0x0052c060: a paused timer keeps (float)0x7fffffff as its duration (the exe's 0x4f000000)
void timTimerStop(udword scopeHash, udword nameHash)
{
    Timer *tp;
    sdword rem;

    tp = timTimerFind(scopeHash, nameHash);
    if (!tp)
        return;

    rem = tp->bPaused ? SDWORD_Max : timTimerLeft(tp);
    tp->enabled = 0;
    tp->duration = (real32)rem;
}

// CAT: 0x0052c190 (a paused timer has not expired)
sdword timTimerExpired(udword scopeHash, udword nameHash)
{
    Timer *tp;

    tp = timTimerFind(scopeHash, nameHash);
    if (!tp)
        return 0;  // non-existent timers haven't "expired"

    if (tp->bPaused)
        return 0;
    return !timTimerLeft(tp);
}

// CAT: 0x0052c290
sdword timTimerExpiredDestroy(udword scopeHash, udword nameHash)
{
    Timer *tp;

    tp = timTimerFind(scopeHash, nameHash);
    if (!tp)
        return 0;  // non-existent timers haven't "expired"

    if (tp->bPaused || timTimerLeft(tp))
        return 0;

    timTimerDestroy(scopeHash, nameHash);
    return 1;
}

// CAT: 0x0052c3d0
sdword timTimerRemaining(udword scopeHash, udword nameHash)
{
    Timer *tp;

    tp = timTimerFind(scopeHash, nameHash);
    if (!tp)
        return 0;

    if (tp->bPaused)
    {
        return(SDWORD_Max);
    }
    return timTimerLeft(tp);
}

// CAT: 0x0052c480
void timTimerSet(udword scopeHash, udword nameHash, sdword duration)
{
    Timer *tp;

    tp = timTimerFind(scopeHash, nameHash);
    if (!tp)
        return;

    tp->duration = (real32)duration;
}

// CAT: 0x0052c4d0
void timTimerDestroy(udword scopeHash, udword nameHash)
{
    sdword i;

    for (i = 0; i < timersUsed; ++i)
        if ((timers[i].scopeHash == scopeHash) && (timers[i].nameHash == nameHash))
            break;

    if (i >= timersUsed)
        return;  // doesn't exist

    while (i < timersUsed-1)
    {
        memcpy(timers + i, timers + i+1, sizeof(Timer));
        ++i;
    }
    --timersUsed;
}

void timTimerDestroyAll(void)
{
    if (timers)
    {
        memFree(timers);
        timers = NULL;
    }
    timersAllocated = 0;
    timersUsed = 0;
}

// CAT: 0x0052c580: the hashes replace the name copy
void timTimerCreate(udword scopeHash, udword nameHash)
{
    if (timTimerFind(scopeHash, nameHash))
        return;  // already exists

    // allocate more if necessary
    if (timersUsed >= timersAllocated)
    {
        timers = memRealloc(timers, sizeof(Timer) * (timersAllocated + TIMER_ALLOC_INCREMENT), "timers", 0);
        timersAllocated += TIMER_ALLOC_INCREMENT;
    }

    timers[timersUsed].scopeHash = scopeHash;
    timers[timersUsed].nameHash = nameHash;
    timers[timersUsed].duration = 0;
    timers[timersUsed].enabled = 0;
    timers[timersUsed].bPaused = 0;
    ++timersUsed;
}

//
//  saves having to do a create, then a set, then a start
//  with separate operations.
//
// CAT: 0x0052c660 (the three inlined)
void timTimerCreateSetStart(udword scopeHash, udword nameHash, sdword duration)
{
    timTimerCreate(scopeHash, nameHash);
    timTimerSet(scopeHash, nameHash, duration);
    timTimerStart(scopeHash, nameHash);
}

/*-----------------------------------------------------------------------------
    Name        : timTimerPause
    Description : Pause a timer.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void timTimerPause(Timer *tim)
{
    dbgAssertOrIgnore(tim);
    tim->pauseTime = universe.totaltimeelapsed;
    tim->bPaused = TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : timTimerPauseAllNotScoped
    Description : Pauses all timers that do not have the specified scope name
    Inputs      : scopeHash - the hash of the FSM that called this function (CAT: 0x0052c7e0).
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void timTimerPauseAllNotScoped(udword scopeHash)
{
    Timer *tim = timers;
    sdword index;

    // CAT: 0x0052c7e0: a timer of another scope is one whose scope hash differs (HW1 compared the name's prefix)
    for (index = 0; index < timersUsed; index++, tim++)
    {
        if (tim->scopeHash != scopeHash)
        {                                                   //if does not match scope name
            timTimerPause(tim);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : timTimerUnpause
    Description : Unpause a timer.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void timTimerUnpause(Timer *tim)
{
    tim->duration += universe.totaltimeelapsed - tim->pauseTime;
    tim->bPaused = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : timTimerUnpauseAll
    Description : Resume all previously paused timers.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void timTimerUnpauseAll(void)
{
    Timer *tim = timers;
    sdword index;

    for (index = 0; index < timersUsed; index++, tim++)
    {
        if (tim->bPaused)
        {                                                   //if this timer is paused
            timTimerUnpause(tim);
        }
    }
}

/*=============================================================================
    Save Game Stuff below:
=============================================================================*/

void timTimerSave(void)
{
    SaveInfoNumber(timersAllocated);
    SaveInfoNumber(timersUsed);

    if (timersAllocated > 0)
    {
        dbgAssertOrIgnore(timers);
        // just number data, no pointers in Timer so we can just save this way
        SaveStructureOfSize(timers,timersAllocated * sizeof(Timer));
    }
}

void timTimerLoad(void)
{
    timersAllocated = LoadInfoNumber();
    timersUsed = LoadInfoNumber();

    if (timersAllocated > 0)
    {
        timers = LoadStructureOfSize(timersAllocated * sizeof(Timer));
    }
    else
    {
        timers = NULL;
    }
}

/*-----------------------------------------------------------------------------
    CAT (M2-R3 ships-a): Cataclysm's unnamed timers (Timer.h CatTimer)
-----------------------------------------------------------------------------*/
#define CAT_TIMER_MAGIC 0x499602D2

// CAT: the exe's real-time timers read the task scheduler's own wall-clock sum (cat_taskRealTimeElapsed, advanced by
//      taskExecuteAllPending 0x0052a840 from the unscaled ticks; Task.c, M2-R4 rest-oracle), not universe.realtimeelapsed

void cat_timerStartGameTime(CatTimer *t, real32 duration)                  // 0x00528410
{
    t->duration = duration;
    t->magic = CAT_TIMER_MAGIC;
    t->startTime = universe.totaltimeelapsed;
    t->realTime = FALSE;
}

void cat_timerStartRealTime(CatTimer *t, real32 duration)                  // 0x00528440
{
    t->duration = duration;
    t->magic = CAT_TIMER_MAGIC;
    t->startTime = cat_taskRealTimeElapsed;
    t->realTime = TRUE;
}

bool32 cat_timerExpired(CatTimer *t)                                        // 0x00528490
{
    real32 now = t->realTime ? cat_taskRealTimeElapsed : universe.totaltimeelapsed;

    return ((now - t->startTime) < t->duration) ? FALSE : TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_timerFractionElapsed
    Description : CAT: how much of a CatTimer's duration has passed (0 for a timer without a duration; unclamped)
                  (cat_timerFractionElapsed 0x005284c0)
    Inputs      : t
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
real32 cat_timerFractionElapsed(CatTimer *t)
{
    real32 now = t->realTime ? cat_taskRealTimeElapsed : universe.totaltimeelapsed;

    if (t->duration <= 0.0f)
    {
        return 0.0f;
    }
    return (now - t->startTime) / t->duration;
}

/*-----------------------------------------------------------------------------
    Name        : cat_timerElapsed / cat_timerRemaining / cat_timerRestartWithRemainder
    Description : CAT (M2-R4 ships-b): the time since a CatTimer started (cat_timerElapsed 0x00528470), the time left
                  (cat_timerRemaining 0x00528510), and a restart that keeps the time left and adds `extra`
                  (cat_timerRestartWithRemainder 0x00528540: the restarted timer always runs on game time)
----------------------------------------------------------------------------*/
real32 cat_timerElapsed(CatTimer *t)
{
    return (t->realTime ? cat_taskRealTimeElapsed : universe.totaltimeelapsed) - t->startTime;
}

real32 cat_timerRemaining(CatTimer *t)
{
    return t->duration - ((t->realTime ? cat_taskRealTimeElapsed : universe.totaltimeelapsed) - t->startTime);
}

void cat_timerRestartWithRemainder(CatTimer *t, real32 extra)
{
    real32 now = t->realTime ? cat_taskRealTimeElapsed : universe.totaltimeelapsed;

    t->magic = CAT_TIMER_MAGIC;
    t->duration = (t->duration - (now - t->startTime)) + extra;
    t->startTime = universe.totaltimeelapsed;
    t->realTime = FALSE;
}
