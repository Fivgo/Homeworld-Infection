/*=============================================================================
    Name    : SaveGame.c
    Purpose : Code for saving and loading games (saves entire contents of universe, etc.)

    Created 98/07/12 by gshaw
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "SaveGame.h"
#include "FOW.h"
#include "RepairDroid.h"
#include "SaveCat32.h"      // CAT (M2-R6 oracle-mode-a): reading cataclysm.exe's 0x10ad0027 saves
#include "Blast.h"
#include "Mimic.h"
#include "NIS.h"
#include "render.h"
#include "ShipNames.h"
#include "Task.h"
#include "Salvage.h"
#include "BeastWeapon.h"
#include "NetCheck.h"        // CAT (M2-R9 recorder-leftovers): recordingStartSave

#include <string.h>

#include "AIPlayer.h"
#include "Blobs.h"
#include "BTG.h"
#include "CameraCommand.h"
#include "Clouds.h"
#include "CommandDefs.h"
#include "CommandWrap.h"
#include "ConsMgr.h"
#include "Damage.h"
#include "File.h"
#include "Gun.h"
#include "InfoOverlay.h"
#include "LevelLoad.h"
#include "Light.h"
#include "LinkedList.h"
#include "Memory.h"
#include "MultiplayerGame.h"
#include "Objectives.h"
#include "Ping.h"
#include "Randy.h"
#include "Select.h"
#include "Sensors.h"
#include "SinglePlayer.h"
#include "SoundEvent.h"
#include "Star3d.h"
#include "StringSupport.h"
#include "Tactics.h"
#include "Teams.h"
#include "TradeMgr.h"
#include "Tutor.h"
#include "Universe.h"
#include "UnivUpdate.h"
#include "utility.h"
#include "KASFunc.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

void SaveConsMgr(void);
void LoadConsMgr(void);

GrowSelection SpaceObjRegistry;
GrowSelection BlobRegistry;

filehandle savefile = 0;

sdword savefilestatus        = 0;
sdword saveGameVersionNumber = 0;

// all the save game format versions this binary supports
sdword supportedVersionNumbers[] = {
    SAVE_VERSION_NUMBER_HWSDL_1,
    SAVE_VERSION_NUMBER_HWSDL_2,
};

/*=============================================================================
    Private Function prototypes
=============================================================================*/

void RegisterAllBlobs(void);
void RegisterAllSpaceObjs(void);
void LoadAllBlobs(void);
void SaveAllBlobs(void);
void FixAllBlobs(void);
void SaveBlob(blob *tblob);
blob *LoadBlob(void);
void FixBlob(blob *tblob);
void SaveAllSpaceObjs(void);
void LoadAllSpaceObjs(void);
void FixAllSpaceObjs(void);
SpaceObj *LoadSpaceObj(void);
void FixSpaceObj(SpaceObj *obj);
void SaveSpaceObj(SpaceObj *obj);
void LoadUniverse(void);
void SaveUniverse(void);
static void saveCat32SaveCaptainGameInfo(void);    // CAT (M2-R9 recorder-leftovers)

typedef void (*ListAddCB)(LinkedList *list,SpaceObj *obj);

void SaveLinkedListOfSpaceObjs(LinkedList *list);
void LoadLinkedListOfSpaceObjs(LinkedList *list,ListAddCB listAddCB);
void JustLoadLinkedListOfSpaceObjs(LinkedList *list);
void FixLinkedListOfSpaceObjs(LinkedList *list,ListAddCB listAddCB);

extern sdword actorFlagsEnabled;

/*=============================================================================
    Functions:
=============================================================================*/

void SpaceObjRegistryInit(void)
{
    Node* objnode = universe.ShipList.head;
    while (objnode != NULL)
    {
        SpaceObj* spaceobj = (SpaceObj*)listGetStructOfNode(objnode);
        dmgStopEffect((Ship*)spaceobj, DMG_All);
        objnode = objnode->next;
    }
    growSelectInit(&SpaceObjRegistry);
}

void SpaceObjRegistryClose(void)
{
    growSelectClose(&SpaceObjRegistry);
}

sdword SpaceObjRegistryObjPresent(SpaceObj *obj)
{
    sdword i;
    sdword numObjs = SpaceObjRegistry.selection->numShips;

    for (i=0;i<numObjs;i++)
    {
        if (obj == (SpaceObj *)SpaceObjRegistry.selection->ShipPtr[i])
        {
            return i;
        }
    }

    return -1;
}

void SpaceObjRegistryRegister(SpaceObj *obj)
{
    sdword index = SpaceObjRegistryObjPresent(obj);
    if (index == -1)        // object not present, so add
    {
        growSelectAddShip(&SpaceObjRegistry,(Ship *)obj);
    }
}

void SpaceObjRegistryRegisterNoCheck(SpaceObj *obj)
{
    growSelectAddShip(&SpaceObjRegistry,(Ship *)obj);
}

sdword SpaceObjRegistryGetID(SpaceObj *obj)
{
    if (obj != NULL)
    {
        return SpaceObjRegistryObjPresent(obj);
    }
    
    return -1;
}

SpaceObj *SpaceObjRegistryGetObj(sdword id)
{
    if (id == -1)
    {
        return NULL;
    }

    dbgAssertOrIgnore(id < SpaceObjRegistry.selection->numShips);

    return (SpaceObj *)SpaceObjRegistry.selection->ShipPtr[id];
}

Ship *SpaceObjRegistryGetShip(sdword id)
{
    if (id != -1)
    {
        Ship *ship;

        dbgAssertOrIgnore(id < SpaceObjRegistry.selection->numShips);
        ship = SpaceObjRegistry.selection->ShipPtr[id];
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
        return ship;
    }
    
    return NULL;
}

Resource *SpaceObjRegistryGetResource(sdword id)
{
    if (id != -1)
    {
        Resource *resource;

        dbgAssertOrIgnore(id < SpaceObjRegistry.selection->numShips);
        resource = (Resource *)SpaceObjRegistry.selection->ShipPtr[id];
        dbgAssertOrIgnore(resource->header.flags & SOF_Resource);
        return resource;
    }
    
    return NULL;
}

Bullet *SpaceObjRegistryGetBullet(sdword id)
{
    if (id != -1)
    {
        Bullet *bullet;

        dbgAssertOrIgnore(id < SpaceObjRegistry.selection->numShips);
        bullet = (Bullet *)SpaceObjRegistry.selection->ShipPtr[id];
        dbgAssertOrIgnore(bullet->header.objtype == OBJ_BulletType);
        return bullet;
    }
    
    return NULL;
}

TargetPtr SpaceObjRegistryGetTarget(sdword id)
{
    if (id != -1)
    {
        TargetPtr target;

        dbgAssertOrIgnore(id < SpaceObjRegistry.selection->numShips);
        target = (TargetPtr)SpaceObjRegistry.selection->ShipPtr[id];
        dbgAssertOrIgnore(target->header.flags & SOF_Targetable);
        return target;
    }
    
    return NULL;
}

void BlobRegistryInit(void)
{
    growSelectInit(&BlobRegistry);
}

void BlobRegistryClose(void)
{
    growSelectClose(&BlobRegistry);
}

sdword BlobRegistryBlobPresent(blob *tblob)
{
    sdword i;
    sdword numObjs = BlobRegistry.selection->numShips;

    for (i=0;i<numObjs;i++)
    {
        if (tblob == (blob *)BlobRegistry.selection->ShipPtr[i])
        {
            return i;
        }
    }

    return -1;
}

void BlobRegistryRegister(blob *tblob)
{
    dbgAssertOrIgnore(BlobRegistryBlobPresent(tblob) == -1);
    growSelectAddShip(&BlobRegistry,(Ship *)tblob);
}

sdword BlobRegistryGetIDWrapper(blob *tblob, bool32 check_valid_id)
{
    if (tblob != NULL)
    {
        sdword id = BlobRegistryBlobPresent(tblob);
        
        if (check_valid_id)
        {
            dbgAssertOrIgnore(id != -1);
        }
        
        return id;
    }
    
    return -1;
}

sdword BlobRegistryGetID(blob *tblob)
{
    return BlobRegistryGetIDWrapper(tblob, TRUE);
}

sdword BlobRegistryGetIDNoBlobOkay(blob *tblob)
{
    return BlobRegistryGetIDWrapper(tblob, FALSE);
}

