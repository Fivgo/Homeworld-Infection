/*
 * hwo_format.h - the checksum-oracle record stream (.hwo), shared by both producers:
 *   - hwi-engine (src/Game/OracleDump.c, 64-bit, producer 2)
 *   - the GOG-side hook DLL (tools/oracle/dll/hwi_oracle.c, 32-bit i686-pc-windows-msvc, producer 1)
 * and read by tools/oracle/hwo_compare.py (which mirrors the layout below; change both together).
 *
 * Design: docs/m2-checksum-oracle.md section 3.4. As implemented: docs/m2-r2-oracle.md "Record format".
 *
 * Rules:
 *   - fixed-width types only; every record is serialised field by field, little-endian, by the hwoPack* functions
 *     below, never by copying a C struct, so a 64-bit and a 32-bit producer write the same bytes;
 *   - floats travel as their IEEE-754 bit patterns (uint32_t); producers copy them, they never compute with them;
 *   - header-only and freestanding (only <stdint.h>/<stddef.h>; no libc), so the DLL can use it without a CRT.
 *
 * Layout (all little-endian):
 *   file header, 64 bytes (hwoPackHeader);
 *   then records: u8 type, u8 reserved (0), u16 length of the payload that follows, payload.
 *     'T' tick record    HWO_TICK_PAYLOAD   bytes (hwoPackTick)
 *     'P' packet record  HWO_PACKET_PAYLOAD bytes (hwoPackPacket)
 *     'E' event record   8 + text bytes     (hwoPackEvent)
 *   A reader skips record types it does not know by their length.
 */
#ifndef HWO_FORMAT_H
#define HWO_FORMAT_H

#include <stddef.h>
#include <stdint.h>

#define HWO_MAGIC           "HWIORCL1"      /* 8 bytes, no NUL in the file */
#define HWO_VERSION         1
#define HWO_HEADER_SIZE     64
#define HWO_RECPREFIX_SIZE  4
#define HWO_TAG_SIZE        36

/* header: producer */
#define HWO_PRODUCER_GOG    1               /* hwi_oracle.dll inside cataclysm.exe build 12948 */
#define HWO_PRODUCER_HWI    2               /* hwi-engine */

/* header: run mode */
#define HWO_MODE_PLAYBACK   1               /* recorded-game playback (design mode A) */
#define HWO_MODE_SAVERUN    2               /* saved game loaded and left running (design mode C) */
#define HWO_MODE_RECORD     3               /* making a recording */
#define HWO_MODE_OBSERVE    4               /* whatever the game does, observed */
#define HWO_MODE_SELFTEST   5               /* hwi-engine /oracleSelfTest: the scripted lockstep start */

/* header: options bitmask */
#define HWO_OPT_CLOCK_VIRTUAL   0x00000001u /* the simulation's clock is derived from the tick counter (GOG: hook H3) */
#define HWO_OPT_RNG0_SPLIT      0x00000002u /* render-only/player-AI draws on stream 0 use a private stream (GOG: H4) */
#define HWO_OPT_DETERMCOMP      0x00000004u /* determCompPlayer forced on (GOG: H5) */
#define HWO_OPT_PERTURBED       0x00000100u /* hwi-engine self-test with a deliberate perturbation (never a reference) */

/* record types */
#define HWO_REC_TICK        0x54            /* 'T' */
#define HWO_REC_PACKET      0x50            /* 'P' */
#define HWO_REC_EVENT       0x45            /* 'E' */

/* 'T' valid bits: a field is compared only when both files set its bit */
#define HWO_TV_REAL         0x0001u         /* real = realtimeelapsed (hwi-engine since M2-R3) */
#define HWO_TV_SHIPMISC     0x0002u         /* ships_misc: type, race, owner, command order/attributes */
#define HWO_TV_HEALTH       0x0004u         /* ships_health */
#define HWO_TV_LISTS        0x0008u         /* missiles/bullets/derelicts/resources counts and hashes */
#define HWO_TV_PLAYERS      0x0010u         /* player_ru / player_ships */
#define HWO_TV_CHECKSUMS    0x0020u         /* univcheck/nships/statcheck computed by the game's own functions */

/* 'P' flags */
#define HWO_PF_EXTRA        0x0001u         /* extra = Cataclysm's 4th sync-entry field (GOG; hwi-engine since M2-R6 oracle-mode-a) */

