// =============================================================================
//  RepairCorvette.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "RepairCorvette.h"

#include "AIShip.h"
#include "AITrack.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "DefaultShip.h"
#include "SaveGame.h"
#include "SoundEvent.h"
#include "Universe.h"
#include "UnivUpdate.h"


#define DEBUG_REPAIR_CORVETTE  0


void stopRepairEffect(Ship *ship);

// CAT (M2-R4 ships-b): no RepairCorvetteHeader any more: Cataclysm has no repair corvette type (the HW1 RepairCorvette
//      id was a dormant one, gone with the other 21; no shipHeaders row ever pointed at the header). The exe keeps the
//      header's bodies as dead code (RepairCorvetteStaticInit 0x005c21e0 .. RepairCorvetteSpecialTarget 0x005c2990);
//      the repair beam and the refuel/repair loop below are the shared code the Cataclysm repairers call
//      (startRepairEffect 0x005c22c0, stopRepairEffect 0x005c23b0, ModifyRepairEffect 0x005c2410, refuelRepairShips
//      0x005c24e0).

void startRepairEffect(Ship *ship,SpaceObjRotImpTarg *target,vector *trajectory,real32 distance)
{
    ShipStaticInfo *shipstatic = (ShipStaticInfo *)ship->staticinfo;
    RepairNozzleStatic *repnozzlestatic = shipstatic->repairNozzleStatic;
    //TURN ON EFFECT HERE
    //
    //Use ship->rceffect, since ship isn't a resource collector,
    //the pointer isn't being used
    etglod *etgLOD = etgSpecialPurposeEffectTable[EGT_REPAIR_BEAM];

    etgeffectstatic *stat;
    sdword LOD;
    udword intLength;
    udword intWidth;
    matrix coordsys;
    real32 targetRadius = target->staticinfo->common.staticCollInfo.collspheresize;
    vector repairBeamPosition;

    matMultiplyMatByVec(&repairBeamPosition,&ship->rotatable.rotinfo.coordsys,&repnozzlestatic->position);

    matCreateCoordSysFromHeading(&coordsys,trajectory);

    intLength = Real32ToUdword(distance);
    intWidth = Real32ToUdword(targetRadius);

    vecAddTo(repairBeamPosition,ship->obj.posinfo.position);

    //create an effect for bullet, if applicable
    if (etgLOD != NULL)
    {
        LOD = ship->obj.currentLOD;
        if (LOD >= etgLOD->nLevels)
        {
            stat = NULL;
        }
        else
        {
            stat = etgLOD->level[LOD];
        }
    }
    else
    {
        stat = NULL;
    }
    #if ETG_DISABLEABLE
    if (stat != NULL && etgEffectsEnabled && !etgFrequencyExceeded(stat))
    #else
    if (stat != NULL && !etgFrequencyExceeded(stat))
    #endif
    {
        ship->rceffect = etgEffectCreate(stat, ship, &repairBeamPosition, &ship->obj.posinfo.velocity, &coordsys, 1.0f, EAF_Velocity | EAF_NLips, 2, intLength, intWidth);
    }
    else
    {
        ship->rceffect = NULL;                              //play no effect
    }


}
void stopRepairEffect(Ship *ship)
{

#ifdef HW_BUILD_FOR_DEBUGGING
    //make sure we only entered here if effect was playing!
    dbgAssertOrIgnore(ship->rceffect != NULL);
#endif

    if (bitTest(ship->rceffect->staticinfo->specialOps, ESO_SelfDeleting))
    {                                                       //if the effect will delete itself
        ((real32 *)ship->rceffect->variable)[ETG_ResourceDurationParam] =
            ship->rceffect->timeElapsed;                            //time it out
    }
    else
    {                                                       //else it's a free-running effect... delete it
        etgEffectDelete(ship->rceffect);
        univRemoveObjFromRenderList((SpaceObj *)ship->rceffect);
        listDeleteNode(&ship->rceffect->header.objlink);
    }

    //soundEventStop(ship->soundevent.specialHandle);

    ship->rceffect = NULL;
}

