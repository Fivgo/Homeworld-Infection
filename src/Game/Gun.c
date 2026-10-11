/*=============================================================================
    Name    : Gun.c
    Purpose : Handles gun firing

    Created 6/30/1997 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AIShip.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "Debug.h"
#include "ETG.h"
#include "FastMath.h"
#include "glinc.h"
#include "Globals.h"
#include "Gun.h"
#include "LinkedList.h"
#include "MadLinkIn.h"
#include "MadLinkInDefs.h"
#include "mainrgn.h"
#include "Matrix.h"
#include "Memory.h"
#include "MEX.h"
#include "NIS.h"
#include "Options.h"
#include "Physics.h"
#include "prim3d.h"
#include "Randy.h"
#include "render.h"
#include "Ships.h"
#include "SoundEvent.h"
#include "SoundEventDefs.h"
#include "Tactics.h"
#include "Tweak.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "Vector.h"

#define GUNBURST_STATE_READY        0
#define GUNBURST_STATE_BURST        1
#define GUNBURST_STATE_COOLOFF      2

/*=============================================================================
    Data:
=============================================================================*/
real32 gunRecoilTable[GUN_RecoilTableLength + 1];
bool32 vanillaGunsMode = FALSE;

/*=============================================================================
    Functions:
=============================================================================*/

/*-----------------------------------------------------------------------------
    CAT (M2-R3 ships-a; M2-R4 combat): the Cataclysm gun queries. In the exe each first asks whether the gun is in
    its upgraded second fire mode: its weapon word (Gun +0x00, Gun.catWeaponType) is the gun's EndUpgradeLevel and
    that differs from its StartUpgradeLevel (the test is inlined at every user). Then it uses the "2" keys
    (firetime2, bulletspeed2, bulletlifetime2) and the second sound type (GunStatic +0x08).
-----------------------------------------------------------------------------*/
bool32 cat_gunInSecondMode(Gun *gun)
{
    GunStatic *gunstatic = gun->gunstatic;

    return (gun->catWeaponType == gunstatic->endUpgradeLevel) &&
           (gunstatic->startUpgradeLevel != gunstatic->endUpgradeLevel);
}

// the damage (sound) type of a weapon of a gun size (cat_gunSoundTypeLookup 0x0048b7c0): GS_* by WEAPON_* and GUNSIZE_*
sdword cat_gunSoundTypeLookup(sdword weapon, sdword gunSize)              // 0x0048b7c0
{
    switch (weapon)
    {
        case WEAPON_ENERGY:
            switch (gunSize)
            {
                case GUNSIZE_LARGE:     return GS_LargeEnergyCannon;
                case GUNSIZE_MEDIUM:    return GS_MediumEnergyCannon;
                case GUNSIZE_SMALL:     return GS_SmallEnergyCannon;
                case GUNSIZE_VERYLARGE: return GS_VeryLargeEnergyCannon;
            }
            break;
        case WEAPON_ION:
            switch (gunSize)
            {
                case GUNSIZE_LARGE:     return GS_LargeIonCannon;
                case GUNSIZE_MEDIUM:    return GS_MediumIonCannon;
                case GUNSIZE_SMALL:     return GS_SmallIonCannon;
                case GUNSIZE_VERYLARGE: return GS_VeryLargeIonCannon;
            }
            break;
        case WEAPON_PLASMABOMB:
            switch (gunSize)
            {
                case GUNSIZE_LARGE:     return GS_LargePlasmaBomb;
                case GUNSIZE_MEDIUM:    return GS_MediumPlasmaBomb;
                case GUNSIZE_SMALL:     return GS_SmallPlasmaBomb;
                case GUNSIZE_VERYLARGE: return GS_VeryLargePlasmaBomb;
            }
            break;
        case WEAPON_PROJECTILE:
            switch (gunSize)
            {
                case GUNSIZE_LARGE:     return GS_LargeProjectile;
                case GUNSIZE_MEDIUM:    return GS_MediumProjectile;
                case GUNSIZE_SMALL:     return GS_SmallProjectile;
            }
            break;
        case WEAPON_MINE:               return GS_MineLauncher;
        case WEAPON_MISSILE:            return GS_MissleLauncher;
        case WEAPON_LASER:              return GS_Laser;
        case WEAPON_BIGGUN:             return GS_BigGun;
        case WEAPON_BEASTINFECT:        return GS_BeastWeapon;
        default:
            break;
    }
    // the exe returns the gun size for a combination it has no row for
    return gunSize;
}

// the two damage types of a gun: its first (StartUpgradeLevel) and second (EndUpgradeLevel) weapon at its size, into
// the HW1 gunsoundtype (+0x04) and bulletType (+0x08) words (cat_gunStaticSetSoundTypes 0x0048b790; called for every
// gun of a new ship and of a loaded one)
void cat_gunStaticSetSoundTypes(GunStatic *gunstatic)                     // 0x0048b790
{
    gunstatic->gunsoundtype = (GunSoundType)cat_gunSoundTypeLookup(gunstatic->startUpgradeLevel, gunstatic->gunSize);
    gunstatic->bulletType = (BulletType)cat_gunSoundTypeLookup(gunstatic->endUpgradeLevel, gunstatic->gunSize);
}

// the damage type a gun fires now (cat_gunGetSoundType 0x0048b8c0, also inlined in cat_gunCreateBullet)
GunSoundType cat_gunGetSoundType(Gun *gun)                                 // 0x0048b8c0
{
    if (cat_gunInSecondMode(gun))
    {
        return (GunSoundType)gun->gunstatic->bulletType;
    }
    return gun->gunstatic->gunsoundtype;
}

bool32 cat_gunCanFire(Ship *ship, Gun *gun)                                 // 0x0048a1c0
{
    GunStatic *gunstatic = gun->gunstatic;
    real32 elapsed, latest;
    sdword i, numSecondMode;
    Gun *other;

    // a dissolving ship does not fire
    if ((ship != NULL) && (ship->catDissolving == TRUE))
    {
        return FALSE;
    }
    if (!cat_gunInSecondMode(gun))
    {
        return gunCanShoot(ship, gun);
    }

    elapsed = universe.totaltimeelapsed - gun->lasttimefired;
    if (gunstatic->firetime2 <= 0.0f)
    {
        // the exe prints "Hobbes call Mike upgraded weapon does..." (no firetime2) and uses firetime
        return gunstatic->firetime < elapsed;
    }
    if (elapsed <= gunstatic->firetime2)
    {
        return FALSE;
    }
    // the second-mode guns of a ship share firetime2: with n of them, one may fire every firetime2 / n
    latest = gun->lasttimefired;
    numSecondMode = 0;
    for (i = 0; i < ship->gunInfo->numGuns; i++)
    {
        other = &ship->gunInfo->guns[i];
        if (cat_gunInSecondMode(other))
        {
            numSecondMode++;
            if (latest < other->lasttimefired)
            {
                latest = other->lasttimefired;
            }
        }
    }
    if (numSecondMode > 1)
    {
        return latest < universe.totaltimeelapsed - gunstatic->firetime2 / (real32)numSecondMode;
    }
    return TRUE;
}

real32 cat_gunGetBulletSpeed(Gun *gun)                                      // 0x0048b8e0
{
    GunStatic *gunstatic = gun->gunstatic;

    if (cat_gunInSecondMode(gun))
    {
        // the exe falls back to the first speed only when bulletspeed2 <= 0 and bulletspeed > 0
        // [audit: M2-R3 ships-a; was "bulletspeed2 > 0 ? bulletspeed2 : bulletspeed", which differs when both are <= 0]
        if ((gunstatic->bulletspeed2 <= 0.0f) && (gunstatic->bulletspeed > 0.0f))
        {
            return gunstatic->bulletspeed;
        }
        return gunstatic->bulletspeed2;
    }
    return gunstatic->bulletspeed;
}

