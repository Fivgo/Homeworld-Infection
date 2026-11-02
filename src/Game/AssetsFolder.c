/*=============================================================================
    AssetsFolder.c: HWI-FEATURE(assets-folder) the moddable Assets folder (docs/features/assets-folder.md).

    At start-up the folder is indexed once, by name, size and time only. A file is ACTIVE when the export's manifest
    (assets-manifest.txt) does not list it or lists another size or time; an untouched export has no active file and
    changes nothing. An active file is used when its path (case ignored) names a game file, or when it is a PNG of a
    .lif texture (<name>.png, <name>_teambase.png, <name>_teamstripe.png). Atlas pages whose source textures changed
    are rebuilt from assets-pages.txt (the export writes it). An edited ship model (an OBJ of Models/ that
    assets-models.txt names, HWI-FEATURE(assets-models)) replaces its mesh, built when the game loads that mesh
    (src/Game/AssetsModels.c). Everything else is ignored.

    Memory comes from malloc, not the game heap, so a run without active files has the same heap as a run without
    the folder. The pixels are built in src/SDL/assettex.c.
=============================================================================*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <dirent.h>
#endif

#include <SDL2/SDL.h>

#include "AssetsFolder.h"
#include "AssetsModels.h"       // HWI-FEATURE(assets-models)
#include "File.h"
#include "Memory.h"

#define AF_FolderName       "Assets"
#define AF_ManifestName     "assets-manifest.txt"
#define AF_PagesName        "assets-pages.txt"
#define AF_ReadmeName       "README.txt"
#define AF_ModelsName       "assets-models.txt"     // HWI-FEATURE(assets-models)
#define AF_MaxIgnoredLogged 20
#define AF_MaxDepth         24              // folder levels scanned (a symbolic-link loop ends here)

/*=============================================================================
    Data:
=============================================================================*/

typedef struct
{
    char *key;
    void *value;
}
afslot;

typedef struct
{
    afslot *slot;
    udword mask;
    udword count;
}
afhash;

typedef struct
{
    uqword size;
    sqword mtime;
}
afmanifestentry;

typedef struct
{
    char *key;                      // lower case relative path
    char *rel;                      // relative path as on disk
    char *path;                     // full path
    bool32 logged;
}
affile;

static char afFolder[PATH_MAX] = "";
static char afPathSwitch[PATH_MAX] = "";
static bool32 afOff = FALSE;
static sdword afSuspended = 0;

static afhash afManifest = {NULL, 0, 0};    // during the scan only
static afmanifestentry *afManifestEntries = NULL;
static char *afManifestText = NULL;

static affile *afFiles = NULL;              // active files
static sdword afFilesN = 0, afFilesMax = 0;

static afhash afNative = {NULL, 0, 0};      // key -> affile: replacements of game files by path
static afhash afTextures = {NULL, 0, 0};    // texture key -> assetstexture
static sdword afActive = 0;

static sdword afScanned = 0, afUnchanged = 0, afFolders = 0, afIgnored = 0;

// HWI-FEATURE(assets-models): an edited OBJ and the game mesh it replaces
typedef struct
{
    char *ship;                     // race_x/ship
    sdword level;
    char *meshName;                 // the template: the source mesh (main OBJ) or the in-game mesh (ingame/ OBJ)
    char *objRel;                   // Models/.../<ship>_lodN.obj
    char *objPath;
    sdword state;                   // 0: not built yet, 1: built, -1: cannot be used
    ubyte *image;                   // the mesh image (malloc)
    sdword length;
}
afmodel;

typedef struct
{
    char *ship;
    sdword level;
    char *base;                     // the mesh path without its extension (key)
    afmodel *model;                 // edited, else NULL
}
afmodelline;

static afhash afModels = {NULL, 0, 0};      // mesh base key -> afmodel (edited, or the level it stands in for)
static afmodel **afModelList = NULL;
static sdword afModelN = 0, afModelStandIns = 0;
static afmodelline *afModelLines = NULL;
static sdword afModelLineN = 0;
static afhash afModelLogged = {NULL, 0, 0}; // base keys whose stand-in use was logged

/*=============================================================================
    Helpers:
=============================================================================*/

// FNV-1a
static udword afHashString(const char *s)
{
    udword h = 2166136261u;

    while (*s)
    {
        h ^= (ubyte)*s++;
        h *= 16777619u;
    }
    return h;
}

static void afHashInit(afhash *hash, udword minimum)
{
    udword size = 64;

    while (size < minimum * 2)
    {
        size <<= 1;
    }
    hash->slot = calloc(size, sizeof(afslot));
    hash->mask = size - 1;
    hash->count = 0;
}

static void afHashFree(afhash *hash)
{
    free(hash->slot);
    hash->slot = NULL;
    hash->mask = 0;
    hash->count = 0;
}

static void *afHashGet(afhash *hash, const char *key)
{
    udword i;

    if (hash->slot == NULL)
    {
        return NULL;
    }
    for (i = afHashString(key) & hash->mask; hash->slot[i].key != NULL; i = (i + 1) & hash->mask)
    {
        if (strcmp(hash->slot[i].key, key) == 0)
        {
            return hash->slot[i].value;
        }
    }
    return NULL;
}

static void afHashPut(afhash *hash, char *key, void *value)
{
    udword i;

    if (hash->slot == NULL)
    {
        afHashInit(hash, 32);
    }
    if ((hash->count + 1) * 2 > hash->mask + 1)
    {
        afhash bigger;
        udword j;

        afHashInit(&bigger, hash->count + 1);
        for (j = 0; j <= hash->mask; j++)
        {
            if (hash->slot[j].key != NULL)
            {
                afHashPut(&bigger, hash->slot[j].key, hash->slot[j].value);
            }
        }
        afHashFree(hash);
        *hash = bigger;
    }
    for (i = afHashString(key) & hash->mask; hash->slot[i].key != NULL; i = (i + 1) & hash->mask)
    {
        if (strcmp(hash->slot[i].key, key) == 0)
        {
            hash->slot[i].value = value;
            return;
        }
    }
    hash->slot[i].key = key;
    hash->slot[i].value = value;
    hash->count++;
}

