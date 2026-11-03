/*=============================================================================
    AssetsModels.h: HWI-FEATURE(assets-models) ship models edited in Blender, read back into the game.

    An OBJ of the Assets folder's Models/ (written by the export, edited and saved by the player) is turned back into
    the game's mesh format, with the game's own mesh as the template: its objects (names, hierarchy, matrices), its
    materials and its per-vertex glow. docs/features/assets-models.md
=============================================================================*/

#ifndef ___ASSETSMODELS_H
#define ___ASSETSMODELS_H

#include <stddef.h>

#include "Types.h"

typedef struct
{
    sdword objects;                 // template objects that got faces
    sdword unknownObjects;          // OBJ objects the template does not have (their faces went to object 0)
    sdword faces;                   // OBJ faces read
    sdword triangles;               // polygons written
    sdword vertices;                // vertices written
    sdword newMaterials;            // materials the template does not have (or with another picture)
    sdword newPictures;             // pictures the game does not have
    sdword glowCopied;              // vertices whose glow entry came from the template
}
amstats;

// a picture the game does not have, named by an imported model: key = lower case game path without extension
// ("race_sect/sacolyte/rl0/lod0/hwi_mypaint"), path = the PNG on disk. Returns FALSE when it cannot be used
typedef bool32 (*amnewpicture)(const char *key, const char *pngPath);

// does the game have this file (case ignored; never a replacement from the Assets folder)
typedef bool32 (*amgamefile)(const char *path);

// The game mesh image (the on-disk .geo layout, "RMF97ba", 32-bit offsets, as meshLoad reads a file) built from the
// OBJ at objPath. folder: the Assets folder; meshName: the game path of the template mesh, whose folder holds the
// textures; tmpl: the template file's bytes. Returns a malloc'd image (length in *outLength) or NULL with the reason
// in why. Messages about the model are printed with the prefix "assets: model <objRel>: ".
ubyte *amObjToMesh(const char *folder, const char *objPath, const char *objRel, const char *meshName,
                   const ubyte *tmpl, sdword tmplLength, amnewpicture newPicture, amgamefile gameFile,
                   sdword *outLength, amstats *stats, char *why, size_t whySize);

// the round-trip check: an image built from the export's own OBJ against its template
typedef struct
{
    sdword objects, polygons;
    real64 maxPosition;             // largest corner position difference (game units, object space)
    real64 maxUV;                   // largest texture coordinate difference
    real64 maxNormal;               // largest normal difference (flat polygons: the face normal; smooth: the vertex's)
    real64 maxSmoothFace;           // largest face normal difference of smooth polygons (not drawn; information)
    sdword materialDiffers;         // polygons whose material differs (texture, flags or colours)
    sdword glowDiffers;             // corners whose glow entry differs
    sdword nanNormals;              // normals of the game's mesh that are not a number (not compared)
}
amcompare;

bool32 amCompare(const ubyte *tmpl, sdword tmplLength, const ubyte *image, sdword imageLength, amcompare *result,
                 char *why, size_t whySize);

#endif