void ModifyRepairEffect(Effect *effect,Ship *ship,vector *trajectory,real32 distance, SpaceObjRotImpTarg *target)
{
    ShipStaticInfo *shipstatic = (ShipStaticInfo *)ship->staticinfo;
    RepairNozzleStatic *repnozzlestatic = shipstatic->repairNozzleStatic;
    vector repairBeamPosition;

    dbgAssertOrIgnore(effect);

    matMultiplyMatByVec(&repairBeamPosition,&ship->rotatable.rotinfo.coordsys,&repnozzlestatic->position);

    vecAdd(effect->obj.posinfo.position,repairBeamPosition,ship->obj.posinfo.position);

    matCreateCoordSysFromHeading(&effect->rotatable.rotinfo.coordsys,trajectory);

	distance = max(200.0f,distance);
	if(target->header.objtype == OBJ_ShipType)
	{
		// CAT: any mothership-class target (ModifyRepairEffect 0x005c2410; HW1: the Mothership type)
		if(((Ship *)target)->staticinfo->shipclass == CLASS_Mothership)
		{
			distance = max(800.0f,distance);
		}
	}


    ((real32 *)effect->variable)[ETG_ResourceLengthParam] = distance;
    ((real32 *)effect->variable)[ETG_ResourceRadiusParam] = target->staticinfo->common.staticCollInfo.collspheresize;
}

