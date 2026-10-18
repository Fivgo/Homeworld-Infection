/*=============================================================================
    Name    : univupdate.h
    Purpose : Definitions for univupdate.c

    Created 10/4/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___UNIVUPDATE_H
#define ___UNIVUPDATE_H

#include "LinkedList.h"
#include "ShipSelect.h"
#include "SpaceObj.h"
#include "Types.h"

/*=============================================================================
    Types:
=============================================================================*/

typedef struct DeleteShip
{
    Node objlink;
    Ship *ship;
    sdword deathBy;
} DeleteShip;

// CAT (M2-R3 ships-a): also called by MothershipHouseKeep (exe 0x00545f00)
void MakeExplosionRockBumpables(Ship *ship,bool32 scuttle);

typedef struct DeleteResource
{
    Node objlink;
    Resource *resource;
} DeleteResource;

typedef struct DeleteMissile
{
    Node objlink;
    Missile *missile;
    sdword deathBy;
} DeleteMissile;

typedef struct DeleteDerelict
{
    Node objlink;
    Derelict *derelict;
    sdword deathBy;
} DeleteDerelict;

// For Ship ID Decoding:
typedef struct IDToPtrTable
{
    sdword numEntries;
    SpaceObjPtr *objptrs;
} IDToPtrTable;

/*=============================================================================
    Functions:
=============================================================================*/

void univupdateReset();
void univupdateInit();
void univupdateClose();

void univupdateCloseAllObjectsAndMissionSpheres();

bool32 univUpdate(real32 phystimeelapsed);
real32 cat_univRealTimeDelta(void);              // CAT: 0x0054c360
extern bool32 univRealTimeVirtual;              // HWI-ORACLE: fixed 1/16 s per tick (UnivUpdate.c)

void univRotateObjYaw(SpaceObjRot *robj,real32 rot);
void univRotateObjPitch(SpaceObjRot *robj,real32 rot);
void univRotateObjRoll(SpaceObjRot *robj,real32 rot);

Ship *univCreateShip(ShipType shiptype,ShipRace shiprace,vector *shippos,struct Player *playerowner,sdword built,struct Ship *creator);   // CAT: the exe's 6th argument (M2-R6 managers)
Ship *univAddShip(ShipType shiptype,ShipRace shiprace,vector *shippos,struct Player *playerowner,sdword built);
Asteroid *univAddAsteroid(AsteroidType asteroidtype,vector *astpos);
DustCloud *univAddDustCloud(DustCloudType dustcloudtype,vector *cloudpos);
GasCloud *univAddGasCloud(GasCloudType gascloudtype,vector *cloudpos);
Nebula *univAddNebula(NebulaType nebulatype, vector *nebpos, void *stub);
Effect *univAddEffect(Ship *ship, etgeffectstatic *stat);
//Effect *univAddEffect(Ship *ship, EffectType type);

void univFreeShipContents(Ship *ship);
void univFreeBulletContents(Bullet *bullet);
void univFreeMissileContents(Missile *missile);
void univFreeEffectContents(Effect *effect);

void univFreeResourceContents(Resource *resource);
void univFreeAsteroidContents(Asteroid *asteroid);
void univFreeNebulaContents(Nebula *nebula);
void univFreeGasCloudContents(GasCloud *gascloud);
void univFreeDustCloudContents(DustCloud *dustcloud);

void univFreeDerelictContents(Derelict *derelict);

void MakeNewGasCloudStaticInfo(GasCloud *gascloud);
void MakeNewAsteroidStaticInfo(Asteroid *asteroid);
void MakeNewDustCloudStaticInfo(DustCloud *dustcloud);

void univDeleteEffect(Effect *effect);
void univRemoveShipFromOutside(Ship *ship);
void univDeleteWipeInsideShipOutOfExistence(Ship *ship);
void univDeleteDeadShip(Ship *ship, sdword deathBy);// deletes ship, but lets part of it hang around for explosion
void univWipeShipOutOfExistence(Ship *ship);                // totally deletes ship, frees memory
void univWipeDerelictOutOfExistance(Derelict *derelict);    //totally deletes the derelict, frees memory and spawns no new effects
void univReallyDeleteThisShipRightNow(Ship *ship);      //totally deletes it immediately
void univReallyDeleteThisResourceRightNow(Resource *resource);
void univReallyDeleteThisDerelictRightNow(Derelict *derelict);
bool8 univRemoveShipFromHotkeyGroup(Ship *ship, bool8 PlayDefeat); // removes ship from hotkey group
void univHideShipFromSpheres(Ship *ship);           //hides from all mission spheres
void univUnhideJustAboutEverything(void);
void univHideJustAboutEverything(void);

void univGetResourceStatistics(sdword *resourceValue,sdword *numHarvestableResources,sdword *numAsteroid0s);
Resource *univFindNearestResource(Ship *ship,real32 volumeRadius,vector *volumePosition);
void cat_univClampResourceTimer(Missile *missile);     // CAT: 0x00547530 (M2-R5 dock)