static char *afStrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = malloc(n);

    memcpy(d, s, n);
    return d;
}

// a game path as a key: lower case, '/' separators, single slashes, no leading "./" or '/'
static void afKey(const char *in, char *out, size_t size)
{
    size_t n = 0;
    char ch, prev = '/';

    while (in[0] == '.' && (in[1] == '/' || in[1] == '\\'))
    {
        in += 2;
    }
    for (; (ch = *in) != 0 && n + 1 < size; in++)
    {
        if (ch == '\\')
        {
            ch = '/';
        }
        if (ch == '/' && prev == '/')
        {
            continue;
        }
        out[n++] = (char)tolower((ubyte)ch);
        prev = ch;
    }
    out[n] = 0;
}

static bool32 afEndsWith(const char *s, const char *end)
{
    size_t n = strlen(s), m = strlen(end);

    return n >= m && strcmp(s + n - m, end) == 0;
}

static udword afBE32(const ubyte *p)
{
    return ((udword)p[0] << 24) | ((udword)p[1] << 16) | ((udword)p[2] << 8) | (udword)p[3];
}

static udword afLE32(const ubyte *p)
{
    return (udword)p[0] | ((udword)p[1] << 8) | ((udword)p[2] << 16) | ((udword)p[3] << 24);
}

// the size in a PNG's IHDR
static bool32 afPngSize(const char *path, sdword *width, sdword *height)
{
    ubyte head[24];
    FILE *f = fopen(path, "rb");
    size_t n;

    if (f == NULL)
    {
        return FALSE;
    }
    n = fread(head, 1, sizeof(head), f);
    fclose(f);
    if (n != sizeof(head) || memcmp(head, "\x89PNG\r\n\x1a\n", 8) != 0 || memcmp(head + 12, "IHDR", 4) != 0)
    {
        return FALSE;
    }
    *width = (sdword)afBE32(head + 16);
    *height = (sdword)afBE32(head + 20);
    return *width > 0 && *height > 0 && *width <= AF_MaxSide && *height <= AF_MaxSide;
}

// the size in a .lif header ("Willy 7", docs/formats/textures.md)
static bool32 afLifSize(const char *path, sdword *width, sdword *height)
{
    ubyte head[0x30];
    FILE *f = fopen(path, "rb");
    size_t n;

    if (f == NULL)
    {
        return FALSE;
    }
    n = fread(head, 1, sizeof(head), f);
    fclose(f);
    if (n != sizeof(head) || memcmp(head, "Willy 7", 8) != 0)
    {
        return FALSE;
    }
    *width = (sdword)afLE32(head + 16);
    *height = (sdword)afLE32(head + 20);
    return *width > 0 && *height > 0 && *width <= AF_MaxSide && *height <= AF_MaxSide;
}

