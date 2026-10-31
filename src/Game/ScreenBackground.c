/*=============================================================================
    Name    : ScreenBackground.c
    Purpose : HWI-FEATURE(screen-background) (the user, 2026-10-31: "update ... the background for the video options
              menu in the main menu. Right now the UI scale and smooth motion settings are put to the side where they
              are in the complete black. Please provide the background image so I can edit it in gimp").

              A picture behind a front-end screen: <settings folder>/Assets/feman/backgrounds/<screen name>.png (the
              settings folder is fileUserSettingsPath; the screen names are the .fib's, e.g. Video_Options for the
              front end's Options -> Video and Video for the Escape menu's). It is mapped onto the screen's 640x480
              layout area, which the front end draws centred at the logical size (feResRepositionCentredX/Y), so it
              moves and scales with the UI scale and the window; its alpha shows the screen's own look through. Any
              size maps onto the area; 1280x960 (2x) stays sharp up to 200%.

              Where it is drawn (the screen's own order otherwise unchanged):
              - first, under the screen's base atom (the in-game screens' panel; the game's view behind);
              - again inside each of the screen's black background fills (atoms with FAF_Background: the front-end
                screens mask the space around their panels with them, drawn after the panel, so a picture drawn only
                before the atoms would stay hidden): the fill's black, then the picture clipped to the fill.
              Everything else (the panels, buttons, text) is drawn over it as before. Without the file nothing changes.
              The file is looked at each time the screen opens and once a second while it is shown: a new or changed
              picture shows without a restart, a deleted one goes.

              HWI-TEST: /catScreenBgExport <screen> (and /catScreenBgDir <dir>): the picture to paint on. Once <screen>
              is the top screen, four frames are drawn and read back over the layout area at the physical scale: the
              screen's art only (its panels, boxes and decorative pieces; no buttons, text, sliders, lists or user
              regions), then the whole screen, each once over black and once over white, which gives each pixel's
              alpha (a = 1 - (white - black)); the background fills draw the pass's colour, so they come out
              transparent. Written: <screen>-reference.png, <screen>-controls.png, <screen>.ora (OpenRaster, a zip
              GIMP opens with its layers: a hidden controls guide, an empty "Your paint" layer, the art) and
              README.txt, into <dir> (default: the settings folder's Assets/feman/backgrounds); then the engine quits.

    Created 2026-10-31 for Homeworld-Infection. Not Relic or Cataclysm code: the whole file is
    HWI-FEATURE(screen-background) (the export: HWI-TEST). This tree is REULA-licensed, see src/COPYING.REULA.
=============================================================================*/

#include "ScreenBackground.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <SDL2/SDL.h>

#include "stb_image.h"

#include "Color.h"
#include "Debug.h"
#include "FEFlow.h"
#include "File.h"
#include "glinc.h"
#include "main.h"
#include "prim2d.h"
#include "Region.h"
#include "render.h"
#include "texreg.h"
#include "utility.h"

// stb_image_write's PNG encoder (screenshot.c holds the implementation; the header declares only its file writers)
unsigned char *stbi_write_png_to_mem(const unsigned char *pixels, int stride_bytes, int x, int y, int n, int *out_len);

#define SBG_AREA_W          640                 // the .fib layout
#define SBG_AREA_H          480
#define SBG_PICTURES        8
#define SBG_LOOK_MS         1000                // the file is looked at again once a second while drawn
#define SBG_DIR             "Assets/feman/backgrounds/"

typedef struct
{
    char name[64];                              // the screen
    char path[PATH_MAX];
    time_t mtime;                               // the loaded file's time and size
    long long size;
    ubyte *pixels;                              // decoded (RGBA, top row first) until uploaded
    sdword width, height;
    GLuint tex;                                 // 0: not uploaded
    Uint32 looked;                              // SDL_GetTicks at the last look
    bool32 used;
}
sbgpicture;

static sbgpicture sbgPicture[SBG_PICTURES];

/*-----------------------------------------------------------------------------
    The file
-----------------------------------------------------------------------------*/
static void sbgPath(char *out, size_t size, const char *screenName)
{
    snprintf(out, size, "%s" SBG_DIR "%s.png", fileUserSettingsPath, screenName);
}

static void sbgDrop(sbgpicture *p)
{
    if (p->tex != 0)
    {
        glDeleteTextures(1, &p->tex);
        p->tex = 0;
    }
    if (p->pixels != NULL)
    {
        stbi_image_free(p->pixels);
        p->pixels = NULL;
    }
    p->mtime = 0;
    p->size = -1;
}

