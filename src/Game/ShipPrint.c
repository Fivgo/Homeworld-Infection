// =============================================================================
//  ShipPrint.c
//  - Cataclysm's ship info panel: the first selected ship's name, experience,
//    technologies, class, health, armour and guns, drawn top left over the
//    main view and the sensors manager (cataclysm.exe 0x0050f910-0x00511752)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R5 hud). The bodies follow the r6 decompile and, where it hides an operand (sprintf arguments,
//      __ftol), the disassembly; addresses on each function. The panel is drawn in two passes: the first only measures
//      (catShipPrintRight / catShipPrintBottom), the second prints over the translucent box the first one sized.

#include "ShipPrint.h"
#include "Console.h"

#include <stdio.h>

#include "Gun.h"
#include "KeyBindings.h"
#include "main.h"
#include "mainrgn.h"
#include "Mothership.h"
#include "mouse.h"
#include "NIS.h"
#include "ObjTypes.h"
#include "prim2d.h"
#include "render.h"
#include "Select.h"
#include "Sensors.h"
#include "ShipSelect.h"
#include "SinglePlayer.h"
#include "StringSupport.h"
#include "Universe.h"
#include "FontReg.h"
#include "font.h"
#include "InfoOverlay.h"

/*=============================================================================
    Data (exe addresses; names chosen here)
=============================================================================*/
bool32 catShipPrintOn = FALSE;                  // 0x00a752b8
sdword catShipPrintX = 30;                      // 0x00893794
sdword catShipPrintY = 200;                     // 0x00893798

// the eight display toggles, keys '1'..'8' (cat_shipPrintToggle)
static bool32 catShipPrintShowGuns = TRUE;      // 0x00893778 '1'
static bool32 catShipPrintShowArmour = TRUE;    // 0x0089377c '2'
static bool32 catShipPrintShowTechHas = TRUE;   // 0x00893780 '3'
static bool32 catShipPrintShowAllTech = FALSE;  // 0x00a5e230 '4'
static bool32 catShipPrintShowMaxTech = TRUE;   // 0x00893784 '5' (multiplayer only)
static bool32 catShipPrintShowTechNeeds = TRUE; // 0x00893788 '6'
static bool32 catShipPrintShowClass = TRUE;     // 0x0089378c '7'
static bool32 catShipPrintShowHealth = TRUE;    // 0x00893790 '8'

static fonthandle catShipPrintFont;             // 0x00a5e240 ("default.hff")
static udword catShipPrintRight = 0;            // 0x00a5e23c: right edge of what the pass printed (+10)
static udword catShipPrintBottom = 0;           // 0x00a5e238: bottom edge (+10)
static bool32 catShipPrintPrinting = FALSE;     // 0x00a5e214: FALSE in the measuring pass
static sdword catShipPrintIndent = 0;           // 0x00a5e234: x offset of the columns right of the technologies

// dragging the panel (cat_shipPrintDragStart / Update / End)
static bool32 catShipPrintDragging = FALSE;     // 0x00a5e208
static rectangle catShipPrintDragRect;          // 0x00a5e218
static sdword catShipPrintDragStartX, catShipPrintDragStartY;   // 0x00a5e228 / 0x00a5e22c
static sdword catShipPrintDragLastX, catShipPrintDragLastY;     // 0x00a5e210 / 0x00a5e20c