blob *BlobRegistryGetBlob(sdword id)
{
    if (id != -1)
    {
        dbgAssertOrIgnore(id >= 0);
        dbgAssertOrIgnore(id < BlobRegistry.selection->numShips);
        return (blob *)BlobRegistry.selection->ShipPtr[id];
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
    Name        : SaveThisChunk
    Description : saves thischunk
    Inputs      : thischunk
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void SaveThisChunk(SaveChunk *thischunk)
{
    FILE *fp;
    dbgAssertOrIgnore(!fileUsingBigfile(savefile));
    fp = fileStream(savefile);

    if (fwrite(thischunk, sizeofSaveChunk(thischunk->contentsSize), 1, fp) != 1)
    {
        savefilestatus = 1;
    }
}

SaveChunk *CreateChunk(TypeOfSaveChunk type,sdword contentsSize,void *contents)
{
    SaveChunk *chunk = memAlloc(sizeofSaveChunk(contentsSize),"savechunk",Pyrophoric);

    chunk->type = type;
    chunk->contentsSize = contentsSize;

    dbgAssertOrIgnore(contentsSize >= 0);
    if ((contentsSize > 0) && (contents != NULL))
    {
        memcpy(chunkContents(chunk),contents,contentsSize);
    }

    return chunk;
}

SaveChunk *LoadNextChunk(void)
{
    SaveChunk readchunk;
    sdword num;
    SaveChunk *returnchunk;
    FILE *fp;

    dbgAssertOrIgnore(!fileUsingBigfile(savefile));
    fp = fileStream(savefile);

    num = fread(&readchunk,sizeof(SaveChunk),1,fp);
    dbgAssertOrIgnore(num != 0);

    dbgAssertOrIgnore(readchunk.contentsSize >= 0);

    returnchunk = memAlloc(sizeofSaveChunk(readchunk.contentsSize),"readchunk",Pyrophoric);

    memcpy(returnchunk,&readchunk,sizeof(SaveChunk));

    if (readchunk.contentsSize > 0)
    {
        num = fread(((ubyte *)returnchunk) + sizeof(SaveChunk),readchunk.contentsSize,1,fp);
        dbgAssertOrIgnore(num != 0);
    }

    return returnchunk;
}

SaveChunk *LoadNextChunkSafe(void)
{
    SaveChunk readchunk;
    sdword num;
    SaveChunk *returnchunk;
    FILE *fp;

    dbgAssertOrIgnore(!fileUsingBigfile(savefile));
    fp = fileStream(savefile);

    num = fread(&readchunk,sizeof(SaveChunk),1,fp);
    if (num == 0)
    {
        return NULL;
    }

    if (readchunk.contentsSize < 0)
    {
        return NULL;
    }

    returnchunk = memAlloc(sizeofSaveChunk(readchunk.contentsSize),"readchunk",Pyrophoric);

    memcpy(returnchunk,&readchunk,sizeof(SaveChunk));

    if (readchunk.contentsSize > 0)
    {
        num = fread(((ubyte *)returnchunk) + sizeof(SaveChunk),readchunk.contentsSize,1,fp);
        if (num == 0)
        {
            memFree(returnchunk);
            return NULL;
        }
    }

    return returnchunk;
}

void SaveInfoNumber(sdword info)
{
    InfoChunkContents infocontents;
    SaveChunk *chunk;

    infocontents.info = info;

    SaveThisChunk(chunk = CreateChunk(INFO_CHUNK,sizeof(InfoChunkContents),&infocontents));
    memFree(chunk);
}

sdword LoadInfoNumber(void)
{
    SaveChunk *chunk;
    sdword num;

    chunk = LoadNextChunk();
    VerifyChunk(chunk,INFO_CHUNK,sizeof(InfoChunkContents));

    num = ((InfoChunkContents *)chunkContents(chunk))->info;
    memFree(chunk);

    return num;
}

bool32 LoadInfoNumberOptional(sdword *info)
{
    SaveChunk *chunk;

    chunk = LoadNextChunkSafe();
    if (chunk == NULL)
    {
        return FALSE;
    }

    VerifyChunk(chunk,INFO_CHUNK,sizeof(InfoChunkContents));

    *info = ((InfoChunkContents *)chunkContents(chunk))->info;
    memFree(chunk);

    return TRUE;
}

void SaveMissionNumber(void)
{
    SaveInfoNumber(spGetCurrentMission());
}

void LoadMissionNumber(void)
{
    MissionEnum saveGameMissionEnum = MISSION_ENUM_NOT_INITIALISED;
    sdword saveGameMissionReference = LoadInfoNumber();
    
    switch (saveGameVersionNumber)
    {
        case SAVE_VERSION_NUMBER_ORIGINAL:
        case SAVE_VERSION_NUMBER_HWSDL_1:
            // these used a straight 1-16 absolute mission value,
            // so we need to convert these to a MissionEnum
            switch (saveGameMissionReference)
            {
                case  1:  saveGameMissionEnum = MISSION_1_KHARAK_SYSTEM;               break;
                case  2:  saveGameMissionEnum = MISSION_2_OUTSKIRTS_OF_KHARAK_SYSTEM;  break;
                case  3:  saveGameMissionEnum = MISSION_3_RETURN_TO_KHARAK;            break;
                case  4:  saveGameMissionEnum = MISSION_4_GREAT_WASTELANDS_TRADERS;    break;
                case  5:  saveGameMissionEnum =
#ifdef HW_GAME_RAIDER_RETREAT
                                                MISSION_5B_TURANIC_RAIDER_PLANETOID;
#else
                                                MISSION_5_GREAT_WASTELANDS_REVENGE;
#endif
                          break;
                case  6:  saveGameMissionEnum = MISSION_6_DIAMOND_SHOALS;              break;
                case  7:  saveGameMissionEnum = MISSION_7_THE_GARDENS_OF_KADESH;       break;
                case  8:  saveGameMissionEnum = MISSION_8_THE_CATHEDRAL_OF_KADESH;     break;
                case  9:  saveGameMissionEnum = MISSION_9_SEA_OF_LOST_SOULS;           break;
                case 10:  saveGameMissionEnum = MISSION_10_SUPER_NOVA_STATION;         break;
                case 11:  saveGameMissionEnum = MISSION_11_TENHAUSER_GATE;             break;
                case 12:  saveGameMissionEnum = MISSION_12_GALACTIC_CORE;              break;
                case 13:  saveGameMissionEnum = MISSION_13_THE_KAROS_GRAVEYARD;        break;
                case 14:  saveGameMissionEnum = MISSION_14_BRIDGE_OF_SIGHS;            break;
                case 15:  saveGameMissionEnum = MISSION_15_CHAPEL_PERILOUS;            break;
                case 16:  saveGameMissionEnum = MISSION_16_HIIGARA;                    break;
                
                default:  break;
            }
            
            break;
        
        case SAVE_VERSION_NUMBER_HWSDL_2:
            // use mission value as-is; it's a MissionEnum
            saveGameMissionEnum = (MissionEnum) saveGameMissionReference;
            break;
        
        default:
            break;
    }

    if (saveGameMissionEnum == MISSION_ENUM_NOT_INITIALISED)
    {
        dbgFatalf(DBG_Loc,
            "Failed to interpret save game (version 0x%x): MissionEnum",
            saveGameVersionNumber);
    }

    spSetCurrentMission(saveGameMissionEnum);
}

void SaveBackground(void)
{
    real32 theta = btgGetTheta();
    real32 phi = btgGetPhi();

    SaveInfoNumber(Real32ToSdword(theta));
    SaveInfoNumber(Real32ToSdword(phi));
    Save_String(btgLastBackground);
}

void LoadBackground(void)
{
    sdword datatheta = LoadInfoNumber();
    sdword dataphi = LoadInfoNumber();

    btgSetTheta(SdwordToReal32(datatheta));
    btgSetPhi(SdwordToReal32(dataphi));
    Load_StringToAddress(btgLastBackground);

    btgLoadTextures();
}

void SaveLighting(void)
{
    Save_String(lightCurrentLighting);
}

void LoadLighting(void)
{
    Load_StringToAddress(lightCurrentLighting);

    if (lightCurrentLighting[0])
    {
        char tmp_path[PATH_MAX];
        strcpy(tmp_path, lightCurrentLighting);
        lightParseHSF(tmp_path);
    }
    else
    {
        lightDefaultLightSet();
    }
}

void SaveVersionInfo(void)
{
    // CAT (M2-R9 recorder-leftovers): an exe-format save carries cataclysm.exe's number (0x10ad0027)
    sdword version = saveCat32Saving ? SAVE_VERSION_NUMBER_CATACLYSM : SAVE_VERSION_NUMBER;
    FILE *fp;

    dbgAssertOrIgnore(!fileUsingBigfile(savefile));
    fp = fileStream(savefile);

    if (fwrite(&version, sizeof(sdword), 1, fp) != 1)
    {
        savefilestatus = 1;
    }
}

sdword LoadVersionInfo(void)
{
    FILE *fp;
    udword i;

    dbgAssertOrIgnore(!fileUsingBigfile(savefile));
    fp = fileStream(savefile);

    if (fread(&saveGameVersionNumber, sizeof(sdword), 1, fp) == 0)
    {
        return VERIFYSAVEFILE_ERROROPENING;
    }

    for (i = 0; i < sizeof(supportedVersionNumbers)/sizeof(sdword); ++i)
    {
        if (saveGameVersionNumber == supportedVersionNumbers[i])
        {
            return VERIFYSAVEFILE_OK;
        }
    }

    if (saveGameVersionNumber == SAVE_VERSION_NUMBER_CATACLYSM)
    {
        return saveCatLoadHook();                   // CAT: a GOG Cataclysm save (PreLoadGame 0x004fdfc0 accepts only this)
    }

    return VERIFYSAVEFILE_BADVERSION;
}

/*-----------------------------------------------------------------------------
    Name        : saveCatChunkSize32
    Description : CAT: the contents size the exe writes for a chunk type whose size is fixed (docs/formats/savegame.md;
                  SaveGame.h SAVE_CAT32_*), or -1 for the types whose size varies (BASIC_STRUCTURE sub-type 0 holds
                  several structs: player names 0x90, tpGameCreated 0x2B0, the 0x9C state block, RNG streams 0x14 ...)
    Inputs      : type - chunk type (flags | sub-type)
    Outputs     :
    Return      : bytes, or -1
----------------------------------------------------------------------------*/
sdword saveCatChunkSize32(TypeOfSaveChunk type)
{
    switch (type)
    {
        case INFO_CHUNK:                                        return (sdword)sizeof(InfoChunkContents);
        case BASIC_STRUCTURE|SAVE_UNIVERSE:                     return SAVE_CAT32_UNIVERSE;
        case BASIC_STRUCTURE|SAVE_BLOB:                         return SAVE_CAT32_BLOB;
        case BASIC_STRUCTURE|SAVE_SHIPSINGLEPLAYERGAMEINFO:     return SAVE_CAT32_SHIPSPGAMEINFO;   // CAT: the ship's 0x48 (M2-R6)
        case BASIC_STRUCTURE|SAVE_CAT_PREGAMETABLE:             return SAVE_CAT32_PREGAMETABLE;
        case BASIC_STRUCTURE|SAVE_CAT_MISC9:                    return SAVE_CAT32_MISC9;
        case BASIC_STRUCTURE|SAVE_CAT_MISCA:                    return SAVE_CAT32_MISCA;
        case BASIC_STRUCTURE|SAVE_CAT_MISCB:                    return SAVE_CAT32_MISCB;
        default:                                                return -1;
    }
}

/*-----------------------------------------------------------------------------
    Name        : saveCatLoadHook
    Description : CAT (M2-R6 oracle-mode-a): a GOG Cataclysm save (version 0x10ad0027; PreLoadGame 0x004fdfc0 and
                  VerifySaveFile 0x004fdf40 accept only this number). The container is walked first: every chunk
                  header to EOF, the fixed-size chunk types against saveCatChunkSize32 and the ship chunks
                  (VARIABLE_STRUCTURE | SAVE_SPACEOBJ | OBJ_ShipType) against the 0x63C minimum. A sound container is
                  accepted: PreLoadGame / LoadGame then read it in the exe's order, converting each ILP32 struct
                  chunk field by field (saveCat32Loading, SaveCat32.c). The file is left positioned after the version.
    Inputs      : savefile positioned just after the version
    Outputs     : one log line
    Return      : VERIFYSAVEFILE_OK, or VERIFYSAVEFILE_BADVERSION for a broken container
----------------------------------------------------------------------------*/
sdword saveCatLoadHook(void)
{
    FILE *fp = fileStream(savefile);
    SaveChunk header;
    sdword chunks = 0, checked = 0, wrong = 0, ships = 0, expect;
    long start = ftell(fp);

    while (fread(&header, sizeof(SaveChunk), 1, fp) == 1)
    {
        if (header.contentsSize < 0)
        {
            wrong++;
            break;
        }
        chunks++;
        expect = saveCatChunkSize32(header.type);
        if (expect >= 0)
        {
            checked++;
            if (header.contentsSize != expect)
            {
                wrong++;
            }
        }
        else if (header.type == (VARIABLE_STRUCTURE|SAVE_SPACEOBJ|OBJ_ShipType))
        {
            ships++;
            if (header.contentsSize < SAVE_CAT32_SHIP - 4)
            {
                wrong++;
            }
        }
        if (fseek(fp, header.contentsSize, SEEK_CUR) != 0)
        {
            wrong++;
            break;
        }
    }
    fseek(fp, start, SEEK_SET);
    dbgMessagef("Cataclysm save (version 0x%x): %d chunks, %d fixed-size and %d ship chunks checked, %d wrong",
                SAVE_VERSION_NUMBER_CATACLYSM, chunks, checked, ships, wrong);
    return (wrong == 0 && chunks > 0) ? VERIFYSAVEFILE_OK : VERIFYSAVEFILE_BADVERSION;
}

void SavePreGameInfo(void)
{
    sdword i;

    SaveInfoNumber(singlePlayerGame);
    SaveInfoNumber(musicEventCurrentTrack());
    SaveInfoNumber(universe.numPlayers);
    SaveStructureOfSize(&playerNames[0][0],MAX_MULTIPLAYER_PLAYERS*MAX_PERSONAL_NAME_LEN);
    for (i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
    {
        SaveInfoNumber(universe.players[i].race);
        SaveInfoNumber(teColorSchemes[i].textureColor.base);
        SaveInfoNumber(teColorSchemes[i].textureColor.detail);
    }

    if (singlePlayerGame)
    {
        SaveMissionNumber();
        // CAT: the difficulty's health factor follows the mission number (SaveGame 0x004fdaf0: 0x00894194)
        SaveInfoNumber(Real32ToSdword(spDifficultyHealthModifier));
    }
    else
    {
        if (saveCat32Saving)
        {
            saveCat32SaveCaptainGameInfo();     // CAT (M2-R9 recorder-leftovers): the exe's 0x2B0 bytes
        }
        else
        {
            SaveStructureOfSize(&tpGameCreated,sizeof(tpGameCreated));
        }
    }

    if (tutorial != TUTORIAL_ONLY)
    {
        universeSaveEverythingNeeded();
    }
}

/*-----------------------------------------------------------------------------
    Name        : saveCat32LoadCaptainGameInfo
    Description : CAT (M2-R6 oracle-mode-a): the exe's tpGameCreated chunk (0x2B0 bytes; PreLoadGame 0x004fdfc0
                  copies 0xac dwords to 0x00886028). Its first 0xC8 bytes are HW1's CaptainGameInfo up to numPlayers
                  (Name and Password as Windows' 2-byte wchar_t, the resource settings as 4-byte longs), converted
                  field by field; Cataclysm inserted 0x48 bytes there (the exe reads +0xC8 .. +0xF8 in lgStartGame,
                  mgCreateGameNow, titanRequestReceivedCB, gameStart; meaning not established), so the rest is not
                  HW1's playerInfo[] and is left zero here, with a log line when it is not zero in the file.
                  CAT-TODO(stage 5 multiplayer): the exe's 0x2B0 layout after +0xC8 (the 0x48 new bytes and playerInfo), for LAN interop and for a recorded multiplayer game
----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------
    Name        : saveCat32SaveCaptainGameInfo
    Description : CAT (M2-R9 recorder-leftovers): the inverse of saveCat32LoadCaptainGameInfo for an exe-format save: the
                  first 0xC8 bytes are tpGameCreated converted back into the ILP32 layout, the rest of the exe's 0x2B0
                  bytes (Cataclysm's 0x48 bytes and its playerInfo, layout not established) are 0, as in the GOG
                  skirmish recordings' saves (oracle01 of the 10-17 session: every byte after +0xC8 is 0)
----------------------------------------------------------------------------*/
static void saveCat32SaveCaptainGameInfo(void)
{
    ubyte block[SAVE_CAT32_CAPTAINGAMEINFO];
    ubyte head[0x268];

    memset(block, 0, sizeof(block));
    memset(head, 0, sizeof(head));
#if SAVECAT32_LP64
    saveCat32ConvertBack(&saveCat32Layout_CaptainGameInfo, &tpGameCreated, head);
#else
    memcpy(head, &tpGameCreated, sizeof(head));
#endif
    memcpy(block, head, 0xC8);
    SaveStructureOfSize(block, SAVE_CAT32_CAPTAINGAMEINFO);
}

static void saveCat32LoadCaptainGameInfo(void)
{
    SaveChunk *chunk = LoadNextChunk();
    ubyte head[0x268];      // the tree's ILP32 CaptainGameInfo: the first 0xC8 bytes from the file, playerInfo zero
    const ubyte *src = (const ubyte *)chunkContents(chunk);
    sdword i, nonzero = 0;

    VerifyChunk(chunk,BASIC_STRUCTURE,SAVE_CAT32_CAPTAINGAMEINFO);
    memset(head, 0, sizeof(head));
    memcpy(head, src, 0xC8);
    for (i = 0xC8; i < chunk->contentsSize; i++)
    {
        nonzero += (src[i] != 0);
    }
    if (nonzero != 0)
    {
        dbgMessagef("Cataclysm save: tpGameCreated has %d non-zero bytes after +0xC8 (not converted)", nonzero);
    }
    memset(&tpGameCreated, 0, sizeof(tpGameCreated));
#if SAVECAT32_LP64
    saveCat32Convert(&saveCat32Layout_CaptainGameInfo, head, &tpGameCreated);
#else
    memcpy(&tpGameCreated, head, sizeof(head));
#endif
    memFree(chunk);
}

void LoadPreGameInfo(void)
{
    sdword i;
    sdword sp;

    sp = LoadInfoNumber();
    if (sp)
    {
        dbgAssertOrIgnore(singlePlayerGame);
    }
    else
    {
        dbgAssertOrIgnore(!singlePlayerGame);
    }

    SongNumber = LoadInfoNumber();
    numPlayers = (uword)LoadInfoNumber();
    LoadStructureOfSizeToAddress(&playerNames[0][0],MAX_MULTIPLAYER_PLAYERS*MAX_PERSONAL_NAME_LEN);
    for (i=0;i<MAX_MULTIPLAYER_PLAYERS;i++)
    {
        // CAT: the race is read as 0 or 1 (PreLoadGame 0x004fdfc0: neg / sbb / neg of the number)
        universe.players[i].race = (LoadInfoNumber() != 0) ? 1 : 0;
        teColorSchemes[i].textureColor.base = LoadInfoNumber();
        teColorSchemes[i].textureColor.detail = LoadInfoNumber();
    }

    if (sp)
    {
        if (saveCat32Loading)
        {
            // CAT: the exe's mission number is Cataclysm's 1 .. 17 (PreLoadGame 0x004fdfc0 stores it in 0x00c0a5c0 as it
            //      is), which this tree's MissionEnum numbers alike (MISSION_1_KHARAK_SYSTEM 1 .. MISSION_17)
            spSetCurrentMission((MissionEnum)LoadInfoNumber());
        }
        else
        {
            LoadMissionNumber();
        }
        // CAT: then the difficulty's health factor (PreLoadGame 0x004fdfc0 -> 0x00894194)
        spDifficultyHealthModifier = SdwordToReal32(LoadInfoNumber());
    }
    else
    {
        if (saveCat32Loading)
        {
            saveCat32LoadCaptainGameInfo();
        }
        else
        {
            LoadStructureOfSizeToAddress(&tpGameCreated,sizeof(tpGameCreated));
        }
    }

    if (tutorial != TUTORIAL_ONLY)
    {
        universeLoadEverythingNeeded();
    }

    // CAT: no harvesting switch: PreLoadGame 0x004fdfc0 ends here (HW1 disabled the resource collectors of a
    //      multiplayer game without harvesting)
}

/*-----------------------------------------------------------------------------
    Name        : SaveGame
    Description :
    Inputs      :
    Outputs     :
    Return      : TRUE on success
----------------------------------------------------------------------------*/
bool32 SaveGame(char *filename)
{
    sdword i;
    Node *node;

    savefile = fileOpen(filename, FF_WriteMode | FF_ReturnNULLOnFail | FF_UserSettingsPath);
    if (savefile == (filehandle)NULL)
    {
        return FALSE;
    }
    savefilestatus = 0;

    // CAT (M2-R9 recorder-leftovers): a recording's save is written in cataclysm.exe's format (version 0x10ad0027, the
    //      exe's ILP32 struct layouts, SaveCat32.c), so the GOG game can play an hwi-engine recording back; this tree's
    //      other saves keep its native format
    if (recordingStartSave)
    {
        saveCat32SaveBegin();
    }

    SaveVersionInfo();
    SavePreGameInfo();

    // CAT: SaveGame 0x004fdaf0 stops every ship's damage effects before the registries are made
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        dmgStopEffect((Ship *)listGetStructOfNode(node), 3);
    }

    SpaceObjRegistryInit();
    BlobRegistryInit();

    cat_saveStateBlock();                       // CAT: the renderer's 0x9C-byte state block (0x00c32c00)

    RegisterAllSpaceObjs();
    RegisterAllBlobs();
    SaveAllSpaceObjs();
    SaveAllBlobs();
    cat_saveGamePreGameTable();                 // CAT: the task clocks and the song (0x004fda00)
    SaveUniverse();
    SaveIDToPtrTable(&ShipIDToPtr);
    SaveIDToPtrTable(&ResourceIDToPtr);
    SaveIDToPtrTable(&DerelictIDToPtr);
    SaveIDToPtrTable(&MissileIDToPtr);
    nebSave_Nebula();
    SaveConsMgr();
    aiplayerSave();

    SaveSinglePlayerGame();
    cat_saveGameMiscChunks();                   // CAT: the mimic disguise tables (0x005ba170, Mimic.c)
    // CAT (M2-R5 hud): the overlay's mission timer (cat_missionTimerSave 0x004acbb0, after cat_saveGameMiscChunks)
    cat_missionTimerSave();

    if (singlePlayerGame)
    {
        kasSave();
        objectiveSave();
        tmSave();
        pingSave();
        Save_String(CurrentLevelName);
    }

    if(tutorial == TUTORIAL_ONLY)
    {
        tutSaveTutorialGame();
    }

    SaveMaxSelection(&selSelected);
    for (i=0;i<SEL_NumberHotKeyGroups;i++)
    {
        SaveMaxSelection(&selHotKeyGroup[i]);
    }

    SaveBackground();
    SaveLighting();

    smSave();
    ranSave();

    SaveInfoNumber(actorFlagsEnabled);
    SaveInfoNumber(smGhostMode);

    SaveConsMgrDetermOptional();                // added for V1.04 patch

    fileClose(savefile);
    savefile = 0;
    saveCat32SaveEnd(filename);                 // CAT (M2-R9 recorder-leftovers): the writer's log line, if it ran

#ifdef __EMSCRIPTEN__
    EM_ASM(
        FS.syncfs(function (err) {
            if (err) console.log("FS.syncfs error when saving game: " + err);
        });
    );
#endif


    if (savefilestatus)
    {
        fileDelete(filename);
        savefilestatus = 0;
        return FALSE;
    }
    
    return TRUE;        // save successful
}

/*-----------------------------------------------------------------------------
    Name        : VerifySaveFile
    Description : verifies filename is valid save game file
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
sdword VerifySaveFile(char *filename)
{
    sdword verify;

    savefile = fileOpen(filename, FF_ReturnNULLOnFail | FF_UserSettingsPath);

    if (savefile == 0)
    {
        return VERIFYSAVEFILE_ERROROPENING;
    }

    verify = LoadVersionInfo();
    fileClose(savefile);
    savefile = 0;

    return verify;
}

/*-----------------------------------------------------------------------------
    Name        : PreLoadGame
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void PreLoadGame(char *filename)
{
    sdword verify;

    savefile = fileOpen(filename, FF_UserSettingsPath);
    verify = LoadVersionInfo();
    if (verify != VERIFYSAVEFILE_OK)
    {
        dbgFatalf(DBG_Loc,"Error %d loading game %s",verify,filename);
        return;
    }
    if (saveGameVersionNumber == SAVE_VERSION_NUMBER_CATACLYSM)
    {
        saveCat32Begin();                       // CAT (M2-R6 oracle-mode-a): the exe's ILP32 structs follow
    }
    LoadPreGameInfo();
}

/*-----------------------------------------------------------------------------
    Name        : LoadGame
    Description :
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void LoadGame(char *filename)
{
    sdword i;
    Node *node;
    ShipType shiptype;
    ShipStaticInfo *shipstatic;

    dbgAssertOrIgnore(savefile);

    // CAT: LoadGame 0x004fe0e0 first stops the damage effects of the ships still in the universe
    for (node = universe.ShipList.head; node != NULL; node = node->next)
    {
        dmgStopEffect((Ship *)listGetStructOfNode(node), 3);
    }

    SpaceObjRegistryInit();
    BlobRegistryInit();

    cat_loadStateBlock();                       // CAT: the renderer's state block (0x00c32c00, 39 dwords)
    cat_rndFogLayerTextureLoad();               // CAT: LoadGame 0x004fe0e0 (render.c)

    LoadAllSpaceObjs();
    LoadAllBlobs();
    cat_loadGamePreGameTable();                 // CAT: the task clocks and the song (the 0x00892660 table)
    LoadUniverse();
    LoadIDToPtrTable(&ShipIDToPtr);
    LoadIDToPtrTable(&ResourceIDToPtr);
    LoadIDToPtrTable(&DerelictIDToPtr);
    LoadIDToPtrTable(&MissileIDToPtr);
    nebLoad_Nebula();
    LoadConsMgr();
    aiplayerLoad();

    LoadSinglePlayerGame();
    cat_loadGameMiscChunks();                   // CAT: the mimic disguise tables (0x005ba1e0, Mimic.c)
    // CAT (M2-R5 hud): the overlay's mission timer (cat_missionTimerLoad 0x004acbc0, after cat_loadGameMiscChunks)
    cat_missionTimerLoad();

    if (singlePlayerGame)
    {
        kasLoad();
        objectiveLoad();
        tmLoad();
        pingLoad();
        Load_StringToAddress(&CurrentLevelName[0]);
    }

    if (tutorial == TUTORIAL_ONLY)
    {
        tutLoadTutorialGame();
    }
    
    LoadMaxSelectionAndFix(&selSelected);
    for (i=0;i<SEL_NumberHotKeyGroups;i++)
    {
        LoadMaxSelectionAndFix(&selHotKeyGroup[i]);
    }

    LoadBackground();
    LoadLighting();

    smLoad();
    ranLoad();

    actorFlagsEnabled = LoadInfoNumber();
    smGhostMode = LoadInfoNumber();

    LoadConsMgrDetermOptional();            // optional, won't crash if not there, added for V1.04 patch

    FixAllSpaceObjs();
    FixAllBlobs();
    cat_loadShipRecordTableFix();               // CAT: the ship names' ships (0x0050f8e0, ShipNames.c)

    listInit(&universe.effectList);

    fileClose(savefile);
    savefile = 0;

    // CAT: a single-player load ends with the script's auto save (LoadGame 0x004fe3aa: singlePlayerGame 0x00a43c9c ->
    //      cat_kasAutoSaveOnLoad 0x00497230), before the registries close
    if (singlePlayerGame)
    {
        cat_kasAutoSaveOnLoad(filename);
    }
    // CAT: then every loaded Somtaaw ship type gets its maximum health again (LoadGame 0x004fe0e0: infoFlags
    //      IF_InfoLoaded at static +0x30, cat_univSetShipMaxHealth, FirstShipTypeOfRace[0] .. LastShipTypeOfRace[0])
    for (shiptype = FirstShipTypeOfRace[RACE_Sect]; shiptype <= LastShipTypeOfRace[RACE_Sect]; shiptype++)
    {
        shipstatic = GetShipStaticInfo(shiptype, RACE_Sect);
        if (bitTest(shipstatic->common.infoFlags, IF_InfoLoaded))
        {
            cat_univSetShipMaxHealth(shipstatic);
        }
    }

    saveCat32End(filename);
}

/*-----------------------------------------------------------------------------
    Name        : cat_saveStateBlock / cat_loadStateBlock
    Description : CAT (M2-R6 oracle-mode-a): the BASIC chunk of 0x9C bytes right after the registries (SaveGame
                  0x004fdaf0 / LoadGame 0x004fe0e0): 39 dwords from 0x00c32c00, the renderer's per-object state
                  (the object being drawn and its effect words, cleared first: +0x00, +0x84, +0x88) and the fog layer
                  (+0x14 .. +0x4c, render.c cat_rndStateBlockGet / Set). Plain words in both layouts.
----------------------------------------------------------------------------*/
void cat_saveStateBlock(void)
{
    udword block[SAVE_CAT32_STATEBLOCK / sizeof(udword)];

    memset(block, 0, sizeof(block));
    cat_rndStateBlockGet(block);
    SaveStructureOfSize(block, SAVE_CAT32_STATEBLOCK);
}

void cat_loadStateBlock(void)
{
    udword block[SAVE_CAT32_STATEBLOCK / sizeof(udword)];

    LoadStructureOfSizeToAddress(block, SAVE_CAT32_STATEBLOCK);
    cat_rndStateBlockSet(block);
}

/*-----------------------------------------------------------------------------
    Name        : cat_saveGamePreGameTable / cat_loadGamePreGameTable
    Description : CAT (M2-R6 oracle-mode-a): the BASIC | 0x4000 chunk after the blobs (cat_saveGamePreGameTable
                  0x004fda00; LoadGame 0x004fe0e0 scatters it back): the {address, size} table at 0x00892660, three
                  dwords: taskTimeElapsed (0x00a6b204), cat_taskRealTimeElapsed (0x00a6b20c) and SongNumber
                  (0x00883960). A load restores the task clocks the game had when it was saved.
----------------------------------------------------------------------------*/
void cat_saveGamePreGameTable(void)
{
    udword table[SAVE_CAT32_PREGAMETABLE / sizeof(udword)];
    SaveChunk *chunk;

    table[0] = Real32ToSdword(taskTimeElapsed);
    table[1] = Real32ToSdword(cat_taskRealTimeElapsed);
    table[2] = (udword)SongNumber;
    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_CAT_PREGAMETABLE, SAVE_CAT32_PREGAMETABLE, table);
    SaveThisChunk(chunk);
    memFree(chunk);
}

void cat_loadGamePreGameTable(void)
{
    SaveChunk *chunk = LoadNextChunk();
    udword table[SAVE_CAT32_PREGAMETABLE / sizeof(udword)];

    VerifyChunk(chunk, BASIC_STRUCTURE|SAVE_CAT_PREGAMETABLE, SAVE_CAT32_PREGAMETABLE);
    memcpy(table, chunkContents(chunk), sizeof(table));
    memFree(chunk);
    taskTimeElapsed = SdwordToReal32((sdword)table[0]);
    cat_taskRealTimeElapsed = SdwordToReal32((sdword)table[1]);
    SongNumber = (sdword)table[2];
}

/*=============================================================================
    Actual Save routines for various key data structures:
=============================================================================*/

void RegisterAllBlobs(void)
{
    Node *blobnode = universe.collBlobList.head;
    blob *thisBlob;

    // for each blob, check all collisions within that blob
    while (blobnode != NULL)
    {
        thisBlob = (blob *)listGetStructOfNode(blobnode);

        BlobRegistryRegister(thisBlob);

        blobnode = blobnode->next;
    }
}

// CAT (M2-R9 recorder-leftovers): RegisterAllSpaceObjsInsideMe 0x004fe400 also registers the ships inside each ship it
//      registers (a recursion through Ship+0x4B4 shipsInsideMe); HW1 registered one level
void RegisterAllSpaceObjsInsideMe(ShipsInsideMe *shipsInsideMe)
{
    Node *insidenode = shipsInsideMe->insideList.head;
    InsideShip *insideship;

    while (insidenode != NULL)
    {
        insideship = (InsideShip *)listGetStructOfNode(insidenode);
        dbgAssertOrIgnore(insideship->ship->header.objtype == OBJ_ShipType);
        if ((insideship->ship->header.flags & SOF_Dead) == 0)
        {
            SpaceObjRegistryRegister((SpaceObj *)insideship->ship);
            if (insideship->ship->shipsInsideMe != NULL)
            {
                RegisterAllSpaceObjsInsideMe(insideship->ship->shipsInsideMe);
            }
        }

        insidenode = insidenode->next;
    }
}

// CAT (M2-R9 recorder-leftovers): RegisterAllSpaceObjs 0x004fe470 as the exe: the dead ships of the object list get
//      univRemoveShipReferences, a mimic's fake asteroid is registered, the ships inside are registered recursively
void RegisterAllSpaceObjs(void)
{
    Node *node = universe.SpaceObjList.head;
    SpaceObj *obj;

    while (node != NULL)
    {
        obj = (SpaceObj *)listGetStructOfNode(node);

        if (obj->header.objtype == OBJ_EffectType)
        {
            goto nextnode;
        }
        if (obj->header.flags & SOF_Dead)
        {
            // CAT: a dead ship still in the list has its references removed (RegisterAllSpaceObjs 0x004fe470:
            //      univRemoveShipReferences 0x0054b230)
            if (obj->header.objtype == OBJ_ShipType)
            {
                univRemoveShipReferences((Ship *)obj);
            }
            goto nextnode;
        }

        SpaceObjRegistryRegister(obj);
        if (obj->header.objtype == OBJ_ShipType)
        {
            if (((Ship *)obj)->shipsInsideMe != NULL)
            {
                // check ships inside too
                RegisterAllSpaceObjsInsideMe(((Ship *)obj)->shipsInsideMe);
            }
            // CAT: and a mimic's fake asteroid, which is in no list the save walks (0x004fe567: the +0x18 mimic
            //      bit, cat_mimicGetFakeResource 0x005b9960)
            if (bitTest(obj->header.catFlags18, CAT_SHIPFLAG_Mimic) && (cat_mimicGetFakeResource((Ship *)obj) != NULL))
            {
                SpaceObjRegistryRegister((SpaceObj *)cat_mimicGetFakeResource((Ship *)obj));
            }
        }

nextnode:
        node = node->next;
    }

    node = universe.MinorSpaceObjList.head;
    while (node != NULL)
    {
        obj = (SpaceObj *)listGetStructOfNode(node);

        if ((obj->header.flags & SOF_Dead) == 0)
        {
            SpaceObjRegistryRegister(obj);
        }

        node = node->next;
    }

    // ALWAYS saved now because we can have hyperspace ships in multiplayergame too
    {
        Ship *ship;
        InsideShip *insideship;
        node = singlePlayerGameInfo.ShipsInHyperspace.head;
        while (node != NULL)
        {
            insideship = (InsideShip *)listGetStructOfNode(node);
            ship = insideship->ship;
            dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
            dbgAssertOrIgnore((ship->header.flags & SOF_Dead) == 0);

            SpaceObjRegistryRegister((SpaceObj *)ship);
            if (ship->shipsInsideMe != NULL)
            {
                // check ships inside too
                RegisterAllSpaceObjsInsideMe(ship->shipsInsideMe);
            }

            node = node->next;
        }
    }
}

void SaveAllBlobs(void)
{
    sdword numBlobs = BlobRegistry.selection->numShips;
    sdword i;
    InfoChunkContents infocontents;
    SaveChunk *chunk;

    infocontents.info = numBlobs;

    SaveThisChunk(chunk = CreateChunk(INFO_CHUNK,sizeof(InfoChunkContents),&infocontents));

    for (i=0;i<numBlobs;i++)
    {
        SaveBlob((blob *)BlobRegistry.selection->ShipPtr[i]);
    }

    memFree(chunk);
}

void LoadAllBlobs(void)
{
    SaveChunk *infochunk;
    sdword numBlobs;
    blob *tblob;
    sdword i;

    infochunk = LoadNextChunk();
    VerifyChunk(infochunk,INFO_CHUNK,sizeof(InfoChunkContents));

    numBlobs = ((InfoChunkContents *)chunkContents(infochunk))->info;

    memFree(infochunk);

    for (i=0;i<numBlobs;i++)
    {
        tblob = LoadBlob();
        BlobRegistryRegister(tblob);
    }
}

void FixAllBlobs(void)
{
    sdword numBlobs = BlobRegistry.selection->numShips;
    sdword i;

    for (i=0;i<numBlobs;i++)
    {
        FixBlob((blob *)BlobRegistry.selection->ShipPtr[i]);
    }
}

void SaveBlob(blob *tblob)
{
    blob *sblob;
    SaveChunk *chunk;
    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_BLOB,sizeof(blob),tblob);

    sblob = chunkContents(chunk);

    sblob->subBlobs.num = BIT31;        // flag subblobs as nonexistent

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(blob));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save

    dbgAssertOrIgnore(bitTest(sblob->flags, BTF_FreeThisBlob));
    dbgAssertOrIgnore(bitTest(sblob->flags, BTF_FreeBlobObjects));

    if (sblob->blobShips != NULL) SaveSelection((SpaceObjSelection *)sblob->blobShips);
    if (sblob->blobSmallShips != NULL) SaveSelection((SpaceObjSelection *)sblob->blobSmallShips);
    if (sblob->blobBigShips != NULL) SaveSelection((SpaceObjSelection *)sblob->blobBigShips);
    if (sblob->blobSmallTargets != NULL) SaveSelection((SpaceObjSelection *)sblob->blobSmallTargets);
    if (sblob->blobBigTargets != NULL) SaveSelection((SpaceObjSelection *)sblob->blobBigTargets);
    if (sblob->blobResources != NULL) SaveSelection((SpaceObjSelection *)sblob->blobResources);
    if (sblob->blobDerelicts != NULL) SaveSelection((SpaceObjSelection *)sblob->blobDerelicts);
    if (sblob->blobBullets != NULL) SaveSelection((SpaceObjSelection *)sblob->blobBullets);
    if (sblob->blobMissileMissiles != NULL) SaveSelection((SpaceObjSelection *)sblob->blobMissileMissiles);
    if (sblob->blobMissileMines != NULL) SaveSelection((SpaceObjSelection *)sblob->blobMissileMines);

    if (sblob->blobObjects != NULL) SaveSelection((SpaceObjSelection *)sblob->blobObjects);

    memFree(chunk);
}

