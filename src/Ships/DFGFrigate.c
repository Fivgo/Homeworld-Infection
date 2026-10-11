// =============================================================================
//  DFGFrigate.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 6/30/1997 by gshaw
// =============================================================================

#include "DFGFrigate.h"

#include <math.h>

#include "Attack.h"
#include "CommandDefs.h"
#include "Collision.h"
#include "CommandLayer.h"
#include "ETG.h"
#include "FastMath.h"
#include "glinc.h"
#include "Mothership.h"
#include "Ping.h"
#include "Randy.h"
#include "RKO.h"
#include "Sentinel.h"
#include "StatScript.h"
#include "UnivUpdate.h"
#include "Universe.h"

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

// CAT (M2-R4 ships-b): is pos inside the field of any DFG frigate (cat_dfgFieldContainsPoint 0x005b0df0: the field
//      radius squared of the statics, around the ship's position)
bool32 cat_dfgFieldContainsPoint(vector *pos)
{
    Node *node;
    Ship *ship;
    bool32 inside = FALSE;

    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        ship = (Ship *)listGetStructOfNode(node);
        if (((ship->shiptype == bDFGFrigate) || (ship->shiptype == tDFGFrigate)) &&
            cat_vecWithinDistanceSq(pos, &ship->obj.posinfo.position,
                                    ((DFGFrigateStatics *)ship->staticinfo->custstatinfo)->DFGFrigateFieldRadiusSqr))
        {
            inside = TRUE;
        }
    }
    return inside;
}

bool32 univDFGFieldEffect(Ship *ship, Bullet *bullet,real32 totaltimeelapsed)   // CAT: returns TRUE when it destroyed the bullet
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
        return FALSE;
    }
    if(bullet->SpecialEffectFlag & 0x0001)
    {
        //bullet has been effected by field so don't touch it ever again!
        return FALSE;
    }

    // CAT: the field detonates a Sect big-gun shell outright (univDFGFieldEffect 0x005b0940: cat_isBigGunBullet ->
    //      cat_bigGunBulletExplode, then the shell, its effect and its ping are deleted; 0x005b0991 .. 0x005b09ee)
    //      [M2-R4 ships-b]
    if (cat_isBigGunBullet((SpaceObj *)bullet))
    {
        Effect *effect = bullet->effect;

        cat_bigGunBulletExplode(bullet);
        if (effect != NULL)
        {
            etgEffectDelete(effect);
            univRemoveObjFromRenderList(&effect->obj);
            listDeleteNode(&effect->header.objlink);
        }
        pingRemovePingByOwner((SpaceObj *)bullet);
        univFreeBulletContents(bullet);
        bobObjectDied((SpaceObj *)bullet, &universe.collBlobList);
        listRemoveNode(&bullet->bulletlink);
        univRemoveObjFromRenderList((SpaceObj *)bullet);
        listDeleteNode(&bullet->header.objlink);
        return TRUE;
    }

    bullet->SpecialEffectFlag |= 0x0001;

    randomnum = randombetween(0,100);
    if(randomnum < DFGFstatics->percentageThatGetThrough)
    {
        //bullet is fortunate enough to not be deflected
        return FALSE;
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
            // CAT: the exe scales the damage by cat_weaponDamageVsArmour(the bullet's damage type, 0) 0x00514460 (and by
            //      etgSoftwareScalarHit in software rendering, which this tree does not have) before the magnitude
            floatDamage = cat_weaponDamageVsArmour(bullet->soundType, 0) * (real32)bullet->damage;
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
    return FALSE;
}

void DFGFrigateAttack(Ship *ship,SpaceObjRotImpTarg *target,real32 maxdist)
{
    DFGFrigateStatics *DFGFstatics = (DFGFrigateStatics *) ((ShipStaticInfo *)(ship->staticinfo))->custstatinfo;

    attackStraightForward(ship,target,DFGFstatics->DFGFrigateFieldRadius,1000.0f);
}


void DFGFrigateHouseKeep(Ship *ship)
{
    // CAT: new slot-10 body (DFGFrigateHouseKeep 0x005b0cf0). On 3 of every 4 universe updates
    //      ((univUpdateCounter & 0xC) != 0) the DFG field disrupts every enemy Sentinel field formation (formation
    //      type 9 or 10) whose lead ship is within its field radius: each ship of it is disrupted for 2 s (Sentinel
    //      specifics, exe Ship+0x97C / +0x980; Sentinel.c since M2-R4 ships-b)
    DFGFrigateStatics *dfgstat = (DFGFrigateStatics *)ship->staticinfo->custstatinfo;
    CommandToDo *command;
    SelectCommand *selection;
    Ship *lead;
    Node *node;
    vector diff;
    sdword i;

    if ((universe.univUpdateCounter & 0xc) == 0)
    {
        return;
    }
    for (node = universe.mainCommandLayer.todolist.head; node != NULL; node = node->next)
    {
        command = (CommandToDo *)listGetStructOfNode(node);
        if (!(command->ordertype.attributes & COMMAND_MASK_FORMATION) ||
            ((command->formation.formationtype != FORCEFIELD_WALL_FORMATION) &&
             (command->formation.formationtype != FORCEFIELD_SPHERE_FORMATION)))
        {
            continue;
        }
        selection = command->selection;
        if (selection->numShips <= 0)
        {
            continue;
        }
        lead = selection->ShipPtr[0];
        diff.x = lead->obj.posinfo.position.x - ship->obj.posinfo.position.x;
        diff.y = lead->obj.posinfo.position.y - ship->obj.posinfo.position.y;
        diff.z = lead->obj.posinfo.position.z - ship->obj.posinfo.position.z;
        if ((lead->playerowner != ship->playerowner) &&
            (diff.x * diff.x + diff.y * diff.y + diff.z * diff.z < dfgstat->DFGFrigateFieldRadiusSqr))
        {
            for (i = 0; i < selection->numShips; i++)
            {
                ((SentinelSpec *)selection->ShipPtr[i]->ShipSpecifics)->disrupted = TRUE;
                ((SentinelSpec *)command->selection->ShipPtr[i]->ShipSpecifics)->disruptEndTime =
                    universe.totaltimeelapsed + 2.0f;   // 0x0072a6c0
            }
        }
    }
}

// CAT: the exe's header (DFGFrigateHeader 0x008c3ff8: bDFGFrigate, tDFGFrigate; original/scratch-m2/m2r2-shipstatic-tools/headers.txt).
CustShipHeader DFGFrigateHeader =
{
    .sizeofShipSpecifics = sizeof(DFGFrigateSpec),
    .CustShipStaticInit = DFGFrigateStaticInit,
    .CustShipHousekeep = DFGFrigateHouseKeep,                // CAT: new slot 10
    .CustShipAttack = DFGFrigateAttack,
};

