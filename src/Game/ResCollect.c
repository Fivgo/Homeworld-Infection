#include "ResCollect.h"

#include <stdlib.h>

#include "AIShip.h"
#include "AITrack.h"
#include "Battle.h"
#include "Clouds.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "Debug.h"
#include "FastMath.h"
#include "Memory.h"
#include "Nebulae.h"
#include "Physics.h"
#include "Randy.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "StatScript.h"
#include "Tutor.h"
#include "Tweak.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "Vector.h"
#include "Worker.h"

#define DEBUG_COLLECT_RESOURCES 0

/*=============================================================================
    Tweakables:
=============================================================================*/

real32 COLLECTRESOURCE_DIST = 400.0f;
real32 MAX_RESOURCE_VELOCITY_TO_CHASE = 50.0f;
real32 ASTEROID_HARVEST_RANGE = 400.0f;
real32 ASTEROID_MAXHARVEST_RANGE = 600.0f;
udword CIRCLE_HARVEST_ASTEROIDS_BIGGEREQUAL_THAN = 2;
real32 CIRCLE_RIGHT_VELOCITY = 20.0f;
real32 CIRCLE_RIGHT_THRUST = 0.1f;
real32 DUSTCLOUD_RU_DAMAGE_RATIO;
// CAT: no ASTEROID_RU_DAMAGE_RATIO[] / ASTEROIDBREAK_* tweakables: AsteroidTakesDamage 0x004f1730 and BreakAsteroidUp
//      0x004f1580 use neither, and cataclysm.exe has none of those key strings