blob *LoadBlob(void)
{
    SaveChunk *chunk;
    blob *lblob;

    chunk = LoadNextChunk();
    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(blob));       // CAT (M2-R6 oracle-mode-a): the exe's 0x100
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_BLOB,sizeof(blob));

    lblob = memAlloc(sizeof(blob),"loadblob",0);
    memcpy(lblob,chunkContents(chunk),sizeof(blob));

    lblob->subBlobs.num = BIT31;            // flag subblobs as nonexistent

    memFree(chunk);

    if (lblob->blobShips != NULL) { lblob->blobShips = (SelectCommand *)LoadSelection(); lblob->maxNumShips = lblob->blobShips->numShips; }
    if (lblob->blobSmallShips != NULL) { lblob->blobSmallShips = (SelectCommand *)LoadSelection(); lblob->maxNumSmallShips = lblob->blobSmallShips->numShips; }
    if (lblob->blobBigShips != NULL) { lblob->blobBigShips = (SelectCommand *)LoadSelection(); lblob->maxNumBigShips = lblob->blobBigShips->numShips; }
    if (lblob->blobSmallTargets != NULL) { lblob->blobSmallTargets = (SelectAnyCommand *)LoadSelection(); lblob->maxNumSmallTargets = lblob->blobSmallTargets->numTargets; };
    if (lblob->blobBigTargets != NULL) { lblob->blobBigTargets = (SelectAnyCommand *)LoadSelection(); lblob->maxNumBigTargets = lblob->blobBigTargets->numTargets; }
    if (lblob->blobResources != NULL) { lblob->blobResources = (ResourceSelection *)LoadSelection(); lblob->maxNumResources = lblob->blobResources->numResources; }
    if (lblob->blobDerelicts != NULL) { lblob->blobDerelicts = (DerelictSelection *)LoadSelection(); lblob->maxNumDerelicts = lblob->blobDerelicts->numDerelicts; }
    if (lblob->blobBullets != NULL) { lblob->blobBullets = (BulletSelection *)LoadSelection(); lblob->maxNumBullets = lblob->blobBullets->numBullets; }
    if (lblob->blobMissileMissiles != NULL) { lblob->blobMissileMissiles = (MissileSelection *)LoadSelection(); lblob->maxNumMissileMissiles = lblob->blobMissileMissiles->numMissiles; }
    if (lblob->blobMissileMines != NULL) { lblob->blobMissileMines = (MissileSelection *)LoadSelection(); lblob->maxNumMissileMines = lblob->blobMissileMines->numMissiles; }

    if (lblob->blobObjects != NULL) { lblob->blobObjects = LoadSelection(); lblob->maxNumObjects = lblob->blobObjects->numSpaceObjs; }

    return lblob;
}

void FixBlob(blob *tblob)
{
    if (tblob->blobShips != NULL) FixSelection((SpaceObjSelection *)tblob->blobShips);
    if (tblob->blobSmallShips != NULL) FixSelection((SpaceObjSelection *)tblob->blobSmallShips);
    if (tblob->blobBigShips != NULL) FixSelection((SpaceObjSelection *)tblob->blobBigShips);
    if (tblob->blobSmallTargets != NULL) FixSelection((SpaceObjSelection *)tblob->blobSmallTargets);
    if (tblob->blobBigTargets != NULL) FixSelection((SpaceObjSelection *)tblob->blobBigTargets);
    if (tblob->blobResources != NULL) FixSelection((SpaceObjSelection *)tblob->blobResources);
    if (tblob->blobDerelicts != NULL) FixSelection((SpaceObjSelection *)tblob->blobDerelicts);
    if (tblob->blobBullets != NULL) FixSelection((SpaceObjSelection *)tblob->blobBullets);
    if (tblob->blobMissileMissiles != NULL) FixSelection((SpaceObjSelection *)tblob->blobMissileMissiles);
    if (tblob->blobMissileMines != NULL) FixSelection((SpaceObjSelection *)tblob->blobMissileMines);

    if (tblob->blobObjects != NULL) FixSelection((SpaceObjSelection *)tblob->blobObjects);
}

void SaveAllSpaceObjs(void)
{
    sdword numObjs = SpaceObjRegistry.selection->numShips;
    sdword i;
    InfoChunkContents infocontents;
    SaveChunk *chunk;

    infocontents.info = numObjs;

    SaveThisChunk(chunk = CreateChunk(INFO_CHUNK,sizeof(InfoChunkContents),&infocontents));
    for (i=0;i<numObjs;i++)
    {
        SaveSpaceObj((SpaceObj *)SpaceObjRegistry.selection->ShipPtr[i]);
    }

    memFree(chunk);
}

void LoadAllSpaceObjs(void)
{
    SaveChunk *infochunk;
    sdword numObjs;
    SpaceObj *obj;
    sdword i;

    infochunk = LoadNextChunk();
    VerifyChunk(infochunk,INFO_CHUNK,sizeof(InfoChunkContents));

    numObjs = ((InfoChunkContents *)chunkContents(infochunk))->info;

    memFree(infochunk);

    for (i=0;i<numObjs;i++)
    {
        obj = LoadSpaceObj();
        SpaceObjRegistryRegisterNoCheck(obj);
    }
}

void FixAllSpaceObjs(void)
{
    sdword numObjs = SpaceObjRegistry.selection->numShips;
    sdword i;

    for (i=0;i<numObjs;i++)
    {
        FixSpaceObj((SpaceObj *)SpaceObjRegistry.selection->ShipPtr[i]);
    }
}

void SaveAttackTargets(AttackTargets *multipleAttackTargets)
{
    AttackTargets *savecontents;
    SaveChunk *chunk;
    sdword i;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_ATTACKTARGETS,sizeofAttackTargets(multipleAttackTargets->numAttackTargets),multipleAttackTargets);
    savecontents = chunkContents(chunk);

    for (i=0;i<multipleAttackTargets->numAttackTargets;i++)
    {
        savecontents->TargetPtr[i] = (TargetPtr)SpaceObjRegistryGetID((SpaceObj *)multipleAttackTargets->TargetPtr[i]);
    }

    saveCat32SaveArray(chunk, NULL, 4, (udword)offsetof(AttackTargets, TargetPtr), NULL, multipleAttackTargets->numAttackTargets);   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);
}

AttackTargets *LoadAttackTargets(void)
{
    AttackTargets *loadedAttackTargets;
    AttackTargets *originalAttackTargets;
    SaveChunk *chunk;
    sdword size;

    chunk = LoadNextChunk();
    // CAT (M2-R6 oracle-mode-a): the exe's count + 4-byte IDs (LoadShip 0x00500490)
    chunk = saveCat32ChunkArray(chunk, NULL, 4, (udword)offsetof(AttackTargets, TargetPtr), NULL, saveCat32ChunkCount(chunk, 0));
    VerifyChunkNoSize(chunk,BASIC_STRUCTURE|SAVE_ATTACKTARGETS);

    originalAttackTargets = (AttackTargets *)chunkContents(chunk);
    size = sizeofAttackTargets(originalAttackTargets->numAttackTargets);
    dbgAssertOrIgnore(size == chunk->contentsSize);
    loadedAttackTargets = memAlloc(size,"AttackTargets",0);
    memcpy(loadedAttackTargets,originalAttackTargets,size);

    memFree(chunk);

    return loadedAttackTargets;
}

void FixAttackTargets(AttackTargets *multipleAttackTargets)
{
    sdword i;

    for (i=0;i<multipleAttackTargets->numAttackTargets;i++)
    {
        multipleAttackTargets->TargetPtr[i] = SpaceObjRegistryGetTarget((sdword)multipleAttackTargets->TargetPtr[i]);
    }
}

void SaveGunInfo(Ship *ship)
{
    GunInfo *originalGunInfo = ship->gunInfo;
    GunInfo *savecontents;
    SaveChunk *chunk;
    sdword i;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_GUNINFO,sizeofGunInfo(originalGunInfo->numGuns),originalGunInfo);
    savecontents = chunkContents(chunk);

    for (i=0;i<originalGunInfo->numGuns;i++)
    {
        savecontents->guns[i].gunstatic = NULL;
    }

    saveCat32SaveArray(chunk, NULL, 4, (udword)offsetof(GunInfo, guns), SAVECAT32_LAYOUT(Gun), originalGunInfo->numGuns);   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);
}

void LoadGunInfo(Ship *ship)
{
    GunInfo *loadedGunInfo;
    GunInfo *originalGunInfo;
    SaveChunk *chunk;
    sdword size;
    sdword i;

    chunk = LoadNextChunk();
    // CAT (M2-R6 oracle-mode-a): the exe's count + 0x88-byte guns (LoadShip 0x00500490)
    chunk = saveCat32ChunkArray(chunk, NULL, 4, (udword)offsetof(GunInfo, guns), SAVECAT32_LAYOUT(Gun), saveCat32ChunkCount(chunk, 0));
    VerifyChunkNoSize(chunk,BASIC_STRUCTURE|SAVE_GUNINFO);

    originalGunInfo = (GunInfo *)chunkContents(chunk);
    size = sizeofGunInfo(originalGunInfo->numGuns);
    dbgAssertOrIgnore(size == chunk->contentsSize);
    loadedGunInfo = memAlloc(size,"gunInfo",NonVolatile);
    memcpy(loadedGunInfo,originalGunInfo,size);

    memFree(chunk);

    ship->gunInfo = loadedGunInfo;

    for (i=0;i<loadedGunInfo->numGuns;i++)
    {
        loadedGunInfo->guns[i].gunstatic = &ship->staticinfo->gunStaticInfo->gunstatics[i];
        // CAT (M2-R4 combat): the gun's two damage types come from its weapons and size (LoadShip 0x00500490)
        cat_gunStaticSetSoundTypes(loadedGunInfo->guns[i].gunstatic);
    }
}

// CAT (M2-R5 dock): the exe's DockInfo is 0x24 + 4n plain words (no pointers): saved and loaded as a block of the
//      static's point count; nothing to fix up
void SaveDockInfo(Ship *ship)
{
    SaveChunk *chunk;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_DOCKINFO,sizeofDockInfo(ship->staticinfo->dockStaticInfo->numDockPoints),ship->dockInfo);
    SaveThisChunk(chunk);
    memFree(chunk);
}

void LoadDockInfo(Ship *ship)
{
    DockInfo *loadedDockInfo;
    SaveChunk *chunk;
    sdword size;

    chunk = LoadNextChunk();
    VerifyChunkNoSize(chunk,BASIC_STRUCTURE|SAVE_DOCKINFO);

    size = sizeofDockInfo(ship->staticinfo->dockStaticInfo->numDockPoints);
    dbgAssertOrIgnore(size == chunk->contentsSize);
    loadedDockInfo = memAlloc(size,"dockInfo",NonVolatile);
    memcpy(loadedDockInfo,chunkContents(chunk),size);

    memFree(chunk);

    ship->dockInfo = loadedDockInfo;
}

void FixDockInfo(Ship *ship)
{
    (void)ship;
}

// CAT (M2-R4 combat): no SaveSalvageInfo/LoadSalvageInfo/FixSalvageInfo: Cataclysm has no HW1 per-object salvage
//      points (ShipOrDerelict.salvageInfo is gone, see SpaceObj.h), so no SAVE_SALVAGEINFO chunk is written or read

void SaveShipSinglePlayerGameInfo(Ship *ship)
{
//    ShipSinglePlayerGameInfo *savecontents;
    SaveChunk *chunk;

    ship->shipSinglePlayerGameInfo->hyperspaceEffect = NULL;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_SHIPSINGLEPLAYERGAMEINFO,sizeof(ShipSinglePlayerGameInfo),ship->shipSinglePlayerGameInfo);
//    savecontents = chunkContents(chunk);

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(ShipSinglePlayerGameInfo));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);
}

void LoadShipSinglePlayerGameInfo(Ship *ship)
{
    ShipSinglePlayerGameInfo *loadedShipSinglePlayerInfo;
    ShipSinglePlayerGameInfo *originalShipSinglePlayerInfo;
    SaveChunk *chunk;

    chunk = LoadNextChunk();
    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(ShipSinglePlayerGameInfo));   // CAT (M2-R6 oracle-mode-a)
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_SHIPSINGLEPLAYERGAMEINFO,sizeof(ShipSinglePlayerGameInfo));

    originalShipSinglePlayerInfo = (ShipSinglePlayerGameInfo *)chunkContents(chunk);
    loadedShipSinglePlayerInfo = memAlloc(sizeof(ShipSinglePlayerGameInfo),"shipsingleGame",0);
    memcpy(loadedShipSinglePlayerInfo,originalShipSinglePlayerInfo,sizeof(ShipSinglePlayerGameInfo));

    ship->shipSinglePlayerGameInfo = loadedShipSinglePlayerInfo;

    ship->shipSinglePlayerGameInfo->hyperspaceEffect = NULL;

    memFree(chunk);
}

void Save_String(char *string)
{
    SaveChunk *chunk;

    if (string == NULL)
    {
        chunk = CreateChunk(VARIABLE_STRUCTURE|SAVESTRING,0,string);
    }
    else
    {
        chunk = CreateChunk(VARIABLE_STRUCTURE|SAVESTRING,strlen(string)+1,string);
    }

    SaveThisChunk(chunk);
    memFree(chunk);
}

char *Load_String(void)
{
    SaveChunk *chunk;
    char *str;
    sdword size;

    chunk = LoadNextChunk();
    VerifyChunkNoSize(chunk,VARIABLE_STRUCTURE|SAVESTRING);

    if (chunk->contentsSize == 0)
    {
        memFree(chunk);
        return NULL;
    }

    size = strlen((char *)chunkContents(chunk)) + 1;
    dbgAssertOrIgnore(size == chunk->contentsSize);

    str = memAlloc(size,"loadedstring",0);
    memcpy(str,chunkContents(chunk),size);

    memFree(chunk);
    return str;
}

void Load_StringToAddress(char *addr)
{
    SaveChunk *chunk;
    sdword size;

    chunk = LoadNextChunk();
    VerifyChunkNoSize(chunk,VARIABLE_STRUCTURE|SAVESTRING);

    size = strlen((char *)chunkContents(chunk)) + 1;
    dbgAssertOrIgnore(size == chunk->contentsSize);

    memcpy(addr,chunkContents(chunk),size);

    memFree(chunk);
}

void SaveStructureOfSize(void *structure,sdword size)
{
    SaveChunk *chunk;
    chunk = CreateChunk(BASIC_STRUCTURE,size,structure);

    SaveThisChunk(chunk);
    memFree(chunk);
}

