// =============================================================================
//  Blast.h
//  - Cataclysm's blasts: expanding spherical shock waves (mothership and
//    crystal deaths, the Dreadnought, the Nomad moon, the slipgate and the
//    big gun) that push and damage what they pass (cataclysm.exe 0x00515940-0x00516f90)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___BLAST_H
#define ___BLAST_H

#include "LinkedList.h"
#include "SpaceObj.h"
#include "Types.h"

// CAT: new file (M2-R4 combat). The blast types index the exe's six templates (0x00893da0, 0x68 bytes apart).
#define CAT_BLAST_SHOCKWAVE         0   // 13000 range (CrystalDied, ...)
#define CAT_BLAST_SHOCKWAVE_LARGE   1   // 18000 range; also the single-player big-gun blast
#define CAT_BLAST_MOTHERSHIP        2   // MothershipDied 0x005bb9c0
#define CAT_BLAST_TYPE3             3
#define CAT_BLAST_TYPE4             4   // spares the allies of its player (and the Nomad ships, the RKO)
#define CAT_BLAST_TYPE5             5   // spares the Beast race, the Nomad ships, the RKO, single player's player 1
#define CAT_NUM_BLAST_TYPES         6

// one blast, 0x64 bytes on ILP32 (cat_blastCreate 0x00515a50 copies 0x19 dwords of the template, then sets the
// position, start time and player; the node is memAlloc(0x74) with the blast at +0x10)
typedef struct CatBlast
{
    sdword type;                    // +0x00 CAT_BLAST_*
    sdword effectIndex;             // +0x04 etgSpecialPurposeEffectTable index of its effect, -1 none
    vector position;                // +0x08
    real32 damageConst;             // +0x14 damage = const + k/d + k/sqrt(d) + k/d^2 (cat_blastAffectShip)
    real32 damageOverDist;          // +0x18
    real32 damageOverSqrtDist;      // +0x1C
    real32 damageOverDistSq;        // +0x20
    real32 force;                   // +0x24 push = (force + forceBySize * collspheresize^2) * speed
    real32 forceBySize;             // +0x28
    real32 speed;                   // +0x2C how fast the shell grows
    real32 startTime;               // +0x30
    real32 delay;                   // +0x34 the shell starts this long after creation
    real32 thickness;               // +0x38 width of the shell that acts
    real32 radius;                  // +0x3C current radius of the shell
    real32 startRadius;             // +0x40
    real32 maxRadius;               // +0x44 the blast ends here
    udword unknown48;               // +0x48
    sdword playerIndex;             // +0x4C the player its damage is credited to
    bool8 effectSpawned;            // +0x50
    ubyte pad51[3];
    udword unknown54;               // +0x54 cleared at creation
    sdword soundHandle;             // +0x58 -1 none (cat_soundEventBlast, stage 3)
    sdword soundEvent;              // +0x5C
    real32 damageScale;             // +0x60
} CatBlast;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatBlast) == 0x64, "a blast is 0x64 bytes in cataclysm.exe");
#endif

void cat_blastInit(void);
void cat_blastReset(void);
CatBlast *cat_blastCreate(sdword type, real32 x, real32 y, real32 z, sdword playerIndex);
void cat_blastCreateSlipgateExplosion(real32 x, real32 y, real32 z);
void cat_blastCreateBigGunExplosion(real32 x, real32 y, real32 z, sdword playerIndex, real32 damageScale);
void cat_blastUpdateAll(void);
void cat_blastRequestEffects(void);
// CAT (M2-R5 render)
void cat_blastRenderAll(void);                                                  // 0x00516750
void cat_dreadnoughtChargeEffect(vector *pos, real32 frac);                     // 0x00516a40
void cat_nomadMoonBeamEffect(vector *moon, real32 frac, vector *emitter, vector *point);   // 0x00516bb0
void cat_bigGunBulletTrack(Bullet *bullet);
void cat_bigGunBulletUntrack(Bullet *bullet);
void cat_bigGunBulletRebuildList(void);
bool32 cat_bigGunBulletsInFlight(void);

#endif
