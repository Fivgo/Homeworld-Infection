// =============================================================================
//  dBeastMothership.h
//  - Cataclysm's derelict dBeastMothership: the Beast mothership as a mission
//    object, with the infection weapon (cataclysm.exe 0x005b00d0-0x005b0260)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___DBEASTMOTHERSHIP_H
#define ___DBEASTMOTHERSHIP_H

#include "BeastWeapon.h"
#include "SpaceObj.h"

// CAT: new file (M2-R4 ships-b). 0x18 bytes in cataclysm.exe (dBeastMothershipHeader 0x008c3920 sizeofShipSpecifics
//      24): the Beast weapon block, at +0x00 as in MothershipSpec, so the header's slots 8 and 9 can be Mothership's
typedef struct
{
    CatBeastWeaponSpec beastWeapon;     // +0x00
} dBeastMothershipSpec;
#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(dBeastMothershipSpec) == 0x18, "dBeastMothershipSpec is 0x18 bytes in cataclysm.exe");
#endif

extern CustShipHeader dBeastMothershipHeader;

#endif
