// =============================================================================
//  BeastWeapon.c
//  - Cataclysm's Beast infection weapon of bHeavyCruiser, bCarrier,
//    bMothership and dBeastMothership (cataclysm.exe 0x004298f0-0x0042b650)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R3 ships-a); see BeastWeapon.h for what is ported now and what is M2-R4 #3's.

#include "BeastWeapon.h"

#include "Debug.h"
#include "SaveGame.h"
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
    spec->unknown14 = 0;
    bitClear(ship->shipOrDerelict.specialFlags2, SPECIAL_2_CatBeastInfectionUnlimited);
}

void cat_beastWeaponClose(Ship *ship)                                       // 0x0042a100
{
    CatBeastWeaponSpec *spec = cat_beastWeaponGetSpec(ship);

    listRemoveAll(&spec->bulletList);
    spec->target = NULL;
}

void cat_beastWeaponOrderChanged(Ship *ship)                                // 0x0042a6a0
{
    cat_beastWeaponGetSpec(ship)->target = NULL;
}

void cat_beastWeaponHouseKeep(Ship *ship)                                   // 0x0042a900
{
    // CAT-PROVISIONAL: the weapon's charge, targeting and infection bullets (1,495 bytes in the exe) are M2-R4 #3
    (void)ship;
}

void cat_beastWeaponBulletRemoval(Ship *ship, Ship *unused, struct Bullet *bullet)    // 0x0042a1b0
{
    // CAT-PROVISIONAL: drops the bullet from the weapon's bullet list; no infection bullet exists until M2-R4 #3
    (void)ship;
    (void)unused;
    (void)bullet;
}

void cat_beastWeaponRemoveShipReferences(Ship *ship, Ship *shiptoremove)   // 0x0042a280
{
    // CAT: the target part of the exe body; the bullet-list part waits for the bullets (CAT-PROVISIONAL, M2-R4 #3)
    CatBeastWeaponSpec *spec = cat_beastWeaponGetSpec(ship);

    if (spec->target == shiptoremove)
    {
        spec->target = NULL;
    }
}

bool32 cat_beastWeaponHasTarget(Ship *ship, SelectCommand *targets)        // 0x0042a4a0
{
    // CAT-PROVISIONAL: M2-R4 #3; no target is ever valid for the weapon until then, so callers take their
    //      non-weapon path (CarrierSpecialTarget repairs, as HW1)
    (void)ship;
    (void)targets;
    return FALSE;
}

bool32 cat_beastWeaponAttack(Ship *ship, void *targets)                     // 0x0042a550
{
    // CAT-PROVISIONAL: M2-R4 #3; reports the special order as done
    (void)ship;
    (void)targets;
    return TRUE;
}

void cat_beastWeaponSave(Ship *ship)                                        // 0x0042b270
{
    // CAT-PROVISIONAL: M2-R4 #3 with the save format (the user's saves)
    (void)ship;
}

void cat_beastWeaponLoad(Ship *ship)                                        // 0x0042b380
{
    // CAT-PROVISIONAL: M2-R4 #3; a loaded ship starts with an empty weapon block
    CatBeastWeaponSpec *spec = cat_beastWeaponGetSpec(ship);

    listInit(&spec->bulletList);
    spec->target = NULL;
}

void cat_beastWeaponFix(Ship *ship)                                         // 0x0042b490
{
    // CAT: the target pointer is fixed up as the exe does; the bullet entries wait for M2-R4 #3 (the list is empty)
    CatBeastWeaponSpec *spec = cat_beastWeaponGetSpec(ship);

    spec->target = (Ship *)SpaceObjRegistryGetObj((sdword)(smemsize)spec->target);
}
