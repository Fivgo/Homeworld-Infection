// =============================================================================
//  Mimic.h
//  - Cataclysm's sMimic: a fighter that disguises itself as another ship or
//    as an asteroid (cataclysm.exe 0x005b8330-0x005b9c60); the mimic block is
//    shared by sMCV (MCV.c) and p1StandardCorvette
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___MIMIC_H
#define ___MIMIC_H

#include <stddef.h>

#include "GenericInterceptor.h"
#include "ShipLink.h"
#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 0x13C bytes in cataclysm.exe (MimicHeader 0x008c4df0 and MCVHeader 0x008c4d10
//      sizeofShipSpecifics 316; P1StandardCorvetteSpec starts with the same block). Every function here reads the
//      block at Ship+0x638 whatever the ship's type, so the three types share this layout. Field names chosen here.
//      A disguise is a ship type (apparentType, the type's mesh and bindings) or an asteroid (apparentType -1,
//      apparentAsteroid the AsteroidType); the next* fields hold a disguise waiting for the current one to fade.
typedef struct
{
    ubyte interceptor[0x18];            // +0x000 (Ship+0x638) the interceptor specifics of sMimic (GenericInterceptorInit)
    CatLinkInfo link;                   // +0x018 (Ship+0x650)
    ubyte unknown44[0xAC - 0x44];       // +0x044 (Ship+0x67C) sMCV/p1StandardCorvette: the side step (MCVInit)
    bool32 detected[MAX_MULTIPLAYER_PLAYERS];   // +0x0AC (Ship+0x6E4) per player (cat_shipDetectedByPlayer)
    sdword apparentType;                // +0x0CC (Ship+0x704) the ship type shown, -1 an asteroid or none
    sdword apparentRace;                // +0x0D0 (Ship+0x708)
    lodinfo *apparentLOD;               // +0x0D4 (Ship+0x70C) drawn in place of the own mesh
    shipbindings *apparentBindings;     // +0x0D8 (Ship+0x710) cat_univMeshBindingsDupeNoGuns of the shown type
    sdword apparentColor;               // +0x0DC (Ship+0x714) the colour scheme shown (single player)
    real32 apparentRadius;              // +0x0E0 (Ship+0x718) 1.2 x the larger collision sphere
    sdword apparentPlayer;              // +0x0E4 (Ship+0x71C) the player it poses as, -1 none
    sdword apparentAsteroid;            // +0x0E8 (Ship+0x720) the AsteroidType shown, -1 none
    real32 asteroidRadius;              // +0x0EC (Ship+0x724)
    real32 asteroidCameraRadius;        // +0x0F0 (Ship+0x728)
    sdword asteroidResourceValue;       // +0x0F4 (Ship+0x72C) 10 x the asteroid type's resource value
    sdword nextType;                    // +0x0F8 (Ship+0x730) the waiting disguise, as above
    sdword nextRace;                    // +0x0FC
    lodinfo *nextLOD;                   // +0x100
    shipbindings *nextBindings;         // +0x104 (Ship+0x73C)
    sdword nextColor;                   // +0x108
    real32 nextRadius;                  // +0x10C
    sdword nextPlayer;                  // +0x110
    sdword nextAsteroid;                // +0x114
    real32 nextAsteroidRadius;          // +0x118
    real32 nextAsteroidCameraRadius;    // +0x11C
    sdword nextAsteroidResourceValue;   // +0x120
    real32 fade;                        // +0x124 (Ship+0x75C) 0 shown as itself .. 1 fully disguised
    ubyte mode;                         // +0x128 (Ship+0x760) MIMIC_* below
    ubyte pad129[3];
    real32 delayEnd;                    // +0x12C (Ship+0x764) the end of a MIMIC_Delay* state
    bool32 started;                     // +0x130 (Ship+0x768) set when a delayed start begins, cleared on unmimic
    struct Resource *fakeResource;      // +0x134 (Ship+0x76C) the asteroid it leaves while idle (cat_univAddMimicResource)
    real32 ramDamageScale;              // +0x138 (Ship+0x770) 2.0 (cat_shipGetRamDamageScale)
} MimicSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(offsetof(MimicSpec, link) == 0x18, "mimic link block at Ship+0x650 in cataclysm.exe");
_Static_assert(offsetof(MimicSpec, detected) == 0xAC, "mimic detected flags at Ship+0x6E4 in cataclysm.exe");
_Static_assert(offsetof(MimicSpec, apparentType) == 0xCC, "mimic type at Ship+0x704 in cataclysm.exe");
_Static_assert(offsetof(MimicSpec, nextType) == 0xF8, "mimic next type at Ship+0x730 in cataclysm.exe");
_Static_assert(offsetof(MimicSpec, mode) == 0x128, "mimic mode at Ship+0x760 in cataclysm.exe");
_Static_assert(offsetof(MimicSpec, fakeResource) == 0x134, "mimic resource at Ship+0x76C in cataclysm.exe");
_Static_assert(sizeof(MimicSpec) == 0x13C, "MimicSpec is 0x13C bytes in cataclysm.exe");
#endif

