/*=============================================================================
    Name    : TeamColourTest.c
    Purpose : HWI-TEST (M3-R1 team-colours-warp): the team-colour dump and self-test (/catTeamColourTest; harness flag
              --teamcolour in the single-player modes: missiontest, sp, loadgame). Not in the exe; read only: it never
              changes a ship, a texture or a random draw, so every oracle stream is the same with it.

    The player's colours (the front end's colour picker: utyBaseColor / utyStripeColor, cpColorsPicked; Cataclysm.cfg
    rows PlayerBaseColor / PlayerStripeColor / ColorsPicked) go into teColorSchemes[0] at gameStart (a saved game
    brings its own schemes) and are kept for the human player by levelPreInit at every level change. A ship draws its
    textures with the handle of its own colour scheme (the mesh material's ->texture[ship->colorScheme]); the handle's
    high word is the palette index of the registry entry, whose colour info at that index is the (base, stripe) pair
    the texture is coloured with. A paletted texture is coloured when its palette is built (trMeshSortListLoad) only if
    its mesh-sort list carries a team flag (texreg.c catTrPaletteTeamFlags records the flags each palette got).

    Lines (all start "teamcolour:"):
      dump before|statics|after ...  singlePlayerLoadNewLevel's three points (the old level, after the new level's
                                     statics, after its textures): the schemes, every ship type of the player's race
                                     that player 0 has (the universe and the hyperspace ether) with the colour schemes
                                     its ships use and, for its level-0 mesh, each textured material's registry entry
                                     for scheme 0 ("tex": handle, index, palette, nPalettes, flags, usage count, shared
                                     parent, sizes, the colour info at the palette) and its palette ("pal": the block,
                                     its usage count, the palette's CRC, the team flags it was built with and those
                                     its mesh-sort list has, the no-palette entry and its keys)
      check <where> ...              the self-test's check (catTeamColourCheck): after each level's textures are
                                     loaded ("after"), at the 64th tick of each level ("level") and at the run's end
                                     ("end"); one "fail" line per fault and a result
      done checks=N failed=F afterLoads=N levels=... result=PASS|FAIL   at the run's end (the mission self-test's done
                                     line, the load-game test's, or the sp loop's quit)

    Created 2026-10-20 (M3-R1)
=============================================================================*/

#include <stdio.h>
#include <string.h>

#include "TeamColourTest.h"
#include "Debug.h"
#include "Globals.h"
#include "LOD.h"
#include "Mesh.h"
#include "ObjTypes.h"
#include "SinglePlayer.h"
#include "SpaceObj.h"
#include "StatScript.h"
#include "Teams.h"
#include "texreg.h"
#include "Universe.h"
#include "utility.h"
#include "ColPick.h"
#include "CRC32.h"
#include "File.h"
#include "Memory.h"

/*=============================================================================
    Data:
=============================================================================*/
bool32 catTeamColourTest = FALSE;           // /catTeamColourTest given
bool32 catTeamColourLoadedGame = FALSE;     // the colours came from a saved game (the load-game test sets it)
sdword catTeamColourNextTicks = 32;         // /catTeamColourTicks N: the missiontest run's ticks in the next mission level

static udword tcLastTick = 0xffffffff;      // the last universe tick seen by the frame hook
static udword tcSegment = 0;                // the level segment (a new level restarts the tick counter)
static udword tcSegmentTicks = 0;           // ticks seen in this segment
static bool32 tcSegmentChecked = FALSE;     // the segment's "level" check is done
static sdword tcChecks = 0, tcChecksFailed = 0, tcAfterChecks = 0;
static bool32 tcDone = FALSE;
static char tcLevels[768] = "";             // the levels checked, in order

#define TC_MAX_TYPES    64

/*=============================================================================
    Functions:
=============================================================================*/
static const char *tcLevelName(void)
{
    return (spMissionsFile[0] != '\0') ? spMissionsFile : (CurrentLevelName[0] != '\0') ? CurrentLevelName : "-";
}

// Cataclysm's player is always the Somtaaw (gameStart; the tutorial too)
#define TC_PLAYER_RACE  RACE_Sect

// the colour infos of an allocated registry entry with palettes, or NULL
static trcolorinfo *tcInfos(sdword index)
{
    texreg *reg = &trTextureRegistry[index];

    if (!bitTest(reg->flags, TRF_Allocated) || reg->palettes == NULL || reg->nPalettes <= 0)
    {
        return NULL;
    }
    return trColorInfosPointer(index);
}

