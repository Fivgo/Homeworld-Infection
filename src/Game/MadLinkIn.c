/*=============================================================================
    Name    : madLinkIn.c
    Purpose : contains all code used to bind mesh animations to
              actual events.  Esentially a hard coded nightmare.

    Created 31/08/1998 by bryce pasechnik
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "MadLinkIn.h"

#include <stdlib.h>

#include "Debug.h"
#include "MadLinkInDefs.h"
#include "RaceDefs.h"
#include "Randy.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "SpaceObj.h"
#include "Types.h"
#include "Universe.h"


#define DEBUG_MESH_ANIMATIONS  0

// CAT (M2-R6 nis-fow-camera): the sound events the cue functions store for an animation's start (Ship+0x320): the exe
//      stores 0x405 for every opening (guns, wings, door, special, damaged or not) and 0x406 for every closing
//      (madOpenGunsShip 0x004a5560, madLinkInCloseGunsShip 0x004a56b0, madLinkInPostDockingShip 0x004a5800,
//      madLinkInPreDockingShip 0x004a5910, madLinkInOpenDoor 0x004a5a20, madLinkInCloseDoor 0x004a5ae0,
//      madLinkInOpenSpecialShip 0x004a5ba0, madLinkInCloseSpecialShip 0x004a5cb0); HW1 stored ShipCmn_AnimOpening /
//      Closing / DamagedOpen / DamagedClose (0x406..0x409). Names chosen here.
#define CAT_SHIPCMN_ANIM_OPEN   (ShipCmn_Flag + 5)
#define CAT_SHIPCMN_ANIM_CLOSE  (ShipCmn_Flag + 6)

//Variables in ship structure to support mesh animations (amongst others)
//    udword madAnimationFlags;        //general purpose flags for mad animations
//    real32 madAnimationTime;        //general purpose variable for counting
//    udword cuedAnimation[MAD_MAX_CUEABLE];            //Animation index waiting in the wings to be played


/*-----------------------------------------------------------------------------
    Name        : madLinkInUpdateMeshAnimations
    Description : starts,stops and does whatever needs to be done
                  for mesh animations based on code given cues.
    Inputs      : Ship to have animation updated
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R6 nis-fow-camera): madLinkInUpdateMeshAnimations 0x004a51a0: a cued animation starts with mode 0 (the
//      exe's madAnimationStart takes care of a playing one) and plays its cued sound; no HW1 Mothership door upkeep;
//      the door's end plays no extra "Ka-chunk" (the exe has no Ship_MoshipDoorClosed call)
void madLinkInUpdateMeshAnimations(Ship *ship)
{
    if(bitTest(ship->madAnimationFlags,MAD_NEED_TO_START_NEW_ANIMATION))
    {
        //we need to switch to a new animation for some odd reason
        madAnimationStart(ship, ship->cuedAnimationIndex, MAD_ModePauseAtEnd);  //start 0th cued animation
		/* play special animation sound here */
		soundEvent(ship, ship->soundEventAnimationTypeFlag);
        bitClear(ship->madAnimationFlags,MAD_NEED_TO_START_NEW_ANIMATION);
        return;
    }

    //time to start animation

    if(madAnimationUpdate(ship, universe.phystimeelapsed))
    {
#if DEBUG_MESH_ANIMATIONS
        dbgMessagef("Animation Finished.");
#endif
        //start and pause poo here for pooeyness...(wings just closed, gotta stay closed...etc..

        //cleanup ended animation
        switch(ship->cuedAnimationType)
        {
        case MAD_ANIMATION_GUN_OPENING:
            ship->cuedAnimationType = MAD_ANIMATION_NOTHING;
            ship->madGunStatus = MAD_STATUS_GUNS_OPEN;
            break;
        case MAD_ANIMATION_GUN_CLOSING:
            ship->cuedAnimationType = MAD_ANIMATION_NOTHING;
            ship->madGunStatus = MAD_STATUS_GUNS_CLOSED;
            //setup closed gun hack...
            madAnimationStart(ship, ship->staticinfo->madStatic->gunOpenIndexes[0], MAD_ModePauseAtEnd);    //start 0th animation
            madAnimationPause(ship,!ship->madBindings->bPaused);
            break;
        case MAD_ANIMATION_WINGS_OPENING:
            ship->cuedAnimationType = MAD_ANIMATION_NOTHING;
            ship->madWingStatus = MAD_STATUS_WINGS_OPEN;
            break;
        case MAD_ANIMATION_WINGS_CLOSING:
            ship->cuedAnimationType = MAD_ANIMATION_NOTHING;
            ship->madWingStatus = MAD_STATUS_WINGS_CLOSED;
            //setup closed gun hack...
            madAnimationStart(ship, ship->staticinfo->madStatic->PostDockIndexes[0], MAD_ModePauseAtEnd);   //start 0th animation
            madAnimationPause(ship,!ship->madBindings->bPaused);
            break;
        case MAD_ANIMATION_DOOR_OPENING:
            ship->cuedAnimationType = MAD_ANIMATION_NOTHING;
            ship->madDoorStatus = MAD_STATUS_DOOR_OPEN;
            madAnimationStart(ship, ship->staticinfo->madStatic->DoorCloseIndexes[0], MAD_ModePauseAtEnd);  //start 0th animaiton
            madAnimationPause(ship,!ship->madBindings->bPaused);
            break;
        case MAD_ANIMATION_DOOR_CLOSING:
            ship->cuedAnimationType = MAD_ANIMATION_NOTHING;
            ship->madDoorStatus = MAD_STATUS_DOOR_CLOSED;
            break;
        case MAD_ANIMATION_SPECIAL_OPENING:
            ship->cuedAnimationType = MAD_ANIMATION_NOTHING;
            ship->madSpecialStatus = MAD_STATUS_SPECIAL_OPEN;
            break;
        case MAD_ANIMATION_SPECIAL_CLOSING:
            ship->cuedAnimationType = MAD_ANIMATION_NOTHING;
            ship->madSpecialStatus = MAD_STATUS_SPECIAL_CLOSED;
            madAnimationStart(ship, ship->staticinfo->madStatic->specialOpenIndexes[0], MAD_ModePauseAtEnd);    //start 0th animaiton
            madAnimationPause(ship,!ship->madBindings->bPaused);
            break;
        }

        bitClear(ship->madAnimationFlags,MAD_ANIMATION_NEED_PROC);

        //play animations that are waiting...
        if(ship->nextAnim != 0)
        {
            switch(ship->nextAnim)
            {
            case MAD_ANIMATION_GUN_OPENING:
                madOpenGunsShip(ship);
                break;
            case MAD_ANIMATION_GUN_CLOSING:
                madLinkInCloseGunsShip(ship);
                break;
            case MAD_ANIMATION_WINGS_OPENING:
                madLinkInPostDockingShip(ship);
                break;
            case MAD_ANIMATION_WINGS_CLOSING:
                madLinkInPreDockingShip(ship);
                break;
            case MAD_ANIMATION_DOOR_OPENING:
                madLinkInOpenDoor(ship);
                break;
            case MAD_ANIMATION_DOOR_CLOSING:
                madLinkInCloseDoor(ship);
                break;
            case MAD_ANIMATION_SPECIAL_OPENING:
                madLinkInOpenSpecialShip(ship);
                break;
            case MAD_ANIMATION_SPECIAL_CLOSING:
                madLinkInCloseSpecialShip(ship);
                break;
            default:
                dbgFatalf(DBG_Loc,"\nUnknown animation in nextAnim variable.");
                break;
            }
            ship->nextAnim = 0;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : madLinkInSetUpInitialBuiltMadState
    Description : For ships like the defender, probe, etc...
                  they need to have animations started then paused
                  upon creation.  This function does that.
    Inputs      : Ship to have animation set up
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R6 nis-fow-camera): madLinkInSetUpInitialBuiltMadState 0x004a5420: only for a type that does not number its animations
//      (madstatic.animsByNumber 0), started with mode 0 and paused; the one type with a start-up state is the Kushan
//      kHeavyDefender (guns closed); the other Cataclysm races have none (HW1's R1/R2 cases retired)
void madLinkInSetUpInitialBuiltMadState(Ship *ship)
{
#if DEBUG_MESH_ANIMATIONS
    dbgAssertOrIgnore(ship->madBindings != NULL);
#endif

    if (ship->staticinfo->madStatic->needStartBuiltAnimation && !ship->staticinfo->madStatic->animsByNumber)
    {
        //if ship has need to be set to a specific starting animation
        madAnimationStart(ship, ship->staticinfo->madStatic->startBuiltAnimationIndex, MAD_ModePauseAtEnd);
        madAnimationPause(ship, !ship->madBindings->bPaused);
        switch(ship->shiprace)
        {
        case RACE_Sect:
        case RACE_Beast:
        case RACE_Taiidan:
        case RACE_P1:
        case RACE_Traders:
        case RACE_Derelict:
            break;
        case RACE_Kushan:
            if (ship->shiptype == kHeavyDefender)
            {
                ship->madGunStatus = MAD_STATUS_GUNS_CLOSED;
            }
            break;
        default:
            dbgFatalf(DBG_Loc,"\nUnknown race type with a start up mesh animation (1).");
        }
    }
}
/*-----------------------------------------------------------------------------
    Name        : madLinkInSetUpInitialBuiltMadState
    Description : For ships like the defender, probe, etc...
                  they need to have animations started then paused
                  upon creation.  This function does that.
    Inputs      : Ship to have animation set up
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R6 nis-fow-camera): madLinkInSetUpInitialPlacedMadState 0x004a54c0: only for a type that does not number its animations
//      (madstatic.animsByNumber 0), started with mode 0 and paused; the one type with a start-up state is the Kushan
//      kHeavyDefender (guns closed); the other Cataclysm races have none (HW1's R1/R2 cases retired)
void madLinkInSetUpInitialPlacedMadState(Ship *ship)
{
#if DEBUG_MESH_ANIMATIONS
    dbgAssertOrIgnore(ship->madBindings != NULL);
#endif

    if (ship->staticinfo->madStatic->needStartPlacedAnimation && !ship->staticinfo->madStatic->animsByNumber)
    {
        //if ship has need to be set to a specific starting animation
        madAnimationStart(ship, ship->staticinfo->madStatic->startPlacedAnimationIndex, MAD_ModePauseAtEnd);
        madAnimationPause(ship, !ship->madBindings->bPaused);
        switch(ship->shiprace)
        {
        case RACE_Sect:
        case RACE_Beast:
        case RACE_Taiidan:
        case RACE_P1:
        case RACE_Traders:
        case RACE_Derelict:
            break;
        case RACE_Kushan:
            if (ship->shiptype == kHeavyDefender)
            {
                ship->madGunStatus = MAD_STATUS_GUNS_CLOSED;
            }
            break;
        default:
            dbgFatalf(DBG_Loc,"\nUnknown race type with a start up mesh animation (1).");
        }
    }
}

//General Comment:
//
//priority below:  1 - standard  (opens based on standard timings)
//                 2 - immediate (cues animation to play immediatly)

void madOpenGunsShip(Ship *ship)
{
    sdword animIndex;
    if (ship->madBindings != NULL && ship->staticinfo->madStatic != NULL)
    {
        //if ship has mesh animations associated with it

        if(ship->staticinfo->madStatic->numGunOpenIndexes < 1)
        {
            //ship doesn't have gun opening animations
            return;
        }

        if(ship->madGunStatus == MAD_STATUS_GUNS_OPEN ||
           ship->madGunStatus == MAD_STATUS_GUNS_OPENING)
        {
            //guns are currently open...so lets ferget about it
            return;
        }
        else if(ship->madGunStatus == MAD_STATUS_GUNS_CLOSING)
        {
            ship->nextAnim = MAD_ANIMATION_GUN_OPENING;
            return;
        }

        //signal that an animation has been added to the ships cue
        bitSet(ship->madAnimationFlags,MAD_ANIMATION_NEED_PROC);

        if(ship->targetable.health*ship->staticinfo->health.oneOverMaxHealth < ship->staticinfo->madGunOpenDamagedHealthThreshold && ship->staticinfo->madStatic->numGunOpenDamagedIndexes > 0)
        {
            //ships health is below threshold
            //and ship has damaged animations as well
            animIndex = randombetween(0,(ship->staticinfo->madStatic->numGunOpenDamagedIndexes-1));
            ship->cuedAnimationIndex = ship->staticinfo->madStatic->gunOpenDamagedIndexes[animIndex];
			ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_OPEN;     // CAT: 0x405 for every opening animation (the exe)

        }
        else
        {
            //ships health is normalish..use normal gun anims
            animIndex = randombetween(0,(ship->staticinfo->madStatic->numGunOpenIndexes-1));
            ship->cuedAnimationIndex = ship->staticinfo->madStatic->gunOpenIndexes[animIndex];
			ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_OPEN;     // CAT: 0x405 for every opening animation (the exe)
        }
        ship->cuedAnimationType = MAD_ANIMATION_GUN_OPENING;
        ship->madGunStatus = MAD_STATUS_GUNS_OPENING;
        bitSet(ship->madAnimationFlags,MAD_NEED_TO_START_NEW_ANIMATION);
    }
}

/*-----------------------------------------------------------------------------
    Name        : madLinkInOpenGuns
    Description : Sets up ships with gun animations to open their guns imediatly (regardless of certain other things).
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void madLinkInOpenGuns(CommandToDo *command,udword priority)
{
    sdword i;
    Ship *ship;

    for(i=0;i<command->selection->numShips;i++)
    {
        ship = command->selection->ShipPtr[i];
        madOpenGunsShip(ship);
    }
}

/*-----------------------------------------------------------------------------
    Name        : madLinkInCloseGuns
    Description : Sets the ship to imediatly begin a gun closing animation
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R6 nis-fow-camera): the ion array frigates keep their guns out (madLinkInCloseGuns 0x004a5660 skips
//      bIonArrayFrigate 0x2C and p1IonArrayFrigate 0x5E)
void madLinkInCloseGuns(CommandToDo *command,udword priority)
{
    sdword i;
    Ship *ship;

    for(i=0;i<command->selection->numShips;i++)
    {
        ship = command->selection->ShipPtr[i];
        if ((ship->shiptype != bIonArrayFrigate) && (ship->shiptype != p1IonArrayFrigate))
        {
            madLinkInCloseGunsShip(ship);
        }
    }
}

void madLinkInCloseGunsShip(Ship *ship)
{
    sdword animIndex;
    if (ship->madBindings != NULL && ship->staticinfo->madStatic != NULL)
    {
        //if ship has mesh animations associated with it

        if(ship->staticinfo->madStatic->numGunCloseIndexes < 1)
        {
            //ship doesn't have gun opening animations
            return;
        }
        if(ship->madGunStatus == MAD_STATUS_GUNS_CLOSED ||
           ship->madGunStatus == MAD_STATUS_GUNS_CLOSING)
        {
            //guns are currently open/opening...so lets ferget about it
            return;
        }
        else if(ship->madGunStatus == MAD_STATUS_GUNS_OPENING)
        {
            ship->nextAnim = MAD_ANIMATION_GUN_CLOSING;
            return;
        }

        //signal that an animation has been added to the ships cue
        bitSet(ship->madAnimationFlags,MAD_ANIMATION_NEED_PROC);

        if(ship->targetable.health*ship->staticinfo->health.oneOverMaxHealth < ship->staticinfo->madGunOpenDamagedHealthThreshold && ship->staticinfo->madStatic->numGunCloseDamagedIndexes > 0)
        {
            //ships health is below threshold
            //and ship has damaged animations as well
            animIndex = randombetween(0,(ship->staticinfo->madStatic->numGunCloseDamagedIndexes-1));
            ship->cuedAnimationIndex = ship->staticinfo->madStatic->gunCloseDamagedIndexes[animIndex];
			ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_CLOSE;    // CAT: 0x406 for every closing animation (the exe)
        }
        else
        {
            //ships health is normalish..use normal gun anims
            animIndex = randombetween(0,(ship->staticinfo->madStatic->numGunCloseIndexes-1));
            ship->cuedAnimationIndex = ship->staticinfo->madStatic->gunCloseIndexes[animIndex];
			ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_CLOSE;    // CAT: 0x406 for every closing animation (the exe)
		}
        ship->cuedAnimationType = MAD_ANIMATION_GUN_CLOSING;
        ship->madGunStatus = MAD_STATUS_GUNS_CLOSING;
        bitSet(ship->madAnimationFlags,MAD_NEED_TO_START_NEW_ANIMATION);
    }
}

/*-----------------------------------------------------------------------------
    Name        : madLinkInPostDocking
    Description : Cues up animations for ships that have finished
                  docking (opens fins, etc...)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void madLinkInPostDocking(CommandToDo *command,udword priority)
{
    sdword i;
    Ship *ship;

    for(i=0;i<command->selection->numShips;i++)
    {
        ship = command->selection->ShipPtr[i];
        madLinkInPostDockingShip(ship);
    }
}

/*-----------------------------------------------------------------------------
    Name        : madLinkInPostDockingShip
    Description : utility funciton for above function
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void madLinkInPostDockingShip(Ship *ship)
{
    sdword animIndex;
    if (ship->madBindings != NULL && ship->staticinfo->madStatic != NULL)
    {
        //if ship has mesh animations associated with it

        if(ship->staticinfo->madStatic->numDockIndexes < 1)
        {
            //ship doesn't have gun opening animations
            return;
        }

        if(ship->madWingStatus == MAD_STATUS_WINGS_OPEN ||
           ship->madWingStatus == MAD_STATUS_WINGS_OPENING)
        {
            //guns are currently open...so lets ferget about it
            return;
        }
        else if(ship->madWingStatus == MAD_STATUS_WINGS_CLOSING)
        {
            ship->nextAnim = MAD_ANIMATION_WINGS_OPENING;
            return;
        }

        //signal that an animation has been added to the ships cue
        bitSet(ship->madAnimationFlags,MAD_ANIMATION_NEED_PROC);

        if(ship->targetable.health*ship->staticinfo->health.oneOverMaxHealth < ship->staticinfo->madWingOpenDamagedHealthThreshold && ship->staticinfo->madStatic->numPostDockDamagedIndexes > 0)
        {
            //ships health is below threshold
            //and ship has damaged animations as well
            animIndex = randombetween(0,(ship->staticinfo->madStatic->numPostDockDamagedIndexes-1));
            ship->cuedAnimationIndex = ship->staticinfo->madStatic->PostDockDamagedIndexes[animIndex];
			ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_OPEN;     // CAT: 0x405 for every opening animation (the exe)
		}
        else
        {
            //ships health is normalish..use normal gun anims
            animIndex = randombetween(0,(ship->staticinfo->madStatic->numPostDockIndexes-1));
            ship->cuedAnimationIndex = ship->staticinfo->madStatic->PostDockIndexes[animIndex];
			ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_OPEN;     // CAT: 0x405 for every opening animation (the exe)
        }
        ship->cuedAnimationType = MAD_ANIMATION_WINGS_OPENING;
        ship->madWingStatus = MAD_STATUS_WINGS_OPENING;
        bitSet(ship->madAnimationFlags,MAD_NEED_TO_START_NEW_ANIMATION);
    }
}

/*-----------------------------------------------------------------------------
    Name        : madLinkInPreDocking
    Description : Cues up animations for ships that are docking (closes fins, etc...)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void madLinkInPreDocking(CommandToDo *command,udword priority)
{

    sdword i;
    Ship *ship;

    for(i=0;i<command->selection->numShips;i++)
    {
        ship = command->selection->ShipPtr[i];
        madLinkInPreDockingShip(ship);
    }
}
/*-----------------------------------------------------------------------------
    Name        : madLinkInPreDockingShip
    Description : utility function for above function
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void madLinkInPreDockingShip(Ship *ship)
{
    sdword animIndex;
    if (ship->madBindings != NULL && ship->staticinfo->madStatic != NULL)
    {
        //if ship has mesh animations associated with it

        if(ship->staticinfo->madStatic->numPostDockIndexes < 1)
        {
            //ship doesn't have gun opening animations
            return;
        }

        if(ship->madWingStatus == MAD_STATUS_WINGS_CLOSED ||
           ship->madWingStatus == MAD_STATUS_WINGS_CLOSING)
        {
            //guns are currently open...so lets ferget about it
            return;
        }
        else if(ship->madWingStatus == MAD_STATUS_WINGS_OPENING)
        {
            ship->nextAnim = MAD_ANIMATION_WINGS_CLOSING;
            return;
        }

        //signal that an animation has been added to the ships cue
        bitSet(ship->madAnimationFlags,MAD_ANIMATION_NEED_PROC);

        if(ship->targetable.health*ship->staticinfo->health.oneOverMaxHealth < ship->staticinfo->madWingOpenDamagedHealthThreshold && ship->staticinfo->madStatic->numDockDamagedIndexes > 0)
        {
            //ships health is below threshold
            //and ship has damaged animations as well
            animIndex = randombetween(0,(ship->staticinfo->madStatic->numDockDamagedIndexes-1));
            ship->cuedAnimationIndex = ship->staticinfo->madStatic->DockDamagedIndexes[animIndex];
			ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_CLOSE;    // CAT: 0x406 for every closing animation (the exe)
        }
        else
        {
            //ships health is normalish..use normal gun anims
            animIndex = randombetween(0,(ship->staticinfo->madStatic->numDockIndexes-1));
            ship->cuedAnimationIndex = ship->staticinfo->madStatic->DockIndexes[animIndex];
            ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_CLOSE;    // CAT: 0x406 for every closing animation (the exe)
        }
        ship->cuedAnimationType = MAD_ANIMATION_WINGS_CLOSING;
        ship->madWingStatus = MAD_STATUS_WINGS_CLOSING;
        bitSet(ship->madAnimationFlags,MAD_NEED_TO_START_NEW_ANIMATION);
    }
}

/*-----------------------------------------------------------------------------
    Name        : madLinkInOpenDoor
    Description : opens door on R1 mothership (for now)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void madLinkInOpenDoor(Ship *ship)
{
    sdword animIndex;
    if (ship->madBindings != NULL && ship->staticinfo->madStatic != NULL)
    {
        //if ship has mesh animations associated with it

        if(ship->staticinfo->madStatic->numDoorOpenIndexes < 1)
        {
            //ship doesn't have gun opening animations
            return;
        }

        if(ship->madDoorStatus == MAD_STATUS_DOOR_OPEN ||
           ship->madDoorStatus == MAD_STATUS_DOOR_OPENING)
        {
            //guns are currently open...so lets ferget about it
            return;
        }
        else if(ship->madDoorStatus == MAD_STATUS_DOOR_CLOSING)
        {
            //ALWAYS return;
            ship->nextAnim = MAD_ANIMATION_DOOR_OPENING;
            return;
        }

        //signal that an animation has been added to the ships cue
        bitSet(ship->madAnimationFlags,MAD_ANIMATION_NEED_PROC);

        animIndex = randombetween(0,(ship->staticinfo->madStatic->numDoorOpenIndexes-1));
        ship->cuedAnimationIndex = ship->staticinfo->madStatic->DoorOpenIndexes[animIndex];

        ship->cuedAnimationType = MAD_ANIMATION_DOOR_OPENING;
        ship->madDoorStatus = MAD_STATUS_DOOR_OPENING;
        bitSet(ship->madAnimationFlags,MAD_NEED_TO_START_NEW_ANIMATION);
		ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_OPEN;     // CAT: 0x405 for every opening animation (the exe)
    }
}

/*-----------------------------------------------------------------------------
    Name        : madLinkInCloseDoor
    Description : opens door on R1 mothership (for now)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void madLinkInCloseDoor(Ship *ship)
{
    sdword animIndex;
    if (ship->madBindings != NULL && ship->staticinfo->madStatic != NULL)
    {
        //if ship has mesh animations associated with it

        if(ship->staticinfo->madStatic->numDoorCloseIndexes < 1)
        {
            //ship doesn't have gun opening animations
            return;
        }

        if(ship->madDoorStatus == MAD_STATUS_DOOR_CLOSED ||
           ship->madDoorStatus == MAD_STATUS_DOOR_CLOSING)
        {
            //guns are currently open...so lets ferget about it
            return;
        }
        else if(ship->madDoorStatus == MAD_STATUS_DOOR_OPENING)
        {
            //ALWAYS return;
            ship->nextAnim = MAD_ANIMATION_DOOR_CLOSING;
            return;
        }

        //signal that an animation has been added to the ships cue
        bitSet(ship->madAnimationFlags,MAD_ANIMATION_NEED_PROC);

        animIndex = randombetween(0,(ship->staticinfo->madStatic->numDoorCloseIndexes-1));
        ship->cuedAnimationIndex = ship->staticinfo->madStatic->DoorCloseIndexes[animIndex];

        ship->cuedAnimationType = MAD_ANIMATION_DOOR_CLOSING;
        ship->madDoorStatus = MAD_STATUS_DOOR_CLOSING;
        bitSet(ship->madAnimationFlags,MAD_NEED_TO_START_NEW_ANIMATION);
		ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_CLOSE;    // CAT: 0x406 for every closing animation (the exe)
    }
}

/*-----------------------------------------------------------------------------
    Name        : getDoorInfo
    Description : fills in coordsys and position with the doors information for this ship
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

bool32 madLinkInGetDoorInfo(Ship *ship, matrix *coordsys, vector *position)
{
    sdword madIndex, gunIndex;
    vector positiont;
    matrix coordsyst;

    madIndex = madBindingIndexFindByName(ship->staticinfo->madStatic->header, "Hangardoor");
    gunIndex = madGunBindingIndexFindByName(ship->staticinfo, "Hangardoor");
    dbgAssertOrIgnore(madIndex != -1);
    dbgAssertOrIgnore(gunIndex != -1);
    madAnimBindingMatrix(&coordsyst,&positiont,ship,gunIndex, madIndex);

    matMultiplyMatByMat(coordsys,&ship->rotatable.rotinfo.coordsys,&coordsyst);
    //matMultiplyVecByMat(position,&positionSS,&ship->rotatable.rotinfo.coordsys);
    matMultiplyMatByVec(position,&ship->rotatable.rotinfo.coordsys,&positiont);
    vecAddTo(*position,ship->obj.posinfo.position);
    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : madLinkInOpenSpecialShip
    Description : runs the 'open' special animation of the ships.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void madLinkInOpenSpecialShip(Ship *ship)
{
    sdword animIndex;
    if (ship->madBindings != NULL && ship->staticinfo->madStatic != NULL)
    {
        //if ship has mesh animations associated with it

        if(ship->staticinfo->madStatic->numSpecialOpenIndexes < 1)
        {
            //ship doesn't have gun opening animations
            return;
        }

        if(ship->madSpecialStatus == MAD_STATUS_SPECIAL_OPEN ||
           ship->madSpecialStatus == MAD_STATUS_SPECIAL_OPENING)
        {
            //guns are currently open...so lets ferget about it
            return;
        }
        else if(ship->madSpecialStatus == MAD_STATUS_SPECIAL_CLOSING)
        {
            //toggle Reverse here!
            //Luke...Make it so.
            //for now, overwrite animation
            ship->nextAnim = MAD_ANIMATION_SPECIAL_OPENING;
            return;
        }

        //signal that an animation has been added to the ships cue
        bitSet(ship->madAnimationFlags,MAD_ANIMATION_NEED_PROC);

        if(ship->targetable.health*ship->staticinfo->health.oneOverMaxHealth < ship->staticinfo->madGunOpenDamagedHealthThreshold && ship->staticinfo->madStatic->numSpecialOpenDamagedIndexes > 0)
        {
            //ships health is below threshold
            //and ship has damaged animations as well
            animIndex = randombetween(0,(ship->staticinfo->madStatic->numSpecialOpenDamagedIndexes-1));
            ship->cuedAnimationIndex = ship->staticinfo->madStatic->specialOpenDamagedIndexes[animIndex];
            ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_OPEN;     // CAT: 0x405 for every opening animation (the exe)
        }
        else
        {
            //ships health is normalish..use normal gun anims
            animIndex = randombetween(0,(ship->staticinfo->madStatic->numSpecialOpenIndexes-1));
            ship->cuedAnimationIndex = ship->staticinfo->madStatic->specialOpenIndexes[animIndex];
			ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_OPEN;     // CAT: 0x405 for every opening animation (the exe)
        }
        ship->cuedAnimationType = MAD_ANIMATION_SPECIAL_OPENING;
        ship->madSpecialStatus = MAD_STATUS_SPECIAL_OPENING;
        bitSet(ship->madAnimationFlags,MAD_NEED_TO_START_NEW_ANIMATION);
    }
}

/*-----------------------------------------------------------------------------
    Name        : madLinkInCloseSpecialShip
    Description : sets the 'close' special animation to play
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void madLinkInCloseSpecialShip(Ship *ship)
{
    sdword animIndex;
    if (ship->madBindings != NULL && ship->staticinfo->madStatic != NULL)
    {
        //if ship has mesh animations associated with it

        if(ship->staticinfo->madStatic->numSpecialCloseIndexes < 1)
        {
            //ship doesn't have gun opening animations
            return;
        }
        if(ship->madSpecialStatus == MAD_STATUS_SPECIAL_CLOSED ||
           ship->madSpecialStatus == MAD_STATUS_SPECIAL_CLOSING)
        {
            //guns are currently open/opening...so lets ferget about it
            return;
        }
        else if(ship->madSpecialStatus == MAD_STATUS_SPECIAL_OPENING)

        {
            //cue this anim for play right after the current one finishings
            ship->nextAnim = MAD_ANIMATION_SPECIAL_CLOSING;
            return;

        }

        //signal that an animation has been added to the ships cue
        bitSet(ship->madAnimationFlags,MAD_ANIMATION_NEED_PROC);

        if(ship->targetable.health*ship->staticinfo->health.oneOverMaxHealth < ship->staticinfo->madGunOpenDamagedHealthThreshold && ship->staticinfo->madStatic->numSpecialCloseDamagedIndexes > 0)
        {
            //ships health is below threshold
            //and ship has damaged animations as well
            animIndex = randombetween(0,(ship->staticinfo->madStatic->numSpecialCloseDamagedIndexes-1));
            ship->cuedAnimationIndex = ship->staticinfo->madStatic->specialCloseDamagedIndexes[animIndex];
			ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_CLOSE;    // CAT: 0x406 for every closing animation (the exe)
        }
        else
        {
            //ships health is normalish..use normal gun anims
            animIndex = randombetween(0,(ship->staticinfo->madStatic->numSpecialCloseIndexes-1));
            ship->cuedAnimationIndex = ship->staticinfo->madStatic->specialCloseIndexes[animIndex];
			ship->soundEventAnimationTypeFlag = CAT_SHIPCMN_ANIM_CLOSE;    // CAT: 0x406 for every closing animation (the exe)
        }
        ship->cuedAnimationType = MAD_ANIMATION_SPECIAL_CLOSING;
        ship->madSpecialStatus = MAD_STATUS_SPECIAL_CLOSING;
        bitSet(ship->madAnimationFlags,MAD_NEED_TO_START_NEW_ANIMATION);
    }
}

