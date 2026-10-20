#include "LevelLoad.h"
#include "render.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AIPlayer.h"
#include "AITeam.h"
#include "Attributes.h"
#include "BTG.h"
#include "CommandLayer.h"
#include "Debug.h"
#include "FastMath.h"
#include "FOW.h"
#include "File.h"
#include "Globals.h"
#include "KAS.h"
#include "KASVar.h"
#include "Light.h"
#include "mainrgn.h"
#include "Memory.h"
#include "MultiplayerGame.h"
#include "Nebulae.h"
#include "OracleDump.h"     // HWI-TEST (skirmish-start): the skirmish self-test rides on the headless mission run's clock
#include "Ping.h"
#include "Randy.h"
#include "ResearchAPI.h"
#include "ShipSelect.h"
#include "ScenPick.h"
#include "Sensors.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "SpaceObj.h"
#include "StatScript.h"
#include "Teams.h"
#include "Tweak.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "utility.h"        // HWI-TEST (skirmish-start): the front end's start callbacks
#include "Vector.h"
#include "Collision.h"     // CAT (M2-R9 missions-02-09): the Mine / Slipgate readers
#include "ETG.h"     // CAT (M2-R9 missions-02-09): the Mine / Slipgate readers
#include "GameStats.h"     // CAT (M2-R9 missions-02-09): the Mine / Slipgate readers
#include "ShipDisable.h"     // CAT (M2-R9 missions-02-09): the Mine / Slipgate readers
#include "Slipgate.h"     // CAT (M2-R9 missions-02-09): the Mine / Slipgate readers
#include "Options.h"     // HWI-TEST (M2-R10 tests-leftovers): opCPUAttacks, the skirmish test's bigotry

#ifdef _MSC_VER
    #define strcasecmp _stricmp
#else
    #include <strings.h>
#endif

#define CapturableCapitalShipToGive StandardFrigate

udword nebAttributes = 0;
sdword SongNumber = -1;

typedef struct
{
    sdword playerNumber;
    vector position;
    real32 sphererotz;
} MissionSphereInfo;

ResourceDistribution resourceDistTemplate;

/*=============================================================================
    Private Function prototypes:
=============================================================================*/

static void scriptSetDerelictCB     (char *directory, char *field, void *dataToFillIn);
static void scriptSetResourcesCB    (char *directory, char *field, void *dataToFillIn);
static void scriptSetShipsCB        (char *directory, char *field, void *dataToFillIn);
static void scriptSetAIPointCB      (char *directory, char *field, void *dataToFillIn);
static void scriptSetAIPathCB       (char *directory, char *field, void *dataToFillIn);
static void scriptSetAIBoxCB        (char *directory, char *field, void *dataToFillIn);
static void scriptSetAISphereCB     (char *directory, char *field, void *dataToFillIn);
static void scriptSetMissionSphereCB(char *directory, char *field, void *dataToFillIn);
static void cat_scriptSetMeteorStormCB(char *directory, char *field, void *dataToFillIn);
static void cat_scriptSetMineCB     (char *directory, char *field, void *dataToFillIn);
static void cat_scriptSetSlipgateCB (char *directory, char *field, void *dataToFillIn);
// CAT (M2-R9 missions-02-09): 0x00c362dc, the slipgate read last that still waits for its partner (name chosen)
static Ship *catSlipgatePending = NULL;
static void scriptSetLightingCB     (char *directory, char *field, void *dataToFillIn);
static void scriptSetBackgroundCB   (char *directory, char *field, void *dataToFillIn);
static void scriptSetUword2CB       (char *directory, char *field, void *dataToFillIn);

/*=============================================================================
    Private Data:
=============================================================================*/

scriptStructEntry AsteroidDistScriptTable[] =
{
    { "Asteroid0", scriptSetUword2CB, &resourceDistTemplate.probResources[Asteroid0][0], &resourceDistTemplate },
    { "Asteroid1", scriptSetUword2CB, &resourceDistTemplate.probResources[Asteroid1][0], &resourceDistTemplate },
    { "Asteroid2", scriptSetUword2CB, &resourceDistTemplate.probResources[Asteroid2][0], &resourceDistTemplate },
    { "Asteroid3", scriptSetUword2CB, &resourceDistTemplate.probResources[Asteroid3][0], &resourceDistTemplate },
    { "Asteroid4", scriptSetUword2CB, &resourceDistTemplate.probResources[Asteroid4][0], &resourceDistTemplate },

    END_SCRIPT_STRUCT_ENTRY
};

scriptStructEntry DustCloudDistScriptTable[] =
{
    { "DustCloud0", scriptSetUword2CB, &resourceDistTemplate.probResources[DustCloud0][0], &resourceDistTemplate },
    { "DustCloud1", scriptSetUword2CB, &resourceDistTemplate.probResources[DustCloud1][0], &resourceDistTemplate },
    { "DustCloud2", scriptSetUword2CB, &resourceDistTemplate.probResources[DustCloud2][0], &resourceDistTemplate },
    { "DustCloud3", scriptSetUword2CB, &resourceDistTemplate.probResources[DustCloud3][0], &resourceDistTemplate },

    END_SCRIPT_STRUCT_ENTRY
};

scriptStructEntry GasCloudDistScriptTable[] =
{
    { "GasCloud0", scriptSetUword2CB, &resourceDistTemplate.probResources[GasCloud0][0], &resourceDistTemplate },
    { "GasCloud1", scriptSetUword2CB, &resourceDistTemplate.probResources[GasCloud1][0], &resourceDistTemplate },

    END_SCRIPT_STRUCT_ENTRY
};

scriptStructEntry MissionSphereScriptTable[] =
{
    { "Ships",     scriptSetShipsCB,     0, 0 },
    { "AIPoint",   scriptSetAIPointCB,   0, 0 },
    { "AIPath",    scriptSetAIPathCB,    0, 0 },
    { "AIBox",     scriptSetAIBoxCB,     0, 0 },
    { "AISphere",  scriptSetAISphereCB,  0, 0 },
    { "Resources", scriptSetResourcesCB, 0, 0 },
    { "MeteorStorm", cat_scriptSetMeteorStormCB, 0, 0 },     // CAT: 0x004a0d10, after "Resources" (table 0x00883aa8)
    // CAT (M2-R9 missions-02-09): the exe's table 0x00883a48 has "Mine" and "Slipgate" between "MeteorStorm" and
    //      "Derelict" (LiveFireExercise2 and other multiplayer maps, Mission03 / 10 / 12 / 14)
    { "Mine",      cat_scriptSetMineCB,      0, 0 },        // CAT: 0x004a3f20 (table 0x00883ab8)
    { "Slipgate",  cat_scriptSetSlipgateCB,  0, 0 },        // CAT: 0x004a1020 (table 0x00883ac8)
    { "Derelict",  scriptSetDerelictCB,  0, 0 },

    END_SCRIPT_STRUCT_ENTRY
};

scriptEntry SMMissionTweaks[] =
{
    makeEntry(smDepthCueRadius,      scriptSetReal32CB),
    makeEntry(smDepthCueStartRadius, scriptSetReal32CB),
    makeEntry(smCircleBorder,        scriptSetSdwordCB),
    makeEntry(smZoomMax,             scriptSetReal32CB),
    makeEntry(smZoomMin,             scriptSetReal32CB),
    makeEntry(smInitialDistance,     scriptSetReal32CB),
    makeEntry(smUniverseSizeX,       scriptSetReal32CB),
    makeEntry(smUniverseSizeY,       scriptSetReal32CB),
    makeEntry(smUniverseSizeZ,       scriptSetReal32CB),
    makeEntry(SongNumber,            scriptSetSdwordCB),
    
    END_SCRIPT_ENTRY
};

scriptStructEntry MissionScriptTable[] =
{
    { "MissionSphere", scriptSetMissionSphereCB, 0, 0 },
    { "Lighting",      scriptSetLightingCB,      0, 0 },
    { "Background",    scriptSetBackgroundCB,    0, 0 },
    // CAT (M2-R5 render): the mission script table 0x00883b80 also reads the lens flare and the fog layer
    { "LensFlare",     cat_scriptSetLensFlareCB, 0, 0 },
    { "Fog",           cat_scriptSetFogCB,       0, 0 },

    END_SCRIPT_STRUCT_ENTRY
};

static void llExcludeShip              (char *directory, char *field, void *dataToFillIn);
static void llExcludeAsteroid          (char *directory, char *field, void *dataToFillIn);
static void llExcludeDustCloud         (char *directory, char *field, void *dataToFillIn);
static void llExcludeGasCloud          (char *directory, char *field, void *dataToFillIn);
static void llExcludeNebula            (char *directory, char *field, void *dataToFillIn);
static void llExcludeDerelict          (char *directory, char *field, void *dataToFillIn);
static void llAvailableColorScheme     (char *directory, char *field, void *dataToFillIn);
static void scriptSetShipsToBeNeeded   (char *directory, char *field, void *dataToFillIn);
static void scriptSetDerelictToBeNeeded(char *directory, char *field, void *dataToFillIn);
static void scriptPreMissionSphereCB   (char *directory, char *field, void *dataToFillIn);

bool32 AnyShipOverlapsAsteroid(vector *position,AsteroidType asteroidtype);

scriptEntry MissionPreloadScriptTable[] =
{
    { "TrailColor",            teTrailColorSet,        NULL      },  //player #, control point #, R, G, B
#if !TO_STANDARD_COLORS
    { "TOColor",               teColorSet, &teColorSchemes[0].tacticalColor},         //player #, R, G, B
#endif
    { "BaseColor",             teColorSet, &teColorSchemes[0].textureColor.base   },  //player #, R, G, B
    { "StripeColor",           teColorSet, &teColorSchemes[0].textureColor.detail },  //player #, R, G, B

    { "ExcludeShips",          llExcludeShip,          NULL      },  //race, <shiptype|all]>
    { "IncludeShips",          llExcludeShip,         (void *) 1 },  //race, <shiptype|all]>
    { "ExcludeAsteroid",       llExcludeAsteroid,      NULL      },  //asteroidType
    { "IncludeAsteroid",       llExcludeAsteroid,     (void *) 1 },  //asteroidType
    { "ExcludeDustCloud",      llExcludeDustCloud,     NULL      },  //Type
    { "IncludeDustCloud",      llExcludeDustCloud,    (void *) 1 },  //Type
    { "ExcludeGasCloud",       llExcludeGasCloud,      NULL      },  //Type
    { "IncludeGasCloud",       llExcludeGasCloud,     (void *) 1 },  //Type
    { "ExcludeNebula",         llExcludeNebula,        NULL      },  //Type
    { "IncludeNebula",         llExcludeNebula,       (void *) 1 },  //Type
    { "ExcludeDerelict",       llExcludeDerelict,      NULL      },  //Type
    { "IncludeDerelict",       llExcludeDerelict,     (void *) 1 },  //Type
    { "AvailableColorSchemes", llAvailableColorScheme, NULL      },  //race <shiptype|all> scheme#[,scheme#[,...]]

    END_SCRIPT_ENTRY
};

scriptEntry MissionPreloadScriptTablePass2[] =
{
    { "MissionSphere", scriptPreMissionSphereCB, NULL },

    END_SCRIPT_ENTRY
};

// CAT: 0x00a447ac / 0x00a447b0: levelPreInit 0x004a3230 zeroes both before the MissionSphere pass and
//      scriptSetShipsToBeNeeded 0x004a2f30 only clears them (for a Kushan / Taiidan player's capturable ship), so the
//      multiplayer capture test after the pass (0x00a447b0 -> tStandardFrigate) never loads anything in the exe
bool32 needR1CapitalShip = FALSE;
bool32 needR2CapitalShip = FALSE;

scriptStructEntry MissionPreloadMissphereTable[] =
{
    { "Ships",    scriptSetShipsToBeNeeded,    0, 0 },
    { "Derelict", scriptSetDerelictToBeNeeded, 0, 0 },

    END_SCRIPT_STRUCT_ENTRY
};

bool32 missionman = FALSE;

/*=============================================================================
    Functions:
=============================================================================*/

static void scriptSetUword2CB(char *directory,char *field,void *dataToFillIn)
{
    sdword fillin[2] = { -1,-1 };

    RemoveCommasFromString(field);

    sscanf(field,"%d %d",&fillin[0],&fillin[1]);

    if (fillin[1] == -1)
    {
        fillin[1] = fillin[0];
    }

    ((uword *)dataToFillIn)[0] = (uword)fillin[0];
    ((uword *)dataToFillIn)[1] = (uword)fillin[1];
}

#define CAT_UNIVERSE_SIZE_MARGIN    5000.0f     // CAT: 0x0072a618 (M2-R6 nis-fow-camera)

static void SetSMMissionTweaks(char *directory,char *filename)
{
    // set to defaults in case .level file doesn't define them
    smDepthCueRadius = SM_DepthCueRadius;
    smDepthCueStartRadius = SM_DepthCueStartRadius;
    smCircleBorder = SM_CircleBorder;
    smZoomMax = SM_ZoomMax;
    smZoomMin = SM_ZoomMin;

    SongNumber = -1;

    scriptSet(directory,filename,SMMissionTweaks);

    // CAT (M2-R6 nis-fow-camera): the universe box grows by 5000 on each axis before the sensors parameters
    //      (levelInit 0x004a3a10 and levelStartNext 0x004a3c80 inline this function: 0x0072a618 = 5000.0); the
    //      fog-of-war grid covers the grown box
    smUniverseSizeX += CAT_UNIVERSE_SIZE_MARGIN;
    smUniverseSizeY += CAT_UNIVERSE_SIZE_MARGIN;
    smUniverseSizeZ += CAT_UNIVERSE_SIZE_MARGIN;

    // later, put properly into sensors.c
    smUpdateParameters();
}

static void GetResourceDistribution(ResourceDistribution *resourceDistribution,char *directory,char *name,ObjType resourceobjtype)
{
    sdword i,j;
    uword cum;

    memset(resourceDistribution,0,sizeof(ResourceDistribution));

    switch (resourceobjtype)
    {
        case OBJ_AsteroidType:
            scriptSetStruct(directory,name,AsteroidDistScriptTable,(ubyte *)resourceDistribution);
            break;

        case OBJ_DustType:
            scriptSetStruct(directory,name,DustCloudDistScriptTable,(ubyte *)resourceDistribution);
            break;

        case OBJ_GasType:
            scriptSetStruct(directory,name,GasCloudDistScriptTable,(ubyte *)resourceDistribution);
            break;

        default:
            dbgFatalf(DBG_Loc,"Unknown Object Type %d for resource type",resourceobjtype);
            break;
    }

    for (j=0;j<2;j++)
    {
        for (i=0,cum=0;i<MAXNUM_RESOURCETYPES;i++)
        {
            cum += resourceDistribution->probResources[i][j];
            resourceDistribution->cumResources[i][j] = cum;
        }
        if (cum == 0)
        {
            dbgFatalf(DBG_Loc,"\nNo distribution set for %s",ObjTypeToStr(resourceobjtype));
        }
        resourceDistribution->sumTotal[j] = cum;
    }
}

udword GetRandomResourceType(ResourceDistribution *resourceDistribution,bool32 regrowing)
{
    udword randnum;
    udword i;
    udword index = 0;

    if (regrowing)
    {
        index = 1;
    }

    randnum = randomG(resourceDistribution->sumTotal[index]);

    for (i=0;i<MAXNUM_RESOURCETYPES;i++)
    {
        if (randnum < resourceDistribution->cumResources[i][index])
        {
            return i;
        }
    }
    dbgAssertOrIgnore(FALSE);
    return 0;
}

void AddRandomRotationToAsteroid(Asteroid *asteroid)
{
    dbgAssertOrIgnore(asteroid->asteroidtype < NUM_ASTEROIDTYPES);
    if (asteroid->asteroidtype >= Asteroid3)
    if (((udword)(gamerand() & 255)) <= ASTEROID_ROTATE_PROB)
    {
        real32 lorotate = ASTEROID_LO_ROTATE[asteroid->asteroidtype];
        real32 hirotate = ASTEROID_HI_ROTATE[asteroid->asteroidtype];
        asteroid->rotatable.rotinfo.rotspeed.x = frandombetween(lorotate,hirotate);
        asteroid->rotatable.rotinfo.rotspeed.y = frandombetween(lorotate,hirotate);
        asteroid->rotatable.rotinfo.rotspeed.z = frandombetween(lorotate,hirotate);
    }
}

