/*=============================================================================
    Name    : SoundEventPlay.c
    Purpose : Sound Event Play functions

    Created 5/8/98 by salfreds
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

// #include "SoundEventPlay.h"  // this does not exist

#include "Debug.h"
#include "FastMath.h"
#include "Gun.h"
#include "mainrgn.h"
#include "mainswitches.h"
#include "Matrix.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "SoundEventPrivate.h"
#include "soundlow.h"
#include "Switches.h"
#include "UnivUpdate.h"
#include "fqcodec.h"         // CAT: fqRint
#include "NIS.h"             // CAT: nisIsRunning
#include "Sensors.h"         // smSensorsActive
#include "utility.h"         // CAT: whichRaceSelected

#define OBJ_None    -1
#define OBJ_Gun     -2

#define EFFECT_SIZE	500.0f

extern real32 volSFX;
typedef struct
{
	sdword max;
	sdword handle[2];
} effectHandles;

effectHandles effectHandle[NUM_HIT_EVENTS];

void SEinitHandles(void)
{
	sdword i;

	for (i = 0; i < NUM_HIT_EVENTS; i++)
	{
		if ((i == Hit_Nick) || (i == Hit_Ricochet))
		{
			effectHandle[i].max = 1;
		}
		else
		{
			effectHandle[i].max = 2;
		}
		effectHandle[i].handle[0] = SOUND_NOTINITED;
		effectHandle[i].handle[1] = SOUND_NOTINITED;
	}
}

/*-----------------------------------------------------------------------------
    Name        :   soundEventPlay
    Description :   CAT: the exe's sound event player (soundEventPlay 0x005209f0). Differences from HW1: no object
                    out of view plays (no hyperdrive exception); Cataclysm's LUT rows (SoundEventPrivate.h); the
                    UI sounds per race (whichRaceSelected is the UI LUT's object) and UI event 0x1c8 plays ship
                    event 0x821; gun sounds by cat_gunGetSoundType, the Beast weapon (type 19) on row 9, the
                    multibeam frigates' and dAcolyte's loops and the Dreadnought's / destroyers' shots from
                    GunSpecEvents.lut, the burst remembers the gun's index; ship events without a range test (the
                    Nomad moon closer during an NIS); explosions 0x1002 / 0x1004 / 0x1007 add the 0x100d boom;
                    every patch through splayFPRVL with the exe's priorities.
    Inputs      :   object - pointer to a spaceobj, ie a Ship or an Effect
                    event - the sound event to play
                    gun - pointer to a gun if this is a gun event
    Outputs     :
    Return      :   handle to the sound, this can be used to turn off the sound later
----------------------------------------------------------------------------*/
sdword soundEventPlay(void *object, sdword event, Gun *gun)
{
    sdword handle = SOUND_NOTINITED;
#if SOUND
    Ship *ship = (Ship *)object;
    sword vol, vol2;
    sword pan;
	sdword i, row, priority, patch;
    real32 dist = 0;
    SpaceObj *spaceobject;
    Effect *effect = (Effect *)object;
    ObjType objtype = OBJ_None;
    real32 tempEQ[SOUND_EQ_SIZE];
    GunSoundType soundtype;
    ShipClass shipclass;
    SFXLUT *lut;

    if (!enableSFX)
    {
        return (SOUND_OK);
    }

    if (object != NULL)
    {
        spaceobject = (SpaceObj *)object;
        if (!univSpaceObjInRenderList(spaceobject))
        {
            return (handle);        // only do sound events for viewable objects
        }

        if (gun != NULL)
        {
            objtype = OBJ_Gun;
        }
        else
        {
            objtype = spaceobject->header.objtype;
        }

        dist = fsqrt(spaceobject->cameraDistanceSquared);
    }

    switch (event & SFX_Flag_Mask)
    {
        case UI_Flag:
            if (event <= CAT_UI_LAST_AMBIENT)
            {
                // play the ambient sound
                soundstop(ambienthandle, SOUND_FADE_STOPTIME);
                ambienthandle = splayFPRVL(UIBank, UIEventsLUT->lookup[GetPatch(UIEventsLUT, whichRaceSelected, event)],
                                           NULL, (real32)SOUND_DEFAULT, SOUND_PAN_CENTER, SOUND_PRIORITY_STREAM,
                                           (sword)fqRint(SFX_AMBIENT_VOLUME * volSFX), FALSE, TRUE, FALSE);
                return (SOUND_ERR);
            }
            vol = (sword)fqRint(volSFX * (real32)SOUND_VOL_MAX);
            if (event == 0x1c8)
            {
                handle = splayFPRVL(ShipBank, ShipEventsLUT->lookup[GetPatch(ShipEventsLUT, 0, 0x821)],
                                    NULL, (real32)SOUND_DEFAULT, SOUND_PAN_CENTER, SOUND_PRIORITY_MAX, vol, FALSE, FALSE, FALSE);
            }
            else
            {
                handle = splayFPRVL(UIBank, UIEventsLUT->lookup[GetPatch(UIEventsLUT, whichRaceSelected, event)],
                                    NULL, (real32)SOUND_DEFAULT, SOUND_PAN_CENTER, SOUND_PRIORITY_MAX, vol, FALSE, FALSE, FALSE);
            }
            return (handle);

        case Gun_Flag:
            if (smSensorsActive || !mrRenderMainScreen)
            {
                // don't want to play this sound from the manager screens
                return (SOUND_ERR);
            }
            soundtype = cat_gunGetSoundType(gun);
            if ((udword)soundtype >= (udword)GunEventsLUT->numobjects)
            {
                return (SOUND_ERR);
            }

            if ((event == Gun_WeaponFireLooped) &&
                (soundtype != GS_LargeEnergyCannon) && (soundtype != GS_MediumEnergyCannon) && (soundtype != GS_SmallEnergyCannon))
            {
                if (!SEinrange(soundtype + GUNSHOT_OFFSET, dist) ||          // is it in range
                    (ship->soundevent.burstHandle != SOUND_NOTINITED) ||     // does the sound engine think its already playing
                    !soundover(ship->soundevent.burstHandle))                // is it actually still playing
                {
                    return (SOUND_ERR);
                }

                ship->soundevent.burstfiring = TRUE;
                vol = (sword)fqRint((1.0f - ship->soundevent.coverage * 0.5f) * (real32)SEequalize(soundtype + GUNSHOT_OFFSET, dist, tempEQ));
                pan = getPanAngle(ship->obj.posinfo.position, ship->staticinfo->common.staticCollInfo.approxcollspheresize, dist);

                if ((ship->shiptype == sMultiBeamFrigate) || (ship->shiptype == bMultiBeamFrigate))
                {
                    lut = GunSpecEventsLUT;
                    patch = GetPatch(lut, 0, 0x8001);
                }
                else if (ship->shiptype == dAcolyte)
                {
                    lut = GunSpecEventsLUT;
                    patch = GetPatch(lut, 0, 0x8000);
                }
                else
                {
                    lut = GunEventsLUT;
                    patch = GetPatch(lut, soundtype, Gun_WeaponFireLooped);
                }
                handle = splayFPRVL(GunBank, lut->lookup[patch], tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_MAX, vol, FALSE, TRUE, FALSE);
                ship->soundevent.burstHandle = handle;
                // the burst remembers its gun by the gun's index (Ship +0x48C, the gunstatic's +0x20)
                ship->soundevent.gun = (void *)(memsize)gun->gunstatic->gunindex;
                return (handle);
            }

            if ((event == Gun_WeaponShot) || (event == Gun_WeaponFireLooped))
            {                                           // a looped energy cannon fires as single shots
                if (soundtype == GS_BeastWeapon)
                {                                       // the Beast weapon is heard on row 9 at full volume
                    if (!SEinrange(9, dist))
                    {
                        return (SOUND_ERR);
                    }
                    vol = (sword)fqRint((real32)SEequalize(9, dist, tempEQ));
                }
                else
                {
                    if (!SEinrange(soundtype + GUNSHOT_OFFSET, dist) || !soundover(ship->soundevent.gunHandle))
                    {
                        return (SOUND_ERR);
                    }
                    vol = (sword)fqRint((1.0f - ship->soundevent.coverage) * (real32)SEequalize(soundtype + GUNSHOT_OFFSET, dist, tempEQ));
                }
                pan = getPanAngle(ship->obj.posinfo.position, ship->staticinfo->common.staticCollInfo.approxcollspheresize, dist);

                if ((ship->shiptype == sDreadnought) || (ship->shiptype == sDestroyer) || (ship->shiptype == tMissileDestroyer))
                {
                    lut = GunSpecEventsLUT;
                    patch = GetPatch(lut, 0, 0x8002);
                }
                else
                {
                    lut = GunEventsLUT;
                    patch = GetPatch(lut, soundtype, Gun_WeaponShot);
                }
                handle = splayFPRVL(GunBank, lut->lookup[patch], tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_HIGH, vol, FALSE, TRUE, FALSE);
                if (soundtype != GS_BeastWeapon)
                {
                    ship->soundevent.gunHandle = handle;
                }
                return (handle);
            }

            // event = Gun_WeaponMove
            if (!SEinrange(soundtype + GUNMOVE_OFFSET, dist) || !soundover(ship->soundevent.gunHandle))
            {
                return (SOUND_ERR);
            }
            vol = (sword)fqRint((1.0f - ship->soundevent.coverage) * (real32)SEequalize(soundtype + GUNMOVE_OFFSET, dist, tempEQ));
            pan = getPanAngle(ship->obj.posinfo.position, ship->staticinfo->common.staticCollInfo.approxcollspheresize, dist);
            handle = splayFPRVL(GunBank, GunEventsLUT->lookup[GetPatch(GunEventsLUT, soundtype, event)], tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_MIN, vol, FALSE, TRUE, FALSE);
            return (handle);

        case ShipCmn_Flag:
            if (smSensorsActive || !mrRenderMainScreen)
            {
                // don't want to play this sound from the manager screens
                return (SOUND_ERR);
            }
            if (objtype != OBJ_ShipType)
            {
                return (SOUND_ERR);
            }

            shipclass = ship->staticinfo->shipclass;
            if (!SEinrange(shipclass, dist))
            {
                return (SOUND_ERR);
            }

            priority = SOUND_PRIORITY_MIN;
            if (event == ShipCmn_Engine)
            {
                if (ship->soundevent.engineHandle != SOUND_NOTINITED)
                {
                    return (SOUND_ERR);
                }

                ship->soundevent.engineState = SOUND_STARTING;
                if (shipclass == CLASS_Corvette)
                {
                    priority = SOUND_PRIORITY_MAX - 1;
                }
                else if (shipclass == CLASS_Fighter)
                {
                    priority = SOUND_PRIORITY_MAX - 2;
                }
                else
                {
                    priority = SOUND_PRIORITY_MAX;
                }

                vol = SEequalize(shipclass, dist, tempEQ);

                if ((shipclass == CLASS_Fighter) || (shipclass == CLASS_Corvette))
                {
                    vol = (sword)((1.0f - ship->soundevent.coverage) * (real32)vol);
                }
            }
            else
            {
                vol = SEequalize(shipclass + AMBIENT_OFFSET, dist, tempEQ);
                vol = (sword)((1.0f - ship->soundevent.coverage) * (real32)vol);
            }

            pan = getPanAngle(ship->obj.posinfo.position, ship->staticinfo->common.staticCollInfo.approxcollspheresize, dist);

            if ((udword)ship->shiptype < (udword)ShipCmnEventsLUT->numobjects)
            {
                handle = splayFPRVL(ShipBank, ShipCmnEventsLUT->lookup[GetPatch(ShipCmnEventsLUT, ship->shiptype, event)], tempEQ, (real32)SOUND_DEFAULT, pan, priority, vol, FALSE, TRUE, FALSE);
            }

            if (event == ShipCmn_Engine)
            {
                ship->soundevent.engineHandle = handle;
                if (handle == SOUND_ERR)
                {
                    ship->soundevent.engineState = SOUND_NOTINITED;
                }
            }
            return (handle);

        case Ship_Flag:
            if (smSensorsActive || !mrRenderMainScreen)
            {
                // don't want to play this sound from the manager screens
                return (SOUND_ERR);
            }
            if (objtype != OBJ_ShipType)
            {
                // can't do ship events if its not a ship
                return (SOUND_ERR);
            }

            vol = SEequalize(ship->staticinfo->shipclass, dist, tempEQ);
            if (nisIsRunning && (ship->shiptype == dNomadMoon))
            {                                           // the Nomad moon is heard closer in an NIS
                vol = SEequalize(ship->staticinfo->shipclass, dist * 0.125f, tempEQ);
            }
            pan = getPanAngle(ship->obj.posinfo.position, ship->staticinfo->common.staticCollInfo.approxcollspheresize, dist);
            handle = splayFPRVL(ShipBank, ShipEventsLUT->lookup[GetPatch(ShipEventsLUT, 0, event)], tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_NORMAL, vol, FALSE, TRUE, FALSE);
            return (handle);

        case Exp_Flag:
            if (smSensorsActive || !mrRenderMainScreen)
            {
                // don't want to play this sound from the manager screens
                return (SOUND_ERR);
            }
            dist = fsqrt(effect->obj.cameraDistanceSquared);
            row = (event < Exp_Flag + 0x10) ? (event - Exp_Flag) + EXPLOSION_OFFSET : CAT_EXPLOSION_LAST_ROW;
            if (!SEinrange(row, dist))
            {
                return (SOUND_ERR);
            }
            vol = SEequalize(row, dist, tempEQ);
            pan = getPanAngle(effect->obj.posinfo.position, EFFECT_SIZE, dist);
            handle = splayFPRVL(SpecialEffectBank, SpecExpEventsLUT->lookup[GetPatch(SpecExpEventsLUT, 0, event)], tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_STREAM, vol, FALSE, TRUE, FALSE);
            // the big explosions add a boom (event 0x100d) when it is loud enough
            if (((event == Exp_Flag + 2) || (event == Exp_Flag + 4) || (event == Exp_Flag + 7)) &&
                SEinrange(0xd + EXPLOSION_OFFSET, dist))
            {
                vol2 = SEequalize(0xd + EXPLOSION_OFFSET, dist, tempEQ);
                if (vol2 >= 0x65)
                {
                    handle = splayFPRVL(SpecialEffectBank, SpecExpEventsLUT->lookup[GetPatch(SpecExpEventsLUT, 0, Exp_Flag + 0xd)], tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_STREAM, vol2, FALSE, TRUE, FALSE);
                }
            }
            return (handle);

        case Hit_Flag:
            if (smSensorsActive || !mrRenderMainScreen)
            {
                // don't want to play this sound from the manager screens
                return (SOUND_ERR);
            }
            row = (event == Hit_Cat7) ? CAT_HIT_CAT7_ROW : (event - Hit_Flag) + HIT_OFFSET;
            if (!SEinrangeSqr(row, effect->obj.cameraDistanceSquared))
            {
                return (SOUND_ERR);
            }
            for (i = 0; i < effectHandle[event & SFX_Event_Mask].max; i++)
            {
                if (soundover(effectHandle[event & SFX_Event_Mask].handle[i]))
                {
                    dist = fsqrt(effect->obj.cameraDistanceSquared);
                    vol = SEequalize(row, dist, tempEQ);
                    pan = getPanAngle(effect->obj.posinfo.position, EFFECT_SIZE, dist);

                    handle = splayFPRVL(SpecialEffectBank, SpecHitEventsLUT->lookup[GetPatch(SpecHitEventsLUT, 0, event)], tempEQ, (real32)SOUND_DEFAULT, pan, SOUND_PRIORITY_HIGH, vol, FALSE, TRUE, FALSE);
                    effectHandle[event & SFX_Event_Mask].handle[i] = handle;
                    return (handle);
                }
            }
            return (SOUND_ERR);

        default:
            return (SOUND_ERR);
    }
#endif

    return (handle);
}


