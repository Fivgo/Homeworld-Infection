// =============================================================================
//  GravWellGenerator.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 01/06/1998 by bpasechnik
// =============================================================================

#include "GravWellGenerator.h"

#include "Attack.h"
#include "Battle.h"
#include "Blobs.h"
#include "FastMath.h"
#include "Randy.h"
#include "MadLinkIn.h"
#include "Memory.h"
#include "SaveGame.h"
#include "Select.h"
#include "SoundEvent.h"
#include "Universe.h"
#include "UnivUpdate.h"


typedef struct
{
    Node objnode;
    Ship *ship;
    sdword stoppingstate;
    real32 xangle;
    real32 yangle;
    real32 zangle;
} GravStruct;


GravWellGeneratorStatics GravWellGeneratorStatic;

// CAT: Kushan (0x00a7ada0) and every other race (0x00a7ade0, i.e. Taiidan) (GravWellGeneratorStaticInit 0x005b40a0);
//      HW1 had R1/R2
GravWellGeneratorStatics GravWellGeneratorStaticKushan;
GravWellGeneratorStatics GravWellGeneratorStaticTaiidan;

//after effect is spawned, wait between these two numbers before re-spawning
#define gravwellEffectLifeLow       3.0f
#define gravwellEffectLifeHigh      5.0f

#define GRAV_TIME_BEFORE_EXPLODE    3.5f

#define GRAV_BEGIN      0
#define GRAV_STOPPED    2
#define GRAV_STAGE2     1

scriptStructEntry GravWellGeneratorStaticScriptTable[] =
{
    { "GravWellRadius",    scriptSetReal32CB,  &(GravWellGeneratorStatic.GravWellRadius),  &(GravWellGeneratorStatic) },
    { "OperationTime",    scriptSetReal32CB,  &(GravWellGeneratorStatic.OperationTime),  &(GravWellGeneratorStatic) },
    { "EffectConstant",    scriptSetReal32CB,  &(GravWellGeneratorStatic.EffectConstant),  &(GravWellGeneratorStatic) },
    { "scanrate",    scriptSetUdwordCB,  &(GravWellGeneratorStatic.scanrate),  &(GravWellGeneratorStatic) },
    { "repulseForce",    scriptSetReal32CB,  &(GravWellGeneratorStatic.repulseForce),  &(GravWellGeneratorStatic) },
    { "warmupdowntime",    scriptSetReal32CB,  &(GravWellGeneratorStatic.warmupdowntime),  &(GravWellGeneratorStatic) },
    { "xrot",    scriptSetReal32CB,  &(GravWellGeneratorStatic.xrot),  &(GravWellGeneratorStatic) },
    { "yrot",    scriptSetReal32CB,  &(GravWellGeneratorStatic.yrot),  &(GravWellGeneratorStatic) },
    { "zrot",    scriptSetReal32CB,  &(GravWellGeneratorStatic.zrot),  &(GravWellGeneratorStatic) },

    END_SCRIPT_STRUCT_ENTRY
};

void GravWellGeneratorStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    // CAT: Kushan, else the Taiidan block (GravWellGeneratorStaticInit 0x005b40a0)
    GravWellGeneratorStatics *GravWellGeneratorstat = (statinfo->shiprace == RACE_Kushan) ? &GravWellGeneratorStaticKushan : &GravWellGeneratorStaticTaiidan;

    statinfo->custstatinfo = GravWellGeneratorstat;
    scriptSetStruct(directory,filename,GravWellGeneratorStaticScriptTable,(ubyte *)GravWellGeneratorstat);
    GravWellGeneratorstat->GravWellRadiusSqr = GravWellGeneratorstat->GravWellRadius*GravWellGeneratorstat->GravWellRadius;
}

void GravWellGeneratorInit(Ship *ship)
{
    GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;
    spec->GravFieldOn = FALSE;
    spec->GravFired = FALSE;
    spec->ready = TRUE;
    spec->TimeOn = 0.0f;
    spec->GravDerelict = FALSE;
    spec->gravityEffect = NULL;
    listInit(&spec->GravList);
}

