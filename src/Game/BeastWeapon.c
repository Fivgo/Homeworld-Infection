// =============================================================================
//  BeastWeapon.c
//  - Cataclysm's Beast infection weapon of bHeavyCruiser, bCarrier,
//    bMothership and dBeastMothership (cataclysm.exe 0x004298f0-0x0042b650)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R3 ships-a); see BeastWeapon.h for what is ported now and what is M2-R4 #3's.

#include "BeastWeapon.h"
#include "KASVar.h"
#include "SinglePlayer.h"
#include "KASFunc.h"
#include "KAS.h"

#include "AIPlayer.h"
#include "AIShip.h"
#include "Collision.h"
#include "CommandDefs.h"
#include "CommandLayer.h"
#include "Debug.h"
#include "ETG.h"
#include "Gun.h"
#include "Memory.h"
#include "MeshAnim.h"
#include "Randy.h"
#include "SoundEvent.h"
#include "SpeechEvent.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "SaveGame.h"
#include "ShipSelect.h"
#include "StatScript.h"

CatBeastWeaponTweaks cat_beastWeaponTweaks;                                 // 0x00c52140

scriptStructEntry cat_beastWeaponScriptTable[] =                            // 0x00751698
{
    { "beastAttackCascadePoints",         scriptSetReal32CB, &(cat_beastWeaponTweaks.beastAttackCascadePoints),         &(cat_beastWeaponTweaks) },
    { "beastAttackCascadeDelay",          scriptSetReal32CB, &(cat_beastWeaponTweaks.beastAttackCascadeDelay),          &(cat_beastWeaponTweaks) },
    { "beastAttackCascadeDistance",       scriptSetReal32CB, &(cat_beastWeaponTweaks.beastAttackCascadeDistance),       &(cat_beastWeaponTweaks) },
    { "beastAttackTimeSpentStunned",      scriptSetReal32CB, &(cat_beastWeaponTweaks.beastAttackTimeSpentStunned),      &(cat_beastWeaponTweaks) },
    { "beastAttackTimeToCompleteAttacks", scriptSetReal32CB, &(cat_beastWeaponTweaks.beastAttackTimeToCompleteAttacks), &(cat_beastWeaponTweaks) },
    { "stunnedSpinRate",                  scriptSetReal32CB, &(cat_beastWeaponTweaks.stunnedSpinRate),                  &(cat_beastWeaponTweaks) },

    END_SCRIPT_STRUCT_ENTRY
};

static bool32 cat_isBeastWeaponType(ShipType type)
{
    // bHeavyCruiser 0x2E, bCarrier 0x2F, bMothership 0x35, dBeastMothership 0x82 (the switch of
    // cat_staticHasBeastWeapon 0x00429cf0 / cat_shipHasBeastWeapon 0x00429bc0)
    switch (type)
    {
        case bHeavyCruiser:
        case bCarrier:
        case bMothership:
        case dBeastMothership:
            return TRUE;
        default:
            return FALSE;
    }
}

static CatBeastWeaponSpec *cat_beastWeaponGetSpec(Ship *ship)
{
    // the exe uses Ship+0x638 (the start of the specifics) for the four types and address 0 otherwise
    // (cat_beastWeaponInit 0x0042a040); only callers that checked cat_shipHasBeastWeapon reach here
    dbgAssertOrIgnore(cat_isBeastWeaponType(ship->shiptype));
    return (CatBeastWeaponSpec *)ship->ShipSpecifics;
}

bool32 cat_staticHasBeastWeapon(ShipStaticInfo *statinfo)                  // 0x00429cf0
{
    return cat_isBeastWeaponType(statinfo->shiptype);
}

bool32 cat_shipHasBeastWeapon(Ship *ship)                                   // 0x00429bc0
{
    // the exe tests the type of the ship's static info (Ship+0x20 -> +0x1E0)
    return cat_isBeastWeaponType(ship->staticinfo->shiptype);
}

