/*
 * hook_sites.h - what hwi_oracle.dll touches in cataclysm.exe (GOG "Homeworld: Emergence" build 12948, 1.01 hotfix,
 * sha256 f03d9a3e...eef3ed4, docs/binary-fingerprint.md). Every address is a VA (image base 0x00400000, the exe has
 * no relocations). Design: docs/m2-checksum-oracle.md section 3 (with its audit fixes); as built:
 * docs/m2-r2-oracle.md "dll". tools/oracle/dll/check_sites.py checks this table against the exe offline.
 *
 * HWI_HOOK_SITES(X): X(id, name, va, kind, patch_len, "expected bytes")
 *   kind ENTRY:    function entry; the first patch_len bytes (whole instructions, no relative branch) are copied to a
 *                  trampoline and replaced by jmp rel32 to the detour (+ nop padding)
 *   kind CALLSITE: a call instruction replaced by call rel32 to a replacement (+ nop padding)
 *   kind DATA:     a variable written once at attach (expected = its value at load: .bss, zero)
 * The expected bytes cover the patched bytes and the instructions after them, as read with objdump/pe.py for this
 * item (2026-10-10). At attach the DLL compares EVERY site, whatever the options; one mismatch and it patches nothing.
 */
#ifndef HWI_HOOK_SITES_H
#define HWI_HOOK_SITES_H

/* exe identity (PE header, checked at attach before anything else) */
#define CAT_IMAGE_BASE          0x00400000u
#define CAT_TIMEDATESTAMP       0x3A4265AAu     /* Thu Dec 21 2000 */
#define CAT_SIZEOFIMAGE         0x00894000u

#define HOOK_ENTRY      1
#define HOOK_CALLSITE   2
#define HOOK_DATA       3

#define HWI_HOOK_SITES(X) \
    /* H1: per-tick record, on univUpdate's exit, only when univUpdateCounter moved (mov eax,[0x00A6ED74]; sub esp,0xD0) */ \
    X(H1, "univUpdate",            0x0054C3F0u, HOOK_ENTRY,    5, "A1 74 ED A6 00 81 EC D0 00 00 00") \
    /* H2: the game's own sync checksums once per sync packet (sub esp,8; push ebx; push ebp; push esi; lea eax,[esp+0x10]; push edi) */ \
    X(H2, "netcheckShow",          0x004CC400u, HOOK_ENTRY,    5, "83 EC 08 53 55 56 8D 44 24 10 57") \
    /* H3: univUpdate's QueryPerformanceCounter call (call [0x0072A228]) -> virtual clock; preceded by push ecx at 0x0054C56D */ \
    X(H3, "univUpdate+0x18F QPC",  0x0054C57Fu, HOOK_CALLSITE, 6, "FF 15 28 A2 72 00 8B 6C 24 10") \
    /* H4: ranRandomFn entry, stream-0 split (mov eax,[esp+4]; push esi; lea eax,[eax+eax*4]) */ \
    X(H4, "ranRandomFn",           0x004EF860u, HOOK_ENTRY,    5, "8B 44 24 04 56 8D 04 80") \
    /* H5: determCompPlayer (.bss), forced to 1 in record mode */ \
    X(H5, "determCompPlayer",      0x00A75248u, HOOK_DATA,     4, "00 00 00 00") \
    /* H6: taskExecuteAllPending entry, main-loop one-shot trigger (push ebp; mov ebp,esp; push ecx; push ebx; push esi; push edi) */ \
    X(H6, "taskExecuteAllPending", 0x0052A840u, HOOK_ENTRY,    5, "55 8B EC 51 53 56 57") \
    /* H7: gameStart exit record, tick 0 (sub esp,0xC; push ebx; push ebp; push esi; push edi) */ \
    X(H7, "gameStart",             0x005A9290u, HOOK_ENTRY,    5, "83 EC 0C 53 55 56 57")

