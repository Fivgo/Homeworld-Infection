/*=============================================================================
    assettex.h: HWI-FEATURE(assets-folder) textures from the Assets folder (docs/features/assets-folder.md)
=============================================================================*/

#ifndef ___ASSETTEX_H
#define ___ASSETTEX_H

#include "Types.h"
#include "texreg.h"

// a texture replaced by a PNG, team masks or a rebuilt atlas page, as a .lif in memory (memFree it), or NULL:
// load the file as usual. registry: the texture registry's load, which takes any size and RGBA for a paletted
// texture; any other caller gets the original's size and format (a paletted one is mapped to its own palette)
lifheader *assetsLifLoad(char *fileName, udword memFlags, bool32 registry);

// HWI-FEATURE(assets-models): the size and the .lif flags (TRF_Alpha when it has transparency, team flags for its
// masks) of a picture the game does not have (AF_New), as the registry measures a .lif
struct assetstexture;
bool32 assetsNewPictureMeasure(struct assetstexture *tex, sdword *width, sdword *height, udword *flags);

#endif