// the team flags of the entry's mesh-sort list (trMeshSortAdd: the same mesh, the same colour infos) from every
//      allocated entry's own flags: what a load of the whole list from scratch gives its palettes
static udword tcExpectedTeamFlags(sdword index)
{
    texreg *reg = &trTextureRegistry[index], *other;
    trcolorinfo *infos = tcInfos(index), *otherInfos;
    udword flags = 0;
    sdword j;

    if (infos == NULL)
    {
        return 0;
    }
    for (j = 0; j < TR_RegistrySize; j++)
    {
        other = &trTextureRegistry[j];
        if (!bitTest(other->flags, TRF_Allocated) || other->meshReference != reg->meshReference ||
            other->nPalettes != reg->nPalettes || (otherInfos = tcInfos(j)) == NULL)
        {
            continue;
        }
        if (memcmp(otherInfos, infos, sizeof(trcolorinfo) * reg->nPalettes) == 0)
        {
            flags |= other->flags & (TRF_TeamColor0 | TRF_TeamColor1);
        }
    }
    return flags;
}

// one material's registry entry for the handle of colour scheme <scheme>
static void tcMaterialLine(const char *tag, const char *typeName, sdword material, trhandle handle, sdword scheme)
{
    texreg *reg;
    trcolorinfo *infos;
    sdword index, palette;

    if (handle == TR_Invalid)
    {
        dbgMessagef("teamcolour: tex %s %s mat=%d cs=%d handle=invalid", tag, typeName, (int)material, (int)scheme);
        return;
    }
    index = (sdword)trIndex(handle);
    palette = (sdword)trPaletteIndex(handle);
    reg = &trTextureRegistry[index];
    infos = tcInfos(index);
    dbgMessagef("teamcolour: tex %s %s mat=%d cs=%d handle=0x%lx idx=%d pal=%d nPal=%d flags=0x%02x%s%s use=%d "
                "shared=%d size=%dx%d/%dx%d info=%08x/%08x name=%s", tag, typeName, (int)material, (int)scheme,
                (unsigned long)handle, (int)index, (int)palette, (int)reg->nPalettes, (unsigned)reg->flags,
                bitTest(reg->flags, TRF_Pending) ? " pending" : " loaded",
                bitTest(reg->flags, TRF_Paletted) ? " paletted" : " rgb", (int)reg->nUsageCount,
                (reg->sharedFrom == TR_NotShared) ? -1 : (int)reg->sharedFrom, (int)reg->diskWidth,
                (int)reg->diskHeight, (int)reg->scaledWidth, (int)reg->scaledHeight,
                (palette < reg->nPalettes && infos != NULL) ? (unsigned)infos[palette].base : 0u,
                (palette < reg->nPalettes && infos != NULL) ? (unsigned)infos[palette].detail : 0u,
                (reg->fileName != NULL) ? reg->fileName : "-");
    if (bitTest(reg->flags, TRF_Paletted) && !bitTest(reg->flags, TRF_Pending) && reg->sharedFrom == TR_NotShared &&
        reg->palettes != NULL && palette < reg->nPalettes)
    {                                       // the palette the texture is drawn with, and its no-palette entry
        nopalreg *np = (reg->handle < TR_RegistrySize) ? trNoPalStructure(reg->handle) : NULL;

        dbgMessagef("teamcolour: pal %s %s mat=%d cs=%d idx=%d palettes=%p count=%u palcrc=%08x built=0x%02x "
                    "expected=0x%02x np=%u npparent=%ld npkeys=%08x,%08x,%08x,%08x key=%08x", tag, typeName,
                    (int)material, (int)scheme, (int)index, (void *)reg->palettes,
                    (unsigned)(((color *)reg->palettes)[0] >> 24),
                    (unsigned)crc32Compute(reg->palettes + palette * TR_PaletteSize + 4, TR_PaletteSize - 4),
                    (unsigned)catTrPaletteTeamFlags[index], (unsigned)tcExpectedTeamFlags(index),
                    (unsigned)reg->handle, (np != NULL) ? (long)np->texreghandle : -2L,
                    (np != NULL) ? (unsigned)np->crc[0] : 0u, (np != NULL) ? (unsigned)np->crc[1] : 0u,
                    (np != NULL) ? (unsigned)np->crc[2] : 0u, (np != NULL) ? (unsigned)np->crc[3] : 0u,
                    (unsigned)(udword)(memsize)(reg->palettes + palette * TR_PaletteSize));
    }
}

