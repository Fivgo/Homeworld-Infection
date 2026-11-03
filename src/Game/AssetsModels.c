/*=============================================================================
    AssetsModels.c: HWI-FEATURE(assets-models) ship models edited in Blender, read back into the game
    (docs/features/assets-models.md).

    The export (tools/assets/export_ships.py write_obj) writes each mesh level as an OBJ: one OBJ object per polygon
    object (its name), vertices in ship space with the hierarchy applied and the axes turned for Blender
    (OBJ (x, y, z) = game (-y, x, z)), vt = (s, 1 - t), the normals the game lights with, one material per distinct
    texture / flags / colours, named after the texture (+ _glow, _2sided, _smooth, _vN). Here that is undone, with the
    game's own mesh as the template: an OBJ object takes the template object of its name (name, flags, hierarchy and
    local matrix kept, so the .mex points, the .mad animations and the code still find their parts), a material its
    template material by name, and each vertex the template's per-vertex glow entry at its place. The result is the
    on-disk .geo image ("RMF97ba", 32-bit file offsets) that meshLoad reads like a file.

    Everything here uses malloc, not the game heap; nothing runs unless a model in the Assets folder was edited.
=============================================================================*/

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "AssetsModels.h"

#ifndef PATH_MAX
#define PATH_MAX 260
#endif

// the on-disk layout (docs/formats/mesh.md)
#define AM_HeaderSize       0x44
#define AM_ObjectSize       0x74
#define AM_MaterialSize     0x20
#define AM_VertexSize       16
#define AM_VertexInfoSize   16
#define AM_NormalSize       16
#define AM_PolygonSize      40

#define AM_MDF_Smoothing    0x02    // Mesh.h MDF_*
#define AM_MDF_2Sided       0x08
#define AM_MDF_SelfIllum    0x40

#define AM_MaxFileSize      (256 * 1024 * 1024)
#define AM_MaxIndex         65535   // polyentry vertex and material indices are uwords

/*=============================================================================
    Small helpers:
=============================================================================*/

static udword amLE32(const ubyte *p)
{
    return (udword)p[0] | ((udword)p[1] << 8) | ((udword)p[2] << 16) | ((udword)p[3] << 24);
}

static uword amLE16(const ubyte *p)
{
    return (uword)(p[0] | (p[1] << 8));
}

static real32 amLEReal(const ubyte *p)
{
    udword u = amLE32(p);
    real32 f;

    memcpy(&f, &u, 4);
    return f;
}

static void amPut32(ubyte *p, udword v)
{
    p[0] = (ubyte)v;
    p[1] = (ubyte)(v >> 8);
    p[2] = (ubyte)(v >> 16);
    p[3] = (ubyte)(v >> 24);
}

static void amPut16(ubyte *p, uword v)
{
    p[0] = (ubyte)v;
    p[1] = (ubyte)(v >> 8);
}

static void amPutReal(ubyte *p, real32 f)
{
    udword u;

    memcpy(&u, &f, 4);
    amPut32(p, u);
}

static char *amStrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *out = malloc(n);

    memcpy(out, s, n);
    return out;
}

// a growable array of fixed-size items
typedef struct
{
    void *data;
    sdword count, max;
    size_t size;
}
amarray;

static void amArrayInit(amarray *a, size_t size)
{
    a->data = NULL;
    a->count = a->max = 0;
    a->size = size;
}

static void *amArrayAdd(amarray *a)
{
    if (a->count == a->max)
    {
        a->max = a->max ? a->max * 2 : 64;
        a->data = realloc(a->data, a->size * (size_t)a->max);
    }
    memset((ubyte *)a->data + a->size * (size_t)a->count, 0, a->size);
    return (ubyte *)a->data + a->size * (size_t)a->count++;
}

#define amAt(a, type, i)    (((type *)(a).data)[i])

static void amArrayFree(amarray *a)
{
    free(a->data);
    amArrayInit(a, a->size);
}

// an open-addressing map of 64-bit keys to indices
typedef struct
{
    uqword *keys;
    sdword *values;
    udword mask;
}
ammap;

static void amMapInit(ammap *map, sdword expected)
{
    udword n = 64;

    while (n < (udword)expected * 2 + 16)
    {
        n *= 2;
    }
    map->keys = malloc(sizeof(uqword) * n);
    map->values = malloc(sizeof(sdword) * n);
    map->mask = n - 1;
    memset(map->values, 0xff, sizeof(sdword) * n);          // -1: empty
}

static udword amMapSlot(ammap *map, uqword key)
{
    uqword h = key * 0x9E3779B97F4A7C15ULL;
    udword i = (udword)(h >> 32) & map->mask;

    while (map->values[i] != -1 && map->keys[i] != key)
    {
        i = (i + 1) & map->mask;
    }
    return i;
}

static sdword amMapGet(ammap *map, uqword key)
{
    return map->values[amMapSlot(map, key)];
}

static void amMapPut(ammap *map, uqword key, sdword value)
{
    udword i = amMapSlot(map, key);

    map->keys[i] = key;
    map->values[i] = value;
}

static void amMapFree(ammap *map)
{
    free(map->keys);
    free(map->values);
    map->keys = NULL;
    map->values = NULL;
}

// a decimal number, locale-independent ("1.25", "-3e-2"); returns the end or NULL
static const char *amParseReal(const char *s, real64 *out)
{
    static const real64 pow10[23] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14,
                                     1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
    const char *p = s;
    bool32 negative = FALSE, digits = FALSE;
    uqword mantissa = 0;
    sdword exponent = 0, e = 0, nDigits = 0;
    real64 v;

    if (*p == '-' || *p == '+')
    {
        negative = (*p == '-');
        p++;
    }
    while (isdigit((unsigned char)*p))
    {
        digits = TRUE;
        if (nDigits < 18)
        {
            mantissa = mantissa * 10 + (uqword)(*p - '0');
            nDigits += (mantissa != 0);
        }
        else
        {
            exponent++;
        }
        p++;
    }
    if (*p == '.')
    {
        p++;
        while (isdigit((unsigned char)*p))
        {
            digits = TRUE;
            if (nDigits < 18)
            {
                mantissa = mantissa * 10 + (uqword)(*p - '0');
                nDigits += (mantissa != 0);
                exponent--;
            }
            p++;
        }
    }
    if (!digits)
    {
        if ((p[0] == 'n' || p[0] == 'N') && (p[1] == 'a' || p[1] == 'A') && (p[2] == 'n' || p[2] == 'N'))
        {
            return NULL;                                    // nan: refused
        }
        return NULL;
    }
    if (*p == 'e' || *p == 'E')
    {
        const char *q = p + 1;
        bool32 eNegative = FALSE;

        if (*q == '-' || *q == '+')
        {
            eNegative = (*q == '-');
            q++;
        }
        if (isdigit((unsigned char)*q))
        {
            while (isdigit((unsigned char)*q))
            {
                if (e < 10000)
                {
                    e = e * 10 + (*q - '0');
                }
                q++;
            }
            exponent += eNegative ? -e : e;
            p = q;
        }
    }
    v = (real64)mantissa;
    if (exponent < 0)
    {
        while (exponent < -22)
        {
            v /= 1e22;
            exponent += 22;
        }
        v /= pow10[-exponent];
    }
    else
    {
        while (exponent > 22)
        {
            v *= 1e22;
            exponent -= 22;
        }
        v *= pow10[exponent];
    }
    *out = negative ? -v : v;
    return isfinite(v) ? p : NULL;
}

static const char *amSkipSpace(const char *p)
{
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }
    return p;
}

// "name" with spaces trimmed, into out
static void amRestOfLine(const char *p, char *out, size_t size)
{
    size_t n;

    p = amSkipSpace(p);
    n = strlen(p);
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t'))
    {
        n--;
    }
    if (n >= size)
    {
        n = size - 1;
    }
    memcpy(out, p, n);
    out[n] = 0;
}

static char *amReadFile(const char *path, sdword *length)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *text;

    if (f == NULL)
    {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || n > AM_MaxFileSize || fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return NULL;
    }
    text = malloc((size_t)n + 1);
    if (fread(text, 1, (size_t)n, f) != (size_t)n)
    {
        free(text);
        fclose(f);
        return NULL;
    }
    fclose(f);
    text[n] = 0;
    if (length != NULL)
    {
        *length = (sdword)n;
    }
    return text;
}

/*=============================================================================
    Paths (lexical: '/' separators, "." and ".." resolved; compared without case):
=============================================================================*/

static void amPathNormalize(char *path)
{
    char *segments[256];
    sdword n = 0, i;
    char work[PATH_MAX], *p, *start;
    bool32 absolute;
    char prefix[8] = "";
    size_t len;

    snprintf(work, sizeof(work), "%s", path);
    for (p = work; *p; p++)
    {
        if (*p == '\\')
        {
            *p = '/';
        }
    }
    p = work;
    if (isalpha((unsigned char)p[0]) && p[1] == ':')       // a drive letter
    {
        prefix[0] = p[0];
        prefix[1] = ':';
        prefix[2] = 0;
        p += 2;
    }
    absolute = (*p == '/');
    while (*p)
    {
        while (*p == '/')
        {
            *p++ = 0;
        }
        if (!*p)
        {
            break;
        }
        start = p;
        while (*p && *p != '/')
        {
            p++;
        }
        if (*p)
        {
            *p++ = 0;
        }
        if (strcmp(start, ".") == 0)
        {
            continue;
        }
        if (strcmp(start, "..") == 0 && n > 0 && strcmp(segments[n - 1], "..") != 0)
        {
            n--;
            continue;
        }
        if (n < 256)
        {
            segments[n++] = start;
        }
    }
    len = (size_t)snprintf(path, PATH_MAX, "%s%s", prefix, absolute ? "/" : "");
    for (i = 0; i < n && len < PATH_MAX; i++)
    {
        len += (size_t)snprintf(path + len, PATH_MAX - len, "%s%s", i ? "/" : "", segments[i]);
    }
}

static bool32 amPathAbsolute(const char *path)
{
    return path[0] == '/' || path[0] == '\\' || (isalpha((unsigned char)path[0]) && path[1] == ':');
}