void *LoadStructureOfSize(sdword size)
{
    SaveChunk *chunk;
    void *structure;

    chunk = LoadNextChunk();
    VerifyChunk(chunk,BASIC_STRUCTURE,size);

    structure = memAlloc(size,"loadedstructure",0);
    memcpy(structure,chunkContents(chunk),size);

    memFree(chunk);
    return structure;
}

void LoadStructureOfSizeToAddress(void *address,sdword size)
{
    SaveChunk *chunk;

    chunk = LoadNextChunk();
    VerifyChunk(chunk,BASIC_STRUCTURE,size);

    memcpy(address,chunkContents(chunk),size);

    memFree(chunk);
}

void SaveStructureWithSize(void *structure)
{
    SaveChunk *chunk;
    chunk = CreateChunk(VARIABLE_STRUCTURE,*((sdword *)structure),structure);

    SaveThisChunk(chunk);
    memFree(chunk);
}

void *LoadStructureWithSize(void)
{
    SaveChunk *chunk;
    void *chunkcontents;
    void *loadedstructure;

    chunk = LoadNextChunk();
    VerifyChunkNoSize(chunk,VARIABLE_STRUCTURE);

    chunkcontents = chunkContents(chunk);
    dbgAssertOrIgnore( (*((sdword *)chunkcontents)) == chunk->contentsSize);

    loadedstructure = memAlloc(chunk->contentsSize,"loadedvarstr",0);
    memcpy(loadedstructure,chunkcontents,chunk->contentsSize);

    memFree(chunk);

    return loadedstructure;
}

void SaveShipsInsideMe(Ship *ship)
{
    SaveChunk *chunk;
    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_SHIPSINSIDEME,sizeof(ShipsInsideMe),ship->shipsInsideMe);

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(ShipsInsideMe));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);

    SaveLinkedListOfInsideShips(&ship->shipsInsideMe->insideList);
}

void LoadShipsInsideMe(Ship *ship)
{
    SaveChunk *chunk;
    ShipsInsideMe *shipsInsideMe;

    chunk = LoadNextChunk();
    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(ShipsInsideMe));  // CAT (M2-R6 oracle-mode-a)
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_SHIPSINSIDEME,sizeof(ShipsInsideMe));

    shipsInsideMe = memAlloc(sizeof(ShipsInsideMe),"ShipsInsideMe",NonVolatile);
    memcpy(shipsInsideMe,chunkContents(chunk),sizeof(ShipsInsideMe));
    ship->shipsInsideMe = shipsInsideMe;

    memFree(chunk);

    LoadLinkedListOfInsideShips(&ship->shipsInsideMe->insideList);
}

// CAT (M2-R5 layout): SaveSlaveInfo / LoadSlaveInfo / FixSlaveInfo are gone with HW1's slave trade (no SlaveInfo in
//      the exe's Ship; SOF_Slaveable has no bit)

void FixShipsInsideMe(Ship *ship)
{
    FixLinkedListOfInsideShips(&ship->shipsInsideMe->insideList);
}

void CopyPointersFromRealStaticHeader(StaticInfo *dynamicstaticheader,StaticInfo *realstaticheader)
{
    dynamicstaticheader->LOD = realstaticheader->LOD;
    dynamicstaticheader->pMexData = realstaticheader->pMexData;
}

void CopyPointersFromRealAsteroidStaticInfo(AsteroidStaticInfo *dynamicstaticinfo,AsteroidStaticInfo *realstaticinfo)
{
    CopyPointersFromRealStaticHeader(&dynamicstaticinfo->common,&realstaticinfo->common);
}

void CopyPointersFromRealDustCloudStaticInfo(DustCloudStaticInfo *dynamicstaticinfo,DustCloudStaticInfo *realstaticinfo)
{
    CopyPointersFromRealStaticHeader(&dynamicstaticinfo->common,&realstaticinfo->common);
}

void CopyPointersFromRealShipStaticInfo(ShipStaticInfo *dynamicstaticinfo,ShipStaticInfo *realstaticinfo)
{
    sdword i;

    CopyPointersFromRealStaticHeader(&dynamicstaticinfo->common,&realstaticinfo->common);

    // CAT: no salvageStaticInfo in Cataclysm (CopyPointersFromRealShipStaticInfo 0x004ffc60 copies none)
    dynamicstaticinfo->custshipheader = realstaticinfo->custshipheader;
    dynamicstaticinfo->custstatinfo = realstaticinfo->custstatinfo;

    for (i=0;i<MAX_NUM_TRAILS;i++)
    {
        dynamicstaticinfo->trailStatic[i] = realstaticinfo->trailStatic[i];
    }

    dynamicstaticinfo->madStatic = realstaticinfo->madStatic;

    dynamicstaticinfo->gunStaticInfo = realstaticinfo->gunStaticInfo;
    dynamicstaticinfo->dockStaticInfo = realstaticinfo->dockStaticInfo;
    dynamicstaticinfo->dockOverideInfo = realstaticinfo->dockOverideInfo;
    dynamicstaticinfo->navlightStaticInfo = realstaticinfo->navlightStaticInfo;
    dynamicstaticinfo->resNozzleStatic = realstaticinfo->resNozzleStatic;
    // CAT: no tractorEmitterStatic in Cataclysm (CopyPointersFromRealShipStaticInfo 0x004ffc60 copies none), and no
    //      HW1 dock callbacks (M2-R5 dock)
}

void SaveClampInfo(ClampInfo *clampInfo)
{
    SaveChunk *chunk;
    ClampInfo *sc;

    chunk = CreateChunk(BASIC_STRUCTURE,sizeof(ClampInfo),clampInfo);
    sc = chunkContents(chunk);

    sc->host = (SpaceObjRotImpTarg*)SpaceObjRegistryGetID((SpaceObj *)sc->host);

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(ClampInfo));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save

    // CAT: a host's clamped-object list follows as a selection (SaveShip 0x004ffd70 / LoadSpaceObj's derelict case)
    if (clampInfo->clampedObjs != NULL)
    {
        SaveSelection((SpaceObjSelection *)clampInfo->clampedObjs);
    }

    memFree(chunk);
}

ClampInfo *LoadClampInfo(void)
{
    SaveChunk *chunk = LoadNextChunk();
    ClampInfo *clampInfo;
    SpaceObjSelection *selection;

    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(ClampInfo));    // CAT (M2-R6 oracle-mode-a)
    VerifyChunk(chunk,BASIC_STRUCTURE,sizeof(ClampInfo));
    clampInfo = memAlloc(sizeof(ClampInfo),"loadedstructure",0);
    memcpy(clampInfo,chunkContents(chunk),sizeof(ClampInfo));
    memFree(chunk);
    // CAT: a clamp without a host (ID -1) is a host whose clamped-object list follows as a selection, read into a
    //      fresh 1000-entry list (LoadShip 0x00500490: memAlloc(0xFA8), LoadSelection); FixClampInfo resolves its IDs.
    //      Otherwise the saved list pointer is stale.
    clampInfo->clampedObjs = NULL;
    if ((sdword)(size_t)clampInfo->host == -1)
    {
        clampInfo->clampedObjs = memAlloc(sizeof(CatClampedList),"clampedObjs",NonVolatile);
        selection = LoadSelection();
        memcpy(clampInfo->clampedObjs, selection, sizeofSelectCommand(selection->numSpaceObjs));
        memFree(selection);
    }
    return clampInfo;
}

void FixClampInfo(ClampInfo *clampInfo)
{
    clampInfo->host = SpaceObjRegistryGetTarget((sdword)(size_t)clampInfo->host);
    // CAT: the host's list (FixShip 0x00500950: FixSelection of +0x34 when the host is NULL)
    if ((clampInfo->host == NULL) && (clampInfo->clampedObjs != NULL))
    {
        FixSelection((SpaceObjSelection *)clampInfo->clampedObjs);
    }
}

void SaveShip(Ship *ship)
{
    udword sizespec;
    udword sizeship = sizeof(Ship), totalsize; //, madoffset;
    ShipStaticInfo *shipstaticinfo = ship->staticinfo;
    sdword i,j;

    Ship *savecontents;
    SaveChunk *chunk;

    if ((sizespec = shipstaticinfo->custshipheader.sizeofShipSpecifics) != 0)
    {
        sizeship += sizespec - sizeof(udword);
    }
    sizeship = (sizeship + 7) & ~(udword)7;     // CAT-FIX: the blocks behind the ship start 8-aligned, as univCreateShip makes them
    totalsize = sizeship;
    if (shipstaticinfo->common.staticCollInfo.preciseSelection != 0xff)
    {
        totalsize += sizeof(PreciseSelection);
    }

    if (ship->madBindings != NULL)
    {
        //madoffset = totalsize;
        totalsize += ship->madBindings->totalSize;
    }

    ship->catDissolveEffect = NULL;             // CAT: SaveShip 0x004ffd70 first clears +0x594 on the ship itself

    chunk = CreateChunk(VARIABLE_STRUCTURE|SAVE_SPACEOBJ|OBJ_ShipType,totalsize,ship);

    savecontents = chunkContents(chunk);

    savecontents->staticinfo = NULL;
    savecontents->obj.collMyBlob = (blob*)BlobRegistryGetID(ship->obj.collMyBlob);
    savecontents->impactable.collInfo.precise = NULL;
    savecontents->playerowner = (Player*)SavePlayerToPlayerIndex(savecontents->playerowner);

    savecontents->shipOrDerelict.dockingship = (Ship *)SpaceObjRegistryGetID((SpaceObj *)ship->shipOrDerelict.dockingship);
    savecontents->rowGetOutOfWay = (Ship*)SpaceObjRegistryGetID((SpaceObj *)ship->rowGetOutOfWay);
    savecontents->gettingrocked = NULL;
    savecontents->recentAttacker = (Ship*)SpaceObjRegistryGetID((SpaceObj *)ship->recentAttacker);
    savecontents->firingAtUs = (Ship*)SpaceObjRegistryGetID((SpaceObj *)ship->firingAtUs);
    // CAT: the Beast infection's ship and owner, the salvage depot and big gun, the creator and the queued attack
    //      target are saved as IDs too (SaveShip 0x004ffd70: +0x4E8, +0x4EC, +0x3EC, +0x608, +0x4AC, +0x4B0)
    savecontents->catBeastInfector = (Ship*)SpaceObjRegistryGetID((SpaceObj *)ship->catBeastInfector);
    savecontents->catBeastInfectorOwner = (Player*)SavePlayerToPlayerIndex(ship->catBeastInfectorOwner);
    savecontents->catSalvageDepot = (Ship*)SpaceObjRegistryGetID((SpaceObj *)ship->catSalvageDepot);
    savecontents->catSalvageBigGun = (Ship*)SpaceObjRegistryGetID((SpaceObj *)ship->catSalvageBigGun);
    savecontents->catCreator = (Ship*)SpaceObjRegistryGetID((SpaceObj *)ship->catCreator);
    savecontents->catQueuedAttackTarget = (Ship*)SpaceObjRegistryGetID((SpaceObj *)ship->catQueuedAttackTarget);

    // ship->flightmanInfo saved in chunk if non-NULL
    // CAT: formationcommand is saved as it is (SaveShip 0x004ffd70 converts only the command at +0x370)
    savecontents->command = (CommandToDo *)ConvertPointerInListToNum(&universe.mainCommandLayer.todolist,savecontents->command);

    //ship->attackvars.multipleAttackTarget saved in chunk if non-NULL

    savecontents->attackvars.attacktarget = (SpaceObjRotImpTarg*)SpaceObjRegistryGetID((SpaceObj *)ship->attackvars.attacktarget);
    savecontents->attackvars.myLeaderIs = (Ship*)SpaceObjRegistryGetID((SpaceObj *)ship->attackvars.myLeaderIs);
    savecontents->attackvars.myWingmanIs = (Ship*)SpaceObjRegistryGetID((SpaceObj *)ship->attackvars.myWingmanIs);

    savecontents->dockvars.dockship = (Ship*)SpaceObjRegistryGetID((SpaceObj *)savecontents->dockvars.dockship);
    // CAT (M2-R5 dock): the other four dock words are plain values

    savecontents->rceffect = NULL;

    PreFix_ShipXHarvestsResourceY(savecontents);

    for (i=0;i<3;i++)
    {
        for (j=0;j<3;j++)
        {
            savecontents->showingDamage[i][j] = NULL;
        }
    }

    savecontents->tractorbeam_playerowner = (Player*)SavePlayerToPlayerIndex(savecontents->tractorbeam_playerowner);

    savecontents->newDockWithTransfer = (Ship*)SpaceObjRegistryGetID((SpaceObj *)savecontents->newDockWithTransfer);

    for (i=0;i<MAX_NUM_TRAILS;i++)
    {
        savecontents->trail[i] = 0;     // on load re-initialize trails
    }

    // savecontents->soundevent will get reinitialized on load

    // slaveInfo saved

    // gunInfo saved
    // dockInfo saved

    // reinitialize navLightInfo

    // shipsInsideMe saved

    if (ship->madBindings != NULL)
    {
        //savecontents->madBindings = (madanim *)(((ubyte *)savecontents) + madoffset);
        PreFix_madBindings(ship,savecontents);
    }

    // fix later
    for (i=0;i<2;i++)
    {
        savecontents->lightning[i] = 0;
    }

    if (shipstaticinfo->custshipheader.CustShip_PreFix != NULL)
    {
        shipstaticinfo->custshipheader.CustShip_PreFix(savecontents);
    }

    saveCat32SaveShip(chunk, ship);   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save

    if (ship->header.flags & SOF_StaticInfoIsDynamic)
    {
        SaveChunk *statchunk = CreateChunk(BASIC_STRUCTURE|SAVE_STATICINFO,sizeof(ShipStaticInfo),ship->staticinfo);
        saveCat32SaveStruct(statchunk, SAVECAT32_LAYOUT(ShipStaticInfo));
        memFree(statchunk);
    }

    if (ship->guidance.clampInfo != NULL)
    {
        SaveClampInfo(ship->guidance.clampInfo);
    }


    if (ship->flightmanInfo != NULL)
    {
        SaveStructureWithSize(ship->flightmanInfo);
    }

    if (ship->attackvars.multipleAttackTargets != NULL)
    {
        SaveAttackTargets(ship->attackvars.multipleAttackTargets);
    }

    if (ship->damageLights)
    {
        dbgAssertOrIgnore(ship->nDamageLights > 0);
        SaveStructureOfSize(ship->damageLights,ship->nDamageLights*sizeof(DamageLightStatic));
    }

    if (ship->gunInfo != NULL)
    {
        SaveGunInfo(ship);
    }

    if (ship->dockInfo != NULL)
    {
        SaveDockInfo(ship);
    }

    if (ship->shipsInsideMe != NULL)
    {
        SaveShipsInsideMe(ship);
    }

    if (ship->madBindings != NULL)
    {
        Save_madBindings(ship);
    }

    if (ship->shipSinglePlayerGameInfo != NULL)
    {
        SaveShipSinglePlayerGameInfo(ship);
    }

    if (shipstaticinfo->custshipheader.CustShip_Save != NULL)
    {
        shipstaticinfo->custshipheader.CustShip_Save(ship);
    }
//done:
    memFree(chunk);
}

Ship *LoadShip(SaveChunk *chunk)
{
    Ship *ship;
    udword sizespec;
    udword sizeship = sizeof(Ship), totalsize;
    ShipStaticInfo *shipstaticinfo;

    VerifyChunkNoSize(chunk,VARIABLE_STRUCTURE|SAVE_SPACEOBJ|OBJ_ShipType);
    chunk = saveCat32ChunkShip(chunk);         // CAT (M2-R6 oracle-mode-a): the exe's Ship + specifics + blocks

    ship = memAlloc(chunk->contentsSize,"Ship",NonVolatile);
    memcpy(ship,chunkContents(chunk),chunk->contentsSize);

    if (ship->header.flags & SOF_StaticInfoIsDynamic)
    {
        SaveChunk *statchunk = LoadNextChunk();
        statchunk = saveCat32ChunkStruct(statchunk, SAVECAT32_LAYOUT(ShipStaticInfo));
        VerifyChunk(statchunk,BASIC_STRUCTURE|SAVE_STATICINFO,sizeof(ShipStaticInfo));

        ship->staticinfo = shipstaticinfo = memAlloc(sizeof(ShipStaticInfo),"shipstatic",NonVolatile);
        memcpy(ship->staticinfo,chunkContents(statchunk),statchunk->contentsSize);
        memFree(statchunk);

        CopyPointersFromRealShipStaticInfo(ship->staticinfo,GetShipStaticInfo(ship->shiptype,ship->shiprace));
    }
    else
    {
        ship->staticinfo = shipstaticinfo = GetShipStaticInfo(ship->shiptype,ship->shiprace);
    }

    if ((sizespec = shipstaticinfo->custshipheader.sizeofShipSpecifics) != 0)
    {
        sizeship += sizespec - sizeof(udword);
    }
    sizeship = (sizeship + 7) & ~(udword)7;     // CAT-FIX: the blocks behind the ship start 8-aligned, as univCreateShip makes them
    totalsize = sizeship;
    if (shipstaticinfo->common.staticCollInfo.preciseSelection != 0xff)
    {
        totalsize += sizeof(PreciseSelection);
    }

    if (shipstaticinfo->common.staticCollInfo.preciseSelection != 0xff)
    {
        ship->impactable.collInfo.precise = (PreciseSelection *)(((ubyte *)ship) + sizeship);
    }
    else
    {
        ship->impactable.collInfo.precise = NULL;
    }

    if (ship->madBindings)
    {
        ship->madBindings = (madanim *)(((ubyte *)ship) + totalsize);
        totalsize += ship->madBindings->totalSize;
    }

    dbgAssertOrIgnore(totalsize == chunk->contentsSize);

    memFree(chunk);

    // re-initialize engine trails
    InitializeEngineTrails(ship);

    if (ship->guidance.clampInfo != NULL)
    {
        ship->guidance.clampInfo = LoadClampInfo();
    }


    if (ship->flightmanInfo != NULL)
    {
        ship->flightmanInfo = LoadStructureWithSize();
    }

    if (ship->attackvars.multipleAttackTargets != NULL)
    {
        ship->attackvars.multipleAttackTargets = LoadAttackTargets();
    }

    if (ship->damageLights != NULL)
    {
        dbgAssertOrIgnore(ship->nDamageLights > 0);
        ship->damageLights = LoadStructureOfSize(ship->nDamageLights*sizeof(DamageLightStatic));  // plain words in both layouts
    }

    if (ship->gunInfo != NULL)
    {
        LoadGunInfo(ship);
    }

    if (ship->dockInfo != NULL)
    {
        LoadDockInfo(ship);
    }

    InitializeNavLights(ship);

    if (ship->shipsInsideMe != NULL)
    {
        LoadShipsInsideMe(ship);
    }

    if (ship->bindings != NULL)
    {
        ship->bindings = univMeshBindingsDupe(shipstaticinfo, ship);        // reinitialize bindings
    }

    if (ship->madBindings != NULL)
    {
        Load_madBindings(ship);
    }

    if (ship->shipSinglePlayerGameInfo != NULL)
    {
        LoadShipSinglePlayerGameInfo(ship);
    }

    if (shipstaticinfo->custshipheader.CustShip_Load != NULL)
    {
        shipstaticinfo->custshipheader.CustShip_Load(ship);
    }
    ship->catDissolveEffect = NULL;             // CAT: LoadShip 0x00500490 clears +0x594 (a stale effect pointer)
//done:
    return ship;
}