// The Beast's infection takes hold of target: it is stunned and spun, disabled, dropped by the AI and, when
// cat_univBeastInfectionUpdate's time comes, converted for the infector's owner
void cat_beastInfectShip(Ship *target, Ship *infector)                      // 0x004298f0
{
    Player *owner = infector->playerowner;
    ShipStaticInfo *statinfo = target->staticinfo;
    real32 spin = cat_beastWeaponTweaks.stunnedSpinRate;
    real32 stunned;
    ShipType convertTo;

    bitSet(target->header.flags, SOF_Crazy|SOF_Disabled);
    // a random spin direction (catFlags18 bit 7)
    if ((ranRandom(RANDOM_CAT_SHIPS) % 100) > 50)
    {
        bitSet(target->header.catFlags18, 0x80);
    }
    else
    {
        bitClear(target->header.catFlags18, 0x80);
    }
    // the exe divides by the one-over-moment fields (StaticInfo +0x1C/+0x20/+0x24)
    target->rotatable.rotinfo.torque.x += (spin * 0.5f) / statinfo->common.oneOverMomentOfInertiaX;
    target->rotatable.rotinfo.torque.y += (spin * 0.33333334f) / statinfo->common.oneOverMomentOfInertiaY;
    target->rotatable.rotinfo.torque.z += (spin * 0.25f) / statinfo->common.oneOverMomentOfInertiaZ;

    stunned = cat_beastWeaponTweaks.beastAttackTimeSpentStunned;
    if (target->shiptype == dEcclesiam)
    {
        stunned = stunned + stunned;
    }
    target->catBeastInfector = infector;
    target->catBeastInfectorOwner = owner;
    target->catBeastConvertTime = universe.totaltimeelapsed + stunned;
    target->catBeastStunStart = universe.totaltimeelapsed + 1.0f;
    target->catBeastStunState = 2;
    target->catBeastStunEnd = target->catBeastStunStart + stunned;

    // the bindings of the Beast type it will become; the exe reads the statics block unchecked (every infectable
    // type has beastAttacksRequired >= 0)
    convertTo = statinfo->beastAttackConvertTo;
    target->catBeastBindings = cat_univMeshBindingsDupeNoGuns(GetShipStaticInfo(convertTo, RACE_Beast));
    if (!singlePlayerGame)
    {
        target->catBeastColor = (owner->PlayerMothership != NULL) ? (ubyte)owner->PlayerMothership->colorScheme : 0;
    }
    else
    {
        target->catBeastColor = 1;
    }
    if (target->madBindings != NULL)
    {
        madAnimationStop(target);
    }
    // CAT: Cataclysm's "ship infected" status speech (the exe's literal 0x10008a1)
    speechEventQueue(target, 0x10008a1, target->shiptype, -1, -1, -1, -1, 0.0f, -1);
    aiplayerShipDied(target);
    shipHasJustBeenDisabled(target, TRUE);          // the exe passes 1 (0x00429aa2)
    // CAT: in Mission 7 an infected prison ship (dPrisonShip, 0x86) is added to the KAS selection
    //      "FirstInfectedConvoyShip" while the global KAS variable G_TRIGGER_FirstShipInfected is 0, and the speech
    //      is queued once more with a 5 s timeout (0x00429aa7 .. 0x00429b5c: cat_kasHashString, cat_kasVarFind,
    //      kasGrowSelectionPtr, kasfShipsAdd; M2-R8 kasf library)
    if (singlePlayerGame && (singlePlayerGameInfo.currentMission == 7) && (target->shiptype == dPrisonShip))
    {
        CatKasVar *trigger = cat_kasVarFind(0, cat_kasHashString("G_TRIGGER_FirstShipInfected"));

        if ((trigger != NULL) && (trigger->value == 0))
        {
            GrowSelection infected;

            growSelectInit(&infected);
            growSelectAddShip(&infected, target);
            kasfShipsAdd(kasGrowSelectionPtr(cat_kasHashString("FirstInfectedConvoyShip")), &infected);
            growSelectClose(&infected);
        }
        speechEventQueue(NULL, 0x10008a1, target->shiptype, -1, 0, -1, -1, 5.0f, -1);
    }
}