// the colours (exe constants 0x0072b110..)
#define CAT_SP_White        0xffffffff
#define CAT_SP_Green        0xff00ff00
#define CAT_SP_Red          0xff0000ff
#define CAT_SP_Yellow       0xff00ffff
static color catSPNameColour = CAT_SP_White;            // 0x0072b110
static color catSPTechHeaderColour = CAT_SP_White;      // 0x0072b114
static color catSPAllTechColour = CAT_SP_White;         // 0x0072b118
static color catSPTechHasColour = CAT_SP_Green;         // 0x0072b11c
static color catSPMaxTechColour = CAT_SP_Red;           // 0x0072b120
static color catSPTechNeedsColour = CAT_SP_Yellow;      // 0x0072b124
static color catSPGunLabelColour = CAT_SP_White;        // 0x0072b128
static color catSPGunSizeColour[4] = { CAT_SP_White, CAT_SP_White, CAT_SP_White, CAT_SP_White };   // 0x0072b12c..
static color catSPGunUpgradeColour = CAT_SP_Yellow;     // 0x0072b13c
static color catSPGunTypeColour = CAT_SP_Green;         // 0x0072b144
static color catSPArmourColour = CAT_SP_Green;          // 0x0072b14c
static color catSPArmourUpgradeColour = CAT_SP_Yellow;  // 0x0072b150
static color catSPClassColour = CAT_SP_White;           // 0x0072b154
static color catSPHealthColour = CAT_SP_White;          // 0x0072b15c
static color catSPKeyOnColour = CAT_SP_Green;           // 0x0072b160
static color catSPKeyOffColour = CAT_SP_Red;            // 0x0072b164
static color catSPBoxColour = 0x64323232;               // 0x0072b168 (translucent)
static color catSPBoxSolidColour = 0xff000000;          // 0x0072b16c (rgl.dll software device)
static color catSPBoxOutlineColour = CAT_SP_White;      // 0x0072b170
static color catSPKeyBoxColour = 0xff000000;            // 0x0072b174
static color catSPKeyBoxOutlineColour = CAT_SP_White;   // 0x0072b178
static color catSPDragColour = CAT_SP_White;            // 0x0072b17c
static uword catSPBoxThickness = 2, catSPBoxRound = 2;  // 0x0072b180 / 0x0072b182
static sdword catSPDragThickness = 2;                   // 0x0072b184
static uword catSPKeyBoxThickness = 2, catSPKeyBoxRound = 2;   // 0x0072b188 / 0x0072b18a

// the health colours: the first row whose percentage threshold the ship's health reaches (0x0072b190, 3 dwords each)
static struct { real32 percent; color dark; color light; } catSPHealthColours[6] =
{
    { 70.0f, 0xff32a01e, 0xff00ff00 }, { 50.0f, 0xff1ea01e, 0xff00ff00 }, { 35.0f, 0xff1e8c8c, 0xff00ffff },
    { 25.0f, 0xff148c8c, 0xff00ffff }, { 12.0f, 0xff1e1491, 0xff0000ff }, { 0.0f, 0xff141491, 0xff0000ff },
};

// the technology rows (0x008937e0)
#define CAT_SPT(m, s)   { (m), (s), FALSE, CAT_SP_White, 0 }
static CatShipPrintTech catShipPrintTechs[CAT_SHIPPRINT_NUM_TECHS] =
{
    CAT_SPT(0x00000001, str_SO_AdvancedFighterDrive),   CAT_SPT(0x00000002, str_SO_AdvancedFrigateDrive),
    CAT_SPT(0x00000004, str_SO_AdvancedDestroyerDrive), CAT_SPT(0x00000008, str_SO_LinkingAbility),
    CAT_SPT(0x00000020, str_SO_MimicAbility),           CAT_SPT(0x00000040, str_SO_LeechingAbility),
    CAT_SPT(0x00000080, str_SO_ExplosiveCharge),        CAT_SPT(0x00000100, str_SO_Afterburners),
    CAT_SPT(0x00000200, str_SO_EMPWeapon),              CAT_SPT(0x00000400, str_SO_RepulseWeapon),
    CAT_SPT(0x00000800, str_SO_SwarmingAbility),        CAT_SPT(0x00001000, str_SO_ForceField1),
    CAT_SPT(0x00002000, str_SO_ForceField2),            CAT_SPT(0x00004000, str_SO_ForceField3),
    CAT_SPT(0x00008000, str_SO_DeflectionAbility),      CAT_SPT(0x00010000, str_SO_InfectingAbility),
    CAT_SPT(0x00020000, str_SO_CloakingAbility),        CAT_SPT(0x00040000, str_SO_RepairingAbility),
    CAT_SPT(0x00080000, str_SO_SalvagingAbility),       CAT_SPT(0x00100000, str_SO_CrystalHarvestAbility),
    CAT_SPT(0x00200000, str_SO_SelfRepairAbility),      CAT_SPT(0x00400000, str_SO_BeastSalvagingAbility),
    CAT_SPT(0x00800000, str_SO_AdvancedSensors),        CAT_SPT(0x04000000, str_SO_AbilityOrdinance),
    // the exe's last two rows are empty (mask 0, string 0): shown only with "all technologies", as string 0
    { 0, 0, FALSE, CAT_SP_White, 0 }, { 0, 0, FALSE, 0, 0 },
};
#undef CAT_SPT

// the experience level names (0x0072bba0, string ids)
static sdword catShipExperienceLevelStrings[] =
{
    strExperienceLevel0, strExperienceLevel1, strExperienceLevel2, strExperienceLevel3, strExperienceLevel4
};

