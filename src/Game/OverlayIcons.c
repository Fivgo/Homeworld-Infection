// =============================================================================
//  OverlayIcons.c
//  - Cataclysm's overlay icons: small textured quads (popup menu borders, the
//    build/research bars, the stats screen buttons, the experience star, the
//    sensors scroll arrows, the disabled "X") read from
//    OverlayIcons\OverlayIcons.script (cataclysm.exe 0x004dbb70-0x004dc2f3)
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
// =============================================================================

// CAT: new file (M2-R5 hud). The exe links these five functions after Options.c's cat_opCataclysmFramerate
//      0x004dbb30, a source file of their own; the bodies follow the r6 decompile and disassembly of each (addresses
//      on each function).

#include "OverlayIcons.h"

#include <stdio.h>
#include <string.h>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

#include "Debug.h"
#include "FEReg.h"
#include "glinc.h"
#include "main.h"
#include "Memory.h"
#include "prim2d.h"
#include "render.h"
#include "StatScript.h"
#include "InfoOverlay.h"

// FEReg.c's HW1 function (no prototype in FEReg.h)
void ferMirrorBitmapVert(udword *bitmap, sdword width, sdword height);

#define CAT_OI(n)   { #n, -1.0f, -1.0f, -1.0f, -1.0f, -1, 0, 0 }
// 0x0088c4a8: name, then u / v / du / dv -1 and texture -1 until the script sets them
CatOverlayIcon catOverlayIcons[CAT_OI_NumberIcons] =
{
    CAT_OI(PopupMenu_BorderCorner_TopLeft), CAT_OI(PopupMenu_BorderCorner_TopRight),
    CAT_OI(PopupMenu_BorderCorner_BottomLeft), CAT_OI(PopupMenu_BorderCorner_BottomRight),
    CAT_OI(PopupMenu_Background), CAT_OI(PopupMenu_BorderEdge_Top), CAT_OI(PopupMenu_BorderEdge_Bottom),
    CAT_OI(PopupMenu_BorderEdge_Left), CAT_OI(PopupMenu_BorderEdge_Right), CAT_OI(PopupMenu_MouseOverLeftEnd),
    CAT_OI(PopupMenu_MouseOverRightEnd), CAT_OI(PopupMenu_MouseOverMiddle), CAT_OI(PopupMenu_SelectedIndicator),
    CAT_OI(PopupMenu_SubMenuIndicator), CAT_OI(PopupMenu_SubMenuIndicator_Disabled), CAT_OI(SectMothership),
    CAT_OI(BeastMothership), CAT_OI(Build_ProgressBar), CAT_OI(Research_ProgressBar),
    CAT_OI(Stats_Exit_Button_Left), CAT_OI(Stats_Exit_Button_Right), CAT_OI(Stats_Exit_Button_Mid),
    CAT_OI(Stats_Exit_Button_Left_Mouse), CAT_OI(Stats_Exit_Button_Right_Mouse), CAT_OI(Stats_Exit_Button_Mid_Mouse),
    CAT_OI(Stats_Picker_1), CAT_OI(Stats_Picker_1_Mouse), CAT_OI(Stats_Picker_2), CAT_OI(Stats_Picker_2_Mouse),
    CAT_OI(Stats_Picker_Mid), CAT_OI(Stats_Picker_Mid_Mouse), CAT_OI(Exp_Star), CAT_OI(Stats_First),
    CAT_OI(Stats_Second), CAT_OI(Stats_Third), CAT_OI(Stats_Fourth), CAT_OI(Stats_Fifth), CAT_OI(Stats_Sixth),
    CAT_OI(Stats_Seventh), CAT_OI(Stats_Eigth), CAT_OI(SensorsScroll_Left), CAT_OI(SensorsScroll_Right),
    CAT_OI(SensorsScroll_Up), CAT_OI(SensorsScroll_Down), CAT_OI(SensorsScroll_Left_Mouse),
    CAT_OI(SensorsScroll_Right_Mouse), CAT_OI(SensorsScroll_Up_Mouse), CAT_OI(SensorsScroll_Down_Mouse),
    CAT_OI(Buffer_Up_Mouse), CAT_OI(Buffer_Up), CAT_OI(Buffer_Down_Mouse), CAT_OI(Buffer_Down), CAT_OI(X),
};
#undef CAT_OI

// 0x00a4cc78: the loaded textures, and their count 0x00a50008
static CatOverlayIconTexture catOverlayIconTextures[CAT_OI_MAX_TEXTURES];
sdword catOverlayIconTextureCount = 0;

