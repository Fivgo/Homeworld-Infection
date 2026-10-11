// =============================================================================
//  Cloak.c
//  - Cataclysm's ship cloak: a charge that drains while cloaked and refills
//    while not, with a random fade in/out (cataclysm.exe 0x00435f10-0x004363b0)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file. The exe links these six functions between Clipper.c and Clouds.c, so they came from a Cataclysm
//      source file of their own; the bodies below follow the r5 decompile of each (addresses on each function).

#include "Cloak.h"

#include <math.h>

#include "CommandDefs.h"
#include "CommandLayer.h"
#include "Debug.h"
#include "Randy.h"
#include "ShipSelect.h"
#include "SoundEvent.h"
#include "Universe.h"

// CAT: Cataclysm's event ids (cat_cloakActivate 0x00435f50 / cat_cloakUpdate 0x004361c0). The speech ids are Fleet
//      Command commands (SPEECH_TYPE_COMMAND | SPEECH_ACTOR_FLEET_COMMAND | n); the names say what the exe uses them
//      for, the speech tables themselves are stage 3. The two ship sound events are Cataclysm's Ship_Flag 17 and 18.
#define CAT_SPEECH_CloakOn                  0x04000889
#define CAT_SPEECH_CloakInsufficientPower   0x0400088B
#define CAT_SPEECH_CloakOff                 0x0400088D
#define CAT_SOUND_ShipCloakOn               0x811
#define CAT_SOUND_ShipCloakOff              0x812

#define CAT_CLOAK_FADE_STEP                 0.05f       // 0x0072a654
#define CAT_CLOAK_CHARGE_TO_FRACTION        0.001f      // 0x0072a650

CatCloak *cat_cloakGet(Ship *ship)
{
    // CAT: the type switch the exe inlines in cat_cloakActivate 0x00435f50, cat_cloakUpdate 0x004361c0,
    //      cat_cloakGetFadeState 0x00436130 and cat_cloakGetChargeFraction 0x00436180
    switch (ship->shiptype)
    {
        case bCloakedFighter:
            return (CatCloak *)((ubyte *)ship->ShipSpecifics + CAT_CLOAK_SPECOFS_CLOAKEDFIGHTER);
        case bIonArrayFrigate:
        case p1IonArrayFrigate:
            return (CatCloak *)((ubyte *)ship->ShipSpecifics + CAT_CLOAK_SPECOFS_IONARRAYFRIGATE);
        default:
            return NULL;
    }
}

bool32 *cat_cloakDetectedFlags(Ship *ship)
{
    // CAT: the per-player "detected" flags of the cloaking types (sub_53F9F0 0x0053f9f0,
    //      cat_shipStealthDetectedSet 0x0053fad0)
    switch (ship->shiptype)
    {
        case bCloakedFighter:
            return (bool32 *)((ubyte *)ship->ShipSpecifics + CAT_DETECTED_SPECOFS_CLOAKEDFIGHTER);
        case bIonArrayFrigate:
        case p1IonArrayFrigate:
            return (bool32 *)((ubyte *)ship->ShipSpecifics + CAT_DETECTED_SPECOFS_IONARRAYFRIGATE);
        default:
            return NULL;
    }
}

// CAT: the random fade delay of cat_cloakActivate / cat_cloakUpdate: 1.0 - e^(ranRandom(1) * (100 / 2^30) * -0.005),
//      i.e. 0 to about 0.86 s (x87 f2xm1/fscale of log2(e) * r * 9.313226e-08 * -0.005, then 1.0 - that + now)
static real32 cat_cloakFadeStartTime(void)
{
    return (real32)(cat_randomExpDelay() + (double)universe.totaltimeelapsed);
}

/*-----------------------------------------------------------------------------
    Name        : cat_randomExpDelay
    Description : CAT (M2-R4 ships-b): 1.0 - e^(ranRandom(RANDOM_CAT_SHIPS) * (100 / 2^30) * -0.005), 0 .. about 0.86,
                  which the exe inlines (x87 f2xm1/fscale, 0x0072a64c / 0x0072a648 / 0x0072a548) wherever a ship
                  waits a random moment: the cloak fade, the Acolyte's wing animation, the hive frigate's launches,
                  the mimic's unmimic. Returned in double, as the x87 keeps it before the caller scales and stores it.
----------------------------------------------------------------------------*/
double cat_randomExpDelay(void)
{
    udword r = ranRandom(RANDOM_CAT_SHIPS);

    return 1.0 - exp((double)r * (double)9.313225746154785e-08f * (double)-0.005f);
}

void cat_cloakInit(Ship *ship, CatCloak *cloak, real32 drainRate, real32 chargeRate)    // 0x00435f10
{
    bitSet(ship->header.flags, SOF_CatCloakDevice);
    cloak->drainRate = drainRate;
    cloak->state = CAT_CLOAK_Off;
    cloak->charge = CAT_CLOAK_MAX_CHARGE;
    cloak->chargeRate = chargeRate;
    cloak->fadeStartTime = 0.0f;
    cloak->fade = 0.0f;
}