// MimicSpec.mode (MimicHouseKeep 0x005b8f30; names chosen here)
#define MIMIC_Off               0
#define MIMIC_On                1   // disguised
#define MIMIC_FadeIn            2   // fading into the disguise
#define MIMIC_FadeOut           3   // fading back to itself
#define MIMIC_Switch            4   // fading out, then into the next disguise
#define MIMIC_FadeOutUnlink     5   // fading out, then unlinking
#define MIMIC_Unlink            6
#define MIMIC_FadeOutRelink     7
#define MIMIC_Linked            8   // part of a link, the disguise kept for after it
#define MIMIC_DelayFadeIn       9
#define MIMIC_DelayFadeOut      10
#define MIMIC_DelaySwitch       11

// the target modes of cat_mimicCommand (the COMMAND_CAT_MIMIC order and kasfMimic)
#define MIMIC_COMMAND_Unmimic   0
#define MIMIC_COMMAND_ShipType  1
#define MIMIC_COMMAND_Target    2

extern CustShipHeader MimicHeader;

void cat_mimicUnmimic(Ship *ship, bool32 animate);                                  // 0x005b84f0
void cat_mimicCopyFromPartner(Ship *ship, Ship *partner);                           // 0x005b8620
void cat_mimicResetState(Ship *ship);                                               // 0x005b8700
sdword cat_mimicRandomDisguiseType(ShipType type, sdword race);                     // 0x005b83e0
void cat_mimicBecomeShipType(Ship *ship, ShipType type, sdword player);             // 0x005b88f0
void cat_mimicBecomeTarget(Ship *ship, sdword asteroid);                            // 0x005b8b00
void cat_mimicCommand(Ship *ship, sdword mode, sdword type, sdword player, sdword asteroid);  // 0x005b8d70
void cat_mimicCleanup(Ship *ship);                                                  // 0x005b8ea0
real32 cat_mimicGetAvoidRadius(Ship *ship);                                         // 0x005b9550
real32 cat_mimicGetCameraRadius(Ship *ship);                                        // 0x005b95c0
lodinfo *cat_mimicGetStaticInfo(Ship *ship);                                        // 0x005b9640
real32 cat_mimicGetApparentScale(Ship *ship);                                       // 0x005b9670
udword cat_mimicGetApparentPlayerIndex(Ship *ship);                                 // 0x005b9760
ShipClass cat_mimicGetApparentShipClass(Ship *ship);                                // 0x005b9810
sdword cat_mimicGetApparentObject(Ship *ship);                                      // 0x005b9940
struct Resource *cat_mimicGetFakeResource(Ship *ship);                              // 0x005b9960
real32 cat_shipGetRamDamageScale(Ship *ship);                                       // 0x005b9980

// the Mimic slot bodies sMCV and p1StandardCorvette call
bool32 MimicSpecialActivate(Ship *ship);                                            // 0x005b8820
bool32 MimicSpecialTarget(Ship *ship, void *custom);                                // 0x005b8dd0
void MimicHouseKeep(Ship *ship);                                                    // 0x005b8f30
void MimicDied(Ship *ship);                                                         // 0x005b9460
bool32 MimicHaltCommand(Ship *ship);                                                // 0x005b99e0
void Mimic_Fix(Ship *ship);                                                         // 0x005b9a50
void Mimic_PreFix(Ship *ship);                                                      // 0x005b9b90
void MimicRemoveShipReferences(Ship *ship, Ship *shiptoremove);                     // 0x005b9bf0

// CAT (M2-R6 mainrgn-popup): the disguise lists of the popup menu's mimic submenus (cat_mimicInitDisguiseTables
//      0x005b9c60 .. cat_mimicItemAdd 0x005ba0d0). Players 0..7 list ship types (the exe's 8-byte rows, 0x54 per player
//      at 0x00a76310); player 8 ("objects") lists the four asteroid disguises (the row's second word 0..3)
#define CAT_MIMIC_ITEM_PLAYERS      9           // the rows' players 0..8
#define CAT_MIMIC_ITEM_MAX          0x54        // rows per player
#define CAT_MIMIC_PLAYER_OBJECTS    8           // the asteroid disguises' player
typedef struct CatMimicItem
{
    udword type;                                // +0x00 the ship type (players 0..7)
    udword index;                               // +0x04 the asteroid size 0..3 (player 8)
} CatMimicItem;
void cat_mimicInitDisguiseTables(void);                                             // 0x005b9c60
ubyte cat_mimicGetCurrentPlayer(void);                                              // 0x005b9f20
void cat_mimicSetCurrentPlayer(ubyte player);                                       // 0x005b9f30
bool32 cat_mimicPlayerAvailable(ubyte player);                                      // 0x005b9f40
char *cat_mimicPlayerName(ubyte player);                                            // 0x005b9fa0
sdword cat_mimicItemCount(ubyte player, bool32 added);                              // 0x005b9ff0
CatMimicItem *cat_mimicItemGet(ubyte player, udword unused, sdword index);          // 0x005ba020
char *cat_mimicItemName(ubyte player, udword unused, sdword index);                 // 0x005ba050
void cat_mimicItemAdd(ubyte player, ShipType type);                                 // 0x005ba0d0
void cat_saveGameMiscChunks(void);                                                  // 0x005ba170 (M2-R6 oracle-mode-a)
void cat_loadGameMiscChunks(void);                                                  // 0x005ba1e0
void cat_spSaveMiscInfo(void);                                                      // 0x005b9df0
void cat_spLoadMiscInfo(void);                                                      // 0x005b9e90

#endif
