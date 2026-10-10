// =============================================================================
//  ClassDefs.h
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___CLASSDEFS_H
#define ___CLASSDEFS_H

// NB: these #define strings are hardcoded within the .shp
//     description files (see the "shipclass" entry in
//     Homeworld.big/r1/targetdrone.shp for example)

#define CLASS_Mothership    0
#define CLASS_HeavyCruiser  1
#define CLASS_Carrier       2
#define CLASS_Destroyer     3
#define CLASS_Frigate       4
#define CLASS_Corvette      5
#define CLASS_Fighter       6
#define CLASS_Resource      7
#define CLASS_NonCombat     8
// CAT: three new classes (shipclassinfo 0x00889a80 "CLASS_MonsterShip", "CLASS_Component", "CLASS_Micro";
//      CET game/Game/ClassDefs.h)
#define CLASS_MonsterShip   9
#define CLASS_Component     10
#define CLASS_Micro         11

#define NUM_CLASSES         (CLASS_Micro + 1)

#endif
