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

#endif
