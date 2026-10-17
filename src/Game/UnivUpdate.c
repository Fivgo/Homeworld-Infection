/*=============================================================================
    Name    : univupdate
    Purpose : updates the universe, main game loop

    Created 10/4/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "UnivUpdate.h"
#include "Dissolve.h"
#include "ModuleShip.h"
#include "Sentinel.h"

#include <math.h>

#include "AIPlayer.h"
#include "AIShip.h"
#include "AITrack.h"
#include "Alliance.h"
#include "Battle.h"
#include "Bounties.h"
#include "Clamp.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "CommandWrap.h"
#include "ConsMgr.h"
#include "Crates.h"
#include "Damage.h"
#include "Debug.h"
#include "FastMath.h"
#include "FlightMan.h"
#include "glinc.h"
#include "HS.h"
#include "InfoOverlay.h"
#include "LaunchMgr.h"
#include "LevelLoad.h"
#include "MadLinkIn.h"
#include "MadLinkInDefs.h"
#include "mainrgn.h"
#include "Memory.h"
#include "MEX.h"
#include "Mothership.h"     // CAT: cat_mothershipStartDeathSequence (AddShipToDeleteShipList)
#include "mouse.h"
#include "MultiplayerGame.h"
#include "NetCheck.h"
#include "NIS.h"
#include "Physics.h"
#include "PiePlate.h"
#include "Ping.h"
#include "ProfileTimers.h"
#include "Randy.h"
#include "ResCollect.h"
#include "Salvage.h"
#include "SaveGame.h"
#include "Shader.h"         // CAT: catShCurrentObject (the bullet-hit glow)
#include "Select.h"
#include "Sensors.h"
#include "Ships.h"
#include "ShipNames.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "Star3d.h"
#include "StatScript.h"
#include "StringsOnly.h"
#include "Tactics.h"
#include "TradeMgr.h"
#include "Tutor.h"
#include "Tweak.h"
#include "Universe.h"
#include "Blast.h"
#include "utility.h"


#define DEBUG_COLLISIONS 0

// CAT: a missile whose target is gone lives at most this much longer (0x008b5c68; univDeleteDeadShip 0x00547d00,
//      univRemoveShipFromOutside 0x00547990)
#define CAT_MISSILE_LOST_TARGET_LIFE    5.0f

/*-----------------------------------------------------------------------------
    CAT: per-player ship counters (statics of the exe's UnivUpdate.c; names chosen here). The KAS functions
    kasfGetShipCountSalvaged/Beastified and kasfGetNumberShipsPlayerKilled/Lost read them, the salvage depot
    (M2-R4 #3) and ApplyDamageToTarget's attacker argument (M2-R4 combat) count into them, univupdateInit 0x0054af40 and
    univUpdateReset 0x0054acd0 clear them, SaveUniverse/LoadUniverse save them (cat_saveShipCounters 0x00540130,
    cat_loadShipCounters 0x005401d0; CAT-TODO(saves): not wired into this tree's save format yet).
-----------------------------------------------------------------------------*/
sdword catShipsSalvaged[MAX_MULTIPLAYER_PLAYERS][TOTAL_NUM_SHIPS];      // 0x00a6c5d0, 0x488 dwords
sdword catShipsBeastified[MAX_MULTIPLAYER_PLAYERS][TOTAL_NUM_SHIPS];    // 0x00a6d9a8, 0x488 dwords
sdword catShipsKilledByClass[MAX_MULTIPLAYER_PLAYERS][NUM_CLASSES];     // 0x00a6ebe8, 0x60 dwords: [killer][class]
sdword catShipsLostByClass[MAX_MULTIPLAYER_PLAYERS][NUM_CLASSES];       // 0x00a6d820, 0x60 dwords: [owner][class]
real32 catProtectedSpeedLimitTime = 0.0f;   // 0x00a6ed6c: when cat_univLimitProtectedShipSpeed runs next
                                            //   (univUpdateAllPosVelShips; saved after the four tables)
sdword catShipCounterA = 0;         // 0x00a6ed68: cleared with them (no other user found)
// CAT: debug frame stepping (0x00a6ed74 on, 0x00a6ed78 frames left): univUpdate 0x0054c3f0 returns at once unless a
//      frame is left; LoadUniverse restores the switch, gameEnd/nisUpdateTask/cat_kasWideScreenInFinish and
//      univUpdateReset clear it (no setter in the release game)
bool32 catUnivFrameStep = FALSE;
sdword catUnivFrameStepsLeft = 0;
// CAT: the multiplayer autosave of univUpdate (switch 0x00a74dac, next time 0x00b3a870, number 0x00a6ed80)
bool32 catMultiplayerAutoSave = FALSE;
real32 catMultiplayerAutoSaveTime = 0.0f;
sdword catMultiplayerAutoSaveNumber = 0;
#ifdef _WIN32
#define TMP_SAVEDGAMES_PATH_SEP "\\"
#else
#define TMP_SAVEDGAMES_PATH_SEP "/"
#endif

// CAT: the deferred-update ship list (see cat_univShipDeferredUpdate below)
static void catDeferredShipListReset(void);
// CAT (M2-R5 layout-dock): the exe's debug switch 0x00a751c0 (re-apply every ship's upgrades each tick; a console
//      toggle, stage 3). Name chosen here.
static bool32 catDebugReapplyUpgrades = FALSE;

static void catShipCountersReset(void)
{
    memset(catShipsKilledByClass, 0, sizeof(catShipsKilledByClass));
    memset(catShipsLostByClass, 0, sizeof(catShipsLostByClass));
    memset(catShipsSalvaged, 0, sizeof(catShipsSalvaged));
    memset(catShipsBeastified, 0, sizeof(catShipsBeastified));
}

// cat_univGetShipCountSalvaged 0x00540030: one type's count, or the sum over all types for type -1
sdword cat_univGetShipCountSalvaged(sdword shipType, sdword playerIndex)
{
    sdword i, total = 0;

    if (shipType == -1)
    {
        for (i = 0; i < TOTAL_NUM_SHIPS; i++)
        {
            total += catShipsSalvaged[playerIndex][i];
        }
        return total;
    }
    return catShipsSalvaged[playerIndex][shipType];
}

// cat_univGetShipCountBeastified 0x00540080: as cat_univGetShipCountSalvaged, for ships the Beast took over
sdword cat_univGetShipCountBeastified(sdword shipType, sdword playerIndex)
{
    sdword i, total = 0;

    if (shipType == -1)
    {
        for (i = 0; i < TOTAL_NUM_SHIPS; i++)
        {
            total += catShipsBeastified[playerIndex][i];
        }
        return total;
    }
    return catShipsBeastified[playerIndex][shipType];
}

// cat_univShipSalvagedCountInc 0x005400d0: the exe indexes the [8][145] table as [a + b * 8] (unlike its getter),
// which is kept
void cat_univShipSalvagedCountInc(sdword a, sdword b)
{
    ((sdword *)catShipsSalvaged)[a + b * MAX_MULTIPLAYER_PLAYERS]++;
}

// cat_univGetNumberShipsPlayerKilled 0x005400f0 / cat_univGetNumberShipsPlayerLost 0x00540110
sdword cat_univGetNumberShipsPlayerKilled(sdword playerIndex, sdword shipClass)
{
    return catShipsKilledByClass[playerIndex][shipClass];
}

sdword cat_univGetNumberShipsPlayerLost(sdword playerIndex, sdword shipClass)
{
    return catShipsLostByClass[playerIndex][shipClass];
}

vector defaultshipupvector = { 0.0f, 0.0f, 1.0f };
vector defaultshiprightvector = { 0.0f, -1.0f, 0.0f };
vector defaultshipheadingvector = { 1.0f, 0.0f, 0.0f };
matrix defaultshipmatrix = { 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f };

IDToPtrTable ShipIDToPtr;
IDToPtrTable ResourceIDToPtr;
IDToPtrTable DerelictIDToPtr;
IDToPtrTable MissileIDToPtr;

/*=============================================================================
    Private functions:
=============================================================================*/



// CAT (M2-R3 ships-a): not static, MothershipHouseKeep 0x005bcad0 calls it for the mothership death sequence
void MakeExplosionRockBumpables(Ship *ship,bool32 scuttle);
void pointExplosionInSpace(vector *position,real32 blastRadius,real32 maxDamage,sdword playerIndex);

void univCheckShipState(Ship *ship);
void univSetupShipForControl(Ship *ship);
void univMinorSetupShipForControl(Ship *ship);

/******************************************************************************
Private data
******************************************************************************/

bool32 gameIsEnding=FALSE;

// CAT: real-time clock behind universe.realtimeelapsed (realtimeremainder 0x00A6D9A0, a 64-bit QueryPerformanceCounter
//      value in the exe; here SDL_GetTicks milliseconds, GoK's task timer, with the same divisor utyTimerDivisor)
extern Uint32 utyTimerDivisor;
static Uint32 univRealTimeRemainder = 0;
// HWI-ORACLE: TRUE in checksum-oracle runs: realtimeelapsed then grows by exactly 1/16 s per tick, as the GOG hook DLL's
// virtual QueryPerformanceCounter (hook H3, 15 periods of the 240 Hz task timer) makes it grow (docs/m2-r2-oracle.md)
bool32 univRealTimeVirtual = FALSE;
#define UNIV_REALTIME_VIRTUAL_TICK  0.0625f

/*=============================================================================
    Tweakables
=============================================================================*/

real32 FLY_INTO_WORLD_PERCENT_DIST = 0.7f;

udword NUM_STARS            = 800;
udword NUM_BIG_STARS        = 175;
sdword MIN_COLLISION_DAMAGE =   5;

real32 MAX_VELOCITY_TANGENT_FACTOR = 1.0f;

color backgroundColor = colRGB(0, 25, 0);

scriptEntry StarTweaks[] =
{
    { "NumStars",          scriptSetUdwordCB, &NUM_STARS         },
    { "NumBigStars",       scriptSetUdwordCB, &NUM_BIG_STARS     },
    { "starMaxColor",      scriptSetUdwordCB, &starMaxColor      },
    { "starMinColor",      scriptSetUdwordCB, &starMinColor      },
    { "starRedVariance",   scriptSetUdwordCB, &starRedVariance   },
    { "starGreenVariance", scriptSetUdwordCB, &starGreenVariance },
    { "starBlueVariance",  scriptSetUdwordCB, &starBlueVariance  },

    END_SCRIPT_ENTRY
};

scriptEntry GalaxyTweaks[] =
{
    { "BackgroundColor", scriptSetRGBCB, &backgroundColor },

    END_SCRIPT_ENTRY
};

void univUpdateObjRotInfo(SpaceObjRot *rotatable)
{
    if (!bitTest(rotatable->header.flags, SOF_Impactable)) return;

    SpaceObjRotImp* asImpactable = (SpaceObjRotImp *)rotatable;
    matMultiplyMatByVec(
        &asImpactable->collInfo.collOffset,
        &rotatable->rotinfo.coordsys,
        &rotatable->obj.staticinfo->staticCollInfo.collsphereoffset
    );
    vecAdd(asImpactable->collInfo.collPosition, rotatable->obj.posinfo.position, asImpactable->collInfo.collOffset);
    collUpdateCollRectangle(asImpactable);
    if (rotatable->header.objtype != OBJ_ShipType) return;

    Ship * asShip = (Ship *)rotatable;
    matMultiplyMatByVec(
        &asShip->engineOffset,
        &rotatable->rotinfo.coordsys,
        &asShip->staticinfo->engineNozzleOffset[0]);

    vecAdd(asShip->enginePosition, rotatable->obj.posinfo.position, asShip->engineOffset);
}

/*-----------------------------------------------------------------------------
    Name        : univRotateObjPitch
    Description : rotates an obj about its right vector (producing pitch), correctly
                  rotating collision information, etc.
    Inputs      : obj
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univRotateObjPitch(SpaceObjRot *robj,real32 rot)
{
    matrix rotymat;
    matrix tempmat;

    matMakeRotAboutY(&rotymat,(real32)cos(rot),(real32)sin(rot));
    tempmat = robj->rotinfo.coordsys;
    matMultiplyMatByMat(&robj->rotinfo.coordsys,&tempmat,&rotymat);

    univUpdateObjRotInfo(robj);
}

/*-----------------------------------------------------------------------------
    Name        : univRotateObjYaw
    Description : rotates an obj about its up vector (producing yaw), correctly
                  rotating collision information, etc.
    Inputs      : obj
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univRotateObjYaw(SpaceObjRot *robj,real32 rot)
{
    matrix rotxmat;
    matrix tempmat;

    matMakeRotAboutX(&rotxmat,(real32)cos(rot),(real32)sin(rot));
    tempmat = robj->rotinfo.coordsys;
    matMultiplyMatByMat(&robj->rotinfo.coordsys,&tempmat,&rotxmat);

    univUpdateObjRotInfo(robj);
}

/*-----------------------------------------------------------------------------
    Name        : univRotateObjRoll
    Description : rotates an obj about its heading vector (producing Roll), correctly
                  rotating collision information, etc.
    Inputs      : obj
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univRotateObjRoll(SpaceObjRot *robj,real32 rot)
{
    matrix rotzmat;
    matrix tempmat;

    matMakeRotAboutZ(&rotzmat,(real32)cos(rot),(real32)sin(rot));
    tempmat = robj->rotinfo.coordsys;
    matMultiplyMatByMat(&robj->rotinfo.coordsys,&tempmat,&rotzmat);

    univUpdateObjRotInfo(robj);
}

/*-----------------------------------------------------------------------------
    Name        : univInitSpaceObjPosRot
    Description : initializes the position and rotation info of a space object,
                  including orientation (heading), position
    Inputs      : position, randomOrientation flag
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univInitSpaceObjPosRot(SpaceObj *obj,vector *position,bool32 randomOrientation)
{
    memset(&(obj->posinfo),0,sizeof(obj->posinfo));
    obj->posinfo.position = *position;
    SET_MOVING_IMMOBILE(obj->posinfo.isMoving);
    obj->posinfo.haventCalculatedDist = TRUE;

    if (bitTest(obj->header.flags, SOF_Rotatable))
    {
        SpaceObjRot* asRotatable = (SpaceObjRot*)obj;
        memset(&asRotatable->rotinfo,0,sizeof(asRotatable->rotinfo));
        asRotatable->rotinfo.coordsys = defaultshipmatrix;

        if (randomOrientation)
        {
            real32 rotaboutx = frandom(2*PI);
            real32 rotabouty = frandom(2*PI);
            real32 rotaboutz = frandom(2*PI);
            matrix rotxmat;
            matrix rotymat;
            matrix rotzmat;
            matrix tempmat;

            matMakeRotAboutX(&rotxmat,(real32)cos(rotaboutx),(real32)sin(rotaboutx));
            matMakeRotAboutY(&rotymat,(real32)cos(rotabouty),(real32)sin(rotabouty));
            matMakeRotAboutZ(&rotzmat,(real32)cos(rotaboutz),(real32)sin(rotaboutz));

            tempmat = asRotatable->rotinfo.coordsys;
            matMultiplyMatByMat(&asRotatable->rotinfo.coordsys,&tempmat,&rotxmat);
            matMultiplyMatByMat(&tempmat,&asRotatable->rotinfo.coordsys,&rotymat);
            matMultiplyMatByMat(&asRotatable->rotinfo.coordsys,&tempmat,&rotzmat);
        }

        univUpdateObjRotInfo(asRotatable);
    }
}

#define IDTOPTR_GROWBATCH       250

void IDToPtrTableInit(IDToPtrTable *table)
{
    table->numEntries = 0;
    table->objptrs = NULL;
}

void IDToPtrTableClose(IDToPtrTable *table)
{
    if (table->objptrs)
    {
        memFree(table->objptrs);
        table->objptrs = NULL;
    }
    table->numEntries = 0;
}

void IDToPtrTableReset(IDToPtrTable *table)
{
    IDToPtrTableClose(table);
    IDToPtrTableInit(table);
}

void IDToPtrTableObjDied(IDToPtrTable *table,uword ID)
{
    if (ID >= table->numEntries)
    {
        return;
    }

    table->objptrs[ID] = NULL;      // remove reference
}

SpaceObjPtr IDToPtrTableIDToObj(IDToPtrTable *table,uword ID)
{
    if (ID >= table->numEntries)
    {
        return NULL;
    }

    return table->objptrs[ID];
}

void IDToPtrTableAdd(IDToPtrTable *table,uword ID,SpaceObj *obj)
{
    dbgAssertOrIgnore(obj);

    if (ID >= table->numEntries)
    {
        sdword newnumber = table->numEntries+IDTOPTR_GROWBATCH;

        if (table->objptrs)
        {
            table->objptrs = memRealloc(table->objptrs,sizeof(SpaceObjPtr) * newnumber,"idtoptrs",0);
        }
        else
        {
            table->objptrs = memAlloc(sizeof(SpaceObjPtr) * newnumber,"idtoptrs",0);
        }
        memset(&table->objptrs[table->numEntries],0,sizeof(SpaceObjPtr)*IDTOPTR_GROWBATCH);
        table->numEntries += IDTOPTR_GROWBATCH;
    }

    dbgAssertOrIgnore(ID < table->numEntries);

    if (table->objptrs[ID])
    {
        dbgFatalf(DBG_Loc,"Obj with this ID %d already exists",ID);
    }
    else
    {
        table->objptrs[ID] = obj;
    }
    return;
}

/*=============================================================================
    Save Game Stuff
=============================================================================*/

typedef struct SaveIDToPtrTableStruct {
    sdword num;
    sdword ID[1];
} SaveIDToPtrTableStruct;

#define sizeofSaveIDToPtrTableStruct(n) (sizeof(SaveIDToPtrTableStruct) + (n-1)*sizeof(sdword))

void SaveIDToPtrTable(IDToPtrTable *table)
{
    sdword num = table->numEntries;
    SaveChunk *chunk;
    struct SaveIDToPtrTableStruct *savecontents;
    sdword i;

    chunk = CreateChunk(IDTOPTRTABLE,sizeofSaveIDToPtrTableStruct(num),NULL);

    savecontents = chunkContents(chunk);
    savecontents->num = num;

    for (i=0;i<num;i++)
    {
        savecontents->ID[i] = SpaceObjRegistryGetID(table->objptrs[i]);
    }

    SaveThisChunk(chunk);
    memFree(chunk);
}

void LoadIDToPtrTable(IDToPtrTable *table)
{
    SaveChunk *chunk;
    SaveIDToPtrTableStruct *loadcontents;
    sdword num;
    sdword i;

    chunk = LoadNextChunk();
    VerifyChunkNoSize(chunk,IDTOPTRTABLE);

    loadcontents = (SaveIDToPtrTableStruct *)chunkContents(chunk);
    num = loadcontents->num;

    dbgAssertOrIgnore(sizeofSaveIDToPtrTableStruct(num) == chunk->contentsSize);

    table->numEntries = num;
    if (num == 0)
    {
        table->objptrs = NULL;
    }
    else
    {
        table->objptrs = memAlloc(sizeof(SpaceObjPtr) * num,"idtoptrs",0);
    }

    for (i=0;i<num;i++)
    {
        table->objptrs[i] = SpaceObjRegistryGetObj(loadcontents->ID[i]);
}

    memFree(chunk);
}


/*=============================================================================
    Save Game Stuff End
=============================================================================*/

void univInitFastNetworkIDLookups(void)
{
    IDToPtrTableInit(&ShipIDToPtr);
    IDToPtrTableInit(&ResourceIDToPtr);
    IDToPtrTableInit(&DerelictIDToPtr);
    IDToPtrTableInit(&MissileIDToPtr);
}

void univCloseFastNetworkIDLookups(void)
{
    IDToPtrTableClose(&ShipIDToPtr);
    IDToPtrTableClose(&ResourceIDToPtr);
    IDToPtrTableClose(&DerelictIDToPtr);
    IDToPtrTableClose(&MissileIDToPtr);
}

void univResetFastNetworkIDLookups(void)
{
    IDToPtrTableReset(&ShipIDToPtr);
    IDToPtrTableReset(&ResourceIDToPtr);
    IDToPtrTableReset(&DerelictIDToPtr);
    IDToPtrTableReset(&MissileIDToPtr);
}

/*-----------------------------------------------------------------------------
    Name        : ShipIDtoShip
    Description : Converts a ShipID to a pointer to the actual ship
    Inputs      : shipID
    Outputs     :
    Return      :
    Notes       : Later optimize this routine (perhaps use a table look-up)
----------------------------------------------------------------------------*/
Ship *ShipIDtoShip(ShipID shipID,bool32 considerInsideShips)
{
    Ship *ship = (Ship *)IDToPtrTableIDToObj(&ShipIDToPtr,shipID.shipNumber);

    if (ship != NULL)
    {
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
        if (!considerInsideShips)
        {
            if (ship->header.flags & SOF_Hide)
            {
                ship = NULL;
            }
        }
    }

    return ship;
}

/*-----------------------------------------------------------------------------
    Name        : univFindShipIAmInside
    Description : returns the ship I am inside if I'm inside one, otherwise NULL
    Inputs      : me
    Outputs     :
    Return      : returns the ship I am inside if I'm inside one, otherwise NULL
----------------------------------------------------------------------------*/
Ship *univFindShipIAmInside(Ship *me)
{
    Node *objnode = universe.ShipList.head;
    Ship *ship;

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (ship->shipsInsideMe)
        {
            Node *insidenode = ship->shipsInsideMe->insideList.head;
            InsideShip *insideship;

            while (insidenode != NULL)
            {
                insideship = (InsideShip *)listGetStructOfNode(insidenode);
                dbgAssertOrIgnore(insideship->ship->header.objtype == OBJ_ShipType);

                if (insideship->ship == me)
                {
                    return ship;
                }

                insidenode = insidenode->next;
            }
        }

        objnode = objnode->next;
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
    Name        : univAmIInsideThisShip
    Description : returns TRUE if me ship is inside ship
    Inputs      : me,ship
    Outputs     :
    Return      : returns TRUE if me ship is inside ship
----------------------------------------------------------------------------*/
bool32 univAmIInsideThisShip(Ship *me,Ship *ship)
{
    if (ship->shipsInsideMe)
    {
        Node *insidenode = ship->shipsInsideMe->insideList.head;
        InsideShip *insideship;

        while (insidenode != NULL)
        {
            insideship = (InsideShip *)listGetStructOfNode(insidenode);
            dbgAssertOrIgnore(insideship->ship->header.objtype == OBJ_ShipType);

            if (insideship->ship == me)
            {
                return TRUE;
            }

            insidenode = insidenode->next;
        }
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : ResourceIDtoResourcePtr
    Description : Converts a resourceID to a pointer to the actual resource
    Inputs      : resourceID
    Outputs     :
    Return      :
    Notes       : Later optimize this routine (perhaps use a table look-up)
----------------------------------------------------------------------------*/
ResourcePtr ResourceIDtoResourcePtr(ResourceID resourceID)
{
    return (Resource *)IDToPtrTableIDToObj(&ResourceIDToPtr,resourceID.resourceNumber);
}

/*-----------------------------------------------------------------------------
    Name        : DerelictIDtoDerelictPtr
    Description : Converts a derelictID to a pointer to the actual derelict
    Inputs      : derelictID
    Outputs     :
    Return      :
    Notes       : Later optimize this routine (perhaps use a table look-up)
----------------------------------------------------------------------------*/
DerelictPtr DerelictIDToDerelictPtr(DerelictID derelictID)
{
    return (Derelict *)IDToPtrTableIDToObj(&DerelictIDToPtr,derelictID.derelictNumber);
}

/*-----------------------------------------------------------------------------
    Name        : MissileIDtoMissilePtr
    Description : Converts a missileID to a pointer to the actual missile
    Inputs      : missileID
    Outputs     :
    Return      :
    Notes       : Later optimize this routine (perhaps use a table look-up)
----------------------------------------------------------------------------*/
MissilePtr MissileIDtoMissilePtr(MissileID missileID)
{
    return (Missile *)IDToPtrTableIDToObj(&MissileIDToPtr,missileID.missileNumber);
}

/*-----------------------------------------------------------------------------
    Name        : univAddAsteroid
    Description : adds an asteroid to the mission sphere at position astpos
    Inputs      : astpos
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
Asteroid *univAddAsteroid(AsteroidType asteroidtype,vector *astpos)
{
    Asteroid *newAsteroid;

    dbgAssertOrIgnore(asteroidtype < NUM_ASTEROIDTYPES);

    newAsteroid = memAlloc(sizeof(Asteroid),"Asteroid",NonVolatile);

    newAsteroid->resource.resourceID.resourceNumber = universe.resourceNumber;
    IDToPtrTableAdd(&ResourceIDToPtr,universe.resourceNumber,(SpaceObj *)newAsteroid);
    universe.resourceNumber++;

    newAsteroid->header.objtype = OBJ_AsteroidType;
    newAsteroid->header.flags = SOF_Rotatable + SOF_Impactable + SOF_Targetable + SOF_Resource;
    if (asteroidtype == Asteroid0) newAsteroid->header.flags |= SOF_DontApplyPhysics;      // optimization for Asteroid0
    AsteroidStaticInfo* staticInfo = &asteroidStaticInfos[asteroidtype];
    newAsteroid->staticinfo = staticInfo;
    ClearNode(newAsteroid->obj.renderlink);
    newAsteroid->obj.collMyBlob = NULL;
    newAsteroid->obj.currentLOD = 0;
    newAsteroid->obj.cameraDistanceSquared = REALlyBig;
    newAsteroid->obj.attributes = 0;
    newAsteroid->obj.attributesParam = 0;
    newAsteroid->impactable.collInfo.precise = NULL;
    newAsteroid->targetable.flashtimer = 0;
    newAsteroid->targetable.lasttimecollided = 0;
    newAsteroid->targetable.health = staticInfo->health.maxhealth;
    newAsteroid->resource.resourcevalue = staticInfo->resource.resourcevalue;
    newAsteroid->resource.resourceNotAccessible = 0;
    newAsteroid->resource.resourceVolume = NULL;
    newAsteroid->asteroidtype = asteroidtype;
    newAsteroid->scaling = 1.0f;

    vecZeroVector(newAsteroid->obj.posinfo.force);
    vecZeroVector(newAsteroid->rotatable.rotinfo.torque);

    univInitSpaceObjPosRot(&newAsteroid->obj,astpos,TRUE);

    if (asteroidtype == Asteroid0)
    {
        newAsteroid->obj.collMyBlob = NULL;
        listAddNode(&universe.MinorSpaceObjList,&(newAsteroid->header.objlink),newAsteroid);
    }
    else
    {
        collAddSpaceObjToCollBlobs(&newAsteroid->obj);
        listAddNode(&universe.SpaceObjList,&(newAsteroid->header.objlink),newAsteroid);
        listAddNode(&universe.ResourceList,&(newAsteroid->resource.resourcelink),newAsteroid);
        listAddNode(&universe.ImpactableList,&(newAsteroid->impactable.impactablelink),newAsteroid);
        cat_univShipMeshStateCreate(&newAsteroid->obj);    // CAT: univAddAsteroid 0x00540a30 (NULL: not a ship)
    }
    newAsteroid->obj.catEffectBlock = NULL;                 // CAT: both paths clear +0x84 and +0x80 (0x00540a30)
    newAsteroid->obj.catMeshState = NULL;

    return newAsteroid;
}

/*-----------------------------------------------------------------------------
    Name        : cat_univAddMimicResource
    Description : CAT (M2-R4 ships-b): the fake asteroid an idle mimic posing as an asteroid leaves (cat_univAddMimicResource
                  0x005408b0): an asteroid of the type with a resource number, on the resource list only (not in the
                  object, impactable or render lists, no collision blob or mesh state)
    Inputs      : asteroidtype, pos
    Return      : the resource
----------------------------------------------------------------------------*/
Resource *cat_univAddMimicResource(sdword asteroidtype, vector *pos)
{
    Asteroid *newAsteroid = memAlloc(sizeof(Asteroid),"MimicAsteroid",NonVolatile);
    AsteroidStaticInfo *staticInfo = &asteroidStaticInfos[asteroidtype];

    memset(newAsteroid, 0, sizeof(Asteroid));
    newAsteroid->resource.resourceID.resourceNumber = universe.resourceNumber;
    IDToPtrTableAdd(&ResourceIDToPtr,universe.resourceNumber,(SpaceObj *)newAsteroid);
    universe.resourceNumber++;

    newAsteroid->staticinfo = staticInfo;
    newAsteroid->header.objtype = OBJ_AsteroidType;
    newAsteroid->header.flags |= SOF_Rotatable + SOF_Impactable + SOF_Targetable + SOF_Resource;
    ClearNode(newAsteroid->obj.renderlink);
    newAsteroid->obj.collMyBlob = NULL;
    newAsteroid->obj.currentLOD = 0;
    newAsteroid->obj.cameraDistanceSquared = REALlyBig;
    newAsteroid->obj.attributesParam = 0;
    newAsteroid->resource.resourcevalue = staticInfo->resource.resourcevalue;
    newAsteroid->asteroidtype = asteroidtype;
    newAsteroid->scaling = 1.0f;
    newAsteroid->targetable.health = staticInfo->health.maxhealth;
    univInitSpaceObjPosRot(&newAsteroid->obj,pos,TRUE);
    listAddNode(&universe.ResourceList,&(newAsteroid->resource.resourcelink),newAsteroid);
    newAsteroid->obj.catEffectBlock = NULL;
    newAsteroid->obj.catMeshState = NULL;
    return &newAsteroid->resource;
}

/*-----------------------------------------------------------------------------
    Name        : univAddDerelict
    Description : adds a derelict to the mission sphere at position pos
    Inputs      : pos
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
Derelict *univAddDerelict(DerelictType derelicttype,vector *pos)
{
    dbgAssertOrIgnore(derelicttype < NUM_DERELICTTYPES);
    return(univAddDerelictByStatInfo(derelicttype, (DerelictStaticInfo *)&derelictStaticInfos[derelicttype], pos));
}

Derelict *univAddHyperspaceGateAsDerelict(hvector *posAndRotation)
{
    Derelict *derelict = univAddDerelict(HyperspaceGate,(vector *)posAndRotation);
    univRotateObjYaw((SpaceObjRot *)derelict,posAndRotation->w);
    bitSet(derelict->header.flags,SOF_NotBumpable|SOF_DontApplyPhysics);

    // debug
    //bitSet(derelict->header.flags,SOF_ForceVisible);

    return derelict;
}

/*-----------------------------------------------------------------------------
    Name        : univAddMissile
    Description : Adds a missile to the world.  This missile will have no
                    owner, no target and it's velocity, position and coordsys
                    must be set up by the owner.  Some missile this is!
    Inputs      : race - race of missile to create
    Outputs     :
    Return      : pointer to newly created missile
----------------------------------------------------------------------------*/
Missile *univAddMissile(ShipRace race)
{
    Missile *missile;
    sdword i;

    missile = memAlloc(sizeof(Missile),"Missile",0);
    memset(missile,0,sizeof(Missile));

    missile->header.objtype = OBJ_MissileType;
    missile->header.flags = SOF_Rotatable + SOF_Impactable + SOF_NotBumpable + SOF_Targetable;

    missile->enginePosition.x = 0.0f;
    missile->enginePosition.y = 0.0f;
    missile->enginePosition.z = 0.0f;
    missile->trail = NULL;

    missile->staticinfo = &missileStaticInfos[race];
    missile->maxtraveldist = missile->staticinfo->common.maxvelocity * universe.phystimeelapsed;
    missile->missileType = MISSILE_Regular;
    missile->FORCE_DROPPED = FALSE;
    missile->formationinfo = NULL;
    missile->trail = mistrailNew(missile->staticinfo->trailStatic, missile);

    missile->targetable.health = missile->staticinfo->health.maxhealth;

    ClearNode(missile->obj.renderlink);
    missile->obj.currentLOD = 0;
    missile->obj.cameraDistanceSquared = 1.0f;  //!!!
    missile->obj.attributes = 0;
    missile->obj.attributesParam = 0;
    missile->targetable.flashtimer = 0;
    missile->targetable.lasttimecollided = 0;

    vecZeroVector(missile->obj.posinfo.force);
    vecZeroVector(missile->rotatable.rotinfo.torque);

    for (i=0;i<NUM_TRANS_DEGOFFREEDOM;i++)
    {
       missile->guidance.nonstatvars.thruststrength[i] = missile->staticinfo->guidance.thruststrengthstat[i];
    }
    for(i=0;i< NUM_ROT_DEGOFFREEDOM;i++)
    {
       missile->guidance.nonstatvars.rotstrength[i] = missile->staticinfo->guidance.rotstrengthstat[i];
    }
    for(i=0;i< NUM_TURN_TYPES;i++)
    {
       missile->guidance.nonstatvars.turnspeed[i] = missile->staticinfo->guidance.turnspeedstat[i];
    }
    // CAT (M2-R5 layout): the 16th NonStats float, the static top speed (univAddMissile 0x00540c70 at 0x00540e6f)
    missile->guidance.nonstatvars.catMaxVelocity = missile->staticinfo->common.maxvelocity;

    missile->soundType = GS_MissleLauncher;
    missile->race = race;
    missile->timelived = 0.0f;
    missile->totallifetime = 12.26f;                        //!!! from the missile destroyer .ship file
    missile->playerowner = &universe.players[((race == universe.players[0].race)) ? 0 : 1];
    missile->owner = NULL;
    missile->target = NULL;

    missile->damage = 48.0f;                                //from the.shp file

    missile->missileID.missileNumber = universe.missileNumber;
    // CAT-TODO(stage 3/5 recorder): the exe also adds the ID while a game is being recorded in multiplayer
    //      (univAddMissile 0x00540c70: multiPlayerGame || (0x00a7519c && 0x00a75190), the two flags set by
    //      recPackInGameStartCBSafeToStart 0x004cbbb0)
    if (multiPlayerGame) IDToPtrTableAdd(&MissileIDToPtr,universe.missileNumber,(SpaceObj *)missile);
    universe.missileNumber++;

    //position, velocity and coordsys left blank
    SET_MOVING_LINEARLY(missile->obj.posinfo.isMoving);
    missile->obj.posinfo.haventCalculatedDist = TRUE;

    collAddSpaceObjToCollBlobs((SpaceObj *)missile);

    //proper hit effect
    missile->hitEffect = etgGunEventTable[race][GS_MissleLauncher][EGT_GunHit];//get pointer to hit effect

    listAddNode(&universe.SpaceObjList,&(missile->header.objlink),missile);
    listAddNode(&universe.ImpactableList,&(missile->impactable.impactablelink),missile);
    listAddNode(&universe.MissileList,&(missile->missilelink),missile);
    missile->obj.catEffectBlock = NULL;                     // CAT: univAddMissile 0x00540c70 clears +0x84, +0x80
    missile->obj.catMeshState = NULL;

    return(missile);
}

/*-----------------------------------------------------------------------------
    Name        : univAddToWorldList
    Description : Add a derelict to the universe's world list
    Inputs      : world - what object to add to the world list
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void univAddToWorldList(Derelict *world)
{
    sdword index;

    for (index = 0; index < UNIV_NUMBER_WORLDS; index++)
    {
        if (universe.world[index] == NULL)
        {
            universe.world[index] = world;
            return;
        }
    }
    dbgAssertOrIgnore(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : univRemoveFromWorldList
    Description : Remove a derelict reference from the universe's world list
    Inputs      : world - what world we are to remove
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void univRemoveFromWorldList(Derelict *world)
{
    sdword index, j;

    for (index = 0; index < UNIV_NUMBER_WORLDS; index++)
    {
        if (universe.world[index] == world)
        {
            if (index < UNIV_NUMBER_WORLDS - 1)
            {
                for (j = UNIV_NUMBER_WORLDS - 1; j > index; j--)
                {
                    if (universe.world[j] != NULL)
                    {
                        universe.world[index] = universe.world[j];
                        universe.world[j] = NULL;
                        return;
                    }
                }
            }
            universe.world[index] = NULL;
            break;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : univAddDerelictByStatInfo
    Description : Add a derelict by direct staticinfo pointer rather than just
                    by type.  Mostly used for generic objects in NIS's.
    Inputs      : derelictType - type of derelict.  Can be outside range of 0..NUM_DERELICTTYPES
                  stat - static info for this derelict.
                  pos - where to create it
    Outputs     :
    Return      : Newly allocated derelict.
----------------------------------------------------------------------------*/
Derelict *univAddDerelictByStatInfo(DerelictType derelictType, DerelictStaticInfo *stat, vector *pos)
{
    Derelict *newDerelict;
    sdword i;

    newDerelict = memAlloc(sizeof(Derelict),"Derelict",NonVolatile);

    newDerelict->derelictID.derelictNumber = universe.derelictNumber;
    IDToPtrTableAdd(&DerelictIDToPtr,universe.derelictNumber,&newDerelict->obj);
    universe.derelictNumber++;

    newDerelict->header.objtype = OBJ_DerelictType;
    newDerelict->header.flags = SOF_Rotatable + SOF_Impactable + SOF_Targetable;
    newDerelict->derelicttype = derelictType;
    newDerelict->staticinfo = stat;
    newDerelict->TractorTrue = FALSE;
    newDerelict->tractorbeaminfo = 0;
    newDerelict->tractorbeam_playerowner = NULL;
    newDerelict->tractorPosinfo[0] = FALSE;
    newDerelict->tractorPosinfo[1] = FALSE;
    newDerelict->tractorPosinfo[2] = FALSE;
    newDerelict->tractorPosinfo[3] = FALSE;
    newDerelict->creationTime = universe.totaltimeelapsed;
    newDerelict->ambientSoundHandle = SOUND_NOTINITED;
    newDerelict->randomSoundHandle = SOUND_NOTINITED;
    newDerelict->nextRandomTime = universe.totaltimeelapsed;
    ClearNode(newDerelict->obj.renderlink);
    newDerelict->obj.collMyBlob = NULL;
    newDerelict->obj.renderedLODs = 0;
    newDerelict->obj.currentLOD = 0;
    newDerelict->obj.cameraDistanceSquared = REALlyBig;
    newDerelict->obj.attributes = 0;
    newDerelict->obj.attributesParam = 0;
    newDerelict->impactable.collInfo.precise = NULL;
    newDerelict->targetable.flashtimer = 0;
    newDerelict->targetable.lasttimecollided = 0;
    newDerelict->targetable.health = newDerelict->staticinfo->health.maxhealth;
    newDerelict->guidance.clampInfo = NULL;
    newDerelict->shipOrDerelict.dockingship = NULL;
    newDerelict->shipOrDerelict.putOnDoor=FALSE;
    newDerelict->shipOrDerelict.aistate = 0;
    newDerelict->shipOrDerelict.specialFlags = 0;
    newDerelict->shipOrDerelict.specialFlags2 = 0;

    vecZeroVector(newDerelict->obj.posinfo.force);
    vecZeroVector(newDerelict->rotatable.rotinfo.torque);

    for(i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
    {
        newDerelict->shipOrDerelict.salvageNumTagged[i] = 0;
    }


    for (i=0;i<NUM_TRANS_DEGOFFREEDOM;i++)
    {
        newDerelict->guidance.nonstatvars.thruststrength[i] = stat->guidance.thruststrengthstat[i];
    }
    for(i=0;i< NUM_ROT_DEGOFFREEDOM;i++)
    {
        newDerelict->guidance.nonstatvars.rotstrength[i] = stat->guidance.rotstrengthstat[i];
    }
    for(i=0;i< NUM_TURN_TYPES;i++)
    {
        newDerelict->guidance.nonstatvars.turnspeed[i] = stat->guidance.turnspeedstat[i];
    }

    newDerelict->newDockWithTransfer = NULL;
    univInitSpaceObjPosRot(&newDerelict->obj,pos,FALSE);

    // CAT (M2-R4 combat): no HW1 per-object salvage points (setSalvageInfo and ShipOrDerelict.salvageInfo are gone):
    //      Cataclysm salvages ships at their DPT_Salvage dock points (Salvage.c); univAddDerelictByStatInfo 0x00541000
    //      allocates nothing of the kind

    InitializeNavLights((Ship *)newDerelict);

    collAddSpaceObjToCollBlobs(&newDerelict->obj);
    listAddNode(&universe.SpaceObjList,&(newDerelict->header.objlink),newDerelict);
    listAddNode(&universe.DerelictList,&(newDerelict->derelictlink),newDerelict);
    listAddNode(&universe.ImpactableList,&(newDerelict->impactable.impactablelink),newDerelict);
    newDerelict->obj.catEffectBlock = NULL;                 // CAT: univAddDerelictByStatInfo 0x00541000 clears +0x84
    cat_univShipMeshStateCreate(&newDerelict->obj);         //      and creates the mesh state (NULL: not a ship)

    if (stat->worldRender)
    {                                                       //if planet-style rendering
//        bitSet(newDerelict->header.flags, SOF_ForceVisible);
        univAddToWorldList(newDerelict);
    }

    //find a color scheme
    for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++)
    {
        if (stat->shipOrDerelict.teamColor[i])
        {
            newDerelict->colorScheme = i;
            break;
        }
    }

    //if a crate, report the placement!
    if (!singlePlayerGame)
    {
        if(derelictType == Crate)
        {
            cratesReportCratePlacement(newDerelict);
        }
    }

    return newDerelict;
}

/*-----------------------------------------------------------------------------
    Name        : univAddDustCloud
    Description : adds a dustcloud to the mission sphere at position cloudpos
    Inputs      : cloudpos
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
DustCloud *univAddDustCloud(DustCloudType dustcloudtype,vector *cloudpos)
{
    DustCloud* newCloud;

    dbgAssertOrIgnore(dustcloudtype < NUM_DUSTCLOUDTYPES);

    newCloud = memAlloc(sizeof(DustCloud),"DustCloud",NonVolatile);

    newCloud->resource.resourceID.resourceNumber = universe.resourceNumber;
    IDToPtrTableAdd(&ResourceIDToPtr,universe.resourceNumber,(SpaceObj *)newCloud);
    universe.resourceNumber++;

    newCloud->header.objtype = OBJ_DustType;
    newCloud->header.flags = SOF_Rotatable + SOF_Impactable + SOF_Targetable + SOF_Resource + SOF_NotBumpable;
    newCloud->staticinfo = &dustcloudStaticInfos[dustcloudtype];
    newCloud->dustcloudtype = dustcloudtype;
    newCloud->scaling = 1.0f;
    ClearNode(newCloud->obj.renderlink);
    newCloud->obj.collMyBlob = NULL;
    newCloud->obj.currentLOD = 0;
    newCloud->obj.cameraDistanceSquared = REALlyBig;
    newCloud->obj.attributes = 0;
    newCloud->obj.attributesParam = 0;
    newCloud->impactable.collInfo.precise = NULL;
    newCloud->targetable.flashtimer = 0;
    newCloud->targetable.lasttimecollided = 0;
    newCloud->targetable.health = newCloud->staticinfo->health.maxhealth;
    newCloud->resource.resourcevalue = newCloud->staticinfo->resource.resourcevalue;
    newCloud->resource.resourceNotAccessible = 0;
    newCloud->resource.resourceVolume = NULL;
    {
        cloudSystem* csys = cloudCreateSystem(newCloud->staticinfo->common.staticCollInfo.collspheresize, 0.1f);
        csys->position = &newCloud->obj.posinfo.position;
        csys->rotation = &newCloud->rotatable.rotinfo.coordsys;
        csys->maxhealth = (sdword) newCloud->targetable.health;
        newCloud->stub = csys;
    }

    vecZeroVector(newCloud->obj.posinfo.force);
    vecZeroVector(newCloud->rotatable.rotinfo.torque);

    univInitSpaceObjPosRot((SpaceObj *)newCloud,cloudpos,TRUE);

    collAddSpaceObjToCollBlobs((SpaceObj *)newCloud);
    listAddNode(&universe.SpaceObjList,&(newCloud->header.objlink),newCloud);
    listAddNode(&universe.ResourceList,&(newCloud->resource.resourcelink),newCloud);
    listAddNode(&universe.ImpactableList,&(newCloud->impactable.impactablelink),newCloud);
    newCloud->obj.catEffectBlock = NULL;    // CAT: univAddDustCloud 0x00541300 / univAddGasCloud 0x00541500 clear +0x84, +0x80
    newCloud->obj.catMeshState = NULL;

    return newCloud;
}

/*-----------------------------------------------------------------------------
    Name        : univAddGasCloud
    Description : adds a Gascloud to the mission sphere at position cloudpos
    Inputs      : cloudpos
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
GasCloud *univAddGasCloud(GasCloudType gascloudtype,vector *cloudpos)
{
    GasCloud *newCloud;

    dbgAssertOrIgnore(gascloudtype < NUM_GASCLOUDTYPES);

    newCloud = memAlloc(sizeof(GasCloud),"GasCloud",NonVolatile);

    newCloud->resource.resourceID.resourceNumber = universe.resourceNumber;
    IDToPtrTableAdd(&ResourceIDToPtr,universe.resourceNumber,(SpaceObj *)newCloud);
    universe.resourceNumber++;

    newCloud->header.objtype = OBJ_GasType;
    newCloud->header.flags = SOF_Rotatable + SOF_Impactable + SOF_Targetable + SOF_Resource + SOF_NotBumpable;
    newCloud->staticinfo = &gascloudStaticInfos[gascloudtype];
    newCloud->gascloudtype = gascloudtype;
    newCloud->scaling = 1.0f;
    ClearNode(newCloud->obj.renderlink);
    newCloud->obj.collMyBlob = NULL;
    newCloud->obj.currentLOD = 0;
    newCloud->obj.cameraDistanceSquared = REALlyBig;
    newCloud->obj.attributes = 0;
    newCloud->obj.attributesParam = 0;
    newCloud->impactable.collInfo.precise = NULL;
    newCloud->targetable.flashtimer = 0;
    newCloud->targetable.lasttimecollided = 0;
    newCloud->targetable.health = newCloud->staticinfo->health.maxhealth;
    newCloud->resource.resourcevalue = newCloud->staticinfo->resource.resourcevalue;
    newCloud->resource.resourceNotAccessible = 0;
    newCloud->resource.resourceVolume = NULL;

    vecZeroVector(newCloud->obj.posinfo.force);
    vecZeroVector(newCloud->rotatable.rotinfo.torque);

    univInitSpaceObjPosRot((SpaceObj *)newCloud,cloudpos,TRUE);

    collAddSpaceObjToCollBlobs((SpaceObj *)newCloud);
    listAddNode(&universe.SpaceObjList,&(newCloud->header.objlink),newCloud);
    listAddNode(&universe.ResourceList,&(newCloud->resource.resourcelink),newCloud);
    listAddNode(&universe.ImpactableList,&(newCloud->impactable.impactablelink),newCloud);
    newCloud->obj.catEffectBlock = NULL;    // CAT: univAddDustCloud 0x00541300 / univAddGasCloud 0x00541500 clear +0x84, +0x80
    newCloud->obj.catMeshState = NULL;

    return newCloud;
}

/*-----------------------------------------------------------------------------
    Name        : univAddNebula
    Description : adds a nebula to the mission sphere at position nebpos
    Inputs      : nebpos
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
Nebula* univAddNebula(NebulaType nebulatype,vector* nebpos, void* stub)
{
    Nebula* newNeb;

    dbgAssertOrIgnore(nebulatype < NUM_NEBULATYPES);

    newNeb = memAlloc(sizeof(Nebula), "Nebula", NonVolatile);

    newNeb->resource.resourceID.resourceNumber = universe.resourceNumber;
    IDToPtrTableAdd(&ResourceIDToPtr,universe.resourceNumber,(SpaceObj *)newNeb);
    universe.resourceNumber++;

    newNeb->header.objtype = OBJ_NebulaType;
    newNeb->header.flags = SOF_Rotatable + SOF_Impactable + SOF_Targetable + SOF_Resource + SOF_NotBumpable;
    newNeb->staticinfo = &nebulaStaticInfos[nebulatype];
    newNeb->nebulatype = nebulatype;
    newNeb->scaling = 1.0f;
    ClearNode(newNeb->obj.renderlink);
    newNeb->obj.collMyBlob = NULL;
    newNeb->obj.currentLOD = 0;
    newNeb->obj.cameraDistanceSquared = REALlyBig;
    newNeb->obj.attributes = nebAttributes;
    newNeb->obj.attributesParam = 0;
    newNeb->impactable.collInfo.precise = NULL;
    newNeb->targetable.flashtimer = 0;
    newNeb->targetable.lasttimecollided = 0;
    newNeb->targetable.health = newNeb->staticinfo->health.maxhealth;
    newNeb->resource.resourcevalue = newNeb->staticinfo->resource.resourcevalue;
    newNeb->resource.resourceNotAccessible = 0;
    newNeb->resource.resourceVolume = NULL;

    newNeb->stub = stub;
    nebNewNeb(newNeb);

    vecZeroVector(newNeb->obj.posinfo.force);
    vecZeroVector(newNeb->rotatable.rotinfo.torque);

    univInitSpaceObjPosRot((SpaceObj*)newNeb, nebpos, TRUE);

    collAddSpaceObjToCollBlobs((SpaceObj *)newNeb);
    listAddNode(&universe.SpaceObjList, &(newNeb->header.objlink), newNeb);
    listAddNode(&universe.ResourceList, &(newNeb->resource.resourcelink), newNeb);
    listAddNode(&universe.ImpactableList, &(newNeb->impactable.impactablelink), newNeb);
    newNeb->obj.catEffectBlock = NULL;                      // CAT: univAddNebula 0x005416d0 clears +0x84, +0x80
    newNeb->obj.catMeshState = NULL;

    return newNeb;
}

/*-----------------------------------------------------------------------------
    Name        : univDeleteEffect
    Description : Deletes an effect
    Inputs      : effect - pointer to effect to delete
    Outputs     : frees the effect and it's effector list (actually a single memFree operation)
    Return      :
----------------------------------------------------------------------------*/
void univDeleteEffect(Effect *effect)
{
    univRemoveObjFromRenderList((SpaceObj *)effect);
    listDeleteNode(&effect->header.objlink);
}


/*-----------------------------------------------------------------------------
    Name        : univMeshBindingsDupe
    Description : Duplicate the local static bindings for a ship and fix up gun
                    pointers local to this ship instance.
    Inputs      : shipstaticinfo - staticinfo for this ship
                  newShip - newly allocated ship
    Outputs     : allocates new RAM and copies/fixes up the binding info
    Return      : address of newly allocated RAM or NULL
----------------------------------------------------------------------------*/
shipbindings *univMeshBindingsDupe(ShipStaticInfo *shipstaticinfo, Ship *newShip)
{
    shipbindings *newBindings;
    mhbinding *bindingListSource;
    mhlocalbinding *bindingListDest;
    sdword nLevels, index, nObjects, j;
    lodinfo *lodInfo = shipstaticinfo->common.LOD;

    if (shipstaticinfo->hierarchySize == 0)
    {                                                       //do nothing if there is no bindings to dupe
        return(NULL);
    }
    nLevels = lodInfo->nLevels;                             //record number of LOD's
    newBindings = memAlloc(shipBindingsLength(nLevels) +    //allocate bindings
                           shipstaticinfo->hierarchySize * sizeof(mhlocalbinding),
                           "ShipLocalBindings", NonVolatile);
#if MESH_PRE_CALLBACK
    newBindings->preCallback = NULL;
#endif
    newBindings->postCallback = NULL;                       //no callbacks needed for guns
    bindingListDest = (mhlocalbinding *)((ubyte *)newBindings +
                    shipBindingsLength(nLevels));           //start of first binding list
    for (index = 0; index < nLevels; index++)
    {                                                       //for each LOD
        if ((lodInfo->level[index].flags & LM_LODType) != LT_Mesh)
        {
            newBindings->localBinding[index] = NULL;
            continue;
        }
        newBindings->localBinding[index] = bindingListDest; //store list pointer for this LOD
        nObjects = ((meshdata *)lodInfo->level[index].pData)->nPolygonObjects;
        bindingListSource = lodInfo->level[index].hBindings;
//        memcpy(bindingList, lodInfo->level[index].hBindings,//copy the bindings
//                nObjects * sizeof(mhbinding));
        for (j = 0; j < nObjects; j++)
        {
            if (bindingListSource->function != NULL)
            {
                //!!! should do a switch statement based upon type, however,
                //there is only one type for now:guns
                dbgAssertOrIgnore(newShip->gunInfo != NULL);
                //using an index set up in setGunBindInfo(), set new gun pointer
                bindingListDest->function = bindingListSource->function;
                bindingListDest->userData = &newShip->gunInfo->guns[bindingListSource->userID];
                bindingListDest->userID = (smemsize)newShip;    //also save ship pointer
                bindingListDest->flags = bindingListSource->flags;
            }
            else
            {
                bindingListDest->function = NULL;         //this is all we need if there is no function
            }
            // CAT: every binding gets its object, with or without a function (univMeshBindingsDupe 0x005419d0;
            //      cat_meshObjectMatrixConcat 0x0053f900 reads the object of function-less bindings)
            bindingListDest->object = bindingListSource->object;
            bindingListDest++;
            bindingListSource++;
        }
//        bindingListDest += nObjects;                        //update location of list
    }
    newBindings->frequencyCounter = 0;                      //counter expires right now
    dbgAssertOrIgnore((udword)((ubyte *)bindingListDest - (ubyte *)newBindings) == shipBindingsLength(nLevels) + shipstaticinfo->hierarchySize * sizeof(mhlocalbinding));
#if MESH_VERBOSE_LEVEL >= 2
    dbgMessagef("univMeshBindingsDupe: localizing bindings 0x%x for ship 0x%x lengh %d",
                newBindings, newShip, shipBindingsLength(nLevels) + shipstaticinfo->hierarchySize * sizeof(mhlocalbinding));
#endif
    cat_univMeshBindingsLinkParents(shipstaticinfo, newBindings);  // CAT: univMeshBindingsDupe 0x005419d0
    return(newBindings);
}

/*-----------------------------------------------------------------------------
    Name        : cat_univMeshBindingsLinkParents (Cataclysm 0x00541ae0)
    Description : Points every local binding of every mesh LOD at its parent object's binding, from the parent
                  index the static binding list keeps (meshBindingListRecurse 0x004b47b0), or NULL for a top-level
                  object.
    Inputs      : shipstaticinfo - the ship's static info (its LOD and static bindings); bindings - the ship's
                  local bindings (univMeshBindingsDupe, cat_univMeshBindingsDupeNoGuns, madAnimBindingsDupe)
    Outputs     : sets mhlocalbinding.parent
    Return      : void
----------------------------------------------------------------------------*/
void cat_univMeshBindingsLinkParents(ShipStaticInfo *shipstaticinfo, shipbindings *bindings)
{
    lodinfo *lodInfo = shipstaticinfo->common.LOD;
    mhlocalbinding *local;
    mhbinding *source;
    sdword level, j, nObjects;

    for (level = 0; level < lodInfo->nLevels; level++)
    {
        if ((lodInfo->level[level].flags & LM_LODType) != LT_Mesh)
        {
            continue;
        }
        local = bindings->localBinding[level];
        source = lodInfo->level[level].hBindings;
        nObjects = ((meshdata *)lodInfo->level[level].pData)->nPolygonObjects;
        for (j = 0; j < nObjects; j++, local++, source++)
        {
            local->parent = (source->parentIndex == 0xff) ? NULL : &bindings->localBinding[level][source->parentIndex];
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_univMeshBindingsDupeNoGuns (Cataclysm 0x005418f0)
    Description : Like univMeshBindingsDupe, but with no binding functions: every local binding is zero except its
                  object (and its parent link). Callers in the exe: FixShip, cat_beastInfectShip, the salvage depot
                  and the Mimic fix-up (none of them ported yet).
    Inputs      : shipstaticinfo - static info whose LOD and static bindings are copied
    Outputs     : allocates the bindings
    Return      : the new bindings, or NULL if the ship has no hierarchy
----------------------------------------------------------------------------*/
shipbindings *cat_univMeshBindingsDupeNoGuns(ShipStaticInfo *shipstaticinfo)
{
    lodinfo *lodInfo = shipstaticinfo->common.LOD;
    shipbindings *newBindings;
    mhlocalbinding *dest;
    mhbinding *source;
    sdword nLevels, level, j, nObjects;
    udword length;

    if (shipstaticinfo->hierarchySize == 0)
    {
        return NULL;
    }
    nLevels = lodInfo->nLevels;
    length = shipBindingsLength(nLevels) + shipstaticinfo->hierarchySize * sizeof(mhlocalbinding);
    newBindings = memAlloc(length, "ShipLocalBindings", NonVolatile);
    memset(newBindings, 0, length);
    dest = (mhlocalbinding *)((ubyte *)newBindings + shipBindingsLength(nLevels));
    for (level = 0; level < nLevels; level++)
    {
        if ((lodInfo->level[level].flags & LM_LODType) != LT_Mesh)
        {
            newBindings->localBinding[level] = NULL;
            continue;
        }
        newBindings->localBinding[level] = dest;
        nObjects = ((meshdata *)lodInfo->level[level].pData)->nPolygonObjects;
        source = lodInfo->level[level].hBindings;
        for (j = 0; j < nObjects; j++, dest++, source++)
        {
            dest->object = source->object;
        }
    }
    newBindings->frequencyCounter = 0;
    cat_univMeshBindingsLinkParents(shipstaticinfo, newBindings);
    return newBindings;
}

/*-----------------------------------------------------------------------------
    CAT: the per-vertex mesh state (types in SpaceObj.h). In the exe only ships get one, and only when the
    low-memory switch 0x00c5236c is clear: utyGameSystemsInit sets it (and clears the dynamic-CPV option 0x00c52378)
    when the machine has less than 128 MB of physical memory. This tree never sets it (stage 3 owns the low-memory
    overrides of utyGameSystemsInit).
-----------------------------------------------------------------------------*/
bool32 catMeshStateDisabled = FALSE;

/*-----------------------------------------------------------------------------
    Name        : cat_univMeshStateAllocRecursive (Cataclysm 0x00541b80)
    Description : Gives a polygon object and, depth first, all its daughters a state slot each, numbered in the
                  same pre-order as the hierarchy render walk.
    Inputs      : slots - the level's slot array; index - this object's slot; object - the polygon object
    Outputs     : fills the slots and allocates their vertex states
    Return      : the next free slot index
----------------------------------------------------------------------------*/
sdword cat_univMeshStateAllocRecursive(CatMeshObjectState *slots, sdword index, polygonobject *object)
{
    CatMeshObjectState *slot = &slots[index];
    polygonobject *daughter;
    sdword i;

    slot->vertex = memAlloc(object->nVertices * sizeof(CatVertexState), "CatVertexState", NonVolatile);
    for (i = 0; i < object->nVertices; i++)
    {
        memset(&slot->vertex[i], 0, sizeof(CatVertexState));
        slot->vertex[i].dirty = 1;
        slot->vertex[i].lastUpdate = -1.0f;
        if (bitTest(object->pVertexInfo[i].flags, 0x04))
        {
            slot->vertex[i].value08 = 1.0f;
        }
    }
    slot->nVertices = object->nVertices;
    slot->objectWord0 = (sdword)(smemsize)object->pName;    // the exe copies the object's first dword
    index++;
    for (daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
    {
        index = cat_univMeshStateAllocRecursive(slots, index, daughter);
    }
    return index;
}

/*-----------------------------------------------------------------------------
    Name        : cat_univMeshStateFreeRecursive (Cataclysm 0x00541c70)
    Description : Frees the vertex states of a polygon object's daughters (depth first) and then its own.
    Inputs      : slots - the level's slot array; index - this object's slot; object - the polygon object
    Outputs     : frees and clears slot vertex arrays
    Return      : the next slot index
----------------------------------------------------------------------------*/
sdword cat_univMeshStateFreeRecursive(CatMeshObjectState *slots, sdword index, polygonobject *object)
{
    polygonobject *daughter;
    sdword next = index + 1;

    for (daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
    {
        next = cat_univMeshStateFreeRecursive(slots, next, daughter);
    }
    if (slots[index].vertex != NULL)
    {
        memFree(slots[index].vertex);
        slots[index].vertex = NULL;
    }
    return next;
}

/*-----------------------------------------------------------------------------
    Name        : cat_univShipMeshStateCreate (Cataclysm 0x00541cd0)
    Description : Creates an object's per-vertex mesh state: a table with one entry per LOD level and, for every
                  mesh level, one slot per polygon object with a state per vertex. Only ships get one; every other
                  object (and every object when catMeshStateDisabled is set) gets NULL. Called by the creators of
                  ships, asteroids, derelicts, effects, missiles and bullets.
    Inputs      : obj - the new object
    Outputs     : sets obj->catMeshState
    Return      : void
----------------------------------------------------------------------------*/
void cat_univShipMeshStateCreate(SpaceObj *obj)
{
    lodinfo *lodInfo;
    meshdata *mesh;
    CatMeshObjectState *slots;
    polygonobject *object;
    sdword level, index;

    if (catMeshStateDisabled || obj->header.objtype != OBJ_ShipType)
    {
        obj->catMeshState = NULL;
        return;
    }
    lodInfo = obj->staticinfo->LOD;
    obj->catMeshState = memAlloc(catMeshStateSize(lodInfo->nLevels), "CatMeshState", NonVolatile);
    obj->catMeshState->nLevels = lodInfo->nLevels;
    for (level = 0; level < lodInfo->nLevels; level++)
    {
        obj->catMeshState->level[level] = NULL;
        if ((lodInfo->level[level].flags & LM_LODType) == LT_Mesh)
        {
            mesh = (meshdata *)lodInfo->level[level].pData;
            slots = memAlloc(mesh->nPolygonObjects * sizeof(CatMeshObjectState), "CatMeshObjectState", NonVolatile);
            for (index = 0, object = &mesh->object[0]; object != NULL; object = object->pSister)
            {
                index = cat_univMeshStateAllocRecursive(slots, index, object);
            }
            obj->catMeshState->level[level] = slots;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_univMeshStateFree (Cataclysm 0x00541df0)
    Description : Frees an object's per-vertex mesh state, if it has one. Effects are skipped (they never have
                  one). Called by every free and delete path of the universe.
    Inputs      : obj - the object
    Outputs     : frees obj->catMeshState and clears it
    Return      : void
----------------------------------------------------------------------------*/
void cat_univMeshStateFree(SpaceObj *obj)
{
    lodinfo *lodInfo;
    CatMeshObjectState *slots;
    polygonobject *object, *daughter;
    sdword level, index, next;

    if (obj->catMeshState == NULL || obj->header.objtype == OBJ_EffectType)
    {
        return;
    }
    //the exe takes the level count from the LOD (object types 0..5 and 8); only ships ever get a state, so the
    //table's own count is the same number
    lodInfo = obj->staticinfo->LOD;
    for (level = 0; level < obj->catMeshState->nLevels; level++)
    {
        slots = obj->catMeshState->level[level];
        if (slots == NULL)
        {
            continue;
        }
        index = 0;
        for (object = &((meshdata *)lodInfo->level[level].pData)->object[0]; object != NULL; object = object->pSister)
        {
            next = index + 1;
            for (daughter = object->pDaughter; daughter != NULL; daughter = daughter->pSister)
            {
                next = cat_univMeshStateFreeRecursive(slots, next, daughter);
            }
            if (slots[index].vertex != NULL)
            {
                memFree(slots[index].vertex);
                slots[index].vertex = NULL;
            }
            index = next;
        }
        memFree(slots);
        obj->catMeshState->level[level] = NULL;
    }
    memFree(obj->catMeshState);
    obj->catMeshState = NULL;
}

/*-----------------------------------------------------------------------------
    Name        : cat_univFreeEffectBlock
    Description : Frees an object's one-byte block (SpaceObj.catEffectBlock, exe +0x84), as every free path of the
                  exe does inline (univFreeShipContents 0x0054b550, univupdateCloseAllObjectsAndMissionSpheres
                  0x0054b780, etgEffectDelete 0x0046a6f0 ...). A helper of this tree, not an exe function.
    Inputs      : obj - the object
    Outputs     : frees and clears the block
    Return      : void
----------------------------------------------------------------------------*/
void cat_univFreeEffectBlock(SpaceObj *obj)
{
    if (obj->catEffectBlock != NULL)
    {
        memFree(obj->catEffectBlock);
        obj->catEffectBlock = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipNozzleInfoGet (Cataclysm 0x0053f8c0)
    Description : The local binding of the mesh object an engine nozzle moves with, in the ship's current LOD.
    Inputs      : ship; index - ShipStaticInfo.engineNozzleObject[LOD][trail] (0xFF = none, read as object 0)
    Outputs     :
    Return      : the binding, or NULL if the ship has no local bindings
----------------------------------------------------------------------------*/
mhlocalbinding *cat_shipNozzleInfoGet(Ship *ship, sdword index)
{
    mhlocalbinding *list;

    if ((sbyte)index == -1)
    {
        index = 0;
    }
    if (ship->bindings == NULL)
    {
        return NULL;
    }
    list = ship->bindings->localBinding[ship->obj.currentLOD];
    return (list == NULL) ? NULL : &list[index];            // HWI: no pointer arithmetic on a NULL list
}

/*-----------------------------------------------------------------------------
    Name        : cat_meshObjectMatrixConcat (Cataclysm 0x0053f900)
    Description : The object-to-ship matrix of a binding: its parents' matrices (outermost first) times its own,
                  which is the binding's animated matrix if it has a function, else the object's local matrix.
    Inputs      : dest - where to put the matrix; binding - the local binding
    Outputs     : dest
    Return      : void
----------------------------------------------------------------------------*/
void cat_meshObjectMatrixConcat(hmatrix *dest, mhlocalbinding *binding)
{
    hmatrix *own = (binding->function == NULL) ? &binding->object->localMatrix : &binding->matrix;
    hmatrix temp;

    if (binding->parent != NULL)
    {
        cat_meshObjectMatrixConcat(dest, binding->parent);
        hmatMultiplyHMatByHMat(&temp, dest, own);
        *dest = temp;
        return;
    }
    *dest = *own;
}

/*-----------------------------------------------------------------------------
    Name        : cat_meshObjectMatrixGet (Cataclysm 0x0053f970)
    Description : cat_meshObjectMatrixConcat, with the identity for no binding.
    Inputs      : dest - where to put the matrix; binding - the local binding or NULL
    Outputs     : dest
    Return      : void
----------------------------------------------------------------------------*/
void cat_meshObjectMatrixGet(hmatrix *dest, mhlocalbinding *binding)
{
    if (binding == NULL)
    {
        *dest = IdentityHMatrix;                            // the exe copies its identity at 0x0072aed8
        return;
    }
    cat_meshObjectMatrixConcat(dest, binding);
}

/*-----------------------------------------------------------------------------
    Name        : univMeshBindingsDelete
    Description : Delete the mesh bindings for a ship about to be deleted.
    Inputs      : ship - ship being deleted
    Outputs     : frees the memory
    Return      :
----------------------------------------------------------------------------*/
void univMeshBindingsDelete(Ship *ship)
{
    if (ship->madBindings && ship->madBindings->nCurrentAnim != MAD_NoAnimation)
    {                                                       //if there's a mesh animation going on
        ship->bindings = ship->madBindings->saveBindings;   //restore old bindings
    }
    if (ship->bindings == NULL)
    {
        return;
    }
#if MESH_VERBOSE_LEVEL >= 2
    dbgMessagef("univMeshBindingsDelete: freeing bindings for ship 0x%x", ship);
#endif
    dbgAssertOrIgnore(ship->bindings != NULL);
    memFree(ship->bindings);
}

void InitializeEngineTrails(Ship *newship)
{
    ShipStaticInfo *shipstaticinfo = newship->staticinfo;
    char engine[5];
    sdword idx;
    sdword i;
    sdword numTrails;

    numTrails = 0;
    strcpy(engine, "EngX");
    for (idx = 0; idx < MAX_NUM_TRAILS; idx++)
    {
        engine[3] = (char) ('0' + (char)idx);
        if (mexGetChunk((void*)shipstaticinfo->common.pMexData, "Eng", engine) != 0)
        {
            numTrails = idx + 1;
        }
    }

    for (i = 0; i < MAX_NUM_TRAILS; i++)
    {
        newship->trail[i] = NULL;
    }
    if (shipstaticinfo->trailStatic[0] != NULL)
    {
        for (i = 0; i < numTrails; i++)
        {
            bool8 second = (bool8) ((i == 0) ? 0 : 1);
            newship->trail[i] = trailNew(shipstaticinfo->trailStatic[i], (void*)newship, second, (ubyte)i);
        }
    }
    else
    {
        //capital ship engine glow
        for (i = 0; i < numTrails; i++)
        {
            // CAT: Beast and Taiidan ships get glow style 2 (InitializeEngineTrails 0x00541f40: race 1 or 3; HW1 R2)
            if (newship->shiprace == RACE_Beast || newship->shiprace == RACE_Taiidan)
            {
                newship->trail[i] = trailNew(shipstaticinfo->trailStatic[i], (void*)newship, 2, (ubyte)i);
            }
            else
            {
                newship->trail[i] = trailNew(shipstaticinfo->trailStatic[i], (void*)newship, 3, (ubyte)i);
            }
        }
    }
}

void univResetNewGimbleGun(Gun *gun)
{
    GunStatic *gunstatic = gun->gunstatic;
    dbgAssertOrIgnore(gunstatic->guntype == GUN_NewGimble);

    gun->angle = 0.0f;
    gun->declination = 0.0f;
    gun->anglespeed = 0.0f;
    gun->declinationspeed = 0.0f;
    gun->curgunorientation = gunstatic->defaultgunorientation;
    if (bitTest(gunstatic->flags, GF_MultiLevelMatrix))
    {
        gun->curGunOrientationNonConcat = gunstatic->defaultGunOrientationNonConcat;
    }
    matGetVectFromMatrixCol3(gun->gunheading,gun->curgunorientation);
}

void InitializeNavLights(Ship *newship)
{
    ShipStaticInfo *shipstaticinfo = newship->staticinfo;

    if (shipstaticinfo->navlightStaticInfo != NULL)
    {
        NAVLightStaticInfo *navlightstaticinfo = shipstaticinfo->navlightStaticInfo;
        sdword numNAVLights = navlightstaticinfo->numNAVLights;
        NAVLightStatic *curnavlightstatic;
        NAVLight *curnavlight;
        sdword i;

        dbgAssertOrIgnore(numNAVLights > 0);

        newship->shipOrDerelict.navLightInfo = memAlloc(sizeofNavLightInfo(numNAVLights),"navlightInfo",NonVolatile);
        newship->shipOrDerelict.navLightInfo->numLights = numNAVLights;

        for (i=0,curnavlight=&newship->shipOrDerelict.navLightInfo->navLights[0],curnavlightstatic=&navlightstaticinfo->navlightstatics[0];i<numNAVLights;i++,curnavlight++,curnavlightstatic++)
        {
            curnavlight->navlightstatic = curnavlightstatic;
            curnavlight->lightstate = 0;
            curnavlight->lastTimeFlashed = curnavlightstatic->startdelay;
        }
    }
    else
    {
        newship->shipOrDerelict.navLightInfo = NULL;
    }
}

void InitializeShowingDamage(Ship* ship)
{
    sdword i, j;

    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            ship->showingDamage[i][j] = NULL;
        }
    }

    ship->nDamageLights = 0;
    ship->damageLights = NULL;

    dmgGetLights(ship);
}

/*-----------------------------------------------------------------------------
    Name        : univCreateShip
    Description : creates a ship, but does not add it to the universe
    Inputs      : shiptype, shiprace, shippos, playerowner,
                  built - flag indicating if ship was built or placed in world
    Outputs     :
    Return      : address of ship created
----------------------------------------------------------------------------*/
// CAT: univCreateShip 0x00542320 takes 7 arguments and adds the ship to the universe itself unless a flag says not to;
//      this tree keeps HW1's split (univCreateShip creates, univAddShip adds and makes the mothership modules). Of the
//      exe's work here, done since M2-R4: the 0x63C-byte Ship with the gun (0x88 per gun), mad and precise-selection
//      blocks behind it, the damage points (cat_dmgShipLoadDamagePoints 0x00462950 = dmgGetLights, in
//      InitializeShowingDamage), upgrades (cat_shipUpgradesInit/ApplyUpgrades), dock info (cat_dockInitShipDockInfo),
//      the mothership modules (cat_univCreateMothershipModules), experience, stealth, dissolve, ship names and the
//      multiplayer derelicts' helplessness. Not yet, with the owner [integrate: M2-R5; was an M2-R4 #3/#4/#5 TODO]:
//      CAT-TODO(M2-R6 ships): the repair-droid array (droidCreateArray 0x005c3070 -> Ship+0x4C0, NULL here);
//      CAT-TODO(M2-R6 ConsMgr): cat_suShipAdded 0x00459a90 (the support units), the external-build start at health 0
//      behind CAT_EXTERNAL_BUILD_DRIVER (below) and the factory block for the four builder types only;
//      CAT-TODO(M2-R6 FOW): the FOW cube (fowActivateCubeW with cat_shipGetFOWRadius once the ship is added).
Ship *univCreateShip(ShipType shiptype,ShipRace shiprace,vector *shippos,struct Player *playerowner,sdword built)
{
    Ship *newship;
    udword sizespec;
    udword sizeship = sizeof(Ship), totalsize = 0, madAssedSize = 0;
    ShipStaticInfo *shipstaticinfo = GetShipStaticInfo(shiptype,shiprace);
    sdword i;
    ubyte *extraPointer;

    dbgAssertOrIgnore(playerowner != NULL);

#ifdef HW_BUILD_FOR_DEBUGGING
    if (!bitTest(shipstaticinfo->common.infoFlags, IF_InfoLoaded))
    {                                                       //if this ships was not loaded properly
        dbgFatalf(DBG_Loc,"\n%s\\%s was not loaded properly - stubbing out.", ShipRaceToStr(shiprace), ShipTypeToStr(shiptype));
    }
#endif
    if ((sizespec = shipstaticinfo->custshipheader.sizeofShipSpecifics) != 0)
    {
        sizeship += sizespec - sizeof(udword);
    }
    // CAT-FIX (M2-R5 layout): the precise-selection and mesh-animation blocks behind the ship need 8-byte alignment on
    //      LP64 (madanim holds pointers; memFree reads its cookie there). With the exe's 0x63C-byte Ship plus a
    //      specifics block the sum is not always a multiple of 8, so the extra blocks start at the next multiple
    sizeship = (sizeship + 7) & ~(udword)7;
    totalsize = sizeship;
    if (shipstaticinfo->common.staticCollInfo.preciseSelection != 0xff)
    {                                                       //account for the local precise selection
        totalsize += sizeof(PreciseSelection);
    }
    if (shipstaticinfo->madStatic != NULL)
    {                                                       //account for the local animation bindings
        madAssedSize = madAnimSize(shipstaticinfo->common.LOD->nLevels) + //size of the binding info
                     shipstaticinfo->hierarchySize * sizeof(mhlocalbinding) + //size of the actual binding structures
                     shipstaticinfo->madStatic->header->nObjects * sizeof(splinecurve) * 6; //size of spline curves, enough for all objects

        totalsize += madAssedSize;
    }
    newship = memAlloc(totalsize,"Ship",NonVolatile);
    memset(newship,0,sizeship);                             // set everything to 0 by default
    extraPointer = (ubyte *)newship + sizeship;             //point to end of ship where extra RAM was allocated
    // CAT (M2-R4 combat): right after zeroing the ship the exe sets the salvage-target fields (univCreateShip
    //      0x00542320 at 0x005423c2..0x005423f6): +0x618 = 0x6258d727 (1e21, "never"), +0x615 = -1, +0x238 = -1;
    //      and the dock point index +0x4A0 = -1 further on (0x00542899)
    newship->catSalvageNotifyTime = CAT_SALVAGE_NEVER;
    newship->catSalvagePointIndex = -1;
    newship->catSalvagePlayer = -1;
    newship->dockvars.catDockPointIndex = -1;
    // CAT (M2-R5 layout): the other non-zero initial words of univCreateShip: +0x60C = -1 (catKilledByPlayer,
    //      0x0054240b), +0x5B0 = 1 (0x00542863; no reader found), +0x3DC and +0x3E0 = 1.0 (damageModifier and the
    //      KAS damage factor, 0x005426d4), +0x1B8 = the static top speed (0x005425f3)
    newship->catKilledByPlayer = -1;
    newship->catShipReserved5B0 = 1;
    newship->catDamageFactor = 1.0f;
    newship->guidance.nonstatvars.catMaxVelocity = shipstaticinfo->common.maxvelocity;
    newship->catNavLightStatic = shipstaticinfo->navlightStaticInfo;    // +0x634 (0x005423fc)
    newship->shipDeCloakTime = -20.0f;                  //initialize to a negative number so if a battle occurs at time 0.0f or anytime near by, then it will not screw up.
    newship->header.objtype = OBJ_ShipType;
    newship->header.flags = SOF_Rotatable + SOF_Impactable + SOF_Selectable + SOF_Targetable;
    // CAT: no Drone case (HW1's drones are not in Cataclysm; univCreateShip 0x00542320 sets 0x102003 for every type)
    if (shipstaticinfo->common.staticCollInfo.collspheresize > VERYBIGSHIP_SIZE)
    {
        newship->header.flags |= SOF_BigObject;
    }
    if (shipstaticinfo->common.staticCollInfo.preciseSelection != 0xff)
    {
//        newship->impactable.collInfo.precise = (PreciseSelection *)(((ubyte *)newship) + sizeship);
        newship->impactable.collInfo.precise = (PreciseSelection *)extraPointer;
        extraPointer += sizeof(PreciseSelection);
    }
    else
    {
        newship->impactable.collInfo.precise = NULL;
    }
    newship->staticinfo = shipstaticinfo;
    bitClear(newship->shipOrDerelict.specialFlags,SPECIAL_ParadeNeedTomoveCloser);
//    ClearNode(newship->obj.renderlink);

    //need to copy over static info to non static structures so we
    //can beging fiddling with them...

    for (i=0;i<NUM_TRANS_DEGOFFREEDOM;i++)
    {
        newship->guidance.nonstatvars.thruststrength[i] = shipstaticinfo->guidance.thruststrengthstat[i];
    }
    for(i=0;i< NUM_ROT_DEGOFFREEDOM;i++)
    {
        newship->guidance.nonstatvars.rotstrength[i] = shipstaticinfo->guidance.rotstrengthstat[i];
    }
    for(i=0;i< NUM_TURN_TYPES;i++)
    {
        newship->guidance.nonstatvars.turnspeed[i] = shipstaticinfo->guidance.turnspeedstat[i];
    }

    newship->shipID.shipNumber = universe.shipNumber;
    IDToPtrTableAdd(&ShipIDToPtr,universe.shipNumber,&newship->obj);
    universe.shipNumber++;

    newship->shiptype = shiptype;
    newship->shiprace = shiprace;
    newship->tacticstype = Neutral;         //default to neutral tactical setting
    newship->tactics_ordertype = NO_ORDER_NO_ENEMY;  //default to no bonuses...
    newship->tacticsTalk = 0;
    newship->tacticsNeedToFlightMan = FALSE;
    newship->dockwaitforNorm=FALSE;
    newship->shipOrDerelict.putOnDoor=FALSE;
    newship->tacticsFormationVar1 = FALSE;
    newship->DodgeTime = 0.0f;
    newship->isDodging = FALSE;
    newship->timeCreated = universe.totaltimeelapsed;
//    newship->obj.currentLOD = 0;                                //start at highest LOD by default
//    newship->obj.collMyBlob = NULL;
    newship->obj.cameraDistanceSquared = REALlyBig;
    newship->playerowner = playerowner;
    newship->shipOrDerelict.dockingship = NULL;
    newship->shipOrDerelict.specialFlags = 0;
    newship->shipOrDerelict.specialFlags = 0;
    newship->kamikazeState = 0;
    newship->visibleToWho = 0;      //initialize cloaked visible flag to invisible to everyone!
    newship->visibleToWhoPreviousFrame = 0;
    newship->damageModifier = 1.0f;
    newship->forceCancelDock=FALSE;
    newship->hyperspacePing=FALSE;
    newship->gravwellTimeEffect=0.0f;
    newship->passiveAttackCancelTimer=0.0f;
    // Alex's Repair Droid stuff...
//    newship->pRepairDroids = memAlloc(sizeof(RepairDroid), "RDroid", NonVolatile);

    univInitSpaceObjPosRot(&newship->obj,shippos,FALSE);

    newship->tractorbeaminfo = 0;
    newship->tractorbeam_playerowner = NULL;
    newship->tractorPosinfo[0] = FALSE;
    newship->tractorPosinfo[1] = FALSE;
    newship->tractorPosinfo[2] = FALSE;
    newship->tractorPosinfo[3] = FALSE;
    newship->tractorstate = 0;            //info variable for tractorbeaming state
    newship->needNewLeader = FALSE;

    for(i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
    {
        newship->shipOrDerelict.salvageNumTagged[i] = 0;
    }

    newship->guidance.clampInfo = NULL;

    newship->dontrotateever = FALSE;
    newship->dontapplyforceever = FALSE;
    newship->targetable.health = shipstaticinfo->health.maxhealth;
    newship->fuel = shipstaticinfo->maxfuel;
//    newship->resources = 0;
//    newship->shipOrDerelict.aistate = 0;
//    newship->aistateattack = 0;
//    newship->hotKeyGroup = 0;

    newship->shipSinglePlayerGameInfo = spNewShipSinglePlayerGameInfo();

    InitializeEngineTrails(newship);

    InitializeShowingDamage(newship);

    if (shipstaticinfo->gunStaticInfo != NULL)
    {
        sdword numGuns = shipstaticinfo->gunStaticInfo->numGuns;
        Gun *gun;
        GunStatic *gunstatic;

        newship->gunInfo = memAlloc(sizeofGunInfo(numGuns),"gunInfo",NonVolatile);
        newship->gunInfo->numGuns = numGuns;

        for (i=0;i<numGuns;i++)
        {
            gun = &newship->gunInfo->guns[i];
            gunstatic = &shipstaticinfo->gunStaticInfo->gunstatics[i];
            gun->gunstatic = gunstatic;

            switch (gunstatic->guntype)
            {
                case GUN_MissileLauncher:
                    gun->numMissiles = gunstatic->maxMissiles;
                    // deliberately fall through
                case GUN_MineLauncher:
                    gun->numMissiles = gunstatic->maxMissiles;      //Use same var for mines
                case GUN_Gimble:
                case GUN_Fixed:
                    gun->gunheading = gunstatic->gunnormal;
                    break;

                case GUN_NewGimble:
                    univResetNewGimbleGun(gun);
                    break;
            }
            gun->lasttimefired = 0.0f;
            gun->burstStartTime = 0.0f;
            gun->burstState = 0;
            gun->gimblehandle = SOUND_NOTINITED;
        }
    }
    else
    {
        newship->gunInfo = NULL;
    }

    // CAT (M2-R4 combat): the upgrade state (univCreateShip 0x00542320 at 0x00542a2b): the type's starting armour and
    //      each gun's first weapon, then the player's research, except for an NIS ship (bit 2 of built)
    cat_shipUpgradesInit(newship);
    if (!(built & 2))
    {
        cat_shipApplyUpgrades(newship);
    }

    // CAT (M2-R4 combat): no HW1 per-object salvage points (setSalvageInfo is gone): Cataclysm's salvage points are
    //      the DPT_Salvage/DPT_SalvageLatch dock points of the target, held in dockInfo (Salvage.c)

    cat_dockInitShipDockInfo(newship);                      // CAT (M2-R5 dock): 0x00463bb0, as univCreateShip calls it (0x005428b4)

    InitializeNavLights(newship);

    // CAT (M2-R4 ships-b): also for the types with catTypeFlags 0x8000 (sHiveFrigate, bHiveFrigate, dRKO:
    //      cat_univInitShipTypeFlags 0x0053b8c0), which keep ships inside without a dock (univCreateShip 0x005428d7)
    if ((shipstaticinfo->canBuildShips | shipstaticinfo->canReceiveShipsPermanently) ||
        (shipstaticinfo->catTypeFlags & 0x8000))
    {
        newship->shipsInsideMe = memAlloc(sizeof(ShipsInsideMe),"ShipsInsideMe",NonVolatile);
        listInit(&newship->shipsInsideMe->insideList);
        newship->shipsInsideMe->FightersInsideme = 0;
        newship->shipsInsideMe->CorvettesInsideme = 0;
//        listInit(&newship->shipsInsideMe->launchList);
    }
    else
    {
        newship->shipsInsideMe = NULL;
    }

    if (shipstaticinfo->canBuildShips)
    {
        cmAddFactory(newship,shipstaticinfo->canBuildBigShips);
    }

    /* init the sound variables for this ship */
    soundEventInitStruct(&newship->soundevent);

    // CAT (M2-R5 dock): the type's CustShipInit runs after the mesh-animation bindings are set up, as univCreateShip
    //      0x00542320 orders it (bindings dupe 0x005428cc, mad bindings 0x00542917 and the initial mad state, then the
    //      init slot at 0x00542a75): ModuleShipInit starts an animation on an sMothershipCommand, which needs
    //      madBindings. HW1 called the init first (moved below)

    //lightning effect stuff
    newship->lightning[0] = NULL;
    newship->lightning[1] = NULL;

    //create the instance version of the hierarchy bindings
    newship->bindings = univMeshBindingsDupe(shipstaticinfo, newship);

    if (shipstaticinfo->madStatic != NULL)
    {
        newship->madBindings = (madanim *)extraPointer;
        extraPointer += sizeof(madanim);
        newship->madBindings->header = shipstaticinfo->madStatic->header;
        newship->madBindings->nCurrentAnim = -1;
        newship->madBindings->time = 0.0f;
        newship->madBindings->totalSize = madAssedSize;
        madAnimBindingsDupe(newship, shipstaticinfo, FALSE);

        //ship has mesh animations associated with it
        //so lets set its inital state.
        newship->madAnimationFlags = 0;
        newship->madGunStatus = 0;            //gun status //invalid states!
        newship->madWingStatus = 0;           //wing animation status
        newship->madSpecialStatus = 0;
        newship->madDoorStatus = MAD_STATUS_DOOR_CLOSED;
        newship->cuedAnimationIndex = 0;
        newship->cuedAnimationType = 0;

        if(built == 1)
        {
            madLinkInSetUpInitialBuiltMadState(newship);
        }
        else if(built == 0)
        {
            madLinkInSetUpInitialPlacedMadState(newship);
        }
        else if(built == 2)
        {
            //ignore since it wasn't built or placed (nis ship)
        }
        else
        {
            dbgFatalf(DBG_Loc,"\nUnknown create ship state.");
        }

    }

    if (((ShipStaticInfo *)newship->staticinfo)->custshipheader.CustShipInit)
    {
        ((ShipStaticInfo *)newship->staticinfo)->custshipheader.CustShipInit(newship);
    }

    //determine what color scheme to use
    if ((playerowner->playerIndex < MAX_MULTIPLAYER_PLAYERS) && (shipstaticinfo->shipOrDerelict.teamColor[playerowner->playerIndex]))
    {                                                       //if this player's default scheme woudl be a good color
        newship->colorScheme = playerowner->playerIndex;
    }
    else
    {                                                       //else let's find any old color scheme
        for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++)
        {
            if (shipstaticinfo->shipOrDerelict.teamColor[i])
            {
                newship->colorScheme = i;
                break;
            }
        }
    }
    // CAT: after the colour scheme, univCreateShip 0x00542320 clears +0x84 and creates the ship's per-vertex mesh
    //      state (0x00542b0a: cat_univShipMeshStateCreate 0x00541cd0)
    newship->obj.catEffectBlock = NULL;
    cat_univShipMeshStateCreate(&newship->obj);
    // CAT: then the external build state (0x00542ac4..0x00542b10): an externalBuild type created with bit 0 of built
    //      set starts at health 0, progress 0 and SPECIAL_2_CatBeingBuilt; every other ship at full health, progress
    //      1.0, flag clear. CAT-PROVISIONAL: CAT_EXTERNAL_BUILD_DRIVER (CommandLayer.h) keeps the first case off
    //      until ConsMgr's external-build driver is ported
    if (CAT_EXTERNAL_BUILD_DRIVER && shipstaticinfo->externalBuild && (built & 1))
    {
        newship->targetable.health = 0.0f;
        newship->catBuildProgress = 0.0f;
        bitSet(newship->shipOrDerelict.specialFlags2,SPECIAL_2_CatBeingBuilt);
    }
    else
    {
        newship->targetable.health = shipstaticinfo->health.maxhealth;
        newship->catBuildProgress = 1.0f;
        bitClear(newship->shipOrDerelict.specialFlags2,SPECIAL_2_CatBeingBuilt);
    }
    // CAT (M2-R5 layout-dock): the exe zeroes the kills per class (Ship+0x5C8) and calls cat_shipUpdateExperienceLevel
    //      0x005147d0 once per class (0x00542bf0; the level comes out 0 and no message is posted); one call here
    for (i = 0; i < NUM_CLASSES; i++)
    {
        newship->catKillsByClass[i] = 0;
    }
    cat_shipUpdateExperienceLevel(newship);
    // CAT (M2-R4 ships-b): the stealth flags (cat_shipStealthInit 0x0053fce0, call at 0x00542c15)
    cat_shipStealthInit(newship);
    // CAT (M2-R4 ships-b): after the stealth state the exe resets the dissolve state (0x00542c1b:
    //      cat_shipDissolveInit 0x00460220), and in multiplayer a derelict-race ship that is no crystal and is
    //      targetable is a KAS derelict, helpless for good (0x00542c00..0x00542c5a: catFlags18 bit 4,
    //      cat_shipSetHelpless(ship, 1e21)). CAT-TODO(stage 3/5 recorder): the exe also takes this path while a game
    //      is recorded (0x00a7519c && 0x00a75190)
    cat_shipDissolveInit(newship);
    // CAT (M2-R5 hud): a local named ship (carrier, mothership) gets the first free name of its type
    //      (cat_shipNamesAssign 0x0050f320, call at 0x00542c21)
    cat_shipNamesAssign(newship);
    if (multiPlayerGame && (shiptype >= DERELICT_FIRST_SHIP) && (shiptype <= DERELICT_LAST_SHIP) &&
        !cat_shipIsCrystal(newship) && !cat_shipIsUntargetable(newship))
    {
        bitSet(newship->header.catFlags18, CAT_SHIPFLAG_KasDerelict);
        cat_shipSetHelpless(newship, 1e21f);
    }
    return newship;
}

/*-----------------------------------------------------------------------------
    Name        : univAddShip
    Description : adds a ship to the universe
    Inputs      : shiptype, shiprace, shippos, playerowner
    Outputs     :
    Return      : address of ship added
----------------------------------------------------------------------------*/
Ship *univAddShip(ShipType shiptype,ShipRace shiprace,vector *shippos,struct Player *playerowner,sdword built)
{
    Ship *newship = univCreateShip(shiptype,shiprace,shippos,playerowner,built);

    collAddSpaceObjToCollBlobs(&newship->obj);
    listAddNode(&universe.SpaceObjList,&(newship->header.objlink),newship);
    listAddNode(&universe.ShipList,&(newship->shiplink),newship);
    listAddNode(&universe.ImpactableList,&(newship->impactable.impactablelink),newship);

    // CAT: no CryoTray case (an HW1-only type)

    // CAT (M2-R4 ships-b): a Sect or Beast mothership comes with its module ships. The exe's univCreateShip 0x00542320
    //      makes them right after adding the ship (0x00542bba: unless the type is sMothership with the 7th argument
    //      set outside mission 1); this tree's univCreateShip neither adds ships nor has that argument, so the
    //      modules are made here, for every mothership added to the universe (a ship only created, e.g. by the
    //      fight statistics, gets none)
    cat_univCreateMothershipModules(newship, shiprace, shippos, playerowner, built);

    return newship;
}

/*-----------------------------------------------------------------------------
    Name        : cat_univCreateMothershipModules
    Description : CAT (M2-R4 ships-b): the module ships of a new sMothership or bMothership
                  (cat_univCreateMothershipModules 0x005420b0): each type of cat_univMothershipModuleTypesGet /
                  cat_univBeastMothershipModuleTypesGet at the host's position and velocity, clamped to it as a ship
                  part; in multiplayer an sMothership also gets seven more modules that count against the unit caps
                  and are not ship parts (0x008b5c28)
    Inputs      : host, race, pos, player, built - the host's creation arguments
----------------------------------------------------------------------------*/
void cat_univCreateMothershipModules(Ship *host, ShipRace race, vector *pos, struct Player *player, sdword built)
{
    static const ShipType multiplayerModules[] = { sMothershipSpecial, sMothershipEngineering, sMothershipArmour,
        sMothershipMicro, sMothershipWeapons, sMothershipDockingBay, sMothershipBigGun };
    CatModuleTypeList modules;
    Ship *module;
    sdword i;
    bool32 gameOptionNoModules = FALSE;

    if (host->shiptype == sMothership)
    {
        cat_univMothershipModuleTypesGet(&modules);
    }
    else if (host->shiptype == bMothership)
    {
        cat_univBeastMothershipModuleTypesGet(&modules);
    }
    else
    {
        return;
    }
    for (i = 0; i < modules.num; i++)
    {
        module = univAddShip(modules.types[i], race, pos, player, built);
        module->catCreator = host->catCreator;      // the exe passes its own 6th argument on
        if (race == RACE_Sect)
        {
            // CAT: a module built with the mothership opens its lab quietly (cat_rmResearchModuleActivated 0x004f4530)
            cat_rmResearchModuleActivated(player, module->shiptype, FALSE);
        }
        module->obj.posinfo.position = host->obj.posinfo.position;
        module->obj.posinfo.velocity = host->obj.posinfo.velocity;
        clampObjToObj((SpaceObjRotImpTargGuidance *)module, (SpaceObjRotImpTargGuidance *)host);
        bitSet(module->header.flags, SOF_CatShipPart);
    }
    // CAT-TODO(stage 5 multiplayer options): gameOptionNoModules is bit 0x8 of the multiplayer game options word 0x008860ec (the
    //      Cataclysm tpGameCreated layout); until that layout is ported the option counts as off
    if ((!singlePlayerGame) && (!gameOptionNoModules) && (host->shiptype == sMothership))
    {
        for (i = 0; i < (sdword)(sizeof(multiplayerModules) / sizeof(multiplayerModules[0])); i++)
        {
            module = univAddShip(multiplayerModules[i], race, pos, player, built);
            module->catCreator = host->catCreator;
            unitCapCreateShip(module, module->playerowner);
            module->obj.posinfo.position = host->obj.posinfo.position;
            module->obj.posinfo.velocity = host->obj.posinfo.velocity;
            clampObjToObj((SpaceObjRotImpTargGuidance *)module, (SpaceObjRotImpTargGuidance *)host);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : univUpdateAllPosVelMinorObjs
    Description : Exclusive physics updating for what would seem to
                  be Asteroid ZERO's in the single player game?
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/

void univUpdateAllPosVelMinorObjs()
{
    Node *objnode;
    SpaceObj *spaceobj;

    if ((singlePlayerGame) && (singlePlayerGameInfo.asteroid0sCanMove))
    {
        objnode = universe.MinorSpaceObjList.head;

        while (objnode != NULL)
        {
            spaceobj = (SpaceObj *)listGetStructOfNode(objnode);
            physUpdateObjPosVelBasic(spaceobj,universe.phystimeelapsed);
            objnode = objnode->next;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : univUpdateAllPosVelBullets
    Description : Exclusive physics updating for bulletes
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R4 combat): the exe's bullets carry the firing gun's weapon word (bullet +0x108 = Gun +0x00, WEAPON_*:
//      cat_gunCreateBullet 0x0048bb00 at 0x0048bb90; cat_shipApplyUpgrades 0x00514490 raises it from PROJECTILE to
//      ENERGY), in the field HW1 calls bulletType (SpaceObj.h)
#define CAT_BULLET_WEAPON_TYPE(bullet)      ((bullet)->bulletType)

/*-----------------------------------------------------------------------------
    Name        : cat_shipGetAimPoint (Cataclysm 0x00415af0)
    Description : The point a homing bullet steers at: the target's collision position. For a trFloatingCity the
                  exe computes a point on the ship's axis (local (0,0,-755) in world space) into a local of its own
                  and leaves the result alone, so the collision position stands for every target.
    Inputs      : target - the bullet's target; aim - the collision position, from the caller
    Outputs     : aim (unchanged)
    Return      : void
----------------------------------------------------------------------------*/
static void cat_shipGetAimPoint(SpaceObjRotImpTarg *target, vector *aim)
{
    vector local, unused;

    if (target->header.objtype == OBJ_ShipType && ((Ship *)target)->shiptype == trFloatingCity)
    {
        local.x = 0.0f;
        local.y = 0.0f;
        local.z = -755.0f;                                  // 0x0074da20
        matMultiplyMatByVec(&unused, &target->rotatable.rotinfo.coordsys, &local);
        vecAddTo(unused, target->obj.posinfo.position);    // cat_vecShipLocalToWorld 0x004601d0, result dropped
        return;
    }
    *aim = target->impactable.collInfo.collPosition;
}

/*-----------------------------------------------------------------------------
    Name        : cat_bulletHomingUpdate (Cataclysm 0x00542cb0)
    Description : Steers a homing bullet older than half a second at its target (not at swarmers): its velocity
                  keeps its speed and turns towards the target (towards the target's lead position inside 200
                  units); inside 100 units it moves with the target plus its own speed (halved above 150) along the
                  first direction. The heading follows the velocity.
    Inputs      : bullet
    Outputs     : bullet velocity and heading
    Return      : void
----------------------------------------------------------------------------*/
void cat_bulletHomingUpdate(Bullet *bullet)
{
    SpaceObjRotImpTarg *target = bullet->target;
    vector aim, d, dir, first, lead;
    real32 dist2, speed, speed2, t;

    if (target == NULL || !(bullet->timelived > 0.5f))              // 0x008b5c54
    {
        return;
    }
    if (target->header.objtype == OBJ_ShipType &&
        (((Ship *)target)->shiptype == sSwarmer || ((Ship *)target)->shiptype == bSwarmer))
    {
        return;
    }
    aim = target->impactable.collInfo.collPosition;
    cat_shipGetAimPoint(target, &aim);
    vecSub(d, aim, bullet->obj.posinfo.position);
    dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
    dir = d;
    vecNormalize(&dir);
    first = dir;
    if (!(dist2 < 40000.0f) &&                                      // 0x008b5c58
        (d.z * bullet->obj.posinfo.velocity.z + d.y * bullet->obj.posinfo.velocity.y +
         d.x * bullet->obj.posinfo.velocity.x) < -0.866025f)        // 0x008b5c50
    {
        return;
    }
    speed2 = bullet->obj.posinfo.velocity.x * bullet->obj.posinfo.velocity.x +
             bullet->obj.posinfo.velocity.y * bullet->obj.posinfo.velocity.y +
             bullet->obj.posinfo.velocity.z * bullet->obj.posinfo.velocity.z;
    if (speed2 < 2560000.0f + 10000.0f && speed2 > 2560000.0f - 10000.0f)   // 0x008b5c44, 0x008b5c48
    {
        speed = 1600.0f;                                            // 0x008b5c4c: the usual bullet speed, no sqrt
    }
    else
    {
        speed = (real32)sqrt((double)speed2);
    }
    if (dist2 < 40000.0f && speed > 0.0f)
    {                                                               // lead the target by the flight time
        t = (real32)(sqrt((double)dist2) / speed);
        lead.x = t * target->obj.posinfo.velocity.x + d.x;
        lead.y = t * target->obj.posinfo.velocity.y + d.y;
        lead.z = t * target->obj.posinfo.velocity.z + d.z;
        dir = lead;
        vecNormalize(&dir);
    }
    bullet->obj.posinfo.velocity.x = dir.x * speed;
    bullet->bulletheading = dir;
    bullet->obj.posinfo.velocity.y = dir.y * speed;
    bullet->obj.posinfo.velocity.z = dir.z * speed;
    if (dist2 < 10000.0f)                                           // 0x008b5c5c
    {
        if (!(target->header.objtype == OBJ_ShipType &&
              (((Ship *)target)->shiptype == sSwarmer || ((Ship *)target)->shiptype == bSwarmer)))
        {
            if (speed > 150.0f)                                     // 0x008b5c60
            {
                speed /= 2.0f;                                      // 0x008b5c64
            }
            bullet->obj.posinfo.velocity.x = target->obj.posinfo.velocity.x + first.x * speed;
            bullet->obj.posinfo.velocity.y = target->obj.posinfo.velocity.y + first.y * speed;
            bullet->obj.posinfo.velocity.z = target->obj.posinfo.velocity.z + first.z * speed;
        }
        dir = bullet->obj.posinfo.velocity;
        vecNormalize(&dir);
        bullet->bulletheading = dir;
    }
}

void univUpdateAllPosVelBullets()
{
    Node* bulletnode = universe.BulletList.head;
    Bullet *bullet;
    bool32 addEffect = FALSE;
    etglod *etgLOD;
    sdword LOD;
    etgeffectstatic *stat;
    Effect *effect;

    while (bulletnode != NULL)
    {
        bullet = (Bullet *)listGetStructOfNode(bulletnode);

        if ((bullet->header.flags & SOF_DontApplyPhysics) == 0)
        {
            // CAT: homing bullets steer before they move (univUpdateAllPosVelBullets 0x00543030 calls
            //      cat_bulletHomingUpdate 0x00542cb0 when the bullet's weapon word, bullet +0x108 = the firing gun's
            //      Gun +0x00, is WEAPON_ENERGY)
            if (CAT_BULLET_WEAPON_TYPE(bullet) == WEAPON_ENERGY)
            {
                cat_bulletHomingUpdate(bullet);
            }
            if (physUpdateBulletPosVel(bullet,universe.phystimeelapsed))
            {
                //bullet has died so clean it up

                //turn off sound of beam
                if (bullet->bulletType == BULLET_Beam)
                {
                    if ((bullet->owner != NULL) && (bullet->gunowner != NULL))
                    {
                        soundEventBurstStop(bullet->owner, bullet->gunowner);
                    }
                }

                //delete effect associated with bullet
                effect = bullet->effect;
#if ETG_DISABLEABLE
                if (effect != NULL && etgEffectsEnabled)
#else
                if (effect != NULL)
#endif
                {
                    if (bitTest(effect->staticinfo->specialOps, ESO_SelfDeleting))
                    {                               //if the effect can delete itself
                        ((real32 *)effect->variable)[ETG_BulletDurationParam] =
                            effect->timeElapsed;    //time it out
                        effect->owner = NULL;
                        bitClear(effect->header.flags, SOF_AttachPosition);
                        bitClear(effect->header.flags, SOF_AttachCoordsys);
                        bitClear(effect->header.flags, SOF_AttachVelocity);
                    }
                    else
                    {
                        etgEffectDelete(effect);
                        univRemoveObjFromRenderList((SpaceObj *)(effect));
                        listDeleteNode(&effect->header.objlink);
                        addEffect = TRUE;
                    }

                    /**
                     * That's all fun and games until the bullet is a defense fighter laser. Those actually have *two*
                     * effects (the beam and the flash) and we don't have any back-pointer to the flash effect. We could
                     * go through the render list to find which effect still has `bullet` as its owner, but since we'll
                     * go through that list anyway when rendering, I'm just adding a check there and leaving the effect
                     * to be freed automatically.
                     *
                     * Gitlab #30 "Segfault on defense fighter laser" - Thibault Lemaire - 2024-04-10 - 2 of 3
                     */
                }
                bulletnode = bulletnode->next;

                // CAT: the burst explosion is keyed on the bullet's burst flag (bullet +0x10c, set by
                //      cat_gunCreateBullet 0x0048bb00 when the firing ship has specialFlags 0x40, burst firing),
                //      not on HW1's BULLET_SpecialBurst type; its damage is scaled by cat_weaponDamageVsArmour(the
                //      bullet's sound type, armour 0) 0x00514460 (0x0054319d) [M2-R4 combat]
                if (bullet->catBurst)
                {
                    etgLOD = etgSpecialPurposeEffectTable[EGT_BURST_EXPLOSION_EFFECT];

                    if (etgLOD != NULL)
                    {
                        LOD = bullet->obj.currentLOD;

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

                    if (addEffect)
                    {
#if ETG_DISABLEABLE
                        if (stat != NULL && etgEffectsEnabled && !etgFrequencyExceeded(stat))
#else
                        if (stat != NULL && !etgFrequencyExceeded(stat))
#endif
                        {
                            vector velocity;
                            matrix coordsys = bullet->rotatable.rotinfo.coordsys;
                            velocity.x = 0.0f;
                            velocity.y = 0.0f;
                            velocity.z = 0.0f;

                            etgEffectCreate(stat, NULL, &bullet->obj.posinfo.position, &velocity, &coordsys, 1.0f, EAF_AllButNLips, 0);
                        }
                    }

                    pointExplosionInSpace(&bullet->obj.posinfo.position,burstRadius,
                                          cat_weaponDamageVsArmour(bullet->soundType, 0) * bullet->damage,
                                          bullet->playerowner->playerIndex);
                }

                // CAT: a Sect big-gun shell explodes where it dies and loses its ping (0x005431d6: sound type
                //      GS_BigGun -> cat_bigGunBulletExplodeAt 0x005bd0c0, pingRemovePingByOwner) [M2-R4 ships-b]
                if (bullet->soundType == GS_BigGun)
                {
                    cat_bigGunBulletExplodeAt(bullet, &bullet->obj.posinfo.position);
                    pingRemovePingByOwner((SpaceObj *)bullet);
                }

                // CAT: every ship drops its references to the bullet (CustShipHeader slot 14, 0x0054322c), its mesh
                //      state and +0x84 block are freed (0x00543253); HW1 told only the DefenseFighters
                univFreeBulletContents(bullet);

                bobObjectDied(((SpaceObj *)bullet),&universe.collBlobList);
                listRemoveNode(&bullet->bulletlink);
                univRemoveObjFromRenderList(((SpaceObj *)bullet));
                listDeleteNode(&bullet->header.objlink);
                bullet->owner = NULL;

                continue;
            }
        }
        bulletnode = bulletnode->next;
    }
}
/*-----------------------------------------------------------------------------
    Name        : univUpdateAllPosVelMissiles
    Description : Exclusive physics updating for misiles
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univUpdateAllPosVelMissiles()
{
    Node *misnode = universe.MissileList.head;
    Missile *missile;
    bool32 deleteflag;

    while (misnode != NULL)
    {
        missile = (Missile *)listGetStructOfNode(misnode);

        if ((missile->header.flags & SOF_DontApplyPhysics) == 0)
        {
            if(missile->missileType == MISSILE_Regular)
            {   //if missileType is a normal missile
                deleteflag = aishipGuideMissile(missile);
            }
            else if(missile->missileType == MISSILE_Mine)
            {   //if missiletype is a mine
                deleteflag = aishipGuideMine(missile);
            }
            else
            {
                deleteflag = FALSE;
                dbgAssertOrIgnore(FALSE);
            }

            physUpdateObjPosVelMissile(missile,universe.phystimeelapsed);
            if ((deleteflag) && ((missile->header.flags & SOF_Dead) == 0))
            {
                AddMissileToDeleteMissileList(missile, -1);
            }
        }
        misnode = misnode->next;
    }
}
/*-----------------------------------------------------------------------------
    Name        : univUpdateAllPosVelDerelicts
    Description : Exclusive physics updating for derelicts
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univUpdateAllPosVelDerelicts()
{
    Node *objnode = universe.DerelictList.head;
    Derelict *derelict;

    while (objnode != NULL)
    {
        derelict = (Derelict *)listGetStructOfNode(objnode);

        if ((derelict->header.flags & SOF_DontApplyPhysics) == 0)
        {
            if ((derelict->header.flags & SOF_NISShip) == 0)
            {
                physUpdateObjPosVelDerelicts(derelict,universe.phystimeelapsed);
            }
        }
        objnode = objnode->next;
    }
}
/*-----------------------------------------------------------------------------
    Name        : univUpdateAllPosVelResources
    Description : Exclusive physics updating for resources
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univUpdateAllPosVelResources()
{
    Node *objnode = universe.ResourceList.head;
    Resource *resource;

    while (objnode != NULL)
    {
        resource = (Resource *)listGetStructOfNode(objnode);

        if ((resource->header.flags & (SOF_DontApplyPhysics|SOF_NISShip)) == 0)
        {
            physUpdateObjPosVelBasic(&resource->obj,universe.phystimeelapsed);
            // CAT-TODO(stage 6 level load / KAS): univUpdateAllPosVelResources 0x005432d0 then clamps a resource whose
            //      attribute dword (exe +0x38) has bit 0x200 into the box +-(exe +0x1A8, +0x1AC, +0x1B0): three of the
            //      reserved dwords of this tree's Asteroid (SpaceObj.h), which nothing writes yet. The box is level
            //      data: neither univAddAsteroid 0x00540a30 nor scriptSetResourcesCB 0x004a07b0 stores it, so its
            //      writer is a mission-script path; port the clamp with that writer and name the three fields then
            //      [integrate: M2-R5; was an M2-R4 #3 asteroids, #5 level-load attributes TODO]
        }

        if (resource->resourceNotAccessible > 0)
        {
            resource->resourceNotAccessible--;
        }

        objnode = objnode->next;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_univLimitProtectedShipSpeed
    Description : every ship that an escort command protects gets that command's group speed as its speed limit
                  (Ship.catProtectedSpeedLimit, the lowest when several protect it)
----------------------------------------------------------------------------*/
// CAT: new (cat_univLimitProtectedShipSpeed 0x00544e00): not for a docking ship, mission 8's tHeavyInterceptors and
//      mission 9's tResourceCollectors, or a salvager within 750 of its target; a limit below 0 is first raised to
//      1e21; GetShipsTravelVel's speed of the escorts caps it
void cat_univLimitProtectedShipSpeed(void)
{
    Node *node;
    CommandToDo *command;
    real32 travelvel;
    sdword i;
    Ship *ship;
    CatSalvagerInfo *info;

    for (node = universe.mainCommandLayer.todolist.head; node != NULL; node = node->next)
    {
        command = (CommandToDo *)listGetStructOfNode(node);
        if (!(command->ordertype.attributes & COMMAND_MASK_PROTECTING))
        {
            continue;
        }
        travelvel = GetShipsTravelVel(command->selection,1.0f);
        for (i = 0; i < command->protect->numShips; i++)
        {
            ship = command->protect->ShipPtr[i];
            if (ship->dockvars.dockship != NULL)
            {
                continue;
            }
            if (singlePlayerGame &&
                (((singlePlayerGameInfo.currentMission == 8) && (ship->shiptype == tHeavyInterceptor)) ||
                 ((singlePlayerGameInfo.currentMission == 9) && (ship->shiptype == tResourceCollector))))
            {
                continue;
            }
            if (cat_shipIsSalvager(ship))
            {
                info = cat_salvageGetSalvagerInfo(ship);
                if ((info->target != NULL) &&
                    (cat_vecDistance(&ship->obj.posinfo.position,&info->target->obj.posinfo.position) < 750.0f))   // 0x0072bd70
                {
                    continue;
                }
            }
            if (ship->catProtectedSpeedLimit < 0.0f)
            {
                ship->catProtectedSpeedLimit = 1e21f;
            }
            if (travelvel < ship->catProtectedSpeedLimit)
            {
                ship->catProtectedSpeedLimit = travelvel;
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : univUpdateAllPosVelShips
    Description : Exclusive physics updating for ships
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univUpdateAllPosVelShips()
{
    Node *objnode = universe.ShipList.head;
    Ship *ship;
    sdword i;

    growSelectReset(&universe.HousekeepShipList);
    catDeferredShipListReset();                             // CAT: univUpdateAllPosVelShips 0x00544f40
    if(ClampedShipList.selection == NULL)
    {
        growSelectInit(&ClampedShipList);
    }
    growSelectReset(&ClampedShipList);

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);

        // CAT: no SOF_NISShip test and no slave handling (0x00544f9a: only SOF_DontApplyPhysics; dockUpdateSlaves
        //      is not in the exe, no Cataclysm ship is slaveable)
        if ((ship->header.flags & SOF_DontApplyPhysics) == 0)
        {
            univCheckShipState(ship);

            // CAT (M2-R4 combat): the exe's clamp scheme (0x00544fa6): a ship clamped to a ship that is not itself
            //      clamped puts that host on the list (once, from the host's first clamped object) and skips its own
            //      physics; the host then moves its clamped objects (updateClampedObject on the host, below). Every
            //      other ship, clamped or not, runs physUpdateObjPosVelShip.
            if ((ship->guidance.clampInfo != NULL) && (ship->guidance.clampInfo->host != NULL) &&
                (ship->guidance.clampInfo->host->header.objtype == OBJ_ShipType) &&
                (((Ship *)ship->guidance.clampInfo->host)->guidance.clampInfo->host == NULL) &&
                (((Ship *)ship->guidance.clampInfo->host)->guidance.clampInfo->clampedObjs->ShipPtr[0] == ship))
            {
                growSelectAddShip(&ClampedShipList,(Ship *)ship->guidance.clampInfo->host);
            }
            else
            {
                physUpdateObjPosVelShip(ship,universe.phystimeelapsed);
            }
            univSetupShipForControl(ship);
        }
        else
        {
            // CAT: a ship that does not move still gets its housekeep and deferred update (0x0054500a..0x00545082)
            univMinorSetupShipForControl(ship);
            // CAT (M2-R4 combat): a clamped one is carried along without physics (0x00545047)
            if ((ship->guidance.clampInfo != NULL) && (ship->guidance.clampInfo->host != NULL))
            {
                cat_updateClampedObjectNoPhysics((SpaceObjRotImpTargGuidance *)ship);
            }
            if (ship->staticinfo->custshipheader.CustShipHousekeep != NULL)
            {
                growSelectAddShip(&universe.HousekeepShipList,ship);
            }
            growSelectAddShip(&universe.catSelection1A0,ship);
        }
        //update mesh animations
        // CAT: ships whose .shp numbers their animations (madstatic.animsByNumber, the ANIM keys) skip the HW1
        //      mad link-in processing, and the link-in runs only while an animation is playing (0x00545089)
        if (ship->madBindings != NULL)
        {
            if ((ship->staticinfo->madStatic->animsByNumber == 0) && (ship->madAnimationFlags & MAD_ANIMATION_NEED_PROC))
            {
                if (ship->madBindings->nCurrentAnim != -1)
                {
                    madLinkInUpdateMeshAnimations(ship);
                }
            }
            else if (ship->madBindings->nCurrentAnim != -1)
            {
                madAnimationUpdate(ship, universe.phystimeelapsed);
            }
        }

        objnode = objnode->next;
    }

    {
        Ship *obj;
        SelectCommand *clampedShipSelection;
        sdword numShips;

        //update those poor baby clamped ships!
        clampedShipSelection = ClampedShipList.selection;
        numShips = clampedShipSelection->numShips;

        for (i=0;i<numShips;i++)
        {
            obj = clampedShipSelection->ShipPtr[i];
            // CAT (M2-R4 combat): the list holds hosts (0x005450f4): a host still holding objects and clamped to
            //      nothing moves them
            if (obj)    //???????
            {
                if ((obj->guidance.clampInfo != NULL) && (obj->guidance.clampInfo->host == NULL))
                {
                    //do this check again because it is possible for a ship to become unclamped at some point down the road...
                    updateClampedObject((SpaceObjRotImpTargGuidance *)obj);
                }
            }
        }
    }

    // CAT: the deferred updates run before the housekeeps (0x00545147)
    {
        SelectCommand *deferredSelection = universe.catSelection1A0.selection;

        for (i = 0; i < deferredSelection->numShips; i++)
        {
            if (deferredSelection->ShipPtr[i] != NULL)
            {
                cat_univShipDeferredUpdate(deferredSelection->ShipPtr[i]);
            }
        }
    }

    {
        Ship *obj;
        SelectCommand *housekeepSelection;
        sdword numShips;

        housekeepSelection = universe.HousekeepShipList.selection;
        numShips = housekeepSelection->numShips;

        for (i=0;i<numShips;i++)
        {
            obj = housekeepSelection->ShipPtr[i];
            if (obj)
            {
                obj->staticinfo->custshipheader.CustShipHousekeep(obj);
            }
        }
    }

    // CAT: every 5 seconds (0x0072bd5c) the protected-speed limits are recomputed (0x005451a1)
    if (catProtectedSpeedLimitTime < universe.totaltimeelapsed)
    {
        cat_univLimitProtectedShipSpeed();
        catProtectedSpeedLimitTime = 5.0f + universe.totaltimeelapsed;
    }

    growSelectReset(&universe.HousekeepShipList);       // don't need to keep track of them anymore
    growSelectReset(&universe.catSelection1A0);
    growSelectReset(&ClampedShipList);       // don't need to keep track of them anymore
}

/*-----------------------------------------------------------------------------
    Name        : univPausedUpdateAllPosVelShips
    Description : A minor version of the above function that only does update
                    operations appropriate for when the universe is paused
                    by nisUniversePaused.
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void univPausedUpdateAllPosVelShips(void)
{
    Node *objnode = universe.ShipList.head;
    Ship *ship;

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        univMinorSetupShipForControl(ship);
        objnode = objnode->next;
    }
}

/*-----------------------------------------------------------------------------
    Name        : univUpdateAllPosVelEffects
    Description : Exclusive physics updating for Effects -
                  must update effects AFTER anything that can be an
                  effect owner (be on safe side and do it last)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univUpdateAllPosVelEffects()
{
    Node *objnode = universe.effectList.head;
    Node *deletenode;
    Effect *effect;

    while (objnode != NULL)
    {
        effect = (Effect *)listGetStructOfNode(objnode);

        if ((effect->header.flags & SOF_DontApplyPhysics) == 0)
        {
            if (etgEffectUpdate(effect, universe.phystimeelapsed))
            {
                deletenode = objnode;
                objnode = objnode->next;

                etgEffectDelete(effect);
                univRemoveObjFromRenderList((SpaceObj *)effect);
                listDeleteNode(&effect->header.objlink);
                continue;
            }
        }
        objnode = objnode->next;
    }

}

/*-----------------------------------------------------------------------------
    Name        : MakeNewAsteroidStaticInfo
    Description : makes a new asteroid staticinfo based on its scaling
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeNewAsteroidStaticInfo(Asteroid *asteroid)
{
    AsteroidStaticInfo *originalAsteroidStaticInfo = &asteroidStaticInfos[asteroid->asteroidtype];
    AsteroidStaticInfo *newAsteroidStaticInfo;
    StaticInfo *newStaticHeader;
    StaticInfo *originalStaticHeader;
    StaticCollInfo *newStaticCollInfo;
    StaticCollInfo *originalStaticCollInfo;
    real32 scaling = asteroid->scaling;

    dbgAssertOrIgnore(scaling > 0.0f);

    if ((asteroid->header.flags & SOF_StaticInfoIsDynamic) == 0)
    {
        asteroid->staticinfo = memAlloc(sizeof(AsteroidStaticInfo),"aststatic",NonVolatile);
        *(asteroid->staticinfo) = asteroidStaticInfos[asteroid->asteroidtype];  // structure copy
        asteroid->header.flags |= SOF_StaticInfoIsDynamic;
    }

    newAsteroidStaticInfo = asteroid->staticinfo;

    newAsteroidStaticInfo->resource.harvestableByMultipleShips = (udword)(((real32)originalAsteroidStaticInfo->resource.harvestableByMultipleShips) * scaling);
    if (newAsteroidStaticInfo->resource.harvestableByMultipleShips == 0)
    {
        newAsteroidStaticInfo->resource.harvestableByMultipleShips = 1;
    }

    newStaticHeader = &newAsteroidStaticInfo->common;
    originalStaticHeader = &originalAsteroidStaticInfo->common;

    newStaticHeader->mass = originalStaticHeader->mass * scaling;
    newStaticHeader->momentOfInertiaX = originalStaticHeader->momentOfInertiaX * scaling;
    newStaticHeader->momentOfInertiaY = originalStaticHeader->momentOfInertiaY * scaling;
    newStaticHeader->momentOfInertiaZ = originalStaticHeader->momentOfInertiaZ * scaling;

    newStaticHeader->oneOverMass =             1.0f /      newStaticHeader->mass;
    newStaticHeader->oneOverMomentOfInertiaX = 1.0f /      newStaticHeader->momentOfInertiaX;
    newStaticHeader->oneOverMomentOfInertiaY = 1.0f /      newStaticHeader->momentOfInertiaY;
    newStaticHeader->oneOverMomentOfInertiaZ = 1.0f /      newStaticHeader->momentOfInertiaZ;

    newStaticCollInfo = &newStaticHeader->staticCollInfo;
    originalStaticCollInfo = &originalStaticHeader->staticCollInfo;

    newStaticCollInfo->originalCollSphereSize = originalStaticCollInfo->originalCollSphereSize;  // DO NOT CHANGE INTENTIONALLY
    newStaticCollInfo->approxcollspheresize = originalStaticCollInfo->approxcollspheresize * scaling;
    newStaticCollInfo->avoidcollspheresize = originalStaticCollInfo->avoidcollspheresize * scaling;
    newStaticCollInfo->avoidcollspherepad = originalStaticCollInfo->avoidcollspherepad * scaling;
    newStaticCollInfo->collspheresize = originalStaticCollInfo->collspheresize * scaling;
    newStaticCollInfo->collspheresizeSqr = newStaticCollInfo->collspheresize * newStaticCollInfo->collspheresize;

    vecScalarMultiply(newStaticCollInfo->collsphereoffset,originalStaticCollInfo->collsphereoffset,scaling);
    vecScalarMultiply(newStaticCollInfo->collrectoffset,originalStaticCollInfo->collrectoffset,scaling);

    newStaticCollInfo->forwardlength = originalStaticCollInfo->forwardlength * scaling;
    newStaticCollInfo->rightlength = originalStaticCollInfo->rightlength * scaling;
    newStaticCollInfo->uplength = originalStaticCollInfo->uplength * scaling;

    newStaticCollInfo->diagonallength = originalStaticCollInfo->diagonallength * scaling;

    collUpdateCollRectangle((SpaceObjRotImp *)asteroid);
}

/*-----------------------------------------------------------------------------
    Name        : MakeNewDustCloudStaticInfo
    Description : makes a new dustcloud staticinfo based on its scaling
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeNewDustCloudStaticInfo(DustCloud *dustcloud)
{
    DustCloudStaticInfo *originalDustCloudStaticInfo = &dustcloudStaticInfos[dustcloud->dustcloudtype];
    DustCloudStaticInfo *newDustCloudStaticInfo;
    StaticInfo *newStaticHeader;
    StaticInfo *originalStaticHeader;
    StaticCollInfo *newStaticCollInfo;
    StaticCollInfo *originalStaticCollInfo;
    real32 scaling = dustcloud->scaling;

    dbgAssertOrIgnore(scaling > 0.0f);

    if ((dustcloud->header.flags & SOF_StaticInfoIsDynamic) == 0)
    {
        dustcloud->staticinfo = memAlloc(sizeof(DustCloudStaticInfo),"dcloudstatic",NonVolatile);
        *((DustCloudStaticInfo *)(dustcloud->staticinfo)) = dustcloudStaticInfos[dustcloud->dustcloudtype];  // structure copy
        dustcloud->header.flags |= SOF_StaticInfoIsDynamic;
    }

    newDustCloudStaticInfo = (DustCloudStaticInfo *)dustcloud->staticinfo;

    newDustCloudStaticInfo->resource.harvestableByMultipleShips = (udword)(((real32)originalDustCloudStaticInfo->resource.harvestableByMultipleShips) * scaling);
    if (newDustCloudStaticInfo->resource.harvestableByMultipleShips == 0)
    {
        newDustCloudStaticInfo->resource.harvestableByMultipleShips = 1;
    }

    newStaticHeader = &newDustCloudStaticInfo->common;
    originalStaticHeader = &originalDustCloudStaticInfo->common;

    newStaticHeader->mass = originalStaticHeader->mass * scaling;
    newStaticHeader->momentOfInertiaX = originalStaticHeader->momentOfInertiaX * scaling;
    newStaticHeader->momentOfInertiaY = originalStaticHeader->momentOfInertiaY * scaling;
    newStaticHeader->momentOfInertiaZ = originalStaticHeader->momentOfInertiaZ * scaling;

    newStaticHeader->oneOverMass =             1.0f /      newStaticHeader->mass;
    newStaticHeader->oneOverMomentOfInertiaX = 1.0f /      newStaticHeader->momentOfInertiaX;
    newStaticHeader->oneOverMomentOfInertiaY = 1.0f /      newStaticHeader->momentOfInertiaY;
    newStaticHeader->oneOverMomentOfInertiaZ = 1.0f /      newStaticHeader->momentOfInertiaZ;

    newStaticCollInfo = &newStaticHeader->staticCollInfo;
    originalStaticCollInfo = &originalStaticHeader->staticCollInfo;

    newStaticCollInfo->originalCollSphereSize = originalStaticCollInfo->originalCollSphereSize;  // DO NOT CHANGE INTENTIONALLY
    newStaticCollInfo->approxcollspheresize = originalStaticCollInfo->approxcollspheresize * scaling;
    newStaticCollInfo->avoidcollspheresize = originalStaticCollInfo->avoidcollspheresize * scaling;
    newStaticCollInfo->avoidcollspherepad = originalStaticCollInfo->avoidcollspherepad * scaling;
    newStaticCollInfo->collspheresize = originalStaticCollInfo->collspheresize * scaling;
    newStaticCollInfo->collspheresizeSqr = newStaticCollInfo->collspheresize * newStaticCollInfo->collspheresize;

    vecScalarMultiply(newStaticCollInfo->collsphereoffset,originalStaticCollInfo->collsphereoffset,scaling);
    vecScalarMultiply(newStaticCollInfo->collrectoffset,originalStaticCollInfo->collrectoffset,scaling);

    newStaticCollInfo->forwardlength = originalStaticCollInfo->forwardlength * scaling;
    newStaticCollInfo->rightlength = originalStaticCollInfo->rightlength * scaling;
    newStaticCollInfo->uplength = originalStaticCollInfo->uplength * scaling;

    newStaticCollInfo->diagonallength = originalStaticCollInfo->diagonallength * scaling;

    collUpdateCollRectangle((SpaceObjRotImp *)dustcloud);
}

/*-----------------------------------------------------------------------------
    Name        : MakeNewGasCloudStaticInfo
    Description : makes a new gascloud staticinfo based on its scaling
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void MakeNewGasCloudStaticInfo(GasCloud *gascloud)
{
    GasCloudStaticInfo *originalGasCloudStaticInfo = &gascloudStaticInfos[gascloud->gascloudtype];
    GasCloudStaticInfo *newGasCloudStaticInfo;
    StaticInfo *newStaticHeader;
    StaticInfo *originalStaticHeader;
    StaticCollInfo *newStaticCollInfo;
    StaticCollInfo *originalStaticCollInfo;
    real32 scaling = gascloud->scaling;

    dbgAssertOrIgnore(scaling > 0.0f);

    if ((gascloud->header.flags & SOF_StaticInfoIsDynamic) == 0)
    {
        gascloud->staticinfo = memAlloc(sizeof(GasCloudStaticInfo),"gcloudstatic",NonVolatile);
        *((GasCloudStaticInfo *)(gascloud->staticinfo)) = gascloudStaticInfos[gascloud->gascloudtype];  // structure copy
        gascloud->header.flags |= SOF_StaticInfoIsDynamic;
    }

    newGasCloudStaticInfo = (GasCloudStaticInfo *)gascloud->staticinfo;

    newGasCloudStaticInfo->resource.harvestableByMultipleShips = (udword)(((real32)originalGasCloudStaticInfo->resource.harvestableByMultipleShips) * scaling);
    if (newGasCloudStaticInfo->resource.harvestableByMultipleShips == 0)
    {
        newGasCloudStaticInfo->resource.harvestableByMultipleShips = 1;
    }

    newStaticHeader = &newGasCloudStaticInfo->common;
    originalStaticHeader = &originalGasCloudStaticInfo->common;

    newStaticHeader->mass = originalStaticHeader->mass * scaling;
    newStaticHeader->momentOfInertiaX = originalStaticHeader->momentOfInertiaX * scaling;
    newStaticHeader->momentOfInertiaY = originalStaticHeader->momentOfInertiaY * scaling;
    newStaticHeader->momentOfInertiaZ = originalStaticHeader->momentOfInertiaZ * scaling;

    newStaticHeader->oneOverMass =             1.0f /      newStaticHeader->mass;
    newStaticHeader->oneOverMomentOfInertiaX = 1.0f /      newStaticHeader->momentOfInertiaX;
    newStaticHeader->oneOverMomentOfInertiaY = 1.0f /      newStaticHeader->momentOfInertiaY;
    newStaticHeader->oneOverMomentOfInertiaZ = 1.0f /      newStaticHeader->momentOfInertiaZ;

    newStaticCollInfo = &newStaticHeader->staticCollInfo;
    originalStaticCollInfo = &originalStaticHeader->staticCollInfo;

    newStaticCollInfo->originalCollSphereSize = originalStaticCollInfo->originalCollSphereSize;  // DO NOT CHANGE INTENTIONALLY
    newStaticCollInfo->approxcollspheresize = originalStaticCollInfo->approxcollspheresize * scaling;
    newStaticCollInfo->avoidcollspheresize = originalStaticCollInfo->avoidcollspheresize * scaling;
    newStaticCollInfo->avoidcollspherepad = originalStaticCollInfo->avoidcollspherepad * scaling;
    newStaticCollInfo->collspheresize = originalStaticCollInfo->collspheresize * scaling;
    newStaticCollInfo->collspheresizeSqr = newStaticCollInfo->collspheresize * newStaticCollInfo->collspheresize;

    vecScalarMultiply(newStaticCollInfo->collsphereoffset,originalStaticCollInfo->collsphereoffset,scaling);
    vecScalarMultiply(newStaticCollInfo->collrectoffset,originalStaticCollInfo->collrectoffset,scaling);

    newStaticCollInfo->forwardlength = originalStaticCollInfo->forwardlength * scaling;
    newStaticCollInfo->rightlength = originalStaticCollInfo->rightlength * scaling;
    newStaticCollInfo->uplength = originalStaticCollInfo->uplength * scaling;

    newStaticCollInfo->diagonallength = originalStaticCollInfo->diagonallength * scaling;

    collUpdateCollRectangle((SpaceObjRotImp *)gascloud);
}

/*-----------------------------------------------------------------------------
    Name        : ApplyDamageToCollidingObjects
    Description : Applies collision damage to two colliding objects
    Inputs      : obj1, obj2, distvector, dist
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
#define HYPERING_IN_DAMAGE    twHyperingInDamage
#define HYPERED_INTO_DAMAGE   twHyperedIntoDamage

// CAT: the attribute bit of the exe's killer collisions, CAT_ATTRIBUTES_KillerCollision 0x200 (Attributes.h; moved there
//      by M2-R4 combat for collCheckShipResourceColl)

void ApplyDamageToCollidingObjects(SpaceObjRotImpTarg *obj1,SpaceObjRotImpTarg *obj2,vector *distvector,real32 dist)
{
    real32 damage;
    SpaceObjRotImpTarg *target;
    etglod *etgLOD;
    etgeffectstatic *stat;
    vector heading, position;
    matrix coordsys;

    // CAT: Cataclysm's collisions do no damage, except a killer collision: when exactly one of the two objects has the
    //      killer attribute, both take the smaller of their healths (cause 9 / 10 and the other object as attacker in
    //      the 7-argument ApplyDamageToTarget, M2-R4 combat) and the hit effect of Sect very-large plasma bombs
    //      (etgGunEventTable[0][GS_VeryLargePlasmaBomb][EGT_GunHit], 0x00c412c4) plays on the killer object
    //      (ApplyDamageToCollidingObjects 0x00545540). The distance arguments are no longer used.
    (void)distvector;
    (void)dist;
    if (((obj1->obj.attributes ^ obj2->obj.attributes) & CAT_ATTRIBUTES_KillerCollision) == 0)
    {
        // the exe looks up the ships' commands here and ignores the result (0x00545562)
        return;
    }
    damage = min(obj1->health, obj2->health);
    // CAT: each side is the other's attacker (ApplyDamageToCollidingObjects 0x00545500: call sites 9 and 10)
    ApplyDamageToTarget(obj1,damage,0,DEATH_Killed_By_Collision,99,(Ship *)obj2,9);
    ApplyDamageToTarget(obj2,damage,0,DEATH_Killed_By_Collision,99,(Ship *)obj1,10);

    target = (obj1->obj.attributes & CAT_ATTRIBUTES_KillerCollision) ? obj1 : obj2;
    etgLOD = etgGunEventTable[0][GS_VeryLargePlasmaBomb][EGT_GunHit];
    if ((etgLOD == NULL) || (target->obj.currentLOD >= etgLOD->nLevels))
    {
        stat = NULL;
    }
    else
    {
        stat = etgLOD->level[target->obj.currentLOD];
    }
    if (univSpaceObjInRenderList(&target->obj) && (stat != NULL) && !etgFrequencyExceeded(stat))
    {
        vecCopyAndNegate(heading, target->obj.posinfo.velocity);
        vecNormalize(&heading);
        matCreateCoordSysFromHeading(&coordsys, &heading);
        vecAdd(position, target->obj.posinfo.position, target->obj.posinfo.velocity);
        etgEffectCreate(stat, NULL, &position, &target->obj.posinfo.velocity, &coordsys, 0.0f, 0, 2,
                        SCALECAST(Real32ToUdword(damage)), SCALECAST(1));
    }
}

bool32 ObjectsHaveCollidedLately(SpaceObjRotImpTarg *obj1,SpaceObjRotImpTarg *obj2)
{
    if ( ((universe.totaltimeelapsed - obj1->lasttimecollided) < COLLISION_CLEAR_TIME) &&
         ((universe.totaltimeelapsed - obj2->lasttimecollided) < COLLISION_CLEAR_TIME) )
    {
        return TRUE;
    }
    obj1->lasttimecollided =  universe.totaltimeelapsed;
    obj2->lasttimecollided =  universe.totaltimeelapsed;
    return FALSE;
}

bool32 ShouldDoGlancingCollision(SpaceObjRotImpTarg *fastobj,real32 fastobjvelmag2,SpaceObjRotImpTarg *slowobj,real32 slowobjvelmag2)
{
    // don't do glancing collision for kamikaze stuff
    if(fastobj->header.objtype == OBJ_ShipType)
        if(bitTest(((Ship *)fastobj)->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            return FALSE;

    if(slowobj->header.objtype == OBJ_ShipType)
        if(bitTest(((Ship *)slowobj)->shipOrDerelict.specialFlags,SPECIAL_Kamikaze))
            return FALSE;

    if ((fastobjvelmag2/slowobjvelmag2) >= GLANCE_COLLISION_VEL_RATIO)
    {
        return TRUE;
    }

    if (vecDotProduct(fastobj->obj.posinfo.velocity,slowobj->obj.posinfo.velocity) < 0)
    {
        return TRUE;
    }

    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : ObjectsCollided
    Description : Handles momentum transferral of objects colliding
    Inputs      : obj1, obj2, distvector, dist, distsquared
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ObjectsCollided(SpaceObjRotImpTarg *obj1,SpaceObjRotImpTarg *obj2,real32 colldist,vector *distvector,real32 dist,real32 distsquared)
{
    vector collforce;
    vector cforce1;
    vector cforce2;
    vector repulseoverlap;
    real32 temp;
    real32 repulse = (colldist - dist) * COLLISION_REPULSE_FACTOR / dist;
    vector up = { 0.0f, 0.0f, 1.0f };
    real32 collforcemag;
    vector tangforce;

    real32 obj1velmag2;
    real32 obj2velmag2;

    real32 slowobjvelmag2;
    real32 fastobjvelmag2;

    SpaceObjRotImpTarg *slowobj;
    SpaceObjRotImpTarg *fastobj;

    vector fastesttoslowest;

    vector leftrightdecidevec;

#if DEBUG_COLLISIONS
    dbgMessagef("%x and %x collided, Repulsion %f",(udword)obj1,(udword)obj2,repulse);
#endif

    if (!ObjectsHaveCollidedLately(obj1,obj2))
    {
        // decide which ship is fastest ship in collision

        obj1velmag2 = vecMagnitudeSquared(obj1->obj.posinfo.velocity);
        obj2velmag2 = vecMagnitudeSquared(obj2->obj.posinfo.velocity);

        if (obj1velmag2 > obj2velmag2)
        {
            fastobj = obj1; fastobjvelmag2 = obj1velmag2;
            slowobj = obj2; slowobjvelmag2 = obj2velmag2;
        }
        else
        {
            fastobj = obj2; fastobjvelmag2 = obj2velmag2;
            slowobj = obj1; slowobjvelmag2 = obj1velmag2;
        }

        // collforce = (m2v2 - m1v1)/t
        vecScalarMultiply(cforce1,obj2->obj.posinfo.velocity,obj2->staticinfo->common.mass);
        vecScalarMultiply(cforce2,obj1->obj.posinfo.velocity,obj1->staticinfo->common.mass);
        vecSub(collforce,cforce1,cforce2);
        vecDivideByScalar(collforce,universe.phystimeelapsed,temp);

        if (ShouldDoGlancingCollision(fastobj,fastobjvelmag2,slowobj,slowobjvelmag2))
        {
#if DEBUG_COLLISIONS
        dbgMessage("Glancing Collision");
#endif
            if ((collforce.x == 0.0f) && (collforce.y == 0.0f))
            {
                collforce.x = 0.1f;     // so cross product with up doesn't blow up
            }

            collforcemag = fsqrt(vecMagnitudeSquared(collforce));

            // make force tangential:

            vecCrossProduct(tangforce,collforce,up);
            vecNormalizeToLength(&tangforce,collforcemag);

            // decide which way to apply tangential force based on if "velocity" vector of fastest obj is left or right
            // of vector from fastestobj to slowestobj

            vecSub(fastesttoslowest,slowobj->impactable.collInfo.collPosition,fastobj->impactable.collInfo.collPosition);

            vecCrossProduct(leftrightdecidevec,fastesttoslowest,fastobj->obj.posinfo.velocity);

            if (leftrightdecidevec.z > 0)
            {
                vecCopyAndNegate(collforce,tangforce);
            }
            else
            {
                collforce = tangforce;
            }
        }
        else
        {
#if DEBUG_COLLISIONS
        dbgMessage("Xchanging Velocities");
#endif
        }

        if (!fastobj->staticinfo->common.immobile)
        {
            if(!(fastobj->obj.attributes & ATTRIBUTES_KillerCollDamage))
            {
                vecAddTo(fastobj->obj.posinfo.force,collforce);
            }
        }
        if (!slowobj->staticinfo->common.immobile)
        {
            if(!(slowobj->obj.attributes & ATTRIBUTES_KillerCollDamage))
            {
                vecSubFrom(slowobj->obj.posinfo.force,collforce);
            }
        }
    }
    else
    {
        // add a repulsion factor if objects overlap, so they don't stick together
        vecScalarMultiply(repulseoverlap,*distvector,repulse);
        if (!obj2->staticinfo->common.immobile)
        {
            if(!(obj2->obj.attributes & ATTRIBUTES_KillerCollDamage))
            {
                vecAddTo(obj2->obj.posinfo.force,repulseoverlap);
            }
        }
        if (!obj1->staticinfo->common.immobile)
        {
            if(!(obj1->obj.attributes & ATTRIBUTES_KillerCollDamage))
            {
               vecSubFrom(obj1->obj.posinfo.force,repulseoverlap);
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : ApplyCareen
    Description : Applies careen (random spin) to a ship
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void ApplyCareen(Ship *ship)
{
    // apply three random torques, to ship,

    ship->rotatable.rotinfo.torque.x += frandombetween(YAWCAREENLO,YAWCAREENHI);
    ship->rotatable.rotinfo.torque.y += frandombetween(PITCHCAREENLO,PITCHCAREENHI);
    ship->rotatable.rotinfo.torque.z += frandombetween(ROLLCAREENLO,ROLLCAREENHI);
}

void ApplyCareenRotationDirectly(Ship *ship)
{
    StaticInfo *staticheader = &ship->staticinfo->common;
    vector a;

    // apply three random torques, to ship,

    a.x = frandombetween(YAWCRAZYLO,YAWCRAZYHI) * universe.phystimeelapsed; // * staticheader->oneOverMomentOfInertiaX;
    a.y = frandombetween(PITCHCRAZYLO,PITCHCRAZYHI) * universe.phystimeelapsed; // * staticheader->oneOverMomentOfInertiaY;
    a.z = frandombetween(ROLLCRAZYLO,ROLLCRAZYHI) * universe.phystimeelapsed; // * staticheader->oneOverMomentOfInertiaZ;

    // w = w + alpha*t
    vecAddTo(ship->rotatable.rotinfo.rotspeed,a);

    // cap rotation speed
    vecCapVectorSloppy(&ship->rotatable.rotinfo.rotspeed,staticheader->maxrot);
}

/*-----------------------------------------------------------------------------
    Name        : AddShipToDeleteShipList
    Description : adds ship to the DeleteShip list (for deletion at end of main loop)
    Inputs      : ship, damageType
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void AddShipToDeleteShipList(Ship *ship,GunSoundType damageType,sdword unused)
{
    DeleteShip *deleteShip;
    ShipStaticInfo *shipstaticinfo = (ShipStaticInfo *)ship->staticinfo;

    // CAT: a third argument the body never reads (its callers pass 1); a dying Sect mothership starts its death
    //      sequence instead of going on the delete list (AddShipToDeleteShipList 0x00545b80 ->
    //      cat_mothershipStartDeathSequence 0x005bd5f0; MothershipHouseKeep explodes and deletes it later)
    (void)unused;
    if (ship->shiptype == sMothership)
    {
        cat_mothershipStartDeathSequence(ship, damageType);
        return;
    }

    ship->header.flags |= SOF_Dead;

    if (!shipstaticinfo->common.immobile)
    {
        ApplyCareen(ship);
    }

    //if (shipstaticinfo->shipclass != CLASS_Fighter)
        MakeExplosionRockBumpables(ship,(damageType == -1));

    // add to delete ship list
    deleteShip = memAlloc(sizeof(DeleteShip),"DeleteShip",Pyrophoric);
    deleteShip->ship = ship;
    // CAT: the exe drops the damageType != -1 test and reads etgDeathModeByGunType[-1], the zero byte before the
    //      table (0x0087ac63 = EDT_AccumDamage), so the HW1 test gives the same result without the out-of-bounds read
    if (ship->targetable.health <= etgBigDeathFactor[shipstaticinfo->shiprace][shipstaticinfo->shipclass] && damageType != -1)
    {                                   //ship took a really big bong hit
        deleteShip->deathBy = (udword)etgDeathModeByGunType[damageType];
    }
    else
    {                                   //if it just barely died, use the small explosion
        deleteShip->deathBy = EDT_AccumDamage;
    }
    listAddNode(&universe.DeleteShipList,&(deleteShip->objlink),deleteShip);
}

/*-----------------------------------------------------------------------------
    Name        : AddResourceToDeleteResourceList
    Description : adds resource to the DeleteResource list (for deletion at end of main loop)
    Inputs      : resource
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void AddResourceToDeleteResourceList(Resource *resource)
{
    // add this resource to the delete resource list
    DeleteResource *deleteResource = memAlloc(sizeof(DeleteResource),"DeleteResource",Pyrophoric);

    resource->header.flags |= SOF_Dead;

    deleteResource->resource = resource;
    listAddNode(&universe.DeleteResourceList,&(deleteResource->objlink),deleteResource);
}

/*-----------------------------------------------------------------------------
    Name        : AddDerelictToDeleteDerelictList
    Description : adds derelict to the DeleteDerelict list (for deletion at end of main loop)
    Inputs      : derelict
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void AddDerelictToDeleteDerelictList(Derelict *derelict,GunSoundType damageType)
{
    // add this derelict to the delete derelict list
    DeleteDerelict *deleteDerelict = memAlloc(sizeof(DeleteDerelict),"DeleteDerelict",Pyrophoric);

    bobObjectDied((SpaceObj *)derelict,&universe.collBlobList);//can't have dead derelicts in the blobs
    derelict->header.flags |= SOF_Dead;

    deleteDerelict->derelict = derelict;
    if (derelict->targetable.health <= etgBigDeathFactorDerelict[derelict->derelicttype])
    {                                   //derelict took a really big bong hit
        // HWI: crates are deleted with damageType -1 (Crates.c); the exe then reads the zero byte before the table
        //      (0x0087ac63 = EDT_AccumDamage), which this gives without the out-of-bounds read
        deleteDerelict->deathBy = (damageType < 0) ? EDT_AccumDamage : (udword)etgDeathModeByGunType[damageType];
    }
    else
    {                                   //if it just barely died, use the small explosion
        deleteDerelict->deathBy = EDT_AccumDamage;
    }
    listAddNode(&universe.DeleteDerelictList,&(deleteDerelict->objlink),deleteDerelict);
    //univRemoveFromWorldList(derelict);                      //it may be a planet; remove from the planet list if so.
}

/*-----------------------------------------------------------------------------
    Name        : AddMissileToDeleteMissileList
    Description : adds missile to the DeleteMissile list (for deletion at end of main loop)
    Inputs      : missile
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void AddMissileToDeleteMissileList(Missile *missile,GunSoundType damageType)
{
    // add this missile to the delete missile list
    DeleteMissile *deleteMissile = memAlloc(sizeof(DeleteMissile),"DeleteMissile",Pyrophoric);

    missile->header.flags |= SOF_Dead;

    deleteMissile->missile = missile;
    deleteMissile->deathBy = damageType;
    listAddNode(&universe.DeleteMissileList,&(deleteMissile->objlink),deleteMissile);
}

/*-----------------------------------------------------------------------------
    Name        : AddTargetToDeleteList
    Description : adds target (ship, asteroid, missile) to appropriate delete list
    Inputs      : target, soundType (indicating type of death)
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void AddTargetToDeleteList(SpaceObjRotImpTarg *target,GunSoundType soundType,sdword unused)
{
    (void)unused;                   // CAT: a third argument the body never reads (AddTargetToDeleteList 0x00545dd0)
    switch (target->header.objtype)
    {
        case OBJ_ShipType:
            AddShipToDeleteShipList((Ship *)target,soundType,1);
            break;

        case OBJ_AsteroidType:
        case OBJ_NebulaType:
        case OBJ_GasType:
        case OBJ_DustType:
            AddResourceToDeleteResourceList((Resource *)target);
            break;

        case OBJ_MissileType:
            AddMissileToDeleteMissileList((Missile *)target, soundType);
            break;

        case OBJ_DerelictType:
            AddDerelictToDeleteDerelictList((Derelict *)target, soundType);
            break;

        default:
            dbgAssertOrIgnore(FALSE);
            break;
    }
}

void ApplyExplosionRockToTarget(SpaceObjRotImp *obj,real32 blaststrength)
{
    obj->rotatable.rotinfo.torque.x += frandombetween(YAWEXPLOSIONROCKLO,YAWEXPLOSIONROCKHI) * blaststrength;
    obj->rotatable.rotinfo.torque.y += frandombetween(PITCHEXPLOSIONROCKLO,PITCHEXPLOSIONROCKHI) * blaststrength;
    obj->rotatable.rotinfo.torque.z += frandombetween(ROLLEXPLOSIONROCKLO,ROLLEXPLOSIONROCKHI) * blaststrength;
}

void ApplyRockToTarget(SpaceObjRotImpTarg *target,real32 projectilemass)
{
    target->rotatable.rotinfo.torque.x += frandombetween(YAWROCKLO,YAWROCKHI) * projectilemass;
    target->rotatable.rotinfo.torque.y += frandombetween(PITCHROCKLO,PITCHROCKHI) * projectilemass;
    target->rotatable.rotinfo.torque.z += frandombetween(ROLLROCKLO,ROLLROCKHI) * projectilemass;
}

/*-----------------------------------------------------------------------------
    Name        : MakeExplosionRockBumpables
    Description : Make all impactable objects within blast radius get rocked
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R4 combat): MakeExplosionRockBumpables 0x00545f00 calls the 7-argument ApplyDamageToTarget (the ship as attacker).
void MakeExplosionRockBumpables(Ship *ship,bool32 scuttle)
{
    Node *objnode = universe.ImpactableList.head;
    ShipStaticInfo *shipstatic = ship->staticinfo;
    SpaceObjRotImp *obj;
    vector distvector;
    real32 distsquared;
    real32 dist;
    real32 blastradius = shipstatic->common.staticCollInfo.collspheresize * shipstatic->blastRadiusShockWave *
                         (scuttle ? BLAST_RADIUS_SCUTTLE_BONUS : 1.0f);
    real32 blastradiussqr = blastradius * blastradius;
    real32 blaststrength;
    real32 damage;
    real32 damageperc;

    while (objnode != NULL)
    {
        obj = (SpaceObjRotImp *) listGetStructOfNode(objnode);

        if ((obj->header.flags & (SOF_Impactable|SOF_NotBumpable)) != (SOF_Impactable))
        {
            goto nextnode;      // object must be impactable, bumpable
        }

        vecSub(distvector,obj->collInfo.collPosition,ship->impactable.collInfo.collPosition);

        if (!isBetweenInclusive(distvector.x,-blastradius,blastradius))
        {
            goto nextnode;
        }

        if (!isBetweenInclusive(distvector.y,-blastradius,blastradius))
        {
            goto nextnode;
        }

        if (!isBetweenInclusive(distvector.z,-blastradius,blastradius))
        {
            goto nextnode;
        }

        distsquared = vecMagnitudeSquared(distvector);
        if (distsquared > blastradiussqr)
        {
            goto nextnode;
        }

        if (obj == &ship->impactable)
        {
            goto nextnode;
        }

        if (obj->obj.staticinfo->immobile)
        {
            goto nextnode;
        }

        // obj is within blast radius, so it should get rocked

        if (distsquared < 1.0f)
        {
            distsquared = 1.0f;
        }
        dist = fsqrt(distsquared);
        blaststrength = 1.0f - (dist / blastradius);
        vecNormalizeToLength(&distvector,(scuttle ? BLAST_CONSTANT_SCUTTLE : BLAST_CONSTANT) * blaststrength);

        ApplyExplosionRockToTarget(obj,blaststrength);
        physApplyForceVectorToObj(obj,distvector);

        if (obj->header.flags & SOF_Targetable)
        {
            // calculate damage based on ship blowing up
            real32 health = (shipstatic->blastRadiusDamage != 0.0f) ? shipstatic->blastRadiusDamage : shipstatic->health.maxhealth;

            damageperc = (scuttle ? BLAST_DAMAGE_MAX_PERCENT_SCUTTLE : BLAST_DAMAGE_MAX_PERCENT) * blaststrength;
            damage =  health * damageperc;

            if (damage > 0)
            {
                // CAT: the exploding ship is the attacker (MakeExplosionRockBumpables 0x00545f00: call sites 11, 12)
                if(ship->playerowner != NULL)
                    ApplyDamageToTarget((SpaceObjRotImpTarg *)obj,damage,0,DEATH_Killed_By_Player_Explosion,ship->playerowner->playerIndex,ship,11);
                else
                    ApplyDamageToTarget((SpaceObjRotImpTarg *)obj,damage,0,DEATH_Killed_By_Dead_Player,99,ship,12);
            }
        }

nextnode:
        objnode = objnode->next;
    }
}

/*-----------------------------------------------------------------------------
    Name        : ApplyDamageToTarget
    Description : applies damage to target, possible resulting in target being added
                  to the delete list.
    Inputs      : damageHow - Defines how the damaged was taken:
                              DEATH_Killed_By_Collision    - collision
                              DEATH_Killed_By_Player - player ship attacking
                              (anything else is assumed to be something else and essentially nothing
                              to worry about)
                  playerIndex = playerIndex of playerdoing damage
    Outputs     :
    Return      : returns TRUE if damage was fatal damage
----------------------------------------------------------------------------*/
// CAT (M2-R4 combat): ApplyDamageToTarget 0x00546250 takes two more arguments: the attacking ship (or NULL) and a code
//      for the call site (the exe's callers pass 1..0x48; the body never reads it). A Dreadnought part
//      (CLASS_Component with SOF_CatShipPart) passes its damage to the ship it is clamped to. A ship takes no damage
//      in an NIS, as a KAS derelict (catFlags18 0x4), or while the NIS bars are fully closed unless it is marked
//      (specialFlags2 0x1000000) or a kamikaze. Other objects lose the health first; every hit may send an evasive
//      ship home (cat_tacticsEvasiveRetreatToDock). A kill counts for the attacker (per class, its experience, the
//      player's kill table; a Swarmer's kill goes to the ship in its specifics +0x0C) and against the loser's player.
//      No HW1 slave ships.
bool32 ApplyDamageToTarget(SpaceObjRotImpTarg *target,real32 damagetaken,GunSoundType soundType,sdword damageHow,sdword playerIndex,
                           struct Ship *attacker,sdword callSite)
{
    bool32 targetWasAlive = ((target->header.flags & SOF_Dead) == 0);
    real32 previousHealth = target->health;
    real32 maxHealth;
    Ship *ship;
    Ship *killer;
    ShipClass shipclass;

    (void)callSite;
    if (target->header.objtype != OBJ_ShipType)
    {
        target->health -= damagetaken;
    }
    cat_tacticsEvasiveRetreatToDock((Ship *)target);

    switch (target->header.objtype)
    {
        case OBJ_AsteroidType:
            if (AsteroidTakesDamage((Asteroid *)target,(sdword)damagetaken,targetWasAlive))
            {
                AddTargetToDeleteList(target,soundType,1);
                return TRUE;                    //used by effect
            }
            return FALSE;

        case OBJ_DustType:
            switch (soundType)
            {
                case GS_LargeIonCannon:
                case GS_MediumIonCannon:
                case GS_SmallIonCannon:
                case GS_VeryLargeIonCannon:
                    //charge, but don't take damage
                    DustCloudChargesUp((DustCloud*)target, (sdword)damagetaken, targetWasAlive);
                    return FALSE;
                default:
                    return FALSE;
            }

        case OBJ_DerelictType:
            if (((Derelict *)target)->derelicttype == HyperspaceGate)
            {
                hsDerelictTakesDamage((Derelict *)target);
            }
            break;

        case OBJ_ShipType:
            ship = (Ship *)target;
            if ((ship->staticinfo->shipclass == CLASS_Component) && bitTest(ship->header.flags, SOF_CatShipPart) &&
                (ship->guidance.clampInfo != NULL) && (ship->guidance.clampInfo->host != NULL))
            {
                target = ship->guidance.clampInfo->host;
                ship = (Ship *)target;
            }
            if (nisIsRunning)
            {                                                   //do no damage during an NIS
                return FALSE;
            }
            if (bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Orphaned))
            {
                return FALSE;
            }
            if (nisFullyScissored && !bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatInvincibleInWidescreen) &&
                !bitTest(ship->shipOrDerelict.specialFlags, SPECIAL_Kamikaze))
            {
                return FALSE;
            }
            target->health -= damagetaken;
            switch(damageHow)
            {
                case DEATH_Killed_By_Player:
                case DEATH_Killed_By_Kamikaze:
                case DEATH_Killed_By_Player_Explosion:
                    // CAT: the exe's table is playerStats[playerIndex] + 0x7D4 + owner * 4 (0x00b63504 + (player * 0x1FE +
                    //      owner) * 4), which is pointsOfDamageDoneToWho at the GameStats layout's +0x7D4 (M2-R5 layout-dock)
                    universe.gameStats.playerStats[playerIndex].pointsOfDamageDoneToWho[ship->playerowner->playerIndex] += damagetaken;
                    break;
            }
            maxHealth = ship->staticinfo->health.maxhealth;
            if (previousHealth /  maxHealth >= BAT_InTheRedFactor)
            {                                               //it wasn't in the red
                if (target->health / maxHealth < BAT_InTheRedFactor)
                {                                           //if it's just now in the red
                    bitSet(ship->chatterBits, BCB_DamageInTheRed);//flag as such
                }
            }
            break;

        default:
            break;
    }

    if ((target->health > 0.0f) || (!targetWasAlive))
    {
        return FALSE;
    }

    // target just died, so add to the delete list
    if (target->header.objtype == OBJ_ShipType)
    {
        ship = (Ship *)target;
        shipclass = ship->staticinfo->shipclass;
        ship->howDidIDie = damageHow;
        ship->whoKilledMe = playerIndex; //if was collision death, who cares
        bountyShipWasKilled(ship);
        if ((damageHow >= DEATH_Killed_By_Player) && (damageHow <= DEATH_Killed_By_Kamikaze))
        {
            ship->catKilledByPlayer = playerIndex;
        }
        if ((attacker != NULL) && (attacker->header.objtype == OBJ_ShipType) && !bitTest(attacker->header.flags, SOF_Dead) &&
            (attacker->playerowner != ship->playerowner))
        {
            killer = attacker;
            if ((attacker->shiptype == sSwarmer) || (attacker->shiptype == bSwarmer))
            {
                // CAT: a Swarmer's kill counts for its hive frigate (specifics +0x0C, Ship+0x644), none without one
                //      [M2-R4 ships-b]
                killer = ((SwarmerSpec *)attacker->ShipSpecifics)->hive;
            }
            if (killer != NULL)
            {
                killer->catKillsByClass[shipclass]++;
                cat_shipUpdateExperienceLevel(killer);      // CAT (M2-R5 layout-dock): 0x005147d0, the level from the kills
            }
            if (attacker->playerowner->playerIndex < MAX_MULTIPLAYER_PLAYERS)
            {
                catShipsKilledByClass[attacker->playerowner->playerIndex][shipclass]++;
            }
        }
        if (ship->playerowner->playerIndex < MAX_MULTIPLAYER_PLAYERS)
        {
            catShipsLostByClass[ship->playerowner->playerIndex][shipclass]++;
        }
    }
    AddTargetToDeleteList(target,soundType,1);
    return TRUE;                    //used by effect
}

/*-----------------------------------------------------------------------------
    Name        : pointExplosionInSpace(vector *position,real32 blastRadius)
    Description : Make all impactable objects within blast radius get rocked
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R4 combat): pointExplosionInSpace 0x00546780 calls the 7-argument ApplyDamageToTarget.
void pointExplosionInSpace(vector *position,real32 blastRadius,real32 maxDamage,sdword playerIndex)
{
    Node *objnode = universe.ImpactableList.head;
    SpaceObjRotImp *obj;
    vector distvector;
    real32 distsquared;
    real32 dist;
    //real32 blastradius = shipstatic->common.staticCollInfo.collspheresize * shipstatic->blastRadiusShockWave;
    real32 blastradiussqr = blastRadius * blastRadius;

    real32 blaststrength;

    while (objnode != NULL)
    {
        obj = (SpaceObjRotImp *) listGetStructOfNode(objnode);

        if ((obj->header.flags & (SOF_Impactable|SOF_NotBumpable)) != (SOF_Impactable))
        {
            goto nextnode;      // object must be impactable, bumpable
        }

        vecSub(distvector,obj->collInfo.collPosition,*position);

        if (!isBetweenInclusive(distvector.x,-blastRadius,blastRadius))
        {
            goto nextnode;
        }

        if (!isBetweenInclusive(distvector.y,-blastRadius,blastRadius))
        {
            goto nextnode;
        }

        if (!isBetweenInclusive(distvector.z,-blastRadius,blastRadius))
        {
            goto nextnode;
        }

        distsquared = vecMagnitudeSquared(distvector);
        if (distsquared > blastradiussqr)
        {
            goto nextnode;
        }

        if (obj->obj.staticinfo->immobile)
        {
            goto nextnode;
        }

        // obj is within blast radius, so it should get rocked

        if (distsquared < 1.0f)
        {
            distsquared = 1.0f;
        }
        dist = fsqrt(distsquared);
        blaststrength = 1.0f - (dist / blastRadius);
        vecNormalizeToLength(&distvector,BLAST_CONSTANT * blaststrength);

        ApplyExplosionRockToTarget(obj,blaststrength);
        vecScalarMultiply(distvector,distvector,TW_BURST_ATTACK_FORCE_MODIFIER);
        physApplyForceVectorToObj(obj,distvector);

        if (obj->header.flags & SOF_Targetable)
        {
            // calculate damage based on blast as a % of maxhealth
            // CAT: the exe passes the target itself as the attacker (pointExplosionInSpace 0x00546780, call site 13)
            ApplyDamageToTarget((SpaceObjRotImpTarg *)obj,maxDamage,0,DEATH_Killed_By_Player_Explosion,playerIndex,(Ship *)obj,13);
        }

nextnode:
        objnode = objnode->next;
    }
}

/*-----------------------------------------------------------------------------
    Name        : univBulletCollidedWithTarget
    Description : Call when bullet collides with a target
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univBulletCollidedWithTarget(bool32 applyDamage,SpaceObjRotImpTarg *target,StaticInfo *targetstaticheader,Bullet *bullet,real32 collideLineDist,sdword collSide)
{
    real32 damagetaken;
    Ship *bulletowner = bullet->owner;
    real32 massovertime;
    vector recoil;
    etgeffectstatic *stat;
    sdword LOD;
    vector LOF;
    bool32 fatalHit = TRUE;         // CAT: the exe's "fatal" effect parameter starts at 1.0 (local_78 = 1.0, 0x00546a32)
    vector hitLocation;
    matrix hitCoordsys;
    real32 floatDamage;
    real32 damageMult = 1.0f;
    real32 targetScale;
    real32 glowAmount = 1.0f;

    // CAT: univBulletCollidedWithTarget 0x00546a20 takes a new first argument: FALSE for the beam frames in which a
    //      beam only draws its hit (collCheckBeamColl 0x0043c900 passes 0 or 1, collCheckBulletTargetColl 1). Without
    //      it there is no damage, push or attacker bookkeeping, only the hit effect.

    // CAT: a Sect big-gun shell explodes where it hits and loses its ping (0x00546a48: sound type GS_BigGun,
    //      cat_bigGunBulletExplodeAt 0x005bd0c0) [M2-R4 ships-b]
    if (bullet->soundType == GS_BigGun)
    {
        cat_bigGunBulletExplodeAt(bullet, &bullet->obj.posinfo.position);
        pingRemovePingByOwner((SpaceObj *)bullet);
    }

    // CAT: the damage is the bullet's damage scaled by its weapon against the target's armour level
    //      (cat_weaponDamageVsArmour 0x00514460(sound type, Ship +0x3F0 armour level; 0 for other targets),
    //      0x00546a8d / 0x00546c4a) [M2-R4 combat]
    if (target->header.objtype == OBJ_ShipType)
    {
        damagetaken = cat_weaponDamageVsArmour(bullet->soundType, ((Ship *)target)->catArmourLevel) * bullet->damage;
    }
    else
    {
        damagetaken = cat_weaponDamageVsArmour(bullet->soundType, 0) * bullet->damage;
    }
    if (target->header.objtype == OBJ_ShipType)
    {
        Ship *ship = (Ship *)target;

        // crystals are not hurt by projectiles, plasma bombs or missiles (0x00546aa4)
        if (cat_shipIsCrystal(ship) &&
            (bullet->soundType == GS_LargeProjectile || bullet->soundType == GS_LargePlasmaBomb ||
             bullet->soundType == GS_MediumPlasmaBomb || bullet->soundType == GS_MediumProjectile ||
             bullet->soundType == GS_MissleLauncher || bullet->soundType == GS_SmallPlasmaBomb ||
             bullet->soundType == GS_SmallProjectile || bullet->soundType == GS_VeryLargePlasmaBomb))
        {
            damagetaken = 0.0f;
        }
        // a Nomad moon does not hurt its own field emitters (0x00546ae5)
        if (bulletowner != NULL && bulletowner->shiptype == dNomadMoon && ship->shiptype == dNomadFieldEmitter)
        {
            damagetaken = 0.0f;
        }
        if (!applyDamage)
        {
            goto hiteffect;
        }
        // an active Sentinel field takes the hit for its Sentinel (0x00546b10)
        if ((ship->shiptype == sSentinel || ship->shiptype == bSentinel) && cat_shipIsSentinelFieldActive(ship))
        {
            cat_sentinelFieldBulletHit(ship, (void *)-1, bullet);       // CAT: 0x005c6c60 (M2-R4 ships-b)
            applyDamage = FALSE;
            goto hiteffect;
        }
    }
    if (!applyDamage)
    {
        goto hiteffect;
    }

    // if target isn't immobile, apply impact force from bullet
    if (!targetstaticheader->immobile && !bitTest(target->header.flags, SOF_NotBumpable))
    {
        if(target->header.objtype == OBJ_ShipType )
        {
            if(((Ship *)target)->shipOrDerelict.specialFlags & SPECIAL_ShipInGravwell)   // CAT: specialFlags in the exe
            {
                goto nobulletmasstransfer;
            }
        }
        massovertime = bullet->bulletmass / universe.phystimeelapsed;
        recoil = bullet->obj.posinfo.velocity;
        vecMultiplyByScalar(recoil,massovertime);
        ApplyRockToTarget(target,bullet->bulletmass);
        physApplyForceVectorToObj(target,recoil);
    }

nobulletmasstransfer:
    if (target->header.objtype == OBJ_ShipType)
    {
        Ship* ship = (Ship*)target;
        damageMult = ship->magnitudeSquared;

        // apply different damage modifier based on collision side:
        if (collSide >= 0)
        {
            dbgAssertOrIgnore(collSide < NUM_TRANS_DEGOFFREEDOM);
            damagetaken *= ship->staticinfo->collSideModifiers[collSide];
        }

        if (bulletowner != NULL)
        {
            // CAT: armour-piercing heavy defenders (kHeavyDefender, tHeavyDefender) against fighters (0x00546bd8)
            if (((bulletowner->shiptype == kHeavyDefender) || (bulletowner->shiptype == tHeavyDefender)) &&
                (ship->staticinfo->shipclass == CLASS_Fighter))
            {
                // armour piercing bullets vs fighters
                damagetaken *= armourPiercingModifier;
            }

            if (bulletowner->playerowner == ship->playerowner)
            {                                               //if it's friendly fire
                // CAT: the retail exe always applies it (0x00546c15); HW1 skipped it in debug builds
                damagetaken *= friendlyFireModifier;
                bitSet(ship->chatterBits, BCB_FriendlyFire);
            }
            else if (allianceIsShipAlly(ship, bulletowner->playerowner))
            {
                bitSet(ship->chatterBits, BCB_AlliedFire);
            }
            else
            {
                speechEventUnderAttack(ship);
            }
            ship->gettingrocked = bulletowner;

            ship->recentAttacker = bulletowner;
            ship->recentlyAttacked = RECENT_ATTACK_DURATION;  // start counting down
        }
        else
        {
            speechEventUnderAttack(ship);
        }
    }

    // CAT (M2-R4 combat): ApplyDamageToTarget 0x00546250 takes two more arguments here: the attacking ship (for its
    //      experience and the kill counts) and a call-site code (0xE with an owner, 0xF without)
    if(bulletowner != NULL && bulletowner->playerowner != NULL)
        fatalHit = ApplyDamageToTarget(target,damagetaken,bullet->soundType,DEATH_Killed_By_Player,bulletowner->playerowner->playerIndex,bulletowner,14);
    else
        fatalHit = ApplyDamageToTarget(target,damagetaken,bullet->soundType,DEATH_Killed_By_Dead_Player,99,bulletowner,15);

hiteffect:
    //delete the bullet effect
    // CAT: the exe keeps the flight effect of beams and of Beast infection bullets (weapon word 2 or 8, 0x00546b5a)
    //      [M2-R4 combat: the weapon word]

#if ETG_DISABLEABLE
    if (bullet->effect != NULL && etgEffectsEnabled && bullet->bulletType != WEAPON_ION && bullet->bulletType != WEAPON_BEASTINFECT)
#else
    if (bullet->effect != NULL && bullet->bulletType != WEAPON_ION && bullet->bulletType != WEAPON_BEASTINFECT)
#endif
    {
        etgEffectDelete(bullet->effect);
        univRemoveObjFromRenderList((SpaceObj *)bullet->effect);
        listDeleteNode(&bullet->effect->header.objlink);
        bullet->effect = NULL;
    }

    if (bullet->bulletType != BULLET_Beam)
    {
        if (collideLineDist == 0.0f)
        {                                                   //if it's a beam weapon and there is no collision vector
            return;                                         //don't play any hit effects
        }
    }

    // CAT: the hit location is worked out before the effect test: the bullet-hit glow below uses it too (the exe
    //      computes it inside the effect branch only and hands the glow an unset vector when no effect is made)
    if (collideLineDist == 0.0f)
    {
        hitLocation = bullet->obj.posinfo.position;
    }
    else
    {
        vecScalarMultiply(hitLocation,bullet->bulletheading,collideLineDist);
        vecAdd(hitLocation,bullet->obj.posinfo.position,hitLocation);
    }
    //N-Lip the impact position
    if (target->header.objtype == OBJ_ShipType)
    {
        targetScale = ((Ship *)target)->magnitudeSquared;
        vecSubFrom(hitLocation, target->obj.posinfo.position);
        vecMultiplyByScalar(hitLocation, targetScale);
        vecAddTo(hitLocation, target->obj.posinfo.position);
    }
    else
    {
        targetScale = 1.0f;
    }

    //play the imact effect
    if (bullet->hitEffect != NULL)
    {
        LOD = target->obj.currentLOD;
        if (LOD < bullet->hitEffect->nLevels)
        {
            stat = bullet->hitEffect->level[LOD];
        }
        else
        {
            stat = NULL;
        }
    }
    else
    {
        stat = NULL;
    }
    if (univSpaceObjInRenderList((SpaceObj *)target))
    {
#if ETG_DISABLEABLE
        if (stat != NULL && etgHitEffectsEnabled && etgEffectsEnabled && (!etgFrequencyExceeded(stat)))
#else
        if (stat != NULL && etgHitEffectsEnabled && (!etgFrequencyExceeded(stat)))
#endif
        {
            vecCopyAndNegate(LOF,bullet->bulletheading);
            matCreateCoordSysFromHeading(&hitCoordsys,&LOF);
            floatDamage = (real32)damagetaken;
            floatDamage *= damageMult;
            // CAT: both effect parameters are floats in the exe: the damage, and the fatal flag as 0.0 / 1.0
            //      ((float)ApplyDamageToTarget(...), 0x00546eb6; HW1 passed the flag as an integer)
            etgEffectCreate(stat, NULL, &hitLocation, &target->obj.posinfo.velocity, &hitCoordsys, targetScale, 0, 2,
                            SCALECAST(Real32ToUdword(floatDamage)), SCALECAST(Real32ToUdword(fatalHit ? 1.0f : 0.0f)));
        }
    }

    // CAT: a ship that takes damage glows where it was hit (0x0054703a: cat_rndObjectEffectsSetup(target), then
    //      cat_meshApplyBulletHit(hit point, radius^2 1.0, target, cat_meshBulletHitGlowCB 0x004b5820, &1.0f)).
    //      The render object is reset afterwards (HWI: the exe leaves it pointing at the target until the next
    //      render sets it)
    if (applyDamage && target->header.objtype == OBJ_ShipType)
    {
        cat_rndObjectEffectsSetup((SpaceObj *)target);
        cat_meshApplyBulletHit(&hitLocation, 1.0f, (Ship *)target, cat_meshBulletHitGlowCB, &glowAmount);
        catShCurrentObject = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : univMissileCollidedWithTarget
    Description : Call when a missile collides with a target
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: univMissileCollidedWithTarget 0x00547080 takes a new first argument as univBulletCollidedWithTarget does:
//      without it only the hit effect plays. All four callers (collCheckMissileShipColl 0x0043d330,
//      collCheckMissileResourceColl 0x0043d720, collCheckMissileDerelictColl 0x0043d970, collCheckMissileMineColl
//      0x0043dbb0) pass 1.
void univMissileCollidedWithTarget(bool32 applyDamage,SpaceObjRotImpTarg *target,StaticInfo *targetstaticheader,Missile *missile,real32 collideLineDist,sdword collSide)
{
    real32 damagetaken = missile->damage;
    Ship *missileowner = missile->owner;
    MissileStaticInfo *missilestaticinfo = (MissileStaticInfo *)missile->staticinfo;
    real32 massovertime;
    vector recoil;
    etgeffectstatic *stat;
    sdword LOD;
    bool32 fatalHit;
    real32 floatDamage;
    udword intDamage;
    vector missileheading;
    vector hitLocation;
    real32 targetScale;
    real32 damageMult = 1.0f;

    if (!applyDamage)
    {
        fatalHit = FALSE;
        goto hiteffect;
    }

    if(target->header.objtype == OBJ_ShipType)
    {
        Ship* ship = (Ship*)target;
        if(ship->playerowner->playerIndex == universe.curPlayerIndex)
        {
            if(missile->FORCE_DROPPED)
            {
                // CAT: the fleet reports it, by hot-key group when the ship has one (univMissileCollidedWithTarget
                //      0x005470e2: battleChatterFleetAttempt(-1, 0x34 / 0x35, -1 / group, NULL)); no
                //      battleCanChatterAtThisTime test
                if (selHotKeyGroupNumberTest(ship) == SEL_InvalidHotKey)
                {
                    battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_STAT_Grp_InMineField, SOUND_EVENT_DEFAULT, NULL);
                }
                else
                {
                    battleChatterFleetAttempt(SOUND_EVENT_DEFAULT, BCE_STAT_AssGrp_InMineField, selHotKeyGroupNumberTest(ship), NULL);
                }
            }
        }
    }
    // if target isn't immobile, apply impact force from missile
    if (!targetstaticheader->immobile && !bitTest(target->header.flags, SOF_NotBumpable))
    {
        massovertime = missilestaticinfo->common.mass / universe.phystimeelapsed;
        recoil = missile->obj.posinfo.velocity;
        vecMultiplyByScalar(recoil,massovertime);
        ApplyRockToTarget(target,missilestaticinfo->common.mass);
        physApplyForceVectorToObj(target,recoil);
    }

    if (target->header.objtype == OBJ_ShipType)
    {
        Ship* ship = (Ship*)target;
        damageMult = ship->magnitudeSquared;
        // apply different damage modifier based on collision side:
        if (collSide >= 0)
        {
            dbgAssertOrIgnore(collSide < NUM_TRANS_DEGOFFREEDOM);
            damagetaken *= ship->staticinfo->collSideModifiers[collSide];
        }

        if (missileowner != NULL)
        {
            if (missileowner->playerowner == ship->playerowner)
            {                                               //if it's friendly fire
                // CAT: the retail exe always applies it (0x00547244); HW1 skipped it in debug builds
                damagetaken *= friendlyFireModifier;
                bitSet(ship->chatterBits, BCB_FriendlyFire);
            }
            else if (allianceIsShipAlly(ship, missileowner->playerowner))
            {
                bitSet(ship->chatterBits, BCB_AlliedFire);
            }
            else
            {
                speechEventUnderAttack(ship);
            }
            ship->gettingrocked = missileowner;

            ship->recentAttacker = missileowner;
            ship->recentlyAttacked = RECENT_ATTACK_DURATION;  // start counting down
        }
        else
        {
            speechEventUnderAttack(ship);
        }
    }

    if(missileowner != NULL && missileowner->playerowner != NULL)
        fatalHit = ApplyDamageToTarget(target,damagetaken,missile->soundType,DEATH_Killed_By_Player,missileowner->playerowner->playerIndex,missileowner,16);
    else
        fatalHit = ApplyDamageToTarget(target,damagetaken,missile->soundType,DEATH_Killed_By_Dead_Player,99,missileowner,17);

hiteffect:
    if (missile->hitEffect != NULL)
    {
        LOD = target->obj.currentLOD;
        if (LOD < missile->hitEffect->nLevels)
        {
            stat = missile->hitEffect->level[LOD];
        }
        else
        {
            stat = NULL;
        }
    }
    else
    {
        stat = NULL;
    }
    if (univSpaceObjInRenderList((SpaceObj *)target))
    {
#if ETG_DISABLEABLE
        if (stat != NULL && etgHitEffectsEnabled && etgEffectsEnabled && (!etgFrequencyExceeded(stat)))
#else
        if (stat != NULL && etgHitEffectsEnabled && (!etgFrequencyExceeded(stat)))
#endif
        {
            if (collideLineDist == 0.0f)
            {
                hitLocation = missile->obj.posinfo.position;
            }
            else
            {
                dbgAssertOrIgnore(collideLineDist > 0.0f);
                matGetVectFromMatrixCol3(missileheading,missile->rotatable.rotinfo.coordsys);
                vecScalarMultiply(hitLocation,missileheading,collideLineDist);
                vecAdd(hitLocation,missile->obj.posinfo.position,hitLocation);
            }
            //N-Lip the impact position
            if (target->header.objtype == OBJ_ShipType)
            {
                targetScale = ((Ship *)target)->magnitudeSquared;
                vecSubFrom(hitLocation, target->obj.posinfo.position);
                vecMultiplyByScalar(hitLocation, targetScale);
                vecAddTo(hitLocation, target->obj.posinfo.position);
            }
            else
            {
                targetScale = 1.0f;
            }

            floatDamage = (real32)damagetaken;
            floatDamage *= damageMult;
            intDamage = Real32ToUdword(floatDamage);
            etgEffectCreate(stat, NULL, &hitLocation, &target->obj.posinfo.velocity, &missile->rotatable.rotinfo.coordsys, targetScale, 0, 2, SCALECAST(intDamage), SCALECAST(fatalHit));
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : univRemoveAllTargetReferencesFromBullets
    Description : removes all ship references from any bullets
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univRemoveAllTargetReferencesFromBullets(SpaceObjRotImpTarg *target)
{
    Node *objnodebullet = universe.BulletList.head;
    Bullet *bullet;

    // Update bullet->owner for all bullets whose owner is now dead
    while (objnodebullet != NULL)
    {
        bullet = (Bullet *)listGetStructOfNode(objnodebullet);
        dbgAssertOrIgnore(bullet->header.objtype == OBJ_BulletType);

        if (&bullet->owner->targetable == target)
        {
            bullet->owner = NULL;       // bullet has no owner now that parent ship died
            bullet->gunowner = NULL;
            if (bullet->bulletType == BULLET_Beam)
            {
                bullet->timelived += 10000.0f;  // delete bullet if it is a beam weapon and target wielding it died
            }
        }

        if (bullet->target == target)
        {
            bullet->target = NULL;
        }

        objnodebullet = objnodebullet->next;
    }
}

/*-----------------------------------------------------------------------------
    Name        : univRemoveAllTargetReferencesFromMissiles
    Description : removes all target references from any missiles
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        : cat_univClampResourceTimer
    Description : CAT: a missile that loses its target self-destructs within 5 seconds (cataclysm.exe 0x00547530,
                  its binary-map name; univReallyDeleteThisResourceRightNow 0x005490d0 and univDeleteDeadResources
                  0x00549270 call it for a missile whose target resource died, univRemoveAllTargetReferencesFromMissiles
                  0x00547570 inlines the same clamp): totallifetime is capped at timelived + the tweak 0x008b5c68 (5.0)
    Inputs      : missile (the exe accepts NULL)
----------------------------------------------------------------------------*/
void cat_univClampResourceTimer(Missile *missile)
{
    if ((missile != NULL) && (missile->totallifetime - missile->timelived > CAT_MISSILE_LOST_TARGET_LIFE))
    {
        missile->totallifetime = missile->timelived + CAT_MISSILE_LOST_TARGET_LIFE;
    }
}

void univRemoveAllTargetReferencesFromMissiles(SpaceObjRotImpTarg *target)
{
    Node *objnodemissile = universe.MissileList.head;
    Missile *missile;

    // Update missile->owner for all missiles whose owner is now dead
    while (objnodemissile != NULL)
    {
        missile = (Missile *)listGetStructOfNode(objnodemissile);
        dbgAssertOrIgnore(missile->header.objtype == OBJ_MissileType);

        if (&missile->owner->targetable == target)
        {
            missile->owner = NULL;       // missile has no owner now that parent ship died
        }

        if (missile->target == target)
        {
            missile->target = NULL;
            cat_univClampResourceTimer(missile);        // CAT: within 5 s (univRemoveAllTargetReferencesFromMissiles 0x00547570 inlines it)
        }

        objnodemissile = objnodemissile->next;
    }
}

// CAT (M2-R4 combat): no univRemoveAllDerelictReferencesFromSpecialShips: it only told HW1's salvagers to forget a
//      derelict (salCapRemoveDerelictReferences); cataclysm.exe has no such function and Cataclysm does not salvage
//      derelicts (cat_salvageCheckTarget 0x004f7eb0 refuses every non-ship)

void univRemoveAllShipReferencesFromSpecialShip(Ship *shiptoremove,Ship *ship)
{
    // CAT: the attacker memories are cleared whenever they point at the ship, counted down or not
    //      (univRemoveAllShipReferencesFromSpecialShip 0x00547640)
    if (ship->recentAttacker == shiptoremove)
    {
        // forget that this ship had recently attacked anyone
        ship->recentlyAttacked = 0;
        ship->recentAttacker = NULL;
    }
    if (ship->firingAtUs == shiptoremove)
    {
        ship->recentlyFiredUpon = 0;
        ship->firingAtUs = NULL;
    }
    // CAT: and so is gettingrocked
    if (ship->gettingrocked == shiptoremove)
    {
        ship->gettingrocked = NULL;
    }

    // CAT: no right-of-way callback any more (the exe calls an empty function there)
    if (ship->rowGetOutOfWay == shiptoremove)
    {
        ship->rowGetOutOfWay = NULL;
    }

    // CAT: three Cataclysm ship pointers are cleared too (exe Ship +0x3EC, +0x608, +0x4E8) [M2-R4 ships-b]
    if (ship->catSalvageDepot == shiptoremove)
    {
        ship->catSalvageDepot = NULL;
    }
    if (ship->catSalvageBigGun == shiptoremove)
    {
        ship->catSalvageBigGun = NULL;
    }
    if (ship->catBeastInfector == shiptoremove)
    {
        ship->catBeastInfector = NULL;
    }

    // CAT: the ship's CustShipRemoveShipReferences (slot 11) always runs (HW1 skipped research ships at game end)
    if(((ShipStaticInfo *)ship->staticinfo)->custshipheader.CustShipRemoveShipReferences != NULL)
    {
        ((ShipStaticInfo *)ship->staticinfo)->custshipheader.CustShipRemoveShipReferences(ship, shiptoremove);
    }
}

/*-----------------------------------------------------------------------------
    Name        : univRemoveAllShipReferencesFromSpecialShips
    Description : removes all ship references from special ships
    Inputs      : shiptoremove
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univRemoveAllShipReferencesFromSpecialShips(Ship *shiptoremove)
{
    Node *node = universe.ShipList.head;
    Node *insidenode;
    Ship *ship;
    InsideShip *insideShip;

    while (node != NULL)
    {
        ship = (Ship *)listGetStructOfNode(node);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        univRemoveAllShipReferencesFromSpecialShip(shiptoremove,ship);

        // CAT: the ship stops docking with it and drops it as its queued attack target (Ship +0x4B0; only the ships of
        //      the ShipList, not the ones in hyperspace below) (univRemoveAllShipReferencesFromSpecialShips 0x005476e0)
        //      [integrate: M2-R5; was an M2-R4 #4 TODO]
        if (ship->dockvars.dockship == shiptoremove)
        {
            ship->dockvars.dockship = NULL;
        }
        if (ship->catQueuedAttackTarget == shiptoremove)
        {
            ship->catQueuedAttackTarget = NULL;
        }
        // CAT: the exe then looks for an inside-ship entry that holds the ship itself (not shiptoremove) and, if it
        //      finds one, deletes it, updates the launch manager and counts it out by shiptoremove's class
        //      (fighter/micro or corvette/resource); kept as written, although no ship is ever inside itself
        if (ship->shipsInsideMe != NULL)
        {
            for (insidenode = ship->shipsInsideMe->insideList.head; insidenode != NULL; insidenode = insidenode->next)
            {
                insideShip = (InsideShip *)listGetStructOfNode(insidenode);
                if (insideShip->ship == ship)
                {
                    listDeleteNode(insidenode);
                    lmUpdateShipsInside();
                    switch (shiptoremove->staticinfo->shipclass)
                    {
                        case CLASS_Fighter:
                        case CLASS_Micro:
                            ship->shipsInsideMe->FightersInsideme--;
                            break;
                        case CLASS_Corvette:
                        case CLASS_Resource:
                            ship->shipsInsideMe->CorvettesInsideme--;
                            break;
                        default:
                            break;
                    }
                    break;
                }
            }
        }

        node = node->next;
    }

    // walk through ships in hyperspace too!

    node = singlePlayerGameInfo.ShipsInHyperspace.head;

    while (node != NULL)
    {
        insideShip = (InsideShip *)listGetStructOfNode(node);
        ship = insideShip->ship;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        univRemoveAllShipReferencesFromSpecialShip(shiptoremove,ship);
        if (ship->dockvars.dockship == shiptoremove)
        {                                                   // CAT: 0x0054779b
            ship->dockvars.dockship = NULL;
        }

        node = node->next;
    }
}

/*-----------------------------------------------------------------------------
    Name        : DeleteShipsInsideMe
    Description : deletes ships that are inside me
    Inputs      : ship which contains other ships
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void DeleteShipsInsideMe(Ship *ship)
{
    InsideShip *insideShip;
    Node *node = ship->shipsInsideMe->insideList.head;

    while (node != NULL)
    {
        insideShip = listGetStructOfNode(node);

        dbgAssertOrIgnore(insideShip->ship);
        insideShip->ship->howDidIDie = ship->howDidIDie;
        insideShip->ship->whoKilledMe = ship->whoKilledMe;
        univDeleteWipeInsideShipOutOfExistence(insideShip->ship);

        node = node->next;
    }

    listDeleteAll(&ship->shipsInsideMe->insideList);
    memFree(ship->shipsInsideMe);
    ship->shipsInsideMe = NULL;
}

/*-----------------------------------------------------------------------------
    Name        : FreeShipsInsideMe
    Description : frees ships contents that are inside me
    Inputs      : ship which contains other ships
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void FreeShipsInsideMe(Ship *ship)
{
    InsideShip *insideShip;
    Node *node = ship->shipsInsideMe->insideList.head;

    while (node != NULL)
    {
        insideShip = listGetStructOfNode(node);

        dbgAssertOrIgnore(insideShip->ship);
        insideShip->ship->howDidIDie = ship->howDidIDie;
        insideShip->ship->whoKilledMe = ship->whoKilledMe;
        univFreeShipContents(insideShip->ship);
        memFree(insideShip->ship);

        node = node->next;
    }

    listDeleteAll(&ship->shipsInsideMe->insideList);
    memFree(ship->shipsInsideMe);
    ship->shipsInsideMe = NULL;
}

/*-----------------------------------------------------------------------------
    Name        : univWipeShipOutOfExistence
    Description : totally destroys and wipes this ship out of memory, and
                  removes any remaining references.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: set by kasfMission13DeleteEngineeringModuleHack 0x0049a480 (stage 6) for a ship that is in no object list:
//      univWipeShipOutOfExistence 0x005477d0 then frees it directly and clears the flag (0x00a44000)
bool32 catWipeShipFreeDirectly = FALSE;

void univWipeShipOutOfExistence(Ship *ship)
{
    dbgAssertOrIgnore(ship != NULL);
    // CAT-TODO(stage 3 FOW): the exe first takes the ship's fog-of-war cube away (cat_playerGetFOWGrid, fowSelect,
    //      cat_shipAffectsFOW, fowDeactivateCubeW(position, cat_shipGetFOWRadius(ship, 0), 0) at 0x005477d0)
    // CAT: then the two Cataclysm binding blocks go (exe Ship +0x4FC, +0x510) [M2-R4 ships-b]
    if (ship->catBeastBindings != NULL)
    {
        memFree(ship->catBeastBindings);
        ship->catBeastBindings = NULL;
    }
    if (ship->catSalvageConvertBindings != NULL)
    {
        memFree(ship->catSalvageConvertBindings);
        ship->catSalvageConvertBindings = NULL;
    }
    if (ship->shiplink.belongto != NULL)
    {
        listRemoveNode(&ship->shiplink);
    }
    if (ship->impactable.impactablelink.belongto != NULL)
    {
        listRemoveNode(&ship->impactable.impactablelink);
    }
    nisObjectDied((SpaceObj *)ship);
    etgShipDied(ship);
    univRemoveObjFromRenderList((SpaceObj *)ship);
    univFreeShipContents(ship);
    if (catWipeShipFreeDirectly)
    {                                                       // CAT: 0x005478c2
        catWipeShipFreeDirectly = FALSE;
        memFree(ship);
        return;
    }
    if (ship->header.objlink.belongto != NULL)
    {                                                       // CAT: only if it is in a list (0x005478e0)
        listDeleteNode(&ship->header.objlink);
    }
}

/*-----------------------------------------------------------------------------
    Name        : univDeleteWipeInsideShipOutOfExistence
    Description : totally destroys and wipes a "ship that was inside another ship"
                  out of memory.
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univDeleteWipeInsideShipOutOfExistence(Ship *ship)
{
    // CAT: the ship leaves the ship, impactable and object lists and every subsystem hears of it before it is freed
    //      (univDeleteWipeInsideShipOutOfExistence 0x00547900; univDeleteDeadShip 0x00547d00 inlines the same body
    //      for the ships inside a dying ship)
    if (ship->shiplink.belongto != NULL)
    {
        listRemoveNode(&ship->shiplink);
    }
    if (ship->impactable.impactablelink.belongto != NULL)
    {
        listRemoveNode(&ship->impactable.impactablelink);
    }
    if (ship->header.objlink.belongto != NULL)
    {
        listRemoveNode(&ship->header.objlink);
    }
    etgShipDied(ship);
    soundEventShipRemove(ship);
    pieShipDied(ship);
    mouseClickShipDied(ship);
    clShipDied(&universe.mainCommandLayer,ship);
    univRemoveAllShipReferencesFromSpecialShips(ship);
    univFreeShipContents(ship);
    memFree(ship);                  //added Ala the request of Gary
}

/*-----------------------------------------------------------------------------
    Name        : univRemoveShipFromOutside
    Description : removes a ship from the universe and from mission spheres so that it
                  can be placed inside a ship
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univRemoveShipFromOutside(Ship *ship)
{
    Node *node;
    Missile *missile;

    // if a ship goes inside another ship, missile can no longer target it
    // CAT: and a missile that loses its target self-destructs within 5 seconds (univRemoveShipFromOutside 0x00547990
    //      inlines this loop; univDeleteDeadShip 0x00547d00 has the same one)
    for (node = universe.MissileList.head; node != NULL; node = node->next)
    {
        missile = (Missile *)listGetStructOfNode(node);
        if (missile->owner == ship)
        {
            missile->owner = NULL;
        }
        if (&missile->target->header == &ship->header)
        {
            missile->target = NULL;
            if (missile->totallifetime - missile->timelived > CAT_MISSILE_LOST_TARGET_LIFE)
            {
                missile->totallifetime = missile->timelived + CAT_MISSILE_LOST_TARGET_LIFE;
            }
        }
    }

    shipHasJustDisappeared(ship);

    // Update Camera focus stacks
    ccRemoveShip(&universe.mainCameraCommand,ship);

    // Let any mission spheres know ship is leaving it
//    msShipLeavingMissphere(ship,ship->myMissphere,ship->playerowner->homeMissphere);

    // Remove ships that are destroyed from any current selection
    clRemoveShipFromSelection((SelectAnyCommand *)&selSelected,ship);
    clRemoveShipFromSelection((SelectAnyCommand *)&selSelecting,ship);
    ioUpdateShipTotals();

    // Remove ship from hotkey group if it belongs to one.
    //univRemoveShipFromHotkeyGroup(ship, FALSE);       maintain hotkey groups

    // CAT: smObjectDied is called whether or not the sensors manager is up (0x00547a52)
    smObjectDied(ship);

    bobObjectDied((SpaceObj *)ship,&universe.collBlobList);
    pingObjectDied((SpaceObj *)ship);
    ship->obj.collMyBlob = NULL;

    pieShipDied(ship);
    mouseClickShipDied(ship);

    // CAT: the AI players hear of the ship leaving in multiplayer only (0x00547a8e)
    if (!singlePlayerGame)
    {
        aiplayerShipDied(ship);
    }

    if (ship->shiplink.belongto != NULL)
    {
        listRemoveNode(&ship->shiplink);
    }
    if (ship->impactable.impactablelink.belongto != NULL)
    {
        listRemoveNode(&ship->impactable.impactablelink);
    }
    if (ship->header.objlink.belongto != NULL)
    {
        listRemoveNode(&ship->header.objlink);
    }
    univRemoveObjFromRenderList((SpaceObj *)ship);

    // Tell the sound layer that the ship is removed from the universe
    soundEventShipRemove(ship);

    ship->header.flags |= SOF_Hide;

    // CAT (M2-R4 ships-b): the ship's module ships leave the render list and are hidden too (0x00547b02:
    //      cat_moduleIterFirst 0x005afed0 / cat_moduleIterNext 0x005afee0)
    {
        Ship *module;

        for (module = cat_moduleIterFirst(ship); module != NULL; module = cat_moduleIterNext())
        {
            if (module->obj.renderlink.belongto != NULL)
            {
                listRemoveNode(&module->obj.renderlink);
            }
            module->header.flags |= SOF_Hide;
        }
    }
    // CAT-TODO(stage 3 FOW): with its second argument set, the exe also takes
    //      the ship's fog-of-war cube away (fowDeactivateCubeW(..., 1)); the argument gates nothing else, so this
    //      tree keeps the one-argument form until the fog of war is ported.
    // CAT: the ship forgets who was attacking it (0x00547b4a..0x00547b65)
    ship->gettingrocked = NULL;
    ship->recentAttacker = NULL;
    ship->recentlyAttacked = 0;
    ship->firingAtUs = NULL;
    ship->recentlyFiredUpon = 0;
}

/*-----------------------------------------------------------------------------
    Name        : univRemoveShipFromHotkeyGroup
    Description : Removes ship from hotkey group
    Inputs      : ship
    Outputs     : if ship was removed or not.
    Return      : bool8
----------------------------------------------------------------------------*/
bool8 univRemoveShipFromHotkeyGroup(Ship *ship, bool8 PlayDefeat)
{
    udword i;

    if (selAnyHotKeyTest(ship))
    {                                                       //if ship in any hot key groups
        // Remove ships that are destroyed from any hot key groups
        for (i=0;i<SEL_NumberHotKeyGroups;i++)
        {
            if (selHotKeyTest(ship, i))
            {                                               //if it's in this hot-key group
                if (clRemoveShipFromSelection((SelectCommand *)&selHotKeyGroup[i],ship))
                {
                    if (selHotKeyGroup[i].numShips == 0)
                    {
                        selHotKeyGroup[i].timeLastStatus = universe.totaltimeelapsed;
                        if (PlayDefeat)
                        {
                            // CAT: Cataclysm's event 0x2002076 (STAT_Intel_AssGrp_Defeat by the pattern of
                            //      Battle.c's 0x2002075 Victory; univRemoveShipFromHotkeyGroup 0x00547b70)
                            speechEventFleet(0x2002076, i, ship->playerowner->playerIndex);
                        }
                    }
                }
            }
        }

        ship->hotKeyGroup = 0;

        return(TRUE);
    }
    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : univHideShipFromSpheres
    Description : Removes a ship from all mission spheres but not from the universe
    Inputs      :
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void univHideShipFromSpheres(Ship *ship)
{
    univRemoveObjFromRenderList((SpaceObj *)ship);
    ship->header.flags |= SOF_Hide;
}

/*-----------------------------------------------------------------------------
    Name        : univHideJustAboutEverything
    Description : Sets the SOF_Hide bit on evertything that is not a resource
                    and does not have the SOF_NISShip bit set.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univHideJustAboutEverything(void)
{
    Node *node;
    SpaceObj *obj;

    for (node = universe.SpaceObjList.head; node != NULL; node = node->next)
    {
        obj = (SpaceObj *)listGetStructOfNode(node);
        if (!bitTest(obj->header.flags, SOF_NISShip))
        {
            switch (obj->header.objtype)
            {
                case OBJ_ShipType:
                case OBJ_BulletType:
                case OBJ_DerelictType:
                case OBJ_EffectType:
                case OBJ_MissileType:
                    if (bitTest(obj->header.flags, SOF_Hide))
                    {
                        bitSet(obj->header.flags, SOF_HideSave2);
                    }
                    else
                    {
                        univRemoveObjFromRenderList(obj);
                        bitSet(obj->header.flags, SOF_Hide);
                    }
                    break;
                case OBJ_AsteroidType:
                case OBJ_NebulaType:
                case OBJ_GasType:
                case OBJ_DustType:
                    break;
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : univUnhideJustAboutEverything
    Description : Unhide everything that was hidden in a previous call
                    to univHideJustAboutEverything.
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void univUnhideJustAboutEverything(void)
{
    Node *node;
    SpaceObj *obj;

    for (node = universe.SpaceObjList.head; node != NULL; node = node->next)
    {
        obj = (SpaceObj *)listGetStructOfNode(node);
        if (bitTest(obj->header.flags, SOF_HideSave2))
        {                                                   //if it was already hidden last univHideJustAboutEverything
            bitClear(obj->header.flags, SOF_HideSave2);
        }
        else
        {
            bitClear(obj->header.flags, SOF_Hide);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : univDeleteDeadShip
    Description : Removes all references to the ship, except for a few ones
                  to let the ship hang around after the explosion.
    Inputs      : ship
                  deathby - mode of death, or -1 for no death effects
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univDeleteDeadShip(Ship *ship, sdword deathBy)
{
    Effect *effect;
    etgeffectstatic *explosion;
    etglod *etgLOD;
    sdword LOD;
    real32 colSize;
    udword colSizeDword, velMagnitudeDword;
    real32 velMagnitude, maxVelocity;
    // CAT: the big-ship death explosions (univDeleteDeadShip 0x00547d00, 0x00547e5d..0x00548175)
    bool32 bigExplosion = FALSE;
    sdword bigMode = 0;
    real32 bigScale = 1.0f;
    StaticCollInfo *coll;
    Node *node;
    Bullet *bullet;
    Missile *missile;

    vecZeroVector(ship->obj.posinfo.force);
    vecZeroVector(ship->rotatable.rotinfo.torque);

    ship->header.flags |= SOF_Dead;
    bitClear(ship->header.flags,SOF_Selectable);

    // CAT (M2-R4 combat): in single player the AI's salvage depot stops being it (0x00547d37: cat_salvageGetAIDepotShip 0x004fd2f0
    //      == ship -> cat_salvageSetAIDepotShip(NULL) 0x004fd2e0)
    if (singlePlayerGame && (cat_salvageGetAIDepotShip() == ship))
    {
        cat_salvageSetAIDepotShip(NULL);
    }

    // CAT: the ship's CustShipDied comes first (statinfo +0x144, 0x00547d60), before the factory and launch updates
    if(((ShipStaticInfo *)ship->staticinfo)->custshipheader.CustShipDied != NULL)
    {
        ((ShipStaticInfo *)ship->staticinfo)->custshipheader.CustShipDied(ship);
    }

    // CAT: a factory is removed unless it is an enemy's in single player (0x00547d6c)
    if ((((ShipStaticInfo *)(ship->staticinfo))->canBuildShips) &&
        (!singlePlayerGame || ship->playerowner->playerIndex == 0))
    {
        cmRemoveFactory(ship);
    }

    // CAT: externally built ships (0x00547d97). CAT-TODO(M2-R6 ConsMgr, the external-build driver):
    //      cat_cmExternalBuildShipDied 0x00455960 (the build queue entry that waits for the ship). Then the exe clears
    //      SPECIAL_2_CatBeingBuilt (0x00547dab) [audit: ported by the M2-R4 command audit; the flag is at the exe's bit
    //      since that port]. A docked externally built ship leaves the dock
    if (ship->staticinfo->externalBuild)
    {
        bitClear(ship->shipOrDerelict.specialFlags2,SPECIAL_2_CatBeingBuilt);
    }
    if (ship->staticinfo->externalBuild && ship->dockvars.dockship != NULL)
    {
        RemoveShipFromDocking(ship);
    }

    if (ship->rceffect != NULL)
    {
        // CAT: the repair effect, not HW1's harvest effect (stopRepairEffect at 0x00547dce)
        stopRepairEffect(ship);
    }

    // Check if launch manager is up, if it is then fix it update it on a ships death.

    if (lmActive)
    {
        // CAT: by class, CLASS_Carrier or CLASS_Mothership (0x00547de0), not by HW1's two ship types
        if ((ship->staticinfo->shipclass == CLASS_Carrier) || (ship->staticinfo->shipclass == CLASS_Mothership))
        {
            lmLaunchCapableShipDied(ship);
        }
    }

    tacticsShipDied(ship);

    if (ship->shipsInsideMe)
    {
        DeleteShipsInsideMe(ship);
    }

    // CAT-TODO(M2-R6 ships, the repair droids): cat_droidFreeArray(ship +0x4C0) 0x005c32b0. The droids are proxies that
    //      crawl over the hull and make welding effects; M2-R5 render ported their renderer (cat_droidRenderArray,
    //      RepairDroid.c), their create / update / free stay with the ship runtime: droidUpdateArray draws from
    //      random stream 0, which the oracle hashes, so they change the stream (RepairDroid.h). Ship +0x4C0 stays NULL.
    //      HW1's slave clean-up (dockDealWithDeadSlaveable) is gone: no Cataclysm ship is slaveable (SOF_Slaveable 0)

#if ETG_DISABLEABLE
    if (deathBy >= 0 && etgEffectsEnabled)
#else
    if (deathBy >= 0)
#endif
    {
        if (deathBy == EDT_AccumDamage)
        {
            battleShipDyingWithTimeToScream(ship);
        }
        // CAT: big ships use the death effects of their race's CLASS_Mothership row (CLASS_MonsterShip for dRKO and
        //      dNomadMoon) and create them with 10 parameters: a scale, a mode, the size, the speed and the
        //      collision box (0x00547e5d: switch on the static info's ship type)
        switch (ship->shiptype)
        {
            case sDreadnoughtCannon: case sMothership: case sMothershipOreCanArmour: case bMothership:
            case bSectMoship:
                etgLOD = etgDeathEventTable[ship->staticinfo->shiprace][CLASS_Mothership][deathBy];
                bigExplosion = TRUE;
                bigMode = 2;
                bigScale = 0.5f;
                break;
            case sCarrierSupport: case sMothershipSpecial: case sMothershipEngineering: case sMothershipArmour:
            case sMothershipMicro: case sMothershipSupport: case sMothershipCommand: case sMothershipEnginesLower:
            case sMothershipLowerSpine: case sMothershipBigGun: case bMothershipSupport:
                etgLOD = etgDeathEventTable[ship->staticinfo->shiprace][CLASS_Mothership][deathBy];
                bigExplosion = TRUE;
                bigMode = 1;
                bigScale = 0.4f;
                break;
            case sMothershipWeapons: case sMothershipDockingBay: case sMothershipEnginesUpper:
            case bMothershipDockingBay: case bMothershipEnginesLower: case trFloatingCity: case trGhostship:
                etgLOD = etgDeathEventTable[ship->staticinfo->shiprace][CLASS_Mothership][deathBy];
                bigExplosion = TRUE;
                bigMode = 3;
                bigScale = 0.75f;
                break;
            case dRKO: case dNomadMoon:
                etgLOD = etgDeathEventTable[ship->staticinfo->shiprace][CLASS_MonsterShip][deathBy];
                bigExplosion = TRUE;
                bigMode = 3;
                bigScale = 0.75f;
                break;
            default:
                etgLOD = etgDeathEventTable[((ShipStaticInfo *)ship->staticinfo)->shiprace]
                    [((ShipStaticInfo *)ship->staticinfo)->shipclass][deathBy];
                break;
        }
        if (etgLOD != NULL)
        {
            LOD = ship->obj.currentLOD;
            explosion = etgLOD->level[min(etgLOD->nLevels - 1, LOD)];
        }
        else
        {
            explosion = NULL;
        }
        if (explosion)
        {
            colSize = ship->staticinfo->common.staticCollInfo.collspheresize;
            colSize *= ship->magnitudeSquared;
            // CAT: the exe's software-renderer branch (RGL device 0: size and scale x0.75, mode one level down,
            //      0x00547f9b) cannot run here: this tree renders with GL only

            // CAT (M2-R5 layout-dock): the exe divides by the ship's top speed times its speed multiplier
            //      (cat_shipGetSpeedMultiplier 0x00514940: experience, upgrades, health, ram and Beast bonuses; 0x00548010)
            maxVelocity = cat_shipGetSpeedMultiplier(ship) * ship->staticinfo->common.maxvelocity;
            if (maxVelocity != 0.0f)
            {
                velMagnitude = fsqrt(vecMagnitudeSquared(ship->obj.posinfo.velocity));
                velMagnitude /= maxVelocity;
            }
            else
            {
                velMagnitude = 0;
            }
            colSizeDword = Real32ToUdword(colSize);
            velMagnitudeDword = Real32ToUdword(velMagnitude);

            if (bigExplosion)
            {
                coll = &ship->staticinfo->common.staticCollInfo;
                effect = etgEffectCreate(explosion, ship, NULL, NULL, NULL, 1.0f, 0, 10,
                                         SCALECAST(Real32ToUdword(bigScale)), SCALECAST(bigMode),
                                         SCALECAST(colSizeDword), SCALECAST(velMagnitudeDword),
                                         SCALECAST(Real32ToUdword(coll->collrectoffset.x)),     // statinfo +0x60
                                         SCALECAST(Real32ToUdword(coll->collrectoffset.y)),     // +0x64
                                         SCALECAST(Real32ToUdword(coll->collrectoffset.z)),     // +0x68
                                         SCALECAST(Real32ToUdword(coll->collrectoffset.x + coll->uplength)),    // +0x60 + +0x78
                                         SCALECAST(Real32ToUdword(coll->collrectoffset.y + coll->rightlength)));// +0x64 + +0x7C
            }
            else
            {
                //!!!??? should there be some EAF_ here???
                effect = etgEffectCreate(explosion, ship, NULL, NULL, NULL, 1.0f, 0, 2, SCALECAST(colSizeDword), SCALECAST(velMagnitudeDword));
            }
            bitSet(effect->header.flags, SOF_ForceVisible);
        }
    }

    //notify leader of a lost wingman
    if (ship->tacticstype == Evasive)
    {                                               //if in evasive tactics
        if (ship->attackvars.myLeaderIs != NULL)
        {                                           //if ship has a wingman
            bitSet(ship->attackvars.myLeaderIs->chatterBits, BCB_WingmanLost);
        }
    }

    // CAT: the bullet and missile references are cleared inline (0x005481a0..0x00548235): a beam whose owner died
    //      times out, and a missile that loses its target self-destructs within 5 seconds
    for (node = universe.BulletList.head; node != NULL; node = node->next)
    {
        bullet = (Bullet *)listGetStructOfNode(node);
        if (bullet->owner == ship)
        {
            bullet->owner = NULL;
            bullet->gunowner = NULL;
            if (bullet->bulletType == BULLET_Beam)
            {
                bullet->timelived += 10000.0f;                  // 0x0072a488
            }
        }
        if (&bullet->target->header == &ship->header)
        {
            bullet->target = NULL;
        }
    }
    for (node = universe.MissileList.head; node != NULL; node = node->next)
    {
        missile = (Missile *)listGetStructOfNode(node);
        if (missile->owner == ship)
        {
            missile->owner = NULL;
        }
        if (&missile->target->header == &ship->header)
        {
            missile->target = NULL;
            if (missile->totallifetime - missile->timelived > CAT_MISSILE_LOST_TARGET_LIFE)
            {
                missile->totallifetime = missile->timelived + CAT_MISSILE_LOST_TARGET_LIFE;
            }
        }
    }

    // CAT: smObjectDied is called whether or not the sensors manager is up (0x00548235)
    smObjectDied(ship);

    bobObjectDied((SpaceObj *)ship,&universe.collBlobList);
    pingObjectDied((SpaceObj *)ship);
    ship->obj.collMyBlob = NULL;

    // Update Ship Command Layer to reflect ship death
    clShipDied(&universe.mainCommandLayer,ship);

    // CAT: special ships drop their references here (HW1 did it before the sensors manager), and ships let go of
    //      clamps (0x0054826b..0x00548285): every object clamped to this ship, then the ship itself
    univRemoveAllShipReferencesFromSpecialShips(ship);
    cat_clampUnclampAllFromShip((SpaceObjRotImpTargGuidance *)ship);
    unClampObj((SpaceObjRotImpTargGuidance *)ship);

    // Update computer player structures to reflect ship death
    aiplayerShipDied(ship);

    // Update Camera focus stacks
    ccRemoveShip(&universe.mainCameraCommand,ship);

    // Tell single player game module ship died
    singlePlayerShipDied(ship);

    if(tutorial)
        tutPlayerShipDied(ship);

    // CAT: no death sound for a ship deleted on the spot (deathBy -1, 0x005482c7)
    if (deathBy != -1)
    {
        soundEventShipDied(ship);
    }

    IDToPtrTableObjDied(&ShipIDToPtr,ship->shipID.shipNumber);

    // CAT: the ship's entry is set to NULL rather than removed (cat_growSelectNullShip 0x00511b10 at 0x00548300), and
    //      the same for the deferred-update selection (universe +0x1A0, unk1 in the export, 0x005482f2) [M2-R5 layout-dock]
    cat_growSelectNullShip(&universe.catSelection1A0,ship);
    cat_growSelectNullShip(&universe.HousekeepShipList,ship);

    // Remove ships that are destroyed from any current selection
    clRemoveShipFromSelection((SelectCommand *)&selSelected,ship);
    clRemoveShipFromSelection((SelectCommand *)&selSelecting,ship);
    ioUpdateShipTotals();

    pieShipDied(ship);
    mouseClickShipDied(ship);

    // Remove ships that are destroyed from any hot key groups and play group death sound if all dead
    univRemoveShipFromHotkeyGroup(ship, TRUE);
}

void DisableAllPlayersShips(sdword playerIndex)
{
    Node *objnode = universe.ShipList.head;
    Ship *ship;
    Player *player;

    // CAT: only for a valid player index (DisableAllPlayersShips 0x00548450: playerIndex < 8)
    if (playerIndex >= MAX_MULTIPLAYER_PLAYERS)
    {
        return;
    }
    player = &universe.players[playerIndex];

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        if (ship->playerowner == player)
        {
            if(!(ship->header.flags & SOF_Disabled))
            {
                ship->header.flags |= SOF_Crazy;
                ship->deathtime = universe.totaltimeelapsed + frandombetween(PLAYERLOSE_SHIPDEATHTIMEMIN,PLAYERLOSE_SHIPDEATHTIMEMAX);
                // CAT: a mimic drops its disguise (DisableAllPlayersShips 0x00548450: cat_mimicUnmimic(ship, 0)) [M2-R4 ships-b]
                if (bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic))
                {
                    cat_mimicUnmimic(ship, FALSE);
                }
                // CAT: neither mothership careens (sMothership, bMothership; HW1 tested its Mothership only)
                if ((!singlePlayerGame) && (ship->shiptype != sMothership) && (ship->shiptype != bMothership)) ApplyCareenRotationDirectly(ship);
            }

            bitClear(ship->header.flags,SOF_Selectable);

            // Remove ships that are destroyed from any current selection
            clRemoveShipFromSelection((SelectCommand *)&selSelected,ship);

            // Remove ships that are destroyed from any hot key groups
            univRemoveShipFromHotkeyGroup(ship,FALSE);
        }

        objnode = objnode->next;
    }
    ioUpdateShipTotals();
    if (piePointSpecMode != PSM_Idle)
    {                                                       //make sure the movement mechanism is off
        piePointModeOnOff(PIE_SPEC_None, NULL);   // CAT: off, mode 0 as the exe
    }
}

void DisplayPlayerHasBeenDefeatedMessage(sdword playerIndex)
{
    char defeatmsg[100];

    strcpy(defeatmsg,playerNames[playerIndex]);
    strcat(defeatmsg," ");
    strcat(defeatmsg,strGetString(strPlayerHasDied));
    clCommandMessage(defeatmsg, 0xff0000ff);
}

void CheckPlayerWin(void)
{
    udword numPlayersAlive = 0;
    udword i;

    if (gatherStats|showStatsFight|showStatsFancyFight)
        return;     // just gathering stats, don't want to do the player win thing

    // CAT-TODO(stage 3/5 recorder): the exe also returns while the second recorder global 0x00a751a0 is set

    // CAT: the test is multiPlayerGame (CheckPlayerWin 0x005485e0; HW1 tested !singlePlayerGame). A game that is
    //      neither multiplayer nor single player (a skirmish against the computer) therefore ends only when no player
    //      is left alive, the stock 1.01 behaviour that CET's CheckPlayerWin_GameTypeCheck patch changes
    if (!multiPlayerGame)
    {
        if (!singlePlayerGame)
        {
            for (i=0;i<universe.numPlayers;i++)
            {
                if (universe.players[i].playerState == PLAYER_ALIVE)
                {
                    return;
                }
            }
        }
        else if (universe.players[0].playerState != PLAYER_DEAD)
        {
            return;
        }
        // end of game
        universe.quittime = universe.realtimeelapsed + WAIT_TIME_TO_QUIT;     // CAT: real time
        universe.wintime = universe.realtimeelapsed + WAIT_TIME_TO_WIN;
        return;
    }

    // check how many players are still alive
    for (i=0;i<universe.numPlayers;i++)
    {
        if (universe.players[i].playerState == PLAYER_ALIVE)
        {
            numPlayersAlive++;
        }
    }

    if (numPlayersAlive <= 1)
    {
        // end of game
        universe.wintime = universe.realtimeelapsed + WAIT_TIME_TO_WIN;      // CAT: real time (CheckPlayerWin 0x005485e0)
        universe.quittime = universe.realtimeelapsed + WAIT_TIME_TO_QUIT;
    }

    //check for allied victory
    if((!singlePlayerGame) && (tpGameCreated.flag & MG_AlliedVictory))
    {
        sdword ally = -1;
        bool32 alliedvictory = TRUE;
        //all living players on same team?
        //find allies of first player, make sure all other living players
        // are allies

        for (i=0; ((i<universe.numPlayers) && alliedvictory); i++)
        {
            if (universe.players[i].playerState == PLAYER_ALIVE)
            {
                if (ally == -1)
                {
                    //first player
                    ally = i;

                }
                else
                {
                    if ( !((1<<i) & universe.players[ally].Allies) )
                    {

                        alliedvictory = FALSE;
                    }
                }
            }
        }
        if (alliedvictory)
        {
            // end of game
            universe.quittime = universe.realtimeelapsed + WAIT_TIME_TO_QUIT;     // CAT: real time
            universe.wintime = universe.realtimeelapsed + WAIT_TIME_TO_WIN;
        }
    }
}

// CAT-TODO(stage 3/5): DeclarePlayerWin 0x005487f0 reports the results (cat_DeclarePlayerWinReportResults
//      0x00548750) and is twice HW1's size (multiplayer result screens).
void DeclarePlayerWin(void)
{
    udword numPlayersAlive = 0;
    udword i, count=0;
    char winmsg[200];

    if (singlePlayerGame)
    {
        return;
    }

    // check how many players are still alive
    for (i=0;i<universe.numPlayers;i++)
    {
        if (universe.players[i].playerState == PLAYER_ALIVE)
        {
            numPlayersAlive++;
        }
    }

    if (numPlayersAlive == 0)
    {
        bigmessageDisplay(strGetString(strAllPlayersDead),1);
    }
    else
    {
        sdword otherplayersdied = 0;
        sdword otherplayersdroppedout = 0;

        strcpy(winmsg,"");
        for (i=0;i<universe.numPlayers;i++)
        {
            if (universe.players[i].playerState == PLAYER_ALIVE)
            {
                strcat(winmsg,playerNames[i]);
                strcat(winmsg," ");
                count++;
                if ((numPlayersAlive-count)>1)
                {
                    strcat(winmsg, ", ");
                } else if ((numPlayersAlive-count)==1)
                {
                    strcat(winmsg, strGetString(strAnd));
                    strcat(winmsg, " ");
                }
            }
            else
            {
                if (i != universe.curPlayerIndex)
                {
                    otherplayersdied++;
                    if (universe.gameStats.playerStats[i].typeOfPlayerDeath == PLAYERKILLED_DROPPEDOUT)
                    {
                        otherplayersdroppedout++;
                    }
                }
            }
        }

        if ((otherplayersdroppedout > 0) && (otherplayersdroppedout == otherplayersdied))
        {
            bigmessageDisplay(strGetString(strAllOtherPlayersDroppedOut),1);
        }
        else
        {
            strcat(winmsg," ");
            if (numPlayersAlive > 1)
                strcat(winmsg,strGetString(strPlayersHaveWon));
            else
                strcat(winmsg,strGetString(strPlayerHasWon));
            bigmessageDisplay(winmsg,1);
        }
    }
}


/*-----------------------------------------------------------------------------
    Name        : univFindBackupMothership
    Description : Looks through the universeshiplist to find out if the
                  player who's mothership died has a backup mothership
    Inputs      : player - the player who's mothership just died
    Outputs     :
    Return      : TRUE if a backup was found
----------------------------------------------------------------------------*/
// CAT: the ships that can take over as a player's mothership: per race, the race's carrier or any CLASS_Mothership
//      ship (univFindBackupMothership 0x00548ad0: Sect race 0 sCarrier, Beast race 1 bCarrier; other races none)
static bool32 univIsBackupMothershipFor(Ship *ship, struct Player *player)
{
    if (((ship->header.flags & SOF_Dead) != 0) || (ship->playerowner != player) || (ship == player->PlayerMothership))
    {
        return FALSE;
    }
    if (player->race == RACE_Sect)
    {
        return (ship->shiptype == sCarrier) || (ship->staticinfo->shipclass == CLASS_Mothership);
    }
    if (player->race == RACE_Beast)
    {
        return (ship->shiptype == bCarrier) || (ship->staticinfo->shipclass == CLASS_Mothership);
    }
    return FALSE;
}

bool32 univFindBackupMothership(struct Player *player)
{
    Node *shipnode = universe.ShipList.head;
    Ship *ship;
    InsideShip *insideShip;

    while (shipnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(shipnode);

        if (univIsBackupMothershipFor(ship, player))
        {
            if (player->PlayerMothership != NULL)
            {
                // CAT: when the race's mothership (sMothership / bMothership) is lost: event 0x200101d
                //      (HW1 STAT_F_ReassignedToCarrier)
                if (gameIsRunning &&
                    (player->PlayerMothership->shiptype == ((player->race == RACE_Sect) ? sMothership : bMothership)))
                {
                    speechEventFleet(0x200101d, -1, player->playerIndex);
                }
            }
            player->PlayerMothership = ship;
            return TRUE;
        }

        shipnode = shipnode->next;
    }

    // CAT: in a multiplayer game with hyperspace on, a ship waiting in the hyperspace ether counts too
    //      (0x00548b0e: tpGameCreated.flag & MG_Hyperspace, singlePlayerGameInfo.ShipsInHyperspace; no speech)
    if (!singlePlayerGame && bitTest(tpGameCreated.flag, MG_Hyperspace))
    {
        for (shipnode = singlePlayerGameInfo.ShipsInHyperspace.head; shipnode != NULL; shipnode = shipnode->next)
        {
            insideShip = (InsideShip *)listGetStructOfNode(shipnode);
            if (univIsBackupMothershipFor(insideShip->ship, player))
            {
                player->PlayerMothership = insideShip->ship;
                return TRUE;
            }
        }
    }
    return FALSE;
}

// CAT-TODO(stage 3): univKillOtherPlayersIfDead 0x00548c70 closes the construction, launch and research
//      managers (cmCloseIfOpen, lmCloseIfOpen, rmCloseIfOpen), looks at the hyperspace ether too, and calls the
//      three-argument univKillPlayer.
void univKillOtherPlayersIfDead(Ship *ship)
{
    bool32 aplayerdied=FALSE;
    sdword i;
    //for (i=0;i<universe.numPlayers;i++)
    //{
    i = ship->playerowner->playerIndex;

        if (ship == universe.players[i].PlayerMothership)
        {
            if (singlePlayerGame)
            {
                if (i != 0)
                {
                    // player 1 of single player game just lost mothership - but don't kill player
                    universe.players[i].PlayerMothership = NULL;
                }
                else
                {
                    aplayerdied = TRUE;
                    univKillPlayer(i,PLAYERKILLED_STANDARDDEATH);
                }
            }
            else if (!univFindBackupMothership(&universe.players[i]))
            {
                if(tpGameCreated.flag & MG_LastMotherShip || universe.players[i].totalships == 0)
                {
                    aplayerdied = TRUE;
                    univKillPlayer(i,PLAYERKILLED_STANDARDDEATH);
                }
                else
                {
                    universe.players[i].PlayerMothership = NULL;
                }
            }
        }
        else if(universe.players[i].totalships == 0)
        {
            if(!singlePlayerGame)
            {
                //all players ships are dead...he's dead
                aplayerdied = TRUE;
                univKillPlayer(i,PLAYERKILLED_STANDARDDEATH);
            }
        }

    if (aplayerdied)
    {
        CheckPlayerWin();
    }
    //}
}

/*-----------------------------------------------------------------------------
    Name        : univDeleteDeadShips
    Description : deletes all dead ships
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univDeleteDeadShips()
{
    DeleteShip *deleteShip;
    Ship *ship;

    // CAT: the list is consumed from its head until it is empty, so ships added while it is being processed are
    //      handled too (univDeleteDeadShips 0x00548e00: listDeleteNode of the head each time; HW1 walked a saved
    //      next pointer and then deleted the whole list)
    while (universe.DeleteShipList.head != NULL)
    {
        deleteShip = (DeleteShip *)listGetStructOfNode(universe.DeleteShipList.head);
        ship = deleteShip->ship;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        univDeleteDeadShip(ship,deleteShip->deathBy);
        if (ship->impactable.impactablelink.belongto != NULL)
        {
            listRemoveNode(&ship->impactable.impactablelink);
        }

        // CAT: any CLASS_Resource ship carrying resources leaves a dust cloud (0x00548e3e: class 7, HW1 only the
        //      ResourceCollector type); the cloud joins the render list next to the ship if the ship is in it
        if (ship->staticinfo->shipclass == CLASS_Resource)
        {
            if (ship->resources > 0)
            {
                DustCloud *dustCloud = univAddDustCloud(DustCloud0,&ship->obj.posinfo.position);
                dustCloud->resource.resourcevalue = (sdword) (ship->resources * DUSTCLOUD_RCOLLECTOR_BLOWUP_SCALE);
                HandleDustCloudScaling(dustCloud);
                univAddObjToRenderListIf((SpaceObj *)dustCloud,(SpaceObj *)ship);     // add to render list if parent is in render list
            }
        }

        univKillOtherPlayersIfDead(ship);

        // don't totally delete ship, until after explosion
#if ETG_DISABLEABLE
        if (!etgEffectsEnabled)
        {
            univWipeShipOutOfExistence(ship);
        }
#endif

        listDeleteNode(universe.DeleteShipList.head);
    }
}

/*-----------------------------------------------------------------------------
    Name        : univReallyDeleteThisShipRightNow
    Description : Like you need to ask?  It kills it.  No effects, no melodrama,
                    no waiting around to delete it later.  Just BOOM!
    Inputs      : ship - ship to delete
    Outputs     :
    Return      : void
    Note        : do not call this while you are walking any ship-related
                    linked lists.
----------------------------------------------------------------------------*/
void univReallyDeleteThisShipRightNow(Ship *ship)
{
    univDeleteDeadShip(ship, -1);
    if (ship->impactable.impactablelink.belongto != NULL)
    {
        listRemoveNode(&ship->impactable.impactablelink);
    }
    univWipeShipOutOfExistence(ship);
}
/*-----------------------------------------------------------------------------
    Name        : DeleteDeadResource
    Description : Deletes an individual resource, and removes all references
                  to it.  Also frees its contents.
    Inputs      : resource
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void DeleteDeadResource(Resource *resource)
{
    // CAT: smObjectDied whether or not the sensors manager is up (univReallyDeleteThisResourceRightNow 0x005490d0,
    //      univDeleteDeadResources 0x00549270)
    smObjectDied(&resource->obj);

    ccRemoveShip(&universe.mainCameraCommand,(Ship *)resource);

    bobObjectDied(&resource->obj,&universe.collBlobList);
    pingObjectDied(&resource->obj);

    // Update Command Layer to reflect resource death
    univRemoveAllTargetReferencesFromBullets((SpaceObjRotImpTarg *)resource);
    univRemoveAllTargetReferencesFromMissiles((SpaceObjRotImpTarg *)resource);
    clResourceDied(&universe.mainCommandLayer,resource);
    aiplayerResourceDied(resource);

    IDToPtrTableObjDied(&ResourceIDToPtr,resource->resourceID.resourceNumber);

    // CAT (M2-R4 ships-b): ramming frigates drop the resource as a target (cat_univRemoveTargetFromRammingFrigates
    //      0x00549230); CAT-TODO(stage 6): KAS selections drop it (cat_kasShipRemovedFromSelections)
    cat_univRemoveTargetFromRammingFrigates(resource);
    univFreeResourceContents(resource);
}

/*-----------------------------------------------------------------------------
    Name        : univReallyDeleteThisResourceRightNow
    Description : Like you need to ask?  It kills it.  No effects, no melodrama,
                  no waiting around to delete it later.  Just BOOM!
    Inputs      : resource - resource to delete
    Outputs     :
    Return      : do not call this while you are walking any resource-related
                  linked lists.
----------------------------------------------------------------------------*/
void univReallyDeleteThisResourceRightNow(Resource *resource)
{
    DeleteDeadResource(resource);

    if ((resource->header.objtype == OBJ_AsteroidType) && (((Asteroid *)resource)->asteroidtype == Asteroid0))
    {
        univRemoveObjFromMinorRenderList(&resource->obj);
        listDeleteNode(&resource->header.objlink);
    }
    else
    {
        listRemoveNode(&resource->resourcelink);
        listRemoveNode(&resource->impactable.impactablelink);
        univRemoveObjFromRenderList(&resource->obj);
        listDeleteNode(&resource->header.objlink);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_univRemoveTargetFromRammingFrigates
    Description : CAT (M2-R4 ships-b): every ramming frigate drops obj as its target (cat_univRemoveTargetFromRammingFrigates
                  0x00549230: RammingFrigateRemoveShipReferences for sRammingFrigate and bRammingFrigate)
    Inputs      : obj - the resource (or ship) going away
----------------------------------------------------------------------------*/
void cat_univRemoveTargetFromRammingFrigates(void *obj)
{
    Node *node;
    Ship *ship;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if ((ship->shiptype == sRammingFrigate) || (ship->shiptype == bRammingFrigate))
        {
            RammingFrigateRemoveShipReferences(ship, (Ship *)obj);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_univDeleteMimicResource
    Description : CAT (M2-R4 ships-b): deletes the fake asteroid of a mimic (cat_univDeleteMimicResource 0x00548f40):
                  DeleteDeadResource's steps (the bullet/missile loops inlined), then off the resource list. The
                  object is in no other list (cat_univAddMimicResource).
    Inputs      : resource - the fake asteroid
----------------------------------------------------------------------------*/
void cat_univDeleteMimicResource(Resource *resource)
{
    smObjectDied(&resource->obj);
    ccRemoveShip(&universe.mainCameraCommand,(Ship *)resource);
    bobObjectDied(&resource->obj,&universe.collBlobList);
    pingObjectDied(&resource->obj);
    univRemoveAllTargetReferencesFromBullets((SpaceObjRotImpTarg *)resource);
    univRemoveAllTargetReferencesFromMissiles((SpaceObjRotImpTarg *)resource);
    clResourceDied(&universe.mainCommandLayer,resource);
    aiplayerResourceDied(resource);
    IDToPtrTableObjDied(&ResourceIDToPtr,resource->resourceID.resourceNumber);
    cat_univRemoveTargetFromRammingFrigates(resource);
    // CAT-TODO(stage 6): KAS selections drop it (cat_kasShipRemovedFromSelections)
    univFreeResourceContents(resource);
    listRemoveNode(&resource->resourcelink);
    cat_univRemoveTargetFromRammingFrigates(resource);
    // CAT-FIX: the exe never frees the object (its 0x1B4-byte block leaks on every unmimic); nothing refers to it now
    memFree(resource);
}

/*-----------------------------------------------------------------------------
    Name        : univDeleteDeadResources
    Description : deletes all dead resources
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univDeleteDeadResources()
{
    Node *objnode = universe.DeleteResourceList.head;
    DeleteResource *deleteResource;
    Resource *resource;

    if (objnode == NULL)
    {
        return;
    }

    do
    {
        deleteResource = (DeleteResource *)listGetStructOfNode(objnode);
        resource = deleteResource->resource;

        if (resource->resourcevalue == 0)
        {
            // resource was harvested away, so give opportunity for more to grow:
            if (resource->resourceVolume)
            {
                if (resource->resourceVolume->actualnumber > 0)
                {
                    resource->resourceVolume->actualnumber--;
                }
            }
        }

        // CAT (M2-R4 combat): the exe breaks every dying asteroid that is no killer (attribute bit 0x200), whatever
        //      its resource value (univDeleteDeadResources 0x00549270; BreakAsteroidUp in ResCollect.c)
        if ((resource->header.objtype == OBJ_AsteroidType) &&
            !(resource->obj.attributes & CAT_ATTRIBUTES_KillerCollision))
        {
            BreakAsteroidUp((Asteroid *)resource);
        }

        univReallyDeleteThisResourceRightNow(resource);

        objnode = objnode->next;

    } while (objnode != NULL);

    listDeleteAll(&universe.DeleteResourceList);
}

/*-----------------------------------------------------------------------------
    Name        : DeleteDeadDerelict
    Description : Deletes an individual derelict, and removes all references
                  to it.  Also frees its contents.
    Inputs      : derelict - derelict to delete
                  deathBy - how it died.  Used to create an effect.  -1 means no effect.
    Outputs     :
    Return      : TRUE if an effect for deleting the derelict was played
----------------------------------------------------------------------------*/
bool32 DeleteDeadDerelict(Derelict *derelict, sdword deathBy)
{
    Effect *effect;
    etgeffectstatic *explosion = NULL;
    etglod *etgLOD;
    sdword LOD;
    real32 colSize;
    udword colSizeDword, velMagnitudeDword;
    real32 velMagnitude;

    if (derelict->derelicttype == HyperspaceGate)
    {
        hsDerelictDied(derelict);
    }

    bobObjectDied((SpaceObj *)derelict,&universe.collBlobList);
    // CAT: smObjectDied whether or not the sensors manager is up (DeleteDeadDerelict 0x00549430)
    smObjectDied((SpaceObj *)derelict);

    /* shut off the derelicts sounds */
    soundEventDerelictRemove(derelict);

    // CAT: no HW1 salvage-corvette references to drop (the exe has no univRemoveAllDerelictReferencesFromSpecialShips
    //      call; HW1's salvage code goes with M2-R4 #3)
    pingObjectDied((SpaceObj *)derelict);
    ccRemoveShip(&universe.mainCameraCommand,(Ship *)derelict);
    // Update Command Layer to reflect derelict death
    univRemoveAllTargetReferencesFromBullets((SpaceObjRotImpTarg *)derelict);
    univRemoveAllTargetReferencesFromMissiles((SpaceObjRotImpTarg *)derelict);
    clDerelictDied(&universe.mainCommandLayer,derelict);

    IDToPtrTableObjDied(&DerelictIDToPtr,derelict->derelictID.derelictNumber);

#if ETG_DISABLEABLE
    if (deathBy >= 0 && etgEffectsEnabled)
#else
    if (deathBy >= 0)
#endif
    {                                                       //if we should create an effect
        etgLOD = etgDeathEventTableDerelict[derelict->derelicttype][deathBy];
        if (etgLOD != NULL)
        {
            LOD = derelict->obj.currentLOD;
            explosion = etgLOD->level[min(etgLOD->nLevels - 1, LOD)];
        }
        else
        {
            explosion = NULL;
        }
        if (explosion)
        {
            colSize = derelict->staticinfo->common.staticCollInfo.collspheresize;
            //colSize *= derelict->magnitudeSquared;

            velMagnitude = fsqrt(vecMagnitudeSquared(derelict->obj.posinfo.velocity));
            velMagnitude /= derelict->staticinfo->common.maxvelocity;
            colSizeDword = Real32ToUdword(colSize);
            velMagnitudeDword = Real32ToUdword(velMagnitude);
            //??? EAF_ flags ???
            effect = etgEffectCreate(explosion, (Ship *)derelict, NULL, NULL, NULL, 1.0f, 0, 2, SCALECAST(colSizeDword), SCALECAST(velMagnitudeDword));
            bitSet(effect->header.flags, SOF_ForceVisible);
        }
    }
    cat_univMeshStateFree(&derelict->obj);                  // CAT: 0x0054962a
    cat_univFreeEffectBlock(&derelict->obj);
    univFreeDerelictContents(derelict);
    listRemoveNode(&derelict->derelictlink);
    listRemoveNode(&derelict->impactable.impactablelink);
    derelict->derelictlink.belongto = NULL;
    derelict->impactable.impactablelink.belongto = NULL;
    univRemoveFromWorldList(derelict);                      //if it's a planet, remove it from the planet list
    return(explosion != NULL);
}

/*-----------------------------------------------------------------------------
    Name        : univReallyDeleteThisDerelictRightNow
    Description : Like you need to ask?  It kills it.  No effects, no melodrama,
                  no waiting around to delete it later.  Just BOOM!
    Inputs      : derelict - derelict to delete
    Outputs     :
    Return      : do not call this while you are walking any derelict-related
                  linked lists.
----------------------------------------------------------------------------*/
void univReallyDeleteThisDerelictRightNow(Derelict *derelict)
{
    bitSet(derelict->header.flags, SOF_Dead);
    DeleteDeadDerelict(derelict, -1);
//    listRemoveNode(&derelict->derelictlink);
//    listRemoveNode(&derelict->impactable.impactablelink);
    univRemoveObjFromRenderList((SpaceObj *)derelict);
//    univRemoveFromWorldList(derelict);                      //if it's a planet, remove it from the planet list
    listDeleteNode(&derelict->header.objlink);
}

/*-----------------------------------------------------------------------------
    Name        : univWipeDerelictOutOfExistance
    Description : This function deletes a derelict upon command of the death effect.
    Inputs      : derelict - derelict to go bye-bye
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void univWipeDerelictOutOfExistance(Derelict *derelict)
{
    bobObjectDied((SpaceObj *)derelict,&universe.collBlobList);
    nisObjectDied((SpaceObj *)derelict);
    if (derelict->derelictlink.belongto != NULL)
    {
        listRemoveNode(&derelict->derelictlink);
    }
    if (derelict->impactable.impactablelink.belongto != NULL)
    {
        listRemoveNode(&derelict->impactable.impactablelink);
    }
    univRemoveObjFromRenderList((SpaceObj *)derelict);
    etgShipDied((Ship *)derelict);
    univRemoveFromWorldList(derelict);                      //if it's a planet, remove it from the planet list
    listDeleteNode(&derelict->header.objlink);
}

/*-----------------------------------------------------------------------------
    Name        : univDeleteDeadDerelicts
    Description : deletes all dead derelicts
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univDeleteDeadDerelicts()
{
    Node *objnode = universe.DeleteDerelictList.head;
    DeleteDerelict *deleteDerelict;
    Derelict *derelict;

    if (objnode == NULL)
    {
        return;
    }

    do
    {
        deleteDerelict = (DeleteDerelict *)listGetStructOfNode(objnode);
        derelict = deleteDerelict->derelict;
        dbgAssertOrIgnore(derelict->header.objtype == OBJ_DerelictType);

        // don't totally delete ship, until after explosion
#if ETG_DISABLEABLE
        if (!etgEffectsEnabled)
        {
            univReallyDeleteThisDerelictRightNow(derelict);
        }
        else
#endif
        {
            if (!DeleteDeadDerelict(derelict, deleteDerelict->deathBy))
            {                                               //if there was no effect
                univRemoveObjFromRenderList((SpaceObj *)derelict);
                listDeleteNode(&derelict->header.objlink);         //finish deleting it properly
            }
        }

        objnode = objnode->next;

    } while (objnode != NULL);

    listDeleteAll(&universe.DeleteDerelictList);
}

/*-----------------------------------------------------------------------------
    Name        : DeleteDeadMissile
    Description : Deletes a dead missile, and removes all references to it.
                  Also frees it contents.
    Inputs      : deathBy - how the missile/mine died
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void DeleteDeadMissile(Missile *missile, sdword deathBy)
{
    etgeffectstatic *stat;
    sdword LOD;
    bool32 fatalHit;
    real32 floatDamage;
    udword intDamage;
    vector hitLocation;

    if (deathBy >= 0)
    {                                                       //if it was destroyed by forced attack
        if (missile->hitEffect != NULL)
        {                                                   //use the regular hit effect
            LOD = missile->obj.currentLOD;
            if (LOD < missile->hitEffect->nLevels)
            {
                stat = missile->hitEffect->level[LOD];
            }
            else
            {
                stat = NULL;
            }
        }
        else
        {
            stat = NULL;
        }
        if (univSpaceObjInRenderList((SpaceObj *)missile))
        {
#if ETG_DISABLEABLE
            if (stat != NULL && etgHitEffectsEnabled && etgEffectsEnabled && (!etgFrequencyExceeded(stat)))
#else
            if (stat != NULL && etgHitEffectsEnabled && (!etgFrequencyExceeded(stat)))
#endif
            {
                hitLocation = missile->obj.posinfo.position;

                floatDamage = missile->damage;
                intDamage = Real32ToUdword(floatDamage);
                fatalHit = FALSE;
                etgEffectCreate(stat, NULL, &hitLocation, &missile->obj.posinfo.velocity, &missile->rotatable.rotinfo.coordsys, 1.0, 0, 2, SCALECAST(intDamage), SCALECAST(fatalHit));
            }
        }
    }
    bobObjectDied((SpaceObj *)missile,&universe.collBlobList);
    pingObjectDied((SpaceObj *)missile);

    univRemoveAllTargetReferencesFromBullets((SpaceObjRotImpTarg *)missile);
    univRemoveAllTargetReferencesFromMissiles((SpaceObjRotImpTarg *)missile);
    nisRemoveMissileReference(missile);
    clMissileDied(&universe.mainCommandLayer,missile);

    IDToPtrTableObjDied(&MissileIDToPtr,missile->missileID.missileNumber);

    // CAT-TODO(stage 6): KAS selections drop the missile (cat_kasShipRemovedFromSelections)
    cat_univMeshStateFree(&missile->obj);                   // CAT: DeleteDeadMissile 0x00549880
    cat_univFreeEffectBlock(&missile->obj);
    univFreeMissileContents(missile);
    // CAT: and the sensors manager and the camera forget it (0x00549aaa)
    smObjectDied(&missile->obj);
    ccRemoveShip(&universe.mainCameraCommand,(Ship *)missile);
}

/*-----------------------------------------------------------------------------
    Name        : univDeleteDeadMissiles
    Description : deletes all dead missiles
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univDeleteDeadMissiles()
{
    Node *objnode = universe.DeleteMissileList.head;
    DeleteMissile *deleteMissile;
    Missile *missile;

    if (objnode == NULL)
    {
        return;
    }

    do
    {
        deleteMissile = (DeleteMissile *)listGetStructOfNode(objnode);
        missile = deleteMissile->missile;

        DeleteDeadMissile(missile, deleteMissile->deathBy);
        listRemoveNode(&missile->missilelink);
        listRemoveNode(&missile->impactable.impactablelink);
        univRemoveObjFromRenderList((SpaceObj *)missile);
        listDeleteNode(&missile->header.objlink);

        objnode = objnode->next;

    } while (objnode != NULL);

    listDeleteAll(&universe.DeleteMissileList);
}

/*-----------------------------------------------------------------------------
    Name        : univGetResourceStatistics
    Description : outputs the total resourceValue, numHarvestableResources, and numAsteroid0s in the universe
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univGetResourceStatistics(sdword *resourceValue,sdword *numHarvestableResources,sdword *numAsteroid0s)
{
    Node *objnode = universe.ResourceList.head;
    Resource *resource;
    sdword total = 0;
    sdword numharvestable = 0;
    sdword numasteroid0s = 0;

    while (objnode != NULL)
    {
        resource = (Resource *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(resource->header.flags & SOF_Resource);
        if (nisIsRunning && (resource->targetable.health >= REALlyBig))
        {
            goto nextnode;       // don't pick NIS resources
        }

        total += resource->resourcevalue;
        numharvestable++;

nextnode:
        objnode = objnode->next;
    }

    for (objnode = universe.MinorSpaceObjList.head;objnode != NULL;objnode = objnode->next)
    {
        resource = (Resource *)listGetStructOfNode(objnode);
        if ((resource->header.objtype == OBJ_AsteroidType) && (((Asteroid *)resource)->asteroidtype == Asteroid0))
        {
            numasteroid0s++;
        }
    }

    *resourceValue = total;
    *numHarvestableResources = numharvestable;
    *numAsteroid0s = numasteroid0s;
}

// CAT (M2-R5 hud): univObjectOutsideWorld 0x00549bb0 clips the zero vector from the object's position
//      (pieMovePointClipToLimits(&pos, &zero): the box clip moves its second argument) and compares the unchanged
//      position, so it always returns FALSE in the exe, as here
bool32 univObjectOutsideWorld(SpaceObj *obj)
{
    vector pos = obj->posinfo.position;
    vector oldpos = pos;
    vector zero = { 0.0f, 0.0f, 0.0f };

    pieMovePointClipToLimits(&pos, &zero);

    if (vecAreEqual(pos,oldpos))
    {
        return FALSE;
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : univFindNearestResource
    Description : finds the nearest resource to ship in optional volume specified by volumePosition and volumeRadius.
                  If VolumeRadius == 0, then volume is ignored
    Inputs      :
    Outputs     :
    Return      : returns the nearest resource (or NULL if none found)
----------------------------------------------------------------------------*/
// CAT-TODO(M2-R6 FOW): univFindNearestResource 0x00549c50 takes a fourth argument and checks the fog of war
//      (fowIsValidW, fowGetVisitedW on the FOW pointer at Player+0x6500), the player's technology (cat_rmTechSetHas
//      on Player+0x2A4) and stealth (0x0053f9f0) for an asteroid another player's ship holds, with a random pick, and
//      drops resources outside the movement limits (pieMovePointClipToLimits) instead of univObjectOutsideWorld.
//      It needs the Player layout (#5); M2-R4 #3 ported its callers (rescollectFindNearestResource 0x004f11f0).
Resource *univFindNearestResource(Ship *ship,real32 volumeRadius,vector *volumePosition)
{
    Node *objnode = universe.ResourceList.head;
    vector diff;
    real32 dist;
    real32 mindist = (real32)1e20;
    Resource *closestResource = NULL;
    Resource *resource;

    while (objnode != NULL)
    {
        resource = (Resource *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(resource->header.flags & SOF_Resource);
        if (nisIsRunning && (resource->targetable.health >= REALlyBig))
        {
            goto nextnode;       // don't pick NIS resources
        }

        if (ResourceMovingTooFast(resource))
        {
            goto nextnode;
        }

        if (ResourceAlreadyBeingHarvested(&universe.mainCommandLayer,NULL,resource))
        {
            goto nextnode;
        }

        if (resource->resourceNotAccessible)
        {
            goto nextnode;
        }

        if (volumeRadius > 0.0f)
        {
            vecSub(diff,resource->obj.posinfo.position,*volumePosition);

            if (!isBetweenInclusive(diff.x,-volumeRadius,volumeRadius))
            {
                goto nextnode;
            }

            if (!isBetweenInclusive(diff.y,-volumeRadius,volumeRadius))
            {
                goto nextnode;
            }

            if (!isBetweenInclusive(diff.z,-volumeRadius,volumeRadius))
            {
                goto nextnode;
            }
        }

        if (univObjectOutsideWorld(&resource->obj))
        {
            goto nextnode;
        }

        vecSub(diff,resource->obj.posinfo.position,ship->obj.posinfo.position);
        dist = vecMagnitudeSquared(diff);

        if (dist < mindist)
        {
            mindist = dist;
            closestResource = resource;
        }

nextnode:
        objnode = objnode->next;
    }

    return closestResource;
}

/*-----------------------------------------------------------------------------
    Name        : univGetChecksum
    Description : returns a checksum representing the status of the universe
    Inputs      :
    Outputs     : numShipsInChecksum
    Return      : checksum of mission sphere at this point in time
----------------------------------------------------------------------------*/
real32 univGetChecksum(sdword *numShipsInChecksum)
{
    Node *objnode = universe.ShipList.head;
    Ship *ship;
#if BINNETLOG
    Bullet   *bullet;
#endif
    sdword countShips = 0;

    real32 x=0.0f;
    real32 y=0.0f;
    real32 z=0.0f;

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        if ((ship->header.flags & SOF_Dead) == 0)
        {
            x += ship->obj.posinfo.position.x;
            y += ship->obj.posinfo.position.y;
            z += ship->obj.posinfo.position.z;

            if ((netlogfile) && (logEnable == LOG_VERBOSE))
            {
#if BINNETLOG
                binnetlogShipInfo bns;

                bns.header = makenetcheckHeader('S','S','S','S');
                bns.shipID = ship->shipID.shipNumber;
                bns.playerIndex = ship->playerowner->playerIndex;
                bns.shiprace = ship->shiprace;
                bns.shiptype = ship->shiptype;

                if (ship->command)
                {
                    bns.shiporder = ship->command->ordertype.order;
                    bns.shipattributes = ship->command->ordertype.attributes;
                }
                else
                {
                    bns.shiporder = -1;
                    bns.shipattributes = -1;
                }

                bns.tacticstype = ship->tacticstype;
                bns.isDodging = ship->isDodging;
                bns.DodgeDir = ship->DodgeDir;

                bns.health = ship->targetable.health;
                bns.x = ship->obj.posinfo.position.x;    bns.y = ship->obj.posinfo.position.y;    bns.z = ship->obj.posinfo.position.z;
                bns.vx = ship->obj.posinfo.velocity.x;  bns.vy = ship->obj.posinfo.velocity.y;    bns.vz = ship->obj.posinfo.velocity.z;
                bns.fuel = ship->fuel;

                fwrite(&bns, sizeof(bns), 1 ,netlogfile);
#else
                fprintf(netlogfile,"S:%d %d %d %d (%f %f %f) %f %d %d %d %d (%d %d)\n",ship->playerowner->playerIndex,ship->shiprace,ship->shiptype,
                        ship->shipID.shipNumber,ship->obj.posinfo.position.x,ship->obj.posinfo.position.y,ship->obj.posinfo.position.z,ship->fuel,ship->tacticstype,ship->isDodging,ship->DodgeDir,ship->tactics_ordertype,
                        (ship->command == NULL) ? -1 : ship->command->ordertype.order,
                        (ship->command == NULL) ? -1 : ship->command->ordertype.attributes);
#endif

                if ((ship->command) && (ship->command->ordertype.order == COMMAND_COLLECT_RESOURCES))
                {
                    CollectResourceCommand *collect = &ship->command->collect;
#if BINNETLOG
                    binnetlogShipResourceInfo bnr;
                    bnr.header = makenetcheckHeader('S','R','S','R');
                    bnr.resourceID = (collect->resource != NULL) ? collect->resource->resourceID.resourceNumber : -1;
                    bnr.volume = collect->resourceVolumeSize;
                    bnr.x = collect->resourceVolumePosition.x;
                    bnr.y = collect->resourceVolumePosition.y;
                    bnr.z = collect->resourceVolumePosition.z;
                    fwrite(&bnr, sizeof(bnr), 1 ,netlogfile);
#else
                    fprintf(netlogfile,"R:%d ",(collect->resource != NULL) ? collect->resource->resourceID.resourceNumber : -1,collect->resourceVolumeSize,
                            collect->resourceVolumePosition.x,collect->resourceVolumePosition.y,collect->resourceVolumePosition.z);
#endif
                }

                if (ship->dockInfo != NULL)
                {
                    DockInfo *dockInfo = ship->dockInfo;
                    sdword numDockPoints = ship->staticinfo->dockStaticInfo->numDockPoints;   // CAT (M2-R5 dock): the count is the static's
                    sdword i;
#if BINNETLOG
                    binnetlogShipDockInfo bnd;
                    udword orflag = 1;
                    bnd.header = makenetcheckHeader('S','D','S','D');
                    bnd.busyness = 0;                           // CAT: no busyness in the exe's DockInfo
                    bnd.numDockPoints = numDockPoints;
                    bnd.thisDockBusy = 0;
                    for (i=0;i<numDockPoints;i++)
                    {
                        if (dockInfo->thisDockBusy[i])
                        {
                            bnd.thisDockBusy |= orflag;
                        }
                        orflag <<= 1;
                    }
                    fwrite(&bnd, sizeof(bnd), 1 ,netlogfile);
#else
                    fprintf(netlogfile,"B:%d P:",0);
                    for (i=0;i<numDockPoints;i++)
                    {
                        fprintf(netlogfile," %d",dockInfo->thisDockBusy[i] ? 1 : 0);
                    }
                    fprintf(netlogfile,"\n");
#endif
                }
                if (ship->madBindings != NULL)
                {
#if BINNETLOG
                    binnetlogShipMadInfo bnm;
                    bnm.header = makenetcheckHeader('S','M','S','M');
                    bnm.info[0] = ship->madAnimationFlags;
                    bnm.info[1] = ship->madGunStatus;
                    bnm.info[2] = ship->madWingStatus;
                    bnm.info[3] = ship->madDoorStatus;
                    bnm.info[4] = ship->madSpecialStatus;
                    bnm.info[5] = ship->cuedAnimationIndex;
                    bnm.info[6] = ship->cuedAnimationType;
                    bnm.info[7] = 0;
                    bnm.info[8] = 0;
                    fwrite(&bnm, sizeof(bnm), 1 ,netlogfile);
#else
                    //                  flags,guns,wings,dock,special,index,type
                    fprintf(netlogfile,"M: %df %dg %dw %dd %ds %di %dt\n", ship->madAnimationFlags,ship->madGunStatus,ship->madWingStatus, ship->madDoorStatus, ship->madSpecialStatus,ship->cuedAnimationIndex,ship->cuedAnimationType);
#endif
                }
            }

            countShips++;
        }
        objnode = objnode->next;
    }

#ifdef HW_BUILD_FOR_DEBUGGING

#if BINNETLOG
    if ((netlogfile) && (logEnable == LOG_VERBOSE))
    {
        objnode = universe.BulletList.head;
        while (objnode != NULL)
        {
            bullet = (Bullet *)listGetStructOfNode(objnode);

            if ((!bitTest(bullet->header.flags, SOF_Dead)))
            {
#if BINNETLOG
                binnetlogBulletInfo bi;

                bi.header = makenetcheckHeader('B','B','B','B');
                bi.bullettype = bullet->bulletType;
                bi.bulletplayerowner = (bullet->playerowner!= NULL) ? bullet->playerowner->playerIndex : -1;
                bi.bulletowner = (bullet->owner != NULL) ? bullet->owner->shipID.shipNumber : -1;
                bi.x = bullet->obj.posinfo.position.x; bi.y = bullet->obj.posinfo.position.y; bi.z = bullet->obj.posinfo.position.z;
                bi.vx = bullet->obj.posinfo.velocity.x; bi.vy = bullet->obj.posinfo.velocity.y; bi.vz = bullet->obj.posinfo.velocity.z;
                bi.timelived = bullet->timelived;
                bi.totallifetime = bullet->totallifetime;
                bi.traveldist = bullet->traveldist;
                bi.damage = bullet->damage;
                bi.damageFull = bullet->damage;       // CAT (M2-R5 layout): HW1's damageFull is gone (exe +0x13C is catMoved)
                bi.DFGFieldEntryTime = bullet->DFGFieldEntryTime;
                bi.BulletSpeed = 0.0f;
                bi.collBlobSortDist = (bullet->obj.collMyBlob) ? bullet->obj.collMyBlob->sortDistance : 0.0f;
                fwrite(&bi,sizeof(bi),1,netlogfile);
#else
                if (bullet->playerowner!= NULL)
                    fprintf(netlogfile,"B: %d %d ", universe.univUpdateCounter, bullet->playerowner->playerIndex);
                else
                    fprintf(netlogfile,"B: %d N ", universe.univUpdateCounter);

                fprintf(netlogfile,"(%f %f %f) [%f %f] %f <%f %f> %f %f ",
                        bullet->obj.posinfo.position.x,
                        bullet->obj.posinfo.position.y,
                        bullet->obj.posinfo.position.z,
                        bullet->timelived,
                        bullet->totallifetime,
                        bullet->traveldist,
                        bullet->damage,
                        bullet->damage,                 // CAT (M2-R5 layout): HW1's damageFull is gone
                        bullet->DFGFieldEntryTime,
                        bullet->BulletSpeed);

                if (bullet->obj.collMyBlob)
                    fprintf(netlogfile, "%f\n", bullet->obj.collMyBlob->sortDistance);
                else
                    fprintf(netlogfile, "N\n");
#endif
            }

            objnode = objnode->next;
        }
    }
#endif

#if BINNETLOG
    if ((netlogfile) && (logEnable == LOG_VERBOSE))
    {
        clChecksum();
    }
#endif

    // CAT: the universe checksum sums live ships only (univGetChecksum 0x00549f50 has no derelict or resource loop;
    //      docs/m2-checksum-oracle.md 2.2). GoK summed derelicts and all resources here in debug builds only.

#endif

    *numShipsInChecksum = countShips;

    // CAT: fld z; fadd y; fadd x on the x87 stack, rounded to float once by the caller's fstp (univGetChecksum
    //      0x0054a210, netcheckShow 0x004cc411); at 53-bit precision control this is the double sum below
    return (real32)(((real64)z + (real64)y) + (real64)x);
}

/*-----------------------------------------------------------------------------
    Name        : univCalcShipChecksum
    Description : returns a checksum representing the status of the ships in the universe
    Inputs      :
    Outputs     : numShipsInChecksum
    Return      : checksum of mission sphere at this point in time
----------------------------------------------------------------------------*/
udword univCalcShipChecksum(void)
{
    udword  shipcheck=0;
    Node   *search;

    Bullet *bullet;

    /*
    Ship   *ship;
    udword  i;

    search = universe.ShipList.head;

    while (search!=NULL)
    {
        ship = listGetStructOfNode(search);
        if(ship->header.flags & SOF_Dead)
        {
            //don't consider dead ships...they're bad news
            //they still get written to the syncdump.txt, but
            //they won't generate a sync error.
            search = search->next;
            continue;
        }
        shipcheck += Real32ToUdword(ship->obj.collOptimizeDist);

        shipcheck += Real32ToUdword(ship->obj.posinfo.position.x);
        shipcheck += Real32ToUdword(ship->obj.posinfo.position.y);
        shipcheck += Real32ToUdword(ship->obj.posinfo.position.z);
        shipcheck += Real32ToUdword(ship->obj.posinfo.velocity.x);
        shipcheck += Real32ToUdword(ship->obj.posinfo.velocity.y);
        shipcheck += Real32ToUdword(ship->obj.posinfo.velocity.z);
        shipcheck += Real32ToUdword(ship->obj.posinfo.force.x);
        shipcheck += Real32ToUdword(ship->obj.posinfo.force.y);
        shipcheck += Real32ToUdword(ship->obj.posinfo.force.z);
        shipcheck += (udword)ship->obj.posinfo.isMoving;
        shipcheck += (udword)ship->obj.posinfo.haventCalculatedDist;

        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.coordsys.m11);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.coordsys.m21);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.coordsys.m31);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.coordsys.m12);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.coordsys.m22);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.coordsys.m32);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.coordsys.m13);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.coordsys.m23);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.coordsys.m33);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.rotspeed.x);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.rotspeed.y);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.rotspeed.z);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.torque.x);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.torque.y);
        shipcheck += Real32ToUdword(ship->rotatable.rotinfo.torque.z);

        shipcheck += Real32ToUdword(ship->impactable.collInfo.collOffset.x);
        shipcheck += Real32ToUdword(ship->impactable.collInfo.collOffset.y);
        shipcheck += Real32ToUdword(ship->impactable.collInfo.collOffset.z);
        shipcheck += Real32ToUdword(ship->impactable.collInfo.collPosition.x);
        shipcheck += Real32ToUdword(ship->impactable.collInfo.collPosition.y);
        shipcheck += Real32ToUdword(ship->impactable.collInfo.collPosition.z);

        for (i=0;i<8;i++)
        {
            shipcheck += Real32ToUdword(ship->impactable.collInfo.rectpos[i].x);
            shipcheck += Real32ToUdword(ship->impactable.collInfo.rectpos[i].y);
            shipcheck += Real32ToUdword(ship->impactable.collInfo.rectpos[i].z);
        }

        shipcheck += Real32ToUdword(ship->targetable.health);
        shipcheck += Real32ToUdword(ship->targetable.lasttimecollided);

        for (i=0;i<NUM_TRANS_DEGOFFREEDOM;i++)
            shipcheck += Real32ToUdword(ship->guidance.nonstatvars.thruststrength[i]);
        for (i=0;i<NUM_ROT_DEGOFFREEDOM;i++)
            shipcheck += Real32ToUdword(ship->guidance.nonstatvars.rotstrength[i]);
        for (i=0;i<NUM_TURN_TYPES;i++)
            shipcheck += Real32ToUdword(ship->guidance.nonstatvars.turnspeed[i]);

        shipcheck += (udword)ship->shipOrDerelict.aistate;
        shipcheck += ship->shipOrDerelict.specialFlags&(~(SPECIAL_Resourcing|SPECIAL_Finished_Resource));
        shipcheck += ship->shipOrDerelict.specialFlags2;

        for (i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
            shipcheck += ship->shipOrDerelict.salvageNumTagged[i];

        shipcheck += Real32ToUdword(ship->moveTo.x);
        shipcheck += Real32ToUdword(ship->moveTo.y);
        shipcheck += Real32ToUdword(ship->moveTo.z);
        shipcheck += Real32ToUdword(ship->moveFrom.x);
        shipcheck += Real32ToUdword(ship->moveFrom.y);
        shipcheck += Real32ToUdword(ship->moveFrom.z);
        shipcheck += Real32ToUdword(ship->formationOffset.x);
        shipcheck += Real32ToUdword(ship->formationOffset.y);
        shipcheck += Real32ToUdword(ship->formationOffset.z);
        shipcheck += Real32ToUdword(ship->formationHeading.x);
        shipcheck += Real32ToUdword(ship->formationHeading.y);
        shipcheck += Real32ToUdword(ship->formationHeading.z);
        shipcheck += Real32ToUdword(ship->engineOffset.x);
        shipcheck += Real32ToUdword(ship->engineOffset.y);
        shipcheck += Real32ToUdword(ship->engineOffset.z);
        shipcheck += Real32ToUdword(ship->enginePosition.x);
        shipcheck += Real32ToUdword(ship->enginePosition.y);
        shipcheck += Real32ToUdword(ship->enginePosition.z);

        shipcheck += Real32ToUdword(ship->rowOriginalPoint.x);
        shipcheck += Real32ToUdword(ship->rowOriginalPoint.y);
        shipcheck += Real32ToUdword(ship->rowOriginalPoint.z);
        shipcheck += Real32ToUdword(ship->rowOutOfWayPoint.x);
        shipcheck += Real32ToUdword(ship->rowOutOfWayPoint.y);
        shipcheck += Real32ToUdword(ship->rowOutOfWayPoint.z);

        shipcheck += (udword)ship->shiptype;
        shipcheck += (udword)ship->shiprace;
        shipcheck += (udword)ship->tacticstype;

        shipcheck += (udword)ship->isDodging;
        shipcheck += Real32ToUdword(ship->DodgeTime);
        shipcheck += (udword)ship->DodgeDir;
        shipcheck += (udword)ship->tacticsFormationVar1;
        shipcheck += ship->DodgeFlag;
        shipcheck += (udword)ship->tactics_ordertype;
        shipcheck += ship->tacticsTalk;
        shipcheck += (udword)ship->tacticsNeedToFlightMan;

        shipcheck += Real32ToUdword(ship->timeCreated);
        shipcheck += (udword)ship->kamikazeState;

        shipcheck += Real32ToUdword(ship->kamikazeVector.x);
        shipcheck += Real32ToUdword(ship->kamikazeVector.y);
        shipcheck += Real32ToUdword(ship->kamikazeVector.z);

        shipcheck += Real32ToUdword(ship->speedBurstTime);
        shipcheck += Real32ToUdword(ship->singlePlayerSpeedLimiter);
        shipcheck += Real32ToUdword(ship->deathtime);
        shipcheck += ship->madAnimationFlags;
        shipcheck += ship->madGunStatus;
        shipcheck += ship->madWingStatus;
        shipcheck += ship->madDoorStatus;
        shipcheck += ship->madSpecialStatus;
        shipcheck += ship->cuedAnimationIndex;
        shipcheck += ship->cuedAnimationType;
        shipcheck += ship->nextAnim;
        shipcheck += (udword)ship->forceCancelDock;
        shipcheck += (udword)ship->resources;
        shipcheck += Real32ToUdword(ship->fuel);
        shipcheck += Real32ToUdword(ship->lastTimeRepaired);

        shipcheck += (udword)ship->shipidle;
        shipcheck += (udword)ship->autostabilizeship;
        shipcheck += (udword)ship->autostabilizestate;
        shipcheck += (udword)ship->autostabilizewait;
        shipcheck += (udword)ship->aistateattack;
        shipcheck += (udword)ship->flightmanState1;
        shipcheck += (udword)ship->flightmanState2;
        shipcheck += (udword)ship->rcstate1;
        shipcheck += (udword)ship->rcstate2;
        shipcheck += (udword)ship->aistatecommand;
        shipcheck += (udword)ship->dontrotateever;
        shipcheck += (udword)ship->dontapplyforceever;
        shipcheck += (udword)ship->shipisattacking;
        shipcheck += (udword)ship->colorScheme;
        shipcheck += (udword)ship->recentlyAttacked;
        shipcheck += (udword)ship->recentlyFiredUpon;
        shipcheck += (udword)ship->rowState;
        shipcheck += Real32ToUdword(ship->aidescend);
        shipcheck += ship->flightman;

        shipcheck += ship->attackvars.flightmansLeft;
        shipcheck += ship->attackvars.attacksituation;
        shipcheck += ship->attackvars.attackevents;

        // CAT (M2-R5 dock): the exe's five dock words (HW1's destination and sub-states are gone). CAT-TODO(stage 5):
        //      univGetChecksum's Cataclysm body decides which of them it sums
        shipcheck += Real32ToUdword(ship->dockvars.catDockDistance);
        shipcheck += (udword)ship->dockvars.dockstate;
        shipcheck += (udword)ship->dockvars.catDockPointIndex;
        shipcheck += Real32ToUdword(ship->dockvars.catDockTime);

        shipcheck += (udword)ship->visibleToWho;
        shipcheck += (udword)ship->visibleToWhoPreviousFrame;
        shipcheck += (udword)ship->howDidIDie;
        shipcheck += (udword)ship->whoKilledMe;
        shipcheck += (udword)ship->nDamageLights;

        shipcheck += Real32ToUdword(ship->holdingPatternPos.x);
        shipcheck += Real32ToUdword(ship->holdingPatternPos.y);
        shipcheck += Real32ToUdword(ship->holdingPatternPos.z);
        shipcheck += Real32ToUdword(ship->damageModifier);

        shipcheck += ship->tractorbeaminfo;
        shipcheck += ship->tractorvector;

        shipcheck += (udword)ship->tractorstate;

        for (i=0;i<4;i++)
            shipcheck += (udword)ship->tractorPosinfo[i];

        shipcheck += (udword)ship->needNewLeader;
        shipcheck += (udword)ship->newDockIndexTransfer;

        shipcheck += Real32ToUdword(ship->shipDeCloakTime);
        shipcheck += (udword)ship->shipID.shipNumber;

        search = search->next;
    }
    */

    search = universe.BulletList.head;

    while (search!=NULL)
    {
        bullet = (Bullet *)listGetStructOfNode(search);
        if(bullet->header.flags & SOF_Dead)
        {
            search = search->next;
            continue;
        }

        //shipcheck += Real32ToUdword(bullet->obj.posinfo.position.x);
        //shipcheck += Real32ToUdword(bullet->obj.posinfo.position.y);
        //shipcheck += Real32ToUdword(bullet->obj.posinfo.position.z);

        //shipcheck += Real32ToUdword(bullet->obj.posinfo.velocity.x);
        //shipcheck += Real32ToUdword(bullet->obj.posinfo.velocity.y);
        //shipcheck += Real32ToUdword(bullet->obj.posinfo.velocity.z);
        //shipcheck += (udword)bullet->obj.posinfo.isMoving;
        //shipcheck += (udword)bullet->obj.posinfo.haventCalculatedDist;

        //shipcheck += Real32ToUdword(bullet->lengthmag);
        //shipcheck += Real32ToUdword(bullet->traveldist);
        //shipcheck += Real32ToUdword(bullet->timelived);
        //shipcheck += Real32ToUdword(bullet->totallifetime);
        //shipcheck += Real32ToUdword(bullet->damage);
        //shipcheck += Real32ToUdword(bullet->damageFull);
        //shipcheck += (udword)bullet->SpecialEffectFlag;
        //shipcheck += Real32ToUdword(bullet->DFGFieldEntryTime);
        //shipcheck += Real32ToUdword(bullet->BulletSpeed);
        if(bullet->owner != NULL)
        {
            shipcheck += 128;
        }
        //if(bullet->gunowner != NULL)
        //{
        //    shipcheck += 32;
        //}
        search = search->next;
    }

    return (shipcheck);
}

/*-----------------------------------------------------------------------------
    Name        : univMinorSetupShipForControl
    Description : A version of univSetupShipForControl specific to ships with
                    the SOF_DontApplyPhysics bit set (such as NIS ships).
    Inputs      : Ship - ship to "set up"
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univMinorSetupShipForControl(Ship *ship)
{
    sdword i;

    ship->gettingrocked = NULL;     // always make sure this gets set to NULL - don't want any bad references

    dmgShipThink(ship);

    //update engine trails
    if((universe.univUpdateCounter % UNIVERSE_UPDATE_RATE_FACTOR) == 0)
    {
        for (i = 0; i < MAX_NUM_TRAILS; i++)
        {
            if (ship->trail[i] != NULL)
            {
                trailUpdate(ship->trail[i], &ship->enginePosition);
            }
        }
    }
}

void univSetupShipForControl(Ship *ship)
{
    ShipStaticInfo *shipstatic;
    sdword i;

    ship->gettingrocked = NULL;     // always make sure this gets set to NULL - don't want any bad references

    if (ship->header.flags & SOF_Dead)
    {
        return;
    }

    shipstatic = (ShipStaticInfo *)ship->staticinfo;

    // CAT: no proximity-sensor cloak visibility pass here any more (univSetupShipForControl 0x0054a230 has no
    //      shipHasJustCloaked; Cataclysm's stealth is cat_shipStealthDetectionUpdate, M2-R4 #4)

    ship->shipidle = TRUE;
    ship->autostabilizeship = TRUE;

    //clear this flag incase it is a target of a salcapcorvette
    // CAT: specialFlags 0x08000000 in the exe (SPECIAL_SalvageTargetAlreadyDriven since the M2-R4 command port)
    bitClear(ship->shipOrDerelict.specialFlags,SPECIAL_SalvageTargetAlreadyDriven);   // CAT: specialFlags in the exe
    // CAT: the idle-Worker count goes too (Ship +0x600, 0x0054a26b) [M2-R4 ships-b]
    ship->catRepairIdleWorkers = 0;
    // memory of attackers fades away...
    if (ship->recentlyAttacked)
    {
        if (!ship->recentAttacker)
        {
            ship->recentlyAttacked = 0;
        }
        else
        {
            --ship->recentlyAttacked;
            if (!ship->recentlyAttacked)
                ship->recentAttacker = NULL;
        }
    }
    if (ship->recentlyFiredUpon)
    {
        if (ship->firingAtUs == NULL)
        {
            ship->recentlyFiredUpon = 0;
        }
        else
        {
            ship->recentlyFiredUpon--;
            if (ship->recentlyFiredUpon == 0)
            {
                ship->firingAtUs = NULL;
            }
        }
    }

    bitClear(ship->header.flags, SOF_DontDrawTrails);  // always clear this bit, SOF_Disabled ships never have trails drawn

    dmgShipThink(ship);

    //update engine trails
    if((universe.univUpdateCounter % UNIVERSE_UPDATE_RATE_FACTOR) == 0)
    {
        for (i = 0; i < MAX_NUM_TRAILS; i++)
        {
            if (ship->trail[i] != NULL)
            {
                trailUpdate(ship->trail[i], &ship->enginePosition);
            }
        }
    }

    // CAT: the HW1 self-repair is gone from here: Cataclysm repairs in the deferred update, and only Beast ships
    //      and ships with the self-repair upgrade (cat_univShipSelfRepair 0x005433d0)

    if (shipstatic->custshipheader.CustShipHousekeep != NULL)       // put in a list, and execute all housekeeps later
    {                                                               // to make sure all of the ships have been properly setup
        growSelectAddShip(&universe.HousekeepShipList,ship);
    }
    // CAT: every live ship gets a deferred update after the ship loop (0x0054a31f)
    growSelectAddShip(&universe.catSelection1A0,ship);
}

/*-----------------------------------------------------------------------------
    Name        : univSetupShipsForControl
    Description : initializes the shipidle and gettingrocked flags for this pass
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univSetupShipsForControl()
{
    Node *objnode = universe.ShipList.head;
    Ship *ship;
    SelectCommand *housekeepSelection;
    SelectCommand *deferredSelection;
    sdword i;
    sdword numShips;

    growSelectReset(&universe.HousekeepShipList);
    catDeferredShipListReset();                             // CAT: univSetupShipsForControl 0x0054a340

    while (objnode != NULL)
    {
        ship = (Ship *)listGetStructOfNode(objnode);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        univSetupShipForControl(ship);

        objnode = objnode->next;
    }

    // CAT: the deferred updates run before the housekeeps (0x0054a39e)
    deferredSelection = universe.catSelection1A0.selection;
    for (i = 0; i < deferredSelection->numShips; i++)
    {
        if (deferredSelection->ShipPtr[i] != NULL)
        {
            cat_univShipDeferredUpdate(deferredSelection->ShipPtr[i]);
        }
    }

    housekeepSelection = universe.HousekeepShipList.selection;
    numShips = housekeepSelection->numShips;

    for (i=0;i<numShips;i++)
    {
        ship = housekeepSelection->ShipPtr[i];
        if (ship)
        {
            ship->staticinfo->custshipheader.CustShipHousekeep(ship);
        }
    }

    growSelectReset(&universe.HousekeepShipList);       // don't need to keep track of them anymore
    growSelectReset(&universe.catSelection1A0);
}

// CAT: getEnemiesWithinProximity 0x0054a3f0 returns a static selection of at most 6 ships (0x00a6ebc8; the next call
//      reuses it, callers do not free it): the ships of thisship's blob that its owner can target
//      (cat_shipIsTargetableBy) and has detected (cat_shipDetectedByPlayer), no allies, no friendly or hyperspacing
//      ships, no disabled or hidden ships, no motherships, within the retaliate zone on each axis; a recently
//      decloaked ship is seen by chance (TW_*_SEE_*); NULL for a ship without a blob, a trFloatingCity or a ship with
//      SPECIAL_FriendlyStatus
#define CAT_ENEMIES_MAX     6
static struct
{
    sdword numShips;
    real32 timeLastStatus;
    Ship *ShipPtr[CAT_ENEMIES_MAX];
} catEnemiesWithinProximity;

SelectCommand *getEnemiesWithinProximity(Ship *thisship,real32 retaliateZone)
{
    Player *playerowner = thisship->playerowner;
    SelectCommand *blobships;
    udword num;
    udword objindex;
    sdword found = 0;
    Ship *ship;
    real32 negRetaliateZone = -retaliateZone;
    real32 timesincecloak;
    real32 dx, dy, dz;

    if (thisship->obj.collMyBlob == NULL)
    {
        return NULL;
    }
    blobships = thisship->obj.collMyBlob->blobShips;
    num = (udword)blobships->numShips;
    if ((thisship->shiptype == trFloatingCity) || bitTest(thisship->shipOrDerelict.specialFlags, SPECIAL_FriendlyStatus))
    {
        return NULL;      // traders get special status, not considered enemy to anyone
    }
    catEnemiesWithinProximity.numShips = 0;
    if (num == 0)
    {
        return NULL;
    }

    for (objindex = 0; objindex < num; objindex++)
    {
        if (found >= CAT_ENEMIES_MAX)
        {
            break;
        }
        ship = blobships->ShipPtr[objindex];

        if (!cat_shipIsTargetableBy(thisship->playerowner, ship) || allianceIsShipAlly(ship, playerowner) ||
            bitTest(ship->shipOrDerelict.specialFlags, SPECIAL_FriendlyStatus|SPECIAL_Hyperspacing) ||
            !cat_shipDetectedByPlayer(ship, thisship->playerowner->playerIndex))
        {
            continue;
        }
        timesincecloak = universe.totaltimeelapsed - ship->shipDeCloakTime;
        if (!(timesincecloak > TW_SECOND_SEE_TIME))
        {
            if (timesincecloak < TW_NOONE_SEE_TIME)
            {
                continue;       // decloaked so recently that thisship cannot see it yet
            }
            if (timesincecloak < TW_FIRST_SEE_TIME)
            {
                if ((ranRandom(RANDOM_GAME) % 101) > TW_FIRST_SEE_PERCENTAGE)
                {
                    continue;
                }
            }
            else if (timesincecloak < TW_SECOND_SEE_TIME)
            {
                if ((ranRandom(RANDOM_GAME) % 101) > TW_SECOND_SEE_PERCENTAGE)
                {
                    continue;
                }
            }
        }
        if (bitTest(ship->header.flags, SOF_Disabled|SOF_Hide) || (ship->shiptype == sMothership) ||
            (ship->shiptype == bMothership))
        {
            continue;
        }
        dx = ship->obj.posinfo.position.x - thisship->obj.posinfo.position.x;
        dy = ship->obj.posinfo.position.y - thisship->obj.posinfo.position.y;
        dz = ship->obj.posinfo.position.z - thisship->obj.posinfo.position.z;
        if ((negRetaliateZone < dx) && (dx < retaliateZone) && (negRetaliateZone < dy) && (dy < retaliateZone) &&
            (negRetaliateZone < dz) && (dz < retaliateZone))
        {
            catEnemiesWithinProximity.ShipPtr[found++] = ship;
        }
    }

    if (found == 0)
    {
        return NULL;
    }
    catEnemiesWithinProximity.numShips = found;
    return (SelectCommand *)&catEnemiesWithinProximity;
}

// CAT: univCheckShipState 0x0054a650 (M2-R5 command-tactics). The delayed death (deathtime) kills a grav-well
//      generator only when derelict (mode 2), a mimicking ship not at all, any other ship (mode 4; a mimic type that
//      does not mimic, mode 3), disabled or not. The out-of-world check is dead code in the exe: it clips the zero
//      vector instead of the ship's position (pieMovePointClipToLimits 0x004e76b0 moves its second argument) and
//      compares the unchanged position, so no ship is ever flown back into the world; it is left out. A ship
//      retaliates through cat_tacticsShipAttacked: an idle one when its attacker (gettingrocked) is no ally, or on
//      its passive-attack beat when it is selectable and neither fully cloaked nor mimicking; a moving or docking one
//      on its moving beat when it is a capital ship (static shipIsCapital +0x2A8). An idle resource collector
//      that is attacked docks to deposit unless aggressive. No tacticsUpdate (HW1's per-ship tactics are gone).
void univCheckShipState(Ship *ship)
{
    SelectCommand selectone;
    SelectCommand *attackers;
    CommandToDo *command;
    sdword mode;

    // CAT: the GoK base's guard against NaN positions is kept (the exe has none): such a ship would poison the blobs
    // kill ships with NaN positions
    // seems to happen on Mission 10 frequently preventing objective to be fullfilled as ships can't be found and killed
    // NOTE: what is causing the NaNs in the first place is unclear at this time.
    //       Since I encountered in mission 10 it might be the radiation effect?
    if ((ship->header.flags & SOF_Dead) == 0) {
      if (isnan(ship->obj.posinfo.position.x) || isnan(ship->obj.posinfo.position.y) || isnan(ship->obj.posinfo.position.z)) {
        dbgMessage("univCheckShipState(): Ship position contains NaN! Killing it.");
        ship->targetable.health = 0;
        AddShipToDeleteShipList(ship,-1,1);
      }
    }

    if (ship->deathtime != 0.0f)
    {
        if (ship->header.flags & SOF_Dead)
        {
            return;
        }
        if (universe.totaltimeelapsed >= ship->deathtime)
        {
            mode = 0;
            if ((ship->shiptype == kGravWellGenerator) || (ship->shiptype == tGravWellGenerator))
            {
                if (((GravWellGeneratorSpec *)ship->ShipSpecifics)->GravDerelict)
                {
                    mode = 2;
                }
            }
            else if (!bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic))
            {
                mode = 4;
            }
            else if (!cat_shipIsMimicking(ship))
            {
                mode = 3;
            }
            if (mode != 0)
            {
                // Self-destruct ship
                ship->targetable.health = 0;
                AddShipToDeleteShipList(ship,-1,mode);
            }
        }
    }

    if (ship->header.flags & SOF_Dead)
    {
        return;
    }

    if (ship->shipOrDerelict.specialFlags & SPECIAL_rowGettingOutOfWay)
    {
        rowFlyShipOutOfWay(ship);
        //ship->autostabilizeship = FALSE;
    }

    if (ship->autostabilizeship && !bitTest(ship->header.flags, SOF_Disabled|SOF_Hide|SOF_Hyperspace|SOF_NISShip|SOF_Crazy) && !bitTest(ship->obj.attributes,ATTRIBUTES_DontApplyFriction))
    {
        bool32 isShipSteady;

        if (ship->shiptype == trMiningBase)
            isShipSteady = aitrackIsShipSteadyAnywhere(ship);       // don't care about orientation up
        else
            isShipSteady = aitrackIsShipSteady(ship);

        if (isShipSteady)
        {
            aitrackForceSteadyShip(ship);
            ship->autostabilizestate = 0;
        }
        else
        {
            switch (ship->autostabilizestate)
            {
                case 0:
                    // we just detected ship isn't stable
                    ship->autostabilizewait = AUTOSTABILIZE_FRAME_KICK_IN;
                    ship->autostabilizestate = 1;

                case 1:
                    if (ship->autostabilizewait > 0)
                    {
                        ship->autostabilizewait--;
                        break;
                    }
                    else
                    {
                        ship->autostabilizestate = 2;
                    }

                case 2:
                    if (ship->shiptype == trMiningBase)
                        aitrackSteadyShipAnywhere(ship);
                    else
                        aitrackSteadyShip(ship);
                    break;

                default:
                    break;
            }
        }
    }

    if (nisIsRunning)
    {
        return;      // don't autoretaliate if NIS running
    }

    if (ship->shipidle)
    {
        if (bitTest(ship->header.flags, SOF_Disabled))
        {
            return;
        }
        if (cat_shipIsCollectingResources(ship))
        {
            if ((ship->gettingrocked != NULL) && (ship->tacticstype != Aggressive))
            {                                                   // tell the collector to run away
                selectone.numShips = 1;
                selectone.timeLastStatus = 0.0f;     // CAT-FIX: the exe leaves it uninitialised (0x0054aa9b)
                selectone.ShipPtr[0] = ship;
                clDock(&universe.mainCommandLayer,&selectone,CAT_DOCK_DEPOSIT_RESOURCES,NULL);
            }
            return;
        }
        if ((ship->gettingrocked != NULL) && !allianceIsShipAlly(ship,ship->gettingrocked->playerowner))
        {
            // attack ship->gettingrocked and its formation
            attackers = getShipAndItsFormation(&universe.mainCommandLayer,ship->gettingrocked);
            command = getShipAndItsCommand(&universe.mainCommandLayer,ship);
            cat_tacticsShipAttacked(ship,command,(AttackCommand *)attackers);
            memFree(attackers);     // CAT-FIX: the exe never frees getShipAndItsFormation's copy here (0x0054ab72)
            return;
        }
        if (UNIVERSE_WOODPECKER(CHECK_PASSIVE_ATTACK_RATE, ship->shipID.shipNumber & CHECK_PASSIVE_ATTACK_RATE) &&
            (ship->staticinfo->passiveRetaliateZone != 0.0f) && !cat_shipIsCloakedFully(ship) &&
            !(bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic) && cat_shipIsMimicking(ship)) &&
            bitTest(ship->header.flags, SOF_Selectable))
        {
            command = getShipAndItsCommand(&universe.mainCommandLayer,ship);
            attackers = getEnemiesWithinProximity(ship,ship->staticinfo->passiveRetaliateZone);
            cat_tacticsShipAttacked(ship,command,(AttackCommand *)attackers);
        }
        return;
    }

    //ship isn't idle
    //so lets do a check every so often to see if
    //ship should attempt passive attacking
    if (bitTest(ship->header.flags, SOF_Disabled))
    {
        return;
    }
    if (UNIVERSE_WOODPECKER(CHECK_PASSIVE_ATTACK_WHILEMOVING_RATE, ship->shipID.shipNumber & CHECK_PASSIVE_ATTACK_WHILEMOVING_RATE) &&
        (ship->staticinfo->passiveRetaliateZone != 0.0f) && ship->staticinfo->shipIsCapital)
    {
        command = getShipAndItsCommand(&universe.mainCommandLayer,ship);
        if ((command != NULL) &&
            ((command->ordertype.order == COMMAND_MOVE) || (command->ordertype.order == COMMAND_DOCK)) &&
            !cat_shipIsCloakedFully(ship) &&
            !(bitTest(ship->header.catFlags18, CAT_SHIPFLAG_Mimic) && cat_shipIsMimicking(ship)))
        {
            attackers = getEnemiesWithinProximity(ship,ship->staticinfo->passiveRetaliateZone);
            cat_tacticsShipAttacked(ship,command,(AttackCommand *)attackers);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : univupdateReset
    Description : resets univupdate
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univupdateReset()
{
    universe.phystimeelapsed = UNIVERSE_UPDATE_PERIOD;
    universe.totaltimeelapsed = 0.0f;
    universe.realtimeelapsed = 0.0f;                        // CAT: univUpdateReset 0x0054acd0
    universe.radius = 20000.0f;
    universe.univUpdateCounter = 0;
    universe.resourceNumber = 0;
    universe.derelictNumber = 0;
    universe.shipNumber = 0;
    universe.missileNumber = 0;
    univResetFastNetworkIDLookups();
    universe.quittime = 0.0f;
    universe.wintime = 0.0f;
    universe.aiplayerProcessing = FALSE;

    universe.CapitalShipCaptured = 0;
    universe.PlayerWhoWon = 0;

    universe.lasttimeadded = 0;
    universe.DerelictTech=FALSE;

    universe.crateTimer = 0.0f;
    universe.numCratesInWorld = 0;
    universe.bounties = FALSE;

    universe.collUpdateAllBlobs = FALSE;

    listInit(&universe.RenderList);
    listInit(&universe.MinorRenderList);
    universe.dontUpdateRenderList = FALSE;

    listInit(&universe.SpaceObjList);
    listInit(&universe.MinorSpaceObjList);
    listInit(&universe.effectList);
    listInit(&universe.ShipList);
    listInit(&universe.BulletList);
    listInit(&universe.ResourceList);
    listInit(&universe.DerelictList);
    listInit(&universe.ImpactableList);
    listInit(&universe.MissileList);
    listInit(&universe.MineFormationList);

    listInit(&universe.DeleteMissileList);
    listInit(&universe.DeleteResourceList);
    listInit(&universe.DeleteDerelictList);
    listInit(&universe.DeleteShipList);

    listInit(&universe.collBlobList);

    // tactics stuff
    // CAT: the one tactics list, the attack memory at universe+0x180; HW1's RetreatList is gone (Universe.h)
    listInit(&universe.AttackMemory);

    // regrow resources stuff
    listInit(&universe.ResourceVolumeList);

    growSelectClose(&universe.HousekeepShipList);
    growSelectInit(&universe.HousekeepShipList);
    // CAT: the second universe grow-selection is reset with HousekeepShipList (univUpdateReset 0x0054acd0)
    growSelectClose(&universe.catSelection1A0);
    growSelectInit(&universe.catSelection1A0);
    growSelectClose(&ClampedShipList);
    growSelectInit(&ClampedShipList);
    // CAT: the sensors manager's render-list hold is cleared (universe+0xB8, univUpdateReset 0x0054acd0)
    universe.catSensorsHoldRenderList = FALSE;

    // CAT: the per-player ship counters are cleared (univUpdateReset 0x0054acd0), and the overlay's mission timer
    //      restarts (cat_missionTimerReset 0x004acb80) [M2-R5 hud]
    catShipCountersReset();
    cat_missionTimerReset();
    catShipCounterA = 0;
    catUnivFrameStep = FALSE;
    catProtectedSpeedLimitTime = 0.0f;
}

/*-----------------------------------------------------------------------------
    Name        : univupdateInit
    Description : intializes univupdate
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univupdateInit()
{
    univInitFastNetworkIDLookups();
    growSelectInit(&universe.HousekeepShipList);
    growSelectInit(&ClampedShipList);

    universe.bountySize = 0;
    univupdateReset();
    universe.star3dinfo = star3dInit(NUM_STARS, universe.radius - 1.0f, universe.radius);
    scriptSet(NULL,"Galaxy.script",GalaxyTweaks);
    universe.backgroundColor = backgroundColor;
    // CAT: and clears the per-player ship counters again (univupdateInit 0x0054af40)
    catShipCountersReset();
    catShipCounterA = 0;
    catProtectedSpeedLimitTime = 0.0f;
}

/*-----------------------------------------------------------------------------
    Name        : univFreeEffectContents
    Description : frees the effect (for exiting the program)
    Inputs      : effect
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univFreeEffectContents(Effect *effect)
{
    etgEffectDelete(effect);
}

/*-----------------------------------------------------------------------------
    Name        : univFreeDerelictContents
    Description : Frees the contents of a derelict without freeing the derelict itself.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univFreeDerelictContents(Derelict *derelict)
{
    if (derelict->shipOrDerelict.navLightInfo)
    {
        memFree(derelict->shipOrDerelict.navLightInfo);
        derelict->shipOrDerelict.navLightInfo = NULL;
    }
    if(derelict->guidance.clampInfo != NULL)
    {
        memFree(derelict->guidance.clampInfo);
        derelict->guidance.clampInfo = NULL;
    }

}

/*-----------------------------------------------------------------------------
    Name        : univFreeAsteroidContents
    Description : frees the asteroid contents
    Inputs      : asteroid
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univFreeAsteroidContents(Asteroid *asteroid)
{
    if (asteroid->header.flags & SOF_StaticInfoIsDynamic)
    {
        memFree(asteroid->staticinfo);
    }
}

void univFreeNebulaContents(Nebula *nebula)
{
    if (nebula->stub != NULL)
    {
        nebDeleteChunk((nebChunk*)nebula->stub);
        nebula->stub = NULL;
    }

    if (nebula->header.flags & SOF_StaticInfoIsDynamic)
    {
        memFree(nebula->staticinfo);
    }
}

void univFreeGasCloudContents(GasCloud *gascloud)
{
    if (gascloud->header.flags & SOF_StaticInfoIsDynamic)
    {
        memFree(gascloud->staticinfo);
    }
}

void univFreeDustCloudContents(DustCloud *dustcloud)
{
    if (dustcloud->stub != NULL)
    {
        cloudDeleteSystem((cloudSystem*)dustcloud->stub);
        dustcloud->stub = NULL;
    }

    if (dustcloud->header.flags & SOF_StaticInfoIsDynamic)
    {
        memFree(dustcloud->staticinfo);
    }
}

void univFreeResourceContents(Resource *resource)
{
    // CAT: every resource type frees its mesh state and its +0x84 block first (univFreeResourceContents 0x0054b030
    //      calls cat_univMeshStateFree before the switch and again in each case; the second call finds NULL)
    cat_univMeshStateFree(&resource->obj);
    cat_univFreeEffectBlock(&resource->obj);
    switch (resource->header.objtype)
    {
        case OBJ_AsteroidType:
            univFreeAsteroidContents((Asteroid *)resource);
            break;

        case OBJ_NebulaType:
            univFreeNebulaContents((Nebula *)resource);
            break;

        case OBJ_GasType:
            univFreeGasCloudContents((GasCloud *)resource);
            break;

        case OBJ_DustType:
            univFreeDustCloudContents((DustCloud *)resource);
            break;
        default:
            break;
    }
}

/*-----------------------------------------------------------------------------
    Name        : univFreeBulletContents
    Description : frees the bullet (for exiting program)
    Inputs      : bullet
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univFreeBulletContents(Bullet *bullet)
{
    // CAT: ships drop their references to the bullet, then its mesh state and +0x84 block go (univFreeBulletContents
    //      0x0054b1d0; the same loop is cat_univRemoveBulletReferencesFromShips 0x0054b1a0)
    cat_univRemoveBulletReferencesFromShips(bullet);
    cat_univMeshStateFree(&bullet->obj);
    cat_univFreeEffectBlock(&bullet->obj);
}

/*-----------------------------------------------------------------------------
    Name        : cat_univRemoveBulletReferencesFromShips (Cataclysm 0x0054b1a0)
    Description : Calls every ship's CustShipBulletRemoval (CustShipHeader slot 14) for a bullet that is going away
                  (HW1 only told the DefenseFighters, from univUpdateAllPosVelBullets).
    Inputs      : bullet - the bullet
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void cat_univRemoveBulletReferencesFromShips(Bullet *bullet)
{
    Node *node;
    Ship *ship;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (ship->staticinfo->custshipheader.CustShipBulletRemoval != NULL)
        {
            ship->staticinfo->custshipheader.CustShipBulletRemoval(ship, (struct Bullet *)bullet);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : univFreeMissileContents
    Description : frees the missile (for exiting program)
    Inputs      : missile
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univFreeMissileContents(Missile *missile)
{
    if(missile->formationinfo != NULL)
    {
        listRemoveNode(&missile->formationLink);     //take out of formation info list
    }
    if(missile->trail != NULL)
    {
        mistrailDelete(missile->trail);
        missile->trail = NULL;
    }
    if(missile->guidance.clampInfo != NULL)
    {
        memFree(missile->guidance.clampInfo);
        missile->guidance.clampInfo = NULL;
    }
}

/*-----------------------------------------------------------------------------
    Name        : univRemoveShipReferences
    Description : Calls functions to remove references to ships.  This will
                    always be called no matter how a ship dies.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univRemoveShipReferences(Ship *ship)
{
    Node *node;
    Bullet *bullet;
    Missile *missile;

    ship->header.flags |= SOF_Dead;
    bitClear(ship->header.flags,SOF_Selectable);

    //hopefully, at this point, the ships howDidIDied variable is set and valid!
    gameStatsShipDied(ship);

    // CAT: a factory is removed unless it is an enemy's in single player (univRemoveShipReferences 0x0054b230)
    if ((((ShipStaticInfo *)(ship->staticinfo))->canBuildShips) &&
        (!singlePlayerGame || ship->playerowner->playerIndex == 0))
    {
        cmRemoveFactory(ship);
    }

    if (ship->rceffect != NULL)
    {
        stopRepairEffect(ship);                 // CAT: the repair effect, not HW1's harvest effect
    }

    // CAT: no CustShipDied here any more (HW1 called it again unless the game was ending with a research ship);
    //      the exe calls it once, from univDeleteDeadShip 0x00547d00

    tacticsShipDied(ship);

    if (ship->shipsInsideMe)
    {
        DeleteShipsInsideMe(ship);
    }

    // CAT: HW1's slave clean-up is gone (no Cataclysm ship is slaveable). The bullet and missile references are
    //      cleared inline, as in univDeleteDeadShip: a beam whose owner is gone times out, a missile that loses its
    //      target self-destructs within 5 seconds (0x0054b2f4..0x0054b39b)
    for (node = universe.BulletList.head; node != NULL; node = node->next)
    {
        bullet = (Bullet *)listGetStructOfNode(node);
        if (bullet->owner == ship)
        {
            bullet->owner = NULL;
            bullet->gunowner = NULL;
            if (bullet->bulletType == BULLET_Beam)
            {
                bullet->timelived += 10000.0f;      // 0x0072a488
            }
        }
        if (&bullet->target->header == &ship->header)
        {
            bullet->target = NULL;
        }
    }
    for (node = universe.MissileList.head; node != NULL; node = node->next)
    {
        missile = (Missile *)listGetStructOfNode(node);
        if (missile->owner == ship)
        {
            missile->owner = NULL;
        }
        if (&missile->target->header == &ship->header)
        {
            missile->target = NULL;
            if (missile->totallifetime - missile->timelived > CAT_MISSILE_LOST_TARGET_LIFE)
            {
                missile->totallifetime = missile->timelived + CAT_MISSILE_LOST_TARGET_LIFE;
            }
        }
    }

    // Update Ship Command Layer to reflect ship death
    clShipDied(&universe.mainCommandLayer,ship);

    univRemoveAllShipReferencesFromSpecialShips(ship);

    // CAT: clamps are released (cat_clampUnclampAllFromShip 0x00434900 for every object clamped to this ship, then
    //      the ship itself is unclamped); smObjectDied runs whether or not the sensors manager is up and the camera
    //      command forgets the ship (0x0054b3a3..0x0054b3c6)
    cat_clampUnclampAllFromShip((SpaceObjRotImpTargGuidance *)ship);
    unClampObj((SpaceObjRotImpTargGuidance *)ship);
    smObjectDied(ship);
    ccRemoveShip(&universe.mainCameraCommand,ship);

    bobObjectDied((SpaceObj *)ship,&universe.collBlobList);
    pingObjectDied((SpaceObj *)ship);

    // Update computer player structures to reflect ship death
    aiplayerShipDied(ship);

    // Tell single player game module ship died
    singlePlayerShipDied(ship);

    if(tutorial)
        tutPlayerShipDied(ship);

    // Tell the sound layer that the ship died
//    soundEventShipDied(ship);
//  06/23/99 SA - I changed the above line because I think it'll only be called when
//  a ship is retired, this was causing it to say "ship lost" when it was retired.
//  the soundEventShipRemove() doesn't say the ship died speech event
    soundEventShipRemove(ship);

    IDToPtrTableObjDied(&ShipIDToPtr,ship->shipID.shipNumber);

    // CAT: the entry is set to NULL rather than removed (cat_growSelectNullShip 0x00511b10), in the deferred-update
    //      selection too (universe +0x1A0, unk1 in the export) [M2-R5 layout-dock]
    cat_growSelectNullShip(&universe.catSelection1A0,ship);
    cat_growSelectNullShip(&universe.HousekeepShipList,ship);

    // Remove ships that are destroyed from any hot key groups
    univRemoveShipFromHotkeyGroup(ship,FALSE);

    // CAT (M2-R5 hud): the ship gives its name back after the hot-key groups (cat_shipNamesRelease 0x0050f660)
    cat_shipNamesRelease(ship);
    mrShipDied(ship);
    // CAT-TODO(M2-R6 popup menu): cat_popupMenuRemoveShip 0x004ef840 follows mrShipDied (the right-click menu is not ported)
    // CAT (M2-R5 hud): the info overlay forgets the ship (cat_ioRemoveShipReferences, univRemoveShipReferences)
    cat_ioRemoveShipReferences(ship);
}

/*-----------------------------------------------------------------------------
    Name        : univFreeShipContents
    Description : frees the ship (for exiting program)
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univFreeShipContents(Ship *ship)
{
    udword i;

    // CAT: the ship's per-vertex mesh state and +0x84 block go first (univFreeShipContents 0x0054b550).
    //      CAT-TODO(M2-R6 ConsMgr): cat_suShipRemoved 0x00459ae0 (support units) comes before them in the exe
    cat_univMeshStateFree(&ship->obj);
    cat_univFreeEffectBlock(&ship->obj);

    univRemoveShipReferences(ship);

    if (ship->attackvars.multipleAttackTargets != NULL)
    {
        memFree(ship->attackvars.multipleAttackTargets);
        ship->attackvars.multipleAttackTargets = NULL;
    }

    if (ship->flightman != FLIGHTMAN_NULL)
    {
        flightmanClose(ship);
    }

    if (ship->shipSinglePlayerGameInfo)
    {
        memFree(ship->shipSinglePlayerGameInfo);
        ship->shipSinglePlayerGameInfo = NULL;
    }

    // Remove repair droids...
/*    if(ship->pRepairDroids)
    {
        memFree(ship->pRepairDroids);
        ship->pRepairDroids = NULL;
    }*/

    if (ship->shipOrDerelict.navLightInfo)
    {
        memFree(ship->shipOrDerelict.navLightInfo);
        ship->shipOrDerelict.navLightInfo = NULL;
    }
    if(ship->guidance.clampInfo != NULL)
    {
        memFree(ship->guidance.clampInfo);
        ship->guidance.clampInfo = NULL;
    }
    //remove lightnings
    for (i = 0; i < 2; i++)
    {
        if (ship->lightning[i] != NULL)
        {
            cloudKillLightning((lightning*)ship->lightning[i]);
            ship->lightning[i] = NULL;
        }
    }

    dmgStopEffect(ship, DMG_All);
    dmgClearLights(ship);

    for (i = 0; i < MAX_NUM_TRAILS; i++)
    {
        if (ship->trail[i] != NULL)
        {
            trailDelete(ship->trail[i]);
            ship->trail[i] = NULL;
        }
    }

    if (ship->gunInfo != NULL)
    {
        memFree(ship->gunInfo);
        ship->gunInfo = NULL;
    }

    if (ship->dockInfo != NULL)
    {
        memFree(ship->dockInfo);
        ship->dockInfo = NULL;
    }

    if (ship->shipsInsideMe)
    {
        FreeShipsInsideMe(ship);
    }

    if (((ShipStaticInfo *)ship->staticinfo)->custshipheader.CustShipClose)
    {
        ((ShipStaticInfo *)ship->staticinfo)->custshipheader.CustShipClose(ship);
    }
    univMeshBindingsDelete(ship);
    if (ship->header.flags & SOF_StaticInfoIsDynamic)
    {
        memFree(ship->staticinfo);
    }
}


/*-----------------------------------------------------------------------------
    Name        : univupdateCloseAllObjectsAndMissionSpheres
    Description : closes all objects in the universe and all mission spheres in the universe
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univupdateCloseAllObjectsAndMissionSpheres()
{
    Node *objnode = universe.SpaceObjList.head;
    SpaceObj *obj;
    sdword i;

    univCloseFastNetworkIDLookups();
    growSelectClose(&universe.HousekeepShipList);
    growSelectClose(&universe.catSelection1A0);             // CAT: the deferred-update list (unk1 in the export)
    growSelectClose(&ClampedShipList);

    gameIsEnding = TRUE;    //set global flag to indicate to other functions that the game is ending

    singlePlayerGameCleanup();

    while (objnode != NULL)
    {
        obj = (SpaceObj *)listGetStructOfNode(objnode);

        // CAT: every object type but ships (univFreeShipContents) and effects (etgEffectDelete) frees its mesh state
        //      and +0x84 block here (univupdateCloseAllObjectsAndMissionSpheres 0x0054b780)
        if (obj->header.objtype != OBJ_ShipType && obj->header.objtype != OBJ_EffectType)
        {
            cat_univMeshStateFree(obj);
            cat_univFreeEffectBlock(obj);
        }

        switch (obj->header.objtype)
        {
            case OBJ_ShipType:
                ((Ship *)obj)->howDidIDie = DEATH_BY_GAME_END;
                univFreeShipContents((Ship *)obj);
                break;

            case OBJ_BulletType:
                univFreeBulletContents((Bullet *)obj);
                break;

            case OBJ_AsteroidType:
                univFreeAsteroidContents((Asteroid *)obj);
                break;

            case OBJ_NebulaType:
                univFreeNebulaContents((Nebula *)obj);
                break;

            case OBJ_GasType:
                univFreeGasCloudContents((GasCloud *)obj);
                break;

            case OBJ_DustType:
                univFreeDustCloudContents((DustCloud *)obj);
                break;

            case OBJ_DerelictType:
                univFreeDerelictContents((Derelict *)obj);
                break;

            case OBJ_EffectType:
                univFreeEffectContents((Effect *)obj);
                break;

            case OBJ_MissileType:
                univFreeMissileContents((Missile *)obj);
                break;

            default:
                dbgAssertOrIgnore(FALSE);
                break;
        }
        univRemoveObjFromRenderList(obj);

        objnode = objnode->next;
    }

    objnode = universe.MinorSpaceObjList.head;
    while (objnode != NULL)
    {
        obj = (SpaceObj *)listGetStructOfNode(objnode);

        switch (obj->header.objtype)
        {
            case OBJ_AsteroidType:
                cat_univMeshStateFree(obj);                 // CAT: 0x0054b7d4
                cat_univFreeEffectBlock(obj);
                univFreeAsteroidContents((Asteroid *)obj);
                break;

            default:
                dbgAssertOrIgnore(FALSE);
                break;
        }
        univRemoveObjFromMinorRenderList(obj);

        objnode = objnode->next;
    }

    listDeleteAll(&universe.SpaceObjList);
    listDeleteAll(&universe.MinorSpaceObjList);

    listInit(&universe.effectList);
    listInit(&universe.ShipList);
    listInit(&universe.BulletList);
    listInit(&universe.ResourceList);
    listInit(&universe.DerelictList);
    listInit(&universe.ImpactableList);
    listInit(&universe.MissileList);

    bobListDelete(&universe.collBlobList);

    listDeleteAll(&universe.MineFormationList);
    listDeleteAll(&universe.ResourceVolumeList);

    // deselect everything as well
    selSelected.numShips = 0;
    selSelecting.numTargets = 0;

    if (!singlePlayerGameLoadNewLevelFlag)
    {
        for (i=0;i<SEL_NumberHotKeyGroups;i++)      // clear hotkey groups if not loading a new level
        {
            selHotKeyGroup[i].numShips = 0;
        }

        growSelectClose(&singlePlayerGameInfo.ShipsToHyperspace);
    }
    gameIsEnding=FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : univupdateClose
    Description : closes a mission
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univupdateClose()
{
    univupdateCloseAllObjectsAndMissionSpheres();

    star3dClose(universe.star3dinfo);
    universe.star3dinfo = NULL;
}

/*-----------------------------------------------------------------------------
    Name        : univRegrowAsteroids
    Description : regrows an asteroid
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univRegrowAsteroids()
{
    ;       // complete later, regrowing asteroids properly
}

/*-----------------------------------------------------------------------------
    Name        : univSpaceObjInRenderList
    Description : returns TRUE if the SpaceObj is in the render list
    Inputs      :
    Outputs     :
    Return      : returns TRUE if the SpaceObj is in the render list
----------------------------------------------------------------------------*/
bool32 univSpaceObjInRenderList(SpaceObj *spaceobj)
{
    if (spaceobj->renderlink.belongto != NULL)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : univRemoveObjFromRenderList
    Description : removes the spaceobj from the render list (if it is there)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univRemoveObjFromRenderList(SpaceObj *spaceobj)
{
    listVerify(&universe.RenderList);
    if (univSpaceObjInRenderList(spaceobj))
    {
        listRemoveNode(&spaceobj->renderlink);
    }
    listVerify(&universe.RenderList);
}

/*-----------------------------------------------------------------------------
    Name        : univAddObjToRenderListIf
    Description : adds newobj to renderlist if ifobj is in render list
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univAddObjToRenderListIf(SpaceObj *newobj,SpaceObj *ifobj)
{
    listVerify(&universe.RenderList);
    if (univSpaceObjInRenderList(ifobj))
    {
        if ((newobj->header.flags & SOF_Hide) == 0)        // don't add hidden objects
        {
            if (bitTest(newobj->header.flags, SOF_AlwaysSortFront))
            {
                univAddObjToRenderList(newobj);
                return;
            }
            newobj->cameraDistanceVector = ifobj->cameraDistanceVector;
            newobj->cameraDistanceSquared = ifobj->cameraDistanceSquared;

            listAddNodeAfter(&ifobj->renderlink,&newobj->renderlink,newobj);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : univAddObjToRenderList
    Description : adds newobj to renderlist
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univAddObjToRenderList(SpaceObj *newobj)
{
    listVerify(&universe.RenderList);
    if ((newobj->header.flags & SOF_Hide) == 0)
    {
        vector eyeposition = universe.mainCameraCommand.actualcamera.eyeposition;
        Node *objnode;
        SpaceObj *obj;

        vecSub(newobj->cameraDistanceVector,eyeposition,newobj->posinfo.position);
        newobj->cameraDistanceSquared = vecMagnitudeSquared(newobj->cameraDistanceVector);

        // now find where to put object in list to maintain sort order

        // render list is sorted from back to front, so find first node which has cameraDistanceSquared less than ours

        if (bitTest(newobj->header.flags, SOF_AlwaysSortFront))
        {                                                   //if this object has to go in front
            objnode = universe.RenderList.tail;
            while (objnode != NULL)
            {
                obj = (SpaceObj *)listGetStructOfNode(objnode);

                if (obj->cameraDistanceSquared > newobj->cameraDistanceSquared)
                {
                    listAddNodeAfter(objnode, &newobj->renderlink,newobj);
                    listVerify(&universe.RenderList);
                    goto doneForward;
                }
                objnode = objnode->prev;
            }
            listAddNode(&universe.RenderList,&newobj->renderlink,newobj);
            listVerify(&universe.RenderList);
doneForward:;
        }
        else
        {
            objnode = universe.RenderList.head;
            while (objnode != NULL)
            {
                obj = (SpaceObj *)listGetStructOfNode(objnode);

                if (obj->cameraDistanceSquared < newobj->cameraDistanceSquared)
                {
                    listAddNodeBefore(objnode,&newobj->renderlink,newobj);
                    goto doneRegular;
                }

                objnode = objnode->next;
            }

            listAddNode(&universe.RenderList,&newobj->renderlink,newobj);
doneRegular:
            ;
        }
    }
}

void univUpdateMinorRenderList()
{
    Node *objnode = universe.MinorSpaceObjList.head;
    SpaceObj *obj;
    vector distvec;
    vector lookatposition;
    vector eyeposition;
    real32 distsqr;

    if (mrCamera == NULL)
    {
        return;     // don't update render list if no mrCamera
    }

    lookatposition = mrCamera->lookatpoint;
    eyeposition = mrCamera->eyeposition;

    listVerify(&universe.RenderList);
    listRemoveAll(&universe.MinorRenderList);

    while (objnode != NULL)
    {
        obj = (SpaceObj *)listGetStructOfNode(objnode);

        vecSub(distvec,lookatposition,obj->posinfo.position);
        distsqr = vecMagnitudeSquared(distvec);

        if (distsqr < RENDER_VIEWABLE_DISTANCE_SQR)
        {
            if ((obj->header.flags & SOF_Hide) == 0)   // don't add hidden objects
            {
                vecSub(obj->cameraDistanceVector,eyeposition,obj->posinfo.position);
                obj->cameraDistanceSquared = vecMagnitudeSquared(obj->cameraDistanceVector);

                listAddNode(&universe.MinorRenderList,&obj->renderlink,obj);
            }
        }

        objnode = objnode->next;
    }
    listVerify(&universe.RenderList);
}

/*-----------------------------------------------------------------------------
    Name        : univUpdateRenderList
    Description : updates the render list
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R5 render): univUpdateRenderList 0x0054be80 squares the class limits once (the tweaks are distances in
//      Cataclysm, Tweak.c) and adds the three new classes (MonsterShip and Component as the mothership, Micro as the
//      fighter, 0x0054bf0d); the rest (the static's own limit, the derelicts, the sort-front list) is HW1's.
void univUpdateRenderList()
{
    Node *objnode = universe.SpaceObjList.head;
    Node *nextnode;
    SpaceObj *obj;
    vector distvec;
    vector lookatposition;
    vector eyeposition;
    real32 distsqr;
    static bool32 limited = FALSE;
    static real32 limits[NUM_CLASSES];
    LinkedList sortFrontList;

    listVerify(&universe.RenderList);
    listInit(&sortFrontList);

    if (mrCamera == NULL)
    {
        return;     // don't update render list if no mrCamera
    }

    if (!limited)
    {
        limits[CLASS_Mothership] = RENDER_LIMIT_MOTHERSHIP * RENDER_LIMIT_MOTHERSHIP;
        limits[CLASS_HeavyCruiser] = RENDER_LIMIT_HEAVYCRUISER * RENDER_LIMIT_HEAVYCRUISER;
        limits[CLASS_Carrier] = RENDER_LIMIT_CARRIER * RENDER_LIMIT_CARRIER;
        limits[CLASS_Destroyer] = RENDER_LIMIT_DESTROYER * RENDER_LIMIT_DESTROYER;
        limits[CLASS_Frigate] = RENDER_LIMIT_FRIGATE * RENDER_LIMIT_FRIGATE;
        limits[CLASS_Corvette] = RENDER_LIMIT_CORVETTE * RENDER_LIMIT_CORVETTE;
        limits[CLASS_Fighter] = RENDER_LIMIT_FIGHTER * RENDER_LIMIT_FIGHTER;
        limits[CLASS_Resource] = RENDER_LIMIT_RESOURCE * RENDER_LIMIT_RESOURCE;
        limits[CLASS_NonCombat] = RENDER_LIMIT_NONCOMBAT * RENDER_LIMIT_NONCOMBAT;
        limits[CLASS_MonsterShip] = limits[CLASS_Mothership];
        limits[CLASS_Component] = limits[CLASS_Mothership];
        limits[CLASS_Micro] = limits[CLASS_Fighter];
        limited = TRUE;
    }

    lookatposition = mrCamera->lookatpoint;
    eyeposition = mrCamera->eyeposition;

    listRemoveAll(&universe.RenderList);

    while (objnode != NULL)
    {
        obj = (SpaceObj *)listGetStructOfNode(objnode);

        vecSub(distvec,lookatposition,obj->posinfo.position);
        distsqr = vecMagnitudeSquared(distvec);

        if (obj->header.flags & SOF_ForceVisible)
        {                                                   //if object is to always be in render list
            vecSub(obj->cameraDistanceVector, eyeposition, obj->posinfo.position);
            obj->cameraDistanceSquared = vecMagnitudeSquared(obj->cameraDistanceVector);

            listAddNode(&universe.RenderList, &obj->renderlink, obj);
        }
        else
        {                                                   //else check if object should be in render list
            if (obj->header.objtype == OBJ_ShipType)
            {
                ShipClass shipclass;
                real32 limit;

                shipclass = ((ShipStaticInfo*)obj->staticinfo)->shipclass;
                dbgAssertOrIgnore((udword)shipclass < NUM_CLASSES);

                if (((ShipStaticInfo*)obj->staticinfo)->renderlistLimitSqr != 0.0f)
                {
                    limit = ((ShipStaticInfo*)obj->staticinfo)->renderlistLimitSqr;
                }
                else
                {
                    limit = limits[(sdword)shipclass];
                }

                if ((distsqr < limit) && !(obj->header.flags & SOF_Hide))
                {
                    vecSub(obj->cameraDistanceVector, eyeposition, obj->posinfo.position);
                    obj->cameraDistanceSquared = vecMagnitudeSquared(obj->cameraDistanceVector);

                    listAddNode(&universe.RenderList, &obj->renderlink, obj);
                }
            }
            else if (obj->header.objtype == OBJ_DerelictType)
            {
                real32 limit;

                if (((DerelictStaticInfo*)obj->staticinfo)->renderlistLimitSqr != 0.0f)
                {
                    limit = ((DerelictStaticInfo*)obj->staticinfo)->renderlistLimitSqr;
                }
                else
                {
                    limit = (obj->header.flags & SOF_BigObject) ? RENDER_MAXVIEWABLE_DISTANCE_SQR : RENDER_VIEWABLE_DISTANCE_SQR;
                }

                if (distsqr < limit)
                {
                    if ((obj->header.flags & SOF_Hide) == 0)
                    {
                        vecSub(obj->cameraDistanceVector, eyeposition, obj->posinfo.position);
                        obj->cameraDistanceSquared = vecMagnitudeSquared(obj->cameraDistanceVector);

                        if (bitTest(obj->header.flags, SOF_AlwaysSortFront))
                        {
                            listAddNode(&sortFrontList, &obj->renderlink, obj);
                        }
                        else
                        {
                            listAddNode(&universe.RenderList, &obj->renderlink, obj);
                        }
                    }
                }
            }
            else
            {
                if (distsqr < RENDER_MAXVIEWABLE_DISTANCE_SQR)
                {
                    if ((distsqr < RENDER_VIEWABLE_DISTANCE_SQR) || (obj->header.flags & SOF_BigObject))
                    {
                        if ((obj->header.flags & SOF_Hide) == 0)   // don't add hidden objects
                        {
                            vecSub(obj->cameraDistanceVector,eyeposition,obj->posinfo.position);
                            obj->cameraDistanceSquared = vecMagnitudeSquared(obj->cameraDistanceVector);

                            if (bitTest(obj->header.flags, SOF_AlwaysSortFront))
                            {
                                listAddNode(&sortFrontList, &obj->renderlink, obj);
                            }
                            else
                            {
                                listAddNode(&universe.RenderList,&obj->renderlink,obj);
                            }
                        }
                    }
                }
            }
        }

        objnode = objnode->next;
    }

    listVerify(&universe.RenderList);
    listMergeSort2(&universe.RenderList);

    listVerify(&universe.RenderList);
    if (sortFrontList.num > 0)
    {                                                       //if there are force-front-sort objects
        listMergeSort2(&sortFrontList);                     //sort them like regular rederlist objects
        objnode = sortFrontList.head;
        while (objnode != NULL)
        {
            obj = (SpaceObj *)listGetStructOfNode(objnode);
            nextnode = objnode->next;
            dbgAssertOrIgnore(&obj->renderlink == objnode);
            listAddNode(&universe.RenderList, &obj->renderlink, obj);
            objnode = nextnode;
        }
    }
    listVerify(&universe.RenderList);
}

/*-----------------------------------------------------------------------------
    Name        : univCheckRegrowResources
    Description : checks if any resources should be regrown
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void univCheckRegrowResources(void)
{
    Node *node = universe.ResourceVolumeList.head;
    ResourceVolume *resvol;

    while (node != NULL)
    {
        resvol = (ResourceVolume *)listGetStructOfNode(node);

        if (resvol->actualnumber < resvol->number)
        {
            // we are short some resources, so there is a chance we might regrow them

            real32 shortperc = 1.0f - ( (real32) resvol->actualnumber / (real32) resvol->number );
            sdword regrowrate = resvol->attributesParam;

            if ((regrowrate < 0) || (regrowrate > REGROWRATE_Max))
            {
                regrowrate = 0;
            }

            shortperc *= REGROW_RATE_MODIFIER[regrowrate];

            if (frandom(1) < shortperc)
            {
                sdword tr;

                // let's regrow the resource ... or at least try 3 times (make sure it doesn't overlap ship)
                switch (resvol->resourceVolumeType)
                {
                    case ResourceSphereType:
                        for (tr=0;tr<3;tr++)
                        {
                            if (AddResourceToSphere(resvol,TRUE))
                            {
#ifdef HW_BUILD_FOR_DEBUGGING
                                dbgMessage("Regrowing resource!");
#endif
                                resvol->actualnumber++;
                                break;
                            }
                        }
                        break;

                    case ResourceCylinderType:
                        for (tr=0;tr<3;tr++)
                        {
                            if (AddResourceToCylinder(resvol,TRUE))
                            {
#ifdef HW_BUILD_FOR_DEBUGGING
                                dbgMessage("Regrowing resource!");
#endif
                                resvol->actualnumber++;
                                break;
                            }
                        }
                        break;

                    case ResourceRectangleType:
                        for (tr=0;tr<3;tr++)
                        {
                            if (AddResourceToRectangle(resvol,TRUE))
                            {
#ifdef HW_BUILD_FOR_DEBUGGING
                                dbgMessage("Regrowing resource!");
#endif
                                resvol->actualnumber++;
                                break;
                            }
                        }
                        break;

                    default:
                        dbgAssertOrIgnore(FALSE);
                        break;
                }
            }
        }

        node = node->next;
    }
}

/*=============================================================================
    CAT: the per-ship deferred update. In the exe univSetupShipForControl 0x0054a230 puts every live ship on a
    universe grow-selection (unk1 in the export, after HousekeepShipList: universe.catSelection1A0 since M2-R4
    rest-oracle; this list uses it since M2-R5 layout-dock), and after the ship loop univUpdateAllPosVelShips
    0x00544f40 / univSetupShipsForControl 0x0054a340 run cat_univShipDeferredUpdate on each, before the
    CustShipHousekeeps.
=============================================================================*/
static void catDeferredShipListReset(void)
{
    if (universe.catSelection1A0.selection == NULL)
    {
        growSelectInit(&universe.catSelection1A0);
    }
    growSelectReset(&universe.catSelection1A0);
}

/*-----------------------------------------------------------------------------
    Name        : cat_univRemoveObjReferencesFromShips (Cataclysm 0x0054d350)
    Description : Marks an object (bit 0x100 of its +0x18 word) and has every ship, in the universe and in the
                  hyperspace ether, drop its references to it (CustShipHeader slot 12). The mark makes the
                  object's next deferred update tell the ships again (cat_univShipDeferredUpdate). Callers in the
                  exe: cat_dockShipDocksAtShip, UpdateMidLevelHyperspacingShips (not ported yet).
    Inputs      : obj - the object
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
static void catUnivTellShipsToDropReferences(SpaceObj *obj)
{
    Node *node;
    Ship *ship;
    InsideShip *insideShip;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (ship->staticinfo->custshipheader.CustShipRemoveObjReferences != NULL)
        {
            ship->staticinfo->custshipheader.CustShipRemoveObjReferences(ship, (struct SpaceObj *)obj);
        }
    }
    for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = node->next)
    {
        insideShip = (InsideShip *)listGetStructOfNode(node);
        ship = insideShip->ship;
        if (ship->staticinfo->custshipheader.CustShipRemoveObjReferences != NULL)
        {
            ship->staticinfo->custshipheader.CustShipRemoveObjReferences(ship, (struct SpaceObj *)obj);
        }
    }
}

// CAT: the exe's +0x18 word of an object (SpaceObjHeader.catFlags18, one field for every object type since the M2-R5
//      layout pass; it was Ship.catShipFlags for ships, bridged by CAT_OBJ_FLAGS18)
#define CAT_OBJFLAG18_RemoveReferences  0x100

void cat_univRemoveObjReferencesFromShips(SpaceObj *obj)
{
    obj->header.catFlags18 |= CAT_OBJFLAG18_RemoveReferences;
    catUnivTellShipsToDropReferences(obj);
}

/*-----------------------------------------------------------------------------
    Name        : cat_univShipSelfRepair (Cataclysm 0x005433d0)
    Description : Ship self-repair, which in Cataclysm only Beast ships and ships with the self-repair upgrade
                  (catUpgradeFlags 0x200000) have, and not while they are being built: every repairTime seconds
                  (repairCombatTime in combat: forced attack status, recently attacked or attacking) the ship
                  gains repairDamage (repairCombatDamage) x its damageModifier word, up to its maximum health.
                  Component-class ships and ships at full health only restart the timer (a Sect mothership at
                  full health still runs its module repair).
    Inputs      : ship
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
#define CAT_UPGRADE_SelfRepair      0x00200000      // cat_shipHasUpgrade(ship, 0x200000) at 0x0054341a (name chosen here)
#define CAT_MODULE_REPAIR_SHARE     7.0f            // 0x0072bd6c

// the module repair of a Sect mothership (inlined in cat_univShipSelfRepair 0x005433d0 at 0x00543505)
static void cat_univMothershipRepairModules(Ship *ship, bool32 inCombat)
{
    MothershipSpec *spec = (MothershipSpec *)ship->ShipSpecifics;
    CatClampedList *clamped;
    Ship *module;
    Ship *slotModule[CAT_MOTHERSHIP_MODULE_SLOTS];
    sdword damaged[3] = { 0, 0, 0 };
    sdword priority;
    sdword slot;
    sdword i;
    real32 rate;

    if ((ship->guidance.clampInfo == NULL) || ((clamped = ship->guidance.clampInfo->clampedObjs) == NULL))
    {
        return;
    }
    for (i = 0; i < CAT_MOTHERSHIP_MODULE_SLOTS; i++)
    {
        slotModule[i] = NULL;
    }
    for (i = 0; i < clamped->numShips; i++)
    {
        module = clamped->ShipPtr[i];
        switch (module->shiptype)
        {
            case sMothershipDockingBay: slot = 0; break;
            case sMothershipWeapons:    slot = 1; break;
            case sMothershipArmour:     slot = 2; break;
            case sMothershipMicro:      slot = 3; break;
            case sMothershipSpecial:    slot = 4; break;
            case sMothershipSupport:    slot = 5; break;
            case sMothershipBigGun:     slot = 6; break;
            default:                    continue;
        }
        if (slot == 5)
        {
            // the support modules: the first damaged one counts, then the weakest of them is repaired
            if (slotModule[5] == NULL)
            {
                if (module->targetable.health < module->staticinfo->health.maxhealth)
                {
                    damaged[spec->moduleSlots[5].count]++;
                    slotModule[5] = module;
                }
            }
            else if (module->targetable.health < slotModule[5]->targetable.health)
            {
                slotModule[5] = module;
            }
            continue;
        }
        slotModule[slot] = module;
        if (module->staticinfo->health.maxhealth <= module->targetable.health)
        {
            slotModule[slot] = NULL;
        }
        else
        {
            damaged[spec->moduleSlots[slot].count]++;
        }
    }
    for (i = 0; i < CAT_MOTHERSHIP_MODULE_SLOTS; i++)
    {
        module = slotModule[i];
        if (module == NULL)
        {
            continue;
        }
        rate = inCombat ? module->staticinfo->repairCombatDamage : module->staticinfo->repairDamage;
        if (0.0f < rate)
        {
            priority = spec->moduleSlots[i].count;
            if (damaged[0] >= 1)
            {
                if (priority == 0)
                {
                    module->targetable.health += (CAT_MODULE_REPAIR_SHARE / (real32)damaged[0]) * module->damageModifier * rate;
                }
            }
            else if (damaged[1] >= 1)
            {
                if (priority == 1)
                {
                    module->targetable.health += (CAT_MODULE_REPAIR_SHARE / (real32)damaged[1]) * module->damageModifier * rate;
                }
            }
            else if (priority == 2)
            {
                module->targetable.health += (CAT_MODULE_REPAIR_SHARE / (real32)damaged[2]) * module->damageModifier * rate;
            }
            if (module->staticinfo->health.maxhealth < module->targetable.health)
            {
                module->targetable.health = module->staticinfo->health.maxhealth;
            }
        }
    }
}

void cat_univShipSelfRepair(Ship *ship)
{
    ShipStaticInfo *shipstatic = ship->staticinfo;
    bool32 inCombat;
    real32 repairTime, repairDamage;

    // CAT: the forced-attack bit is specialFlags 0x400000 in the exe (SPECIAL_ForcedAttackStatus since the M2-R4 command port)
    inCombat = bitTest(ship->shipOrDerelict.specialFlags, SPECIAL_ForcedAttackStatus) ||
               (ship->recentlyAttacked != 0) || (ship->shipisattacking != 0);
    repairTime = inCombat ? shipstatic->repairCombatTime : shipstatic->repairTime;
    repairDamage = inCombat ? shipstatic->repairCombatDamage : shipstatic->repairDamage;

    if (!(cat_shipHasUpgrade(ship, CAT_UPGRADE_SelfRepair) || ship->shiprace == RACE_Beast) ||
        !(repairDamage > 0.0f) || bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
    {
        return;
    }
    // CAT-TODO(M2-R6 ships, the repair droids): Sect ships with repair droids update them here (droidUpdateArray
    //      0x005c32f0, Ship +0x4C0, when the droid option 0x00c52374 is on; random stream 0, RepairDroid.h)
    if ((shipstatic->shipclass == CLASS_Component) ||
        ((ship->targetable.health >= shipstatic->health.maxhealth) && (ship->shiptype != sMothership)))
    {
        ship->lastTimeRepaired = universe.totaltimeelapsed;
        return;
    }
    if ((universe.totaltimeelapsed - ship->lastTimeRepaired) >= repairTime)
    {
        ship->lastTimeRepaired = universe.totaltimeelapsed;
        // CAT (M2-R4 ships-b): a Sect mothership first repairs its damaged module ships (0x00543505): each module type
        //      has a priority (the second word of its specifics module slot, 0..2); only the damaged modules of the
        //      highest priority present gain 7 / their number x their damageModifier x their own repair rate
        if (ship->shiptype == sMothership)
        {
            cat_univMothershipRepairModules(ship, inCombat);
        }
        // CAT (M2-R4 ships-b): a Beast mothership with an active system does not repair itself
        //      (cat_beastMothershipSystemActive 0x005bcac0)
        if (((ship->shiptype != bMothership) || !cat_beastMothershipSystemActive(ship)) &&
            (ship->targetable.health < shipstatic->health.maxhealth))
        {
            ship->targetable.health += repairDamage * ship->damageModifier;
            if (ship->targetable.health > shipstatic->health.maxhealth)
            {
                ship->targetable.health = shipstatic->health.maxhealth;
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_univConvertShip (Cataclysm 0x00543950)
    Description : Replaces ship by a new ship of newType owned by newOwner, at the same place, speed, frame, health
                  (capped) and resources (capped): a salvaged ship joining its salvager's player, an infected ship
                  turning Beast. A ship the new owner has no support units for is destroyed instead.
    Inputs      : ship, newType, newOwner
    Outputs     :
    Return      : the new ship, or NULL
----------------------------------------------------------------------------*/
Ship *cat_univConvertShip(Ship *ship, ShipType newType, Player *newOwner)
{
    Ship *newship;
    Player *oldOwner = ship->playerowner;
    ShipType oldType = ship->shiptype;
    Ship *depot = ship->catSalvageDepot;
    real32 health, savedRotInfo[15];
    sdword resources;
    udword upgradeFlags;
    sdword armourLevel;             // CAT (M2-R5 layout-dock): Ship+0x3F0
    sdword i, nguns;                // [integrate: M2-R5] the gun-word copy
    vector position, velocity, force;
    bool16 isMoving;

    (void)oldOwner;
    (void)oldType;

    ship->catBeastConvertTime = 0.0f;
    if (ship->catBeastBindings != NULL)
    {
        memFree(ship->catBeastBindings);
        ship->catBeastBindings = NULL;
    }
    if (ship->catSalvageConvertBindings != NULL)
    {
        memFree(ship->catSalvageConvertBindings);
        ship->catSalvageConvertBindings = NULL;
    }

    if (!cat_cmHasSupportUnits(newType, newOwner) && (ship->catSalvageDepot == NULL))
    {
        // CAT (M2-R5 hud): for the current player the exe shows a command message first (clCommandMessage
        //      0x004ac820, string 0x00bb5008, red)
        if (newOwner == universe.curPlayerPtr)
        {
            clCommandMessage(strGetString(strNotEnoughSupportUnitsToConvert), 0xff0000ff);
        }
        ApplyDamageToTarget(&ship->targetable, 5000.0f, (GunSoundType)0x13, 0x29a, newOwner->playerIndex, NULL, 8);
        ship->catBeastConvertTime = 0.0f;
        bitClear(ship->header.flags, SOF_Crazy|SOF_Disabled);
        return NULL;
    }

    // the state the new ship keeps (0x00543a2f: rotinfo's 15 dwords, posinfo, health, Ship+0x3F0, upgrades, resources)
    memcpy(savedRotInfo, &ship->rotatable.rotinfo.coordsys, sizeof(savedRotInfo));
    position = ship->obj.posinfo.position;
    velocity = ship->obj.posinfo.velocity;
    force = ship->obj.posinfo.force;
    isMoving = ship->obj.posinfo.isMoving;
    health = ship->targetable.health;
    upgradeFlags = ship->catUpgradeFlags;
    armourLevel = ship->catArmourLevel;
    resources = ship->resources;

    // CAT: univCreateShip 0x00542320 takes 7 arguments and adds the ship itself; univAddShip is this tree's equivalent
    //      (the split is this tree's, see univCreateShip: no port) [integrate: M2-R5; was an M2-R4 #3..#5 TODO]
    newship = univAddShip(newType, GetValidRaceForShipType(newType), &position, newOwner, 0);

    // CAT: the exe copies each gun's current-weapon word (Gun +0x00, catWeaponType) from the old ship to the new one,
    //      the first min(numGuns) guns (cat_univConvertShip 0x00543950, the 0x88-stride loop at 0x00543ad6)
    //      [integrate: M2-R5; was an M2-R4 combat, the gun word TODO]
    if ((ship->gunInfo != NULL) && (newship->gunInfo != NULL))
    {
        nguns = min(ship->gunInfo->numGuns, newship->gunInfo->numGuns);
        for (i = 0; i < nguns; i++)
        {
            newship->gunInfo->guns[i].catWeaponType = ship->gunInfo->guns[i].catWeaponType;
        }
    }

    univAddObjToRenderListIf((SpaceObj *)newship, (SpaceObj *)ship);
    unitCapCreateShip(newship, newship->playerowner);
    // CAT-TODO(M2-R6 research): the exe copies the old type's 0xA8-byte research/upgrade block of the old owner (Player
    //      +0x350) to the new type's block of the new owner

    // CAT: the exe pushes a third word, 1 (0x00543ba8), which univDeleteDeadShip 0x00547d00 never reads (its body uses
    //      ship and deathBy only): no port [integrate: M2-R5; was an M2-R4 #1 follow-up TODO]
    univDeleteDeadShip(ship, -1);
    if (ship->impactable.impactablelink.belongto != NULL)
    {
        listRemoveNode(&ship->impactable.impactablelink);
    }
    // CAT: the exe pushes a second word, 2 (0x00543bce), which univWipeShipOutOfExistence 0x005477d0 never reads: no
    //      port [integrate: M2-R5; was an M2-R4 #1 follow-up TODO]
    univWipeShipOutOfExistence(ship);

    newship->obj.posinfo.velocity = velocity;
    newship->obj.posinfo.force = force;
    memcpy(&newship->rotatable.rotinfo.coordsys, savedRotInfo, sizeof(savedRotInfo));
    newship->targetable.health = health;
    newship->obj.posinfo.isMoving = isMoving;
    if (newship->staticinfo->health.maxhealth < health)
    {
        newship->targetable.health = newship->staticinfo->health.maxhealth;
    }
    newship->lastTimeRepaired = 0.0f;
    newship->resources = resources;
    if (newship->staticinfo->maxresources < resources)
    {
        newship->resources = newship->staticinfo->maxresources;
    }
    // CAT (M2-R5 layout-dock): the armour level (Ship+0x3F0) and the upgrade flags (+0x3F4) carry over (0x00543d6f)
    newship->catArmourLevel = armourLevel;
    newship->catUpgradeFlags = upgradeFlags;
    univUpdateObjRotInfo((SpaceObjRot *)newship);

    if (newOwner->race == RACE_Beast)
    {
        if ((depot == NULL) || (depot->shiprace == RACE_Beast))
        {
            newship->catBeastStunState = 2;
            newship->catBeastStunStart = universe.totaltimeelapsed + 0.5f;
            newship->catBeastStunEnd = newship->catBeastStunStart + 0.5f;
        }
        if (!singlePlayerGame)
        {
            // CAT-TODO(stage 4): a computer player's new ship (not a bProcessor) is sent to dock, 1 in 10 with dock type
            //      7 (clWrapDock with universe.aiplayerProcessing set); a bProcessor acquires the old ship's techs
            //      (cat_rmAcquireShipTechs, M2-R6 research); for the current player the exe compares the new owner's
            //      research with the type's tech sets and shows "new technology" messages (cat_ioMessageAdd, stage 3)
        }
    }
    return newship;
}

/*-----------------------------------------------------------------------------
    Name        : cat_univBeastInfectionUpdate (Cataclysm 0x00543860)
    Description : An infected ship stops what it is doing and, when its time has come, turns into the Beast type its
                  statics name (bWorker when they name none) for the infector's owner.
    Inputs      : ship
    Outputs     :
    Return      : TRUE when the ship has been converted (the caller stops updating it)
----------------------------------------------------------------------------*/
bool32 cat_univBeastInfectionUpdate(Ship *ship)
{
    SelectCommand selectone;
    ShipType newType;
    Ship *newship;

    if ((ship->catBeastConvertTime != 0.0f) && !nisIsRunning)
    {
        if (ship->command != NULL)
        {
            selectone.numShips = 1;
            selectone.timeLastStatus = 0.0f;
            selectone.ShipPtr[0] = ship;
            // CAT: the exe passes RemoveShipsFromDoingStuff 0x00444810 the reason 0x2A
            RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone,0x2A);
        }
        if (ship->catBeastConvertTime <= universe.totaltimeelapsed)
        {
            newType = (ship->staticinfo->beastAttacksRequired >= 0) ? ship->staticinfo->beastAttackConvertTo : bWorker;
            // the exe indexes the [8][145] table as [newType + player * 8], as cat_univShipSalvagedCountInc
            ((sdword *)catShipsBeastified)[newType + ship->catBeastInfectorOwner->playerIndex * MAX_MULTIPLAYER_PLAYERS]++;
            ship->howDidIDie = 0x29a;
            ship->whoKilledMe = ship->catBeastInfectorOwner->playerIndex;
            newship = cat_univConvertShip(ship, newType, ship->catBeastInfectorOwner);
            if (newship != NULL)
            {
                bitSet(newship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeastified);
            }
            return TRUE;
        }
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_univShipDeferredUpdate (Cataclysm 0x005447b0)
    Description : The per-ship update that runs after every ship has moved and been set up for control. Ported:
                  the remove-references broadcast of a marked ship and the self-repair. The rest is marked
                  CAT-TODO with its owner.
    Inputs      : ship
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void cat_univShipDeferredUpdate(Ship *ship)
{
    if (bitTest(ship->header.catFlags18, CAT_OBJFLAG18_RemoveReferences))
    {                                                       // 0x005447b7
        catUnivTellShipsToDropReferences((SpaceObj *)ship);
        bitClear(ship->header.catFlags18, CAT_OBJFLAG18_RemoveReferences);
    }
    // CAT (M2-R4 combat): an infected ship that turns Beast ends the update (0x00544829)
    if (cat_univBeastInfectionUpdate(ship))
    {
        return;
    }
    // CAT (M2-R4 combat): a queued attack target becomes an attack order (0x00544867)
    if (ship->catQueuedAttackTarget != NULL)
    {
        SelectCommand selectone;
        AttackCommand attackone;

        selectone.numShips = 1;
        selectone.ShipPtr[0] = ship;
        attackone.numTargets = 1;
        attackone.TargetPtr[0] = (TargetPtr)ship->catQueuedAttackTarget;
        clAttack(&universe.mainCommandLayer, &selectone, &attackone);
        ship->catQueuedAttackTarget = NULL;
    }
    // CAT (M2-R4 ships-b): a Sentinel's DFG disruption ends with its timer, checked on 31 of 32 updates (0x0054487b)
    if (((universe.univUpdateCounter & 0x1f) != 0) && ((ship->shiptype == sSentinel) || (ship->shiptype == bSentinel)) &&
        (((SentinelSpec *)ship->ShipSpecifics)->disruptEndTime < universe.totaltimeelapsed))
    {
        ((SentinelSpec *)ship->ShipSpecifics)->disrupted = FALSE;
    }
    // CAT: the protected-speed limit is cleared when it is due to be recomputed (0x005448a7)
    if (universe.totaltimeelapsed > catProtectedSpeedLimitTime)
    {
        ship->catProtectedSpeedLimit = -1.0f;
    }
    cat_shipDissolveUpdate(ship);                           // CAT: 0x00460850 (M2-R4 ships-b)
    // CAT (M2-R4 combat): the stun timers of a Beast hit (0x005448ca): when the stun period is over, a hit ship
    //      (state 1) is stunned again after 0.4..1.2 s for 1.5..3 s, or at its spin time when that is set; an infected
    //      one (state 2) is released
    if ((ship->catBeastStunState != 0) && (universe.totaltimeelapsed > ship->catBeastStunStart) &&
        (universe.totaltimeelapsed > ship->catBeastStunEnd))
    {
        if (ship->catBeastStunState == 1)
        {
            if (ship->catBeastSpin < 0.0f)
            {
                ship->catBeastStunStart = (real32)ranRandom(RANDOM_GAME) * 1.8626453e-10f + 0.4f + universe.totaltimeelapsed;
            }
            else
            {
                ship->catBeastStunStart = ship->catBeastSpin;
            }
            ship->catBeastStunEnd = (real32)ranRandom(RANDOM_GAME) * 3.4924597e-10f + 1.5f + ship->catBeastStunStart;
        }
        else
        {
            ship->catBeastStunState = 0;
        }
    }
    cat_univShipSelfRepair(ship);
    // CAT (M2-R4 combat): a burst that has run 0.1 s past its fire time ends (0x00544995: every gun but an ion beam in
    //      its burst state)
    if (ship->catBurstFiring && (ship->gunInfo != NULL))
    {
        sdword i;
        Gun *gun;

        for (i = 0; i < ship->gunInfo->numGuns; i++)
        {
            gun = &ship->gunInfo->guns[i];
            if ((gun->catWeaponType != WEAPON_ION) && (gun->burstState == 1) &&
                (gun->useBurstFireTime + (real32)0.1 < universe.totaltimeelapsed - gun->burstStartTime))
            {
                gun->burstState = 2;
                ship->catBurstFiring = FALSE;
                gun->burstStartTime = universe.totaltimeelapsed;
                soundEventBurstStop(ship, NULL);
            }
        }
    }
    // CAT (M2-R5 layout-dock): the end of an upgrade (cat_univShipDeferredUpdate 0x005447b0, 0x00544a4e: catUpgrading
    //      with its end time at Ship+0x5A0 passed: the ship is enabled again, its upgrades applied and the "upgrade
    //      complete" speech 0x2000895 queued for the owner), then the debug re-upgrade switch (0x00a751c0: a console
    //      toggle, stage 3; file static here)
    if (ship->catUpgrading && (ship->catUpgradeEnd < universe.totaltimeelapsed))
    {
        ship->catUpgrading = FALSE;
        cat_shipSetDisableReason(ship, 0, 0);
        cat_shipApplyUpgrades(ship);
        speechEventQueue(NULL, 0x2000895, -1, -1, 0, ship->playerowner->playerIndex, -1, 5.0f, -1);
    }
    if (catDebugReapplyUpgrades)
    {
        cat_shipApplyUpgrades(ship);
    }
    cat_shipUpdateDrift(ship);                              // CAT: 0x005b3d60 (M2-R4 ships-b)
    // CAT-TODO(M2-R6 FOW): then the stealth detection of the catTypeFlags 0x20000 types on every 16th tick by ship
    //      number (cat_shipStealthDetectionUpdate 0x0053fd90: a player detects the ship when one of its detector ships,
    //      catTypeFlags 0x10000 with upgrade 0x800000, is within its FOW radius, cat_shipGetFOWRadius 0x00514a30, which
    //      needs the research tech sets cat_rmTechSetHas), and for live ships cat_shipUpdateExternalBuildAndAI
    //      0x00543f10 with universe.aiplayerProcessing set
}

/*-----------------------------------------------------------------------------
    CAT: bullet effects after a load (cat_univRequestBulletEffectsRecreate 0x0054d240, called by LoadUniverse;
    cat_univRecreateBulletEffectsIfNeeded 0x0054d330, called by univUpdate; cat_univRecreateBulletEffects 0x0054d250):
    big-gun shells and homing bullets that have no flight effect get one again.
-----------------------------------------------------------------------------*/
static bool32 catBulletEffectsRecreateRequested = FALSE;   // 0x00a6ed88

void cat_univRequestBulletEffectsRecreate(void)
{
    catBulletEffectsRecreateRequested = TRUE;
}

void cat_univRecreateBulletEffects(void)
{
    Node *node;
    Bullet *bullet;
    etglod *etgLOD;
    etgeffectstatic *stat;

    for (node = universe.BulletList.head; node != NULL; node = node->next)
    {
        bullet = (Bullet *)listGetStructOfNode(node);
        if (bullet->effect != NULL ||
            !(bullet->soundType == GS_BigGun || CAT_BULLET_WEAPON_TYPE(bullet) == WEAPON_ENERGY))
        {
            continue;
        }
        etgLOD = etgGunEventTable[bullet->playerowner->race][bullet->soundType][EGT_GunBullet];
        if (etgLOD == NULL)
        {
            continue;
        }
        stat = etgLOD->level[0];
        if (stat == NULL || !etgBulletEffectsEnabled || etgFrequencyExceeded(stat))
        {
            continue;
        }
        if (bullet->soundType == GS_BigGun)
        {                                                   // one parameter, 12345.0f (0x4640e400)
            bullet->effect = etgEffectCreate(stat, (Ship *)bullet, NULL, &bullet->obj.posinfo.velocity, NULL, 1.0f,
                                             EAF_AllButNLips, 1, SCALECAST(Real32ToUdword(12345.0f)));
        }
        else
        {
            bullet->effect = etgEffectCreate(stat, (Ship *)bullet, NULL, &bullet->obj.posinfo.velocity, NULL, 1.0f,
                                             EAF_AllButNLips, 0);
        }
    }
}

void cat_univRecreateBulletEffectsIfNeeded(void)
{
    if (catBulletEffectsRecreateRequested)
    {
        catBulletEffectsRecreateRequested = FALSE;
        cat_univRecreateBulletEffects();
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_univDeleteBulletNow (Cataclysm 0x0054d110)
    Description : Removes a bullet at once (no effects): its +0x84 block, its flight effect's owner link, its
                  collision blob, bullet and render lists, and the bullet itself. Caller in the exe:
                  collCheckBulletTargetColl (M2-R4 #3).
    Inputs      : bullet
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void cat_univDeleteBulletNow(Bullet *bullet)
{
    cat_univFreeEffectBlock(&bullet->obj);
    if (bullet->effect != NULL)
    {
        bullet->effect->owner = NULL;
    }
    bobObjectDied((SpaceObj *)bullet,&universe.collBlobList);
    listRemoveNode(&bullet->bulletlink);
    univRemoveObjFromRenderList((SpaceObj *)bullet);
    listDeleteNode(&bullet->header.objlink);
}

/*-----------------------------------------------------------------------------
    Name        : cat_univSetShipPosition (Cataclysm 0x0054d180)
    Description : Moves a ship to a position (Clamp.c, Dock.c, Formation.c use it in the exe; not ported yet).
                  CAT-TODO(stage 3 FOW): a ship that affects the fog of war first moves its cube
                  (cat_univShipFOWUpdate 0x00544b10).
    Inputs      : ship, position
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void cat_univSetShipPosition(Ship *ship, vector *position)
{
    ship->obj.posinfo.position = *position;
}

/*-----------------------------------------------------------------------------
    Name        : cat_univPlayerHasFlaggedShip (Cataclysm 0x0054d200)
    Description : TRUE if the current player owns a ship that is being built (specialFlags2 bit 0x8;
                  SPECIAL_2_CatBeingBuilt). Caller: spHyperspaceButtonPushed (stage 6).
    Inputs      :
    Outputs     :
    Return      : bool32
----------------------------------------------------------------------------*/
bool32 cat_univPlayerHasFlaggedShip(void)
{
    Node *node;
    Ship *ship;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (ship->playerowner == universe.curPlayerPtr && bitTest(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeingBuilt))
        {
            return TRUE;
        }
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_univRealTimeDelta
    Description : CAT: cat_univRealTimeDelta 0x0054c360: real time since the last call, in seconds, counted in whole
                  periods of the task timer; the remainder is carried to the next call. univUpdate inlines the same
                  code (QueryPerformanceCounter at 0x0054c57f) to advance realtimeelapsed; universeUpdateTask calls it
                  while the universe is paused, so the paused time is consumed and realtimeelapsed does not jump.
                  The exe's counter starts at 0, so its first call returns the time since the counter's origin;
                  likewise here (SDL_GetTicks counts from SDL_Init).
    Inputs      :
    Outputs     : advances univRealTimeRemainder
    Return      : elapsed seconds (whole timer periods / taskFrequency)
----------------------------------------------------------------------------*/
real32 cat_univRealTimeDelta(void)
{
    Uint32 now = SDL_GetTicks();
    Uint32 difference = now - univRealTimeRemainder;

    univRealTimeRemainder = now - difference % utyTimerDivisor;
    return (real32)(difference / utyTimerDivisor) / taskFrequency;
}

/*-----------------------------------------------------------------------------
    Name        : univUpdate
    Description : Updates the mission sphere ships and objects
    Inputs      : phystimeelapsed, time since last time it was updated
    Outputs     :
    Return      : TRUE if done (game over)
----------------------------------------------------------------------------*/
bool32 univUpdate(real32 phystimeelapsed)
{
#ifdef _WIN32
#define TMP_SAVEDGAMES_PATH "SavedGames\\"
#else
#define TMP_SAVEDGAMES_PATH "SavedGames/"
#endif

    // CAT: debug frame stepping: no update unless a step is left (univUpdate 0x0054c3f0, 0x0054c3fb)
    if (catUnivFrameStep)
    {
        if (catUnivFrameStepsLeft < 1)
        {
            return FALSE;
        }
        catUnivFrameStepsLeft--;
    }

    // CAT: multiplayer autosave every 200 s into SavedGames\multiplayer\AS0..AS10 (0x0054c41b; the switch
    //      0x00a74dac has no setter in the code, presumably a command-line option, so it is off here)
    if (catMultiplayerAutoSave && !singlePlayerGame && (catMultiplayerAutoSaveTime < universe.totaltimeelapsed))
    {
        char savegamename[200];

        sprintf(savegamename, TMP_SAVEDGAMES_PATH "multiplayer" TMP_SAVEDGAMES_PATH_SEP "AS%d", catMultiplayerAutoSaveNumber);
        SaveGame(savegamename);
        clCommandMessage(savegamename, 0xffffffff);
        catMultiplayerAutoSaveTime = universe.totaltimeelapsed + 200.0f;                 // 0x0072a440
        catMultiplayerAutoSaveNumber++;
        if (catMultiplayerAutoSaveNumber > 10)
        {
            catMultiplayerAutoSaveNumber = 0;
        }
    }

    if ((autoSaveDebug) && (UNIVERSE_WOODPECKER(31, 0)))    // every 2s
    {
        char savegamename[200];
        static sdword savenumber = 0;

        if (multiPlayerGame)
        {
            sprintf(savegamename,TMP_SAVEDGAMES_PATH "AutoSave%f",universe.totaltimeelapsed);
            SaveGame(savegamename);
            clCommandMessage(strGetString(strSavedGame), 0xffffffff);
        }
        else
        {
            if (UNIVERSE_WOODPECKER(255, 0))        // only save every 16s
            {
                if (((singlePlayerGame) && (singlePlayerGameInfo.hyperspaceState)) ||
                    (nisIsRunning))
                {
                    ; // don't save game
                }
                else
                {
                    sprintf(savegamename,TMP_SAVEDGAMES_PATH "AutoSaveDebug%d",savenumber);
                    savenumber = (savenumber+1) & 7;
                    SaveGame(savegamename);
                    clCommandMessage(strGetString(strSavedGame), 0xffffffff);
                }
            }
        }
    }

#undef TMP_SAVEDGAMES_PATH

#ifdef GOD_LIKE_SYNC_CHECKING
    if(multiPlayerGame)
    {
        if(syncDumpOn)
        {
            industrialStrengthSyncDebugging(universe.univUpdateCounter);
            netcheckIndustrialChecksum();
        }
    }
#endif

    universe.univUpdateCounter++;

#define firstTime (universe.univUpdateCounter == 1)

    universe.phystimeelapsed = phystimeelapsed;
    universe.totaltimeelapsed += phystimeelapsed;
    // CAT: then the real-time clock (univUpdate 0x0054c3f0, 0x0054c57f-0x0054c5f6: realtimeelapsed += whole QPC periods
    //      / taskFrequency); a fixed 1/16 s per tick in oracle runs (univRealTimeVirtual)
    universe.realtimeelapsed = (univRealTimeVirtual ? UNIV_REALTIME_VIRTUAL_TICK : cat_univRealTimeDelta())
                               + universe.realtimeelapsed;

    // CAT: the quit and win times are kept in real time (univUpdate 0x0054c3f0 compares them with
    //      universe.realtimeelapsed, and every setter adds to it: CheckPlayerWin, singlePlayerGameUpdate,
    //      singlePlayerMissionCompleteCB), so a paused or slowed game still ends on time
    if (universe.quittime != 0.0f)
    {
        real32 usequittime = universe.quittime;

        if (universe.wintime != 0.0f)
        {
            if (universe.realtimeelapsed >= universe.wintime)
            {
                DeclarePlayerWin();
                universe.wintime = 0.0f;
            }
        }

        if ((multiPlayerGame) && (IAmCaptain))
        {
            usequittime += 5.0f;    // give a extra 5s so we transmit some more sync pkts other people so they quit too...
        }

        if (universe.realtimeelapsed >= usequittime)
        {
            // quit the game here
            spMainScreen();

#if (defined(HW_GAME_DEMO) || defined(HW_GAME_RAIDER_RETREAT))
            if ((singlePlayerGame) && (!(utyCreditsSequence|utyPlugScreens)))
#else
            if ((singlePlayerGame) && (!(utyCreditsSequence)))
#endif
            {
                gameIsRunning = FALSE;
                mrReset();
                feScreenStart(ghMainRegion, "SP_Game_Over");
                return TRUE;
            }
            else
            {
                // CAT-TODO(stage 3): the exe goes to the game stats screen when there is one
                //      (cat_utyGameQuitToMainOrStats 0x005ab5b0, spMainScreen(1))
                utyGameQuitToMain(NULL,NULL);
                return TRUE;
            }
        }
    }
    else if (multiPlayerGame)
    {
        if (queueNumberOverruns(ProcessSyncPktQ) > 0)
        {
            bigmessageDisplay(strGetString(strSyncPktQOverrun),1);
            universe.quittime = universe.realtimeelapsed + WAIT_TIME_TO_QUIT;    // CAT: real time
        }
    }

    PTSLAB(0,"univupdate");
#ifdef OPTIMIZE_VERBOSE
    vecNormalizeCounter=0;
#endif
    if ((!singlePlayerGame) && (UNIVERSE_WOODPECKER(regenerateRUrate, 0)))
    {
        sdword i;
        Player *player;

        for (i=0;i<universe.numPlayers;i++)
        {
            player = &universe.players[i];

            if ((player->playerState != PLAYER_DEAD) && (player->resourceUnits < regenerateRUsIfUnder))
            {
                sdword before = player->resourceUnits;
                player->resourceUnits += regenerateRUs;
                if (player->resourceUnits > regenerateRUsIfUnder)
                {
                    player->resourceUnits = regenerateRUsIfUnder;

                }
                universe.gameStats.playerStats[player->playerIndex].totalRegeneratedResourceUnits += (player->resourceUnits - before);
            }
        }
    }

    if(tutorialdone)
    {
        // quit the game here
        spMainScreen();
        utyGameQuitToMain(NULL,NULL);
        return TRUE;
    }

    PTSLAB(1,"collblobs");

    if (firstTime || (UNIVERSE_WOODPECKER(REFRESH_COLLBLOB_RATE, REFRESH_COLLBLOB_FRAME)))
    {
        collUpdateCollBlobs();
    }
    else
    {
        if (UNIVERSE_WOODPECKER(REFRESH_COLLBLOB_RATE, REFRESH_COLLBLOB_BATTLEPING_FRAME))
        {
            pingBattlePingsCreate(&universe.collBlobList);
        }
        collUpdateObjsInCollBlobs();
    }

    PTEND(1);

    cloudSetFog();      //dust/gas cloud think
    if ((universe.univUpdateCounter % (UNIVERSE_UPDATE_RATE_FACTOR)) == 0) nebSetFog();        //nebula think

    //construction manager deterministic think
    cmDeterministicBuildProcess();

    // crappy fucking construction manager, no longer a task
    cmBuildTaskFunction();

    if (UNIVERSE_WOODPECKER(REFRESH_RESEARCH_RATE, REFRESH_RESEARCH_FRAME))
    {
        rmUpdateResearch();
    }

    if (!singlePlayerGame)
    {
        if ( (bitTest(tpGameCreated.flag,MG_ResourceInjections)) &&
             ((universe.univUpdateCounter-universe.lasttimeadded) == tpGameCreated.resourceInjectionInterval) )
        {
            universe.lasttimeadded = universe.univUpdateCounter;
            resourceInjectionTask();
            //speech event to notify of this:
            // CAT: Cataclysm's event 0x2001044 with var -1 (univUpdate 0x0054c3f0)
            if (universe.players[universe.curPlayerIndex].playerState != PLAYER_DEAD)
                speechEventFleet(0x2001044,-1,universe.curPlayerIndex);
        }

        if ( (bitTest(tpGameCreated.flag,MG_ResourceLumpSum)) &&
             (universe.univUpdateCounter == tpGameCreated.resourceLumpSumTime))
        {
            resourceLumpSum();
            if (universe.players[universe.curPlayerIndex].playerState != PLAYER_DEAD)
                speechEventFleet(0x2001044,-1,universe.curPlayerIndex);     // CAT
        }

        if (universe.univUpdateCounter == universe.players[sigsPlayerIndex].AllianceProposalsTimeout)
        {
            universe.players[sigsPlayerIndex].AllianceProposals = 0;
        }
    }

#ifdef COLLISION_CHECK_STATS
    shipsavoidingstuff = 0;
    shipsavoidedwalks = 0;
    shipsavoidedchecks = 0;
#endif

    //univZeroForcesOnResources();      // no longer needed - force, torque cleared after physUpdateObjPosVel acts on it
    //univZeroForcesOnDerelicts();
    //univZeroForcesOnMissiles();
    if (firstTime)
    {
        univSetupShipsForControl();     // only do it first time, since its done at end of univUpdateAllPosVel
        gameStatsCalcShipCostTotals();      // calculate the total ship cost at game start for the game stats
    }

    if (wkTradeStuffActive)
    {
        // this seems to update some velocities/angles -> update with correct rate
        if ((universe.univUpdateCounter % (UNIVERSE_UPDATE_RATE_FACTOR)) == 0)
        {
            wkTradeUpdate();
        }
    }

    if ((universe.univUpdateCounter % (UNIVERSE_UPDATE_RATE_FACTOR)) == 0)
    {
        if (singlePlayerGame)
            singlePlayerGameUpdate();
        else if (bitTest(tpGameCreated.flag,MG_Hyperspace))
            UpdateMidLevelHyperspacingShips();
    }

    PTSLAB(3,"collbulmis");
    collCheckAllBulletMissileCollisions();
    PTEND(3);

    PTSLAB(4,"compplayer");

    cratesUpdate();

    aiplayerUpdateAll();
    PTEND(4);

    PTSLAB(5,"cmdlayer");

    PTSLABLITTLE(2,"aiship");

    if (!nisUniversePause)
    {
        clProcess(&universe.mainCommandLayer);

        clPostProcess(&universe.mainCommandLayer);
    }

    PTENDLITTLE(2);

    // CAT: no global tactics update any more (the exe has no tacticsGlobalUpdate: retreat tracking and attack
    //      memory are not processed in univUpdate 0x0054c3f0)

    univUpdateMineWallFormations();

    PTEND(5);

    PTSLAB(7,"collbump");
    collCheckAllBumpCollisions();
    PTEND(7);

    PTSLAB(6,"updateobjpos");

    //old school function replaced with new functions below
    //univUpdateAllPosVel();       // also does a univCheckShipStates
    univUpdateAllPosVelMinorObjs();
    univUpdateAllPosVelBullets();
    univUpdateAllPosVelMissiles();
    if (!nisUniversePause)
    {

        univUpdateAllPosVelDerelicts();
        univUpdateAllPosVelResources();
        univUpdateAllPosVelShips();     // also does a univCheckShipStates
        // CAT: then the blasts and the bullet effects a load asked for (cat_blastUpdateAll 0x00516320 [M2-R4 combat];
        //      cat_univRecreateBulletEffectsIfNeeded 0x0054d330)
        cat_blastUpdateAll();
        cat_univRecreateBulletEffectsIfNeeded();

    }
    else
    {
        univPausedUpdateAllPosVelShips();   //certain operations only for when universe is paused
    }
    univUpdateAllPosVelEffects();   // MUST do effects sometime after ships+bullets and possibly other things that they may be attached to

    PTEND(6);

    univDeleteDeadShips();
    univDeleteDeadResources();
    univDeleteDeadDerelicts();
    univDeleteDeadMissiles();

    if (UNIVERSE_WOODPECKER(REGROW_RESOURCES_CHECK_RATE, REGROW_RESOURCES_CHECK_FRAME))
    {
        univCheckRegrowResources();
    }

    PTEND(0);

    if (singlePlayerGameLoadNewLevelFlag)
    {
        singlePlayerLoadNewLevel();
        if (hrAbortLoadingGame)
        {
            utyGameQuitToMain(NULL,NULL);
            hrAbortLoadingGame = FALSE;
            return TRUE;
        }
    }

    if(!universe.bounties || (UNIVERSE_WOODPECKER(CHECK_BOUNTIES_RATE, CHECK_BOUNTIES_FRAME)))
    {
        universe.bounties = TRUE;
        calculatePlayerBounties();
    }
    // CAT-TODO(stage 6): a KAS save request is carried out last (0x0054ce03: 0x00a43fec -> cat_kasSaveLevelNow)
    return FALSE;
}

// CAT-TODO(stage 3): univKillPlayer 0x0054ce20 takes a third argument, closes the construction, launch
//      and research managers and no longer writes the game stats file.
void univKillPlayer(sdword i,sdword playerdeathtype)
{
    char filename[50];

    if (gatherStats|showStatsFight|showStatsFancyFight)
        return;     // just gathering stats, don't want to do the player win thing

    if (universe.players[i].playerState == PLAYER_DEAD)
    {
        return;     // player already dead
    }

    if (i == MAX_MULTIPLAYER_PLAYERS)
    {
        return;     // special player for autoguns, just exit
    }

    universe.players[i].PlayerMothership = NULL;
    universe.players[i].playerState = PLAYER_DEAD;
    universe.players[i].sensorLevel = 2;
    universe.gameStats.playerStats[i].timeOfDeath = universe.totaltimeelapsed;
    universe.gameStats.playerStats[i].typeOfPlayerDeath = playerdeathtype;
    aiplayerPlayerDied(&universe.players[i]);
    if (i == universe.curPlayerIndex)
    {
        smGhostMode = TRUE;
    }

    /* This will only be played if its a Relic player that died */
    speechEventFleet(STAT_F_RelicPlayerDies, i, SOUND_EVENT_DEFAULT);

    // player i has lost the game
    if (!singlePlayerGame) DisplayPlayerHasBeenDefeatedMessage(i);
    DisableAllPlayersShips(i);
    if (universe.curPlayerIndex == i)
    {
        if (singlePlayerGame)
        {
            bigmessageDisplay(strGetString(strQuestIsLost),0);
        }
        else
        {
            if (playerdeathtype == PLAYERKILLED_DROPPEDOUT)
            {
                bigmessageDisplay(strGetString(strHaveBeenDroppedOut),0);
            }
            else
            {
                bigmessageDisplay(strGetString(strHaveBeenDefeated),0);
            }
        }
    }
    strcpy(filename,playerNames[i]);
    strcat(filename,strGetString(strStatsDiedStats));
    writeGameStatsToFile(filename);
}


/*-----------------------------------------------------------------------------
    Name        : cat_univPointInUniverse
    Description : CAT: is the point inside the universe box (+- smUniverseSize on each axis)? (cat_univPointInUniverse
                  0x005c1f70)
    Inputs      : pos
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 cat_univPointInUniverse(vector *pos)
{
    bool32 inside = (pos->x <= smUniverseSizeX);

    if (pos->x < -smUniverseSizeX)
    {
        inside = FALSE;
    }
    if (smUniverseSizeY < pos->y)
    {
        inside = FALSE;
    }
    if (pos->y < -smUniverseSizeY)
    {
        inside = FALSE;
    }
    if (smUniverseSizeZ < pos->z)
    {
        inside = FALSE;
    }
    if (pos->z < -smUniverseSizeZ)
    {
        return FALSE;
    }
    return inside;
}
