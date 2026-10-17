/*
 * hwi_oracle.c - the GOG-side checksum-oracle hook DLL (skeleton): loaded into cataclysm.exe build 12948 by
 * hwi_oracle_loader.exe, it writes the .hwo stream (tools/oracle/hwo_format.h) from the game's own state and
 * checksums, and can start a recording or a recorded-game playback by calling the game's own functions (user decision
 * 5 of 2026-10-10: no OS input is synthesised). Design: docs/m2-checksum-oracle.md section 3-4; as built:
 * docs/m2-r2-oracle.md "dll". Hook sites and addresses: hook_sites.h.
 *
 * Built freestanding (no CRT, no windows.h; kernel32 only) with clang-18 --target=i686-pc-windows-msvc and lld-link:
 * tools/oracle/dll/build.sh. NEVER loaded by the build: the user runs the first live session.
 *
 * Configuration: one environment variable, read at attach, e.g.
 *   HWI_ORACLE="mode=play:oracle01;clock=virtual;rng0=split;out=oracle01-play.hwo"
 *   mode=observe (default) | record:NAME | play:NAME | load:mp:NAME | load:sp:NAME
 *   clock=real (default) | virtual     H3: univUpdate's QueryPerformanceCounter call returns a tick-derived counter
 *   rng0=off (default) | split         H4: render-only and player-AI stream-0 draws use a private stream
 *   out=FILE (default hwi_oracle.hwo, relative to the game directory); the text log is FILE.log
 *   stop=N (record mode: stop recording at tick N, default 9600 = 10 minutes)
 *   menuwait=N (play/load: main-loop calls with no game running before the trigger fires, default 120)
 *   exit=0|1 (play mode: ExitProcess(0) when playback ends, default 1)
 *
 * Rules kept from the design: the detours do no floating-point arithmetic (floats are copied as bits; the one x87
 * operation is the fstp that rounds univGetChecksum's result, exactly as the game's own caller does), never call
 * ranRandomFn, call the two checksum functions with the net log muted, write one record per WriteFile, and patch
 * nothing unless the exe identity and every hook site's bytes match hook_sites.h.
 */
#include <stddef.h>
#include <stdint.h>

#include "../hwo_format.h"
#include "hook_sites.h"

/* ---- the Win32 subset this DLL uses (declared here: no Windows SDK on the build machine) ------------------- */
typedef int BOOL;
typedef uint32_t DWORD;
typedef void *HANDLE;
typedef void *HMODULE;
#define WINAPI __stdcall
#define DLLIMPORT __declspec(dllimport)
#define DLL_PROCESS_ATTACH 1
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 0x1u
#define CREATE_ALWAYS 2u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define MEM_COMMIT 0x1000u
#define MEM_RESERVE 0x2000u
#define PAGE_EXECUTE_READWRITE 0x40u

DLLIMPORT HANDLE WINAPI CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
DLLIMPORT BOOL WINAPI WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
DLLIMPORT BOOL WINAPI CloseHandle(HANDLE);
DLLIMPORT BOOL WINAPI FlushFileBuffers(HANDLE);
DLLIMPORT DWORD WINAPI GetEnvironmentVariableA(const char *, char *, DWORD);
DLLIMPORT HMODULE WINAPI GetModuleHandleA(const char *);
DLLIMPORT void *WINAPI VirtualAlloc(void *, size_t, DWORD, DWORD);
DLLIMPORT BOOL WINAPI VirtualProtect(void *, size_t, DWORD, DWORD *);
DLLIMPORT BOOL WINAPI FlushInstructionCache(HANDLE, const void *, size_t);
DLLIMPORT HANDLE WINAPI GetCurrentProcess(void);
DLLIMPORT void WINAPI ExitProcess(unsigned int);
DLLIMPORT void WINAPI OutputDebugStringA(const char *);
DLLIMPORT void *WINAPI AddVectoredExceptionHandler(DWORD, void *);

/* ---- crash reporter (2026-10-17): first-chance access violations and the game's own int3 (dbgFatalf, when
   dbgInt3Enabled) are logged with address, module, registers, stack top, ebp-chain return addresses and the
   fatal-message buffer, then the exception continues to its normal fate (EXCEPTION_CONTINUE_SEARCH).
   Layouts are the i386 WinNT ones. ---- */
typedef struct { DWORD ExceptionCode, ExceptionFlags; void *ExceptionRecord, *ExceptionAddress; DWORD NumberParameters; DWORD ExceptionInformation[15]; } CR_EXCEPTION_RECORD;
typedef struct { CR_EXCEPTION_RECORD *ExceptionRecord; uint8_t *ContextRecord; } CR_EXCEPTION_POINTERS;
#define CR_CTX(ctx, off) (*(uint32_t *)((ctx) + (off)))      /* i386 CONTEXT: Edi 156 Esi 160 Ebx 164 Edx 168 Ecx 172 Eax 176 Ebp 180 Eip 184 Esp 196 */
#define CAT_FATAL_TEXT 0x00C42C00u                             /* lpText buffer filled by dbgFatalf (vsnprintf, 0x7ff) */
static void crash_report(CR_EXCEPTION_POINTERS *ep);
static uint32_t cr_tick = 0;                                  /* last tick seen by the per-tick path, for the crash line */
static int32_t WINAPI crash_handler(CR_EXCEPTION_POINTERS *ep)
{
    static int busy = 0;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != 0xC0000005u && code != 0x80000003u && code != 0xC0000094u && code != 0xC000001Du) return 0;
    if (busy) return 0;
    busy = 1; crash_report(ep); busy = 0;
    return 0;                                                  /* EXCEPTION_CONTINUE_SEARCH */
}

/* MSVC targets reference _fltused when a translation unit touches floating point (the univGetChecksum return) */
int _fltused = 0;

/* the compiler may emit calls to these */
void *memset(void *d, int c, size_t n)
{
    unsigned char *p = (unsigned char *)d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}

void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *p = (unsigned char *)d;
    const unsigned char *q = (const unsigned char *)s;
    while (n--) *p++ = *q++;
    return d;
}

