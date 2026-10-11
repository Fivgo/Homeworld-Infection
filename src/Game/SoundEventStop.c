/*=============================================================================
    Name    : SoundEventStop.c
    Purpose : Sound Event Stop functions

    Created 5/8/98 by salfreds
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

// #include "SoundEventStop.h"  // this does not exist

#include "mainswitches.h"
#include "SoundEventPrivate.h"
#include "soundlow.h"

/*-----------------------------------------------------------------------------
    Name        :	soundEventStopSound
    Description :
    Inputs      :	soundhandle - handle of the sound to stop
					numblocks - number of DCT blocks to fade over
    Outputs     :
    Return      :	SOUND_OK if successful, error ( < 0) if fails
----------------------------------------------------------------------------*/
sdword soundEventStopSound(sdword soundhandle, sdword numblocks)
{
	if (!enableSFX)
	{
		return (SOUND_OK);
	}

	if (numblocks < SOUND_OK)
		return (soundstop(soundhandle, SOUND_FADE_STOPNOW));

	return (soundstop(soundhandle, SOUND_FADE_STOPTIME));
}


/*-----------------------------------------------------------------------------
    Name        :	soundEventBustStop
    Description :
    Inputs      :	ship - the ship that is firing
					gun - the gun that is firing
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void soundEventBurstStop(Ship *ship, Gun *gun)
{
	if (!enableSFX)
	{
		return;
	}

	if (ship->soundevent.burstfiring)
	{
		ship->soundevent.burstfiring = FALSE;

		if (ship->soundevent.burstHandle > SOUND_NOTINITED)
		{
			// CAT: no DefenseFighter instant stop, Cataclysm has no such type (soundEventBurstStop 0x00521710 always
			//      fades over SOUND_FADE_STOPTIME)
			soundstop(ship->soundevent.burstHandle, SOUND_FADE_STOPTIME);
		    ship->soundevent.burstHandle = SOUND_NOTINITED;
		    // CAT: the exe also clears the burst gun (Ship+0x48C = soundevent.gun; soundEventBurstStop 0x00521710)
		    //      [audit M2-R4 ships-b]
		    ship->soundevent.gun = NULL;
		}
	}
}

