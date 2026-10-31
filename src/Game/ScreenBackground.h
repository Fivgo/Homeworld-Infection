// =============================================================================
//  ScreenBackground.h
//  - HWI-FEATURE(screen-background): a picture the user paints, drawn behind a
//    front-end screen (<settings folder>/Assets/feman/backgrounds/<screen>.png),
//    and the HWI-TEST export that writes the screen's own art for painting on
//    (docs/features/screen-background.md). Not in cataclysm.exe.
// =============================================================================

#ifndef ___SCREENBACKGROUND_H
#define ___SCREENBACKGROUND_H

#include "Types.h"
#include "Region.h"

struct tagfescreen;

void sbgRegionsAdded(struct tagfescreen *screen, regionhandle baseRegion);  // FEFlow.c feRegionsAdd: the screen's draw hooks
void sbgDiscardGL(void);                    // main.c mainResetRender: the pictures' GL textures go (loaded again when drawn)

// HWI-TEST: /catScreenBgExport <screen> [/catScreenBgDir <dir>]: once <screen> is the top screen, its art (the panels and
//      decorative pieces, no buttons, text, sliders or lists) and its controls are read back at the physical scale and
//      written as <screen>-reference.png, <screen>-controls.png and <screen>.ora (OpenRaster: GIMP opens its layers),
//      with a README.txt; then the engine quits
void sbgExportSet(char *screenName);        // HWI-TEST
void sbgExportDirSet(char *dir);            // HWI-TEST
void sbgExportFrameEnd(void);               // HWI-TEST: render.c, each frame after the mouse, before the swap

#endif