/* ---- small helpers ---------------------------------------------------------------------------------------- */
#define RD8(a)   (*(volatile uint8_t *)(uintptr_t)(a))
#define RD16(a)  (*(volatile uint16_t *)(uintptr_t)(a))
#define RD32(a)  (*(volatile uint32_t *)(uintptr_t)(a))
#define WR32(a, v) (*(volatile uint32_t *)(uintptr_t)(a) = (uint32_t)(v))

static size_t str_len(const char *s) { size_t n = 0; while (s[n]) n++; return n; }

static void str_cpy(char *d, const char *s, size_t cap)
{
    size_t i = 0;
    if (cap == 0) return;
    while (s[i] && i + 1 < cap) { d[i] = s[i]; i++; }
    d[i] = '\0';
}

static void str_cat(char *d, const char *s, size_t cap)
{
    size_t n = str_len(d);
    if (n < cap) str_cpy(d + n, s, cap - n);
}

static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static int str_prefix(const char *s, const char *p)
{
    while (*p) { if (*s++ != *p++) return 0; }
    return 1;
}

static void hex32(char *d, uint32_t v)
{
    static const char x[] = "0123456789ABCDEF";
    int i;
    for (i = 0; i < 8; i++) d[i] = x[(v >> (28 - 4 * i)) & 0xF];
    d[8] = '\0';
}

static void dec32(char *d, uint32_t v)
{
    char t[12];
    int n = 0, i;
    do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 11);
    for (i = 0; i < n; i++) d[i] = t[n - 1 - i];
    d[n] = '\0';
}

static uint32_t parse_u32(const char *s)
{
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10u + (uint32_t)(*s++ - '0');
    return v;
}

/* ---- configuration ---------------------------------------------------------------------------------------- */
enum { MODE_OBSERVE = 0, MODE_RECORD, MODE_PLAY, MODE_LOAD_MP, MODE_LOAD_SP };

static struct
{
    int mode;
    char name[48];          /* recording or save name */
    int clockVirtual;
    int rng0Split;
    char out[260];
    uint32_t stop;
    uint32_t menuwait;
    int exitOnEnd;
} cfg;

static void parse_config(void)
{
    static char env[512];
    char *p, *item, *val;
    DWORD n;

    cfg.mode = MODE_OBSERVE;
    cfg.stop = 9600;
    cfg.menuwait = 120;
    cfg.exitOnEnd = 1;
    str_cpy(cfg.out, "hwi_oracle.hwo", sizeof(cfg.out));

    n = GetEnvironmentVariableA("HWI_ORACLE", env, sizeof(env));
    if (n == 0 || n >= sizeof(env)) return;
    p = env;
    while (*p)
    {
        item = p;
        while (*p && *p != ';') p++;
        if (*p) *p++ = '\0';
        val = item;
        while (*val && *val != '=') val++;
        if (*val) *val++ = '\0';
        if (str_eq(item, "mode"))
        {
            if (str_prefix(val, "record:")) { cfg.mode = MODE_RECORD; str_cpy(cfg.name, val + 7, sizeof(cfg.name)); }
            else if (str_prefix(val, "play:")) { cfg.mode = MODE_PLAY; str_cpy(cfg.name, val + 5, sizeof(cfg.name)); }
            else if (str_prefix(val, "load:mp:")) { cfg.mode = MODE_LOAD_MP; str_cpy(cfg.name, val + 8, sizeof(cfg.name)); }
            else if (str_prefix(val, "load:sp:")) { cfg.mode = MODE_LOAD_SP; str_cpy(cfg.name, val + 8, sizeof(cfg.name)); }
        }
        else if (str_eq(item, "clock")) cfg.clockVirtual = str_eq(val, "virtual");
        else if (str_eq(item, "rng0")) cfg.rng0Split = str_eq(val, "split");
        else if (str_eq(item, "out")) str_cpy(cfg.out, val, sizeof(cfg.out));
        else if (str_eq(item, "stop")) cfg.stop = parse_u32(val);
        else if (str_eq(item, "menuwait")) cfg.menuwait = parse_u32(val);
        else if (str_eq(item, "exit")) cfg.exitOnEnd = (val[0] != '0');
    }
}

/* ---- output: the .hwo stream and a text log ----------------------------------------------------------------- */
static HANDLE hwoFile = INVALID_HANDLE_VALUE;
static HANDLE logFile = INVALID_HANDLE_VALUE;
static uint8_t recbuf[HWO_RECORD_MAX + 64];

static void log_line(const char *a, const char *b, const char *c)
{
    static char line[512];
    DWORD w;
    line[0] = '\0';
    str_cat(line, "hwi_oracle: ", sizeof(line));
    str_cat(line, a, sizeof(line));
    if (b) str_cat(line, b, sizeof(line));
    if (c) str_cat(line, c, sizeof(line));
    str_cat(line, "\r\n", sizeof(line));
    OutputDebugStringA(line);
    if (logFile != INVALID_HANDLE_VALUE) WriteFile(logFile, line, (DWORD)str_len(line), &w, NULL);
}

static void hwo_write(const uint8_t *buf, size_t n)
{
    DWORD w = 0;
    if (hwoFile == INVALID_HANDLE_VALUE) return;
    if (!WriteFile(hwoFile, buf, (DWORD)n, &w, NULL) || w != n)
    {
        log_line("write to the .hwo stream failed; stream closed", NULL, NULL);
        CloseHandle(hwoFile);
        hwoFile = INVALID_HANDLE_VALUE;
    }
}

static uint16_t x87_cw(void)
{
    uint16_t cw = 0;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}

static void emit_event(uint16_t code, const char *text)
{
    hwo_write(recbuf, hwoPackEvent(recbuf, RD32(CAT_univUpdateCounter), code, text));
}