/* 'E' event codes */
#define HWO_EV_GAME_NEW         1           /* gameStart(NULL) returned (GOG H7 / hwi-engine gameStart end); a 'T' follows */
#define HWO_EV_GAME_LOAD        2           /* gameStart(file) returned; text = file; a 'T' follows */
#define HWO_EV_RECORD_START     3
#define HWO_EV_RECORD_STOP      4
#define HWO_EV_PLAYBACK_START   5
#define HWO_EV_PLAYBACK_END     6
#define HWO_EV_QUIT             7
#define HWO_EV_SELFTEST_START   8           /* hwi-engine self-test start; a 'T' (tick 0) follows */
#define HWO_EV_SELFTEST_END     9
#define HWO_EV_HOOKS            10          /* GOG: hooks installed (text lists them) */
#define HWO_EV_OFFSET_CHECK     11          /* GOG: first-tick offset self-test result */
#define HWO_EV_NOTE             12
#define HWO_EV_PERTURB          13          /* hwi-engine self-test: the deliberate perturbation happened here */

#define HWO_MAX_PLAYERS     8
#define HWO_EVENT_TEXT_MAX  200

typedef struct HwoHeader
{
    uint16_t version;           /* HWO_VERSION */
    uint8_t  producer;          /* HWO_PRODUCER_* */
    uint8_t  mode;              /* HWO_MODE_* */
    uint32_t options;           /* HWO_OPT_* */
    uint32_t ident1;            /* GOG: exe TimeDateStamp; hwi-engine: git commit hash, first 8 hex digits */
    uint32_t ident2;            /* GOG: exe SizeOfImage;   hwi-engine: FNV-1a-32 of the build-flags string */
    uint16_t fpcw;              /* GOG: x87 control word at attach; hwi-engine: MXCSR & 0xFFC0 (control bits; [audit: was "low 16 bits", which carried the sticky status flags]) */
    uint16_t ticks_per_second;  /* 16 (stock), 64 with hwi-engine /gok64Hz */
    char     tag[HWO_TAG_SIZE]; /* run tag (recording name / self-test name), NUL-padded */
} HwoHeader;

typedef struct HwoRanStream     /* one RNG stream, the game's {x, y, z, c, n} */
{
    uint32_t x, y, z, c, n;
} HwoRanStream;

typedef struct HwoTick
{
    uint32_t tick;              /* universe.univUpdateCounter */
    uint32_t dt, total, real;   /* universe.phystimeelapsed / totaltimeelapsed / realtimeelapsed, f32 bits */
    uint16_t fpcw;              /* GOG: fnstcw; hwi-engine: MXCSR & 0xFFC0 (control bits only, see OracleDump.c) */
    uint16_t valid;             /* HWO_TV_* */
    uint32_t univcheck;         /* univGetChecksum(), f32 bits (net log muted) */
    uint16_t nships;            /* its out-parameter */
    uint32_t statcheck;         /* statsGetStatChecksum() (net log muted) */
    uint16_t rng_streams;       /* number of streams rng_all covers (27 on both sides since M2-R3) */
    uint32_t rng_all;           /* hwoCrc32 over every stream's x,y,z,c,n (u32 LE each) in stream order */
    HwoRanStream rng10, rng6, rng0;     /* gamerand (RAN_Game), AI player, particle/mixed */
    uint16_t ships_n;           /* live ships (SOF_Dead clear) in ShipList order */
    uint64_t ships_pos;         /* FNV-1a-64, per live ship: shipID u16, position x,y,z f32 bits */
    uint64_t ships_vel;         /* per live ship: shipID u16, velocity x,y,z */
    uint64_t ships_rot;         /* per live ship: shipID u16, rotinfo.coordsys m11,m21,m31,m12,m22,m32,m13,m23,m33 */
    uint64_t ships_misc;        /* per live ship: shipID u16, shiptype u32, shiprace u32, owner playerIndex u16,
                                   command order u16, command attributes u16 (0xFFFF both when no command) */
    uint64_t ships_health;      /* per live ship: shipID u16, health f32 bits */
    uint16_t missiles_n;  uint64_t missiles_h;  /* per live object: position x,y,z (list order) */
    uint16_t bullets_n;   uint64_t bullets_h;
    uint16_t derelicts_n; uint64_t derelicts_h;
    uint16_t resources_n; uint64_t resources_h;
    uint32_t player_ru[HWO_MAX_PLAYERS];        /* resourceUnits */
    uint32_t player_ships[HWO_MAX_PLAYERS];     /* totalships */
} HwoTick;

