// =============================================================================
//  ShipDisable.c
//  - Cataclysm's ship disable state: the 13 disable reasons, helplessness,
//    EMP shock and drift (cataclysm.exe 0x005b3900-0x005b4090)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R4 ships-b). Each body follows the r6 decompile of its exe function (address on each);
//      cat_shipSetDisableReason moved here from ShipSelect.c, where M2-R3 had put it.

#include "ConsMgr.h"
#include "ShipDisable.h"
#include "KASVar.h"

#include "CommandDefs.h"
#include "CommandLayer.h"
#include "Debug.h"
#include "Globals.h"
#include "Leech.h"
#include "RammingFrigate.h"
#include "Salvage.h"
#include "Timer.h"
#include "UnivUpdate.h"
#include "Universe.h"

// the exe recomputes Ship+0x210 from the 13 reasons inline after every change
static void catShipRecomputeDisabled(Ship *ship)
{
    sdword i;

    ship->shipOrDerelict.catDisabled = FALSE;
    for (i = 0; i < CAT_NUM_DISABLE_REASONS; i++)
    {
        if (ship->shipOrDerelict.catDisableReason[i] != 0)
        {
            ship->shipOrDerelict.catDisabled = TRUE;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipSetDisableReason
    Description : CAT: one of 13 disable reasons; the ship is disabled while any of them is set
                  (cat_shipSetDisableReason 0x005b3fe0)
----------------------------------------------------------------------------*/
void cat_shipSetDisableReason(Ship *ship, sdword value, sdword reason)
{
    dbgAssertOrIgnore((reason >= 0) && (reason < CAT_NUM_DISABLE_REASONS));
    ship->shipOrDerelict.catDisableReason[reason] = value;
    catShipRecomputeDisabled(ship);
}

// the start of the helplessness, inlined by cat_empShockShip and cat_shipSetHelpless: the ship becomes crazy and
// disabled (SOF_Crazy | SOF_Disabled, 0xA00000) with the old bits and its autostabilize saved, and careens
static void catShipBecomeHelpless(Ship *ship, real32 duration)
{
    udword flags = ship->header.flags;

    ship->catHelpless = TRUE;
    ship->shipOrDerelict.catDisableReason[CAT_DISABLE_REASON_Helpless] = 1;
    catShipRecomputeDisabled(ship);
    ship->catHelplessSavedDisabled = bitTest(flags, SOF_Disabled) ? 1 : 0;
    ship->catHelplessSavedCrazy = bitTest(flags, SOF_Crazy) ? 1 : 0;
    ship->header.flags = flags | SOF_Crazy | SOF_Disabled;
    ship->catHelplessSavedAutostabilize = (sdword)(sbyte)ship->autostabilizeship;
    cat_timerStartGameTime(&ship->catHelplessTimer, duration);
    ApplyCareenRotationDirectly(ship);
}

// the end of the helplessness, inlined by cat_shipClearHelpless and cat_shipUpdateDrift: the saved bits and
// autostabilize come back (a ship that still moves stabilises), and the EMP stun ends
static void catShipEndHelpless(Ship *ship)
{
    ship->shipOrDerelict.catDisableReason[CAT_DISABLE_REASON_Helpless] = 0;
    catShipRecomputeDisabled(ship);
    ship->header.flags = (ship->header.flags & ~(SOF_Crazy | SOF_Disabled)) |
                         (((ship->catHelplessSavedDisabled & 1) << 2 | (ship->catHelplessSavedCrazy & 1)) << 21);
    ship->autostabilizeship = (bool8)ship->catHelplessSavedAutostabilize;
    if ((ship->obj.posinfo.velocity.x != 0.0f) || (ship->obj.posinfo.velocity.y != 0.0f) ||
        (ship->obj.posinfo.velocity.z != 0.0f))
    {
        ship->autostabilizeship = TRUE;
    }
    ship->catHelpless = FALSE;
    if (ship->catEmpStunned == 1)
    {
        ship->catEmpStunned = 0;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipStartDrift
    Description : CAT: a ship inside the universe starts drifting for time (cat_shipStartDrift 0x005b3b40); a
                  drifting one gets time added, but no more than extra left
----------------------------------------------------------------------------*/
void cat_shipStartDrift(Ship *ship, real32 time, real32 extra)
{
    if (cat_univPointInUniverse(&ship->obj.posinfo.position) != 1)
    {
        return;
    }
    if (!ship->catDrifting)
    {
        ship->catDriftVelocity = ship->obj.posinfo.velocity;
        ship->catDrifting = TRUE;
        cat_timerStartGameTime(&ship->catDriftTimer, time);
        return;
    }
    cat_timerRestartWithRemainder(&ship->catDriftTimer, time);
    if (extra < cat_timerRemaining(&ship->catDriftTimer))
    {
        cat_timerStartGameTime(&ship->catDriftTimer, extra);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipSetHelpless
    Description : CAT: the ship is helpless for duration, or duration more (cat_shipSetHelpless 0x005b3bd0)
----------------------------------------------------------------------------*/
void cat_shipSetHelpless(Ship *ship, real32 duration)
{
    if (!ship->catHelpless)
    {
        catShipBecomeHelpless(ship, duration);
        return;
    }
    cat_timerRestartWithRemainder(&ship->catHelplessTimer, duration);
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipClearHelpless
    Description : CAT: ends the helplessness at once (cat_shipClearHelpless 0x005b3c80)
----------------------------------------------------------------------------*/
void cat_shipClearHelpless(Ship *ship)
{
    if (ship->catHelpless)
    {
        catShipEndHelpless(ship);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipUpdateDrift
    Description : CAT: per update (cat_univShipDeferredUpdate): a drift ends when its timer runs out or the ship
                  has left the universe; the helplessness ends when its timer runs out, or at once for a ship
                  hyperspacing in multiplayer; an EMP-stunned ship slows down (x0.9 per update)
                  (cat_shipUpdateDrift 0x005b3d60)
----------------------------------------------------------------------------*/
void cat_shipUpdateDrift(Ship *ship)
{
    if (ship->catDrifting == 1)
    {
        if (!cat_univPointInUniverse(&ship->obj.posinfo.position) || cat_timerExpired(&ship->catDriftTimer))
        {
            if ((ship->obj.posinfo.velocity.x != 0.0f) || (ship->obj.posinfo.velocity.y != 0.0f) ||
                (ship->obj.posinfo.velocity.z != 0.0f))
            {
                ship->autostabilizeship = TRUE;
            }
            ship->catDrifting = FALSE;
        }
    }
    if (ship->catHelpless == 1)
    {
        if (!cat_timerExpired(&ship->catHelplessTimer))
        {
            if (ship->catEmpStunned == 1)
            {
                ship->obj.posinfo.velocity.x *= 0.9f;       // 0x008c4710
                ship->obj.posinfo.velocity.y *= 0.9f;
                ship->obj.posinfo.velocity.z *= 0.9f;
            }
            if ((!singlePlayerGame) && (ship->command != NULL) &&
                (ship->command->ordertype.order == COMMAND_MP_HYPERSPACING) && ship->catHelpless)
            {
                catShipEndHelpless(ship);
            }
        }
        else if (ship->catHelpless)
        {
            catShipEndHelpless(ship);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipSetSUExempt
    Description : CAT: whether the ship counts against the support units (SPECIAL_2_CatExempt20)
                  (cat_shipSetSUExempt 0x005b4020)
----------------------------------------------------------------------------*/
void cat_shipSetSUExempt(Ship *ship, bool32 exempt)
{
    if (exempt == 1)
    {
        if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
        {
            cat_suShipRemoved(ship);        // CAT (M2-R6 managers): 0x00459ae0, the support-unit count
        }
    }
    else if ((exempt == 0) && bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
    {
        bitClear(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20);
        cat_suShipAdded(ship, ship->playerowner);   // CAT (M2-R6 managers): 0x00459a90
    }
    ship->shipOrDerelict.specialFlags2 = (ship->shipOrDerelict.specialFlags2 & ~SPECIAL_2_CatExempt20) |
                                         ((exempt & 1) ? SPECIAL_2_CatExempt20 : 0);
}

/*-----------------------------------------------------------------------------
    Name        : cat_empShockShip
    Description : CAT: an ACV's EMP pulse hits ship (cat_empShockShip 0x005b3900). A ship that is not being
                  infected and is not hyperspacing in multiplayer becomes helpless for strength (at most 10) times
                  its TimeShockedByEMP; the first shock also makes a leech let go, releases a latched ramming
                  frigate and detaches salvagers. A ship immune to EMP (TimeShockedByEMP 0) that KAS paralyzed
                  (SPECIAL_2_CatParalyzed, 0x005b3a7f: and 0x4000) only counts the shocks and spins.
----------------------------------------------------------------------------*/
void cat_empShockShip(Ship *ship, sdword strength)
{
    sdword clamped = (strength > 10) ? 10 : strength;
    real32 duration;
    real32 fraction;
    union { sdword i; real32 f; } bits;

    if ((ship->catBeastConvertTime != 0.0f) ||
        ((!singlePlayerGame) && (ship->command != NULL) && (ship->command->ordertype.order == COMMAND_MP_HYPERSPACING)))
    {
        return;
    }
    if (ship->staticinfo->TimeShockedByEMP > 0.0f)
    {
        if (ship->catEmpStunned == 0)
        {
            ship->catEmpStunned = 1;
            if (cat_shipIsLeech(ship))
            {
                cat_leechGetSpecifics(ship)->released = 1;
            }
            cat_rammingFrigateForceRelease(ship);
            cat_salvageForceDetach(ship);
        }
        duration = (real32)clamped * ship->staticinfo->TimeShockedByEMP;
        if (ship->catHelpless)
        {
            cat_timerRestartWithRemainder(&ship->catHelplessTimer, duration);
        }
        else
        {
            catShipBecomeHelpless(ship, duration);
        }
        ship->catEmpShockCount += clamped;
        return;
    }
    if (bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatParalyzed))
    {
        ship->catEmpShockCount += strength;
        // CAT: the fraction is catEmpShockCount (unsigned) / the global KAS variable G_NaggarokEMP's value
        //      (cat_kasHashString 0x00491860, cat_kasVarFind 0x00491b20; M2-R8 kasf library). Without the variable
        //      the exe loads the clamped count's bits as a float (0x005b3ad5: fld [esp+0x18]), a denormal that
        //      rounds the fraction to 0; that path is kept.
        {
            CatKasVar *emp = cat_kasVarFind(0, cat_kasHashString("G_NaggarokEMP"));

            if (emp != NULL)
            {
                fraction = (real32)(udword)ship->catEmpShockCount / (real32)emp->value;
            }
            else
            {
                bits.i = clamped;
                fraction = bits.f;
            }
        }
        if (fraction > 1.0f)
        {
            fraction = 1.0f;
        }
        else if (fraction < 0.0f)
        {
            fraction = 0.0f;
        }
        ship->catBeastSpin = 1.0f - fraction;
        ship->catBeastStunState = 1;
        ship->catBeastStunStart = ship->catBeastSpin + universe.totaltimeelapsed;
        ship->catBeastStunEnd = ship->catBeastStunStart + 1.0f;
    }
}