// CAT: new (cat_beastGetAttackStaticInfo 0x00429b70): the type's Beast-attack statics, NULL when it cannot be
//      infected (beastAttacksRequired < 0)
CatBeastAttackStatic *cat_beastGetAttackStaticInfo(Ship *ship)
{
#if defined(__i386__) || defined(_M_IX86)
    if (ship->staticinfo->beastAttacksRequired >= 0)
    {
        return (CatBeastAttackStatic *)&ship->staticinfo->beastAttackCascadeCost;
    }
    return NULL;
#else
    // HWI-FIX (M2-R9 missions-02-09): on the 64-bit build ShipType is 8 bytes (ShipDefs.h, HW_PTR_64), so the static's
    //      three fields are not this struct's layout there (beastAttackConvertTo is 8-aligned, 4 bytes further on):
    //      the struct read through the cast took the padding and the next word for the type (Mission07's infected
    //      convoy ship stopped the render on GetShipStaticInfo's check). The values are handed out in a copy instead
    //      (every caller only reads them or tests NULL); the ILP32 build keeps the exe's pointer above
    static CatBeastAttackStatic copy;

    if (ship->staticinfo->beastAttacksRequired >= 0)
    {
        copy.cascadeCost = ship->staticinfo->beastAttackCascadeCost;
        copy.attacksRequired = ship->staticinfo->beastAttacksRequired;
        copy.convertTo = ship->staticinfo->beastAttackConvertTo;
        return &copy;
    }
    return NULL;
#endif
}

void cat_beastWeaponStaticInit(void)                                        // 0x00429b90
{
    // read once, by the first beast-weapon ship's StaticInit
    if (!cat_beastWeaponTweaks.loaded)
    {
        scriptSetStruct("RACE_BEAST\\", "beastweapon.script", cat_beastWeaponScriptTable, &cat_beastWeaponTweaks);
        cat_beastWeaponTweaks.loaded = TRUE;
    }
}

void cat_beastWeaponInit(Ship *ship)                                        // 0x0042a040
{
    CatBeastWeaponSpec *spec = cat_beastWeaponGetSpec(ship);

    listInit(&spec->bulletList);
    spec->target = NULL;
    spec->charging = FALSE;
    bitClear(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeastInfectionUnlimited);
}

void cat_beastWeaponClose(Ship *ship)                                       // 0x0042a100
{
    CatBeastWeaponSpec *spec = cat_beastWeaponGetSpec(ship);

    // CAT: the exe's listRemoveAll leaves the entries allocated; they are freed here (no behaviour depends on it)
    listDeleteAll(&spec->bulletList);
    spec->target = NULL;
}

void cat_beastWeaponOrderChanged(Ship *ship)                                // 0x0042a6a0
{
    cat_beastWeaponGetSpec(ship)->target = NULL;
}

// the ship whose hit passes on, for the cascade filter (0x008dfba0)
static Ship *cat_beastCascadeFrom = NULL;

// a hit can pass on to a ship of the same player that is not being infected, is no Leech or Swarmer, has no beam of
// any Beast weapon on it, and costs no more than the cascade points left (any cost with unlimited infection)
// (the filter at 0x0042a740 that cat_beastWeaponHouseKeep hands cat_univFindNearestShip)
static bool32 cat_beastCascadeFilter(Ship *ship)
{
    Ship *from = cat_beastCascadeFrom;
    real32 points = from->catBeastCascadePoints;
    Node *node, *hitnode;
    Ship *beastShip;

    if (ship->playerowner != from->playerowner)
    {
        return FALSE;
    }
    if (ship->catBeastConvertTime != 0.0f)
    {
        return FALSE;
    }
    if ((ship->shiptype == sLeech) || (ship->shiptype == sSwarmer) || (ship->shiptype == bSwarmer))
    {
        return FALSE;
    }
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        beastShip = (Ship *)listGetStructOfNode(node);
        if (cat_staticHasBeastWeapon(beastShip->staticinfo))
        {
            for (hitnode = cat_beastWeaponGetSpec(beastShip)->bulletList.head; hitnode != NULL; hitnode = hitnode->next)
            {
                if (((CatBeastHit *)listGetStructOfNode(hitnode))->target == ship)
                {
                    return FALSE;
                }
            }
        }
    }
    if (bitTest(from->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeastInfectionUnlimited))
    {
        return TRUE;
    }
    return ship->staticinfo->beastAttackCascadeCost < points;
}

bool32 cat_shipCanUseBeastWeapon(Ship *ship)                                // 0x00429c50
{
    return cat_staticHasBeastWeapon(ship->staticinfo) && cat_shipHasUpgrade(ship, 0x10000);
}

real32 cat_beastTimeSpentStunned(ShipType type)                             // 0x0042b630
{
    if (type != dEcclesiam)
    {
        return cat_beastWeaponTweaks.beastAttackTimeSpentStunned;
    }
    return cat_beastWeaponTweaks.beastAttackTimeSpentStunned + cat_beastWeaponTweaks.beastAttackTimeSpentStunned;
}