static char *afReadText(const char *path)
{
    FILE *f = fopen(path, "rb");
    long length;
    char *text;

    if (f == NULL)
    {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (length < 0)
    {
        fclose(f);
        return NULL;
    }
    text = malloc((size_t)length + 1);
    if (fread(text, 1, (size_t)length, f) != (size_t)length)
    {
        free(text);
        fclose(f);
        return NULL;
    }
    fclose(f);
    text[length] = 0;
    return text;
}

// does the game have this file (archives or the game folder)? never a replacement
static bool32 afGameFileExists(char *key)
{
    bool32 found;

    assetsSuspend(TRUE);
    found = fileExists(key, 0);
    assetsSuspend(FALSE);
    return found;
}

static assetstexture *afTextureGet(const char *key)
{
    assetstexture *tex = afHashGet(&afTextures, key);

    if (tex == NULL)
    {
        tex = calloc(1, sizeof(assetstexture));
        tex->key = afStrdup(key);
        tex->pageScale = 1;
        afHashPut(&afTextures, tex->key, tex);
    }
    return tex;
}

/*=============================================================================
    The manifest and the scan:
=============================================================================*/

// assets-manifest.txt: "<size> <mtime> <sha256> <path>" per line, '#' comments (tools/assets/export_assets.py)
static sdword afManifestLoad(void)
{
    char path[PATH_MAX];
    char *line, *next, *p, *end;
    sdword lines = 0, n = 0;

    snprintf(path, sizeof(path), "%s/%s", afFolder, AF_ManifestName);
    afManifestText = afReadText(path);
    if (afManifestText == NULL)
    {
        return -1;
    }
    for (p = afManifestText; *p; p++)
    {
        lines += (*p == '\n');
    }
    afManifestEntries = malloc(sizeof(afmanifestentry) * (size_t)(lines + 1));
    afHashInit(&afManifest, (udword)lines + 1);
    for (line = afManifestText; line != NULL && *line; line = next)
    {
        uqword size;
        sqword mtime;

        next = strchr(line, '\n');
        if (next != NULL)
        {
            *next++ = 0;
        }
        end = line + strlen(line);
        while (end > line && (end[-1] == '\r' || end[-1] == ' '))
        {
            *--end = 0;
        }
        if (line[0] == '#' || line[0] == 0)
        {
            continue;
        }
        size = strtoull(line, &p, 10);
        if (p == line || *p != ' ')
        {
            continue;
        }
        line = p;
        mtime = strtoll(line, &p, 10);
        if (p == line || *p != ' ')
        {
            continue;
        }
        while (*p == ' ')
        {
            p++;
        }
        p = strchr(p, ' ');                                 // the sha256 (for the tools)
        if (p == NULL)
        {
            continue;
        }
        while (*p == ' ')
        {
            p++;
        }
        afKey(p, p, strlen(p) + 1);
        afManifestEntries[n].size = size;
        afManifestEntries[n].mtime = mtime;
        afHashPut(&afManifest, p, &afManifestEntries[n]);
        n++;
    }
    return n;
}

static bool32 afSkipped(const char *relLower, bool32 isFolder, bool32 atTop)
{
    if (isFolder)
    {
        // Models: the OBJ/MTL files for Blender; feman/backgrounds: the front end's own pictures (its own loader)
        return (atTop && strcmp(relLower, "models") == 0) || strcmp(relLower, "feman/backgrounds") == 0;
    }
    return atTop && (strcmp(relLower, "readme.txt") == 0 || strcmp(relLower, AF_ManifestName) == 0 ||
                     strcmp(relLower, AF_PagesName) == 0 || strcmp(relLower, AF_ModelsName) == 0);
}

static void afFileSeen(const char *path, const char *rel, uqword size, sqword mtime)
{
    char key[PATH_MAX];
    afmanifestentry *entry;

    afScanned++;
    afKey(rel, key, sizeof(key));
    entry = afHashGet(&afManifest, key);
    if (entry != NULL && entry->size == size && entry->mtime == mtime)
    {
        afUnchanged++;                                      // as exported: not used
        return;
    }
    if (afFilesN == afFilesMax)
    {
        afFilesMax = afFilesMax ? afFilesMax * 2 : 64;
        afFiles = realloc(afFiles, sizeof(affile) * (size_t)afFilesMax);
    }
    afFiles[afFilesN].key = afStrdup(key);
    afFiles[afFilesN].rel = afStrdup(rel);
    afFiles[afFilesN].path = afStrdup(path);
    afFiles[afFilesN].logged = FALSE;
    afFilesN++;
}

// path: the folder's full path (PATH_MAX buffer), rel: its path under Assets (PATH_MAX buffer)
static void afScan(char *path, size_t pathLength, char *rel, size_t relLength, sdword depth)
{
    char relLower[PATH_MAX];
#ifdef _WIN32
    WIN32_FIND_DATAA find;
    HANDLE handle;
    char spec[PATH_MAX];

    if (depth > AF_MaxDepth)
    {
        return;
    }
    snprintf(spec, sizeof(spec), "%s\\*", path);
    handle = FindFirstFileA(spec, &find);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return;
    }
    afFolders++;
    do
    {
        const char *name = find.cFileName;
        size_t n = strlen(name);
        bool32 isFolder = (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (name[0] == '.' || pathLength + n + 2 >= PATH_MAX || relLength + n + 2 >= PATH_MAX)
        {
            continue;
        }
        path[pathLength] = '\\';
        memcpy(path + pathLength + 1, name, n + 1);
        if (relLength)
        {
            rel[relLength] = '/';
        }
        memcpy(rel + relLength + (relLength ? 1 : 0), name, n + 1);
        afKey(rel, relLower, sizeof(relLower));
        if (!afSkipped(relLower, isFolder, relLength == 0))
        {
            if (isFolder)
            {
                afScan(path, pathLength + 1 + n, rel, relLength + (relLength ? 1 : 0) + n, depth + 1);
            }
            else
            {
                uqword size = ((uqword)find.nFileSizeHigh << 32) | find.nFileSizeLow;
                uqword ft = ((uqword)find.ftLastWriteTime.dwHighDateTime << 32) | find.ftLastWriteTime.dwLowDateTime;

                afFileSeen(path, rel, size, (sqword)(ft / 10000000ULL) - 11644473600LL);
            }
        }
        path[pathLength] = 0;
        rel[relLength] = 0;
    }
    while (FindNextFileA(handle, &find));
    FindClose(handle);
#else
    DIR *dir = (depth > AF_MaxDepth) ? NULL : opendir(path);
    struct dirent *entry;
    struct stat st;

    if (dir == NULL)
    {
        return;
    }
    afFolders++;
    while ((entry = readdir(dir)) != NULL)
    {
        const char *name = entry->d_name;
        size_t n = strlen(name);
        bool32 isFolder;

        if (name[0] == '.' || pathLength + n + 2 >= PATH_MAX || relLength + n + 2 >= PATH_MAX)
        {
            continue;
        }
        path[pathLength] = '/';
        memcpy(path + pathLength + 1, name, n + 1);
        if (relLength)
        {
            rel[relLength] = '/';
        }
        memcpy(rel + relLength + (relLength ? 1 : 0), name, n + 1);
        if (stat(path, &st) == 0 && (S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)))
        {
            isFolder = S_ISDIR(st.st_mode) != 0;
            afKey(rel, relLower, sizeof(relLower));
            if (!afSkipped(relLower, isFolder, relLength == 0))
            {
                if (isFolder)
                {
                    afScan(path, pathLength + 1 + n, rel, relLength + (relLength ? 1 : 0) + n, depth + 1);
                }
                else
                {
                    afFileSeen(path, rel, (uqword)st.st_size, (sqword)st.st_mtime);
                }
            }
        }
        path[pathLength] = 0;
        rel[relLength] = 0;
    }
    closedir(dir);
#endif
}

/*=============================================================================
    What each active file replaces:
=============================================================================*/

static void afIgnore(affile *file, const char *why)
{
    afIgnored++;
    if (afIgnored <= AF_MaxIgnoredLogged)
    {
        printf("assets: ignored %s (%s)\n", file->rel, why);
    }
}

static void afClassify(affile *file)
{
    char base[PATH_MAX], lif[PATH_MAX];
    size_t n = strlen(file->key);
    assetstexture *tex;

    if (afGameFileExists(file->key))
    {
        afHashPut(&afNative, file->key, file);
        afActive++;
        if (afEndsWith(file->key, ".lif"))
        {
            memcpy(base, file->key, n - 4);
            base[n - 4] = 0;
            tex = afTextureGet(base);
            tex->kinds |= AF_Native;
            tex->nativePath = file->path;
            if (!afLifSize(file->path, &tex->nativeWidth, &tex->nativeHeight))
            {
                printf("assets: WARNING %s is not a readable .lif\n", file->rel);
            }
            printf("assets: active %s (replaces the game's texture, %dx%d)\n", file->rel,
                   (int)tex->nativeWidth, (int)tex->nativeHeight);
        }
        else
        {
            printf("assets: active %s (replaces the game's file)\n", file->rel);
        }
        return;
    }
    if (!afEndsWith(file->key, ".png"))
    {
        afIgnore(file, "no game file of that name");
        return;
    }
    memcpy(base, file->key, n - 4);
    base[n - 4] = 0;
    snprintf(lif, sizeof(lif), "%s.lif", base);
    if (afGameFileExists(lif))
    {
        sdword width, height;

        if (!afPngSize(file->path, &width, &height))
        {
            afIgnore(file, "not a PNG of 1 to 4096 pixels a side");
            return;
        }
        tex = afTextureGet(base);
        tex->kinds |= AF_Png;
        tex->pngPath = file->path;
        tex->pngWidth = width;
        tex->pngHeight = height;
        afActive++;
        printf("assets: active %s (texture %s.lif, %dx%d)\n", file->rel, base, (int)width, (int)height);
        return;
    }
    {
        static const char *suffix[2] = {"_teambase", "_teamstripe"};
        sdword i;

        for (i = 0; i < 2; i++)
        {
            if (afEndsWith(base, suffix[i]))
            {
                base[strlen(base) - strlen(suffix[i])] = 0;
                snprintf(lif, sizeof(lif), "%s.lif", base);
                if (afGameFileExists(lif))
                {
                    tex = afTextureGet(base);
                    tex->kinds |= (i == 0) ? AF_TeamBase : AF_TeamStripe;
                    tex->teamPath[i] = file->path;
                    afActive++;
                    printf("assets: active %s (team %s mask of %s.lif)\n", file->rel, i == 0 ? "base" : "stripe", base);
                    return;
                }
            }
        }
    }
    afIgnore(file, "no game file or texture of that name");
}