// dir of a file path (without the separator)
static void amPathDir(const char *path, char *out, size_t size)
{
    const char *slash = NULL, *p;

    for (p = path; *p; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            slash = p;
        }
    }
    if (slash == NULL)
    {
        snprintf(out, size, ".");
        return;
    }
    {
        size_t n = (size_t)(slash - path);

        if (n >= size)
        {
            n = size - 1;
        }
        memmove(out, path, n);                              // out may be path
        out[n] = 0;
    }
}

// a path relative to dir unless absolute, normalized
static void amPathJoin(const char *dir, const char *name, char *out)
{
    if (amPathAbsolute(name))
    {
        snprintf(out, PATH_MAX, "%s", name);
    }
    else
    {
        snprintf(out, PATH_MAX, "%s/%s", dir, name);
    }
    amPathNormalize(out);
}

static bool32 amPathEqual(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
        {
            return FALSE;
        }
        a++;
        b++;
    }
    return *a == *b;
}

// is path inside folder (both normalized)? returns the part below it
static const char *amPathBelow(const char *path, const char *folder)
{
    size_t n = strlen(folder);
    size_t i;

    for (i = 0; i < n; i++)
    {
        if (tolower((unsigned char)path[i]) != tolower((unsigned char)folder[i]))
        {
            return NULL;
        }
    }
    return (path[n] == '/') ? path + n + 1 : NULL;
}

static bool32 amFileExists(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG;
}

static bool32 amEndsWithNoCase(const char *s, const char *end)
{
    size_t n = strlen(s), m = strlen(end);

    return n >= m && amPathEqual(s + n - m, end);
}

/*=============================================================================
    The template (the game's own mesh, on-disk layout):
=============================================================================*/

typedef struct
{
    const char *name;
    ubyte flags;
    sdword mother, daughter, sister;
    real32 local[16];
    sdword nV, nFN, nVN, nP;
    udword oV, oVI, oN, oP;
    real64 world[16];               // column-major, the mother chain's product (as meshObjectRenderR composes)
    real64 inverse[16];             // world inverted (affine)
    char *exportName;               // the OBJ object name the export writes
}
amtobject;

typedef struct
{
    const char *name;
    udword ambient, diffuse, specular;
    real32 kAlpha;
    const char *texture;            // NULL: none
    uword flags;
    ubyte nFullAmbient;
    char *exportName;               // the MTL name the export writes (used materials only)
    sdword canonical;               // the material the export's name stands for
}
amtmaterial;

typedef struct
{
    const ubyte *d;
    sdword length;
    udword nPub, nLoc, oPub, oLoc, nObj;
    amtobject *obj;
    amtmaterial *mat;
    sdword nMat;
}
amtemplate;

static const char *amString(const ubyte *d, sdword length, udword offset)
{
    udword i;

    if (offset == 0 || offset >= (udword)length)
    {
        return NULL;
    }
    for (i = offset; i < (udword)length; i++)
    {
        if (d[i] == 0)
        {
            return (const char *)d + offset;
        }
    }
    return NULL;
}

// column-major 4x4 product a*b ([col*4+row])
static void amMatMul(const real64 *a, const real64 *b, real64 *out)
{
    sdword c, r;
    real64 t[16];

    for (c = 0; c < 4; c++)
    {
        for (r = 0; r < 4; r++)
        {
            t[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0] + a[1 * 4 + r] * b[c * 4 + 1] + a[2 * 4 + r] * b[c * 4 + 2] +
                           a[3 * 4 + r] * b[c * 4 + 3];
        }
    }
    memcpy(out, t, sizeof(t));
}

// affine inverse: the linear part inverted, the translation moved back
static bool32 amMatInverse(const real64 *m, real64 *out)
{
    real64 a = m[0], b = m[4], c = m[8];        // row 0
    real64 d = m[1], e = m[5], f = m[9];        // row 1
    real64 g = m[2], h = m[6], i = m[10];       // row 2
    real64 det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    real64 inv[9];
    sdword r;

    if (fabs(det) < 1e-12)
    {
        return FALSE;
    }
    inv[0] = (e * i - f * h) / det;
    inv[1] = (c * h - b * i) / det;
    inv[2] = (b * f - c * e) / det;
    inv[3] = (f * g - d * i) / det;
    inv[4] = (a * i - c * g) / det;
    inv[5] = (c * d - a * f) / det;
    inv[6] = (d * h - e * g) / det;
    inv[7] = (b * g - a * h) / det;
    inv[8] = (a * e - b * d) / det;
    memset(out, 0, sizeof(real64) * 16);
    for (r = 0; r < 3; r++)
    {
        out[0 * 4 + r] = inv[r * 3 + 0];
        out[1 * 4 + r] = inv[r * 3 + 1];
        out[2 * 4 + r] = inv[r * 3 + 2];
        out[3 * 4 + r] = -(inv[r * 3 + 0] * m[12] + inv[r * 3 + 1] * m[13] + inv[r * 3 + 2] * m[14]);
    }
    out[15] = 1.0;
    return TRUE;
}

static void amXformPoint(const real64 *m, const real64 *p, real64 *out)
{
    real64 x = p[0], y = p[1], z = p[2];

    out[0] = m[0] * x + m[4] * y + m[8] * z + m[12];
    out[1] = m[1] * x + m[5] * y + m[9] * z + m[13];
    out[2] = m[2] * x + m[6] * y + m[10] * z + m[14];
}

static void amXformVector(const real64 *m, const real64 *p, real64 *out)
{
    real64 x = p[0], y = p[1], z = p[2];

    out[0] = m[0] * x + m[4] * y + m[8] * z;
    out[1] = m[1] * x + m[5] * y + m[9] * z;
    out[2] = m[2] * x + m[6] * y + m[10] * z;
}