void FixShip(Ship *ship)
{
    ShipStaticInfo *shipstaticinfo = ship->staticinfo;

    ship->obj.collMyBlob = BlobRegistryGetBlob((sdword)(size_t)ship->obj.collMyBlob);

    ship->playerowner = SavePlayerIndexToPlayer(ship->playerowner);

    ship->shipOrDerelict.dockingship = SpaceObjRegistryGetShip((sdword)(size_t)ship->shipOrDerelict.dockingship);

    ship->rowGetOutOfWay = SpaceObjRegistryGetShip((sdword)(size_t)ship->rowGetOutOfWay);
    ship->recentAttacker = SpaceObjRegistryGetShip((sdword)(size_t)ship->recentAttacker);
    ship->firingAtUs = SpaceObjRegistryGetShip((sdword)(size_t)ship->firingAtUs);

    // CAT: the Beast infection's ship and owner, and the bindings of the type it turns into (FixShip 0x00500950)
    ship->catBeastInfector = (Ship *)SpaceObjRegistryGetObj((sdword)(size_t)ship->catBeastInfector);
    ship->catBeastInfectorOwner = SavePlayerIndexToPlayer(ship->catBeastInfectorOwner);
    if (ship->catBeastBindings != NULL)
    {
        ship->catBeastBindings = cat_univMeshBindingsDupeNoGuns(GetShipStaticInfo(cat_beastGetAttackStaticInfo(ship)->convertTo, RACE_Beast));
    }

    // CAT: formationcommand is left as saved (FixShip 0x00500950 fixes only the command)
    ship->command = ConvertNumToPointerInList(&universe.mainCommandLayer.todolist,(sdword)(size_t)ship->command);

    if (ship->guidance.clampInfo)
    {
        FixClampInfo(ship->guidance.clampInfo);
    }

    if (ship->attackvars.multipleAttackTargets)
    {
        FixAttackTargets(ship->attackvars.multipleAttackTargets);
    }
    ship->attackvars.attacktarget = SpaceObjRegistryGetTarget((sdword)(size_t)ship->attackvars.attacktarget);
    ship->attackvars.myLeaderIs = SpaceObjRegistryGetShip((sdword)(size_t)ship->attackvars.myLeaderIs);
    ship->attackvars.myWingmanIs = SpaceObjRegistryGetShip((sdword)(size_t)ship->attackvars.myWingmanIs);

    ship->dockvars.dockship = SpaceObjRegistryGetShip((sdword)(size_t)ship->dockvars.dockship);

    // CAT: the salvage depot (and the bindings of the type a delivered ship becomes) and the big gun
    ship->catSalvageDepot = (Ship *)SpaceObjRegistryGetObj((sdword)(size_t)ship->catSalvageDepot);
    if (ship->catSalvageConvertBindings != NULL)
    {
        ship->catSalvageConvertBindings = cat_univMeshBindingsDupeNoGuns(GetShipStaticInfo(cat_salvageConvertType(ship, ship->catSalvageDepot), RACE_Sect));
    }
    ship->catSalvageBigGun = (Ship *)SpaceObjRegistryGetObj((sdword)(size_t)ship->catSalvageBigGun);

    if (ship->dockInfo != NULL)
    {
        FixDockInfo(ship);
    }

    if (ship->shipsInsideMe != NULL)
    {
        FixShipsInsideMe(ship);
    }

    if (ship->madBindings != NULL)
    {
        Fix_madBindings(ship);
    }

    // CAT: the creator and the queued attack target (FixShip 0x00500950: +0x4AC, +0x4B0)
    ship->catCreator = (Ship *)SpaceObjRegistryGetObj((sdword)(size_t)ship->catCreator);
    ship->catQueuedAttackTarget = (Ship *)SpaceObjRegistryGetObj((sdword)(size_t)ship->catQueuedAttackTarget);

    Fix_ShipXHarvestsResourceY(ship);

    ship->tractorbeam_playerowner = SavePlayerIndexToPlayer(ship->tractorbeam_playerowner);

    ship->newDockWithTransfer = SpaceObjRegistryGetShip((sdword)(size_t)ship->newDockWithTransfer);

    if (shipstaticinfo->custshipheader.CustShip_Fix != NULL)
    {
        shipstaticinfo->custshipheader.CustShip_Fix(ship);
    }
    // CAT (M2-R6 nis-fow-camera): the repair droids are made anew after a load (FixShip 0x00500950: droidCreateArray
    //      for a Sect type with nRepairDroids, else NULL; the saved pointer is stale)
    if ((shipstaticinfo->nRepairDroids == 0) || (ship->shiprace != RACE_Sect))
    {
        ship->catRepairDroids = NULL;
    }
    else
    {
        ship->catRepairDroids = droidCreateArray(ship, (sdword)shipstaticinfo->nRepairDroids);
    }
    // CAT: then the static's maximum health is set again, and a single-player Somtaaw ship's health takes the
    //      difficulty's factor (FixShip 0x00500950: cat_univSetShipMaxHealth, static race +0x1E4 == 0, 0x008941a4)
    cat_univSetShipMaxHealth(shipstaticinfo);
    if (singlePlayerGame && (shipstaticinfo->shiprace == RACE_Sect))
    {
        ship->targetable.health *= spDifficultyHealthLoadFactor;
    }
}

void SaveAsteroid(Asteroid *asteroid)
{
    Asteroid *savecontents;
    SaveChunk *chunk;
    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_SPACEOBJ|OBJ_AsteroidType,sizeof(Asteroid),asteroid);

    savecontents = chunkContents(chunk);

    savecontents->staticinfo = NULL;
    savecontents->obj.collMyBlob = (blob*)BlobRegistryGetIDNoBlobOkay(asteroid->obj.collMyBlob);

    savecontents->resource.resourceVolume = (ResourceVolume *)ConvertPointerInListToNum(&universe.ResourceVolumeList,asteroid->resource.resourceVolume);
    // CAT: the mimic posing as this asteroid is saved as its ID (FixAllSpaceObjs 0x004ff3e0 resolves +0x1A4)
    savecontents->catMimicShip = (Ship *)(size_t)SpaceObjRegistryGetID((SpaceObj *)asteroid->catMimicShip);

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(Asteroid));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);

    dbgAssertOrIgnore(asteroid->staticinfo != NULL);
    if (asteroid->header.flags & SOF_StaticInfoIsDynamic)
    {
        SaveChunk *statchunk = CreateChunk(BASIC_STRUCTURE|SAVE_STATICINFO,sizeof(AsteroidStaticInfo),asteroid->staticinfo);
        saveCat32SaveStruct(statchunk, SAVECAT32_LAYOUT(AsteroidStaticInfo));
        memFree(statchunk);
    }
}

Asteroid *LoadAsteroid(SaveChunk *chunk)
{
    Asteroid *asteroid;

    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(Asteroid));  // CAT (M2-R6 oracle-mode-a): the exe's 0x1B4
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_SPACEOBJ|OBJ_AsteroidType,sizeof(Asteroid));

    asteroid = memAlloc(sizeof(Asteroid),"Asteroid",NonVolatile);

    memcpy(asteroid,chunkContents(chunk),chunk->contentsSize);

    memFree(chunk);

    if (asteroid->header.flags & SOF_StaticInfoIsDynamic)
    {
        SaveChunk *statchunk = LoadNextChunk();
        statchunk = saveCat32ChunkStruct(statchunk, SAVECAT32_LAYOUT(AsteroidStaticInfo));
        VerifyChunk(statchunk,BASIC_STRUCTURE|SAVE_STATICINFO,sizeof(AsteroidStaticInfo));

        asteroid->staticinfo = memAlloc(sizeof(AsteroidStaticInfo),"aststatic",NonVolatile);
        memcpy(asteroid->staticinfo,chunkContents(statchunk),statchunk->contentsSize);
        memFree(statchunk);

        CopyPointersFromRealAsteroidStaticInfo(asteroid->staticinfo,&asteroidStaticInfos[asteroid->asteroidtype]);
    }
    else
    {
        asteroid->staticinfo = &asteroidStaticInfos[asteroid->asteroidtype];
    }

    // asteroid->obj.collMyBlob gets fixed in FixAllSpaceObjs

    return asteroid;
}

void FixAsteroid(Asteroid *asteroid)
{
    asteroid->obj.collMyBlob = BlobRegistryGetBlob((sdword)asteroid->obj.collMyBlob);
    asteroid->resource.resourceVolume = ConvertNumToPointerInList(&universe.ResourceVolumeList,(sdword)asteroid->resource.resourceVolume);
    // CAT: the mimic posing as it, from its ID (FixAllSpaceObjs 0x004ff3e0: +0x1A4, -1 NULL)
    asteroid->catMimicShip = (Ship *)SpaceObjRegistryGetObj((sdword)(size_t)asteroid->catMimicShip);
}

void SaveCloudSystem(DustCloud *dustcloud)
{
    cloudSystem *savecontents;
    SaveChunk *chunk;
    sdword i;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_CLOUDSYSTEM,sizeof(cloudSystem),dustcloud->stub);

    savecontents = chunkContents(chunk);

    savecontents->position = NULL;
    savecontents->rotation = NULL;

    for (i=0;i<MAX_BURSTS;i++)
    {
        savecontents->bursts[i] = 0;        // reset all lightening for now.  Fix later
    }

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(cloudSystem));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save

    memFree(chunk);
}

void LoadCloudSystem(DustCloud *dustcloud)
{
    cloudSystem *stub;
    SaveChunk *chunk;

    chunk = LoadNextChunk();
    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(cloudSystem));    // CAT (M2-R6 oracle-mode-a)
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_CLOUDSYSTEM,sizeof(cloudSystem));

    stub = memAlloc(sizeof(cloudSystem),"cloud system", NonVolatile);

    memcpy(stub,chunkContents(chunk),chunk->contentsSize);

    memFree(chunk);

    dustcloud->stub = stub;
    stub->position = &dustcloud->obj.posinfo.position;
    stub->rotation = &dustcloud->rotatable.rotinfo.coordsys;
}

void SaveDustCloud(DustCloud *dustcloud)
{
    DustCloud *savecontents;
    SaveChunk *chunk;
    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_SPACEOBJ|OBJ_DustType,sizeof(DustCloud),dustcloud);

    savecontents = chunkContents(chunk);

    savecontents->staticinfo = NULL;
    savecontents->obj.collMyBlob = (blob*)BlobRegistryGetID(dustcloud->obj.collMyBlob);

    savecontents->resource.resourceVolume = (ResourceVolume *)ConvertPointerInListToNum(&universe.ResourceVolumeList,dustcloud->resource.resourceVolume);

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(DustCloud));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);

    dbgAssertOrIgnore(dustcloud->staticinfo != NULL);
    if (dustcloud->header.flags & SOF_StaticInfoIsDynamic)
    {
        SaveChunk *statchunk = CreateChunk(BASIC_STRUCTURE|SAVE_STATICINFO,sizeof(DustCloudStaticInfo),dustcloud->staticinfo);
        saveCat32SaveStruct(statchunk, SAVECAT32_LAYOUT(DustCloudStaticInfo));
        memFree(statchunk);
    }

    if (dustcloud->stub != NULL)
    {
        SaveCloudSystem(dustcloud);
    }
}

DustCloud *LoadDustCloud(SaveChunk *chunk)
{
    DustCloud *dustcloud;

    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(DustCloud)); // CAT (M2-R6 oracle-mode-a)
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_SPACEOBJ|OBJ_DustType,sizeof(DustCloud));

    dustcloud = memAlloc(sizeof(DustCloud),"DustCloud",NonVolatile);

    memcpy(dustcloud,chunkContents(chunk),chunk->contentsSize);

    memFree(chunk);

    if (dustcloud->header.flags & SOF_StaticInfoIsDynamic)
    {
        SaveChunk *statchunk = LoadNextChunk();
        statchunk = saveCat32ChunkStruct(statchunk, SAVECAT32_LAYOUT(DustCloudStaticInfo));
        VerifyChunk(statchunk,BASIC_STRUCTURE|SAVE_STATICINFO,sizeof(DustCloudStaticInfo));

        dustcloud->staticinfo = memAlloc(sizeof(DustCloudStaticInfo),"dcloudstatic",NonVolatile);
        memcpy(dustcloud->staticinfo,chunkContents(statchunk),statchunk->contentsSize);
        memFree(statchunk);

        CopyPointersFromRealDustCloudStaticInfo((DustCloudStaticInfo *)dustcloud->staticinfo,&dustcloudStaticInfos[dustcloud->dustcloudtype]);
    }
    else
    {
        dustcloud->staticinfo = &dustcloudStaticInfos[dustcloud->dustcloudtype];
    }

    // dustcloud->obj.collMyBlob gets fixed in FixAllSpaceObjs

    if (dustcloud->stub != NULL)
    {
        LoadCloudSystem(dustcloud);
    }

    return dustcloud;
}

void FixDustCloud(DustCloud *dustcloud)
{
    dustcloud->obj.collMyBlob = BlobRegistryGetBlob((sdword)dustcloud->obj.collMyBlob);
    dustcloud->resource.resourceVolume = ConvertNumToPointerInList(&universe.ResourceVolumeList,(sdword)dustcloud->resource.resourceVolume);
}

void SaveNebula(Nebula *nebula)
{
    Nebula *savecontents;
    SaveChunk *chunk;
    nebChunk* nebchunk;
    nebulae_t* neb;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_SPACEOBJ|OBJ_NebulaType,sizeof(Nebula),nebula);

    dbgAssertOrIgnore((nebula->header.flags & SOF_StaticInfoIsDynamic) == 0);

    savecontents = chunkContents(chunk);

    savecontents->staticinfo = NULL;
    savecontents->obj.collMyBlob = (blob*)BlobRegistryGetID(nebula->obj.collMyBlob);
    savecontents->resource.resourceVolume = (ResourceVolume*)ConvertPointerInListToNum(&universe.ResourceVolumeList,nebula->resource.resourceVolume);
    nebchunk = (nebChunk*)nebula->stub;
    neb = (nebulae_t*)nebchunk->nebulae;
    savecontents->stub = (nebChunk*)nebChunkPtrToNum(neb, nebula->stub);

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(Nebula));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);
}

Nebula *LoadNebula(SaveChunk *chunk)
{
    Nebula *nebula;

    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(Nebula));    // CAT (M2-R6 oracle-mode-a)
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_SPACEOBJ|OBJ_NebulaType,sizeof(Nebula));

    nebula = memAlloc(sizeof(Nebula),"Nebula",NonVolatile);

    memcpy(nebula,chunkContents(chunk),chunk->contentsSize);

    memFree(chunk);

    nebula->staticinfo = &nebulaStaticInfos[nebula->nebulatype];

    return nebula;
}

void FixNebula(Nebula *nebula)
{
    nebulae_t* neb;

    nebula->obj.collMyBlob = BlobRegistryGetBlob((sdword)nebula->obj.collMyBlob);
    nebula->resource.resourceVolume = ConvertNumToPointerInList(&universe.ResourceVolumeList,(sdword)nebula->resource.resourceVolume);

    neb = &nebNebulae[nebula->nebNebulaeIndex];
    nebula->stub = nebNumToChunkPtr(neb, (sdword)nebula->stub);
}

void SaveBullet(Bullet *bullet)
{
    Bullet *savecontents;
    SaveChunk *chunk;
    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_SPACEOBJ|OBJ_BulletType,sizeof(Bullet),bullet);

    savecontents = chunkContents(chunk);

    dbgAssertOrIgnore(savecontents->obj.staticinfo == NULL);
    savecontents->obj.collMyBlob = (blob*)BlobRegistryGetID(bullet->obj.collMyBlob);

    savecontents->owner = (Ship*)SpaceObjRegistryGetID((SpaceObj *)bullet->owner);
    if (bullet->gunowner != NULL)
    {
        savecontents->gunowner = (Gun*)bullet->gunowner->gunstatic->gunindex;
    }
    else
    {
        savecontents->gunowner = (Gun*)-1;
    }
    savecontents->target = (SpaceObjRotImpTarg*)SpaceObjRegistryGetID((SpaceObj *)bullet->target);

    savecontents->hitEffect = NULL;
    savecontents->effect = NULL;

    savecontents->playerowner = (Player*)SavePlayerToPlayerIndex(bullet->playerowner);

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(Bullet));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save

    memFree(chunk);
}

Bullet *LoadBullet(SaveChunk *chunk)
{
    Bullet *bullet;

    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(Bullet));    // CAT (M2-R6 oracle-mode-a): the exe's 0x164
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_SPACEOBJ|OBJ_BulletType,sizeof(Bullet));

    bullet = memAlloc(sizeof(Bullet),"Bullet",0);

    memcpy(bullet,chunkContents(chunk),chunk->contentsSize);

    memFree(chunk);

    // bullet->obj.collMyBlob gets fixed in FixAllSpaceObjs
    return bullet;
}

void FixBullet(Bullet *bullet)
{
    bullet->obj.collMyBlob = BlobRegistryGetBlob((sdword)bullet->obj.collMyBlob);

    bullet->owner = SpaceObjRegistryGetShip((sdword)bullet->owner);
    if (bullet->gunowner != (Gun*)-1)
    {
        dbgAssertOrIgnore((sdword)bullet->gunowner < bullet->owner->gunInfo->numGuns);
        bullet->gunowner = &bullet->owner->gunInfo->guns[(sdword)bullet->gunowner];
    }
    else
    {
        bullet->gunowner = NULL;
    }
    bullet->target = SpaceObjRegistryGetTarget((sdword)bullet->target);

    bullet->playerowner = SavePlayerIndexToPlayer(bullet->playerowner);
}

void SaveDerelict(Derelict *derelict)
{
    Derelict *savecontents;
    SaveChunk *chunk;
    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_SPACEOBJ|OBJ_DerelictType,sizeof(Derelict),derelict);

    savecontents = chunkContents(chunk);

    savecontents->staticinfo = NULL;
    savecontents->obj.collMyBlob = (blob*)BlobRegistryGetIDNoBlobOkay(derelict->obj.collMyBlob);       // planets aren't in blobs

    savecontents->shipOrDerelict.dockingship = (Ship*)SpaceObjRegistryGetID((SpaceObj *)derelict->shipOrDerelict.dockingship);
    savecontents->tractorbeam_playerowner = (Player*)SavePlayerToPlayerIndex(derelict->tractorbeam_playerowner);

    savecontents->newDockWithTransfer = (Ship*)SpaceObjRegistryGetID((SpaceObj *)derelict->newDockWithTransfer);

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(Derelict));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save

    memFree(chunk);

    if (derelict->guidance.clampInfo != NULL)
    {
        SaveClampInfo(derelict->guidance.clampInfo);
    }

}

Derelict *LoadDerelict(SaveChunk *chunk)
{
    Derelict *derelict;

    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(Derelict));  // CAT (M2-R6 oracle-mode-a): the exe's 0x294
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_SPACEOBJ|OBJ_DerelictType,sizeof(Derelict));

    derelict = memAlloc(sizeof(Derelict),"Derelict",NonVolatile);

    memcpy(derelict,chunkContents(chunk),chunk->contentsSize);

    memFree(chunk);

    dbgAssertOrIgnore(derelict->derelicttype < NUM_DERELICTTYPES);
    derelict->staticinfo = (DerelictStaticInfo *)&derelictStaticInfos[derelict->derelicttype];

    InitializeNavLights((Ship *)derelict);

    if (derelict->guidance.clampInfo != NULL)
    {
        derelict->guidance.clampInfo = LoadClampInfo();
    }


    return derelict;
}

void FixDerelict(Derelict *derelict)
{
    derelict->obj.collMyBlob = BlobRegistryGetBlob((sdword)derelict->obj.collMyBlob);

    derelict->shipOrDerelict.dockingship = SpaceObjRegistryGetShip((sdword)derelict->shipOrDerelict.dockingship);
    derelict->tractorbeam_playerowner = SavePlayerIndexToPlayer(derelict->tractorbeam_playerowner);

    derelict->newDockWithTransfer = SpaceObjRegistryGetShip((sdword)derelict->newDockWithTransfer);

    if (derelict->guidance.clampInfo)
    {
        FixClampInfo(derelict->guidance.clampInfo);
    }

}

void SaveMissile(Missile *missile)
{
    Missile *savecontents;
    SaveChunk *chunk;
    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_SPACEOBJ|OBJ_MissileType,sizeof(Missile),missile);

    savecontents = chunkContents(chunk);

    savecontents->staticinfo = NULL;
    savecontents->obj.collMyBlob = (blob*)BlobRegistryGetID(missile->obj.collMyBlob);

    savecontents->trail = NULL;

    savecontents->playerowner = (Player*)SavePlayerToPlayerIndex(missile->playerowner);
    savecontents->owner = (Ship*)SpaceObjRegistryGetID((SpaceObj *)missile->owner);

    savecontents->target = (SpaceObjRotImpTarg*)SpaceObjRegistryGetID((SpaceObj *)missile->target);

    savecontents->hitEffect = (etglod*)saveEtglodGunEventToIndex(missile->hitEffect);

    savecontents->formationinfo = (MineFormationInfo*)ConvertPointerInListToNum(&universe.MineFormationList,savecontents->formationinfo);

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(Missile));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save

    memFree(chunk);

    if (missile->guidance.clampInfo != NULL)
    {
        SaveClampInfo(missile->guidance.clampInfo);
    }
}

