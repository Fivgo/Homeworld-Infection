/*=============================================================================
    AssetsExport.h: HWI-FEATURE(assets-autoexport) the game fills its Assets folder itself.

    At start-up, when the Assets folder (Documents/Homeworld Infection/Assets, or /assetsPath) has no
    assets-manifest.txt, a background thread writes every texture of the archives as a PNG, every ship as an OBJ + MTL
    for Blender and the export's lists, exactly as tools/assets/export_assets.py does (docs/features/assets-autoexport.md).
=============================================================================*/

#ifndef ___ASSETSEXPORT_H
#define ___ASSETSEXPORT_H

#include "Types.h"

#define AE_ManifestName     "assets-manifest.txt"
#define AE_JournalName      "assets-manifest.partial"   // the files an unfinished export has written (same format)
#define AE_PagesName        "assets-pages.txt"
#define AE_ModelsName       "assets-models.txt"
#define AE_ReadmeName       "README.txt"

// start the export into folder (no trailing separator) from the archives, highest precedence first; FALSE when not
// started (logged)
bool32 assetsExportStart(const char *folder, char **archives, sdword nArchives);

// quit: stop a running export (it resumes at the next start) and wait for the thread; with /catAssetsExportWait the
// export is allowed to finish first
void assetsExportStop(void);

bool32 assetsExportOffSet(char *string);     // /noAssetsExport
bool32 assetsExportWaitSet(char *string);    // /catAssetsExportWait (tests)
bool32 assetsExportIsOff(void);

#endif