/* ---- game functions the DLL calls (all __cdecl; prototypes as applied in the round-5 Ghidra export) ---------- */
#define CAT_FN_univGetChecksum                      0x00549F50u /* real32 (sdword *numShipsInChecksum) */
#define CAT_FN_statsGetStatChecksum                 0x00525170u /* sdword (void) */
#define CAT_FN_recPackInGameStartCB                 0x004CBAF0u /* void (char *filename): names + startRecordingGameWhenSafe */
#define CAT_FN_recPackInGameStopCB                  0x004CBD10u /* void (void) */
#define CAT_FN_recPackPlayInGameInit                0x004CC010u /* void (void): playPackets = TRUE, opens recordPacketFileName */
#define CAT_FN_VerifySaveFile                       0x004FDF40u /* sdword (char *filename), 0 = OK */
#define CAT_FN_feScreenDisappear                    0x00475200u /* void (char *, featom *) (thunk to feCurrentScreenDelete) */
#define CAT_FN_utyLoadMultiPlayerGameGivenFilename  0x005AAA50u /* void (char *filename) */
#define CAT_FN_utyLoadSinglePlayerGameGivenFilename 0x005AA5F0u /* void (char *filename) */
#define CAT_FN_sprintf                              0x00670B30u /* the CRT sprintf LoadRecordedGame calls (0x00487E8D) */

/* ---- globals ------------------------------------------------------------------------------------------------ */
#define CAT_universe                0x00B62A00u
#define CAT_phystimeelapsed         0x00B62ABCu /* f32 */
#define CAT_totaltimeelapsed        0x00B62AC0u /* f32 */
#define CAT_realtimeelapsed         0x00B62AC4u /* f32, Cataclysm only (QPC-fed in univUpdate) */
#define CAT_ShipList                0x00B62AF0u /* LinkedList {head, tail, num}; Node {next, prev, belongto, structptr} */
#define CAT_BulletList              0x00B62AFCu /* universe + 0xFC  (CET offsets; checked every tick, see LISTS) */
#define CAT_ResourceList            0x00B62B08u /* universe + 0x108 */
#define CAT_DerelictList            0x00B62B14u /* universe + 0x114 */
#define CAT_MissileList             0x00B62B2Cu /* universe + 0x12C */
#define CAT_univUpdateCounter       0x00B62BACu
#define CAT_numPlayers              0x00B674F8u /* uword (universe.numPlayers) */
#define CAT_singlePlayerGame        0x00A43C9Cu /* bool32 (symbols.csv r6); recording is refused when set */
#define CAT_multiPlayerGame         0x00A43C94u /* bool32 (symbols.csv r6); recording needs it set (skirmish/LAN) */
#define CAT_players                 0x00B67500u /* Player[], stride 0x6530 */
#define CAT_PLAYER_STRIDE           0x6530u
#define CAT_PLAYER_resourceUnits    0x4u        /* VERIFIED (M2-R4 rest-oracle): CET universe.h asserts; gameStart 0x005a9290 writes it; the Player layout is in src/Game/Universe.h */
#define CAT_PLAYER_totalships       0x2Cu       /* VERIFIED (M2-R4 rest-oracle): unitCapCanCreateShip 0x0053ece0 pPVar3->totalships; CET universe.h assert */
#define CAT_ranStream               0x00C2FD80u /* 27 x {x, y, z, c, n} */
#define CAT_RAN_STREAMS             27
#define CAT_netsyncchecksums        0x00C30FC0u /* 128 x 16-byte entries {u16 packetnum, u16 randcheck, f32 univcheck, u32 blobcheck, u32 extra} */
#define CAT_lastchecksumCalculated  0x00C30FA4u /* index of the entry netcheckShow just wrote */
#define CAT_randSyncErr             0x00C317D4u
#define CAT_univSyncErr             0x00C317DCu
#define CAT_blobSyncErr             0x00C319E0u
#define CAT_pktSyncErr              0x00C317CCu
#define CAT_randSyncErrFrame        0x00C317C0u
#define CAT_univSyncErrFrame        0x00C317C4u
#define CAT_blobSyncErrFrame        0x00C30FA0u
#define CAT_netlogfile              0x00A4CA30u /* FILE *; muted while the DLL calls the checksum functions */
#define CAT_gameIsRunning           0x00A43C90u
#define CAT_startingGame            0x00A43CB8u
#define CAT_playPackets             0x00A7518Cu
#define CAT_recordPackets           0x00A75188u
#define CAT_playbackEnded           0x00A6C5C8u /* set when playback hits NO_PACKET (meaning inferred, design 2.5) */
#define CAT_recordPacketFileName    0x00B39F00u /* char[64] */
#define CAT_recordPacketSaveFileName 0x00B39F40u /* char[64] */
#define CAT_LRG_flag1               0x00A7519Cu /* LoadRecordedGame writes 1 (0x00487E5C); read by 28 sim functions */
#define CAT_LRG_flag2               0x00A751A0u /* LoadRecordedGame writes 1 (0x00487E62) */
#define CAT_LRG_nameMismatch        0x00A75190u /* LoadRecordedGame: 0, or 1 if a "%s %i" player name differs */
#define CAT_LRG_nameFormat          0x008812D8u /* "%s %i" */
#define CAT_LRG_computerName        0x00BB3FB8u /* char * (the computer-player name string) */
#define CAT_LRG_playerNames         0x00C39652u /* 0x12-byte names, player 1 first */
#define CAT_qpcRemainder            0x00A6D9A0u /* u64: QPC value of the last whole timer period (univUpdate 0x0054C589) */
#define CAT_utyTimerDivisor         0x00AB7520u /* u64: QPF / 240 */
#define CAT_taskFrequency           0x00BB3260u /* f32: 240.0 (bits 0x43700000) */
#define CAT_TASKFREQ_240_BITS       0x43700000u
#define CAT_QPC_IAT                 0x0072A228u /* IAT slot of QueryPerformanceCounter */