void AddRandomVelocityTowards(SpaceObj *obj,vector *from,vector *towards,real32 avgVel,real32 deviation)
{
    real32 dev2 = deviation*2.0f;
    vector devvector;
    vector diff;

    devvector.x = frandom(dev2) - deviation;
    devvector.y = frandom(dev2) - deviation;
    devvector.z = frandom(dev2) - deviation;

    vecSub(diff,*towards,*from);
    vecNormalize(&diff);
    vecAddTo(diff,devvector);

    vecMultiplyByScalar(diff,avgVel);

    obj->posinfo.velocity = diff;
    SET_MOVING_LINEARLY(obj->posinfo.isMoving);
}

void AddRandomVelocityDirection(SpaceObj *obj,vector *direction,real32 avgVel,real32 deviation)
{
    //real32 dev2 = deviation*2.0f;
    vector devvector;

    devvector.x = 0;//frandom(dev2) - deviation;
    devvector.y = 0;//frandom(dev2) - deviation;
    devvector.z = 0;//frandom(dev2) - deviation;

    vecNormalize(direction);
    vecAddTo(*direction,devvector);

    vecMultiplyByScalar(*direction,avgVel);

    obj->posinfo.velocity = *direction;
    SET_MOVING_LINEARLY(obj->posinfo.isMoving);
}

bool32 AddResourceToVolumeAtPosition(struct ResourceVolume *volume,bool32 regrowing,vector *position)
{
    switch (volume->resourceObjType)
    {
        case OBJ_AsteroidType:
        {
            Asteroid *asteroid;
            AsteroidType asteroidtype;
            asteroidtype = (AsteroidType)GetRandomResourceType(&volume->resourceDistribution,regrowing);
            dbgAssertOrIgnore(asteroidtype < NUM_ASTEROIDTYPES);

            if (regrowing)
            {
                // may not actually add asteroid if it overlaps...
                if (AnyShipOverlapsAsteroid(position,asteroidtype))
                {
                    return FALSE;
                }
            }

            asteroid = univAddAsteroid(asteroidtype,position);
            asteroid->obj.attributes = volume->attributes;
            asteroid->obj.attributesParam = volume->attributesParam;
            if (volume->attributes & ATTRIBUTES_Regrow)
            {
                asteroid->resource.resourceVolume = volume;
            }
            AddRandomRotationToAsteroid(asteroid);
            // CAT (M2-R8 kas-runtime-mission01): AddResourceToVolumeAtPosition 0x0049fcf0: the velocity towards the
            //      "TowardsVector" point is that direction at attrVELOCITY_TOWARDS exactly (no random deviation), the
            //      asteroid marked moving; then Asteroid +0x1A0 is cleared; HW1's head-shot velocity is gone: 0x200 is
            //      Cataclysm's meteor-storm drift (CAT_ATTRIBUTES_KillerCollision, set by cat_scriptSetMeteorStormCB):
            //      the volume's axis (1, 0, 0) turned about y by roty and about z by rotz, normalised, times the drift
            //      speed, plus the two added terms on y and x, and the wrap box the larger of the universe's half size
            //      and the volume's extents per axis; such an asteroid gets a second chance at a random rotation
            if (volume->attributes & ATTRIBUTES_VelToMothership)
            {
                hvector dirvector;

                GetPointOfName(&dirvector,"TowardsVector");
                vecNormalize((vector *)&dirvector);
                asteroid->obj.posinfo.velocity.x = dirvector.x * attrVELOCITY_TOWARDS;
                asteroid->obj.posinfo.velocity.y = dirvector.y * attrVELOCITY_TOWARDS;
                asteroid->obj.posinfo.velocity.z = dirvector.z * attrVELOCITY_TOWARDS;
                asteroid->obj.posinfo.isMoving = TRUE;
            }
            asteroid->catAsteroidReserved1A0 = 0;               // CAT: 0x004a0012
            if (volume->attributes & CAT_ATTRIBUTES_KillerCollision)
            {
                matrix rotymat, rotzmat;
                vector axis = { 1.0f, 0.0f, 0.0f };
                vector turned, direction;

                matMakeRotAboutY(&rotymat,(real32)cos(volume->roty),(real32)sin(volume->roty));
                matMakeRotAboutZ(&rotzmat,(real32)cos(volume->rotz),(real32)sin(volume->rotz));
                matMultiplyMatByVec(&turned,&rotymat,&axis);
                matMultiplyMatByVec(&direction,&rotzmat,&turned);
                vecNormalize(&direction);
                asteroid->obj.posinfo.velocity.x = volume->catDriftSpeed * direction.x;
                asteroid->obj.posinfo.isMoving = TRUE;
                asteroid->obj.posinfo.velocity.y = volume->catDriftSpeed * direction.y;
                asteroid->obj.posinfo.velocity.z = volume->catDriftSpeed * direction.z;
                asteroid->obj.posinfo.velocity.y = volume->catDriftAddY + asteroid->obj.posinfo.velocity.y;
                asteroid->obj.posinfo.velocity.x = volume->catDriftAddX + asteroid->obj.posinfo.velocity.x;
                asteroid->catWrapExtent.x = (volume->catWrapExtent.x > smUniverseSizeX) ? volume->catWrapExtent.x : smUniverseSizeX;
                asteroid->catWrapExtent.y = (volume->catWrapExtent.y > smUniverseSizeY) ? volume->catWrapExtent.y : smUniverseSizeY;
                asteroid->catWrapExtent.z = (volume->catWrapExtent.z > smUniverseSizeZ) ? volume->catWrapExtent.z : smUniverseSizeZ;
                AddRandomRotationToAsteroid(asteroid);
            }
        }
        break;

        case OBJ_DustType:
        {
            DustCloud *dustcloud;
            DustCloudType dusttype;
            dusttype = (DustCloudType)GetRandomResourceType(&volume->resourceDistribution,regrowing);
            dbgAssertOrIgnore(dusttype < NUM_DUSTCLOUDTYPES);
            dustcloud = univAddDustCloud(dusttype,position);
            dustcloud->obj.attributes = volume->attributes;
            dustcloud->obj.attributesParam = volume->attributesParam;
            if (dustcloud->obj.attributes & ATTRIBUTES_Regrow)
            {
                dustcloud->resource.resourceVolume = volume;
            }
        }
        break;

        case OBJ_GasType:
        {
            GasCloud *gascloud;
            GasCloudType gastype;
            gastype = (GasCloudType)GetRandomResourceType(&volume->resourceDistribution,regrowing);
            dbgAssertOrIgnore(gastype < NUM_GASCLOUDTYPES);
            gascloud = univAddGasCloud(gastype,position);
            gascloud->obj.attributes = volume->attributes;
            gascloud->obj.attributesParam = volume->attributesParam;
            if (gascloud->obj.attributes & ATTRIBUTES_Regrow)
            {
                gascloud->resource.resourceVolume = volume;
            }
        }
        break;
    default:
        break;
    }

    return TRUE;
}

bool32 AddResourceToRectangle(struct ResourceVolume *rectangle,bool32 regrowing)
{
    real32 width = rectangle->radius;
    real32 length = rectangle->length;
    real32 height = rectangle->roty;
    real32 rotaboutz = rectangle->rotz;

    matrix rotzmat;
    vector position1;
    vector position2;
    vector finalpos;

    dbgAssertOrIgnore(rectangle->resourceVolumeType == ResourceRectangleType);

    position1.x = frandombetween(-width,width);
    position1.y = frandombetween(-length,length);
    position1.z = frandombetween(-height,height);

    matMakeRotAboutZ(&rotzmat,(real32)cos(rotaboutz),(real32)sin(rotaboutz));

    matMultiplyMatByVec(&position2,&rotzmat,&position1);

    vecAdd(finalpos,position2,rectangle->centre);

    return AddResourceToVolumeAtPosition(rectangle,regrowing,&finalpos);
}

static void AddRectangle(ResourceVolume *rectangle)
{
    sdword i;

    dbgAssertOrIgnore(rectangle->resourceVolumeType == ResourceRectangleType);

    for (i=0;i<rectangle->number;i++)
    {
        AddResourceToRectangle(rectangle,FALSE);
    }

    if (rectangle->attributes & ATTRIBUTES_Regrow)
    {
        listAddNode(&universe.ResourceVolumeList,&rectangle->link,rectangle);
    }
    else
    {
        memFree(rectangle);
    }
}

bool32 AddResourceToCylinder(struct ResourceVolume *cylinder,bool32 regrowing)
{
    real32 rotabouty = cylinder->roty;
    real32 rotaboutz = cylinder->rotz;
    real32 radius = cylinder->radius;
    real32 length = cylinder->length;
    real32 lengthDiv2 = length * 0.5f;
    real32 diameter = radius * 2.0f;

    matrix rotymat;
    matrix rotzmat;
    vector position1;
    vector position2;

    vector finalpos;

    dbgAssertOrIgnore(cylinder->resourceVolumeType == ResourceCylinderType);

    matMakeRotAboutY(&rotymat,(real32)cos(rotabouty),(real32)sin(rotabouty));
    matMakeRotAboutZ(&rotzmat,(real32)cos(rotaboutz),(real32)sin(rotaboutz));

    // CAT (M2-R8 kas-runtime-mission01): the volume's half extents, the corner (length / 2, radius, radius) turned as
    //      the volume, per axis without sign (the meteor storm's wrap box; AddResourceToCylinder 0x004a03e0)
    position1.x = lengthDiv2;
    position1.y = radius;
    position1.z = radius;
    matMultiplyMatByVec(&position2,&rotymat,&position1);
    matMultiplyMatByVec(&position1,&rotzmat,&position2);
    cylinder->catWrapExtent.x = (position1.x < 0.0f) ? -position1.x : position1.x;
    cylinder->catWrapExtent.y = (position1.y < 0.0f) ? -position1.y : position1.y;
    cylinder->catWrapExtent.z = (position1.z < 0.0f) ? -position1.z : position1.z;

    position1.x = frandom(length) - lengthDiv2;
    position1.y = frandom(diameter) - radius;
    position1.z = frandom(diameter) - radius;

    matMultiplyMatByVec(&position2,&rotymat,&position1);
    matMultiplyMatByVec(&position1,&rotzmat,&position2);

    vecAdd(finalpos,position1,cylinder->centre);

    return AddResourceToVolumeAtPosition(cylinder,regrowing,&finalpos);
}

static void AddCylinder(ResourceVolume *cylinder)
{
    sdword i;

    dbgAssertOrIgnore(cylinder->resourceVolumeType == ResourceCylinderType);

    for (i=0;i<cylinder->number;i++)
    {
        AddResourceToCylinder(cylinder,FALSE);
    }

    if (cylinder->attributes & ATTRIBUTES_Regrow)
    {
        listAddNode(&universe.ResourceVolumeList,&cylinder->link,cylinder);
    }
    else
    {
        memFree(cylinder);
    }
}

bool32 AddResourceToSphere(struct ResourceVolume *sphere,bool32 regrowing)
{
    real32 diameter;
    real32 radius;
    vector randpos;

    dbgAssertOrIgnore(sphere->resourceVolumeType == ResourceSphereType);

    diameter = sphere->radius * 2.0f;
    radius = sphere->radius;

    if (sphere->attributes & ATTRIBUTES_ShellOfResources)
    {
        real32 width = radius * attrRING_WIDTH_SCALE;
        real32 r = frandombetween(width,radius);
        real32 ang = frandom(TWOPI);
        real32 decl = frandom(DEG_TO_RAD(160.0f)) - DEG_TO_RAD(80.0f);
        matrix roty,rotz;
        vector temp,temp2;

        vecSet(temp,r,0.0f,0.0f);
        matMakeRotAboutY(&roty,(real32)cos(decl),(real32)sin(decl));
        matMakeRotAboutZ(&rotz,(real32)cos(ang),(real32)sin(ang));
        matMultiplyMatByVec(&temp2,&roty,&temp);
        matMultiplyMatByVec(&randpos,&rotz,&temp2);
    }
    else
    {
        randpos.x = frandom(diameter) - radius;
        randpos.y = frandom(diameter) - radius;
        randpos.z = frandom(diameter) - radius;
    }

    vecAddTo(randpos,sphere->centre);

    return AddResourceToVolumeAtPosition(sphere,regrowing,&randpos);
}

static void AddSphere(ResourceVolume *sphere)
{
    sdword i;

    dbgAssertOrIgnore(sphere->resourceVolumeType == ResourceSphereType);

    for (i=0;i<sphere->number;i++)
    {
        AddResourceToSphere(sphere,FALSE);
    }

    if (sphere->attributes & ATTRIBUTES_Regrow)
    {
        listAddNode(&universe.ResourceVolumeList,&sphere->link,sphere);
    }
    else
    {
        memFree(sphere);
    }
}