/*-----------------------------------------------------------------------------
    Name        :   soundEffect
    Description :   This is just a wrapper for soundEventPlay since the ETG
                    doesn't like it when I use a #define for soundEffect.
    Inputs      :   effect - pointer to an Effect
                    event - the sound event to play
    Outputs     :
    Return      :   handle to the sound, this can be used to turn off the sound later
----------------------------------------------------------------------------*/
memsize soundEffect(memsize *args)     // CAT: an ETG callable (0x005216b0), args[0] effect, args[1] event
{
    return ((udword)soundEventPlay((void *)args[0], (sdword)args[1], NULL));
}


/*-----------------------------------------------------------------------------
    Name        :   soundEffectType
    Description :
    Inputs      :   effect - pointer to an Effect
                    event - the sound event to play
                    objecttype - the type of object playing the event (gun sound type)
    Outputs     :
    Return      :   handle to the sound, this can be used to turn off the sound later
----------------------------------------------------------------------------*/
sdword soundEffectType(Effect *effect, sdword event, sdword objecttype)
{
    sdword handle = SOUND_NOTINITED;
    sword vol;
    sword pan;
    real32 dist;
    real32 tempEQ[SOUND_EQ_SIZE];
    sdword eventflag;

    if (!enableSFX)
    {
        return (SOUND_OK);
    }

    if (event & SFX_Flag_Mask)
    {
        eventflag = event & SFX_Flag_Mask;

        switch (eventflag)
        {
            case Hit_Flag:
				if (smSensorsActive || !mrRenderMainScreen)
				{
					// don't want to play this sound from the manager screens
					break;
				}
                if (SEinrangeSqr((event - Hit_Flag) + HIT_OFFSET, effect->obj.cameraDistanceSquared))
                {
                    dist = fsqrt(effect->obj.cameraDistanceSquared);
                    vol = SEequalize((event - Hit_Flag) + HIT_OFFSET, dist, tempEQ);
                    pan = getPanAngle(effect->obj.posinfo.position, EFFECT_SIZE, dist);

                    handle = splayEPRV(SpecialEffectBank, SpecHitEventsLUT->lookup[GetPatch(SpecHitEventsLUT, objecttype, event)], tempEQ, pan, SOUND_PRIORITY_NORMAL, vol);
                }
                break;

            default:
                dbgMessagef("Should use soundEffect for %d event instead.", event);
                break;
        }
    }

    return (handle);
}
