// =============================================================================
//  ShipPrint.h
//  - Cataclysm's ship info panel: the first selected ship's name, experience,
//    technologies, class, health, armour and guns, drawn top left over the
//    main view and the sensors manager (cataclysm.exe 0x0050f910-0x00511752)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___SHIPPRINT_H
#define ___SHIPPRINT_H

#include "Color.h"
#include "SpaceObj.h"
#include "Types.h"

// CAT: new file (M2-R5 hud). The exe links these functions between Shader.c and ShipSelect.c (after the ship names,
//      ShipNames.c), a source file of their own. Names and addresses from the r6 export; the panel's variables have
//      names chosen here.

// one technology row of the panel (0x14 bytes in the exe, 26 rows at 0x008937e0)
typedef struct CatShipPrintTech
{
    udword mask;                // +0x00 the Ship.catUpgradeFlags bit
    sdword stringId;            // +0x04 the row's string (str_SO_* technologies)
    bool32 show;                // +0x08 set by cat_shipPrintTech every draw
    color colour;               // +0x0C
    sdword shownTime;           // +0x10 (sdword)universe.totaltimeelapsed when last shown
} CatShipPrintTech;

#define CAT_SHIPPRINT_NUM_TECHS     26

extern bool32 catShipPrintOn;           // 0x00a752b8: the panel is switched on (a key binding toggles it)
extern sdword catShipPrintX;            // 0x00893794: the panel's top left (30, 200; moved by dragging)
extern sdword catShipPrintY;            // 0x00893798

void cat_shipPrintLine(sdword x, sdword y, color c, char *text);    // 0x0050f910
void cat_shipPrintTech(Ship *ship);                                 // 0x0050f980
void cat_shipPrintArmour(Ship *ship);                               // 0x0050fc00
void cat_shipPrintGuns(Ship *ship);                                 // 0x0050fd60
void cat_shipPrintKeyHelp(void);                                    // 0x00510940
void cat_shipPrintToggle(char key);                                 // 0x00510ca0
void cat_shipPrintDraw(Ship *ship);                                 // 0x00510e60
void cat_shipPrintStartup(void);                                    // 0x005114e0
void cat_shipPrintReset(void);                                      // 0x00511500
void cat_shipPrintHealthColor(Ship *ship, color *c1, color *c2);    // 0x00511510
bool32 cat_shipPrintDragStart(void);                                // 0x00511590
void cat_shipPrintDragEnd(void);                                    // 0x00511640
void cat_shipPrintDragUpdate(void);                                 // 0x00511690
bool32 cat_shipPrintIsDragging(void);                               // 0x00511710
sdword cat_shipPrintActive(void);                                   // 0x00511720

// CAT: the ship's experience level name (ShipView.c's neighbour in the exe)
char *cat_shipExperienceLevelName(Ship *ship);                      // 0x00514a10
bool32 cat_shipWeaponTypeUsable(Ship *ship, sdword weaponType);     // 0x00514230

#endif