void univRemoveObjFromRenderList(SpaceObj *spaceobj);
void univAddObjToRenderListIf(SpaceObj *newobj,SpaceObj *ifobj);
void univAddObjToRenderList(SpaceObj *newobj);
bool32 univSpaceObjInRenderList(SpaceObj *spaceobj);

#define univRemoveObjFromMinorRenderList univRemoveObjFromRenderList
#define univSpaceObjInMinorRenderList univSpaceObjInRenderList

real32 univGetChecksum(sdword *numShipsInChecksum);
udword univCalcShipChecksum();


void univUpdateRenderList();
void univUpdateMinorRenderList();

Ship *univFindShipIAmInside(Ship *me);
bool32 univAmIInsideThisShip(Ship *me,Ship *ship);

Ship *ShipIDtoShip(ShipID shipID,bool32 considerInsideShips);
ResourcePtr ResourceIDtoResourcePtr(ResourceID resourceID);
MissilePtr MissileIDtoMissilePtr(MissileID missileID);
DerelictPtr DerelictIDToDerelictPtr(DerelictID derelictID);

void univInitSpaceObjPosRot(SpaceObj *obj,vector *position,bool32 randomOrientation);
void univUpdateObjRotInfo(SpaceObjRot *robj);

void AddResourceToDeleteResourceList(Resource *resource);
void AddDerelictToDeleteDerelictList(Derelict *derelict,GunSoundType damageType);
void AddMissileToDeleteMissileList(Missile *missile,GunSoundType damageType);
void AddShipToDeleteShipList(Ship *ship,GunSoundType damageType,sdword unused);    // CAT: 3 arguments (0x00545b80)
void AddTargetToDeleteList(SpaceObjRotImpTarg *target,GunSoundType soundType,sdword unused);   // CAT: 3 arguments (0x00545dd0)
void univAddToWorldList(Derelict *world);
void univRemoveFromWorldList(Derelict *world);

Missile *univAddMissile(ShipRace race);

Derelict *univAddDerelict(DerelictType derelicttype,vector *pos);
Derelict *univAddDerelictByStatInfo(DerelictType derelictType, DerelictStaticInfo *stat, vector *pos);
Derelict *univAddHyperspaceGateAsDerelict(hvector *posAndRotation);

void ObjectsCollided(SpaceObjRotImpTarg *obj1,SpaceObjRotImpTarg *obj2,real32 colldist,vector *distvector,real32 dist,real32 distsquared);
void ApplyDamageToCollidingObjects(SpaceObjRotImpTarg *obj1,SpaceObjRotImpTarg *obj2,vector *distvector,real32 dist);
bool32 ApplyDamageToTarget(SpaceObjRotImpTarg *target,real32 damagetaken,GunSoundType soundType,sdword damageHow,sdword playerIndex,
                           struct Ship *attacker,sdword callSite);   // CAT: 7 arguments (0x00546250)

// CAT: new first argument applyDamage (0x00546a20; FALSE for the beam frames that only draw the hit)
void univBulletCollidedWithTarget(bool32 applyDamage,SpaceObjRotImpTarg *target,StaticInfo *targetstaticheader,Bullet *bullet,real32 collideLineDist,sdword collSide);
void univMissileCollidedWithTarget(bool32 applyDamage,SpaceObjRotImpTarg *target,StaticInfo *targetstaticheader,Missile *missile,real32 collideLineDist,sdword collSide);

SelectCommand *getEnemiesWithinProximity(Ship *thisship,real32 retaliateZone);   // CAT: a static selection (do not free)
void cat_univLimitProtectedShipSpeed(void);   // CAT: 0x00544e00

#define PLAYERKILLED_DROPPEDOUT         -1
#define PLAYERKILLED_STANDARDDEATH      0
#define PLAYERKILLED_CAPTUREDSHIP       1
#define PLAYERKILLED_LOSTMISSION        2

void univKillPlayer(sdword i,sdword playerdeathtype);

void CheckPlayerWin(void);

void InitializeEngineTrails(Ship *newship);

shipbindings *univMeshBindingsDupe(ShipStaticInfo *shipstaticinfo, Ship *newShip);
void univResetNewGimbleGun(Gun *gun);