// 0x0088cb48: the script table, one row per icon. CAT-FIX: the exe's table has no end row (the script names only
//      keys it has, so the lookup never runs past the 53 rows); this one ends with END_SCRIPT_ENTRY
#define CAT_OI_ENTRY(id)    { NULL, cat_feOverlayIconTextureSetCB, &catOverlayIcons[id] }
static scriptEntry catOverlayIconsTweaks[CAT_OI_NumberIcons + 1];

// CAT: nextLog2 0x00476920 (FEReg.c keeps its own static copy, as HW1's)
static sdword catOINextLog2(sdword n)
{
    sdword p = 2;
    while (p < n && p < 256)
    {
        p <<= 1;
    }
    return (n > 256) ? -1 : p;
}

/*-----------------------------------------------------------------------------
    Name        : cat_overlayIconTextureCreate                      0x004dbb70
    Description : Make the GL texture of a loaded icon image: the rows copied bottom up into a power-of-two buffer.
                  CAT: the exe packs the image to 16 bits (GL_RGBA4, rgl's packed format) when the device supports
                  it; GoK's GL backend has no capability table, so this always takes the exe's 32-bit GL_RGBA path
----------------------------------------------------------------------------*/
void cat_overlayIconTextureCreate(sdword texture)
{
    lifheader *lif = catOverlayIconTextures[texture].lif;
    sdword width = catOINextLog2(lif->width);
    sdword height = catOINextLog2(lif->height);
    udword *buffer, *src, *dest;
    sdword y, x;
    GLuint handle;

    buffer = memAlloc(width * height * 4, "overlayicontex", 0);
    for (y = 0; y < lif->height; y++)
    {
        src = (udword *)lif->data + lif->width * y;
        dest = buffer + ((lif->height - y) - 1) * width;
        for (x = lif->width; x != 0; x--)
        {
            *dest++ = *src++;
        }
    }
    glGenTextures(1, &handle);
    trClearCurrent();
    glBindTexture(GL_TEXTURE_2D, handle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    memFree(buffer);
    catOverlayIconTextures[texture].glhandle = handle;
}

/*-----------------------------------------------------------------------------
    Name        : cat_feOverlayIconTextureSetCB                     0x004dbda0
    Description : Script callback: "<texture> x y width height textureWidth textureHeight". Loads
                  OverlayIcons\<texture>.lif once (mirrored vertically) and sets the icon's texel rectangle.
----------------------------------------------------------------------------*/
void cat_feOverlayIconTextureSetCB(char *directory, char *field, void *dataToFillIn)
{
    CatOverlayIcon *icon = (CatOverlayIcon *)dataToFillIn;
    char name[256], fileName[512];
    sdword x = 0, y = 0, width = 0, height = 0, texWidth = 1, texHeight = 1;
    sdword index, found = -1;
    lifheader *lif;

    (void)directory;
    sscanf(field, "%255s%d%d%d%d%d%d", name, &x, &y, &width, &height, &texWidth, &texHeight);
    snprintf(fileName, sizeof(fileName), "OverlayIcons\\%s.lif", name);
    for (index = 0; index < catOverlayIconTextureCount && found < 0; index++)
    {
        if (strcasecmp(fileName, catOverlayIconTextures[index].name) == 0)
        {
            found = index;
        }
    }
    if (found >= 0)
    {
        icon->texture = found;
    }
    else
    {
        // CAT-FIX: the exe writes past its 50-entry list without a check (the install's script uses 45 textures)
        dbgAssertOrIgnore(catOverlayIconTextureCount < CAT_OI_MAX_TEXTURES);
        if (catOverlayIconTextureCount >= CAT_OI_MAX_TEXTURES)
        {
            return;
        }
        strncpy(catOverlayIconTextures[catOverlayIconTextureCount].name, fileName,
                sizeof(catOverlayIconTextures[0].name) - 1);
        lif = trLIFFileLoad(fileName, NonVolatile);
        if (lif == NULL)
        {
            dbgFatalf(DBG_Loc, "\nFailed to load OverlayIcon texture: %s", fileName);
        }
        ferMirrorBitmapVert((udword *)lif->data, lif->width, lif->height);
        catOverlayIconTextures[catOverlayIconTextureCount].lif = lif;
        catOverlayIconTextures[catOverlayIconTextureCount].glhandle = 0;
        icon->texture = catOverlayIconTextureCount;
        catOverlayIconTextureCount++;
    }
    icon->texHeight = texHeight;
    icon->texWidth = texWidth;
    icon->u = (real32)x / (real32)texWidth;
    icon->v = (real32)y / (real32)texHeight;
    icon->du = (real32)width / (real32)texWidth;
    icon->dv = (real32)height / (real32)texHeight;
}

/*-----------------------------------------------------------------------------
    Name        : cat_overlayIconsStartup                           0x004dbf80
    Description : Read OverlayIcons\OverlayIcons.script (utyGameSystemsInit).
----------------------------------------------------------------------------*/
void cat_overlayIconsStartup(void)
{
    sdword index;

    for (index = 0; index < CAT_OI_NumberIcons; index++)
    {
        scriptEntry entry = CAT_OI_ENTRY(index);
        entry.name = catOverlayIcons[index].name;
        catOverlayIconsTweaks[index] = entry;
    }
    catOverlayIconsTweaks[CAT_OI_NumberIcons] = (scriptEntry)END_SCRIPT_ENTRY;
    scriptSet("OverlayIcons\\", "OverlayIcons.script", catOverlayIconsTweaks);
}

/*-----------------------------------------------------------------------------
    Name        : cat_overlayIconsDiscardGL                         0x004dbfa0
    Description : Delete the icons' GL textures (renderer reset); they are made again on the next draw.
----------------------------------------------------------------------------*/
void cat_overlayIconsDiscardGL(void)
{
    sdword index;

    for (index = 0; index < catOverlayIconTextureCount; index++)
    {
        if (catOverlayIconTextures[index].glhandle != 0)
        {
            GLuint handle = catOverlayIconTextures[index].glhandle;
            glDeleteTextures(1, &handle);
            catOverlayIconTextures[index].glhandle = 0;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_overlayIconDraw                               0x004dbfe0
    Description : Draw an icon as a textured quad, top-left corner (x, y), in the colour's RGB (the texture's alpha).
                  The exe takes y before x.
----------------------------------------------------------------------------*/
void cat_overlayIconDraw(sdword icon, sdword y, sdword x, sdword width, sdword height, color c)
{
    CatOverlayIcon *oi = &catOverlayIcons[icon];
    sdword texEnabled, lighting;

    catHudCounts.icons++;                                   // CAT: port diagnostic (the HUD self-test)
    udword texEnv;
    bool32 cull, blend, alphaTest, additive;
    real32 u0, v0, u1, v1;

    texEnabled = rndTextureEnable(TRUE);
    lighting = rndLightingEnable(FALSE);
    texEnv = rndTextureEnvironment(RTE_Replace);           // the exe's mode 1 (its table 0x008bab2c: GL_REPLACE)
    cull = glIsEnabled(GL_CULL_FACE) ? TRUE : FALSE;
    blend = glIsEnabled(GL_BLEND);
    alphaTest = glIsEnabled(GL_ALPHA_TEST);
    additive = rndAdditiveBlends(FALSE);
    glDisable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glColor3ub(colRed(c), colGreen(c), colBlue(c));
    u0 = oi->u;
    v0 = oi->v;
    u1 = oi->du + oi->u;
    v1 = oi->dv + oi->v;
    // CAT: the exe switches to its mode 0 (GL_MODULATE) on the software renderer only (RGLtype 0x00b3a700 == 0);
    //      GoK always renders through GL, as the exe's GL device, so the quad replaces (the colour is not applied)
    if (catOverlayIconTextures[oi->texture].glhandle == 0)
    {
        cat_overlayIconTextureCreate(oi->texture);
    }
    trClearCurrent();
    glBindTexture(GL_TEXTURE_2D, catOverlayIconTextures[oi->texture].glhandle);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0);
    glVertex2f(primScreenToGLX(x), primScreenToGLY(y));
    glTexCoord2f(u1, v0);
    glVertex2f(primScreenToGLX(x + width), primScreenToGLY(y));
    glTexCoord2f(u1, v1);
    glVertex2f(primScreenToGLX(x + width), primScreenToGLY(y + height));
    glTexCoord2f(u0, v1);
    glVertex2f(primScreenToGLX(x), primScreenToGLY(y + height));
    glEnd();
    if (!blend)
    {
        glDisable(GL_BLEND);
    }
    if (!alphaTest)
    {
        glDisable(GL_ALPHA_TEST);                           // as the exe: disabled again, not re-enabled
    }
    rndTextureEnvironment(texEnv);
    rndAdditiveBlends(additive);
    rndTextureEnable(texEnabled);
    rndLightingEnable(lighting);
    if (cull)
    {
        glEnable(GL_CULL_FACE);
    }
}