static int open_outputs(void)
{
    static char logname[280];
    HwoHeader h;
    uint8_t buf[HWO_HEADER_SIZE];

    str_cpy(logname, cfg.out, sizeof(logname));
    str_cat(logname, ".log", sizeof(logname));
    logFile = CreateFileA(logname, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    hwoFile = CreateFileA(cfg.out, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hwoFile == INVALID_HANDLE_VALUE)
    {
        log_line("cannot create ", cfg.out, NULL);
        return 0;
    }
    memset(&h, 0, sizeof(h));
    h.version = HWO_VERSION;
    h.producer = HWO_PRODUCER_GOG;
    h.mode = (cfg.mode == MODE_PLAY) ? HWO_MODE_PLAYBACK : (cfg.mode == MODE_RECORD) ? HWO_MODE_RECORD
           : (cfg.mode == MODE_OBSERVE) ? HWO_MODE_OBSERVE : HWO_MODE_SAVERUN;
    h.options = (cfg.clockVirtual ? HWO_OPT_CLOCK_VIRTUAL : 0u) | (cfg.rng0Split ? HWO_OPT_RNG0_SPLIT : 0u)
              | (cfg.mode == MODE_RECORD ? HWO_OPT_DETERMCOMP : 0u);
    h.ident1 = CAT_TIMEDATESTAMP;
    h.ident2 = CAT_SIZEOFIMAGE;
    h.fpcw = x87_cw();
    h.ticks_per_second = 16;            /* stock: univUpdate(0.0625f) at every call site (no CET in oracle runs) */
    str_cpy(h.tag, cfg.name[0] ? cfg.name : "observe", sizeof(h.tag));
    hwoPackHeader(buf, &h);
    hwo_write(buf, HWO_HEADER_SIZE);
    return hwoFile != INVALID_HANDLE_VALUE;
}

/* ---- calls into the game ---------------------------------------------------------------------------------- */
typedef float (__cdecl *fn_univGetChecksum)(int32_t *);
typedef int32_t (__cdecl *fn_statsGetStatChecksum)(void);
typedef void (__cdecl *fn_charp)(char *);
typedef void (__cdecl *fn_void)(void);
typedef int32_t (__cdecl *fn_VerifySaveFile)(char *);
typedef void (__cdecl *fn_feScreenDisappear)(char *, void *);
typedef int (__cdecl *fn_sprintf)(char *, const char *, ...);

/* the game's checksums, computed by the game's code with netlogfile muted (both functions write to it) */
static void game_checksums(uint32_t *univbits, uint16_t *nships, uint32_t *stat)
{
    uint32_t savedLog = RD32(CAT_netlogfile);
    int32_t n = 0;
    volatile float u;           /* the fstp here is the same float rounding netcheckShow's fstp at 0x004CC411 does */

    WR32(CAT_netlogfile, 0);
    u = ((fn_univGetChecksum)(uintptr_t)CAT_FN_univGetChecksum)(&n);
    *stat = (uint32_t)((fn_statsGetStatChecksum)(uintptr_t)CAT_FN_statsGetStatChecksum)();
    WR32(CAT_netlogfile, savedLog);
    memcpy(univbits, (const void *)&u, 4);
    *nships = (uint16_t)n;
}

/* ---- the 'T' record --------------------------------------------------------------------------------------- */
static int listsValid = 1;          /* cleared for good when a list's walk disagrees with its own count */
static int offsetCheckDone = 0;

static uint64_t hash_pos(uint64_t h, uint32_t obj, uint32_t off)
{
    h = hwoFnvU32(h, RD32(obj + off));
    h = hwoFnvU32(h, RD32(obj + off + 4));
    return hwoFnvU32(h, RD32(obj + off + 8));
}

/* live objects of a list: count + position hash; returns 0 when the walk disagrees with list.num or a node is not in
   this list (a wrong offset) */
static int hash_list(uint32_t list, uint16_t *count, uint64_t *hash)
{
    uint32_t node = RD32(list), obj, walked = 0, live = 0;
    uint64_t h = HWO_FNV64_INIT;
    while (node != 0 && walked < 65536u)
    {
        if (RD32(node + 8) != list) return 0;      /* Node.belongto */
        obj = RD32(node + 0xC);                     /* Node.structptr */
        walked++;
        if ((RD32(obj + CAT_SHIP_flags) & CAT_SOF_Dead) == 0)
        {
            h = hash_pos(h, obj, CAT_SHIP_position);
            live++;
        }
        node = RD32(node);
    }
    if (walked != RD32(list + 8)) return 0;
    *count = (uint16_t)live;
    *hash = h;
    return 1;
}

static uint16_t owner_index(uint32_t player)
{
    uint32_t k, base = CAT_players;
    if (player == 0) return 0xFFFFu;
    for (k = 0; k <= HWO_MAX_PLAYERS; k++, base += CAT_PLAYER_STRIDE)
        if (player == base) return (uint16_t)k;
    return 0xFFFEu;                                 /* not a universe.players entry */
}

static void fill_tick(HwoTick *t)
{
    cr_tick = (uint32_t)RD32(CAT_univUpdateCounter);
    uint32_t node, ship, cmd, i, crc = 0, s, shipsWalked = 0, shipsBad = 0;
    uint16_t id, order, attr;

    memset(t, 0, sizeof(*t));
    t->tick = RD32(CAT_univUpdateCounter);
    t->dt = RD32(CAT_phystimeelapsed);
    t->total = RD32(CAT_totaltimeelapsed);
    t->real = RD32(CAT_realtimeelapsed);
    t->fpcw = x87_cw();
    t->valid = HWO_TV_REAL | HWO_TV_SHIPMISC | HWO_TV_CHECKSUMS | HWO_TV_HEALTH | HWO_TV_PLAYERS;    /* M2-R4 rest-oracle: health +0x170 and the Player fields verified (hook_sites.h) */
    game_checksums(&t->univcheck, &t->nships, &t->statcheck);

    for (s = 0; s < CAT_RAN_STREAMS; s++)
        for (i = 0; i < 5; i++)
            crc = hwoCrc32Update(crc, RD32(CAT_ranStream + 20u * s + 4u * i));
    t->rng_streams = CAT_RAN_STREAMS;
    t->rng_all = crc;
#define COPY_STREAM(dst, n) memcpy(&(dst), (const void *)(uintptr_t)(CAT_ranStream + 20u * (n)), 20)
    COPY_STREAM(t->rng10, 10);
    COPY_STREAM(t->rng6, 6);
    COPY_STREAM(t->rng0, 0);
#undef COPY_STREAM

    t->ships_pos = t->ships_vel = t->ships_rot = t->ships_misc = t->ships_health = HWO_FNV64_INIT;
    for (node = RD32(CAT_ShipList); node != 0 && shipsWalked < 65536u; node = RD32(node))
    {
        ship = RD32(node + 0xC);
        shipsWalked++;
        if (RD32(ship + CAT_SHIP_objtype) != 0) shipsBad++;
        if (RD32(ship + CAT_SHIP_flags) & CAT_SOF_Dead) continue;
        id = RD16(ship + CAT_SHIP_shipID);
        t->ships_pos = hash_pos(hwoFnvU16(t->ships_pos, id), ship, CAT_SHIP_position);
        t->ships_vel = hash_pos(hwoFnvU16(t->ships_vel, id), ship, CAT_SHIP_velocity);
        t->ships_rot = hwoFnvU16(t->ships_rot, id);
        for (i = 0; i < 9; i++) t->ships_rot = hwoFnvU32(t->ships_rot, RD32(ship + CAT_SHIP_coordsys + 4u * i));
        cmd = RD32(ship + CAT_SHIP_command);
        order = cmd ? RD16(cmd + 0x14) : 0xFFFFu;
        attr = cmd ? RD16(cmd + 0x16) : 0xFFFFu;
        t->ships_misc = hwoFnvU16(t->ships_misc, id);
        t->ships_misc = hwoFnvU32(t->ships_misc, RD32(ship + CAT_SHIP_shiptype));
        t->ships_misc = hwoFnvU32(t->ships_misc, RD32(ship + CAT_SHIP_shiprace));
        t->ships_misc = hwoFnvU16(t->ships_misc, owner_index(RD32(ship + CAT_SHIP_playerowner)));
        t->ships_misc = hwoFnvU16(t->ships_misc, order);
        t->ships_misc = hwoFnvU16(t->ships_misc, attr);
        t->ships_health = hwoFnvU32(hwoFnvU16(t->ships_health, id), RD32(ship + CAT_SHIP_health));
        t->ships_n++;
    }

    if (listsValid)
    {
        listsValid = hash_list(CAT_MissileList, &t->missiles_n, &t->missiles_h)
                  && hash_list(CAT_BulletList, &t->bullets_n, &t->bullets_h)
                  && hash_list(CAT_DerelictList, &t->derelicts_n, &t->derelicts_h)
                  && hash_list(CAT_ResourceList, &t->resources_n, &t->resources_h);
        if (!listsValid)
        {
            t->missiles_n = t->bullets_n = t->derelicts_n = t->resources_n = 0;
            t->missiles_h = t->bullets_h = t->derelicts_h = t->resources_h = 0;
            emit_event(HWO_EV_OFFSET_CHECK, "object lists: a walk disagrees with its LinkedList (offsets wrong); lists off");
        }
    }
    if (listsValid) t->valid |= HWO_TV_LISTS;

    for (i = 0; i < HWO_MAX_PLAYERS; i++)
    {
        t->player_ru[i] = RD32(CAT_players + CAT_PLAYER_STRIDE * i + CAT_PLAYER_resourceUnits);
        t->player_ships[i] = RD32(CAT_players + CAT_PLAYER_STRIDE * i + CAT_PLAYER_totalships);
    }

    if (!offsetCheckDone && t->tick != 0)
    {
        /* first-tick structural self-test (design 3.4): every ShipList node is a ship, and the live count matches the
           game's own univGetChecksum count */
        static char text[160];
        char num[12];
        offsetCheckDone = 1;
        text[0] = '\0';
        str_cat(text, (shipsBad == 0 && t->ships_n == t->nships) ? "ships ok" : "ships MISMATCH", sizeof(text));
        str_cat(text, " walked=", sizeof(text)); dec32(num, shipsWalked); str_cat(text, num, sizeof(text));
        str_cat(text, " live=", sizeof(text)); dec32(num, t->ships_n); str_cat(text, num, sizeof(text));
        str_cat(text, " univGetChecksum n=", sizeof(text)); dec32(num, t->nships); str_cat(text, num, sizeof(text));
        str_cat(text, listsValid ? " lists ok" : " lists off", sizeof(text));
        emit_event(HWO_EV_OFFSET_CHECK, text);
        if (shipsBad != 0 || t->ships_n != t->nships)
            log_line("offset self-test failed: ", text, NULL);
    }
    if (offsetCheckDone && (shipsBad != 0))
        t->valid &= (uint16_t)~HWO_TV_SHIPMISC;
}

static void emit_tick(void)
{
    static HwoTick t;
    fill_tick(&t);
    hwo_write(recbuf, hwoPackTick(recbuf, &t));
}

/* ---- trampolines and patching ----------------------------------------------------------------------------- */
static uint8_t *tramp_alloc(uint32_t site, uint32_t len)
{
    uint8_t *t = (uint8_t *)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    uint32_t back;
    if (t == NULL) return NULL;
    memcpy(t, (const void *)(uintptr_t)site, len);     /* whole instructions, no relative branch (hook_sites.h) */
    back = (site + len) - ((uint32_t)(uintptr_t)t + len + 5u);
    t[len] = 0xE9;
    memcpy(t + len + 1, &back, 4);
    return t;
}

static int patch_bytes(uint32_t site, const uint8_t *bytes, uint32_t len)
{
    DWORD old = 0, tmp;
    if (!VirtualProtect((void *)(uintptr_t)site, len, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy((void *)(uintptr_t)site, bytes, len);
    VirtualProtect((void *)(uintptr_t)site, len, old, &tmp);
    FlushInstructionCache(GetCurrentProcess(), (const void *)(uintptr_t)site, len);
    return 1;
}

/* E9/E8 rel32 to target, padded with nops to len */
static int patch_branch(uint32_t site, uint32_t len, uint8_t opcode, void *target)
{
    uint8_t b[16];
    uint32_t rel = (uint32_t)(uintptr_t)target - (site + 5u), i;
    b[0] = opcode;
    memcpy(b + 1, &rel, 4);
    for (i = 5; i < len; i++) b[i] = 0x90;
    return patch_bytes(site, b, len);
}

/* ---- detours ---------------------------------------------------------------------------------------------- */
typedef int (__cdecl *fn_univUpdate)(uint32_t dtbits);          /* real32 argument passed as its bits, untouched */
typedef void (__cdecl *fn_netcheckShow)(void *packet);
typedef uint32_t (__cdecl *fn_ranRandomFn)(int32_t index);
typedef int32_t (__cdecl *fn_taskExecuteAllPending)(int32_t ticks);
typedef void (__cdecl *fn_gameStart)(char *loadfilename);

static fn_univUpdate tr_univUpdate;
static fn_netcheckShow tr_netcheckShow;
static fn_ranRandomFn tr_ranRandomFn;
static fn_taskExecuteAllPending tr_taskExecuteAllPending;
static fn_gameStart tr_gameStart;

static int recordingStarted = 0, recordingStopped = 0, playbackEnded = 0;

static void start_recording(void)
{
    static char path[64];
    /* 2026-10-17: the engine's recorded-game feature is multiplayer/skirmish only (GamePick.c guards it with
       multiPlayerGame; single-player saves have a different chunk layout). Recording the campaign crashed the game at
       the first order and produced a recording that cannot replay, so refuse instead. */
    /* 2026-10-17 (second session): a Cataclysm SKIRMISH is a local game with multiPlayerGame == 0; the engine's own
       recording path handles that case (recPackInGameStartCBSafeToStart's multiPlayerGame==0 branch) and the game
       itself wrote a valid skirmish recording. So refuse only the campaign (singlePlayerGame != 0). */
    if (RD32(CAT_singlePlayerGame) != 0)
    {
        emit_event(HWO_EV_NOTE, "refused: recording needs a SKIRMISH (multiplayer-type) game; the campaign cannot be recorded by the engine");
        log_line("refused: recording needs a SKIRMISH (Main menu -> Multiplayer -> Skirmish); the campaign cannot be recorded by the engine. Nothing recorded.", NULL, NULL);
        return;
    }
    str_cpy(path, "SavedGames\\RecordedGames\\", sizeof(path));
    str_cat(path, cfg.name, sizeof(path));
    /* the game's own in-game record button path (IG_SaveRecordedGame 0x00487EF0 calls the same function): it only
       stores the names and sets startRecordingGameWhenSafe; universeUpdateTask then saves the game and starts */
    ((fn_charp)(uintptr_t)CAT_FN_recPackInGameStartCB)(path);
    emit_event(HWO_EV_RECORD_START, path);
    log_line("recording requested: ", path, NULL);
}

static void on_tick(void)
{
    emit_tick();
    if (cfg.mode == MODE_RECORD)
    {
        if (!recordingStarted)
        {
            recordingStarted = 1;
            start_recording();
        }
        else if (!recordingStopped && RD32(CAT_univUpdateCounter) >= cfg.stop)
        {
            recordingStopped = 1;
            ((fn_void)(uintptr_t)CAT_FN_recPackInGameStopCB)();
            emit_event(HWO_EV_RECORD_STOP, cfg.name);
            log_line("recording stopped at the stop tick", NULL, NULL);
        }
    }
    if (cfg.mode == MODE_PLAY && !playbackEnded && RD32(CAT_playbackEnded) != 0)
    {
        playbackEnded = 1;
        emit_event(HWO_EV_PLAYBACK_END, cfg.name);
        log_line("playback ended", NULL, NULL);
        if (hwoFile != INVALID_HANDLE_VALUE) FlushFileBuffers(hwoFile);
        if (cfg.exitOnEnd) ExitProcess(0);
    }
}

/* H1 */
static int __cdecl det_univUpdate(uint32_t dtbits)
{
    uint32_t before = RD32(CAT_univUpdateCounter);
    int r = tr_univUpdate(dtbits);
    if (RD32(CAT_univUpdateCounter) != before) on_tick();      /* the early-return tick gate does not count */
    return r;
}

/* H2 */
static void __cdecl det_netcheckShow(void *packet)
{
    HwoPacket k;
    uint32_t e;
    tr_netcheckShow(packet);
    e = CAT_netsyncchecksums + 16u * (RD32(CAT_lastchecksumCalculated) & 0x7Fu);
    memset(&k, 0, sizeof(k));
    k.tick = RD32(CAT_univUpdateCounter);
    k.packetnum = RD16(e);
    k.randcheck = RD16(e + 2);
    k.univcheck = RD32(e + 4);
    k.blobcheck = RD32(e + 8);
    k.extra = RD32(e + 12);
    k.flags = HWO_PF_EXTRA;
    k.ncommands = RD16((uint32_t)(uintptr_t)packet + 0x18);    /* Cataclysm header: numberOfCommands at +0x18 */
    k.randSyncErr = RD32(CAT_randSyncErr);
    k.univSyncErr = RD32(CAT_univSyncErr);
    k.blobSyncErr = RD32(CAT_blobSyncErr);
    k.pktSyncErr = RD32(CAT_pktSyncErr);
    k.randSyncErrFrame = RD32(CAT_randSyncErrFrame);
    k.univSyncErrFrame = RD32(CAT_univSyncErrFrame);
    k.blobSyncErrFrame = RD32(CAT_blobSyncErrFrame);
    hwo_write(recbuf, hwoPackPacket(recbuf, &k));
}

/* H3: QueryPerformanceCounter replacement at univUpdate 0x0054C57F (__stdcall, one argument, like the original).
   The code after the call adds floor((qpc - qpcRemainder) / utyTimerDivisor) / taskFrequency to realtimeelapsed and
   keeps the remainder (0x0054C589-0x0054C5F6). Returning qpcRemainder + 15 x utyTimerDivisor makes that exactly
   15 / 240 = 1/16 s per tick. Integer only (15x = 16x - x). */
static int WINAPI vqpc(uint64_t *out)
{
    static int warned = 0;
    uint64_t base, div;
    if (RD32(CAT_taskFrequency) != CAT_TASKFREQ_240_BITS)
    {
        if (!warned) { warned = 1; log_line("clock=virtual: taskFrequency is not 240.0; real counter used", NULL, NULL); }
        return ((int (WINAPI *)(uint64_t *))(uintptr_t)RD32(CAT_QPC_IAT))(out);
    }
    base = *(volatile uint64_t *)(uintptr_t)CAT_qpcRemainder;
    div = *(volatile uint64_t *)(uintptr_t)CAT_utyTimerDivisor;
    *out = base + ((div << 4) - div);
    return 1;
}

/* H4: the private stream-0 generator (ranRandomFn's algorithm, integer only) */
static uint32_t priv[5];
static int privInit = 0;

static int split_caller(uint32_t ra)
{
#define IN_RANGE(start, size, name) if (ra >= (start) && ra < (start) + (size)) return 1;
    HWI_RNG0_SPLIT_RANGES(IN_RANGE)
#undef IN_RANGE
    return 0;
}

static uint32_t __cdecl det_ranRandomFn(int32_t index)
{
    uint32_t ra = (uint32_t)(uintptr_t)__builtin_return_address(0), s;
    if (index != 0 || !split_caller(ra)) return tr_ranRandomFn(index);
    if (!privInit)
    {
        memcpy(priv, (const void *)(uintptr_t)CAT_ranStream, 20);
        privInit = 1;
    }
    /* x, y, z, c, n = priv[0..4] */
    if (priv[1] > priv[0] + priv[3]) { s = priv[1] - (priv[0] + priv[3]); priv[3] = 0; }
    else { s = (priv[0] + priv[3]) - priv[1] - 18u; priv[3] = 1; }
    priv[0] = priv[1]; priv[1] = priv[2];
    priv[2] = s;
    priv[4] = 69069u * priv[4] + 1013904243u;
    return s + priv[4];
}

/* H6: the one-shot start of a playback or a saved game, on the main thread, once the main menu has been idle */
static uint32_t idleCalls = 0;
static int triggered = 0;

static void start_playback_or_load(void)
{
    static char save[64], buf[64];
    int32_t r;
    uint32_t i, n;
    const char *comp;

    if (cfg.mode == MODE_PLAY)
    {
        /* LoadRecordedGame 0x00487CF0, replayed (decision 5): names into recordPacketSaveFileName/recordPacketFileName,
           VerifySaveFile, feScreenDisappear(NULL, NULL), recPackPlayInGameInit(), utyLoadMultiPlayerGameGivenFilename,
           then its three flag writes and the per-player name check (0x00487E4D-0x00487EDE, design 4.3 audit fix) */
        str_cpy((char *)(uintptr_t)CAT_recordPacketSaveFileName, "SavedGames\\RecordedGames\\", 64);
        str_cat((char *)(uintptr_t)CAT_recordPacketSaveFileName, cfg.name, 64);
        str_cpy((char *)(uintptr_t)CAT_recordPacketFileName, (const char *)(uintptr_t)CAT_recordPacketSaveFileName, 64);
        str_cat((char *)(uintptr_t)CAT_recordPacketFileName, ".pkts", 64);
        r = ((fn_VerifySaveFile)(uintptr_t)CAT_FN_VerifySaveFile)((char *)(uintptr_t)CAT_recordPacketSaveFileName);
        if (r != 0)
        {
            emit_event(HWO_EV_NOTE, "VerifySaveFile refused the recording's save; nothing started");
            log_line("VerifySaveFile refused ", (const char *)(uintptr_t)CAT_recordPacketSaveFileName, NULL);
            return;
        }
        ((fn_feScreenDisappear)(uintptr_t)CAT_FN_feScreenDisappear)(NULL, NULL);
        ((fn_void)(uintptr_t)CAT_FN_recPackPlayInGameInit)();
        ((fn_charp)(uintptr_t)CAT_FN_utyLoadMultiPlayerGameGivenFilename)((char *)(uintptr_t)CAT_recordPacketSaveFileName);
        WR32(CAT_LRG_flag1, 1);
        WR32(CAT_LRG_flag2, 1);
        WR32(CAT_LRG_nameMismatch, 0);
        n = RD16(CAT_numPlayers);
        comp = (const char *)(uintptr_t)RD32(CAT_LRG_computerName);
        for (i = 1; i < n; i++)
        {
            ((fn_sprintf)(uintptr_t)CAT_FN_sprintf)(buf, (const char *)(uintptr_t)CAT_LRG_nameFormat, comp, (int)i);
            if (!str_eq((const char *)(uintptr_t)(CAT_LRG_playerNames + 0x12u * (i - 1)), buf))
                WR32(CAT_LRG_nameMismatch, 1);
        }
        emit_event(HWO_EV_PLAYBACK_START, (const char *)(uintptr_t)CAT_recordPacketSaveFileName);
        log_line("playback started: ", (const char *)(uintptr_t)CAT_recordPacketSaveFileName, NULL);
    }
    else
    {
        /* mode C (design 4.1): a saved game loaded and left running. UNVERIFIED whether the front end needs more than
           feScreenDisappear here; the menu's own load path is the reference to compare against */
        str_cpy(save, cfg.mode == MODE_LOAD_MP ? "SavedGames\\MultiPlayer\\" : "SavedGames\\SinglePlayer\\", sizeof(save));
        str_cat(save, cfg.name, sizeof(save));
        r = ((fn_VerifySaveFile)(uintptr_t)CAT_FN_VerifySaveFile)(save);
        if (r != 0)
        {
            emit_event(HWO_EV_NOTE, "VerifySaveFile refused the save; nothing started");
            log_line("VerifySaveFile refused ", save, NULL);
            return;
        }
        ((fn_feScreenDisappear)(uintptr_t)CAT_FN_feScreenDisappear)(NULL, NULL);
        if (cfg.mode == MODE_LOAD_MP)
            ((fn_charp)(uintptr_t)CAT_FN_utyLoadMultiPlayerGameGivenFilename)(save);
        else
            ((fn_charp)(uintptr_t)CAT_FN_utyLoadSinglePlayerGameGivenFilename)(save);
        emit_event(HWO_EV_GAME_LOAD, save);
        log_line("save loaded: ", save, NULL);
    }
}

static int32_t __cdecl det_taskExecuteAllPending(int32_t ticks)
{
    if (!triggered)
    {
        if (RD32(CAT_gameIsRunning) == 0 && RD32(CAT_startingGame) == 0)
        {
            if (++idleCalls >= cfg.menuwait)
            {
                triggered = 1;
                start_playback_or_load();
            }
        }
        else
        {
            idleCalls = 0;
        }
    }
    return tr_taskExecuteAllPending(ticks);
}

/* H7 */
static void __cdecl det_gameStart(char *loadfilename)
{
    tr_gameStart(loadfilename);
    emit_event(loadfilename ? HWO_EV_GAME_LOAD : HWO_EV_GAME_NEW, loadfilename ? loadfilename : "new");
    emit_tick();
}

/* ---- attach: identity, site check, hooks ------------------------------------------------------------------- */
/* VA_Hn / LEN_Hn from the table, so the patch sites cannot drift from the checked sites */
#define DECL_SITE(id, name, va, kind, len, bytes) enum { VA_##id = (va), LEN_##id = (len) };
HWI_HOOK_SITES(DECL_SITE)
#undef DECL_SITE

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* compares the bytes at va with a "AA BB ..." string; on mismatch writes the first differing offset */
static int site_matches(uint32_t va, const char *expected, uint32_t *badOffset)
{
    uint32_t off = 0;
    int hi, lo;
    while (*expected)
    {
        if (*expected == ' ') { expected++; continue; }
        hi = hexval(expected[0]);
        lo = hexval(expected[1]);
        if (hi < 0 || lo < 0) { *badOffset = off; return 0; }
        if (RD8(va + off) != (uint8_t)(hi * 16 + lo)) { *badOffset = off; return 0; }
        off++;
        expected += 2;
    }
    return 1;
}

static int identity_ok(void)
{
    uint32_t base = (uint32_t)(uintptr_t)GetModuleHandleA(NULL), pe;
    if (base != CAT_IMAGE_BASE) return 0;
    if (RD16(base) != 0x5A4Du) return 0;                    /* MZ */
    pe = base + RD32(base + 0x3C);
    if (RD32(pe) != 0x00004550u) return 0;                  /* PE\0\0 */
    if (RD32(pe + 8) != CAT_TIMEDATESTAMP) return 0;        /* FileHeader.TimeDateStamp */
    if (RD32(pe + 24 + 56) != CAT_SIZEOFIMAGE) return 0;    /* OptionalHeader.SizeOfImage */
    return 1;
}

static int check_all_sites(void)
{
    int ok = 1;
    uint32_t bad = 0;
    char num[12];
#define CHECK_SITE(id, name, va, kind, len, bytes) \
    if (!site_matches((va), (bytes), &bad)) \
    { \
        ok = 0; hex32(num, (va) + bad); \
        log_line("site " #id " " name ": bytes differ from hook_sites.h at 0x", num, NULL); \
    }
    HWI_HOOK_SITES(CHECK_SITE)
#undef CHECK_SITE
    return ok;
}

static int install_entry(uint32_t site, uint32_t len, void *detour, void **trampoline)
{
    uint8_t *t = tramp_alloc(site, len);
    if (t == NULL) return 0;
    *trampoline = t;
    return patch_branch(site, len, 0xE9, detour);
}

static void crash_report(CR_EXCEPTION_POINTERS *ep)
{
    char line[900], h[12], d[12]; uint32_t addr = (uint32_t)(uintptr_t)ep->ExceptionRecord->ExceptionAddress, i;
    uint8_t *c = ep->ContextRecord; uint32_t esp = CR_CTX(c, 196);
    HMODULE self = GetModuleHandleA("hwi_oracle.dll");
    str_cpy(line, "CRASH code=", sizeof line); hex32(h, ep->ExceptionRecord->ExceptionCode); str_cat(line, h, sizeof line);
    str_cat(line, " addr=", sizeof line); hex32(h, addr); str_cat(line, h, sizeof line);
    if (addr >= 0x00400000u && addr < 0x00400000u + 0x00894000u) { str_cat(line, " in cataclysm.exe+", sizeof line); hex32(h, addr - 0x00400000u); str_cat(line, h, sizeof line); }
    else if (self && addr >= (uint32_t)(uintptr_t)self && addr < (uint32_t)(uintptr_t)self + 0x20000u) { str_cat(line, " in hwi_oracle.dll+", sizeof line); hex32(h, addr - (uint32_t)(uintptr_t)self); str_cat(line, h, sizeof line); }
    else str_cat(line, " (other module)", sizeof line);
    if (ep->ExceptionRecord->ExceptionCode == 0xC0000005u && ep->ExceptionRecord->NumberParameters >= 2)
    { str_cat(line, ep->ExceptionRecord->ExceptionInformation[0] ? " write to " : " read of ", sizeof line); hex32(h, ep->ExceptionRecord->ExceptionInformation[1]); str_cat(line, h, sizeof line); }
    str_cat(line, " tick=", sizeof line); dec32(d, cr_tick); str_cat(line, d, sizeof line);
    log_line(line, NULL, NULL);
    str_cpy(line, "CRASH regs eax=", sizeof line); hex32(h, CR_CTX(c,176)); str_cat(line, h, sizeof line);
    str_cat(line, " ebx=", sizeof line); hex32(h, CR_CTX(c,164)); str_cat(line, h, sizeof line);
    str_cat(line, " ecx=", sizeof line); hex32(h, CR_CTX(c,172)); str_cat(line, h, sizeof line);
    str_cat(line, " edx=", sizeof line); hex32(h, CR_CTX(c,168)); str_cat(line, h, sizeof line);
    str_cat(line, " esi=", sizeof line); hex32(h, CR_CTX(c,160)); str_cat(line, h, sizeof line);
    str_cat(line, " edi=", sizeof line); hex32(h, CR_CTX(c,156)); str_cat(line, h, sizeof line);
    str_cat(line, " ebp=", sizeof line); hex32(h, CR_CTX(c,180)); str_cat(line, h, sizeof line);
    str_cat(line, " esp=", sizeof line); hex32(h, esp); str_cat(line, h, sizeof line);
    str_cat(line, " eip=", sizeof line); hex32(h, CR_CTX(c,184)); str_cat(line, h, sizeof line);
    log_line(line, NULL, NULL);
    str_cpy(line, "CRASH stack", sizeof line);
    for (i = 0; i < 24; i++) { str_cat(line, " ", sizeof line); hex32(h, RD32(esp + 4u * i)); str_cat(line, h, sizeof line); }
    log_line(line, NULL, NULL);
    str_cpy(line, "CRASH stack2", sizeof line);
    for (i = 24; i < 64; i++) { str_cat(line, " ", sizeof line); hex32(h, RD32(esp + 4u * i)); str_cat(line, h, sizeof line); }
    log_line(line, NULL, NULL);
    { char nm[72]; uint32_t j; const char *g1 = (const char *)(uintptr_t)CAT_recordPacketSaveFileName, *g2 = (const char *)(uintptr_t)CAT_recordPacketFileName;
      for (j = 0; j < 64 && g1[j]; j++) nm[j] = (g1[j] >= 0x20 && g1[j] < 0x7F) ? g1[j] : '?'; nm[j] = 0; log_line("CRASH recordPacketSaveFileName=", nm, NULL);
      for (j = 0; j < 64 && g2[j]; j++) nm[j] = (g2[j] >= 0x20 && g2[j] < 0x7F) ? g2[j] : '?'; nm[j] = 0; log_line("CRASH recordPacketFileName=", nm, NULL); }
    { uint32_t bp = CR_CTX(c,180); str_cpy(line, "CRASH above-ebp", sizeof line);   /* 96 dwords from ebp+8: the callers' frames, scanned offline for return addresses */
      for (i = 0; i < 96; i++) { str_cat(line, " ", sizeof line); hex32(h, RD32(bp + 8u + 4u * i)); str_cat(line, h, sizeof line); if ((i & 31) == 31) { log_line(line, NULL, NULL); str_cpy(line, "CRASH above-ebp", sizeof line); } } }
    { uint32_t ebp = CR_CTX(c,180), n = 0; str_cpy(line, "CRASH frames", sizeof line);
      while (ebp > 0x10000u && ebp < 0x7FFF0000u && n < 12) { uint32_t ret = RD32(ebp + 4); str_cat(line, " ", sizeof line); hex32(h, ret); str_cat(line, h, sizeof line); ebp = RD32(ebp); n++; }
      log_line(line, NULL, NULL); }
    { const char *t = (const char *)(uintptr_t)CAT_FATAL_TEXT; char txt[320]; uint32_t n = 0;
      uint32_t k = 0; while (k < 8 && (t[k] == '\n' || t[k] == '\r' || t[k] == '\t')) k++;   /* the format starts with a newline */
      while (n < sizeof txt - 1 && t[k+n] != 0) { char ch = t[k+n]; txt[n] = (ch >= 0x20 && ch < 0x7F) ? ch : (ch == '\n' ? '|' : '?'); n++; }
      txt[n] = 0; if (n) log_line("CRASH fatal-text: ", txt, NULL); }
}

static void attach(void)
{
    static char hooks[160];
    int ok = 1;

    parse_config();
    if (!identity_ok())
    {
        OutputDebugStringA("hwi_oracle: not cataclysm.exe build 12948 at 0x00400000; nothing patched\r\n");
        return;
    }
    if (!open_outputs()) return;
    AddVectoredExceptionHandler(1, (void *)crash_handler);             /* crash reporter */
    if (!check_all_sites())
    {
        emit_event(HWO_EV_HOOKS, "REFUSED: a hook site differs from hook_sites.h; nothing patched");
        log_line("refusing to hook: a hook site differs; nothing patched", NULL, NULL);
        return;
    }

    hooks[0] = '\0';
    ok &= install_entry(VA_H1, LEN_H1, (void *)det_univUpdate, (void **)&tr_univUpdate);          /* H1 */
    ok &= install_entry(VA_H2, LEN_H2, (void *)det_netcheckShow, (void **)&tr_netcheckShow);       /* H2 */
    ok &= install_entry(VA_H7, LEN_H7, (void *)det_gameStart, (void **)&tr_gameStart);             /* H7 */
    str_cat(hooks, "H1 H2 H7", sizeof(hooks));
    if (cfg.clockVirtual)
    {
        ok &= patch_branch(VA_H3, LEN_H3, 0xE8, (void *)vqpc);                                    /* H3 */
        str_cat(hooks, " H3", sizeof(hooks));
    }
    if (cfg.rng0Split)
    {
        ok &= install_entry(VA_H4, LEN_H4, (void *)det_ranRandomFn, (void **)&tr_ranRandomFn);     /* H4 */
        str_cat(hooks, " H4", sizeof(hooks));
    }
    if (cfg.mode == MODE_RECORD)
    {
        WR32(VA_H5, 1);                                                                       /* H5 */
        str_cat(hooks, " H5", sizeof(hooks));
    }
    if (cfg.mode == MODE_PLAY || cfg.mode == MODE_LOAD_MP || cfg.mode == MODE_LOAD_SP)
    {
        ok &= install_entry(VA_H6, LEN_H6, (void *)det_taskExecuteAllPending, (void **)&tr_taskExecuteAllPending); /* H6 */
        str_cat(hooks, " H6", sizeof(hooks));
    }
    if (!ok) str_cat(hooks, " (a patch FAILED: VirtualAlloc/VirtualProtect)", sizeof(hooks));
    emit_event(HWO_EV_HOOKS, hooks);
    log_line("hooks: ", hooks, NULL);
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, void *reserved)
{
    (void)module;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
        attach();
    return 1;               /* never fail the load: a refusal leaves the game untouched */
}