// CAT: the panel's help and its dragging need Cataclysm's key command 0x62 held (Toggle_Overlay_Options,
//      kbCommandKeyIsHit(0x62) 0x0054ec60) and the developer console closed (0x00a2dcd4) [M2-R6 startup-keys: the loaded
//      bindings (KeyBindings.c) and the console (Console.c)]
#define CAT_KB_SHIP_INFO_HELD()     kbCommandKeyIsHit(CAT_KB_ToggleOverlayOptions)
#define CAT_CONSOLE_OPEN()          cat_consoleIsOpen()

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintLine                                 0x0050f910
    Description : One string of the panel: widens the panel's measured box; prints in the printing pass.
----------------------------------------------------------------------------*/
void cat_shipPrintLine(sdword x, sdword y, color c, char *text)
{
    sdword width = fontWidth(text);
    sdword height = fontHeight(text);

    if ((udword)(width + x) > catShipPrintRight)
    {
        catShipPrintRight = width + x + 10;
    }
    if ((udword)(height + y) > catShipPrintBottom)
    {
        catShipPrintBottom = height + y + 10;
    }
    if (catShipPrintPrinting)
    {
        fontPrint(x, y, c, text);
        catHudCounts.infoLines++;                           // CAT: port diagnostic (the HUD self-test)
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintTech                                 0x0050f980
    Description : The technology column: the upgrades the ship has (green), the ones its owner has researched for its
                  type that it lacks (yellow), in multiplayer the most its type can have (red), or every row (white),
                  as the toggles say; sets the indent of the columns to its right.
----------------------------------------------------------------------------*/
void cat_shipPrintTech(Ship *ship)
{
    sdword lineHeight = fontHeight(" ");
    sdword index, shown, y, width;
    CatShipPrintTech *tech;
    char *text;

    for (index = 0; index < CAT_SHIPPRINT_NUM_TECHS; index++)
    {
        tech = &catShipPrintTechs[index];
        if (catShipPrintShowTechHas && (ship->catUpgradeFlags & tech->mask) != 0)
        {
            tech->show = TRUE;
            tech->colour = catSPTechHasColour;
        }
        else if (catShipPrintShowTechNeeds &&
                 (tech->mask & cat_shipTypeUpgradeMask(ship->shiptype) & ship->playerowner->catUpgradeBits) != 0)
        {
            tech->show = TRUE;
            tech->colour = catSPTechNeedsColour;
        }
        else if (!singlePlayerGame && catShipPrintShowMaxTech && (tech->mask & cat_shipTypeUpgradeMask(ship->shiptype)) != 0)
        {
            tech->show = TRUE;
            tech->colour = catSPMaxTechColour;
        }
        else if (catShipPrintShowAllTech)
        {
            tech->show = TRUE;
            tech->colour = catSPAllTechColour;
        }
        else
        {
            tech->show = FALSE;
        }
    }
    y = lineHeight * 2;
    catShipPrintIndent = 10;
    shown = 0;
    for (index = 0; index < CAT_SHIPPRINT_NUM_TECHS; index++)
    {
        tech = &catShipPrintTechs[index];
        if (tech->show == TRUE)
        {
            text = strGetString(tech->stringId);
            cat_shipPrintLine(catShipPrintX, y + catShipPrintY + 0x14, tech->colour, text);
            width = fontWidth(strGetString(tech->stringId));
            if (catShipPrintIndent < width + 10)
            {
                catShipPrintIndent = fontWidth(strGetString(tech->stringId)) + 10;
            }
            shown++;
            tech->shownTime = (sdword)universe.totaltimeelapsed;
            y += lineHeight;
        }
    }
    if (shown != 0)
    {
        cat_shipPrintLine(catShipPrintX, catShipPrintY + 0x14, catSPTechHeaderColour, strGetString(str_SO_Technology));
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintArmour                               0x0050fc00
    Description : The armour level line (yellow while an armour upgrade waits).
----------------------------------------------------------------------------*/
void cat_shipPrintArmour(Ship *ship)
{
    char text[256];

    if (!catShipPrintShowArmour)
    {
        return;
    }
    text[0] = '\0';
    if (ship->catArmourLevel == 0)
    {
        sprintf(text, "%s", strGetString(str_SO_ArmourLevel1));     // the exe passes the string as the format
    }
    else if (ship->catArmourLevel == 1)
    {
        sprintf(text, "%s", strGetString(str_SO_ArmourLevel2));
    }
    else if (ship->catArmourLevel == 2)
    {
        sprintf(text, "%s", strGetString(str_SO_ArmourLevel3));
    }
    cat_shipPrintLine(catShipPrintX + catShipPrintIndent, catShipPrintY + 0x32,
                      cat_shipCanUpgradeArmour(ship) ? catSPArmourUpgradeColour : catSPArmourColour, text);
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintGuns                                 0x0050fd60
    Description : The guns table: per gun the label, size, weapon type (yellow while an upgrade to another type waits)
                  and damage, under the column headers; or "no guns" (a ramming frigate: its cutting beam).
----------------------------------------------------------------------------*/
void cat_shipPrintGuns(Ship *ship)
{
    sdword lineHeight = fontHeight(" ");
    GunInfo *gunInfo = ship->gunInfo;
    char text[256];
    bool32 hidden[30];
    bool32 pending[NUM_WEAPON_TYPES];
    sdword index, printed, width;
    sdword columnSize = 0, columnType = 0, columnDamage = 0;
    sdword x0, y, yHeader;
    color c;
    Gun *gun;
    char *weaponText;

    if (!catShipPrintShowGuns)
    {
        return;
    }
    text[0] = '\0';
    if (gunInfo != NULL)
    {
        // a missile gun without the acolyte link upgrade and a Beast infection gun without its upgrade are not shown
        for (index = 0; index < gunInfo->numGuns; index++)
        {
            if (gunInfo->guns[index].catWeaponType == WEAPON_MISSILE && !cat_shipHasUpgrade(ship, CAT_UPGRADE_AcolyteLinkFire))
            {
                hidden[index] = TRUE;
            }
            else if (ship->gunInfo->guns[index].catWeaponType == WEAPON_BEASTINFECT && !cat_shipHasUpgrade(ship, 0x10000))   // the infecting ability
            {
                hidden[index] = TRUE;
            }
            else
            {
                hidden[index] = FALSE;
            }
        }
        // the column positions: the widest label ("Gun 0:" .. as the exe measures them, from 0), size and type
        for (index = 0; index < ship->gunInfo->numGuns; index++)
        {
            if (!hidden[index])
            {
                sprintf(text, "%s %d:", strGetString(str_SO_Gun), index);
                if (columnSize < fontWidth(text))
                {
                    columnSize = fontWidth(text);
                }
            }
        }
        gunInfo = ship->gunInfo;
        for (index = 0; index < gunInfo->numGuns; index++)
        {
            gun = &gunInfo->guns[index];
            if (!hidden[index] && cat_shipWeaponTypeUsable(ship, gun->catWeaponType))
            {
                switch (gun->gunstatic->gunSize)
                {
                    case 0: sprintf(text, "%s", strGetString(str_SO_Small)); break;
                    case 1: sprintf(text, "%s", strGetString(str_SO_Medium)); break;
                    case 2: sprintf(text, "%s", strGetString(str_SO_Large)); break;
                    case 3: sprintf(text, "%s", strGetString(str_SO_VeryLarge)); break;
                    default: break;                     // the exe keeps the last text
                }
                if (columnType < fontWidth(text) + columnSize + fontWidth(" "))
                {
                    columnType = fontWidth(text) + columnSize + fontWidth(" ");
                }
            }
            gunInfo = ship->gunInfo;
        }
        if (fontWidth(strGetString(str_SO_Type)) + columnType + fontWidth(" ") > 0)
        {
            columnDamage = fontWidth(strGetString(str_SO_Type)) + columnType + fontWidth(" ");
        }
        gunInfo = ship->gunInfo;
        for (index = 0; index < gunInfo->numGuns; index++)
        {
            gun = &gunInfo->guns[index];
            if (!hidden[index] && cat_shipWeaponTypeUsable(ship, gun->catWeaponType))
            {
                weaponText = NULL;
                switch (gun->catWeaponType)
                {
                    case WEAPON_PROJECTILE: weaponText = strGetString(str_SO_Projectile); break;
                    case WEAPON_ENERGY: weaponText = strGetString(str_SO_Energy); break;
                    case WEAPON_ION: weaponText = strGetString(str_SO_Ion); break;
                    case WEAPON_PLASMABOMB: weaponText = strGetString(str_SO_PlasmaBomb); break;
                    case WEAPON_LASER: weaponText = strGetString(str_SO_Laser); break;
                    case WEAPON_MISSILE: weaponText = strGetString(str_SO_Missile); break;
                    case WEAPON_MINE: weaponText = strGetString(str_SO_Mine); break;
                    case WEAPON_BIGGUN: weaponText = strGetString(str_SO_SiegeCannon); break;
                    case WEAPON_BEASTINFECT: weaponText = strGetString(str_SO_BeastInfect); break;
                    default: break;
                }
                if (weaponText != NULL)
                {
                    sprintf(text, "%s", weaponText);
                }
                if (columnDamage < fontWidth(text) + columnType + fontWidth(" "))
                {
                    columnDamage = fontWidth(text) + columnType + fontWidth(" ");
                }
            }
            gunInfo = ship->gunInfo;
        }
        // the rows
        printed = 0;
        gunInfo = ship->gunInfo;
        for (index = 0; index < gunInfo->numGuns; index++)
        {
            if (!hidden[index])
            {
                gun = &gunInfo->guns[index];
                if (cat_shipWeaponTypeUsable(ship, gun->catWeaponType))
                {
                    sdword row = (sdword)(((double)printed + 2.5) * (double)lineHeight);
                    printed++;
                    sprintf(text, "%s %d:", strGetString(str_SO_Gun), printed);
                    x0 = catShipPrintX + catShipPrintIndent;
                    y = row + 0x46 + catShipPrintY;
                    cat_shipPrintLine(x0, y, catSPGunLabelColour, text);
                    c = 0;
                    switch (gun->gunstatic->gunSize)
                    {
                        case 0: sprintf(text, "%s", strGetString(str_SO_Small)); c = catSPGunSizeColour[0]; break;
                        case 1: sprintf(text, "%s", strGetString(str_SO_Medium)); c = catSPGunSizeColour[1]; break;
                        case 2: sprintf(text, "%s", strGetString(str_SO_Large)); c = catSPGunSizeColour[2]; break;
                        case 3: sprintf(text, "%s", strGetString(str_SO_VeryLarge)); c = catSPGunSizeColour[3]; break;
                        default: break;
                    }
                    cat_shipPrintLine(catShipPrintX + catShipPrintIndent + columnSize, y, c, text);
                    c = catSPGunTypeColour;
                    weaponText = NULL;
                    switch (gun->catWeaponType)
                    {
                        case WEAPON_PROJECTILE: weaponText = strGetString(str_SO_Projectile); break;
                        case WEAPON_ENERGY: weaponText = strGetString(str_SO_Energy); break;
                        case WEAPON_ION: weaponText = strGetString(str_SO_Ion); break;
                        case WEAPON_PLASMABOMB: weaponText = strGetString(str_SO_PlasmaBomb); break;
                        case WEAPON_LASER: weaponText = strGetString(str_SO_Laser); break;
                        case WEAPON_MISSILE: weaponText = strGetString(str_SO_Missile); break;
                        case WEAPON_MINE: weaponText = strGetString(str_SO_Mine); break;
                        case WEAPON_BIGGUN: weaponText = strGetString(str_SO_SiegeCannon); break;
                        case WEAPON_BEASTINFECT: weaponText = strGetString(str_SO_BeastInfect); break;
                        default: break;
                    }
                    if (weaponText != NULL)
                    {
                        sprintf(text, "%s", weaponText);
                    }
                    if (cat_shipGetPendingWeaponUpgrades(ship, pending) && pending[gun->catWeaponType])
                    {
                        c = catSPGunUpgradeColour;
                    }
                    cat_shipPrintLine(catShipPrintX + catShipPrintIndent + columnType, y, c, text);
                    if (cat_gunGetSoundType(gun) == 0x12 || cat_gunGetSoundType(gun) == 0x13)
                    {
                        sprintf(text, "%s", strGetString(strNotApplicable));
                    }
                    else
                    {
                        real32 damage = cat_weaponDamageVsArmour(cat_gunGetSoundType(gun), 0);
                        sprintf(text, "%.0f", (double)(cat_shipGetDamageMultiplier(ship) * damage * gun->gunstatic->damageMult));
                    }
                    cat_shipPrintLine(catShipPrintX + catShipPrintIndent + columnDamage, y, 0xffffffff, text);
                }
            }
            gunInfo = ship->gunInfo;
        }
        if (printed != 0)
        {   // the column headers, 1.5 lines above the first row
            x0 = catShipPrintX + catShipPrintIndent;
            cat_shipPrintLine(x0, catShipPrintY + 0x46, 0xffffffff, strGetString(str_SO_Weapons));
            yHeader = (catShipPrintY - (sdword)((double)lineHeight * -1.5)) + 0x46;
            cat_shipPrintLine(x0 + columnSize, yHeader, 0xffffffff, strGetString(str_SO_Size));
            cat_shipPrintLine(x0 + columnType, yHeader, 0xffffffff, strGetString(str_SO_Type));
            cat_shipPrintLine(x0 + columnDamage, yHeader, 0xffffffff, strGetString(str_SO_Damage));
            return;
        }
    }
    width = 0;
    (void)width;
    if (ship->shiptype != sRammingFrigate)
    {
        cat_shipPrintLine(catShipPrintX + catShipPrintIndent, catShipPrintY + 0x46, 0xffffffff, strGetString(str_SO_NoGuns));
        return;
    }
    cat_shipPrintLine(catShipPrintX + catShipPrintIndent, catShipPrintY + 0x46, 0xff00ff00,
                      cat_shipHasUpgrade(ship, CAT_UPGRADE_RamCuttingBeam) ?
                      strGetString(str_SO_RammingFrigateLaserUpgraded) : strGetString(str_SO_RammingFrigateLaser));
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintKeyHelp                              0x00510940
    Description : The toggles' help box top left: "(1) Show guns Enabled" .. "(8) ...".
----------------------------------------------------------------------------*/
void cat_shipPrintKeyHelp(void)
{
    sdword lineHeight = fontHeight(" ");
    rectangle rect;
    char text[256];
    static const sdword names[8] =
    {
        str_SO_ShowGuns, str_SO_ShowArmour, str_SO_ShowTechShipHas, str_SO_ShowAllTech, str_SO_ShowMaxTech,
        str_SO_ShowTechShipNeeds, str_SO_ShowClass, str_SO_ShowHealth
    };
    bool32 *toggles[8] =
    {
        &catShipPrintShowGuns, &catShipPrintShowArmour, &catShipPrintShowTechHas, &catShipPrintShowAllTech,
        &catShipPrintShowMaxTech, &catShipPrintShowTechNeeds, &catShipPrintShowClass, &catShipPrintShowHealth
    };
    sdword index;

    rect.x0 = 10;
    rect.y0 = 10;
    rect.x1 = fontWidth("(1) SHOW MAX TECHNOLOGY THE SHIP CAN HAVE DISABLED") + 0x32;
    rect.y1 = lineHeight * 7 + 0x28;
    primRectSolid2(&rect, catSPKeyBoxColour);
    primRoundRectOutline(&rect, catSPKeyBoxThickness, catSPKeyBoxOutlineColour, catSPKeyBoxRound, catSPKeyBoxRound);
    for (index = 0; index < 8; index++)
    {
        sprintf(text, "(%c) %s %s", '1' + index, strGetString(names[index]),
                *toggles[index] ? strGetString(strMO_OptionEnabled) : strGetString(strMO_OptionDisabled));
        fontPrint(0x14, lineHeight * index + 0x14, *toggles[index] ? catSPKeyOnColour : catSPKeyOffColour, text);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintToggle                               0x00510ca0
    Description : Keys '1'..'8' switch the panel's parts (the exe also calls an empty debug stub after each).
----------------------------------------------------------------------------*/
void cat_shipPrintToggle(char key)
{
    switch (key)
    {
        case '1': catShipPrintShowGuns = !catShipPrintShowGuns; break;
        case '2': catShipPrintShowArmour = !catShipPrintShowArmour; break;
        case '3': catShipPrintShowTechHas = !catShipPrintShowTechHas; break;
        case '4': catShipPrintShowAllTech = !catShipPrintShowAllTech; break;
        case '5': catShipPrintShowMaxTech = !catShipPrintShowMaxTech; break;
        case '6': catShipPrintShowTechNeeds = !catShipPrintShowTechNeeds; break;
        case '7': catShipPrintShowClass = !catShipPrintShowClass; break;
        case '8': catShipPrintShowHealth = !catShipPrintShowHealth; break;
        default: break;
    }
}

// CAT: the name line, "<type>   (<experience>)" for races 0 and 1, "<type>" otherwise
static void catShipPrintNameLine(Ship *ship, char *text)
{
    char *name = ShipTypeToNiceStr(ship->shiptype);

    if (ship->shiprace == 0 || ship->shiprace == 1)
    {
        sprintf(text, "%s   (%s)", name, cat_shipExperienceLevelName(ship));
    }
    else
    {
        sprintf(text, "%s", name);
    }
}

// CAT: the class and health lines (both passes)
static void catShipPrintClassHealth(Ship *ship, char *text, char *percent)
{
    color c1, c2;

    if (catShipPrintShowClass)
    {
        sprintf(percent, "%s: %s", strGetString(str_SO_Class), ShipClassToNiceStr(ship->staticinfo->shipclass));
        cat_shipPrintLine(catShipPrintIndent + catShipPrintX, catShipPrintY + 0x14, catSPClassColour, percent);
    }
    if (catShipPrintShowHealth)
    {
        sprintf(text, "%s: %.0f / %.0f", strGetString(str_SO_Health), (double)ship->targetable.health,
                (double)ship->staticinfo->health.maxhealth);
        cat_shipPrintLine(catShipPrintIndent + catShipPrintX, catShipPrintY + 0x23, catSPHealthColour, text);
        sprintf(percent, "   %.0f %%", (double)(ship->targetable.health / ship->staticinfo->health.maxhealth) * 100.0);
        cat_shipPrintHealthColor(ship, &c1, &c2);
        cat_shipPrintLine(fontWidth(text) + catShipPrintIndent + catShipPrintX, catShipPrintY + 0x23, c1, percent);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintDraw                                 0x00510e60
    Description : Draw the panel for the first selected ship (mrRegionDraw, smViewportRender): a measuring pass, the
                  translucent box, the printing pass, the drag outline and the help box.
----------------------------------------------------------------------------*/
void cat_shipPrintDraw(Ship *ship)
{
    fonthandle oldFont;
    rectangle rect;
    char text[256], percent[260];

    oldFont = fontMakeCurrent(catShipPrintFont);            // the exe returns early without restoring it
    if (!mrRenderMainScreen && !smSensorsActive)
    {
        return;
    }
    if (!gameIsRunning || mrWhiteOut || nisCaptureCamera || !cat_shipPrintActive() || mrDisabled || nisFullyScissored)
    {
        return;
    }
    // CAT: the exe also returns while the popup menu is open on rgl.dll's software device (0x00b3a6d8 and RGLtype
    //      0x00b3a700 == 0); GoK renders through GL only
    cat_rndDepthTestEnable(FALSE);

    // the measuring pass
    catShipPrintBottom = 0;
    catShipPrintRight = 0;
    catShipPrintPrinting = FALSE;
    catShipPrintNameLine(ship, text);
    cat_shipPrintLine(catShipPrintX, catShipPrintY, catSPNameColour, text);
    cat_shipPrintTech(ship);
    catShipPrintClassHealth(ship, text, percent);
    if (catShipPrintShowArmour)
    {                                                       // the exe inlines cat_shipPrintArmour here
        cat_shipPrintArmour(ship);
    }
    cat_shipPrintGuns(ship);
    rect.y0 = catShipPrintY - 10;
    rect.x0 = catShipPrintX - 10;
    rect.x1 = catShipPrintRight;
    rect.y1 = catShipPrintBottom;
    if (catShipPrintRight != 0)
    {
        primRectTranslucent2(&rect, catSPBoxColour);        // CAT: rgl.dll's software device drew it solid
        (void)catSPBoxSolidColour;
        primRoundRectOutline(&rect, catSPBoxThickness, catSPBoxOutlineColour, catSPBoxRound, catSPBoxRound);
    }

    // the printing pass
    catHudCounts.infoPanels++;                              // CAT: port diagnostic (the HUD self-test)
    catShipPrintBottom = 0;
    catShipPrintRight = 0;
    catShipPrintPrinting = TRUE;
    catShipPrintNameLine(ship, text);
    cat_shipPrintLine(catShipPrintX, catShipPrintY, catSPNameColour, text);
    cat_shipPrintTech(ship);
    catShipPrintClassHealth(ship, text, percent);
    cat_shipPrintArmour(ship);
    cat_shipPrintGuns(ship);
    if (catShipPrintDragging)
    {
        primRectOutline2(&catShipPrintDragRect, catSPDragThickness, catSPDragColour);
    }
    if (CAT_KB_SHIP_INFO_HELD() && !CAT_CONSOLE_OPEN())
    {
        cat_shipPrintKeyHelp();
    }
    fontMakeCurrent(oldFont);
    cat_rndDepthTestEnable(TRUE);
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintStartup                              0x005114e0
----------------------------------------------------------------------------*/
void cat_shipPrintStartup(void)
{
    catShipPrintFont = frFontRegister("default.hff");
    cat_shipPrintReset();
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintReset                                0x00511500
----------------------------------------------------------------------------*/
void cat_shipPrintReset(void)
{
    catShipPrintDragging = FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintHealthColor                          0x00511510
    Description : The health percentage's colours: the first row of catSPHealthColours whose threshold it reaches.
----------------------------------------------------------------------------*/
void cat_shipPrintHealthColor(Ship *ship, color *c1, color *c2)
{
    real32 percent = (ship->targetable.health / ship->staticinfo->health.maxhealth) * 100.0f;
    sdword index;

    for (index = 0; index < 6; index++)
    {
        if (catSPHealthColours[index].percent <= percent)
        {
            *c1 = catSPHealthColours[index].light;
            *c2 = catSPHealthColours[index].dark;
            return;
        }
    }
    *c1 = catSPHealthColours[5].light;
    *c2 = catSPHealthColours[5].dark;
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintDragStart                            0x00511590
    Description : With the ship-info key held, a press inside the panel starts dragging it.
----------------------------------------------------------------------------*/
bool32 cat_shipPrintDragStart(void)
{
    if (CAT_KB_SHIP_INFO_HELD() && !CAT_CONSOLE_OPEN() &&
        catShipPrintX - 10 <= mouseCursorXPosition && catShipPrintY - 10 <= mouseCursorYPosition &&
        (udword)mouseCursorXPosition < catShipPrintRight && (udword)mouseCursorYPosition < catShipPrintBottom)
    {
        catShipPrintDragRect.y1 = catShipPrintBottom;
        catShipPrintDragRect.y0 = catShipPrintY - 10;
        catShipPrintDragStartX = mouseCursorXPosition;
        catShipPrintDragLastX = mouseCursorXPosition;
        catShipPrintDragRect.x1 = catShipPrintRight;
        catShipPrintDragging = TRUE;
        catShipPrintDragStartY = mouseCursorYPosition;
        catShipPrintDragLastY = mouseCursorYPosition;
        catShipPrintDragRect.x0 = catShipPrintX - 10;
        return TRUE;
    }
    return FALSE;
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintDragEnd                              0x00511640
----------------------------------------------------------------------------*/
void cat_shipPrintDragEnd(void)
{
    if (catShipPrintDragging)
    {
        catShipPrintX += mouseCursorXPosition - catShipPrintDragStartX;
        catShipPrintY += mouseCursorYPosition - catShipPrintDragStartY;
        catShipPrintDragging = FALSE;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintDragUpdate                           0x00511690
----------------------------------------------------------------------------*/
void cat_shipPrintDragUpdate(void)
{
    if (catShipPrintDragging)
    {
        catShipPrintDragRect.x0 += mouseCursorXPosition - catShipPrintDragLastX;
        catShipPrintDragRect.x1 += mouseCursorXPosition - catShipPrintDragLastX;
        catShipPrintDragRect.y0 += mouseCursorYPosition - catShipPrintDragLastY;
        catShipPrintDragRect.y1 += mouseCursorYPosition - catShipPrintDragLastY;
        catShipPrintDragLastX = mouseCursorXPosition;
        catShipPrintDragLastY = mouseCursorYPosition;
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintIsDragging                           0x00511710
----------------------------------------------------------------------------*/
bool32 cat_shipPrintIsDragging(void)
{
    return catShipPrintDragging;
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipPrintActive                               0x00511720
    Description : The panel is on and the first selected ship is the player's.
----------------------------------------------------------------------------*/
sdword cat_shipPrintActive(void)
{
    if (catShipPrintOn == TRUE && selSelected.numShips > 0 && selSelected.ShipPtr[0] != NULL &&
        selSelected.ShipPtr[0]->playerowner == universe.curPlayerPtr)
    {
        return 1;
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipExperienceLevelName                       0x00514a10
----------------------------------------------------------------------------*/
char *cat_shipExperienceLevelName(Ship *ship)
{
    return strGetString(catShipExperienceLevelStrings[ship->catExperienceLevel]);
}

/*-----------------------------------------------------------------------------
    Name        : cat_shipWeaponTypeUsable                          0x00514230
    Description : Every weapon type but the big gun, which needs the mothership's big gun module.
----------------------------------------------------------------------------*/
bool32 cat_shipWeaponTypeUsable(Ship *ship, sdword weaponType)
{
    switch (weaponType)
    {
        case WEAPON_PROJECTILE:
        case WEAPON_ENERGY:
        case WEAPON_ION:
        case WEAPON_PLASMABOMB:
        case WEAPON_LASER:
        case WEAPON_MISSILE:
        case WEAPON_MINE:
        case WEAPON_BEASTINFECT:
            return TRUE;
        case WEAPON_BIGGUN:
            return cat_mothershipHasBigGunModule(ship);
        default:
            return FALSE;
    }
}