// how far the weapon has charged, 0..1: the time since its GS_BeastWeapon gun last fired over that gun's firetime
real32 cat_beastWeaponChargeFraction(Ship *ship)                            // 0x0042b570
{
    GunInfo *gunInfo = ship->gunInfo;
    real32 fraction = 0.0f;
    real32 elapsed;
    sdword i;
    Gun *gun;

    for (i = 0; i < gunInfo->numGuns; i++)
    {
        gun = &gunInfo->guns[i];
        if (gun->gunstatic->gunsoundtype == GS_BeastWeapon)
        {
            fraction = 1.0f;
            elapsed = universe.totaltimeelapsed - gun->lasttimefired;
            if (elapsed <= gun->gunstatic->firetime)
            {
                if (gun->gunstatic->firetime <= 0.0f)
                {
                    // the exe prints a warning (no firetime) here
                    return 0.0f;
                }
                fraction = elapsed / gun->gunstatic->firetime;
                if (fraction <= 1.0f)
                {
                    return fraction;
                }
                return 1.0f;
            }
        }
    }
    return fraction;
}

// the beam of a hit stays between its two ships while it lasts (physUpdateBulletPosVel 0x004e45b0 calls this for a
// WEAPON_BEASTINFECT bullet)
void cat_beastWeaponBulletUpdate(Bullet *bullet)                            // 0x00429d80
{
    Ship *owner = bullet->owner;
    GunStatic *gunstatic;
    CatBeastHit *hit;
    Node *node;
    SpaceObjRotImpTarg *target;
    vector muzzle, heading;

    bullet->lengthvec.x = 0.1f;
    bullet->lengthvec.y = 0.1f;
    bullet->lengthvec.z = 0.1f;
    if (bullet->effect != NULL)
    {
        ((real32 *)bullet->effect->variable)[SCALESHIFT(ETG_LengthVariable)] = 0.1f;
    }
    if ((owner == NULL) || (bullet->gunowner == NULL))
    {
        return;
    }
    gunstatic = bullet->gunowner->gunstatic;
    // the exe uses the owner's Beast weapon block unchecked (only the four Beast weapon ships fire this bullet)
    for (node = cat_beastWeaponGetSpec(owner)->bulletList.head; node != NULL; node = node->next)
    {
        hit = (CatBeastHit *)listGetStructOfNode(node);
        target = bullet->target;
        if ((hit->bullet != bullet) || (hit->source == NULL) || (target == NULL))
        {
            continue;
        }
        if (cat_staticHasBeastWeapon(hit->source->staticinfo))
        {
            // from the weapon's muzzle
            matMultiplyMatByVec(&muzzle, &owner->rotatable.rotinfo.coordsys, &gunstatic->position);
            vecAdd(bullet->obj.posinfo.position, muzzle, bullet->owner->obj.posinfo.position);
        }
        else
        {
            // a cascade: from the ship it passed on from
            bullet->obj.posinfo.position = hit->source->obj.posinfo.position;
        }
        vecSub(bullet->lengthvec, target->obj.posinfo.position, bullet->obj.posinfo.position);
        heading = bullet->lengthvec;
        vecNormalize(&heading);
        bullet->bulletheading = heading;
        matCreateCoordSysFromHeading(&bullet->rotatable.rotinfo.coordsys, &heading);
        if (bullet->effect != NULL)
        {
            ((real32 *)bullet->effect->variable)[SCALESHIFT(ETG_LengthVariable)] =
                fsqrt(bullet->lengthvec.x * bullet->lengthvec.x + bullet->lengthvec.y * bullet->lengthvec.y +
                      bullet->lengthvec.z * bullet->lengthvec.z);
        }
        return;
    }
}

