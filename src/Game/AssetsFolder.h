/*=============================================================================
    AssetsFolder.h: HWI-FEATURE(assets-folder) the moddable Assets folder.

    <user files folder>/Assets (Documents/Homeworld Infection/Assets; HWI-FEATURE(user-files)) holds files that replace the game's own (same relative path as inside the archives or the
    game folder, case ignored), PNG versions of .lif textures and their team-colour masks. A file is used only when it
    is not in the export's manifest or its size or time differs. docs/features/assets-folder.md
=============================================================================*/

#ifndef ___ASSETSFOLDER_H
#define ___ASSETSFOLDER_H

#include <stddef.h>

#include "Types.h"

// what replaces a texture (assetstexture.kinds)
#define AF_Native       0x01        // <name>.lif itself (a native file)
#define AF_Png          0x02        // <name>.png
#define AF_TeamBase     0x04        // <name>_teambase.png (TeamColor0 weight)
#define AF_TeamStripe   0x08        // <name>_teamstripe.png (TeamColor1 weight)
#define AF_Page         0x10        // an atlas page with changed source textures (assets-pages.txt)
#define AF_New          0x20        // HWI-FEATURE(assets-models): a picture the game does not have, named by an edited model

#define AF_MaxSide      4096        // largest texture side accepted

struct assetstexture;

// one changed source texture inside an atlas page (page pixels, as placed)
typedef struct
{
    struct assetstexture *source;
    sdword x, y, w, h;
    bool32 transposed;              // Paco's pacRotate: the source mirrored across its diagonal
}
assetsplacement;

typedef struct assetstexture
{
    char *key;                      // lower case, '/' separators, no extension
    udword kinds;                   // AF_*
    char *nativePath;               // AF_Native: the .lif on disk
    char *pngPath;                  // AF_Png
    char *teamPath[2];              // AF_TeamBase, AF_TeamStripe
    sdword pngWidth, pngHeight;     // AF_Png: the PNG's size
    sdword nativeWidth, nativeHeight;   // AF_Native: the .lif's size
    sdword origWidth, origHeight;   // the game's own .lif
    udword origFlags;
    sdword outWidth, outHeight;     // the size the texture registry loads
    sdword pageScale;               // AF_Page: the page is rebuilt at this multiple of its size
    sdword nPlaced;
    assetsplacement *placed;        // AF_Page: the changed sources
    char *shareParent;              // textures.ll lists it as a copy of this texture (key), whose file the game draws
    bool32 logged;
}
assetstexture;

void assetsStartup(void);
void assetsShutdown(void);

bool32 assetsPathSet(char *path);       // /assetsPath <dir>
char *assetsFolderPath(void);           // the folder (no trailing separator), NULL with /noAssets
bool32 assetsOffSet(char *string);      // /noAssets

// a native replacement for a game file, or NULL (flags as for fileOpen; settings, CD and absolute paths never)
char *assetsFileFind(char *fileName, udword flags);

// while suspended, nothing is replaced (counted)
void assetsSuspend(bool32 suspend);

// the replacement of a texture (name with or without .lif), or NULL
assetstexture *assetsTextureFind(char *name);

// the texture textures.ll says the game draws in its place (its share parent), or NULL
void assetsTextureShared(assetstexture *tex, char *parentName);

// the size a replaced texture loads at, from the size of the game's own (sets origWidth .. pageScale)
void assetsTextureSize(assetstexture *tex, sdword origWidth, sdword origHeight);

sdword assetsActiveCount(void);

// HWI-FEATURE(assets-models): the mesh image of an edited model (Models/ OBJ) for a mesh file the game loads (.geo or
//      .peo, as the game names it), or FALSE; meshName gets the game mesh it was built from (its folder holds the
//      textures). The image stays owned by the Assets folder: copy it
bool32 assetsModelFind(char *fileName, const ubyte **image, sdword *length, char *meshName, size_t meshNameSize);

// HWI-TEST /catAssetsModelTest: every OBJ of assets-models.txt built and compared with the game's mesh ("modeltest:")
extern char catAssetsModelTestOn;
extern char catAssetsModelDump[];   // /catAssetsModelDump <dir>
void catAssetsModelRunTest(void);

#endif