// assets-pages.txt: "page <page key> <w> <h>" then "src <source key> <x> <y> <w> <h> <transposed>" lines
static void afPagesLoad(void)
{
    char path[PATH_MAX], key[1024];         // 1024: the %1023s below (PATH_MAX is 260 on Windows)
    char *text, *line, *next;
    assetstexture *page = NULL, *source;
    char pageKey[1024] = "";
    sdword x, y, w, h, t, pages = 0;

    snprintf(path, sizeof(path), "%s/%s", afFolder, AF_PagesName);
    text = afReadText(path);
    if (text == NULL)
    {
        printf("assets: no %s: atlas pages are not rebuilt from changed source textures\n", AF_PagesName);
        return;
    }
    for (line = text; line != NULL && *line; line = next)
    {
        next = strchr(line, '\n');
        if (next != NULL)
        {
            *next++ = 0;
        }
        if (strncmp(line, "page ", 5) == 0)
        {
            if (sscanf(line + 5, "%1023s", key) == 1)
            {
                afKey(key, pageKey, sizeof(pageKey));
            }
            page = NULL;
        }
        else if (strncmp(line, "src ", 4) == 0 && pageKey[0] &&
                 sscanf(line + 4, "%1023s %d %d %d %d %d", key, &x, &y, &w, &h, &t) == 6)
        {
            char sourceKey[1024];

            afKey(key, sourceKey, sizeof(sourceKey));
            source = afHashGet(&afTextures, sourceKey);
            if (source == NULL || source->kinds == 0 || w <= 0 || h <= 0)
            {
                continue;
            }
            if (page == NULL)
            {
                page = afTextureGet(pageKey);
                if (!(page->kinds & AF_Page))
                {
                    pages++;
                }
                page->kinds |= AF_Page;
            }
            page->placed = realloc(page->placed, sizeof(assetsplacement) * (size_t)(page->nPlaced + 1));
            page->placed[page->nPlaced].source = source;
            page->placed[page->nPlaced].x = x;
            page->placed[page->nPlaced].y = y;
            page->placed[page->nPlaced].w = w;
            page->placed[page->nPlaced].h = h;
            page->placed[page->nPlaced].transposed = (t != 0);
            page->nPlaced++;
        }
    }
    free(text);
    if (pages)
    {
        udword i;

        for (i = 0; i <= afTextures.mask; i++)
        {
            assetstexture *tex = afTextures.slot[i].value;

            if (afTextures.slot[i].key != NULL && (tex->kinds & AF_Page))
            {
                sdword j;

                printf("assets: page %s.lif is rebuilt (%d changed source%s:", tex->key, (int)tex->nPlaced,
                       tex->nPlaced == 1 ? "" : "s");
                for (j = 0; j < tex->nPlaced && j < 8; j++)
                {
                    printf(" %s", strrchr(tex->placed[j].source->key, '/') ? strrchr(tex->placed[j].source->key, '/') + 1
                                                                           : tex->placed[j].source->key);
                }
                printf("%s)%s\n", tex->nPlaced > 8 ? " ..." : "",
                       (tex->kinds & AF_Native) ? " - no: its .lif is replaced as a whole" : "");
            }
        }
    }
}

/*=============================================================================
    HWI-FEATURE(assets-models): edited ship models (docs/features/assets-models.md)
=============================================================================*/

// size and whole-second time of a file, as the scan reads them (FALSE: no such file)
static bool32 afFileTime(const char *path, uqword *size, sqword *mtime)
{
#ifdef _WIN32
    WIN32_FIND_DATAA find;
    HANDLE handle = FindFirstFileA(path, &find);
    uqword ft;

    if (handle == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }
    FindClose(handle);
    if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        return FALSE;
    }
    *size = ((uqword)find.nFileSizeHigh << 32) | find.nFileSizeLow;
    ft = ((uqword)find.ftLastWriteTime.dwHighDateTime << 32) | find.ftLastWriteTime.dwLowDateTime;
    *mtime = (sqword)(ft / 10000000ULL) - 11644473600LL;
    return TRUE;
#else
    struct stat st;

    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
    {
        return FALSE;
    }
    *size = (uqword)st.st_size;
    *mtime = (sqword)st.st_mtime;
    return TRUE;
#endif
}

// is this file of the folder changed from the export (or not listed)? FALSE also when it does not exist
static bool32 afChanged(const char *rel, bool32 *exists)
{
    char path[PATH_MAX], key[PATH_MAX];
    uqword size;
    sqword mtime;
    afmanifestentry *entry;

    snprintf(path, sizeof(path), "%s/%s", afFolder, rel);
    *exists = afFileTime(path, &size, &mtime);
    if (!*exists)
    {
        return FALSE;
    }
    afKey(rel, key, sizeof(key));
    entry = afHashGet(&afManifest, key);
    return entry == NULL || entry->size != size || entry->mtime != mtime;
}