bool32 cat_cloakActivate(Ship *ship)                                                     // 0x00435f50
{
    CatCloak *cloak = cat_cloakGet(ship);

    dbgAssertOrIgnore(cloak != NULL);
    if (cloak == NULL)
    {
        return TRUE;
    }

    if (cloak->state == CAT_CLOAK_Off)
    {
        // no cloaking while attacking or launching
        if ((ship->command == NULL) ||
            ((ship->command->ordertype.order != COMMAND_ATTACK) &&
             (ship->command->ordertype.order != COMMAND_LAUNCH_SHIP)))
        {
            if (cloak->charge < CAT_CLOAK_MIN_CHARGE)
            {
                speechEventQueue(ship, CAT_SPEECH_CloakInsufficientPower, -1, -1, -1, -1, -1, 0.0f, -1);
                return TRUE;
            }
            cloak->state = CAT_CLOAK_FadingIn;
            cloak->fade = 0.0f;
            cloak->fadeStartTime = cat_cloakFadeStartTime();
            soundEventPlay(ship, CAT_SOUND_ShipCloakOn, NULL);
            speechEventQueue(ship, CAT_SPEECH_CloakOn, -1, -1, -1, -1, -1, 0.0f, -1);
        }
    }
    else if (cloak->state == CAT_CLOAK_On)
    {
        bitClear(ship->header.flags, SOF_Cloaked);
        cloak->state = CAT_CLOAK_FadingOut;
        cloak->fade = 1.0f;
        cloak->fadeStartTime = cat_cloakFadeStartTime();
        soundEventPlay(ship, CAT_SOUND_ShipCloakOff, NULL);
        speechEventQueue(ship, CAT_SPEECH_CloakOff, -1, -1, -1, -1, -1, 0.0f, -1);
        return TRUE;
    }
    return TRUE;
}

bool32 cat_shipIsCloakedFully(Ship *ship)                                                // 0x00436110
{
    return (bitTest(ship->header.flags, SOF_CatCloakDevice) && bitTest(ship->header.flags, SOF_Cloaked)) ? TRUE : FALSE;
}

bool32 cat_cloakGetFadeState(Ship *ship, real32 *fade)                                   // 0x00436130
{
    CatCloak *cloak = cat_cloakGet(ship);

    if (cloak == NULL)
    {
        return FALSE;
    }
    if ((cloak->state > CAT_CLOAK_On) && (cloak->state < 4))
    {
        *fade = cloak->fade;
        return TRUE;
    }
    return FALSE;
}

real32 cat_cloakGetChargeFraction(Ship *ship)                                            // 0x00436180
{
    CatCloak *cloak = cat_cloakGet(ship);

    // the exe reads +0x04 of a NULL block for any other type; such callers check the type first
    dbgAssertOrIgnore(cloak != NULL);
    if (cloak == NULL)
    {
        return 0.0f;
    }
    return cloak->charge * CAT_CLOAK_CHARGE_TO_FRACTION;
}

void cat_cloakUpdate(Ship *ship)                                                         // 0x004361c0
{
    CatCloak *cloak = cat_cloakGet(ship);
    sdword i;

    dbgAssertOrIgnore(cloak != NULL);
    if (cloak == NULL)
    {
        return;
    }

    switch (cloak->state)
    {
        case CAT_CLOAK_Off:
            if (cloak->charge < CAT_CLOAK_MAX_CHARGE)
            {
                cloak->charge += cloak->chargeRate;
                if (cloak->charge >= CAT_CLOAK_MAX_CHARGE)
                {
                    cloak->charge = CAT_CLOAK_MAX_CHARGE;
                }
            }
            break;

        case CAT_CLOAK_On:
            cloak->charge -= cloak->drainRate;
            if (cloak->charge <= 0.0f)
            {
                bitClear(ship->header.flags, SOF_Cloaked);
                cloak->state = CAT_CLOAK_FadingOut;
                cloak->fade = 1.0f;
                cloak->charge = 0.0f;
                cloak->fadeStartTime = cat_cloakFadeStartTime();
                soundEventPlay(ship, CAT_SOUND_ShipCloakOff, NULL);
            }
            break;

        case CAT_CLOAK_FadingOut:
            if (cloak->fadeStartTime < universe.totaltimeelapsed)
            {
                cloak->fade -= CAT_CLOAK_FADE_STEP;
                if (cloak->fade <= 0.0f)
                {
                    cloak->state = CAT_CLOAK_Off;
                    cloak->fade = 0.0f;
                    for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++)
                    {
                        cat_shipStealthDetectedSet(ship, i, TRUE, FALSE);
                    }
                }
            }
            break;

        case CAT_CLOAK_FadingIn:
            // an attack or launch order cancels the cloak
            if ((ship->command != NULL) &&
                ((ship->command->ordertype.order == COMMAND_ATTACK) ||
                 (ship->command->ordertype.order == COMMAND_LAUNCH_SHIP)))
            {
                cloak->state = CAT_CLOAK_FadingOut;
                break;
            }
            if (cloak->fadeStartTime < universe.totaltimeelapsed)
            {
                cloak->fade += CAT_CLOAK_FADE_STEP;
                if (cloak->fade >= 1.0f)
                {
                    bitSet(ship->header.flags, SOF_Cloaked);
                    cloak->state = CAT_CLOAK_On;
                    cloak->fade = 1.0f;
                    for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++)
                    {
                        cat_shipStealthDetectedSet(ship, i, FALSE, FALSE);
                    }
                    shipHasJustCloaked(ship);
                }
            }
            break;

        default:
            break;
    }
}
