/*=============================================================================
    Name    : Gun.h
    Purpose : Definitions for Gun.c

    Created 6/30/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___GUN_H
#define ___GUN_H

#include "SpaceObj.h"
#include "render.h"

/*=============================================================================
    Defines:
=============================================================================*/

#ifdef HW_BUILD_FOR_DISTRIBUTION

#define GUN_TUNE_MODE       0
#define GUN_VERBOSE_LEVEL   0

#else

#define GUN_TUNE_MODE       1
#define GUN_VERBOSE_LEVEL   1

#endif

#define GUN_RecoilTableLength   255
#define GUN_RecoilTime    0.05f
#define GUN_OneOverTime   20.0f
#define ONEPLUS_HIGH_TIME   1.05f
#define GUN_ResetAngleSpeedFactor 0.10f

/*=============================================================================
    Functions:
=============================================================================*/

bool32 gunCanShoot(Ship *ship, Gun *gun);
// CAT (M2-R3 ships-a): Cataclysm's gun queries that the reused HW1 ship bodies call (see Gun.c)
bool32 cat_gunCanFire(Ship *ship, Gun *gun);
real32 cat_gunGetBulletSpeed(Gun *gun);
bool32 cat_gunFixedGunAimedAtTarget(Ship *ship, Gun *gun, SpaceObjRotImpTarg *target, bool32 useGunHeading);
bool32 gunOrientGimbleGun(Ship *ship,Gun *gun,SpaceObjRotImpTarg *target);
void missileShoot(Ship *ship,Gun *gun,SpaceObjRotImpTarg *target);
Bullet *gunShoot(Ship *ship,Gun *gun, SpaceObjRotImpTarg *target);          // CAT: returns the bullet (0x0048b5b0)
Bullet *cat_gunCreateBullet(Ship *ship, Gun *gun, SpaceObj *from, SpaceObjRotImpTarg *target, real32 damage);   // CAT: 0x0048bb00
bool32 cat_gunInSecondMode(Gun *gun);                                     // CAT: the second fire mode test the exe inlines
SpaceObjRotImpTarg *cat_gunPickMothershipModuleTarget(Ship *ship, SpaceObjRotImpTarg *target);   // CAT: 0x0048b950 (M2-R5 layout-dock)
sdword cat_gunSoundTypeLookup(sdword weapon, sdword gunSize);             // CAT: 0x0048b7c0
void cat_gunStaticSetSoundTypes(GunStatic *gunstatic);                    // CAT: 0x0048b790
GunSoundType cat_gunGetSoundType(Gun *gun);                               // CAT: 0x0048b8c0
bool32 gunShootGunsAtTarget(Ship *ship,SpaceObjRotImpTarg *target,real32 range,vector *trajectory);
bool32 gunShootGunsAtMultipleTargets(Ship *ship);

bool32 gunMatrixUpdate(udword flags, hmatrix *startMatrix, hmatrix *matrix, void *data, smemsize ID);

#if RND_VISUALIZATION
void gunDrawGunInfo(Ship *ship);
#endif

//gun location in world code for silly sound boys
void gunGetGunPositionInWorld(vector *positionInWorldCoordSys,matrix *coordsys,Gun *gun);

//compute firepower
real32 gunShipFirePower(ShipStaticInfo *info);                             // CAT: one argument (0x0048cb60)

bool32 gunResetGimbleGuns(Ship *ship);

void gunStartup(void);
void gunShutdown(void);
#if GUN_TUNE_MODE
void gunTuneGun(Ship *ship);
#endif

/*=============================================================================
    Macros:
=============================================================================*/
// only valid for GUN_MissileLauncher
#define gunHasMissiles(gun) ((gun)->numMissiles > 0)

/*=============================================================================
    Variables:
=============================================================================*/

extern bool32 vanillaGunsMode;

#if GUN_TUNE_MODE
extern bool32 gunTuningMode;
extern sdword tuningGun;
#endif

#endif