// CAT: mesh bindings and the per-vertex mesh state (UnivUpdate.c)
void cat_univMeshBindingsLinkParents(ShipStaticInfo *shipstaticinfo, shipbindings *bindings);   // 0x00541ae0
shipbindings *cat_univMeshBindingsDupeNoGuns(ShipStaticInfo *shipstaticinfo);                   // 0x005418f0
sdword cat_univMeshStateAllocRecursive(CatMeshObjectState *slots, sdword index, polygonobject *object); // 0x00541b80
sdword cat_univMeshStateFreeRecursive(CatMeshObjectState *slots, sdword index, polygonobject *object);  // 0x00541c70
void cat_univShipMeshStateCreate(SpaceObj *obj);                                                // 0x00541cd0
void cat_univMeshStateFree(SpaceObj *obj);                                                      // 0x00541df0
void cat_univFreeEffectBlock(SpaceObj *obj);                    // the inline +0x84 free of the exe's free paths
mhlocalbinding *cat_shipNozzleInfoGet(Ship *ship, sdword index);                                // 0x0053f8c0
void cat_meshObjectMatrixConcat(hmatrix *dest, mhlocalbinding *binding);                        // 0x0053f900
void cat_meshObjectMatrixGet(hmatrix *dest, mhlocalbinding *binding);                           // 0x0053f970
extern bool32 catMeshStateDisabled;                             // 0x00c5236c (low-memory switch)
void cat_bulletHomingUpdate(Bullet *bullet);                                                    // 0x00542cb0
void cat_univShipSelfRepair(Ship *ship);                                                        // 0x005433d0
void cat_univShipDeferredUpdate(Ship *ship);                                                    // 0x005447b0 (partial)
void cat_shipUpdateExternalBuildAndAI(Ship *ship);                                              // 0x00543f10 (M2-R6 managers)
void cat_univRemoveObjReferencesFromShips(SpaceObj *obj);                                       // 0x0054d350
void cat_univRequestBulletEffectsRecreate(void);                                                // 0x0054d240
void cat_univRecreateBulletEffects(void);                                                       // 0x0054d250
void cat_univRecreateBulletEffectsIfNeeded(void);                                               // 0x0054d330
void cat_univDeleteBulletNow(Bullet *bullet);                                                   // 0x0054d110
void cat_univCreateMothershipModules(Ship *host, ShipRace race, vector *pos, struct Player *player, sdword built);   // 0x005420b0
void cat_univSetShipPosition(Ship *ship, vector *position);                                     // 0x0054d180
bool32 cat_univPlayerHasFlaggedShip(void);                                                      // 0x0054d200
sdword cat_univGetShipCountSalvaged(sdword shipType, sdword playerIndex);                       // 0x00540030
sdword cat_univGetShipCountBeastified(sdword shipType, sdword playerIndex);                     // 0x00540080
Ship *cat_univConvertShip(Ship *ship, ShipType newType, struct Player *newOwner);                 // 0x00543950
bool32 cat_univPointInUniverse(vector *pos);                                                      // 0x005c1f70
Resource *cat_univAddMimicResource(sdword asteroidtype, vector *pos);                              // 0x005408b0
void cat_univDeleteMimicResource(Resource *resource);                                                  // 0x00548f40
void cat_univRemoveTargetFromRammingFrigates(void *obj);                                               // 0x00549230
bool32 cat_univBeastInfectionUpdate(Ship *ship);                                                 // 0x00543860
void cat_univShipSalvagedCountInc(sdword a, sdword b);                                          // 0x005400d0
sdword cat_univGetNumberShipsPlayerKilled(sdword playerIndex, sdword shipClass);                // 0x005400f0
sdword cat_univGetNumberShipsPlayerLost(sdword playerIndex, sdword shipClass);                  // 0x00540110
extern bool32 catMultiplayerAutoSave;                                                           // 0x00a74dac (/SaveMPGames)
extern bool32 catUnivFrameStep;                                                                 // 0x00a6ed74
extern sdword catUnivFrameStepsLeft;                                                            // 0x00a6ed78
void cat_univRemoveBulletReferencesFromShips(Bullet *bullet);                                   // 0x0054b1a0
void univFreeBulletContents(Bullet *bullet);

void InitializeNavLights(Ship *newship);

bool32 univFindBackupMothership(struct Player *player);

// fast id table lookup stuff
void univInitFastNetworkIDLookups(void);
void univCloseFastNetworkIDLookups(void);
void univResetFastNetworkIDLookups(void);
void IDToPtrTableAdd(IDToPtrTable *table,uword ID,SpaceObj *obj);
// fast id table save stuff
void LoadIDToPtrTable(IDToPtrTable *table);
void SaveIDToPtrTable(IDToPtrTable *table);

void ApplyCareenRotationDirectly(Ship *ship);

bool32 univObjectOutsideWorld(SpaceObj *obj);

void univKillOtherPlayersIfDead(Ship *ship);


/*=============================================================================
    Data:
=============================================================================*/

extern udword NUM_STARS;
extern udword NUM_BIG_STARS;

extern vector defaultshipupvector;
extern vector defaultshiprightvector;
extern vector defaultshipheadingvector;
extern matrix defaultshipmatrix;

extern IDToPtrTable ShipIDToPtr;
extern IDToPtrTable ResourceIDToPtr;
extern IDToPtrTable DerelictIDToPtr;
extern IDToPtrTable MissileIDToPtr;

#endif