// the level-0 mesh of a loaded static, or NULL
static meshdata *tcMesh(ShipStaticInfo *info)
{
    lodinfo *lod = info->common.LOD;

    if (lod == NULL || lod->nLevels < 1 || (lod->level[0].flags & LM_LODType) != LT_Mesh)
    {
        return NULL;
    }
    return (meshdata *)lod->level[0].pData;
}

// player 0's ship types of the player's race (the universe, then the ether) and the colour schemes they use;
//      *kuunLan: whether player 0's sMothership (the Kuun-Lan) is among them, and *kuunLanScheme0: all use scheme 0
static sdword tcPlayerTypes(ShipStaticInfo **types, udword *schemes, sdword *counts, bool32 *kuunLan,
                            bool32 *kuunLanScheme0)
{
    sdword nTypes = 0, i, pass;
    Node *node;
    Ship *ship;

    *kuunLan = FALSE;
    *kuunLanScheme0 = TRUE;
    for (pass = 0; pass < 2; pass++)
    {
        node = (pass == 0) ? universe.ShipList.head : singlePlayerGameInfo.ShipsInHyperspace.head;
        for (; node != NULL; node = node->next)
        {
            ship = (pass == 0) ? (Ship *)listGetStructOfNode(node) : ((InsideShip *)listGetStructOfNode(node))->ship;
            if (ship == NULL || ship->playerowner != &universe.players[0] || ship->shiprace != TC_PLAYER_RACE)
            {
                continue;
            }
            if (strcmp(ShipTypeToStr(ship->shiptype), "sMothership") == 0)
            {
                *kuunLan = TRUE;
                if (ship->colorScheme != 0)
                {
                    *kuunLanScheme0 = FALSE;
                }
            }
            for (i = 0; i < nTypes; i++)
            {
                if (types[i] == ship->staticinfo)
                {
                    break;
                }
            }
            if (i == nTypes)
            {
                if (nTypes >= TC_MAX_TYPES)
                {
                    continue;
                }
                types[nTypes] = ship->staticinfo;
                schemes[nTypes] = 0;
                counts[nTypes] = 0;
                nTypes++;
            }
            counts[i]++;
            if (ship->colorScheme >= 0 && ship->colorScheme < 32)
            {
                schemes[i] |= 1u << ship->colorScheme;
            }
        }
    }
    return nTypes;
}