Missile *LoadMissile(SaveChunk *chunk)
{
    Missile *missile;

    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(Missile));   // CAT (M2-R6 oracle-mode-a): the exe's 0x258
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_SPACEOBJ|OBJ_MissileType,sizeof(Missile));

    missile = memAlloc(sizeof(Missile),"Missile",0);

    memcpy(missile,chunkContents(chunk),chunk->contentsSize);

    memFree(chunk);

    if(missile->missileType == MISSILE_Regular)
    {
        missile->staticinfo = &missileStaticInfos[missile->race];
    }
    else
    {
        dbgAssertOrIgnore(missile->missileType == MISSILE_Mine);
        missile->staticinfo = &mineStaticInfos[missile->race];
    }

    if (missile->guidance.clampInfo != NULL)
    {
        missile->guidance.clampInfo = LoadClampInfo();
    }

    return missile;
}

void FixMissile(Missile *missile)
{
    missile->obj.collMyBlob = BlobRegistryGetBlob((sdword)missile->obj.collMyBlob);

    missile->playerowner = SavePlayerIndexToPlayer(missile->playerowner);

    missile->owner = SpaceObjRegistryGetShip((sdword)missile->owner);
    missile->target = SpaceObjRegistryGetTarget((sdword)missile->target);

    missile->hitEffect = saveIndexToEtglodGunEvent((sdword)missile->hitEffect);

    missile->formationinfo = ConvertNumToPointerInList(&universe.MineFormationList,(sdword)missile->formationinfo);

    if (missile->guidance.clampInfo)
    {
        FixClampInfo(missile->guidance.clampInfo);
    }
}

void SaveSpaceObj(SpaceObj *obj)
{
    switch (obj->header.objtype)
    {
        case OBJ_ShipType:
            SaveShip((Ship *)obj);
            break;

        case OBJ_BulletType:
            SaveBullet((Bullet *)obj);
            break;

        case OBJ_AsteroidType:
            SaveAsteroid((Asteroid *)obj);
            break;

        case OBJ_NebulaType:
            SaveNebula((Nebula *)obj);
            break;

        case OBJ_GasType:
            dbgFatal(DBG_Loc,"Saving Gas clouds not supported yet");
            break;

        case OBJ_DustType:
            SaveDustCloud((DustCloud *)obj);
            break;

        case OBJ_DerelictType:
            SaveDerelict((Derelict *)obj);
            break;

        case OBJ_EffectType:
            dbgAssertOrIgnore(FALSE);
            break;

        case OBJ_MissileType:
            SaveMissile((Missile *)obj);
            break;

        default:
            dbgAssertOrIgnore(FALSE);
            break;
    }
}

void FixSpaceObj(SpaceObj *obj)
{
    switch (obj->header.objtype)
    {
        case OBJ_ShipType:
            FixShip((Ship *)obj);
            break;

        case OBJ_BulletType:
            FixBullet((Bullet *)obj);
            break;

        case OBJ_AsteroidType:
            FixAsteroid((Asteroid *)obj);
            break;

        case OBJ_NebulaType:
            FixNebula((Nebula *)obj);
            break;

        case OBJ_GasType:
            dbgFatal(DBG_Loc,"Saving Gas clouds not supported yet");
            break;

        case OBJ_DustType:
            FixDustCloud((DustCloud *)obj);
            break;

        case OBJ_DerelictType:
            FixDerelict((Derelict *)obj);
            break;

        case OBJ_EffectType:
            dbgAssertOrIgnore(FALSE);
            break;

        case OBJ_MissileType:
            FixMissile((Missile *)obj);
            break;

        default:
            dbgAssertOrIgnore(FALSE);
            break;
    }

    if (obj->header.flags & SOF_Targetable)
    {
        ((SpaceObjRotImpTarg *)obj)->flashtimer = 0.0f;
    }
}

static SpaceObj *LoadSpaceObjByType(SaveChunk *chunk)
{
    switch (chunk->type & SAVE_OBJTYPEMASK)
    {
        case OBJ_ShipType:
            return (SpaceObj *)LoadShip(chunk);

        case OBJ_BulletType:
            return (SpaceObj *)LoadBullet(chunk);

        case OBJ_AsteroidType:
            return (SpaceObj *)LoadAsteroid(chunk);

        case OBJ_NebulaType:
            return (SpaceObj *)LoadNebula(chunk);

        case OBJ_GasType:
            dbgFatal(DBG_Loc,"Saving Gas clouds not supported yet");
            break;

        case OBJ_DustType:
            return (SpaceObj *)LoadDustCloud(chunk);

        case OBJ_DerelictType:
            return (SpaceObj *)LoadDerelict(chunk);

        case OBJ_EffectType:
            dbgAssertOrIgnore(FALSE);
            return NULL;

        case OBJ_MissileType:
            return (SpaceObj *)LoadMissile(chunk);

        default:
            dbgAssertOrIgnore(FALSE);
            return NULL;
    }

	return NULL;
}

SpaceObj *LoadSpaceObj(void)
{
    SaveChunk *chunk;
    SpaceObj *obj;

    chunk = LoadNextChunk();
    dbgAssertOrIgnore(chunk->type & SAVE_SPACEOBJ);

    obj = LoadSpaceObjByType(chunk);
    if (obj != NULL)
    {
        // CAT: a loaded object has no per-vertex mesh state and no effect block (LoadSpaceObj 0x00501660 clears
        //      +0x84 and +0x80 of every object it loads; the saved pointers are stale)
        obj->catEffectBlock = NULL;
        obj->catMeshState = NULL;
    }
    return obj;
}

typedef struct SaveLLSpaceObj {
    sdword num;
    sdword id[1];
} SaveLLSpaceObj;

#define sizeofSaveLLSpaceObj(n) (sizeof(SaveLLSpaceObj) + (n-1)*sizeof(sdword))

void SaveLinkedListOfSpaceObjs(LinkedList *list)
{
    sdword num = list->num;
    SaveChunk *chunk;
    SaveLLSpaceObj *savecontents;
    Node *node = list->head;
    SpaceObj *obj;
    sdword cur = 0;

    chunk = CreateChunk(LINKEDLISTSPACEOBJS,sizeofSaveLLSpaceObj(num),NULL);

    savecontents = chunkContents(chunk);
    savecontents->num = num;

    while (node != NULL)
    {
        obj = (SpaceObj *)listGetStructOfNode(node);
        dbgAssertOrIgnore(obj);

        savecontents->id[cur++] = SpaceObjRegistryGetID(obj);

        node = node->next;
    }

    dbgAssertOrIgnore(cur == num);

    SaveThisChunk(chunk);

    memFree(chunk);
}

void JustLoadLinkedListOfSpaceObjs(LinkedList *list)
{
    SaveChunk *chunk;
    SaveLLSpaceObj *loadcontents;
    SaveLLSpaceObj *returncontents;

    chunk = LoadNextChunk();
    VerifyChunkNoSize(chunk,LINKEDLISTSPACEOBJS);

    loadcontents = (struct SaveLLSpaceObj *)chunkContents(chunk);

    dbgAssertOrIgnore(sizeofSaveLLSpaceObj(loadcontents->num) == chunk->contentsSize);

    returncontents = memAlloc(chunk->contentsSize,"unfixedlistobj",0);
    memcpy(returncontents,loadcontents,chunk->contentsSize);
    memFree(chunk);

    list->head = (void *)returncontents;
}

void FixLinkedListOfSpaceObjs(LinkedList *list,ListAddCB listAddCB)
{
    SaveLLSpaceObj *objs;
    SpaceObj *obj;
    sdword num;
    sdword cur = 0;

    objs = (SaveLLSpaceObj *)list->head;
    dbgAssertOrIgnore(objs);

    listInit(list);

    num = objs->num;

    while (cur < num)
    {
        obj = SpaceObjRegistryGetObj(objs->id[cur++]);
        if (obj != NULL)
        {
            listAddCB(list,obj);
        }
    }

    memFree(objs);
}

void LoadLinkedListOfSpaceObjs(LinkedList *list,ListAddCB listAddCB)
{
    SaveChunk *chunk;
    SaveLLSpaceObj *loadcontents;
    sdword num;
    sdword cur = 0;
    SpaceObj *obj;

    chunk = LoadNextChunk();
    VerifyChunkNoSize(chunk,LINKEDLISTSPACEOBJS);

    loadcontents = (struct SaveLLSpaceObj *)chunkContents(chunk);
    num = loadcontents->num;

    dbgAssertOrIgnore(sizeofSaveLLSpaceObj(num) == chunk->contentsSize);

    listInit(list);

    while (cur < num)
    {
        obj = SpaceObjRegistryGetObj(loadcontents->id[cur++]);
        if (obj != NULL)
        {
            listAddCB(list,obj);
        }
    }

    memFree(chunk);
}

void RenderListAddCB(LinkedList *list,SpaceObj *obj)
{
    listAddNode(list,&obj->renderlink,obj);
}

void SpaceObjListAddCB(LinkedList *list,SpaceObj *obj)
{
    listAddNode(list,&obj->header.objlink,obj);
}

#define MinorRenderListAddCB RenderListAddCB
#define MinorSpaceObjListAddCB SpaceObjListAddCB

void ShipListAddCB(LinkedList *list,SpaceObj *obj)
{
#define ship ((Ship *)obj)
    dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);
    listAddNode(list,&ship->shiplink,obj);
#undef ship
}

void BulletListAddCB(LinkedList *list,SpaceObj *obj)
{
#define bullet ((Bullet *)obj)
    dbgAssertOrIgnore(bullet->header.objtype == OBJ_BulletType);
    listAddNode(list,&bullet->bulletlink,obj);
#undef bullet
}

void ResourceListAddCB(LinkedList *list,SpaceObj *obj)
{
#define resource ((Resource *)obj)
    dbgAssertOrIgnore(resource->header.flags & SOF_Resource);
    listAddNode(list,&resource->resourcelink,obj);
#undef resource
}

void DerelictListAddCB(LinkedList *list,SpaceObj *obj)
{
#define derelict ((Derelict *)obj)
    dbgAssertOrIgnore(derelict->header.objtype == OBJ_DerelictType);
    listAddNode(list,&derelict->derelictlink,obj);
#undef derelict
}

void ImpactableListAddCB(LinkedList *list,SpaceObj *obj)
{
#define impactable ((SpaceObjRotImp *)obj)
    dbgAssertOrIgnore(impactable->header.flags & SOF_Impactable);
    listAddNode(list,&impactable->impactablelink,obj);
#undef impactable
}

void MissileListAddCB(LinkedList *list,SpaceObj *obj)
{
#define missile ((Missile *)obj)
    dbgAssertOrIgnore(missile->header.objtype == OBJ_MissileType);
    listAddNode(list,&missile->missilelink,obj);
#undef missile
}

void MineAddToMineListCB(LinkedList *list,SpaceObj *obj)
{
#define mine ((Missile *)obj)
    dbgAssertOrIgnore(mine->header.objtype == OBJ_MissileType);
    listAddNode(list,&mine->formationLink,obj);
#undef mine
}

typedef struct SaveSelSpaceObj {
    sdword num;
    sdword id[1];
} SaveSelSpaceObj;

#define sizeofSaveSelSpaceObj(n) (sizeof(SaveSelSpaceObj) + (n-1)*sizeof(sdword))


void SaveSelection(SpaceObjSelection *selection)
{
    sdword num = selection->numSpaceObjs;
    SaveChunk *chunk;
    SaveSelSpaceObj *savecontents;
    sdword i;

    chunk = CreateChunk(SELECTION,sizeofSaveSelSpaceObj(num),NULL);

    savecontents = chunkContents(chunk);
    savecontents->num = num;

    for (i=0;i<num;i++)
    {
        dbgAssertOrIgnore(selection->SpaceObjPtr[i] != NULL);
        savecontents->id[i] = SpaceObjRegistryGetID(selection->SpaceObjPtr[i]);
        dbgAssertOrIgnore(savecontents->id[i] != -1);
    }

    SaveThisChunk(chunk);
    memFree(chunk);
}

void SaveMaxSelection(MaxSelection *maxselection)
{
    SaveSelection((SpaceObjSelection *)maxselection);
}

void SaveGrowSelection(GrowSelection *grow)
{
    SaveSelection((SpaceObjSelection *)grow->selection);
}

SpaceObjSelection *LoadSelection(void)
{
    SpaceObjSelection *selection;
    SaveChunk *chunk;
    SaveSelSpaceObj *loadcontents;
    sdword num;
    sdword i;

    chunk = LoadNextChunk();
    VerifyChunkNoSize(chunk,SELECTION);

    loadcontents = (struct SaveSelSpaceObj *)chunkContents(chunk);
    num = loadcontents->num;

    dbgAssertOrIgnore(sizeofSaveSelSpaceObj(num) == chunk->contentsSize);

    selection = memAlloc(sizeofSelectCommand(num),"loadselect",0);
    selection->numSpaceObjs = num;

    for (i=0;i<num;i++)
    {
        dbgAssertOrIgnore(loadcontents->id[i] != -1);
        selection->SpaceObjPtr[i] = (SpaceObjPtr)loadcontents->id[i];
    }

    memFree(chunk);

    return selection;
}

void LoadGrowSelection(GrowSelection *grow)
{
    grow->selection = (SelectCommand *)LoadSelection();
    grow->maxNumShips = grow->selection->numShips;
}

void FixSelection(SpaceObjSelection *selection)
{
    sdword num = selection->numSpaceObjs;
    sdword i;

    for (i=0;i<num;i++)
    {
        selection->SpaceObjPtr[i] = SpaceObjRegistryGetObj((sdword)selection->SpaceObjPtr[i]);
        dbgAssertOrIgnore(selection->SpaceObjPtr[i]);
    }
}

void FixGrowSelection(GrowSelection *grow)
{
    FixSelection((SpaceObjSelection *)grow->selection);
}

SpaceObjSelection *LoadSelectionAndFix(void)
{
    SpaceObjSelection *selection;
    SaveChunk *chunk;
    SaveSelSpaceObj *loadcontents;
    sdword num;
    sdword i;

    chunk = LoadNextChunk();
    VerifyChunkNoSize(chunk,SELECTION);

    loadcontents = (struct SaveSelSpaceObj *)chunkContents(chunk);
    num = loadcontents->num;

    dbgAssertOrIgnore(sizeofSaveSelSpaceObj(num) == chunk->contentsSize);

    selection = memAlloc(sizeofSelectCommand(num),"loadselect",0);
    selection->numSpaceObjs = num;

    for (i=0;i<num;i++)
    {
        selection->SpaceObjPtr[i] = SpaceObjRegistryGetObj(loadcontents->id[i]);
        dbgAssertOrIgnore(selection->SpaceObjPtr[i]);
    }

    memFree(chunk);

    return selection;
}

void LoadMaxSelectionAndFix(MaxSelection *maxselection)
{
    SelectCommand *selection = (SelectCommand *)LoadSelectionAndFix();
    memcpy(maxselection,selection,sizeofSelectCommand(selection->numShips));
    memFree(selection);
}

void LoadGrowSelectionAndFix(GrowSelection *grow)
{
    grow->selection = (SelectCommand *)LoadSelectionAndFix();
    grow->maxNumShips = grow->selection->numShips;
}

void SaveLinkedListOfStuff(LinkedList *list,SaveStuffInLinkedListCB savestuffCB)
{
    Node *node = list->head;

    SaveInfoNumber(list->num);

    while (node != NULL)
    {
        savestuffCB(listGetStructOfNode(node));

        node = node->next;
    }
}

void LoadLinkedListOfStuff(LinkedList *list,LoadStuffInLinkedListCB loadstuffCB)
{
    sdword num = LoadInfoNumber();
    sdword i;

    listInit(list);

    for (i=0;i<num;i++)
    {
        loadstuffCB(list);
    }

    dbgAssertOrIgnore(num == list->num);
}

void FixLinkedListOfStuff(LinkedList *list,FixStuffInLinkedListCB fixstuffCB)
{
    Node *node = list->head;

    while (node != NULL)
    {
        fixstuffCB(listGetStructOfNode(node));

        node = node->next;
    }
}

void SaveFormation(FormationCommand *formcommand)
{
    if (formcommand->formationSpecificInfo != NULL)
    {
        SaveStructureWithSize(formcommand->formationSpecificInfo);
    }
}

void LoadFormation(FormationCommand *formcommand)
{
    if (formcommand->formationSpecificInfo != NULL)
    {
        formcommand->formationSpecificInfo = LoadStructureWithSize();
    }
}

void SaveMilitarySlot(MilitarySlot *milslot)
{
    SaveChunk *chunk;
    MilitarySlot *savecontents;
    sdword i;

    chunk = CreateChunk(VARIABLE_STRUCTURE|SAVE_MILITARYSLOT,sizeofMilitarySlot(milslot->numSubslots),milslot);
    savecontents = (MilitarySlot *)chunkContents(chunk);

    for (i=0;i<savecontents->numSubslots;i++)
    {
        savecontents->subslots[i].ship = (ShipPtr)SpaceObjRegistryGetID((SpaceObj *)savecontents->subslots[i].ship);
    }

    saveCat32SaveArray(chunk, NULL, 8, (udword)offsetof(MilitarySlot, subslots), NULL, milslot->numSubslots);   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);
}

void SaveMilitaryParade(MilitaryParadeCommand *militaryParade)
{
    SaveChunk *chunk;
    MilitaryParadeCommand *savecontents;
    sdword i;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_MILITARYPARADE,sizeof(MilitaryParadeCommand),militaryParade);
    savecontents = (MilitaryParadeCommand *)chunkContents(chunk);

    savecontents->aroundShip = (Ship*)SpaceObjRegistryGetID((SpaceObj *)militaryParade->aroundShip);

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(MilitaryParadeCommand));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);

    for (i=0;i<MAX_MILITARY_SLOTS;i++)
    {
        if (militaryParade->militarySlots[i] != NULL)
        {
            SaveMilitarySlot(militaryParade->militarySlots[i]);
        }
    }
}

MilitarySlot *LoadMilitarySlot(void)
{
    SaveChunk *chunk;
    MilitarySlot *newmilslot;
    sdword i;
    sdword size;

    chunk = LoadNextChunk();
    // CAT (M2-R6 oracle-mode-a): the exe's two words + 4-byte subslot ship IDs (LoadMilitaryParade 0x005021d0)
    chunk = saveCat32ChunkArray(chunk, NULL, 8, (udword)offsetof(MilitarySlot, subslots), NULL, saveCat32ChunkCount(chunk, 4));
    VerifyChunkNoSize(chunk,VARIABLE_STRUCTURE|SAVE_MILITARYSLOT);
    size = sizeofMilitarySlot(((MilitarySlot *)chunkContents(chunk))->numSubslots);

    dbgAssertOrIgnore(size == chunk->contentsSize);

    newmilslot = memAlloc(size,"milslot",0);
    memcpy(newmilslot,chunkContents(chunk),size);

    memFree(chunk);

    for (i=0;i<newmilslot->numSubslots;i++)
    {
        newmilslot->subslots[i].ship = SpaceObjRegistryGetShip((sdword)newmilslot->subslots[i].ship);
    }

    return newmilslot;
}

MilitaryParadeCommand *LoadMilitaryParade(void)
{
    SaveChunk *chunk;
    MilitaryParadeCommand *newmilparade;
    sdword i;

    chunk = LoadNextChunk();
    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(MilitaryParadeCommand));  // CAT (M2-R6 oracle-mode-a): 0x104
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_MILITARYPARADE,sizeof(MilitaryParadeCommand));

    newmilparade = memAlloc(sizeof(MilitaryParadeCommand),"milparadecom",0);
    memcpy(newmilparade,chunkContents(chunk),sizeof(MilitaryParadeCommand));

    memFree(chunk);

    newmilparade->aroundShip = SpaceObjRegistryGetShip((sdword)newmilparade->aroundShip);

    for (i=0;i<MAX_MILITARY_SLOTS;i++)
    {
        if (newmilparade->militarySlots[i] != NULL)
        {
            newmilparade->militarySlots[i] = LoadMilitarySlot();
        }
    }

    return newmilparade;
}

