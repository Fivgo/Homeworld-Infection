// =============================================================================
//  CommandDefs.h
//  - List of commands and command attributes
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 4/26/1999 by fpoiker
// =============================================================================

#ifndef ___COMMANDDEFS_H
#define ___COMMANDDEFS_H

#define COMMAND_NULL                             0
#define COMMAND_MOVE                             1
#define COMMAND_ATTACK                           2
#define COMMAND_DOCK                             3
#define COMMAND_LAUNCH_SHIP                      4
#define COMMAND_COLLECT_RESOURCES                5
#define COMMAND_BUILDING_SHIP                    6
#define COMMAND_SPECIAL                          7
#define COMMAND_HALT                             8
#define COMMAND_MILITARY_PARADE                  9
#define COMMAND_MP_HYPERSPACING                 10
// CAT: Cataclysm's command orders (CommandToDo.ordertype.order, a word at +0x14 on ILP32). The exe has no name
//      table for them; the names follow the cl* function that creates each order and the clProcess 0x0044b950 case
//      that runs it. 13 and 17 have no setter and no clProcess case in the exe, but cat_selMakeShipsProtectCapable
//      0x00511f00 still lists 17 (and every number 11-17 except 13), so both keep a placeholder name.
#define COMMAND_CAT_LINK                        11      // cat_clLink 0x0044d3b0; cat_processLinkToDo
#define COMMAND_CAT_UNLINK                      12      // cat_clUnlink 0x0044d7a0; cat_processUnlinkToDo
#define COMMAND_CAT_13                          13      // no setter or case found
#define COMMAND_CAT_POINT                       14      // cat_clPointOrder 0x0044dd70; cat_bigGunProcessFireCommand
#define COMMAND_CAT_MIMIC                       15      // cat_clMimic 0x0044da80; cat_mimicCommand per ship
#define COMMAND_CAT_RESEARCH                    16      // cat_clResearch 0x0044dba0; cat_processResearchToDo 0x0044dc20
#define COMMAND_CAT_17                          17      // no setter or case found; cat_selMakeShipsProtectCapable tests it

#define COMMAND_MASK_FORMATION              0x0001
#define COMMAND_MASK_PROTECTING             0x0002
#define COMMAND_MASK_PASSIVE_ATTACKING      0x0004
#define COMMAND_MASK_HOLDING_PATTERN        0x0008
#define COMMAND_MASK_ATTACKING_AND_MOVING   0x0010

#endif