/* Ship (offsets from code, docs/m2-checksum-oracle.md 3.4) */
#define CAT_SHIP_flags              0x14u       /* SOF_Dead = 0x800 in Cataclysm */
#define CAT_SOF_Dead                0x800u
#define CAT_SHIP_objtype            0x10u       /* 0 = ship */
#define CAT_SHIP_position           0x58u
#define CAT_SHIP_velocity           0x64u
#define CAT_SHIP_coordsys           0x80u       /* 9 x f32, m11 m21 m31 m12 m22 m32 m13 m23 m33 */
#define CAT_SHIP_shiptype           0x2B0u
#define CAT_SHIP_shiprace           0x2B4u
#define CAT_SHIP_playerowner        0x32Cu      /* Player *; the owner index is found by address in CAT_players */
#define CAT_SHIP_command            0x370u      /* CommandToDo *; order u16 at +0x14, attributes u16 at +0x16 */
#define CAT_SHIP_health             0x170u      /* VERIFIED (M2-R4 rest-oracle): ApplyDamageToTarget 0x00546250 clamps [esi+0x170] to staticinfo+0xA4 maxhealth; also read as health at 0x0054c3f0 (repair), 0x00580xxx (target), docs/m2-r4-rest-oracle.md "oracle" */
#define CAT_SHIP_shipID             0x43Cu      /* u16 */

/* H4: stream-0 callers that draw from the private stream with rng0=split: [start, end) from functions.csv (r5) */
#define HWI_RNG0_SPLIT_RANGES(X) \
    X(0x00523D00u,  583, "Make3dStar") \
    X(0x005776F0u, 9890, "cat_rndRenderObjectList") \
    X(0x004DE880u, 1391, "cat_partRenderUnk1System") \
    X(0x00428E60u,  945, "btgColorVertices") \
    X(0x00516750u,  604, "cat_blastRenderAll") \
    X(0x005163B0u,  598, "cat_blastDrawLightning") \
    X(0x00576010u,  797, "cat_rndLightningBoltDraw") \
    X(0x00576330u,  431, "cat_rndLinkLightningDraw") \
    X(0x005C60A0u, 1806, "cat_sentinelFieldSegmentDraw") \
    X(0x00523480u,  313, "musicEventPlay") \
    X(0x00523660u, 1512, "musicEventUpdateVolume") \
    X(0x00413070u,  460, "airDumbResearchRequests")

#endif /* HWI_HOOK_SITES_H */