void Save_CommandToDo(CommandToDo *command)
{
    SaveChunk *chunk;
    CommandToDo *savecommand;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_COMMANDTODO,sizeof(CommandToDo),command);
    savecommand = (CommandToDo *)chunkContents(chunk);

    // fixing
    switch (command->ordertype.order)
    {
        case COMMAND_MOVE:
            // none needed
            break;

        case COMMAND_ATTACK:
            // save
            break;

        case COMMAND_DOCK:
            // none needed;
            break;

        case COMMAND_LAUNCH_SHIP:
            // fixing
            savecommand->launchship.receiverShip = (ShipPtr)SpaceObjRegistryGetID((SpaceObj *)command->launchship.receiverShip);
            break;

        case COMMAND_COLLECT_RESOURCES:
            // fixing
            savecommand->collect.resource = (ResourcePtr)SpaceObjRegistryGetID((SpaceObj *)command->collect.resource);
            break;

        case COMMAND_BUILDING_SHIP:
            // fixing
            savecommand->buildingship.creator = (ShipPtr)SpaceObjRegistryGetID((SpaceObj *)command->buildingship.creator);
            break;

        case COMMAND_SPECIAL:
            // save
            break;

        case COMMAND_MILITARY_PARADE:
            // save
            break;
    }

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(CommandToDo));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save

    dbgAssertOrIgnore(command->selection);
    SaveSelection((SpaceObjSelection *)command->selection);

    if (command->ordertype.attributes & COMMAND_MASK_FORMATION)
    {
        SaveFormation(&command->formation);
    }

    if (command->ordertype.attributes & COMMAND_MASK_PROTECTING)
    {
        dbgAssertOrIgnore(command->protect);
        SaveSelection((SpaceObjSelection *)command->protect);
    }

    if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        dbgAssertOrIgnore(command->ordertype.order != COMMAND_ATTACK);
        dbgAssertOrIgnore(command->attack);
        SaveSelection((SpaceObjSelection *)command->attack);
    }

    // saving
    switch (command->ordertype.order)
    {
        case COMMAND_MOVE:
            // none needed
            break;

        case COMMAND_ATTACK:
            // save
            dbgAssertOrIgnore((command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING) == 0);
            dbgAssertOrIgnore(command->attack);
            SaveSelection((SpaceObjSelection *)command->attack);
            break;

        case COMMAND_DOCK:
            // none needed;
            break;

        case COMMAND_LAUNCH_SHIP:
            // fixing
            break;

        case COMMAND_COLLECT_RESOURCES:
            // fixing
            break;

        case COMMAND_BUILDING_SHIP:
            // fixing
            break;

        case COMMAND_SPECIAL:
            // save
            if (command->specialtargets != NULL)
            {
                SaveSelection((SpaceObjSelection *)command->specialtargets);
            }
            break;

        case COMMAND_MILITARY_PARADE:
            // save
            dbgAssertOrIgnore(command->militaryParade);
            SaveMilitaryParade(command->militaryParade);
            break;
    }

    memFree(chunk);
}

void Load_CommandToDo(LinkedList *list)
{
    SaveChunk *chunk;
    CommandToDo *loadedcommand;
    CommandToDo *command;

    chunk = LoadNextChunk();
    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(CommandToDo));    // CAT (M2-R6 oracle-mode-a): the exe's 0x330
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_COMMANDTODO,sizeof(CommandToDo));

    loadedcommand = (CommandToDo *)chunkContents(chunk);

    command = memAlloc(sizeof(CommandToDo),"ToDo",NonVolatile);
    memcpy(command,loadedcommand,sizeof(CommandToDo));

    memFree(chunk);

    // fixing
    switch (command->ordertype.order)
    {
        case COMMAND_MOVE:
            // none needed
            break;

        case COMMAND_ATTACK:
            // load
            break;

        case COMMAND_DOCK:
            // none needed;
            break;

        case COMMAND_LAUNCH_SHIP:
            // fixing
            command->launchship.receiverShip = SpaceObjRegistryGetShip((sdword)command->launchship.receiverShip);
            break;

        case COMMAND_COLLECT_RESOURCES:
            // fixing
            command->collect.resource = SpaceObjRegistryGetResource((sdword)command->collect.resource);
            break;

        case COMMAND_BUILDING_SHIP:
            // fixing
            command->buildingship.creator = SpaceObjRegistryGetShip((sdword)command->buildingship.creator);
            break;

        case COMMAND_SPECIAL:
            // load
            break;

        case COMMAND_MILITARY_PARADE:
            // load
            break;
    }

    command->selection = (SelectCommand *)LoadSelectionAndFix();

    if (command->ordertype.attributes & COMMAND_MASK_FORMATION)
    {
        LoadFormation(&command->formation);
    }

    if (command->ordertype.attributes & COMMAND_MASK_PROTECTING)
    {
        command->protect = (ProtectCommand *)LoadSelectionAndFix();
    }

    if (command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING)
    {
        dbgAssertOrIgnore(command->ordertype.order != COMMAND_ATTACK);
        command->attack = (AttackCommand *)LoadSelectionAndFix();
    }

    // loading
    switch (command->ordertype.order)
    {
        case COMMAND_MOVE:
            // none needed
            break;

        case COMMAND_ATTACK:
            // load
            dbgAssertOrIgnore((command->ordertype.attributes & COMMAND_MASK_PASSIVE_ATTACKING) == 0);
            command->attack = (AttackCommand *)LoadSelectionAndFix();
            break;

        case COMMAND_DOCK:
            // none needed;
            break;

        case COMMAND_LAUNCH_SHIP:
            // fixing
            break;

        case COMMAND_COLLECT_RESOURCES:
            // fixing
            break;

        case COMMAND_BUILDING_SHIP:
            // fixing
            break;

        case COMMAND_SPECIAL:
            // load
            if (command->specialtargets != NULL)
            {
                command->specialtargets = (SpecialCommand *)LoadSelectionAndFix();
            }
            break;

        case COMMAND_MILITARY_PARADE:
            // load
            dbgAssertOrIgnore(command->militaryParade);
            command->militaryParade = LoadMilitaryParade();
            break;
    }

    listAddNode(list,&command->todonode,command);
}


void Save_CommandLayer(CommandLayer *comlayer)
{
    sdword num = comlayer->todolist.num;
    sdword cur = 0;
    InfoChunkContents infocontents;
    Node *node = comlayer->todolist.head;
    CommandToDo *command;
    SaveChunk *chunk;

    infocontents.info = num;

    SaveThisChunk(chunk = CreateChunk(INFO_CHUNK,sizeof(InfoChunkContents),&infocontents));

    while (node != NULL)
    {
        command = (CommandToDo *)listGetStructOfNode(node);

        Save_CommandToDo(command);
        cur++;

        node = node->next;
    }

    dbgAssertOrIgnore(cur == num);

    memFree(chunk);
}

void Load_CommandLayer(CommandLayer *comlayer)
{
    SaveChunk *infochunk;
    sdword num;
    sdword i;

    infochunk = LoadNextChunk();
    VerifyChunk(infochunk,INFO_CHUNK,sizeof(InfoChunkContents));

    num = ((InfoChunkContents *)chunkContents(infochunk))->info;

    memFree(infochunk);

    listInit(&comlayer->todolist);
    for (i=0;i<num;i++)
    {
        Load_CommandToDo(&comlayer->todolist);
    }
}

void Save_CameraStackEntry(CameraStackEntry *currententry)
{
    SaveChunk *chunk;
    CameraStackEntry *savecse;
    sdword numShips = currententry->focus.numShips;
    sdword i;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_CAMERASTACKENTRY,sizeofCameraStackEntry(numShips),currententry);
    savecse = (CameraStackEntry *)chunkContents(chunk);

    // fix savecse

    savecse->remembercam.playerowner = (Player *)SavePlayerToPlayerIndex(savecse->remembercam.playerowner);

    for (i=0;i<numShips;i++)
    {
        savecse->focus.ShipPtr[i] = (ShipPtr)SpaceObjRegistryGetID((SpaceObj *)savecse->focus.ShipPtr[i]);
    }

    saveCat32SaveArray(chunk, SAVECAT32_LAYOUT(CameraStackEntry), 0x74, (udword)offsetof(CameraStackEntry, focus.ShipPtr), NULL, numShips);   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);
}

void Load_CameraStackEntry(LinkedList *list)
{
    SaveChunk *chunk;
    CameraStackEntry *loadedcse;
    CameraStackEntry *cse;
    sdword numShips;
    sdword i;

    chunk = LoadNextChunk();
    // CAT (M2-R6 oracle-mode-a): the exe's entry is 0x74 bytes up to the focus ships, their count at +0x6C
    //      (Load_CameraStackEntry 0x00502910), then a 4-byte ID per ship
    chunk = saveCat32ChunkArray(chunk, SAVECAT32_LAYOUT(CameraStackEntry), 0x74, (udword)offsetof(CameraStackEntry, focus.ShipPtr),
                                NULL, saveCat32ChunkCount(chunk, 0x6C));
    VerifyChunkNoSize(chunk,BASIC_STRUCTURE|SAVE_CAMERASTACKENTRY);

    loadedcse = (CameraStackEntry *)chunkContents(chunk);
    numShips = loadedcse->focus.numShips;

    dbgAssertOrIgnore(sizeofCameraStackEntry(numShips) == chunk->contentsSize);

    cse = memAlloc(chunk->contentsSize,"CamStackEntry",NonVolatile);
    memcpy(cse,loadedcse,chunk->contentsSize);

    memFree(chunk);

    // fix cse

    cse->remembercam.playerowner = SavePlayerIndexToPlayer(cse->remembercam.playerowner);

    for (i=0;i<numShips;i++)
    {
        cse->focus.ShipPtr[i] = (Ship *)SpaceObjRegistryGetObj((sdword)cse->focus.ShipPtr[i]);  // don't use SpaceObjRegistryGetShip because it might not actually be a ship
    }

    listAddNode(list,&cse->stacklink,cse);
}

sdword ConvertPointerInListToNum(LinkedList *list,void *entry)
{
    Node *node = list->head;
    sdword cur = -1;

    if (entry == NULL)
    {
        return -1;
    }

    while (node != NULL)
    {
        cur++;
        if (entry == listGetStructOfNode(node))
        {
            return cur;
        }

        node = node->next;
    }

    return -1;
}

void *ConvertNumToPointerInList(LinkedList *list,sdword num)
{
    Node *node;
    sdword cur = 0;

    if (num == -1)
    {
        return NULL;
    }

    dbgAssertOrIgnore(num < list->num);
    node = list->head;

    while (cur < num)
    {
        node = node->next;
        cur++;
    }
    dbgAssertOrIgnore(cur == num);

    return listGetStructOfNode(node);
}

void Save_CameraCommand(CameraCommand *cameracommand)
{
    SaveChunk *chunk;
    CameraCommand *savecc;
    sdword num = cameracommand->camerastack.num;
    sdword cur = 0;
    InfoChunkContents infocontents;
    Node *node = cameracommand->camerastack.head;
    CameraStackEntry *currententry;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_CAMERACOMMAND,sizeof(CameraCommand),cameracommand);
    savecc = (CameraCommand *)chunkContents(chunk);

    // fix savecc
    savecc->currentCameraStack = (CameraStackEntry*)ConvertPointerInListToNum(&savecc->camerastack,savecc->currentCameraStack);
    savecc->actualcamera.playerowner = (Player*)SavePlayerToPlayerIndex(savecc->actualcamera.playerowner);

    savecc->dontFocusOnMe = (CameraStackEntry *)ConvertPointerInListToNum(&savecc->camerastack,savecc->dontFocusOnMe);

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(CameraCommand));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save

    infocontents.info = num;
    SaveThisChunk(CreateChunk(INFO_CHUNK,sizeof(InfoChunkContents),&infocontents));

    while (node != NULL)
    {
        currententry = (CameraStackEntry *)listGetStructOfNode(node);

        Save_CameraStackEntry(currententry);
        cur++;

        node = node->next;
    }

    dbgAssertOrIgnore(cur == num);

    memFree(chunk);
}

void Load_CameraCommand(CameraCommand *cameracommand)
{
    SaveChunk *chunk;
    CameraCommand *loadcc;
    SaveChunk *infochunk;
    sdword num;
    sdword i;

    chunk = LoadNextChunk();
    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(CameraCommand));  // CAT (M2-R6 oracle-mode-a): the exe's 0x94
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_CAMERACOMMAND,sizeof(CameraCommand));

    loadcc = (CameraCommand *)chunkContents(chunk);
    memcpy(cameracommand,loadcc,sizeof(CameraCommand));

    memFree(chunk);

    infochunk = LoadNextChunk();
    VerifyChunk(infochunk,INFO_CHUNK,sizeof(InfoChunkContents));

    num = ((InfoChunkContents *)chunkContents(infochunk))->info;

    memFree(infochunk);

    listInit(&cameracommand->camerastack);
    for (i=0;i<num;i++)
    {
        Load_CameraStackEntry(&cameracommand->camerastack);
    }

    // fix cameracommand

    cameracommand->currentCameraStack = ConvertNumToPointerInList(&cameracommand->camerastack,(sdword)cameracommand->currentCameraStack);
    cameracommand->actualcamera.playerowner = SavePlayerIndexToPlayer(cameracommand->actualcamera.playerowner);

    cameracommand->dontFocusOnMe = ConvertNumToPointerInList(&cameracommand->camerastack,(sdword)cameracommand->dontFocusOnMe);
}

void SaveLinkedListOfInsideShips(LinkedList *list)
{
    sdword num = list->num;
    SaveChunk *chunk;
    SaveLLSpaceObj *savecontents;
    Node *node = list->head;
    Ship *ship;
    sdword cur = 0;

    chunk = CreateChunk(LINKEDLISTSPACEOBJS,sizeofSaveLLSpaceObj(num),NULL);

    savecontents = chunkContents(chunk);
    savecontents->num = num;

    while (node != NULL)
    {
        ship = ((InsideShip *)listGetStructOfNode(node))->ship;
        dbgAssertOrIgnore(ship->header.objtype == OBJ_ShipType);

        savecontents->id[cur] = SpaceObjRegistryGetID((SpaceObj *)ship);
        dbgAssertOrIgnore(savecontents->id[cur] != -1);
        cur++;

        node = node->next;
    }

    dbgAssertOrIgnore(cur == num);

    SaveThisChunk(chunk);

    memFree(chunk);
}

void LoadLinkedListOfInsideShips(LinkedList *list)
{
    SaveChunk *chunk;
    SaveLLSpaceObj *loadcontents;
    sdword num;
    sdword cur = 0;
    InsideShip *insideShip;

    chunk = LoadNextChunk();
    VerifyChunkNoSize(chunk,LINKEDLISTSPACEOBJS);

    loadcontents = (struct SaveLLSpaceObj *)chunkContents(chunk);
    num = loadcontents->num;

    dbgAssertOrIgnore(sizeofSaveLLSpaceObj(num) == chunk->contentsSize);

    listInit(list);

    while (cur < num)
    {
        insideShip = memAlloc(sizeof(InsideShip),"InsideShip",0);
        insideShip->ship = (Ship*)loadcontents->id[cur++];
        if (insideShip->ship != (Ship*)-1)
        {
            listAddNode(list,&insideShip->node,insideShip);
        }
    }

    memFree(chunk);
}

void FixLinkedListOfInsideShips(LinkedList *list)
{
    Node *node = list->head;
    InsideShip *insideShip;

    while (node != NULL)
    {
        insideShip = (InsideShip *)listGetStructOfNode(node);
        insideShip->ship = SpaceObjRegistryGetShip((sdword)insideShip->ship);
        dbgAssertOrIgnore(insideShip->ship != NULL);
        dbgAssertOrIgnore(insideShip->ship->header.objtype == OBJ_ShipType);
        node = node->next;
    }
}

void SaveLinkedListOfResearchTopics(LinkedList *list)
{
    InfoChunkContents infocontents;
    SaveChunk *chunk;
    Node *node = list->head;
    sdword cur = 0;

    infocontents.info = list->num;

    SaveThisChunk(chunk = CreateChunk(INFO_CHUNK,sizeof(InfoChunkContents),&infocontents));
    memFree(chunk);

    while (node != NULL)
    {
        cur++;
        chunk = CreateChunk(BASIC_STRUCTURE|SAVE_RESEARCHTOPIC,sizeof(ResearchTopic),listGetStructOfNode(node));
        saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(ResearchTopic));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
        memFree(chunk);

        node = node->next;
    }

    dbgAssertOrIgnore(cur == list->num);
}

void LoadLinkedListOfResearchTopics(LinkedList *list)
{
    SaveChunk *chunk;
    sdword num;
    sdword i;
    ResearchTopic *newtopic;

    listInit(list);

    chunk = LoadNextChunk();
    VerifyChunk(chunk,INFO_CHUNK,sizeof(InfoChunkContents));

    num = ((InfoChunkContents *)chunkContents(chunk))->info;

    memFree(chunk);

    for (i=0;i<num;i++)
    {
        chunk = LoadNextChunk();
        chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(ResearchTopic));  // CAT (M2-R6 oracle-mode-a): the exe's 0x18
        VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_RESEARCHTOPIC,sizeof(ResearchTopic));
        newtopic = (ResearchTopic *)memAlloc(sizeof(ResearchTopic),"ResearchTopic",NonVolatile);
        memcpy(newtopic,chunkContents(chunk),sizeof(ResearchTopic));
        listAddNode(list, &newtopic->link, newtopic);
        memFree(chunk);
    }
}

void Prefix_PlayerResearchInfo(PlayerResearchInfo *researchinfo)
{
    sdword i;

    for (i=0;i<NUM_RESEARCHLABS;i++)
    {
        if (researchinfo->researchlabs[i].labstatus == LS_RESEARCHITEM)
        {
            researchinfo->researchlabs[i].topic = (ResearchTopic *)ConvertPointerInListToNum(&researchinfo->listoftopics,researchinfo->researchlabs[i].topic);
            dbgAssertOrIgnore(researchinfo->researchlabs[i].topic != (ResearchTopic *) -1);
        }
        else
        {
            researchinfo->researchlabs[i].topic = (ResearchTopic *) -1;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_saveSavePlayerFOW / cat_saveLoadPlayerFOW
    Description : CAT (M2-R6 nis-fow-camera): the local player's fog-of-war grid in the save (cat_saveSavePlayerFOW
                  0x004ff6b0 in SaveUniverse's player loop after the research info, cat_saveLoadPlayerFOW 0x004ff7e0 in
                  LoadUniverse's): two BASIC chunks, the grid (sub-type 0x2000, the exe's 0x20-byte struct: the cell
                  pointer, nx, ny, nz, origin, cell size) and its cells (0x3000, 0x20 bytes each). Loading selects the
                  grid and rebuilds the boundary with every cell dirty (cat_fowBoundaryCreate(TRUE)).
                  While a recorded game plays back (0x00a75194, recordplayPacketsInGame) the load reads nothing and clears
                  the local player's grid instead (M2-R6 oracle-mode-a).
                  CAT (M2-R9 recorder-leftovers): the save writes nothing while a game recording starts
                  (recordingStartSave 0x00a578f8, set by recPackInGameStartCBSafeToStart 0x004cbbb0 around its
                  SaveGame; cat_saveSavePlayerFOW 0x004ff6b0 tests it first), so a recording's save has no grid
                  chunks, which is what the playback's load expects
----------------------------------------------------------------------------*/
typedef struct CatFOWGridSave
{
    udword cells;           // the exe's pointer (unused on load)
    sdword nx, ny, nz;
    vector origin;
    real32 cellSize;
} CatFOWGridSave;
_Static_assert(sizeof(CatFOWGridSave) == 0x20, "the saved FOW grid is the exe's 0x20-byte struct");

static void SaveThisChunkOfType(TypeOfSaveChunk type, sdword size, void *contents)
{
    SaveChunk *chunk = CreateChunk(type, size, contents);

    SaveThisChunk(chunk);
    memFree(chunk);
}

void cat_saveSavePlayerFOW(Player *player)
{
    CatFOWGrid *grid = (CatFOWGrid *)player->catFOWGrid;
    CatFOWGridSave header;
    sdword size;

    if (recordingStartSave || (player->playerIndex != curPlayer))     // CAT: 0x00a578f8 first (0x004ff6b0)
    {
        return;
    }
    memset(&header, 0, sizeof(header));
    if (grid != NULL)
    {
        header.nx = grid->nx;
        header.ny = grid->ny;
        header.nz = grid->nz;
        header.origin = grid->origin;
        header.cellSize = grid->cellSize;
    }
    SaveThisChunkOfType(BASIC_STRUCTURE | SAVE_CAT_FOWGRID, sizeof(header), &header);
    // CAT-FIX: the exe reads the cells through the grid unchecked (the local player always has one there)
    size = (grid != NULL) ? cat_fowGridSize(grid->nx, grid->ny, grid->nz) : 0;
    SaveThisChunkOfType(BASIC_STRUCTURE | SAVE_CAT_FOWCELLS, size, (grid != NULL) ? grid->cells : NULL);
}

void cat_saveLoadPlayerFOW(Player *player)
{
    SaveChunk *chunk;
    CatFOWGridSave header;
    CatFOWGrid *grid;
    sdword size;

    if (recordplayPacketsInGame)
    {                                           // CAT: a playback starts without the grid (0x004ff7e0: 0x00a75194 set)
        universe.curPlayerPtr->catFOWGrid = NULL;
        return;
    }

    if (player->playerIndex != curPlayer)
    {
        return;
    }
    chunk = LoadNextChunk();
    VerifyChunk(chunk, BASIC_STRUCTURE | SAVE_CAT_FOWGRID, sizeof(CatFOWGridSave));
    memcpy(&header, chunkContents(chunk), sizeof(header));
    memFree(chunk);
    grid = memAlloc(sizeof(CatFOWGrid), "fowGrid", NonVolatile);
    grid->nx = header.nx;
    grid->ny = header.ny;
    grid->nz = header.nz;
    grid->origin = header.origin;
    grid->cellSize = header.cellSize;
    player->catFOWGrid = grid;

    chunk = LoadNextChunk();
    size = cat_fowGridSize(grid->nx, grid->ny, grid->nz);
    VerifyChunk(chunk, BASIC_STRUCTURE | SAVE_CAT_FOWCELLS, size);
    grid->cells = memAlloc((size > 0) ? size : (sdword)sizeof(CatFOWCell), "fowCells", NonVolatile);
    if (size > 0)
    {
        memcpy(grid->cells, chunkContents(chunk), size);
    }
    memFree(chunk);
    fowSelect(universe.curPlayerPtr->catFOWGrid);
    cat_fowBoundaryCreate(TRUE);
}

void Save_PlayerResearchInfo(PlayerResearchInfo *researchinfo)
{
    SaveLinkedListOfResearchTopics(&researchinfo->listoftopics);
}

void Load_PlayerResearchInfo(Player *player,PlayerResearchInfo *researchinfo)
{
    sdword i;

    rmInitializeResearchStatics(player);

    LoadLinkedListOfResearchTopics(&researchinfo->listoftopics);

    for (i=0;i<NUM_RESEARCHLABS;i++)
    {
        if (researchinfo->researchlabs[i].labstatus == LS_RESEARCHITEM)
        {
            researchinfo->researchlabs[i].topic = ConvertNumToPointerInList(&researchinfo->listoftopics,(sdword)researchinfo->researchlabs[i].topic);
            dbgAssertOrIgnore(researchinfo->researchlabs[i].topic != NULL);
        }
        else
        {
            researchinfo->researchlabs[i].topic = NULL;
        }
    }
    // CAT: then the player's upgrades from the loaded research (Load_PlayerResearchInfo 0x00502f20 ends with
    //      cat_playerUpdateUpgradesFromResearch 0x00514140)
    cat_playerUpdateUpgradesFromResearch(player);
}

void SaveMineFormationInfo(MineFormationInfo *mineFormationInfo)
{
    SaveChunk *chunk;
    MineFormationInfo *savecontents;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_MINEFORMATIONINFO,sizeof(MineFormationInfo),mineFormationInfo);
    savecontents = (MineFormationInfo *)chunkContents(chunk);

    savecontents->playerowner = (Player*)SavePlayerToPlayerIndex(savecontents->playerowner);
    savecontents->effect = NULL;

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(MineFormationInfo));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);

    SaveLinkedListOfSpaceObjs(&mineFormationInfo->MineList);
}

