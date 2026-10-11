// =============================================================================
//  DFGFrigate.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "DFGFrigate.h"

#include <math.h>

#include "Attack.h"
#include "FastMath.h"
#include "glinc.h"
#include "Mothership.h"
#include "Randy.h"
#include "StatScript.h"
#include "UnivUpdate.h"

typedef struct
{
    udword dummy;
} DFGFrigateSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(DFGFrigateSpec) == 4, "DFGFrigateSpec is 4 bytes in cataclysm.exe (DFGFrigateHeader sizeofShipSpecifics)");   // CAT
#endif


DFGFrigateStatics DFGFrigateStatic;

// CAT: one block for both bDFGFrigate and tDFGFrigate (DFGFrigateStaticInit 0x005b08f0 always uses 0x00aadcc0);
//      HW1 had R1/R2
DFGFrigateStatics DFGFrigateStaticCat;

scriptStructEntry DFGStaticScriptTable[] =
{
    { "fieldRadius",    scriptSetReal32CB,  &(DFGFrigateStatic.DFGFrigateFieldRadius),  &(DFGFrigateStatic) },
    { "BulletDamageMultiplier",    scriptSetReal32CB,  &(DFGFrigateStatic.BulletDamageMultiplier),  &(DFGFrigateStatic) },
    { "BulletLifeExtension",    scriptSetReal32CB,  &(DFGFrigateStatic.BulletLifeExtension),  &(DFGFrigateStatic) },
    { "percentageThatGetThrough",    scriptSetUdwordCB,  &(DFGFrigateStatic.percentageThatGetThrough),  &(DFGFrigateStatic) },
    { "angleVariance",    scriptSetUdwordCB,  &(DFGFrigateStatic.angleVariance),  &(DFGFrigateStatic) },

    END_SCRIPT_STRUCT_ENTRY
};


void DFGFrigateStaticInit(char *directory,char *filename,struct ShipStaticInfo *statinfo)
{
    DFGFrigateStatics *dfgstat = &DFGFrigateStaticCat;    // CAT: one block (0x005b08f0)

    statinfo->custstatinfo = dfgstat;

    scriptSetStruct(directory,filename,DFGStaticScriptTable,(ubyte *)dfgstat);
    dfgstat->DFGFrigateFieldRadiusSqr = dfgstat->DFGFrigateFieldRadius*dfgstat->DFGFrigateFieldRadius;
}