//ship is the ship to have the gravwell effect played around
void runShipEffect(Ship *ship)
{
    sdword LOD;
    etglod *etgLOD;
    etgeffectstatic *stat;

    etgLOD = etgSpecialPurposeEffectTable[EGT_CAUGHT_GRAVWELL];
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
     if (univSpaceObjInRenderList((SpaceObj *)ship))
     {
         // CAT: the frequency limit applies in every build (runShipEffect 0x005b4130 always calls
         //      etgFrequencyExceeded; HW1 only with ETG_DISABLEABLE)
    #if ETG_DISABLEABLE
         if (stat != NULL && etgEffectsEnabled && !etgFrequencyExceeded(stat))
    #else
         if (stat != NULL && !etgFrequencyExceeded(stat))
    #endif
         {
             udword colSizeDword;
             dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
             colSizeDword = Real32ToUdword(ship->staticinfo->common.staticCollInfo.collspheresize);
             etgEffectCreate(stat, ship, &ship->obj.posinfo.position, &ship->obj.posinfo.velocity, &ship->rotatable.rotinfo.coordsys, ship->magnitudeSquared, EAF_Full, 1, colSizeDword);
			 {
				real32 realtemp;
				udword temp;
				etgarg randomArgs[2] = {Real32ToUdword(gravwellEffectLifeLow), Real32ToUdword(gravwellEffectLifeHigh)};   // CAT: etgFRandom takes the ETG argument array
				temp = (udword)etgFRandom(randomArgs);
				realtemp = UdwordToReal32(temp);
				ship->gravwellTimeEffect = universe.totaltimeelapsed + realtemp;
			 }
         }
     }

}
void GravWellGeneratorDied(Ship *ship)
{
    GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;
    Node *gravnode;
    GravStruct *gravstruct;

    gravnode = spec->GravList.head;

    while(gravnode != NULL)
    {
       gravstruct = (GravStruct *)listGetStructOfNode(gravnode);
       //do maintenance here if any needed, otherwise remove loop :)
       bitClear(gravstruct->ship->dontapplyforceever,1);
       bitClear(gravstruct->ship->dontrotateever,1);
       bitClear(gravstruct->ship->shipOrDerelict.specialFlags2, SPECIAL_2_ShipInGravwell);
       gravnode = gravnode->next;
    }
    listDeleteAll(&spec->GravList);        //delete the list
    // CAT: the field effect is detached and timed out here too (GravWellGeneratorDied 0x005b41e0: effect +0xE8 owner
    //      = NULL, duration parameter = time elapsed, spec effect = NULL)
    if (spec->gravityEffect != NULL)
    {
        spec->gravityEffect->owner = NULL;
        ((real32 *)spec->gravityEffect->variable)[ETG_SpecialDurationParam] = spec->gravityEffect->timeElapsed;
        spec->gravityEffect = NULL;
    }
}

