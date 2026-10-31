/*=============================================================================
    assettex.c: HWI-FEATURE(assets-folder) textures from the Assets folder (docs/features/assets-folder.md).

    Builds the .lif image texreg.c loads from a PNG (<name>.png), team-colour masks (<name>_teambase.png,
    <name>_teamstripe.png) and, for an atlas page, the changed source textures pasted at their places (Paco's
    placements, written by tools/assets/export_assets.py into assets-pages.txt). What a file does not give comes from
    the game's own .lif: the pixels, the team weights per pixel (a paletted .lif's team-effect tables looked up per
    pixel, as texreg.c trMeshSortListLoad colours a palette), the flags. Scaling is nearest-pixel. The index is
    src/Game/AssetsFolder.c.
=============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stb_image.h"              // the decoder (STB_IMAGE_IMPLEMENTATION is in screenshot.c)

#include "AssetsFolder.h"
#include "assettex.h"
#include "File.h"
#include "Memory.h"
#include "texreg.h"

typedef struct
{
    sdword width, height;
    ubyte *rgba;                    // width * height * 4: R, G, B, A
    ubyte *team[2];                 // width * height each: TeamColor0 / TeamColor1 weight
}
atimage;

typedef struct
{
    udword flags;
    sdword width, height;
    crc32 paletteCRC, imageCRC;
    bool32 paletted;
    ubyte *indices;                 // paletted: width * height (in raw)
    ubyte *palette;                 // paletted: 256 x R, G, B, A (in raw)
    ubyte *teamEffect[2];           // paletted: 256 each (in raw)
    void *raw;                      // the file as loaded (game heap)
}
atlif;

static udword atLE32(const ubyte *p)
{
    return (udword)p[0] | ((udword)p[1] << 8) | ((udword)p[2] << 16) | ((udword)p[3] << 24);
}

static void atImageFree(atimage *image)
{
    free(image->rgba);
    free(image->team[0]);
    free(image->team[1]);
    memset(image, 0, sizeof(atimage));
}

static void atImageAlloc(atimage *image, sdword width, sdword height)
{
    size_t n = (size_t)width * (size_t)height;

    image->width = width;
    image->height = height;
    image->rgba = malloc(n * 4);
    image->team[0] = calloc(n, 1);
    image->team[1] = calloc(n, 1);
}

// nearest pixel: an exact multiple repeats pixels, an exact divisor keeps every n-th one (as trImageScale)
static void atResample(ubyte *dst, sdword dw, sdword dh, const ubyte *src, sdword sw, sdword sh, sdword bpp)
{
    sdword x, y;

    for (y = 0; y < dh; y++)
    {
        const ubyte *row = src + (size_t)((sqword)y * sh / dh) * (size_t)sw * (size_t)bpp;
        ubyte *out = dst + (size_t)y * (size_t)dw * (size_t)bpp;

        for (x = 0; x < dw; x++)
        {
            memcpy(out + (size_t)x * (size_t)bpp, row + (size_t)((sqword)x * sw / dw) * (size_t)bpp, (size_t)bpp);
        }
    }
}

// image -> a new image of another size
static void atImageResize(atimage *image, sdword width, sdword height)
{
    atimage out;
    sdword i;

    if (image->width == width && image->height == height)
    {
        return;
    }
    atImageAlloc(&out, width, height);
    atResample(out.rgba, width, height, image->rgba, image->width, image->height, 4);
    for (i = 0; i < 2; i++)
    {
        atResample(out.team[i], width, height, image->team[i], image->width, image->height, 1);
    }
    atImageFree(image);
    *image = out;
}

// mirrored across the main diagonal (Paco's pacRotate): w x h -> h x w
static void atImageTranspose(atimage *image)
{
    atimage out;
    sdword x, y;

    atImageAlloc(&out, image->height, image->width);
    for (y = 0; y < image->height; y++)
    {
        for (x = 0; x < image->width; x++)
        {
            size_t s = (size_t)y * (size_t)image->width + (size_t)x, d = (size_t)x * (size_t)out.width + (size_t)y;

            memcpy(out.rgba + d * 4, image->rgba + s * 4, 4);
            out.team[0][d] = image->team[0][s];
            out.team[1][d] = image->team[1][s];
        }
    }
    atImageFree(image);
    *image = out;
}

static void atImagePaste(atimage *dst, const atimage *src, sdword x0, sdword y0)
{
    sdword y, w = src->width;

    if (x0 < 0 || y0 < 0 || x0 >= dst->width || y0 >= dst->height)
    {
        return;
    }
    w = min(w, dst->width - x0);
    for (y = 0; y < src->height && y0 + y < dst->height; y++)
    {
        size_t s = (size_t)y * (size_t)src->width, d = (size_t)(y0 + y) * (size_t)dst->width + (size_t)x0;

        memcpy(dst->rgba + d * 4, src->rgba + s * 4, (size_t)w * 4);
        memcpy(dst->team[0] + d, src->team[0] + s, (size_t)w);
        memcpy(dst->team[1] + d, src->team[1] + s, (size_t)w);
    }
}

// the game's own .lif (or, native, its replacement) and its pixels; docs/formats/textures.md
static bool32 atLifLoad(char *fileName, bool32 native, atlif *lif, atimage *image)
{
    sdword length = 0, n, i;
    ubyte *d;
    udword oData, oPal, oTeam[2];
    bool32 found;

    memset(lif, 0, sizeof(atlif));
    memset(image, 0, sizeof(atimage));
    if (!native)
    {
        assetsSuspend(TRUE);
    }
    found = fileExists(fileName, 0);
    if (found)
    {
        length = fileLoadAlloc(fileName, &lif->raw, 0);
    }
    if (!native)
    {
        assetsSuspend(FALSE);
    }
    if (!found)
    {
        printf("assets: %s not found in the game\n", fileName);
        return FALSE;
    }
    d = lif->raw;
    if (length < 0x30 || memcmp(d, LIF_FileIdentifier, 8) != 0 || atLE32(d + 8) != LIF_FileVersion)
    {
        printf("assets: %s is not a .lif (Willy 7, 0x104)\n", fileName);
        memFree(lif->raw);
        lif->raw = NULL;
        return FALSE;
    }
    lif->flags = atLE32(d + 12);
    lif->width = (sdword)atLE32(d + 16);
    lif->height = (sdword)atLE32(d + 20);
    lif->paletteCRC = atLE32(d + 24);
    lif->imageCRC = atLE32(d + 28);
    oData = atLE32(d + 32);
    oPal = atLE32(d + 36);
    oTeam[0] = atLE32(d + 40);
    oTeam[1] = atLE32(d + 44);
    n = lif->width * lif->height;
    lif->paletted = bitTest(lif->flags, TRF_Paletted) && !bitTest(lif->flags, TRF_Alpha);
    if (lif->width <= 0 || lif->height <= 0 || lif->width > AF_MaxSide || lif->height > AF_MaxSide ||
        (lif->paletted ? (oData + (udword)n > (udword)length || oPal + 1024 > (udword)length ||
                          oTeam[0] + 256 > (udword)length || oTeam[1] + 256 > (udword)length)
                       : (oData + 4 * (udword)n > (udword)length)))
    {
        printf("assets: %s: unexpected .lif layout\n", fileName);
        memFree(lif->raw);
        lif->raw = NULL;
        return FALSE;
    }
    atImageAlloc(image, lif->width, lif->height);
    if (lif->paletted)
    {
        lif->indices = d + oData;
        lif->palette = d + oPal;
        lif->teamEffect[0] = d + oTeam[0];
        lif->teamEffect[1] = d + oTeam[1];
        for (i = 0; i < n; i++)
        {
            ubyte index = lif->indices[i];

            memcpy(image->rgba + (size_t)i * 4, lif->palette + (size_t)index * 4, 4);
            image->team[0][i] = lif->teamEffect[0][index];  // both, as the paletted path colours by its list's flags
            image->team[1][i] = lif->teamEffect[1][index];
        }
    }
    else
    {
        memcpy(image->rgba, d + oData, (size_t)n * 4);
        for (i = 0; i < 2; i++)
        {
            if (bitTest(lif->flags, i == 0 ? TRF_TeamColor0 : TRF_TeamColor1) && oTeam[i] + (udword)n <= (udword)length)
            {
                memcpy(image->team[i], d + oTeam[i], (size_t)n);
            }
        }
    }
    return TRUE;
}

static bool32 atPngLoad(const char *path, sdword channels, ubyte **pixels, sdword *width, sdword *height)
{
    int w, h, n;
    stbi_uc *p = stbi_load(path, &w, &h, &n, (int)channels);

    if (p == NULL)
    {
        printf("assets: cannot read %s (%s); the game's own pixels are used\n", path, stbi_failure_reason());
        return FALSE;
    }
    *pixels = p;
    *width = w;
    *height = h;
    return TRUE;
}

// the pixels of a replaced texture at its load size; lif: the game's own (raw freed by the caller). shared: the
// registry's load, which draws the share parent's file for a texture textures.ll lists as a copy (texreg.c
// trRegistryRefresh); the export's PNG holds those pixels too
static bool32 atBuild(assetstexture *tex, atlif *lif, atimage *out, bool32 shared)
{
    char name[PATH_MAX];
    atimage orig;
    ubyte *pixels;
    sdword w, h, i;

    snprintf(name, sizeof(name), "%s.lif", (shared && tex->shareParent != NULL) ? tex->shareParent : tex->key);
    if (!atLifLoad(name, (tex->kinds & AF_Native) != 0, lif, &orig))
    {
        return FALSE;
    }
    assetsTextureSize(tex, lif->width, lif->height);
    if (tex->kinds & AF_Native)
    {
        *out = orig;                                        // a source replaced by a whole .lif
        return TRUE;
    }
    atImageAlloc(out, tex->outWidth, tex->outHeight);
    if ((tex->kinds & AF_Png) && atPngLoad(tex->pngPath, 4, &pixels, &w, &h))
    {
        atResample(out->rgba, out->width, out->height, pixels, w, h, 4);
        stbi_image_free(pixels);
    }
    else
    {
        atResample(out->rgba, out->width, out->height, orig.rgba, orig.width, orig.height, 4);
    }
    for (i = 0; i < 2; i++)
    {
        if ((tex->kinds & (i == 0 ? AF_TeamBase : AF_TeamStripe)) && atPngLoad(tex->teamPath[i], 1, &pixels, &w, &h))
        {
            atResample(out->team[i], out->width, out->height, pixels, w, h, 1);
            stbi_image_free(pixels);
        }
        else
        {
            atResample(out->team[i], out->width, out->height, orig.team[i], orig.width, orig.height, 1);
        }
    }
    atImageFree(&orig);
    if (tex->kinds & AF_Page)
    {
        sdword k = tex->pageScale;

        for (i = 0; i < tex->nPlaced; i++)
        {
            assetsplacement *p = &tex->placed[i];
            atlif sourceLif;
            atimage source;

            if (!atBuild(p->source, &sourceLif, &source, FALSE))   // Paco copied the source's own file
            {
                continue;
            }
            memFree(sourceLif.raw);
            if (p->transposed)
            {
                atImageTranspose(&source);
            }
            atImageResize(&source, p->w * k, p->h * k);
            atImagePaste(out, &source, p->x * k, p->y * k);
            atImageFree(&source);
        }
    }
    return TRUE;
}

static lifheader *atLifNew(atlif *orig, sdword width, sdword height, udword flags, size_t bytes, udword memFlags)
{
    lifheader *lif = memAlloc((sdword)(sizeof(lifheader) + bytes), "assetsLiF", memFlags & (NonVolatile | Pyrophoric));

    memset(lif, 0, sizeof(lifheader));
    strcpy(lif->ident, LIF_FileIdentifier);
    lif->version = LIF_FileVersion;
    lif->flags = (sdword)flags;
    lif->width = width;
    lif->height = height;
    lif->paletteCRC = orig->paletteCRC;
    lif->imageCRC = orig->imageCRC;
    lif->data = (ubyte *)(lif + 1);
    return lif;
}

// RGBA + both team maps
static lifheader *atLifRGBA(atlif *orig, atimage *image, udword flags, udword memFlags)
{
    size_t n = (size_t)image->width * (size_t)image->height;
    lifheader *lif = atLifNew(orig, image->width, image->height, flags, n * 6, memFlags);

    memcpy(lif->data, image->rgba, n * 4);
    lif->teamEffect0 = lif->data + n * 4;
    lif->teamEffect1 = lif->data + n * 5;
    lif->palette = NULL;
    memcpy(lif->teamEffect0, image->team[0], n);
    memcpy(lif->teamEffect1, image->team[1], n);
    return lif;
}

// the original's palette and team tables; a pixel the PNG left as it was keeps its index, any other takes the
// nearest palette colour (the team weight then follows that index, as in any paletted .lif)
static lifheader *atLifPaletted(atlif *orig, atimage *image, udword memFlags)
{
    size_t n = (size_t)image->width * (size_t)image->height, i;
    lifheader *lif = atLifNew(orig, image->width, image->height, orig->flags, n + 1024 + 512, memFlags);
    udword lastColour = 0;
    ubyte lastIndex = 0;
    bool32 haveLast = FALSE;

    lif->palette = (color *)(lif->data + n);
    lif->teamEffect0 = lif->data + n + 1024;
    lif->teamEffect1 = lif->data + n + 1024 + 256;
    memcpy(lif->palette, orig->palette, 1024);
    memcpy(lif->teamEffect0, orig->teamEffect[0], 256);
    memcpy(lif->teamEffect1, orig->teamEffect[1], 256);
    for (i = 0; i < n; i++)
    {
        const ubyte *c = image->rgba + i * 4;
        udword colour = atLE32(c);
        sdword j, best = 0;
        sqword bestDistance = -1;

        if (memcmp(c, orig->palette + (size_t)orig->indices[i] * 4, 4) == 0)
        {
            lif->data[i] = orig->indices[i];
            continue;
        }
        if (haveLast && colour == lastColour)
        {
            lif->data[i] = lastIndex;
            continue;
        }
        for (j = 0; j < 256; j++)
        {
            const ubyte *p = orig->palette + (size_t)j * 4;
            sqword d = 0;
            sdword k;

            for (k = 0; k < 4; k++)
            {
                d += (sqword)(c[k] - p[k]) * (sqword)(c[k] - p[k]);
            }
            if (bestDistance < 0 || d < bestDistance)
            {
                bestDistance = d;
                best = j;
            }
        }
        lif->data[i] = (ubyte)best;
        lastColour = colour;
        lastIndex = (ubyte)best;
        haveLast = TRUE;
    }
    return lif;
}

lifheader *assetsLifLoad(char *fileName, udword memFlags, bool32 registry)
{
    assetstexture *tex = assetsTextureFind(fileName);
    atlif orig;
    atimage image;
    lifheader *lif;
    udword maskFlags, flags;

    if (tex == NULL || (tex->kinds & AF_Native))
    {
        return NULL;                                        // a whole .lif comes through the file layer
    }
    if (!atBuild(tex, &orig, &image, registry))
    {
        return NULL;
    }
    maskFlags = ((tex->kinds & AF_TeamBase) ? TRF_TeamColor0 : 0) | ((tex->kinds & AF_TeamStripe) ? TRF_TeamColor1 : 0);
    if (registry)
    {
        // any size; RGB for a paletted texture (texreg.c then colours it with its list's team flags)
        flags = orig.paletted ? ((orig.flags & ~(udword)(TRF_Paletted | TRF_TeamColor0 | TRF_TeamColor1)) | maskFlags)
                              : (orig.flags | maskFlags);
        lif = atLifRGBA(&orig, &image, flags, memFlags);
    }
    else
    {
        // drawn by code that expects the original: its size and its format
        atImageResize(&image, orig.width, orig.height);
        lif = orig.paletted ? atLifPaletted(&orig, &image, memFlags) : atLifRGBA(&orig, &image, orig.flags | maskFlags, memFlags);
    }
    if (!tex->logged)
    {
        printf("assets: texture %s.lif %dx%d%s from%s%s%s%s (game's %dx%d %s)%s\n", tex->key, (int)lif->width,
               (int)lif->height, registry ? "" : " (kept at the game's size and format)",
               (tex->kinds & AF_Png) ? " png" : "", (tex->kinds & AF_TeamBase) ? " teambase" : "",
               (tex->kinds & AF_TeamStripe) ? " teamstripe" : "", (tex->kinds & AF_Page) ? " changed-sources" : "",
               (int)orig.width, (int)orig.height, orig.paletted ? "paletted" : "rgba",
               (tex->kinds & AF_Page) && tex->pageScale > 1 ? " page scaled up" : "");
        tex->logged = TRUE;
    }
    atImageFree(&image);
    memFree(orig.raw);
    return lif;
}