/*-----------------------------------------------------------------------------
    Name        : catTeamColourDump
    Description : HWI-TEST: the dump described at the top of the file.
    Inputs      : tag - where the dump is taken (before / statics / after)
-----------------------------------------------------------------------------*/
void catTeamColourDump(const char *tag)
{
    ShipStaticInfo *types[TC_MAX_TYPES];
    udword schemes[TC_MAX_TYPES];           // bit per colour scheme seen on that type
    sdword counts[TC_MAX_TYPES];
    sdword nTypes, i, m;
    bool32 kuunLan, kuunLanScheme0;

    if (!catTeamColourTest)
    {
        return;
    }
    dbgMessagef("teamcolour: dump %s tick=%u segment=%u level=%s mission=%d picker=%08x/%08x picked=%d", tag,
                (unsigned)universe.univUpdateCounter, (unsigned)tcSegment, tcLevelName(),
                (int)singlePlayerGameInfo.currentMission, (unsigned)utyBaseColor, (unsigned)utyStripeColor,
                (int)cpColorsPicked);
    for (i = 0; i < MAX_MULTIPLAYER_PLAYERS; i++)
    {
        dbgMessagef("teamcolour: scheme %s %d %08x/%08x", tag, (int)i, (unsigned)teColorSchemes[i].textureColor.base,
                    (unsigned)teColorSchemes[i].textureColor.detail);
    }
    nTypes = tcPlayerTypes(types, schemes, counts, &kuunLan, &kuunLanScheme0);
    for (i = 0; i < nTypes; i++)
    {
        ShipStaticInfo *info = types[i];
        meshdata *mesh = tcMesh(info);
        const char *name = ShipTypeToStr(info->shiptype);

        dbgMessagef("teamcolour: ship %s %s n=%d schemes=0x%02x teamColor=%d%d%d%d%d%d%d%d mesh=%s", tag, name,
                    (int)counts[i], (unsigned)schemes[i],
                    (int)info->shipOrDerelict.teamColor[0], (int)info->shipOrDerelict.teamColor[1],
                    (int)info->shipOrDerelict.teamColor[2], (int)info->shipOrDerelict.teamColor[3],
                    (int)info->shipOrDerelict.teamColor[4], (int)info->shipOrDerelict.teamColor[5],
                    (int)info->shipOrDerelict.teamColor[6], (int)info->shipOrDerelict.teamColor[7],
                    (mesh != NULL && mesh->fileName != NULL) ? mesh->fileName : "-");
        if (mesh == NULL)
        {
            continue;
        }
        for (m = 0; m < mesh->nLocalMaterials + mesh->nPublicMaterials; m++)
        {
            materialentry *mat = &mesh->localMaterial[m];

            if (!mat->bTexturesRegistered || mat->texture == NULL || mat->texture == (trhandle *)TR_Invalid)
            {
                continue;
            }
            tcMaterialLine(tag, name, m, ((trhandle *)mat->texture)[0], 0);
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : catTeamColourCheck
    Description : HWI-TEST: the self-test's check. For every loaded static of the player's race that player 0 can own
                  (teamColor[0]: the Kuun-Lan's sMothership and modules, the Acolyte, the Worker, the Recon ...), each
                  textured material of its level-0 mesh, the handle of colour scheme 0:
                    - valid, its entry allocated and loaded, nPalettes > 0, the palette index inside it;
                    - the colour info at the palette index is the player's base / stripe, and that index is the one
                      trColorsEqual finds for the player's colours;
                    - a paletted texture's palette was built with the team flags of its mesh-sort list (the flags of
                      every entry in that list: a list with a team flag colours every palette in it).
                  The player's colours: teColorSchemes[0]; in a new game (not a loaded save) with cpColorsPicked they
                  must be the picker's (utyBaseColor / utyStripeColor). Player 0's sMothership (the Kuun-Lan, in the
                  universe or the ether) must use scheme 0 and its textures must have been checked.
    Inputs      : where - the check's point (after, level, end)
    Return      : TRUE when it passed
-----------------------------------------------------------------------------*/
bool32 catTeamColourCheck(const char *where)
{
    ShipStaticInfo *types[TC_MAX_TYPES];
    udword schemes[TC_MAX_TYPES];
    sdword counts[TC_MAX_TYPES];
    trcolorinfo player = teColorSchemes[0].textureColor;
    sdword checked = 0, failed = 0, statics = 0, mothership = 0, acolyte = 0, worker = 0, recon = 0, coloured = 0;
    sdword m, nTypes;
    ShipType type;
    bool32 kuunLan, kuunLanScheme0, schemeOk = TRUE, pass;
    char ln[64];

    if (!catTeamColourTest)
    {
        return TRUE;
    }
    if (cpColorsPicked && !catTeamColourLoadedGame &&
        (player.base != utyBaseColor || player.detail != utyStripeColor))
    {                                                       // a new game: the picker's colours are the player's
        schemeOk = FALSE;
        dbgMessagef("teamcolour: fail %s scheme0=%08x/%08x picker=%08x/%08x", where, (unsigned)player.base,
                    (unsigned)player.detail, (unsigned)utyBaseColor, (unsigned)utyStripeColor);
    }
    nTypes = tcPlayerTypes(types, schemes, counts, &kuunLan, &kuunLanScheme0);
    if (!kuunLanScheme0)
    {
        dbgMessagef("teamcolour: fail %s the Kuun-Lan (sMothership) does not use scheme 0", where);
    }
    for (type = FirstShipTypeOfRace[TC_PLAYER_RACE]; type <= LastShipTypeOfRace[TC_PLAYER_RACE]; type++)
    {
        ShipStaticInfo *info = GetShipStaticInfo(type, TC_PLAYER_RACE);
        meshdata *mesh;
        const char *name;

        if (info == NULL || !bitTest(info->common.infoFlags, IF_InfoLoaded) || !info->shipOrDerelict.teamColor[0] ||
            (mesh = tcMesh(info)) == NULL)
        {
            continue;
        }
        statics++;
        name = ShipTypeToStr(type);
        for (m = 0; m < mesh->nLocalMaterials + mesh->nPublicMaterials; m++)
        {
            materialentry *mat = &mesh->localMaterial[m];
            trhandle handle;
            texreg *reg;
            trcolorinfo *infos;
            sdword index, palette, want;
            const char *why = NULL;

            if (!mat->bTexturesRegistered || mat->texture == NULL || mat->texture == (trhandle *)TR_Invalid)
            {
                continue;
            }
            checked++;
            handle = ((trhandle *)mat->texture)[0];
            if (handle == TR_Invalid)
            {
                failed++;
                dbgMessagef("teamcolour: fail %s %s mat=%d no handle for scheme 0", where, name, (int)m);
                continue;
            }
            index = (sdword)trIndex(handle);
            palette = (sdword)trPaletteIndex(handle);
            reg = &trTextureRegistry[index];
            infos = tcInfos(index);
            want = (infos != NULL) ? trColorsEqual(&player, index) : -1;
            if (!bitTest(reg->flags, TRF_Allocated) || bitTest(reg->flags, TRF_Pending))
            {
                why = "not loaded";
            }
            else if (reg->nPalettes <= 0 || infos == NULL || palette >= reg->nPalettes)
            {
                why = "no palette";
            }
            else if (infos[palette].base != player.base || infos[palette].detail != player.detail)
            {
                why = "colour info is not the player's";
            }
            else if (palette != want)
            {
                why = "palette index is not the player's";
            }
            else if (bitTest(reg->flags, TRF_Paletted) && reg->sharedFrom == TR_NotShared &&
                     (catTrPaletteTeamFlags[index] & 0x80) &&
                     (udword)(catTrPaletteTeamFlags[index] & (TRF_TeamColor0 | TRF_TeamColor1)) !=
                     tcExpectedTeamFlags(index))
            {
                why = "palette built without its list's team colour";
            }
            if (why != NULL)
            {
                failed++;
                dbgMessagef("teamcolour: fail %s %s mat=%d idx=%d pal=%d want=%d nPal=%d flags=0x%02x built=0x%02x "
                            "expected=0x%02x info=%08x/%08x player=%08x/%08x %s: %s", where, name, (int)m, (int)index,
                            (int)palette, (int)want, (int)reg->nPalettes, (unsigned)reg->flags,
                            (unsigned)catTrPaletteTeamFlags[index], (unsigned)tcExpectedTeamFlags(index),
                            (infos != NULL && palette < reg->nPalettes) ? (unsigned)infos[palette].base : 0u,
                            (infos != NULL && palette < reg->nPalettes) ? (unsigned)infos[palette].detail : 0u,
                            (unsigned)player.base, (unsigned)player.detail,
                            (reg->fileName != NULL) ? reg->fileName : "-", why);
                continue;
            }
            if (bitTest(reg->flags, TRF_Paletted) && (catTrPaletteTeamFlags[index] & (TRF_TeamColor0 | TRF_TeamColor1)))
            {
                coloured++;
            }
            if (strcmp(name, "sMothership") == 0)
            {
                mothership++;
            }
            else if (strcmp(name, "sAcolyte") == 0)
            {
                acolyte++;
            }
            else if (strcmp(name, "sWorker") == 0)
            {
                worker++;
            }
            else if (strcmp(name, "sRecon") == 0)
            {
                recon++;
            }
        }
    }
    // the Kuun-Lan is in every level of the campaign: its textures must have been checked
    pass = schemeOk && kuunLanScheme0 && (failed == 0) && (mothership > 0);
    tcChecks++;
    if (!pass)
    {
        tcChecksFailed++;
    }
    dbgMessagef("teamcolour: check %s level=%s tick=%u segment=%u player=%08x/%08x statics=%d textures=%d failed=%d "
                "mothership=%d acolyte=%d worker=%d recon=%d colouredPalettes=%d kuunlan=%s ships=%d result=%s",
                where, tcLevelName(), (unsigned)universe.univUpdateCounter, (unsigned)tcSegment, (unsigned)player.base,
                (unsigned)player.detail, (int)statics, (int)checked, (int)failed, (int)mothership, (int)acolyte,
                (int)worker, (int)recon, (int)coloured,
                !kuunLan ? "absent" : kuunLanScheme0 ? "scheme0" : "FAIL", (int)nTypes, pass ? "PASS" : "FAIL");
    snprintf(ln, sizeof(ln), "%s%s:%s", (tcLevels[0] != '\0') ? "," : "", tcLevelName(), where);
    if (strlen(tcLevels) + strlen(ln) < sizeof(tcLevels))
    {
        strcat(tcLevels, ln);
    }
    return pass;
}

// the colour-info blocks live in the heap after a level's textures are loaded: the memory analysis trRegistryRefresh
//      writes at its end in a debug build (MEM_ANALYSIS: <settings>/mem.analysis, one line per allocated block); the
//      M3 soak's input (M2-R10: 2,438 "dupe trcolorinfo list" blocks at Mission09's load)
static void tcHeapLine(void)
{
#if MEM_ANALYSIS
    char line[256];
    FILE *f = fopen(filePathPrepend("mem.analysis", FF_UserSettingsPath), "rt");   // Memory.c MEM_ANALYSIS_FILE_NAME
    sdword dupe = 0, temp = 0, nopal = 0;

    if (f == NULL)
    {
        dbgMessagef("teamcolour: heap level=%s no memory analysis", tcLevelName());
        return;
    }
    while (fgets(line, sizeof(line), f) != NULL)
    {
        if (strstr(line, "allocated") == NULL)
        {
            continue;
        }
        if (strstr(line, "dupe trcolorinfo list") != NULL)
        {
            dupe++;
        }
        else if (strstr(line, "Temp trcolorinfo list") != NULL)
        {
            temp++;
        }
        else if (strstr(line, "TexturePalettePool") != NULL)
        {
            nopal++;
        }
    }
    fclose(f);
    dbgMessagef("teamcolour: heap level=%s dupe=%d temp=%d palettes=%d (allocated blocks after the level's textures loaded)",
                tcLevelName(), (int)dupe, (int)temp, (int)nopal);
#endif
}

/*-----------------------------------------------------------------------------
    Name        : catTeamColourLevelChange
    Description : HWI-TEST: singlePlayerLoadNewLevel's hook: stage 0 at its start (the old level's state), 1 after the
                  new level's statics are loaded and recoloured (universeStaticInit: meshRecolorize), before the
                  registry refresh, 2 after the new level's textures are loaded (trRegistryRefresh): the dump, and at
                  stage 2 the check
-----------------------------------------------------------------------------*/
void catTeamColourLevelChange(sdword stage)
{
    if (!catTeamColourTest || tcDone)
    {
        return;
    }
    catTeamColourDump((stage == 0) ? "before" : (stage == 1) ? "statics" : "after");
    if (stage == 2)
    {
        tcHeapLine();
        catTeamColourCheck("after");
        tcAfterChecks++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : catTeamColourFrameEnd
    Description : HWI-TEST: the render task's hook (next to catShotFrameEnd): the level segment and its "level" check
                  at the 64th tick of each level the game runs (a new game's first level, each level change, a loaded
                  save)
-----------------------------------------------------------------------------*/
void catTeamColourFrameEnd(void)
{
    udword tick = universe.univUpdateCounter;

    if (!catTeamColourTest || !gameIsRunning || tcDone)
    {
        return;
    }
    if (tick == tcLastTick)
    {
        return;
    }
    if (tcLastTick != 0xffffffff && tick < tcLastTick)
    {
        tcSegment++;                                        // a new level: its ticks count from 0 again
        tcSegmentTicks = 0;
        tcSegmentChecked = FALSE;
    }
    tcLastTick = tick;
    tcSegmentTicks++;
    if (!tcSegmentChecked && tcSegmentTicks >= 64)
    {
        tcSegmentChecked = TRUE;
        catTeamColourCheck("level");
    }
}

/*-----------------------------------------------------------------------------
    Name        : catTeamColourDone
    Description : HWI-TEST: the run's end: a last check and the done line (once). PASS: at least one check ran and
                  every check passed
-----------------------------------------------------------------------------*/
void catTeamColourDone(void)
{
    bool32 pass;

    if (!catTeamColourTest || tcDone)
    {
        return;
    }
    if (gameIsRunning)
    {
        catTeamColourCheck("end");
    }
    tcDone = TRUE;
    pass = (tcChecks > 0) && (tcChecksFailed == 0);
    dbgMessagef("teamcolour: done checks=%d failed=%d afterLoads=%d levels=%s result=%s", (int)tcChecks,
                (int)tcChecksFailed, (int)tcAfterChecks, (tcLevels[0] != '\0') ? tcLevels : "-",
                pass ? "PASS" : "FAIL");
}