bool32 gravwellIsShipStuckForHyperspaceing(Ship *ship)
{
    //multiplayer game...lets test if in grav well..if we are, we'll pause and let us get pummeled!
    Ship *posShip;
    sdword i;
    for(i=0;i<ship->obj.collMyBlob->blobShips->numShips;i++)
    {
        posShip = ship->obj.collMyBlob->blobShips->ShipPtr[i];
        // CAT: both Cataclysm gravwell types (gravwellIsShipStuckForHyperspaceing 0x005b4260 tests 0x40 / 0x4F)
        if((posShip->shiptype == kGravWellGenerator) || (posShip->shiptype == tGravWellGenerator))
        {
            //SHIP IS A GRAV WELL!
            if(((GravWellGeneratorSpec *)(posShip->ShipSpecifics))->GravFieldOn)
            {
                //GRAV WELL IS ON!
                //probably in it, but lets see...
                vector sep;
                real32 distsqr;
                vecSub(sep,ship->obj.posinfo.position,posShip->obj.posinfo.position);
                distsqr = vecMagnitudeSquared(sep);
                if(distsqr < (((GravWellGeneratorStatics *)posShip->staticinfo->custstatinfo)->GravWellRadiusSqr))
                {
                    //we are inside it...so don't update this ships HSing!
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}



void turnoffGravwell(Ship *ship)
{
    //turning the gravwell generator off
    /////////////////////////
    //Speech Event here:
    //event num: COMM_Grav_Off
    //use battle chatter
    GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;
    sdword LOD;
    etglod *etgLOD;
    etgeffectstatic *stat;
    if(ship->playerowner->playerIndex == universe.curPlayerIndex)
    {
        if (battleCanChatterAtThisTime(BCE_COMM_Grav_Off, ship))
        {
            battleChatterAttempt(SOUND_EVENT_DEFAULT, BCE_COMM_Grav_Off, ship, SOUND_EVENT_DEFAULT);
        }
    }


    //////////////////////////

    madLinkInCloseSpecialShip(ship);
    // CAT: Cataclysm's gravwell-off sound, ship event 0x80F (turnoffGravwell 0x005b42f0)
    soundEventPlay(ship, 0x80F, NULL);
    spec->GravFieldOn = FALSE;
    GravWellGeneratorDied(ship); //not literal...just cleans up linked list of ships
    //make the continuous effect time out by modifying the duration parameter directly
    if (spec->gravityEffect != NULL)
    {                                               //if there is a continuous effect playing here
        ((real32 *)spec->gravityEffect->variable)[ETG_SpecialDurationParam] =
            spec->gravityEffect->timeElapsed;       //time the effect out
        spec->gravityEffect = NULL;
    }
    //spawn a gravwell off effect
    etgLOD = etgSpecialPurposeEffectTable[EGT_GRAVWELL_OFF];
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
    if (univSpaceObjInRenderList((SpaceObj *)ship))
    {
#if ETG_DISABLEABLE
        if (stat != NULL && etgEffectsEnabled)
#else
        if (stat != NULL)
#endif
        {                                       //if there is a gravwell off effect for this LOD
            dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
            etgEffectCreate(stat, ship, &ship->obj.posinfo.position, NULL, &ship->rotatable.rotinfo.coordsys, ship->magnitudeSquared, EAF_Full, 1, ship->shiprace);
        }
    }
}
bool32 GravWellGeneratorSpecialActivate(Ship *ship)
{
    sdword LOD;
    etglod *etgLOD;
    etgeffectstatic *stat;
    GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;
    GravWellGeneratorStatics *gravwellgeneratorstatics;
    gravwellgeneratorstatics = (GravWellGeneratorStatics *) ((ShipStaticInfo *)(ship->staticinfo))->custstatinfo;

    //SelectCommand selection;
    if (spec->GravFired == FALSE)
    {   //Grav Generator hasn't expired yet
        spec->ready = FALSE;
        if(spec->GravFieldOn)
        {
            turnoffGravwell(ship);

        }
        else
        {                                                   //else turning it on
            //selection.numShips = 1;
            //selection.ShipPtr[0] = ship;
            //clHalt(&universe.mainCommandLayer,&selection);
            if(spec->TimeOn < gravwellgeneratorstatics->OperationTime)
            {
                madLinkInOpenSpecialShip(ship);
                // CAT: Cataclysm's grav-on command speech (Fleet Command event 0x0400089E) and its gravwell-on sound,
                //      ship event 0x80E (GravWellGeneratorSpecialActivate 0x005b4430)
                speechEventQueue(ship, 0x0400089E, 0, -1, -1, -1, -1, 0.0f, -1);
                soundEventPlay(ship, 0x80E, NULL);
                spec->GravFieldOn = TRUE;
                //create the looping "gravity on" effect
                etgLOD = etgSpecialPurposeEffectTable[EGT_GRAVWELL_ON];
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
                if (univSpaceObjInRenderList((SpaceObj *)ship))
                {
#if ETG_DISABLEABLE
                    if (stat != NULL && etgEffectsEnabled)
#else
                    if (stat != NULL)
#endif
                    {                                       //if there is a gravwell on effect
                        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
                        spec->gravityEffect = etgEffectCreate(stat, ship, &ship->obj.posinfo.position, NULL, &ship->rotatable.rotinfo.coordsys, ship->magnitudeSquared, EAF_Full, 1, ship->shiprace);
                        if (((real32 *)spec->gravityEffect->variable)[ETG_SpecialDurationParam] != 999999.0f)
                        {                                   //if it's not one of those free-running effects
                            spec->gravityEffect = NULL;     //don't try to time it out later
                        }
                    }
                }
            }
        }
    }

    return TRUE;
}

void GravWellGeneratorJustDisabled(Ship *ship)
{
    GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;

    if(spec->GravFieldOn)
    {                                                   //turning the gravwell generator off
        GravWellGeneratorSpecialActivate(ship);
    }
}

bool32 ShipInGravField(Ship *ship, Ship *objinquestion)
{
    GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;
    Node *gravnode;
    GravStruct *gravstruct;

    gravnode = spec->GravList.head;

    while(gravnode != NULL)
    {
        gravstruct = (GravStruct *)listGetStructOfNode(gravnode);
        if (gravstruct->ship == objinquestion)
        {     //if ship is already in list
            return(FALSE);             //
        }

        gravnode = gravnode->next;
    }
    return(TRUE);
}

void GravWellGeneratorAddObj(Ship *ship,  Ship *objtoadd)
{                       //Adds shiptoadd to 'ship's cloaklist
    GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;  //maybe don't need
    GravStruct *newgravstruct;
    GravWellGeneratorStatics *gravwellgeneratorstatics;


    gravwellgeneratorstatics = (GravWellGeneratorStatics *) ((ShipStaticInfo *)(ship->staticinfo))->custstatinfo;

    if(bitTest(objtoadd->header.flags,SOF_Dead))
    {
        return;     //don't add dead objects...
    }

    //Need to check if already in list!!
    if (!ShipInGravField(ship,objtoadd))
    {   //Ship is already in field, so don't add
        //re set variable in case it was claered b4.
        bitSet(objtoadd->shipOrDerelict.specialFlags2,SPECIAL_2_ShipInGravwell);
        return;
    }

    //////////////////////
    //speech event ships stuck in gravwell
    //need to definatly limit with battle chatter!
    //event define:   STAT_Strike_StuckInGrav
    // CAT: a fleet-level "stuck in gravwell" chatter with no can-chatter test (GravWellGeneratorAddObj 0x005b4570)
    if(objtoadd->playerowner->playerIndex == universe.curPlayerIndex)
    {
        battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_STAT_Strike_StuckInGrav, SOUND_EVENT_DEFAULT, NULL);
    }

    ////////////////////////

    newgravstruct = memAlloc(sizeof(GravStruct),"GravStruct",0);

    newgravstruct->ship = objtoadd;
    newgravstruct->stoppingstate = GRAV_BEGIN;
    newgravstruct->xangle = frandombetween(-gravwellgeneratorstatics->xrot,gravwellgeneratorstatics->xrot);
    newgravstruct->yangle = frandombetween(-gravwellgeneratorstatics->yrot,gravwellgeneratorstatics->yrot);
    newgravstruct->zangle = frandombetween(-gravwellgeneratorstatics->zrot,gravwellgeneratorstatics->zrot);
    listAddNode(&spec->GravList,&newgravstruct->objnode,newgravstruct);


    bitSet(objtoadd->shipOrDerelict.specialFlags2,SPECIAL_2_ShipInGravwell);

}

void GravAddObjectsInProximity(Ship *gravship)
{
    Node *objnode = universe.ShipList.head;
    GravWellGeneratorStatics *gravwellgeneratorstatics;
    Ship *ship;

    real32 distanceSqr;
    vector diff;

    gravwellgeneratorstatics = (GravWellGeneratorStatics *) ((ShipStaticInfo *)(gravship->staticinfo))->custstatinfo;

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);

        // CAT: corvettes, fighters, micro-class ships and sWorker are caught; the two SalCap corvettes are not
        //      (GravAddObjectsInProximity 0x005b46c0; HW1: corvettes and fighters, not the SalCapCorvette type)
        if(ship->staticinfo->shipclass == CLASS_Corvette ||
           ship->staticinfo->shipclass == CLASS_Fighter ||
           ship->staticinfo->shipclass == CLASS_Micro ||
           ship->shiptype == sWorker)
        {
            if((ship->shiptype == kSalCapCorvette) || (ship->shiptype == tSalCapCorvette))
                goto nextnode;
            //later, do a cubular check instead?
            vecSub(diff,ship->obj.posinfo.position,gravship->obj.posinfo.position);
            distanceSqr = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
            if(distanceSqr <= gravwellgeneratorstatics->GravWellRadiusSqr)
            {
                GravWellGeneratorAddObj(gravship,  ship);
            }
        }
nextnode:
        objnode = objnode->next;
    }
}

void GravWellGeneratorRemoveShipReferences(Ship *ship,Ship *shiptoremove)
{
  GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;
  Node *gravnode;
  GravStruct *gravstruct;

  gravnode = spec->GravList.head;

  while(gravnode != NULL)
  {
    gravstruct = (GravStruct *)listGetStructOfNode(gravnode);
    if (gravstruct->ship == shiptoremove)
    {
        bitClear(gravstruct->ship->dontapplyforceever,1);
        bitClear(gravstruct->ship->dontrotateever,1);
        bitClear(shiptoremove->shipOrDerelict.specialFlags2,SPECIAL_2_ShipInGravwell);
        listDeleteNode(gravnode);     //delete nodes memory...may not work as I think...ask gary..
        break;             //done so break and return
    }
    gravnode = gravnode->next;
  }
}

void GravWellClose(Ship *ship)
{
    GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;
    listDeleteAll(&spec->GravList);        //delete the list
}

#define speedlimit  0.7f
void GravWellGeneratorHouseKeep(Ship *ship)
{
    GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;
    GravWellGeneratorStatics *gravwellgeneratorstatics;
    Node *gravnode;
    GravStruct *gravstruct;
    vector diff;
    real32 distanceSqr;
    real32 distance,oneOverDistance;
    vector gravToShip;
    sdword LOD;
    etglod *etgLOD;
    etgeffectstatic *stat;

    //don't do any processing if a derelict now
    if(spec->GravDerelict == TRUE)
    {
        //may need to clear selectionbit each frame,
        //we'll see...
        return;
    }

    if(spec->GravFieldOn == TRUE)
    {
        gravwellgeneratorstatics = (GravWellGeneratorStatics *) ((ShipStaticInfo *)(ship->staticinfo))->custstatinfo;
        spec->ready = TRUE;
        // CAT: only an ownerless gravwell uses up its operation time (GravWellGeneratorHouseKeep 0x005b47e0 adds
        //      phystimeelapsed only when playerowner, Ship+0x32C, is NULL)
        if (ship->playerowner == NULL)
        {
            spec->TimeOn += universe.phystimeelapsed;
        }
        if(spec->TimeOn >= gravwellgeneratorstatics->OperationTime)
        {   //Grav Generator has used up its time so shut it down
            // CAT: Cataclysm's collapse status speech (event 0x0200089F) (0x005b47e0)
            speechEventQueue(ship, 0x0200089F, 0, -1, -1, -1, -1, 0.0f, -1);
            GravWellGeneratorDied(ship);
            spec->GravFieldOn = FALSE;
            spec->GravDerelict = TRUE;
            ship->deathtime= universe.totaltimeelapsed+GRAV_TIME_BEFORE_EXPLODE;

            //derelictize ship
            bitSet(ship->header.flags,SOF_Disabled);
            bitClear(ship->header.flags,SOF_Selectable);
            clRemoveShipFromSelection(&selSelected,ship);
            //make the effect time out by modifying the duration parameter directly
            if (spec->gravityEffect != NULL)
            {
                ((real32 *)spec->gravityEffect->variable)[ETG_SpecialDurationParam] =
                    spec->gravityEffect->timeElapsed;       //time the effect out
                spec->gravityEffect = NULL;
            }
        }
        else if (UNIVERSE_WOODPECKER(gravwellgeneratorstatics->scanrate, gravwellgeneratorstatics->scanrate))
        {       //15th frame..so do a search on other ships withing 'proximity'
            if (spec->gravityEffect == NULL)
            {
                //GRAVWELL WASN'T ON WHEN NEARBY...LETS TURN IT ON!
etgLOD = etgSpecialPurposeEffectTable[EGT_GRAVWELL_ON];
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
                if (univSpaceObjInRenderList((SpaceObj *)ship))
                {
#if ETG_DISABLEABLE
                    if (stat != NULL && etgEffectsEnabled)
#else
                    if (stat != NULL)
#endif
                    {                                       //if there is a gravwell on effect
                        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
                        spec->gravityEffect = etgEffectCreate(stat, ship, &ship->obj.posinfo.position, NULL, &ship->rotatable.rotinfo.coordsys, ship->magnitudeSquared, EAF_Full, 1, ship->shiprace);
                        if (((real32 *)spec->gravityEffect->variable)[ETG_SpecialDurationParam] != 999999.0f)
                        {                                   //if it's not one of those free-running effects
                            spec->gravityEffect = NULL;     //don't try to time it out later
                        }
                    }
                }

            }

			GravAddObjectsInProximity(ship);
        }

        gravnode = spec->GravList.head;

        while(gravnode != NULL)
        {
            gravstruct = (GravStruct *)listGetStructOfNode(gravnode);

            vecSub(diff,gravstruct->ship->obj.posinfo.position,ship->obj.posinfo.position);
            distanceSqr = diff.x*diff.x + diff.y*diff.y + diff.z*diff.z;
            gravToShip = diff;
            if(distanceSqr >= gravwellgeneratorstatics->GravWellRadiusSqr)
            {
                gravnode = gravnode->next;
                GravWellGeneratorRemoveShipReferences(ship,  (Ship *) gravstruct->ship);
                continue;
            }
            if(bitTest(gravstruct->ship->header.flags,SOF_Dead))
            {
                gravnode = gravnode->next;
                GravWellGeneratorRemoveShipReferences(ship,  (Ship *) gravstruct->ship);
                continue;
            }
            //play gravwell effect
            if(universe.totaltimeelapsed > gravstruct->ship->gravwellTimeEffect)
            {
                //time to run the effect again
                runShipEffect((Ship *)gravstruct->ship);
            }


            bitSet(gravstruct->ship->dontapplyforceever,1);
            switch(gravstruct->stoppingstate)
            {
            case GRAV_BEGIN:
                vecScalarMultiply(gravstruct->ship->obj.posinfo.velocity,gravstruct->ship->obj.posinfo.velocity,gravwellgeneratorstatics->EffectConstant);
                if(vecMagnitudeSquared(gravstruct->ship->obj.posinfo.velocity) < 2500)
                {
                    gravstruct->stoppingstate = GRAV_STAGE2;
                }
                break;
            case GRAV_STAGE2:
                gravstruct->ship->autostabilizeship = FALSE;
                if(gravstruct->ship->rotatable.rotinfo.torque.x == 0.0 &&
                    gravstruct->ship->rotatable.rotinfo.torque.y == 0.0 &&
                    gravstruct->ship->rotatable.rotinfo.torque.z == 0.0)
                {
                    if(gravstruct->ship->rotatable.rotinfo.rotspeed.x < speedlimit &&
                        gravstruct->ship->rotatable.rotinfo.rotspeed.x > -speedlimit)
                    {
                        gravstruct->ship->rotatable.rotinfo.torque.x = gravstruct->xangle;
                    }
                    if(gravstruct->ship->rotatable.rotinfo.rotspeed.y < speedlimit &&
                        gravstruct->ship->rotatable.rotinfo.rotspeed.y > -speedlimit)
                    {
                        gravstruct->ship->rotatable.rotinfo.torque.y = gravstruct->yangle;
                    }
                    if(gravstruct->ship->rotatable.rotinfo.rotspeed.z < speedlimit &&
                        gravstruct->ship->rotatable.rotinfo.rotspeed.z > -speedlimit)
                    {
                        gravstruct->ship->rotatable.rotinfo.torque.z = gravstruct->zangle;
                    }

                }
                distance = fsqrt(distanceSqr);
                oneOverDistance = 1.0f/distance;
                gravToShip.x = gravToShip.x*oneOverDistance;
                gravToShip.y = gravToShip.y*oneOverDistance;
                gravToShip.z = gravToShip.z*oneOverDistance;
                vecScalarMultiply(gravstruct->ship->obj.posinfo.force,gravToShip,gravwellgeneratorstatics->repulseForce);

                break;
            default:
                dbgFatalf(DBG_Loc,"Unknown gravwellgenerator stopping state %d",gravstruct->stoppingstate);
                break;
            }

            gravnode = gravnode->next;
        }
    }
}

void SaveGravStruct(GravStruct *gravStruct)
{
    SaveChunk *chunk;
    GravStruct *savecontents;

    chunk = CreateChunk(BASIC_STRUCTURE,sizeof(GravStruct),gravStruct);
    savecontents = (GravStruct *)chunkContents(chunk);

    savecontents->ship = (Ship *)SpaceObjRegistryGetID((SpaceObj *)savecontents->ship);

    SaveThisChunk(chunk);
    memFree(chunk);
}

void GravWellGenerator_Save(Ship *ship)
{
    GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;
    Node *node = spec->GravList.head;
    sdword cur = 0;

    SaveInfoNumber(spec->GravList.num);

    while (node != NULL)
    {
        cur++;
        SaveGravStruct((GravStruct *)listGetStructOfNode(node));
        node = node->next;
    }

    dbgAssertOrIgnore(cur == spec->GravList.num);
}

GravStruct *LoadGravStruct(void)
{
    SaveChunk *chunk;
    GravStruct *gravStruct;

    chunk = LoadNextChunk();
    VerifyChunk(chunk,BASIC_STRUCTURE,sizeof(GravStruct));

    gravStruct = memAlloc(sizeof(GravStruct),"GravStruct",0);
    memcpy(gravStruct,chunkContents(chunk),sizeof(GravStruct));

    memFree(chunk);

    return gravStruct;
}

void GravWellGenerator_Load(Ship *ship)
{
    GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;
    sdword num;
    sdword i;
    GravStruct *gravStruct;

    num = LoadInfoNumber();

    listInit(&spec->GravList);

    for (i=0;i<num;i++)
    {
        gravStruct = LoadGravStruct();
        listAddNode(&spec->GravList,&gravStruct->objnode,gravStruct);
    }
}

void GravWellGenerator_Fix(Ship *ship)
{
    GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;
    Node *node = spec->GravList.head;
    GravStruct *gravStruct;

    while (node != NULL)
    {
        gravStruct = (GravStruct *)listGetStructOfNode(node);

        gravStruct->ship = SpaceObjRegistryGetShip((sdword)gravStruct->ship);

        node = node->next;
    }
}

void GravWellGenerator_PreFix(Ship *ship)
{
    GravWellGeneratorSpec *spec = (GravWellGeneratorSpec *)ship->ShipSpecifics;
    spec->gravityEffect = NULL; //set to NULL!
}

void GravWellAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    attackStraightForward(ship,target,4000.0f,3000.0f);
}