// an OBJ is edited when it or its .mtl changed
static bool32 afModelEdited(const char *objRel)
{
    char mtl[PATH_MAX];
    bool32 exists, mtlExists, changed;
    size_t n = strlen(objRel);

    if (strcmp(objRel, "-") == 0 || n < 5)
    {
        return FALSE;
    }
    changed = afChanged(objRel, &exists);
    if (!exists)
    {
        return FALSE;                                       // deleted: the game's own mesh
    }
    snprintf(mtl, sizeof(mtl), "%.*s.mtl", (int)(n - 4), objRel);
    return changed || (afChanged(mtl, &mtlExists) && mtlExists);
}

static void afBaseKey(const char *path, char *out, size_t size)
{
    char *dot, *slash;

    afKey(path, out, size);
    dot = strrchr(out, '.');
    slash = strrchr(out, '/');
    if (dot != NULL && (slash == NULL || dot > slash))
    {
        *dot = 0;
    }
}

static afmodel *afModelNew(const char *ship, sdword level, const char *meshName, const char *objRel)
{
    char path[PATH_MAX];
    afmodel *model = calloc(1, sizeof(afmodel));

    snprintf(path, sizeof(path), "%s/%s", afFolder, objRel);
    model->ship = afStrdup(ship);
    model->level = level;
    model->meshName = afStrdup(meshName);
    model->objRel = afStrdup(objRel);
    model->objPath = afStrdup(path);
    afModelList = realloc(afModelList, sizeof(afmodel *) * (size_t)(afModelN + 1));
    afModelList[afModelN++] = model;
    return model;
}

// assets-models.txt: "ship<TAB>level<TAB>source mesh<TAB>OBJ<TAB>in-game mesh<TAB>in-game OBJ or -"
static char **afModelsFields(char *line, sdword *n)
{
    static char *fields[8];
    char *p = line;

    *n = 0;
    while (*n < 8)
    {
        fields[(*n)++] = p;
        p = strchr(p, '\t');
        if (p == NULL)
        {
            break;
        }
        *p++ = 0;
    }
    return fields;
}

// read at start-up, while the manifest is loaded: which OBJs are edited, and the levels they stand in for
static void afModelsLoad(void)
{
    char path[PATH_MAX], base[PATH_MAX];
    char *text, *line, *next;
    sdword i, j, edited = 0;

    snprintf(path, sizeof(path), "%s/%s", afFolder, AF_ModelsName);
    text = afReadText(path);
    if (text == NULL)
    {
        snprintf(path, sizeof(path), "%s/Models", afFolder);
        {
            struct stat st;

            if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
            {
                printf("assets: no %s: edited models are not read (an older export)\n", AF_ModelsName);
            }
        }
        return;
    }
    for (line = text; line != NULL && *line; line = next)
    {
        char **f;
        sdword n;
        size_t len;
        afmodelline *ml;
        bool32 mainEdited, gameEdited;

        next = strchr(line, '\n');
        if (next != NULL)
        {
            *next++ = 0;
        }
        len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' '))
        {
            line[--len] = 0;
        }
        if (line[0] == '#' || line[0] == 0)
        {
            continue;
        }
        f = afModelsFields(line, &n);
        if (n < 6)
        {
            continue;
        }
        afBaseKey(f[2], base, sizeof(base));
        afModelLines = realloc(afModelLines, sizeof(afmodelline) * (size_t)(afModelLineN + 1));
        ml = &afModelLines[afModelLineN++];
        ml->ship = afStrdup(f[0]);
        ml->level = (sdword)strtol(f[1], NULL, 10);
        ml->base = afStrdup(base);
        ml->model = NULL;
        mainEdited = afModelEdited(f[3]);
        gameEdited = afModelEdited(f[5]);
        if (!mainEdited && !gameEdited)
        {
            continue;
        }
        if (afHashGet(&afModels, ml->base) != NULL)
        {
            afmodel *other = afHashGet(&afModels, ml->base);

            if (strcmp(other->objRel, mainEdited ? f[3] : f[5]) != 0)
            {
                printf("assets: model %s: its mesh %s is already replaced by %s; not used\n",
                       mainEdited ? f[3] : f[5], f[2], other->objRel);
            }
            ml->model = other;
            continue;
        }
        if (mainEdited && gameEdited)
        {
            printf("assets: model %s and %s are both edited: %s is used (the main model wins)\n", f[3], f[5], f[3]);
        }
        ml->model = mainEdited ? afModelNew(f[0], ml->level, f[2], f[3]) : afModelNew(f[0], ml->level, f[4], f[5]);
        afHashPut(&afModels, ml->base, ml->model);
        printf("assets: model %s edited: it replaces %s (%s level %d)\n", ml->model->objRel,
               mainEdited ? "the mesh and its paged version" : "the paged mesh", f[0], (int)ml->level);
        edited++;
    }
    free(text);
    // a level without an edited model takes the nearest lower edited level of its ship, so the edit shows at any
    // distance
    for (i = 0; i < afModelLineN; i++)
    {
        afmodelline *ml = &afModelLines[i];
        afmodelline *best = NULL;

        if (ml->model != NULL || afHashGet(&afModels, ml->base) != NULL)
        {
            continue;
        }
        for (j = 0; j < afModelLineN; j++)
        {
            afmodelline *o = &afModelLines[j];

            if (o->model != NULL && o->level < ml->level && strcmp(o->ship, ml->ship) == 0 &&
                (best == NULL || o->level > best->level))
            {
                best = o;
            }
        }
        if (best != NULL)
        {
            afHashPut(&afModels, ml->base, best->model);
            afModelStandIns++;
        }
    }
    // one line per ship with stand-ins
    for (i = 0; i < afModelN; i++)
    {
        afmodel *model = afModelList[i];
        char levels[256] = "";
        size_t len = 0;

        for (j = 0; j < afModelLineN; j++)
        {
            afmodelline *o = &afModelLines[j];

            if (o->model == NULL && afHashGet(&afModels, o->base) == model && strcmp(o->ship, model->ship) == 0 &&
                len + 8 < sizeof(levels))
            {
                len += (size_t)snprintf(levels + len, sizeof(levels) - len, "%s%d", len ? ", " : "", (int)o->level);
            }
        }
        if (len)
        {
            printf("assets: model %s also stands in for %s level%s %s (no edited model of its own)\n", model->objRel,
                   model->ship, strchr(levels, ',') ? "s" : "", levels);
        }
    }
    if (edited)
    {
        printf("assets: models: %d edited, %d levels stand in\n", (int)edited, (int)afModelStandIns);
    }
}