// a beam of the weapon reaches target: the hit counts towards infection, or does damage to a ship that cannot be
// infected; either way it is remembered for the cascade (cat_beastAttackHit 0x0042af40)
bool32 cat_beastAttackHit(Ship *source, Ship *target, Bullet *bullet)
{
    ShipStaticInfo *statinfo = target->staticinfo;
    bool32 infectable = (statinfo->beastAttacksRequired >= 0);
    Ship *infector = target->catBeastInfector;
    CatBeastHit *hit;
    real32 damage;

    if (infector == NULL)
    {
        return FALSE;
    }
    if (infectable)
    {
        target->catBeastHits++;
        if (((target->shiptype == sSentinel) || (target->shiptype == bSentinel)) && cat_shipIsSentinelFieldActive(target))
        {
            return TRUE;
        }
        if (target->catBeastHits < statinfo->beastAttacksRequired)
        {
            // not yet: stunned for a moment
            target->catBeastSpin = -1.0f;
            target->catBeastStunState = 1;
            target->catBeastStunStart = universe.totaltimeelapsed + 1.0f;
            target->catBeastStunEnd = target->catBeastStunStart + 2.5f;
        }
        else
        {
            cat_beastInfectShip(target, infector);
        }
        target->catBeastCascadeTime = universe.totaltimeelapsed;
        target->catBeastCascadeEnd = cat_beastWeaponTweaks.beastAttackCascadeDelay + universe.totaltimeelapsed;
        target->catBeastCascadePoints = cat_beastWeaponTweaks.beastAttackCascadePoints;
    }
    else
    {
        // a fifth of its health, but no less than 2000 unless that kills it (then all but 1)
        damage = target->targetable.health * 0.2f;
        if (damage < 2000.0f)
        {
            damage = target->targetable.health - 1.0f;
        }
        // the exe passes damage type 0x15 (one past GS_BeastWeapon)
        ApplyDamageToTarget(&target->targetable, damage, (GunSoundType)(NUM_GUN_SOUND_TYPES|GS_LargeIonCannon), 0x29a,
                            infector->playerowner->playerIndex, NULL, 3);
        source->catQueuedAttackTarget = NULL;
        if ((!singlePlayerGame) && (source->shiptype == bHeavyCruiser))
        {
            source->catQueuedAttackTarget = target;
        }
        target->catBeastCascadeTime = universe.totaltimeelapsed;
        target->catBeastCascadeEnd = cat_beastWeaponTweaks.beastAttackCascadeDelay + universe.totaltimeelapsed;
        target->catBeastCascadePoints = 0.0f;
    }
    // the exe uses the infector's Beast weapon block unchecked
    hit = memAlloc(sizeof(CatBeastHit), "BeastHit", 0);
    hit->bullet = bullet;
    hit->source = source;
    hit->target = target;
    listAddNode(&cat_beastWeaponGetSpec(infector)->bulletList, &hit->link, hit);
    return TRUE;
}