// the screen's picture, or NULL; <now>: look at the file now (else at most once a second)
static sbgpicture *sbgPictureGet(const char *name, bool32 now)
{
    sbgpicture *p = NULL, *spare = NULL;
    char path[PATH_MAX];
    struct stat st;
    sdword i;
    int w, h, n;

    for (i = 0; i < SBG_PICTURES; i++)
    {
        if (sbgPicture[i].used && strcmp(sbgPicture[i].name, name) == 0)
        {
            p = &sbgPicture[i];
            break;
        }
        if (!sbgPicture[i].used && spare == NULL)
        {
            spare = &sbgPicture[i];
        }
    }
    if (p != NULL && !now && SDL_GetTicks() - p->looked < SBG_LOOK_MS)
    {
        return (p->pixels != NULL || p->tex != 0) ? p : NULL;
    }
    sbgPath(path, sizeof(path), name);
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
    {
        if (p != NULL)
        {
            if (p->pixels != NULL || p->tex != 0)
            {
                dbgMessagef("screenbg: %s: %s gone, the screen's own look", name, path);
            }
            sbgDrop(p);
            p->looked = SDL_GetTicks();
        }
        return NULL;
    }
    if (p == NULL)
    {
        if (spare == NULL)
        {
            dbgMessagef("screenbg: %s: no room for another picture (%d)", name, SBG_PICTURES);
            return NULL;
        }
        p = spare;
        memset(p, 0, sizeof(*p));
        p->used = TRUE;
        p->size = -1;
        snprintf(p->name, sizeof(p->name), "%s", name);
    }
    p->looked = SDL_GetTicks();
    if (p->mtime == st.st_mtime && p->size == (long long)st.st_size && (p->pixels != NULL || p->tex != 0))
    {
        return p;                               // unchanged
    }
    sbgDrop(p);
    snprintf(p->path, sizeof(p->path), "%s", path);
    p->mtime = st.st_mtime;                     // a file that does not decode is not tried again until it changes
    p->size = (long long)st.st_size;
    p->pixels = stbi_load(path, &w, &h, &n, 4);
    if (p->pixels == NULL || w <= 0 || h <= 0)
    {
        dbgMessagef("screenbg: %s: %s does not load (%s)", name, path, stbi_failure_reason());
        if (p->pixels != NULL)
        {
            stbi_image_free(p->pixels);
            p->pixels = NULL;
        }
        return NULL;
    }
    p->width = w;
    p->height = h;
    dbgMessagef("screenbg: %s: %s %dx%d loaded", name, path, w, h);
    return p;
}

static bool32 sbgUpload(sbgpicture *p)
{
    if (p->tex != 0)
    {
        return TRUE;
    }
    if (p->pixels == NULL)
    {
        return FALSE;
    }
    glGenTextures(1, &p->tex);
    trClearCurrent();                           // the registry's bound-texture cache (as ferDraw does)
    glBindTexture(GL_TEXTURE_2D, p->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p->width, p->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, p->pixels);
    glGenerateMipmap(GL_TEXTURE_2D);            // any size: smooth when drawn smaller (2x at 100%)
    stbi_image_free(p->pixels);
    p->pixels = NULL;
    return TRUE;
}

void sbgDiscardGL(void)
{
    sdword i;

    for (i = 0; i < SBG_PICTURES; i++)
    {
        if (sbgPicture[i].used)
        {
            sbgDrop(&sbgPicture[i]);            // decoded again from the file at the next draw
            sbgPicture[i].looked = 0;
        }
    }
}

/*-----------------------------------------------------------------------------
    The screen's layout area and the draws
-----------------------------------------------------------------------------*/
// the screen an atom belongs to, among the screens shown
static fescreen *sbgScreenOf(featom *atom)
{
    sdword i;

    for (i = feStackIndex; i >= 0; i--)
    {
        fescreen *screen = feStack[i].screen;

        if (screen != NULL && atom >= screen->atoms && atom < screen->atoms + screen->nAtoms)
        {
            return screen;
        }
    }
    return NULL;
}