//function will return the most heavily damaged target in the list, OR
//if all are fully healthy, it will return the FIRST target in the list
bool32 areAllHealthy(Ship *ship,SelectAnyCommand *targets)
{
    sdword i;
    for(i=0;i<targets->numTargets;i++)
    {
        if(targets->TargetPtr[i]->health != targets->TargetPtr[i]->staticinfo->maxhealth)
        {
            return FALSE;
        }
    }
    return (TRUE);
}
bool32 refuelRepairShips(Ship *ship, SelectAnyCommand *targets,real32 rangetoRefuel)
{
    //remove unwantedships from this selection
    //optimize by doing only once for this ship
    SelectCommand selectOne;
    sdword i;
    SpaceObjRotImpTarg *target,*targettemp;
    vector trajectory,curheading;
    real32 range,dotprod;
    CommandToDo *targetCommand;

    //filter target list for unrepairable ships!

    for(i=0;i<targets->numTargets;)
    {
        if(targets->TargetPtr[i]->header.objtype != OBJ_ShipType)
        {
            targets->numTargets--;
            targets->TargetPtr[i] = targets->TargetPtr[targets->numTargets];
            continue;
        }

        //object is a ship
        Ship* targetShip = (Ship *)targets->TargetPtr[i];
        targetCommand = getShipAndItsCommand(&universe.mainCommandLayer, targetShip);
        if(targetCommand != NULL)
        {
            if(targetCommand->ordertype.order == COMMAND_DOCK)
            {
                //remove docking ships from the list
                if(targetShip->shipOrDerelict.dockingship != NULL)
                {
                    //only if in final stages
                    targets->numTargets--;
                    targets->TargetPtr[i] = targets->TargetPtr[targets->numTargets];
                    continue;
                }
            }
        }

        if((targetShip->staticinfo->shipclass != CLASS_Fighter &&
            targetShip->staticinfo->shipclass != CLASS_Corvette) ||
            targetShip->playerowner != ship->playerowner)
        {
            if(ship->staticinfo->repairBeamCapable)
            {
                //repair corvette can repair capital ships!
                //class doesn't matter
                if(targetShip->playerowner == ship->playerowner)
                {
                    //ship is not a fighter or a corvette, but is ours, so we can fix it!
                    i++;
                    continue;
                }
            }
            targets->numTargets--;
            targets->TargetPtr[i] = targets->TargetPtr[targets->numTargets];
        }
        else
        {
            i++;
        }
    }

    if (targets->numTargets <= 0)
    {
        return TRUE;
    }



    target = targets->TargetPtr[0];

    // CAT: a ship never repairs itself (refuelRepairShips 0x005c24e0 returns FALSE when the first target is the
    //      repairing ship)
    if (target == (SpaceObjRotImpTarg *)ship)
    {
        return FALSE;
    }

//need to add
//repairBeamCapable
//    real32 healthPerSecond,CapitalDistanceRepairStart;
//    real32 CapitalDistanceRepairStart2;

    if(ship->staticinfo->repairBeamCapable)
    {
        //assume target is a ship!!!!
        dbgAssertOrIgnore(target->header.objtype == OBJ_ShipType);
        Ship* targetShip = (Ship*)target;

        //targets might be a capital ship
        if(targetShip->staticinfo->shipclass != CLASS_Fighter)
        {
            //target is such that it should be BEAM repaired!
            //so lets fly upto it and ZAP repair it!
            real32 range1,range2;
            aishipGetTrajectory(ship,target,&trajectory);
            range = RangeToTarget(ship,target,&trajectory);

            vecNormalize(&trajectory);
            aitrackHeading(ship,&trajectory,0.999f);

            // CAT: any mothership-class target doubles the distances (HW1: the Mothership type) (0x005c24e0)
            if(targetShip->staticinfo->shipclass == CLASS_Mothership)
            {
                range1 = 2*ship->staticinfo->CapitalDistanceRepairStart;
                range2 = 2*ship->staticinfo->CapitalDistanceRepairStart2;
            }
            else
            {
                range1 = ship->staticinfo->CapitalDistanceRepairStart;
                range2 = ship->staticinfo->CapitalDistanceRepairStart2;
            }
            //we add 300 to the range checker for a sort of hysterysis!
            if(range > range1)
            {
                aishipFlyToShipAvoidingObjs(ship,target,AISHIP_PointInDirectionFlying + AISHIP_FastAsPossible,0.0f);
                if(range > range2)
                {
                    //far enough away to stop the effect and ONLY fly towards ship
                    if(ship->rceffect != NULL)
                    {
                        //turn off ships effect
                        stopRepairEffect(ship);
                    }
                    return FALSE;
                }
            }
            else
            {
                aitrackZeroVelocity(ship);
            }

            //in range to repair this li'l ship


            matGetVectFromMatrixCol3(curheading,ship->rotatable.rotinfo.coordsys);
            dotprod = vecDotProduct(trajectory,curheading);

            if(!areAllHealthy(ship,targets))
            {
                if(dotprod > ship->staticinfo->AngleDotProdThreshold)
                {
                    //within angle of 'repair beam'
                    if(ship->rceffect == NULL)
                    {
                        //turn on effect if we should!
                        startRepairEffect(ship,target,&trajectory,range);
                    }
                    else
                    {
                        //fix up effect
                        ModifyRepairEffect(ship->rceffect,ship,&trajectory,range, target);
                    }
                    // CAT: an NIS ship is not healed (0x005c24e0 tests SOF_NISShip)
                    if (!bitTest(target->header.flags, SOF_NISShip))
                    {
                        target->health+=ship->staticinfo->healthPerSecond*universe.phystimeelapsed;
                    }
                    if(targetShip->targetable.health > targetShip->staticinfo->health.maxhealth)
                    {
                        if(ship->rceffect != NULL)
                        {
                            //turn off ships effect
                            stopRepairEffect(ship);
                        }
                        target->health = targetShip->staticinfo->health.maxhealth;

                        //fix this in a bit
                        targettemp = targets->TargetPtr[0];
                        for(i=0;i<(targets->numTargets-1);i++)
                        {
                            targets->TargetPtr[i]=targets->TargetPtr[i+1];
                        }
                        targets->TargetPtr[targets->numTargets-1] = targettemp;

                        //if(targets->numTargets <= 0)
                        //    return TRUE;
                        //targets->TargetPtr[0] = targets->TargetPtr[targets->numTargets];
                        return FALSE;
                    }
                }
                else
                {
                    //not pointing at ship, turn off effect and other
                    //needed things!
                    if(ship->rceffect != NULL)
                    {
                        //turn off ships effect
                        stopRepairEffect(ship);
                    }
                }
            }
            else
            {
                if(ship->rceffect != NULL)
                {
                    //turn off ships effect
                    stopRepairEffect(ship);
                }
            }
            return FALSE;
        }
    }

    aishipGetTrajectory(ship,target,&trajectory);
    aishipFlyToShipAvoidingObjs(ship,target,AISHIP_PointInDirectionFlying + AISHIP_FastAsPossible +AISHIP_FirstPointInDirectionFlying,0.0f);
    range = RangeToTarget(ship,target,&trajectory);

    if(range <= rangetoRefuel)
    {
        //TODO(type safety, 2026-04-26): Make `target` a Ship* from the get-go. The for loop at the top of this function filters out anything that isn't
        dbgAssertOrIgnore(target->header.objtype == OBJ_ShipType);
        Ship* targetShip = (Ship*)target;
        //within repair range
        selectOne.numShips = 1;
        selectOne.ShipPtr[0] = targetShip;
        targetShip->fuel += 300.0f;   //??? appropriate ammount?
        if(targetShip->fuel > targetShip->staticinfo->maxfuel)
            targetShip->fuel = targetShip->staticinfo->maxfuel;

        //"speech event?  Switching to Emergency Docking Fuel Supply?
        //cldock
        clDock(&universe.mainCommandLayer,&selectOne,CAT_DOCK_SUPPORT,ship);    // the exe's purpose 0 with dockwith = ship (refuelRepairShips 0x005c1d10)
        targets->numTargets--;
        if(targets->numTargets <= 0)
            return TRUE;
        targets->TargetPtr[0] = targets->TargetPtr[targets->numTargets];
    }

    return FALSE;
}