typedef struct HwoPacket
{
    uint32_t tick;              /* universe.univUpdateCounter when netcheckShow ran */
    uint16_t packetnum;         /* packet->frame (low 16 bits, as the sync entry stores it) */
    uint16_t randcheck;
    uint32_t univcheck;         /* f32 bits */
    uint32_t blobcheck;
    uint32_t extra;             /* Cataclysm's new 4-byte sync field (GOG: copy of 0x00AB7658), else 0 */
    uint16_t ncommands;         /* packet numberOfCommands */
    uint16_t flags;             /* HWO_PF_* */
    uint32_t randSyncErr, univSyncErr, blobSyncErr, pktSyncErr;
    uint32_t randSyncErrFrame, univSyncErrFrame, blobSyncErrFrame;
} HwoPacket;

#define HWO_TICK_PAYLOAD    242     /* 16 + 4 + 4 + 2 + 4 + 2 + 4 + 60 + 2 + 40 + 40 + 64 */
#define HWO_PACKET_PAYLOAD  52
#define HWO_EVENT_FIXED     8
#define HWO_RECORD_MAX      (HWO_RECPREFIX_SIZE + HWO_TICK_PAYLOAD)     /* the largest record */

/* ---- hashing ---------------------------------------------------------------------------------------------- */

#define HWO_FNV64_INIT      0xcbf29ce484222325ull
#define HWO_FNV64_PRIME     0x00000100000001b3ull

static inline uint64_t hwoFnvByte(uint64_t h, uint8_t b)
{
    return (h ^ (uint64_t)b) * HWO_FNV64_PRIME;
}

static inline uint64_t hwoFnvU16(uint64_t h, uint16_t v)
{
    h = hwoFnvByte(h, (uint8_t)(v & 0xFFu));
    return hwoFnvByte(h, (uint8_t)(v >> 8));
}

static inline uint64_t hwoFnvU32(uint64_t h, uint32_t v)
{
    h = hwoFnvU16(h, (uint16_t)(v & 0xFFFFu));
    return hwoFnvU16(h, (uint16_t)(v >> 16));
}

/* FNV-1a-32 over a NUL-terminated string (hwi-engine build-flags identity) */
static inline uint32_t hwoFnv32Str(const char *s)
{
    uint32_t h = 0x811c9dc5u;
    while (*s)
    {
        h = (h ^ (uint8_t)*s++) * 0x01000193u;
    }
    return h;
}

/* CRC-32 (IEEE 802.3, reflected 0xEDB88320, the zlib/binascii.crc32 value), bitwise: no table, no libc */
static inline uint32_t hwoCrc32Update(uint32_t crc, uint32_t v)
{
    int i, k;
    crc = ~crc;
    for (i = 0; i < 4; i++)
    {
        crc ^= (v >> (8 * i)) & 0xFFu;
        for (k = 0; k < 8; k++)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

/* ---- serialisation ---------------------------------------------------------------------------------------- */

static inline uint8_t *hwoPutU8(uint8_t *p, uint8_t v)   { p[0] = v; return p + 1; }
static inline uint8_t *hwoPutU16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); return p + 2; }
static inline uint8_t *hwoPutU32(uint8_t *p, uint32_t v)
{
    p = hwoPutU16(p, (uint16_t)(v & 0xFFFFu));
    return hwoPutU16(p, (uint16_t)(v >> 16));
}
static inline uint8_t *hwoPutU64(uint8_t *p, uint64_t v)
{
    p = hwoPutU32(p, (uint32_t)(v & 0xFFFFFFFFu));
    return hwoPutU32(p, (uint32_t)(v >> 32));
}
static inline uint8_t *hwoPutRan(uint8_t *p, const HwoRanStream *r)
{
    p = hwoPutU32(p, r->x); p = hwoPutU32(p, r->y); p = hwoPutU32(p, r->z);
    p = hwoPutU32(p, r->c); return hwoPutU32(p, r->n);
}

/* writes the 64-byte file header; returns HWO_HEADER_SIZE */
static inline size_t hwoPackHeader(uint8_t *buf, const HwoHeader *h)
{
    uint8_t *p = buf;
    const char *m = HWO_MAGIC;
    int i;
    for (i = 0; i < 8; i++) p = hwoPutU8(p, (uint8_t)m[i]);
    p = hwoPutU16(p, h->version);
    p = hwoPutU8(p, h->producer);
    p = hwoPutU8(p, h->mode);
    p = hwoPutU32(p, h->options);
    p = hwoPutU32(p, h->ident1);
    p = hwoPutU32(p, h->ident2);
    p = hwoPutU16(p, h->fpcw);
    p = hwoPutU16(p, h->ticks_per_second);
    for (i = 0; i < HWO_TAG_SIZE; i++) p = hwoPutU8(p, (uint8_t)h->tag[i]);
    return (size_t)(p - buf);       /* 8+2+1+1+4+4+4+2+2+36 = 64 */
}