void cat_beastWeaponHouseKeep(Ship *ship)                                   // 0x0042a900
{
    GunInfo *gunInfo = ship->gunInfo;
    CatBeastWeaponSpec *spec;
    SpaceObjRotImpTarg *aim;
    vector dir;
    real32 bestDistSq, distSq, t;
    Node *node, *nextnode;
    CommandToDo *command;
    SelectCommand *selection;
    Ship *sentinel, *from, *next, *savedInfector;
    Player *savedOwner;
    bool32 blocked;
    sdword i;
    Gun *gun;
    Bullet *bullet;
    CatBeastHit *hit;
    SelectCommand selectone;

    if (gunInfo == NULL)
    {
        return;
    }
    spec = cat_beastWeaponGetSpec(ship);
    if (spec->target == NULL)
    {
        if (spec->charging)
        {
            ship->autostabilizeship = TRUE;
            spec->charging = FALSE;
        }
    }
    else
    {
        aim = (SpaceObjRotImpTarg *)spec->target;
        vecSub(dir, aim->obj.posinfo.position, ship->obj.posinfo.position);
        bestDistSq = vecMagnitudeSquared(dir);
        vecNormalize(&dir);
        // an active Sentinel field of another player between the ship and its target takes the beam: aim at the
        // nearest Sentinel of that field instead
        for (node = universe.mainCommandLayer.todolist.head; node != NULL; node = node->next)
        {
            command = (CommandToDo *)listGetStructOfNode(node);
            if (!(command->ordertype.attributes & COMMAND_MASK_PROTECTING) || (command->protect == NULL) ||
                (command->protect->numShips == 0))
            {
                continue;
            }
            selection = command->selection;
            if (selection->ShipPtr[0]->playerowner == ship->playerowner)
            {
                continue;
            }
            blocked = FALSE;
            for (i = 0; i < selection->numShips; i++)
            {
                sentinel = selection->ShipPtr[i];
                if (((sentinel->shiptype == sSentinel) || (sentinel->shiptype == bSentinel)) &&
                    cat_shipIsSentinelFieldActive(sentinel) &&
                    (cat_collLineSentinelFieldIntersect(sentinel, &ship->obj.posinfo.position, &dir, &t) != -1))
                {
                    blocked = TRUE;
                }
            }
            if (blocked)
            {
                for (i = 0; i < selection->numShips; i++)
                {
                    sentinel = selection->ShipPtr[i];
                    if ((sentinel->shiptype == sSentinel) || (sentinel->shiptype == bSentinel))
                    {
                        vector d;

                        vecSub(d, ship->obj.posinfo.position, sentinel->obj.posinfo.position);
                        distSq = vecMagnitudeSquared(d);
                        if (distSq < bestDistSq)
                        {
                            aim = (SpaceObjRotImpTarg *)sentinel;
                            bestDistSq = distSq;
                        }
                    }
                }
            }
        }
        spec->target = (Ship *)aim;
        ship->autostabilizeship = FALSE;
        aishipFlyToPointAvoidingObjsFunc(ship, &aim->obj.posinfo.position,
                                         AISHIP_FirstPointInDirectionFlying|AISHIP_PointInDirectionFlying|AISHIP_FastAsPossible|AISHIP_CarTurn, 0.0f, NULL);
        if (cat_beastWeaponChargeFraction(ship) == 1.0f)
        {
            vector d;

            vecSub(d, ship->obj.posinfo.position, aim->obj.posinfo.position);
            distSq = vecMagnitudeSquared(d);
            bullet = NULL;
            for (i = 0; (i < gunInfo->numGuns) && (bullet == NULL); i++)
            {
                gun = &gunInfo->guns[i];
                if ((gun->gunstatic->gunsoundtype == GS_BeastWeapon) &&
                    (distSq < gun->gunstatic->bulletlength * gun->gunstatic->bulletlength) &&
                    gunOrientGimbleGun(ship, gun, aim) && (gun->catWeaponType == WEAPON_BEASTINFECT))
                {
                    bullet = gunShoot(ship, gun, aim);
                }
            }
            if (bullet != NULL)
            {
                Ship *aimShip = (Ship *)aim;

                bullet->hitEffect = etgGunEventTable[RACE_Beast][GS_BeastWeapon][EGT_GunHit];     // 0x00c41490
                savedInfector = aimShip->catBeastInfector;
                aimShip->catBeastInfector = ship;
                ship->catBeastInfector = ship;
                savedOwner = aimShip->catBeastInfectorOwner;
                aimShip->catBeastInfectorOwner = ship->playerowner;
                ship->catBeastInfectorOwner = ship->playerowner;
                if (!cat_beastAttackHit(ship, aimShip, bullet))
                {
                    aimShip->catBeastInfector = savedInfector;
                    aimShip->catBeastInfectorOwner = savedOwner;
                }
                else
                {
                    soundEventPlay(ship, Gun_WeaponShot, bullet->gunowner);
                }
                spec->target = NULL;
                selectone.numShips = 1;
                selectone.ShipPtr[0] = ship;
                // CAT: the exe passes RemoveShipsFromDoingStuff 0x00444810 the reason 0x1D
                RemoveShipsFromDoingStuff(&universe.mainCommandLayer, &selectone,0x1D);
                ship->catAttackStarted = FALSE;
            }
        }
        spec->charging = TRUE;
    }

    // the cascades: when a hit ship's cascade delay is over, the hit passes on to the nearest ship of its player
    // within beastAttackCascadeDistance, which costs that ship's beastAttackCascadeCost of the points left
    for (node = spec->bulletList.head; node != NULL; node = node->next)
    {
        hit = (CatBeastHit *)listGetStructOfNode(node);
        from = hit->target;
        if ((from == NULL) || (hit->bullet == NULL) || (from->catBeastCascadeEnd == 0.0f) ||
            !(from->catBeastCascadeEnd < universe.totaltimeelapsed))
        {
            continue;
        }
        cat_beastCascadeFrom = from;
        next = cat_univFindNearestShip((SpaceObj *)from, cat_beastCascadeFilter);
        if (next != NULL)
        {
            vector d;

            vecSub(d, from->obj.posinfo.position, next->obj.posinfo.position);
            distSq = vecMagnitudeSquared(d);
        }
        if ((next == NULL) ||
            (cat_beastWeaponTweaks.beastAttackCascadeDistance * cat_beastWeaponTweaks.beastAttackCascadeDistance <= distSq))
        {
            from->catBeastCascadePoints = 0.0f;
        }
        else
        {
            savedInfector = next->catBeastInfector;
            savedOwner = next->catBeastInfectorOwner;
            if ((savedInfector == NULL) || (savedInfector->playerowner != ship->playerowner) ||
                bitTest(next->shipOrDerelict.specialFlags2, SPECIAL_2_CatExempt20))
            {
                next->catBeastInfector = ship->catBeastInfector;
                next->catBeastInfectorOwner = ship->catBeastInfectorOwner;
                bullet = cat_gunCreateBullet(ship, hit->bullet->gunowner, (SpaceObj *)from, (SpaceObjRotImpTarg *)next, 1.0f);
                if (bullet != NULL)
                {
                    bullet->hitEffect = etgGunEventTable[RACE_Beast][GS_BeastWeapon][EGT_GunHit];
                    if (!cat_beastAttackHit(from, next, bullet))
                    {
                        from->catBeastCascadePoints = 0.0f;
                        next->catBeastInfector = savedInfector;
                        next->catBeastInfectorOwner = savedOwner;
                    }
                    else
                    {
                        // the exe prints the cascade points left here
                        next->catBeastCascadePoints = from->catBeastCascadePoints - next->staticinfo->beastAttackCascadeCost;
                        from->catBeastCascadePoints = 0.0f;
                        // CAT: Cataclysm's cascade sound event 0x837 (the exe's literal)
                        soundEventPlay(from, 0x837, NULL);
                    }
                }
            }
        }
        from->catBeastCascadeEnd = 0.0f;
    }

    // hits whose beam is gone are dropped (the exe's listRemoveNode leaves the entry allocated; it is freed here)
    for (node = spec->bulletList.head; node != NULL; node = nextnode)
    {
        nextnode = node->next;
        if (((CatBeastHit *)listGetStructOfNode(node))->bullet == NULL)
        {
            listDeleteNode(node);
        }
    }
}