void univDFGFieldEffect(Ship *ship, Bullet *bullet,real32 totaltimeelapsed)
{
    vector  dir1,shiptobullet;
    vector  newbulletdir;
    sdword  randomnum;
    real32 bulletspeed;
    real32 theta;
    matrix rotmatrix,newmatrix,tmpmatrix;
    sdword LOD;
    etglod *etgLOD;
    etgeffectstatic *stat;
    udword intDamage;
    real32 floatDamage;

    DFGFrigateStatics *DFGFstatics;
    DFGFstatics = (DFGFrigateStatics *) ((ShipStaticInfo *)(ship->staticinfo))->custstatinfo;

    if(bullet->playerowner == ship->playerowner)
    {
        //don't affect players bullets
        return;
    }
    if(bullet->SpecialEffectFlag & 0x0001)
    {
        //bullet has been effected by field so don't touch it ever again!
        return;
    }

    // CAT: the field detonates a Sect big-gun shell outright (univDFGFieldEffect 0x005b0940: cat_isBigGunBullet ->
    //      cat_bigGunBulletExplode, then the shell, its effect and its ping are deleted).
    //      CAT-PROVISIONAL: no bullet carries the WEAPON_BIGGUN type before the bullet/big-gun ports (M2-R4 #3/#4),
    //      so cat_isBigGunBullet is FALSE and the detonation is not written yet
    if (cat_isBigGunBullet((SpaceObj *)bullet))
    {
        return;
    }

    bullet->SpecialEffectFlag |= 0x0001;

    randomnum = randombetween(0,100);
    if(randomnum < DFGFstatics->percentageThatGetThrough)
    {
        //bullet is fortunate enough to not be deflected
        return;
    }

    bulletspeed = fsqrt(vecMagnitudeSquared(bullet->obj.posinfo.velocity));

    //coorsys for deflection effect
    vecSub(shiptobullet,bullet->obj.posinfo.position,ship->obj.posinfo.position);

    vecNormalize(&shiptobullet);
    matCreateCoordSysFromHeading(&tmpmatrix,&shiptobullet);

    //create an effect where the collision occurred
//    etgLOD = etgGunEventTable[ship->staticinfo->shiprace][bullet->soundType][EGT_BulletDeflected];//get pointer to bullet effect
    etgLOD = etgSpecialPurposeEffectTable[EGT_BULLET_DEFLECT];
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
        if (stat != NULL && etgHitEffectsEnabled && etgEffectsEnabled && !etgFrequencyExceeded(stat))
#else
        if (stat != NULL && etgHitEffectsEnabled && !etgFrequencyExceeded(stat))
#endif
        {                                                   //if there is a deflection effect
//            matCreateCoordSysFromHeading(&newCoordsys,&shiptobullet);
            // CAT-PROVISIONAL: the exe scales the damage by cat_weaponDamageVsArmour(bullet weapon type, 0)
            //      0x00514460 (and by etgSoftwareScalarHit in software rendering) before the magnitude; the
            //      weapon-vs-armour table and the bullet's weapon type are M2-R4 #3, so the factor is 1 here
            floatDamage = (real32)bullet->damage;
            floatDamage *= ship->magnitudeSquared;
            intDamage = Real32ToUdword(floatDamage);
            etgEffectCreate(stat, ship, &bullet->obj.posinfo.position, NULL, &tmpmatrix, ship->magnitudeSquared, EAF_Velocity | EAF_NLips, 1, intDamage);
        }
    }

    theta = DEG_TO_RAD(randombetween(0,DFGFstatics->angleVariance));
    // CAT: the exe flips on an odd random number (ranRandom(10) & 1); HW1's randombetween(1,2) & 1 flipped on an even
    //      one (0x005b0940; the same for the direction flip below)
    if(gamerand() & 1)
    {
        theta *= -1.0f;             //could optimize by using a bit toggle
    }

    matMakeRotAboutZ(&rotmatrix,(real32) cos(theta),(real32) sin(theta));
    matMultiplyMatByMat(&newmatrix,&rotmatrix,&tmpmatrix);

    matGetVectFromMatrixCol1(dir1,newmatrix);

    if(gamerand() & 1)
    {
        vecScalarMultiply(dir1,dir1,-1.0f);
    }

    newbulletdir = dir1;

    vecNormalize(&newbulletdir);     //maybe don't need

    matCreateCoordSysFromHeading(&bullet->rotatable.rotinfo.coordsys,&newbulletdir);
    vecScalarMultiply(bullet->obj.posinfo.velocity,newbulletdir,bulletspeed);
    vecScalarMultiply(bullet->lengthvec,newbulletdir,bullet->lengthmag);
    bullet->bulletheading = newbulletdir;

    // CAT: no BulletDamageMultiplier any more (the key is still read); the life extension stays (0x005b0940)
    bullet->totallifetime += DFGFstatics->BulletLifeExtension;
}

void DFGFrigateAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    DFGFrigateStatics *DFGFstatics = (DFGFrigateStatics *) ((ShipStaticInfo *)(ship->staticinfo))->custstatinfo;

    attackStraightForward(ship,target,DFGFstatics->DFGFrigateFieldRadius,1000.0f);
}


void DFGFrigateHouseKeep(Ship *ship)
{
    // CAT: new slot-10 body (DFGFrigateHouseKeep 0x005b0cf0). On 3 of every 4 universe updates
    //      ((univUpdateCounter & 0xC) != 0) the exe walks the command layer for formation commands of type 9 or 10
    //      (the Sentinel field formations) whose lead ship belongs to another player and lies within the DFG's
    //      fieldRadius (statics +0x04, squared), and marks every ship of such a command (Sentinel specifics, exe
    //      Ship+0x97C = 1, Ship+0x980 = now + 0x0072a6c0): the DFG field disrupts enemy Sentinel fields.
    //      CAT-PROVISIONAL: the Sentinel specifics and its field formations are M2-R4 #4 (ship code B), so nothing
    //      is marked until then.
    (void)ship;
}

// CAT: the exe's header (DFGFrigateHeader 0x008c3ff8: bDFGFrigate, tDFGFrigate; original/scratch-m2/m2r2-shipstatic-tools/headers.txt).
CustShipHeader DFGFrigateHeader =
{
    .sizeofShipSpecifics = sizeof(DFGFrigateSpec),
    .CustShipStaticInit = DFGFrigateStaticInit,
    .CustShipHousekeep = DFGFrigateHouseKeep,                // CAT: new slot 10
    .CustShipAttack = DFGFrigateAttack,
};