static inline uint8_t *hwoPutPrefix(uint8_t *p, uint8_t type, uint16_t len)
{
    p = hwoPutU8(p, type);
    p = hwoPutU8(p, 0);
    return hwoPutU16(p, len);
}

/* writes a whole 'T' record (prefix + payload); returns its size, HWO_RECPREFIX_SIZE + HWO_TICK_PAYLOAD */
static inline size_t hwoPackTick(uint8_t *buf, const HwoTick *t)
{
    uint8_t *p = hwoPutPrefix(buf, HWO_REC_TICK, HWO_TICK_PAYLOAD);
    int i;
    p = hwoPutU32(p, t->tick);
    p = hwoPutU32(p, t->dt);
    p = hwoPutU32(p, t->total);
    p = hwoPutU32(p, t->real);
    p = hwoPutU16(p, t->fpcw);
    p = hwoPutU16(p, t->valid);
    p = hwoPutU32(p, t->univcheck);
    p = hwoPutU16(p, t->nships);
    p = hwoPutU32(p, t->statcheck);
    p = hwoPutU16(p, t->rng_streams);
    p = hwoPutU32(p, t->rng_all);
    p = hwoPutRan(p, &t->rng10);
    p = hwoPutRan(p, &t->rng6);
    p = hwoPutRan(p, &t->rng0);
    p = hwoPutU16(p, t->ships_n);
    p = hwoPutU64(p, t->ships_pos);
    p = hwoPutU64(p, t->ships_vel);
    p = hwoPutU64(p, t->ships_rot);
    p = hwoPutU64(p, t->ships_misc);
    p = hwoPutU64(p, t->ships_health);
    p = hwoPutU16(p, t->missiles_n);  p = hwoPutU64(p, t->missiles_h);
    p = hwoPutU16(p, t->bullets_n);   p = hwoPutU64(p, t->bullets_h);
    p = hwoPutU16(p, t->derelicts_n); p = hwoPutU64(p, t->derelicts_h);
    p = hwoPutU16(p, t->resources_n); p = hwoPutU64(p, t->resources_h);
    for (i = 0; i < HWO_MAX_PLAYERS; i++)
    {
        p = hwoPutU32(p, t->player_ru[i]);
        p = hwoPutU32(p, t->player_ships[i]);
    }
    return (size_t)(p - buf);
}

/* writes a whole 'P' record; returns HWO_RECPREFIX_SIZE + HWO_PACKET_PAYLOAD */
static inline size_t hwoPackPacket(uint8_t *buf, const HwoPacket *k)
{
    uint8_t *p = hwoPutPrefix(buf, HWO_REC_PACKET, HWO_PACKET_PAYLOAD);
    p = hwoPutU32(p, k->tick);
    p = hwoPutU16(p, k->packetnum);
    p = hwoPutU16(p, k->randcheck);
    p = hwoPutU32(p, k->univcheck);
    p = hwoPutU32(p, k->blobcheck);
    p = hwoPutU32(p, k->extra);
    p = hwoPutU16(p, k->ncommands);
    p = hwoPutU16(p, k->flags);
    p = hwoPutU32(p, k->randSyncErr);
    p = hwoPutU32(p, k->univSyncErr);
    p = hwoPutU32(p, k->blobSyncErr);
    p = hwoPutU32(p, k->pktSyncErr);
    p = hwoPutU32(p, k->randSyncErrFrame);
    p = hwoPutU32(p, k->univSyncErrFrame);
    p = hwoPutU32(p, k->blobSyncErrFrame);
    return (size_t)(p - buf);
}

/* writes a whole 'E' record: tick u32, code u16, text length u16, text (no NUL; cut at HWO_EVENT_TEXT_MAX) */
static inline size_t hwoPackEvent(uint8_t *buf, uint32_t tick, uint16_t code, const char *text)
{
    uint16_t n = 0;
    uint8_t *p;
    if (text != NULL)
    {
        while (text[n] != '\0' && n < HWO_EVENT_TEXT_MAX) n++;
    }
    p = hwoPutPrefix(buf, HWO_REC_EVENT, (uint16_t)(HWO_EVENT_FIXED + n));
    p = hwoPutU32(p, tick);
    p = hwoPutU16(p, code);
    p = hwoPutU16(p, n);
    {
        uint16_t i;
        for (i = 0; i < n; i++) p = hwoPutU8(p, (uint8_t)text[i]);
    }
    return (size_t)(p - buf);
}

#endif /* HWO_FORMAT_H */
