/*=============================================================================
    AssetsExport.c: HWI-FEATURE(assets-autoexport) the game fills its Assets folder itself
    (docs/features/assets-autoexport.md).

    The user (2026-11-02): "I wanted that packed as a feature so that players themselves can modify the ships if they
    wanted. When they use the appimage to unpack the game, I expect an assets folder to be found inside so that any
    changes to the folder will result in those updated models to be piped into the game."

    A C port of tools/assets/export_assets.py (with the parts of export_ships.py and hwformats.py it uses). Same
    output: the same files, byte for byte, except that a PNG's compressed bytes differ (its pixels and colour type are
    the same); tools/assets/compare_exports.py checks it. Keep both in sync.

    It runs on its own thread with its own handles to the .big archives (never the engine's), from malloc memory,
    and touches nothing of the game but the folder. Every file goes through a temporary name and a rename, and is
    journalled in assets-manifest.partial; the journal becomes assets-manifest.txt at the end, so a quit or a crash
    leaves an export that the next start resumes. A file already in the folder that the export did not write (the
    player's) is never overwritten.
=============================================================================*/

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <direct.h>
#else
    #include <dirent.h>
    #include <unistd.h>
#endif

#include <SDL2/SDL.h>

// the encoder (STB_IMAGE_WRITE_IMPLEMENTATION is in screenshot.c; stbi_write_png_to_mem is not in the header)
extern unsigned char *stbi_zlib_compress(unsigned char *data, int data_len, int *out_len, int quality);
#include "stb_image.h"              // the decoder (STB_IMAGE_IMPLEMENTATION is in screenshot.c)

#include "AssetsExport.h"

#ifdef __MINGW32__
    #define ae_vsnprintf __mingw_vsnprintf  // correctly rounded "%.6f" (the OBJ text must equal Python's)
    #define ae_snprintf __mingw_snprintf
#else
    #define ae_vsnprintf vsnprintf
    #define ae_snprintf snprintf
#endif

#define AE_MaxArchives      4
#define AE_Window           4096
#define AE_BackgroundsKey   "feman/backgrounds/"

#define MDF_Smoothing       0x2
#define MDF_2Sided          0x8
#define MDF_SelfIllum       0x40

#define TRF_Paletted        0x2
#define TRF_Alpha           0x8
#define TRF_TeamColor0      0x10
#define TRF_TeamColor1      0x20

static const char *aeRaces[] = {"race_sect", "race_beast", "race_kushan", "race_taidan", "race_p1", "race_traders",
                                "race_derelict"};

/*=============================================================================
    Small helpers:
=============================================================================*/

typedef struct
{
    char *d;
    size_t n, cap;
}
aebuf;

static void aeBufAdd(aebuf *b, const void *s, size_t n)
{
    if (b->n + n + 1 > b->cap)
    {
        size_t cap = b->cap ? b->cap : 256;

        while (b->n + n + 1 > cap)
        {
            cap *= 2;
        }
        b->d = realloc(b->d, cap);
        b->cap = cap;
    }
    memcpy(b->d + b->n, s, n);
    b->n += n;
    b->d[b->n] = 0;
}

static void aeBufStr(aebuf *b, const char *s)
{
    aeBufAdd(b, s, strlen(s));
}

static void aeBufPrintf(aebuf *b, const char *format, ...)
{
    char small[1024];
    va_list ap;
    int n;

    va_start(ap, format);
    n = ae_vsnprintf(small, sizeof(small), format, ap);
    va_end(ap);
    if (n < 0)
    {
        return;
    }
    if ((size_t)n < sizeof(small))
    {
        aeBufAdd(b, small, (size_t)n);
        return;
    }
    {
        char *big = malloc((size_t)n + 1);

        va_start(ap, format);
        ae_vsnprintf(big, (size_t)n + 1, format, ap);
        va_end(ap);
        aeBufAdd(b, big, (size_t)n);
        free(big);
    }
}

static void aeBufFree(aebuf *b)
{
    free(b->d);
    b->d = NULL;
    b->n = b->cap = 0;
}

static char *aeStrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = malloc(n);

    memcpy(d, s, n);
    return d;
}

static char *aeStrndup(const char *s, size_t n)
{
    char *d = malloc(n + 1);

    memcpy(d, s, n);
    d[n] = 0;
    return d;
}

static void aeLower(char *s)
{
    for (; *s; s++)
    {
        *s = (char)tolower((unsigned char)*s);
    }
}

static bool32 aeStartsWith(const char *s, const char *start)
{
    return strncmp(s, start, strlen(start)) == 0;
}

static bool32 aeEndsWith(const char *s, const char *end)
{
    size_t n = strlen(s), m = strlen(end);

    return n >= m && strcmp(s + n - m, end) == 0;
}

static int aeStrCmp(const void *a, const void *b)
{
    return strcmp(*(char * const *)a, *(char * const *)b);
}

// Python's export_ships.fmt: "%.6f" without trailing zeros, "-0" as "0"
static void aeFmt(char *out, size_t size, double x)
{
    size_t n;

    if (isnan(x))
    {
        snprintf(out, size, "nan");                         // Python prints a NaN without its sign
        return;
    }
    if (isinf(x))
    {
        snprintf(out, size, x < 0 ? "-inf" : "inf");
        return;
    }
    ae_snprintf(out, size, "%.6f", x);
    n = strlen(out);
    if (strchr(out, '.') != NULL)
    {
        while (n > 0 && out[n - 1] == '0')
        {
            out[--n] = 0;
        }
        if (n > 0 && out[n - 1] == '.')
        {
            out[--n] = 0;
        }
    }
    if (strcmp(out, "-0") == 0 || out[0] == 0)
    {
        snprintf(out, size, "0");
    }
}

static udword aeLE32(const ubyte *p)
{
    return (udword)p[0] | ((udword)p[1] << 8) | ((udword)p[2] << 16) | ((udword)p[3] << 24);
}

static uword aeLE16(const ubyte *p)
{
    return (uword)(p[0] | (p[1] << 8));
}

static float aeLEf(const ubyte *p)
{
    udword u = aeLE32(p);
    float f;

    memcpy(&f, &u, 4);
    return f;
}

/*=============================================================================
    SHA-256 (FIPS 180-4; public-domain style implementation):
=============================================================================*/

typedef struct
{
    udword h[8];
    ubyte block[64];
    uqword length;
    size_t used;
}
aesha;

static const udword aeShaK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98,
    0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8,
    0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2};