MineFormationInfo *LoadMineFormationInfo(void)
{
    SaveChunk *chunk;
    MineFormationInfo *mineFormationInfo;

    chunk = LoadNextChunk();
    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(MineFormationInfo));  // CAT (M2-R6 oracle-mode-a): the exe's 0x4C
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_MINEFORMATIONINFO,sizeof(MineFormationInfo));

    mineFormationInfo = memAlloc(sizeof(MineFormationInfo),"MineFormationInfo",NonVolatile);
    memcpy(mineFormationInfo,chunkContents(chunk),sizeof(MineFormationInfo));
    memFree(chunk);

    mineFormationInfo->playerowner = SavePlayerIndexToPlayer(mineFormationInfo->playerowner);

    LoadLinkedListOfSpaceObjs(&mineFormationInfo->MineList,MineAddToMineListCB);

    return mineFormationInfo;
}

void SaveMineFormationList(LinkedList *list)
{
    InfoChunkContents infocontents;
    SaveChunk *chunk;
    Node *node = list->head;
    sdword cur = 0;
    MineFormationInfo *mineFormationInfo;

    infocontents.info = list->num;

    SaveThisChunk(chunk = CreateChunk(INFO_CHUNK,sizeof(InfoChunkContents),&infocontents));
    memFree(chunk);

    while (node != NULL)
    {
        cur++;
        mineFormationInfo = (MineFormationInfo *)listGetStructOfNode(node);
        SaveMineFormationInfo(mineFormationInfo);
        node = node->next;
    }

    dbgAssertOrIgnore(cur == list->num);
}

void LoadMineFormationList(LinkedList *list)
{
    SaveChunk *chunk;
    sdword num;
    sdword i;
    MineFormationInfo *mineFormationInfo;

    listInit(list);

    chunk = LoadNextChunk();
    VerifyChunk(chunk,INFO_CHUNK,sizeof(InfoChunkContents));

    num = ((InfoChunkContents *)chunkContents(chunk))->info;

    memFree(chunk);

    for (i=0;i<num;i++)
    {
        mineFormationInfo = LoadMineFormationInfo();
        listAddNode(list, &mineFormationInfo->FormLink, mineFormationInfo);
    }
}

void SaveRetreatAtom(RetreatAtom *retreat)
{
    SaveChunk *chunk;
    RetreatAtom *savecontents;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_RETREATATOM,sizeof(RetreatAtom),retreat);
    savecontents = (RetreatAtom *)chunkContents(chunk);

    savecontents->retreater = (Ship*)SpaceObjRegistryGetID((SpaceObj *)savecontents->retreater);
    savecontents->fleeingfrom = (CommandToDo*)ConvertPointerInListToNum(&universe.mainCommandLayer.todolist,savecontents->fleeingfrom);

    SaveThisChunk(chunk);
    memFree(chunk);
}

void SaveRetreatList(LinkedList *list)
{
    sdword num = list->num;
    sdword cur = 0;
    Node *node = list->head;

    SaveInfoNumber(num);

    while (node != NULL)
    {
        cur++;
        SaveRetreatAtom((RetreatAtom *)listGetStructOfNode(node));

        node = node->next;
    }

    dbgAssertOrIgnore(cur == num);
}

RetreatAtom *LoadRetreatAtom(void)
{
    SaveChunk *chunk;
    RetreatAtom *retreat;

    chunk = LoadNextChunk();
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_RETREATATOM,sizeof(RetreatAtom));
    retreat = (RetreatAtom *)memAlloc(sizeof(RetreatAtom),"Retreat Data", NonVolatile);
    memcpy(retreat,chunkContents(chunk),sizeof(RetreatAtom));
    memFree(chunk);

    retreat->retreater = SpaceObjRegistryGetShip((sdword)retreat->retreater);
    retreat->fleeingfrom = ConvertNumToPointerInList(&universe.mainCommandLayer.todolist,(sdword)retreat->fleeingfrom);

    return retreat;
}

void LoadRetreatList(LinkedList *list)
{
    sdword num;
    sdword i;
    RetreatAtom *retreat;

    num = LoadInfoNumber();

    listInit(list);

    for (i=0;i<num;i++)
    {
        retreat = LoadRetreatAtom();
        listAddNode(list,&retreat->retreatLink,retreat);
    }
}

void SaveAttackAtom(AttackAtom *attackAtom)
{
    SaveChunk *chunk;
    AttackAtom *savecontents;
    sdword size;
    sdword i;
    sdword numShips;

    size = sizeof(AttackAtom) + sizeof(ShipPtr)*(attackAtom->attackerList.numShips-1);

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_ATTACKATOM,size,attackAtom);
    savecontents = (AttackAtom *)chunkContents(chunk);

    savecontents->retreater = (Ship*)SpaceObjRegistryGetID((SpaceObj *)savecontents->retreater);

    numShips = savecontents->attackerList.numShips;
    for (i=0;i<numShips;i++)
    {
        savecontents->attackerList.ShipPtr[i] = (ShipPtr)SpaceObjRegistryGetID((SpaceObj *)savecontents->attackerList.ShipPtr[i]);
    }

    saveCat32SaveArray(chunk, SAVECAT32_LAYOUT(AttackAtom), 0x24, (udword)offsetof(AttackAtom, attackerList.ShipPtr), NULL, numShips);   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save
    memFree(chunk);
}

void SaveAttackMemory(LinkedList *list)
{
    sdword num = list->num;
    sdword cur = 0;
    Node *node = list->head;

    SaveInfoNumber(num);

    while (node != NULL)
    {
        cur++;
        SaveAttackAtom((AttackAtom *)listGetStructOfNode(node));

        node = node->next;
    }

    dbgAssertOrIgnore(cur == num);
}

AttackAtom *LoadAttackAtom(void)
{
    SaveChunk *chunk;
    AttackAtom *attackAtom;
    AttackAtom *loadedAttackAtom;
    sdword size;
    sdword i;
    sdword numShips;

    chunk = LoadNextChunk();
    // CAT (M2-R6 oracle-mode-a): the exe's atom is 0x24 bytes up to the attackers, their count at +0x1C
    //      (LoadRetreatAtom 0x00503380), then a 4-byte ID per ship
    chunk = saveCat32ChunkArray(chunk, SAVECAT32_LAYOUT(AttackAtom), 0x24, (udword)offsetof(AttackAtom, attackerList.ShipPtr),
                                NULL, saveCat32ChunkCount(chunk, 0x1C));
    VerifyChunkNoSize(chunk,BASIC_STRUCTURE|SAVE_ATTACKATOM);
    loadedAttackAtom = (AttackAtom *)chunkContents(chunk);

    numShips = loadedAttackAtom->attackerList.numShips;
    size = sizeof(AttackAtom) + sizeof(ShipPtr)*(numShips-1);

    dbgAssertOrIgnore(size == chunk->contentsSize);

    attackAtom = (AttackAtom *)memAlloc(size,"tacticsattackmemory",NonVolatile);
    memcpy(attackAtom,loadedAttackAtom,size);

    attackAtom->retreater = SpaceObjRegistryGetShip((sdword)attackAtom->retreater);

    for (i=0;i<numShips;i++)
    {
        attackAtom->attackerList.ShipPtr[i] = SpaceObjRegistryGetShip((sdword)attackAtom->attackerList.ShipPtr[i]);
    }

    memFree(chunk);
    return attackAtom;
}

void LoadAttackMemory(LinkedList *list)
{
    sdword num;
    sdword i;
    AttackAtom *attack;

    num = LoadInfoNumber();

    listInit(list);

    for (i=0;i<num;i++)
    {
        attack = LoadAttackAtom();
        listAddNode(list,&attack->attackLink,attack);
    }
}

void SaveResourceVolumeCB(void *stuff)
{
    // CAT (M2-R9 recorder-leftovers): SaveStructureOfSize's chunk, in the exe's 0xB0 layout in an exe-format save
    SaveChunk *chunk = CreateChunk(BASIC_STRUCTURE,sizeof(ResourceVolume),stuff);
    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(ResourceVolume));
    memFree(chunk);
}

void LoadResourceVolumeCB(LinkedList *list)
{
    ResourceVolume *resvol;
    SaveChunk *chunk;

    if (saveCat32Loading)
    {                                           // CAT (M2-R6 oracle-mode-a): the exe's 0x98-byte volume
        chunk = saveCat32ChunkStruct(LoadNextChunk(), SAVECAT32_LAYOUT(ResourceVolume));
        VerifyChunk(chunk,BASIC_STRUCTURE,sizeof(ResourceVolume));
        resvol = memAlloc(sizeof(ResourceVolume),"loadedstructure",0);
        memcpy(resvol,chunkContents(chunk),sizeof(ResourceVolume));
        memFree(chunk);
    }
    else
    {
        resvol = LoadStructureOfSize(sizeof(ResourceVolume));
    }

    listAddNode(list,&resvol->link,resvol);
}

void SaveUniverse(void)
{
    SaveChunk *chunk;
    Universe *savecontents;
    sdword i;

    chunk = CreateChunk(BASIC_STRUCTURE|SAVE_UNIVERSE,sizeof(Universe),&universe);
    savecontents = (Universe *)chunkContents(chunk);

    savecontents->curPlayerPtr = (Player*)SavePlayerToPlayerIndex(universe.curPlayerPtr);

    for (i=0;i<universe.numPlayers;i++)
    {
        savecontents->players[i].PlayerMothership = (Ship*)SpaceObjRegistryGetID((SpaceObj *)universe.players[i].PlayerMothership);
        Prefix_PlayerResearchInfo(&savecontents->players[i].researchinfo);
    }

    for (i=0;i<UNIV_NUMBER_WORLDS;i++)
    {
        savecontents->world[i] = (Derelict*)SpaceObjRegistryGetID((SpaceObj *)savecontents->world[i]);
    }

    cat_saveShipRecordTablePreFix();            // CAT: the ship names' IDs (SaveUniverse 0x005034c0, 0x0050f870)

    saveCat32SaveStruct(chunk, SAVECAT32_LAYOUT(Universe));   // CAT (M2-R9 recorder-leftovers): the exe's layout in an exe-format save

    Save_CommandLayer(&universe.mainCommandLayer);
    Save_CameraCommand(&universe.mainCameraCommand);

    SaveLinkedListOfSpaceObjs(&universe.RenderList);
    SaveLinkedListOfSpaceObjs(&universe.MinorRenderList);

    SaveLinkedListOfSpaceObjs(&universe.SpaceObjList);
    SaveLinkedListOfSpaceObjs(&universe.MinorSpaceObjList);
    SaveLinkedListOfSpaceObjs(&universe.ShipList);
    SaveLinkedListOfSpaceObjs(&universe.BulletList);
    SaveLinkedListOfSpaceObjs(&universe.ResourceList);
    SaveLinkedListOfSpaceObjs(&universe.DerelictList);
    SaveLinkedListOfSpaceObjs(&universe.ImpactableList);
    SaveLinkedListOfSpaceObjs(&universe.MissileList);
    SaveMineFormationList(&universe.MineFormationList);

    // CAT (M2-R9 recorder-leftovers): the dead ships, resources, derelicts and missiles are deleted here, after the
    //      lists are saved (SaveUniverse 0x005034c0 calls univDeleteDeadShips 0x00548e00, univDeleteDeadResources
    //      0x00549270, univDeleteDeadDerelicts 0x00549820 and univDeleteDeadMissiles 0x00549ac0 in that order after
    //      SaveMineFormationList); HW1 asserted the delete lists empty. A recording's save runs at the end of a
    //      universe update, where they need not be
    univDeleteDeadShips();
    univDeleteDeadResources();
    univDeleteDeadDerelicts();
    univDeleteDeadMissiles();

    // CAT: one tactics list: the attack memory at universe+0x180 (SaveUniverse 0x005034c0 saves its count and each
    //      atom with SaveRetreatAtom, 0x24 + 4n bytes = HW1's AttackAtom); HW1's RetreatList is gone. Then the blast
    //      list and the ship-name table (cat_saveUniverseList5e288 0x005169f0, cat_saveShipRecordTable)
    SaveAttackMemory(&universe.AttackMemory);
    cat_saveUniverseList5e288();
    cat_saveShipRecordTable();

    SaveLinkedListOfStuff(&universe.ResourceVolumeList,SaveResourceVolumeCB);

    SaveGrowSelection(&universe.HousekeepShipList);
    SaveGrowSelection(&universe.catSelection1A0);   // CAT: the second grow selection (SaveUniverse 0x005034c0)

    for (i=0;i<universe.numPlayers;i++)
    {
        Save_PlayerResearchInfo(&universe.players[i].researchinfo);
        cat_saveSavePlayerFOW(&universe.players[i]);       // CAT: SaveUniverse 0x005034c0 (M2-R6 nis-fow-camera)
    }

    cat_saveShipCounters();                     // CAT: the ship counters and the frame-step switch 0x00a6ed74
    SaveInfoNumber(catUnivFrameStep);           //      (SaveUniverse 0x005034c0)

    memFree(chunk);
}

void FillOutCollBlobList(LinkedList *list)
{
    sdword numBlobs = BlobRegistry.selection->numShips;
    sdword i;
    blob *tblob;

    listInit(list);
    for (i=0;i<numBlobs;i++)
    {
        tblob = (blob *)BlobRegistry.selection->ShipPtr[i];
        listAddNode(list,&tblob->node,tblob);
    }
}

void LoadUniverse(void)
{
    SaveChunk *chunk;
    sdword i;

    if (universe.star3dinfo)
    {
        star3dClose(universe.star3dinfo);
        universe.star3dinfo = NULL;
    }

    chunk = LoadNextChunk();
    chunk = saveCat32ChunkStruct(chunk, SAVECAT32_LAYOUT(Universe));    // CAT (M2-R6 oracle-mode-a): the exe's 0x3D9BC
    VerifyChunk(chunk,BASIC_STRUCTURE|SAVE_UNIVERSE,sizeof(Universe));

    memcpy(&universe,chunkContents(chunk),chunk->contentsSize);

    memFree(chunk);

    Load_CommandLayer(&universe.mainCommandLayer);
    Load_CameraCommand(&universe.mainCameraCommand);

    LoadLinkedListOfSpaceObjs(&universe.RenderList,RenderListAddCB);
    LoadLinkedListOfSpaceObjs(&universe.MinorRenderList,MinorRenderListAddCB);

    LoadLinkedListOfSpaceObjs(&universe.SpaceObjList,SpaceObjListAddCB);
    LoadLinkedListOfSpaceObjs(&universe.MinorSpaceObjList,MinorSpaceObjListAddCB);

    LoadLinkedListOfSpaceObjs(&universe.ShipList,ShipListAddCB);
    LoadLinkedListOfSpaceObjs(&universe.BulletList,BulletListAddCB);
    LoadLinkedListOfSpaceObjs(&universe.ResourceList,ResourceListAddCB);
    LoadLinkedListOfSpaceObjs(&universe.DerelictList,DerelictListAddCB);
    LoadLinkedListOfSpaceObjs(&universe.ImpactableList,ImpactableListAddCB);
    LoadLinkedListOfSpaceObjs(&universe.MissileList,MissileListAddCB);
    LoadMineFormationList(&universe.MineFormationList);

    listInit(&universe.DeleteMissileList);
    listInit(&universe.DeleteResourceList);
    listInit(&universe.DeleteDerelictList);
    listInit(&universe.DeleteShipList);

    FillOutCollBlobList(&universe.collBlobList);

    LoadAttackMemory(&universe.AttackMemory);   // CAT: the one tactics list (see SaveUniverse)
    // CAT: then the blast list, the bullet effects to make again and the ship-name table (LoadUniverse 0x00503910:
    //      cat_loadUniverseList5e288 0x00516a10, cat_univRequestBulletEffectsRecreate 0x0054d240,
    //      cat_loadShipRecordTable 0x0050f8c0)
    cat_loadUniverseList5e288();
    cat_univRequestBulletEffectsRecreate();
    cat_loadShipRecordTable();

    LoadLinkedListOfStuff(&universe.ResourceVolumeList,LoadResourceVolumeCB);

    LoadGrowSelectionAndFix(&universe.HousekeepShipList);
    LoadGrowSelectionAndFix(&universe.catSelection1A0);     // CAT: the second grow selection (LoadUniverse 0x00503910)

    universe.star3dinfo = star3dInit(NUM_STARS, universe.radius - 1.0f, universe.radius);

    universe.curPlayerPtr = SavePlayerIndexToPlayer(universe.curPlayerPtr);

    for (i=0;i<universe.numPlayers;i++)
    {
        universe.players[i].PlayerMothership = SpaceObjRegistryGetShip((sdword)universe.players[i].PlayerMothership);
        Load_PlayerResearchInfo(&universe.players[i],&universe.players[i].researchinfo);
        // CAT (M2-R6 nis-fow-camera): the saved pointer is stale; the local player's grid is read back (LoadUniverse
        //      0x00503910: cat_saveLoadPlayerFOW 0x004ff7e0)
        universe.players[i].catFOWGrid = NULL;
        cat_saveLoadPlayerFOW(&universe.players[i]);
    }

    memset(&universe.players[MAX_MULTIPLAYER_PLAYERS],0,sizeof(Player));
    universe.players[MAX_MULTIPLAYER_PLAYERS].playerIndex = MAX_MULTIPLAYER_PLAYERS;

    for (i=0;i<UNIV_NUMBER_WORLDS;i++)
    {
        universe.world[i] = (Derelict *)SpaceObjRegistryGetObj((sdword)universe.world[i]);
    }

    cat_loadShipCounters();                     // CAT: the ship counters and the frame-step switch 0x00a6ed74
    catUnivFrameStep = (bool32)LoadInfoNumber();    //  (LoadUniverse 0x00503910)
}