static void scriptSetResourcesCB(char *directory,char *field,void *dataToFillIn)
{
    MissionSphereInfo *missionSphereInfo = (MissionSphereInfo *)dataToFillIn;
    vector position;
    real32 radius;
    char resourcetypestr[50];
    char layoutstr[50];
    char distributionstr[50];
    udword numAsteroids;
    real32 length;
    real32 roty,rotz;
    ResourceVolume *resourceVolume;
    matrix rotaboutz;
    vector rotatedposition;
    ObjType resourceobjtype;
    udword attributes = 0;
    sdword attributesParam = 0;

    RemoveCommasFromString(field);

    sscanf(field,"%s %f %f %f %s %s %d %f %f %f %f %d %d",resourcetypestr,&position.x,&position.y,&position.z,layoutstr,distributionstr,
                                              &numAsteroids,&radius,&length,&roty,&rotz,&attributes,&attributesParam);

    dbgAssertOrIgnore(attributesParam <= SWORD_Max && attributesParam >= SWORD_Min);
    if ((distributionstr[0] == '0') && (distributionstr[1] == 0))
    {
        dbgFatalf(DBG_Loc,"No distribution specified in %s %s",directory,field);
    }

    //roty = DEG_TO_RAD(roty);  not used as angle in every case, delay DEG_TO_RAD till later
    rotz = DEG_TO_RAD(rotz);

    resourceobjtype = StrToObjType(resourcetypestr);

    if (!missionman)
    {
        matMakeRotAboutZ(&rotaboutz,(real32)cos(missionSphereInfo->sphererotz),(real32)sin(missionSphereInfo->sphererotz));

        matMultiplyMatByVec(&rotatedposition,&rotaboutz,&position);
        vecAddTo(rotatedposition,missionSphereInfo->position);

        rotz += missionSphereInfo->sphererotz;
    }
    else
    {
        rotatedposition = position;
    }

    if (singlePlayerGame && bitTest(attributes, ATTRIBUTES_ScaleResources))
    {
        // scale the number of asteroids depending on fleet strength
        // CAT (M2-R8 kas-runtime-mission01): the x87 shape of scriptSetResourcesCB 0x004a07b0 (0x004a0917 .. 0x004a094b):
        //      the count times the modifier in extended precision, truncated once (__ftol), with no rounding of the
        //      product to a float (HW1 multiplied two floats); a double is exact for these operands
        if (bitTest(attributes, ATTRIBUTES_KillerCollDamage))
        {
            // scale up
            numAsteroids = (udword)((real64)numAsteroids * (real64)spFleetModifier);
        }
        else
        {
            // scale down
            numAsteroids = (udword)((1.0 - (real64)spFleetModifier) * (real64)(sdword)numAsteroids);
        }
    }

    if (strcmp(layoutstr,"Sphere") == 0)
    {
        resourceVolume = memAlloc(sizeof(ResourceVolume),"ResourceSphere",NonVolatile);

        resourceVolume->resourceVolumeType = ResourceSphereType;
        resourceVolume->centre = rotatedposition;
        resourceVolume->radius = radius;
        resourceVolume->number = numAsteroids;
        resourceVolume->actualnumber = numAsteroids;
        resourceVolume->attributes = attributes;
        resourceVolume->attributesParam = (sword)attributesParam;
        resourceVolume->resourceObjType = resourceobjtype;
        GetResourceDistribution(&resourceVolume->resourceDistribution,directory,distributionstr,resourceobjtype);

        AddSphere(resourceVolume);
    }
    else if (strcmp(layoutstr,"Cylinder") == 0)
    {
        resourceVolume = memAlloc(sizeof(ResourceVolume),"ResourceCyl",NonVolatile);

        resourceVolume->resourceVolumeType = ResourceCylinderType;
        resourceVolume->centre = rotatedposition;
        resourceVolume->radius = radius;
        resourceVolume->length = length;
        resourceVolume->roty = DEG_TO_RAD(roty);
        resourceVolume->rotz = rotz;
        resourceVolume->number = numAsteroids;
        resourceVolume->actualnumber = numAsteroids;
        resourceVolume->attributes = attributes;
        resourceVolume->attributesParam = (sword)attributesParam;
        resourceVolume->resourceObjType = resourceobjtype;
        GetResourceDistribution(&resourceVolume->resourceDistribution,directory,distributionstr,resourceobjtype);

        AddCylinder(resourceVolume);
    }
    else if (strcmp(layoutstr,"Rectangle") == 0)
    {
        resourceVolume = memAlloc(sizeof(ResourceVolume),"ResourceRect",NonVolatile);

        resourceVolume->resourceVolumeType = ResourceRectangleType;
        resourceVolume->centre = rotatedposition;
        resourceVolume->radius = radius;
        resourceVolume->length = length;
        resourceVolume->roty = roty;
        resourceVolume->rotz = rotz;
        resourceVolume->number = numAsteroids;
        resourceVolume->actualnumber = numAsteroids;
        resourceVolume->attributes = attributes;
        resourceVolume->attributesParam = (sword)attributesParam;
        resourceVolume->resourceObjType = resourceobjtype;
        GetResourceDistribution(&resourceVolume->resourceDistribution,directory,distributionstr,resourceobjtype);

        AddRectangle(resourceVolume);
    }
    else if (strcmp(layoutstr,"Nebula") == 0)
    {
        vector dimension;

        dimension.x = radius;
        dimension.y = length;
//        dimension.z = 10000.0f;//DEG_TO_RAD(roty);
        dimension.z = roty;
        nebAttributes = attributes;
        nebGo(&rotatedposition, &dimension, /*(udword)rotz*/10000, numAsteroids);
        nebAttributes = 0;
    }
    else
    {
        dbgFatalf(DBG_Loc,"Unknown layout found in %s",field);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_scriptSetMeteorStormCB                        0x004a0d10
    Description : CAT (M2-R8 kas-runtime-mission01): a mission sphere's "MeteorStorm" line (Mission02 and the
                  multiplayer maps' resource spheres): x, y, z, layout (Sphere or Cylinder), distribution file,
                  number, radius, length, rot y, rot z (degrees), attributes, drift speed, added y and x velocity
                  ("%f %f %f %s %s %d %f %f %f %f %d %f %f %f"). Its asteroids drift at the speed along the volume's
                  axis and wrap in a box (CAT_ATTRIBUTES_KillerCollision is set in the attributes); the volume is kept
                  only when it regrows (ATTRIBUTES_Regrow)
----------------------------------------------------------------------------*/
static void cat_scriptSetMeteorStormCB(char *directory,char *field,void *dataToFillIn)
{
    MissionSphereInfo *missionSphereInfo = (MissionSphereInfo *)dataToFillIn;
    vector position, rotatedposition;
    real32 roty, rotz;
    char layoutstr[64];
    char distributionstr[52];
    sdword number = 0;
    real32 radius = 0.0f, length = 0.0f;
    udword attributes = 0;
    real32 speed = 0.0f, addY = 0.0f, addX = 0.0f;   // CAT-FIX: the exe leaves the ones a "?" ends early uninitialised
    ResourceVolume *volume;
    matrix rotaboutz;
    sdword i;

    RemoveCommasFromString(field);
    sscanf(field, "%f %f %f %s %s %d %f %f %f %f %d %f %f %f", &position.x, &position.y, &position.z, layoutstr,
           distributionstr, &number, &radius, &length, &roty, &rotz, &attributes, &speed, &addY, &addX);
    if ((distributionstr[0] == '0') && (distributionstr[1] == '\0'))
    {
        dbgFatalf(DBG_Loc, "No distribution specified in %s %s", directory, field);     // the exe's text (0x00884110)
    }
    roty = DEG_TO_RAD(roty);
    rotz = DEG_TO_RAD(rotz);
    if (!missionman)
    {
        matMakeRotAboutZ(&rotaboutz,(real32)cos(missionSphereInfo->sphererotz),(real32)sin(missionSphereInfo->sphererotz));
        matMultiplyMatByVec(&rotatedposition,&rotaboutz,&position);
        vecAddTo(rotatedposition,missionSphereInfo->position);
        rotz += missionSphereInfo->sphererotz;
    }
    else
    {
        rotatedposition = position;
    }

    volume = memAlloc(sizeof(ResourceVolume), "MeteorStorm", NonVolatile);
    if (strcmp(layoutstr, "Sphere") == 0)
    {
        volume->resourceVolumeType = ResourceSphereType;
    }
    else if (strcmp(layoutstr, "Cylinder") == 0)
    {
        volume->resourceVolumeType = ResourceCylinderType;
    }
    else
    {
        dbgFatalf(DBG_Loc, "Invalid meteorstorm layout (must be cylinder or sphere) in %s.", field);   // the exe's text (0x00884198)
    }
    volume->centre = rotatedposition;
    volume->radius = radius;
    volume->length = length;
    volume->roty = roty;
    volume->rotz = rotz;
    volume->catDriftSpeed = speed;
    volume->catDriftAddY = addY;
    volume->catDriftAddX = addX;
    // CAT-FIX: the exe never sets a sphere's wrap box (only AddResourceToCylinder does): zero, the universe's half size wins
    volume->catWrapExtent.x = volume->catWrapExtent.y = volume->catWrapExtent.z = 0.0f;
    volume->number = number;
    volume->actualnumber = number;
    volume->attributes = attributes | CAT_ATTRIBUTES_KillerCollision;
    volume->attributesParam = 0;
    volume->resourceObjType = OBJ_AsteroidType;
    GetResourceDistribution(&volume->resourceDistribution, directory, distributionstr, OBJ_AsteroidType);
    if (volume->resourceVolumeType == ResourceSphereType)
    {
        for (i = 0; i < volume->number; i++)
        {
            AddResourceToSphere(volume, FALSE);
        }
    }
    else
    {
        for (i = 0; i < volume->number; i++)
        {
            AddResourceToCylinder(volume, FALSE);
        }
    }
    if (volume->attributes & ATTRIBUTES_Regrow)
    {
        listAddNode(&universe.ResourceVolumeList, &volume->link, volume);
    }
    else
    {
        memFree(volume);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_scriptSetMineCB                               0x004a3f20
    Description : CAT (M2-R9 missions-02-09): a mission sphere's "Mine" line (x, y, z, rotation in degrees:
                  "%f %f %f %f", 0x0088456c): a mine of the derelict race (mineStaticInfos[RACE_Derelict], 0x00b3b2e8)
                  owned by player 1 in a single-player game and by player 7 in a multiplayer one, made in place as
                  univAddMissile 0x00540c70 makes a missile (the exe inlines it): a mine (MISSILE_Mine) with no owner
                  ship, target or trail, the mine launcher's sound type, 5000 damage, a lifetime and camera distance
                  of REALlyBig and the hit effect of its player's race's missile launcher; not in a formation
                  (mineAIState 0). The exe turns the point by the sphere and its rotation (0x004a3f7c .. 0x004a3fe1)
                  but places the mine at the point as read (univInitSpaceObjPosRot at 0x004a41e8 takes the scanned
                  vector) and never uses the rotation: so here
----------------------------------------------------------------------------*/
static void cat_scriptSetMineCB(char *directory, char *field, void *dataToFillIn)
{
    struct Player *player = singlePlayerGame ? &universe.players[1] : &universe.players[7];
    vector position;
    real32 rot;
    Missile *mine;
    sdword i;

    RemoveCommasFromString(field);
    sscanf(field, "%f %f %f %f", &position.x, &position.y, &position.z, &rot);
    (void)rot;                                              // read and turned to radians, never used (the exe's)

    mine = memAlloc(sizeof(Missile), "Mine", 0);           // 0x258 bytes (memAllocFunction 0x004adee0)
    memset(mine, 0, sizeof(Missile));
    mine->header.objtype = OBJ_MissileType;
    mine->header.flags = SOF_Rotatable + SOF_Impactable + SOF_NotBumpable + SOF_Targetable;
    mine->enginePosition.x = 0.0f;
    mine->enginePosition.y = 0.0f;
    mine->enginePosition.z = 0.0f;
    mine->trail = NULL;
    mine->race = RACE_Derelict;
    mine->staticinfo = &mineStaticInfos[RACE_Derelict];
    mine->missileType = MISSILE_Mine;
    mine->catMissileReserved214[0] = RACE_Derelict;         // +0x214: the race again (0x004a4068)
    mine->mineAIState = 0;
    mine->FORCE_DROPPED = FALSE;
    mine->formationinfo = NULL;
    // the static top speed over one 16 Hz update (0x0072aae8 = 0.0625; univAddMissile multiplies by phystimeelapsed)
    mine->maxtraveldist = mine->staticinfo->common.maxvelocity * 0.0625f;
    mine->playerowner = player;
    mine->haveCheckedForLoss = 0;

    ClearNode(mine->obj.renderlink);
    mine->obj.currentLOD = 0;
    mine->obj.cameraDistanceSquared = REALlyBig;            // 0x6258d727
    mine->obj.attributes = 0;
    mine->obj.attributesParam = 0;
    mine->obj.collMyBlob = NULL;
    mine->targetable.health = mine->staticinfo->health.maxhealth;
    mine->targetable.flashtimer = 0;
    mine->targetable.lasttimecollided = 0;
    vecZeroVector(mine->obj.posinfo.force);
    vecZeroVector(mine->rotatable.rotinfo.torque);
    for (i = 0; i < NUM_TRANS_DEGOFFREEDOM; i++)
    {
        mine->guidance.nonstatvars.thruststrength[i] = mine->staticinfo->guidance.thruststrengthstat[i];
    }
    for (i = 0; i < NUM_ROT_DEGOFFREEDOM; i++)
    {
        mine->guidance.nonstatvars.rotstrength[i] = mine->staticinfo->guidance.rotstrengthstat[i];
    }
    for (i = 0; i < NUM_TURN_TYPES; i++)
    {
        mine->guidance.nonstatvars.turnspeed[i] = mine->staticinfo->guidance.turnspeedstat[i];
    }
    mine->guidance.nonstatvars.catMaxVelocity = mine->staticinfo->common.maxvelocity;
    mine->totallifetime = REALlyBig;
    mine->soundType = GS_MineLauncher;
    mine->timelived = 0.0f;
    mine->owner = NULL;
    mine->target = NULL;
    mine->hitEffect = etgGunEventTable[player->race][GS_MissleLauncher][EGT_GunHit];    // 0x00c41238 + race * 0x190
    mine->damage = 5000.0f;                                 // 0x459c4000

    mine->missileID.missileNumber = universe.missileNumber;
    if (multiPlayerGame)                                    // 0x00a43c94 only (univAddMissile also tests the recorder)
    {
        IDToPtrTableAdd(&MissileIDToPtr, universe.missileNumber, (SpaceObj *)mine);
    }
    universe.missileNumber++;

    univInitSpaceObjPosRot((SpaceObj *)mine, &position, FALSE);
    mine->obj.posinfo.isMoving = 1;                         // 0x004a41f8 / 0x004a41fc: both words set to 1
    mine->obj.posinfo.haventCalculatedDist = 1;
    collAddSpaceObjToCollBlobs((SpaceObj *)mine);
    listAddNode(&universe.SpaceObjList, &(mine->header.objlink), mine);
    listAddNode(&universe.ImpactableList, &(mine->impactable.impactablelink), mine);
    listAddNode(&universe.MissileList, &(mine->missilelink), mine);
    cat_univShipMeshStateCreate((SpaceObj *)mine);
}

/*-----------------------------------------------------------------------------
    Name        : cat_scriptSetSlipgateCB                           0x004a1020
    Description : CAT (M2-R9 missions-02-09): a mission sphere's "Slipgate" line ("%f %f %f %d/%d %f %s",
                  0x0088423c: x, y, z, two numbers the exe reads and never uses, rotation in degrees, ship type): the
                  gate is made for the sphere's player with the derelict race (univCreateShip 0x00542320 with race 6),
                  counted in the game statistics and turned by its rotation; gates pair up in the order they are read
                  (each odd one waits in 0x00c362dc, which levelPreInit clears, for the next: each becomes the other's
                  partner, cat_slipgateSetPartner 0x005c71b0); a gate is neither targetable nor bumpable, and disabled
                  (cat_shipSetDisableReason(gate, 1, 6))
----------------------------------------------------------------------------*/
static void cat_scriptSetSlipgateCB(char *directory, char *field, void *dataToFillIn)
{
    MissionSphereInfo *missionSphereInfo = (MissionSphereInfo *)dataToFillIn;
    struct Player *player = &universe.players[missionSphereInfo->playerNumber];
    vector position, rotatedposition;
    sdword unusedA, unusedB;
    real32 rot;
    char shiptypestr[52];
    ShipType shiptype;
    matrix rotaboutz;
    Ship *gate;

    RemoveCommasFromString(field);
    if (sscanf(field, "%f %f %f %d/%d %f %s", &position.x, &position.y, &position.z, &unusedA, &unusedB, &rot,
               shiptypestr) != 7)
    {
        dbgFatalf(DBG_Loc, "Bad Slipgate point definition: %s", field);   // the exe's text (0x00884218)
    }
    rot = DEG_TO_RAD(rot);
    shiptype = StrToShipType(shiptypestr);
    if ((udword)shiptype > 0x90)
    {
        dbgFatalf(DBG_Loc, "Bad shiptype in %s", field);    // the exe's text (0x00884204)
    }
    if (!missionman)
    {
        matMakeRotAboutZ(&rotaboutz,(real32)cos(missionSphereInfo->sphererotz),(real32)sin(missionSphereInfo->sphererotz));
        matMultiplyMatByVec(&rotatedposition,&rotaboutz,&position);
        vecAddTo(rotatedposition,missionSphereInfo->position);
        rot += missionSphereInfo->sphererotz;
    }
    else
    {
        rotatedposition = position;
    }

    gate = univAddShip(shiptype, RACE_Derelict, &rotatedposition, player, 0);
    gameStatsAddShip(gate, player->playerIndex);
    univRotateObjYaw((SpaceObjRot *)gate, rot);
    if (catSlipgatePending != NULL)
    {
        cat_slipgateSetPartner(gate, catSlipgatePending);
        cat_slipgateSetPartner(catSlipgatePending, gate);
        catSlipgatePending = NULL;
    }
    else
    {
        catSlipgatePending = gate;
    }
    gate->header.flags = (gate->header.flags & ~SOF_Targetable) | SOF_NotBumpable;
    cat_shipSetDisableReason(gate, 1, 6);
}

static void scriptSetDerelictCB(char *directory,char *field,void *dataToFillIn)
{
    vector position;
    MissionSphereInfo *missionSphereInfo = (MissionSphereInfo *)dataToFillIn;
    real32 roty,rotz;
    DerelictType derelicttype;
    Derelict *derelict;
    char derelicttypestr[50];
    udword attributes = 0;
    sdword attributesParam = 0;
    sdword colorScheme = -1;
    char miscStr[50];

    matrix rotaboutz;
    vector rotatedposition;

    RemoveCommasFromString(field);

    miscStr[0] = 0;
    sscanf(field,"%s %f %f %f %f %f %d %d %s",derelicttypestr,&position.x,&position.y,&position.z,&roty,&rotz,&attributes,&attributesParam,miscStr);
    dbgAssertOrIgnore(attributesParam <= SWORD_Max && attributesParam >= SWORD_Min);
    roty = -DEG_TO_RAD(roty);
    rotz = DEG_TO_RAD(rotz);

    derelicttype = StrToDerelictType(derelicttypestr);

    if (!((derelicttype >= 0) && (derelicttype <= NUM_DERELICTTYPES)))
    {
        dbgFatalf(DBG_Loc,"Bad derelict type in %s",field);
    }

    if (!missionman)
    {
        matMakeRotAboutZ(&rotaboutz,(real32)cos(missionSphereInfo->sphererotz),(real32)sin(missionSphereInfo->sphererotz));

        matMultiplyMatByVec(&rotatedposition,&rotaboutz,&position);
        vecAddTo(rotatedposition,missionSphereInfo->position);

        rotz += missionSphereInfo->sphererotz;
    }
    else
    {
        rotatedposition = position;
    }

    derelict = univAddDerelict(derelicttype,&rotatedposition);
    derelict->obj.attributes = attributes;
    derelict->obj.attributesParam = (sword)attributesParam;
    if (attributes & ATTRIBUTES_Anomaly)
    {
        pingAnomalyObjectPingAdd("Anomaly",(SpaceObj *)derelict);
    }
    //hard coded scaffold spinning
//removed and replaced by the kas function RotateDerelictType
/*    if (singlePlayerGame)
    {
        switch (derelict->derelicttype)
        {
            case Scaffold_scarred:
                derelict->rotatable.rotinfo.rotspeed.x = SCAFFOLD_SCARRED_ROTX;
                derelict->rotatable.rotinfo.rotspeed.y = SCAFFOLD_SCARRED_ROTY;
                derelict->rotatable.rotinfo.rotspeed.z = SCAFFOLD_SCARRED_ROTZ;
                break;
            case ScaffoldFingerA_scarred:
                derelict->rotatable.rotinfo.rotspeed.x = SCAFFOLDFINGERA_SCARRED_ROTX;
                derelict->rotatable.rotinfo.rotspeed.y = SCAFFOLDFINGERA_SCARRED_ROTY;
                derelict->rotatable.rotinfo.rotspeed.z = SCAFFOLDFINGERA_SCARRED_ROTZ;
                break;
            case ScaffoldFingerB_scarred:
                derelict->rotatable.rotinfo.rotspeed.x = SCAFFOLDFINGERB_SCARRED_ROTX;
                derelict->rotatable.rotinfo.rotspeed.y = SCAFFOLDFINGERB_SCARRED_ROTY;
                derelict->rotatable.rotinfo.rotspeed.z = SCAFFOLDFINGERB_SCARRED_ROTZ;
                break;
        }

    }*/
    univRotateObjPitch((SpaceObjRot *)derelict,roty);
    univRotateObjYaw((SpaceObjRot *)derelict,rotz);

    //parse the color scheme, if there was one
    if (strstr(miscStr, "colorScheme") == miscStr)
    {
        sscanf(miscStr, "colorScheme(%d)", &colorScheme);
        dbgAssertOrIgnore(colorScheme >= 0 && colorScheme < MAX_MULTIPLAYER_PLAYERS);
        derelict->colorScheme = colorScheme;
    }
}

static char llMiscDelimiters[] = "|?,";

// CAT: the Sect <-> Beast substitute of each standard ship type (0x00883cf8: 60 ShipType entries, one per type
//      sRecon..bMissileCorvette; HW1 swapped only CloakedFighter/DefenseFighter and DDDFrigate/DFGFrigate)
static const ShipType catAppropriateShipTypeSwap[STD_LAST_SHIP + 1] =
{
    bRecon,                     // sRecon
    bAcolyte,                   // sAcolyte
    bCloakedFighter,            // sMimic
    bAttackBomber,              // sLeech
    bACV,                       // sACV
    bMultiGunCorvette,          // sMCV
    bRammingFrigate,            // sRammingFrigate
    bMultiBeamFrigate,          // sMultiBeamFrigate
    bHiveFrigate,               // sHiveFrigate
    bSwarmer,                   // sSwarmer
    bHeavyCruiser,              // sDreadnought
    bRecon,                     // sDreadnoughtCannon
    bCarrier,                   // sCarrier
    bRecon,                     // sCarrierSupport
    bWorker,                    // sWorker
    bRepairDroid,               // sRepairDroid
    bProcessor,                 // sProcessor
    bSentinel,                  // sSentinel
    bMothership,                // sMothership
    bRecon,                     // sMothershipSpecial
    bRecon,                     // sMothershipEngineering
    bRecon,                     // sMothershipArmour
    bRecon,                     // sMothershipMicro
    bRecon,                     // sMothershipWeapons
    bMothershipDockingBay,      // sMothershipDockingBay
    bIonArrayFrigate,           // sDestroyer
    bMothershipSupport,         // sMothershipSupport
    bRecon,                     // sMothershipCommand
    bMothershipEnginesLower,    // sMothershipEnginesLower
    bRecon,                     // sMothershipEnginesUpper
    bRecon,                     // sMothershipLowerSpine
    bRecon,                     // sMothershipBigGun
    bRecon,                     // sMothershipOreCanArmour
    sRecon,                     // bRecon
    sAcolyte,                   // bAcolyte
    sAcolyte,                   // bInterceptor
    sMimic,                     // bCloakedFighter
    sAcolyte,                   // bAttackBomber
    sACV,                       // bACV
    sMCV,                       // bMultiGunCorvette
    sACV,                       // bHeavyCorvette
    sMultiBeamFrigate,          // bMultiBeamFrigate
    sHiveFrigate,               // bHiveFrigate
    sSwarmer,                   // bSwarmer
    sDestroyer,                 // bIonArrayFrigate
    sMultiBeamFrigate,          // bDFGFrigate
    sDreadnought,               // bHeavyCruiser
    sCarrier,                   // bCarrier
    sRammingFrigate,            // bRammingFrigate
    sWorker,                    // bWorker
    sRepairDroid,               // bRepairDroid
    sProcessor,                 // bProcessor
    sSentinel,                  // bSentinel
    sMothership,                // bMothership
    sMothershipDockingBay,      // bMothershipDockingBay
    sMothershipEnginesLower,    // bMothershipEnginesLower
    sRecon,                     // bSectMoship
    sMothershipSupport,         // bMothershipSupport
    sRecon,                     // bCruiseMissile
    sRecon,                     // bMissileCorvette
};

// CAT: a type the race may not own is swapped through catAppropriateShipTypeSwap, with no race assertion and no
//      check that the substitute is allowed (GetAppropriateShipTypeForRace 0x004a13c0). The exe indexes its 60-entry
//      table with any type and reads past it (string data) for a type >= 60; such a type is returned unchanged here.
ShipType GetAppropriateShipTypeForRace(ShipType request,ShipRace shiprace)
{
    ShipType shiptype = request;

    if ((RacesAllowedForGivenShip[shiptype] & RaceToRaceBits(shiprace)) == 0)
    {
        if (shiptype <= STD_LAST_SHIP)
        {
            shiptype = catAppropriateShipTypeSwap[shiptype];
        }
    }

    return shiptype;
}

#define SetShipColorScheme(cs)                      \
    if (cs != -1)                                   \
    {                                               \
        ship->colorScheme = cs;                     \
    }                                               \
    if ((ship->colorScheme < 0) || (ship->colorScheme >= MAX_MULTIPLAYER_PLAYERS))      \
    {                                                                                   \
        ship->colorScheme = (MAX_MULTIPLAYER_PLAYERS-1);                                \
    }

// CAT: the 16 types of the switch the exe inlines in scriptSetMissionSphereCB 0x004a2420 and scriptSetShipsToBeNeeded
//      0x004a2f30 (0x2d, 0x3c, 0x3f, 0x40, 0x42, 0x45, 0x49, 0x4a, 0x4d .. 0x50, 0x53, 0x55, 0x5a, 0x5b): HW1's nine
//      capital ships in their Kushan and Taiidan versions where the race has one, and the Beast DFG frigate
bool32 isCapturableCapitalShip(ShipType shiptype)
{
    switch (shiptype)
    {
        case bDFGFrigate           :
        case kAdvanceSupportFrigate:
        case kCloakGenerator       :
        case kGravWellGenerator    :
        case kHeavyCruiser         :
        case kIonCannonFrigate     :
        case kStandardDestroyer    :
        case tAdvanceSupportFrigate:
        case tCloakGenerator       :
        case tDFGFrigate           :
        case tGravWellGenerator    :
        case tHeavyCruiser         :
        case tIonCannonFrigate     :
        case tMissileDestroyer     :
        case tStandardDestroyer    :
        case tStandardFrigate      :
            return TRUE;
        default:
            break;
    }
    return FALSE;
}

bool32 PlayerDoesntHaveCapturableCapitalShip(Player *player)
{
    Node *node;
    Ship *ship;

    for (node=universe.ShipList.head;node != NULL;node=node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        if (ship->playerowner == player)
        {
            if (isCapturableCapitalShip(ship->shiptype))
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

// CAT: the gift is the race's Hive Frigate (scriptSetMissionSphereCB 0x004a2420, inlined; HW1 gave
//      CapturableCapitalShipToGive, a Standard Frigate)
void GivePlayerCapitalShip(Player *player)
{
    ShipType shiptype;
    ShipRace shiprace = player->race;
    Ship *ship;

#ifdef HW_BUILD_FOR_DEBUGGING
    dbgMessage("@@@@@@@@@@ Giving Hive Frigate as default");      // CAT: the exe's call of it is compiled out
#endif
    if (shiprace == RACE_Sect)
    {
        shiptype = sHiveFrigate;
    }
    else if (shiprace == RACE_Beast)
    {
        shiptype = bHiveFrigate;
    }
    else
    {                                                       // CAT-FIX: the exe takes an unset local as the type here (a skirmish player is always Sect or Beast)
        return;
    }

    if (player->PlayerMothership)
    {
        ShipStaticInfo *testStatic = GetShipStaticInfo(shiptype,shiprace);
        dbgAssertOrIgnore(bitTest(testStatic->common.infoFlags, IF_InfoLoaded));  // we want it to crash if ship not loaded
        {
            vector zerovector = { 0.0f, 0.0f, 0.0f };
            ship = univAddShip(shiptype,shiprace,&zerovector,player,0);
            gameStatsAddShip(ship,player->playerIndex);

            // Update ship totals for player
            unitCapCreateShip(ship,player);

            // CAT: no HW1 ResearchShip docking (M2-R4 ships-b)
            GroupShipIntoMilitaryParade(&universe.mainCommandLayer,ship,player->PlayerMothership);
        }
    }
}

static void scriptSetShipsCB(char *directory,char *field,void *dataToFillIn)
{
    vector position;
    real32 rot;
    char racestr[50];
    char shiptypestr[50];
    char miscstr[256];
    char *miscPointer;
    char label[KAS_TEAM_NAME_MAX_LENGTH+1];
    udword numShips;
    udword numShipsUpper = 0;
    udword i;
    MissionSphereInfo *missionSphereInfo = (MissionSphereInfo *)dataToFillIn;
    Ship *ship;
    Player *player = &universe.players[missionSphereInfo->playerNumber];
    bool32 paradeFormation = FALSE;
    bool32 useAsMothership = FALSE;
    AITeam *teamp = NULL;
    udword fieldStart;
    sdword colorScheme = -1;
    udword attributes = 0;
    sdword attributesParam = 0;

    ShipRace shiprace;
    ShipType shiptype;

    udword sizeofselect;
    SelectCommand *selectcom;
    TypeOfFormation formation = NO_FORMATION;

    matrix rotaboutz;
    vector rotatedposition;

    if (field[0] == '?') field++;

    RemoveCommasFromString(field);

    // grab optional label
    label[0] = 0;
    fieldStart = 0;
    while (field[fieldStart] && isspace(field[fieldStart]) && field[fieldStart] == '?')
        ++fieldStart;
    if (toupper(field[fieldStart]) >= 'A' && toupper(field[fieldStart]) <= 'Z')
    {
        if (sscanf(field, "%s", label))
            fieldStart += strlen(label);
        // assign ships to computer player (#1 in single player game)
        player = &universe.players[1];

        // if labelled team doesn't currently exist, create it
        // otherwise, add ships to existing labelled team
        // CAT: the team is matched by cat_kasHashString(label) (AITeam +0x58) and the label text is not kept
        //      (scriptSetShipsCB 0x004a13f0; M2-R8 kasf library)
        for (i = 0; i < aiCurrentAIPlayer->teamsUsed; i++)
        {
            teamp = aiCurrentAIPlayer->teams[i];
            if (teamp->teamType == ScriptTeam && teamp->kasLabel == cat_kasHashString(label))
                break;
        }
        if (i >= aiCurrentAIPlayer->teamsUsed)
        {
            teamp = aitCreate(ScriptTeam);
            teamp->kasLabel = cat_kasHashString(label);
            // initialize original team size & type (to be updated later)
            teamp->kasOrigShipsCount = 0;
            // CAT: the default is the race's Recon (scriptSetShipsCB 0x004a13f0: sRecon for Sect, bRecon for Beast);
            //      HW1's LightInterceptor is gone (M2-R4 ships-b)
            if (aiCurrentAIPlayer->player->race == RACE_Sect)
            {
                teamp->kasOrigShipsType = sRecon;
            }
            else if (aiCurrentAIPlayer->player->race == RACE_Beast)
            {
                teamp->kasOrigShipsType = bRecon;
            }
        }
    }

    miscstr[0] = 0;
    sscanf(field + fieldStart,"%f %f %f %f %s %s %d %s %d %d %d",&position.x,&position.y,&position.z,&rot,racestr,shiptypestr,&numShips,miscstr,&attributes,&attributesParam,&numShipsUpper);
    dbgAssertOrIgnore(attributesParam <= SWORD_Max && attributesParam >= SWORD_Min);
    rot = DEG_TO_RAD(rot);

    if (!singlePlayerGame)
    {
        numShipsUpper = 0;
    }
    else
    {
        if ((numShipsUpper) && (spFleetModifier))
        {
            // set numShips to be between numShips and numShipsUpper, based on spFleetModifier (0 to 1)
            real32 diff = (real32) (numShipsUpper - numShips);

            if (numShipsUpper < numShips)
                dbgFatalf(DBG_Loc,"Error numShipsUpper %d too low on line %s",numShipsUpper,field);
            else if (numShipsUpper > numShips)
            {
                diff *= spFleetModifier;

                numShips += (udword)(diff + 0.5f);
            }
        }
    }

    shiprace = StrToShipRace(racestr);
    if (!((shiprace >= 0) && (shiprace < NUM_RACES)))
    {
        dbgFatalf(DBG_Loc,"Bad shiprace in %s",field);
    }
    dbgAssertOrIgnore(shiprace != -1);

    shiptype = StrToShipType(shiptypestr);
    if (!((shiptype >= 0) && (shiptype < TOTAL_NUM_SHIPS)))
    {
        dbgFatalf(DBG_Loc,"Bad shiptype in %s",field);
    }
    dbgAssertOrIgnore(shiptype != -1);

    // CAT: no HW1 CryoTray (M2-R4 ships-b); no MG_HarvestinEnabled ResourceCollector gate and no MG_CarrierIsMothership
    //      swap (scriptSetShipsCB 0x004a13f0 has neither) [M2-R4 rest-oracle]

    // CAT: the race override is multiplayer-only (scriptSetShipsCB 0x004a13f0 also tests !singlePlayerGame): single
    //      player missions place each ship with the race the mission names. The type is swapped only when the player's
    //      race may not own it (RacesAllowedForGivenShip) [M2-R4 rest-oracle]
    if ((!singlePlayerGame) && (missionman) && ((shiprace == RACE_Sect) || (shiprace == RACE_Beast)))
    {
        if ((player->race != shiprace) && ((player->race == RACE_Sect) || (player->race == RACE_Beast)))
        {
            // override shiprace and shiptype based on player's race:
            shiprace = player->race;
            if (!(RacesAllowedForGivenShip[shiptype] & (1 << player->race)))
            {
                shiptype = GetAppropriateShipTypeForRace(shiptype,shiprace);
            }
        }
    }

    //parse everything else to the end of the line including formations
    miscPointer = strtok(miscstr, llMiscDelimiters);
    if (miscPointer != NULL)
    {
        do
        {
            if (strcmp(miscPointer,"PARADE_FORMATION") == 0)
            {
                paradeFormation = TRUE;
            }
            else if (strcasecmp(miscPointer,"UseAsMothership") == 0)
            {
                useAsMothership = TRUE;
            }
            else if (strstr(miscPointer, "colorScheme") == miscPointer)
            {
                sscanf(miscPointer, "colorScheme(%d)", &colorScheme);
                dbgAssertOrIgnore(colorScheme >= 0 && colorScheme < MAX_MULTIPLAYER_PLAYERS);
            }
            else if ((miscPointer[0] != '0') && (miscPointer[0] != '?'))
            {
                formation = StrToTypeOfFormation(miscPointer);
            }
        }
        while ((miscPointer = strtok(NULL, llMiscDelimiters)) != NULL);
    }

    if (!missionman)
    {
        matMakeRotAboutZ(&rotaboutz,(real32)cos(missionSphereInfo->sphererotz),(real32)sin(missionSphereInfo->sphererotz));
    }

    if (teamp)
    {
        // if we have a team of mixed ship types, this will obviously be a bit screwed up, but it's not critical
        teamp->kasOrigShipsCount += numShips;
        teamp->kasOrigShipsType = shiptype;
    }

    if (numShips == 1)
    {
        if (!missionman)
        {
            matMultiplyMatByVec(&rotatedposition,&rotaboutz,&position);
            vecAddTo(rotatedposition,missionSphereInfo->position);

            rot += missionSphereInfo->sphererotz;
        }
        else
        {
            rotatedposition = position;
        }

        if (shiptype == MiningBase)
        {
            sdword slab = 0;
            real32 roll = 0.0f;
            ShipStaticInfo *testStatic = GetShipStaticInfo(shiptype,shiprace);
            dbgAssertOrIgnore(bitTest(testStatic->common.infoFlags, IF_InfoLoaded)); // we want it to crash if ship not loaded
            {
                //if info for this ship isn't loaded, we'll skip trying to add it
                for (slab=0;slab<8;slab++,roll+=(TWOPI/8))
                {
                    ship = univAddShip(shiptype,shiprace,&rotatedposition,player,0);
                    gameStatsAddShip(ship,player->playerIndex);
                    ship->obj.attributes = attributes;
                    ship->obj.attributesParam = (sword)attributesParam;
                    ship->rotatable.rotinfo.rotspeed.z = MINING_BASE_ROTATION_SPEED;
                    SetShipColorScheme(colorScheme);
                    univRotateObjYaw((SpaceObjRot *)ship,rot);
                    univRotateObjRoll((SpaceObjRot *)ship,roll);

                    // Update ship totals for player
                    unitCapCreateShip(ship,player);

                    // add ship to team, if applicable
                    if (teamp)
                    {
                        aitAddShip(teamp, ship);
                        teamp->newships--;
                    }
                }
            }
        }
        else
        {
            {
                ShipStaticInfo *testStatic = GetShipStaticInfo(shiptype,shiprace);
                dbgAssertOrIgnore(bitTest(testStatic->common.infoFlags, IF_InfoLoaded)); // we want it to crash if ship not loaded
                {
                    ship = univAddShip(shiptype,shiprace,&rotatedposition,player,0);
                    cat_shipInitResearchUpgrades(ship);     // CAT: the type's static upgrade bits (0x00514af0)
                    gameStatsAddShip(ship,player->playerIndex);
                    ship->obj.attributes = attributes;
                    ship->obj.attributesParam = (sword)attributesParam;
                    SetShipColorScheme(colorScheme);
                    univRotateObjYaw((SpaceObjRot *)ship,rot);

                    // Update ship totals for player
                    unitCapCreateShip(ship,player);
                                        // add ship to team, if applicable
                    if (teamp)
                    {
                        aitAddShip(teamp, ship);
                        teamp->newships--;
                    }

                    // CAT: no addMonkeyResearchShip, Cataclysm has no research ship type (scriptSetShipsCB 0x004a13f0)
                }
            }
        }


        // CAT: no ResearchShip lab activation, Cataclysm has no research ship type: a Sect player's placed module opens
        //      its lab quietly (cat_rmResearchModuleActivated 0x004f4530; scriptSetShipsCB 0x004a13f0)
        if (player->race == RACE_Sect)
        {
            cat_rmResearchModuleActivated(player, shiptype, FALSE);
        }

        // CAT: the player's mothership is any of sMothership, bMothership, kCarrier, tCarrier, trFloatingCity
        //      (scriptSetShipsCB 0x004a13f0: types 0x12, 0x35, 0x3E, 0x4C, 0x62)
        if ((shiptype == sMothership) || (shiptype == bMothership) || (shiptype == kCarrier) ||
            (shiptype == tCarrier) || (shiptype == trFloatingCity) || (useAsMothership))
        {
            player->PlayerMothership = ship;
        }
        else if (paradeFormation)
        {
            if (player->PlayerMothership == NULL)
            {
                dbgFatal(DBG_Loc,"Mothership must be defined for parade formation");
            }

            GroupShipIntoMilitaryParade(&universe.mainCommandLayer,ship,player->PlayerMothership);
        }
    }
    else if (numShips > 1)
    {
        if (paradeFormation)
        {
            if (player->PlayerMothership == NULL)
            {
                dbgFatal(DBG_Loc,"Mothership must be defined for parade formation");
            }

            if (!missionman)
            {
                matMultiplyMatByVec(&rotatedposition,&rotaboutz,&position);
                vecAddTo(rotatedposition,missionSphereInfo->position);
            }
            else
            {
                rotatedposition = position;
            }

            for (i=0;i<numShips;i++)
            {
                ShipStaticInfo *testStatic = GetShipStaticInfo(shiptype,shiprace);
                dbgAssertOrIgnore(bitTest(testStatic->common.infoFlags, IF_InfoLoaded)); // we want it to crash if ship not loaded
                {
                    ship = univAddShip(shiptype,shiprace,&rotatedposition,player,0);
                    cat_shipInitResearchUpgrades(ship);     // CAT: 0x00514af0
                    gameStatsAddShip(ship,player->playerIndex);
                    ship->obj.attributes = attributes;
                    ship->obj.attributesParam = (sword)attributesParam;
                    SetShipColorScheme(colorScheme);

                    // Update ship totals for player
                    unitCapCreateShip(ship,player);

                    // add ship to team, if applicable
                    if (teamp)
                    {
                        aitAddShip(teamp, ship);
                        teamp->newships--;
                    }

                    // CAT: no ResearchShip lab activation (the type is gone): a Sect module opens its lab
                    //      (cat_rmResearchModuleActivated, scriptSetShipsCB 0x004a13f0)
                    if (player->race == RACE_Sect)
                    {
                        cat_rmResearchModuleActivated(player, shiptype, FALSE);
                    }

                    // CAT: no addMonkeyResearchShip (the type is gone): every ship joins the parade
                    GroupShipIntoMilitaryParade(&universe.mainCommandLayer,ship,player->PlayerMothership);
                }
            }
        }
        else
        {
            sizeofselect = sizeofSelectCommand(numShips);
            selectcom = memAlloc(sizeofselect,"selectform",0);

            selectcom->numShips = numShips;

            if (!missionman)
            {
                matMultiplyMatByVec(&rotatedposition,&rotaboutz,&position);
                vecAddTo(rotatedposition,missionSphereInfo->position);

                rot += missionSphereInfo->sphererotz;
            }
            else
            {
                rotatedposition = position;
            }

            for (i=0;i<numShips;i++)
            {
                ShipStaticInfo *testStatic = GetShipStaticInfo(shiptype,shiprace);
                dbgAssertOrIgnore(bitTest(testStatic->common.infoFlags, IF_InfoLoaded));  // we want it to crash if ship not loaded
                {
                    ship = univAddShip(shiptype,shiprace,&rotatedposition,player,0);
                    cat_shipInitResearchUpgrades(ship);     // CAT: 0x00514af0
                    gameStatsAddShip(ship,player->playerIndex);
                    ship->obj.attributes = attributes;
                    ship->obj.attributesParam = (sword)attributesParam;
                    SetShipColorScheme(colorScheme);
                    univRotateObjYaw((SpaceObjRot *)ship,rot);

                    // Update ship totals for player
                    unitCapCreateShip(ship,player);

                    // add ship to team, if applicable
                    if (teamp)
                    {
                        aitAddShip(teamp, ship);
                        teamp->newships--;
                    }

                    // CAT: no ResearchShip lab activation (the type is gone): a Sect module opens its lab
                    //      (cat_rmResearchModuleActivated, scriptSetShipsCB 0x004a13f0)
                    if (player->race == RACE_Sect)
                    {
                        cat_rmResearchModuleActivated(player, shiptype, FALSE);
                    }


                    selectcom->ShipPtr[i] = ship;

                    // CAT: no addMonkeyResearchShip (the type is gone): every ship takes a column slot
                    rotatedposition.z += (ship->staticinfo->common.staticCollInfo.collspheresize * 2.0f);
                }
            }

            if (formation != -1)
            {
                // CAT: the exe's clFormation takes a 4th argument, 0 here (scriptSetShipsCB 0x004a1d66)
                clFormation(&universe.mainCommandLayer,selectcom,formation,0);
            }

            memFree(selectcom);
        }
    }
}

static void scriptSetAIPointCB(char *directory,char *field,void *dataToFillIn)
{
    char label[KAS_MAX_LABEL_LENGTH+1];
    real32 x, y, z, w;

    w = 0.0f;
    RemoveCommasFromString(field);

    if (sscanf(field, "%s %f %f %f %f", label, &x, &y, &z, &w) < 4)
        dbgFatalf(DBG_Loc,"Bad AIPoint definition: %s",field);

    kasLabelledVectorAdd(cat_kasHashString(label), x, y, z, w);    // CAT: by hash (scriptSetAIPointCB)
    aiplayerLog((0,"AIPoint %s added: %f,%f,%f,%f", label, x, y, z, w));
}

static void scriptSetAIPathCB(char *directory,char *field,void *dataToFillIn)
{
    char label[KAS_MAX_LABEL_LENGTH+1];
    char openClosed[KAS_MAX_LABEL_LENGTH+1];
    sdword closed;
    sdword pointNum, numPoints;
    Path *path;
    vector point;

    RemoveCommasFromString(field);

    if (sscanf(field, "%s %f %f %f %s %d/%d",
            label, &(point.x), &(point.y), &(point.z), openClosed, &pointNum, &numPoints) != 7)
        dbgFatalf(DBG_Loc,"Bad AIPath point definition: %s",field);

    closed = !strcasecmp(openClosed, "closed");

    path = kasPathPtrNoErrorChecking(cat_kasHashString(label));     // CAT: by hash (scriptSetAIPathCB)
    if (path == NULL)
    {
        path = kasLabelledPathAdd(cat_kasHashString(label), numPoints, closed);
        aiplayerLog((0,"AIPath %s added", label));
    }

    // add point
    aiuAddPointToPath(point, pointNum-1, path);
    aiplayerLog((0,"AIPath %s point %d/%d added: %f,%f,%f", label, pointNum, numPoints, point.x, point.y, point.z));
}

static void scriptSetAIBoxCB(char *directory,char *field,void *dataToFillIn)
{
    char label[KAS_MAX_LABEL_LENGTH+1];
    real32 x, y, z, width, height, depth;
    Volume *volume;

    RemoveCommasFromString(field);

    if (sscanf(field, "%s %f %f %f %f %f %f", label, &x, &y, &z, &width, &height, &depth) != 7)
        dbgFatalf(DBG_Loc,"Bad AIBox definition: %s",field);

    volume = kasLabelledVolumeAdd(cat_kasHashString(label));        // CAT: by hash (scriptSetAIBoxCB)
    volume->type = VOLUME_AA_BOX;
    volume->attribs.aaBox.x0 = x - width/2;
    volume->attribs.aaBox.x1 = x + width/2;
    volume->attribs.aaBox.y0 = y - height/2;
    volume->attribs.aaBox.y1 = y + height/2;
    volume->attribs.aaBox.z0 = z - depth/2;
    volume->attribs.aaBox.z1 = z + depth/2;
    aiplayerLog((0,"AIBox %s added", label));
}

static void scriptSetAISphereCB(char *directory,char *field,void *dataToFillIn)
{
    char label[KAS_MAX_LABEL_LENGTH+1];
    real32 x, y, z, diameter;
    Volume *volume;

    RemoveCommasFromString(field);

    if (sscanf(field, "%s %f %f %f %f", label, &x, &y, &z, &diameter) != 5)
        dbgFatalf(DBG_Loc,"Bad AISphere definition: %s",field);

    volume = kasLabelledVolumeAdd(cat_kasHashString(label));        // CAT: by hash (scriptSetAISphereCB)
    volume->type = VOLUME_SPHERE;
    volume->attribs.sphere.center.x = x;
    volume->attribs.sphere.center.y = y;
    volume->attribs.sphere.center.z = z;
    volume->attribs.sphere.radius = diameter/2;
    aiplayerLog((0,"AISphere %s added", label));
}

static void scriptSetLightingCB(char *directory,char *field,void *dataToFillIn)
{
    char lightingFileName[50];

    RemoveCommasFromString(field);
    sscanf(field, "%s", lightingFileName);
    if (strlen(field) > 1)
    {
        char fullFileName[PATH_MAX];

        strcpy(fullFileName, directory);
        strcat(fullFileName, lightingFileName);
        if (strchr(lightingFileName, '.') == NULL)
        {
            strcat(fullFileName, ".hsf");
        }

        if (fileExists(fullFileName,0))
        {
            lightParseHSF(fullFileName);
        }
        else
        {
#ifdef _WIN32
            strcpy(fullFileName, "HSF\\");
#else
            strcpy(fullFileName, "HSF/");
#endif
            strcat(fullFileName, lightingFileName);
            if (strchr(lightingFileName, '.') == NULL)
            {
                strcat(fullFileName, ".hsf");
            }

            if (fileExists(fullFileName,0))
            {
                lightParseHSF(fullFileName);
            }
            else
            {
                lightDefaultLightSet();
            }
        }
    }
    else
    {
        lightDefaultLightSet();
    }
}

static void scriptSetBackgroundCB(char* directory, char* field, void* dataToFillIn)
{
    char btgFileName[50];
    real32 theta = 0.0f,phi = 0.0f;

    RemoveCommasFromString(field);
    sscanf(field, "%s %f %f", btgFileName,&theta,&phi);
    btgSetTheta(theta);
    btgSetPhi(phi);
    if (strlen(field) > 1)
    {
        char fullFileName[PATH_MAX];

        // first try current directory:

        strcpy(fullFileName, directory);
        strcat(fullFileName, btgFileName);
        if (strchr(btgFileName, '.') == NULL)
        {
            strcat(fullFileName, ".btg");
        }

        if (fileExists(fullFileName,0))
        {
            btgLoad(fullFileName);
        }
        else
        {
#ifdef _WIN32
            strcpy(fullFileName, "BTG\\");
#else
            strcpy(fullFileName, "BTG/");
#endif
            strcat(fullFileName, btgFileName);
            if (strchr(btgFileName, '.') == NULL)
            {
                strcat(fullFileName, ".btg");
            }

            if (fileExists(fullFileName,0))
            {
                btgLoad(fullFileName);
            }
            else
            {
#ifdef _WIN32
                btgLoad("BTG\\default.btg");
#else
                btgLoad("BTG/default.btg");
#endif
            }
        }
    }
    else
    {
#ifdef _WIN32
        btgLoad("BTG\\default.btg");
#else
        btgLoad("BTG/default.btg");
#endif
    }
}

/*-----------------------------------------------------------------------------
    Name        : catMissionSphereStartPositions
    Description : CAT: the start-position table both MissionSphere readers map a sphere's player number through: each
                  copies it into a local table of 8 first (scriptSetMissionSphereCB 0x004a2420 /
                  scriptPreMissionSphereCB 0x004a2820). Outside a network game it is gameStart's table 0x00ab77e0
                  (catStartPositions, SDL/utility.c)
    Inputs      : table - the 8 entries to fill
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
static void catMissionSphereStartPositions(sdword table[MAX_MULTIPLAYER_PLAYERS])
{
    sdword index;

    if (!multiPlayerGame)
    {
        memcpy(table, catStartPositions, MAX_MULTIPLAYER_PLAYERS * sizeof(sdword));
    }
    else
    {
        // CAT-TODO(stage 5 LAN): a network game's table is the 8 start-position bytes of Cataclysm's CaptainGameInfo (tpGameCreated +0xC8, 0x008860f0; the tree's struct has no such field), shuffled by mgStartGameCB 0x004bc7a0 / lgStartGame 0x004c5900 with 20 swaps of two ranRandomFn(0) picks and sent with the game info; the identity until then
        for (index = 0; index < MAX_MULTIPLAYER_PLAYERS; index++)
        {
            table[index] = index;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : scriptSetMissionSphereCB
    Description : Level load callback for a MissionSphere line: gives the sphere's player its starting RUs and creates
                    the sphere's contents (ships, resources, ...)
    Inputs      :
    Outputs     :
    Return      :
    Note        : CAT (scriptSetMissionSphereCB 0x004a2420): the sphere's player number is a start position, mapped to
                    a game player through the start-position table (catMissionSphereStartPositions). A map carries a
                    sphere for each position it has room for (armed8: 0..7) whatever the number of players, so a
                    sphere whose position maps to no player of this game is skipped; -1 is the sphere of no owner (a
                    ResourceSphere, race "?"), which loads its contents for the misc player and touches no player. HW1
                    asserted playerNumber < numPlayers and indexed universe.players with the raw number, -1 included.
----------------------------------------------------------------------------*/
static void scriptSetMissionSphereCB(char *directory,char *field,void *dataToFillIn)
{
    sdword playerNumber;
    sdword startPosition[MAX_MULTIPLAYER_PLAYERS];          // CAT: the readers' local copy of the table
    char raceName[50];
    ShipRace shiprace;
    vector position;
    real32 radius;
    char contents[50];
    udword startingResourceUnits;
    Player *player;
    MissionSphereInfo missionSphereInfo;
    real32 sphererotz = 0.0f;

    RemoveCommasFromString(field);

    if (missionman)
    {
        char dummystr[50];
        sscanf(field,"%d %s %s %f %f %f %f %f %s %d",&playerNumber,raceName,dummystr,&position.x,&position.y,&position.z,&radius,
                                                  &sphererotz,contents,&startingResourceUnits);
    }
    else
    {
        sscanf(field,"%d %s %f %f %f %f %f %s %d",&playerNumber,raceName,&position.x,&position.y,&position.z,&radius,
                                                  &sphererotz,contents,&startingResourceUnits);
    }

    catMissionSphereStartPositions(startPosition);
    if (playerNumber >= MAX_MULTIPLAYER_PLAYERS)
    {                                                       // CAT-FIX: the exe reads past its local table of 8; no level has such a sphere
        return;
    }
    if ((playerNumber >= 0) && (startPosition[playerNumber] >= numPlayers))
    {                                                       // CAT: a position no player of this game starts at
        return;
    }

    shiprace = StrToShipRace(raceName);

    if ((playerNumber >= 0) && (!(tutorial==TUTORIAL_ONLY)))
    {
        if (!((shiprace >= 0) && (shiprace < NUM_RACES)))
        {
            dbgFatalf(DBG_Loc,"Bad shiprace in %s",field);
        }
        player = &universe.players[startPosition[playerNumber]];
        if (!missionman)
        {
            if (shiprace != player->race)
            {
                return;     // ignore, until we find correct race for player
            }
        }
        else
        {
            // CAT: a race that is none of RACE_Kushan / RACE_Taiidan (HW1's R1 / R2 ids) / RACE_Sect / RACE_Beast is
            //      given to the player (P1, Traders: "specified pirates, so let's give it to them"); HW1 tested R1 / R2
            if ((shiprace != RACE_Kushan) && (shiprace != RACE_Taiidan) && (shiprace != RACE_Sect) && (shiprace != RACE_Beast))
            {
                player->race = shiprace;
            }
        }
    }

    if ((singlePlayerGame) && (!(tutorial==TUTORIAL_ONLY)))
    {
        missionSphereInfo.playerNumber = 1;
    }
    else
    {                                                       // CAT: the mapped player; no owner: the misc player for autoguns, etc.
        missionSphereInfo.playerNumber = (playerNumber >= 0) ? startPosition[playerNumber] : MAX_MULTIPLAYER_PLAYERS;
    }
    missionSphereInfo.position = position;

    if (sphererotz >= 1000.0)
    {
        missionSphereInfo.sphererotz = (real32)atan2((double)position.y,(double)position.x) - (PI/2.0f);
    }
    else
    {
        missionSphereInfo.sphererotz = DEG_TO_RAD(sphererotz);
    }

    if (playerNumber >= 0)
    {
        // this is a player's mission sphere

        player = &universe.players[startPosition[playerNumber]];

        player->resourceUnits = startingResourceUnits;
        player->playerState = PLAYER_ALIVE;
        universe.gameStats.playerStats[player->playerIndex].totalResourceUnits += startingResourceUnits;

        universeSwitchToPlayer((uword)startPosition[playerNumber]);
    }

    scriptSetStruct(directory,contents,MissionSphereScriptTable,(ubyte *)&missionSphereInfo);

    // CAT: the exe inlines PlayerDoesntHaveCapturableCapitalShip and GivePlayerCapitalShip here, for the mapped player
    if ((playerNumber >= 0) && (startPosition[playerNumber] >= 0) && (startPosition[playerNumber] < (sdword)universe.numPlayers))
    {
        if ((!singlePlayerGame) && (bitTest(tpGameCreated.flag,MG_CaptureCapitalShip)))
        {
            player = &universe.players[startPosition[playerNumber]];
            if (PlayerDoesntHaveCapturableCapitalShip(player)) GivePlayerCapitalShip(player);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : scriptPreMissionSphereCB
    Description : Level preload callback to scan the mission sphere and flag
                    all ships therein for loading.
    Inputs      :
    Outputs     :
    Return      :
    Note        : CAT (scriptPreMissionSphereCB 0x004a2820): the same mapping and skip as scriptSetMissionSphereCB; the
                    sphere of no owner keeps the number -1 here, so scriptSetShipsToBeNeeded touches no player for it.
                    HW1 asserted playerNumber < numPlayers. The race name is read and not used, as in HW1.
----------------------------------------------------------------------------*/
static void scriptPreMissionSphereCB(char *directory,char *field,void *dataToFillIn)
{
    sdword playerNumber;
    sdword startPosition[MAX_MULTIPLAYER_PLAYERS];          // CAT: the readers' local copy of the table
    char raceName[50];
    vector position;
    real32 radius;
    char contents[50];
    udword startingResourceUnits;
    real32 sphererotz = 0.0f;
    MissionSphereInfo missionSphereInfo;

    RemoveCommasFromString(field);

    if (missionman)
    {
        char dummystr[50];
        sscanf(field,"%d %s %s %f %f %f %f %f %s %d",&playerNumber,raceName,dummystr,&position.x,&position.y,&position.z,&radius,
                                                  &sphererotz,contents,&startingResourceUnits);
    }
    else
    {
        sscanf(field,"%d %s %f %f %f %f %f %s %d",&playerNumber,raceName,&position.x,&position.y,&position.z,&radius,
                                                  &sphererotz,contents,&startingResourceUnits);
    }

    catMissionSphereStartPositions(startPosition);
    if (playerNumber >= MAX_MULTIPLAYER_PLAYERS)
    {                                                       // CAT-FIX: the exe reads past its local table of 8; no level has such a sphere
        return;
    }
    if ((playerNumber >= 0) && (startPosition[playerNumber] >= numPlayers))
    {                                                       // CAT: a position no player of this game starts at
        return;
    }

    if ((singlePlayerGame) && (!(tutorial==TUTORIAL_ONLY)))
    {
        missionSphereInfo.playerNumber = 1;
    }
    else
    {                                                       // CAT: the mapped player, or -1 for no owner
        missionSphereInfo.playerNumber = (playerNumber >= 0) ? startPosition[playerNumber] : playerNumber;
    }
    missionSphereInfo.position = position;

    if (sphererotz >= 1000.0)
    {
        missionSphereInfo.sphererotz = (real32)atan2((double)position.y,(double)position.x) - (PI/2.0f);
    }
    else
    {
        missionSphereInfo.sphererotz = DEG_TO_RAD(sphererotz);
    }

    scriptSetStruct(directory,contents,MissionPreloadMissphereTable,(ubyte *)&missionSphereInfo);

    if (singlePlayerGame)
    {
        bitSet(derelictStaticInfos[HyperspaceGate].common.infoFlags, IF_InfoNeeded);
    }
}

#define MAXLINENEED 50
bool32 isLevelMissionManGenerated(char *directory,char *filename)
{
    filehandle fh;
    char line[MAXLINENEED];
    char fullfilename[80];
    sdword status;

    if (directory != NULL)
    {
        strcpy(fullfilename,directory);
        strcat(fullfilename,filename);
    }
    else
    {
        strcpy(fullfilename,filename);
    }

    fh = fileOpen(fullfilename,FF_TextMode|FF_IgnorePrepend);

    status = fileLineRead(fh,line,MAXLINENEED);

    fileClose(fh);

    if (status == FR_EndOfFile)
    {
        return FALSE;
    }

    if (strncmp("; MissionMan",line,12) == 0)
    {
        return TRUE;
    }

    return FALSE;
}

ShipRace GetSinglePlayerRaceEquivalent(ShipRace race)
{
    if (race == R1)
    {
        return universe.players[0].race;
    }
    else if (race == R2)
    {
        return ((universe.players[0].race == R1) ? R2 : R1);
    }
    else
    {
        return race;
    }
}

/*-----------------------------------------------------------------------------
    Name        : SetInfoNeededForShipAndRelatedStaticInfo
    Description : Sets InfoNeeded for a ship and also for its related components (missiles, etc, if it has any)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SetInfoNeededForShipAndRelatedStaticInfo(ShipType type,ShipRace race,bool8 dataToFillIn)
{
    ShipStaticInfo *info = GetShipStaticInfo(type, race);
    bitSetTo(info->common.infoFlags, IF_InfoNeeded, dataToFillIn);

    // CAT: the missile carriers are Cataclysm's six types, the mine layer is tMinelayerCorvette, and HW1's
    //      DDDFrigate/Drone pair is gone (SetInfoNeededForShipAndRelatedStaticInfo 0x004a29f0: types 0x55, 10, 0x5f,
    //      0x19, 1, 0x22 -> missileStaticInfos[race]; 0x54 -> mineStaticInfos[race])
    if (type == tMissileDestroyer || type == sDreadnought || type == p1MissileCorvette ||
        type == sDestroyer || type == sAcolyte || type == bAcolyte)
    {
        bitSetTo(missileStaticInfos[race].common.infoFlags, IF_InfoNeeded, dataToFillIn);
    }
    else if (type == tMinelayerCorvette)
    {
        bitSetTo(mineStaticInfos[race].common.infoFlags, IF_InfoNeeded, dataToFillIn);
    }
}

/*-----------------------------------------------------------------------------
    Name        : SetTeamColorForShipAndRelatedStaticInfo
    Description : Sets teamcolor for ship staticinfo and its related components (missiles, etc)
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SetTeamColorForShipAndRelatedStaticInfo(ShipType type,ShipRace race,bool8 schemes[])
{
    ShipStaticInfo *shipstaticinfo;

    if ((RacesAllowedForGivenShip[type] & RaceToRaceBits(race)) == 0)
    {
        dbgMessagef("Warning: not setting team color for type %d race %d",type,race);
        return;
    }

    shipstaticinfo = GetShipStaticInfo(type,race);
    memcpy(shipstaticinfo->shipOrDerelict.teamColor, schemes, sizeof(shipstaticinfo->shipOrDerelict.teamColor));
    // CAT (M2-R8 kas-runtime-mission01): the missile carriers are Cataclysm's seven types and the mine layer
    //      tMinelayerCorvette (llAvailableColorScheme 0x004a2cf0 inlines this body: types 10, 0x55, 0x3b, 1, 0x22, 0x19,
    //      0x5f -> missileStaticInfos[race], 0x54 -> mineStaticInfos[race]; bMissileCorvette is the one more than
    //      SetInfoNeededForShipAndRelatedStaticInfo's six)
    if (type == sDreadnought || type == tMissileDestroyer || type == bMissileCorvette || type == sAcolyte ||
        type == bAcolyte || type == sDestroyer || type == p1MissileCorvette)
    {
        memcpy(missileStaticInfos[race].teamColor, schemes, sizeof(missileStaticInfos[race].teamColor));
    }
    else if (type == tMinelayerCorvette)
    {
        memcpy(mineStaticInfos[race].teamColor, schemes, sizeof(mineStaticInfos[race].teamColor));
    }
    // CAT: no HW1 DDDFrigate drones (M2-R4 ships-b)
}

/*-----------------------------------------------------------------------------
    Callbacks for parsing the pre-load of the .level file
-----------------------------------------------------------------------------*/
char llDelimiters[] = " ,()\t[]";
/*-----------------------------------------------------------------------------
    Name        : llExcludeShip
    Description : Include or exclude a ship from being loaded next frame.
    Inputs      : field - <race>, <shiptype|"all">
                  dataToFillIn - 0 = exclude, 1 = include
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
static void llExcludeShip(char *directory,char *field,void *dataToFillIn)
{
    char *string;
    ShipRace race;
    ShipType type;

    //parse the race
    string = strtok(field, llDelimiters);
    dbgAssertOrIgnore(string != NULL);
    race = StrToShipRace(string);
    dbgAssertOrIgnore(race != 0xffffffff);
    if (singlePlayerGame || tutorial)
    {
        race = GetSinglePlayerRaceEquivalent(race);
    }

    //parse the ship type
    string = strtok(NULL, llDelimiters);
    dbgAssertOrIgnore(string != NULL);
    if (strcasecmp(string, "ALL") == 0)
    {                                                       //if we should do this to all ships for this race
        universeFlagRaceNeeded(race, (bool8)(size_t)dataToFillIn);
    }
    else
    {                                                       //else just a single ship
        type = StrToShipType(string);
        dbgAssertOrIgnore(type != 0xffffffff);
        SetInfoNeededForShipAndRelatedStaticInfo(type,race,(bool8)(size_t)dataToFillIn);
        rmEnableShip(race, type, (bool32)dataToFillIn);
    }
}

/*-----------------------------------------------------------------------------
    Name        : llExclude<Asteroid|DustCloud|GasCloud|Nebula|Derelict>
    Description : Include or exclude loading of one of the above mentioned objects.
    Inputs      : field - type of object to include/exclude
                  dataToFillIn - 0 = exclude, 1 = include
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
static void llExcludeAsteroid(char *directory,char *field,void *dataToFillIn)
{
    AsteroidType type;

    type = StrToAsteroidType(field);
    if (type == 0xffffffff)
    {
        dbgMessagef("Warning: tried to exclude unknown asteroid");
        return;
    }

    bitSetTo(asteroidStaticInfos[type].common.infoFlags, IF_InfoNeeded, (bool8)(size_t)dataToFillIn);
}
static void llExcludeDustCloud(char *directory,char *field,void *dataToFillIn)
{
    DustCloudType type;

    type = StrToDustCloudType(field);
    dbgAssertOrIgnore(type != 0xffffffff);

    bitSetTo(dustcloudStaticInfos[type].common.infoFlags, IF_InfoNeeded, (bool8)(size_t)dataToFillIn);
}
static void llExcludeGasCloud(char *directory,char *field,void *dataToFillIn)
{
    GasCloudType type;

    type = StrToGasCloudType(field);
    dbgAssertOrIgnore(type != 0xffffffff);

    bitSetTo(gascloudStaticInfos[type].common.infoFlags, IF_InfoNeeded, (bool8)(size_t)dataToFillIn);
}
static void llExcludeNebula(char *directory,char *field,void *dataToFillIn)
{
    NebulaType type;

    type = StrToNebulaType(field);
    dbgAssertOrIgnore(type != 0xffffffff);

    bitSetTo(nebulaStaticInfos[type].common.infoFlags, IF_InfoNeeded, (bool8)(size_t)dataToFillIn);
}
static void llExcludeDerelict(char *directory,char *field,void *dataToFillIn)
{
    DerelictType type;

    if (strcasecmp(field, "ALL") == 0)
    {                                                       //if we should do this to all ships for this race
        for (type = 0; type < NUM_DERELICTTYPES; type++)
        {
            bitSetTo(derelictStaticInfos[type].common.infoFlags, IF_InfoNeeded, (bool8)(size_t)dataToFillIn);
        }
    }
    else
    {
        type = StrToDerelictType(field);
        dbgAssertOrIgnore(type != 0xffffffff);
        bitSetTo(derelictStaticInfos[type].common.infoFlags, IF_InfoNeeded, (bool8)(size_t)dataToFillIn);
    }
}
/*-----------------------------------------------------------------------------
    Name        : llAvailableColorScheme
    Description : Make a certain ship load with one or more specific color schemes.
    Inputs      : field - <race> <shiptype|"all"> <scheme#>,[<scheme#>[,<scheme#>]]
    Outputs     :
    Return      :
    Note        : overrides whatever default color schemes might have been coded
                    for this ship
----------------------------------------------------------------------------*/
static void llAvailableColorScheme(char *directory,char *field,void *dataToFillIn)
{
    char *string;
    ShipRace race;
    ShipType type;
    bool8 schemes[MAX_MULTIPLAYER_PLAYERS];
    ShipType firstshiptype;
    ShipType lastshiptype;
    sdword index;

    memset(schemes, 0, MAX_MULTIPLAYER_PLAYERS);
    //parse the race
    string = strtok(field, llDelimiters);
    dbgAssertOrIgnore(string != NULL);
    if (strcasecmp(string, "Derelicts") == 0)
    {                                                       //if color scheme for a derelict
        string = strtok(NULL, llDelimiters);
        dbgAssertOrIgnore(string != NULL);
        type = StrToDerelictType(string);                   //get derelict type
        dbgAssertOrIgnore(type != 0xffffffff);
        while ((string = strtok(NULL, llDelimiters)) != NULL)
        {                                                   //get list of color schemes
            sscanf(string, "%d", &index);
            dbgAssertOrIgnore(index >= 0 && index < MAX_MULTIPLAYER_PLAYERS);
            schemes[index] = TRUE;
        }                                                   //set the color scheme list
        memcpy(derelictStaticInfos[type].shipOrDerelict.teamColor, schemes, sizeof(derelictStaticInfos[type].shipOrDerelict.teamColor));
        return;                                             //done
    }
    race = StrToShipRace(string);
    dbgAssertOrIgnore(race != 0xffffffff);
    if (singlePlayerGame)
    {
        race = GetSinglePlayerRaceEquivalent(race);
    }

    //parse the ship type
    string = strtok(NULL, llDelimiters);
    dbgAssertOrIgnore(string != NULL);
    if (strcasecmp(string, "ALL") == 0)
    {                                                       //if we should do this to all ships for this race
        type = 0xffffffff;
    }
    else
    {                                                       //else just a single ship
        type = StrToShipType(string);
        dbgAssertOrIgnore(type != 0xffffffff);
    }
    while ((string = strtok(NULL, llDelimiters)) != NULL)
    {
        sscanf(string, "%d", &index);
        dbgAssertOrIgnore(index >= 0 && index < MAX_MULTIPLAYER_PLAYERS);
        schemes[index] = TRUE;
    }

    if (type == 0xffffffff)
    {                                                       //if "all"
        firstshiptype = FirstShipTypeOfRace[race];          //set color scheme for all ships in this race
        lastshiptype = LastShipTypeOfRace[race];
    }
    else
    {                                                       //else just the one ship
        firstshiptype = type;
        lastshiptype = type + 1;
    }
    for (type=firstshiptype;type<lastshiptype;type++)
    {                                                       //set the team colors for whatever ships we are to effect
        SetTeamColorForShipAndRelatedStaticInfo(type,race,schemes);
    }
}

/*-----------------------------------------------------------------------------
    Name        : scriptSetShipsToBeNeeded
    Description : Level preload callback that flags a ship for loading just by
                    being in the level.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT (M2-R8 kas-runtime-mission01): scriptSetShipsToBeNeeded 0x004a2f30: no single-player race swap, the colorScheme
//      word is parsed but no team colour is loaded, a race above 6 or a type above 0x90 is fatal; in a multiplayer
//      game with captures on, a capturable ship of a Kushan (2) / Taiidan (3) player clears 0x00a447ac / 0x00a447b0
//      (needR1CapitalShip / needR2CapitalShip here; the Kushan case returns after flagging the ship)
static void scriptSetShipsToBeNeeded(char *directory,char *field,void *dataToFillIn)
{
    MissionSphereInfo *missionSphereInfo = (MissionSphereInfo *)dataToFillIn;
    Player *player;
    vector position;
    real32 rot;
    char racestr[50];
    char shiptypestr[50];
    ShipRace shiprace;
    ShipType shiptype;
    ShipStaticInfo *shipstaticinfo;
    sdword fieldStart;
    char label[KAS_TEAM_NAME_MAX_LENGTH+1];
    char miscstr[256];
    char *miscPointer;
    udword numShips, nScanned;
    sdword colorScheme = -1;
    bool8 teamCol[MAX_MULTIPLAYER_PLAYERS];

    if (field[0] == '?') field++;

    RemoveCommasFromString(field);

    // skip optional label
    fieldStart = 0;
    while (field[fieldStart] && isspace(field[fieldStart]) && field[fieldStart] == '?')
        ++fieldStart;
    if (toupper(field[fieldStart]) >= 'A' && toupper(field[fieldStart]) <= 'Z')
    {
        if (sscanf(field, "%s", label))
            fieldStart += strlen(label);
    }
    //sscanf(field + fieldStart,"%f %f %f %f %s %s",&position.x,&position.y,&position.z,&rot,racestr,shiptypestr);
    miscstr[0] = 0;
    nScanned = sscanf(field + fieldStart,"%f %f %f %f %s %s %d %s ",&position.x,&position.y,&position.z,&rot,racestr,shiptypestr,&numShips,miscstr);
    dbgAssertOrIgnore(nScanned >= 7);
    rot = DEG_TO_RAD(rot);

    shiprace = StrToShipRace(racestr);
    if ((udword)shiprace > 6)                                   // CAT: unsigned "6 < race" (0x004a3048)
    {
        dbgFatalf(DBG_Loc,"Bad shiprace in %s",field);
    }

    shiptype = StrToShipType(shiptypestr);
    if ((udword)shiptype > 0x90)                                // CAT: unsigned "0x90 < type" (0x004a306c)
    {
        dbgFatalf(DBG_Loc,"Bad shiptype in %s",field);
    }

    // CAT: no single-player race swap (scriptSetShipsToBeNeeded 0x004a2f30)

    //find what color scheme these ships will be
    miscPointer = strtok(miscstr, llMiscDelimiters);
    if (miscPointer != NULL)
    {
        do
        {
            if (strstr(miscPointer, "colorScheme") == miscPointer)
            {
                nScanned = sscanf(miscPointer, "colorScheme(%d)", &colorScheme);
                dbgAssertOrIgnore(colorScheme >= 0 && colorScheme < MAX_MULTIPLAYER_PLAYERS && nScanned == 1);
            }
        }
        while ((miscPointer = strtok(NULL, llMiscDelimiters)) != NULL);
    }

    shipstaticinfo = GetShipStaticInfo(shiptype,shiprace);
    if ((!singlePlayerGame) && (bitTest(tpGameCreated.flag,MG_CaptureCapitalShip)) && (missionSphereInfo->playerNumber >= 0))
    if (isCapturableCapitalShip(shiptype))
    {
        // CAT: the player is looked up only here, after the test of the number (scriptSetShipsToBeNeeded 0x004a2f30):
        //      the sphere of no owner passes -1 (scriptPreMissionSphereCB); HW1 indexed universe.players with it on entry
        player = &universe.players[missionSphereInfo->playerNumber];
        if (player->race == RACE_Kushan)                        // CAT: races 2 / 3 (HW1: R1 / R2)
        {
            needR1CapitalShip = FALSE;
            bitSet(shipstaticinfo->common.infoFlags, IF_InfoNeeded);
            return;
        }
        if (player->race == RACE_Taiidan)
        {
            needR2CapitalShip = FALSE;
        }
    }
    bitSet(shipstaticinfo->common.infoFlags, IF_InfoNeeded);//this ship will have to be loaded
    // CAT: the colour scheme is parsed but loads no team colour (scriptSetShipsToBeNeeded 0x004a2f30)
    (void)colorScheme;
    (void)teamCol;
}

/*-----------------------------------------------------------------------------
    Name        : scriptSetDerelictToBeNeeded
    Description : Level preload callback that sets a derelict to be loaded.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
static void scriptSetDerelictToBeNeeded(char *directory,char *field,void *dataToFillIn)
{
    DerelictType derelicttype;
    char derelicttypestr[50];

    RemoveCommasFromString(field);

    sscanf(field,"%s ",derelicttypestr);

    derelicttype = StrToDerelictType(derelicttypestr);

    if (!((derelicttype >= 0) && (derelicttype <= NUM_DERELICTTYPES)))
    {
        dbgFatalf(DBG_Loc,"Bad derelict type in %s",field);
    }
    bitSet(derelictStaticInfos[derelicttype].common.infoFlags, IF_InfoNeeded);
}

/*-----------------------------------------------------------------------------
    Name        : levelPreInit
    Description : A level init function to be called before any ships are loaded.
    Inputs      : directory, picked mission - path/name of .level file
    Outputs     : Sets the IF_InfoNeeded and teamColor members of ships
    Return      :
----------------------------------------------------------------------------*/
void levelPreInit(char *directory,char *pickedMission)
{
    tecolorscheme playerSchemes[MAX_MULTIPLAYER_PLAYERS];
    sdword index;
    Node *node;
    Ship *ship;
    InsideShip* insideship;
    bool8 teamCol[MAX_MULTIPLAYER_PLAYERS];

    catSlipgatePending = NULL;                              // CAT (M2-R9 missions-02-09): 0x004a3245
    missionman = isLevelMissionManGenerated(directory,pickedMission);

    memcpy(playerSchemes, teColorSchemes, sizeof(playerSchemes));
    //...set everything to be loaded this mission (the level file will manually exclude ships)
    universeFlagEverythingNeeded();
    //set default color schemes
    teReset();
    // CAT (M2-R5 render): the last level's fog layer and lens flare go (levelPreInit 0x004a3230: after teReset)
    cat_rndFogLayerOff();
    cat_rndFogLayerTextureUnload();
    cat_rndLensFlareTextureUnload();
    universeDefaultTeamColors();
    //load the actual mission
    scriptSet(directory, pickedMission, MissionPreloadScriptTable);
    cat_soundLevelReset();      // CAT: the sound's level reset (levelPreInit 0x004a3230 calls 0x0051c980 after this script)

    for (index = 0; index < MAX_MULTIPLAYER_PLAYERS; index++)
    {                                                       //for all players
        if (index < numPlayers && universe.players[index].aiPlayer == NULL)
        {                                                   //if this is a human player
            teColorSchemes[index] = playerSchemes[index];   //restore this player's color scheme
        }
        else if (teColorSchemes[index].textureColor.base != playerSchemes[index].textureColor.base ||
                 teColorSchemes[index].textureColor.detail != playerSchemes[index].textureColor.detail)
        {                                                   //if this color scheme has changed
            teTeamColorsSet(index, teColorSchemes[index].textureColor.base,
                            teColorSchemes[index].textureColor.detail);//recompute all the engine trails etc.
        }
    }
    //go through all the players' ships and make sure they'll be loaded
    for (node = singlePlayerGameInfo.ShipsInHyperspace.head; node != NULL; node = node->next)
    {
        insideship = (InsideShip*)listGetStructOfNode(node);
        ship = insideship->ship;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
        SetInfoNeededForShipAndRelatedStaticInfo(ship->shiptype,ship->shiprace,TRUE);
        memcpy(teamCol,ship->staticinfo->shipOrDerelict.teamColor,sizeof(teamCol));
        teamCol[ship->colorScheme] = TRUE;                          // preserve color schemes
        SetTeamColorForShipAndRelatedStaticInfo(ship->shiptype,ship->shiprace,teamCol);

        // check inside ships too!
        if (ship->shipsInsideMe != NULL)
        {
            // check ships inside too
            Node *insidenode = ship->shipsInsideMe->insideList.head;
            InsideShip *insideship2;
            Ship *tiship;

            while (insidenode != NULL)
            {
                insideship2 = (InsideShip *)listGetStructOfNode(insidenode);
                tiship = insideship2->ship;
                dbgAssertOrIgnore(tiship->header.objtype == OBJ_ShipType);

                SetInfoNeededForShipAndRelatedStaticInfo(tiship->shiptype,tiship->shiprace,TRUE);
                memcpy(teamCol,tiship->staticinfo->shipOrDerelict.teamColor,sizeof(teamCol));
                teamCol[tiship->colorScheme] = TRUE;                          // preserve color schemes
                SetTeamColorForShipAndRelatedStaticInfo(tiship->shiptype,tiship->shiprace,teamCol);

                insidenode = insidenode->next;
            }
        }
    }

    // CAT: both words start at 0 (levelPreInit 0x004a3230 at 0x004a3655; HW1 set them for every player of R1 / R2)
    needR1CapitalShip = FALSE;
    needR2CapitalShip = FALSE;

    scriptSet(directory, pickedMission, MissionPreloadScriptTablePass2);

    // CAT: port diagnostic (M2-R5 command-tactics, not in the exe): /catCommandTest also needs the statics of the
    //      Mimic, the MCV it links into and the Sentinel, which Mission01 does not load
    if (catCommandSelfTestRequested())
    {
        SetInfoNeededForShipAndRelatedStaticInfo(sMimic,RACE_Sect,TRUE);
        SetInfoNeededForShipAndRelatedStaticInfo(sMCV,RACE_Sect,TRUE);
        SetInfoNeededForShipAndRelatedStaticInfo(sSentinel,RACE_Sect,TRUE);
    }

    // CAT (M2-R8 singleplayer-tutorial): no warp preload in a single-player game (levelPreInit 0x004a3230; HW1's
    //      singlePlayerPreLoadCheck read the warp script's fleet)
    if (!singlePlayerGame)
    {
        // CAT: only 0x00a447b0 is tested, for one static (tStandardFrigate's InfoNeeded bit; levelPreInit 0x004a3664)
        if (bitTest(tpGameCreated.flag,MG_CaptureCapitalShip) && needR2CapitalShip)
        {
            bitSet(GetShipStaticInfo(tStandardFrigate, RACE_Taiidan)->common.infoFlags, IF_InfoNeeded);
        }

        if (bitTest(tpGameCreated.flag,MG_CratesEnabled))
        {
            bitSet(derelictStaticInfos[Crate].common.infoFlags, IF_InfoNeeded);
        }
    }
}

void TryToFindMothershipsForPlayers(void)
{
    sdword i;
    Player *player;

    for (i=0;i<universe.numPlayers;i++)
    {
        player = &universe.players[i];

        if (player->PlayerMothership == NULL)
        {
            dbgMessage("Warning: no mothership assigned, trying to find one:");
            if (!univFindBackupMothership(player))
            {
                dbgMessage("Warning: no mothership found for this player");
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_fowCreatePlayerGrids
    Description : CAT (M2-R6 nis-fow-camera): the local player (curPlayer, 0x00a43cae) gets a fog-of-war grid over
                  the universe box (+-smUniverseSize), and the ships it already has light their cubes
                  (cat_fowCreatePlayerGrids 0x004a38c0; levelInit and singlePlayerLoadNewLevel call it). The other
                  players get none: a player without a grid sees every ship that is not fully cloaked and undetected
                  (cat_fowCanPlayerSeeShip). If the grid cannot be made, the grids made so far (the local player's,
                  and in the exe's loop any computer player's) are destroyed again.
    Return      : FALSE if a grid could not be made
----------------------------------------------------------------------------*/
bool32 cat_fowCreatePlayerGrids(void)
{
    void *previous = fowSelect(NULL);
    vector boxMin, boxMax;
    sdword index, back;
    Node *node;
    Ship *ship;

    for (index = 0; index < universe.numPlayers; index++)
    {
        if (index == curPlayer)
        {
            // CAT-FIX: the exe makes a new grid without freeing the one a previous level made (singlePlayerLoadNewLevel
            //      0x0051ac80 calls this again mid-game); the old one is destroyed first here
            if (universe.players[index].catFOWGrid != NULL)
            {
                fowDestroy(universe.players[index].catFOWGrid);
                universe.players[index].catFOWGrid = NULL;
            }
            boxMin.x = -smUniverseSizeX;
            boxMax.x = smUniverseSizeX;
            boxMin.y = -smUniverseSizeY;
            boxMax.y = smUniverseSizeY;
            boxMin.z = -smUniverseSizeZ;
            boxMax.z = smUniverseSizeZ;
            universe.players[index].catFOWGrid = fowCreate(&boxMin, &boxMax);
            if (universe.players[index].catFOWGrid == NULL)
            {
                for (back = index - 1; back >= 0; back--)
                {
                    if ((back == curPlayer) || (universe.players[back].aiPlayer != NULL))
                    {
                        fowDestroy(universe.players[back].catFOWGrid);
                        universe.players[back].catFOWGrid = NULL;
                    }
                }
                fowSelect(previous);
                return FALSE;
            }
        }
    }
    fowSelect(universe.curPlayerPtr->catFOWGrid);
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (ship->playerowner == universe.curPlayerPtr)
        {
            cat_univShipFOWActivate(ship);
        }
    }
    return TRUE;
}

void levelInit(char *directory,char *pickedMission)
{
    udword i;

    dbgAssertOrIgnore(numPlayers <= MAX_MULTIPLAYER_PLAYERS);
    dbgAssertOrIgnore(curPlayer < numPlayers);

    missionman = isLevelMissionManGenerated(directory,pickedMission);

    for (i=0;i<TOTAL_NUM_SHIPS;i++)
    {
        universe.shipMaxUnits[i] = 0;
    }

    if (singlePlayerGame)
    {
        singlePlayerSetMissionAttributes(directory,pickedMission);
    }
    SetSMMissionTweaks(directory,pickedMission);
    cat_fowCreatePlayerGrids();                             // CAT: the local player's fog-of-war grid (levelInit 0x004a3a10)
    scriptSetStruct(directory,pickedMission,MissionScriptTable,NULL);
    TryToFindMothershipsForPlayers();

    mrCamera = &(universe.mainCameraCommand.actualcamera);

    universeSwitchToPlayer(curPlayer);

    if ((tutorial) || (!singlePlayerGame))
    {
        clPresetShipsToPosition(&universe.mainCommandLayer);

        if (universe.curPlayerPtr->PlayerMothership)
        {
            ccFocusOnPlayersMothership(&universe.mainCameraCommand,curPlayer);
        }
        else
        {
            ccFocusOnFleet(&universe.mainCameraCommand);
        }
    }

    if (singlePlayerGame)
    {
        universe.curPlayerPtr->resourceUnits = SINGLEPLAYER_STARTINGRUS;
    }
    else
    {
        soundEventPlayMusic(SongNumber);
    }

    if(tutorial==TUTORIAL_ONLY)
    {
        soundEventPlayMusic(SongNumber);
        universe.curPlayerPtr->resourceUnits = resourceStartTutorial;
    }
}

void levelStartNext(char *directory,char *pickedMission)
{
    dbgAssertOrIgnore(singlePlayerGame);        // this function is only for the single player game

    missionman = isLevelMissionManGenerated(directory,pickedMission);

    singlePlayerSetMissionAttributes(directory,pickedMission);
    SetSMMissionTweaks(directory,pickedMission);
    scriptSetStruct(directory,pickedMission,MissionScriptTable,NULL);

    TryToFindMothershipsForPlayers();
}

bool32 AnyShipOverlapsAsteroid(vector *position,AsteroidType asteroidtype)
{
    Node *node;
    Ship *ship;
    vector checkpos = *position;
    real32 consider,negconsider;
    real32 asteroidr = asteroidStaticInfos[asteroidtype].common.staticCollInfo.collspheresize;
    vector diff;

    node = universe.ShipList.head;
    while (node != NULL)
    {
        ship = (Ship *)listGetStructOfNode(node);

        if ((ship->header.flags & (SOF_Dead|SOF_Hide)) == 0)
        {
            if (ship->shiptype == MiningBase)
            {
                goto next;      // MiningBase doesn't interfere with regrowing...
            }
            consider = asteroidr + ship->staticinfo->common.staticCollInfo.collspheresize;
            negconsider = -consider;
            vecSub(diff,ship->obj.posinfo.position,checkpos);

            if (!isBetweenInclusive(diff.x,negconsider,consider))
            {
                goto next;
            }

            if (!isBetweenInclusive(diff.y,negconsider,consider))
            {
                goto next;
            }

            if (!isBetweenInclusive(diff.z,negconsider,consider))
            {
                goto next;
            }

            // consider it too close...
            return TRUE;
        }

next:
        node = node->next;
    }

    return FALSE;
}

/*=============================================================================
    HWI-TEST (skirmish-start), port diagnostic, not in the exe: /catSkirmishTest <ticks> [/catSkirmishMap <name>]
    (harness mode skirmish; docs/skirmish-start-fix.md). The front end's skirmish start, headless and with no input,
    so that the level loader's skirmish path (the mission-sphere readers) is covered:
      start      once the start-up has put up the main screen (utyGameSystemsInit), the buttons' own callbacks in
                 the order a player presses them: FE_Multiplayer_Game (utyMultiplayerGameStart), MG_Skirmish
                 (mgSkirmish: one human and one computer opponent), the map picked as the scenario picker's OK does
                 (ScenPick.c catSpScenarioPickForTest: the map's description game type, spCurrentSelected; the default
                 LiveFireExercise2 is commented out in multiPlayerMissions.script, so the test adds its line first),
                 Sect as the colour screen's race toggle sets it (whichRaceSelected), then StartNewGame
                 (utyNewGameStart: gameStart, levelPreInit / levelInit with the map's MissionSphere lines). Every other
                 skirmish option keeps the value the options file and the map's description give it
      run        the universe runs on the virtual clock of /oracleMission (one tick per frame, the oracle stream
                 written with /oracleDump, the quit after <ticks>)
      judgement  at tick <ticks>: the game has the two players asked for, player 0 the human and player 1 the
                 computer player; both have a mothership in the universe, player 0's an sMothership and player 0 the
                 Sect; the no-owner resource sphere's asteroids exist (no player sphere of either map has any); no
                 player is in a state other than alive; no ship belongs to a player the map has a sphere for but the
                 game does not have (an 8-player map with 2 players: spheres 2..7 skipped); "skirmishtest: done
                 ticks=N ships=N result=PASS|FAIL", then the mission run's own quit
=============================================================================*/
#define CAT_SKIRMISHTEST_PLAYERS        2
#define CAT_SKIRMISHTEST_DEFAULT_TICKS  600
#define CAT_SKIRMISHTEST_DEFAULT_MAP    "LiveFireExercise2"

static sdword catSkirmishSelfTestTicks = 0;
static char catSkirmishSelfTestMap[64] = CAT_SKIRMISHTEST_DEFAULT_MAP;
static bool32 catSkirmishSelfTestStarted = FALSE;
static bool32 catSkirmishSelfTestDone = FALSE;
static bool32 catSkirmishSelfTestPass = TRUE;
static sdword catSkirmishSelfTestBigotry = -1;              // HWI-TEST (M2-R10 tests-leftovers): -1 = the front end's

void catSkirmishSelfTestSet(char *string)
{
    catSkirmishSelfTestTicks = (sdword)strtol(string, NULL, 10);
    if (catSkirmishSelfTestTicks <= 0)
    {
        catSkirmishSelfTestTicks = CAT_SKIRMISHTEST_DEFAULT_TICKS;
    }
    oracleSetMission(string);                               // the virtual clock and the quit after <ticks>
}

void catSkirmishSelfTestSetMap(char *string)
{
    strncpy(catSkirmishSelfTestMap, string, sizeof(catSkirmishSelfTestMap) - 1);
    catSkirmishSelfTestMap[sizeof(catSkirmishSelfTestMap) - 1] = '\0';
}

// HWI-TEST (M2-R10 tests-leftovers): /catSkirmishBigotry <0..2>: the skirmish screen's CPU-attacks radio button
//      (OP_CPU_Attacks on update.big's Skirmish_Basic: three buttons, pData 0, 1, 2) pressed before StartNewGame
void catSkirmishSelfTestSetBigotry(char *string)
{
    catSkirmishSelfTestBigotry = (sdword)strtol(string, NULL, 10);
    if ((catSkirmishSelfTestBigotry < 0) || (catSkirmishSelfTestBigotry > 2))
    {
        catSkirmishSelfTestBigotry = -1;
    }
}

bool32 catSkirmishSelfTestRequested(void)
{
    return (catSkirmishSelfTestTicks > 0);
}

static void catSkirmishSelfTestExpect(bool32 condition, const char *what, sdword index)
{
    if (!condition)
    {
        catSkirmishSelfTestPass = FALSE;
        dbgMessagef("skirmishtest: FAIL tick=%u %s (player %d)", (unsigned)universe.univUpdateCounter, what, (int)index);
    }
}

void catSkirmishSelfTestStart(void)
{
    sdword index;

    dbgMessagef("skirmishtest: start map=%s ticks=%d", catSkirmishSelfTestMap, (int)catSkirmishSelfTestTicks);
    utyMultiplayerGameStart(NULL, NULL);                    // FE_Multiplayer_Game
    mgSkirmish(NULL, NULL);                                 // MG_Skirmish
    index = catSpScenarioPickForTest(catSkirmishSelfTestMap);   // MG_ChooseScenario, a line, OK
    if (index < 0)
    {
        catSkirmishSelfTestPass = FALSE;
        dbgMessagef("skirmishtest: FAIL no map '%s' (no .level title in MultiPlayer)", catSkirmishSelfTestMap);
        dbgMessagef("skirmishtest: done ticks=0 ships=0 result=FAIL");
        catSkirmishSelfTestDone = TRUE;
        utyGameQuit(NULL, NULL);
        return;
    }
    whichRaceSelected = RACE_Sect;                          // Skirmish_Colour's CP_RaceOne
    if (catSkirmishSelfTestBigotry >= 0)
    {
        // HWI-TEST (M2-R10 tests-leftovers): the radio button's click: opCPUAttacks 0x004db440 with the button's atom
        //      (not its first call) sets tpGameCreated.aiplayerBigotry from the atom's pData; cat_aisSetupAlliances
        //      0x004310a0 then sends the alliances at the skirmish AI's first play
        featom bigotryButton;

        memset(&bigotryButton, 0, sizeof(bigotryButton));
        bigotryButton.pData = (void *)(memsize)catSkirmishSelfTestBigotry;
        opCPUAttacks("OP_CPU_Attacks", &bigotryButton);
        dbgMessagef("skirmishtest: bigotry=%u (OP_CPU_Attacks radio button %d)", (unsigned)tpGameCreated.aiplayerBigotry,
                    (int)catSkirmishSelfTestBigotry);
    }
    dbgMessagef("skirmishtest: picked index=%d file=%s title='%s' maxplayers=%u selected=%d humans=%u computers=%u race=%d flag=0x%x fleet=%u resources=%u",
                (int)index, spScenarios[index].fileSpec, spScenarios[index].title, (unsigned)spScenarios[index].maxplayers,
                (int)spCurrentSelected, (unsigned)tpGameCreated.numPlayers, (unsigned)tpGameCreated.numComputers,
                (int)whichRaceSelected, (unsigned)tpGameCreated.flag, (unsigned)tpGameCreated.startingFleet,
                (unsigned)tpGameCreated.startingResources);
    catSkirmishSelfTestStarted = TRUE;
    utyNewGameStart(NULL, NULL);                            // StartNewGame
    dbgMessagef("skirmishtest: started numPlayers=%u universe.numPlayers=%u curPlayer=%u singlePlayerGame=%d multiPlayerGame=%d missionman=%d ships=%u resources=%u",
                (unsigned)numPlayers, (unsigned)universe.numPlayers, (unsigned)curPlayer, (int)singlePlayerGame,
                (int)multiPlayerGame, (int)missionman, (unsigned)universe.ShipList.num, (unsigned)universe.ResourceList.num);
}

void catSkirmishSelfTestTick(void)
{
    Node *node;
    Ship *ship;
    Resource *resource;
    Player *player;
    sdword i;
    udword owned[MAX_MULTIPLAYER_PLAYERS + 1];
    udword strays = 0, absent = 0, asteroids = 0, dust = 0, gas = 0, nebulae = 0;

    if (!catSkirmishSelfTestStarted || catSkirmishSelfTestDone ||
        ((sdword)universe.univUpdateCounter < catSkirmishSelfTestTicks))
    {
        return;
    }
    catSkirmishSelfTestDone = TRUE;

    for (i = 0; i <= MAX_MULTIPLAYER_PLAYERS; i++)
    {
        owned[i] = 0;
    }
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (bitTest(ship->header.flags, SOF_Dead) || (ship->playerowner == NULL))
        {
            continue;
        }
        i = ship->playerowner->playerIndex;
        if ((i >= 0) && (i <= MAX_MULTIPLAYER_PLAYERS))
        {
            owned[i]++;
        }
        else
        {
            strays++;
        }
    }
    for (node = universe.ResourceList.head; node != NULL; node = node->next)
    {
        resource = (Resource *)listGetStructOfNode(node);
        switch (resource->header.objtype)
        {
            case OBJ_AsteroidType: asteroids++; break;
            case OBJ_DustType:     dust++;      break;
            case OBJ_GasType:      gas++;       break;
            case OBJ_NebulaType:   nebulae++;   break;
            default: break;
        }
    }

    catSkirmishSelfTestExpect(universe.numPlayers == CAT_SKIRMISHTEST_PLAYERS, "the game does not have the players asked for", universe.numPlayers);
    for (i = 0; i < universe.numPlayers; i++)
    {
        player = &universe.players[i];
        ship = player->PlayerMothership;
        dbgMessagef("skirmishtest: player %d race=%d state=%d mothership=%s ships=%u RU=%u computer=%s",
                    (int)i, (int)player->race, (int)player->playerState,
                    (ship != NULL) ? ShipTypeToStr(ship->shiptype) : "none", (unsigned)owned[i],
                    (unsigned)player->resourceUnits, (player->aiPlayer != NULL) ? "yes" : "no");
        catSkirmishSelfTestExpect(ship != NULL, "no mothership", i);
        if (ship != NULL)
        {
            catSkirmishSelfTestExpect(!bitTest(ship->header.flags, SOF_Dead) && (ship->playerowner == player),
                                      "the mothership is dead or not the player's", i);
        }
        catSkirmishSelfTestExpect(player->playerState == PLAYER_ALIVE, "the player is not alive", i);
    }
    catSkirmishSelfTestExpect((universe.players[0].PlayerMothership != NULL) &&
                              (universe.players[0].PlayerMothership->shiptype == sMothership), "player 0's mothership is not an sMothership", 0);
    catSkirmishSelfTestExpect(universe.players[0].race == RACE_Sect, "player 0 is not the Sect", 0);
    catSkirmishSelfTestExpect((universe.players[0].aiPlayer == NULL) && (universe.players[1].aiPlayer != NULL),
                              "player 0 is not the human and player 1 the computer player", 1);
    for (i = universe.numPlayers; i < MAX_MULTIPLAYER_PLAYERS; i++)
    {
        catSkirmishSelfTestExpect(owned[i] == 0, "a ship belongs to a player the game does not have", i);
        absent += owned[i];
    }
    catSkirmishSelfTestExpect(strays == 0, "a ship has an owner outside the player table", -1);
    catSkirmishSelfTestExpect(asteroids > 0, "no asteroid from the resource sphere", -1);
    dbgMessagef("skirmishtest: resources asteroids=%u dust=%u gas=%u nebulae=%u; no-owner ships=%u (player %d); absent players' ships=%u",
                (unsigned)asteroids, (unsigned)dust, (unsigned)gas, (unsigned)nebulae, (unsigned)owned[MAX_MULTIPLAYER_PLAYERS],
                MAX_MULTIPLAYER_PLAYERS, (unsigned)absent);
    dbgMessagef("skirmishtest: done ticks=%u ships=%u result=%s", (unsigned)universe.univUpdateCounter,
                (unsigned)universe.ShipList.num, catSkirmishSelfTestPass ? "PASS" : "FAIL");
}