static real64 amLength(const real64 *v)
{
    return sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static bool32 amNormalize(real64 *v)
{
    real64 n = amLength(v);

    if (n < 1e-12)
    {
        return FALSE;
    }
    v[0] /= n;
    v[1] /= n;
    v[2] /= n;
    return TRUE;
}

// the export's object name: whitespace runs as '_', "object<N>" when empty, "<name>_<N>" when taken
static void amObjectExportNames(amtemplate *t)
{
    udword i, j;

    for (i = 0; i < t->nObj; i++)
    {
        const char *src = (t->obj[i].name != NULL && t->obj[i].name[0]) ? t->obj[i].name : NULL;
        char name[512];
        size_t n = 0;
        bool32 taken = FALSE;

        if (src == NULL)
        {
            snprintf(name, sizeof(name), "object%d", (int)i);
        }
        else
        {
            while (*src && n < sizeof(name) - 16)
            {
                if (isspace((unsigned char)*src))
                {
                    while (isspace((unsigned char)*src))
                    {
                        src++;
                    }
                    name[n++] = '_';
                }
                else
                {
                    name[n++] = *src++;
                }
            }
            name[n] = 0;
        }
        for (j = 0; j < i; j++)
        {
            taken |= (strcmp(t->obj[j].exportName, name) == 0);
        }
        if (taken)
        {
            size_t m = strlen(name);

            snprintf(name + m, sizeof(name) - m, "_%d", (int)i);
        }
        t->obj[i].exportName = amStrdup(name);
    }
}

// the export's material names (export_ships.Exporter.write_obj): per used material, in index order, the first of
// each (texture, flags, colours) key names it
static void amMaterialExportNames(amtemplate *t)
{
    bool32 *used = calloc((size_t)t->nMat + 1, sizeof(bool32));
    udword i;
    sdword mi, mj, k;

    for (i = 0; i < t->nObj; i++)
    {
        sdword p;

        for (p = 0; p < t->obj[i].nP; p++)
        {
            uword m = amLE16(t->d + t->obj[i].oP + (udword)p * AM_PolygonSize + 0x0A);

            if (m < t->nMat)
            {
                used[m] = TRUE;
            }
        }
    }
    for (mi = 0; mi < t->nMat; mi++)
    {
        amtmaterial *m = &t->mat[mi];
        char base[512], name[540];
        const char *tex = m->texture;
        size_t n;

        m->canonical = -1;
        if (!used[mi])
        {
            continue;
        }
        for (mj = 0; mj < mi; mj++)                         // the same key used earlier: that one's name
        {
            amtmaterial *o = &t->mat[mj];

            if (used[mj] && o->canonical == mj && o->flags == m->flags && o->ambient == m->ambient &&
                o->diffuse == m->diffuse && o->specular == m->specular &&
                amPathEqual(o->texture ? o->texture : "", tex ? tex : ""))
            {
                break;
            }
        }
        if (mj < mi)
        {
            m->canonical = mj;
            m->exportName = amStrdup(t->mat[mj].exportName);
            continue;
        }
        m->canonical = mi;
        if (tex != NULL && tex[0])
        {
            snprintf(base, sizeof(base), "%s", tex);
        }
        else
        {
            snprintf(base, sizeof(base), "untextured_%06x", (unsigned)(m->diffuse & 0xffffff));
        }
        for (n = 0; base[n]; n++)
        {
            char c = (char)tolower((unsigned char)base[n]);

            base[n] = (isalnum((unsigned char)c) && (unsigned char)c < 128) || c == '_' || c == '.' || c == '-' ? c : '_';
        }
        if (m->flags & AM_MDF_SelfIllum)
        {
            strncat(base, "_glow", sizeof(base) - strlen(base) - 1);
        }
        if (m->flags & AM_MDF_2Sided)
        {
            strncat(base, "_2sided", sizeof(base) - strlen(base) - 1);
        }
        if (m->flags & AM_MDF_Smoothing)
        {
            strncat(base, "_smooth", sizeof(base) - strlen(base) - 1);
        }
        snprintf(name, sizeof(name), "%s", base);
        for (k = 2; ; k++)
        {
            bool32 taken = FALSE;

            for (mj = 0; mj < mi; mj++)
            {
                taken |= (t->mat[mj].canonical == mj && strcmp(t->mat[mj].exportName, name) == 0);
            }
            if (!taken)
            {
                break;
            }
            snprintf(name, sizeof(name), "%s_v%d", base, (int)k);
        }
        m->exportName = amStrdup(name);
    }
    free(used);
}

static void amTemplateFree(amtemplate *t)
{
    udword i;
    sdword m;

    if (t->obj != NULL)
    {
        for (i = 0; i < t->nObj; i++)
        {
            free(t->obj[i].exportName);
        }
    }
    if (t->mat != NULL)
    {
        for (m = 0; m < t->nMat; m++)
        {
            free(t->mat[m].exportName);
        }
    }
    free(t->obj);
    free(t->mat);
    memset(t, 0, sizeof(amtemplate));
}

static bool32 amTemplateRead(const ubyte *d, sdword length, amtemplate *t, char *why, size_t whySize)
{
    udword i;
    sdword m;

    memset(t, 0, sizeof(amtemplate));
    t->d = d;
    t->length = length;
    if (length < AM_HeaderSize || (memcmp(d, "RMF97ba", 7) != 0 && memcmp(d, "RMF99ba", 7) != 0))
    {
        snprintf(why, whySize, "the game's mesh is not RMF97ba/RMF99ba");
        return FALSE;
    }
    t->nPub = amLE32(d + 0x18);
    t->nLoc = amLE32(d + 0x1C);
    t->oPub = amLE32(d + 0x20);
    t->oLoc = amLE32(d + 0x24);
    t->nObj = amLE32(d + 0x28);
    if (t->nObj > 4096 || t->nPub + t->nLoc > 65535 || t->oPub != AM_HeaderSize + t->nObj * AM_ObjectSize ||
        t->oPub + (t->nPub + t->nLoc) * AM_MaterialSize > (udword)length)
    {
        snprintf(why, whySize, "the game's mesh does not have Cataclysm's layout");
        return FALSE;
    }
    if (t->nLoc != 0 || t->oLoc != t->oPub)
    {
        snprintf(why, whySize, "the game's mesh has local materials (not handled)");
        return FALSE;
    }
    t->nMat = (sdword)(t->nPub + t->nLoc);
    t->obj = calloc((size_t)t->nObj + 1, sizeof(amtobject));
    t->mat = calloc((size_t)t->nMat + 1, sizeof(amtmaterial));
    for (m = 0; m < t->nMat; m++)
    {
        const ubyte *p = d + t->oPub + (udword)m * AM_MaterialSize;
        amtmaterial *mat = &t->mat[m];

        mat->name = amString(d, length, amLE32(p + 0x00));
        mat->ambient = amLE32(p + 0x04);
        mat->diffuse = amLE32(p + 0x08);
        mat->specular = amLE32(p + 0x0C);
        mat->kAlpha = amLEReal(p + 0x10);
        mat->texture = amString(d, length, amLE32(p + 0x14));
        mat->flags = amLE16(p + 0x18);
        mat->nFullAmbient = p[0x1A];
        mat->canonical = -1;
    }
    for (i = 0; i < t->nObj; i++)
    {
        const ubyte *p = d + AM_HeaderSize + i * AM_ObjectSize;
        amtobject *o = &t->obj[i];
        udword links[3];
        sdword k;

        o->name = amString(d, length, amLE32(p + 0x00));
        o->flags = p[0x04];
        o->nV = (sdword)amLE32(p + 0x08);
        o->nFN = (sdword)amLE32(p + 0x0C);
        o->nVN = (sdword)amLE32(p + 0x10);
        o->nP = (sdword)amLE32(p + 0x14);
        o->oV = amLE32(p + 0x18);
        o->oVI = amLE32(p + 0x1C);
        o->oN = amLE32(p + 0x20);
        o->oP = amLE32(p + 0x24);
        links[0] = amLE32(p + 0x28);
        links[1] = amLE32(p + 0x2C);
        links[2] = amLE32(p + 0x30);
        for (k = 0; k < 16; k++)
        {
            o->local[k] = amLEReal(p + 0x34 + 4 * k);
        }
        if (o->nV < 0 || o->nFN < 0 || o->nVN < 0 || o->nP < 0 ||
            (uqword)o->oV + (uqword)o->nV * AM_VertexSize > (uqword)length ||
            (uqword)o->oVI + (uqword)o->nV * AM_VertexInfoSize > (uqword)length ||
            (uqword)o->oN + (uqword)(o->nFN + o->nVN) * AM_NormalSize > (uqword)length ||
            (uqword)o->oP + (uqword)o->nP * AM_PolygonSize > (uqword)length)
        {
            snprintf(why, whySize, "object %d of the game's mesh has a list past the end", (int)i);
            amTemplateFree(t);
            return FALSE;
        }
        for (k = 0; k < 3; k++)
        {
            sdword *link = (k == 0) ? &o->mother : (k == 1) ? &o->daughter : &o->sister;

            *link = -1;
            if (links[k] != 0)
            {
                if (links[k] < AM_HeaderSize || (links[k] - AM_HeaderSize) % AM_ObjectSize != 0 ||
                    (links[k] - AM_HeaderSize) / AM_ObjectSize >= t->nObj)
                {
                    snprintf(why, whySize, "object %d of the game's mesh has a bad hierarchy link", (int)i);
                    amTemplateFree(t);
                    return FALSE;
                }
                *link = (sdword)((links[k] - AM_HeaderSize) / AM_ObjectSize);
            }
        }
    }
    // world matrices: the mother chain, mother first (export_ships / hwformats.world_matrices)
    {
        bool32 *done = calloc((size_t)t->nObj + 1, sizeof(bool32));
        udword pass;

        for (pass = 0; pass <= t->nObj + 1; pass++)
        {
            bool32 progress = FALSE;

            for (i = 0; i < t->nObj; i++)
            {
                amtobject *o = &t->obj[i];
                real64 local[16];
                sdword k;

                if (done[i])
                {
                    continue;
                }
                for (k = 0; k < 16; k++)
                {
                    local[k] = (real64)o->local[k];
                }
                if (o->mother < 0 || pass > t->nObj)            // a loop: the local matrix alone
                {
                    memcpy(o->world, local, sizeof(local));
                }
                else if (done[o->mother])
                {
                    amMatMul(t->obj[o->mother].world, local, o->world);
                }
                else
                {
                    continue;
                }
                done[i] = TRUE;
                progress = TRUE;
            }
            if (!progress)
            {
                bool32 all = TRUE;

                for (i = 0; i < t->nObj; i++)
                {
                    all &= done[i];
                }
                if (all)
                {
                    break;
                }
            }
        }
        free(done);
    }
    for (i = 0; i < t->nObj; i++)
    {
        if (!amMatInverse(t->obj[i].world, t->obj[i].inverse))
        {
            snprintf(why, whySize, "object %d of the game's mesh has a matrix that cannot be inverted", (int)i);
            amTemplateFree(t);
            return FALSE;
        }
    }
    amObjectExportNames(t);
    amMaterialExportNames(t);
    return TRUE;
}

/*=============================================================================
    The OBJ and its MTL:
=============================================================================*/

typedef struct
{
    sdword v, t, n;                 // 0-based, -1: none
    sdword group;                   // the OBJ object the corner's face is in
}
amcorner;

typedef struct
{
    sdword first, count;            // corners
    sdword object;                  // template object, -1: unknown (object 0)
    sdword material;                // usemtl name index, -1: none
    bool32 smooth;                  // 's' on
}
amface;

typedef struct
{
    sdword startV, endV;            // the 'v' lines between this 'o' and the next
    sdword object;
}
amgroup;

typedef struct
{
    char name[256];
    bool32 hasKd, hasKa, hasKs;
    real64 kd[3], ka[3], ks[3];
    char mapKd[PATH_MAX];           // absolute, normalized; "" none
}
ammtl;

typedef struct
{
    amarray v, vt, vn;              // real64[3], real64[2], real64[3]
    amarray corners, faces, groups; // amcorner, amface, amgroup
    amarray materialNames;          // char[256]: usemtl names
    amarray unknownNames;           // char[256]: OBJ objects the template does not have
    amarray mtllibs;                // char[PATH_MAX]
    amarray mtls;                   // ammtl
}
amobj;

static void amObjInit(amobj *o)
{
    amArrayInit(&o->v, sizeof(real64) * 3);
    amArrayInit(&o->vt, sizeof(real64) * 2);
    amArrayInit(&o->vn, sizeof(real64) * 3);
    amArrayInit(&o->corners, sizeof(amcorner));
    amArrayInit(&o->faces, sizeof(amface));
    amArrayInit(&o->groups, sizeof(amgroup));
    amArrayInit(&o->materialNames, 256);
    amArrayInit(&o->unknownNames, 256);
    amArrayInit(&o->mtllibs, PATH_MAX);
    amArrayInit(&o->mtls, sizeof(ammtl));
}

static void amObjFree(amobj *o)
{
    amArrayFree(&o->v);
    amArrayFree(&o->vt);
    amArrayFree(&o->vn);
    amArrayFree(&o->corners);
    amArrayFree(&o->faces);
    amArrayFree(&o->groups);
    amArrayFree(&o->materialNames);
    amArrayFree(&o->unknownNames);
    amArrayFree(&o->mtllibs);
    amArrayFree(&o->mtls);
}

// a name without Blender's ".001" suffix (a '.' and three or more digits at the end); FALSE if it has none
static bool32 amStripBlenderSuffix(const char *name, char *out, size_t size)
{
    size_t n = strlen(name), i = n;

    while (i > 0 && isdigit((unsigned char)name[i - 1]))
    {
        i--;
    }
    if (n - i < 3 || i == 0 || name[i - 1] != '.')
    {
        return FALSE;
    }
    i--;
    if (i >= size)
    {
        i = size - 1;
    }
    memmove(out, name, i);                                  // out may be name
    out[i] = 0;
    return TRUE;
}

static sdword amObjectFind(amtemplate *t, const char *name)
{
    char stripped[256];
    udword i;
    sdword pass;

    for (pass = 0; pass < 4; pass++)
    {
        const char *n = name;

        if (pass & 1)
        {
            if (!amStripBlenderSuffix(name, stripped, sizeof(stripped)))
            {
                continue;
            }
            n = stripped;
        }
        for (i = 0; i < t->nObj; i++)
        {
            if ((pass < 2) ? strcmp(t->obj[i].exportName, n) == 0 : amPathEqual(t->obj[i].exportName, n))
            {
                return (sdword)i;
            }
        }
    }
    return -1;
}

static sdword amMaterialFind(amtemplate *t, const char *name)
{
    char stripped[256];
    sdword i, pass;

    for (pass = 0; pass < 4; pass++)
    {
        const char *n = name;

        if (pass & 1)
        {
            if (!amStripBlenderSuffix(name, stripped, sizeof(stripped)))
            {
                continue;
            }
            n = stripped;
        }
        for (i = 0; i < t->nMat; i++)
        {
            if (t->mat[i].canonical == i &&
                ((pass < 2) ? strcmp(t->mat[i].exportName, n) == 0 : amPathEqual(t->mat[i].exportName, n)))
            {
                return i;
            }
        }
    }
    return -1;
}

static sdword amIndex(sdword index, sdword count)
{
    if (index > 0)
    {
        return (index <= count) ? index - 1 : -2;
    }
    if (index < 0)
    {
        return (count + index >= 0) ? count + index : -2;
    }
    return -2;
}

static bool32 amObjParse(amtemplate *t, const char *path, amobj *o, char *why, size_t whySize)
{
    sdword length;
    char *text = amReadFile(path, &length);
    char *line, *next;
    sdword current = -2, material = -1, group = -1, lineNumber = 0;
    bool32 smooth = FALSE;
    char name[256];

    if (text == NULL)
    {
        snprintf(why, whySize, "cannot read the file");
        return FALSE;
    }
    for (line = text; line != NULL && *line; line = next)
    {
        const char *p;
        size_t n;

        lineNumber++;
        next = strchr(line, '\n');
        if (next != NULL)
        {
            *next++ = 0;
        }
        n = strlen(line);
        while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t'))
        {
            line[--n] = 0;
        }
        p = amSkipSpace(line);
        if (*p == 0 || *p == '#')
        {
            continue;
        }
        if ((p[0] == 'v' && (p[1] == ' ' || p[1] == '\t')) || (p[0] == 'v' && p[1] == 'n' && (p[2] == ' ' || p[2] == '\t')) ||
            (p[0] == 'v' && p[1] == 't' && (p[2] == ' ' || p[2] == '\t')))
        {
            amarray *a = (p[1] == 'n') ? &o->vn : (p[1] == 't') ? &o->vt : &o->v;
            sdword want = (p[1] == 't') ? 2 : 3, k;
            real64 value[3] = {0.0, 0.0, 0.0};
            real64 *out;

            p += (p[1] == ' ' || p[1] == '\t') ? 1 : 2;
            for (k = 0; k < want; k++)
            {
                const char *e;

                p = amSkipSpace(p);
                if (*p == 0 && k == 1 && want == 2)
                {
                    break;                                  // "vt u": v = 0
                }
                e = amParseReal(p, &value[k]);
                if (e == NULL && a == &o->vn && (strncmp(p, "nan", 3) == 0 || strncmp(p, "-nan", 4) == 0 ||
                                                 strncmp(p, "NaN", 3) == 0 || strncmp(p, "inf", 3) == 0 ||
                                                 strncmp(p, "-inf", 4) == 0))
                {                                           // the game's own meshes have a few NaN normals
                    value[k] = NAN;
                    e = p + ((*p == '-') ? 4 : 3);
                }
                if (e == NULL)
                {
                    snprintf(why, whySize, "line %d: a number is expected", (int)lineNumber);
                    free(text);
                    return FALSE;
                }
                p = e;
            }
            out = amArrayAdd(a);
            memcpy(out, value, sizeof(real64) * (size_t)want);
            if (a == &o->v && group >= 0)
            {
                amAt(o->groups, amgroup, group).endV = o->v.count;
            }
        }
        else if (p[0] == 'f' && (p[1] == ' ' || p[1] == '\t'))
        {
            amface *face;
            sdword first = o->corners.count;

            p += 1;
            for (;;)
            {
                sdword idx[3] = {0, 0, 0}, k = 0;
                amcorner *c;

                p = amSkipSpace(p);
                if (*p == 0)
                {
                    break;
                }
                for (k = 0; k < 3; k++)
                {
                    if (*p != '/' && *p != ' ' && *p != '\t' && *p != 0)
                    {
                        char *e;
                        long value = strtol(p, &e, 10);

                        if (e == p)
                        {
                            snprintf(why, whySize, "line %d: a vertex index is expected", (int)lineNumber);
                            free(text);
                            return FALSE;
                        }
                        idx[k] = (sdword)value;
                        p = e;
                    }
                    if (*p != '/')
                    {
                        break;
                    }
                    p++;
                }
                while (*p && *p != ' ' && *p != '\t')
                {
                    p++;
                }
                c = amArrayAdd(&o->corners);
                c->v = amIndex(idx[0], o->v.count);
                c->t = idx[1] ? amIndex(idx[1], o->vt.count) : -1;
                c->n = idx[2] ? amIndex(idx[2], o->vn.count) : -1;
                c->group = group;
                if (c->v < 0 || c->t == -2 || c->n == -2)
                {
                    snprintf(why, whySize, "line %d: an index is out of range", (int)lineNumber);
                    free(text);
                    return FALSE;
                }
                if (c->n >= 0)
                {
                    real64 *nn = &amAt(o->vn, real64, c->n * 3);

                    if (!isfinite(nn[0]) || !isfinite(nn[1]) || !isfinite(nn[2]))
                    {
                        c->n = -1;                          // a NaN normal: as if none
                    }
                }
            }
            if (o->corners.count - first < 3)
            {
                o->corners.count = first;                   // a point or a line: not a face
                continue;
            }
            face = amArrayAdd(&o->faces);
            face->first = first;
            face->count = o->corners.count - first;
            face->object = current;
            face->material = material;
            face->smooth = smooth;
        }
        else if ((p[0] == 'o' || p[0] == 'g') && (p[1] == ' ' || p[1] == '\t' || p[1] == 0))
        {
            sdword found;

            amRestOfLine(p + 1, name, sizeof(name));
            found = amObjectFind(t, name);
            if (p[0] == 'g' && found < 0)
            {
                continue;                                   // a group the template does not have: the object stays
            }
            current = found;
            if (found < 0)
            {
                sdword k;
                bool32 seen = FALSE;

                for (k = 0; k < o->unknownNames.count; k++)
                {
                    seen |= (strcmp((char *)o->unknownNames.data + 256 * (size_t)k, name) == 0);
                }
                if (!seen)
                {
                    memcpy(amArrayAdd(&o->unknownNames), name, 256);
                }
            }
            if (p[0] == 'o')
            {
                amgroup *g = amArrayAdd(&o->groups);

                g->startV = g->endV = o->v.count;
                g->object = found;
                group = o->groups.count - 1;
            }
        }
        else if (strncmp(p, "usemtl", 6) == 0 && (p[6] == ' ' || p[6] == '\t' || p[6] == 0))
        {
            sdword k;

            amRestOfLine(p + 6, name, sizeof(name));
            for (k = 0; k < o->materialNames.count; k++)
            {
                if (strcmp((char *)o->materialNames.data + 256 * (size_t)k, name) == 0)
                {
                    break;
                }
            }
            if (k == o->materialNames.count)
            {
                memcpy(amArrayAdd(&o->materialNames), name, 256);
            }
            material = k;
        }
        else if (strncmp(p, "mtllib", 6) == 0 && (p[6] == ' ' || p[6] == '\t'))
        {
            char file[PATH_MAX];

            amRestOfLine(p + 6, file, sizeof(file));
            memcpy(amArrayAdd(&o->mtllibs), file, PATH_MAX);
        }
        else if (p[0] == 's' && (p[1] == ' ' || p[1] == '\t'))
        {
            amRestOfLine(p + 1, name, sizeof(name));
            smooth = !(strcmp(name, "off") == 0 || strcmp(name, "0") == 0 || name[0] == 0);
        }
        // anything else (vp, l, p, curves, ...) is not a face
    }
    free(text);
    if (o->faces.count == 0)
    {
        snprintf(why, whySize, "no faces");
        return FALSE;
    }
    return TRUE;
}

// "map_Kd [-options ...] file": the file
static void amMapPath(const char *p, char *out, size_t size)
{
    static const struct { const char *name; sdword args; } options[] =
    {
        {"-blendu", 1}, {"-blendv", 1}, {"-boost", 1}, {"-mm", 2}, {"-o", 3}, {"-s", 3}, {"-t", 3},
        {"-texres", 1}, {"-clamp", 1}, {"-bm", 1}, {"-imfchan", 1}, {"-type", 1}, {"-cc", 1},
    };
    sdword i;

    p = amSkipSpace(p);
    while (*p == '-')
    {
        const char *end = p;
        sdword args = -1, k;

        while (*end && *end != ' ' && *end != '\t')
        {
            end++;
        }
        for (i = 0; i < (sdword)(sizeof(options) / sizeof(options[0])); i++)
        {
            if ((size_t)(end - p) == strlen(options[i].name) && strncmp(p, options[i].name, (size_t)(end - p)) == 0)
            {
                args = options[i].args;
            }
        }
        if (args < 0)
        {
            break;                                          // a file name that starts with '-'
        }
        p = amSkipSpace(end);
        for (k = 0; k < args && *p; k++)                    // up to args numbers or words (on / off)
        {
            const char *e = p;
            real64 v;

            if (amParseReal(p, &v) == NULL && strncmp(p, "on", 2) != 0 && strncmp(p, "off", 3) != 0 &&
                !isalpha((unsigned char)*p))
            {
                break;
            }
            if (k > 0 && amParseReal(p, &v) == NULL)
            {
                break;                                      // only the first argument may be a word
            }
            while (*e && *e != ' ' && *e != '\t')
            {
                e++;
            }
            p = amSkipSpace(e);
        }
    }
    amRestOfLine(p, out, size);
}

static void amMtlRead(amobj *o, const char *objDir)
{
    sdword lib;

    for (lib = 0; lib < o->mtllibs.count; lib++)
    {
        char path[PATH_MAX], dir[PATH_MAX];
        char *text, *line, *next;
        ammtl *current = NULL;

        amPathJoin(objDir, (char *)o->mtllibs.data + PATH_MAX * (size_t)lib, path);
        text = amReadFile(path, NULL);
        if (text == NULL)
        {
            continue;
        }
        amPathDir(path, dir, sizeof(dir));
        for (line = text; line != NULL && *line; line = next)
        {
            const char *p;
            size_t n;

            next = strchr(line, '\n');
            if (next != NULL)
            {
                *next++ = 0;
            }
            n = strlen(line);
            while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t'))
            {
                line[--n] = 0;
            }
            p = amSkipSpace(line);
            if (strncmp(p, "newmtl", 6) == 0 && (p[6] == ' ' || p[6] == '\t'))
            {
                current = amArrayAdd(&o->mtls);
                amRestOfLine(p + 6, current->name, sizeof(current->name));
            }
            else if (current != NULL && (p[0] == 'K') && (p[1] == 'd' || p[1] == 'a' || p[1] == 's') &&
                     (p[2] == ' ' || p[2] == '\t'))
            {
                real64 *c = (p[1] == 'd') ? current->kd : (p[1] == 'a') ? current->ka : current->ks;
                bool32 *has = (p[1] == 'd') ? &current->hasKd : (p[1] == 'a') ? &current->hasKa : &current->hasKs;
                sdword k;
                const char *q = p + 2;

                for (k = 0; k < 3; k++)
                {
                    const char *e = amParseReal(amSkipSpace(q), &c[k]);

                    if (e == NULL)
                    {
                        c[k] = (k > 0) ? c[k - 1] : 0.0;        // "Kd 0.5": grey
                    }
                    else
                    {
                        q = e;
                    }
                }
                *has = TRUE;
            }
            else if (current != NULL && strncmp(p, "map_Kd", 6) == 0 && (p[6] == ' ' || p[6] == '\t'))
            {
                char file[PATH_MAX];

                amMapPath(p + 6, file, sizeof(file));
                if (file[0])
                {
                    amPathJoin(dir, file, current->mapKd);
                }
            }
        }
        free(text);
    }
}

static ammtl *amMtlFind(amobj *o, const char *name)
{
    sdword i;

    for (i = 0; i < o->mtls.count; i++)
    {
        if (strcmp(amAt(o->mtls, ammtl, i).name, name) == 0)
        {
            return &amAt(o->mtls, ammtl, i);
        }
    }
    return NULL;
}

/*=============================================================================
    Building the mesh:
=============================================================================*/

typedef struct
{
    const char *name;               // material name (template's or the OBJ's)
    udword ambient, diffuse, specular;
    real32 kAlpha;
    const char *texture;            // texture name as the mesh stores it (relative to the mesh's folder) or NULL
    uword flags;
    ubyte nFullAmbient;
}
ammaterial;

typedef struct
{
    real32 x, y, z;
    real64 normal[3];               // the vertex normal (object space)
    real64 accumulated[3];          // face normals around it, for a vertex without an OBJ normal
    sdword key;                     // OBJ normal index, -1 flat only, -2 smooth without an OBJ normal
    sdword sameIndex;               // the template vertex of the same number (the OBJ object has as many), or -1
    ubyte info[AM_VertexInfoSize];
}
amvertex;

typedef struct
{
    sdword v[3];
    uword material;
    real32 uv[6];
    real64 normal[3];
}
ampolygon;

typedef struct
{
    amarray vertices;               // amvertex
    amarray polygons;               // ampolygon
}
amoutobject;

static void amColour(const real64 *c, udword *out)
{
    sdword k;
    udword v = 0;

    for (k = 0; k < 3; k++)
    {
        real64 x = c[k] < 0.0 ? 0.0 : c[k] > 1.0 ? 1.0 : c[k];

        v |= (udword)(x * 255.0 + 0.5) << (8 * k);
    }
    *out = v;                                               // bytes R, G, B and 0, as the game's meshes
}

// the picture a material names: a texture of the mesh's folder, or a new one (registered); NULL: none
static const char *amPicture(const char *folder, const char *meshDir, const char *png, amnewpicture newPicture,
                             amgamefile gameFile, amarray *names, amstats *stats, const char *objRel)
{
    char dir[PATH_MAX], base[256], key[PATH_MAX], lif[PATH_MAX], meshFolder[PATH_MAX];
    const char *below, *slash;
    sdword k, i;

    if (!amEndsWithNoCase(png, ".png"))
    {
        printf("assets: model %s: %s is not a PNG: untextured\n", objRel, png);
        return NULL;
    }
    if (!amFileExists(png))
    {
        printf("assets: model %s: picture %s not found: untextured\n", objRel, png);
        return NULL;
    }
    below = amPathBelow(png, folder);
    if (below == NULL)
    {
        printf("assets: model %s: picture %s is outside the Assets folder: untextured\n", objRel, png);
        return NULL;
    }
    slash = strrchr(png, '/');
    snprintf(base, sizeof(base), "%.*s", (int)(strlen(slash + 1) - 4), slash + 1);
    for (i = 0; base[i]; i++)
    {
        base[i] = (char)tolower((unsigned char)base[i]);
    }
    amPathDir(png, dir, sizeof(dir));
    snprintf(meshFolder, sizeof(meshFolder), "%s/%s", folder, meshDir);
    amPathNormalize(meshFolder);
    snprintf(lif, sizeof(lif), "%s/%s.lif", meshDir, base);
    if (amPathEqual(dir, meshFolder) && gameFile(lif))
    {
        char *name = amStrdup(base);                        // a texture of the mesh's own folder

        *(char **)amArrayAdd(names) = name;
        return name;
    }
    for (i = 0; base[i]; i++)
    {
        if (!isalnum((unsigned char)base[i]) && base[i] != '_' && base[i] != '-')
        {
            base[i] = '_';
        }
    }
    for (k = 1; k < 100; k++)
    {
        char name[256];

        if (k == 1)
        {
            snprintf(name, sizeof(name), "hwi_%.200s", base);
        }
        else
        {
            snprintf(name, sizeof(name), "hwi_%.200s_%d", base, (int)k);
        }
        snprintf(lif, sizeof(lif), "%s/%s.lif", meshDir, name);
        snprintf(key, sizeof(key), "%s/%s", meshDir, name);
        if (gameFile(lif))
        {
            continue;
        }
        if (newPicture(key, png))
        {
            char *out = amStrdup(name);

            *(char **)amArrayAdd(names) = out;
            stats->newPictures++;
            printf("assets: model %s: new picture %s (texture %s)\n", objRel, below, key);
            return out;
        }
    }
    printf("assets: model %s: no texture name left for %s: untextured\n", objRel, png);
    return NULL;
}

static void amTriangleNormal(const amvertex *a, const amvertex *b, const amvertex *c, real64 *out)
{
    real64 u[3] = {(real64)b->x - a->x, (real64)b->y - a->y, (real64)b->z - a->z};
    real64 v[3] = {(real64)c->x - a->x, (real64)c->y - a->y, (real64)c->z - a->z};

    out[0] = u[1] * v[2] - u[2] * v[1];
    out[1] = u[2] * v[0] - u[0] * v[2];
    out[2] = u[0] * v[1] - u[1] * v[0];
}

ubyte *amObjToMesh(const char *folderIn, const char *objPath, const char *objRel, const char *meshName,
                   const ubyte *tmpl, sdword tmplLength, amnewpicture newPicture, amgamefile gameFile,
                   sdword *outLength, amstats *stats, char *why, size_t whySize)
{
    amtemplate t;
    amobj obj;
    amarray materials, textureNames;
    sdword *materialOf = NULL;      // usemtl index -> output material
    sdword noMaterial = -1;
    amoutobject *out = NULL;
    char folder[PATH_MAX], objDir[PATH_MAX], meshDir[PATH_MAX];
    sdword i, f;
    ubyte *image = NULL;
    bool32 ok = FALSE;

    memset(stats, 0, sizeof(amstats));
    *outLength = 0;
    why[0] = 0;
    if (!amTemplateRead(tmpl, tmplLength, &t, why, whySize))
    {
        return NULL;
    }
    amObjInit(&obj);
    amArrayInit(&materials, sizeof(ammaterial));
    amArrayInit(&textureNames, sizeof(char *));
    snprintf(folder, sizeof(folder), "%s", folderIn);
    amPathNormalize(folder);
    snprintf(objDir, sizeof(objDir), "%s", objPath);
    amPathNormalize(objDir);
    amPathDir(objDir, objDir, sizeof(objDir));
    amPathDir(meshName, meshDir, sizeof(meshDir));
    for (i = 0; meshDir[i]; i++)
    {
        meshDir[i] = (meshDir[i] == '\\') ? '/' : (char)tolower((unsigned char)meshDir[i]);
    }
    if (!amObjParse(&t, objPath, &obj, why, whySize))
    {
        goto done;
    }
    amMtlRead(&obj, objDir);
    stats->faces = obj.faces.count;
    stats->unknownObjects = obj.unknownNames.count;
    for (i = 0; i < obj.unknownNames.count; i++)
    {
        printf("assets: model %s: object \"%s\" is not part of the game's mesh: its faces go to the root object %s\n",
               objRel, (char *)obj.unknownNames.data + 256 * (size_t)i, t.obj[0].exportName);
    }

    // materials: the template's, in their order, then the new ones
    for (i = 0; i < t.nMat; i++)
    {
        ammaterial *m = amArrayAdd(&materials);
        amtmaterial *s = &t.mat[i];

        m->name = s->name;
        m->ambient = s->ambient;
        m->diffuse = s->diffuse;
        m->specular = s->specular;
        m->kAlpha = s->kAlpha;
        m->texture = s->texture;
        m->flags = s->flags;
        m->nFullAmbient = s->nFullAmbient;
    }
    materialOf = malloc(sizeof(sdword) * (size_t)(obj.materialNames.count + 1));
    for (i = 0; i < obj.materialNames.count; i++)
    {
        const char *name = (char *)obj.materialNames.data + 256 * (size_t)i;
        sdword mi = amMaterialFind(&t, name);
        ammtl *mtl = amMtlFind(&obj, name);
        bool32 smooth = FALSE;

        for (f = 0; f < obj.faces.count; f++)
        {
            smooth |= (amAt(obj.faces, amface, f).material == i && amAt(obj.faces, amface, f).smooth);
        }
        if (mi >= 0)
        {
            const char *texture = t.mat[mi].texture;
            char expected[PATH_MAX];

            materialOf[i] = mi;
            if (mtl == NULL || mtl->mapKd[0] == 0)
            {
                continue;                                   // as the game has it
            }
            if (texture != NULL && texture[0])
            {
                char lower[256];
                sdword k;

                snprintf(lower, sizeof(lower), "%s", texture);
                for (k = 0; lower[k]; k++)
                {
                    lower[k] = (char)tolower((unsigned char)lower[k]);
                }
                snprintf(expected, sizeof(expected), "%s/%s/%s.png", folder, meshDir, lower);
                amPathNormalize(expected);
                if (amPathEqual(expected, mtl->mapKd))
                {
                    continue;                               // the export's own picture
                }
            }
            {
                const char *picture = amPicture(folder, meshDir, mtl->mapKd, newPicture, gameFile, &textureNames,
                                                stats, objRel);
                ammaterial *m;

                if (picture == NULL)
                {
                    continue;                               // keep the game's
                }
                m = amArrayAdd(&materials);
                *m = amAt(materials, ammaterial, mi);
                m->texture = picture;
                materialOf[i] = materials.count - 1;
                stats->newMaterials++;
                printf("assets: model %s: material %s now uses picture %s\n", objRel, name, picture);
            }
        }
        else
        {
            ammaterial *m = amArrayAdd(&materials);
            char base[256];
            real64 grey[3] = {0.8, 0.8, 0.8};
            sdword guard;

            materialOf[i] = materials.count - 1;
            m->name = name;
            snprintf(base, sizeof(base), "%s", name);
            amStripBlenderSuffix(base, base, sizeof(base));     // in place
            for (guard = 0; guard < 8; guard++)             // the export's suffixes: _vN, _smooth, _2sided, _glow
            {
                size_t n = strlen(base), k = n;

                while (k > 0 && isdigit((unsigned char)base[k - 1]))
                {
                    k--;
                }
                if (k < n && k >= 2 && base[k - 1] == 'v' && base[k - 2] == '_')
                {
                    base[k - 2] = 0;
                }
                else if (amEndsWithNoCase(base, "_smooth"))
                {
                    m->flags |= AM_MDF_Smoothing;
                    base[n - 7] = 0;
                }
                else if (amEndsWithNoCase(base, "_2sided"))
                {
                    m->flags |= AM_MDF_2Sided;
                    base[n - 7] = 0;
                }
                else if (amEndsWithNoCase(base, "_glow"))
                {
                    m->flags |= AM_MDF_SelfIllum;
                    base[n - 5] = 0;
                }
                else
                {
                    break;
                }
            }
            if (smooth)
            {
                m->flags |= AM_MDF_Smoothing;
            }
            amColour((mtl != NULL && mtl->hasKd) ? mtl->kd : grey, &m->diffuse);
            if (mtl != NULL && mtl->hasKa && (mtl->ka[0] + mtl->ka[1] + mtl->ka[2]) > 0.0)
            {
                amColour(mtl->ka, &m->ambient);
            }
            else
            {
                real64 quarter[3];
                const real64 *kd = (mtl != NULL && mtl->hasKd) ? mtl->kd : grey;

                quarter[0] = kd[0] * 0.25;
                quarter[1] = kd[1] * 0.25;
                quarter[2] = kd[2] * 0.25;
                amColour(quarter, &m->ambient);
            }
            m->specular = 0;
            m->texture = (mtl != NULL && mtl->mapKd[0]) ?
                amPicture(folder, meshDir, mtl->mapKd, newPicture, gameFile, &textureNames, stats, objRel) : NULL;
            stats->newMaterials++;
            printf("assets: model %s: new material %s (%s%s%s%s)\n", objRel, name,
                   m->texture ? m->texture : "untextured", (m->flags & AM_MDF_Smoothing) ? ", smooth" : "",
                   (m->flags & AM_MDF_2Sided) ? ", 2-sided" : "", (m->flags & AM_MDF_SelfIllum) ? ", glow" : "");
        }
    }
    for (f = 0; f < obj.faces.count; f++)
    {
        if (amAt(obj.faces, amface, f).material < 0 && noMaterial < 0)
        {
            ammaterial *m = amArrayAdd(&materials);
            real64 grey[3] = {0.8, 0.8, 0.8}, quarter[3] = {0.2, 0.2, 0.2};

            noMaterial = materials.count - 1;
            m->name = "hwi_nomaterial";
            amColour(grey, &m->diffuse);
            amColour(quarter, &m->ambient);
            stats->newMaterials++;
            printf("assets: model %s: faces without a material are drawn untextured grey\n", objRel);
        }
    }
    if (materials.count > AM_MaxIndex)
    {
        snprintf(why, whySize, "too many materials (%d)", (int)materials.count);
        goto done;
    }

    // geometry, per template object (faces of unknown objects go to object 0)
    out = calloc((size_t)t.nObj + 1, sizeof(amoutobject));
    {
        ammap vertexOf, firstOf;

        for (i = 0; i < (sdword)t.nObj; i++)
        {
            amtobject *to = &t.obj[i];
            amoutobject *oo = &out[i];
            sdword nCorners = 0, k;

            amArrayInit(&oo->vertices, sizeof(amvertex));
            amArrayInit(&oo->polygons, sizeof(ampolygon));
            for (f = 0; f < obj.faces.count; f++)
            {
                amface *face = &amAt(obj.faces, amface, f);

                if ((face->object < 0 ? 0 : face->object) == i)
                {
                    nCorners += face->count;
                }
            }
            if (nCorners == 0)
            {
                continue;
            }
            stats->objects++;
            amMapInit(&vertexOf, nCorners);
            amMapInit(&firstOf, nCorners);
            for (f = 0; f < obj.faces.count; f++)
            {
                amface *face = &amAt(obj.faces, amface, f);
                sdword mo = (face->material < 0) ? noMaterial : materialOf[face->material];
                bool32 smooth = (amAt(materials, ammaterial, mo).flags & AM_MDF_Smoothing) != 0;
                sdword corner[256], c, n;

                if ((face->object < 0 ? 0 : face->object) != i)
                {
                    continue;
                }
                n = face->count;
                if (n > 256)
                {
                    snprintf(why, whySize, "a face with %d corners (at most 256)", (int)n);
                    goto doneGeometry;
                }
                // each corner's vertex: one per OBJ position and normal; a flat face takes any vertex at its position
                for (c = 0; c < n; c++)
                {
                    amcorner *cr = &amAt(obj.corners, amcorner, face->first + c);
                    sdword key = smooth ? (cr->n >= 0 ? cr->n : -2) : -1;
                    uqword mapKey = ((uqword)(udword)cr->v << 32) | (udword)(key + 2);
                    sdword vi = amMapGet(&vertexOf, mapKey);

                    if (vi < 0 && key == -1)
                    {
                        vi = amMapGet(&firstOf, (uqword)(udword)cr->v);
                    }
                    if (vi < 0)
                    {
                        amvertex *v = amArrayAdd(&oo->vertices);
                        real64 *pos = &amAt(obj.v, real64, cr->v * 3);
                        real64 game[3], local[3];

                        game[0] = pos[1];                   // game (x, y, z) = OBJ (y, -x, z)
                        game[1] = -pos[0];
                        game[2] = pos[2];
                        amXformPoint(to->inverse, game, local);
                        v->x = (real32)local[0];
                        v->y = (real32)local[1];
                        v->z = (real32)local[2];
                        v->key = key;
                        v->sameIndex = -1;
                        if (cr->group >= 0)
                        {
                            amgroup *g = &amAt(obj.groups, amgroup, cr->group);

                            if (g->object == i && g->endV - g->startV == to->nV && cr->v >= g->startV && cr->v < g->endV)
                            {
                                v->sameIndex = cr->v - g->startV;
                            }
                        }
                        if (key >= 0)
                        {
                            real64 *nn = &amAt(obj.vn, real64, key * 3);
                            real64 gn[3], ln[3];

                            gn[0] = nn[1];
                            gn[1] = -nn[0];
                            gn[2] = nn[2];
                            amXformVector(to->inverse, gn, ln);
                            if (!amNormalize(ln))
                            {
                                ln[0] = ln[1] = 0.0;
                                ln[2] = 1.0;
                            }
                            memcpy(v->normal, ln, sizeof(ln));
                        }
                        vi = oo->vertices.count - 1;
                        amMapPut(&vertexOf, mapKey, vi);
                        if (amMapGet(&firstOf, (uqword)(udword)cr->v) < 0)
                        {
                            amMapPut(&firstOf, (uqword)(udword)cr->v, vi);
                        }
                    }
                    corner[c] = vi;
                }
                // a fan of triangles (right for convex polygons)
                for (c = 1; c + 1 < n; c++)
                {
                    ampolygon *poly = amArrayAdd(&oo->polygons);
                    sdword fan[3] = {0, c, c + 1}, k3;
                    real64 geometric[3], sum[3] = {0.0, 0.0, 0.0};
                    bool32 allNormals = TRUE;
                    amvertex *va, *vb, *vc;

                    poly->material = (uword)mo;
                    for (k3 = 0; k3 < 3; k3++)
                    {
                        amcorner *cr = &amAt(obj.corners, amcorner, face->first + fan[k3]);

                        poly->v[k3] = corner[fan[k3]];
                        if (cr->t >= 0)
                        {
                            real64 *uv = &amAt(obj.vt, real64, cr->t * 2);

                            poly->uv[k3 * 2 + 0] = (real32)uv[0];
                            poly->uv[k3 * 2 + 1] = (real32)(1.0 - uv[1]);
                        }
                        if (cr->n >= 0)
                        {
                            real64 *nn = &amAt(obj.vn, real64, cr->n * 3);
                            real64 gn[3], ln[3];

                            gn[0] = nn[1];
                            gn[1] = -nn[0];
                            gn[2] = nn[2];
                            amXformVector(to->inverse, gn, ln);
                            amNormalize(ln);
                            sum[0] += ln[0];
                            sum[1] += ln[1];
                            sum[2] += ln[2];
                        }
                        else
                        {
                            allNormals = FALSE;
                        }
                    }
                    va = &amAt(oo->vertices, amvertex, poly->v[0]);
                    vb = &amAt(oo->vertices, amvertex, poly->v[1]);
                    vc = &amAt(oo->vertices, amvertex, poly->v[2]);
                    amTriangleNormal(va, vb, vc, geometric);
                    for (k3 = 0; k3 < 3; k3++)              // area-weighted, for vertices without an OBJ normal
                    {
                        amvertex *v = &amAt(oo->vertices, amvertex, poly->v[k3]);

                        if (v->key < 0)
                        {
                            v->accumulated[0] += geometric[0];
                            v->accumulated[1] += geometric[1];
                            v->accumulated[2] += geometric[2];
                        }
                    }
                    if (!smooth && allNormals && amNormalize(sum))
                    {
                        memcpy(poly->normal, sum, sizeof(sum));     // flat: the normal the OBJ gives its corners
                    }
                    else if (amNormalize(geometric))
                    {
                        memcpy(poly->normal, geometric, sizeof(geometric));
                    }
                    else if (amNormalize(sum))
                    {
                        memcpy(poly->normal, sum, sizeof(sum));
                    }
                    else
                    {
                        poly->normal[2] = 1.0;
                    }
                    stats->triangles++;
                }
            }
            if (oo->vertices.count > AM_MaxIndex + 1)
            {
                snprintf(why, whySize, "object %s has %d vertices (at most %d)", to->exportName,
                         (int)oo->vertices.count, AM_MaxIndex + 1);
                goto doneGeometry;
            }
            // vertex normals without an OBJ normal; the glow entry of the template vertex at the same place
            for (k = 0; k < oo->vertices.count; k++)
            {
                amvertex *v = &amAt(oo->vertices, amvertex, k);
                sdword j, best = -1;
                real64 bestDistance = 0.0;
                real64 size = fabs((real64)v->x) + fabs((real64)v->y) + fabs((real64)v->z);
                real64 tolerance = 1e-3 + 1e-6 * size;

                if (v->key < 0)
                {
                    memcpy(v->normal, v->accumulated, sizeof(v->normal));
                    if (!amNormalize(v->normal))
                    {
                        v->normal[0] = v->normal[1] = 0.0;
                        v->normal[2] = 1.0;
                    }
                }
                // the template vertex at the same place: the one of the same number (the export's own order), else
                // the one whose normal is closest (several vertices can share a place), else, for a moved vertex,
                // the one of the same number
                for (j = -1; j < to->nV; j++)
                {
                    sdword tj = (j < 0) ? v->sameIndex : j;
                    const ubyte *tv;
                    real64 dx, dy, dz, d, tn[3];

                    if (tj < 0 || tj >= to->nV)
                    {
                        continue;
                    }
                    tv = t.d + to->oV + (udword)tj * AM_VertexSize;
                    dx = (real64)amLEReal(tv + 0) - v->x;
                    dy = (real64)amLEReal(tv + 4) - v->y;
                    dz = (real64)amLEReal(tv + 8) - v->z;
                    if (fabs(dx) + fabs(dy) + fabs(dz) > tolerance)
                    {
                        continue;
                    }
                    if (j < 0)
                    {
                        best = tj;                          // same number, same place
                        break;
                    }
                    d = 0.0;
                    {
                        sdword ivn = (sdword)amLE32(tv + 12);

                        if (ivn >= 0 && ivn < to->nFN + to->nVN)
                        {
                            const ubyte *nn = t.d + to->oN + (udword)ivn * AM_NormalSize;

                            tn[0] = amLEReal(nn + 0);
                            tn[1] = amLEReal(nn + 4);
                            tn[2] = amLEReal(nn + 8);
                            if (amNormalize(tn))
                            {
                                d = (tn[0] - v->normal[0]) * (tn[0] - v->normal[0]) +
                                    (tn[1] - v->normal[1]) * (tn[1] - v->normal[1]) +
                                    (tn[2] - v->normal[2]) * (tn[2] - v->normal[2]);
                            }
                        }
                    }
                    if (best < 0 || d < bestDistance)
                    {
                        best = tj;
                        bestDistance = d;
                    }
                }
                if (best < 0)
                {
                    best = v->sameIndex;                    // moved: the vertex of the same number
                }
                if (best >= 0)
                {
                    memcpy(v->info, t.d + to->oVI + (udword)best * AM_VertexInfoSize, AM_VertexInfoSize);
                    stats->glowCopied++;
                }
            }
            stats->vertices += oo->vertices.count;
            amMapFree(&vertexOf);
            amMapFree(&firstOf);
            continue;
doneGeometry:
            amMapFree(&vertexOf);
            amMapFree(&firstOf);
            goto done;
        }
    }

    // the image: header, objects, materials, then the lists and the names (behind the material table, as the
    // 64-bit loader expects every offset)
    {
        udword nMat = (udword)materials.count;
        udword oObjects = AM_HeaderSize, oMaterials = oObjects + t.nObj * AM_ObjectSize;
        udword oData = oMaterials + nMat * AM_MaterialSize;
        uqword size = oData;
        udword at, *nameAt, *matNameAt, *texNameAt;
        ubyte *d;

        for (i = 0; i < (sdword)t.nObj; i++)
        {
            size += (uqword)out[i].vertices.count * (AM_VertexSize + AM_VertexInfoSize + AM_NormalSize) +
                    (uqword)out[i].polygons.count * (AM_NormalSize + AM_PolygonSize);
            size += (t.obj[i].name ? strlen(t.obj[i].name) : 0) + 4;
        }
        for (i = 0; i < materials.count; i++)
        {
            ammaterial *m = &amAt(materials, ammaterial, i);

            size += (m->name ? strlen(m->name) : 0) + 4 + (m->texture ? strlen(m->texture) : 0) + 4;
        }
        if (size > AM_MaxFileSize)
        {
            snprintf(why, whySize, "the mesh would be too big");
            goto done;
        }
        d = calloc(1, (size_t)size + 16);
        nameAt = calloc((size_t)t.nObj + 1, sizeof(udword));
        matNameAt = calloc((size_t)nMat + 1, sizeof(udword));
        texNameAt = calloc((size_t)nMat + 1, sizeof(udword));
        at = oData;
        // names first (their offsets are known before the tables are written)
        for (i = 0; i < (sdword)t.nObj; i++)
        {
            if (t.obj[i].name != NULL)
            {
                size_t n = strlen(t.obj[i].name) + 1;

                nameAt[i] = at;
                memcpy(d + at, t.obj[i].name, n);
                at += (udword)((n + 3) & ~(size_t)3);
            }
        }
        for (i = 0; i < materials.count; i++)
        {
            ammaterial *m = &amAt(materials, ammaterial, i);

            if (m->name != NULL)
            {
                size_t n = strlen(m->name) + 1;

                matNameAt[i] = at;
                memcpy(d + at, m->name, n);
                at += (udword)((n + 3) & ~(size_t)3);
            }
            if (m->texture != NULL)
            {
                size_t n = strlen(m->texture) + 1;

                texNameAt[i] = at;
                memcpy(d + at, m->texture, n);
                at += (udword)((n + 3) & ~(size_t)3);
            }
        }
        // header
        memcpy(d, "RMF97ba", 8);
        amPut32(d + 0x08, 0x500);
        amPut32(d + 0x0C, 0);
        amPut32(d + 0x14, 0);
        amPut32(d + 0x18, nMat);
        amPut32(d + 0x1C, 0);
        amPut32(d + 0x20, oMaterials);
        amPut32(d + 0x24, oMaterials);
        amPut32(d + 0x28, t.nObj);
        memset(d + 0x2C, 0xAB, 24);
        // materials
        for (i = 0; i < materials.count; i++)
        {
            ammaterial *m = &amAt(materials, ammaterial, i);
            ubyte *p = d + oMaterials + (udword)i * AM_MaterialSize;

            amPut32(p + 0x00, matNameAt[i]);
            amPut32(p + 0x04, m->ambient);
            amPut32(p + 0x08, m->diffuse);
            amPut32(p + 0x0C, m->specular);
            amPutReal(p + 0x10, m->kAlpha);
            amPut32(p + 0x14, texNameAt[i]);
            amPut16(p + 0x18, m->flags);
            p[0x1A] = m->nFullAmbient;
        }
        // objects and their lists
        for (i = 0; i < (sdword)t.nObj; i++)
        {
            amtobject *to = &t.obj[i];
            amoutobject *oo = &out[i];
            ubyte *p = d + oObjects + (udword)i * AM_ObjectSize;
            sdword nV = oo->vertices.count, nP = oo->polygons.count, k;
            udword oV = at, oVI = oV + (udword)nV * AM_VertexSize, oN = oVI + (udword)nV * AM_VertexInfoSize;
            udword oP = oN + (udword)(nP + nV) * AM_NormalSize;

            amPut32(p + 0x00, nameAt[i]);
            p[0x04] = to->flags;
            amPut32(p + 0x08, (udword)nV);
            amPut32(p + 0x0C, (udword)nP);                  // face normals: one per polygon
            amPut32(p + 0x10, (udword)nV);                  // vertex normals: one per vertex
            amPut32(p + 0x14, (udword)nP);
            amPut32(p + 0x18, oV);
            amPut32(p + 0x1C, oVI);
            amPut32(p + 0x20, oN);
            amPut32(p + 0x24, oP);
            amPut32(p + 0x28, to->mother < 0 ? 0 : AM_HeaderSize + (udword)to->mother * AM_ObjectSize);
            amPut32(p + 0x2C, to->daughter < 0 ? 0 : AM_HeaderSize + (udword)to->daughter * AM_ObjectSize);
            amPut32(p + 0x30, to->sister < 0 ? 0 : AM_HeaderSize + (udword)to->sister * AM_ObjectSize);
            for (k = 0; k < 16; k++)
            {
                amPutReal(p + 0x34 + 4 * k, to->local[k]);
            }
            for (k = 0; k < nV; k++)
            {
                amvertex *v = &amAt(oo->vertices, amvertex, k);
                ubyte *q = d + oV + (udword)k * AM_VertexSize;
                ubyte *n = d + oN + (udword)(nP + k) * AM_NormalSize;

                amPutReal(q + 0, v->x);
                amPutReal(q + 4, v->y);
                amPutReal(q + 8, v->z);
                amPut32(q + 12, (udword)(nP + k));
                memcpy(d + oVI + (udword)k * AM_VertexInfoSize, v->info, AM_VertexInfoSize);
                amPutReal(n + 0, (real32)v->normal[0]);
                amPutReal(n + 4, (real32)v->normal[1]);
                amPutReal(n + 8, (real32)v->normal[2]);
            }
            for (k = 0; k < nP; k++)
            {
                ampolygon *poly = &amAt(oo->polygons, ampolygon, k);
                ubyte *q = d + oP + (udword)k * AM_PolygonSize;
                ubyte *n = d + oN + (udword)k * AM_NormalSize;
                sdword j;

                amPutReal(n + 0, (real32)poly->normal[0]);
                amPutReal(n + 4, (real32)poly->normal[1]);
                amPutReal(n + 8, (real32)poly->normal[2]);
                amPut32(q + 0x00, (udword)k);
                amPut16(q + 0x04, (uword)poly->v[0]);
                amPut16(q + 0x06, (uword)poly->v[1]);
                amPut16(q + 0x08, (uword)poly->v[2]);
                amPut16(q + 0x0A, poly->material);
                for (j = 0; j < 6; j++)
                {
                    amPutReal(q + 0x0C + 4 * j, poly->uv[j]);
                }
                amPut16(q + 0x24, 0);
                q[0x26] = 0xAB;                             // as in every .geo
                q[0x27] = 0xAB;
            }
            at = oP + (udword)nP * AM_PolygonSize;
        }
        amPut32(d + 0x10, at - AM_HeaderSize);
        free(nameAt);
        free(matNameAt);
        free(texNameAt);
        image = d;
        *outLength = (sdword)at;
        ok = TRUE;
    }

done:
    if (out != NULL)
    {
        for (i = 0; i < (sdword)t.nObj; i++)
        {
            amArrayFree(&out[i].vertices);
            amArrayFree(&out[i].polygons);
        }
        free(out);
    }
    free(materialOf);
    amArrayFree(&materials);
    for (i = 0; i < textureNames.count; i++)
    {
        free(amAt(textureNames, char *, i));
    }
    amArrayFree(&textureNames);
    amObjFree(&obj);
    amTemplateFree(&t);
    if (!ok)
    {
        free(image);
        image = NULL;
    }
    return image;
}

/*=============================================================================
    The round-trip comparison:
=============================================================================*/

// FALSE: the stored normal is not a number (the game's own meshes have a few, on degenerate polygons)
static bool32 amNormalAt(const amtemplate *t, const amtobject *o, sdword index, real64 *out)
{
    const ubyte *n = t->d + o->oN + (udword)index * AM_NormalSize;

    out[0] = amLEReal(n + 0);
    out[1] = amLEReal(n + 4);
    out[2] = amLEReal(n + 8);
    if (!isfinite(out[0]) || !isfinite(out[1]) || !isfinite(out[2]))
    {
        return FALSE;
    }
    if (!amNormalize(out))
    {
        out[0] = out[1] = 0.0;
        out[2] = 1.0;
    }
    return TRUE;
}

static real64 amDifference(const real64 *a, const real64 *b)
{
    real64 d[3] = {a[0] - b[0], a[1] - b[1], a[2] - b[2]};

    return amLength(d);
}

bool32 amCompare(const ubyte *tmpl, sdword tmplLength, const ubyte *image, sdword imageLength, amcompare *r,
                 char *why, size_t whySize)
{
    amtemplate a, b;
    udword i;
    bool32 ok = FALSE;

    memset(r, 0, sizeof(amcompare));
    if (!amTemplateRead(tmpl, tmplLength, &a, why, whySize))
    {
        return FALSE;
    }
    if (!amTemplateRead(image, imageLength, &b, why, whySize))
    {
        amTemplateFree(&a);
        return FALSE;
    }
    if (a.nObj != b.nObj)
    {
        snprintf(why, whySize, "%d objects, the game's mesh has %d", (int)b.nObj, (int)a.nObj);
        goto done;
    }
    r->objects = (sdword)a.nObj;
    for (i = 0; i < a.nObj; i++)
    {
        amtobject *oa = &a.obj[i], *ob = &b.obj[i];
        sdword p;

        if (strcmp(oa->name ? oa->name : "", ob->name ? ob->name : "") != 0 || oa->flags != ob->flags ||
            oa->mother != ob->mother || oa->daughter != ob->daughter || oa->sister != ob->sister ||
            memcmp(oa->local, ob->local, sizeof(oa->local)) != 0)
        {
            snprintf(why, whySize, "object %d differs (name, flags, hierarchy or matrix)", (int)i);
            goto done;
        }
        if (oa->nP != ob->nP)
        {
            snprintf(why, whySize, "object %s: %d polygons, the game's mesh has %d", oa->exportName, (int)ob->nP,
                     (int)oa->nP);
            goto done;
        }
        for (p = 0; p < oa->nP; p++)
        {
            const ubyte *pa = a.d + oa->oP + (udword)p * AM_PolygonSize, *pb = b.d + ob->oP + (udword)p * AM_PolygonSize;
            uword ma = amLE16(pa + 0x0A), mb = amLE16(pb + 0x0A);
            amtmaterial *mta, *mtb;
            bool32 smooth;
            sdword k;

            if (ma >= a.nMat || mb >= b.nMat)
            {
                snprintf(why, whySize, "object %s polygon %d: material index out of range", oa->exportName, (int)p);
                goto done;
            }
            mta = &a.mat[ma];
            mtb = &b.mat[mb];
            if (mta->flags != mtb->flags || mta->ambient != mtb->ambient || mta->diffuse != mtb->diffuse ||
                mta->specular != mtb->specular || !amPathEqual(mta->texture ? mta->texture : "",
                                                               mtb->texture ? mtb->texture : ""))
            {
                r->materialDiffers++;
            }
            smooth = (mta->flags & AM_MDF_Smoothing) != 0;
            for (k = 0; k < 3; k++)
            {
                uword va = amLE16(pa + 0x04 + 2 * k), vb = amLE16(pb + 0x04 + 2 * k);
                const ubyte *xa, *xb;
                real64 posA[3], posB[3], d;
                sdword j;

                if (va >= oa->nV || vb >= ob->nV)
                {
                    snprintf(why, whySize, "object %s polygon %d: vertex index out of range", oa->exportName, (int)p);
                    goto done;
                }
                xa = a.d + oa->oV + (udword)va * AM_VertexSize;
                xb = b.d + ob->oV + (udword)vb * AM_VertexSize;
                for (j = 0; j < 3; j++)
                {
                    posA[j] = amLEReal(xa + 4 * j);
                    posB[j] = amLEReal(xb + 4 * j);
                    d = fabs(posA[j] - posB[j]);
                    if (d > r->maxPosition)
                    {
                        r->maxPosition = d;
                    }
                }
                for (j = 0; j < 2; j++)
                {
                    d = fabs((real64)amLEReal(pa + 0x0C + 4 * (2 * k + j)) - (real64)amLEReal(pb + 0x0C + 4 * (2 * k + j)));
                    if (d > r->maxUV)
                    {
                        r->maxUV = d;
                    }
                }
                if (memcmp(a.d + oa->oVI + (udword)va * AM_VertexInfoSize, b.d + ob->oVI + (udword)vb * AM_VertexInfoSize,
                           AM_VertexInfoSize) != 0)
                {
                    r->glowDiffers++;
                }
                if (smooth)
                {
                    real64 na[3], nb[3];
                    sdword ia = (sdword)amLE32(xa + 12), ib = (sdword)amLE32(xb + 12);

                    if (ia < 0 || ia >= oa->nFN + oa->nVN || ib < 0 || ib >= ob->nFN + ob->nVN)
                    {
                        snprintf(why, whySize, "object %s: vertex normal index out of range", oa->exportName);
                        goto done;
                    }
                    if (!amNormalAt(&a, oa, ia, na))
                    {
                        r->nanNormals++;
                    }
                    else
                    {
                        amNormalAt(&b, ob, ib, nb);
                        d = amDifference(na, nb);
                        if (d > r->maxNormal)
                        {
                            r->maxNormal = d;
                        }
                    }
                }
            }
            {
                real64 na[3], nb[3], d;
                sdword ia = (sdword)amLE32(pa), ib = (sdword)amLE32(pb);

                if (ia < 0 || ia >= oa->nFN + oa->nVN || ib < 0 || ib >= ob->nFN + ob->nVN)
                {
                    snprintf(why, whySize, "object %s: face normal index out of range", oa->exportName);
                    goto done;
                }
                if (!amNormalAt(&a, oa, ia, na))
                {
                    r->nanNormals++;
                    r->polygons++;
                    continue;
                }
                amNormalAt(&b, ob, ib, nb);
                d = amDifference(na, nb);
                if (smooth)
                {
                    r->maxSmoothFace = (d > r->maxSmoothFace) ? d : r->maxSmoothFace;
                }
                else if (d > r->maxNormal)
                {
                    r->maxNormal = d;
                }
            }
            r->polygons++;
        }
    }
    ok = TRUE;
done:
    amTemplateFree(&a);
    amTemplateFree(&b);
    return ok;
}
