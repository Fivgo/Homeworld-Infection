// =============================================================================
//  Attributes.h
//  - property definitions set for any object in Mission Editor
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 98/09/09 by gshaw
// =============================================================================

#ifndef ___ATTRIBUTES_H
#define ___ATTRIBUTES_H

#define ATTRIBUTES_StripTechnology          0x0001
#define ATTRIBUTES_ShellOfResources         0x0002
#define ATTRIBUTES_VelToMothership          0x0004
#define ATTRIBUTES_DontApplyFriction        0x0008
#define ATTRIBUTES_KillerCollDamage         0x0010
#define ATTRIBUTES_Anomaly                  0x0020
#define ATTRIBUTES_DeleteAfterHSOut         0x0040
#define ATTRIBUTES_StartInHS                0x0080
#define ATTRIBUTES_Defector                 0x0100
#define ATTRIBUTES_HeadShotVelToMothership  0x0200
// CAT: Cataclysm's killer-collision bit (ApplyDamageToCollidingObjects 0x00545540, collCheckShipResourceColl 0x0043bac0
//      test attribute word bit 0x200; HW1 used ATTRIBUTES_KillerCollDamage 0x10). The attribute word is the exe's dword
//      since M2-R5 layout-dock; the Cataclysm-only bits are named here (names chosen here)
#define CAT_ATTRIBUTES_KillerCollision      0x0200
// CAT (M2-R5 layout-dock): the blast bits of the attribute dword (cat_blastAffectShip 0x00515d30: a large shockwave sets
//      0x20000 on every ship it reaches and spares a ship with 0x10000; kasfShipsAttributesSet 0x004933d0 writes the dword;
//      names chosen here)
#define CAT_ATTRIBUTES_BlastImmune          0x00010000
#define CAT_ATTRIBUTES_BlastHit             0x00020000
#define ATTRIBUTES_HeadShotKillerCollDamage 0x0400
// CAT (M2-R5 dock): two Cataclysm attribute bits of the exe's dword (SpaceObj +0x38), names chosen here from their
//      readers: 0x400 marks a host whose bays are closed (ThisShipCanDockWith 0x00464290 then admits only what
//      cat_dockCanDockWithBusyHost 0x00463a70 allows, and clLaunchShip 0x0044a220 refuses to launch a ship that was not
//      built externally); 0x40000 lets a ship dock with a host that is not its ally.
#define CAT_ATTRIBUTES_DockBaysClosed       0x0400
#define CAT_ATTRIBUTES_DockWithEnemies      0x00040000
#define ATTRIBUTES_Regrow                   0x0800

#define ATTRIBUTES_SMColor1                 0x1000      // 0 indicates no change, 1 indicates invisible, 2 indicates yellow, 3 indicates green
#define ATTRIBUTES_SMColor2                 0x2000
#define ATTRIBUTES_SMColorField             0x3000
#define ATTRIBUTES_SMColorInvisible         0x1000
#define ATTRIBUTES_SMColorYellow            0x2000
#define ATTRIBUTES_SMColorGreen             0x3000

#define ATTRIBUTES_ScaleResources           0x4000

#define ATTRIBUTES_TeamLeader               0x8000          // this ship is considered the leader of a team

// warning, attributes is only a word and cannot hold any more bits!

// defines for attributesParam
#define REGROWRATE_Max       7


#endif