static bool32 afGameFile(const char *path)
{
    char key[PATH_MAX];

    afKey(path, key, sizeof(key));
    return afGameFileExists(key);
}

// a picture an imported model names that the game does not have
static bool32 afNewPicture(const char *key, const char *pngPath)
{
    assetstexture *tex = afHashGet(&afTextures, key);
    sdword width, height, i;
    char mask[PATH_MAX];
    size_t n = strlen(pngPath);

    if (tex != NULL)
    {
        return (tex->kinds & AF_New) && tex->pngPath != NULL && strcmp(tex->pngPath, pngPath) == 0;
    }
    if (!afPngSize(pngPath, &width, &height))
    {
        printf("assets: %s is not a PNG of 1 to %d pixels a side\n", pngPath, AF_MaxSide);
        return FALSE;
    }
    tex = afTextureGet(key);
    tex->kinds = AF_Png | AF_New;
    tex->pngPath = afStrdup(pngPath);
    tex->pngWidth = width;
    tex->pngHeight = height;
    for (i = 0; i < 2 && n > 4; i++)
    {
        snprintf(mask, sizeof(mask), "%.*s%s.png", (int)(n - 4), pngPath, i == 0 ? "_teambase" : "_teamstripe");
        if (afPngSize(mask, &width, &height))
        {
            tex->kinds |= (i == 0) ? AF_TeamBase : AF_TeamStripe;
            tex->teamPath[i] = afStrdup(mask);
        }
    }
    return TRUE;
}

// HWI-FEATURE(assets-models): the mesh image of an edited model for this mesh file (a .geo or .peo path as the game
//      names it), built once; meshName gets the game mesh it was made from (its folder holds the textures)
bool32 assetsModelFind(char *fileName, const ubyte **image, sdword *length, char *meshName, size_t meshNameSize)
{
    char base[PATH_MAX];
    afmodel *model;

    if (afModels.count == 0 || afSuspended > 0 || fileName == NULL)
    {
        return FALSE;
    }
    afBaseKey(fileName, base, sizeof(base));
    model = afHashGet(&afModels, base);
    if (model == NULL || model->state < 0)
    {
        return FALSE;
    }
    if (model->state == 0)
    {
        char why[512];
        amstats stats;
        void *tmpl = NULL;
        sdword tmplLength = 0;
        Uint64 t0 = SDL_GetPerformanceCounter();

        model->state = -1;
        assetsSuspend(TRUE);                                // the game's own mesh is the template
        if (fileExists(model->meshName, 0))
        {
            tmplLength = fileLoadAlloc(model->meshName, &tmpl, NonVolatile);
        }
        assetsSuspend(FALSE);
        if (tmpl == NULL)
        {
            printf("assets: model %s: the game has no %s; not used\n", model->objRel, model->meshName);
            return FALSE;
        }
        model->image = amObjToMesh(afFolder, model->objPath, model->objRel, model->meshName, tmpl, tmplLength,
                                   afNewPicture, afGameFile, &model->length, &stats, why, sizeof(why));
        memFree(tmpl);
        if (model->image == NULL)
        {
            printf("assets: model %s: %s; the game's own mesh is used\n", model->objRel, why);
            return FALSE;
        }
        model->state = 1;
        printf("assets: model %s: %d objects with faces, %d triangles from %d faces, %d vertices, %d new material%s, "
               "%d new picture%s, glow from the game's mesh at %d vertices, %.1f ms\n", model->objRel,
               (int)stats.objects, (int)stats.triangles, (int)stats.faces, (int)stats.vertices,
               (int)stats.newMaterials, stats.newMaterials == 1 ? "" : "s", (int)stats.newPictures,
               stats.newPictures == 1 ? "" : "s", (int)stats.glowCopied,
               (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency());
    }
    if (afHashGet(&afModelLogged, base) == NULL)
    {
        printf("assets: model %s drawn for %s\n", model->objRel, fileName);
        afHashPut(&afModelLogged, afStrdup(base), model);
    }
    *image = model->image;
    *length = model->length;
    snprintf(meshName, meshNameSize, "%s", model->meshName);
    return TRUE;
}

// HWI-TEST /catAssetsModelTest: every OBJ assets-models.txt names, built from the file as it is and compared with
//      the game's mesh (the export's own OBJs: the same objects, triangles, positions, UVs, normals, materials, glow)
char catAssetsModelTestOn = FALSE;

void catAssetsModelRunTest(void)
{
    char path[PATH_MAX];
    char *text, *line, *next;
    sdword files = 0, pass = 0, fail = 0, polygons = 0, nanNormals = 0;
    real64 maxPosition = 0.0, maxUV = 0.0, maxNormal = 0.0, maxSmoothFace = 0.0;
    Uint64 t0 = SDL_GetPerformanceCounter();

    snprintf(path, sizeof(path), "%s/%s", afFolder, AF_ModelsName);
    text = afFolder[0] ? afReadText(path) : NULL;
    if (text == NULL)
    {
        printf("modeltest: no %s in the Assets folder (%s); result=FAIL\n", AF_ModelsName, afFolder);
        return;
    }
    for (line = text; line != NULL && *line; line = next)
    {
        char **f;
        sdword n, k;
        size_t len;

        next = strchr(line, '\n');
        if (next != NULL)
        {
            *next++ = 0;
        }
        len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' '))
        {
            line[--len] = 0;
        }
        if (line[0] == '#' || line[0] == 0)
        {
            continue;
        }
        f = afModelsFields(line, &n);
        if (n < 6)
        {
            continue;
        }
        for (k = 0; k < 2; k++)
        {
            const char *objRel = (k == 0) ? f[3] : f[5];
            const char *meshName = (k == 0) ? f[2] : f[4];
            char objPath[PATH_MAX], why[512] = "";
            void *tmpl = NULL;
            sdword tmplLength = 0, imageLength = 0;
            ubyte *image;
            amstats stats;
            amcompare cmp;
            bool32 ok = FALSE;

            if (strcmp(objRel, "-") == 0)
            {
                continue;
            }
            files++;
            snprintf(objPath, sizeof(objPath), "%s/%s", afFolder, objRel);
            assetsSuspend(TRUE);
            if (fileExists((char *)meshName, 0))
            {
                tmplLength = fileLoadAlloc((char *)meshName, &tmpl, 0);
            }
            assetsSuspend(FALSE);
            memset(&cmp, 0, sizeof(cmp));
            if (tmpl == NULL)
            {
                snprintf(why, sizeof(why), "the game has no %s", meshName);
                image = NULL;
            }
            else
            {
                image = amObjToMesh(afFolder, objPath, objRel, meshName, tmpl, tmplLength, afNewPicture, afGameFile,
                                    &imageLength, &stats, why, sizeof(why));
            }
            if (image != NULL && amCompare(tmpl, tmplLength, image, imageLength, &cmp, why, sizeof(why)))
            {
                real64 tolerancePosition = 1e-4;

                ok = cmp.maxPosition <= tolerancePosition && cmp.maxUV <= 1e-5 && cmp.maxNormal <= 1e-4 &&
                     cmp.materialDiffers == 0 && cmp.glowDiffers == 0;
                if (!ok)
                {
                    snprintf(why, sizeof(why), "outside the tolerances");
                }
                polygons += cmp.polygons;
                nanNormals += cmp.nanNormals;
                maxPosition = max(maxPosition, cmp.maxPosition);
                maxUV = max(maxUV, cmp.maxUV);
                maxNormal = max(maxNormal, cmp.maxNormal);
                maxSmoothFace = max(maxSmoothFace, cmp.maxSmoothFace);
            }
            printf("modeltest: %s objects=%d polygons=%d pos=%.2g uv=%.2g normal=%.2g smoothface=%.2g materials=%d "
                   "glow=%d nan=%d result=%s%s%s\n", objRel, (int)cmp.objects, (int)cmp.polygons, cmp.maxPosition,
                   cmp.maxUV, cmp.maxNormal, cmp.maxSmoothFace, (int)cmp.materialDiffers, (int)cmp.glowDiffers,
                   (int)cmp.nanNormals,
                   ok ? "PASS" : "FAIL", ok ? "" : " ", ok ? "" : why);
            pass += ok;
            fail += !ok;
            free(image);
            if (tmpl != NULL)
            {
                memFree(tmpl);
            }
        }
    }
    free(text);
    printf("modeltest: done files=%d pass=%d fail=%d polygons=%d maxpos=%.3g maxuv=%.3g maxnormal=%.3g "
           "maxsmoothface=%.3g nan=%d %.1f s result=%s\n", (int)files, (int)pass, (int)fail, (int)polygons,
           maxPosition, maxUV, maxNormal, maxSmoothFace, (int)nanNormals,
           (double)(SDL_GetPerformanceCounter() - t0) / (double)SDL_GetPerformanceFrequency(),
           (files > 0 && fail == 0) ? "PASS" : "FAIL");
}