void cat_beastWeaponBulletRemoval(Ship *ship, Ship *deadShip, Bullet *bullet)    // 0x0042a1b0
{
    Node *node;
    CatBeastHit *hit;

    for (node = cat_beastWeaponGetSpec(ship)->bulletList.head; node != NULL; node = node->next)
    {
        hit = (CatBeastHit *)listGetStructOfNode(node);
        if ((bullet != NULL) && (hit->bullet == bullet))
        {
            hit->bullet = NULL;
        }
        if (deadShip != NULL)
        {
            if (hit->source == deadShip)
            {
                hit->source = NULL;
            }
            if (hit->target == deadShip)
            {
                hit->target = NULL;
            }
        }
    }
}

void cat_beastWeaponRemoveShipReferences(Ship *ship, Ship *shiptoremove)   // 0x0042a280
{
    CatBeastWeaponSpec *spec = cat_beastWeaponGetSpec(ship);
    Node *node;
    CatBeastHit *hit;

    if (spec->target == shiptoremove)
    {
        spec->target = NULL;
    }
    for (node = spec->bulletList.head; node != NULL; node = node->next)
    {
        hit = (CatBeastHit *)listGetStructOfNode(node);
        if (shiptoremove != NULL)
        {
            if (hit->source == shiptoremove)
            {
                hit->source = NULL;
            }
            if (hit->target == shiptoremove)
            {
                hit->target = NULL;
            }
        }
    }
}

// the nearest target the weapon may take: no Leech or Swarmer, and no undetected stealth ship in multiplayer
// (cat_beastWeaponFindNearestTarget 0x0042a3d0)
static Ship *cat_beastWeaponFindNearestTarget(Ship *ship, SelectCommand *targets)
{
    Ship *nearest = NULL, *target;
    real32 nearestDistSq = 1e21f, distSq;
    sdword i;

    if (targets->numShips == 0)
    {
        return NULL;
    }
    for (i = 0; i < targets->numShips; i++)
    {
        target = targets->ShipPtr[i];
        if ((target->shiptype != sLeech) && (target->shiptype != sSwarmer) && (target->shiptype != bSwarmer) &&
            ((cat_shipStealthTargetState(target, ship->playerowner) == 0) || singlePlayerGame))
        {
            vector d;

            vecSub(d, target->obj.posinfo.position, ship->obj.posinfo.position);
            distSq = vecMagnitudeSquared(d);
            if (distSq < nearestDistSq)
            {
                nearestDistSq = distSq;
                nearest = target;
            }
        }
    }
    return nearest;
}