// CAT: the exe's header (GravWellGeneratorHeader 0x008c4868: kGravWellGenerator, tGravWellGenerator; original/scratch-m2/m2r2-shipstatic-tools/headers.txt).
CustShipHeader GravWellGeneratorHeader =
{
    .sizeofShipSpecifics = sizeof(GravWellGeneratorSpec),
    .CustShipStaticInit = GravWellGeneratorStaticInit,
    .CustShipInit = GravWellGeneratorInit,
    .CustShipClose = GravWellClose,
    .CustShipSpecialActivate = GravWellGeneratorSpecialActivate,
    .CustShipHousekeep = GravWellGeneratorHouseKeep,
    .CustShipRemoveShipReferences = GravWellGeneratorRemoveShipReferences,
    .CustShipRemoveObjReferences = (void (*)(struct Ship *, struct SpaceObj *))GravWellGeneratorRemoveShipReferences, // CAT: new slot 12, the slot-11 body
    .CustShipDied = GravWellGeneratorDied,
    .CustShip_PreFix = GravWellGenerator_PreFix,
    .CustShip_Save = GravWellGenerator_Save,
    .CustShip_Load = GravWellGenerator_Load,
    .CustShip_Fix = GravWellGenerator_Fix,
    .CustShipAttack = GravWellAttack,
};