static void afModelsShutdown(void)
{
    sdword i;
    udword j;

    for (i = 0; i < afModelN; i++)
    {
        free(afModelList[i]->ship);
        free(afModelList[i]->meshName);
        free(afModelList[i]->objRel);
        free(afModelList[i]->objPath);
        free(afModelList[i]->image);
        free(afModelList[i]);
    }
    free(afModelList);
    afModelList = NULL;
    afModelN = afModelStandIns = 0;
    for (i = 0; i < afModelLineN; i++)
    {
        free(afModelLines[i].ship);
        free(afModelLines[i].base);
    }
    free(afModelLines);
    afModelLines = NULL;
    afModelLineN = 0;
    afHashFree(&afModels);                                  // keys: the lines' bases
    if (afModelLogged.slot != NULL)
    {
        for (j = 0; j <= afModelLogged.mask; j++)
        {
            free(afModelLogged.slot[j].key);
        }
    }
    afHashFree(&afModelLogged);
}

/*=============================================================================
    Functions:
=============================================================================*/

// HWI-FEATURE(user-files): the Assets folder, without a trailing separator: /assetsPath, else Assets in the visible
//      user-files folder (fileUserFilesPath: Documents/Homeworld Infection, or /userFilesPath); NULL with /noAssets.
//      ScreenBackground.c reads its pictures from feman/backgrounds/ in it
char *assetsFolderPath(void)
{
    static char path[PATH_MAX];
    size_t n;

    if (afOff)
    {
        return NULL;
    }
    if (afPathSwitch[0])
    {
        snprintf(path, sizeof(path), "%s", afPathSwitch);
    }
    else
    {
        snprintf(path, sizeof(path), "%s%s", fileUserFilesPath, AF_FolderName);
    }
    n = strlen(path);
    while (n > 1 && (path[n - 1] == '/' || path[n - 1] == '\\'))
    {
        path[--n] = 0;
    }
    return path;
}

bool32 assetsPathSet(char *path)
{
    snprintf(afPathSwitch, sizeof(afPathSwitch), "%s", path);
    return TRUE;
}

bool32 assetsOffSet(char *string)
{
    (void)string;
    afOff = TRUE;
    return TRUE;
}

void assetsSuspend(bool32 suspend)
{
    afSuspended += suspend ? 1 : -1;
}

sdword assetsActiveCount(void)
{
    return afActive;
}