bool32 cat_beastWeaponHasTarget(Ship *ship, SelectCommand *targets)        // 0x0042a4a0
{
    if (cat_beastWeaponGetSpec(ship)->target != NULL)
    {
        return TRUE;
    }
    return cat_beastWeaponFindNearestTarget(ship, targets) != NULL;
}

// the special order: the nearest attackable target that no beam of this weapon already holds becomes the weapon's
// target, once it is not being infected; TRUE when the order is done (cat_beastWeaponAttack 0x0042a550)
bool32 cat_beastWeaponAttack(Ship *ship, void *custom)
{
    SelectCommand *targets = (SelectCommand *)custom;
    CatBeastWeaponSpec *spec = cat_beastWeaponGetSpec(ship);
    Ship *nearest, *previous;
    Node *node;

    cat_selMakeTargetsBeastAttackable(targets);
    if (targets->numShips == 0)
    {
        return TRUE;
    }
    if (!cat_shipHasUpgrade(ship, 0x10000) && !singlePlayerGame)
    {
        return TRUE;
    }
    nearest = cat_beastWeaponFindNearestTarget(ship, targets);
    if (nearest != NULL)
    {
        previous = spec->target;
        for (node = spec->bulletList.head; node != NULL; node = node->next)
        {
            if (((CatBeastHit *)listGetStructOfNode(node))->target == nearest)
            {
                goto held;
            }
        }
        if (nearest->catBeastConvertTime == 0.0f)
        {
            spec->target = nearest;
        }
held:
        if (previous != nearest)
        {
            // CAT: Cataclysm's "Beast weapon target" speech event 0x40008b7 (the exe's literal)
            speechEventQueue(NULL, 0x40008b7, -1, -1, 0, ship->playerowner->playerIndex, -1, 5.0f, -1);
        }
    }
    return spec->target == NULL;
}

void cat_beastWeaponSave(Ship *ship)                                        // 0x0042b270
{
    CatBeastWeaponSpec *spec = cat_beastWeaponGetSpec(ship);
    Node *node;
    SaveChunk *chunk;
    CatBeastHit *saved;

    SaveInfoNumber(spec->bulletList.num);
    SaveInfoNumber(spec->unknown10);
    SaveInfoNumber(SpaceObjRegistryGetID((SpaceObj *)spec->target));
    for (node = spec->bulletList.head; node != NULL; node = node->next)
    {
        chunk = CreateChunk(BASIC_STRUCTURE, sizeof(CatBeastHit), listGetStructOfNode(node));
        saved = (CatBeastHit *)chunkContents(chunk);
        saved->bullet = (Bullet *)(size_t)SpaceObjRegistryGetID((SpaceObj *)saved->bullet);
        saved->source = (Ship *)(size_t)SpaceObjRegistryGetID((SpaceObj *)saved->source);
        saved->target = (Ship *)(size_t)SpaceObjRegistryGetID((SpaceObj *)saved->target);
        SaveThisChunk(chunk);
        memFree(chunk);
    }
}

void cat_beastWeaponLoad(Ship *ship)                                        // 0x0042b380
{
    CatBeastWeaponSpec *spec = cat_beastWeaponGetSpec(ship);
    sdword num, i;
    SaveChunk *chunk;
    CatBeastHit *hit;

    num = LoadInfoNumber();
    spec->unknown10 = LoadInfoNumber();
    spec->target = (Ship *)(size_t)LoadInfoNumber();
    listInit(&spec->bulletList);
    for (i = 0; i < num; i++)
    {
        chunk = LoadNextChunk();
        hit = memAlloc(sizeof(CatBeastHit), "BeastHit", 0);
        memcpy(hit, chunkContents(chunk), sizeof(CatBeastHit));
        memFree(chunk);
        listAddNode(&spec->bulletList, &hit->link, hit);
    }
}

void cat_beastWeaponFix(Ship *ship)                                         // 0x0042b490
{
    CatBeastWeaponSpec *spec = cat_beastWeaponGetSpec(ship);
    Node *node;
    CatBeastHit *hit;

    spec->target = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->target);
    for (node = spec->bulletList.head; node != NULL; node = node->next)
    {
        hit = (CatBeastHit *)listGetStructOfNode(node);
        hit->bullet = (Bullet *)SpaceObjRegistryGetObj((sdword)(size_t)hit->bullet);
        hit->source = (Ship *)SpaceObjRegistryGetObj((sdword)(size_t)hit->source);
        hit->target = (Ship *)SpaceObjRegistryGetObj((sdword)(size_t)hit->target);
    }
}
