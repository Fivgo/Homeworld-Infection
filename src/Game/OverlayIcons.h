// =============================================================================
//  OverlayIcons.h
//  - Cataclysm's overlay icons: small textured quads (popup menu borders, the
//    build/research bars, the stats screen buttons, the experience star, the
//    sensors scroll arrows, the disabled "X") read from
//    OverlayIcons\OverlayIcons.script (cataclysm.exe 0x004dbb70-0x004dc2f3)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

#ifndef ___OVERLAYICONS_H
#define ___OVERLAYICONS_H

#include "Color.h"
#include "texreg.h"
#include "Types.h"

// CAT: new file (M2-R5 hud). The exe links these five functions after Options.c's cat_opCataclysmFramerate
//      0x004dbb30, a source file of their own. The icon ids are the rows of the exe's script table 0x0088cb48 (the
//      icons themselves at 0x0088c4a8, 0x20 bytes each); the names are the script's keys.
typedef enum
{
    CAT_OI_PopupMenu_BorderCorner_TopLeft = 0,
    CAT_OI_PopupMenu_BorderCorner_TopRight,
    CAT_OI_PopupMenu_BorderCorner_BottomLeft,
    CAT_OI_PopupMenu_BorderCorner_BottomRight,
    CAT_OI_PopupMenu_Background,
    CAT_OI_PopupMenu_BorderEdge_Top,
    CAT_OI_PopupMenu_BorderEdge_Bottom,
    CAT_OI_PopupMenu_BorderEdge_Left,
    CAT_OI_PopupMenu_BorderEdge_Right,
    CAT_OI_PopupMenu_MouseOverLeftEnd,
    CAT_OI_PopupMenu_MouseOverRightEnd,             // 10
    CAT_OI_PopupMenu_MouseOverMiddle,
    CAT_OI_PopupMenu_SelectedIndicator,
    CAT_OI_PopupMenu_SubMenuIndicator,
    CAT_OI_PopupMenu_SubMenuIndicator_Disabled,
    CAT_OI_SectMothership,
    CAT_OI_BeastMothership,
    CAT_OI_Build_ProgressBar,
    CAT_OI_Research_ProgressBar,
    CAT_OI_Stats_Exit_Button_Left,
    CAT_OI_Stats_Exit_Button_Right,                 // 20
    CAT_OI_Stats_Exit_Button_Mid,
    CAT_OI_Stats_Exit_Button_Left_Mouse,
    CAT_OI_Stats_Exit_Button_Right_Mouse,
    CAT_OI_Stats_Exit_Button_Mid_Mouse,
    CAT_OI_Stats_Picker_1,
    CAT_OI_Stats_Picker_1_Mouse,
    CAT_OI_Stats_Picker_2,
    CAT_OI_Stats_Picker_2_Mouse,
    CAT_OI_Stats_Picker_Mid,
    CAT_OI_Stats_Picker_Mid_Mouse,                  // 30
    CAT_OI_ExpStar,
    CAT_OI_Stats_First,
    CAT_OI_Stats_Second,
    CAT_OI_Stats_Third,
    CAT_OI_Stats_Fourth,
    CAT_OI_Stats_Fifth,
    CAT_OI_Stats_Sixth,
    CAT_OI_Stats_Seventh,
    CAT_OI_Stats_Eigth,
    CAT_OI_SensorsScroll_Left,                      // 40
    CAT_OI_SensorsScroll_Right,
    CAT_OI_SensorsScroll_Up,
    CAT_OI_SensorsScroll_Down,
    CAT_OI_SensorsScroll_Left_Mouse,
    CAT_OI_SensorsScroll_Right_Mouse,
    CAT_OI_SensorsScroll_Up_Mouse,
    CAT_OI_SensorsScroll_Down_Mouse,
    CAT_OI_Buffer_Up_Mouse,
    CAT_OI_Buffer_Up,
    CAT_OI_Buffer_Down_Mouse,                       // 50
    CAT_OI_Buffer_Down,
    CAT_OI_X,
    CAT_OI_NumberIcons                              // 53
} CatOverlayIconId;

// one icon (0x20 bytes in the exe: the script callback fills +0x04..+0x1C)
typedef struct CatOverlayIcon
{
    char *name;                 // +0x00 the script key
    real32 u, v;                // +0x04 / +0x08 the top-left texel over the texture size
    real32 du, dv;              // +0x0C / +0x10 the icon size over the texture size
    sdword texture;             // +0x14 index in the texture list (-1 until the script sets it)
    sdword texWidth;            // +0x18
    sdword texHeight;           // +0x1C
} CatOverlayIcon;

#define CAT_OI_MAX_TEXTURES     50          // the exe's texture list 0x00a4cc78..0x00a50008, 0x108 bytes each

// one texture of the icons (0x108 bytes in the exe)
typedef struct CatOverlayIconTexture
{
    char name[256];             // +0x000 "OverlayIcons\<name>.lif"
    lifheader *lif;             // +0x100 the image, mirrored vertically at load
    udword glhandle;            // +0x104 the GL texture, created on the first draw (0 = none)
} CatOverlayIconTexture;

#if defined(__i386__) || defined(_M_IX86)
_Static_assert(sizeof(CatOverlayIcon) == 0x20, "an overlay icon is 0x20 bytes in cataclysm.exe");
_Static_assert(sizeof(CatOverlayIconTexture) == 0x108, "an overlay icon texture is 0x108 bytes in cataclysm.exe");
#endif

extern CatOverlayIcon catOverlayIcons[CAT_OI_NumberIcons];     // 0x0088c4a8
extern sdword catOverlayIconTextureCount;                       // 0x00a50008

void cat_overlayIconTextureCreate(sdword texture);              // 0x004dbb70
void cat_feOverlayIconTextureSetCB(char *directory, char *field, void *dataToFillIn);  // 0x004dbda0
void cat_overlayIconsStartup(void);                             // 0x004dbf80
void cat_overlayIconsDiscardGL(void);                           // 0x004dbfa0
void cat_overlayIconDraw(sdword icon, sdword y, sdword x, sdword width, sdword height, color c);   // 0x004dbfe0

#endif