real32 ASTEROID_LO_ROTATE[NUM_ASTEROIDTYPES] = { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
real32 ASTEROID_HI_ROTATE[NUM_ASTEROIDTYPES] = { 1.0f , 1.0f , 1.0f , 1.0f ,  1.0f };

udword ASTEROID_ROTATE_PROB = 150;

real32 DUSTCLOUD_RCOLLECTOR_BLOWUP_SCALE = 1.0f;

real32 COLLECTRESOURCE_ZONE = 2000.0f;

// regrow resources stuff
udword REGROW_RESOURCES_CHECK_RATE = 255;
udword REGROW_RESOURCES_CHECK_FRAME = 86;
real32 REGROW_RATE_MODIFIER[REGROWRATE_Max+1] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

sdword regenerateRUrate = 255;

sdword regenerateRUsIfUnder = 400;
sdword regenerateRUs = 25;

udword CHECK_HARV_OUTOFWORLD_RESOURCE_RATE = 127;
udword CHECK_SHIP_OUTOFWORLD_RATE = 127;

scriptEntry ResCollectTweaks[] =
{
    makeEntry(COLLECTRESOURCE_DIST,scriptSetReal32CB),
    makeEntry(MAX_RESOURCE_VELOCITY_TO_CHASE,scriptSetReal32CB),
    makeEntry(ASTEROID_HARVEST_RANGE,scriptSetReal32CB),
    makeEntry(ASTEROID_MAXHARVEST_RANGE,scriptSetReal32CB),
    makeEntry(CIRCLE_HARVEST_ASTEROIDS_BIGGEREQUAL_THAN,scriptSetUdwordCB),
    makeEntry(CIRCLE_RIGHT_VELOCITY,scriptSetReal32CB),
    makeEntry(CIRCLE_RIGHT_THRUST,scriptSetReal32CB),
    makeEntry(DUSTCLOUD_RU_DAMAGE_RATIO,scriptSetReal32CB),
    makeEntry(ASTEROID_LO_ROTATE[Asteroid0],scriptSetReal32CB),
    makeEntry(ASTEROID_HI_ROTATE[Asteroid0],scriptSetReal32CB),
    makeEntry(ASTEROID_LO_ROTATE[Asteroid1],scriptSetReal32CB),
    makeEntry(ASTEROID_HI_ROTATE[Asteroid1],scriptSetReal32CB),
    makeEntry(ASTEROID_LO_ROTATE[Asteroid2],scriptSetReal32CB),
    makeEntry(ASTEROID_HI_ROTATE[Asteroid2],scriptSetReal32CB),
    makeEntry(ASTEROID_LO_ROTATE[Asteroid3],scriptSetReal32CB),
    makeEntry(ASTEROID_HI_ROTATE[Asteroid3],scriptSetReal32CB),
    makeEntry(ASTEROID_LO_ROTATE[Asteroid4],scriptSetReal32CB),
    makeEntry(ASTEROID_HI_ROTATE[Asteroid4],scriptSetReal32CB),
    makeEntry(ASTEROID_ROTATE_PROB,scriptSetUdwordCB),
    makeEntry(DUSTCLOUD_RCOLLECTOR_BLOWUP_SCALE,scriptSetReal32CB),
    makeEntry(COLLECTRESOURCE_ZONE,scriptSetReal32CB),
    makeEntry(REGROW_RESOURCES_CHECK_RATE,scriptSetUdwordCB),
    makeEntry(REGROW_RESOURCES_CHECK_FRAME,scriptSetUdwordCB),
    makeEntry(REGROW_RATE_MODIFIER[0],scriptSetReal32CB),
    makeEntry(REGROW_RATE_MODIFIER[1],scriptSetReal32CB),
    makeEntry(REGROW_RATE_MODIFIER[2],scriptSetReal32CB),
    makeEntry(REGROW_RATE_MODIFIER[3],scriptSetReal32CB),
    makeEntry(REGROW_RATE_MODIFIER[4],scriptSetReal32CB),
    makeEntry(REGROW_RATE_MODIFIER[5],scriptSetReal32CB),
    makeEntry(REGROW_RATE_MODIFIER[6],scriptSetReal32CB),
    makeEntry(REGROW_RATE_MODIFIER[7],scriptSetReal32CB),
    makeEntry(regenerateRUrate,scriptSetSdwordCB),
    makeEntry(regenerateRUsIfUnder,scriptSetSdwordCB),
    makeEntry(regenerateRUs,scriptSetSdwordCB),
    makeEntry(CHECK_HARV_OUTOFWORLD_RESOURCE_RATE,scriptSetUdwordCB),
    makeEntry(CHECK_SHIP_OUTOFWORLD_RATE,scriptSetUdwordCB),

    END_SCRIPT_ENTRY
};

sdword RESOURCE_NOT_ACCESSIBLE_SECONDS = 60;

/*=============================================================================
    Private functions prototypes:
=============================================================================*/

void InitShipForResourceCollection(Ship *ship,Resource *resource);

/*=============================================================================
    Functions
=============================================================================*/

/*-----------------------------------------------------------------------------
    Name        : ResourceMovingTooFast
    Description : returns TRUE if resource is moving too fast to be harvested
    Inputs      : resource
    Outputs     :
    Return      : returns TRUE if resource is moving too fast to be harvested
----------------------------------------------------------------------------*/
bool32 ResourceMovingTooFast(Resource *resource)
{
    // CAT: ResourceMovingTooFast 0x004f10f0 has no Mission 6 limit (no MAX_RESOURCE_VELOCITY_TO_CHASE_LEVEL6 key in
    //      cataclysm.exe)
    real32 maxvelocitychase = MAX_RESOURCE_VELOCITY_TO_CHASE;

    if ((ABS(resource->obj.posinfo.velocity.x) > maxvelocitychase) ||
        (ABS(resource->obj.posinfo.velocity.y) > maxvelocitychase) ||
        (ABS(resource->obj.posinfo.velocity.z) > maxvelocitychase) )
    {
#if DEBUG_COLLECT_RESOURCES
            dbgMessage("Resource moving too fast. Picking different one...");
#endif
        return TRUE;
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : ResourceAlreadyBeingHarvested
    Description : returns TRUE if resource is already being targeted for harvesting
    Inputs      :
    Outputs     :
    Return      : returns TRUE if resource is already being targeted for harvesting
----------------------------------------------------------------------------*/
bool32 ResourceAlreadyBeingHarvested(struct CommandLayer *comlayer,struct CommandToDo *IAmThisCommand,Resource *resource)
{
    Node *curnode = comlayer->todolist.head;
    ResourceStaticInfo *resourcestaticinfo = resource->staticinfo;
    CommandToDo *command;

    if (!resourcestaticinfo->harvestableByMultipleShips)
    {
        while (curnode != NULL)
        {
            command = (CommandToDo *)listGetStructOfNode(curnode);

            if ((command->ordertype.order == COMMAND_COLLECT_RESOURCES) && (command != IAmThisCommand))
            {
                if (command->collect.resource == resource)
                {
#if DEBUG_COLLECT_RESOURCES
                dbgMessage("Resource already targeted. Picking different one...");
#endif
                    return TRUE;
                }
            }

            curnode = curnode->next;
        }
        return FALSE;
    }
    else
    {
        // harvestable my multiple ships, so find how many ships are targeting it
        udword targetedby = 0;

        while (curnode != NULL)
        {
            command = (CommandToDo *)listGetStructOfNode(curnode);

            if ((command->ordertype.order == COMMAND_COLLECT_RESOURCES) && (command != IAmThisCommand))
            {
                if (command->collect.resource == resource)
                {
                    targetedby++;
                }
            }

            curnode = curnode->next;
        }

        if (targetedby >= resourcestaticinfo->harvestableByMultipleShips)
        {
#if DEBUG_COLLECT_RESOURCES
            dbgMessage("Resource being targeted by too many. Picking different one...");
#endif
            return TRUE;
        }
        return FALSE;
    }
}

Resource *rescollectFindNearestResource(Ship *ship,struct CommandToDo *command)
{
    Resource *resource;

    // CAT: rescollectFindNearestResource 0x004f11f0. A harvester that filled up mid-volume (SPECIAL_2_CatResourcing,
    //      set by processCollectResource) searches COLLECTRESOURCE_ZONE around its volume position again; the flag is
    //      cleared either way. Outside a volume, only the local player's and the AI players' harvesters search further.
    if ((command->collect.resourceVolumeSize != 0.0f) || bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatResourcing))
    {
        if (bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatResourcing))
        {
            command->collect.resourceVolumeSize = COLLECTRESOURCE_ZONE;
        }
        bitClear(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatResourcing);
        // CAT: the exe passes univFindNearestResource 0x00549c50 a fourth argument, 0 here (0x004f124b)
        resource = univFindNearestResource(ship,command->collect.resourceVolumeSize,&command->collect.resourceVolumePosition,FALSE);
        if (resource != NULL)
        {
            return resource;
        }
        command->collect.resourceVolumeSize = 0.0f;     // no more resources in this volume, so don't keep looking in it
    }

    if ((ship->playerowner != universe.curPlayerPtr) && (ship->playerowner->aiPlayer == NULL))
    {
        return NULL;
    }

    // CAT (M2-R5 dock): the exe goes on with cat_clHarvestNearestResource 0x00449790 for a one-ship selection, with no
    //      message and no speech (rescollectFindNearestResource 0x004f11f0)
    {
        SelectCommand selectone;

        selectone.numShips = 1;
        selectone.ShipPtr[0] = ship;
        resource = cat_clHarvestNearestResource(&universe.mainCommandLayer,&selectone,FALSE,FALSE);
    }

    return resource;
}

void ChangeSingleShipToCollectResource(struct CommandToDo *command,Resource *resource)
{
    Ship *ship = command->selection->ShipPtr[0];
    dbgAssertOrIgnore(command->selection->numShips == 1);

    // CAT: ChangeSingleShipToCollectResource 0x004f12c0 takes the resource (clProcess 0x0044b950 passes NULL,
    //      cat_clHarvestNearestResource 0x00449790 the one it found) and only searches without one
    if (resource == NULL)
    {
        resource = rescollectFindNearestResource(ship,command);
    }
    InitShipForResourceCollection(ship,resource);

    command->ordertype.order = COMMAND_COLLECT_RESOURCES;
    command->ordertype.attributes = 0;
    command->collect.resource = resource;

    PrepareOneShipForCommand(ship,command,TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : clCollectResource
    Description : command for a ship to collect a resource
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void clCollectResource(CommandLayer *comlayer,SelectCommand *selectcom,ResourcePtr resource)
{
    CommandToDo *newcommand;
    SelectCommand *selection;
    sdword numShips;
    Ship *ship;
    sdword i;

    numShips = selectcom->numShips;

    if (numShips == 0)
    {
        dbgMessage("No ships to get resources");
        return;
    }

#if DEBUG_COLLECT_RESOURCES
    dbgMessage("Received command to harvest");
#endif

    RemoveShipsFromDoingStuff(comlayer,selectcom,0x22);   // CAT: reason 0x22 as the exe (clCollectResource 0x004f1376) [audit: M2-R5 command-tactics]

    // add COMMAND_COLLECT_RESOURCES commands for each resource collector:
    for (i=0;i<numShips;i++)
    {
        ship = selectcom->ShipPtr[i];
        InitShipForResourceCollection(ship,resource);

        newcommand = memAlloc(sizeof(CommandToDo),"ToDo",0);
        InitCommandToDo(newcommand);

        selection = memAlloc(sizeof(SelectCommand),"ToDoSelection",0);
        selection->numShips = 1;
        selection->ShipPtr[0] = ship;

        newcommand->selection = selection;
        newcommand->ordertype.order = COMMAND_COLLECT_RESOURCES;
        newcommand->ordertype.attributes = 0;

        newcommand->collect.resource = resource;
        newcommand->collect.resourceVolumeSize = 0.0f;
        vecZeroVector(newcommand->collect.resourceVolumePosition);
        if (resource)
        {
            newcommand->collect.resourceVolumeSize = COLLECTRESOURCE_ZONE;
            newcommand->collect.resourceVolumePosition = resource->obj.posinfo.position;
        }

        PrepareShipsForCommand(newcommand,TRUE);

        listAddNode(&comlayer->todolist,&newcommand->todonode,newcommand);
    }
}

// CAT: HW1's HandleAsteroidScaling is gone: Cataclysm asteroids never shrink (AsteroidTakesDamage 0x004f1730 and
//      CollectResources 0x004f17a0 never rescale one, and the exe has no asteroidCanShrink/asteroidMinShrink keys)

void HandleDustCloudScaling(DustCloud* dustcloud)
{
    cloudSystem* csys;
    DustCloudStaticInfo* dustcloudstatic = (DustCloudStaticInfo*)dustcloud->staticinfo;

    if (dustcloudstatic->dustcloudCanShrink)
    {
        real32 oldscaling = dustcloud->scaling;

        dustcloud->scaling = (real32)dustcloud->resource.resourcevalue / (real32)dustcloudstatic->resource.resourcevalue;
        if (dustcloud->scaling > 1.0f)
        {
            dustcloud->scaling = 1.0f;
        }

        if (dustcloud->scaling < dustcloudstatic->dustcloudMinShrink)
        {
            dustcloud->scaling = dustcloudstatic->dustcloudMinShrink;
        }

        if (dustcloud->scaling != oldscaling)
        {
            MakeNewDustCloudStaticInfo(dustcloud);
        }

        csys = (cloudSystem*)dustcloud->stub;
        if (csys != NULL)
        {
            csys->healthFactor = dustcloud->scaling;
        }
    }
}

void HandleGasCloudScaling(GasCloud *gascloud)
{
    GasCloudStaticInfo *gascloudstatic = (GasCloudStaticInfo *)gascloud->staticinfo;
    if (gascloudstatic->gascloudCanShrink)
    {
        real32 oldscaling = gascloud->scaling;

        gascloud->scaling = (real32)gascloud->resource.resourcevalue / (real32)gascloudstatic->resource.resourcevalue;
        if (gascloud->scaling < gascloudstatic->gascloudMinShrink)
        {
            gascloud->scaling = gascloudstatic->gascloudMinShrink;
        }

        if (gascloud->scaling != oldscaling)
        {
            MakeNewGasCloudStaticInfo(gascloud);
        }
    }
}

void HandleNebulaScaling(Nebula* nebula)
{
    NebulaStaticInfo* nebulastatic;
    real32 factor;

    nebulastatic = (NebulaStaticInfo*)nebula->staticinfo;
    factor = (real32)nebula->resource.resourcevalue / (real32)nebulastatic->resource.resourcevalue;

    nebFadeAttachedTendrils((nebChunk*)nebula->stub, factor);
}

/*-----------------------------------------------------------------------------
    Name        : DustCloudTakesDamage
    Description : applies damage to dustcloud, causing it to lose RU's and possibly
                  die and wreak havok as it does so.  charging takes place here, too
    Inputs      : asteroid, damagetaken, targetWasAlive
    Outputs     :
    Return      : returns TRUE if should delete dustcloud
----------------------------------------------------------------------------*/
bool32 DustCloudTakesDamage(DustCloud* dustcloud, sdword damagetaken, bool32 targetWasAlive)
{
    sdword RUdamage;

    if (!targetWasAlive)
    {
        return FALSE;       // target already dead, so ignore
    }

    RUdamage = (sdword) (damagetaken * DUSTCLOUD_RU_DAMAGE_RATIO);
    dustcloud->resource.resourcevalue -= RUdamage;

    if (dustcloud->resource.resourcevalue <= 0)
    {
        return TRUE;        // all RU's have been sapped out.  Just kill asteroid without splitting it up.
    }

    if (dustcloud->targetable.health <= 0)
    {
        //clouds can't break.  instead they do something cool upon losing all health
        return TRUE;
    }
    else
    {
        HandleDustCloudScaling(dustcloud);
        return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : DustCloudChargesUp
    Description : ion cannon fire charges up a dustcloud; this fn performs the charging
    Inputs      : dustcloud, damagetaken, targetWasAlive
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void DustCloudChargesUp(DustCloud* dustcloud, sdword damagetaken, bool32 targetWasAlive)
{
    cloudSystem* csys = (cloudSystem*)dustcloud->stub;

    if (!targetWasAlive || csys == NULL)
    {
        return;
    }

    csys->charge += 0.5f * ((real32)damagetaken / (real32)csys->maxhealth);
    if (csys->charge > 1.0f)
    {
        bitSet(csys->flags, LIGHTNING_DISCHARGING);
        csys->charge = 1.0f;
    }
}

/*-----------------------------------------------------------------------------
    Name        : BreakAsteroidUp
    Description : Breaks the asteroid up into little fragments
    Inputs      : asteroid
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: BreakAsteroidUp 0x004f1580 is Cataclysm's: the asteroid breaks into the pieces its .shp lists
//      (AsteroidStaticInfo.breakPieces): each at its offset in the parent's frame, turned by its orientation, pushed
//      out at params[0] on top of the parent's velocity and spinning at params[1..3]. No HW1 random break-up.
void BreakAsteroidUp(Asteroid *asteroid)
{
    AsteroidStaticInfo *asteroidstatic = asteroid->staticinfo;
    AsteroidBreakPiece *piece;
    Asteroid *newasteroid;
    vector offset, push, position;
    matrix coordsys;
    sdword i;

    dbgAssertOrIgnore(asteroid->header.objtype == OBJ_AsteroidType);

    cat_soundEventAsteroidBreak((SpaceObj *)asteroid);    // CAT: the break sound (BreakAsteroidUp 0x004f1580 calls 0x005215d0)
    for (i = 0; i < asteroidstatic->numBreakPieces; i++)
    {
        piece = &asteroidstatic->breakPieces[i];
        matMultiplyMatByVec(&offset, &asteroid->rotatable.rotinfo.coordsys, &piece->position);
        push = offset;
        if ((offset.x != 0.0f) || (offset.y != 0.0f) || (offset.z != 0.0f))
        {
            vecNormalizeToLength(&push, piece->params[0]);
        }
        vecAdd(position, offset, asteroid->obj.posinfo.position);
        matMultiplyMatByMat(&coordsys, &piece->orientation, &asteroid->rotatable.rotinfo.coordsys);
        newasteroid = univAddAsteroid(piece->asteroidtype, &position);
        vecAdd(newasteroid->obj.posinfo.velocity, push, asteroid->obj.posinfo.velocity);
        newasteroid->rotatable.rotinfo.coordsys = coordsys;
        newasteroid->rotatable.rotinfo.rotspeed.x = piece->params[1];
        newasteroid->rotatable.rotinfo.rotspeed.y = piece->params[2];
        newasteroid->rotatable.rotinfo.rotspeed.z = piece->params[3];
        univAddObjToRenderListIf((SpaceObj *)newasteroid,(SpaceObj *)asteroid);     // add to render list if parent is in render list
    }
}

/*-----------------------------------------------------------------------------
    Name        : AsteroidTakesDamage
    Description : applies damage to asteroid, causing it to use RU's, and possibly
                  die and split into multiple chunks.
    Inputs      : asteroid, damagetaken, targetWasAlive
    Outputs     :
    Return      : returns TRUE if should delete asteroid
----------------------------------------------------------------------------*/
// CAT: AsteroidTakesDamage 0x004f1730 is Cataclysm's: an asteroid's resource value follows its health (the static
//      value times the health fraction, truncated), and it dies when its health is gone; the damage argument is no
//      longer read, and there is no HW1 RU damage ratio or shrinking
bool32 AsteroidTakesDamage(Asteroid *asteroid,sdword damagetaken,bool32 targetWasAlive)
{
    real32 fraction = 0.0f;

    (void)damagetaken;
    if (asteroid->targetable.health > 0.0f)
    {
        fraction = asteroid->targetable.health / asteroid->staticinfo->health.maxhealth;
    }
    asteroid->resource.resourcevalue = (sdword)((real32)asteroid->staticinfo->resource.resourcevalue * fraction);
    return targetWasAlive && (asteroid->targetable.health <= 0.0f);
}

/*-----------------------------------------------------------------------------
    Name        : CollectResources
    Description : if it is a ship-resource collision, the resource will be collected
                  assuming there is room in the ship for the resource.
    Inputs      :
    Outputs     :
    Return      : TRUE if ship collected resource
----------------------------------------------------------------------------*/
// CAT: CollectResources 0x004f17a0: a tutorial harvests five times as fast; a ship with Bulimia (specialFlags2
//      0x200000) gets nothing; "full" is battle chatter 0x4B; an asteroid's health follows its resource value; the
//      clouds scale in place (the asteroid does not shrink: HW1's HandleAsteroidScaling has no counterpart); the AI's
//      resourcing marks are Ship words now (catResourceFinished, catResourcing)
bool32 CollectResources(Ship *ship,Resource *resource)
{
    ShipStaticInfo *shipstatic;
    sdword eat;
    bool32 resourceWasAlive;

    dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

    shipstatic = (ShipStaticInfo *)ship->staticinfo;

    if (shipstatic->maxresources == 0)
    {
        return FALSE;       // not able to collect resources, so return FALSE
    }

    if (ship->resources >= shipstatic->maxresources)
    {
        return FALSE;       // ship full, can't collect resources
    }

    if (UNIVERSE_WOODPECKER(shipstatic->harvestRate, 0))
    {
        resourceWasAlive = ((resource->header.flags & SOF_Dead) == 0);

        eat = (shipstatic->harvestAmount <= resource->resourcevalue) ? shipstatic->harvestAmount : resource->resourcevalue;
        if (tutorial == 1)
        {
            eat = eat * 5;
        }
        if (!bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatKasBulimia))
        {
            ship->resources += eat;
        }

        if (ship->resources > shipstatic->maxresources)
        {                                                          // over eated
            eat += (shipstatic->maxresources - ship->resources);
            ship->resources = shipstatic->maxresources;
            // CAT: Cataclysm's "resource collector full" battle chatter (event 0x4B of cataclysm.exe)
            if (battleCanChatterAtThisTime(0x4b, ship))
            {
                battleChatterAttempt(SOUND_EVENT_DEFAULT, 0x4b, ship, ship->shiptype);
            }
        }

        resource->resourcevalue -= eat;

        if (resource->header.objtype == OBJ_AsteroidType)
        {
            real32 value = (real32)resource->resourcevalue;

            if (value <= 0.0f)
            {
                value = 0.0f;
            }
            if (resource->targetable.health < 0.0f)
            {
                // the exe prints "WARNING: Asteroid health was %f, setting to zero" here
                resource->targetable.health = 0.0f;
            }
            resource->targetable.health = (value / (real32)resource->staticinfo->resourcevalue) *
                                          ((AsteroidStaticInfo *)resource->staticinfo)->health.maxhealth;
        }

        if ((resource->resourcevalue <= 0) && (resourceWasAlive))
        {
            AddResourceToDeleteResourceList(resource);
            ship->shipOrDerelict.catResourceFinished = TRUE;
            ship->shipOrDerelict.catResourcing = FALSE;
            return TRUE;
        }

        if (resource->header.objtype == OBJ_DustType)
        {
            HandleDustCloudScaling((DustCloud *)resource);
        }
        else if (resource->header.objtype == OBJ_GasType)
        {
            HandleGasCloudScaling((GasCloud *)resource);
        }
        else if (resource->header.objtype == OBJ_NebulaType)
        {
            HandleNebulaScaling((Nebula*)resource);
        }
    }

    return TRUE;
}

// CAT: new (cat_shipIsCollectingResources 0x004f2500): TRUE if the ship's command is a collect order
bool32 cat_shipIsCollectingResources(Ship *ship)
{
    CommandToDo *command = getShipAndItsCommand(&universe.mainCommandLayer, ship);

    return (command != NULL) && (command->ordertype.order == COMMAND_COLLECT_RESOURCES);
}

/*-----------------------------------------------------------------------------
    Name        : TurnHarvestEffectOff
    Description : turns the harvest effect off
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void TurnHarvestEffectOff(Ship *ship)
{
    // CAT: stops no harvest sound. The exe folds this body with stopRepairEffect (0x005c23b0 is the copy its callers
    //      use); 0x004f1a50, which the delta map calls TurnHarvestEffectOff, is TurnOffAnyResourcingEffects with
    //      this body inlined
    dbgAssertOrIgnore(ship->rceffect != NULL);

    if (bitTest(ship->rceffect->staticinfo->specialOps, ESO_SelfDeleting))
    {                                                       //if the effect will delete itself
        ((real32 *)ship->rceffect->variable)[ETG_ResourceDurationParam] =
            ship->rceffect->timeElapsed;                            //time it out
    }
    else
    {                                                       //else it's a free-running effect... delete it
        etgEffectDelete(ship->rceffect);
        univRemoveObjFromRenderList(&ship->rceffect->obj);
        listDeleteNode(&ship->rceffect->header.objlink);
    }

    ship->rceffect = NULL;
}

/*-----------------------------------------------------------------------------
    Name        :
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void TurnOffAnyResourcingEffects(struct CommandToDo *todo)
{
    Ship *ship = todo->selection->ShipPtr[0];
    dbgAssertOrIgnore(todo->selection->numShips == 1);

    if (ship->rceffect != NULL)
    {
        TurnHarvestEffectOff(ship);
    }
}

/*-----------------------------------------------------------------------------
    Name        : TurnHarvestEffectOn
    Description : turns the harvest effect on
    Inputs      : ship, resource, heading, dist
                  etgLOD - which harvesting effect to use
                  resourceRadius - radius of the resource we are harvesting
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void TurnHarvestEffectOn(Ship *ship,Resource *resource,vector *trajectory, real32 resourceRadius, etglod *etgLOD)
{
    ShipStaticInfo *shipstatic = (ShipStaticInfo *)ship->staticinfo;
    ResNozzleStatic *resNozzleStatic = shipstatic->resNozzleStatic;
    vector nozzleposition;
    matrix nozzlecoordsys;
    vector nozzletrajectory;
    real32 nozzletrajectorydist;
    etgeffectstatic *stat;
    sdword LOD;
    udword intLength;
    udword intWidth;
    real32 temp;

    dbgAssertOrIgnore(resNozzleStatic);

    matMultiplyMatByVec(&nozzleposition,&ship->rotatable.rotinfo.coordsys,&resNozzleStatic->position);

    vecSub(nozzletrajectory,*trajectory,nozzleposition);

    nozzletrajectorydist = fsqrt(vecMagnitudeSquared(nozzletrajectory));
    vecDivideByScalar(nozzletrajectory,nozzletrajectorydist,temp);

    matCreateCoordSysFromHeading(&nozzlecoordsys,&nozzletrajectory);

    intLength = Real32ToUdword(nozzletrajectorydist);
    intWidth = Real32ToUdword(resourceRadius);

    vecAddTo(nozzleposition,ship->obj.posinfo.position);

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
        // CAT: TurnHarvestEffectOn 0x004f1ac0 starts no harvest sound (HW1: Ship_ResourceAsteroid/DustCloud).
        // CAT-TODO(stage 3): the exe passes the beam length and the resource radius to the effect as real32 values;
        //      this tree's ETG still reads them as integers (SCALECAST), as all its etgEffectCreate callers do
        ship->rceffect = etgEffectCreate(stat, ship, &nozzleposition, &ship->obj.posinfo.velocity, &nozzlecoordsys, 1.0f, EAF_Velocity | EAF_NLips, 2, SCALECAST(intLength), SCALECAST(intWidth));
    }
    else
    {
        ship->rceffect = NULL;                              //play no effect
    }
}

/*-----------------------------------------------------------------------------
    Name        : ModifyHarvestEffect
    Description : Modify the coordinate system, length and width of a
                    resourcing effect.
    Inputs      : resourceRadius - radius of the resource we are harvesting
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ModifyHarvestEffect(Effect *effect,Ship *ship,vector *trajectory, real32 resourceRadius)
{
    ShipStaticInfo *shipstatic = (ShipStaticInfo *)ship->staticinfo;
    ResNozzleStatic *resNozzleStatic = shipstatic->resNozzleStatic;
    vector nozzleposition;
    vector nozzletrajectory;
    real32 nozzletrajectorydist;
    real32 temp;

    dbgAssertOrIgnore(resNozzleStatic);
    dbgAssertOrIgnore(effect);

    matMultiplyMatByVec(&nozzleposition,&ship->rotatable.rotinfo.coordsys,&resNozzleStatic->position);

    vecSub(nozzletrajectory,*trajectory,nozzleposition);

    vecAdd(effect->obj.posinfo.position,nozzleposition,ship->obj.posinfo.position);

    nozzletrajectorydist = fsqrt(vecMagnitudeSquared(nozzletrajectory));
    vecDivideByScalar(nozzletrajectory,nozzletrajectorydist,temp);

    matCreateCoordSysFromHeading(&effect->rotatable.rotinfo.coordsys,&nozzletrajectory);

    ((real32 *)effect->variable)[SCALESHIFT(ETG_ResourceLengthParam)] = nozzletrajectorydist;
    ((real32 *)effect->variable)[SCALESHIFT(ETG_ResourceRadiusParam)] = resourceRadius;
}

void DefaultShipHarvestsResource(struct Ship *ship,struct Resource *resource)
{
    if (!MoveReachedDestinationVariable(ship,&resource->obj.posinfo.position,COLLECTRESOURCE_DIST))
    {
        aishipFlyToPointAvoidingObjsWithVel(ship,&resource->obj.posinfo.position,AISHIP_FastAsPossible + AISHIP_FirstPointInDirectionFlying + AISHIP_PointInDirectionFlying,0.0f,&resource->obj.posinfo.velocity);
    }
    else
    {
        aitrackSteadyShip(ship);
        CollectResources(ship,resource);
    }
}

#define R1RCASTEROID_APPROACHASTEROID       1
#define R1RCASTEROID_HARVEST_ON             2
#define R1RCASTEROID_HARVEST                3

/*-----------------------------------------------------------------------------
    Name        : R1ResourcerHarvestsAsteroid
    Description : Used to be only for asteroids, now generalized
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void R1ResourcerHarvestsAsteroid(struct Ship *ship,struct Resource *resource)
{
    vector trajectory;
    real32 range;
    real32 dist;
    real32 temp;
    etglod *lod;
    real32 resourceRadius;

//    dbgAssertOrIgnore(resource->header.objtype == OBJ_AsteroidType);

    // CAT: R1ResourcerHarvestsAsteroid 0x004f1d40: a NULL resource (processCollectResource 0x004f2130 and
    //      cat_rescollectStopHarvest 0x004f2450 stop a harvest that way) resets the harvest state
    if (resource == NULL)
    {
        ship->rcstate1 = 0;
        return;
    }

    switch (ship->rcstate1)
    {
        case 0:
            // initialize any data
            ship->rcstate1 = R1RCASTEROID_APPROACHASTEROID;
            // deliberately fall through to R1RCASTEROID_APPROACHASTEROID

        case R1RCASTEROID_APPROACHASTEROID:
            aishipGetTrajectory(ship,(SpaceObjRotImpTarg *)resource,&trajectory);
            dist = fsqrt(vecMagnitudeSquared(trajectory));
            range = RangeToTargetGivenDist(ship,(SpaceObjRotImpTarg *)resource,dist);
            if (range > ASTEROID_HARVEST_RANGE)
            {
                if (aishipFlyToPointAvoidingObjsWithVel(ship,&resource->obj.posinfo.position,AISHIP_FastAsPossible + AISHIP_FirstPointInDirectionFlying + AISHIP_PointInDirectionFlying + AISHIP_DontFlyToObscuredPoints,0.0f,&resource->obj.posinfo.velocity) & AISHIP_FLY_OBJECT_IN_WAY)
                    resource->resourceNotAccessible = (UNIVERSE_UPDATE_RATE * RESOURCE_NOT_ACCESSIBLE_SECONDS);
                break;
            }
#if DEBUG_COLLECT_RESOURCES
            dbgMessagef("%x Changing to harvest",ship);
#endif
            ship->rcstate1 = R1RCASTEROID_HARVEST_ON;
            // deliberately fall through to R1RCASTEROID_HARVEST_ON

        case R1RCASTEROID_HARVEST_ON:
            // turn on harvest effect here
            if (resource->header.objtype == OBJ_AsteroidType)
            {
                lod = etgResourceEffectTable[ship->shiprace][ETG_AsteroidEffect];
            }
            else
            {
                lod = etgResourceEffectTable[ship->shiprace][ETG_GaseousEffect];
            }
            resourceRadius = resource->staticinfo->common.staticCollInfo.collspheresize;
            TurnHarvestEffectOn(ship,resource,&trajectory, resourceRadius, lod);

            ship->rcstate1 = R1RCASTEROID_HARVEST;
            break;

        case R1RCASTEROID_HARVEST:
            aishipGetTrajectory(ship,(SpaceObjRotImpTarg *)resource,&trajectory);
            dist = fsqrt(vecMagnitudeSquared(trajectory));
            range = RangeToTargetGivenDist(ship,(SpaceObjRotImpTarg *)resource,dist);
            if(ship->rceffect == NULL)
            {
                //if the effect isn't playing...lets play it...
                if (resource->header.objtype == OBJ_AsteroidType)
                {
                    lod = etgResourceEffectTable[ship->shiprace][ETG_AsteroidEffect];
                }
                else
                {
                    lod = etgResourceEffectTable[ship->shiprace][ETG_GaseousEffect];
                }
                resourceRadius = resource->staticinfo->common.staticCollInfo.collspheresize;
                TurnHarvestEffectOn(ship,resource,&trajectory, resourceRadius, lod);
            }
            if (range > ASTEROID_MAXHARVEST_RANGE)
            {
                // turn off effect here
                if (ship->rceffect != NULL)
                {
                    TurnHarvestEffectOff(ship);
                }
                dbgAssertOrIgnore(ship->rceffect == NULL);

                ship->rcstate1 = R1RCASTEROID_APPROACHASTEROID;
                break;
            }

            // recalculate effect coordsys and length of effect
            if (ship->rceffect != NULL)
            {
                resourceRadius = resource->staticinfo->common.staticCollInfo.collspheresize;
                ModifyHarvestEffect(ship->rceffect,ship,&trajectory, resourceRadius);
            }

            // harvest RU's

            CollectResources(ship,resource);

            vecDivideByScalar(trajectory,dist,temp);

            // circle the asteroid if harvesting big asteroids
            if ((resource->header.objtype == OBJ_AsteroidType) &&
                (((Asteroid *)resource)->asteroidtype < (AsteroidType)CIRCLE_HARVEST_ASTEROIDS_BIGGEREQUAL_THAN))
            {
                aitrackHeading(ship,&trajectory,FLYSHIP_HEADINGACCURACY);
                aitrackZeroVelocity(ship);
            }
            else
            {
                real32 rightvelocity;
                vector right;

                aitrackHeading(ship,&trajectory,FLYSHIP_HEADINGACCURACY);

                matGetVectFromMatrixCol2(right,ship->rotatable.rotinfo.coordsys);
                rightvelocity = vecDotProduct(right,ship->obj.posinfo.velocity);

                if (rightvelocity < CIRCLE_RIGHT_VELOCITY)
                {
//                    dbgMessagef("Applying right thrust %f",rightvelocity);
                    physApplyForceToObj(&ship->obj,ship->guidance.nonstatvars.thruststrength[TRANS_RIGHT]*CIRCLE_RIGHT_THRUST,TRANS_RIGHT);
                }

                if (range > ASTEROID_HARVEST_RANGE)
                {
//                    dbgMessage("Going back towards");
                    aishipFlyToPointAvoidingObjsWithVel(ship,&resource->obj.posinfo.position,0,0.0f,&resource->obj.posinfo.velocity);
                }
                else
                {
                    aitrackZeroForwardVelocity(ship);
                }
            }
            break;
    }
    return;
}

/*-----------------------------------------------------------------------------
    Name        : InitShipForResourceCollection
    Description : initializes ship ai variables for resource collection
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void InitShipForResourceCollection(Ship *ship,Resource *resource)
{
    InitShipAI(ship,TRUE);

    ship->rcstate1 = 0;
    ship->rcstate2 = 0;

    // CAT: the exe inlines this (ChangeSingleShipToCollectResource 0x004f12c0, processCollectResource 0x004f2130):
    //      sWorker and bWorker harvest with a third callback, the code at 0x005c9130 (no Ghidra function; saved as
    //      index 2 by PreFix_ShipXHarvestsResourceY 0x004f2090), every other harvester with R1ResourcerHarvestsAsteroid.
    if ((ship->shiptype == sWorker) || (ship->shiptype == bWorker))
    {
        ship->ShipXHarvestsResourceY = cat_workerHarvestsResource;     // CAT: Worker.c [M2-R4 ships-b]
    }
    else
    {
        ship->ShipXHarvestsResourceY = R1ResourcerHarvestsAsteroid;//DefaultShipHarvestsResource;
    }
}

void PreFix_ShipXHarvestsResourceY(Ship *ship)
{
    if (ship->ShipXHarvestsResourceY == DefaultShipHarvestsResource)
    {
        ship->ShipXHarvestsResourceY = (void *) 0;
    }
    else if (ship->ShipXHarvestsResourceY == R1ResourcerHarvestsAsteroid)
    {
        ship->ShipXHarvestsResourceY = (void *) 1;
    }
    else if (ship->ShipXHarvestsResourceY == cat_workerHarvestsResource)
    {
        ship->ShipXHarvestsResourceY = (void *) 2;     // CAT: the Worker's callback (0x004f2090) [M2-R4 ships-b]
    }
    else
    {
        ship->ShipXHarvestsResourceY = (void *) -1;
    }
}

void Fix_ShipXHarvestsResourceY(Ship *ship)
{
    switch ((sdword)ship->ShipXHarvestsResourceY)
    {
        case 0:
            ship->ShipXHarvestsResourceY = DefaultShipHarvestsResource;
            break;

        case 1:
            ship->ShipXHarvestsResourceY = R1ResourcerHarvestsAsteroid;
            break;

        case 2:
            ship->ShipXHarvestsResourceY = cat_workerHarvestsResource;     // CAT: 0x004f20e0 [M2-R4 ships-b]
            break;

        default:
            ship->ShipXHarvestsResourceY = NULL;
            break;
    }
}


/*-----------------------------------------------------------------------------
    Name        : processCollectResource
    Description : processes a collect resource command
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        : cat_rescollectStopHarvest
    Description : CAT: ends a harvest (cat_rescollectStopHarvest 0x004f2450): an AI harvest mark becomes "finished"
                  and the harvest callback is told to stop (a NULL resource)
    Inputs      : ship
----------------------------------------------------------------------------*/
static void cat_rescollectStopHarvest(Ship *ship)
{
    if (ship->shipOrDerelict.catResourcing)
    {
        ship->shipOrDerelict.catResourcing = FALSE;
        bitClear(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatHarvesting);
        ship->shipOrDerelict.catResourceFinished = TRUE;
    }
    if (ship->ShipXHarvestsResourceY != NULL)
    {
        ship->ShipXHarvestsResourceY(ship,NULL);
    }
}

bool32 processCollectResource(struct CommandToDo *collecttodo)
{
    // CAT: processCollectResource 0x004f2130. Against HW1: a resource that moves too fast, is taken or is outside
    //      the world is dropped together with the harvest (cat_rescollectStopHarvest); a found resource is used only
    //      while the hold is below resourcesAtOneTime, else the ship goes to dock; a full ship marks itself
    //      SPECIAL_2_CatResourcing so that it resumes in its volume; the harvester-done messages and speech are gone,
    //      and the ship is steadied when the order ends or turns into a dock order
    Resource *resource  = collecttodo->collect.resource;
    Ship     *ship      = collecttodo->selection->ShipPtr[0];
    Ship     *dockship;
    bool32   done;

    ShipStaticInfo *shipstatic = (ShipStaticInfo *)ship->staticinfo;

    dbgAssertOrIgnore(collecttodo->selection->numShips > 0);
    dbgAssertOrIgnore(collecttodo->selection->numShips == 1);

    if (ship->resources >= shipstatic->maxresources)
    {
        if (ship->rceffect != NULL)
        {
            TurnHarvestEffectOff(ship);
        }
        collecttodo->collect.resource = NULL;           // override - don't collect resource if full
        bitSet(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatResourcing);
        goto letsdock;
    }

    if (resource != NULL)
    {
        if ((!ResourceMovingTooFast(resource)) &&
            (!ResourceAlreadyBeingHarvested(&universe.mainCommandLayer,collecttodo,resource)) &&
            (!(UNIVERSE_WOODPECKER(CHECK_HARV_OUTOFWORLD_RESOURCE_RATE, resource->resourceID.resourceNumber & CHECK_HARV_OUTOFWORLD_RESOURCE_RATE) &&
               univObjectOutsideWorld((SpaceObj *)resource))))
        {
            ship->ShipXHarvestsResourceY(ship,resource);

            if (resource->resourceNotAccessible)
            {
                TurnOffAnyResourcingEffects(collecttodo);
                collecttodo->collect.resource = NULL;
                cat_rescollectStopHarvest(ship);
            }
            return FALSE;
        }

        // moving too fast, already being harvested by someone else or outside the world: pick another one
        if (ship->rceffect != NULL)
        {
            TurnHarvestEffectOff(ship);
        }
        collecttodo->collect.resource = NULL;
        cat_rescollectStopHarvest(ship);
        return FALSE;
    }

    resource = rescollectFindNearestResource(ship,collecttodo);
    if (resource == NULL)
    {
        // CAT: a network game, recorded multiplayer games included, ends the order here (processCollectResource
        //      0x004f2130: multiPlayerGame || (0x00a7519c && 0x00a75190))
        if (catNetworkGame())
        {
            return TRUE;
        }
        goto letsdock;
    }

    cat_rescollectStopHarvest(ship);
    if (ship->resources < shipstatic->resourcesAtOneTime)
    {
#if DEBUG_COLLECT_RESOURCES
        dbgMessage("Collecting next resource...");
#endif
        InitShipForResourceCollection(ship,resource);
        collecttodo->collect.resource = resource;
        return FALSE;
    }

letsdock:
    dockship = FindNearestShipToDockAt(ship,CAT_DOCK_DEPOSIT_RESOURCES);      // the exe's purpose 3 (M2-R5 dock)
    cat_rescollectStopHarvest(ship);
    if ((ship->resources > 0) && (dockship != NULL))
    {
#if DEBUG_COLLECT_RESOURCES
        dbgMessage("Flying back to deposit resources...");
#endif
        // changing COMMAND_COLLECT_RESOURCES to COMMAND_DOCK
        FreeLastOrder(collecttodo); // free COMMAND_COLLECT_RESOURCES
        dockChangeSingleShipToDock(collecttodo,ship,dockship,TRUE,CAT_DOCK_DEPOSIT_RESOURCES);
        done = FALSE;
    }
    else
    {
        done = TRUE;
    }
    aitrackSteadyShip(ship);
    return done;
}


/*-----------------------------------------------------------------------------
    Name        : R1ResourcerAttacksShip
    Description : used for resourcers attacking ships
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void R1ResourcerAttacksShip(struct Ship *ship,struct SpaceObjRotImpTarg *target,bool32 passiveAttacking)
{
    vector trajectory;
    real32 range;
    real32 dist;
    real32 temp;
    etglod *lod;
    real32 targetRadius;

//    dbgAssertOrIgnore(resource->header.objtype == OBJ_AsteroidType);

    switch (ship->rcstate1)
    {
        case 0:
            // initialize any data
            ship->rcstate1 = R1RCASTEROID_APPROACHASTEROID;
            // deliberately fall through to R1RCASTEROID_APPROACHASTEROID

        case R1RCASTEROID_APPROACHASTEROID:
            aishipGetTrajectory(ship,(SpaceObjRotImpTarg *)target,&trajectory);
            dist = fsqrt(vecMagnitudeSquared(trajectory));
            range = RangeToTargetGivenDist(ship,(SpaceObjRotImpTarg *)target,dist);
            if (range > ASTEROID_HARVEST_RANGE)
            {
                aishipFlyToPointAvoidingObjsWithVel(ship,&target->obj.posinfo.position,AISHIP_FastAsPossible + AISHIP_FirstPointInDirectionFlying + AISHIP_PointInDirectionFlying,0.0f,&target->obj.posinfo.velocity);
                break;
            }

            dbgMessagef("Ship Changing to Harvest/attack mode.");

            ship->rcstate1 = R1RCASTEROID_HARVEST_ON;
            // deliberately fall through to R1RCASTEROID_HARVEST_ON

        case R1RCASTEROID_HARVEST_ON:
            lod = etgResourceEffectTable[ship->shiprace][ETG_AsteroidEffect];

            targetRadius = target->staticinfo->common.staticCollInfo.collspheresize;
            TurnHarvestEffectOn(ship,(Resource *)target,&trajectory, targetRadius, lod);

            ship->rcstate1 = R1RCASTEROID_HARVEST;
            break;

        case R1RCASTEROID_HARVEST:
            aishipGetTrajectory(ship,(SpaceObjRotImpTarg *)target,&trajectory);
            dist = fsqrt(vecMagnitudeSquared(trajectory));
            range = RangeToTargetGivenDist(ship,(SpaceObjRotImpTarg *)target,dist);

            if (range > ASTEROID_MAXHARVEST_RANGE)
            {
                // turn off effect here
                if (ship->rceffect != NULL)
                {
                    TurnHarvestEffectOff(ship);
                }
                dbgAssertOrIgnore(ship->rceffect == NULL);

                ship->rcstate1 = R1RCASTEROID_APPROACHASTEROID;
                break;
            }
            if(ship->rceffect == NULL)
            {
                lod = etgResourceEffectTable[ship->shiprace][ETG_AsteroidEffect];

                targetRadius = target->staticinfo->common.staticCollInfo.collspheresize;
                TurnHarvestEffectOn(ship,(Resource *)target,&trajectory, targetRadius, lod);
            }


            // recalculate effect coordsys and length
            if (ship->rceffect != NULL)
            {
                targetRadius = target->staticinfo->common.staticCollInfo.collspheresize;
                ModifyHarvestEffect(ship->rceffect,ship,&trajectory, targetRadius);
            }

            // harvest RU's

            //CollectResources(ship,resource);

            ApplyDamageToTarget(target,TW_HARVESTER_DAMAGE_PER_SECOND*universe.phystimeelapsed,-1,DEATH_Killed_By_Player,ship->playerowner->playerIndex,ship,0);   // CAT: 7 arguments (no exe caller here: the dock subsystem's ResCollect port)

            vecDivideByScalar(trajectory,dist,temp);

            // circle the asteroid if harvesting big asteroids
            //if ((resource->header.objtype == OBJ_AsteroidType) &&
            //    (((Asteroid *)resource)->asteroidtype < (AsteroidType)CIRCLE_HARVEST_ASTEROIDS_BIGGEREQUAL_THAN))
            //{
            //    aitrackHeading(ship,&trajectory,FLYSHIP_HEADINGACCURACY);
            //    aitrackZeroVelocity(ship);
            //}
            //
            //else
            {
                real32 rightvelocity;
                vector right;

                aitrackHeading(ship,&trajectory,FLYSHIP_HEADINGACCURACY);

                matGetVectFromMatrixCol2(right,ship->rotatable.rotinfo.coordsys);
                rightvelocity = vecDotProduct(right,ship->obj.posinfo.velocity);

                if (rightvelocity < CIRCLE_RIGHT_VELOCITY)
                {
//                    dbgMessagef("Applying right thrust %f",rightvelocity);
                    physApplyForceToObj(&ship->obj,ship->guidance.nonstatvars.thruststrength[TRANS_RIGHT]*CIRCLE_RIGHT_THRUST,TRANS_RIGHT);
                }

                if (range > ASTEROID_HARVEST_RANGE)
                {
//                    dbgMessage("Going back towards");
                    aishipFlyToPointAvoidingObjsWithVel(ship,&target->obj.posinfo.position,0,0.0f,&target->obj.posinfo.velocity);
                }
                else
                {
                    aitrackZeroForwardVelocity(ship);
                }
            }

            break;
    }
    return;
}

void removeResourcerFromAttacking(Ship *ship)
{
    if (ship->rceffect != NULL)
    {
        TurnHarvestEffectOff(ship);
    }
}