void assetsStartup(void)
{
    char path[PATH_MAX], rel[PATH_MAX];
    struct stat st;
    sdword manifestN, i;
    Uint64 t0 = SDL_GetPerformanceCounter();
    size_t n;

    if (afOff)
    {
        printf("assets: off (/noAssets)\n");
        return;
    }
    snprintf(afFolder, sizeof(afFolder), "%s", assetsFolderPath());    // HWI-FEATURE(user-files): the visible folder
    n = strlen(afFolder);
    if (stat(afFolder, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        printf("assets: folder %s: none (nothing replaced)\n", afFolder);
        return;
    }
    manifestN = afManifestLoad();
    if (manifestN < 0)
    {
        afHashInit(&afManifest, 1);
    }
    snprintf(path, sizeof(path), "%s", afFolder);
    rel[0] = 0;
    afScan(path, strlen(path), rel, 0, 0);
    afModelsLoad();                                         // HWI-FEATURE(assets-models): needs the manifest
    afHashFree(&afManifest);
    free(afManifestEntries);
    afManifestEntries = NULL;
    free(afManifestText);
    afManifestText = NULL;

    for (i = 0; i < afFilesN; i++)
    {
        afClassify(&afFiles[i]);
    }
    if (afIgnored > AF_MaxIgnoredLogged)
    {
        printf("assets: ignored %d more\n", (int)(afIgnored - AF_MaxIgnoredLogged));
    }
    if (afTextures.count)
    {
        afPagesLoad();
    }
    printf("assets: folder %s: %d files in %d folders scanned in %.1f ms; manifest %d, unchanged %d, active %d, "
           "ignored %d\n", afFolder, (int)afScanned, (int)afFolders,
           (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency(),
           (int)(manifestN < 0 ? 0 : manifestN), (int)afUnchanged, (int)afActive, (int)afIgnored);
    if (manifestN < 0 && afScanned > 0)
    {
        printf("assets: no %s: every file that names a game file is used\n", AF_ManifestName);
    }
}

void assetsShutdown(void)
{
    udword i;
    sdword j;

    if (afTextures.slot != NULL)
    {
        for (i = 0; i <= afTextures.mask; i++)
        {
            assetstexture *tex = afTextures.slot[i].value;

            if (afTextures.slot[i].key != NULL)
            {
                if (tex->kinds & AF_New)                    // HWI-FEATURE(assets-models): its own copies
                {
                    free(tex->pngPath);
                    free(tex->teamPath[0]);
                    free(tex->teamPath[1]);
                }
                free(tex->placed);
                free(tex->shareParent);
                free(tex->key);
                free(tex);
            }
        }
    }
    afHashFree(&afTextures);
    afHashFree(&afNative);
    afModelsShutdown();                                     // HWI-FEATURE(assets-models)
    for (j = 0; j < afFilesN; j++)
    {
        free(afFiles[j].key);
        free(afFiles[j].rel);
        free(afFiles[j].path);
    }
    free(afFiles);
    afFiles = NULL;
    afFilesN = afFilesMax = 0;
    afActive = 0;
}

char *assetsFileFind(char *fileName, udword flags)
{
    char key[PATH_MAX];
    affile *file;

    if (afNative.count == 0 || afSuspended > 0 || fileName == NULL ||
        bitTest(flags, FF_UserSettingsPath | FF_CDROM | FF_IgnorePrepend | FF_HomeworldDataPath | FF_WriteMode |
                       FF_AppendMode))
    {
        return NULL;
    }
    if (fileName[0] == '/' || fileName[0] == '\\' || (fileName[0] && fileName[1] == ':'))
    {
        return NULL;
    }
    afKey(fileName, key, sizeof(key));
    file = afHashGet(&afNative, key);
    if (file == NULL)
    {
        return NULL;
    }
    if (!file->logged)
    {
        printf("assets: using %s\n", file->path);
        file->logged = TRUE;
    }
    return file->path;
}

assetstexture *assetsTextureFind(char *name)
{
    char key[PATH_MAX];
    size_t n;
    assetstexture *tex;

    if (afTextures.count == 0 || afSuspended > 0 || name == NULL)
    {
        return NULL;
    }
    afKey(name, key, sizeof(key));
    n = strlen(key);
    if (n > 4 && strcmp(key + n - 4, ".lif") == 0)
    {
        key[n - 4] = 0;
    }
    tex = afHashGet(&afTextures, key);
    return (tex != NULL && tex->kinds != 0) ? tex : NULL;
}

void assetsTextureShared(assetstexture *tex, char *parentName)
{
    char key[PATH_MAX];
    size_t n;

    free(tex->shareParent);
    tex->shareParent = NULL;
    if (parentName != NULL)
    {
        afKey(parentName, key, sizeof(key));
        n = strlen(key);
        if (n > 4 && strcmp(key + n - 4, ".lif") == 0)
        {
            key[n - 4] = 0;
        }
        tex->shareParent = afStrdup(key);
    }
}

static sdword afCeilRatio(sdword a, sdword b)
{
    return (b > 0) ? (a + b - 1) / b : 1;
}

void assetsTextureSize(assetstexture *tex, sdword origWidth, sdword origHeight)
{
    sdword scale = 1, i;

    tex->origWidth = origWidth;
    tex->origHeight = origHeight;
    tex->pageScale = 1;
    if (tex->kinds & AF_Native)
    {
        tex->outWidth = tex->nativeWidth;
        tex->outHeight = tex->nativeHeight;
        return;
    }
    tex->outWidth = (tex->kinds & AF_Png) ? tex->pngWidth : origWidth;
    tex->outHeight = (tex->kinds & AF_Png) ? tex->pngHeight : origHeight;
    if (!(tex->kinds & AF_Page))
    {
        return;
    }
    // a page grows by the largest whole factor its changed sources (or its own PNG) need
    if (tex->kinds & AF_Png)
    {
        scale = max(scale, max(afCeilRatio(tex->pngWidth, origWidth), afCeilRatio(tex->pngHeight, origHeight)));
    }
    for (i = 0; i < tex->nPlaced; i++)
    {
        assetsplacement *p = &tex->placed[i];
        assetstexture *s = p->source;
        sdword w = p->transposed ? p->h : p->w, h = p->transposed ? p->w : p->h;
        sdword sw = w, sh = h;

        if (s->kinds & AF_Native)
        {
            sw = s->nativeWidth;
            sh = s->nativeHeight;
        }
        else if (s->kinds & AF_Png)
        {
            sw = s->pngWidth;
            sh = s->pngHeight;
        }
        scale = max(scale, max(afCeilRatio(sw, w), afCeilRatio(sh, h)));
    }
    while (scale > 1 && (origWidth * scale > AF_MaxSide || origHeight * scale > AF_MaxSide))
    {
        scale--;
    }
    tex->pageScale = scale;
    tex->outWidth = origWidth * scale;
    tex->outHeight = origHeight * scale;
}