bool32 cat_gunFixedGunAimedAtTarget(Ship *ship, Gun *gun, SpaceObjRotImpTarg *target, bool32 useGunHeading)    // 0x0048cd40
{
    GunStatic *gunstatic = gun->gunstatic;
    vector trajectory;
    vector gunposition;
    vector heading;
    vector worldheading;
    real32 dist, cosang, sinang;

    // where the target will be, seen from the gun's muzzle
    aishipGetTrajectoryWithVelPrediction(ship, target, cat_gunGetBulletSpeed(gun), &trajectory);
    matMultiplyMatByVec(&gunposition, &ship->rotatable.rotinfo.coordsys, &gunstatic->position);
    vecSubFrom(trajectory, gunposition);

    // the gun's own heading, or the ship's heading column (the exe rotates either by the ship's coordsys)
    if (useGunHeading)
    {
        heading = gun->gunheading;
    }
    else
    {
        matGetVectFromMatrixCol3(heading, ship->rotatable.rotinfo.coordsys);
    }
    matMultiplyMatByVec(&worldheading, &ship->rotatable.rotinfo.coordsys, &heading);

    dist = fsqrt(vecMagnitudeSquared(trajectory));
    cosang = vecDotProduct(worldheading, trajectory) * (1.0f / dist);
    if (cosang > 0.0f)
    {
        if (cosang > 1.0f)
        {
            cosang = 1.0f;
        }
        sinang = fsqrt(1.0f - cosang * cosang);
        // the line of fire passes within 3/4 of the target's collision sphere
        if (sinang * dist < target->staticinfo->common.staticCollInfo.collspheresize * 0.75f)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : gunCanShoot
    Description : returns TRUE if gun can shoot
    Inputs      : gun
    Outputs     :
    Return      : returns TRUE if gun can shoot
----------------------------------------------------------------------------*/
bool32 gunCanShoot(Ship *ship, Gun *gun)
{
    GunStatic *gunstatic = gun->gunstatic;
    CommandToDo *command = getShipAndItsCommand(&universe.mainCommandLayer,ship);
    real32 firetime, healthFraction, compare;

    // CAT: gunCanShoot 0x0048a2f0 (the delta map's IDENTICAL; the body is Cataclysm's): no fuel test; a damaged ship
    //      fires and bursts more slowly (x1.25 below 3/4 health, x1.75 below 1/2, x2.5 below 1/4, x4 below 1/10,
    //      except the big gun and the Beast weapon); no tactics or formation burst modifiers; the burst state is
    //      mirrored in Ship+0x610 (catBurstFiring); the passive-attack penalty is checked after a burst as well
    healthFraction = ship->targetable.health / ship->staticinfo->health.maxhealth;
    if (gunstatic->burstFireTime == 0.0f)  // this gun doesn't fire in bursts
    {
        firetime = gunstatic->firetime;
        if ((gunstatic->gunsoundtype != GS_BeastWeapon) && (gunstatic->gunsoundtype != GS_BigGun))
        {
            if (healthFraction < 0.1f)
            {
                firetime = firetime * (real32)4.0;
            }
            else if (healthFraction < 0.25f)
            {
                firetime = firetime * (real32)2.5;
            }
            else if (healthFraction < 0.5f)
            {
                firetime = firetime * (real32)1.75;
            }
            else if (healthFraction < 0.75f)
            {
                firetime = firetime * (real32)1.25;
            }
        }
        return firetime < (universe.totaltimeelapsed - gun->lasttimefired);
    }

    // this gun fires in bursts
    switch (gun->burstState)
    {
        case GUNBURST_STATE_BURST:
            ship->catBurstFiring = TRUE;
            if ((universe.totaltimeelapsed - gun->burstStartTime) <= gun->useBurstFireTime)
            {
                goto checkfiretime;
            }
            gun->burstState = GUNBURST_STATE_COOLOFF;
            gun->burstStartTime = universe.totaltimeelapsed;
            soundEventBurstStop(ship, gun);
            // fall through to GUNBURST_STATE_COOLOFF
        case GUNBURST_STATE_COOLOFF:
            ship->catBurstFiring = FALSE;
            if ((universe.totaltimeelapsed - gun->burstStartTime) <= gun->useBurstWaitTime)
            {
                return FALSE;
            }
            gun->burstState = GUNBURST_STATE_READY;
            // fall through to GUNBURST_STATE_READY
        case GUNBURST_STATE_READY:
            gun->burstStartTime = universe.totaltimeelapsed;
            gun->burstState = GUNBURST_STATE_BURST;
            gun->useBurstFireTime = gunstatic->burstFireTime;
            gun->useBurstWaitTime = gunstatic->burstWaitTime;
            if (healthFraction < 0.1f)
            {
                gun->useBurstWaitTime = gunstatic->burstWaitTime * (real32)4.0;
            }
            else if (healthFraction < 0.25f)
            {
                gun->useBurstWaitTime = gunstatic->burstWaitTime * (real32)2.5;
            }
            else if (healthFraction < 0.5f)
            {
                gun->useBurstWaitTime = gunstatic->burstWaitTime * (real32)1.75;
            }
            else if (healthFraction < 0.75f)
            {
                gun->useBurstWaitTime = gunstatic->burstWaitTime * (real32)1.25;
            }
            break;
        default:
            break;
    }

checkfiretime:
    if ((command != NULL) && (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING) &&
        !ship->staticinfo->passiveAttackPenaltyExempt &&
        !((command->ordertype.attributes & COMMAND_MASK_PROTECTING) &&
          (command->ordertype.attributes & COMMAND_MASK_FORMATION) &&
          (command->formation.formationtype == SPHERE_FORMATION)))
    {
        //ship is passive attacking, so lets limit its gun fire time
        firetime = tacticsInfo.PassiveRetaliationFireTimeModifierLow + gunstatic->firetime;
        compare = universe.totaltimeelapsed - gun->lasttimefired;
        if ((firetime < compare) &&
            ((tacticsInfo.PassiveRetaliationFireTimeModifierHi - tacticsInfo.PassiveRetaliationFireTimeModifierLow) *
             (1.0f / 4294967296.0f) * (real32)gamerand() + tacticsInfo.PassiveRetaliationFireTimeModifierLow + firetime < compare))
        {
            //gun can now fire
            return TRUE;
        }
        return FALSE;
    }
    return gunstatic->firetime < (universe.totaltimeelapsed - gun->lasttimefired);
}

/*-----------------------------------------------------------------------------
    Name        : gunNewGimbleUpdateCoordSys
    Description : update a New_Gimble gun's coordinate system to reflect gun->angle, gun->declination
    Inputs      : gun, gunstatic
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void gunNewGimbleUpdateCoordSys(Gun *gun,GunStatic *gunstatic)
{
    matrix newmat,rotxmatrix,rotymatrix;

    matMakeRotAboutY(&rotymatrix,(real32)cos(gun->declination),(real32)sin(gun->declination));
    matMakeRotAboutX(&rotxmatrix,(real32)cos(gun->angle),(real32)sin(gun->angle));

    matMultiplyMatByMat(&newmat,&gunstatic->defaultgunorientation,&rotxmatrix);
    matMultiplyMatByMat(&gun->curgunorientation,&newmat,&rotymatrix);
    if (bitTest(gunstatic->flags, GF_MultiLevelMatrix))
    {
        matMultiplyMatByMat(&newmat,&gunstatic->defaultGunOrientationNonConcat,&rotymatrix);
        matMultiplyMatByMat(&gun->curGunOrientationNonConcat,&newmat,&rotxmatrix);
    }

    matGetVectFromMatrixCol3(gun->gunheading,gun->curgunorientation);
}

/*-----------------------------------------------------------------------------
    Name        : gunUpdateSlave
    Description : Update a slave gun's status from it's slave driver
    Inputs      : gun - slave gun
                  guns - list of gunst
                  iSlaveDriver - index of slaveDriver gun
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void gunUpdateSlave(Gun *gun, GunInfo *guns, sdword iSlaveDriver)
{
    Gun *slaveDriver;

    dbgAssertOrIgnore(iSlaveDriver >= 0 && iSlaveDriver < guns->numGuns);
    slaveDriver = &guns->guns[iSlaveDriver];
    gun->gunheading = slaveDriver->gunheading;              //copy the essentials from the slave driver
    gun->curgunorientation = slaveDriver->curgunorientation;
    gun->declination = slaveDriver->declination;
    gun->angle = slaveDriver->angle;
}

/*-----------------------------------------------------------------------------
    Name        : gunOrientGimbleGun
    Description : if possible, orients the gimble gun to point at target
    Inputs      : gun,target
    Outputs     :
    Return      : TRUE if gun oriented as desired
----------------------------------------------------------------------------*/
bool32 gunOrientGimbleGun(Ship *ship,Gun *gun,SpaceObjRotImpTarg *target)
{
    GunStatic *gunstatic = gun->gunstatic;
    vector targetInWorldCoordSys;
    vector targetInShipCoordSys;
    vector targetInGunCoordSys;
    real32 dotprod,bulletspeed;
    real32 triggerHappy, maxAngle, maxAngleSpeed;

    real32 tryangle;
    real32 turndeclination;
    real32 desireddeclinationspeed;
    real32 turnangle;
    real32 desiredanglespeed;

    bool32 updateguncoordsys;

    if (bitTest(ship->header.flags, SOF_NISShip))
    {
        triggerHappy = NIS_TriggerHappyAngle;
        maxAngle = NIS_TriggerHappyAngle;
        maxAngleSpeed = gunstatic->maxanglespeed;
    }
    else
    {
        // CAT: gunOrientGimbleGun 0x0048a5e0 has no HW1 ion-cannon angles against fighters and corvettes
        triggerHappy = gunstatic->triggerHappy;
        maxAngle = gunstatic->cosmaxAngleFromNorm;
        maxAngleSpeed = gunstatic->maxanglespeed;
    }
    // CAT: the second fire mode's speed, no HW1 tactics bonus (the exe inlines cat_gunGetBulletSpeed 0x0048b8e0)
    bulletspeed = cat_gunGetBulletSpeed(gun);


    if (gunstatic->guntype == GUN_Gimble)
    {
        aishipGetTrajectoryWithVelPrediction(ship,target,bulletspeed,&targetInWorldCoordSys);
//        vecSub(targetInWorldCoordSys,target->obj.posinfo.position,ship->obj.posinfo.position);

        matMultiplyVecByMat(&targetInShipCoordSys,&targetInWorldCoordSys,&ship->rotatable.rotinfo.coordsys);

        vecSubFrom(targetInShipCoordSys,gunstatic->position);

        vecNormalize(&targetInShipCoordSys);

        dotprod = vecDotProduct(targetInShipCoordSys,gunstatic->gunnormal);
        if (dotprod > 1.0f)
        {
            dotprod = 1.0f;     // do this because vecNormalize does not normalize perfectly
        }
        if (isBetweenInclusive(dotprod,maxAngle,gunstatic->cosminAngleFromNorm))
        {
            gun->gunheading = targetInShipCoordSys;

            soundEventStop(gun->gimblehandle);
            gun->gimblehandle = -1;

            return TRUE;
        }

        if (gun->gimblehandle < 0)
        {
            gun->gimblehandle = soundEventPlay(ship, Gun_WeaponMove, gun);
        }

        return FALSE;
    }
    else if (gunstatic->guntype == GUN_NewGimble)
    {
        if (gunstatic->slaveDriver >= 0)
        {                                                   //if this gun has a slavedriver
            gunUpdateSlave(gun, ship->gunInfo, gunstatic->slaveDriver);
        }

        aishipGetTrajectoryWithVelPrediction(ship,target,bulletspeed,&targetInWorldCoordSys);
//        vecSub(targetInWorldCoordSys,target->obj.posinfo.position,ship->obj.posinfo.position);

        matMultiplyVecByMat(&targetInShipCoordSys,&targetInWorldCoordSys,&ship->rotatable.rotinfo.coordsys);

        // we have targetInShipCoordSys.  Now change to gun's coordinate system

        vecSubFrom(targetInShipCoordSys,gunstatic->position);

        vecNormalize(&targetInShipCoordSys);

        matMultiplyVecByMat(&targetInGunCoordSys,&targetInShipCoordSys,&gun->curgunorientation);

        dotprod = targetInGunCoordSys.z;        // (0,0,1) dot (x,y,z) == z

        gun->anglespeed = 0.0f;
        gun->declinationspeed = 0.0f;

        // CAT: a gun whose first weapon is a beam (WEAPON_ION, WEAPON_LASER) keeps tracking while on target (the exe
        //      tests StartUpgradeLevel where HW1 tests the BULLET_Beam bullet type)
        if ((dotprod >= triggerHappy) && (gunstatic->startUpgradeLevel != WEAPON_ION) &&
            (gunstatic->startUpgradeLevel != WEAPON_LASER))
        {
            goto onTargetExitEarly;
        }

        updateguncoordsys = FALSE;

        if (gunstatic->slaveDriver < 0)
        {                                                   //only do updates for free guns
            // always want target to be on positive z axis in gun coordinate system (i.e. gun barrel on z should point to target)
            if (targetInGunCoordSys.y != 0)
            {
                desiredanglespeed = -targetInGunCoordSys.y * gunstatic->angletracking;
                if (desiredanglespeed > maxAngleSpeed)
                {
                    desiredanglespeed = maxAngleSpeed;
                }
                else if (desiredanglespeed < -maxAngleSpeed)
                {
                    desiredanglespeed = -maxAngleSpeed;
                }

                turnangle = desiredanglespeed * universe.phystimeelapsed;
                tryangle = turnangle + gun->angle;
                if ((tryangle < gunstatic->maxturnangle) && (tryangle > gunstatic->minturnangle))
                {
                    gun->angle = tryangle;
                    gun->anglespeed = desiredanglespeed;
                    updateguncoordsys = TRUE;
                }

            }

            if (targetInGunCoordSys.x != 0)
            {
                desireddeclinationspeed = targetInGunCoordSys.x * gunstatic->declinationtracking;
                if (desireddeclinationspeed > maxAngleSpeed)
                {
                    desireddeclinationspeed = maxAngleSpeed;
                }
                else if (desireddeclinationspeed < -maxAngleSpeed)
                {
                    desireddeclinationspeed = -maxAngleSpeed;
                }

                turndeclination = desireddeclinationspeed * universe.phystimeelapsed;
                tryangle = turndeclination + gun->declination;
                if ((tryangle < gunstatic->maxdeclination) && (tryangle > gunstatic->mindeclination))
                {
                    gun->declination = tryangle;
                    gun->declinationspeed = desireddeclinationspeed;
                    updateguncoordsys = TRUE;
                }
            }

            if (updateguncoordsys)
            {
                gunNewGimbleUpdateCoordSys(gun,gunstatic);
            }
        }

        if (dotprod >= triggerHappy)
        {
onTargetExitEarly:
            gun->gunheading = targetInShipCoordSys;

            soundEventStop(gun->gimblehandle);
            gun->gimblehandle = -1;

            return TRUE;
        }
        else
        {
            if (gun->gimblehandle < 0)
            {
                gun->gimblehandle = soundEventPlay(ship, Gun_WeaponMove, gun);
            }

            return FALSE;
        }
    }
    // CAT: any other gun type is not aimed (no assert in the exe)
    return(FALSE);
}

void gunGetGunPositionInWorld(vector *positionInWorldCoordSys,matrix *coordsys,Gun *gun)
{
    GunStatic *gunstatic = gun->gunstatic;
    matMultiplyMatByVec(positionInWorldCoordSys,coordsys,&gunstatic->position);
}

/*-----------------------------------------------------------------------------
    Name        : missileShoot
    Description : shoots a missile
    Inputs      : ship which owns gun, gun to fire missile
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void missileShoot(Ship *ship,Gun *gun,SpaceObjRotImpTarg *target)
{
    Missile *missile;
    GunStatic *gunstatic = gun->gunstatic;
    ShipStaticInfo *shipstatic = (ShipStaticInfo *)ship->staticinfo;
    etgeffectstatic *stat;
    etglod *etgLOD;
    sdword LOD,i;
    real32 floatDamage;
    udword intDamage;
    ShipRace shiprace = ship->shiprace;
    matrix newCoordsys;

    MinelayerCorvetteSpec *spec;

    vector gunheadingInWorldCoordSys;
    vector positionInWorldCoordSys,mineup,mineright,minehead;

// Wrapped with #ifdef ENABLE_SHIPRECOIL but unused.
//    vector recoil;
//    real32 massovertime;

    dbgAssertOrIgnore(gunstatic->guntype == GUN_MissileLauncher || gunstatic->guntype == GUN_MineLauncher);
    dbgAssertOrIgnore(gunHasMissiles(gun));

    // CAT: missileShoot 0x0048aac0: a ship being handed to a salvage depot does not fire, nor at such a ship
    if ((ship->header.objtype == OBJ_ShipType) && (ship->catSalvageDepot != NULL))
    {
        return;
    }
    if ((target != NULL) && (target->header.objtype == OBJ_ShipType) && (((Ship *)target)->catSalvageDepot != NULL))
    {
        return;
    }

    //line-of-sight check
    if (target != NULL && gunstatic->guntype == GUN_MissileLauncher)
    {
        vector gunDirection, gunPosition;

        matMultiplyMatByVec(&gunDirection, &ship->rotatable.rotinfo.coordsys, &gun->gunheading);
        matMultiplyMatByVec(&gunPosition, &ship->rotatable.rotinfo.coordsys, &gunstatic->position);
        vecAddTo(gunPosition, ship->obj.posinfo.position);
        if (!collCheckLineOfSight(ship, (Ship*)target, &gunPosition, &gunDirection))
        {
#if GUN_VERBOSE_LEVEL > 1
            dbgMessage("line-of-sight obscured, not shooting");
#endif
            return;
        }
    }

    gun->numMissiles--;
    gun->lasttimefired = universe.totaltimeelapsed;

    missile = memAlloc(sizeof(Missile),"Missile",0);
    memset(missile,0,sizeof(Missile));

    missile->header.objtype = OBJ_MissileType;
    missile->header.flags = SOF_Rotatable + SOF_Impactable + SOF_NotBumpable + SOF_Targetable;

    missile->enginePosition.x = 0.0f;
    missile->enginePosition.y = 0.0f;
    missile->enginePosition.z = 0.0f;
    missile->trail = NULL;

    if(gunstatic->guntype == GUN_MissileLauncher)
    {   //specific definitions for MISSILES
        missile->staticinfo = &missileStaticInfos[shiprace];
        missile->maxtraveldist = missile->staticinfo->common.maxvelocity * universe.phystimeelapsed;
        missile->missileType = MISSILE_Regular;
        missile->FORCE_DROPPED = FALSE;
        missile->formationinfo = NULL;
        missile->trail = mistrailNew(missile->staticinfo->trailStatic, missile);
    }   //specific definitions for MINES
    else
    {   //must be a mine launcher
        missile->staticinfo = &mineStaticInfos[shiprace];
        dbgAssertOrIgnore(ship->shiptype == MinelayerCorvette);
        spec = (MinelayerCorvetteSpec *)ship->ShipSpecifics;
        missile->missileType = MISSILE_Mine;
        missile->mineAIState = spec->mineaistate;       //initialize the Mines AI state
        if(spec->mineaistate == MINE_DROP_FORMATION)
        {
            listAddNode(&spec->mineforminfo->MineList,&(missile->formationLink),missile);
            missile->formationinfo = spec->mineforminfo;
            missile->formation_position = spec->formation_position;         //only way to communicate this info
            missile->formation_number_X = spec->formation_number_X;
            missile->formation_number_Y = spec->formation_number_Y;
            missile->maxtraveldist = ((MissileStaticInfo *)missile->staticinfo)->maxvelocity_FORCED * universe.phystimeelapsed;
            missile->FORCE_DROPPED = TRUE;                                  //set flag
        }
        else
        {
            missile->FORCE_DROPPED = FALSE;                                  //set flag
            missile->formationinfo = NULL;
            missile->maxtraveldist = missile->staticinfo->common.maxvelocity * universe.phystimeelapsed;
        }
    }
    missile->haveCheckedForLoss = 0;    //init target loss variable

    missile->colorScheme = ship->colorScheme;
    missile->targetable.health = missile->staticinfo->health.maxhealth;

    ClearNode(missile->obj.renderlink);
    missile->obj.currentLOD = ship->obj.currentLOD;
    missile->obj.collMyBlob = NULL;
    missile->obj.cameraDistanceSquared = ship->obj.cameraDistanceSquared;
    missile->targetable.flashtimer = 0;
    missile->targetable.lasttimecollided = 0;
    missile->obj.attributes = 0;
    missile->obj.attributesParam = 0;

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


    missile->soundType = cat_gunGetSoundType(gun);     // CAT: the gun's current damage type
    missile->race = shiprace;
    missile->timelived = 0.0f;
    missile->playerowner = ship->playerowner;
    missile->owner = ship;
    missile->target = target;

    if(missile->FORCE_DROPPED == TRUE)
    {   //force dropped mine only!
        MinelayerCorvetteStatics *staticinfo = (MinelayerCorvetteStatics *) ship->staticinfo->custstatinfo;
        missile->damage = (frandombetween(staticinfo->forced_drop_damage_lo,staticinfo->forced_drop_damage_hi));
        missile->totallifetime = staticinfo->forced_drop_lifetime;
    }
    else
    {   //normal
        // CAT: the damage is the ship's damage multiplier times the weapon against the target's armour (0 for a target
        //      that is no ship) times the gun's DamageMult, fixed at launch (missileShoot 0x0048aac0); no HW1 random
        //      tactics damage
        sdword armour = ((target != NULL) && (target->header.objtype == OBJ_ShipType)) ? ((Ship *)target)->catArmourLevel : 0;

        missile->damage = cat_shipGetDamageMultiplier(ship) * cat_weaponDamageVsArmour(missile->soundType, armour) * gunstatic->damageMult;
        missile->totallifetime = gunstatic->bulletlifetime;
    }

    //test to see if needs damage modifier
    if(bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_KasSpecialDamageModifier))
    {
        //ship is being 'beefed' up by KAS...add value to damagemultiplier
        missile->damage*=ship->damageModifier;
    }

    missile->missileID.missileNumber = universe.missileNumber;
    if (multiPlayerGame) IDToPtrTableAdd(&MissileIDToPtr,universe.missileNumber,(SpaceObj *)missile);
    universe.missileNumber++;

    // now fill in missile->obj.posinfo

    // heading
    matMultiplyMatByVec(&gunheadingInWorldCoordSys,&ship->rotatable.rotinfo.coordsys,&gun->gunheading);

    // position
    matMultiplyMatByVec(&positionInWorldCoordSys,&ship->rotatable.rotinfo.coordsys,&gunstatic->position);

    vecAdd(missile->obj.posinfo.position,positionInWorldCoordSys,ship->obj.posinfo.position);
    missile->obj.posinfo.position.x += gunheadingInWorldCoordSys.x * gunstatic->barrelLength;
    missile->obj.posinfo.position.y += gunheadingInWorldCoordSys.y * gunstatic->barrelLength;
    missile->obj.posinfo.position.z += gunheadingInWorldCoordSys.z * gunstatic->barrelLength;

    SET_MOVING_LINEARLY(missile->obj.posinfo.isMoving);
    missile->obj.posinfo.haventCalculatedDist = TRUE;

    vecScalarMultiply(missile->obj.posinfo.velocity,gunheadingInWorldCoordSys,gunstatic->bulletspeed);

    // now fill in missile->rotatable.rotinfo
    if (gunstatic->guntype == GUN_MineLauncher)
    {   //mine launcher so fix mine rotation depending on gun launched from...and race?
        //hard coded fix to a special exception :)

        if(ship->gunInfo->numGuns > 1)
        {    //race 1 ship..do rotation..later make global LOD on this..since its a waste of time..really
            //build mine coordinate system from the ships...BIG savings...
            if(((Gun *) &(ship->gunInfo->guns[0])) == gun)
            {
                matGetVectFromMatrixCol1(mineright,ship->rotatable.rotinfo.coordsys);
                matGetVectFromMatrixCol2(mineup,ship->rotatable.rotinfo.coordsys);
                matGetVectFromMatrixCol3(minehead,ship->rotatable.rotinfo.coordsys);
                vecScalarMultiply(mineup,mineup,-1.0f);
            }
            else
            {
                matGetVectFromMatrixCol1(mineright,ship->rotatable.rotinfo.coordsys);
                matGetVectFromMatrixCol2(mineup,ship->rotatable.rotinfo.coordsys);
                matGetVectFromMatrixCol3(minehead,ship->rotatable.rotinfo.coordsys);
                vecScalarMultiply(mineright,mineright, -1.0f);
            }
            matPutVectIntoMatrixCol1(mineup,missile->rotatable.rotinfo.coordsys);
            matPutVectIntoMatrixCol2(mineright,missile->rotatable.rotinfo.coordsys);
            matPutVectIntoMatrixCol3(minehead,missile->rotatable.rotinfo.coordsys);
        }
        else
        {
            // LM: what is this stranded code doing?
            matGetVectFromMatrixCol1(mineup,ship->rotatable.rotinfo.coordsys);
            matGetVectFromMatrixCol2(mineright,ship->rotatable.rotinfo.coordsys);
            matGetVectFromMatrixCol3(minehead,ship->rotatable.rotinfo.coordsys);
            matPutVectIntoMatrixCol1(mineup,missile->rotatable.rotinfo.coordsys);
            matPutVectIntoMatrixCol2(mineright,missile->rotatable.rotinfo.coordsys);
            matPutVectIntoMatrixCol3(minehead,missile->rotatable.rotinfo.coordsys);
            //            matCreateCoordSysFromHeading(&missile->rotatable.rotinfo.coordsys,&gunheadingInWorldCoordSys);
        }
    }
    else
    {
        matCreateCoordSysFromHeading(&missile->rotatable.rotinfo.coordsys,&gunheadingInWorldCoordSys);
    }

    univUpdateObjRotInfo((SpaceObjRot *)missile);

    if ((target != NULL) && (target->obj.collMyBlob != NULL))
    {
        collAddSpaceObjToSpecificBlob(target->obj.collMyBlob,(SpaceObj *)missile);
    }
    else
    {
        collAddSpaceObjToCollBlobs((SpaceObj *)missile);
    }
    listAddNode(&universe.SpaceObjList,&(missile->header.objlink),missile);
    listAddNode(&universe.ImpactableList,&(missile->impactable.impactablelink),missile);
    listAddNode(&universe.MissileList,&(missile->missilelink),missile);
    cat_univShipMeshStateCreate(&missile->obj);             // CAT: missileShoot 0x0048aac0 (NULL: not a ship)

    univAddObjToRenderListIf((SpaceObj *)missile,&ship->obj);     // add missile to render list if parent ship is in render list

//    if (gunstatic->burstFireTime == 0.0f)
    {
        soundEventPlay(ship, Gun_WeaponShot, gun);
    }

    //muzle flash effect trigger and set collision event
    missile->hitEffect = etgGunEventTable[shipstatic->shiprace][missile->soundType][EGT_GunHit];//get pointer to bullet effect
    etgLOD = etgGunEventTable[shipstatic->shiprace][missile->soundType][EGT_GunFire];//get pointer to bullet effect
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
//    stat = etgGunEventTable[shiprace][gunstatic->gunsoundtype][EGT_GunFire];//get gun effect

    if (univSpaceObjInRenderList(&ship->obj))
    {
#if ETG_DISABLEABLE
        if (stat != NULL && etgFireEffectsEnabled && etgEffectsEnabled && !etgFrequencyExceeded(stat))
#else
        if (stat != NULL && etgFireEffectsEnabled && !etgFrequencyExceeded(stat))
#endif
        {                                                       //if there is a gun fire effect
            real32 typeParam;

            matCreateCoordSysFromHeading(&newCoordsys,&gunheadingInWorldCoordSys);
            floatDamage = (real32)missile->damage;
            floatDamage *= ship->magnitudeSquared;
            intDamage = Real32ToUdword(floatDamage);
            // CAT: a second effect parameter by launcher type (0x0088153c/40/44: sDreadnought 40, sDestroyer 50, the
            //      Acolytes -15, else 0)
            switch (ship->shiptype)
            {
                case sDreadnought:  typeParam = 40.0f;  break;
                case sDestroyer:    typeParam = 50.0f;  break;
                case sAcolyte:
                case bAcolyte:      typeParam = -15.0f; break;
                default:            typeParam = 0.0f;   break;
            }
            etgEffectCreate(stat, ship, &missile->obj.posinfo.position, NULL, &newCoordsys, 1.0f, EAF_Velocity | EAF_NLips, 2,
                            SCALECAST(intDamage), SCALECAST(Real32ToUdword(typeParam)));
        }
    }
    // CAT: no HW1 cloak-field decloak here (the exe has none)
    if (target != NULL && target->header.objtype == OBJ_ShipType)
    {                                                       //if we succeeded in shooting at enemy
        ((Ship *)target)->firingAtUs = ship;
        ((Ship *)target)->recentlyFiredUpon = RECENT_ATTACK_DURATION;
    }
}


/*-----------------------------------------------------------------------------
    Name        : gunRecoilTableCompute
    Description : Computes and fills in the gun recoil motion table
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void gunRecoilTableCompute(void)
{
    sdword index;
    real32 timeIndex, value;

    for (index = 0, timeIndex = 0.0f; timeIndex < GUN_RecoilTime;
         index++, timeIndex += 1.0f / (real32)GUN_RecoilTableLength)
    {
        //y = 1 - ((x - .05) * 20)^2
        value = (timeIndex - GUN_RecoilTime) * GUN_OneOverTime;
        gunRecoilTable[index] = 1.0f - (value * value);
    }
    for (; index < GUN_RecoilTableLength; index++, timeIndex += 1.0f / (real32)GUN_RecoilTableLength)
    {
        //y = ((-atan((x-.4)*8)/PI+.5-.06538)/.825433
        gunRecoilTable[index] = (((real32)-atan((timeIndex-0.4f)*8.0f)/PI+0.5f)-0.06538f)/0.825433f;
    }
    gunRecoilTable[GUN_RecoilTableLength] = 0.0f;
}

/*-----------------------------------------------------------------------------
    Name        : gunRecoilVectorCompute
    Description : Compute a vector representing a gun's recoil as a function
                    of time between shots.
    Inputs      : dest - heading of gun mesh
                  recoilLength - length of the gun's recoil, in meters
                  lastFired - absolute time of the last gun shot
                  rateOfFire - time between shots
    Outputs     : dest - recoil vector to be added to position
    Return      :
----------------------------------------------------------------------------*/
void gunRecoilVectorCompute(vector *dest, real32 recoilLength, real32 lastFired, real32 rateOfFire)
{
    real32 timeElapsed, offset;
    sdword index;

    timeElapsed = universe.totaltimeelapsed - lastFired;
    if (timeElapsed > rateOfFire)
    {                                                       //if the gun is sitting idle
        dest->x = dest->y = dest->z = 0.0f;                 //no recoil at all
        return;
    }
    timeElapsed /= rateOfFire;                              //normalize time elapsed from 0 to 1

    index = (sdword)(timeElapsed * (real32)GUN_RecoilTableLength);
    offset = gunRecoilTable[index];                         //table-lookup for gun recoil curve
    offset *= -recoilLength;
    dest->x *= offset;
    dest->y *= offset;
    dest->z *= offset;
}

/*-----------------------------------------------------------------------------
    Name        : cat_gunCreateBullet (Cataclysm 0x0048bb00)
    Description : CAT: creates the bullet a gun of ship fires at target. gunShoot 0x0048b5b0 calls it with from == NULL
                  (the bullet leaves the gun's muzzle along the gun's heading); the Beast weapon's cascade calls it
                  with from == the infected ship (the bullet starts at ship and heads from from to target).
    Inputs      : ship, gun, from, target (may be NULL), damage
    Outputs     :
    Return      : the bullet
----------------------------------------------------------------------------*/
Bullet *cat_gunCreateBullet(Ship *ship, Gun *gun, SpaceObj *from, SpaceObjRotImpTarg *target, real32 damage)
{
    GunStatic *gunstatic = gun->gunstatic;
    ShipStaticInfo *shipstatic = ship->staticinfo;
    bool32 burst = bitTest(ship->shipOrDerelict.specialFlags, SPECIAL_BurstFiring);
    bool32 secondMode = cat_gunInSecondMode(gun);
    GunSoundType soundType = cat_gunGetSoundType(gun);
    Bullet *bullet;
    vector heading, position, offset0, offset1;
    real32 bulletspeed = 0.0f, effectDamage, effectLength;
    udword intDamage, intVelocity, intLength;
    etglod *etgLOD;
    etgeffectstatic *stat;
    sdword LOD;

    bullet = memAlloc(sizeof(Bullet),"Bu(Bullet)",Pyrophoric);
    bullet->obj.catEffectBlock = NULL;                      // +0x84 cleared first
    bullet->header.objtype = OBJ_BulletType;
    bullet->header.flags = 0;
    bullet->obj.staticinfo = NULL;
    ClearNode(bullet->obj.renderlink);
    bullet->obj.currentLOD = ship->obj.currentLOD;
    bullet->obj.collMyBlob = NULL;
    bullet->obj.cameraDistanceSquared = ship->obj.cameraDistanceSquared;
    cat_univShipMeshStateCreate(&bullet->obj);              // NULL: not a ship

    bullet->catBurst = burst;
    bullet->bulletType = gun->catWeaponType;               // the weapon word (SpaceObj.h)
    bullet->damage = damage;
    bullet->damageFull = damage;                            // HW1 field; the exe's +0x13C is catMoved
    bullet->soundType = soundType;
    bullet->owner = ship;
    bullet->playerowner = ship->playerowner;
    bullet->gunowner = gun;
    bullet->target = ((target != NULL) && !burst) ? target : NULL;
    bullet->bulletColor = etgBulletColor[shipstatic->shiprace][soundType];
    bullet->bulletmass = gunstatic->bulletmass;
    bullet->lengthmag = gunstatic->bulletlength;
    bullet->timelived = 0.0f;
    bullet->totallifetime = secondMode ? gunstatic->bulletlifetime2 : gunstatic->bulletlifetime;
    bullet->SpecialEffectFlag = 0;
    bullet->DFGFieldEntryTime = 0.0f;
    bullet->BulletSpeed = 0.0f;
    bullet->catMoved = FALSE;
    bullet->catBigGunRange = 0.0f;                          // CAT-TODO(M2-R4 #4): set for GS_BigGun shells by the big gun
    bullet->catBigGunTravelled = 0.0f;

    memset(&(bullet->obj.posinfo),0,sizeof(bullet->obj.posinfo));

    // heading: the gun's, or from from towards target
    if (from == NULL)
    {
        matMultiplyMatByVec(&heading,&ship->rotatable.rotinfo.coordsys,&gun->gunheading);
    }
    else
    {
        vecSub(heading, target->obj.posinfo.position, from->posinfo.position);
        vecNormalize(&heading);
    }
    bullet->bulletheading = heading;
    vecScalarMultiply(bullet->lengthvec,heading,gunstatic->bulletlength);
    matCreateCoordSysFromHeading(&bullet->rotatable.rotinfo.coordsys,&heading);

    // CAT: what moves depends on the weapon: shells and bolts fly, the ion beam, the laser and the Beast weapon do
    //      not (the exe's switch on Gun +0x00)
    switch (gun->catWeaponType)
    {
        case WEAPON_PROJECTILE:
        case WEAPON_ENERGY:
        case WEAPON_PLASMABOMB:
        case WEAPON_BIGGUN:
            if (burst)
            {
                bulletspeed = burstSpeed;
            }
            else
            {
                bulletspeed = cat_gunGetBulletSpeed(gun);
            }
            vecScalarMultiply(bullet->obj.posinfo.velocity,heading,bulletspeed);
            bullet->beamtraveldist = 0.0f;
            bullet->traveldist = universe.phystimeelapsed * bulletspeed;
            break;
        case WEAPON_ION:
        case WEAPON_LASER:
        case WEAPON_BEASTINFECT:
            vecZeroVector(bullet->obj.posinfo.velocity);
            bullet->traveldist = gunstatic->bulletlength;
            bullet->beamtraveldist = gunstatic->bulletlength;
            break;
        default:
            break;
    }

    // position: the gun's muzzle, or the ship itself
    if (from == NULL)
    {
        matMultiplyMatByVec(&position,&ship->rotatable.rotinfo.coordsys,&gunstatic->position);
        if (gunstatic->offset.x != 0.0f)
        {                                                       //if there is an offset vector
            matMultiplyMatByVec(&offset0, &gun->curgunorientation, &gunstatic->offset);//offset in gun space
            matMultiplyMatByVec(&offset1, &ship->rotatable.rotinfo.coordsys, &offset0);//offset in world space
            vecAddTo(position, offset1);
        }
        vecAdd(bullet->obj.posinfo.position,position,ship->obj.posinfo.position);
        bullet->obj.posinfo.position.x += heading.x * gunstatic->barrelLength;
        bullet->obj.posinfo.position.y += heading.y * gunstatic->barrelLength;
        bullet->obj.posinfo.position.z += heading.z * gunstatic->barrelLength;
    }
    else
    {
        bullet->obj.posinfo.position = ship->obj.posinfo.position;
    }

    SET_MOVING_LINEARLY(bullet->obj.posinfo.isMoving);
    bullet->obj.posinfo.haventCalculatedDist = TRUE;

    if ((target != NULL) && (target->obj.collMyBlob != NULL))
    {
        collAddSpaceObjToSpecificBlob(target->obj.collMyBlob,(SpaceObj *)bullet);
    }
    else
    {
        collAddSpaceObjToCollBlobs((SpaceObj *)bullet);
    }
    listAddNode(&universe.SpaceObjList,&(bullet->header.objlink),bullet);
    listAddNode(&universe.BulletList,&(bullet->bulletlink),bullet);

    univAddObjToRenderListIf((SpaceObj *)bullet,&ship->obj);     // add to render list if parent ship is in render list

    // CAT: the shot sound: a burst gun loops while it bursts, the ion beam loops, every other weapon but the Beast's
    //      plays a shot
    if (gunstatic->burstFireTime == 0.0f)
    {
        if (bullet->bulletType == WEAPON_ION)
        {
            soundEventPlay(ship, Gun_WeaponFireLooped, gun);
        }
        else if (soundType != GS_BeastWeapon)
        {
            soundEventPlay(ship, Gun_WeaponShot, gun);
        }
    }
    else if (!ship->soundevent.burstfiring)
    {
        soundEventPlay(ship, Gun_WeaponFireLooped, gun);
    }

    // CAT: the effects' damage parameter is the weapon's base damage against no armour, not the bullet's damage
    effectDamage = cat_weaponDamageVsArmour(soundType, 0) * gunstatic->damageMult * ship->magnitudeSquared;
    effectLength = (gunstatic->gunsoundtype == GS_BeastWeapon) ? 0.0f : gunstatic->bulletlength;
    intDamage = Real32ToUdword(effectDamage);
    intVelocity = Real32ToUdword(bulletspeed);
    intLength = (gunstatic->gunsoundtype == GS_BeastWeapon) ? 0 : Real32ToUdword(effectLength);

    //create an effect for bullet, if applicable
    if (burst)
    {
        etgLOD = etgSpecialPurposeEffectTable[EGT_BURST_BULLET_EFFECT];
    }
    else
    {
        etgLOD = etgGunEventTable[shipstatic->shiprace][soundType][EGT_GunBullet];//get pointer to bullet effect
    }
    stat = NULL;
    if (etgLOD != NULL)
    {
        if (target == NULL)
        {
            LOD = ship->obj.currentLOD;
        }
        else
        {
            LOD = min(ship->obj.currentLOD, target->obj.currentLOD);
        }
        if (soundType == GS_BigGun)
        {
            LOD = 0;                                        // CAT: a big-gun shell is always drawn at full detail
        }
        if (LOD < etgLOD->nLevels)
        {
            stat = etgLOD->level[LOD];
        }
    }
    if (((!univSpaceObjInRenderList(&ship->obj)) && (soundType != GS_BigGun)) || (stat == NULL) ||
        (!etgBulletEffectsEnabled) || etgFrequencyExceeded(stat))
    {
        bullet->effect = NULL;                              //play no effect for this bullet
    }
    else if (bullet->bulletType == WEAPON_ION)
    {
        // CAT: the ion beam's effect also gets the ship type and the barrel length
        bullet->effect = etgEffectCreate(stat, (Ship *)bullet, NULL, &ship->obj.posinfo.velocity, NULL, 1.0f, EAF_AllButNLips, 5,
                                         SCALECAST(intDamage), SCALECAST(intVelocity), SCALECAST(intLength),
                                         SCALECAST(ship->shiptype), SCALECAST(Real32ToUdword(gunstatic->barrelLength)));
    }
    else
    {
        bullet->effect = etgEffectCreate(stat, (Ship *)bullet, NULL, &ship->obj.posinfo.velocity, NULL, 1.0f, EAF_AllButNLips, 3,
                                         SCALECAST(intDamage), SCALECAST(intVelocity), SCALECAST(intLength));
    }

    //fire effect trigger and set collision event
    bullet->hitEffect = etgGunEventTable[shipstatic->shiprace][soundType][EGT_GunHit];
    etgLOD = etgGunEventTable[shipstatic->shiprace][soundType][EGT_GunFire];
    if ((etgLOD != NULL) && (ship->obj.currentLOD < etgLOD->nLevels))
    {
        stat = etgLOD->level[ship->obj.currentLOD];
    }
    else
    {
        stat = NULL;
    }
    if (univSpaceObjInRenderList(&ship->obj) && (stat != NULL) && etgFireEffectsEnabled && !etgFrequencyExceeded(stat))
    {                                                       //if there is a gun fire effect
        etgEffectCreate(stat, ship, &bullet->obj.posinfo.position, &ship->obj.posinfo.velocity, &bullet->rotatable.rotinfo.coordsys, 1.0f, EAF_Velocity, 1, SCALECAST(intDamage));
    }

    if (soundType == GS_BigGun)
    {
        pingAnomalyObjectPingAdd("BulletBigGun", (SpaceObj *)bullet);
    }
    bullet->catPrevPosition = bullet->obj.posinfo.position;

    // CAT: no HW1 recoil and no cloak-field decloak here (the exe has neither)
    if (target != NULL && target->header.objtype == OBJ_ShipType)
    {                                                       //if we succeeded in shooting at enemy
        ((Ship *)target)->firingAtUs = ship;
        ((Ship *)target)->recentlyFiredUpon = RECENT_ATTACK_DURATION;
    }
    return bullet;
}

/*-----------------------------------------------------------------------------
    Name        : gunShoot
    Description : shoots a given gun of the given ship
    Inputs      : ship  - that which owns gun
                  gun - gun to fire
                  target - object being shot at or NULL
    Outputs     :
    Return      : CAT: the bullet, or NULL (gunShoot 0x0048b5b0 returns cat_gunCreateBullet's result)
----------------------------------------------------------------------------*/
Bullet *gunShoot(Ship *ship, Gun *gun, SpaceObjRotImpTarg *target)
{
    GunStatic *gunstatic = gun->gunstatic;
    vector gunDirection, gunPosition;
    real32 damage;

    dbgAssertOrIgnore(gunstatic->guntype != GUN_MissileLauncher);

    // CAT-TODO(M2-R4 #4): a dRKO does not fire at the ship its RKO weapon holds (RKO specifics Ship+0x664); the RKO
    //      header is on DefaultShipHeader until ship code B ports it

    // CAT: a ship being handed to a salvage depot neither fires nor is fired at (Ship+0x3EC, catSalvageDepot)
    if ((ship != NULL) && (ship->header.objtype == OBJ_ShipType) && (ship->catSalvageDepot != NULL))
    {
        return NULL;
    }

    //line-of-sight check
    if (target != NULL)
    {
        if ((target->header.objtype == OBJ_ShipType) && (((Ship *)target)->catSalvageDepot != NULL))
        {
            return NULL;
        }
        // CAT: capital classes (Mothership..Frigate) check the line of sight, except for the big gun
        if ((ship->staticinfo->shipclass <= CLASS_Frigate) && (gunstatic->gunsoundtype != GS_BigGun))
        {
            matMultiplyMatByVec(&gunDirection, &ship->rotatable.rotinfo.coordsys, &gun->gunheading);
            matMultiplyMatByVec(&gunPosition, &ship->rotatable.rotinfo.coordsys, &gunstatic->position);
            vecAddTo(gunPosition, ship->obj.posinfo.position);
            if (!collCheckLineOfSight(ship, (Ship*)target, &gunPosition, &gunDirection))
            {
                return NULL;
            }
        }
    }

    //if this is the first gunshot, set the shipisattacking variable
    //for the ship and his wingman as well (if the wingman exists)
    if (!ship->shipisattacking)
    {
        ship->shipisattacking = TRUE;
        ship->catAttackStarted = TRUE;
    }
    if ((ship->attackvars.myWingmanIs)&&(!ship->attackvars.myWingmanIs->shipisattacking))
    {
        ship->attackvars.myWingmanIs->shipisattacking = TRUE;
        ship->catAttackStarted = TRUE;
    }
    if(ship->shipOrDerelict.specialFlags & SPECIAL_ShipInGravwell)   // CAT: specialFlags in the exe
    {
        //ship is in gravwell
        return NULL; //don't let it fire.
    }

    gun->lasttimefired = universe.totaltimeelapsed;

    // CAT: the damage is the ship's damage multiplier (experience, difficulty) times the gun's DamageMult; the weapon
    //      against the target's armour scales it on the hit (univBulletCollidedWithTarget). No HW1 health, formation,
    //      tactics or random damage.
    damage = cat_shipGetDamageMultiplier(ship) * gunstatic->damageMult;
    if (bitTest(ship->shipOrDerelict.specialFlags,SPECIAL_KasSpecialDamageModifier))
    {
        //ship is being 'beefed' up by KAS...add value to damagemultiplier
        damage *= ship->damageModifier;
    }

    // CAT: every shot at a Leech also does it 10 points of damage at once (0x0048b740)
    if ((target != NULL) && (target->header.objtype == OBJ_ShipType) && (((Ship *)target)->shiptype == sLeech))
    {
        ApplyDamageToTarget(target, 10.0f, gunstatic->gunsoundtype, 0x29a, ship->playerowner->playerIndex, ship, 0x12);
    }

    return cat_gunCreateBullet(ship, gun, NULL, target, damage);
}

/*-----------------------------------------------------------------------------
    Name        : gunShootGunsAtTarget
    Description : shoots a ship's guns at target
    Inputs      : ship, target, range, [trajectory] (trajectory required for ships with fixed guns)
    Outputs     :
    Return      :
    Note        : trajectory can be passed in as NULL for ships that don't have
                  any fixed guns.  trajectory is required only for ships that have
                  fixed guns.
----------------------------------------------------------------------------*/
bool32 gunShootGunsAtTarget(Ship *ship,SpaceObjRotImpTarg *target,real32 range,vector *trajectory)
{
    GunInfo *gunInfo = ship->gunInfo;
    sdword numGuns;
    Gun *gun;
    sdword i;
    vector shipheading;
    bool32 shotguns = FALSE;
    GunStatic *gunstatic;
    real32 dotprod;
    real32 triggerHappy;

    if (gunInfo == NULL)
    {
        return shotguns;
    }

    // CAT-TODO(stage 6): in Mission 15 no ship fires while the KAS variable G_StopShipsFiringOnMission15 is set
    //      (gunShootGunsAtTarget 0x0048c2e0: cat_kasHashString, cat_kasVarFind; the Cataclysm KAS variables come with
    //      the kas2c mission code)

    numGuns = gunInfo->numGuns;

    // CAT: gunShootGunsAtTarget 0x0048c2e0: no HW1 tactics range bonus; the big gun and the Beast weapon are not
    //      fired from here, nor any gun of a dissolving ship; a gun in its second fire mode keeps firetime2
    //      (cat_gunCanFire, inlined); a fixed gun aims with cat_gunFixedGunAimedAtTarget; an Acolyte or the derelict
    //      Acolyte does not launch missiles
    for (i=0;i<numGuns;i++)
    {
        gun = &gunInfo->guns[i];
        gunstatic = gun->gunstatic;
        if (bitTest(ship->header.flags, SOF_NISShip))
        {
            triggerHappy = NIS_TriggerHappyAngle;
        }
        else
        {
            triggerHappy = gunstatic->triggerHappy;
        }

        if ((ship->staticinfo->madStatic != NULL) && (ship->staticinfo->madStatic->numGunOpenIndexes > 0) &&
            (ship->madGunStatus != MAD_STATUS_GUNS_OPEN))
        {
            //has gun opening animations and the guns are not open
            if (ship->madGunStatus != MAD_STATUS_GUNS_OPENING)
            {
                madOpenGunsShip(ship);
            }
            return shotguns;
        }

        if ((range < gunstatic->bulletrange) && (gunstatic->gunsoundtype != GS_BeastWeapon) &&
            (gunstatic->gunsoundtype != GS_BigGun) && (ship->catDissolving != TRUE))
        {
            if (cat_gunCanFire(ship, gun))
            {
                switch (gunstatic->guntype)
                {
                    case GUN_MissileLauncher:
                        if (gunHasMissiles(gun))
                        {
                            matGetVectFromMatrixCol3(shipheading,ship->rotatable.rotinfo.coordsys);
                            dotprod = vecDotProduct(*trajectory,shipheading);

                            if ((dotprod >= triggerHappy) && !cat_shipIsAcolyteOrDerelict(ship))
                            {
                                missileShoot(ship,gun,target);
                                shotguns = TRUE;
                            }
                        }
                        break;
                    case GUN_MineLauncher:
                        if (gunHasMissiles(gun))
                        {
                            matGetVectFromMatrixCol3(shipheading,ship->rotatable.rotinfo.coordsys);
                            dotprod = vecDotProduct(*trajectory,shipheading);

                            if (dotprod >= triggerHappy)
                            {
                                missileShoot(ship,gun,target);
                                shotguns = TRUE;
                            }
                        }
                    break;
                    case GUN_Fixed:
                        if (cat_gunFixedGunAimedAtTarget(ship, gun, target, TRUE))
                        {
                            gunShoot(ship,gun,target);
                            shotguns = TRUE;
                        }
                        break;

                    case GUN_Gimble:
                    case GUN_NewGimble:
                        if (gunOrientGimbleGun(ship,gun,target))
                        {
                            gunShoot(ship,gun,target);
                            shotguns = TRUE;
                        }
                        break;

                    default:
                        break;
                }
            }
        }
    }
    return shotguns;
}

/*-----------------------------------------------------------------------------
    Name        : gunShootGunsAtMultipleTargets
    Description : shoots a ship's guns at multiple targets
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
bool32 gunShootGunsAtMultipleTargets(Ship *ship)
{
    GunInfo *gunInfo = ship->gunInfo;
    sdword numGuns = gunInfo->numGuns;
    Gun *gun;
    sdword i;
    bool32 shotguns = FALSE;
    GunStatic *gunstatic;
    AttackTargets *attackTargets = ship->attackvars.multipleAttackTargets;
    SpaceObjRotImpTarg *target;
    bool32 aimed;

    dbgAssertOrIgnore(attackTargets->numAttackTargets == numGuns);      // one attack target for each gun

    // CAT: gunShootGunsAtMultipleTargets 0x0048c690: the big gun and the Beast weapon are skipped; a dissolving ship
    //      only keeps its slave guns with their drivers; cat_gunCanFire's second-mode rules (inlined); a fixed gun aims
    //      with cat_gunFixedGunAimedAtTarget; missiles and mines need no heading check
    for (i=0;i<numGuns;i++)
    {
        gun = &gunInfo->guns[i];
        gunstatic = gun->gunstatic;
        if ((gunstatic->gunsoundtype == GS_BeastWeapon) || (gunstatic->gunsoundtype == GS_BigGun))
        {
            continue;
        }
        if ((ship->staticinfo->madStatic != NULL) && (ship->staticinfo->madStatic->numGunOpenIndexes > 0) &&
            (ship->madGunStatus != MAD_STATUS_GUNS_OPEN))
        {
            //has gun opening animations and the guns are not open
            if (ship->madGunStatus != MAD_STATUS_GUNS_OPENING)
            {
                madOpenGunsShip(ship);
            }
            return shotguns;
        }

        if ((ship->catDissolving != TRUE) && cat_gunCanFire(ship, gun))
        {
            target = attackTargets->TargetPtr[i];

            if (target != NULL)
            {
                switch (gunstatic->guntype)
                {
                    case GUN_MissileLauncher:
                    case GUN_MineLauncher:
                        if (gunHasMissiles(gun))
                        {
                            missileShoot(ship,gun,target);
                            shotguns = TRUE;
                        }
                        break;
                    case GUN_Fixed:
                    case GUN_Gimble:
                    case GUN_NewGimble:
                        if (gunstatic->guntype == GUN_Fixed)
                        {
                            aimed = cat_gunFixedGunAimedAtTarget(ship, gun, target, TRUE);
                        }
                        else
                        {
                            aimed = gunOrientGimbleGun(ship,gun,target);
                        }
                        if (aimed)
                        {
                            gunShoot(ship,gun,target);
                            shotguns = TRUE;
                        }
                        break;

                    default:
                        break;
                }
            }
        }
        else if (gunstatic->slaveDriver >= 0)
        {
            gunUpdateSlave(gun, gunInfo, gunstatic->slaveDriver);
        }
    }
    return shotguns;
}

/*-----------------------------------------------------------------------------
    Name        : gunMatrixUpdate
    Description : Binding matrix callback for updating gun mesh positions.
    Inputs      : flags - flags indicating certain attributes (see mesh.h)
                  startMatrix - starting matrix (from .geo file)
                  matrix - matrix to store result in.
                  data - pointer to Gun structure
                  ID - pointer to ship structure
    Outputs     : computes and fills in the gun matrix.
    Return      : true = matrix updated
----------------------------------------------------------------------------*/
bool32 gunMatrixUpdate(udword flags, hmatrix *startMatrix, hmatrix *matrix, void *data, smemsize ID)
{
    Ship *ship;
    Gun *gun;
    GunStatic *gunstatic;
    bool32 bRecoil;
    vector recoilVector;

    ship = (Ship *)ID;
    dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
    gun = (Gun *)data;
    gunstatic = gun->gunstatic;

    bRecoil = bitTest(flags, HBF_Recoil);
    flags &= (HBF_Declination|HBF_Azimuth);

    switch (gunstatic->guntype)
    {
        case GUN_NewGimble:
            if (flags == (HBF_Declination|HBF_Azimuth))
            {
                if (bitTest(gunstatic->flags, GF_MultiLevelMatrix))
                {
                    hmatMakeHMatFromMat(matrix,&gun->curGunOrientationNonConcat);
                }
                else
                {
                    hmatMakeHMatFromMat(matrix,&gun->curgunorientation);
                }
                matrix->m14 = startMatrix->m14;
                matrix->m24 = startMatrix->m24;
                matrix->m34 = startMatrix->m34;
                break;
            }
            else if (flags == HBF_Azimuth)
            {
                hmatrix rotmatrix;
                real32 angle = gun->angle;
                hmatMakeRotAboutX(&rotmatrix,(real32)cos(angle),(real32)sin(angle));
                hmatMultiplyHMatByHMat(matrix, startMatrix, &rotmatrix);
                break;
            }
            else if (flags == HBF_Declination)
            {
                hmatrix rotmatrix;
                real32 declination = gun->declination;
                hmatMakeRotAboutY(&rotmatrix,(real32)cos(declination),(real32)sin(declination));
                hmatMultiplyHMatByHMat(matrix, startMatrix, &rotmatrix);
                break;
            }
            else
            {
//                dbgAssertOrIgnore(FALSE);
//                return FALSE;
                *matrix = IdentityHMatrix;
                matrix->m14 = startMatrix->m14;
                matrix->m24 = startMatrix->m24;
                matrix->m34 = startMatrix->m34;
                break;
            }

        default:
            dbgAssertOrIgnore(FALSE);
            return FALSE;
    }
    //add recoil, if any
    if (bRecoil)
    {
        hmatGetVectFromHMatrixCol3(recoilVector, *matrix);
        gunRecoilVectorCompute(&recoilVector, gunstatic->recoilLength, gun->lasttimefired, gunstatic->firetime);
//        vecAddTo(bullet->obj.posinfo.position, offset0);
        matrix->m14 += recoilVector.x;
        matrix->m24 += recoilVector.y;
        matrix->m34 += recoilVector.z;
    }


    return(TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : gunDrawGunInfo
    Description : draws gun debug information
    Inputs      : ship
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
#if RND_VISUALIZATION
void gunDrawGunInfo(Ship *ship)
{
    sdword i;
    GunInfo *gunInfo = ship->gunInfo;
    sdword numGuns;
    Gun *gun;
    GunStatic *gunstatic;
    vector to, from, offset0;

    if (gunInfo == NULL)
    {
        return;
    }

    numGuns = gunInfo->numGuns;

    rndLightingEnable(FALSE);
    rndTextureEnable(FALSE);

    for (i=0,gun=&gunInfo->guns[0];i<numGuns;i++,gun++)
    {
        gunstatic = gun->gunstatic;
        from = gun->gunstatic->position;
        vecScalarMultiply(to, gun->gunheading,500.0f);
        vecAddTo(to, from);
        // heading

        //gun offset
        if (gunstatic->offset.x != 0.0f)
        {                                                       //if there is an offset vector
            matMultiplyMatByVec(&offset0, &gun->curgunorientation, &gunstatic->offset);//offset in gun space
            vecAddTo(from, offset0);                            //add to position
            vecAddTo(to, offset0);                            //add to position
        }
        //add barrel length
        from.x += gun->gunheading.x * gunstatic->barrelLength;
        from.y += gun->gunheading.y * gunstatic->barrelLength;
        from.z += gun->gunheading.z * gunstatic->barrelLength;
        to.x += gun->gunheading.x * gunstatic->barrelLength;
        to.y += gun->gunheading.y * gunstatic->barrelLength;
        to.z += gun->gunheading.z * gunstatic->barrelLength;
        primLine3(&from, &to, colFuscia/*colRGB(10, 255, 100)*/);
    }

    rndLightingEnable(TRUE);
}
#endif

/*-----------------------------------------------------------------------------
    Name        : gunShipFirePower
    Description : the firepower of a ship type: each gun's base damage per second (cat_gunStaticBaseDamage over its
                  firetime, 1/16 s when it has none); the three interceptor types share their custom firetime between
                  their guns
    Inputs      : info
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
// CAT: gunShipFirePower 0x0048cb60 takes the static info only (no tactics) and measures damage with the weapon tables;
//      HW1's gunFirePower (damage lo/hi per tactics, the beam and burst factors) has no counterpart in the exe. The
//      interceptor list is the exe's 3-byte table 0x00881534.
#define GUN_NumberInterceptorTypes      3
static const ubyte gunInterceptorTypes[GUN_NumberInterceptorTypes] = { kHeavyInterceptor, tHeavyInterceptor, p1Fighter };
real32 gunShipFirePower(ShipStaticInfo *info)
{
    sdword index;
    real32 power = 0.0f, fireTime, totalFireTime;
    GunStatic *gunstatic;

    if (info->gunStaticInfo == NULL)
    {
        return power;
    }
    if (memchr(gunInterceptorTypes, info->shiptype, GUN_NumberInterceptorTypes))
    {                                                       //interceptors have a different rate of fire computation
        totalFireTime = ((GenericInterceptorStatics *)info->custstatinfo)->firetime;
        for (index = 0; index < info->gunStaticInfo->numGuns; index++)
        {
            gunstatic = &info->gunStaticInfo->gunstatics[index];
            fireTime = (gunstatic->firetime == 0.0f) ? 0.0625f : gunstatic->firetime;
            power = (fireTime / ((real32)info->gunStaticInfo->numGuns * totalFireTime)) *
                    (cat_gunStaticBaseDamage(gunstatic) / fireTime) + power;
        }
    }
    else
    {
        for (index = 0; index < info->gunStaticInfo->numGuns; index++)
        {
            gunstatic = &info->gunStaticInfo->gunstatics[index];
            fireTime = (gunstatic->firetime == 0.0f) ? 0.0625f : gunstatic->firetime;
            power = cat_gunStaticBaseDamage(gunstatic) / fireTime + power;
        }
    }
    return(power);
}

/*-----------------------------------------------------------------------------
    Name        : gunResetGimbleGun
    Description : Reset desired gun to initial position
    Inputs      : gun which should reset it position, reset speed factor
    Outputs     :
    Return      : TRUE = gun on initial position
----------------------------------------------------------------------------*/
bool32 gunResetGimbleGun(Gun *gun, float angleSpeedFactor)
{
    bool32 updateguncoordsys = FALSE;
    GunStatic *gunstatic = gun->gunstatic;
    real32 maxAngleSpeed = gunstatic->maxanglespeed * universe.phystimeelapsed * angleSpeedFactor;
    real32 maxDeclinationSpeed = gunstatic->maxdeclinationspeed * universe.phystimeelapsed * angleSpeedFactor;

    if (gunstatic->guntype == GUN_NewGimble)
    {
        if (gun->angle > 0.0f)
        {
            gun->angle -= maxAngleSpeed;
            if (gun->angle < 0.0f)
            {
                gun->angle = 0.0f;
            }
            updateguncoordsys = TRUE;
        }
        else if (gun->angle < 0.0f)
        {
            gun->angle += maxAngleSpeed;
            if (gun->angle > 0.0f)
            {
                gun->angle = 0.0f;
            }
            updateguncoordsys = TRUE;
        }

        if (gun->declination > 0.0f)
        {
            gun->declination -= maxDeclinationSpeed;
            if (gun->declination < 0.0f)
            {
                gun->declination = 0.0f;
            }
            updateguncoordsys = TRUE;
        }
        else if (gun->declination < 0.0f)
        {
            gun->declination += maxDeclinationSpeed;
            if (gun->declination > 0.0f)
            {
                gun->declination = 0.0f;
            }
            updateguncoordsys = TRUE;
        }
        if (updateguncoordsys)
        {
            gunNewGimbleUpdateCoordSys(gun, gunstatic);
        }

        return (gun->angle == 0.0f) && (gun->declination == 0.0f);
    }
    else
    {
        return TRUE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : gunResetGimbleGuns
    Description : Reset all guns of the ship to initial position
    Inputs      : ship which should reset its guns
    Outputs     :
    Return      : TRUE = all guns on initial position
----------------------------------------------------------------------------*/
bool32 gunResetGimbleGuns(Ship *ship)
{
    bool32 oriented = TRUE;
    GunInfo *gunInfo = ship->gunInfo;
    sdword numGuns;
    sdword i;
    Gun *gun;

    if (gunInfo == NULL)
    {
        return oriented;
    }

    for (i = 0; i < gunInfo->numGuns; i++)
    {
        gun = &gunInfo->guns[i];
        if (gunResetGimbleGun(gun, GUN_ResetAngleSpeedFactor))
        {
            soundEventStop(gun->gimblehandle);
            gun->gimblehandle = -1;
        }
        else
        {
            if (gun->gimblehandle < 0)
            {
                gun->gimblehandle = soundEventPlay(ship, Gun_WeaponMove, gun);
            }
            oriented = FALSE;
        }
    }

    return oriented;
}

/*-----------------------------------------------------------------------------
    Name        : gunStartup
    Description : Start the gun module
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void gunStartup(void)
{
    gunRecoilTableCompute();
    vanillaGunsMode = vanillaMode;
}

/*-----------------------------------------------------------------------------
    Name        : gunShutdown
    Description : Shut down the gun module
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void gunShutdown(void)
{
    ;
}

#if GUN_TUNE_MODE

bool32 gunTuningMode = FALSE;
sdword tuningGun = 0;

void gunTuneGun(Ship *ship)
{
    GunInfo *gunInfo = ship->gunInfo;
    Gun *gun;
    GunStatic *gunstatic;
    bool32 updateguncoordsys = FALSE;

    if (gunInfo == NULL)
    {
        return;
    }

    if (gunInfo->numGuns == 0)
    {
        return;
    }

    if (tuningGun >= gunInfo->numGuns)
    {
        tuningGun = 0;
    }

    gun = &gunInfo->guns[tuningGun];
    gunstatic = gun->gunstatic;

    switch (gunstatic->guntype)
    {
        case GUN_NewGimble:
            if (keyIsHit(ARRLEFT))
            {
                gun->angle -= GUNTUNE_SPEED;
                updateguncoordsys = TRUE;
            }

            if (keyIsHit(ARRRIGHT))
            {
                gun->angle += GUNTUNE_SPEED;
                updateguncoordsys = TRUE;
            }

            if (keyIsHit(ARRUP))
            {
                gun->declination += GUNTUNE_SPEED;
                updateguncoordsys = TRUE;
            }

            if (keyIsHit(ARRDOWN))
            {
                gun->declination -= GUNTUNE_SPEED;
                updateguncoordsys = TRUE;
            }
            if (keyIsHit(NUMPAD0))
            {
                gun->angle = 0.0f;
                gun->declination = 0.0f;
                updateguncoordsys = TRUE;
            }

            if (updateguncoordsys)
            {
                if (gun->angle < gunstatic->minturnangle)
                {
                    gun->angle = gunstatic->minturnangle;
                }

                if (gun->angle > gunstatic->maxturnangle)
                {
                    gun->angle = gunstatic->maxturnangle;
                }

                if (gun->declination < gunstatic->mindeclination)
                {
                    gun->declination = gunstatic->mindeclination;
                }

                if (gun->declination > gunstatic->maxdeclination)
                {
                    gun->declination = gunstatic->maxdeclination;
                }

                gunNewGimbleUpdateCoordSys(gun,gunstatic);
            }

            {
                char buffer[300];
                sprintf(buffer,"Gun %d %s Angle:%4.2f Declin:%4.2f Heading:%2.3f %2.3f %2.3f %d",tuningGun,GunTypeToStr(gunstatic->guntype),
                           RAD_TO_DEG(gun->angle),RAD_TO_DEG(gun->declination),gun->gunheading.x,gun->gunheading.y,gun->gunheading.z,universe.univUpdateCounter);

                strcat(buffer,"\n");
                fontPrint(0, 400, colWhite, buffer);
                //dbgMessage(buffer);
            }
            break;

        default:
            fontPrintf(0, 400, colWhite,
                       "Gun %d %s",tuningGun,GunTypeToStr(gunstatic->guntype));
            break;
    }
}
#endif