// the layout area in logical coordinates: the centring the loader gave the screen's atoms (x - loadedX of one not
// stretched as a background fill; 0 for the menus the front end does not centre)
static void sbgArea(fescreen *screen, rectangle *area)
{
    sdword i, ox = feResRepositionCentredX(0), oy = feResRepositionCentredY(0);

    for (i = 0; i < screen->nAtoms; i++)
    {
        if (!bitTest(screen->atoms[i].flags, FAF_Background))
        {
            ox = screen->atoms[i].x - screen->atoms[i].loadedX;
            oy = screen->atoms[i].y - screen->atoms[i].loadedY;
            break;
        }
    }
    area->x0 = ox;
    area->y0 = oy;
    area->x1 = ox + SBG_AREA_W;
    area->y1 = oy + SBG_AREA_H;
}

// the picture over <area>, the part inside <clip> (NULL: all of it)
static void sbgPictureDraw(sbgpicture *p, rectangle *area, rectangle *clip)
{
    rectangle r = *area;
    real32 u0, u1, v0, v1;
    GLboolean blendOn, alphaOn;
    sdword additive, oldTex;
    udword oldMode;

    if (clip != NULL)
    {
        r.x0 = max(r.x0, clip->x0);
        r.y0 = max(r.y0, clip->y0);
        r.x1 = min(r.x1, clip->x1);
        r.y1 = min(r.y1, clip->y1);
    }
    if (r.x1 <= r.x0 || r.y1 <= r.y0 || !sbgUpload(p))
    {
        return;
    }
    u0 = (real32)(r.x0 - area->x0) / (real32)SBG_AREA_W;
    u1 = (real32)(r.x1 - area->x0) / (real32)SBG_AREA_W;
    v0 = (real32)(r.y0 - area->y0) / (real32)SBG_AREA_H;   // the file's top row at the area's top
    v1 = (real32)(r.y1 - area->y0) / (real32)SBG_AREA_H;

    blendOn = glIsEnabled(GL_BLEND);
    alphaOn = glIsEnabled(GL_ALPHA_TEST);
    additive = rndAdditiveBlends(FALSE);        // normal alpha blending
    if (!blendOn) glEnable(GL_BLEND);
    if (alphaOn) glDisable(GL_ALPHA_TEST);
    trClearCurrent();
    glBindTexture(GL_TEXTURE_2D, p->tex);
    oldTex = rndTextureEnable(TRUE);
    oldMode = rndTextureEnvironment(RTE_Replace);
    {
        GLfloat t[8] = { u0, v0, u0, v1, u1, v0, u1, v1 };
        GLfloat v[8] = { primScreenToGLX(r.x0), primScreenToGLY(r.y0), primScreenToGLX(r.x0), primScreenToGLY(r.y1),
                         primScreenToGLX(r.x1), primScreenToGLY(r.y0), primScreenToGLX(r.x1), primScreenToGLY(r.y1) };

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(2, GL_FLOAT, 0, v);
        glTexCoordPointer(2, GL_FLOAT, 0, t);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    rndTextureEnvironment(oldMode);
    rndTextureEnable(oldTex);
    trClearCurrent();
    if (alphaOn) glEnable(GL_ALPHA_TEST);
    if (!blendOn) glDisable(GL_BLEND);
    rndAdditiveBlends(additive);
}

// HWI-TEST: the export's pass (-1 none): 0 / 1 the art over black / white, 2 / 3 the whole screen over black / white
static fescreen *sbgExportScreen = NULL;
static sdword sbgExportPass = -1;

static color sbgExportColour(void)
{
    return (sbgExportPass & 1) ? colRGB(255, 255, 255) : colRGB(0, 0, 0);
}

static void sbgSolid(rectangle *r, color c)
{
    sdword oldTex = rndTextureEnable(FALSE);

    primRectSolid2(r, c);
    rndTextureEnable(oldTex);
}

// the base region: the picture (an export pass: its colour) over the layout area, then the base atom as before
static void sbgBaseDraw(regionhandle region)
{
    fescreen *screen = sbgScreenOf((featom *)region->userID);
    sbgpicture *p;
    rectangle area;

    if (screen != NULL)
    {
        sbgArea(screen, &area);
        if (sbgExportPass >= 0 && screen == sbgExportScreen)
        {
            sbgSolid(&area, sbgExportColour());
        }
        else if ((p = sbgPictureGet(screen->name, FALSE)) != NULL)
        {
            sbgPictureDraw(p, &area, NULL);
        }
    }
    feBaseRegionDraw(region);
}

// a background fill: its black, then the picture inside it (an export pass: the pass's colour, read back as clear)
static void sbgFillDraw(regionhandle region)
{
    fescreen *screen = sbgScreenOf((featom *)region->userID);
    sbgpicture *p;
    rectangle area;

    if (screen != NULL && sbgExportPass >= 0 && screen == sbgExportScreen)
    {
        sbgSolid(&region->rect, sbgExportColour());
        return;
    }
    feStaticRectangleDraw(region);
    if (screen != NULL && (p = sbgPictureGet(screen->name, FALSE)) != NULL)
    {
        sbgArea(screen, &area);
        sbgPictureDraw(p, &area, &region->rect);
    }
}

static char sbgExportName[64] = "";

void sbgRegionsAdded(fescreen *screen, regionhandle baseRegion)
{
    sdword i;
    bool32 exporting = (sbgExportName[0] != '\0' && strcmp(screen->name, sbgExportName) == 0);

    if (baseRegion == NULL || (sbgPictureGet(screen->name, TRUE) == NULL && !exporting))
    {
        return;                                 // no picture: the screen as it was
    }
    regDrawFunctionSet(baseRegion, sbgBaseDraw);
    for (i = 1; i < screen->nAtoms; i++)
    {
        featom *atom = &screen->atoms[i];
        regionhandle region = (regionhandle)atom->region;

        if (bitTest(atom->flags, FAF_Background) && region != NULL && region->drawFunction == feStaticRectangleDraw)
        {
            regDrawFunctionSet(region, sbgFillDraw);
        }
    }
}

/*=============================================================================
    HWI-TEST: the export (/catScreenBgExport <screen>, /catScreenBgDir <dir>)
=============================================================================*/
#define SBG_EXPORT_SETTLE   30                  // frames the screen is shown before the passes
#define SBG_SAVED_DRAWS     1024

static char sbgExportDir[PATH_MAX] = "";
static sdword sbgExportStep = -2;               // -2 off, -1 waiting for the screen, 0..3 the pass drawn, 4 done
static sdword sbgExportWait = 0;
static ubyte *sbgExportRGB[4];                  // the passes' read-backs (RGB, bottom row first)
static sdword sbgExportW = 0, sbgExportH = 0;
static regionhandle sbgSavedRegion[SBG_SAVED_DRAWS];
static regiondrawfunction sbgSavedDraw[SBG_SAVED_DRAWS];
static sdword sbgSaved = 0;

void sbgExportSet(char *screenName)
{
    snprintf(sbgExportName, sizeof(sbgExportName), "%s", screenName);
    sbgExportStep = -1;
    sbgExportWait = 0;
}

void sbgExportDirSet(char *dir)
{
    snprintf(sbgExportDir, sizeof(sbgExportDir), "%s", dir);
}

// the art: the regions of plain boxes, decorative pieces, cutouts and dividers, and the background fills (they draw the
// pass colour); everything that is a control, text or a user region is not drawn in the art passes (the front end's
// link buttons carry FAF_Background too: they are buttons)
static bool32 sbgArtAtom(regionhandle region)
{
    featom *atom = region->atom;

    if (region->drawFunction == sbgFillDraw)
    {
        return TRUE;
    }
    if (atom == NULL)
    {
        return FALSE;
    }
    switch (atom->type)
    {
        case FA_UserRegion:
        case FA_StaticText:
        case FA_Button:
        case FA_CheckBox:
        case FA_ToggleButton:
        case FA_ScrollBar:
        case FA_StatusBar:
        case FA_TextEntry:
        case FA_ListViewExpandButton:
        case FA_MenuItem:
        case FA_RadioButton:
        case FA_ListWindow:
        case FA_BitmapButton:
        case FA_HorizSlider:
        case FA_VertSlider:
        case FA_DragButton:
            return FALSE;
        default:
            return TRUE;
    }
}

static void sbgArtOnly(regionhandle region, bool32 hide)
{
    for (; region != NULL; region = region->next)
    {
        bool32 off = hide || !sbgArtAtom(region);

        if (off && sbgSaved < SBG_SAVED_DRAWS)
        {
            sbgSavedRegion[sbgSaved] = region;
            sbgSavedDraw[sbgSaved++] = regDrawFunctionSet(region, NULL);
        }
        sbgArtOnly(region->child, off);
    }
}

static void sbgArtRestore(void)
{
    while (sbgSaved > 0)
    {
        sbgSaved--;
        regDrawFunctionSet(sbgSavedRegion[sbgSaved], sbgSavedDraw[sbgSaved]);
    }
}

// the layout area in physical pixels (x from the left, y from the bottom: glReadPixels)
static bool32 sbgExportRect(sdword *x, sdword *y, sdword *w, sdword *h)
{
    rectangle area;
    real32 sx = (real32)MAIN_WindowWidthActual / (real32)MAIN_WindowWidth;
    real32 sy = (real32)MAIN_WindowHeightActual / (real32)MAIN_WindowHeight;

    sbgArea(sbgExportScreen, &area);
    *x = (sdword)(area.x0 * sx + 0.5f);
    *w = (sdword)(SBG_AREA_W * sx + 0.5f);
    *h = (sdword)(SBG_AREA_H * sy + 0.5f);
    *y = MAIN_WindowHeightActual - (sdword)(area.y0 * sy + 0.5f) - *h;
    return *x >= 0 && *y >= 0 && *w > 0 && *h > 0 && *x + *w <= MAIN_WindowWidthActual && *y + *h <= MAIN_WindowHeightActual;
}

// over black and over white -> RGBA, top row first: a = 255 - (white - black), colour = black / a
static ubyte *sbgCompose(ubyte *black, ubyte *white, sdword w, sdword h)
{
    ubyte *out = malloc((size_t)w * (size_t)h * 4);
    sdword x, y, c;

    if (out == NULL)
    {
        return NULL;
    }
    for (y = 0; y < h; y++)
    {
        ubyte *b = black + (size_t)(h - 1 - y) * (size_t)w * 3, *wh = white + (size_t)(h - 1 - y) * (size_t)w * 3;
        ubyte *o = out + (size_t)y * (size_t)w * 4;

        for (x = 0; x < w; x++, b += 3, wh += 3, o += 4)
        {
            sdword d = ((sdword)wh[0] - b[0] + (sdword)wh[1] - b[1] + (sdword)wh[2] - b[2] + 1) / 3;
            sdword a = 255 - max(0, min(255, d));

            for (c = 0; c < 3; c++)
            {
                o[c] = (ubyte)((a > 0) ? min(255, (b[c] * 255 + a / 2) / a) : 0);
            }
            o[3] = (ubyte)a;
        }
    }
    return out;
}

// the thumbnail OpenRaster asks for: at most 256 on a side, each pixel the mean of a block
static ubyte *sbgThumb(ubyte *rgba, sdword w, sdword h, sdword *tw, sdword *th)
{
    sdword f = (max(w, h) + 255) / 256, x, y, i, j, c;
    ubyte *out;

    *tw = w / f;
    *th = h / f;
    if (*tw <= 0 || *th <= 0 || (out = malloc((size_t)*tw * (size_t)*th * 4)) == NULL)
    {
        return NULL;
    }
    for (y = 0; y < *th; y++)
    {
        for (x = 0; x < *tw; x++)
        {
            for (c = 0; c < 4; c++)
            {
                udword sum = 0;

                for (j = 0; j < f; j++)
                {
                    for (i = 0; i < f; i++)
                    {
                        sum += rgba[((size_t)(y * f + j) * (size_t)w + (size_t)(x * f + i)) * 4 + c];
                    }
                }
                out[((size_t)y * (size_t)*tw + (size_t)x) * 4 + c] = (ubyte)(sum / (udword)(f * f));
            }
        }
    }
    return out;
}

static udword sbgCrc(const ubyte *data, size_t n)
{
    static udword table[256];
    static bool32 made = FALSE;
    udword crc = 0xffffffffu, k;
    size_t i;

    if (!made)
    {
        for (i = 0; i < 256; i++)
        {
            udword v = (udword)i;

            for (k = 0; k < 8; k++)
            {
                v = (v & 1) ? (0xedb88320u ^ (v >> 1)) : (v >> 1);
            }
            table[i] = v;
        }
        made = TRUE;
    }
    for (i = 0; i < n; i++)
    {
        crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    }
    return crc ^ 0xffffffffu;
}

static void sbgPut16(FILE *f, udword v) { fputc((int)(v & 0xff), f); fputc((int)((v >> 8) & 0xff), f); }
static void sbgPut32(FILE *f, udword v) { sbgPut16(f, v & 0xffff); sbgPut16(f, v >> 16); }

typedef struct
{
    const char *name;
    const ubyte *data;
    size_t size;
}
sbgzipentry;

// a zip of stored entries (OpenRaster: "mimetype" first, stored, no extra field)
static bool32 sbgZipWrite(const char *path, sbgzipentry *entry, sdword n)
{
    FILE *f = fopen(path, "wb");
    udword offset[8], crc[8], dosTime, dosDate, cdStart, cdSize;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    sdword i;

    if (f == NULL || n > 8)
    {
        if (f != NULL) fclose(f);
        return FALSE;
    }
    dosTime = (t != NULL) ? (udword)((t->tm_hour << 11) | (t->tm_min << 5) | (t->tm_sec / 2)) : 0;
    dosDate = (t != NULL) ? (udword)(((t->tm_year - 80) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday) : 0x21;
    for (i = 0; i < n; i++)
    {
        offset[i] = (udword)ftell(f);
        crc[i] = sbgCrc(entry[i].data, entry[i].size);
        sbgPut32(f, 0x04034b50); sbgPut16(f, 20); sbgPut16(f, 0); sbgPut16(f, 0);
        sbgPut16(f, dosTime); sbgPut16(f, dosDate); sbgPut32(f, crc[i]);
        sbgPut32(f, (udword)entry[i].size); sbgPut32(f, (udword)entry[i].size);
        sbgPut16(f, (udword)strlen(entry[i].name)); sbgPut16(f, 0);
        fwrite(entry[i].name, 1, strlen(entry[i].name), f);
        fwrite(entry[i].data, 1, entry[i].size, f);
    }
    cdStart = (udword)ftell(f);
    for (i = 0; i < n; i++)
    {
        sbgPut32(f, 0x02014b50); sbgPut16(f, 20); sbgPut16(f, 20); sbgPut16(f, 0); sbgPut16(f, 0);
        sbgPut16(f, dosTime); sbgPut16(f, dosDate); sbgPut32(f, crc[i]);
        sbgPut32(f, (udword)entry[i].size); sbgPut32(f, (udword)entry[i].size);
        sbgPut16(f, (udword)strlen(entry[i].name)); sbgPut16(f, 0); sbgPut16(f, 0);
        sbgPut16(f, 0); sbgPut16(f, 0); sbgPut32(f, 0); sbgPut32(f, offset[i]);
        fwrite(entry[i].name, 1, strlen(entry[i].name), f);
    }
    cdSize = (udword)ftell(f) - cdStart;
    sbgPut32(f, 0x06054b50); sbgPut16(f, 0); sbgPut16(f, 0); sbgPut16(f, (udword)n); sbgPut16(f, (udword)n);
    sbgPut32(f, cdSize); sbgPut32(f, cdStart); sbgPut16(f, 0);
    return (fclose(f) == 0) ? TRUE : FALSE;
}

static bool32 sbgFileWrite(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    bool32 ok;

    if (f == NULL)
    {
        return FALSE;
    }
    ok = (fwrite(data, 1, size, f) == size) ? TRUE : FALSE;
    return (fclose(f) == 0) ? ok : FALSE;
}

static const char sbgReadme[] =
    "Homeworld-Infection: your own picture behind a menu screen\n"
    "==========================================================\n"
    "\n"
    "These files are made by the game from its own art (the Video options screens):\n"
    "  Video_Options.ora             Options > Video in the main menu, for GIMP (with layers)\n"
    "  Video.ora                     the same screen in the Escape menu during a game\n"
    "  <screen>-reference.png        the screen's panels at 2x (1280x960), transparent around them\n"
    "  <screen>-controls.png         its buttons, text and sliders (a guide only)\n"
    "\n"
    "To paint a background\n"
    "  1. Open Video_Options.ora in GIMP (File > Open). It has three layers:\n"
    "       Controls guide   hidden; switch it on to see where the buttons and text sit\n"
    "       Your paint       empty: paint here\n"
    "       Game art         the screen's panels (reference)\n"
    "  2. Paint on \"Your paint\". The game draws its own panels, buttons and text over your\n"
    "     picture, so paint where the screen is empty: the black space around the panel, e.g.\n"
    "     right of it, under the AUDIO / VIDEO / GAMEPLAY / KEY BINDINGS tabs.\n"
    "  3. Hide \"Controls guide\" (leaving \"Game art\" visible or not makes no difference:\n"
    "     the game draws the same art over it). Then File > Export As..., type the name\n"
    "     Video_Options.png, choose this folder, click Export, and Export again in the PNG\n"
    "     options (transparent parts stay transparent).\n"
    "  4. Start the game, or open the Video screen again if it is running: your picture is\n"
    "     behind the screen, and it moves and scales with the UI scale. Where it is\n"
    "     transparent the screen looks as before (black in the main menu, the game in the\n"
    "     Escape menu). For the Escape menu's screen, export as Video.png.\n"
    "\n"
    "To go back to the original look, delete (or rename) Video_Options.png / Video.png.\n"
    "\n"
    "Any picture size works (it is stretched over the screen's 640x480 layout); 1280x960 stays\n"
    "sharp at every UI scale up to 200%. Any other screen works the same way: <screen name>.png,\n"
    "the name as the game's .fib files call the screen.\n"
    "\n"
    "To make these files again (developers): tools/m2_harness.sh with the engine switches\n"
    "/catScreenBgExport <screen> /catScreenBgDir <this folder> (docs/features/screen-background.md).\n";

static void sbgExportWrite(void)
{
    sdword w = sbgExportW, h = sbgExportH, tw = 0, th = 0, i;
    ubyte *art = sbgCompose(sbgExportRGB[0], sbgExportRGB[1], w, h);
    ubyte *ctl = sbgCompose(sbgExportRGB[2], sbgExportRGB[3], w, h);
    ubyte *clear = calloc((size_t)w * (size_t)h, 4);
    ubyte *thumb = (art != NULL) ? sbgThumb(art, w, h, &tw, &th) : NULL;
    unsigned char *png[4] = { NULL, NULL, NULL, NULL };
    int len[4] = { 0, 0, 0, 0 };
    char dir[PATH_MAX], path[PATH_MAX + 128], stack[1536];
    bool32 ok = FALSE;

    if (sbgExportDir[0] != '\0')
    {
        snprintf(dir, sizeof(dir), "%s/", sbgExportDir);
    }
    else
    {
        snprintf(dir, sizeof(dir), "%s" SBG_DIR, fileUserSettingsPath);
    }
    fileMakeDirectory(dir);
    if (art != NULL && ctl != NULL && clear != NULL && thumb != NULL)
    {
        png[0] = stbi_write_png_to_mem(art, w * 4, w, h, 4, &len[0]);
        png[1] = stbi_write_png_to_mem(ctl, w * 4, w, h, 4, &len[1]);
        png[2] = stbi_write_png_to_mem(clear, w * 4, w, h, 4, &len[2]);
        png[3] = stbi_write_png_to_mem(thumb, tw * 4, tw, th, 4, &len[3]);
    }
    if (png[0] != NULL && png[1] != NULL && png[2] != NULL && png[3] != NULL)
    {
        sbgzipentry entry[6];
        static const char mime[] = "image/openraster";

        snprintf(stack, sizeof(stack),
                 "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                 "<image version=\"0.0.5\" w=\"%d\" h=\"%d\" xres=\"144\" yres=\"144\">\n"
                 " <stack>\n"
                 "  <layer name=\"Controls guide (hide before exporting)\" src=\"data/controls.png\" x=\"0\" y=\"0\" opacity=\"1.000\" visibility=\"hidden\" composite-op=\"svg:src-over\"/>\n"
                 "  <layer name=\"Your paint\" src=\"data/paint.png\" x=\"0\" y=\"0\" opacity=\"1.000\" visibility=\"visible\" composite-op=\"svg:src-over\"/>\n"
                 "  <layer name=\"Game art (reference)\" src=\"data/reference.png\" x=\"0\" y=\"0\" opacity=\"1.000\" visibility=\"visible\" composite-op=\"svg:src-over\"/>\n"
                 " </stack>\n"
                 "</image>\n", (int)w, (int)h);
        entry[0].name = "mimetype";                  entry[0].data = (const ubyte *)mime;  entry[0].size = strlen(mime);
        entry[1].name = "stack.xml";                 entry[1].data = (const ubyte *)stack; entry[1].size = strlen(stack);
        entry[2].name = "data/controls.png";         entry[2].data = png[1];               entry[2].size = (size_t)len[1];
        entry[3].name = "data/paint.png";            entry[3].data = png[2];               entry[3].size = (size_t)len[2];
        entry[4].name = "data/reference.png";        entry[4].data = png[0];               entry[4].size = (size_t)len[0];
        entry[5].name = "mergedimage.png";           entry[5].data = png[0];               entry[5].size = (size_t)len[0];
        ok = TRUE;
        snprintf(path, sizeof(path), "%s%s-reference.png", dir, sbgExportName);
        ok = sbgFileWrite(path, png[0], (size_t)len[0]) && ok;
        snprintf(path, sizeof(path), "%s%s-controls.png", dir, sbgExportName);
        ok = sbgFileWrite(path, png[1], (size_t)len[1]) && ok;
        snprintf(path, sizeof(path), "%sREADME.txt", dir);
        ok = sbgFileWrite(path, sbgReadme, strlen(sbgReadme)) && ok;
        {
            sbgzipentry all[7];

            for (i = 0; i < 6; i++)
            {
                all[i] = entry[i];
            }
            all[6].name = "Thumbnails/thumbnail.png"; all[6].data = png[3]; all[6].size = (size_t)len[3];
            snprintf(path, sizeof(path), "%s%s.ora", dir, sbgExportName);
            ok = sbgZipWrite(path, all, 7) && ok;
        }
    }
    dbgMessagef("screenbgexport: %s %dx%d (ui %d%%: logical %dx%d, physical %dx%d) dir=%s files=%s-reference.png,%s-controls.png,%s.ora,README.txt "
                "bytes=%d/%d/%d/%d result=%s", sbgExportName, (int)w, (int)h, (int)(MAIN_WindowWidthActual * 100 / MAIN_WindowWidth),
                (int)MAIN_WindowWidth, (int)MAIN_WindowHeight, (int)MAIN_WindowWidthActual, (int)MAIN_WindowHeightActual, dir,
                sbgExportName, sbgExportName, sbgExportName, len[0], len[1], len[2], len[3], ok ? "PASS" : "FAIL");
    for (i = 0; i < 4; i++)
    {
        free(png[i]);
    }
    free(art);
    free(ctl);
    free(clear);
    free(thumb);
}

void sbgExportFrameEnd(void)
{
    fescreen *top = (feStackIndex >= 0) ? feStack[feStackIndex].screen : NULL;
    sdword x, y, w, h;

    if (sbgExportStep < -1 || sbgExportStep > 3)
    {
        return;
    }
    if (top == NULL || strcmp(top->name, sbgExportName) != 0)
    {
        if (sbgExportStep >= 0)
        {
            dbgMessagef("screenbgexport: %s: the screen went before the passes ended result=FAIL", sbgExportName);
            sbgArtRestore();
            sbgExportPass = -1;
            sbgExportStep = 4;
        }
        sbgExportWait = 0;
        return;
    }
    if (sbgExportStep == -1)
    {
        if (++sbgExportWait < SBG_EXPORT_SETTLE)
        {
            return;
        }
        sbgExportScreen = top;
        if (!sbgExportRect(&x, &y, &w, &h))
        {
            dbgMessagef("screenbgexport: %s: the layout area is not inside the window result=FAIL", sbgExportName);
            sbgExportStep = 4;
            return;
        }
        sbgExportW = w;
        sbgExportH = h;
        sbgArtOnly(feStack[feStackIndex].baseRegion->child, FALSE);   // the art passes: no controls drawn
        sbgExportStep = 0;
        sbgExportPass = 0;
        return;
    }
    // the frame of pass sbgExportStep was just drawn
    if (!sbgExportRect(&x, &y, &w, &h) || w != sbgExportW || h != sbgExportH ||
        (sbgExportRGB[sbgExportStep] == NULL && (sbgExportRGB[sbgExportStep] = malloc((size_t)w * (size_t)h * 3)) == NULL))
    {
        dbgMessagef("screenbgexport: %s: the read-back failed result=FAIL", sbgExportName);
        sbgArtRestore();
        sbgExportPass = -1;
        sbgExportStep = 4;
        return;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, sbgExportRGB[sbgExportStep]);
    sbgExportStep++;
    if (sbgExportStep == 2)
    {
        sbgArtRestore();                        // the controls passes: the whole screen
    }
    if (sbgExportStep < 4)
    {
        sbgExportPass = sbgExportStep;
        return;
    }
    sbgExportPass = -1;
    sbgExportWrite();
    for (x = 0; x < 4; x++)
    {
        free(sbgExportRGB[x]);
        sbgExportRGB[x] = NULL;
    }
    utyGameQuit(NULL, NULL);
}