#define AE_ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void aeShaBlock(aesha *s, const ubyte *p)
{
    udword w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;

    for (i = 0; i < 16; i++)
    {
        w[i] = ((udword)p[4 * i] << 24) | ((udword)p[4 * i + 1] << 16) | ((udword)p[4 * i + 2] << 8) | p[4 * i + 3];
    }
    for (i = 16; i < 64; i++)
    {
        udword s0 = AE_ROR(w[i - 15], 7) ^ AE_ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        udword s1 = AE_ROR(w[i - 2], 17) ^ AE_ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);

        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++)
    {
        t1 = h + (AE_ROR(e, 6) ^ AE_ROR(e, 11) ^ AE_ROR(e, 25)) + ((e & f) ^ (~e & g)) + aeShaK[i] + w[i];
        t2 = (AE_ROR(a, 2) ^ AE_ROR(a, 13) ^ AE_ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void aeShaInit(aesha *s)
{
    static const udword h0[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab,
                                 0x5be0cd19};

    memcpy(s->h, h0, sizeof(h0));
    s->length = 0;
    s->used = 0;
}

static void aeShaAdd(aesha *s, const ubyte *p, size_t n)
{
    s->length += n;
    while (n > 0)
    {
        size_t k = 64 - s->used;

        if (k > n)
        {
            k = n;
        }
        memcpy(s->block + s->used, p, k);
        s->used += k;
        p += k;
        n -= k;
        if (s->used == 64)
        {
            aeShaBlock(s, s->block);
            s->used = 0;
        }
    }
}

static void aeShaHex(aesha *s, char *hex)
{
    uqword bits = s->length * 8;
    ubyte pad = 0x80, zero = 0, len[8];
    int i;

    aeShaAdd(s, &pad, 1);
    while (s->used != 56)
    {
        aeShaAdd(s, &zero, 1);
    }
    for (i = 0; i < 8; i++)
    {
        len[i] = (ubyte)(bits >> (56 - 8 * i));
    }
    aeShaAdd(s, len, 8);
    for (i = 0; i < 8; i++)
    {
        sprintf(hex + 8 * i, "%08x", s->h[i]);
    }
    hex[64] = 0;
}

static void aeSha(const void *data, size_t n, char *hex)
{
    aesha s;

    aeShaInit(&s);
    aeShaAdd(&s, data, n);
    aeShaHex(&s, hex);
}

/*=============================================================================
    Files on disk:
=============================================================================*/

static bool32 aeStat(const char *path, uqword *size, sqword *mtime, bool32 *isDir)
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;
    uqword ft;

    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &data))
    {
        return FALSE;
    }
    if (isDir)
    {
        *isDir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    if (size)
    {
        *size = ((uqword)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    }
    // the same seconds as AssetsFolder.c's scan (FindFirstFile's ftLastWriteTime)
    ft = ((uqword)data.ftLastWriteTime.dwHighDateTime << 32) | data.ftLastWriteTime.dwLowDateTime;
    if (mtime)
    {
        *mtime = (sqword)(ft / 10000000ULL) - 11644473600LL;
    }
    return TRUE;
#else
    struct stat st;

    if (stat(path, &st) != 0)
    {
        return FALSE;
    }
    if (isDir)
    {
        *isDir = S_ISDIR(st.st_mode) != 0;
    }
    if (size)
    {
        *size = (uqword)st.st_size;
    }
    if (mtime)
    {
        *mtime = (sqword)st.st_mtime;
    }
    return TRUE;
#endif
}

// mkdir -p (its own; File.c's uses a global stat buffer, not thread safe)
static bool32 aeMakeDirs(const char *dir)
{
    char tmp[PATH_MAX];
    size_t i, n;
    bool32 isDir;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    n = strlen(tmp);
    for (i = 1; i <= n; i++)
    {
        if (i == n || tmp[i] == '/' || tmp[i] == '\\')
        {
            char save = tmp[i];

            if (i > 0 && tmp[i - 1] == ':')
            {
                continue;                                   // "C:\"
            }
            tmp[i] = 0;
            if (!aeStat(tmp, NULL, NULL, &isDir))
            {
#ifdef _WIN32
                if (!CreateDirectoryA(tmp, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
#else
                if (mkdir(tmp, 0777) != 0 && errno != EEXIST)
#endif
                {
                    return FALSE;
                }
            }
            else if (!isDir)
            {
                return FALSE;
            }
            tmp[i] = save;
        }
    }
    return TRUE;
}

static ubyte *aeReadFile(const char *path, size_t *length)
{
    FILE *fp = fopen(path, "rb");
    ubyte *d;
    long n;

    if (fp == NULL)
    {
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (n < 0)
    {
        fclose(fp);
        return NULL;
    }
    d = malloc((size_t)n + 1);
    if (fread(d, 1, (size_t)n, fp) != (size_t)n)
    {
        free(d);
        fclose(fp);
        return NULL;
    }
    d[n] = 0;
    fclose(fp);
    *length = (size_t)n;
    return d;
}

static bool32 aeShaFile(const char *path, char *hex)
{
    FILE *fp = fopen(path, "rb");
    ubyte buf[65536];
    size_t n;
    aesha s;

    if (fp == NULL)
    {
        return FALSE;
    }
    aeShaInit(&s);
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
    {
        aeShaAdd(&s, buf, n);
    }
    fclose(fp);
    aeShaHex(&s, hex);
    return TRUE;
}

static bool32 aeReplace(const char *from, const char *to)
{
#ifdef _WIN32
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return rename(from, to) == 0;
#endif
}

/*=============================================================================
    The archives (tools/bigfile.py): "RBF1.23", a TOC of 32-byte entries, XOR-encrypted names, LZSS data.
=============================================================================*/

typedef struct
{
    char *key;                      // lower case, '/' separators
    sdword archive;                 // precedence: 0 wins
    sdword order;                   // TOC order inside the archive
    udword offset, stored, real;
    ubyte comp;
}
aeentry;

typedef struct
{
    FILE *fp[AE_MaxArchives];
    char *path[AE_MaxArchives];
    sdword nArchives;
    aeentry *e;
    sdword n;
}
aetree;

static int aeEntryCmp(const void *a, const void *b)
{
    const aeentry *x = a, *y = b;
    int c = strcmp(x->key, y->key);

    if (c)
    {
        return c;
    }
    if (x->archive != y->archive)
    {
        return x->archive - y->archive;
    }
    return x->order - y->order;
}

// tolower + '\\' -> '/', '' and '.' parts dropped (bigfile.py safe_out_path)
static char *aeNameKey(const ubyte *name, size_t n)
{
    char *out = malloc(n + 1);
    size_t i, o = 0, partStart = 0;

    for (i = 0; i <= n; i++)
    {
        char ch = (i < n) ? (char)tolower(name[i]) : '/';

        if (ch == '\\')
        {
            ch = '/';
        }
        if (ch == '/')
        {
            size_t len = o - partStart;

            if (len == 0 || (len == 1 && out[partStart] == '.'))
            {
                o = partStart;                              // drop the part
            }
            else if (i < n)
            {
                out[o++] = '/';
                partStart = o;
            }
            continue;
        }
        out[o++] = ch;
    }
    while (o > 0 && out[o - 1] == '/')
    {
        o--;
    }
    out[o] = 0;
    return out;
}

static bool32 aeTreeAdd(aetree *t, const char *path, sdword archive, sdword *cap)
{
    FILE *fp = fopen(path, "rb");
    ubyte head[15];
    sdword numFiles, i, entrySize = 32;
    long fileSize;
    ubyte *toc;
    udword tocStart = 15;

    if (fp == NULL)
    {
        printf("assets: export: cannot open %s\n", path);
        return FALSE;
    }
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fread(head, 1, 15, fp) != 15 || memcmp(head, "RBF1.23", 7) != 0)
    {
        printf("assets: export: %s is not a .big archive\n", path);
        fclose(fp);
        return FALSE;
    }
    numFiles = (sdword)aeLE32(head + 7);
    if (numFiles < 0 || numFiles > 10000000)
    {
        fclose(fp);
        return FALSE;
    }
    // 32-byte entries (HW1, Cataclysm), or 36 (HW Remastered): the first name follows the TOC (bigfile.py)
    toc = malloc((size_t)numFiles * 36 + 1);
    if (numFiles > 0)
    {
        sdword sizes[2] = {32, 36}, k;

        entrySize = 0;
        for (k = 0; k < 2 && entrySize == 0; k++)
        {
            udword minOffset = 0xffffffffu;

            fseek(fp, tocStart, SEEK_SET);
            if (fread(toc, (size_t)sizes[k], (size_t)numFiles, fp) != (size_t)numFiles)
            {
                continue;
            }
            for (i = 0; i < numFiles; i++)
            {
                udword off = aeLE32(toc + i * sizes[k] + 20);

                minOffset = off < minOffset ? off : minOffset;
            }
            if (minOffset == tocStart + (udword)(sizes[k] * numFiles))
            {
                entrySize = sizes[k];
            }
        }
        if (entrySize == 0)
        {
            printf("assets: export: %s: unknown TOC layout\n", path);
            free(toc);
            fclose(fp);
            return FALSE;
        }
        fseek(fp, tocStart, SEEK_SET);
        if (fread(toc, (size_t)entrySize, (size_t)numFiles, fp) != (size_t)numFiles)
        {
            free(toc);
            fclose(fp);
            return FALSE;
        }
    }
    for (i = 0; i < numFiles; i++)
    {
        const ubyte *r = toc + i * entrySize;
        uword nameLength = aeLE16(r + 8);
        ubyte name[65536 + 1], mask = 213;
        aeentry *e;
        sdword k;

        if (t->n == *cap)
        {
            *cap = *cap ? *cap * 2 : 32768;
            t->e = realloc(t->e, sizeof(aeentry) * (size_t)*cap);
        }
        e = &t->e[t->n];
        e->stored = aeLE32(r + 12);
        e->real = aeLE32(r + 16);
        e->offset = aeLE32(r + 20);
        e->comp = r[entrySize == 32 ? 28 : 32];
        if ((long)e->offset + nameLength + 1 + (long)e->stored > fileSize)
        {
            continue;
        }
        fseek(fp, (long)e->offset, SEEK_SET);
        if (fread(name, 1, nameLength, fp) != nameLength)
        {
            continue;
        }
        for (k = 0; k < nameLength; k++)
        {
            name[k] ^= mask;
            mask = name[k];
        }
        e->key = aeNameKey(name, nameLength);
        e->archive = archive;
        e->order = i;
        e->offset += nameLength + 1;
        t->n++;
    }
    free(toc);
    t->fp[archive] = fp;
    return TRUE;
}

static bool32 aeTreeOpen(aetree *t, char **archives, sdword nArchives)
{
    sdword i, j, cap = 0;
    bool32 any = FALSE;

    memset(t, 0, sizeof(*t));
    t->nArchives = nArchives;
    for (i = 0; i < nArchives && i < AE_MaxArchives; i++)
    {
        t->path[i] = aeStrdup(archives[i]);
        any |= aeTreeAdd(t, archives[i], i, &cap);
    }
    if (!any)
    {
        return FALSE;
    }
    qsort(t->e, (size_t)t->n, sizeof(aeentry), aeEntryCmp);
    for (i = j = 0; i < t->n; i++)                          // the highest precedence wins (update.big first)
    {
        if (j > 0 && strcmp(t->e[j - 1].key, t->e[i].key) == 0)
        {
            free(t->e[i].key);
            continue;
        }
        t->e[j++] = t->e[i];
    }
    t->n = j;
    return TRUE;
}

static void aeTreeClose(aetree *t)
{
    sdword i;

    for (i = 0; i < t->n; i++)
    {
        free(t->e[i].key);
    }
    free(t->e);
    for (i = 0; i < AE_MaxArchives; i++)
    {
        if (t->fp[i])
        {
            fclose(t->fp[i]);
        }
        free(t->path[i]);
    }
    memset(t, 0, sizeof(*t));
}

static aeentry *aeTreeFind(aetree *t, const char *key)
{
    sdword lo = 0, hi = t->n - 1;

    while (lo <= hi)
    {
        sdword mid = (lo + hi) / 2;
        int c = strcmp(t->e[mid].key, key);

        if (c == 0)
        {
            return &t->e[mid];
        }
        if (c < 0)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }
    return NULL;
}

// the first entry whose key is >= key
static sdword aeTreeLower(aetree *t, const char *key)
{
    sdword lo = 0, hi = t->n;

    while (lo < hi)
    {
        sdword mid = (lo + hi) / 2;

        if (strcmp(t->e[mid].key, key) < 0)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }
    return lo;
}

// lzssExpandBuffer (tools/bigfile.py lzss_expand): its own window, so it is safe on this thread
static ubyte *aeLzss(const ubyte *src, size_t nSrc, size_t real, size_t *outLength)
{
    size_t cap = AE_Window + real + 32, n = AE_Window, ip = 0;
    ubyte *out = calloc(1, cap);
    uqword acc = 0;
    int nbits = 0;
    udword cur = 1;

    for (;;)
    {
        if (nbits < 17)
        {
            while (nbits <= 24 && ip < nSrc)
            {
                acc = (acc << 8) | src[ip++];
                nbits += 8;
            }
        }
        if (nbits < 1)
        {
            break;
        }
        nbits--;
        if ((acc >> nbits) & 1)
        {
            if (nbits < 8)
            {
                break;
            }
            nbits -= 8;
            if (n >= cap)
            {
                break;
            }
            out[n++] = (ubyte)((acc >> nbits) & 0xff);
            cur = (cur + 1) & 0xfff;
        }
        else
        {
            udword pos, length, d, i;
            size_t s;

            if (nbits < 12)
            {
                break;
            }
            nbits -= 12;
            pos = (udword)((acc >> nbits) & 0xfff);
            if (pos == 0)
            {
                break;                                      // END_OF_STREAM
            }
            if (nbits < 4)
            {
                break;
            }
            nbits -= 4;
            length = (udword)((acc >> nbits) & 0xf) + 2;
            d = (cur - pos) & 0xfff;
            if (d == 0)
            {
                d = AE_Window;
            }
            s = n - d;
            if (n + length > cap)
            {
                break;
            }
            for (i = 0; i < length; i++)
            {
                out[n + i] = out[s + i];
            }
            n += length;
            cur = (cur + length) & 0xfff;
        }
        acc &= (nbits > 0) ? ((((uqword)1) << nbits) - 1) : 0;
        if (n - AE_Window > real)
        {
            break;
        }
    }
    memmove(out, out + AE_Window, n - AE_Window);
    *outLength = n - AE_Window;
    return out;
}

static ubyte *aeTreeRead(aetree *t, aeentry *e, size_t *length)
{
    FILE *fp = t->fp[e->archive];
    ubyte *stored;

    if (fp == NULL)
    {
        return NULL;
    }
    stored = malloc((size_t)e->stored + 1);
    fseek(fp, (long)e->offset, SEEK_SET);
    if (fread(stored, 1, e->stored, fp) != e->stored)
    {
        free(stored);
        return NULL;
    }
    if (e->comp)
    {
        ubyte *data = aeLzss(stored, e->stored, e->real, length);

        free(stored);
        return data;
    }
    *length = e->stored;
    return stored;
}

static ubyte *aeTreeReadKey(aetree *t, const char *key, size_t *length)
{
    aeentry *e = aeTreeFind(t, key);

    return e ? aeTreeRead(t, e, length) : NULL;
}

/*=============================================================================
    .lif textures (hwformats.lif_read):
=============================================================================*/

typedef struct
{
    sdword w, h;
    udword flags;
    bool32 paletted;
    ubyte *rgba;
    ubyte *team[2];
}
aelif;

static void aeLifFree(aelif *lif)
{
    free(lif->rgba);
    free(lif->team[0]);
    free(lif->team[1]);
    memset(lif, 0, sizeof(*lif));
}

// NULL when decoded, else why not (FormatError)
static const char *aeLifDecode(const ubyte *d, size_t n, aelif *lif)
{
    sdword ver, w, h;
    udword flags, oData, oPal, oT0, oT1;
    size_t np;

    memset(lif, 0, sizeof(*lif));
    if (n < 0x30)
    {
        return "short .lif";
    }
    if (memcmp(d, "Willy 7\0", 8) != 0)
    {
        return "ident is not \"Willy 7\"";
    }
    ver = (sdword)aeLE32(d + 8);
    flags = aeLE32(d + 12);
    w = (sdword)aeLE32(d + 16);
    h = (sdword)aeLE32(d + 20);
    oData = aeLE32(d + 32);
    oPal = aeLE32(d + 36);
    oT0 = aeLE32(d + 40);
    oT1 = aeLE32(d + 44);
    if (ver != 0x104)
    {
        return "version is not 0x104";
    }
    if (!(w > 0 && w <= 4096 && h > 0 && h <= 4096))
    {
        return "bad size";
    }
    np = (size_t)w * (size_t)h;
    lif->w = w;
    lif->h = h;
    lif->flags = flags;
    if ((flags & TRF_Paletted) && !(flags & TRF_Alpha))
    {
        const ubyte *idx, *pal;
        size_t i;

        if (oData != 0x30 || oPal != oData + np || oT0 != oPal + 1024 || oT1 != oT0 + 256 || n != (size_t)oT1 + 256)
        {
            return "paletted block layout does not match the header offsets";
        }
        lif->paletted = TRUE;
        idx = d + oData;
        pal = d + oPal;
        lif->rgba = malloc(np * 4);
        for (i = 0; i < np; i++)
        {
            memcpy(lif->rgba + 4 * i, pal + 4 * idx[i], 4);
        }
        if (flags & TRF_TeamColor0)
        {
            lif->team[0] = malloc(np);
            for (i = 0; i < np; i++)
            {
                lif->team[0][i] = d[oT0 + idx[i]];
            }
        }
        if (flags & TRF_TeamColor1)
        {
            lif->team[1] = malloc(np);
            for (i = 0; i < np; i++)
            {
                lif->team[1][i] = d[oT1 + idx[i]];
            }
        }
    }
    else
    {
        size_t end = 0x30 + 4 * np;

        if (!(flags & TRF_Alpha))
        {
            return "neither Paletted nor Alpha flag set";
        }
        if (oData != 0x30)
        {
            return "RGBA data offset is not 0x30";
        }
        if (end > n)
        {
            return "RGBA blocks end past the file";
        }
        lif->rgba = malloc(np * 4);
        memcpy(lif->rgba, d + 0x30, np * 4);
        if (flags & TRF_TeamColor0)
        {
            if (oT0 != end || end + np > n)
            {
                aeLifFree(lif);
                return "team map 0 misplaced";
            }
            lif->team[0] = malloc(np);
            memcpy(lif->team[0], d + oT0, np);
            end += np;
        }
        if (flags & TRF_TeamColor1)
        {
            if (oT1 != end || end + np > n)
            {
                aeLifFree(lif);
                return "team map 1 misplaced";
            }
            lif->team[1] = malloc(np);
            memcpy(lif->team[1], d + oT1, np);
            end += np;
        }
        if (end != n)
        {
            aeLifFree(lif);
            return "RGBA blocks do not end at the end of the file";
        }
    }
    return NULL;
}

static bool32 aeHasAlpha(const aelif *lif)
{
    size_t i, np = (size_t)lif->w * (size_t)lif->h;

    for (i = 0; i < np; i++)
    {
        if (lif->rgba[4 * i + 3] != 255)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/*=============================================================================
    .geo / .peo meshes (hwformats.mesh_read):
=============================================================================*/

typedef struct
{
    double x, y, z;
    sdword ivn;
}
aevert;

typedef struct
{
    sdword ifn;
    udword i0, i1, i2, imat;
    double uv[6];
}
aepoly;

typedef struct
{
    char *name;
    sdword nv, nn, np;
    aevert *v;
    double (*n)[3];
    aepoly *p;
    sdword mother, daughter, sister;
    double m[16];
}
aeobject;

typedef struct
{
    udword ambient, diffuse, specular;
    char *texture;                  // NULL: none (oTexture 0)
    udword flags;
}
aematerial;

typedef struct
{
    aeobject *o;
    sdword no;
    aematerial *m;
    sdword nm;
}
aemesh;

static void aeMeshFree(aemesh *mesh)
{
    sdword i;

    for (i = 0; i < mesh->no; i++)
    {
        free(mesh->o[i].name);
        free(mesh->o[i].v);
        free(mesh->o[i].n);
        free(mesh->o[i].p);
    }
    for (i = 0; i < mesh->nm; i++)
    {
        free(mesh->m[i].texture);
    }
    free(mesh->o);
    free(mesh->m);
    memset(mesh, 0, sizeof(*mesh));
}

static char *aeCstr(const ubyte *d, size_t n, udword off)
{
    size_t e = off;

    while (e < n && d[e])
    {
        e++;
    }
    return off < n ? aeStrndup((const char *)d + off, e - off) : aeStrdup("");
}

static const char *aeMeshRead(const ubyte *d, size_t n, aemesh *mesh)
{
    udword nPub, nLoc, oPub, nObj, stride, i, k;
    sdword nn;

    memset(mesh, 0, sizeof(*mesh));
    if (n < 0x44)
    {
        return "short mesh file";
    }
    {
        size_t len = 8;                                     // ident.rstrip(b'\0') in ('RMF97ba', 'RMF99ba')

        while (len > 0 && d[len - 1] == 0)
        {
            len--;
        }
        if (!(len == 7 && (memcmp(d, "RMF97ba", 7) == 0 || memcmp(d, "RMF99ba", 7) == 0)))
        {
            return "ident is not RMF97ba/RMF99ba";
        }
    }
    nPub = aeLE32(d + 0x18);
    nLoc = aeLE32(d + 0x1c);
    oPub = aeLE32(d + 0x20);
    nObj = aeLE32(d + 0x28);
    if ((uqword)oPub == 0x44 + (uqword)nObj * 0x74)
    {
        stride = 0x74;
    }
    else if ((uqword)oPub == 0x44 + (uqword)nObj * 0x70)
    {
        stride = 0x70;
    }
    else
    {
        return "material table fits neither object stride";
    }
    if ((uqword)oPub + (uqword)(nPub + nLoc) * 0x20 > n || (uqword)0x44 + (uqword)nObj * stride > n)
    {
        return "tables run past the end of the file";
    }
    mesh->nm = (sdword)(nPub + nLoc);
    mesh->m = calloc((size_t)mesh->nm + 1, sizeof(aematerial));
    for (i = 0; i < nPub + nLoc; i++)
    {
        const ubyte *p = d + oPub + i * 0x20;
        udword oTex = aeLE32(p + 0x14);

        mesh->m[i].ambient = aeLE32(p + 4);
        mesh->m[i].diffuse = aeLE32(p + 8);
        mesh->m[i].specular = aeLE32(p + 12);
        mesh->m[i].texture = oTex ? aeCstr(d, n, oTex) : NULL;
        mesh->m[i].flags = aeLE16(p + 0x18);
    }
    mesh->no = (sdword)nObj;
    mesh->o = calloc((size_t)nObj + 1, sizeof(aeobject));
    for (i = 0; i < nObj; i++)
    {
        const ubyte *p = d + 0x44 + i * stride;
        aeobject *ob = &mesh->o[i];
        udword oName = aeLE32(p), pV, pN, pP, links[3];
        sdword nV = (sdword)aeLE32(p + 8), nFn = (sdword)aeLE32(p + 12), nVn = (sdword)aeLE32(p + 16);
        sdword nP = (sdword)aeLE32(p + 20);
        udword mOff;

        if (stride == 0x74)
        {
            pV = aeLE32(p + 0x18); pN = aeLE32(p + 0x20); pP = aeLE32(p + 0x24);
            links[0] = aeLE32(p + 0x28); links[1] = aeLE32(p + 0x2c); links[2] = aeLE32(p + 0x30);
            mOff = 0x34;
        }
        else
        {
            pV = aeLE32(p + 0x18); pN = aeLE32(p + 0x1c); pP = aeLE32(p + 0x20);
            links[0] = aeLE32(p + 0x24); links[1] = aeLE32(p + 0x28); links[2] = aeLE32(p + 0x2c);
            mOff = 0x30;
        }
        if (nV < 0 || nFn + nVn < 0 || nP < 0 || (uqword)pV + (uqword)nV * 16 > n ||
            (uqword)pN + (uqword)(nFn + nVn) * 16 > n || (uqword)pP + (uqword)nP * 40 > n)
        {
            return "a list runs past the end of the file";
        }
        ob->name = oName ? aeCstr(d, n, oName) : aeStrdup("");
        ob->nv = nV;
        ob->nn = nFn + nVn;
        ob->np = nP;
        ob->v = malloc(sizeof(aevert) * ((size_t)nV + 1));
        for (k = 0; k < (udword)nV; k++)
        {
            const ubyte *q = d + pV + k * 16;

            ob->v[k].x = aeLEf(q);
            ob->v[k].y = aeLEf(q + 4);
            ob->v[k].z = aeLEf(q + 8);
            ob->v[k].ivn = (sdword)aeLE32(q + 12);
        }
        ob->n = malloc(sizeof(double[3]) * ((size_t)ob->nn + 1));
        for (k = 0; k < (udword)ob->nn; k++)
        {
            const ubyte *q = d + pN + k * 16;

            ob->n[k][0] = aeLEf(q);
            ob->n[k][1] = aeLEf(q + 4);
            ob->n[k][2] = aeLEf(q + 8);
        }
        ob->p = malloc(sizeof(aepoly) * ((size_t)nP + 1));
        for (k = 0; k < (udword)nP; k++)
        {
            const ubyte *q = d + pP + k * 40;
            sdword j;

            ob->p[k].ifn = (sdword)aeLE32(q);
            ob->p[k].i0 = aeLE16(q + 4);
            ob->p[k].i1 = aeLE16(q + 6);
            ob->p[k].i2 = aeLE16(q + 8);
            ob->p[k].imat = aeLE16(q + 10);
            for (j = 0; j < 6; j++)
            {
                ob->p[k].uv[j] = aeLEf(q + 12 + 4 * j);
            }
        }
        for (k = 0; k < 3; k++)
        {
            sdword *dst = (k == 0) ? &ob->mother : (k == 1) ? &ob->daughter : &ob->sister;
            sqword rel = (sqword)links[k] - 0x44;
            sqword q, r;

            if (links[k] == 0)
            {
                *dst = -1;
                continue;
            }
            // Python: (ptr - 0x44) % stride or not 0 <= (ptr - 0x44) // stride < n_obj (floor division)
            q = rel / (sqword)stride;
            r = rel % (sqword)stride;
            if (r < 0)
            {
                r += stride;
                q -= 1;
            }
            if (r != 0 || q < 0 || q >= (sqword)nObj)
            {
                return "a hierarchy link is not an object boundary";
            }
            *dst = (sdword)q;
        }
        for (k = 0; k < 16; k++)
        {
            ob->m[k] = aeLEf(p + mOff + 4 * k);
        }
        nn = ob->nn;
        for (k = 0; k < (udword)nP; k++)
        {
            aepoly *q = &ob->p[k];

            if (!(q->ifn >= 0 && q->ifn < nn && q->i0 < (udword)nV && q->i1 < (udword)nV && q->i2 < (udword)nV &&
                  q->imat < (udword)mesh->nm))
            {
                return "a polygon index is out of range";
            }
        }
        for (k = 0; k < (udword)nV; k++)
        {
            if (!(ob->v[k].ivn >= 0 && ob->v[k].ivn < nn))
            {
                return "a vertex-normal index is out of range";
            }
        }
    }
    return NULL;
}

static const char *aeMeshLoad(aetree *t, const char *key, aemesh *mesh)
{
    size_t n;
    ubyte *d = aeTreeReadKey(t, key, &n);
    const char *why;

    if (d == NULL)
    {
        memset(mesh, 0, sizeof(*mesh));
        return "not in the archives";
    }
    why = aeMeshRead(d, n, mesh);
    free(d);
    if (why != NULL)
    {
        aeMeshFree(mesh);
    }
    return why;
}

static void aeMatMul(const double *a, const double *b, double *out)
{
    int c, r;

    for (c = 0; c < 4; c++)
    {
        for (r = 0; r < 4; r++)
        {
            out[c * 4 + r] = (a[0 * 4 + r] * b[c * 4 + 0] + a[1 * 4 + r] * b[c * 4 + 1] +
                              a[2 * 4 + r] * b[c * 4 + 2] + a[3 * 4 + r] * b[c * 4 + 3]);
        }
    }
}

// hwformats.world_matrices: the mother chain's product (meshObjectRenderR)
static void aeWorldGet(aemesh *mesh, double (*w)[16], bool32 *done, sdword i, sdword depth)
{
    aeobject *ob = &mesh->o[i];

    if (done[i])
    {
        return;
    }
    if (ob->mother < 0 || depth > 64)
    {
        memcpy(w[i], ob->m, sizeof(double) * 16);
    }
    else
    {
        aeWorldGet(mesh, w, done, ob->mother, depth + 1);
        aeMatMul(w[ob->mother], ob->m, w[i]);
    }
    done[i] = TRUE;
}

/*=============================================================================
    The writer (export_assets.Writer): never overwrites the player's files; journal + rename.
=============================================================================*/

typedef struct
{
    char *key;
    char *rel;
    uqword size;
    sqword mtime;
    char sha[65];
}
aerecord;

typedef struct
{
    aerecord *r;
    sdword n, cap;
    bool32 sorted;
}
aerecords;

typedef struct
{
    char folder[PATH_MAX];
    aetree tree;
    aerecords journal;              // what an unfinished export wrote (sorted by key)
    aerecords entries;              // this export's manifest
    FILE *journalFp;
    sdword written, unchanged, kept, keptUnlisted, failed;
    uqword bytes;
    // the passes' results
    char **pageSlots;               // "page\tsrc\tx\ty\tw\th\tt" lines are not used: see aeslot
}
aeexport;

static void aeRecordsAdd(aerecords *rs, const char *key, const char *rel, uqword size, sqword mtime, const char *sha)
{
    aerecord *r;

    if (rs->n == rs->cap)
    {
        rs->cap = rs->cap ? rs->cap * 2 : 1024;
        rs->r = realloc(rs->r, sizeof(aerecord) * (size_t)rs->cap);
    }
    r = &rs->r[rs->n++];
    r->key = aeStrdup(key);
    r->rel = aeStrdup(rel);
    r->size = size;
    r->mtime = mtime;
    snprintf(r->sha, sizeof(r->sha), "%s", sha);
    rs->sorted = FALSE;
}

static int aeRecordCmp(const void *a, const void *b)
{
    return strcmp(((const aerecord *)a)->key, ((const aerecord *)b)->key);
}

static void aeRecordsFree(aerecords *rs)
{
    sdword i;

    for (i = 0; i < rs->n; i++)
    {
        free(rs->r[i].key);
        free(rs->r[i].rel);
    }
    free(rs->r);
    memset(rs, 0, sizeof(*rs));
}

static aerecord *aeRecordsFind(aerecords *rs, const char *key)
{
    sdword lo = 0, hi = rs->n - 1;

    if (!rs->sorted)
    {
        qsort(rs->r, (size_t)rs->n, sizeof(aerecord), aeRecordCmp);
        rs->sorted = TRUE;
    }
    while (lo <= hi)
    {
        sdword mid = (lo + hi) / 2;
        int c = strcmp(rs->r[mid].key, key);

        if (c == 0)
        {
            return &rs->r[mid];
        }
        if (c < 0)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }
    return NULL;
}

// "<size> <mtime> <sha256> <path>" lines (AssetsFolder.c afManifestLoad, export_assets.read_manifest)
static void aeRecordsLoad(aerecords *rs, const char *path)
{
    size_t n;
    char *text = (char *)aeReadFile(path, &n), *line, *next;

    if (text == NULL)
    {
        return;
    }
    for (line = text; line && *line; line = next)
    {
        char *p, *q, *end, key[PATH_MAX], sha[65];
        uqword size;
        sqword mtime;

        next = strchr(line, '\n');
        if (next)
        {
            *next++ = 0;
        }
        end = line + strlen(line);
        while (end > line && (end[-1] == '\r'))
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
        q = p + 1;
        mtime = strtoll(q, &p, 10);
        if (p == q || *p != ' ')
        {
            continue;
        }
        q = p + 1;
        p = strchr(q, ' ');
        if (p == NULL || p - q != 64)
        {
            continue;
        }
        memcpy(sha, q, 64);
        sha[64] = 0;
        snprintf(key, sizeof(key), "%s", p + 1);
        aeLower(key);
        aeRecordsAdd(rs, key, p + 1, size, mtime, sha);
    }
    free(text);
}

static void aeFullPath(aeexport *ae, const char *rel, char *path, size_t size)
{
    char *p;

    snprintf(path, size, "%s/%s", ae->folder, rel);
#ifdef _WIN32
    for (p = path; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\\';
        }
    }
#else
    (void)p;
#endif
}

// FALSE when the file is already there as an unfinished export wrote it (its record is taken over): a resumed
// export skips building it
static bool32 aeWant(aeexport *ae, const char *rel)
{
    char key[PATH_MAX], path[PATH_MAX];
    aerecord *j;
    uqword size;
    sqword mtime;

    snprintf(key, sizeof(key), "%s", rel);
    aeLower(key);
    if (aeStartsWith(key, AE_BackgroundsKey))
    {
        return FALSE;
    }
    j = aeRecordsFind(&ae->journal, key);
    if (j == NULL)
    {
        return TRUE;
    }
    aeFullPath(ae, rel, path, sizeof(path));
    if (aeStat(path, &size, &mtime, NULL) && size == j->size && mtime == j->mtime)
    {
        aeRecordsAdd(&ae->entries, key, j->rel, j->size, j->mtime, j->sha);
        ae->unchanged++;
        ae->bytes += size;
        return FALSE;
    }
    return TRUE;
}

static void aeJournal(aeexport *ae, aerecord *r)
{
    if (ae->journalFp)
    {
        fprintf(ae->journalFp, "%llu %lld %s %s\n", (unsigned long long)r->size, (long long)r->mtime, r->sha, r->rel);
        fflush(ae->journalFp);
    }
}

static void aeWrite(aeexport *ae, const char *rel, const void *data, size_t length)
{
    char key[PATH_MAX], path[PATH_MAX], tmp[PATH_MAX], sha[65], cur[65], *slash;
    uqword size;
    sqword mtime;
    aerecord *j;
    FILE *fp;

    snprintf(key, sizeof(key), "%s", rel);
    aeLower(key);
    if (aeStartsWith(key, AE_BackgroundsKey))
    {
        return;                                             // the screen pictures are the player's (never written)
    }
    aeSha(data, length, sha);
    aeFullPath(ae, rel, path, sizeof(path));
    if (aeStat(path, &size, &mtime, NULL))
    {
        j = aeRecordsFind(&ae->journal, key);
        if (j == NULL)
        {
            if (size != length || !aeShaFile(path, cur) || strcmp(cur, sha) != 0)
            {
                ae->keptUnlisted++;                         // not ours: the player's file (it stays in use)
                if (ae->keptUnlisted <= 20)
                {
                    printf("assets: export: kept %s (not from an export)\n", rel);
                }
                return;
            }
            // the same content as the export: taken over (export_assets.py rewrites it)
            aeRecordsAdd(&ae->entries, key, rel, size, mtime, sha);
            aeJournal(ae, &ae->entries.r[ae->entries.n - 1]);
            ae->unchanged++;
            ae->bytes += length;
            return;
        }
        if (!aeShaFile(path, cur) || strcmp(cur, j->sha) != 0)
        {
            ae->kept++;                                     // changed since the export wrote it: the player's work
            if (ae->kept <= 20)
            {
                printf("assets: export: kept your %s\n", rel);
            }
            aeRecordsAdd(&ae->entries, key, j->rel, j->size, j->mtime, j->sha);
            return;
        }
        if (strcmp(cur, sha) == 0)
        {
            aeRecordsAdd(&ae->entries, key, rel, size, mtime, sha);
            aeJournal(ae, &ae->entries.r[ae->entries.n - 1]);
            ae->unchanged++;
            ae->bytes += length;
            return;
        }
    }
    // a temporary name the folder scans skip (a leading '.'), then a rename: never half a file
    {
        char dir[PATH_MAX];
        const char *name;

        snprintf(dir, sizeof(dir), "%s", path);
        slash = strrchr(dir, '/');
#ifdef _WIN32
        {
            char *bs = strrchr(dir, '\\');

            slash = (bs > slash) ? bs : slash;
        }
#endif
        if (slash == NULL)
        {
            ae->failed++;
            return;
        }
        name = path + (slash - dir) + 1;
        *slash = 0;
        if (!aeMakeDirs(dir))
        {
            ae->failed++;
            if (ae->failed <= 5)
            {
                printf("assets: export: cannot make the folder %s\n", dir);
            }
            return;
        }
        snprintf(tmp, sizeof(tmp), "%.*s.%s.hwi-export", (int)(name - path), path, name);
    }
    fp = fopen(tmp, "wb");
    if (fp == NULL || fwrite(data, 1, length, fp) != length)
    {
        if (fp)
        {
            fclose(fp);
        }
        remove(tmp);
        ae->failed++;
        if (ae->failed <= 5)
        {
            printf("assets: export: cannot write %s\n", path);
        }
        return;
    }
    if (fclose(fp) != 0 || !aeReplace(tmp, path) || !aeStat(path, &size, &mtime, NULL))
    {
        remove(tmp);
        ae->failed++;
        if (ae->failed <= 5)
        {
            printf("assets: export: cannot write %s\n", path);
        }
        return;
    }
    aeRecordsAdd(&ae->entries, key, rel, size, mtime, sha);
    aeJournal(ae, &ae->entries.r[ae->entries.n - 1]);
    ae->written++;
    ae->bytes += length;
}

static void aeWriteText(aeexport *ae, const char *rel, aebuf *text)
{
    aeWrite(ae, rel, text->d ? text->d : "", text->n);
}

// a PNG as hwformats.png_bytes writes it: 8 bits, grey (1) or RGBA (4), filter type 0 on every row (it compresses
// these textures best), one IDAT; the deflate is stb_image_write's, so the bytes differ from Python's, the pixels not
static udword aeCrcTable[256];

static void aeCrcInit(void)
{
    udword n, c;
    int k;

    for (n = 0; n < 256; n++)
    {
        c = n;
        for (k = 0; k < 8; k++)
        {
            c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
        }
        aeCrcTable[n] = c;
    }
}

static udword aeCrc(const ubyte *p, size_t n)
{
    udword c = 0xffffffffu;
    size_t i;

    for (i = 0; i < n; i++)
    {
        c = aeCrcTable[(c ^ p[i]) & 0xff] ^ (c >> 8);
    }
    return c ^ 0xffffffffu;
}

static void aePngChunk(aebuf *b, const char *tag, const ubyte *data, size_t n)
{
    ubyte head[8];
    ubyte *crcBuf = malloc(n + 4);
    udword crc;

    head[0] = (ubyte)(n >> 24); head[1] = (ubyte)(n >> 16); head[2] = (ubyte)(n >> 8); head[3] = (ubyte)n;
    memcpy(head + 4, tag, 4);
    aeBufAdd(b, head, 8);
    memcpy(crcBuf, tag, 4);
    if (n)
    {
        memcpy(crcBuf + 4, data, n);
        aeBufAdd(b, data, n);
    }
    crc = aeCrc(crcBuf, n + 4);
    free(crcBuf);
    head[0] = (ubyte)(crc >> 24); head[1] = (ubyte)(crc >> 16); head[2] = (ubyte)(crc >> 8); head[3] = (ubyte)crc;
    aeBufAdd(b, head, 4);
}

// an existing PNG the export did not write but with the export's size, colour type and pixels (e.g. written by
// tools/assets/export_assets.py, whose deflate differs): taken over as exported
static bool32 aePngAdopt(aeexport *ae, const char *rel, sdword w, sdword h, sdword comp, const ubyte *pixels)
{
    char key[PATH_MAX], path[PATH_MAX], sha[65];
    uqword size;
    sqword mtime;
    ubyte head[26];
    FILE *fp;
    int fw, fh, fn;
    ubyte *px;
    bool32 same;

    snprintf(key, sizeof(key), "%s", rel);
    aeLower(key);
    aeFullPath(ae, rel, path, sizeof(path));
    if (!aeStat(path, &size, &mtime, NULL) || aeRecordsFind(&ae->journal, key) != NULL)
    {
        return FALSE;
    }
    fp = fopen(path, "rb");
    if (fp == NULL)
    {
        return FALSE;
    }
    same = fread(head, 1, 26, fp) == 26 && memcmp(head, "\x89PNG\r\n\x1a\n", 8) == 0 && head[24] == 8 &&
           head[25] == ((comp == 4) ? 6 : 0);
    fclose(fp);
    if (!same)
    {
        return FALSE;
    }
    px = stbi_load(path, &fw, &fh, &fn, comp);
    same = px != NULL && fw == w && fh == h &&
           memcmp(px, pixels, (size_t)w * (size_t)h * (size_t)comp) == 0;
    stbi_image_free(px);
    if (!same || !aeShaFile(path, sha))
    {
        return FALSE;
    }
    aeRecordsAdd(&ae->entries, key, rel, size, mtime, sha);
    aeJournal(ae, &ae->entries.r[ae->entries.n - 1]);
    ae->unchanged++;
    ae->bytes += size;
    return TRUE;
}

static void aePngWrite(aeexport *ae, const char *rel, sdword w, sdword h, sdword comp, const ubyte *pixels)
{
    size_t stride = (size_t)w * (size_t)comp, y;
    ubyte *raw, ihdr[13];
    int zlen = 0;
    ubyte *z;
    aebuf b = {NULL, 0, 0};

    if (aePngAdopt(ae, rel, w, h, comp, pixels))
    {
        return;
    }
    raw = malloc((stride + 1) * (size_t)h + 1);

    for (y = 0; y < (size_t)h; y++)
    {
        raw[y * (stride + 1)] = 0;
        memcpy(raw + y * (stride + 1) + 1, pixels + y * stride, stride);
    }
    z = stbi_zlib_compress(raw, (int)((stride + 1) * (size_t)h), &zlen, 8);
    free(raw);
    if (z == NULL)
    {
        ae->failed++;
        return;
    }
    aeBufAdd(&b, "\x89PNG\r\n\x1a\n", 8);
    ihdr[0] = (ubyte)(w >> 24); ihdr[1] = (ubyte)(w >> 16); ihdr[2] = (ubyte)(w >> 8); ihdr[3] = (ubyte)w;
    ihdr[4] = (ubyte)(h >> 24); ihdr[5] = (ubyte)(h >> 16); ihdr[6] = (ubyte)(h >> 8); ihdr[7] = (ubyte)h;
    ihdr[8] = 8;
    ihdr[9] = (comp == 4) ? 6 : 0;
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    aePngChunk(&b, "IHDR", ihdr, 13);
    aePngChunk(&b, "IDAT", z, (size_t)zlen);
    aePngChunk(&b, "IEND", NULL, 0);
    free(z);
    aeWrite(ae, rel, b.d, b.n);
    aeBufFree(&b);
}

/*=============================================================================
    Export state shared by the passes:
=============================================================================*/

typedef struct
{
    char *page;                     // page key without .lif
    char *src;                      // source key without .lif
    sdword x, y, w, h, t;
}
aeslot;

typedef struct
{
    char *key;                      // the .lif key
    bool32 alpha;
    char *mask[2];                  // the masks' PNG paths, or NULL
    sdword w, h;
}
aeinfo;

typedef struct
{
    aeslot *s;
    sdword n, cap;
    aeinfo *info;
    sdword nInfo, capInfo;
    char **pageSources;             // sorted "<src>.lif" keys
    sdword nPageSources;
    sdword textures, paletted, rgba, nonPow2, masks, images, fromShareParent, pages, placements, inexact;
    sdword pageProblems, textureErrors, ships, objFiles;
    char *firstPageProblem;
}
aestate;

static SDL_atomic_t aeStopFlag;

static bool32 aeStopped(void)
{
    return SDL_AtomicGet(&aeStopFlag) != 0;
}

static void aeSlotAdd(aestate *st, const char *page, const char *src, sdword x, sdword y, sdword w, sdword h, sdword t)
{
    aeslot *s;

    if (st->n == st->cap)
    {
        st->cap = st->cap ? st->cap * 2 : 1024;
        st->s = realloc(st->s, sizeof(aeslot) * (size_t)st->cap);
    }
    s = &st->s[st->n++];
    s->page = aeStrdup(page);
    s->src = aeStrdup(src);
    s->x = x;
    s->y = y;
    s->w = w;
    s->h = h;
    s->t = t;
}

static int aeSlotCmp(const void *a, const void *b)
{
    const aeslot *x = a, *y = b;
    int c = strcmp(x->page, y->page);

    if (c) return c;
    if ((c = strcmp(x->src, y->src)) != 0) return c;
    if (x->x != y->x) return x->x < y->x ? -1 : 1;
    if (x->y != y->y) return x->y < y->y ? -1 : 1;
    if (x->w != y->w) return x->w < y->w ? -1 : 1;
    if (x->h != y->h) return x->h < y->h ? -1 : 1;
    if (x->t != y->t) return x->t < y->t ? -1 : 1;
    return 0;
}

static void aePageProblem(aestate *st, const char *text)
{
    st->pageProblems++;
    if (st->firstPageProblem == NULL)
    {
        st->firstPageProblem = aeStrdup(text);
    }
}

static int aeInfoCmp(const void *a, const void *b)
{
    return strcmp(((const aeinfo *)a)->key, ((const aeinfo *)b)->key);
}

static aeinfo *aeInfoFind(aestate *st, const char *key)
{
    aeinfo probe;

    probe.key = (char *)key;
    return bsearch(&probe, st->info, (size_t)st->nInfo, sizeof(aeinfo), aeInfoCmp);
}

/*=============================================================================
    Atlas pages (export_ships.Exporter.placements, export_assets.solve_pages):
=============================================================================*/

typedef struct
{
    char *name;
    aelif lif;
    bool32 ok;
}
aenamedlif;

typedef struct
{
    sdword x, y, t;
}
aecand;

typedef struct
{
    char *pt, *st;                  // page and source texture names
    aecand c[2];
    sdword nc;
    sdword polys;
}
aecandkey;

typedef struct
{
    char **s;
    sdword n, cap;
}
aenames;

static void aeNamesAdd(aenames *ns, const char *s)
{
    sdword i;

    for (i = 0; i < ns->n; i++)
    {
        if (strcmp(ns->s[i], s) == 0)
        {
            return;
        }
    }
    if (ns->n == ns->cap)
    {
        ns->cap = ns->cap ? ns->cap * 2 : 16;
        ns->s = realloc(ns->s, sizeof(char *) * (size_t)ns->cap);
    }
    ns->s[ns->n++] = aeStrdup(s);
}

static bool32 aeNamesHas(aenames *ns, const char *s)
{
    sdword i;

    for (i = 0; i < ns->n; i++)
    {
        if (strcmp(ns->s[i], s) == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static void aeNamesSort(aenames *ns)
{
    if (ns->n > 1)
    {
        qsort(ns->s, (size_t)ns->n, sizeof(char *), aeStrCmp);
    }
}

static void aeNamesFree(aenames *ns)
{
    sdword i;

    for (i = 0; i < ns->n; i++)
    {
        free(ns->s[i]);
    }
    free(ns->s);
    memset(ns, 0, sizeof(*ns));
}

// the lower-case texture names of a mesh's materials (those with a texture)
static void aeMeshTextures(aemesh *mesh, aenames *out)
{
    sdword i;

    for (i = 0; i < mesh->nm; i++)
    {
        if (mesh->m[i].texture && mesh->m[i].texture[0])
        {
            char *s = aeStrdup(mesh->m[i].texture);

            aeLower(s);
            aeNamesAdd(out, s);
            free(s);
        }
    }
    aeNamesSort(out);
}

static aenamedlif *aeLifsFind(aenamedlif *lifs, sdword n, const char *name)
{
    sdword i;

    for (i = 0; i < n; i++)
    {
        if (strcmp(lifs[i].name, name) == 0)
        {
            return &lifs[i];
        }
    }
    return NULL;
}

static ubyte *aeTranspose(const ubyte *buf, sdword w, sdword h, sdword bpp)
{
    ubyte *out = malloc((size_t)w * (size_t)h * (size_t)bpp + 1);
    sdword x, y;

    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            memcpy(out + ((size_t)x * h + y) * bpp, buf + ((size_t)y * w + x) * bpp, (size_t)bpp);
        }
    }
    return out;
}

// block(P, pw, x, y, w, h) == src
static bool32 aeBlockEquals(const ubyte *page, sdword pw, sdword x, sdword y, sdword w, sdword h, const ubyte *src)
{
    sdword r;

    for (r = 0; r < h; r++)
    {
        if (memcmp(page + (((size_t)(y + r) * pw) + x) * 4, src + (size_t)r * w * 4, (size_t)w * 4) != 0)
        {
            return FALSE;
        }
    }
    return TRUE;
}

typedef struct
{
    udword i0, i1, i2;
    sdword index;
}
aetri;

static int aeTriCmp(const void *a, const void *b)
{
    const aetri *x = a, *y = b;

    if (x->i0 != y->i0) return x->i0 < y->i0 ? -1 : 1;
    if (x->i1 != y->i1) return x->i1 < y->i1 ? -1 : 1;
    if (x->i2 != y->i2) return x->i2 < y->i2 ? -1 : 1;
    return x->index - y->index;
}

static int aeCandKeyCmp(const void *a, const void *b)
{
    const aecandkey *x = a, *y = b;
    int c = strcmp(x->pt, y->pt);

    return c ? c : strcmp(x->st, y->st);
}

static int aeCandCmp(const void *a, const void *b)
{
    const aecand *x = a, *y = b;

    if (x->x != y->x) return x->x < y->x ? -1 : 1;
    if (x->y != y->y) return x->y < y->y ? -1 : 1;
    return x->t - y->t;
}

static void aePlacements(aeexport *ae, aestate *st, aemesh *src, aemesh *game, aenames *pageNames,
                         aenames *packedNames, const char *lodDir)
{
    aenamedlif *lifs;
    sdword nLifs = 0, i, k, o;
    aecandkey *cand = NULL;
    sdword nCand = 0, capCand = 0, unmatched = 0;
    bool32 sameShape;
    aenames placed = {NULL, 0, 0};
    char text[PATH_MAX * 2];

    lifs = calloc((size_t)(pageNames->n + packedNames->n) + 1, sizeof(aenamedlif));
    for (k = 0; k < 2; k++)
    {
        aenames *ns = k == 0 ? pageNames : packedNames;

        for (i = 0; i < ns->n; i++)
        {
            char key[PATH_MAX];
            size_t n;
            ubyte *d;

            if (aeLifsFind(lifs, nLifs, ns->s[i]))
            {
                continue;
            }
            lifs[nLifs].name = aeStrdup(ns->s[i]);
            snprintf(key, sizeof(key), "%s/%s.lif", lodDir, ns->s[i]);
            d = aeTreeReadKey(&ae->tree, key, &n);
            lifs[nLifs].ok = d != NULL && aeLifDecode(d, n, &lifs[nLifs].lif) == NULL;
            free(d);
            nLifs++;
        }
    }
    sameShape = src->no == game->no;
    for (o = 0; sameShape && o < src->no; o++)
    {
        aeobject *a = &src->o[o], *b = &game->o[o];

        if (a->nv != b->nv)
        {
            sameShape = FALSE;
            break;
        }
        for (k = 0; k < a->nv; k++)
        {
            if (!(a->v[k].x == b->v[k].x && a->v[k].y == b->v[k].y && a->v[k].z == b->v[k].z &&
                  a->v[k].ivn == b->v[k].ivn))
            {
                sameShape = FALSE;
                break;
            }
        }
    }
    if (sameShape)
    {
        for (o = 0; o < src->no; o++)
        {
            aeobject *so = &src->o[o], *go = &game->o[o];
            aetri *tri = malloc(sizeof(aetri) * ((size_t)so->np + 1));

            for (k = 0; k < so->np; k++)
            {
                tri[k].i0 = so->p[k].i0;
                tri[k].i1 = so->p[k].i1;
                tri[k].i2 = so->p[k].i2;
                tri[k].index = k;
            }
            qsort(tri, (size_t)so->np, sizeof(aetri), aeTriCmp);
            for (k = 0; k < go->np; k++)
            {
                aepoly *gp = &go->p[k];
                const char *ptex = game->m[gp->imat].texture ? game->m[gp->imat].texture : "";
                char pt[PATH_MAX];
                aenamedlif *P;
                aetri probe;
                sdword lo, hi;
                bool32 hit = FALSE;

                snprintf(pt, sizeof(pt), "%s", ptex);
                aeLower(pt);
                P = aeLifsFind(lifs, nLifs, pt);
                if (!aeNamesHas(pageNames, pt) || P == NULL || !P->ok)
                {
                    continue;
                }
                probe.i0 = gp->i0;
                probe.i1 = gp->i1;
                probe.i2 = gp->i2;
                probe.index = -1;
                lo = 0;
                hi = so->np;
                while (lo < hi)
                {
                    sdword mid = (lo + hi) / 2;

                    if (aeTriCmp(&tri[mid], &probe) < 0)
                    {
                        lo = mid + 1;
                    }
                    else
                    {
                        hi = mid;
                    }
                }
                for (; lo < so->np && tri[lo].i0 == gp->i0 && tri[lo].i1 == gp->i1 && tri[lo].i2 == gp->i2; lo++)
                {
                    aepoly *sp = &so->p[tri[lo].index];
                    const char *stex = src->m[sp->imat].texture ? src->m[sp->imat].texture : "";
                    char stn[PATH_MAX];
                    aenamedlif *S;
                    aecand found[2];
                    sdword nFound = 0, tr;

                    snprintf(stn, sizeof(stn), "%s", stex);
                    aeLower(stn);
                    S = aeLifsFind(lifs, nLifs, stn);
                    if (!aeNamesHas(packedNames, stn) || S == NULL || !S->ok)
                    {
                        continue;
                    }
                    for (tr = 0; tr < 2; tr++)
                    {
                        sdword w = tr ? S->lif.h : S->lif.w, h = tr ? S->lif.w : S->lif.h, j;
                        double xs[3], ys[3], x, y;
                        bool32 good = TRUE;

                        for (j = 0; j < 3; j++)
                        {
                            double s = sp->uv[2 * j], t = sp->uv[2 * j + 1], a, b;

                            s = s < 0 ? s + 1.0 : s;
                            t = t < 0 ? t + 1.0 : t;
                            a = tr ? t : s;
                            b = tr ? s : t;
                            xs[j] = gp->uv[2 * j] * (double)P->lif.w - a * (double)w;
                            ys[j] = gp->uv[2 * j + 1] * (double)P->lif.h - b * (double)h;
                        }
                        x = rint(xs[0]);                    // Python round(): half to even
                        y = rint(ys[0]);
                        for (j = 0; j < 3; j++)
                        {
                            good &= fabs(xs[j] - x) < 0.02 && fabs(ys[j] - y) < 0.02;
                        }
                        if (good && 0 <= x && 0 <= y && x + w <= P->lif.w && y + h <= P->lif.h)
                        {
                            found[nFound].x = (sdword)x;
                            found[nFound].y = (sdword)y;
                            found[nFound].t = tr;
                            nFound++;
                        }
                    }
                    if (nFound)
                    {
                        aecandkey *ck = NULL;
                        sdword c;

                        for (c = 0; c < nCand; c++)
                        {
                            if (strcmp(cand[c].pt, pt) == 0 && strcmp(cand[c].st, stn) == 0)
                            {
                                ck = &cand[c];
                                break;
                            }
                        }
                        if (ck == NULL)
                        {
                            if (nCand == capCand)
                            {
                                capCand = capCand ? capCand * 2 : 16;
                                cand = realloc(cand, sizeof(aecandkey) * (size_t)capCand);
                            }
                            ck = &cand[nCand++];
                            ck->pt = aeStrdup(pt);
                            ck->st = aeStrdup(stn);
                            memcpy(ck->c, found, sizeof(aecand) * (size_t)nFound);
                            ck->nc = nFound;
                            ck->polys = 0;
                        }
                        else
                        {
                            aecand keep[2];
                            sdword nKeep = 0, a2, b2;

                            for (a2 = 0; a2 < ck->nc; a2++)
                            {
                                for (b2 = 0; b2 < nFound; b2++)
                                {
                                    if (ck->c[a2].x == found[b2].x && ck->c[a2].y == found[b2].y &&
                                        ck->c[a2].t == found[b2].t)
                                    {
                                        keep[nKeep++] = ck->c[a2];
                                        break;
                                    }
                                }
                            }
                            memcpy(ck->c, keep, sizeof(aecand) * (size_t)nKeep);
                            ck->nc = nKeep;
                        }
                        ck->polys++;
                        hit = TRUE;
                    }
                }
                if (!hit)
                {
                    unmatched++;
                }
            }
            free(tri);
        }
    }
    else
    {
        snprintf(text, sizeof(text), "%s: source and in-game meshes differ in objects/vertices", lodDir);
        aePageProblem(st, text);
    }
    if (unmatched)
    {
        snprintf(text, sizeof(text), "%s: %d in-game polygons on a page could not be matched to a source texture by UV",
                 lodDir, (int)unmatched);
        aePageProblem(st, text);
    }
    if (nCand > 1)
    {
        qsort(cand, (size_t)nCand, sizeof(aecandkey), aeCandKeyCmp);
    }
    for (i = 0; i < nCand; i++)
    {
        aenamedlif *P = aeLifsFind(lifs, nLifs, cand[i].pt), *S = aeLifsFind(lifs, nLifs, cand[i].st);
        sdword bx = 0, by = 0, bt = 0, best = 0, c;
        bool32 bestExact = FALSE;
        char pageKey[PATH_MAX], srcKey[PATH_MAX];

        if (cand[i].nc > 1)
        {
            qsort(cand[i].c, (size_t)cand[i].nc, sizeof(aecand), aeCandCmp);
        }
        for (c = 0; c < cand[i].nc; c++)
        {
            sdword tr = cand[i].c[c].t;
            ubyte *srcPix = tr ? aeTranspose(S->lif.rgba, S->lif.w, S->lif.h, 4) : S->lif.rgba;
            sdword w = tr ? S->lif.h : S->lif.w, h = tr ? S->lif.w : S->lif.h;
            bool32 exact = aeBlockEquals(P->lif.rgba, P->lif.w, cand[i].c[c].x, cand[i].c[c].y, w, h, srcPix);

            if (tr)
            {
                free(srcPix);
            }
            if (!best || (exact && !bestExact))
            {
                best = 1;
                bx = cand[i].c[c].x;
                by = cand[i].c[c].y;
                bt = tr;
                bestExact = exact;
            }
        }
        if (!best)
        {
            snprintf(text, sizeof(text), "%s: UVs of %s on %s disagree between polygons", lodDir, cand[i].st,
                     cand[i].pt);
            aePageProblem(st, text);
            continue;
        }
        snprintf(pageKey, sizeof(pageKey), "%s/%s", lodDir, cand[i].pt);
        snprintf(srcKey, sizeof(srcKey), "%s/%s", lodDir, cand[i].st);
        aeSlotAdd(st, pageKey, srcKey, bx, by, bt ? S->lif.h : S->lif.w, bt ? S->lif.w : S->lif.h, bt);
        st->inexact += bestExact ? 0 : 1;
        aeNamesAdd(&placed, cand[i].st);
        if (!bestExact)
        {
            snprintf(text, sizeof(text), "%s/%s: the copy of %s inside %s differs from the current %s.lif; placement "
                     "taken from the UVs", lodDir, cand[i].pt, cand[i].st, cand[i].pt, cand[i].st);
            aePageProblem(st, text);
        }
    }
    // pixel search for packed textures the UV pass did not place
    for (i = 0; i < packedNames->n; i++)
    {
        aenamedlif *S = aeLifsFind(lifs, nLifs, packedNames->s[i]);
        bool32 hit = FALSE;
        sdword p;

        if (aeNamesHas(&placed, packedNames->s[i]) || S == NULL || !S->ok)
        {
            continue;
        }
        for (p = 0; p < pageNames->n && !hit; p++)
        {
            aenamedlif *P = aeLifsFind(lifs, nLifs, pageNames->s[p]);
            sdword tr;

            if (P == NULL || !P->ok)
            {
                continue;
            }
            for (tr = 0; tr < 2 && !hit; tr++)
            {
                ubyte *srcPix = tr ? aeTranspose(S->lif.rgba, S->lif.w, S->lif.h, 4) : S->lif.rgba;
                sdword w = tr ? S->lif.h : S->lif.w, h = tr ? S->lif.w : S->lif.h, y, x;

                for (y = 0; y + h <= P->lif.h && !hit; y++)
                {
                    for (x = 0; x + w <= P->lif.w; x++)
                    {
                        if (memcmp(P->lif.rgba + ((size_t)y * P->lif.w + x) * 4, srcPix, (size_t)w * 4) == 0 &&
                            aeBlockEquals(P->lif.rgba, P->lif.w, x, y, w, h, srcPix))
                        {
                            char pageKey[PATH_MAX], srcKey[PATH_MAX];

                            snprintf(pageKey, sizeof(pageKey), "%s/%s", lodDir, pageNames->s[p]);
                            snprintf(srcKey, sizeof(srcKey), "%s/%s", lodDir, packedNames->s[i]);
                            aeSlotAdd(st, pageKey, srcKey, x, y, w, h, tr);
                            hit = TRUE;
                            break;
                        }
                    }
                }
                if (tr)
                {
                    free(srcPix);
                }
            }
        }
        if (!hit)
        {
            snprintf(text, sizeof(text), "%s: packed texture %s not located in any page", lodDir, packedNames->s[i]);
            aePageProblem(st, text);
        }
    }
    for (i = 0; i < nCand; i++)
    {
        free(cand[i].pt);
        free(cand[i].st);
    }
    free(cand);
    for (i = 0; i < nLifs; i++)
    {
        free(lifs[i].name);
        aeLifFree(&lifs[i].lif);
    }
    free(lifs);
    aeNamesFree(&placed);
}

static void aeSolvePages(aeexport *ae, aestate *st)
{
    aetree *t = &ae->tree;
    sdword i;

    for (i = 0; i < t->n && !aeStopped(); i++)
    {
        const char *key = t->e[i].key;
        char base[PATH_MAX], geoKey[PATH_MAX], lodDir[PATH_MAX], *slash;
        aemesh src, game;
        const char *why;
        aenames srcNames = {NULL, 0, 0}, gameNames = {NULL, 0, 0}, pageNames = {NULL, 0, 0};
        aenames packed = {NULL, 0, 0};
        sdword k;

        if (!aeEndsWith(key, ".peo"))
        {
            continue;
        }
        snprintf(base, sizeof(base), "%.*s", (int)(strlen(key) - 4), key);
        snprintf(geoKey, sizeof(geoKey), "%s.geo", base);
        if (aeTreeFind(t, geoKey) == NULL)
        {
            continue;
        }
        if ((why = aeMeshLoad(t, geoKey, &src)) != NULL)
        {
            char text[PATH_MAX * 2];

            snprintf(text, sizeof(text), "%s: %s", key, why);
            aePageProblem(st, text);
            continue;
        }
        if ((why = aeMeshLoad(t, key, &game)) != NULL)
        {
            char text[PATH_MAX * 2];

            snprintf(text, sizeof(text), "%s: %s", key, why);
            aePageProblem(st, text);
            aeMeshFree(&src);
            continue;
        }
        snprintf(lodDir, sizeof(lodDir), "%s", base);
        slash = strrchr(lodDir, '/');
        if (slash)
        {
            *slash = 0;
        }
        else
        {
            lodDir[0] = 0;
        }
        aeMeshTextures(&src, &srcNames);
        aeMeshTextures(&game, &gameNames);
        for (k = 0; k < gameNames.n; k++)
        {
            char lifKey[PATH_MAX];

            snprintf(lifKey, sizeof(lifKey), "%s/%s.lif", lodDir, gameNames.s[k]);
            if (!aeNamesHas(&srcNames, gameNames.s[k]) && aeTreeFind(t, lifKey) != NULL)
            {
                aeNamesAdd(&pageNames, gameNames.s[k]);
            }
        }
        for (k = 0; k < srcNames.n; k++)
        {
            if (!aeNamesHas(&gameNames, srcNames.s[k]))
            {
                aeNamesAdd(&packed, srcNames.s[k]);
            }
        }
        aeNamesSort(&pageNames);
        aeNamesSort(&packed);
        if (pageNames.n)
        {
            aePlacements(ae, st, &src, &game, &pageNames, &packed, lodDir);
        }
        aeNamesFree(&srcNames);
        aeNamesFree(&gameNames);
        aeNamesFree(&pageNames);
        aeNamesFree(&packed);
        aeMeshFree(&src);
        aeMeshFree(&game);
    }
    // the set of slots: sorted, duplicates dropped (several meshes share a folder and its pages)
    if (st->n > 1)
    {
        sdword j = 0;

        qsort(st->s, (size_t)st->n, sizeof(aeslot), aeSlotCmp);
        for (i = 0; i < st->n; i++)
        {
            if (j > 0 && aeSlotCmp(&st->s[j - 1], &st->s[i]) == 0)
            {
                free(st->s[i].page);
                free(st->s[i].src);
                continue;
            }
            st->s[j++] = st->s[i];
        }
        st->n = j;
    }
    // the page sources (export_assets.export_textures draws them from their own file, not the share parent)
    st->pageSources = malloc(sizeof(char *) * ((size_t)st->n + 1));
    for (i = 0; i < st->n; i++)
    {
        char key[PATH_MAX];

        snprintf(key, sizeof(key), "%s.lif", st->s[i].src);
        st->pageSources[st->nPageSources++] = aeStrdup(key);
    }
    if (st->nPageSources > 1)
    {
        sdword j = 0;

        qsort(st->pageSources, (size_t)st->nPageSources, sizeof(char *), aeStrCmp);
        for (i = 0; i < st->nPageSources; i++)
        {
            if (j > 0 && strcmp(st->pageSources[j - 1], st->pageSources[i]) == 0)
            {
                free(st->pageSources[i]);
                continue;
            }
            st->pageSources[j++] = st->pageSources[i];
        }
        st->nPageSources = j;
    }
}

static void aeWritePages(aeexport *ae, aestate *st)
{
    aebuf b = {NULL, 0, 0};
    sdword i = 0;

    aeBufStr(&b, "# Homeworld-Infection atlas pages (tools/assets/export_assets.py): where each source texture sits in\n"
                 "# the page the game draws (Relic's packer \"Paco\"). The game rebuilds a page when one of its source\n"
                 "# textures changed. Do not edit.\n"
                 "# page <page texture> <width> <height>\n"
                 "# src <source texture> <x> <y> <w> <h> <transposed>   (page pixels; w x h as placed)\n");
    while (i < st->n)
    {
        sdword j = i;
        char lifKey[PATH_MAX];
        aeinfo *pinfo;

        while (j < st->n && strcmp(st->s[j].page, st->s[i].page) == 0)
        {
            j++;
        }
        snprintf(lifKey, sizeof(lifKey), "%s.lif", st->s[i].page);
        pinfo = aeInfoFind(st, lifKey);
        if (pinfo != NULL)
        {
            sdword k;

            aeBufPrintf(&b, "page %s %d %d\n", st->s[i].page, (int)pinfo->w, (int)pinfo->h);
            for (k = i; k < j; k++)
            {
                aeBufPrintf(&b, "src %s %d %d %d %d %d\n", st->s[k].src, (int)st->s[k].x, (int)st->s[k].y,
                            (int)st->s[k].w, (int)st->s[k].h, (int)st->s[k].t);
                st->placements++;
            }
            st->pages++;
        }
        i = j;
    }
    aeWriteText(ae, AE_PagesName, &b);
    aeBufFree(&b);
}

/*=============================================================================
    Textures (export_assets.export_textures) and pictures (export_images):
=============================================================================*/

typedef struct
{
    char *key, *parent;
}
aeshare;

static int aeShareCmp(const void *a, const void *b)
{
    return strcmp(((const aeshare *)a)->key, ((const aeshare *)b)->key);
}

typedef struct
{
    aeshare s;
    sdword order;
}
aeshareorder;

static int aeShareOrderCmp(const void *a, const void *b)
{
    const aeshareorder *x = a, *y = b;
    int c = strcmp(x->s.key, y->s.key);

    return c ? c : x->order - y->order;
}

// textures.ll: {texture .lif key: share parent .lif key} (export_assets.read_sharing)
static aeshare *aeReadSharing(aetree *t, sdword *count)
{
    size_t n;
    ubyte *d = aeTreeReadKey(t, "textures.ll", &n);
    aeshare *out = NULL;
    sdword num, i, k = 0;
    size_t sb;
    char **names;

    *count = 0;
    if (d == NULL)
    {
        return NULL;
    }
    if (n < 28 || memcmp(d, "Event13", 7) != 0)
    {
        free(d);
        return NULL;
    }
    num = (sdword)aeLE32(d + 12);                           // '<8siiiii': ident, version, count, ...
    sb = 28 + (size_t)num * 32;
    if (num < 0 || sb > n)
    {
        free(d);
        return NULL;
    }
    names = calloc((size_t)num + 1, sizeof(char *));
    for (i = 0; i < num; i++)
    {
        size_t off = sb + aeLE32(d + 28 + (size_t)i * 32), e = off;
        char *s, *p;

        while (e < n && d[e])
        {
            e++;
        }
        s = off < n ? aeStrndup((const char *)d + off, e - off) : aeStrdup("");
        for (p = s; *p; p++)
        {
            *p = (*p == '\\') ? '/' : (char)tolower((unsigned char)*p);
        }
        names[i] = s;
    }
    out = calloc((size_t)num + 1, sizeof(aeshare));
    for (i = 0; i < num; i++)
    {
        sdword parent = (sdword)aeLE32(d + 28 + (size_t)i * 32 + 28);

        if (parent >= 0 && parent < num)
        {
            size_t a = strlen(names[i]), b = strlen(names[parent]);

            out[k].key = malloc(a + 5);
            snprintf(out[k].key, a + 5, "%s.lif", names[i]);
            out[k].parent = malloc(b + 5);
            snprintf(out[k].parent, b + 5, "%s.lif", names[parent]);
            k++;
        }
    }
    for (i = 0; i < num; i++)
    {
        free(names[i]);
    }
    free(names);
    free(d);
    // Python's dict: the last entry of a key wins
    if (k > 1)
    {
        aeshareorder *tg = malloc(sizeof(aeshareorder) * (size_t)k);
        sdword j = 0;

        for (i = 0; i < k; i++)
        {
            tg[i].s = out[i];
            tg[i].order = i;
        }
        qsort(tg, (size_t)k, sizeof(aeshareorder), aeShareOrderCmp);
        for (i = 0; i < k; i++)
        {
            if (i + 1 < k && strcmp(tg[i].s.key, tg[i + 1].s.key) == 0)
            {
                free(tg[i].s.key);
                free(tg[i].s.parent);
                continue;
            }
            out[j++] = tg[i].s;
        }
        free(tg);
        k = j;
    }
    *count = k;
    return out;
}

static bool32 aeSorted(char **list, sdword n, const char *key)
{
    return bsearch(&key, list, (size_t)n, sizeof(char *), aeStrCmp) != NULL;
}

static void aeExportTextures(aeexport *ae, aestate *st)
{
    aetree *t = &ae->tree;
    sdword nShare = 0, i;
    aeshare *share = aeReadSharing(t, &nShare);
    static const char *maskSuffix[2] = {"_teambase", "_teamstripe"};

    for (i = 0; i < t->n && !aeStopped(); i++)
    {
        const char *key = t->e[i].key;
        const char *src = key, *why;
        aeshare probe, *sh;
        size_t n;
        ubyte *d;
        aelif lif;
        char stem[PATH_MAX], rel[PATH_MAX];
        aeinfo *inf;
        sdword m;

        if (!aeEndsWith(key, ".lif"))
        {
            continue;
        }
        probe.key = (char *)key;
        sh = share ? bsearch(&probe, share, (size_t)nShare, sizeof(aeshare), aeShareCmp) : NULL;
        if (sh != NULL && aeTreeFind(t, sh->parent) != NULL && !aeSorted(st->pageSources, st->nPageSources, key))
        {
            src = sh->parent;
        }
        d = aeTreeReadKey(t, src, &n);
        why = d ? aeLifDecode(d, n, &lif) : "not readable";
        free(d);
        if (why == NULL && src != key)
        {
            aelif own;
            ubyte *od = aeTreeReadKey(t, key, &n);
            const char *ownWhy = od ? aeLifDecode(od, n, &own) : "not readable";

            free(od);
            if (ownWhy != NULL)
            {
                aeLifFree(&lif);
                why = ownWhy;
            }
            else if (own.w != lif.w || own.h != lif.h)
            {
                aeLifFree(&lif);
                lif = own;
                src = key;
            }
            else
            {
                aeLifFree(&own);
            }
        }
        if (why != NULL)
        {
            st->textureErrors++;
            if (st->textureErrors <= 20)
            {
                printf("assets: export: texture %s: %s\n", key, why);
            }
            continue;
        }
        if (src != key)
        {
            st->fromShareParent++;
        }
        snprintf(stem, sizeof(stem), "%.*s", (int)(strlen(key) - 4), key);
        if (st->nInfo == st->capInfo)
        {
            st->capInfo = st->capInfo ? st->capInfo * 2 : 4096;
            st->info = realloc(st->info, sizeof(aeinfo) * (size_t)st->capInfo);
        }
        inf = &st->info[st->nInfo++];
        inf->key = aeStrdup(key);
        inf->alpha = aeHasAlpha(&lif);
        inf->mask[0] = inf->mask[1] = NULL;
        inf->w = lif.w;
        inf->h = lif.h;
        snprintf(rel, sizeof(rel), "%s.png", stem);
        if (aeWant(ae, rel))
        {
            aePngWrite(ae, rel, lif.w, lif.h, 4, lif.rgba);
        }
        for (m = 0; m < 2; m++)
        {
            char maskLif[PATH_MAX];

            if (lif.team[m] == NULL)
            {
                continue;
            }
            snprintf(maskLif, sizeof(maskLif), "%s%s.lif", stem, maskSuffix[m]);
            if (aeTreeFind(t, maskLif) != NULL)
            {
                continue;                                   // a real texture of that name (mask_collisions)
            }
            snprintf(rel, sizeof(rel), "%s%s.png", stem, maskSuffix[m]);
            if (aeWant(ae, rel))
            {
                aePngWrite(ae, rel, lif.w, lif.h, 1, lif.team[m]);
            }
            inf->mask[m] = aeStrdup(rel);
            st->masks++;
        }
        st->textures++;
        if (lif.paletted)
        {
            st->paletted++;
        }
        else
        {
            st->rgba++;
        }
        if ((lif.w & (lif.w - 1)) || (lif.h & (lif.h - 1)))
        {
            st->nonPow2++;
        }
        aeLifFree(&lif);
    }
    for (i = 0; i < nShare; i++)
    {
        free(share[i].key);
        free(share[i].parent);
    }
    free(share);
    // the keys came in sorted order already (the tree is sorted)
}

static void aeExportImages(aeexport *ae, aestate *st)
{
    aetree *t = &ae->tree;
    sdword i;

    for (i = 0; i < t->n && !aeStopped(); i++)
    {
        const char *key = t->e[i].key;
        size_t n;
        ubyte *d;

        if (!(aeEndsWith(key, ".jpg") || aeEndsWith(key, ".tga")))
        {
            continue;
        }
        st->images++;
        if (!aeWant(ae, key))
        {
            continue;
        }
        d = aeTreeRead(t, &t->e[i], &n);
        if (d != NULL)
        {
            aeWrite(ae, key, d, n);
            free(d);
        }
    }
}

/*=============================================================================
    Ship models (export_ships.Exporter.export_ship / write_obj, export_assets.ModelExporter / write_models):
=============================================================================*/

typedef struct
{
    sdword level;
    char *type;                     // upper case
    char *field;                    // pMeshFileN, or NULL
    char *base;                     // the mesh path without extension, or NULL (not a MESH level)
}
aelevel;

typedef struct
{
    char *ship;                     // race/ship
    sdword level;
    char *srcMesh, *obj, *gameMesh, *gameObj;
    sdword order;
}
aemodelline;

typedef struct
{
    aemodelline *l;
    sdword n, cap;
}
aemodellines;

static bool32 aeIsLineBreak(unsigned char c)
{
    // str.splitlines() on latin-1 text
    return c == '\n' || c == '\r' || c == 0x0b || c == 0x0c || c == 0x1c || c == 0x1d || c == 0x1e || c == 0x85;
}

static bool32 aeIsSpace(unsigned char c)
{
    // str.split() / strip() on latin-1 text
    return c == ' ' || (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x1f) || c == 0x85 || c == 0xa0;
}

static int aeLevelCmp(const void *a, const void *b)
{
    const aelevel *x = a, *y = b;

    return x->level - y->level;
}

static bool32 aeAllDigits(const char *s)
{
    if (*s == 0)
    {
        return FALSE;
    }
    for (; *s; s++)
    {
        if (!isdigit((unsigned char)*s))
        {
            return FALSE;
        }
    }
    return TRUE;
}

// os.path.splitext(rel)[0]
static void aeSplitExt(char *rel)
{
    char *base = strrchr(rel, '/'), *dot;

    base = base ? base + 1 : rel;
    while (*base == '.')
    {
        base++;
    }
    dot = strrchr(base, '.');
    if (dot != NULL)
    {
        *dot = 0;
    }
}

static aelevel *aeLodLevels(aetree *t, const char *race, const char *ship, sdword *count)
{
    char lodKey[PATH_MAX];
    size_t n;
    ubyte *d;
    aelevel *lv = NULL;
    sdword nLv = 0, cap = 0, i;

    *count = 0;
    snprintf(lodKey, sizeof(lodKey), "%s/%s.lod", race, ship);
    d = aeTreeReadKey(t, lodKey, &n);
    if (d != NULL)
    {
        size_t pos = 0;

        // hwformats.lod_read: {level: {type, mesh}} (later keys update earlier ones)
        while (pos < n)
        {
            size_t end = pos, a, b;
            char line[1024], *semi, key[256], val[1024];
            sdword level = -1, which = 0;

            while (end < n && !aeIsLineBreak(d[end]))
            {
                end++;
            }
            a = pos;
            b = end;
            if (end < n && d[end] == '\r' && end + 1 < n && d[end + 1] == '\n')
            {
                end++;
            }
            pos = end + 1;
            if (b - a >= sizeof(line))
            {
                b = a + sizeof(line) - 1;
            }
            memcpy(line, d + a, b - a);
            line[b - a] = 0;
            semi = strchr(line, ';');
            if (semi)
            {
                *semi = 0;
            }
            {
                char *s = line, *e;

                while (*s && aeIsSpace((unsigned char)*s))
                {
                    s++;
                }
                e = s + strlen(s);
                while (e > s && aeIsSpace((unsigned char)e[-1]))
                {
                    *--e = 0;
                }
                if (*s == 0 || *s == '[')
                {
                    continue;
                }
                // split(None, 1)
                {
                    char *k = s, *v;

                    while (*s && !aeIsSpace((unsigned char)*s))
                    {
                        s++;
                    }
                    if (*s == 0)
                    {
                        continue;
                    }
                    snprintf(key, sizeof(key), "%.*s", (int)(s - k), k);
                    v = s;
                    while (*v && aeIsSpace((unsigned char)*v))
                    {
                        v++;
                    }
                    if (*v == 0)
                    {
                        continue;
                    }
                    snprintf(val, sizeof(val), "%s", v);
                }
            }
            aeLower(key);
            if (strncmp(key, "type", 4) == 0 && aeAllDigits(key + 4))
            {
                level = atoi(key + 4);
                which = 1;
            }
            else if (strncmp(key, "pmeshfile", 9) == 0 && aeAllDigits(key + 9))
            {
                level = atoi(key + 9);
                which = 2;
            }
            else
            {
                continue;
            }
            for (i = 0; i < nLv && lv[i].level != level; i++)
            {
            }
            if (i == nLv)
            {
                if (nLv == cap)
                {
                    cap = cap ? cap * 2 : 8;
                    lv = realloc(lv, sizeof(aelevel) * (size_t)cap);
                }
                lv[nLv].level = level;
                lv[nLv].type = NULL;
                lv[nLv].field = NULL;
                lv[nLv].base = NULL;
                nLv++;
            }
            if (which == 1)
            {
                char *p;

                free(lv[i].type);
                lv[i].type = aeStrdup(val);
                for (p = lv[i].type; *p; p++)
                {
                    *p = (char)toupper((unsigned char)*p);
                }
            }
            else
            {
                free(lv[i].field);
                lv[i].field = aeStrdup(val);
            }
        }
        free(d);
        if (nLv > 1)
        {
            qsort(lv, (size_t)nLv, sizeof(aelevel), aeLevelCmp);
        }
        for (i = 0; i < nLv; i++)
        {
            if (lv[i].type == NULL)
            {
                lv[i].type = aeStrdup("?");
            }
            if (strcmp(lv[i].type, "MESH") == 0 && lv[i].field != NULL && lv[i].field[0])
            {
                char rel[PATH_MAX], *p;

                snprintf(rel, sizeof(rel), "%s/%s", race, lv[i].field);
                for (p = rel; *p; p++)
                {
                    *p = (*p == '\\') ? '/' : (char)tolower((unsigned char)*p);
                }
                aeSplitExt(rel);
                lv[i].base = aeStrdup(rel);
            }
        }
        *count = nLv;
        return lv;
    }
    // no .lod: the rl0/lodN folders (export_ships.lod_levels)
    {
        char prefix[PATH_MAX];
        sdword start, k;
        size_t pl;

        snprintf(prefix, sizeof(prefix), "%s/%s/rl0/lod", race, ship);
        pl = strlen(prefix);
        start = aeTreeLower(t, prefix);
        for (k = start; k < t->n && strncmp(t->e[k].key, prefix, pl) == 0; k++)
        {
            const char *rest = t->e[k].key + pl, *p = rest;
            const char *slash, *name;
            sdword level;
            char base[PATH_MAX];

            while (isdigit((unsigned char)*p))
            {
                p++;
            }
            if (p == rest || *p != '/')
            {
                continue;
            }
            level = atoi(rest);
            name = p + 1;
            slash = strchr(name, '/');
            if (slash != NULL || !(aeEndsWith(name, ".geo") || aeEndsWith(name, ".peo")) || strlen(name) <= 4)
            {
                continue;
            }
            snprintf(base, sizeof(base), "%s/%s/rl0/lod%d/%.*s", race, ship, (int)level, (int)(strlen(name) - 4), name);
            for (i = 0; i < nLv; i++)
            {
                if (lv[i].level == level && strcmp(lv[i].base, base) == 0)
                {
                    break;
                }
            }
            if (i < nLv)
            {
                continue;
            }
            if (nLv == cap)
            {
                cap = cap ? cap * 2 : 8;
                lv = realloc(lv, sizeof(aelevel) * (size_t)cap);
            }
            lv[nLv].level = level;
            lv[nLv].type = aeStrdup("MESH");
            lv[nLv].field = NULL;
            lv[nLv].base = aeStrdup(base);
            nLv++;
        }
        // sorted((level, base))
        for (i = 1; i < nLv; i++)
        {
            aelevel x = lv[i];
            sdword p = i - 1;

            while (p >= 0 && (lv[p].level > x.level || (lv[p].level == x.level && strcmp(lv[p].base, x.base) > 0)))
            {
                lv[p + 1] = lv[p];
                p--;
            }
            lv[p + 1] = x;
        }
    }
    *count = nLv;
    return lv;
}

// os.path.relpath(out/target, out/<dir of rel>): every exported PNG path starts outside Models/
static void aeRelFromModelsDir(const char *mtlRel, const char *target, char *out, size_t size)
{
    sdword depth = 0;
    const char *p;
    size_t n = 0;

    for (p = mtlRel; *p; p++)
    {
        depth += (*p == '/');
    }
    out[0] = 0;
    while (depth-- > 0 && n + 3 < size)
    {
        memcpy(out + n, "../", 3);
        n += 3;
        out[n] = 0;
    }
    snprintf(out + n, size - n, "%s", target);
}

typedef struct
{
    char *png;                      // NULL: could not be exported
    bool32 alpha;
    char *mask[2];
}
aetexrec;

typedef struct
{
    char *name;                     // lower case texture name
    aetexrec rec;
}
aetexname;

// export_ships.Exporter.write_obj (the Assets layout of export_assets.ModelExporter)
static void aeWriteObj(aeexport *ae, aemesh *mesh, const char *objRel, aetexname *tex, sdword nTex,
                       const char *header1, const char *header2)
{
    char mtlRel[PATH_MAX], mtlBase[PATH_MAX], num[3][512];
    aebuf obj = {NULL, 0, 0}, mtl = {NULL, 0, 0};
    double (*W)[16] = malloc(sizeof(double[16]) * ((size_t)mesh->no + 1));
    bool32 *done = calloc((size_t)mesh->no + 1, sizeof(bool32));
    char **matName = calloc((size_t)mesh->nm + 1, sizeof(char *));
    // the merged materials: name -> the first material index with that key
    typedef struct { char *name; sdword mi; } aemtl;
    aemtl *mtls = calloc((size_t)mesh->nm + 1, sizeof(aemtl));
    sdword nMtl = 0, i, o, k;
    sdword nv = 0, nvt = 0, nvn = 0;
    bool32 *used = calloc((size_t)mesh->nm + 1, sizeof(bool32));
    aenames objNames = {NULL, 0, 0};
    const char *slash;

    snprintf(mtlRel, sizeof(mtlRel), "%.*s.mtl", (int)(strlen(objRel) - 4), objRel);
    slash = strrchr(mtlRel, '/');
    snprintf(mtlBase, sizeof(mtlBase), "%s", slash ? slash + 1 : mtlRel);
    for (i = 0; i < mesh->no; i++)
    {
        aeWorldGet(mesh, W, done, i, 0);
    }
    for (o = 0; o < mesh->no; o++)
    {
        for (k = 0; k < mesh->o[o].np; k++)
        {
            used[mesh->o[o].p[k].imat] = TRUE;
        }
    }
    // merge identical materials (same texture, flags, colours) and give them readable names
    for (i = 0; i < mesh->nm; i++)
    {
        aematerial *m = &mesh->m[i];
        sdword j;

        if (!used[i])
        {
            continue;
        }
        for (j = 0; j < nMtl; j++)
        {
            aematerial *q = &mesh->m[mtls[j].mi];
            const char *a = m->texture ? m->texture : "", *b = q->texture ? q->texture : "";

            if (strcasecmp(a, b) == 0 && m->flags == q->flags && m->ambient == q->ambient &&
                m->diffuse == q->diffuse && m->specular == q->specular)
            {
                break;
            }
        }
        if (j == nMtl)
        {
            char base[PATH_MAX], name[PATH_MAX], *p;
            sdword v = 2, c;

            if (m->texture && m->texture[0])
            {
                snprintf(base, sizeof(base), "%s", m->texture);
            }
            else
            {
                snprintf(base, sizeof(base), "untextured_%06x", (unsigned)(m->diffuse & 0xffffff));
            }
            aeLower(base);
            for (p = base; *p; p++)
            {
                if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '-') || (unsigned char)*p >= 0x80)
                {
                    *p = '_';
                }
            }
            if (m->flags & MDF_SelfIllum)
            {
                strcat(base, "_glow");
            }
            if (m->flags & MDF_2Sided)
            {
                strcat(base, "_2sided");
            }
            if (m->flags & MDF_Smoothing)
            {
                strcat(base, "_smooth");
            }
            snprintf(name, sizeof(name), "%s", base);
            for (;;)
            {
                for (c = 0; c < nMtl && strcmp(mtls[c].name, name) != 0; c++)
                {
                }
                if (c == nMtl)
                {
                    break;
                }
                snprintf(name, sizeof(name), "%s_v%d", base, (int)v++);
            }
            mtls[nMtl].name = aeStrdup(name);
            mtls[nMtl].mi = i;
            nMtl++;
        }
        matName[i] = mtls[j].name;
    }
    // MTL
    aeBufStr(&mtl, "# Homeworld: Cataclysm materials, written by tools/assets/export_ships.py\n");
    aeBufPrintf(&mtl, "# %s\n# %s\n", header1, header2);
    aeBufStr(&mtl, "# Ka/Kd/Ks come from the mesh material; the game multiplies the lit material colour by the texture.\n"
                   "# *_glow = self-illuminated in game (drawn unlit), *_2sided = no back-face culling,\n"
                   "# *_smooth = smooth shading (the OBJ normals already carry this).\n\n");
    {
        sdword *order = malloc(sizeof(sdword) * ((size_t)nMtl + 1));

        for (i = 0; i < nMtl; i++)
        {
            order[i] = i;
        }
        for (i = 1; i < nMtl; i++)
        {
            sdword x = order[i], p = i - 1;

            while (p >= 0 && strcmp(mtls[order[p]].name, mtls[x].name) > 0)
            {
                order[p + 1] = order[p];
                p--;
            }
            order[p + 1] = x;
        }
        for (i = 0; i < nMtl; i++)
        {
            aematerial *m = &mesh->m[mtls[order[i]].mi];
            udword cols[3] = {m->ambient, m->diffuse, m->specular};
            static const char *labels[3] = {"Ka", "Kd", "Ks"};
            sdword c;

            aeBufPrintf(&mtl, "newmtl %s\n", mtls[order[i]].name);
            for (c = 0; c < 3; c++)
            {
                aeFmt(num[0], sizeof(num[0]), (double)(cols[c] & 0xff) / 255.0);
                aeFmt(num[1], sizeof(num[1]), (double)((cols[c] >> 8) & 0xff) / 255.0);
                aeFmt(num[2], sizeof(num[2]), (double)((cols[c] >> 16) & 0xff) / 255.0);
                aeBufPrintf(&mtl, "%s %s %s %s\n", labels[c], num[0], num[1], num[2]);
            }
            aeBufStr(&mtl, "d 1\nillum 2\n");
            if (m->texture && m->texture[0])
            {
                char lower[PATH_MAX];
                aetexrec *rec = NULL;

                snprintf(lower, sizeof(lower), "%s", m->texture);
                aeLower(lower);
                for (c = 0; c < nTex; c++)
                {
                    if (strcmp(tex[c].name, lower) == 0)
                    {
                        rec = &tex[c].rec;
                        break;
                    }
                }
                if (rec && rec->png)
                {
                    char p[PATH_MAX];

                    aeRelFromModelsDir(mtlRel, rec->png, p, sizeof(p));
                    aeBufPrintf(&mtl, "map_Kd %s\n", p);
                    if (rec->alpha)
                    {
                        aeBufPrintf(&mtl, "map_d %s\n", p);
                    }
                    if (m->flags & MDF_SelfIllum)
                    {
                        aeBufPrintf(&mtl, "Ke 1 1 1\nmap_Ke %s\n", p);
                    }
                    if (rec->mask[0])
                    {
                        aeRelFromModelsDir(mtlRel, rec->mask[0], p, sizeof(p));
                        aeBufPrintf(&mtl, "# team base-colour mask: %s\n", p);
                    }
                    if (rec->mask[1])
                    {
                        aeRelFromModelsDir(mtlRel, rec->mask[1], p, sizeof(p));
                        aeBufPrintf(&mtl, "# team stripe-colour mask: %s\n", p);
                    }
                }
                else
                {
                    aeBufPrintf(&mtl, "# texture %s could not be exported (see manifest.json)\n", m->texture);
                }
            }
            aeBufStr(&mtl, "\n");
        }
        free(order);
    }
    // OBJ
    aeBufStr(&obj, "# Homeworld: Cataclysm mesh, written by tools/assets/export_ships.py\n");
    aeBufPrintf(&obj, "# %s\n# %s\n", header1, header2);
    aeBufStr(&obj, "# Vertices are in ship space with each object's hierarchy matrices applied (rest pose),\n"
                   "# axes turned for Blender: OBJ (x, y, z) = game (-y, x, z), i.e. game (x, y, z) = OBJ (y, -x, z).\n"
                   "# vt = (s, 1 - t): PNG row 0 is the first row of the .lif, which the game samples at t = 0.\n");
    aeBufPrintf(&obj, "mtllib %s\n", mtlBase);
    for (o = 0; o < mesh->no; o++)
    {
        aeobject *ob = &mesh->o[o];
        double *w = W[o];
        char oname[PATH_MAX];
        sdword vb = nv, nb = nvn;
        aebuf faces = {NULL, 0, 0};
        const char *cur = NULL;
        // the UV table of this object: unique (fmt(s), fmt(1 - t)) pairs in first-use order, with a hash of them
        char **uvKeys = malloc(sizeof(char *) * ((size_t)ob->np * 3 + 1));
        sdword nUv = 0;
        udword uvMask = 63;
        sdword *uvHash;

        while (uvMask + 1 < (udword)ob->np * 6 + 64)
        {
            uvMask = uvMask * 2 + 1;
        }
        uvHash = malloc(sizeof(sdword) * ((size_t)uvMask + 1));
        memset(uvHash, 0xff, sizeof(sdword) * ((size_t)uvMask + 1));

        // re.sub(r'\s+', '_', name or 'object%d')
        {
            const char *s = ob->name[0] ? ob->name : NULL;
            char tmp[PATH_MAX];
            size_t on = 0;

            if (s == NULL)
            {
                snprintf(tmp, sizeof(tmp), "object%d", (int)o);
                s = tmp;
            }
            while (*s && on + 1 < sizeof(oname))
            {
                if (aeIsSpace((unsigned char)*s))
                {
                    while (*s && aeIsSpace((unsigned char)*s))
                    {
                        s++;
                    }
                    oname[on++] = '_';
                    continue;
                }
                oname[on++] = *s++;
            }
            oname[on] = 0;
            if (aeNamesHas(&objNames, oname))
            {
                char again[PATH_MAX];

                snprintf(again, sizeof(again), "%s_%d", oname, (int)o);
                snprintf(oname, sizeof(oname), "%s", again);
            }
            aeNamesAdd(&objNames, oname);
        }
        aeBufPrintf(&obj, "o %s\n", oname);
        for (k = 0; k < ob->nv; k++)
        {
            double x = ob->v[k].x, y = ob->v[k].y, z = ob->v[k].z;
            double px = w[0] * x + w[4] * y + w[8] * z + w[12];
            double py = w[1] * x + w[5] * y + w[9] * z + w[13];
            double pz = w[2] * x + w[6] * y + w[10] * z + w[14];

            aeFmt(num[0], sizeof(num[0]), -py);
            aeFmt(num[1], sizeof(num[1]), px);
            aeFmt(num[2], sizeof(num[2]), pz);
            aeBufPrintf(&obj, "v %s %s %s\n", num[0], num[1], num[2]);
        }
        nv += ob->nv;
        for (k = 0; k < ob->nn; k++)
        {
            double x = ob->n[k][0], y = ob->n[k][1], z = ob->n[k][2];
            double nx = w[0] * x + w[4] * y + w[8] * z;
            double ny = w[1] * x + w[5] * y + w[9] * z;
            double nz = w[2] * x + w[6] * y + w[10] * z;
            double ln = sqrt(nx * nx + ny * ny + nz * nz);

            if (ln < 1e-12)
            {
                nx = 0.0;
                ny = 0.0;
                nz = 1.0;
            }
            else
            {
                nx = nx / ln;
                ny = ny / ln;
                nz = nz / ln;
            }
            aeFmt(num[0], sizeof(num[0]), -ny);
            aeFmt(num[1], sizeof(num[1]), nx);
            aeFmt(num[2], sizeof(num[2]), nz);
            aeBufPrintf(&obj, "vn %s %s %s\n", num[0], num[1], num[2]);
        }
        nvn += ob->nn;
        for (k = 0; k < ob->np; k++)
        {
            aepoly *p = &ob->p[k];
            aematerial *m = &mesh->m[p->imat];
            udword vi[3] = {p->i0, p->i1, p->i2};
            sdword c;

            if (cur == NULL || strcmp(matName[p->imat], cur) != 0)
            {
                cur = matName[p->imat];
                aeBufPrintf(&faces, "usemtl %s\n", cur);
            }
            aeBufStr(&faces, "f");
            for (c = 0; c < 3; c++)
            {
                char key[1100];
                sdword u, ni;

                aeFmt(num[0], sizeof(num[0]), p->uv[2 * c]);
                aeFmt(num[1], sizeof(num[1]), 1.0 - p->uv[2 * c + 1]);
                snprintf(key, sizeof(key), "%s %s", num[0], num[1]);
                {
                    udword h = 2166136261u, slot;
                    const char *q;

                    for (q = key; *q; q++)
                    {
                        h = (h ^ (ubyte)*q) * 16777619u;
                    }
                    for (slot = h & uvMask; uvHash[slot] >= 0 && strcmp(uvKeys[uvHash[slot]], key) != 0;
                         slot = (slot + 1) & uvMask)
                    {
                    }
                    if (uvHash[slot] < 0)
                    {
                        uvHash[slot] = nUv;
                        uvKeys[nUv++] = aeStrdup(key);
                    }
                    u = uvHash[slot];
                }
                ni = (m->flags & MDF_Smoothing) ? ob->v[vi[c]].ivn : p->ifn;
                aeBufPrintf(&faces, " %d/%d/%d", (int)(vb + (sdword)vi[c] + 1), (int)(nvt + u + 1), (int)(nb + ni + 1));
            }
            aeBufStr(&faces, "\n");
        }
        for (k = 0; k < nUv; k++)
        {
            aeBufPrintf(&obj, "vt %s\n", uvKeys[k]);
            free(uvKeys[k]);
        }
        free(uvKeys);
        free(uvHash);
        nvt += nUv;
        if (faces.n)
        {
            aeBufAdd(&obj, faces.d, faces.n);
        }
        aeBufFree(&faces);
    }
    aeWriteText(ae, objRel, &obj);
    aeWriteText(ae, mtlRel, &mtl);
    aeBufFree(&obj);
    aeBufFree(&mtl);
    for (i = 0; i < nMtl; i++)
    {
        free(mtls[i].name);
    }
    free(mtls);
    free(matName);
    free(used);
    free(W);
    free(done);
    aeNamesFree(&objNames);
}

static void aeTexRecFor(aestate *st, const char *lodDir, const char *name, aetexname *out)
{
    char key[PATH_MAX];
    aeinfo *inf;

    out->name = aeStrdup(name);
    snprintf(key, sizeof(key), "%s/%s.lif", lodDir, name);
    inf = aeInfoFind(st, key);
    if (inf == NULL)
    {
        memset(&out->rec, 0, sizeof(out->rec));
        return;
    }
    snprintf(key, sizeof(key), "%s/%s.png", lodDir, name);
    out->rec.png = aeStrdup(key);
    out->rec.alpha = inf->alpha;
    out->rec.mask[0] = inf->mask[0] ? aeStrdup(inf->mask[0]) : NULL;
    out->rec.mask[1] = inf->mask[1] ? aeStrdup(inf->mask[1]) : NULL;
}

static void aeTexNamesFree(aetexname *t, sdword n)
{
    sdword i;

    for (i = 0; i < n; i++)
    {
        free(t[i].name);
        free(t[i].rec.png);
        free(t[i].rec.mask[0]);
        free(t[i].rec.mask[1]);
    }
    free(t);
}

static void aeModelLineAdd(aemodellines *ls, const char *ship, sdword level, const char *srcMesh, const char *obj,
                           const char *gameMesh, const char *gameObj)
{
    aemodelline *l;

    if (ls->n == ls->cap)
    {
        ls->cap = ls->cap ? ls->cap * 2 : 256;
        ls->l = realloc(ls->l, sizeof(aemodelline) * (size_t)ls->cap);
    }
    l = &ls->l[ls->n++];
    l->ship = aeStrdup(ship);
    l->level = level;
    l->srcMesh = aeStrdup(srcMesh);
    l->obj = aeStrdup(obj);
    l->gameMesh = aeStrdup(gameMesh);
    l->gameObj = aeStrdup(gameObj);
}

typedef struct
{
    char *srcRel;
    char *obj, *gameObj;            // gameObj NULL: none
}
aefirst;

static void aeExportShip(aeexport *ae, aestate *st, const char *race, const char *ship, aemodellines *lines)
{
    aetree *t = &ae->tree;
    sdword nLv = 0, i, j;
    aelevel *lv = aeLodLevels(t, race, ship, &nLv);
    aefirst *first = calloc((size_t)nLv + 1, sizeof(aefirst));
    sdword nFirst = 0;
    char shipKey[PATH_MAX];

    snprintf(shipKey, sizeof(shipKey), "%s/%s", race, ship);
    for (i = 0; i < nLv && !aeStopped(); i++)
    {
        char geoKey[PATH_MAX], peoKey[PATH_MAX], srcRel[PATH_MAX], gameRel[PATH_MAX], lodDir[PATH_MAX];
        char stem[PATH_MAX], objRel[PATH_MAX], igRel[PATH_MAX], header1[PATH_MAX], header2[PATH_MAX], *slash;
        const char *baseName;
        bool32 hasGeo, hasPeo;
        aemesh src, game;
        sdword basesThisLevel = 0;
        aenames srcNames = {NULL, 0, 0}, gameNames = {NULL, 0, 0};
        aetexname *tex;
        sdword nTex = 0, f;

        if (lv[i].base == NULL)
        {
            continue;
        }
        snprintf(geoKey, sizeof(geoKey), "%s.geo", lv[i].base);
        snprintf(peoKey, sizeof(peoKey), "%s.peo", lv[i].base);
        hasGeo = aeTreeFind(t, geoKey) != NULL;
        hasPeo = aeTreeFind(t, peoKey) != NULL;
        if (!hasGeo && !hasPeo)
        {
            continue;
        }
        snprintf(srcRel, sizeof(srcRel), "%s", hasGeo ? geoKey : peoKey);
        snprintf(gameRel, sizeof(gameRel), "%s", hasPeo ? peoKey : geoKey);
        for (f = 0; f < nFirst; f++)
        {
            if (strcmp(first[f].srcRel, srcRel) == 0)
            {
                break;
            }
        }
        if (f < nFirst)
        {
            // the .lod names the same mesh for several levels: exported once, pointed at
            aeModelLineAdd(lines, shipKey, lv[i].level, srcRel, first[f].obj, gameRel,
                           first[f].gameObj ? first[f].gameObj : "-");
            continue;
        }
        if (aeMeshLoad(t, srcRel, &src) != NULL)
        {
            continue;
        }
        if (strcmp(gameRel, srcRel) == 0)
        {
            game = src;
        }
        else if (aeMeshLoad(t, gameRel, &game) != NULL)
        {
            aeMeshFree(&src);
            continue;
        }
        snprintf(lodDir, sizeof(lodDir), "%s", lv[i].base);
        slash = strrchr(lodDir, '/');
        if (slash)
        {
            *slash = 0;
        }
        else
        {
            lodDir[0] = 0;
        }
        baseName = strrchr(lv[i].base, '/');
        baseName = baseName ? baseName + 1 : lv[i].base;
        for (j = 0; j < nLv; j++)
        {
            basesThisLevel += (lv[j].level == lv[i].level && lv[j].base != NULL);
        }
        if (strcmp(baseName, ship) == 0 || basesThisLevel == 1)
        {
            snprintf(stem, sizeof(stem), "%s", ship);
        }
        else
        {
            snprintf(stem, sizeof(stem), "%s_%s", ship, baseName);
        }
        aeMeshTextures(&src, &srcNames);
        aeMeshTextures(&game, &gameNames);
        tex = calloc((size_t)(srcNames.n + gameNames.n) + 1, sizeof(aetexname));
        for (j = 0; j < srcNames.n; j++)
        {
            aeTexRecFor(st, lodDir, srcNames.s[j], &tex[nTex++]);
        }
        snprintf(header1, sizeof(header1), "ship %s/%s, LOD %d", race, ship, (int)lv[i].level);
        snprintf(objRel, sizeof(objRel), "Models/%s/%s/%s_lod%d.obj", race, ship, stem, (int)lv[i].level);
        snprintf(header2, sizeof(header2), "source mesh %s", srcRel);
        aeWriteObj(ae, &src, objRel, tex, nTex, header1, header2);
        st->objFiles++;
        igRel[0] = 0;
        if (strcmp(gameRel, srcRel) != 0)
        {
            sdword nGameTex = nTex;

            for (j = 0; j < gameNames.n; j++)
            {
                if (!aeNamesHas(&srcNames, gameNames.s[j]))
                {
                    aeTexRecFor(st, lodDir, gameNames.s[j], &tex[nGameTex++]);
                }
            }
            snprintf(igRel, sizeof(igRel), "Models/%s/%s/ingame/%s_lod%d.obj", race, ship, stem, (int)lv[i].level);
            snprintf(header2, sizeof(header2), "in-game (paged) mesh %s", gameRel);
            aeWriteObj(ae, &game, igRel, tex, nGameTex, header1, header2);
            st->objFiles++;
            nTex = nGameTex;
        }
        aeModelLineAdd(lines, shipKey, lv[i].level, srcRel, objRel, gameRel, igRel[0] ? igRel : "-");
        first[nFirst].srcRel = aeStrdup(srcRel);
        first[nFirst].obj = aeStrdup(objRel);
        first[nFirst].gameObj = igRel[0] ? aeStrdup(igRel) : NULL;
        nFirst++;
        aeTexNamesFree(tex, nTex);
        aeNamesFree(&srcNames);
        aeNamesFree(&gameNames);
        if (game.o != src.o)
        {
            aeMeshFree(&game);
        }
        aeMeshFree(&src);
    }
    for (i = 0; i < nFirst; i++)
    {
        free(first[i].srcRel);
        free(first[i].obj);
        free(first[i].gameObj);
    }
    free(first);
    for (i = 0; i < nLv; i++)
    {
        free(lv[i].type);
        free(lv[i].field);
        free(lv[i].base);
    }
    free(lv);
    st->ships++;
}

static int aeModelLineCmp(const void *a, const void *b)
{
    const aemodelline *x = a, *y = b;
    int c = strcmp(x->ship, y->ship);

    return c ? c : x->order - y->order;
}

static void aeExportModels(aeexport *ae, aestate *st)
{
    aetree *t = &ae->tree;
    aemodellines lines = {NULL, 0, 0};
    sdword r, i;
    aebuf b = {NULL, 0, 0};

    for (r = 0; r < (sdword)(sizeof(aeRaces) / sizeof(aeRaces[0])) && !aeStopped(); r++)
    {
        char prefix[PATH_MAX];
        size_t pl;
        sdword k;
        aenames ships = {NULL, 0, 0};

        snprintf(prefix, sizeof(prefix), "%s/", aeRaces[r]);
        pl = strlen(prefix);
        for (k = aeTreeLower(t, prefix); k < t->n && strncmp(t->e[k].key, prefix, pl) == 0; k++)
        {
            const char *rest = t->e[k].key + pl, *slash = strchr(rest, '/');
            char ship[PATH_MAX];

            if (slash == NULL)
            {
                continue;                                   // k.count('/') >= 2
            }
            snprintf(ship, sizeof(ship), "%.*s", (int)(slash - rest), rest);
            if (ships.n == 0 || strcmp(ships.s[ships.n - 1], ship) != 0)
            {
                aeNamesAdd(&ships, ship);
            }
        }
        aeNamesSort(&ships);
        for (k = 0; k < ships.n && !aeStopped(); k++)
        {
            aeExportShip(ae, st, aeRaces[r], ships.s[k], &lines);
        }
        aeNamesFree(&ships);
    }
    if (aeStopped())
    {
        goto done;
    }
    // assets-models.txt (export_assets.write_models): sorted by ship, the levels in .lod order
    {
        aemodelline *sorted = malloc(sizeof(aemodelline) * ((size_t)lines.n + 1));
        sdword n = lines.n;

        memcpy(sorted, lines.l, sizeof(aemodelline) * (size_t)n);
        for (i = 0; i < n; i++)
        {
            sorted[i].order = i;
        }
        qsort(sorted, (size_t)n, sizeof(aemodelline), aeModelLineCmp);    // by ship, then the export order
        aeBufStr(&b, "# assets-models.txt: which OBJ is which game mesh (written by the export; do not edit)\n"
                     "# ship<TAB>level<TAB>source mesh<TAB>OBJ<TAB>in-game mesh<TAB>in-game OBJ (- = the game draws "
                     "the source mesh)\n");
        for (i = 0; i < n; i++)
        {
            aeBufPrintf(&b, "%s\t%d\t%s\t%s\t%s\t%s\n", sorted[i].ship, (int)sorted[i].level, sorted[i].srcMesh,
                        sorted[i].obj, sorted[i].gameMesh, sorted[i].gameObj);
        }
        free(sorted);
        aeWriteText(ae, AE_ModelsName, &b);
        aeBufFree(&b);
    }
done:
    for (i = 0; i < lines.n; i++)
    {
        free(lines.l[i].ship);
        free(lines.l[i].srcMesh);
        free(lines.l[i].obj);
        free(lines.l[i].gameMesh);
        free(lines.l[i].gameObj);
    }
    free(lines.l);
}

/*=============================================================================
    README.txt (export_assets.README_TEXT, byte for byte):
=============================================================================*/

#include "AssetsExportReadme.h"

/*=============================================================================
    The thread:
=============================================================================*/

static SDL_Thread *aeThread = NULL;
static SDL_atomic_t aeDone;
static bool32 aeOff = FALSE;
static bool32 aeWaitAtQuit = FALSE;
static aeexport *aeRunning = NULL;

static void aeWriteManifest(aeexport *ae, const char *archives)
{
    aebuf b = {NULL, 0, 0};
    char path[PATH_MAX], tmp[PATH_MAX], when[64];
    time_t now = time(NULL);
    struct tm *tmNow = localtime(&now);
    sdword i;
    FILE *fp;

    strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", tmNow);
    aeBufPrintf(&b, "# Homeworld-Infection Assets folder manifest (the game, exported %s from %s)\n", when, archives);
    aeBufStr(&b, "# The game uses a file of this folder only when this list does not name it or names another size or\n"
                 "# time: so an untouched export changes nothing. Do not edit; delete it and the game exports the\n"
                 "# folder again at its next start (your changed files are kept and stay in use).\n"
                 "# <size> <mtime (seconds)> <sha256> <path>\n");
    qsort(ae->entries.r, (size_t)ae->entries.n, sizeof(aerecord), aeRecordCmp);
    ae->entries.sorted = TRUE;
    for (i = 0; i < ae->entries.n; i++)
    {
        aerecord *r = &ae->entries.r[i];

        if (i > 0 && strcmp(r->key, ae->entries.r[i - 1].key) == 0)
        {
            continue;
        }
        aeBufPrintf(&b, "%llu %lld %s %s\n", (unsigned long long)r->size, (long long)r->mtime, r->sha, r->rel);
    }
    aeFullPath(ae, AE_ManifestName, path, sizeof(path));
    aeFullPath(ae, "." AE_ManifestName ".tmp", tmp, sizeof(tmp));
    fp = fopen(tmp, "wb");
    if (fp == NULL || fwrite(b.d, 1, b.n, fp) != b.n)
    {
        if (fp)
        {
            fclose(fp);
        }
        ae->failed++;
        aeBufFree(&b);
        return;
    }
    fclose(fp);
    if (!aeReplace(tmp, path))
    {
        ae->failed++;
    }
    aeBufFree(&b);
}

static int aeMain(void *data)
{
    aeexport *ae = data;
    aestate st;
    Uint64 t0 = SDL_GetPerformanceCounter();
    char path[PATH_MAX], archives[PATH_MAX * 2];
    sdword i, resumed;

    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
    aeCrcInit();
    memset(&st, 0, sizeof(st));
    archives[0] = 0;
    for (i = 0; i < ae->tree.nArchives; i++)
    {
        snprintf(archives + strlen(archives), sizeof(archives) - strlen(archives), "%s%s", i ? ", " : "",
                 ae->tree.path[i] ? ae->tree.path[i] : "?");
    }
    aeFullPath(ae, AE_JournalName, path, sizeof(path));
    aeRecordsLoad(&ae->journal, path);
    resumed = ae->journal.n;
    ae->journalFp = fopen(path, "ab");
    printf("assets: export: %s into %s (%d archive files%s)\n", resumed ? "resuming" : "started", ae->folder,
           (int)ae->tree.n, resumed ? ", an unfinished export's files kept" : "");
    fflush(stdout);
    if (ae->journalFp == NULL)
    {
        printf("assets: export: cannot write %s: stopped\n", path);
        goto finish;
    }
    aeSolvePages(ae, &st);
    if (!aeStopped())
    {
        aeExportTextures(ae, &st);
    }
    if (!aeStopped())
    {
        qsort(st.info, (size_t)st.nInfo, sizeof(aeinfo), aeInfoCmp);
        aeWritePages(ae, &st);
        aeExportImages(ae, &st);
    }
    if (!aeStopped())
    {
        aeExportModels(ae, &st);
    }
    if (!aeStopped())
    {
        aebuf b = {NULL, 0, 0};

        aeBufStr(&b, aeReadmeText);
        aeWriteText(ae, AE_ReadmeName, &b);
        aeBufFree(&b);
    }
    if (aeStopped())
    {
        printf("assets: export: stopped at quit after %d files (%d written); the next start resumes it\n",
               (int)ae->entries.n, (int)ae->written);
        goto finish;
    }
    aeWriteManifest(ae, archives);
    fclose(ae->journalFp);
    ae->journalFp = NULL;
    if (ae->failed == 0)
    {
        remove(path);                                       // the journal: the manifest has it all now
    }
    printf("assets: export: done in %.1f s: %d files in the manifest (written %d, already there %d, kept yours %d, "
           "kept not ours %d, failed %d), %.1f MB\n",
           (double)(SDL_GetPerformanceCounter() - t0) / (double)SDL_GetPerformanceFrequency(), (int)ae->entries.n,
           (int)ae->written, (int)ae->unchanged, (int)ae->kept, (int)ae->keptUnlisted, (int)ae->failed,
           (double)ae->bytes / 1e6);
    printf("assets: export: textures %d (paletted %d, rgba %d, not a power of two %d), team masks %d, other pictures "
           "%d; atlas pages %d with %d placements (%d not exact copies); textures from their share parent %d; ship "
           "models: %d ships, %d OBJ files; texture errors %d, page notes %d%s%s\n",
           (int)st.textures, (int)st.paletted, (int)st.rgba, (int)st.nonPow2, (int)st.masks, (int)st.images,
           (int)st.pages, (int)st.placements, (int)st.inexact, (int)st.fromShareParent, (int)st.ships,
           (int)st.objFiles, (int)st.textureErrors, (int)st.pageProblems, st.firstPageProblem ? " (first: " : "",
           st.firstPageProblem ? st.firstPageProblem : "");
    if (st.firstPageProblem)
    {
        printf(")\n");
    }

finish:
    fflush(stdout);
    if (ae->journalFp)
    {
        fclose(ae->journalFp);
        ae->journalFp = NULL;
    }
    for (i = 0; i < st.n; i++)
    {
        free(st.s[i].page);
        free(st.s[i].src);
    }
    free(st.s);
    for (i = 0; i < st.nInfo; i++)
    {
        free(st.info[i].key);
        free(st.info[i].mask[0]);
        free(st.info[i].mask[1]);
    }
    free(st.info);
    for (i = 0; i < st.nPageSources; i++)
    {
        free(st.pageSources[i]);
    }
    free(st.pageSources);
    free(st.firstPageProblem);
    aeTreeClose(&ae->tree);
    aeRecordsFree(&ae->journal);
    aeRecordsFree(&ae->entries);
    SDL_AtomicSet(&aeDone, 1);
    return 0;
}

// a folder of the game (it holds cataclysm.big) is never written (export_assets.check_out)
static bool32 aeIsGameFolder(const char *folder)
{
    char probe[PATH_MAX];

    snprintf(probe, sizeof(probe), "%s", folder);
    for (;;)
    {
        char big[PATH_MAX], *slash, *bs;

        snprintf(big, sizeof(big), "%s/cataclysm.big", probe);
        if (aeStat(big, NULL, NULL, NULL))
        {
            return TRUE;
        }
        snprintf(big, sizeof(big), "%s/Cataclysm.big", probe);
        if (aeStat(big, NULL, NULL, NULL))
        {
            return TRUE;
        }
        slash = strrchr(probe, '/');
        bs = strrchr(probe, '\\');
        slash = bs > slash ? bs : slash;
        if (slash == NULL || slash == probe)
        {
            return FALSE;
        }
        *slash = 0;
    }
}

bool32 assetsExportStart(const char *folder, char **archives, sdword nArchives)
{
    aeexport *ae;

    if (aeOff)
    {
        printf("assets: export: off (/noAssetsExport)\n");
        return FALSE;
    }
    if (aeThread != NULL || nArchives <= 0)
    {
        return FALSE;
    }
    if (aeIsGameFolder(folder))
    {
        printf("assets: export: %s is inside a game folder (cataclysm.big): not written\n", folder);
        return FALSE;
    }
    if (!aeMakeDirs(folder))
    {
        printf("assets: export: cannot make the folder %s\n", folder);
        return FALSE;
    }
    ae = calloc(1, sizeof(aeexport));
    snprintf(ae->folder, sizeof(ae->folder), "%s", folder);
    if (!aeTreeOpen(&ae->tree, archives, nArchives))
    {
        aeTreeClose(&ae->tree);
        free(ae);
        printf("assets: export: no archive could be read\n");
        return FALSE;
    }
    SDL_AtomicSet(&aeStopFlag, 0);
    SDL_AtomicSet(&aeDone, 0);
    aeRunning = ae;
    aeThread = SDL_CreateThread(aeMain, "assets-export", ae);
    if (aeThread == NULL)
    {
        aeTreeClose(&ae->tree);
        free(ae);
        aeRunning = NULL;
        printf("assets: export: no thread: %s\n", SDL_GetError());
        return FALSE;
    }
    return TRUE;
}

void assetsExportStop(void)
{
    Uint32 start;

    if (aeThread == NULL)
    {
        return;
    }
    if (!aeWaitAtQuit)
    {
        SDL_AtomicSet(&aeStopFlag, 1);
    }
    else if (!SDL_AtomicGet(&aeDone))
    {
        printf("assets: export: waiting for the export to finish (/catAssetsExportWait)\n");
        fflush(stdout);
    }
    // the thread stops between two files; a page of 4096 x 4096 is the longest step
    start = SDL_GetTicks();
    while (!SDL_AtomicGet(&aeDone) && (aeWaitAtQuit || SDL_GetTicks() - start < 10000))
    {
        SDL_Delay(5);
    }
    if (SDL_AtomicGet(&aeDone))
    {
        SDL_WaitThread(aeThread, NULL);
        free(aeRunning);
    }
    else
    {
        printf("assets: export: the thread did not stop in 10 s; left to the process exit\n");
        SDL_DetachThread(aeThread);
    }
    aeThread = NULL;
    aeRunning = NULL;
}

bool32 assetsExportOffSet(char *string)
{
    (void)string;
    aeOff = TRUE;
    return TRUE;
}

bool32 assetsExportWaitSet(char *string)
{
    (void)string;
    aeWaitAtQuit = TRUE;
    return TRUE;
}

bool32 assetsExportIsOff(void)
{
    return aeOff;
}
